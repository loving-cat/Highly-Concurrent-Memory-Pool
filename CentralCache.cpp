#include "CentralCache.h"
#include"PageCache.h"
CentralCache CentralCache::_sInst;

//获取一个非空的span
Span* CentralCache::GetOneSpan(SpanList& list, size_t byte_size)
{
	//查看当前的spanlist中是否有还有未分配对象的span
	Span* it = list.Begin();
	while (it != list.End())
	{
		if (it->_freeList != nullptr)
		{
			return it;
		}
		else
		{
			it = it->_next;
		}
	}
	//先把central cache的桶锁解掉，这样如果其他线程释放内存对象回来，不会阻塞
	list._mtx.unlock();

	//走到这里就是没有空闲的span了，只能找page cache 要
	PageCache::GetInstance()->_PageMtx.lock();
	Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(byte_size));
	span->_isUse = true;
	span->_objSize = byte_size;
	PageCache::GetInstance()->_PageMtx.unlock();

	//对获取span进行切分，不需要加锁，因为这会其他线程访问不到这个span
	

	//计算span的大块内存的起始地址和大块内存的大小(字节数)
	char* start = (char*)(span->_pageId << PAGE_SHIFT);
	size_t bytes = span->_n << PAGE_SHIFT;
	char* end = start + bytes; 
	//把大块内存切成自由链表链接起来
	//1.先切一块下来去做头，方便尾插
	span->_freeList = start;
	start += byte_size;
	void* tail = span->_freeList;
	while (start < end)
	{
		NextObj(tail) = start;
		tail = NextObj(tail); //tail = start;
		start += byte_size;
	}
	NextObj(tail) = nullptr;

	/*int j = 0;
	void* cur = start;
	while (cur)
	{
		cur = NextObj(cur);
		++j;
	}
	if (j != bytes/byte_size)
	{
		int x = 0;
	}*/


	//切好span以后，需要把span挂到桶里面去的时候，再加锁
	list._mtx.lock();
	list.PushFront(span);

	return span;
}


//从中心缓存获取一定数量的对象给thread cache
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
{
	//先算，要那个桶里面的
	size_t index = SizeClass::Index(size);
	_spanLists[index]._mtx.lock();
	Span* span = GetOneSpan(_spanLists[index],size);
	assert(span);
	assert(span->_freeList);
	// 从span钟获取batchNum个对象
	//如果不够batchNum个，有多少拿多少
	start = span->_freeList;
	end = start;
	size_t i = 0;
	size_t actualNum = 1;
	while(i < batchNum-1 && NextObj(end) != nullptr)
	{
		end = NextObj(end);
		++i;
		++actualNum;
	}
	span->_freeList = NextObj(end);
	NextObj(end) = nullptr;
	span->_useCount += actualNum;

	/*int j = 0;
	void* cur = start;
	while (cur)
	{
		cur = NextObj(cur);
		++j;
	}
	if (j != actualNum)
	{
		int x = 0;
	}*/

	_spanLists[index]._mtx.unlock();
	return actualNum;
}
void CentralCache::ReleaseListToSpans(void* start, size_t byte_size)
{
	size_t index = SizeClass::Index(byte_size);
	_spanLists[index]._mtx.lock();
	while (start)
	{
		void* next = NextObj(start);
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);
		NextObj(start) = span->_freeList;
		span->_freeList = start;
		span->_useCount--;
		//说明span的切分出去的所有小块内存都回来了
		//这个span就可以再回收给page cache，page cache可以再尝试去做前后页的合并
		if (span->_useCount == 0)
		{
			_spanLists[index].Erase(span);
			span->_freeList = nullptr;
			span->_next = nullptr;
			span->_prev = nullptr;
			//释放span给page cache时，使用page cache的锁就可以了
			//这时把桶锁解掉
			_spanLists[index]._mtx.unlock();
			PageCache::GetInstance()->_PageMtx.lock();
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
			PageCache::GetInstance()->_PageMtx.unlock();

			_spanLists[index]._mtx.lock();

		}

		start = next;
	}
	_spanLists[index]._mtx.unlock();

}