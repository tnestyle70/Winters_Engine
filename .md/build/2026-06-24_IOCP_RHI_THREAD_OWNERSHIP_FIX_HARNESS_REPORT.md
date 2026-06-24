# S17 RHI Validation Harness Report

- Date: 2026-06-24 05:47:05 +09:00
- Repo: `C:\Users\user\Desktop\Winters`
- Configuration: `Debug`
- Platform: `x64`
- Overall: `FAIL`
- Failure: `CMake/Ninja S17 targets failed with exit code -1`

## Steps

- `PASS` `git diff --check` exit=`0` seconds=`0.15`
- `PASS` `Client/Public and Shared concrete graphics audit` exit=`1` seconds=`0.06`
  - Notes: No matches.
- `PASS` `Focused common RHI public header audit` exit=`1` seconds=`0.02`
  - Notes: No matches.
- `FAIL` `CMake/Ninja S17 targets` exit=`-1` seconds=`19.13`

## Output Tail

### git diff --check

```text
git : warning: in the working copy of '.md/EldenRing/02_ASSET_EXTRACTION_TO_WINTERS_BINARY_PIPELINE.md', LF will be rep
laced by CRLF the next time Git touches it
At C:\Users\user\Desktop\Winters\Tools\Harness\Run-S17RhiValidation.ps1:329 char:44
+     Invoke-NativeStep "git diff --check" { git diff --check }
+                                            ~~~~~~~~~~~~~~~~
    + CategoryInfo          : NotSpecified: (warning: in the... Git touches it:String) [], RemoteException
    + FullyQualifiedErrorId : NativeCommandError

warning: in the working copy of '.md/EldenRing/10_ASSET_PIPELINE_TOOLING.md', LF will be replaced by CRLF the next time
 Git touches it
warning: in the working copy of '.md/collab/ACTIVE_WORK_PACKETS.md', LF will be replaced by CRLF the next time Git touc
hes it
warning: in the working copy of '.md/collab/GIT_SYNC_RULES.md', LF will be replaced by CRLF the next time Git touches i
t
warning: in the working copy of '.md/collab/HARNESS_RULES.md', LF will be replaced by CRLF the next time Git touches it
warning: in the working copy of '.md/collab/OWNERSHIP_MATRIX.md', LF will be replaced by CRLF the next time Git touches
 it
warning: in the working copy of 'Client/Private/GameObject/FX/FxLegacyManifest.cpp', LF will be replaced by CRLF the ne
xt time Git touches it
warning: in the working copy of 'Client/Private/Scene/Loader.cpp', LF will be replaced by CRLF the next time Git touche
s it
warning: in the working copy of 'Client/Private/Scene/Scene_Loading.cpp', LF will be replaced by CRLF the next time Git
 touches it
warning: in the working copy of 'Client/Private/Scene/Scene_MatchLoading.cpp', LF will be replaced by CRLF the next tim
e Git touches it
warning: in the working copy of 'Client/Private/UI/EffectTuner.cpp', LF will be replaced by CRLF the next time Git touc
hes it
warning: in the working copy of 'Client/Public/Scene/Loader.h', LF will be replaced by CRLF the next time Git touches i
t
warning: in the working copy of 'Server/Private/Network/IOCPCore.cpp', LF will be replaced by CRLF the next time Git to
uches it
warning: in the working copy of 'Server/Private/Network/Session_Manager.cpp', LF will be replaced by CRLF the next time
 Git touches it
warning: in the working copy of 'Server/Public/Network/Session_Manager.h', LF will be replaced by CRLF the next time Gi
t touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/Command_generated.h', LF will be replaced by CRLF the nex
t time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/Event_generated.h', LF will be replaced by CRLF the next
time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/LobbyTypes_generated.h', LF will be replaced by CRLF the
next time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/Snapshot_generated.h', LF will be replaced by CRLF the ne
xt time Git touches it
warning: in the working copy of 'Tools/EldenAssetPipeline/elden_pipeline.py', LF will be replaced by CRLF the next time
 Git touches it
warning: in the working copy of 'Tools/External/LeagueToolkitProbe/Program.cs', LF will be replaced by CRLF the next ti
me Git touches it
```

### CMake/Ninja S17 targets

```text
C:\Users\user\Desktop\Winters\Engine\Include\WintersMath.h(35): note: 'Vec3' 선언을 참조하십시오.
C:\Users\user\Desktop\Winters\Engine\Public\Renderer/CCamera.h(54): warning C4251: 'CCamera::m_ViewMatrix': 'Mat4'에서는 'CCamera'의 클라이언트에서 DLL 인터페이스를 사용하도록 지정해야 함
C:\Users\user\Desktop\Winters\Engine\Include\WintersMath.h(247): note: 'Mat4' 선언을 참조하십시오.
C:\Users\user\Desktop\Winters\Engine\Public\Renderer/CCamera.h(55): warning C4251: 'CCamera::m_ProjMatrix': 'Mat4'에서는 'CCamera'의 클라이언트에서 DLL 인터페이스를 사용하도록 지정해야 함
C:\Users\user\Desktop\Winters\Engine\Include\WintersMath.h(247): note: 'Mat4' 선언을 참조하십시오.
[116/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\RHI\DX11\DX11Buffer.cpp.obj
[117/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\RHI\DX11\DX11Shader.cpp.obj
[118/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\RHI\RHIShaderCompiler.cpp.obj
[119/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\RHI\DX11\SamplerStateCache.cpp.obj
[120/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\RHI\DX11\DX11Pipeline.cpp.obj
[121/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\ECS\Systems\TurretAISystem.cpp.obj
C:\Users\user\Desktop\Winters\Engine\Public\ECS/Systems/TurretAISystem.h(15): warning C4275: DLL 인터페이스가 아닌 class 'ISystem'이(가) DLL 인터페이스 class 'Engine::CTurretAISystem'의 기본으로 사용되었습니다.
C:\Users\user\Desktop\Winters\Engine\Public\ECS/ISystem.h(9): note: 'ISystem' 선언을 참조하십시오.
C:\Users\user\Desktop\Winters\Engine\Public\ECS/Systems/TurretAISystem.h(15): note: 'Engine::CTurretAISystem' 선언을 참조하십시오.
[122/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\FX\Graph\FxGraph.cpp.obj
[123/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\GameInstance.cpp.obj
[124/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Framework\CEngineApp.cpp.obj
[125/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Renderer\CMaterialPBR.cpp.obj
C:\Users\user\Desktop\Winters\Engine\Public\Renderer/CMaterialPBR.h(49): warning C4251: 'Engine::CMaterialPBR::m_pConstantBuffer': 'std::unique_ptr<DX11ConstantBuffer<Engine::CBPerMaterial>,std::default_delete<DX11ConstantBuffer<Engine::CBPerMaterial>>>'에서는 'Engine::CMaterialPBR'의 클라이언트에서 DLL 인터페이스를 사용하도록 지정해야 함
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\memory(3347): note: 'std::unique_ptr<DX11ConstantBuffer<Engine::CBPerMaterial>,std::default_delete<DX11ConstantBuffer<Engine::CBPerMaterial>>>' 선언을 참조하십시오.
[126/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\RHI\RHITextureLoader.cpp.obj
[127/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Renderer\NormalPass.cpp.obj
C:\Users\user\Desktop\Winters\Engine\Public\Renderer/NormalPass.h(40): warning C4251: 'Engine::CNormalPass::m_pImpl': 'std::unique_ptr<Engine::CNormalPass::Impl,std::default_delete<Engine::CNormalPass::Impl>>'에서는 'Engine::CNormalPass'의 클라이언트에서 DLL 인터페이스를 사용하도록 지정해야 함
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\memory(3347): note: 'std::unique_ptr<Engine::CNormalPass::Impl,std::default_delete<Engine::CNormalPass::Impl>>' 선언을 참조하십시오.
[128/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Renderer\FogOfWarRenderer.cpp.obj
[129/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Renderer\RHIFxSpriteRenderer.cpp.obj
C:\Users\user\Desktop\Winters\Engine\Public\Renderer/RHIFxSpriteRenderer.h(45): warning C4251: 'CRHIFxSpriteRenderer::m_pImpl': 'std::unique_ptr<CRHIFxSpriteRenderer::Impl,std::default_delete<CRHIFxSpriteRenderer::Impl>>'에서는 'CRHIFxSpriteRenderer'의 클라이언트에서 DLL 인터페이스를 사용하도록 지정해야 함
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\memory(3347): note: 'std::unique_ptr<CRHIFxSpriteRenderer::Impl,std::default_delete<CRHIFxSpriteRenderer::Impl>>' 선언을 참조하십시오.
[130/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Renderer\RHIMeshResource.cpp.obj
[131/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Renderer\CubeRenderer.cpp.obj
[132/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Renderer\RHIMaterialResource.cpp.obj
[133/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Renderer\PlaneRenderer.cpp.obj
C:\Users\user\Desktop\Winters\Engine\Public\Renderer/PlaneRenderer.h(58): warning C4251: 'CPlaneRenderer::m_pImpl': 'std::unique_ptr<CPlaneRenderer::Impl,std::default_delete<CPlaneRenderer::Impl>>'에서는 'CPlaneRenderer'의 클라이언트에서 DLL 인터페이스를 사용하도록 지정해야 함
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\memory(3347): note: 'std::unique_ptr<CPlaneRenderer::Impl,std::default_delete<CPlaneRenderer::Impl>>' 선언을 참조하십시오.
C:\Users\user\Desktop\Winters\Engine\Public\Renderer/PlaneRenderer.h(65): warning C4251: 'CPlaneRenderer::m_vFxTint': 'Vec4'에서는 'CPlaneRenderer'의 클라이언트에서 DLL 인터페이스를 사용하도록 지정해야 함
C:\Users\user\Desktop\Winters\Engine\Include\WintersMath.h(236): note: 'Vec4' 선언을 참조하십시오.
C:\Users\user\Desktop\Winters\Engine\Public\Renderer/PlaneRenderer.h(66): warning C4251: 'CPlaneRenderer::m_vFxUVRect': 'Vec4'에서는 'CPlaneRenderer'의 클라이언트에서 DLL 인터페이스를 사용하도록 지정해야 함
C:\Users\user\Desktop\Winters\Engine\Include\WintersMath.h(236): note: 'Vec4' 선언을 참조하십시오.
C:\Users\user\Desktop\Winters\Engine\Public\Renderer/PlaneRenderer.h(67): warning C4251: 'CPlaneRenderer::m_vFxUVScroll': 'Vec2'에서는 'CPlaneRenderer'의 클라이언트에서 DLL 인터페이스를 사용하도록 지정해야 함
C:\Users\user\Desktop\Winters\Engine\Include\WintersMath.h(22): note: 'Vec2' 선언을 참조하십시오.
C:\Users\user\Desktop\Winters\Engine\Public\Renderer/PlaneRenderer.h(71): warning C4251: 'CPlaneRenderer::m_World': 'Mat4'에서는 'CPlaneRenderer'의 클라이언트에서 DLL 인터페이스를 사용하도록 지정해야 함
C:\Users\user\Desktop\Winters\Engine\Include\WintersMath.h(247): note: 'Mat4' 선언을 참조하십시오.
C:\Users\user\Desktop\Winters\Engine\Private\RHI/DX11/BlendStateCache.h(28): warning C4251: 'CBlendStateCache::m_pStates': 'Microsoft::WRL::ComPtr<ID3D11BlendState>'에서는 'CBlendStateCache'의 클라이언트에서 DLL 인터페이스를 사용하도록 지정해야 함
C:\Program Files (x86)\Windows Kits\10\include\10.0.26100.0\winrt\wrl/client.h(210): note: 'Microsoft::WRL::ComPtr<ID3D11BlendState>' 선언을 참조하십시오.
[134/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Resource\Animator.cpp.obj
[135/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Resource\Bone.cpp.obj
[136/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\RHI\DX11\CDX11Device.cpp.obj
[137/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Renderer\RHISceneRenderer.cpp.obj
[138/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Resource\Animation.cpp.obj
[139/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Renderer\RHIFxMeshResource.cpp.obj
[140/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Renderer\SSAOPass.cpp.obj
C:\Users\user\Desktop\Winters\Engine\Public\Renderer/SSAOPass.h(49): warning C4251: 'Engine::CSSAOPass::m_pImpl': 'std::unique_ptr<Engine::CSSAOPass::Impl,std::default_delete<Engine::CSSAOPass::Impl>>'에서는 'Engine::CSSAOPass'의 클라이언트에서 DLL 인터페이스를 사용하도록 지정해야 함
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\memory(3347): note: 'std::unique_ptr<Engine::CSSAOPass::Impl,std::default_delete<Engine::CSSAOPass::Impl>>' 선언을 참조하십시오.
[141/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Renderer\UIRenderer.cpp.obj
[142/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Scripting\LuaRuntime.cpp.obj
[143/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Resource\Mesh.cpp.obj
[144/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Renderer\FxStaticMeshRenderer.cpp.obj
[145/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Resource\Skeleton.cpp.obj
[146/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\RHI\DX12\DX12Device.cpp.obj
[147/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Resource\Texture.cpp.obj
[148/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Timer_Manager.cpp.obj
[149/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Sound\Sound_Manager.cpp.obj
[150/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Scene\Scene_Manager.cpp.obj
[151/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Renderer\ModelRenderer.cpp.obj
C:\Users\user\Desktop\Winters\Engine\Public\Renderer/CMaterialPBR.h(49): warning C4251: 'Engine::CMaterialPBR::m_pConstantBuffer': 'std::unique_ptr<DX11ConstantBuffer<Engine::CBPerMaterial>,std::default_delete<DX11ConstantBuffer<Engine::CBPerMaterial>>>'에서는 'Engine::CMaterialPBR'의 클라이언트에서 DLL 인터페이스를 사용하도록 지정해야 함
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\memory(3347): note: 'std::unique_ptr<DX11ConstantBuffer<Engine::CBPerMaterial>,std::default_delete<DX11ConstantBuffer<Engine::CBPerMaterial>>>' 선언을 참조하십시오.
[152/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Resource\ResourceCache.cpp.obj
[153/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\World\AssetStreamingSystem.cpp.obj
[154/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\WintersEngine.cpp.obj
[155/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Resource\Model.cpp.obj
[156/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Manager\UI\UI_Manager.cpp.obj
[157/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\World\WorldPartitionSystem.cpp.obj
[158/164] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Manager\Navigation\Pathfinder.cpp.obj
[159/164] Linking CXX shared library C:\Users\user\Desktop\Winters\Engine\Bin\Debug\WintersEngine.dll; Deploying WintersEngine artifacts through UpdateLib.bat
FAILED: [code=4294967295] C:/Users/user/Desktop/Winters/Engine/Bin/Debug/WintersEngine.dll C:/Users/user/Desktop/Winters/Engine/Bin/Debug/WintersEngine.lib
C:\Windows\system32\cmd.exe /C "cd . && "C:\Program Files\CMake\bin\cmake.exe" -E vs_link_dll --msvc-ver=1944 --intdir=CMakeFiles\WintersEngine.dir\Debug --rc=C:\PROGRA~2\WI3CF2~1\10\bin\100261~1.0\x64\rc.exe --mt=C:\PROGRA~2\WI3CF2~1\10\bin\100261~1.0\x64\mt.exe --manifests  -- C:\PROGRA~1\MICROS~2\2022\COMMUN~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\link.exe /nologo @CMakeFiles\WintersEngine.Debug.rsp  /out:C:\Users\user\Desktop\Winters\Engine\Bin\Debug\WintersEngine.dll /implib:C:\Users\user\Desktop\Winters\Engine\Bin\Debug\WintersEngine.lib /pdb:C:\Users\user\Desktop\Winters\Engine\Bin\Debug\WintersEngine.pdb /dll /version:0.0 /machine:x64 /debug /INCREMENTAL /DEBUG && C:\Windows\system32\cmd.exe /C "cd /D C:\Users\user\Desktop\Winters && C:\Users\user\Desktop\Winters\UpdateLib.bat""
LINK Pass 1: command "C:\PROGRA~1\MICROS~2\2022\COMMUN~1\VC\Tools\MSVC\1444~1.352\bin\Hostx64\x64\link.exe /nologo @CMakeFiles\WintersEngine.Debug.rsp /out:C:\Users\user\Desktop\Winters\Engine\Bin\Debug\WintersEngine.dll /implib:C:\Users\user\Desktop\Winters\Engine\Bin\Debug\WintersEngine.lib /pdb:C:\Users\user\Desktop\Winters\Engine\Bin\Debug\WintersEngine.pdb /dll /version:0.0 /machine:x64 /debug /INCREMENTAL /DEBUG /MANIFEST /MANIFESTFILE:CMakeFiles\WintersEngine.dir\Debug/intermediate.manifest CMakeFiles\WintersEngine.dir\Debug/manifest.res" failed (exit code 1201) with the following output:
LINK : fatal error LNK1201: 'C:\Users\user\Desktop\Winters\Engine\Bin\Debug\WintersEngine.pdb' 프로그램 데이터베이스를 쓰는 동안 오류가 발생했습니다. 디스크 공간이 부족한지, 경로가 잘못되었는지 또는 권한이 없는지 확인하십시오.

ninja: build stopped: subcommand failed.
```

