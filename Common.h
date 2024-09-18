#pragma once
#include<iostream>
#include<vector>
#include<vector>
#include<map>
#include<algorithm>
#include<unordered_map>
#include<time.h>
#include<assert.h>
#include<mutex>
#include<math.h>
using std::cout;
using std::endl;

#ifdef _WIN32
	#include<windows.h>
#else
	//...
#endif

static const size_t MAX_BYTES = 256 * 1024;
static const size_t NFREE_LISTS = 208;
//页数 128
static const size_t	NPAGES = 128;
//PAGE_SHIFT 常量通常用来表示一个页面大小的对数（通常是2的幂次方），例如如果页面大小是 8KB，则 PAGE_SHIFT 可能被定义为 13
// 因为 2^13 = 8192
static const size_t PAGE_SHIFT = 13;

#ifdef _WIN32
	typedef size_t PAGE_ID;
#elif _WIN64
	typedef size_t unsigned long long PAGE_ID;
#else
	//Linux
#endif // _WIN32

//直接去堆上按页申请空间
inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	//linux下brk mmap 等
#endif
	if (ptr == nullptr)
		throw std::bad_alloc();
	return ptr;
}

inline static void SystemFree(void* ptr)
{
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
	//svrk unmmap等
#endif
}


//取得节点头4/8个字节获得下一个节点 nextNode
static void*& NextObj(void* obj)
{
	return *(void**)obj;
}

//管理切分好的小对象的自由链表
class FreeList
{
public:
	void Push(void* obj)
	{
		assert(obj);
		//头插
		//*(void**)obj = _freeList;
		NextObj(obj) = _freeList;
		_freeList = obj;
		++_size;
	}
	//给一个范围push 
	void PushRange(void* start, void* end,size_t n)
	{
		NextObj(end) = _freeList;
		_freeList = start;

		//测试验证+条件断点
		/*int i = 0;
		void* cur = start;
		while (cur)
		{
			cur = NextObj(cur);
			++i;
		}
		if (n != i)
		{
			int x = 0;
		}*/

		_size += n;
	}

	void* Pop()
	{
		assert(_freeList);
		//头删
		void* obj = _freeList;
		_freeList = NextObj(obj);
		--_size;
		return obj;
	}
	void PopRange(void* start, void* end, size_t n)
	{
		assert(n >= _size);
		start = _freeList;
		end = start;
		for (size_t i = 0; i < n - 1; ++i)
		{
			end = NextObj(end);
		}
		_freeList = NextObj(end);
		NextObj(end) = nullptr;
		_size -= n;
	}
	bool Empty()
	{
		return _freeList == nullptr;
	}
	size_t& MaxSize()
	{
		return _maxSize;
	}
	size_t Size()
	{
		return _size;
	}
private:
	void* _freeList = nullptr;
	size_t _maxSize = 1;
	size_t _size;
};

//计算对象大小的对齐映射规则
class SizeClass
{
public:
	/*size_t _RoundUp(size_t size, size_t AlignNum)
	{
		size_t alignSize;
		if (size % 8 != 0)
		{
			alignSize =(size / AlignNum+1)*AlignNum;
		}
		else
		{
			alignSize = size;
		}
	}*/
	static inline size_t  _RoundUp(size_t size, size_t AlignNum)
	{
		return ((size+AlignNum - 1) & ~(AlignNum - 1));
	
	}

	static inline size_t RoundUp(size_t size)
	{
		if (size <= 128)
		{
			return _RoundUp(size, 8); 
		}
		else if (size <= 1024)
		{
			return _RoundUp(size, 16);
		}
		else if (size <= 8 * 1024)
		{
			return _RoundUp(size, 128);
		}
		else if (size <= 64 * 1024)
		{
			return _RoundUp(size, 1024);
		}
		else if (size <= 256 * 1024)
		{
			return _RoundUp(size, 8* 1024);
		}
		else
		{
			return _RoundUp(size, 1 << PAGE_SHIFT);
		}

	}
	static inline size_t _Index(size_t bytes, size_t align_shift)
	{
		return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
	}
	//计算映射的哪一个自由链表桶
	static inline size_t Index(size_t bytes)
	{
		assert(bytes <= MAX_BYTES);
		//每个区间有多少个链
		static int group_array[4] = { 16,56,56,56 };
		if (bytes <= 128)
		{
			return _Index(bytes, 3);
		}
		else if (bytes <= 1024)
		{
			return _Index(bytes - 128, 4) + group_array[0];
		}
		else if (bytes <= 8 * 1024)
		{
			return _Index(bytes - 1024, 7) + group_array[1] + group_array[0];
		}
		else if (bytes <= 64 * 1024)
		{
			return _Index(bytes - 8 * 1024, 10) + group_array[2] + group_array[1] + group_array[0];
		}
		else if (bytes <= 256 * 1024)
		{
			return _Index(bytes - 64*1024,13)+group_array[3] + group_array[2] + group_array[1] + group_array[0];
		}
		else
		{
			assert(false);
		}
		return -1;
	}

	//一次thread cache 从中心缓存获取多少个
	static size_t NumMoveSize(size_t size)
	{
		assert(size > 0);
		//[2,512],一次批量移动多少个对象的(慢启动)上限值
		//小对象一次批量上限高
		//小对象一次批量上限低
		int num = MAX_BYTES / size;
		if (num < 2)
			num = 2;
		if (num > 512)
			num = 512;
		return num;
	}
	//计算一次向系统获取几个页
	//单个对象 8byte
	//...
	//单个对象 256kb
	static size_t NumMovePage(size_t size)
	{
		size_t num = NumMoveSize(size);
		//算出来的字节数
		size_t npage = num * size;
		//page间的转换，右移13位 
		//
		npage >>= PAGE_SHIFT;
		if (npage == 0)
			npage = 1;
		return npage;
	}

};

//管理多个连续页的大块内存跨度结构
struct Span
{
	PAGE_ID _pageId = 0;//大块内存的起始页的页号
	size_t _n = 0;				// 页的数量

	Span* _next = nullptr;			//双向链表的结构
	Span* _prev = nullptr;

	size_t _objSize = 0; //切好的小对象的大小
	size_t _useCount = 0; // 切好小块内存，被分配给thread cache的计数
	void* _freeList = nullptr;		//切好的小块内存的自由链表
	bool _isUse = false;		//是否在被使用
};

//带头双向循环链表
class SpanList
{
public:
	SpanList()
	{
		_head = new Span;
		_head->_next = _head;
		_head->_prev = _head;
	}
	Span* Begin()
	{
		return _head->_next;
	}
	Span* End()
	{
		return _head;
	}
	bool Empty()
	{
		//真  ---- 空
		return _head->_next == _head;
	}
	void PushFront(Span* span)
	{
		Insert(Begin(), span);
	}
	Span* PopFront()
	{
		Span* front = _head->_next;
		Erase(front);
		return front;
	}
	void Insert(Span* pos, Span* newSpan)
	{
		assert(pos);
		assert(newSpan);

		Span* prev = pos->_prev;
		//prev newspan pos
		prev->_next = newSpan;
		newSpan->_prev = prev;
		newSpan->_next = pos;
		pos->_prev = newSpan;

	}
	void Erase(Span* pos)
	{
		assert(pos);
		//assert(pos != _head);
		//1.条件断点 调用堆栈去查BUG
		//2.查看栈帧
		if (pos != _head)
		{
			int x = 0;
		}

		Span* prev = pos->_prev;
		Span* next = pos->_next;
		
		prev->_next = next;
		next->_prev = prev;
	}
private:
	Span* _head;
public:
	std::mutex _mtx; //桶锁
};