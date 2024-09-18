#include "CentralCache.h"
#include"PageCache.h"
CentralCache CentralCache::_sInst;

//��ȡһ���ǿյ�span
Span* CentralCache::GetOneSpan(SpanList& list, size_t byte_size)
{
	//�鿴��ǰ��spanlist���Ƿ��л���δ��������span
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
	//�Ȱ�central cache��Ͱ�������������������߳��ͷ��ڴ�����������������
	list._mtx.unlock();

	//�ߵ��������û�п��е�span�ˣ�ֻ����page cache Ҫ
	PageCache::GetInstance()->_PageMtx.lock();
	Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(byte_size));
	span->_isUse = true;
	span->_objSize = byte_size;
	PageCache::GetInstance()->_PageMtx.unlock();

	//�Ի�ȡspan�����з֣�����Ҫ��������Ϊ��������̷߳��ʲ������span
	

	//����span�Ĵ���ڴ����ʼ��ַ�ʹ���ڴ�Ĵ�С(�ֽ���)
	char* start = (char*)(span->_pageId << PAGE_SHIFT);
	size_t bytes = span->_n << PAGE_SHIFT;
	char* end = start + bytes; 
	//�Ѵ���ڴ��г�����������������
	//1.����һ������ȥ��ͷ������β��
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


	//�к�span�Ժ���Ҫ��span�ҵ�Ͱ����ȥ��ʱ���ټ���
	list._mtx.lock();
	list.PushFront(span);

	return span;
}


//�����Ļ����ȡһ�������Ķ����thread cache
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
{
	//���㣬Ҫ�Ǹ�Ͱ�����
	size_t index = SizeClass::Index(size);
	_spanLists[index]._mtx.lock();
	Span* span = GetOneSpan(_spanLists[index],size);
	assert(span);
	assert(span->_freeList);
	// ��span�ӻ�ȡbatchNum������
	//�������batchNum�����ж����ö���
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
		//˵��span���зֳ�ȥ������С���ڴ涼������
		//���span�Ϳ����ٻ��ո�page cache��page cache�����ٳ���ȥ��ǰ��ҳ�ĺϲ�
		if (span->_useCount == 0)
		{
			_spanLists[index].Erase(span);
			span->_freeList = nullptr;
			span->_next = nullptr;
			span->_prev = nullptr;
			//�ͷ�span��page cacheʱ��ʹ��page cache�����Ϳ�����
			//��ʱ��Ͱ�����
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