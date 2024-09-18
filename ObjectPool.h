#pragma once
#include<iostream>
#include"Common.h"
using std::cout;
using std::endl;

//�����ڴ��
//template<size_t N>
//class ObjectPool
//{


template<class T>
class ObjectPool
{
public:
	T* New()
	{
		T* obj = nullptr;
		//���Ȱѻ������ڴ������ٴ��ظ�����
		if (_freeList)
		{
			void* next = *((void**)_freeList);
			obj =(T*) _freeList;
			_freeList = next;
		}
		else
		{
			// ʣ���ڴ治��һ�������Сʱ�������¿����ռ�
			if (_remainBytes < sizeof(T))
			{
				_remainBytes = 128 * 1024;
				_memory = (char*)malloc(128 * 1024);
				if (_memory == nullptr)
				{
					throw std::bad_alloc();
				}
			}
			obj = (T*)_memory;
			//���С��һ��ָ���С��ֱ�Ӹ���ָ�룬��֮���������С
			size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
			_memory += objSize;
			_remainBytes -= objSize;
		}
		//��λnew����ʾ����T�Ĺ��캯����ʼ��
		new(obj)T;
		return obj;
	}
	//
	void Delete(T* obj)
	{
		//if (_freeList == nullptr)
		//{
		//	_freeList = obj;
		//	//*(int*)obj = nullptr;
		//	*(void**)obj = nullptr;
		//	//32λ�� void** ��4λ�ֽ�  64 λ �� 8�ֽ�
		//	//32λ��ָ����4�ֽڣ�64λ����8�ֽ�		
		//}
		//��ʾ�������������������
		obj->~T();

			//ͷ��
			*(void**)obj = _freeList;
			_freeList = obj;

	}
private:
	char* _memory;//ָ�����ڴ��ָ�� ����ϵͳȡ�õ��ڴ�  char* Ϊ1�ֽڣ�����ȥϵͳ���ڴ棬��
	size_t _remainBytes = 0;//����ڴ����зֹ�����ʣ���ֽ���
	void* _freeList = nullptr;//���������������ӵ����������ͷָ��
};
