#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <string>

#include "WintersPaths.h"

namespace
{
    bool FileExistsFile(const wchar_t* path)
    {
        const DWORD a = GetFileAttributesW(path);
        return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    bool DirectoryExists(const wchar_t* path)
    {
        const DWORD a = GetFileAttributesW(path);
        return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

    void NormalizeSeparators(std::wstring& s)
    {
        for (wchar_t& c : s)
        {
            if (c == L'/')
                c = L'\\';
        }
    }

    bool StartsWithPath(const std::wstring& s, const wchar_t* prefix)
    {
        const size_t prefixLen = wcslen(prefix);
        return s.size() >= prefixLen && _wcsnicmp(s.c_str(), prefix, prefixLen) == 0;
    }

    bool EndsWithPath(const std::wstring& s, const wchar_t* suffix)
    {
        const size_t suffixLen = wcslen(suffix);
        if (s.size() < suffixLen)
            return false;

        return _wcsicmp(s.c_str() + (s.size() - suffixLen), suffix) == 0;
    }

    void EnsureTrailingSlash(std::wstring& s)
    {
        if (!s.empty() && s.back() != L'\\')
            s.push_back(L'\\');
    }

    bool TryResolveExistingFile(
        const std::wstring& candidate,
        wchar_t* outFullPath,
        uint32_t outCapacityChars)
    {
        wchar_t full[MAX_PATH] = {};
        const DWORD got = GetFullPathNameW(candidate.c_str(), MAX_PATH, full, nullptr);
        if (got == 0 || got >= MAX_PATH || !FileExistsFile(full))
            return false;

        wcscpy_s(outFullPath, outCapacityChars, full);
        return true;
    }

    bool TryGetResourceSubPath(const std::wstring& rel, std::wstring& outSubPath)
    {
        if (StartsWithPath(rel, L"Client\\Bin\\Resource\\"))
        {
            outSubPath = rel.substr(wcslen(L"Client\\Bin\\Resource\\"));
            return true;
        }

        if (StartsWithPath(rel, L"Resource\\"))
        {
            outSubPath = rel.substr(wcslen(L"Resource\\"));
            return true;
        }

        if (StartsWithPath(rel, L"Texture\\") ||
            StartsWithPath(rel, L"Font\\") ||
            StartsWithPath(rel, L"Sound\\") ||
            StartsWithPath(rel, L"UI\\") ||
            StartsWithPath(rel, L"FX\\"))
        {
            outSubPath = rel;
            return true;
        }

        return false;
    }

    bool TryFindCanonicalResourceRoot(const std::wstring& startDir, std::wstring& outRoot)
    {
        std::wstring base = startDir;
        EnsureTrailingSlash(base);

        for (uint32_t depth = 0; depth < 8 && !base.empty(); ++depth)
        {
            if (EndsWithPath(base, L"Client\\Bin\\"))
            {
                std::wstring candidate = base + L"Resource\\";
                if (DirectoryExists(candidate.c_str()))
                {
                    outRoot = candidate;
                    return true;
                }
            }

            {
                std::wstring candidate = base + L"Client\\Bin\\Resource\\";
                if (DirectoryExists(candidate.c_str()))
                {
                    outRoot = candidate;
                    return true;
                }
            }

            std::wstring trimmed = base;
            while (!trimmed.empty() && trimmed.back() == L'\\')
                trimmed.pop_back();

            const size_t slash = trimmed.find_last_of(L'\\');
            if (slash == std::wstring::npos)
                break;

            base = trimmed.substr(0, slash + 1);
        }

        return false;
    }

    bool TryGetDataSubPath(const std::wstring& rel, std::wstring& outSubPath)
    {
        if (!StartsWithPath(rel, L"Data\\"))
            return false;

        outSubPath = rel.substr(wcslen(L"Data\\"));
        return !outSubPath.empty();
    }

    bool TryFindCanonicalDataRoot(const std::wstring& startDir, std::wstring& outRoot)
    {
        std::wstring base = startDir;
        EnsureTrailingSlash(base);

        for (uint32_t depth = 0; depth < 8 && !base.empty(); ++depth)
        {
            const std::wstring solutionPath = base + L"Winters.sln";
            const std::wstring dataRoot = base + L"Data\\";
            if (FileExistsFile(solutionPath.c_str()) && DirectoryExists(dataRoot.c_str()))
            {
                outRoot = dataRoot;
                return true;
            }

            std::wstring trimmed = base;
            while (!trimmed.empty() && trimmed.back() == L'\\')
                trimmed.pop_back();

            const size_t slash = trimmed.find_last_of(L'\\');
            if (slash == std::wstring::npos)
                break;

            base = trimmed.substr(0, slash + 1);
        }

        return false;
    }

    bool TryResolveCanonicalResourcePath(
        const std::wstring& startDir,
        const std::wstring& resourceSubPath,
        wchar_t* outFullPath,
        uint32_t outCapacityChars)
    {
        std::wstring root;
        if (!TryFindCanonicalResourceRoot(startDir, root))
            return false;

        return TryResolveExistingFile(root + resourceSubPath, outFullPath, outCapacityChars);
    }

    bool TryResolveCanonicalDataPath(
        const std::wstring& startDir,
        const std::wstring& dataSubPath,
        wchar_t* outFullPath,
        uint32_t outCapacityChars)
    {
        std::wstring root;
        if (!TryFindCanonicalDataRoot(startDir, root))
            return false;

        return TryResolveExistingFile(root + dataSubPath, outFullPath, outCapacityChars);
    }
}

WINTERS_ENGINE bool WintersResolveContentPath(const wchar_t* relativePath, wchar_t* outFullPath, uint32_t outCapacityChars)
{
    if (!relativePath || !outFullPath || outCapacityChars < MAX_PATH)
        return false;

    std::wstring rel(relativePath);
    NormalizeSeparators(rel);

    while (!rel.empty() && (rel.front() == L'\\' || rel.front() == L'/'))
        rel.erase(0, 1);

    wchar_t exePath[MAX_PATH] = {};
    const DWORD n = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
        return false;

    wchar_t* const lastSlash = wcsrchr(exePath, L'\\');
    if (!lastSlash)
        return false;
    *(lastSlash + 1) = L'\0';

    std::wstring resourceSubPath;
    if (TryGetResourceSubPath(rel, resourceSubPath))
    {
        if (TryResolveCanonicalResourcePath(exePath, resourceSubPath, outFullPath, outCapacityChars))
            return true;

        wchar_t cwd[MAX_PATH] = {};
        const DWORD cwdLen = GetCurrentDirectoryW(MAX_PATH, cwd);
        if (cwdLen > 0 && cwdLen < MAX_PATH &&
            TryResolveCanonicalResourcePath(cwd, resourceSubPath, outFullPath, outCapacityChars))
        {
            return true;
        }

        return false;
    }

    std::wstring dataSubPath;
    if (TryGetDataSubPath(rel, dataSubPath))
    {
        if (TryResolveCanonicalDataPath(exePath, dataSubPath, outFullPath, outCapacityChars))
            return true;

        wchar_t cwd[MAX_PATH] = {};
        const DWORD cwdLen = GetCurrentDirectoryW(MAX_PATH, cwd);
        if (cwdLen > 0 && cwdLen < MAX_PATH &&
            TryResolveCanonicalDataPath(cwd, dataSubPath, outFullPath, outCapacityChars))
        {
            return true;
        }

        return false;
    }

    if (TryResolveExistingFile(rel, outFullPath, outCapacityChars))
        return true;

    {
        std::wstring candidate = std::wstring(exePath) + rel;
        if (TryResolveExistingFile(candidate, outFullPath, outCapacityChars))
            return true;
    }

    {
        std::wstring candidate = std::wstring(exePath) + L"..\\" + rel;
        if (TryResolveExistingFile(candidate, outFullPath, outCapacityChars))
            return true;
    }

    {
        wchar_t cwd[MAX_PATH] = {};
        const DWORD cwdLen = GetCurrentDirectoryW(MAX_PATH, cwd);
        if (cwdLen > 0 && cwdLen < MAX_PATH)
        {
            std::wstring base(cwd);
            if (!base.empty() && base.back() != L'\\')
                base.push_back(L'\\');
            std::wstring candidate = base + rel;
            if (TryResolveExistingFile(candidate, outFullPath, outCapacityChars))
                return true;
        }
    }

    {
        if (TryResolveExistingFile(relativePath, outFullPath, outCapacityChars))
            return true;
    }

    return false;
}
