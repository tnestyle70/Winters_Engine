#include <Windows.h>
#include <shellapi.h>

#pragma comment(lib, "shell32.lib")

//CRT Leak Check, only for debug modes!!
#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "WintersEngine.h"
#include "GameApp.h"
#include "Defines.h"
#include "Replay/ReplayPlayer.h"
#include <cstdlib>
#include <cwchar>
#include <string>

extern "C"
{
    __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

namespace
{
    constexpr uint32_t kDefaultTargetFPS = 60u;

    const wchar_t* GetCommandLineText()
    {
        const wchar_t* pCommandLine = ::GetCommandLineW();
        return pCommandLine ? pCommandLine : L"";
    }

    bool_t HasCommandLineFlag(const wchar_t* pLongFlag, const wchar_t* pShortFlag = nullptr)
    {
        const wchar_t* pCommandLine = GetCommandLineText();
        return (pLongFlag && wcsstr(pCommandLine, pLongFlag)) ||
            (pShortFlag && wcsstr(pCommandLine, pShortFlag));
    }

    const wchar_t* FindCommandLineValue(const wchar_t* pPrefix)
    {
        if (!pPrefix)
            return nullptr;

        const wchar_t* pFound = wcsstr(GetCommandLineText(), pPrefix);
        return pFound ? pFound + wcslen(pPrefix) : nullptr;
    }

    uint32_t ParseRequestedTargetFPS()
    {
        if (HasCommandLineFlag(L"--fps=0", L"/fps:0") ||
            HasCommandLineFlag(L"--uncapped", L"/uncapped"))
        {
            return 0u;
        }

        const wchar_t* pValue = FindCommandLineValue(L"--fps=");
        if (!pValue)
            pValue = FindCommandLineValue(L"/fps:");
        if (!pValue)
            return kDefaultTargetFPS;

        wchar_t* pEnd = nullptr;
        const unsigned long parsed = std::wcstoul(pValue, &pEnd, 10);
        if (pEnd == pValue)
            return kDefaultTargetFPS;

        return static_cast<uint32_t>(parsed);
    }

    bool_t ParseRequestedVSync()
    {
        if (HasCommandLineFlag(L"--vsync", L"/vsync"))
            return true;
        if (HasCommandLineFlag(L"--no-vsync", L"/no-vsync"))
            return false;
        return false;
    }

    struct RHIRequestParseResult
    {
        eEngineRHIBackend backend = eEngineRHIBackend::Auto;
        bool_t valid = true;
        const wchar_t* pSource = L"default";
        std::wstring value = L"auto";
        std::wstring error{};
    };

    std::wstring ReadEnvironmentValue(const wchar_t* pName)
    {
        if (!pName)
            return {};

        const DWORD required = ::GetEnvironmentVariableW(pName, nullptr, 0);
        if (required == 0)
            return {};

        std::wstring value(required, L'\0');
        const DWORD written = ::GetEnvironmentVariableW(
            pName,
            value.data(),
            required);
        if (written == 0 || written >= required)
            return {};

        value.resize(written);
        return value;
    }

    const wchar_t* FindRHIArgumentValue(const wchar_t* pArgument)
    {
        if (!pArgument)
            return nullptr;

        constexpr wchar_t kLongPrefix[] = L"--rhi=";
        constexpr wchar_t kShortPrefix[] = L"/rhi:";
        constexpr size_t kLongPrefixLength = (sizeof(kLongPrefix) / sizeof(wchar_t)) - 1;
        constexpr size_t kShortPrefixLength = (sizeof(kShortPrefix) / sizeof(wchar_t)) - 1;

        if (_wcsnicmp(pArgument, kLongPrefix, kLongPrefixLength) == 0)
            return pArgument + kLongPrefixLength;
        if (_wcsnicmp(pArgument, kShortPrefix, kShortPrefixLength) == 0)
            return pArgument + kShortPrefixLength;
        return nullptr;
    }

    bool_t TryParseRHIBackendName(
        const std::wstring& value,
        eEngineRHIBackend& outBackend)
    {
        if (_wcsicmp(value.c_str(), L"auto") == 0)
            outBackend = eEngineRHIBackend::Auto;
        else if (_wcsicmp(value.c_str(), L"dx11") == 0)
            outBackend = eEngineRHIBackend::DX11;
        else if (_wcsicmp(value.c_str(), L"dx12") == 0)
            outBackend = eEngineRHIBackend::DX12;
        else if (_wcsicmp(value.c_str(), L"vulkan") == 0)
            outBackend = eEngineRHIBackend::Vulkan;
        else
            return false;

        return true;
    }

    RHIRequestParseResult ParseRequestedRHIBackend()
    {
        RHIRequestParseResult result{};
        int argumentCount = 0;
        LPWSTR* ppArguments = ::CommandLineToArgvW(
            GetCommandLineText(),
            &argumentCount);
        if (!ppArguments)
        {
            result.valid = false;
            result.error = L"CommandLineToArgvW failed.";
            return result;
        }

        u32_t rhiArgumentCount = 0;
        for (int i = 1; i < argumentCount; ++i)
        {
            const wchar_t* pValue = FindRHIArgumentValue(ppArguments[i]);
            if (!pValue)
                continue;

            ++rhiArgumentCount;
            result.pSource = L"command-line";
            result.value = pValue;
        }
        ::LocalFree(ppArguments);

        if (rhiArgumentCount > 1)
        {
            result.valid = false;
            result.error = L"Specify exactly one --rhi=<value> argument.";
            return result;
        }

        if (rhiArgumentCount == 1 && result.value.empty())
        {
            result.valid = false;
            result.error = L"The --rhi value must not be empty.";
            return result;
        }

        if (rhiArgumentCount == 0)
        {
            const std::wstring environmentValue =
                ReadEnvironmentValue(L"WINTERS_RHI");
            if (!environmentValue.empty())
            {
                result.pSource = L"environment";
                result.value = environmentValue;
            }
        }

        if (!TryParseRHIBackendName(result.value, result.backend))
        {
            result.valid = false;
            result.error = L"Use auto, dx11, dx12, or vulkan.";
        }

        return result;
    }

    uint32_t ParseRunSeconds()
    {
        const wchar_t* pValue = FindCommandLineValue(L"--run-seconds=");
        if (!pValue)
            return 0u;

        wchar_t* pEnd = nullptr;
        const unsigned long parsed = std::wcstoul(pValue, &pEnd, 10);
        return (pEnd == pValue) ? 0u : static_cast<uint32_t>(parsed);
    }

	std::wstring ParseReplayIndexSmokePath()
	{
		const wchar_t* pValue = FindCommandLineValue(L"--replay-index-smoke=");
		if (!pValue || *pValue == L'\0')
			return {};

		const bool_t bQuoted = *pValue == L'"';
		if (bQuoted)
			++pValue;
		const wchar_t* pEnd = pValue;
		while (*pEnd != L'\0' &&
			(bQuoted ? *pEnd != L'"' : (*pEnd != L' ' && *pEnd != L'\t')))
		{
			++pEnd;
		}
		return std::wstring(pValue, pEnd);
	}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
#ifdef _DEBUG
    //종료 직전 미해제 힙 블록을 Output 창에 덤프
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	const std::wstring replayIndexSmokePath = ParseReplayIndexSmokePath();
	if (!replayIndexSmokePath.empty())
	{
		std::string error;
		const auto player = CReplayPlayer::LoadFromFile(replayIndexSmokePath, error);
		if (!player)
		{
			const std::string message =
				"[ReplayIndexSmoke] FAIL: " + error + "\n";
			::OutputDebugStringA(message.c_str());
			return 2;
		}
		::OutputDebugStringA("[ReplayIndexSmoke] PASS\n");
		return 0;
	}

    const RHIRequestParseResult rhiRequest = ParseRequestedRHIBackend();
    if (!rhiRequest.valid)
    {
        std::wstring message = L"Invalid RHI request: ";
        message += rhiRequest.value;
        message += L"\n";
        message += rhiRequest.error;
        ::OutputDebugStringW((L"[WintersGame] " + message + L"\n").c_str());

        if (ReadEnvironmentValue(L"WINTERS_RHI_PROBE_PATH").empty())
        {
            ::MessageBoxW(
                nullptr,
                message.c_str(),
                L"[Winters] Invalid RHI request",
                MB_OK | MB_ICONERROR);
        }
        return 2;
    }

    wchar_t rhiRequestTrace[192]{};
    swprintf_s(
        rhiRequestTrace,
        L"[WintersGame] RHI request source=%ls value=%ls\n",
        rhiRequest.pSource,
        rhiRequest.value.c_str());
    ::OutputDebugStringW(rhiRequestTrace);
    ::SetEnvironmentVariableW(
        L"WINTERS_INTERNAL_RHI_REQUEST_SOURCE",
        rhiRequest.pSource);

    CGameApp gameApp;

    EngineConfig config;
    config.windowTitle = L"LOL";
    config.rhiBackend = rhiRequest.backend;
    config.allowRHIFallback = rhiRequest.backend == eEngineRHIBackend::Auto;
    config.windowWidth = 1280;
    config.windowHeight = 720;
    config.vsync = ParseRequestedVSync();
    config.targetFPS = ParseRequestedTargetFPS();
    config.fullscreen = false;
    config.iNumScenes = static_cast<uint32_t>(eSceneID::End);
    config.runSeconds = ParseRunSeconds();
    config.profileCaptureOnExit = HasCommandLineFlag(L"--profile-capture-on-exit");

    return WintersRun(&gameApp, config);
}
