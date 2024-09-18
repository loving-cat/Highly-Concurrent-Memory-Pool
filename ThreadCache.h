#pragma once
#include"Common.h"


class ThreadCache
{
public:
	//������ͷ��ڴ����
	void* Allocate(size_t size);
	void Deallocate(void* ptr, size_t size);
	//�����Ļ����ȡ����
	void* FetchFromCentralCache(size_t index, size_t size);
	//�ͷŶ���ʱ���������ʱ�������ڴ�ص����Ļ���
	void ListTooLong(FreeList& list, size_t size);
private:
	//208��freeList��Ͱ
	FreeList _freeLists[NFREE_LISTS];

};
//static �������ֻ�ڵ�ǰ�ļ��ɼ������Խ��ȫ�ֱ����ڶ��CPP�ļ��г���
static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr;
