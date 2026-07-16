#include "World/LoLStageDocument.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{
	bool FileExistsFile(const wchar_t* pPath)
	{
		const DWORD attr = ::GetFileAttributesW(pPath);
		return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
	}

	bool DirectoryExists(const wchar_t* pPath)
	{
		const DWORD attr = ::GetFileAttributesW(pPath);
		return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
	}

	void EnsureTrailingSlash(std::wstring& path)
	{
		if (!path.empty() && path.back() != L'\\' && path.back() != L'/')
			path += L'\\';
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
		const DWORD n = ::GetModuleFileNameW(nullptr, exePath, MAX_PATH);
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
		const DWORD cwdLen = ::GetCurrentDirectoryW(MAX_PATH, cwd);
		if (cwdLen > 0 && cwdLen < MAX_PATH)
			return TryFindWorkspaceDataRootFrom(cwd, outRoot);

		return false;
	}

	bool ReadFileBytes(const wchar_t* pPath, std::vector<unsigned char>& outBytes)
	{
		FILE* pFile = nullptr;
		if (_wfopen_s(&pFile, pPath, L"rb") != 0 || !pFile)
			return false;

		std::fseek(pFile, 0, SEEK_END);
		const long size = std::ftell(pFile);
		std::fseek(pFile, 0, SEEK_SET);

		outBytes.resize(size > 0 ? static_cast<size_t>(size) : 0);
		const bool bOk = outBytes.empty() ||
			std::fread(outBytes.data(), 1, outBytes.size(), pFile) == outBytes.size();

		std::fclose(pFile);
		return bOk;
	}
}

bool_t CLoLStageDocument::ResolveStagePath(i32_t stageIndex, wchar_t* pOutBuf, u32_t capacity)
{
	if (!pOutBuf || capacity < MAX_PATH)
		return false;

	std::wstring dataRoot;
	if (!TryFindWorkspaceDataRoot(dataRoot))
		return false;

	return swprintf_s(pOutBuf, capacity, L"%sStage%d.dat",
		dataRoot.c_str(), static_cast<int>(stageIndex)) > 0;
}

bool_t CLoLStageDocument::ResolveNavGridPath(i32_t stageIndex, wchar_t* pOutBuf, u32_t capacity)
{
	if (!ResolveStagePath(stageIndex, pOutBuf, capacity))
		return false;

	wchar_t* const dot = wcsrchr(pOutBuf, L'.');
	if (!dot)
		return false;

	return wcscpy_s(dot, capacity - static_cast<u32_t>(dot - pOutBuf), L".navgrid") == 0;
}

bool_t CLoLStageDocument::LoadStage(i32_t stageIndex)
{
	wchar_t stagePath[MAX_PATH] = {};
	if (!ResolveStagePath(stageIndex, stagePath, MAX_PATH))
		return false;

	if (!Winters::Map::LoadStageDataFromFile(stagePath, m_data))
		return false;

	m_iStageIndex = stageIndex;
	m_bLoaded = true;
	m_bDirty = false;
	return true;
}

bool_t CLoLStageDocument::SaveStage()
{
	wchar_t stagePath[MAX_PATH] = {};
	if (!ResolveStagePath(m_iStageIndex, stagePath, MAX_PATH))
		return false;

	if (!Winters::Map::SaveStageDataToFile(stagePath, m_data))
		return false;

	m_bLoaded = true;
	m_bDirty = false;
	return true;
}

void CLoLStageDocument::NewStage(i32_t stageIndex)
{
	m_data.Clear();
	m_data.header.magic = Winters::Map::STAGE_MAGIC;
	m_data.header.version = Winters::Map::STAGE_VERSION;
	m_iStageIndex = stageIndex;
	m_bLoaded = true;
	m_bDirty = true;
}

bool_t CLoLStageDocument::VerifyRoundtrip(std::wstring& outMessage) const
{
	wchar_t stagePath[MAX_PATH] = {};
	if (!ResolveStagePath(m_iStageIndex, stagePath, MAX_PATH))
	{
		outMessage = L"stage path resolve failed";
		return false;
	}

	std::vector<unsigned char> original;
	if (!ReadFileBytes(stagePath, original))
	{
		outMessage = L"original read failed";
		return false;
	}

	std::wstring tmpPath = stagePath;
	tmpPath += L".roundtrip.tmp";
	if (!Winters::Map::SaveStageDataToFile(tmpPath.c_str(), m_data))
	{
		outMessage = L"tmp save failed";
		::DeleteFileW(tmpPath.c_str());
		return false;
	}

	std::vector<unsigned char> rewritten;
	const bool bRead = ReadFileBytes(tmpPath.c_str(), rewritten);
	::DeleteFileW(tmpPath.c_str());
	if (!bRead)
	{
		outMessage = L"tmp read failed";
		return false;
	}

	if (original.size() != rewritten.size() ||
		(!original.empty() && std::memcmp(original.data(), rewritten.data(), original.size()) != 0))
	{
		wchar_t msg[128] = {};
		swprintf_s(msg, L"MISMATCH (original=%zu bytes, rewritten=%zu bytes)",
			original.size(), rewritten.size());
		outMessage = msg;
		return false;
	}

	wchar_t msg[64] = {};
	swprintf_s(msg, L"OK (%zu bytes)", original.size());
	outMessage = msg;
	return true;
}
