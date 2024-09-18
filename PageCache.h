#pragma once
#include "Common.h"
#include"ObjectPool.h"
#include"PageMap.h"
class PageCache
{
public:
	static PageCache* GetInstance() 
	{
		return &_sInst;
	}
	//获取对象到Span的映射
	Span* MapObjectToSpan(void* obj);
	//获取一个K页的span
	Span* NewSpan(size_t k);
	//释放空闲span
	void ReleaseSpanToPageCache(Span* span);

	//锁
	std::mutex _PageMtx;
private:
	//表
	SpanList _spanLists[NPAGES];

	ObjectPool<Span> spanPool;

	//std::map<PAGE_ID, Span*> _idSpanMap;
	//std::unordered_map<PAGE_ID, Span*> _idSpanMap;
	//std::unordered_map<PAGE_ID, size_t> _idSizeMap;
	TCMalloc_PageMap1<32 - PAGE_SHIFT> _idSizeMap;
	//单例模式 饿汉模式
	PageCache()
	{}
	PageCache(const PageCache&) = delete;
	static PageCache _sInst;
};
