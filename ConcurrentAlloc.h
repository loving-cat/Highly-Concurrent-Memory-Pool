#pragma once

#include "Common.h"
#include"PageCache.h"
#include "ThreadCache.h"
#include<thread>
static void* ConcurrentAlloc(size_t size)
{
	if (size > MAX_BYTES)
	{
		size_t alignSize = SizeClass::RoundUp(size);
		size_t kpage = alignSize >> PAGE_SHIFT;

		PageCache::GetInstance()->_PageMtx.lock();
		Span* span = PageCache::GetInstance()->NewSpan(kpage);
		//存储在span里
		span->_objSize = size;
		PageCache::GetInstance()->_PageMtx.unlock();

		void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
		return ptr;
	}
	else
	{
		//通过TLS 每个线程无锁的获取自己的专属的ThreadCache对象
		//每个线程获得自己的threadCache的地方
		if (pTLSThreadCache == nullptr)
		{
			static ObjectPool<ThreadCache> tcPool;
			//pTLSThreadCache = new ThreadCache;
			pTLSThreadCache = tcPool.New();
		}
		cout << std::this_thread::get_id() << ":" << pTLSThreadCache << endl;
		return pTLSThreadCache->Allocate(size);
	}
	
}
static void  ConcurrentFree(void* ptr)
{
	Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);
	size_t size = span->_objSize;
	if (size > MAX_BYTES)
	{
		Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);
		PageCache::GetInstance()->_PageMtx.lock();
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		PageCache::GetInstance()->_PageMtx.unlock();

	}
	else
	{
		assert(pTLSThreadCache);
		pTLSThreadCache->Deallocate(ptr, size);
	}
	
}