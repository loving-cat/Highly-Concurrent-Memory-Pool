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
	//��ȡ����Span��ӳ��
	Span* MapObjectToSpan(void* obj);
	//��ȡһ��Kҳ��span
	Span* NewSpan(size_t k);
	//�ͷſ���span
	void ReleaseSpanToPageCache(Span* span);

	//��
	std::mutex _PageMtx;
private:
	//��
	SpanList _spanLists[NPAGES];

	ObjectPool<Span> spanPool;

	//std::map<PAGE_ID, Span*> _idSpanMap;
	//std::unordered_map<PAGE_ID, Span*> _idSpanMap;
	//std::unordered_map<PAGE_ID, size_t> _idSizeMap;
	TCMalloc_PageMap1<32 - PAGE_SHIFT> _idSizeMap;
	//����ģʽ ����ģʽ
	PageCache()
	{}
	PageCache(const PageCache&) = delete;
	static PageCache _sInst;
};
