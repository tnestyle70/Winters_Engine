Session - Elden Ring Client DX12 rendering pipeline을 큐브 검증에서 GI/RayTracing까지 세션 단위로 고정한다.

1. 반영해야 하는 코드

1-1. 문서 기준

이 문서는 EldenRingClient의 DX12 렌더링 확장 순서를 고정한다. 목표는 작은 검증물을 단계적으로 통과시키면서, 최종적으로 추출된 Elden Ring 메시와 월드 장면을 PBR/Forward+/TAA/Volumetric Fog/GI/DXR 경로에 연결하는 것이다.

공통 원칙:
- EldenRingClient는 `ID3D12*`, `d3d12.h`, DX12 native handle을 직접 사용하지 않는다.
- Client는 `IRHIDevice`, `IRHICommandList`, `RHI*Handle`, `RHIPipelineDesc`, `RHIBufferDesc` 같은 공개 RHI API만 사용한다.
- DX12 구현 세부사항은 `C:/Users/tnest/Desktop/Winters/Engine/Private/RHI/DX12` 아래에 둔다.
- Engine public RHI 확장이 필요하면 `C:/Users/tnest/Desktop/Winters/Engine/Public/RHI`를 먼저 고치고, `UpdateLib.bat`로 `EngineSDK/inc` 동기화를 검증한다.
- 각 세션은 visual smoke test를 하나씩 가져야 한다. 화면에 보이는 검증물이 없는 기능은 다음 단계로 넘기지 않는다.
- Elden Ring 원본/추출 에셋은 연구용 로컬 경로에서 읽고, 원본 게임 에셋 자체를 저장소에 추가하지 않는다. 저장소에는 loader/export 문서, 변환 규칙, 테스트용 placeholder만 둔다.

1-2. Session 00 - DX12 RHI Cube

목표:
- 현재 DX12 RHI 최소 그래픽 경로가 실제 화면 출력까지 가능한지 확인한다.
- 검증 대상은 `VertexBuffer`, `IndexBuffer`, `Shader`, `PipelineState`, `DrawIndexed`, `UpdateBuffer`다.

주요 반영 파일:
- `C:/Users/tnest/Desktop/Winters/EldenRingClient/Private/EldenRingRHITestCubeRenderer.cpp`
- `C:/Users/tnest/Desktop/Winters/EldenRingClient/Public/EldenRingRHITestCubeRenderer.h`
- `C:/Users/tnest/Desktop/Winters/EldenRingClient/Private/EldenAssetProbeScene.cpp`
- `C:/Users/tnest/Desktop/Winters/Engine/Private/RHI/DX12/DX12Device.cpp`
- `C:/Users/tnest/Desktop/Winters/Engine/Public/RHI/IRHICommandList.h`
- `C:/Users/tnest/Desktop/Winters/Engine/Public/RHI/IRHIDevice.h`

완료 기준:
- `WintersElden.exe --rhi=dx12` 실행 시 컬러 큐브가 회전한다.
- EldenRingClient 코드 검색에서 `ID3D12`, `d3d12`, `DX12CommandList` 직접 사용이 없어야 한다.
- Debug Output에 `RHI test cube ready`와 `RHI cube scene ready`가 찍힌다.

1-3. Session 01 - Depth + Constant Buffer

목표:
- CPU 클립 공간 큐브를 정식 3D 렌더링 큐브로 바꾼다.
- Depth buffer와 constant buffer를 DX12 RHI에 정식으로 연결한다.

반영 범위:
- RHI texture 생성 API에 depth-stencil usage, DSV format, clear depth를 연결한다.
- DX12 backend에 DSV heap, depth texture 생성, frame begin clear, resize 재생성을 추가한다.
- RHI bind group 또는 root constant/CBV 경로를 최소 구현한다.
- EldenRingClient 큐브 렌더러는 `World`, `View`, `Projection` constant buffer를 사용한다.
- Camera는 우선 고정 orbit camera로 시작한다.

주요 반영 파일:
- `C:/Users/tnest/Desktop/Winters/Engine/Public/RHI/RHIDescriptors.h`
- `C:/Users/tnest/Desktop/Winters/Engine/Public/RHI/IRHIDevice.h`
- `C:/Users/tnest/Desktop/Winters/Engine/Public/RHI/IRHICommandList.h`
- `C:/Users/tnest/Desktop/Winters/Engine/Private/RHI/DX12/DX12Device.cpp`
- `C:/Users/tnest/Desktop/Winters/EldenRingClient/Private/EldenRingRHITestCubeRenderer.cpp`

완료 기준:
- 큐브가 원근 카메라로 보이고, 뒤쪽 면이 앞쪽 면을 뚫고 나오지 않는다.
- window resize 후에도 depth target과 viewport/scissor가 정상 갱신된다.
- RenderDoc 또는 PIX 캡처에서 DSV clear와 depth test가 확인된다.

1-4. Session 02 - Textured Lit Cube

목표:
- RHI texture, SRV, sampler, material binding의 최소 경로를 만든다.
- 컬러만 쓰던 큐브를 texture + directional light 큐브로 바꾼다.

반영 범위:
- RHI texture handle 생성/파괴, native resource 조회, SRV descriptor 할당을 구현한다.
- DX12 descriptor heap 정책을 임시가 아닌 frame-safe 구조로 잡는다.
- Sampler state를 RHI descriptor로 추가한다.
- HLSL은 `baseColorTexture`, `sampler`, `normal`, `lightDirection`, `lightColor`를 받는다.
- texture loader는 기존 Engine texture loader와 RHI texture 생성 사이의 변환 경로를 만든다.

주요 반영 파일:
- `C:/Users/tnest/Desktop/Winters/Engine/Public/RHI/RHIDescriptors.h`
- `C:/Users/tnest/Desktop/Winters/Engine/Public/RHI/RHITextureLoader.h`
- `C:/Users/tnest/Desktop/Winters/Engine/Private/RHI/RHITextureLoader.cpp`
- `C:/Users/tnest/Desktop/Winters/Engine/Private/RHI/DX12/DX12Device.cpp`
- `C:/Users/tnest/Desktop/Winters/EldenRingClient/Private/EldenRingRHITestCubeRenderer.cpp`

완료 기준:
- 큐브에 checker 또는 테스트 texture가 표시된다.
- sampler filtering을 nearest/linear로 바꿨을 때 화면 차이가 보인다.
- directional light 방향 변경 시 면 밝기가 변한다.

1-5. Session 03 - PBR Material Sphere

목표:
- PBR의 최소 기준 검증물을 큐브가 아니라 material sphere로 바꾼다.
- Metallic/Roughness workflow와 GGX BRDF를 붙인다.

반영 범위:
- Sphere geometry 또는 mesh primitive를 RHI vertex/index buffer로 생성한다.
- Material parameter 구조에 `baseColor`, `metallic`, `roughness`, `ao`, `normalScale`을 둔다.
- HLSL에 GGX NDF, Smith geometry, Schlick Fresnel, Lambert diffuse를 구현한다.
- IBL은 첫 세션에서 완전한 environment convolution까지 가지 않고, constant ambient 또는 placeholder cubemap으로 시작한다.
- material debug UI는 값 변경이 바로 화면에 반영되는 수준으로 만든다.

주요 반영 파일:
- `C:/Users/tnest/Desktop/Winters/Engine/Public/RHI/Geometry`
- `C:/Users/tnest/Desktop/Winters/EldenRingClient/Private`
- `C:/Users/tnest/Desktop/Winters/Shaders`

완료 기준:
- roughness 0.05와 0.8의 highlight 폭 차이가 명확하다.
- metallic 0과 1의 diffuse/specular 반응 차이가 명확하다.
- normal map을 켜고 끌 수 있다.

1-6. Session 04 - Elden Ring Extracted Mesh Render

목표:
- 테스트 primitive에서 벗어나 추출된 Elden Ring 메시를 화면에 띄운다.
- 이 단계의 성공 기준은 PBR 완성도가 아니라 asset pipeline 연결이다.

반영 범위:
- asset export 문서에 추출 경로, 변환 규칙, 지원 확장자, 금지 항목을 기록한다.
- 메시 loader는 position, normal, tangent, uv, color, index, submesh, material slot을 읽는다.
- material slot은 우선 PBR parameter와 texture path로 연결한다.
- skinned mesh와 animation은 이 세션의 필수 완료 조건에서 제외한다. 먼저 static mesh를 통과시킨다.
- draw call은 submesh 단위로 분리하고, 각 submesh material을 바인딩한다.

주요 반영 파일:
- `C:/Users/tnest/Desktop/Winters/EldenRingClient/Public`
- `C:/Users/tnest/Desktop/Winters/EldenRingClient/Private`
- `C:/Users/tnest/Desktop/Winters/Engine/Public/AssetFormat`
- `C:/Users/tnest/Desktop/Winters/Engine/Public/Resource/Model.h`
- `C:/Users/tnest/Desktop/Winters/Engine/Private/Resource/Model.cpp`
- `C:/Users/tnest/Desktop/Winters/.md/plan/graphics`

완료 기준:
- 하나의 추출 static mesh가 correct scale/orientation으로 렌더링된다.
- submesh별 material 색 또는 texture가 분리된다.
- missing texture/material은 명확한 fallback으로 표시된다.

1-7. Session 05 - Forward+ PBR Scene

목표:
- 단일 조명 PBR에서 다중 light scene으로 확장한다.
- Elden Ring 지형/오브젝트를 올릴 수 있는 기본 scene renderer 구조를 만든다.

반영 범위:
- depth prepass 또는 depth pyramid 입력을 만든다.
- compute shader로 tile/cluster light culling을 구현한다.
- light list buffer, light grid buffer, visible light count debug를 추가한다.
- PBR pass는 tile/cluster light list를 읽어 누적 조명을 계산한다.
- directional light, point light, spot light를 최소 지원한다.

주요 반영 파일:
- `C:/Users/tnest/Desktop/Winters/Engine/Public/RHI`
- `C:/Users/tnest/Desktop/Winters/Engine/Private/RHI/DX12`
- `C:/Users/tnest/Desktop/Winters/EldenRingClient/Private`
- `C:/Users/tnest/Desktop/Winters/Shaders`

완료 기준:
- 100개 이상 point light를 배치해도 단일 forward loop보다 안정적인 frame time을 보인다.
- light grid debug overlay 또는 OutputDebugStringA trace로 tile별 light count를 확인할 수 있다.
- mesh/material 렌더링이 기존 Session 04 결과보다 퇴행하지 않는다.

1-8. Session 06 - TAA

목표:
- temporal history를 도입해 이후 Volumetric Fog, GI, RayTracing denoise의 기반을 만든다.
- 첫 완료 기준은 완벽한 품질이 아니라 안정적인 jitter/history/resolve 구조다.

반영 범위:
- camera projection에 Halton jitter를 적용한다.
- per-frame constant에 current/previous view-projection matrix를 추가한다.
- velocity buffer 또는 최소 motion vector target을 만든다.
- history color target을 ping-pong으로 관리한다.
- TAA resolve shader는 neighborhood clamp와 feedback factor를 가진다.

주요 반영 파일:
- `C:/Users/tnest/Desktop/Winters/Engine/Public/RHI`
- `C:/Users/tnest/Desktop/Winters/Engine/Private/RHI/DX12`
- `C:/Users/tnest/Desktop/Winters/EldenRingClient/Private`
- `C:/Users/tnest/Desktop/Winters/Shaders`

완료 기준:
- TAA on/off 토글이 가능하다.
- 정지 화면에서 edge shimmer가 줄어든다.
- 카메라 이동 중 ghosting이 심하면 실패로 보고 clamp/velocity를 조정한다.

1-9. Session 07 - Volumetric Fog

목표:
- Elden Ring 분위기의 핵심인 공간 안개를 froxel 기반으로 구현한다.
- 단순 screen-space fog가 아니라 light와 depth를 받는 volume pass로 간다.

반영 범위:
- froxel grid texture 또는 structured buffer를 생성한다.
- depth를 이용해 view-space volume slice를 구성한다.
- directional/point light injection compute shader를 만든다.
- temporal reprojection은 Session 06 history 구조를 재사용한다.
- final composite pass에서 scene color와 fog scattering을 합성한다.

주요 반영 파일:
- `C:/Users/tnest/Desktop/Winters/Engine/Public/RHI`
- `C:/Users/tnest/Desktop/Winters/Engine/Private/RHI/DX12`
- `C:/Users/tnest/Desktop/Winters/EldenRingClient/Private`
- `C:/Users/tnest/Desktop/Winters/Shaders`

완료 기준:
- fog density, height falloff, anisotropy 조절이 화면에 즉시 반영된다.
- light가 fog volume 안에서 보이는 shaft를 만든다.
- TAA off/on에서 노이즈와 ghosting 차이를 비교할 수 있다.

1-10. Session 08 - GI

목표:
- RayTracing으로 바로 뛰기 전에 raster/compute 기반 GI 기준선을 만든다.
- SSGI 또는 probe 기반 DDGI 중 하나를 먼저 통과시킨다.

반영 범위:
- 첫 구현은 SSGI로 시작할 수 있다. depth, normal, scene color, history를 입력으로 쓴다.
- open world 확장까지 고려하면 probe volume 또는 DDGI prototype을 다음 세션으로 둔다.
- diffuse indirect와 direct light를 분리해 debug view를 만든다.
- temporal accumulation은 TAA와 별도 feedback 값을 가진다.

주요 반영 파일:
- `C:/Users/tnest/Desktop/Winters/Engine/Public/RHI`
- `C:/Users/tnest/Desktop/Winters/Engine/Private/RHI/DX12`
- `C:/Users/tnest/Desktop/Winters/EldenRingClient/Private`
- `C:/Users/tnest/Desktop/Winters/Shaders`

완료 기준:
- indirect light on/off 차이가 sphere/mesh contact area에서 보인다.
- history accumulation이 카메라 정지 시 노이즈를 낮춘다.
- direct light만으로는 보이지 않는 bounce 느낌이 debug view에서 확인된다.

1-11. Session 09 - GI / RayTracing

목표:
- DXR을 도입해 RT shadow, RT AO, RT reflection, RT GI를 순서대로 확장한다.
- 첫 DXR 세션의 목표는 full path tracing이 아니라 acceleration structure와 ray dispatch 성공이다.

반영 범위:
- DX12 feature query에 raytracing support check를 추가한다.
- BLAS/TLAS builder를 RHI 내부 DX12 backend에 둔다.
- RHI에는 raytracing이 없는 backend에서 실패/비활성 처리가 명확한 capability API를 둔다.
- RayGen, Miss, ClosestHit shader table을 구성한다.
- 첫 visual target은 RT shadow 또는 RT AO로 잡는다.
- RT GI는 denoise와 temporal accumulation까지 붙은 뒤 정식 완료로 본다.

주요 반영 파일:
- `C:/Users/tnest/Desktop/Winters/Engine/Public/RHI/RHICapabilities.h`
- `C:/Users/tnest/Desktop/Winters/Engine/Public/RHI`
- `C:/Users/tnest/Desktop/Winters/Engine/Private/RHI/DX12`
- `C:/Users/tnest/Desktop/Winters/EldenRingClient/Private`
- `C:/Users/tnest/Desktop/Winters/Shaders`

완료 기준:
- DXR 미지원 GPU에서는 안전하게 fallback 또는 disabled state가 출력된다.
- DXR 지원 GPU에서는 BLAS/TLAS build와 ray dispatch가 PIX에서 확인된다.
- RT shadow 또는 RT AO가 raster path와 비교 가능한 debug view를 가진다.

1-12. 세션 진행 게이트

다음 세션으로 넘어가기 전에 반드시 확인할 것:
- Debug/Release 빌드가 통과해야 한다.
- `git diff --check`가 통과해야 한다.
- EldenRingClient가 RHI 공개 API만 사용하는지 `rg`로 확인해야 한다.
- visual smoke test가 있어야 한다.
- Engine public header 변경 시 `UpdateLib.bat` 동기화 결과가 있어야 한다.
- 실패한 visual check는 문서에 남기고 다음 세션으로 넘기지 않는다.

2. 검증

문서 생성 검증:
- `git diff --check`
- `Get-Content -LiteralPath .md/plan/graphics/2026-06-03_ELDEN_RING_DX12_RENDERING_SESSION_ROADMAP.md -TotalCount 40`

각 세션 공통 빌드 검증:
- `cmake --preset msvc-ninja`
- `cmake --build --preset elden-debug`
- `cmake --build --preset elden-release`
- `MSBuild Engine/Include/Engine.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`
- `MSBuild Engine/Include/Engine.vcxproj /p:Configuration=Release /p:Platform=x64 /m`
- `MSBuild EldenRingClient/Include/EldenRingClient.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`
- `MSBuild EldenRingClient/Include/EldenRingClient.vcxproj /p:Configuration=Release /p:Platform=x64 /m`

각 세션 공통 런타임 검증:
- `C:/Users/tnest/Desktop/Winters/EldenRingClient/Bin/Debug/WintersElden.exe --rhi=dx12`
- Debug Output에서 해당 세션 ready log 확인.
- PIX 또는 RenderDoc capture로 pass/resource/barrier/binding 확인.
- `rg -n "ID3D12|d3d12|DX12CommandList|GetNativeHandle\\(eNativeHandleType::DX12" EldenRingClient -g "*.h" -g "*.cpp"` 결과가 비어 있는지 확인.

세션별 visual smoke 기준:
- Session 00: 회전 컬러 큐브.
- Session 01: depth가 정상인 3D 큐브와 camera projection.
- Session 02: texture와 directional light가 적용된 큐브.
- Session 03: roughness/metallic 차이가 보이는 PBR material sphere.
- Session 04: 추출 static mesh가 material slot과 함께 렌더링.
- Session 05: 다중 light를 Forward+ 경로로 처리하는 PBR scene.
- Session 06: TAA on/off 차이가 보이는 temporal resolve.
- Session 07: light를 받는 volumetric fog.
- Session 08: raster/compute GI debug view.
- Session 09: DXR 지원 GPU에서 RT shadow 또는 RT AO debug view.
