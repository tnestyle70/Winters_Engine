#define _CRT_SECURE_NO_WARNINGS
#include "Map/MapDataIO.h"
#include "Manager/Structure_Manager.h"
#include "Manager/Jungle_Manager.h"
#include "Manager/Minion_Manager.h"
#include <Windows.h>
#include <cstdio>
#include <string>

namespace
{
    namespace WMap = Winters::Map;

    bool FileExistsFile(const wchar_t* path)
    {
        const DWORD attr = GetFileAttributesW(path);
        return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    bool DirectoryExists(const wchar_t* path)
    {
        const DWORD attr = GetFileAttributesW(path);
        return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

    void EnsureTrailingSlash(std::wstring& path)
    {
        if (!path.empty() && path.back() != L'\\' && path.back() != L'/')
            path.push_back(L'\\');
    }

    bool TryFindWorkspaceDataRootFrom(const std::wstring& startDir, std::wstring& outRoot)
    {
        std::wstring base = startDir;
        EnsureTrailingSlash(base);

        for (int depth = 0; depth < 8 && !base.empty(); ++depth)
        {
            const std::wstring solutionPath = base + L"Winters.sln";
            const std::wstring dataRoot = base + L"Data\\";
            if (FileExistsFile(solutionPath.c_str()) && DirectoryExists(dataRoot.c_str()))
            {
                outRoot = dataRoot;
                return true;
            }

            std::wstring trimmed = base;
            while (!trimmed.empty() && (trimmed.back() == L'\\' || trimmed.back() == L'/'))
                trimmed.pop_back();

            const size_t slash = trimmed.find_last_of(L"\\/");
            if (slash == std::wstring::npos)
                break;

            base = trimmed.substr(0, slash + 1);
        }

        return false;
    }

    bool TryFindWorkspaceDataRoot(std::wstring& outRoot)
    {
        wchar_t exePath[MAX_PATH] = {};
        const DWORD n = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        if (n > 0 && n < MAX_PATH)
        {
            std::wstring exeDir = exePath;
            const size_t slash = exeDir.find_last_of(L"\\/");
            if (slash != std::wstring::npos)
            {
                exeDir.resize(slash + 1);
                if (TryFindWorkspaceDataRootFrom(exeDir, outRoot))
                    return true;
            }
        }

        wchar_t cwd[MAX_PATH] = {};
        const DWORD cwdLen = GetCurrentDirectoryW(MAX_PATH, cwd);
        if (cwdLen > 0 && cwdLen < MAX_PATH)
            return TryFindWorkspaceDataRootFrom(cwd, outRoot);

        return false;
    }
}

HRESULT CMapDataIO::Save_Stage(const wchar_t* pAbsPath)
{
    if (!pAbsPath) return E_FAIL;

    FILE* pFile = nullptr;
    if (_wfopen_s(&pFile, pAbsPath, L"wb") != 0 || !pFile)
    {
        char msg[MAX_PATH + 64];
        sprintf_s(msg, "[MapDataIO::Save_Stage] fopen failed: %ws\n", pAbsPath);
        return E_FAIL;
    }

    WMap::StageHeader header{};
    header.magic   = WMap::STAGE_MAGIC;
    header.version = WMap::STAGE_VERSION;
    fwrite(&header, sizeof(WMap::StageHeader), 1, pFile);

    if (FAILED(CStructure_Manager::Get()->Save_ToFile(pFile)))       { fclose(pFile); return E_FAIL; }
    if (FAILED(CJungle_Manager::Get()->Save_ToFile(pFile)))          { fclose(pFile); return E_FAIL; }
    if (FAILED(CMinion_Manager::Get()->Save_ToFile(pFile)))          { fclose(pFile); return E_FAIL; }

    fclose(pFile);

    u32_t wpTotal = 0;
    for (u32_t t = 0; t < 2; ++t) for (u32_t l = 0; l < 3; ++l)
        wpTotal += CMinion_Manager::Get()->Get_WaypointCount(
            static_cast<eMinionTeam>(t), static_cast<eMinionWay>(l));

    char ok[MAX_PATH + 128];
    sprintf_s(ok, "[MapDataIO] saved: S=%u J=%u W=%u -> %ws\n",
        CStructure_Manager::Get()->Get_Count(),
        CJungle_Manager::Get()->Get_Count(),
        wpTotal,
        pAbsPath);
    return S_OK;
}

HRESULT CMapDataIO::Load_Stage(const wchar_t* pAbsPath)
{
    if (!pAbsPath) return E_FAIL;

    FILE* pFile = nullptr;
    if (_wfopen_s(&pFile, pAbsPath, L"rb") != 0 || !pFile)
    {
        char msg[MAX_PATH + 64];
        sprintf_s(msg, "[MapDataIO::Load_Stage] file not found: %ws\n", pAbsPath);
        return E_FAIL;
    }

    WMap::StageHeader header{};
    if (fread(&header, sizeof(WMap::StageHeader), 1, pFile) != 1)
    {
        fclose(pFile);
        return E_FAIL;
    }
    if (header.magic != WMap::STAGE_MAGIC)
    {
        fclose(pFile);
        char m[128]; sprintf_s(m, "[MapDataIO] magic mismatch: 0x%08X\n", header.magic);
        return E_FAIL;
    }
    if (header.version < WMap::STAGE_VERSION_MIN_COMPAT || header.version > WMap::STAGE_VERSION)
    {
        fclose(pFile);
        char m[128]; sprintf_s(m, "[MapDataIO] version out of range: %u (min=%u max=%u)\n",
            header.version, WMap::STAGE_VERSION_MIN_COMPAT, WMap::STAGE_VERSION);
        return E_FAIL;
    }

    if (FAILED(CStructure_Manager::Get()->Load_FromFile(pFile)))       { fclose(pFile); return E_FAIL; }
    if (FAILED(CJungle_Manager::Get()->Load_FromFile(pFile)))          { fclose(pFile); return E_FAIL; }

    if (header.version >= 4)
    {
        if (FAILED(CMinion_Manager::Get()->Load_FromFile(pFile)))      { fclose(pFile); return E_FAIL; }
    }
    else
    {
        // v3 ?섏쐞?명솚 ????λ맂 WP ?놁쓬 ??湲곕낯媛?遺?몄뒪?몃옪
        CMinion_Manager::Get()->LoadDefaults();
    }

    fclose(pFile);

    u32_t wpTotal = 0;
    for (u32_t t = 0; t < 2; ++t) for (u32_t l = 0; l < 3; ++l)
        wpTotal += CMinion_Manager::Get()->Get_WaypointCount(
            static_cast<eMinionTeam>(t), static_cast<eMinionWay>(l));

    char ok[MAX_PATH + 128];
    sprintf_s(ok, "[MapDataIO] loaded(v%u): S=%u J=%u W=%u <- %ws\n",
        header.version,
        CStructure_Manager::Get()->Get_Count(),
        CJungle_Manager::Get()->Get_Count(),
        wpTotal,
        pAbsPath);
    return S_OK;
}

bool CMapDataIO::Get_StagePathW(i32_t stageIndex, wchar_t* pOutBuf, u32_t capacity)
{
    if (!pOutBuf || capacity < MAX_PATH) return false;

    std::wstring dataRoot;
    if (!TryFindWorkspaceDataRoot(dataRoot))
        return false;

    return swprintf_s(pOutBuf, capacity, L"%sStage%d.dat",
        dataRoot.c_str(), static_cast<int>(stageIndex)) > 0;
}

bool CMapDataIO::GetNavGridPathW(i32_t stageIndex, wchar_t* pOutBuf, u32_t capacity)
{
    if (!Get_StagePathW(stageIndex, pOutBuf, capacity))
        return false;

    wchar_t* const dot = wcsrchr(pOutBuf, L'.');
    if (!dot)
        return false;

    return wcscpy_s(dot, capacity - static_cast<u32_t>(dot - pOutBuf), L".navgrid") == 0;
}
