#ifndef Engine_Function_h__
#define Engine_Function_h__

#include "Engine_Typedef.h"

namespace Engine
{
	template<typename T>
	void	Safe_Delete(T& Pointer)
	{
		if (nullptr != Pointer)
		{
			delete Pointer;
			Pointer = nullptr;
		}
	}

	template<typename T>
	void	Safe_Delete_Array(T& Pointer)
	{
		if (nullptr != Pointer)
		{
			delete[] Pointer;
			Pointer = nullptr;
		}
	}

	template<typename T>
	uint32_t Safe_AddRef(T& Instance)
	{
		uint32_t iRefCnt = 0;

		if (nullptr != Instance)
			iRefCnt = Instance->AddRef();

		return iRefCnt;
	}

	template<typename T>
	uint32_t Safe_Release(T& Instance)
	{
		uint32_t iRefCnt = 0;

		if (nullptr != Instance)
		{
			iRefCnt = Instance->Release();

			if (iRefCnt == 0)
				Instance = nullptr;
		}
		return iRefCnt;
	}
}

#endif // Engine_Function_h__
