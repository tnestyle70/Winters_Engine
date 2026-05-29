#pragma once

#include "WintersTypes.h"

#include <string>

struct lua_State;

namespace Engine
{
	class CLuaRuntime final
	{
	public:
		bool_t Initialize();
		void Shutdown();

		bool_t LoadFile(const wchar_t* pPath);
		bool_t CallGlobal(const char* pName, i32_t iArgCount = 0, i32_t iReturnCount = 0);
		void RegisterCFunction(const char* pName, int(*pFn)(lua_State*));
		void PushLightUserData(const char* pName, void* pUserData);

		lua_State* GetState() const { return m_pState; }
		const std::string& GetLastError() const { return m_strLastError; }

	private:
		bool_t ReportError(const char* pPrefix);

		lua_State* m_pState = nullptr;
		std::string m_strLastError;
	};
}