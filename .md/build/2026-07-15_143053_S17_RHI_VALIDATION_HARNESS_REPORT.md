# S17 RHI Validation Harness Report

- Date: 2026-07-15 14:33:48 +09:00
- Repo: `C:\Users\user\Desktop\Winters`
- Configuration: `Debug`
- Platform: `x64`
- Overall: `PASS`

## Steps

- `PASS` `git diff --check` exit=`0` seconds=`0.13`
- `PASS` `Client/Public and Shared concrete graphics audit` exit=`1` seconds=`0.16`
  - Notes: No matches.
- `PASS` `Focused common RHI public header audit` exit=`1` seconds=`0.02`
  - Notes: No matches.
- `PASS` `CMake/Ninja S17 targets` exit=`0` seconds=`12.86`
- `PASS` `MSBuild Winters.sln` exit=`0` seconds=`111.68`
- `PASS` `Runtime smoke` exit=`0` seconds=`50.32`

## Output Tail

### git diff --check

```text
git : warning: in the working copy of 'Shared/Schemas/Generated/cpp/Command_generated.h', LF will be replaced by CRLF t
he next time Git touches it
위치 C:\Users\user\Desktop\Winters\Tools\Harness\Run-S17RhiValidation.ps1:330 문자:44
+     Invoke-NativeStep "git diff --check" { git diff --check }
+                                            ~~~~~~~~~~~~~~~~
    + CategoryInfo          : NotSpecified: (warning: in the... Git touches it:String) [], RemoteException
    + FullyQualifiedErrorId : NativeCommandError

warning: in the working copy of 'Shared/Schemas/Generated/cpp/Event_generated.h', LF will be replaced by CRLF the next
time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/Hello_generated.h', LF will be replaced by CRLF the next
time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/Snapshot_generated.h', LF will be replaced by CRLF the ne
xt time Git touches it
```

### CMake/Ninja S17 targets

```text
[39/77] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Renderer\FogOfWarRenderer.cpp.obj
[40/77] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Renderer\RHIFxSpriteRenderer.cpp.obj
C:\Users\user\Desktop\Winters\Engine\Public\Renderer/RHIFxSpriteRenderer.h(45): warning C4251: 'CRHIFxSpriteRenderer::m_pImpl': 'std::unique_ptr<CRHIFxSpriteRenderer::Impl,std::default_delete<CRHIFxSpriteRenderer::Impl>>'에서는 'CRHIFxSpriteRenderer'의 클라이언트에서 DLL 인터페이스를 사용하도록 지정해야 함
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\memory(3347): note: 'std::unique_ptr<CRHIFxSpriteRenderer::Impl,std::default_delete<CRHIFxSpriteRenderer::Impl>>' 선언을 참조하십시오.
[41/77] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\FX\Graph\FxGraphValidator.cpp.obj
[42/77] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Renderer\PlaneRenderer.cpp.obj
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
[43/77] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Renderer\CubeRenderer.cpp.obj
[44/77] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Renderer\RHISceneRenderer.cpp.obj
[45/77] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Resource\Animator.cpp.obj
[46/77] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Resource\Animation.cpp.obj
[47/77] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\FX\FxAsset.cpp.obj
[48/77] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Renderer\PostFxPass.cpp.obj
C:\Users\user\Desktop\Winters\Engine\Public\Renderer/PostFxPass.h(17): warning C4251: 'Engine::PostFxParams::vTint': 'Vec3'에서는 'Engine::PostFxParams'의 클라이언트에서 DLL 인터페이스를 사용하도록 지정해야 함
C:\Users\user\Desktop\Winters\Engine\Include\WintersMath.h(35): note: 'Vec3' 선언을 참조하십시오.
C:\Users\user\Desktop\Winters\Engine\Public\Renderer/PostFxPass.h(57): warning C4251: 'Engine::CPostFxPass::m_pImpl': 'std::unique_ptr<Engine::CPostFxPass::Impl,std::default_delete<Engine::CPostFxPass::Impl>>'에서는 'Engine::CPostFxPass'의 클라이언트에서 DLL 인터페이스를 사용하도록 지정해야 함
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\memory(3347): note: 'std::unique_ptr<Engine::CPostFxPass::Impl,std::default_delete<Engine::CPostFxPass::Impl>>' 선언을 참조하십시오.
[49/77] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\RHI\DX11\CDX11Device.cpp.obj
[50/77] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Manager\Profiler\ProfilerOverlay.cpp.obj
[51/77] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\FX\Graph\FxGraph.cpp.obj
[52/77] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Resource\Skeleton.cpp.obj
[53/77] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Renderer\FxStaticMeshRenderer.cpp.obj
[54/77] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Resource\Texture.cpp.obj
[55/77] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Resource\ResourceCache.cpp.obj
[56/77] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Resource\Mesh.cpp.obj
[57/77] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Timer_Manager.cpp.obj
[58/77] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Sound\Sound_Manager.cpp.obj
[59/77] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\GameInstance.cpp.obj
[60/77] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Framework\CEngineApp.cpp.obj
[61/77] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Renderer\ModelRenderer.cpp.obj
C:\Users\user\Desktop\Winters\Engine\Private\RHI/DX11/BlendStateCache.h(28): warning C4251: 'CBlendStateCache::m_pStates': 'Microsoft::WRL::ComPtr<ID3D11BlendState>'에서는 'CBlendStateCache'의 클라이언트에서 DLL 인터페이스를 사용하도록 지정해야 함
C:\Program Files (x86)\Windows Kits\10\include\10.0.26100.0\winrt\wrl/client.h(210): note: 'Microsoft::WRL::ComPtr<ID3D11BlendState>' 선언을 참조하십시오.
C:\Users\user\Desktop\Winters\Engine\Public\Renderer/CMaterialPBR.h(49): warning C4251: 'Engine::CMaterialPBR::m_pConstantBuffer': 'std::unique_ptr<DX11ConstantBuffer<Engine::CBPerMaterial>,std::default_delete<DX11ConstantBuffer<Engine::CBPerMaterial>>>'에서는 'Engine::CMaterialPBR'의 클라이언트에서 DLL 인터페이스를 사용하도록 지정해야 함
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include\memory(3347): note: 'std::unique_ptr<DX11ConstantBuffer<Engine::CBPerMaterial>,std::default_delete<DX11ConstantBuffer<Engine::CBPerMaterial>>>' 선언을 참조하십시오.
[62/77] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Scene\Scene_Manager.cpp.obj
[63/77] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Manager\UI\UI_Manager.cpp.obj
[64/77] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Resource\Model.cpp.obj
[65/77] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\WintersEngine.cpp.obj
[66/77] Building CXX object EldenRingEditor\CMakeFiles\WintersEldenRingEditor.dir\Debug\Private\EldenRingEditorApp.cpp.obj
[67/77] Building CXX object EldenRingClient\CMakeFiles\WintersElden.dir\Debug\Private\EldenAssetProbeScene.cpp.obj
[68/77] Building CXX object EldenRingClient\CMakeFiles\WintersElden.dir\Debug\Private\EldenRingApp.cpp.obj
[69/77] Building CXX object EldenRingEditor\CMakeFiles\WintersEldenRingEditor.dir\Debug\Private\EldenRingEditorScene.cpp.obj
[70/77] Building CXX object EldenRingClient\CMakeFiles\WintersElden.dir\Debug\Private\EldenLimgraveShowcaseScene.cpp.obj
[71/77] Building CXX object CMakeFiles\WintersEngine.dir\Debug\Engine\Private\Manager\Navigation\Pathfinder.cpp.obj
[72/77] Linking CXX shared library C:\Users\user\Desktop\Winters\Engine\Bin\Debug\WintersEngine.dll; Deploying WintersEngine artifacts through UpdateLib.bat
파일 삭제 - C:\Users\user\Desktop\Winters\EngineSDK\inc\Core\Timer_Manager.h
파일 삭제 - C:\Users\user\Desktop\Winters\EngineSDK\inc\Manager\UI\Font_Manager.h
파일 삭제 - C:\Users\user\Desktop\Winters\EngineSDK\inc\Manager\UI\UI_Manager.h
파일 삭제 - C:\Users\user\Desktop\Winters\EngineSDK\inc\Scene\Scene_Manager.h
파일 삭제 - C:\Users\user\Desktop\Winters\EngineSDK\inc\Sound\Sound_Manager.h
0개 파일이 복사되었습니다.
0개 파일이 복사되었습니다.
C:\Users\user\Desktop\Winters\Engine\Bin\Debug\WintersEngine.lib
1개 파일이 복사되었습니다.
0개 파일이 복사되었습니다.
C:\Users\user\Desktop\Winters\Engine\Bin\Debug\WintersEngine.dll
1개 파일이 복사되었습니다.
C:\Users\user\Desktop\Winters\Engine\Bin\Debug\WintersEngine.pdb
1개 파일이 복사되었습니다.
0개 파일이 복사되었습니다.
0개 파일이 복사되었습니다.
0개 파일이 복사되었습니다.
0개 파일이 복사되었습니다.
0개 파일이 복사되었습니다.
0개 파일이 복사되었습니다.
0개 파일이 복사되었습니다.
0개 파일이 복사되었습니다.
[73/77] Linking CXX executable C:\Users\user\Desktop\Winters\EldenRingEditor\Bin\Debug\WintersEldenRingEditor.exe; Copy WintersEngine runtime DLL to EldenRingEditor output; Copy third-party runtime DLLs to EldenRingEditor output; Copy shaders to EldenRingEditor output
[74/77] Linking CXX executable C:\Users\user\Desktop\Winters\EldenRingClient\Bin\Debug\WintersElden.exe; Copy WintersEngine runtime DLL to EldenRingClient output; Copy third-party runtime DLLs to EldenRingClient output; Copy shaders to EldenRingClient output
```

### MSBuild Winters.sln

```text
cmd.exe : 'vswhere.exe'은(는) 내부 또는 외부 명령, 실행할 수 있는 프로그램, 또는
위치 C:\Users\user\Desktop\Winters\Tools\Harness\Run-S17RhiValidation.ps1:109 문자:20
+             $raw = & cmd.exe /c $CommandLine 2>&1
+                    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    + CategoryInfo          : NotSpecified: ('vswhere.exe'은(...할 수 있는 프로그램, 또는:String) [], RemoteException
    + FullyQualifiedErrorId : NativeCommandError

배치 파일이 아닙니다.
msbuild 踰꾩쟾 17.14.40+3e7442088(.NET Framework??

鍮뚮뱶?덉뒿?덈떎.
    寃쎄퀬 358媛?
    ?ㅻ쪟 0媛?

寃쎄낵 ?쒓컙: 00:01:50.74
```

### Runtime smoke

```text

Name                       AliveAfterSeconds ExitCode Cleanup
----                       ----------------- -------- -------
WintersElden_probe_dx12                 True          killed
WintersElden_probe_dx11                 True          killed
WintersEldenRingEditor                  True          killed
WintersGame                             True          killed
WintersGame_rhi_scene_only              True          killed
```
