# GGX + Schlick + Smith + Cook-Torrance + Forward+ — Winters 통합 계획서

> **대상**: 현재 unlit 파이프라인 (`Mesh3D.hlsl` / `Skinned3D.hlsl`) 위에 **물리 기반 라이팅** + **수백 개 동적 광원** 처리.
> **위치**: `C:\Users\user\Desktop\Winters\.md\plan\graphics\GGX+A\`
> **선결**: Phase 1 (이렐리아 FBX 메쉬) + Phase FX 완료 (2026-04-26).
> **예상 진입 시점**: Phase 5-B JobSystem race 정식 수정 후 → **Phase E Stage 1+2 (BRDF + Forward+)** 묶음 작업.

---

## 0. 왜 GGX + Cook-Torrance + Forward+ 인가

### 현재 상태

```hlsl
// Shaders/Mesh3D.hlsl L48-53 — 현재 PS
float4 PS(PS_INPUT input) : SV_TARGET
{
    float4 texColor = g_DiffuseMap.Sample(g_Sampler, input.vTexCoord);
    clip(texColor.a - 0.05f);
    return texColor;          // ← unlit. 라이팅 0.
}
```

- **라이팅 없음** — diffuse 텍스처를 그대로 출력. 이렐리아/야스오/맵 전체가 평면 셀룰로이드처럼 보임.
- **광원 1개도 못 처리** — `CBPerFrame` 에 `g_matViewProj` 만. 광원 cbuffer 미존재.
- **MOBA 특성** — 5v5 + 미니언 + 정글몹 + 포탑 + 스킬 FX = 한 화면에 **100+ 점광원** 잠재 (스킬 이펙트, 포탑 아우라, 탄환 광원 등).

### 도입 후

| 측면 | Before (unlit) | After (PBR + Forward+) |
|---|---|---|
| 재질 표현 | albedo 텍스처만 | albedo + metallic + roughness + AO + normal + emissive |
| 광원 수 | 0 | 수백 (동적) |
| 성능 | DrawCall 당 1 PS | DrawCall 당 1 PS + 1 Compute (Light Cull) |
| MOBA 활용 | — | 스킬 FX 광원, 포탑 아우라, 챔프 emissive 림 라이팅 |

### 5대 구성요소 역할

| 컴포넌트 | 역할 | 식 (요약) |
|---|---|---|
| **Cook-Torrance BRDF** | microfacet 반사 모델 (틀) | `f_r = D·G·F / (4·(n·l)·(n·v))` |
| **GGX (D)** | 미세면 분포 함수 (Trowbridge-Reitz) | `D = α² / (π·((n·h)²·(α²-1)+1)²)` |
| **Smith (G)** | 미세면 가림(self-shadowing) | `G = G1(n,l)·G1(n,v)`, `G1 = (n·v) / (n·v·(1-k)+k)` |
| **Schlick (F)** | Fresnel 근사 | `F = F0 + (1-F0)·(1-(h·v))⁵` |
| **Forward+** | 화면 타일별 광원 컬링 | Compute Shader 가 16×16 픽셀 타일마다 영향 광원 리스트 생성 → PS 가 그 리스트만 순회 |

---

## 1. 문서 구성

| 파일 | 내용 | 분량 추정 |
|---|---|---|
| **`00_INDEX.md`** (본 파일) | 전체 흐름, 5 컴포넌트 역할, 일정 | — |
| **`01_THEORY_AND_MATH.md`** | 렌더링 방정식부터 GGX/Smith/Schlick 수식 유도, energy conservation, 비교 검증 | 수학 전문 |
| **`02_HLSL_BRDF_LIBRARY.md`** | `Shaders/BRDF/*.hlsli` + `Mesh3D_PBR.hlsl` + `Skinned3D_PBR.hlsl` 전문 | HLSL 코드 전문 |
| **`03_CPP_API_AND_CBUFFERS.md`** | `Engine/Public/Renderer/PBR/*.h` 신규 + `ModelRenderer` 확장 + cbuffer slot 설계 | C++ 코드 전문 |
| **`04_FORWARD_PLUS_LIGHT_CULLING.md`** | Tile/Cluster 분할, Compute Shader 라이트 컬링, StructuredBuffer 광원 리스트, Depth Pre-pass | Compute 전문 |
| **`05_INTEGRATION_PHASES.md`** | 7단계 롤아웃 (BRDF first → Forward+ later), 각 단계 검증법, 위험 요소, 롤백 전략 | — |

---

## 2. 7단계 롤아웃 (요약)

```
[Stage 0] Depth Pre-pass 도입               (1일)  ← Forward+ 의 전제
[Stage 1] BRDFCommon.hlsli + GGX/Smith/Schlick (1일)  ← 순수 수학 함수
[Stage 2] Mesh3D_PBR.hlsl + Skinned3D_PBR.hlsl (1일)  ← unlit 분기 신설
[Stage 3] CMaterialPBR + cbuffer b3 + 텍스처 5종 (2일)  ← C++ 측
[Stage 4] CDirectionalLight + 단일 광원 검증     (0.5일) ← Furnace test
[Stage 5] Forward+ Light Buffer + Tile Cull CS  (3일)  ← 광원 수백
[Stage 6] CPointLight 시스템 + 스킬 FX 광원 통합 (2일)  ← MOBA 특화
[Stage 7] IBL prefilter (선택, Phase E Stage 2)  (3일)  ← 환경광
```

**총 12.5일** (단일 작업자 기준, 하루 4-6시간 코딩).

---

## 3. 디렉토리 신설

```
Engine/Public/Renderer/PBR/
├── PBRTypes.h               # CBMaterial / CBDirLight / CBPointLight / CBClusterTile (POD)
├── PBRMaterial.h            # class CPBRMaterial : WINTERS_ENGINE
├── DirectionalLight.h       # class CDirectionalLight (POD wrapper)
├── PointLight.h             # class CPointLight  (POD wrapper)
├── LightManager.h           # class CLightManager — 모든 광원 등록/StructuredBuffer 업로드
└── ForwardPlusPipeline.h    # class CForwardPlusPipeline — Tile Cull CS 관리

Engine/Private/Renderer/PBR/
├── PBRMaterial.cpp
├── LightManager.cpp
└── ForwardPlusPipeline.cpp

Shaders/BRDF/
├── BRDFCommon.hlsli         # PI / Luminance / sRGB convert
├── BRDFGGX.hlsli            # D_GGX  / G_SmithGGX / F_Schlick / DiffuseLambert
├── BRDFCookTorrance.hlsli   # Cook-Torrance 결합 함수 EvaluateBRDF()
└── BRDFLighting.hlsli       # 광원 한 개 (point/dir) 처리 함수 ApplyDirLight() / ApplyPointLight()

Shaders/PBR/
├── Mesh3D_PBR.hlsl          # 정적 메시 PBR
├── Skinned3D_PBR.hlsl       # 스키닝 메시 PBR
├── DepthPrepass.hlsl        # Stage 0
└── ForwardPlus_LightCull.hlsl  # Compute Shader (Stage 5)
```

> EngineSDK flat 복사 규칙 — 공개 헤더는 `#include "PBRTypes.h"` 처럼 파일명만 사용. `Engine.vcxproj` `AdditionalIncludeDirectories` 에 `Engine/Public/Renderer/PBR` 추가.

---

## 4. 기존 자산과의 경계 (절대 안 건드릴 것)

| 자산 | 상태 | 이유 |
|---|---|---|
| `Mesh3D.hlsl` / `Skinned3D.hlsl` | **유지** | FX/맵/디버그 빌보드 등 unlit 필요한 곳 그대로 |
| `FxSprite.hlsl` / `FxMesh.hlsl` | **유지** | 이펙트는 PBR 라이팅 받지 않음 (Additive/Premultiplied 전용) |
| `b0=PerFrame, b1=PerObject, b2=BoneMatrices` slot 규약 | **유지** | b3 부터 신규 추가 (PBR Material + Lights) |
| `ModelRenderer::Init(fbxPath, hlslPath)` 시그니처 | **유지** | hlslPath 인자로 PBR vs unlit 분기 가능 |

→ **신규 추가만**. 기존 unlit 경로를 PBR 로 교체하지 않음 (점진 도입).

---

## 5. 의존 관계도

```
[Phase 5-B JobSystem race 수정]
        │
        ↓
[Stage 0 Depth Pre-pass]──┐
        │                  │
        ↓                  │
[Stage 1+2 BRDF 셰이더]    │
        │                  │
        ↓                  │
[Stage 3 CMaterialPBR]    │
        │                  │
        ↓                  │
[Stage 4 단일 광원 검증]──┤
        │                  │
        ↓                  ↓
[Stage 5 Forward+] ────────┘  ← Depth Pre-pass + Light Cull CS 동시 의존
        │
        ↓
[Stage 6 MOBA 광원 통합]
        │
        ↓
[Stage 7 IBL] (Phase E Stage 2)
```

---

## 6. 검증 결정 포인트 (winters-skills/code/SKILL.md 패턴)

각 Stage 완료 시 **빌드 통과 + 시각 검증 + 수치 검증** 셋 다 통과 후 다음 진입.

| Stage | 빌드 검증 | 시각 검증 | 수치 검증 |
|---|---|---|---|
| 0 Depth Pre-pass | Mesh3D 가 depth-only pass 한 번 통과 후 정상 렌더 | 스크린샷 동일 | RenderDoc 에서 depth buffer 가 1st pass 로 채워짐 |
| 1 BRDF math | hlsli 컴파일 통과 | — | Furnace test (백색 환경광 + roughness=1 metal=0 → albedo=0.18 입력 시 출력 0.18 ±2%) |
| 2 PBR shader | Mesh3D_PBR.hlsl 컴파일 통과 | unlit 과 같은 albedo + 광원 0개 시 동일 결과 | albedo 만 출력 = unlit 동일 |
| 3 C++ 측 | Engine.dll 빌드 + Client 링크 통과 | albedo/metallic/roughness 슬라이더 ImGui 노출 | metallic=1 시 검정 (광원 0개 가정) |
| 4 단일 광원 | 빛 방향 변경 시 명도 변화 | dot(N,L) 로 음영 | 정중앙 (n·l=1) 광량 = light intensity (단위 검증) |
| 5 Forward+ | Tile Cull CS 컴파일 + UAV 바인딩 통과 | 100개 점광원 산포 시 60fps | LightCount per Tile RenderDoc 시각화 |
| 6 MOBA 통합 | 스킬 발동 시 FX 위치에 광원 스폰 | 이렐리아 R 펄스에서 주변 챔프 림 라이팅 | 스킬 종료 시 광원 0개 복귀 |

---

## 7. 위험 요소 및 사전 대비

### A. cbuffer 16바이트 정렬 오정렬

- HLSL `cbuffer` 는 16B 단위로 패킹. C++ POD 와 불일치 시 GPU 가 쓰레기 값 읽음.
- **대응**: `static_assert(sizeof(CBMaterial) % 16 == 0)` 모든 POD 에 박제. `_pad0/_pad1` 명시.

### B. PBR 파라미터 hue shift (Color Space)

- albedo 가 sRGB 일 경우 **선형 공간 변환 후 라이팅 후 다시 sRGB** 출력 안 하면 색상 어긋남.
- **대응**: `BRDFCommon.hlsli` 에 `SRGBtoLinear()` / `LinearToSRGB()` 박제. `g_DiffuseMap` 이미 sRGB texture format 으로 로드돼있다면 자동 변환되므로 추가 코드 X.

### C. Forward+ Tile 크기 결정

- 16×16 (NVIDIA 추천) vs 32×32 (덜 정밀하지만 오버헤드 ↓).
- **대응**: 16×16 으로 시작 + ImGui 디버그 패널에 LightCount per Tile 히트맵 노출. 핫스팟 발견 시 32×32 로 fallback.

### D. Light StructuredBuffer 크기

- 점광원 1개 = 32B (pos+radius+color+intensity). 1024개 = 32KB. b 슬롯 (cbuffer) 대신 `StructuredBuffer<PointLight> : register(t10)` 사용.
- **대응**: `Engine/Public/RHI/DX11/DX11StructuredBuffer.h` 이미 존재. `BoneMatrix` alias 와 동일 패턴으로 `PointLightGPU` alias 추가.

### E. Skinned3D_PBR 의 normal 변환 정확도

- 스키닝 행렬에 비균등 스케일 있으면 `(float3x3)skinMatrix` 로 normal 변환 시 직교 손상 → BRDF 결과 왜곡.
- **대응**: `transpose(inverse(skinMatrix))` 사용 또는 챔프 FBX 가 균등 스케일임을 가정 (현재 LoL FBX 는 모두 균등).

---

## 8. CLAUDE.md 컨벤션 준수 체크리스트

- [x] **C 접두사**: `class CPBRMaterial`, `class CDirectionalLight`. struct (POD) 는 `CB` 접두사 (`CBMaterial`).
- [x] **타입 별칭**: `f32_t`/`u32_t`/`bool_t`. raw float/int 금지.
- [x] **flat include**: `#include "PBRTypes.h"` (서브폴더 금지).
- [x] **WINTERS_ENGINE export**: `CPBRMaterial`, `CLightManager`, `CForwardPlusPipeline` 만. 내부 구조체 export 금지.
- [x] **ComPtr FQN**: 공개 헤더는 `Microsoft::WRL::ComPtr<...>` + `#include <wrl/client.h>` 명시.
- [x] **DECLARE_SINGLETON 미사용**: PBR 시스템은 매니저가 아닌 **컴포넌트** 형태 — `CGameInstance::Get_LightManager()` 게터로 접근.
- [x] **GameInstance Tier-2 게터**: `Get_LightManager()`, `Get_ForwardPlusPipeline()` (RHI 류).

---

## 9. 다음 단계

→ `01_THEORY_AND_MATH.md` 부터 순서대로 읽고 구현. 각 문서 끝에 **다음 문서로의 링크** + **이 단계 완료 검증법** 박제.

**작업 진입 명령어** (제안): `/phase-e-pbr-forward-plus` (Phase 2 야스오 완료 후 도입).
