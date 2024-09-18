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
//ҳ�� 128
static const size_t	NPAGES = 128;
//PAGE_SHIFT ����ͨ��������ʾһ��ҳ���С�Ķ�����ͨ����2���ݴη������������ҳ���С�� 8KB���� PAGE_SHIFT ���ܱ�����Ϊ 13
// ��Ϊ 2^13 = 8192
static const size_t PAGE_SHIFT = 13;

#ifdef _WIN32
	typedef size_t PAGE_ID;
#elif _WIN64
	typedef size_t unsigned long long PAGE_ID;
#else
	//Linux
#endif // _WIN32

//ֱ��ȥ���ϰ�ҳ����ռ�
inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	//linux��brk mmap ��
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
	//svrk unmmap��
#endif
}


//ȡ�ýڵ�ͷ4/8���ֽڻ����һ���ڵ� nextNode
static void*& NextObj(void* obj)
{
	return *(void**)obj;
}

//�����зֺõ�С�������������
class FreeList
{
public:
	void Push(void* obj)
	{
		assert(obj);
		//ͷ��
		//*(void**)obj = _freeList;
		NextObj(obj) = _freeList;
		_freeList = obj;
		++_size;
	}
	//��һ����Χpush 
	void PushRange(void* start, void* end,size_t n)
	{
		NextObj(end) = _freeList;
		_freeList = start;

		//������֤+�����ϵ�
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
		//ͷɾ
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

//��������С�Ķ���ӳ�����
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
	//����ӳ�����һ����������Ͱ
	static inline size_t Index(size_t bytes)
	{
		assert(bytes <= MAX_BYTES);
		//ÿ�������ж��ٸ���
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

	//һ��thread cache �����Ļ����ȡ���ٸ�
	static size_t NumMoveSize(size_t size)
	{
		assert(size > 0);
		//[2,512],һ�������ƶ����ٸ������(������)����ֵ
		//С����һ���������޸�
		//С����һ���������޵�
		int num = MAX_BYTES / size;
		if (num < 2)
			num = 2;
		if (num > 512)
			num = 512;
		return num;
	}
	//����һ����ϵͳ��ȡ����ҳ
	//�������� 8byte
	//...
	//�������� 256kb
	static size_t NumMovePage(size_t size)
	{
		size_t num = NumMoveSize(size);
		//��������ֽ���
		size_t npage = num * size;
		//page���ת��������13λ 
		//
		npage >>= PAGE_SHIFT;
		if (npage == 0)
			npage = 1;
		return npage;
	}

};

//����������ҳ�Ĵ���ڴ��Ƚṹ
struct Span
{
	PAGE_ID _pageId = 0;//����ڴ����ʼҳ��ҳ��
	size_t _n = 0;				// ҳ������

	Span* _next = nullptr;			//˫������Ľṹ
	Span* _prev = nullptr;

	size_t _objSize = 0; //�кõ�С����Ĵ�С
	size_t _useCount = 0; // �к�С���ڴ棬�������thread cache�ļ���
	void* _freeList = nullptr;		//�кõ�С���ڴ����������
	bool _isUse = false;		//�Ƿ��ڱ�ʹ��
};

//��ͷ˫��ѭ������
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
		//��  ---- ��
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
		//1.�����ϵ� ���ö�ջȥ��BUG
		//2.�鿴ջ֡
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
	std::mutex _mtx; //Ͱ��
};