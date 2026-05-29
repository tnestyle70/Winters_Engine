# Graphics (Winters 이식 시리즈) — INDEX

> 원전: `C:\Users\user\Desktop\.markdown\Graphics\` (범용 이론/구현 가이드 6편)
> 이 디렉토리: Winters 엔진 경로/규약/레지스터로 **그대로 컴파일되는 형태** 로 번역.
> 상위 Phase 계획: `.md/plan/graphics/00_GRAPHICS_PLAN_INDEX.md` (Stage 1~8 로드맵)

---

## 0. 이 시리즈의 성격

- **원전**: 이론 유도 + 범용 HLSL/C++ 스케치.
- **본 시리즈 (6편)**: Winters DLL 경계, `WintersTypes.h` / `DX11ConstantBuffer<T>` / `CBPerFrame` / `CGameInstance` Tier 경계 전부 준수한 **이식본**.
- 원전이 `float / int` 로 쓴 곳도 본 시리즈는 `f32_t / i32_t / u32_t / wstring_t` 으로 옮긴다.
- 공개 헤더는 **flat include 만** (EngineSDK 복사 규약). 각 문서의 §디렉토리/§레지스터 표 참조.

---

## 1. 6 문서 매핑

| # | Winters 문서 | 원전 | 대응 Phase E Stage |
|---|-------------|------|-------------------|
| 01 | [01_GGX_BRDF_WINTERS.md](01_GGX_BRDF_WINTERS.md) | `01_GGX_BRDF_Complete.md` | **Stage 1 (BRDF)** |
| 02 | [02_Clustered_Deferred_WINTERS.md](02_Clustered_Deferred_WINTERS.md) | `02_Clustered_Deferred.md` | **Stage 2 (PBR/Deferred)** |
| 03 | [03_TAA_WINTERS.md](03_TAA_WINTERS.md) | `03_TAA_Implementation.md` | **Stage 7 (Temporal)** |
| 04 | [04_IBL_Prefiltering_WINTERS.md](04_IBL_Prefiltering_WINTERS.md) | `04_IBL_Prefiltering.md` | **Stage 2 (IBL)** |
| 05 | [05_Motion_Vectors_WINTERS.md](05_Motion_Vectors_WINTERS.md) | `05_Motion_Vectors.md` | **Stage 7 전제** |
| 06 | [06_FromSoft_Lessons_WINTERS.md](06_FromSoft_Lessons_WINTERS.md) | `06_FromSoft_Analysis.md` | **전 Stage 결정 기록** |

---

## 2. 권장 구현 순서 (의존성 기준)

```
01  BRDF (Mesh3D_PBR / Skinned3D_PBR 신설)
     │
     ▼
05  Motion Vectors (TransformComp.matPrevWorld, MV RT, PrevBoneMgr)
     │
     ▼
02  Clustered Deferred (GBuffer MRT 3장 + Cull CS + Lighting PS)
     │
     ▼
04  IBL Prefiltering (BRDF LUT + SH9 + Prefilter mips, 임시 ambient 대체)
     │
     ▼
03  TAA (Jitter + History Ping-Pong + Variance Clipping)
     │
     ▼
06  FromSoft 결정 (Shadow/SSAO/SSR/Volumetric/LUT — Stage 5 이후 확장)
```

**반드시 이 순서**. MV 없이 TAA 넣으면 화면 떨림, BRDF 없이 Clustered 넣으면 합성할 재료 없음.

---

## 3. 한눈에 보는 디렉토리 신설

```
Engine/Public/Renderer/
├── BRDF/             (문서 01) 5 header
├── Clustered/        (문서 02) 3 header
├── IBL/              (문서 04) 3 header
├── MotionVector/     (문서 05) 3 header
├── PostFX/TAA/       (문서 03) 2 header
├── Shadow/           (문서 06 §4) ShadowCSM.h
├── Volumetric/       (문서 06 §5) VolumetricFog.h
└── PostFX/ColorLUT.h (문서 06 §7.2)

Engine/Private/Renderer/
├── BRDF/ Clustered/ IBL/ MotionVector/ PostFX/TAA/ Shadow/ Volumetric/

Shaders/
├── BRDF/                       (01) Mesh3D_PBR / Skinned3D_PBR / GGX hlsli / IS hlsli / Common
├── Clustered/                  (02) BuildClusterAABB / CullLights / LightingPass / ClusterDebug
├── IBL/                        (04) EquirectToCubemap / PrefilterEnv / GenerateBRDFLUT / IBLSample
├── MotionVector/               (05) CameraOnlyMV / Debug
├── PostFX/TAA/                 (03) TAAResolve + Common hlsli
└── Tests/                      (01 §10) FurnaceTest

Tools/
└── WintersIBLBaker/            (04 §6) main.cpp + IBLBaker.h/cpp + vcxproj

Engine/Asset/
└── brdf_lut.dds                (04 §0) 공용 에셋

Resource/
├── IBL/<map>.ibl               (04 §0) 맵별 에셋 (SH9 + Prefilter DDS)
└── LUT/<region>.dds            (06 §7.2) 3D LUT 32³
```

---

## 4. HLSL 전역 레지스터 표 (문서 05 §17 에서 확정)

| slot | 용도 | 정의 문서 |
|------|------|----------|
| b0 | CBPerFrame (matViewProj, matViewProjNoJitter, cameraPos, time) | 01 |
| b1 | CBPerObject (matWorld) | 기존 |
| b2 | CBBones (현재 스키닝) | 기존 |
| b3 | CBMaterial | 01 |
| b4 | CBDirLight | 01 |
| b5 | CBClusterUniforms | 02 |
| b6 | CBIBL | 04 |
| b7 | CBPerFrameMV | 05 |
| b8 | CBPerObjectPrev | 05 |
| b9 | CBPrevBones | 05 |
| b10+ | Skin SSS 등 캐릭터 전용 | 06 |
| t0~t3 | Material (Albedo/Normal/MRA/Emissive) | 01 |
| t5 | IBL Prefilter Cube | 04 |
| t6 | IBL BRDF LUT | 04 |
| t8~t10 | Clustered (ClusterInfo / IndexList / Lights) | 02 |
| t11~t12 | SSS Curvature / Pre-Integrated LUT | 06 |
| s0 | PointClamp | — |
| s1 | LinearClamp | 03 |
| s2 | LinearClamp (IBL mip 전용) | 04 |

신규 Stage 도입 시 본 표를 먼저 업데이트하고 시작할 것.

---

## 5. 프레임 파이프라인 최종 타겟 (문서 06 §1)

```
0  UpdateTAAJitter                                (문서 03 §4)
1  Shadow CSM                                      (문서 06 §4.1)
2  GBuffer Pass (Mesh3D_PBR / Skinned3D_PBR, MRT 3) (문서 02 §8 + 05 §5~6)
3  CameraOnlyMV Fill                               (문서 05 §9)
4  SSAO / Contact Shadow                            (문서 06 §4.3~4.4)
5  Volumetric Fog (Froxel)                          (문서 06 §5)
6  Clustered Cull CS                                (문서 02 §7)
7  Lighting PS + IBL ambient                        (문서 02 §8 + 04 §4)
8  Translucent / Forward (머리카락, 파티클)
9  SSR (IBL fallback)                               (문서 06 §6)
10 TAA Resolve                                     (문서 03)
11 Bloom                                           (Phase E Stage 6, 별도 문서)
12 Tonemap (ACES) + LUT                            (문서 06 §7)
13 UI / ImGui
14 Present
```

---

## 6. Definition of Done 묶음

각 문서의 §14~15 "Stage 완료 기준" 을 한 번에:

### Stage 1 (문서 01)
- [ ] `CBRDFMaterial`, BRDF hlsli 5 파일, `Mesh3D_PBR` / `Skinned3D_PBR` 빌드 통과
- [ ] Furnace Test (rough 0~1) 에서 Stage 2 IBL 전에도 0.4+ 기록
- [ ] ImGui 튜너로 이렐리아 Material 실시간 변경

### Stage 5 전제 (문서 05)
- [ ] `TransformComponent::matPrevWorld` + `bSpawnedThisFrame`
- [ ] `CMotionVectorPass` + `CPrevBoneManager`
- [ ] GBuffer MRT[2] = R16G16_FLOAT MV
- [ ] 단위 테스트 3종 (정지/카메라/캐릭터) 통과

### Stage 2 Clustered (문서 02)
- [ ] `CClusteredLighting` + 5 HLSL 파일
- [ ] 100 point light @ 1080p 60fps
- [ ] Heatmap 토글

### Stage 2 IBL (문서 04)
- [ ] `WintersIBLBaker.exe` + `brdf_lut.dds` + 1+ IBL 에셋
- [ ] Furnace (rough=1 white metal white env) ≥ 0.95
- [ ] 이렐리아 칼날 환경 반사 명확

### Stage 7 TAA (문서 03)
- [ ] `CTAAResolve` 빌드 통과
- [ ] 정지/회전/빠른 이동 시나리오 통과
- [ ] Mip bias / variance gamma ImGui 튜너

### Stage 5+ 확장 (문서 06)
- [ ] CSM 4 cascade
- [ ] Froxel Volumetric Fog (5ms 이내)
- [ ] SSR + IBL fallback
- [ ] Color LUT 3D 로드 + crossfade

---

## 7. CLAUDE.md / 엔진 컨벤션과의 정합성

본 시리즈 6 문서 전체가 아래 규칙을 **일괄 준수**:

| 규칙 | 준수 방식 |
|------|----------|
| `C`/`I` 접두사 | `CBRDFMaterial`, `CClusteredLighting`, `IIBLManager` … |
| 파일명 `C` 금지 | `BRDFMaterial.h` (클래스 `CBRDFMaterial`) |
| `f32_t`/`u32_t`/`wstring_t` | 신규 코드 전면 적용 |
| `WINTERS_ENGINE` + `unique_ptr` = copy delete | 본 시리즈 모든 공개 클래스 §3/§4/§5 표기 |
| 공개 헤더 flat include | 서브폴더 include 금지, `Engine.vcxproj` AdditionalIncludeDirectories 에 서브폴더 추가 |
| ImGui 튜너 의무 | 문서 01 §11, 02 §13, 03 §11, 04 §11, 06 §12 |
| `CGameInstance` Tier 2 Getter | 문서 02 §10, 04 §8 (IClusteredLighting / IIBLManager) |
| HLSL `row_major matrix` | 전 HLSL 파일에서 명시 |
| cbuffer 16B 정렬 | `static_assert(sizeof(T) % 16 == 0)` 전 공유 타입에 첨부 |
| Release 로그 금지 | `OutputDebugString`/`printf` 는 `#ifdef _DEBUG` 안에만 (신규 코드 주석으로 강제) |

---

## 8. 원전 참고 (변경 없음)

6 원전 문서의 §참고 자료 섹션을 그대로 유지. Real-Time Rendering 4th / PBR 3rd / Karis 2013 / Heitz 2014 / Walter 2007 / Burley 2012 / Jimenez 2010 / Hillaire 2015 / Salvi 2016 / Ramamoorthi 2001 / Fdez-Aguera 2019 등.

---

## 9. 다음 단계

본 시리즈 완료 후 **별도 문서로 이어질 내용** (본 시리즈 범위 밖):

- `07_Bloom_FFT_WINTERS.md` — Phase E Stage 6 (Cooley-Tukey + Convolution)
- `08_Ocean_FFT_WINTERS.md` — Phase E Stage 6 (Tessendorf, 엘든링 해변용)
- `09_PathTracer_WINTERS.md` — Phase E Stage 4 (Compute, Ground Truth)
- `10_VXGI_DDGI_WINTERS.md` — Phase E Stage 5 (실시간 GI)
- `11_DXR_Hybrid_WINTERS.md` — Phase E Stage 8 (선택, DX12 백엔드 전제)

---

*인덱스 끝. 여기부터 시작해 01 → 05 → 02 → 04 → 03 → 06 순으로 구현한다.*
