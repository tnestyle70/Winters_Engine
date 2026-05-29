Session - DX12 Vulkan parity boundary

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Client/Private/main.cpp

기존 코드:

```cpp
        if (wcsstr(pCommandLine, L"--rhi=dx11") || wcsstr(pCommandLine, L"/rhi:dx11"))
            return eEngineRHIBackend::DX11;
        if (wcsstr(pCommandLine, L"--rhi=dx12") || wcsstr(pCommandLine, L"/rhi:dx12"))
            return eEngineRHIBackend::DX12;
        if (wcsstr(pCommandLine, L"--rhi=null") || wcsstr(pCommandLine, L"/rhi:null"))
            return eEngineRHIBackend::Null;
```

아래로 교체:

```cpp
        if (wcsstr(pCommandLine, L"--rhi=dx11") || wcsstr(pCommandLine, L"/rhi:dx11"))
            return eEngineRHIBackend::DX11;
        if (wcsstr(pCommandLine, L"--rhi=dx12") || wcsstr(pCommandLine, L"/rhi:dx12"))
            return eEngineRHIBackend::DX12;
        if (wcsstr(pCommandLine, L"--rhi=vulkan") || wcsstr(pCommandLine, L"/rhi:vulkan"))
            return eEngineRHIBackend::Vulkan;
        if (wcsstr(pCommandLine, L"--rhi=auto") || wcsstr(pCommandLine, L"/rhi:auto"))
            return eEngineRHIBackend::Auto;
        if (wcsstr(pCommandLine, L"--rhi=null") || wcsstr(pCommandLine, L"/rhi:null"))
            return eEngineRHIBackend::Null;
```

1-2. C:/Users/user/Desktop/Winters/Engine/Private/Framework/CEngineApp.cpp

기존 코드:

```cpp
    switch (config.rhiBackend)
    {
    case eEngineRHIBackend::DX11:
        tryDX11();
        break;
    case eEngineRHIBackend::DX12:
        tryDX12();
        break;
    case eEngineRHIBackend::Auto:
        if (!tryDX12())
            tryDX11();
        break;
    default:
        OutputDebugStringA("[CEngineApp] Requested RHI backend is not implemented on this platform\n");
        break;
    }
```

아래로 교체:

```cpp
    switch (config.rhiBackend)
    {
    case eEngineRHIBackend::DX11:
        tryDX11();
        break;
    case eEngineRHIBackend::DX12:
        tryDX12();
        break;
    case eEngineRHIBackend::Auto:
        if (!tryDX12())
            tryDX11();
        break;
    case eEngineRHIBackend::Vulkan:
        OutputDebugStringA("[CEngineApp] Vulkan requested but no Vulkan RHI backend is implemented yet\n");
        break;
    case eEngineRHIBackend::Null:
        OutputDebugStringA("[CEngineApp] Null RHI requested but no Null windowed renderer is implemented yet\n");
        break;
    default:
        OutputDebugStringA("[CEngineApp] Requested RHI backend is not implemented on this platform\n");
        break;
    }
```

2. 검증

미검증.

- `git diff --check -- Client/Private/main.cpp Engine/Private/Framework/CEngineApp.cpp`
- `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Engine/Include/Engine.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`
- `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`
- `WintersGame.exe --rhi=dx11`은 기존 DX11 경로로 뜨는지 확인한다.
- `WintersGame.exe --rhi=dx12`는 현재 빌드/런타임 상태에 맞게 DX12 또는 DX11 fallback 로그가 명확히 남는지 확인한다.
- `WintersGame.exe --rhi=vulkan`은 Vulkan이 구현된 것처럼 보이면 실패다. 지금 목표는 명확한 fallback boundary다.
