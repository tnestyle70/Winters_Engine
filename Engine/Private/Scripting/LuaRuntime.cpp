#define _CRT_SECURE_NO_WARNINGS
#include "Scripting/LuaRuntime.h"
#include "WintersPaths.h"

extern "C"
{
#include "../../External/lua-5.4.8/src/lua.h"
#include "../../External/lua-5.4.8/src/lauxlib.h"
#include "../../External/lua-5.4.8/src/lualib.h"
}

#include <Windows.h>
#include <cstdio>

namespace Engine
{
    namespace
    {
        void OpenLuaLibrary(lua_State* pState, const char* pName, lua_CFunction pFn)
        {
            luaL_requiref(pState, pName, pFn, 1);
            lua_pop(pState, 1);
        }

        void DisableLuaGlobal(lua_State* pState, const char* pName)
        {
            lua_pushnil(pState);
            lua_setglobal(pState, pName);
        }

        void OpenSafeUILibraries(lua_State* pState)
        {
            OpenLuaLibrary(pState, LUA_GNAME, luaopen_base);
            OpenLuaLibrary(pState, LUA_TABLIBNAME, luaopen_table);
            OpenLuaLibrary(pState, LUA_STRLIBNAME, luaopen_string);
            OpenLuaLibrary(pState, LUA_MATHLIBNAME, luaopen_math);
            OpenLuaLibrary(pState, LUA_UTF8LIBNAME, luaopen_utf8);

            DisableLuaGlobal(pState, "dofile");
            DisableLuaGlobal(pState, "loadfile");
            DisableLuaGlobal(pState, "load");
            DisableLuaGlobal(pState, "collectgarbage");
        }
    }

    bool_t CLuaRuntime::Initialize()
    {
        if (m_pState)
            return true;

        m_pState = luaL_newstate();
        if (!m_pState)
        {
            m_strLastError = "luaL_newstate failed";
            return false;
        }

        OpenSafeUILibraries(m_pState);
        return true;
    }

    void CLuaRuntime::Shutdown()
    {
        if (m_pState)
        {
            lua_close(m_pState);
            m_pState = nullptr;
        }
        m_strLastError.clear();
    }

    bool_t CLuaRuntime::LoadFile(const wchar_t* pPath)
    {
        if (!m_pState && !Initialize())
            return false;
        if (!pPath)
        {
            m_strLastError = "LoadFile path is null";
            return false;
        }

        wchar_t resolvedPath[MAX_PATH] = {};
        const wchar_t* pReadPath = pPath;
        if (WintersResolveContentPath(pPath, resolvedPath, MAX_PATH))
            pReadPath = resolvedPath;

        FILE* pFile = nullptr;
        if (_wfopen_s(&pFile, pReadPath, L"rb") != 0 || !pFile)
        {
            char msg[512] = {};
            std::snprintf(msg, sizeof(msg), "Lua file open failed");
            m_strLastError = msg;
            return false;
        }

        std::string bytes;
        std::fseek(pFile, 0, SEEK_END);
        const long size = std::ftell(pFile);
        std::fseek(pFile, 0, SEEK_SET);
        if (size > 0)
        {
            bytes.resize(static_cast<std::size_t>(size));
            std::fread(bytes.data(), 1, bytes.size(), pFile);
        }
        std::fclose(pFile);

        if (luaL_loadbufferx(m_pState, bytes.data(), bytes.size(), "WintersLuaUI", nullptr) != LUA_OK)
            return ReportError("luaL_loadbufferx");
        if (lua_pcall(m_pState, 0, 0, 0) != LUA_OK)
            return ReportError("lua_pcall");

        m_strLastError.clear();
        return true;
    }

    bool_t CLuaRuntime::CallGlobal(const char* pName, i32_t iArgCount, i32_t iReturnCount)
    {
        if (!m_pState || !pName)
            return false;

        lua_getglobal(m_pState, pName);
        if (!lua_isfunction(m_pState, -1))
        {
            lua_pop(m_pState, 1);
            return false;
        }

        if (iArgCount > 0)
            lua_insert(m_pState, -1 - iArgCount);

        if (lua_pcall(m_pState, iArgCount, iReturnCount, 0) != LUA_OK)
            return ReportError(pName);

        return true;
    }

    void CLuaRuntime::RegisterCFunction(const char* pName, int(*pFn)(lua_State*))
    {
        if (!m_pState || !pName || !pFn)
            return;

        lua_pushcfunction(m_pState, pFn);
        lua_setglobal(m_pState, pName);
    }

    void CLuaRuntime::PushLightUserData(const char* pName, void* pUserData)
    {
        if (!m_pState || !pName)
            return;

        lua_pushlightuserdata(m_pState, pUserData);
        lua_setglobal(m_pState, pName);
    }

    bool_t CLuaRuntime::ReportError(const char* pPrefix)
    {
        const char* pError = lua_tostring(m_pState, -1);
        m_strLastError = pPrefix ? pPrefix : "Lua";
        m_strLastError += ": ";
        m_strLastError += pError ? pError : "(unknown)";
        lua_pop(m_pState, 1);

        std::string dbg = "[LuaRuntime] " + m_strLastError + "\n";
        OutputDebugStringA(dbg.c_str());
        return false;
    }
}
