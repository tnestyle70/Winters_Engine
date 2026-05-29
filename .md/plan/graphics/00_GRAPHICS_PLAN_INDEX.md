# Phase E — 자체 고급 렌더링 구현 계획 (인덱스)

## 2026-05-18 현재 우선 트랙

LoL/Riot풍 챔피언 품질은 PBR 확장보다 **diffuse-only stylized lighting**을 먼저 적용한다.

| 순서 | 세션 | 문서 |
|---|---|---|
| Roadmap | DX11 render quality session roadmap | [2026-05-20_DX11_RENDER_QUALITY_SESSION_ROADMAP.md](2026-05-20_DX11_RENDER_QUALITY_SESSION_ROADMAP.md) |
| 0 | 코드 분석/판단 근거 | [../../research/2026-05-18_DIFFUSE_ONLY_RIOT_RENDER_AUDIT.md](../../research/2026-05-18_DIFFUSE_ONLY_RIOT_RENDER_AUDIT.md) |
| 1 | diffuse stylized lighting | [2026-05-18_SESSION_01_DIFFUSE_STYLIZED_LIGHTING.md](2026-05-18_SESSION_01_DIFFUSE_STYLIZED_LIGHTING.md) |
| 2 | non-PBR SSAO/contact AO | [2026-05-18_SESSION_02_SSAO_NON_PBR_CONTACT.md](2026-05-18_SESSION_02_SSAO_NON_PBR_CONTACT.md) |
| 3 | shader-local sRGB | [2026-05-18_SESSION_03_SHADER_LOCAL_SRGB.md](2026-05-18_SESSION_03_SHADER_LOCAL_SRGB.md) |
| 4 | Render Debug SSAO tuning | [2026-05-18_SESSION_04_RENDER_DEBUG_SSAO_TUNING.md](2026-05-18_SESSION_04_RENDER_DEBUG_SSAO_TUNING.md) |
| 5 | DX11 reference SSAO tuning controls | [2026-05-20_SESSION_05_DX11_SSAO_TUNING.md](2026-05-20_SESSION_05_DX11_SSAO_TUNING.md) |
| 6 | map diffuse ramp / ambient discipline | [2026-05-20_SESSION_06_MAP_DIFFUSE_RAMP_AMBIENT.md](2026-05-20_SESSION_06_MAP_DIFFUSE_RAMP_AMBIENT.md) |
| 7 | champion readability rim / top light | [2026-05-20_SESSION_07_CHAMPION_READABILITY_RIM.md](2026-05-20_SESSION_07_CHAMPION_READABILITY_RIM.md) |
| 8A | UI/indicator unlit shader split | [2026-05-20_SESSION_08A_UI_UNLIT_SHADER_SPLIT.md](2026-05-20_SESSION_08A_UI_UNLIT_SHADER_SPLIT.md) |
| 8 | non-PBR point light accents | [2026-05-20_SESSION_08_NONPBR_POINT_LIGHT_ACCENTS.md](2026-05-20_SESSION_08_NONPBR_POINT_LIGHT_ACCENTS.md) |
| 9 | AO propagation to objects | [2026-05-20_SESSION_09_AO_PROPAGATION_OBJECTS.md](2026-05-20_SESSION_09_AO_PROPAGATION_OBJECTS.md) |
| 10 | ground contact shadow / decal | [2026-05-20_SESSION_10_GROUND_CONTACT_SHADOW_DECAL.md](2026-05-20_SESSION_10_GROUND_CONTACT_SHADOW_DECAL.md) |
| 11 | Fog/FOW color integration | [2026-05-20_SESSION_11_FOG_FOW_COLOR_INTEGRATION.md](2026-05-20_SESSION_11_FOG_FOW_COLOR_INTEGRATION.md) |
| 12 | tone/gamma/mip filtering audit | [2026-05-20_SESSION_12_TONE_GAMMA_MIP_FILTER_AUDIT.md](2026-05-20_SESSION_12_TONE_GAMMA_MIP_FILTER_AUDIT.md) |
| 13 | DX12/Vulkan parity boundary | [2026-05-20_SESSION_13_DX12_VULKAN_PARITY_BOUNDARY.md](2026-05-20_SESSION_13_DX12_VULKAN_PARITY_BOUNDARY.md) |

## 비전

상용 렌더링 프레임워크 (Unreal RHI, Filament, bgfx) 없이 **물리 기반 렌더링 이론** 을 C++20 +
HLSL 로 직접 구현. MOBA 실시간 파이프라인부터 연구용 Path Tracer 까지 전부 자체.

## 왜 자체 구현인가

- **포트폴리오 목적** — 쿠벨카-뭉크 / 몬테카를로 / BRDF 수학을 실제 이해하고 구현한 이력
- **Phase D 와 수학 공유** — BVH, 난수, 확률 분포가 물리 시뮬과 겹침
- **교육적 가치** — 최신 그래픽스 연구 (Real-Time Rendering 4th Ed., PBR 3rd Ed.) 체화
- **게임 맞춤 최적화** — MOBA 는 상대적으로 가벼운 라이팅 필요 — 범용 엔진 오버스펙 회피

## Stage 로드맵 (1~8)

| Stage | 내용 | 난이도 | 문서 |
|---|---|---|---|
| 1 | BRDF 이론 + 구현 (Lambertian → GGX → Disney) | 🟢 | [02_STAGE1_BRDF.md](02_STAGE1_BRDF.md) |
| 2 | PBR 파이프라인 (Metallic/Roughness + IBL) | 🟡 | [03_STAGE2_PBR.md](03_STAGE2_PBR.md) |
| 3 | 몬테카를로 적분 (이론 + Importance Sampling + MIS) | 🟡 | [04_STAGE3_MONTE_CARLO.md](04_STAGE3_MONTE_CARLO.md) |
| 4 | 오프라인 Path Tracing (Reference 렌더러) | 🔴 | [05_STAGE4_PATH_TRACING.md](05_STAGE4_PATH_TRACING.md) |
| 5 | 실시간 GI (SSAO, SSR, VXGI, DDGI) | 🔴 | [06_STAGE5_REALTIME_GI.md](06_STAGE5_REALTIME_GI.md) |
| 6 | FFT 기반 기법 (Ocean + Bloom + DoF) | 🔴 | [07_STAGE6_FFT.md](07_STAGE6_FFT.md) |
| 7 | Temporal 기법 (TAA + Reprojection) | 🟡 | [08_STAGE7_TEMPORAL.md](08_STAGE7_TEMPORAL.md) |
| 8 | Hardware Ray Tracing (DXR) — 선택 | 🔴 | [09_STAGE8_DXR.md](09_STAGE8_DXR.md) |

## 공통 시스템

| 주제 | 문서 |
|---|---|
| 아키텍처 + RenderGraph + 디렉토리 | [01_ARCHITECTURE.md](01_ARCHITECTURE.md) |
| 디버그 시각화 + ImGui 튜너 + 프레임 Capture | [10_DEBUG_TOOLS.md](10_DEBUG_TOOLS.md) |

## 구현 순서 (의존성 기준)

```
Stage 1 BRDF              ← 모든 라이팅 기반
     ↓
Stage 2 PBR + IBL          ← 실시간 품질 향상
     ↓
Phase 2 RenderGraph        ← 멀티패스 관리 필요
     ↓
Stage 7 TAA                ← 안티앨리어싱 (Stage 2 와 병행 가능)
     ↓
Stage 3 Monte Carlo        ← 수학 이론 (Stage 4 준비)
     ↓
Stage 4 Path Tracing       ← Reference 렌더러 (Ground Truth)
     ↓
Stage 5 실시간 GI          ← Path Tracer 와 비교 검증
     ↓
Stage 6 FFT (Ocean/Bloom)  ← 독립적, 언제든
     ↓
Stage 8 DXR (선택)         ← GPU 세대 요구
```

## MOBA / 엘든링 사용 계획

| Stage | MOBA (LoL) | 엘든링 모작 |
|---|---|---|
| 1 BRDF | GGX 필수 (챔피언 스킨 | PBR) | ✅ |
| 2 PBR IBL | ✅ 환경 반사 | ✅ 필수 |
| 3 Monte Carlo | 오프라인 라이트맵 베이킹 | Path Tracing reference |
| 4 Path Tracing | 스크린샷 오프라인 | 연구/튜닝 용 |
| 5 실시간 GI | SSAO/SSR 필수 | VXGI / DDGI |
| 6 FFT Ocean | ❌ | ✅ (해변 스테이지) |
| 6 FFT Bloom | ✅ (스킬 이펙트) | ✅ |
| 7 TAA | ✅ (60FPS 품질) | ✅ (144FPS) |
| 8 DXR | ❌ (대중 GPU 지원 위해) | ⚠️ 선택 |

## Phase D 와의 수학 공유

| 자료구조/수학 | Phase D Physics | Phase E Graphics |
|---|---|---|
| BVH (SAH 빌드) | Broad Phase | Path Tracing 가속 |
| Ray-AABB / Ray-Sphere | Raycast Query | Ray Generation |
| 난수 / 확률 분포 | PBD (랜덤 초기 속도) | Monte Carlo 적분 |
| 저분산 시퀀스 (Halton/Sobol) | — | Stratified Sampling |
| Quaternion | Rigid Body 회전 | Light / Camera 오리엔테이션 |

## 참고 문헌

### 필수
- **Real-Time Rendering 4th Ed.** — Akenine-Möller/Haines/Hoffman (업계 표준)
- **Physically Based Rendering: From Theory to Implementation 3rd Ed.** — Pharr/Jakob/Humphreys (PBRT, 오픈북)
- **GPU Gems 1/2/3** — NVIDIA (무료)
- **SIGGRAPH Course: Physically Based Shading** (연도별 최신)

### 논문
- **Cook & Torrance 1982** — Microfacet BRDF
- **Walter et al. 2007** — GGX NDF
- **Karis 2013** — UE4 Shading (Split-Sum)
- **Burley 2012** — Disney Principled BSDF
- **Kajiya 1986** — Rendering Equation
- **Tessendorf 2001** — Ocean Wave FFT
- **Crassin 2011** — VXGI (Voxel Cone Tracing)
- **McGuire 2019** — DDGI
- **Karis 2014** — TAA (UE4)

### 코드 참고
- **PBRT** 소스 (C++) — https://pbr-book.org
- **Falcor** (NVIDIA) — DXR 활용
- **FSR / DLSS** 문서

## 의존성

| 필요 | 상태 |
|---|---|
| DX11 RHI | ✅ |
| Compute Shader | ✅ |
| RenderGraph | ⏭️ Phase 2 (중요) |
| G-Buffer | ⏭️ Phase 2 |
| Target_Manager | ⏭️ Phase 2 |
| JobSystem | 🔄 Phase 1b |
| DXR 지원 GPU | 선택 |

## 예상 소요

- Stage 1 BRDF: 1주
- Stage 2 PBR+IBL: 2주
- Stage 3 몬테카를로: 1주 (이론 학습 + 샘플러)
- Stage 4 Path Tracing: 3~4주 (BVH + 재질 분기 + Compute 셰이더)
- Stage 5 실시간 GI: 4~6주 (각 기법별 1~2주)
- Stage 6 FFT: 2주 (Cooley-Tukey + Ocean Tessendorf)
- Stage 7 TAA: 2주 (motion vector + history)
- Stage 8 DXR: 3~4주 (선택)
