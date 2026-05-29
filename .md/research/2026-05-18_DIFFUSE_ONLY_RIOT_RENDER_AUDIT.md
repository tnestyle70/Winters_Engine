# Diffuse-Only Riot-Grade Render Audit - 2026-05-18

## 결론

현재 Winters 챔피언 렌더가 "2009년 출시작"처럼 보이는 1차 원인은 GI, Volumetric Fog, Forward+, RayTracing 부재가 아니다. 핵심 병목은 메인 챔피언 셰이더가 diffuse PNG를 표면 조명으로 거의 조각하지 않는다는 점이다.

현재 기본 챔피언 등록은 대부분 `Shaders/Mesh3D.hlsl`을 사용한다. `ModelRenderer::Init`은 `pHlslPath` 문자열에 `PBR`이 들어간 경우만 PBR 공유 셰이더를 선택하고, 그 외에는 공유 `Mesh3D/Skinned3D` 경로를 쓴다. 즉 현재 챔피언 다수는 실제로 diffuse-only 셰이더 경로에 있다.

## 6회 코드 분석 요약

### 1. 셰이더 출력 모델

- `Shaders/Mesh3D.hlsl`
  - VS에서 world normal/world position을 만들지만 PS는 diffuse texture를 샘플하고 그대로 반환한다.
  - `clip(texColor.a - 0.05f)` 외에는 광원, 카메라, 림, ambient shaping이 없다.
- `Shaders/Skinned3D.hlsl`
  - PS는 diffuse sample을 그대로 반환한다.
  - VS normal은 skin matrix까지만 곱하고 `g_matWorldInvTranspose`를 거치지 않는다.

이 상태에서는 아무리 좋은 PNG를 써도 캐릭터 형태감은 텍스처 페인팅에만 의존한다.

### 2. PBR 경로

- `Shaders/Mesh3D_PBR.hlsl`, `Shaders/Skinned3D_PBR.hlsl`에는 GGX/point light/AO/tone mapping/gamma가 있다.
- 하지만 현재 챔피언 등록 대부분은 `L"Shaders/Mesh3D.hlsl"`이며, `ModelRenderer::Init`은 `PBR` 문자열만 보고 경로를 나눈다.
- 사용자가 원하는 목표가 "albedo/roughness/metallic 없는 순수 diffuse + 광원"이므로, PBR 확장보다 diffuse stylized lighting 경로를 먼저 깎는 것이 맞다.

### 3. ModelRenderer 상수 버퍼와 조명

- `CBPerFrame`에는 camera, directional light, 4 point lights, screen size가 이미 들어 있다.
- PBR 경로는 `cbPerFrame.Bind(pContext, 0)`으로 VS/PS 모두에 바인딩된다.
- non-PBR 경로는 현재 `BindVS`만 호출한다. 따라서 PS가 조명을 읽을 수 없다.
- `UpdateCamera`는 이미 light direction/intensity/color/point lights를 채우므로, 최소 변경으로 diffuse-only PS에서 이 데이터를 활용할 수 있다.

### 4. SSAO / NormalPass

- `CNormalPass`와 `CSSAOPass`는 이미 존재한다.
- 하지만 bootstrap에서 SSAO를 생성한 뒤 바로 disable한다.
- render bridge는 normal pass에서 `UsesPBR()`가 아닌 renderer를 skip한다.
- `ModelRenderer::RenderNormalPassWithVisibility`도 `!bUsePBR`이면 return한다.
- 따라서 현재 non-PBR 챔피언은 SSAO/contact AO를 거의 받지 못한다.

### 5. 텍스처/색공간

- material format은 diffuse path 중심이다. roughness/metallic/normal 기반의 material contract는 현재 메인 경로가 아니다.
- `CTexture::Create`는 `Auto`일 때 WIC/DDS 기본 로더를 쓰고, `IgnoreSRGB`만 별도 처리한다.
- DX11 swapchain은 `DXGI_FORMAT_R8G8B8A8_UNORM`이다.
- 당장 큰 구조 변경 없이 하려면 shader-local sRGB decode/encode를 먼저 시도하는 편이 작고 검증 가능하다.

### 6. 고급 기법의 실제 우선순위

| 기법 | 현재 체감 | 우선순위 | 판단 |
|---|---:|---:|---|
| Stylized diffuse lighting | 매우 큼 | 1 | 캐릭터 형태감, 실루엣, 상하 가독성의 핵심 |
| Contact AO / SSAO | 큼 | 2 | 발밑 접지감과 장비 사이 음영을 만든다 |
| Color-space cleanup | 큼 | 3 | 탁함/과포화/평면감을 줄인다 |
| Debug tuning UI | 중간 | 4 | 수치 조율 속도를 올린다 |
| GI | 중간 | 후순위 | 실시간 GI보다 fake bounce/ambient shaping이 먼저 |
| Volumetric Fog | 장면 분위기 | 후순위 | 챔피언 디테일보다 맵/연출 레이어용 |
| Forward+ | 확장성 | 후순위 | 많은 동적 라이트를 처리하는 구조, 기본 화질의 원인은 아님 |
| RayTracing | 낮음 | 최후순위 | LoL풍 diffuse-only에는 비용 대비 핵심 효과가 작다 |

## 세션별 진행 파일

1. `.md/plan/graphics/2026-05-18_SESSION_01_DIFFUSE_STYLIZED_LIGHTING.md`
   - `Mesh3D/Skinned3D`를 texture-only에서 stylized diffuse lighting으로 전환한다.
   - non-PBR 경로도 `CBPerFrame`을 PS에 바인딩한다.
   - skinned normal을 world inverse transpose까지 통과시킨다.

2. `.md/plan/graphics/2026-05-18_SESSION_02_SSAO_NON_PBR_CONTACT.md`
   - non-PBR renderer도 normal pass와 SSAO를 받게 한다.
   - map depth를 normal pass에 넣어 챔피언 발밑 contact AO를 만든다.

3. `.md/plan/graphics/2026-05-18_SESSION_03_SHADER_LOCAL_SRGB.md`
   - shader-local sRGB decode/encode를 넣어 diffuse 계산을 더 안정화한다.
   - swapchain/RTV 전체 구조 변경은 보류한다.

4. `.md/plan/graphics/2026-05-18_SESSION_04_RENDER_DEBUG_SSAO_TUNING.md`
   - Render Debug 패널에 SSAO enable/radius/intensity를 노출한다.
   - 실화면에서 수치를 빠르게 깎는다.

## 실행 원칙

- 일반 F5 flow에서 roster/map/minion/snapshot/champion 시스템을 숨기지 않는다.
- 렌더 실험은 client visual에 국한한다.
- 서버 권위 GameSim/네트워크 flow는 건드리지 않는다.
- "PBR로 넓히기"보다 "현재 diffuse-only 표면을 조명으로 조각하기"를 우선한다.
- Forward+/Fog/RayTracing은 Session 1~4 검증 뒤, 실제 부족한 부분이 남을 때 별도 계획으로 분리한다.
