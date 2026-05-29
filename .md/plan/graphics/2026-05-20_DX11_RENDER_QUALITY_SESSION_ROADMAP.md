Session - DX11 렌더 퀄리티 남은 단계 로드맵

목표: DX12/Vulkan을 붙이기 전에 DX11에서 먼저 납득 가능한 기준 이미지를 고정한다. 지금 방향은 PBR 확장이 아니라 diffuse-only, stylized direct light, shader-local sRGB, SSAO/contact AO, Fog/FOW 팔레트 정리, 텍스처 감마 규칙 고정이다.

1. 반영해야 하는 코드

1-1. 현재 코드 반영 상태

이미 반영/리뷰된 세션:

| 세션 | 주제 | 문서 | 현재 상태 |
|---:|---|---|---|
| 01 | diffuse stylized lighting | [2026-05-18_SESSION_01_DIFFUSE_STYLIZED_LIGHTING.md](2026-05-18_SESSION_01_DIFFUSE_STYLIZED_LIGHTING.md) | 반영/리뷰 완료 |
| 02 | SSAO non-PBR contact | [2026-05-18_SESSION_02_SSAO_NON_PBR_CONTACT.md](2026-05-18_SESSION_02_SSAO_NON_PBR_CONTACT.md) | 반영/리뷰 완료 |
| 03 | shader-local sRGB | [2026-05-18_SESSION_03_SHADER_LOCAL_SRGB.md](2026-05-18_SESSION_03_SHADER_LOCAL_SRGB.md) | 셰이더 반영 완료 |
| 04 | Render Debug SSAO tuning | [2026-05-18_SESSION_04_RENDER_DEBUG_SSAO_TUNING.md](2026-05-18_SESSION_04_RENDER_DEBUG_SSAO_TUNING.md) | 반영/리뷰 완료 |
| 05 | DX11 SSAO tuning controls | [2026-05-20_SESSION_05_DX11_SSAO_TUNING.md](2026-05-20_SESSION_05_DX11_SSAO_TUNING.md) | 반영 완료 |
| 06 | map diffuse ramp / ambient discipline | [2026-05-20_SESSION_06_MAP_DIFFUSE_RAMP_AMBIENT.md](2026-05-20_SESSION_06_MAP_DIFFUSE_RAMP_AMBIENT.md) | 반영 완료 |
| 07 | champion readability rim / top light | [2026-05-20_SESSION_07_CHAMPION_READABILITY_RIM.md](2026-05-20_SESSION_07_CHAMPION_READABILITY_RIM.md) | 리뷰 수정 완료, F5 최종 눈검증 필요 |
| 08A | UI/indicator unlit shader split | [2026-05-20_SESSION_08A_UI_UNLIT_SHADER_SPLIT.md](2026-05-20_SESSION_08A_UI_UNLIT_SHADER_SPLIT.md) | 반영 완료 |
| 08 | non-PBR point light accents | [2026-05-20_SESSION_08_NONPBR_POINT_LIGHT_ACCENTS.md](2026-05-20_SESSION_08_NONPBR_POINT_LIGHT_ACCENTS.md) | 반영 완료 |
| 09 | AO propagation to objects | [2026-05-20_SESSION_09_AO_PROPAGATION_OBJECTS.md](2026-05-20_SESSION_09_AO_PROPAGATION_OBJECTS.md) | 반영/빌드 완료 |
| 10 | ground contact shadow / decal | [2026-05-20_SESSION_10_GROUND_CONTACT_SHADOW_DECAL.md](2026-05-20_SESSION_10_GROUND_CONTACT_SHADOW_DECAL.md) | 리뷰 수정/빌드 완료 |
| 12 | tone/gamma/mip filtering audit | [2026-05-20_SESSION_12_TONE_GAMMA_MIP_FILTER_AUDIT.md](2026-05-20_SESSION_12_TONE_GAMMA_MIP_FILTER_AUDIT.md) | texture color-space 보정/빌드 완료, F5 색감 검증 필요 |

1-2. Session 12 코드베이스 보정

코드 기준으로 지금 즉시 수정이 필요한 부분:

- `Shaders/Mesh3D.hlsl`, `Shaders/Skinned3D.hlsl`는 이미 shader-local sRGB 변환을 수행한다.
- 그런데 모델 diffuse texture 로드는 아직 기본 WIC/DDS sRGB 정책을 탈 수 있어 double gamma 위험이 남아 있었다.
- 따라서 모델/수동 메시 텍스처 로드는 `eTexColorSpace::ShaderLocalSRGB`로 고정한다.
- UI, portrait, loading image, HP bar, attack range, FX texture는 이번 보정 대상이 아니다.

반영 파일:

- `C:/Users/user/Desktop/Winters/Engine/Public/Resource/Texture.h`
- `C:/Users/user/Desktop/Winters/Engine/Private/Resource/Texture.cpp`
- `C:/Users/user/Desktop/Winters/Engine/Public/Resource/ResourceCache.h`
- `C:/Users/user/Desktop/Winters/Engine/Private/Resource/ResourceCache.cpp`
- `C:/Users/user/Desktop/Winters/Engine/Private/Resource/Model.cpp`
- `C:/Users/user/Desktop/Winters/Engine/Private/Renderer/ModelRenderer.cpp`

1-3. 다음 구현 순서

Session 11 - Fog/FOW color integration

문서:
[2026-05-20_SESSION_11_FOG_FOW_COLOR_INTEGRATION.md](2026-05-20_SESSION_11_FOG_FOW_COLOR_INTEGRATION.md)

목표:
- Fog of War가 검정 필름처럼 얹히지 않고 맵 팔레트에 붙게 만든다.
- unexplored/explored fog 색을 상수 버퍼로 분리한다.
- 챔피언 실루엣, HP bar, UI를 묻히면 실패다.

주요 파일:
- `C:/Users/user/Desktop/Winters/Shaders/FogOfWarWorld.hlsl`
- `C:/Users/user/Desktop/Winters/Engine/Private/Renderer/FogOfWarRenderer.cpp`

Session 12 - tone/gamma/mip filtering audit

문서:
[2026-05-20_SESSION_12_TONE_GAMMA_MIP_FILTER_AUDIT.md](2026-05-20_SESSION_12_TONE_GAMMA_MIP_FILTER_AUDIT.md)

현재 상태:
- shader-local sRGB와 texture loader 정책 불일치 보정은 코드에 반영했고 Engine/Client Debug x64 빌드가 통과했다.
- 남은 것은 F5 색감 검증이다.

F5 실패 기준:
- 챔피언/맵 diffuse가 과하게 어두워지면 실패다.
- 이전보다 뜨거나 뿌옇게 보이면 실패다.
- UI, HP bar, Attack Range, FX 색이 변하면 실패다.

Session 11과 Session 12의 적용 순서:
- 먼저 Session 12 F5 색감 검증으로 감마 기준을 고정한다.
- 그 다음 Session 11 Fog/FOW 색을 얹는다.
- 이유: 감마 기준이 흔들린 상태에서 fog 색을 튜닝하면 값이 다시 틀어진다.

Session 13 - DX12/Vulkan parity boundary

문서:
[2026-05-20_SESSION_13_DX12_VULKAN_PARITY_BOUNDARY.md](2026-05-20_SESSION_13_DX12_VULKAN_PARITY_BOUNDARY.md)

목표:
- DX12/Vulkan을 구현한 척하지 않는다.
- `--rhi=vulkan`, `/rhi:vulkan`, `--rhi=auto`, `/rhi:auto` 요청을 명확히 해석한다.
- Vulkan 요청은 현재 미구현 로그와 DX11 fallback boundary가 분명해야 한다.

주요 파일:
- `C:/Users/user/Desktop/Winters/Client/Private/main.cpp`
- `C:/Users/user/Desktop/Winters/Engine/Private/Framework/CEngineApp.cpp`

1-4. DX11 기준 이미지 고정 후 보류할 것

아직 구현하지 않는다:

- Forward+
- RayTracing/DXR
- Volumetric Fog
- realtime GI/DDGI/VXGI
- DX12/Vulkan full parity

이유:
- 현재 Winters 렌더 문제의 핵심은 고급 기법 부재보다 색공간, diffuse ramp, AO/contact, UI unlit 분리, FOW 팔레트 불일치에 있다.
- DX11 기준 이미지가 고정되기 전에 백엔드를 늘리면 같은 문제를 세 배로 디버깅하게 된다.

2. 검증

완료된 최근 검증:

- `git diff --check`: 통과
- `Shaders/ContactShadowPlane.hlsl` VS/PS `fxc`: 통과
- Engine Debug x64: 통과
- Client Debug x64: 통과
- Server Debug x64: 통과
- Session 12 texture color-space 보정 후 Engine Debug x64: 통과, warning 8 / error 0
- Session 12 texture color-space 보정 후 Client Debug x64: 통과, warning 17 / error 0

Session 12 보정 후 검증 명령:

```powershell
git diff --check -- Engine/Public/Resource/Texture.h Engine/Private/Resource/Texture.cpp Engine/Public/Resource/ResourceCache.h Engine/Private/Resource/ResourceCache.cpp Engine/Private/Resource/Model.cpp Engine/Private/Renderer/ModelRenderer.cpp
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Engine/Include/Engine.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

다음 F5 눈검증:

- 같은 맵, 같은 카메라, 같은 챔피언으로 비교한다.
- 챔피언 흰 머리/피부/무기 하이라이트가 얼음처럼 뜨면 실패다.
- 맵 돌바닥이 2009년식 회색/파랑 필터처럼 보이면 Session 11 fog 전에 Session 12 값을 먼저 의심한다.
- A Range, HP bar, minion HP bar가 월드 조명이나 fog에 물들면 UI unlit 경로 회귀다.
- ContactShadow는 발밑 접촉감만 줘야 하고, 스킬 범위/UI 위로 떠 보이면 실패다.
