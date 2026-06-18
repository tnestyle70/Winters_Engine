Session - 프레임 페이싱과 프로파일러 기준선 고정

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Client/Private/main.cpp

기존 코드:
```cpp
#include "WintersEngine.h"
#include "GameApp.h"
#include "Defines.h"
#include <cwchar>
```

아래에 추가:
```cpp
#include <cstdlib>
```

기존 코드:
```cpp
namespace
{
    eEngineRHIBackend ParseRequestedRHIBackend()
    {
        const wchar_t* pCommandLine = ::GetCommandLineW();
        if (!pCommandLine)
            return eEngineRHIBackend::DX11;

        if (wcsstr(pCommandLine, L"--rhi=dx11") || wcsstr(pCommandLine, L"/rhi:dx11"))
            return eEngineRHIBackend::DX11;
        if (wcsstr(pCommandLine, L"--rhi=dx12") || wcsstr(pCommandLine, L"/rhi:dx12"))
            return eEngineRHIBackend::DX12;
        if (wcsstr(pCommandLine, L"--rhi=null") || wcsstr(pCommandLine, L"/rhi:null"))
            return eEngineRHIBackend::Null;

        return eEngineRHIBackend::DX11;
    }
}
```

아래로 교체:
```cpp
namespace
{
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
            return 0u;

        wchar_t* pEnd = nullptr;
        const unsigned long parsed = std::wcstoul(pValue, &pEnd, 10);
        if (pEnd == pValue)
            return 0u;

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

    eEngineRHIBackend ParseRequestedRHIBackend()
    {
        if (HasCommandLineFlag(L"--rhi=dx11", L"/rhi:dx11"))
            return eEngineRHIBackend::DX11;
        if (HasCommandLineFlag(L"--rhi=dx12", L"/rhi:dx12"))
            return eEngineRHIBackend::DX12;
        if (HasCommandLineFlag(L"--rhi=null", L"/rhi:null"))
            return eEngineRHIBackend::Null;

        return eEngineRHIBackend::DX11;
    }
}
```

기존 코드:
```cpp
    config.vsync = false;
    config.targetFPS = 0;
```

아래로 교체:
```cpp
    config.vsync = ParseRequestedVSync();
    config.targetFPS = ParseRequestedTargetFPS();
```

1-2. C:/Users/tnest/Desktop/Winters/Engine/Private/Framework/CEngineApp.cpp

기존 코드:
```cpp
        {
            WINTERS_PROFILE_SCOPE("Render");
            Render();
        }

        CGameInstance::Get()->Profiler_End();
```

아래에 추가:
```cpp
        WINTERS_PROFILE_COUNT("Frame::LimiterActive", bLimitFrameRate ? 1u : 0u);
        WINTERS_PROFILE_COUNT("Frame::TargetFPS", m_uTargetFPS);
```

1-3. C:/Users/tnest/Desktop/Winters/Engine/Include/EngineConfig.h

기존 코드:
```cpp
    bool     vsync        = true;    // VSync ?쒖꽦???щ?
    uint32   targetFPS    = 60;      // ?뚮뜑 ?꾨젅???곹븳. 0?대㈃ ?쒗븳 ?놁쓬
```

아래로 교체:
```cpp
    bool     vsync        = true;    // Present VSync 사용 여부
    uint32   targetFPS    = 60;      // CPU frame limiter. 0이면 제한 없음
```

2. 검증

```text
빌드:
- git diff --check
- msbuild Engine/Include/Engine.vcxproj /p:Configuration=Debug /p:Platform=x64
- msbuild Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64

런타임:
- Client 실행 인자 없음: 기본 uncapped/no-vsync 확인
- Client --fps=60: Frame::LimiterActive=1, Frame::TargetFPS=60 확인
- Client --uncapped --no-vsync: Frame::LimiterActive=0 확인
- Client --vsync --fps=0: Present VSync 경로 확인

프로파일러 기준선:
- Profiler Overlay 닫힘/열림 각각 10초 Freeze 캡처
- Frame 평균, p95 대체 지표로 Stable rows의 Avg/Max 기록
- 다음 세션부터 같은 실행 인자로만 비교한다.
```
