# 05. 통합 단계 일정 + 검증 + 롤백 전략

> 7단계 롤아웃의 **하루 단위 작업 계획** + 각 단계 빌드/시각/수치 검증 + 위험 발생 시 롤백 절차.
> 진입 조건: Phase 5-B JobSystem race 정식 수정 완료 + Phase 2 야스오 작업 일단락.

---

## 1. 일정 요약

| Stage | 내용 | 일수 | 출력물 |
|---|---|---|---|
| 0 | Depth Pre-pass 셰이더 + DSV/SRV TYPELESS 셋업 | 1 | `DepthPrepass.hlsl` 동작, RenderDoc depth 확인 |
| 1 | BRDFCommon/BRDFGGX/BRDFCookTorrance hlsli + Furnace test | 1 | hlsli 컴파일 통과, white furnace 0.18 ±2% |
| 2 | Mesh3D_PBR.hlsl + Skinned3D_PBR.hlsl + DepthPrepass | 1 | unlit 분기 그대로 + PBR 분기 신설 |
| 3 | CPBRMaterial + CDirectionalLight + ImGui 패널 | 2 | 메탈 구 슬라이더 ImGui 동작 |
| 4 | 단일 dir light 검증 + Furnace test 수치 | 0.5 | 단일 광원 visual 확인 |
| 5 | CLightManager + ForwardPlusPipeline + Cull CS | 3 | 100 광원 산포 + Tile Heatmap |
| 6 | MOBA 광원 통합 (스킬 FX, 포탑, 챔프 림) | 2 | 이렐리아 R 펄스 광원 효과 |
| 7 | (선택) IBL prefilter — 환경광 | 3 | Phase E Stage 2, 별도 작업 |

**합계 12.5일** (Stage 7 제외 9.5일).

---

## 2. Stage 0 — Depth Pre-pass

### 작업 단위

| 단계 | 파일 | 변경 내용 |
|---|---|---|
| 0-1 | `Shaders/PBR/DepthPrepass.hlsl` 신규 | 02 문서 §8 코드 박제 |
| 0-2 | `Engine/Public/Renderer/PBR/ForwardPlusPipeline.h` 신규 | DSV/SRV TYPELESS 셋업 |
| 0-3 | `Engine/Private/Renderer/PBR/ForwardPlusPipeline.cpp` Stage 0 부분 | `CreateDepthBuffer()` 구현 |
| 0-4 | `CEngineApp::Render()` 변경 | 04 문서 §7 BEFORE → AFTER |
| 0-5 | `ModelRenderer::RenderDepthOnly()` 신규 메서드 | 같은 메시 + DepthPrepass 셰이더로 그리기 |

### 빌드 검증

```bat
:: Engine/Client 빌드 통과 확인
msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64
```

### 시각 검증

**RenderDoc 캡처 후 확인**:
1. Depth Pre-pass draw call 이 frame 의 첫 10번째 이내 호출됨.
2. DSV 가 `R24G8_TYPELESS` 텍스처에 바인딩.
3. Pre-pass 끝 직후 depth buffer 가 (white = far / black = near) 로 시각화됨.

### 수치 검증

```cpp
// 디버그 코드 — 임시
ID3D11Texture2D* pStaging = ...;       // CPU readback 가능 텍스처
pCtx->CopyResource(pStaging, m_pDepthTex.Get());
D3D11_MAPPED_SUBRESOURCE map{};
pCtx->Map(pStaging, 0, D3D11_MAP_READ, 0, &map);
float* depthData = (float*)map.pData;
// depthData[centerPx] 가 0.x ~ 1.0 사이 값이어야 함 (cleared 0 또는 1 아님)
```

### 위험 + 롤백

| 위험 | 증상 | 롤백 |
|---|---|---|
| TYPELESS 미사용 | `CreateShaderResourceView` 실패 | DSV 와 SRV 가 같은 텍스처를 공유하려면 TYPELESS 필수. `D24_UNORM_S8_UINT` 직접 사용 시 SRV 못 만듦 |
| Pre-pass 후 main pass 가 depth 다시 씀 | overdraw 그대로 | DepthState 를 main pass 에서 `LessEqual + DepthWrite=OFF` 로 설정 |
| 알파 cutout 미반영 | 투명 영역도 depth 박힘 | `DepthPrepass.hlsl::PS` 에 `clip(albedoTex.a - 0.05f)` 박제 (이미 02 문서에 포함) |

---

## 3. Stage 1 — BRDF 수학 라이브러리

### 작업 단위

| 단계 | 파일 | 분량 |
|---|---|---|
| 1-1 | `Shaders/BRDF/BRDFCommon.hlsli` 신규 | ~60 줄 |
| 1-2 | `Shaders/BRDF/BRDFGGX.hlsli` 신규 | ~80 줄 |
| 1-3 | `Shaders/BRDF/BRDFCookTorrance.hlsli` 신규 | ~50 줄 |
| 1-4 | `Shaders/BRDF/BRDFLighting.hlsli` 신규 | ~70 줄 |
| 1-5 | `Shaders/Tests/FurnaceTest.hlsl` 신규 (Compute) | ~80 줄 |
| 1-6 | `Tools/check_pbr_shaders.bat` — fxc 컴파일 검증 | 6 줄 |

### 빌드 검증

```bat
fxc /T cs_5_0 /E CSMain Shaders/Tests/FurnaceTest.hlsl /Fo nul
```

### 수치 검증 — White Furnace

`FurnaceTestHarness.cpp` 임시 코드로 결과 stagiing texture readback:

| 입력 | 기대 출력 | 허용 |
|---|---|---|
| albedo=0.18, metal=0, rough=1 | 0.176 ~ 0.184 | ±2% |
| albedo=0.5, metal=0, rough=0.5 | 0.49 ~ 0.51 | ±2% |
| albedo=(0.7,0.5,0.3), metal=1, rough=0 | (0.7, 0.5, 0.3) | ±5% (specular spike 영향) |

위 3개 모두 통과 = Stage 1 완료.

### 위험 + 롤백

| 위험 | 증상 | 롤백 |
|---|---|---|
| α=0 시 D 가 inf | Furnace test NaN | `EvaluateBRDF` 에 `alpha = max(roughness², 0.0045)` 박제 (02 문서 박제됨) |
| V 분모 4 중복 곱 | 결과 1/4 어두움 | `V_SmithGGXCorrelated` 가 G 가 아닌 Visibility 임을 댓글로 박제 + 02 문서 §10 검증 |
| sRGB 미변환 | 색이 너무 진함 (감마 누적) | `SRGBtoLinear`/`LinearToSRGB` 호출 누락 검색 |

---

## 4. Stage 2 — PBR 셰이더

### 작업 단위

| 단계 | 파일 |
|---|---|
| 2-1 | `Shaders/PBR/Mesh3D_PBR.hlsl` 신규 |
| 2-2 | `Shaders/PBR/Skinned3D_PBR.hlsl` 신규 |
| 2-3 | OutDir xcopy `/Y` 강제 (Client.vcxproj PostBuild) |
| 2-4 | `EngineSDK/inc/` 동기화 (헤더 변화 없음 — 이 단계는 셰이더만) |

### 시각 검증

광원 0개 + albedo=0.5 + metallic=0 시 거의 검정 (`ambient * 0.03 * AO` 만 출력) — unlit 과 다름.
→ **이 단계는 단독 의미 없음**. Stage 3+4 와 묶어서 검증.

---

## 5. Stage 3+4 — C++ 측 + 단일 광원 검증

### 작업 단위

| 단계 | 파일 |
|---|---|
| 3-1 | `Engine/Public/Renderer/PBR/PBRTypes.h` 신규 |
| 3-2 | `Engine/Public/Renderer/PBR/PBRMaterial.h` + `.cpp` |
| 3-3 | `Engine/Public/Renderer/PBR/DirectionalLight.h` + `.cpp` |
| 3-4 | `Engine/Public/Renderer/PBR/LightManager.h` + `.cpp` (Stage 5 일부 미완) |
| 3-5 | `Engine/Include/GameInstance.h` Tier-2 게터 2개 |
| 3-6 | `Engine/Private/Framework/CEngineApp.cpp` LightManager 멤버 + Initialize |
| 3-7 | `Engine/Public/Renderer/ModelRenderer.h` `InitPBR()` 추가 |
| 3-8 | `Engine/Private/Renderer/ModelRenderer.cpp` Render 분기 |
| 3-9 | `Engine/Public/Editor/PBRDebugPanel.h` + 구현 |
| 3-10 | `EngineSDK/inc/` 동기화 (UpdateLib.bat) |

### 빌드 검증

```bat
:: PreBuildEvent → UpdateLib.bat 자동 호출
msbuild Winters.sln /p:Configuration=Debug

:: 신규 헤더가 EngineSDK 에 복사됐는지
ls EngineSDK/inc/ | grep -E "PBR|Directional|LightMgr"
```

### 시각 검증 — 메탈 구 단독

```cpp
// Scene_InGame::OnEnter() 임시
auto pMat = CPBRMaterial::Create(pDev);
pMat->LoadAlbedo(pCtx, L"Client/Bin/Resource/Texture/Test/gold_albedo.png");
pMat->SetMetallic(1.0f);
pMat->SetRoughness(0.2f);

m_pTestSphere = std::make_unique<ModelRenderer>();
m_pTestSphere->InitPBR("Client/Bin/Resource/Mesh/sphere.fbx", std::move(pMat));

auto* pSun = CGameInstance::Get()->Get_LightManager()->GetSun();
pSun->SetDirection({0.3f, -1.0f, 0.5f});
pSun->SetIntensity(3.0f);
pSun->SetColor({1.0f, 0.95f, 0.8f});
```

ImGui PBRDebugPanel 슬라이더로:
- Roughness 0 → 1: 하이라이트가 점점 퍼지는지
- Metallic 0 → 1: 색이 검어지면서 specular 가 albedo 색으로 바뀌는지
- Sun Direction 변경: 하이라이트 위치가 움직이는지

3개 다 동작 = Stage 3+4 통과.

### 위험 + 롤백

| 위험 | 증상 | 롤백 |
|---|---|---|
| dllexport copy 인스턴스화 실패 | C2672 construct_at 에러 | `CPBRMaterial(const&) = delete` 명시 (CLAUDE.md 함정 박제됨) |
| WICTexture sRGB 자동 변환 안 됨 | 색이 진함 | `D3D11_RESOURCE_MISC_TEXTURECUBE` flag 또는 `WIC_LOADER_FORCE_SRGB` 사용 |
| ComPtr unqualified 사용 (Mr::WRL 누락) | C7568 SDK include 에러 | 03 문서 모든 멤버 `Microsoft::WRL::ComPtr` 명시 |
| FBX 에 normal map UV 없음 | normal 이 평면 | `pMat->SetMetallic / SetRoughness` 만 사용, normal map 생략 (DefaultNormal `(0.5, 0.5, 1.0)` 1×1 텍스처 fallback) |

---

## 6. Stage 5 — Forward+ Light Cull

### 작업 단위

| 단계 | 파일 |
|---|---|
| 5-1 | `Shaders/PBR/ForwardPlus_LightCull.hlsl` 신규 (Compute) |
| 5-2 | `CForwardPlusPipeline::Create()` — Cull CS 컴파일 + UAV/SRV 셋업 |
| 5-3 | `DispatchLightCull` 구현 (04 문서 §4.2) |
| 5-4 | `Mesh3D_PBR.hlsl` PS 의 point light loop 추가 (04 문서 §5.1 AFTER) |
| 5-5 | `CEngineApp::Render()` 흐름 변경 (04 문서 §7 AFTER) |
| 5-6 | `Engine/Public/Editor/TileLightHeatmap.h` + 디버그 PS 추가 (04 문서 §8) |
| 5-7 | 100 점광원 스폰 테스트 코드 (Scene_InGame 임시) |

### 빌드 검증

```bat
fxc /T cs_5_0 /E CSMain Shaders/PBR/ForwardPlus_LightCull.hlsl /Fo nul
```

### 시각 검증

100개 점광원 (서로 다른 색, 반경 5m) 스폰 후:
1. 각 광원 주변 표면이 색상 영향 받음.
2. Tile Heatmap 토글 시 광원 밀집 영역 빨강.
3. RenderDoc 캡처 → CSMain Dispatch 가 (120, 68, 1) 호출.
4. Dispatch 후 g_LightGrid[1234] 가 (offset, count) 정상값.

### 수치 검증 (성능)

```cpp
// 프로파일러로 측정
WINTERS_PROFILE_SCOPE("Forward+ Cull");
m_pForwardPlus->DispatchLightCull(...);
WINTERS_PROFILE_SCOPE("Forward+ Shading");
m_pSceneManager->Render();
```

| 광원 수 | Cull ms (목표) | Shading ms (목표) |
|---|---|---|
| 25 | < 0.2 | < 2 |
| 100 | < 0.5 | < 3 |
| 1024 | < 3 | < 5 |

### 위험 + 롤백

| 위험 | 증상 | 롤백 |
|---|---|---|
| InterlockedAdd 경합 | 같은 슬롯 덮어씀 | groupshared 변수 사용 (이미 박제) |
| Frustum 평면 부호 반전 | 모든 광원 컬됨 (count=0) | NDC y 좌표 DX 좌표계 보정 (CullCS §3) |
| UAV 못 푼 채 다음 패스 | Shading PS 에서 SRV 바인드 실패 | `CSSetUnorderedAccessViews(0, 2, nullUAVs, nullptr)` 호출 박제 |
| MAX_LIGHTS_PER_TILE 초과 | 일부 광원 컬됨 | min() 처리 + ImGui 경고 표시 |
| TILE_SIZE != 16 | Dispatch 와 PS 의 division 불일치 | hlsl 상수 + CB 양쪽 동일 값으로 동기화 |

---

## 7. Stage 6 — MOBA 광원 통합

### 작업 단위

| 단계 | 파일 | 변경 |
|---|---|---|
| 6-1 | `Client/Private/Scene/Scene_InGame.cpp` OnEnter | 포탑 9개 + 챔프 5체에 광원 등록 |
| 6-2 | `Client/Private/IreliaFxPresets.cpp` (있는 함수 8개) | R 펄스 / Q dash / 스턴 mark 시 `AddPointLight(lifetime=...)` 호출 |
| 6-3 | `Engine/Private/Renderer/PBR/LightManager.cpp::TickLifetime` 구현 | 만료 광원 자동 제거 |
| 6-4 | `Engine/Private/Framework/CEngineApp.cpp::Update` | TickLifetime(dt) 호출 |
| 6-5 | `Client/Private/Scene/Scene_InGame.cpp` Update | `LightManager->TickLifetime` 매 프레임 |

### 시각 검증 — MOBA 시나리오

| 시나리오 | 기대 |
|---|---|
| 게임 시작 | 포탑 9개 푸른 아우라, 태양광 |
| 챔프 5체 스폰 | 각 챔프 주변 팀 색 림라이팅 |
| 이렐리아 R 발동 | 펄스 위치에 푸른 광원 0.6초 지속 → 자동 소멸 |
| Q dash | 잔광 광원 0.3초 따라감 |
| 5명 한타 (모든 스킬 동시) | 광원 약 80개 + 60fps 유지 |

### 위험 + 롤백

| 위험 | 증상 | 롤백 |
|---|---|---|
| 스킬 FX 광원 누수 | 광원 무한 증가 → 1024 도달 후 등록 거부 | 모든 `AddPointLight` 호출에 lifetime 명시 |
| TickLifetime 호출 누락 | 같은 증상 | CEngineApp::Update 에 `m_pLightManager->TickLifetime(dt)` 박제 |
| 광원 색이 너무 강해 화면 white-out | tone mapping 안 함 | Stage 7 IBL 단계에서 ACES tone mapping 추가, 또는 Stage 6 종료 시 `Reinhard` 임시 |

---

## 8. Stage 7 (선택) — IBL Prefilter

별도 작업, Phase E Stage 2 일정과 묶어 진행:
- HDR 큐브맵 (`.hdr` 또는 `.exr`) 로드
- Irradiance Map (compute) — diffuse 환경광
- Prefiltered Specular (compute, 5 mip) — 거칠기 별 환경 반사
- BRDF LUT (offline 또는 compute) — Split-Sum approximation
- `Mesh3D_PBR::PS` 에 `IBL_Diffuse + IBL_Specular` 추가

→ 이건 본 GGX+A 폴더 계획서의 범위 밖. Phase E Stage 2 별도 plan 작성.

---

## 9. 점진적 채택 — 챔프별 PBR 전환 순서

| 우선순위 | 대상 | 이유 |
|---|---|---|
| 1 | **이렐리아** | 이미 Phase 1 + Phase FX 완료, 검증된 텍스처 보유 |
| 2 | **야스오** | Phase 2 진행 중, 동일 패턴 흡수 |
| 3 | **칼리스타 / 사일러스 / 비에고** | 본체 텍스처만 (no metallic 텍스처 — `g_fMetallic=0` 고정) |
| 4 | **포탑 / 미니언** | metallic 0, roughness 0.7 통일 |
| 5 | **맵 (Layer 노드 제외)** | roughness 0.85, ambient occlusion baked |

→ 모든 모델 동시 전환 X. 챔프 1체씩 unlit/PBR 분기 검증 후 다음.

---

## 10. 전역 롤백 전략

### 단일 셰이더 롤백

```cpp
// Scene_InGame::OnEnter — 임시 비활성
m_pIrelia->Init("...irelia.fbx", L"Shaders/Skinned3D.hlsl");   // PBR 버전 대신 unlit
// vs.
m_pIrelia->InitPBR("...irelia.fbx", std::move(pMat));            // PBR 버전
```

→ `InitPBR` 한 줄을 `Init` 으로 바꾸면 즉시 unlit 복귀. 셰이더 파일은 보존 (재빌드 X).

### 전체 PBR 비활성

```cpp
// CEngineApp::Render — 분기 추가
#define WINTERS_PBR_ENABLED 0     // 0 = 기존 unlit, 1 = PBR

#if WINTERS_PBR_ENABLED
    m_pForwardPlus->BeginDepthPrepass(pCtx);
    // ...
#else
    // 기존 흐름
    m_pSceneManager->Render();
#endif
```

→ 1줄 매크로로 즉시 전체 unlit 복귀.

### Forward+ 만 비활성 (PBR 유지)

`CLightManager::AddPointLight` 를 항상 UINT32_MAX 반환하도록 임시 변경:
- 점광원 0개 → Cull CS 의 g_uLightCount=0 → 모든 타일 count=0 → PS 의 loop 0회.
- 결과: directional light 만 + ambient → Forward+ 비용 ~0.

---

## 11. 마일스톤 시점 메모리 저장

각 Stage 종료 시 `C:\Users\user\.claude\projects\C--Users-user-Desktop-Winters\memory\` 에 저장:

```markdown
[project_phase_e_stage_N.md]
Phase E Stage N (PBR + Forward+) 완료
- 신규 파일 N 개
- 검증 통과: 빌드/시각/수치 N 항목
- 미해결: ...
- 다음: Stage N+1
```

`MEMORY.md` 인덱스에도 한 줄 추가.

---

## 12. /phase-e-pbr-forward-plus 명령어 제안

`.claude/commands/phase-e-pbr-forward-plus.md` 신규 작성 (작업 진입 자동화):

```markdown
---
name: phase-e-pbr-forward-plus
description: Phase E Stage 0~6 PBR + Forward+ 작업 진입. 컨텍스트 자동 복구 + 권한 열기 포함.
---

# Phase E PBR + Forward+ 진입

## 컨텍스트 자동 복구

1. .md/plan/graphics/GGX+A/00_INDEX.md 부터 05_INTEGRATION_PHASES.md 까지 전부 Read.
2. CLAUDE.md "Phase E" 섹션 확인.
3. 현재 진행 단계 파악 (memory/project_phase_e_stage_N.md).

## 권한

- Engine/Public/Renderer/PBR/* 신규 파일 작성 권한.
- Shaders/BRDF/, Shaders/PBR/ 신규 폴더 권한.
- EngineSDK/inc 동기화 (UpdateLib.bat) 권한.

## 작업 시작

다음 미완료 Stage 의 첫 작업 단위부터 시작.
```

---

## 다음 단계 (이 폴더 외부)

본 GGX+A 계획서 5개 파일 (`00_INDEX` ~ `05_INTEGRATION_PHASES`) 완료 후:
- 실제 Stage 0 작업 진입 (`/phase-e-pbr-forward-plus` 명령어)
- IBL 단계는 별도 폴더 (`graphics/IBL+ToneMapping/`) 로 분리

## 검증 체크리스트 (계획서 자체 완료 조건)

- [x] `00_INDEX.md` — 전체 흐름 + 5 컴포넌트 역할표.
- [x] `01_THEORY_AND_MATH.md` — 모든 수식 + Furnace test + 참고 문헌.
- [x] `02_HLSL_BRDF_LIBRARY.md` — 7개 셰이더 파일 전문 코드.
- [x] `03_CPP_API_AND_CBUFFERS.md` — POD + 클래스 + cbuffer slot 매트릭스.
- [x] `04_FORWARD_PLUS_LIGHT_CULLING.md` — Cull CS 전문 + Tile Grid + 디버그.
- [x] `05_INTEGRATION_PHASES.md` (본 문서) — 7단계 + 검증 + 롤백.
- [ ] (실작업 시) Stage 0 시작 → 각 Stage 메모리 저장.
