# 06. FromSoft 엔진 관찰을 Winters 로 번역

> 원전: `C:\Users\user\Desktop\.markdown\Graphics\06_FromSoft_Analysis.md` (관찰/추정)
> 이 문서: Winters 엔진에 **어떤 결정을 내리고 어떤 코드를 짠다** 의 번역본.
> 원전이 "왜 그들은 그렇게 했는가" 라면, 본 문서는 **"Winters 는 무엇을 하고 무엇을 안 하는가"**.

---

## 0. 전제 — Winters 의 "안 할 것" 목록

CLAUDE.md 의 게임 콘텐츠 목표와 FromSoft 의 "대담한 제약" 철학을 합쳐, 다음을 **명시적 비목표** 로 선언:

| 항목 | 결정 | 이유 |
|------|------|-----|
| Lumen급 실시간 GI | ❌ 안 함 | MOBA 는 안정 60fps + 조명이 아트 디렉션의 핵심 요소 아님 |
| Nanite급 가상화 지오메트리 | ❌ 안 함 | 챔피언 ≤ 10 + 미니언 소수, 지오메트리 병목 아님 |
| 초대형 오픈월드 스트리밍 | ❌ 안 함 | LoL 맵 크기 제한적 (한 번에 전부 VRAM 상주) |
| VR / Stereo | ❌ 안 함 | 대상 플랫폼 외 |
| DXR 하드웨어 RT (Phase E Stage 8) | ⚠️ 선택 | 엘든링 모작 후 고려 |
| OIT (Order-Independent Transparency) | ❌ Phase 1 에선 안 함 | 머리카락 정렬 문제는 alpha-sort 로 해결 |
| 라이트맵 베이킹 파이프 | ✅ 한다 | 장기 — 맵이 정적 + 지역별 시간대 |
| Froxel Volumetric Fog | ✅ 한다 | 비용 대비 효과 최상 (§5) |

FromSoft 의 승리 공식: "**기술 선택이 아트 디렉션을 지지한다**". Winters 가 따를 공식도 같다.

---

## 1. 렌더 파이프라인 (Winters 최종 목표 구조)

원전 §2 을 Winters 규약으로 옮기면:

```
[프레임 시작]
0.  UpdateTAAJitter           (문서 03)
1.  Shadow CSM Pass           (본 문서 §4)      — Phase E Stage 5 중
2.  GBuffer Pass              (문서 02 §8)
       ├─ Mesh3D_PBR / Skinned3D_PBR (MV MRT 포함 — 문서 05)
       └─ World-space normal 저장 (IBL 호환)
3.  CameraOnlyMV Fill         (문서 05 §9)      — 정적 배경
4.  SSAO / Contact Shadow     (본 문서 §4.3)    — Phase E Stage 5
5.  Volumetric Fog - Froxel   (본 문서 §5)      — Phase E Stage 5 후반
6.  Clustered Cull CS         (문서 02)
7.  Lighting PS (+ IBL ambient) (문서 02 §8 + 04 §4)
8.  Translucent / Forward     (머리카락, 파티클) — Phase G 이후
9.  SSR                       (본 문서 §6)      — Phase E Stage 5
10. TAA Resolve               (문서 03)
11. Bloom (FFT)               Phase E Stage 6
12. Tonemap (ACES + LUT)      (본 문서 §7)
13. UI / ImGui                (ImGui 는 LDR 백버퍼에서)
14. Present
```

각 라인은 별도 **문서가 존재**. 이 문서는 **묶음/결정** 만 기록.

---

## 2. API 선택 — DX11 을 유지한다

엘든링/SOTE 는 D3D12. Winters 는 **D3D11 유지** 결정:

- 장점: 이미 RHI 완비 (`CDX11Device`, `DX11VertexBuffer` 등), 학습 곡선 낮음
- 단점: RT, Mesh Shader, Sampler Feedback 등 D3D12 전용 기능 불가
- 대응: **자체 "command list 추상층" 도입 금지** (DX12 로 이식 당분간 안 함). RenderGraph (`.md/plan/graphics/01_ARCHITECTURE.md`) 는 DX11 위에서도 충분.
- DXR 필요 시점 (Phase E Stage 8, 엘든링 모작) 에 한해 DX12 백엔드 추가 검토.

**결정**: 본 Stage 시리즈 (01~06) 는 전부 DX11 로 돌아간다. 변환 안 한다.

---

## 3. GBuffer 구조 (Winters 확정)

원전 §3 의 엘든링 추정 레이아웃을 Winters 에 맞게:

| RT | Format | R | G | B | A |
|----|--------|---|---|---|---|
| `RT0` | `R8G8B8A8_UNORM_SRGB` | Albedo.r | Albedo.g | Albedo.b | Metallic |
| `RT1` | `R10G10B10A2_UNORM` | OctNormal.x | OctNormal.y | Roughness | AO |
| `RT2` | `R16G16_FLOAT` | MotionVec.x | MotionVec.y | — | — |
| `Depth` | `D32_FLOAT` | — | — | — | — |

> 원전은 view-space normal 추정하지만 Winters 는 **world-space normal** 로 통일 (IBL `EvaluateIBL` 가 world 가정, 문서 04 §4 참조). 뷰 스페이스로 갈 필요가 있다면 Lighting PS 에서 `matView` 곱하기.

- Emissive 는 별도 RT 쓰지 않고 **Lighting PS 끝에 직접 합산** (GBuffer 4장은 과함).
- Forward 머티리얼 (머리카락, 파티클) 이 들어오면 **4번째 RT (R16G16B16A16_FLOAT)** 를 추가해 scene color + alpha 조합. Phase G 이후.

---

## 4. Shadow / AO — "Baked + Realtime 하이브리드" 원칙

### 4.1 CSM (Cascaded Shadow Map) — 태양만

- 4 cascade (원전 §5.1)
- Winters 초기 구현: 2048² × 4 = 16 MB shadow atlas
- `Engine/Public/Renderer/Shadow/ShadowCSM.h` — class `CShadowCSM`
- Split 거리: 5m / 20m / 100m / 500m+ (원전 권장)

```cpp
// Engine/Public/Renderer/Shadow/ShadowCSM.h
class WINTERS_ENGINE CShadowCSM
{
public:
    static std::unique_ptr<CShadowCSM> Create(ID3D11Device* pDev, u32_t uCascadeRes = 2048);
    CShadowCSM(const CShadowCSM&) = delete;
    CShadowCSM& operator=(const CShadowCSM&) = delete;
    CShadowCSM(CShadowCSM&&) = default;
    CShadowCSM& operator=(CShadowCSM&&) = default;
    ~CShadowCSM();

    void Render(/* scene, light */);
    ID3D11ShaderResourceView* GetAtlasSRV() const;
    void BindForLighting(ID3D11DeviceContext* pCtx, u32_t uSlot);
private:
    struct Impl; std::unique_ptr<Impl> m_pImpl;
};
```

PCF 3×3 또는 5×5 + cascade blend band. `SamplerComparisonState` 활용.

### 4.2 Point/Spot Shadow — Atlas

원전 §5.4 따라 **단일 Atlas 4096²** 에 패킹. 동적 광원만 실시간, 나머지는 라이트맵에 굽는다 (Phase E Stage 5 후반).

### 4.3 Contact Shadow — Screen-Space Raymarch

원전 §5.3 은 "캐릭터가 바닥에 붙는 느낌" 의 비결로 지목. 비싸지 않고 효과 큼 → Winters Stage 5 에 포함.

```hlsl
// Shaders/GI/ContactShadow.hlsli
float ContactShadow(float3 worldPos, float3 lightDir, Texture2D depth, SamplerState s)
{
    const uint  N_STEPS = 16;
    const float STEP    = 0.015f;

    float3 rayStart = worldPos;
    for (uint i = 0; i < N_STEPS; ++i)
    {
        rayStart += lightDir * STEP;
        float4 clip = mul(float4(rayStart, 1.f), g_matViewProj);
        float2 uv   = clip.xy / clip.w * float2(0.5f, -0.5f) + 0.5f;
        float  d    = depth.Sample(s, uv).r;
        // 깊이 차로 blocker 탐지
        if ( /* linearize(d) < linearize(clip.z/clip.w) - 0.02 */ )
            return 0.0f;   // shadowed
    }
    return 1.0f;
}
```

Lighting PS 에서 direct 성분에 곱함.

### 4.4 SSAO / HBAO+

원전 §2.1 GTAO 추정. Winters 는 **SSAO 부터 HBAO+ 로 단계 상승**:

- Phase E Stage 5 초반: SSAO (Crytek 2007 스타일, `Shaders/GI/SSAO/SSAO.hlsl`)
- Phase E Stage 5 중반: HBAO+ (`Shaders/GI/SSAO/HBAOPlus.hlsl`)
- 입력: Normal (GBuffer), Depth
- 출력: 단일 `R8_UNORM` occlusion RT → Lighting PS `ao *=`

---

## 5. Volumetric Fog — Froxel (우선순위 ↑)

원전 §7 은 "엘든링 시그니처". Winters 도 MOBA 전투 분위기에 큰 기여 → **Phase E Stage 5 에 포함**.

### 5.1 해상도

```cpp
// Engine/Public/Renderer/Volumetric/VolumetricFog.h
constexpr u32_t FROXEL_X = 160;   // 화면 비율
constexpr u32_t FROXEL_Y = 90;
constexpr u32_t FROXEL_Z = 128;   // view-frustum 깊이
// 3D texture: R16G16B16A16_FLOAT = 14.2 MB (메모리 예산 수용 가능)
```

### 5.2 파이프

```
1. (CS) InjectLighting   — 각 froxel 에 in-scattering 저장
2. (CS) Accumulate       — view direction 따라 누적, transmittance
3. (PS) Apply            — 화면 합성 (scene color * T + L_accum)
```

### 5.3 Phase Function

Henyey-Greenstein (원전 §7.2):

```hlsl
float HGPhase(float cosTheta, float g)
{
    float g2 = g * g;
    return (1.0f - g2) / (4.0f * PI * pow(1.0f + g2 - 2.0f * g * cosTheta, 1.5f));
}
```

g=0.2 가 일반 안개, g=0.8 이 광선 효과 (god ray).

### 5.4 Winters 이니셔티브

- 시그니처 비주얼: "밤의 소환사 협곡", "드래곤 둥지 연기", "숲 사이 햇살"
- ImGui 튜너: `fDensity`, `g` (Henyey-Greenstein asymmetry), 지역 밀도 볼륨
- 비용 목표: GPU 5ms 이내 @ 1080p

---

## 6. Reflection — SSR + IBL Fallback

원전 §8. Winters 규약:

1. **Screen-Space Reflection** `Shaders/GI/SSR/SSR.hlsl` (Phase E Stage 5)
2. Hi-Z 버퍼 빌드 → linear raymarch → rough-aware
3. SSR 실패 영역은 **IBL Prefilter 맵으로 fallback** (문서 04 이미 구현)

```hlsl
// Lighting PS 마지막 부분
float3 ssrColor; float ssrHit = SSR(worldPos, N, V, ssrColor);
float3 iblSpec  = EvaluateIBL_Specular(N, V, roughness, F0);
float3 spec     = lerp(iblSpec, ssrColor, ssrHit * (1.0 - roughness));
```

roughness 높은 표면은 자동으로 IBL 쪽이 지배 → 자연스러운 폴백.

---

## 7. Post-Process — 색 파이프라인 (비용 대비 임팩트 최상)

원전 §11. Winters 결정:

### 7.1 Tonemap

- **ACES RRT + ODT 필름 근사** (Narkowicz 2015 간이식):

```hlsl
float3 ACESFilm(float3 x)
{
    const float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}
```

- Phase 2 업그레이드: Uncharted 2 / Hable operator 옵션

### 7.2 Color Grading — 지역별 LUT

원전 §11.2 의 "지역별 3D LUT" 이 FromSoft 아트디렉션의 핵심. Winters 도 적극 채용:

```cpp
// Engine/Public/Renderer/PostFX/ColorLUT.h
class WINTERS_ENGINE CColorLUT
{
public:
    static std::unique_ptr<CColorLUT> Create(ID3D11Device* pDev);
    bool_t LoadFromDDS(const wstring_t& strPath);   // 32³ 3D texture
    void   BindForTonemap(ID3D11DeviceContext* pCtx, u32_t uSlot);
    ...
};
```

스타일 저장 경로:
```
Resource/LUT/
├── default_neutral.dds
├── summoners_rift_day.dds
├── summoners_rift_night.dds
└── dragon_pit.dds
```

지역 진입 시 crossfade:

```cpp
// Scene_InGame 또는 맵 트리거가 호출
pGameInstance->Get_PostFX()->TransitionLUT(L"Resource/LUT/dragon_pit.dds", 1.5f /*seconds*/);
```

### 7.3 Bloom — FFT (Phase E Stage 6)

- Multi-stage downsample + Kawase blur 는 **임시 Phase 2 구현**
- 정식은 FFT Convolution (Stage 6) 로 교체
- 아트 파라미터: threshold, intensity, spread — ImGui 노출

### 7.4 Dithering

`Shaders/PostFX/Dither.hlsl` 에 blue-noise 텍스처 `Resource/Noise/blue64.png` 샘플링. 8-bit backbuffer 전에 노이즈 1LSB 주입 → banding 제거.

---

## 8. 캐릭터 셰이딩

### 8.1 피부 — Pre-Integrated SSS (Jimenez 2010)

원전 §6.1. Winters 는 캐릭터 얼굴 별도 셰이더:

```
Shaders/BRDF/Character_Skin_PBR.hlsl
├─ cbuffer CBSkin : register(b10)  { fCurvatureScale, fSSSScale, vScatterColor }
├─ Texture2D g_CurvatureMap : register(t11)   // 오프라인 베이크
└─ Texture2D g_SSSLUT       : register(t12)   // pre-integrated LUT
```

런타임: `SSS = g_SSSLUT.Sample(float2(dot(N, L) * 0.5 + 0.5, curvature)) * albedo`.

### 8.2 머리카락 — Kajiya-Kay (간이) → Marschner (Phase E 후)

- Phase 2: Kajiya-Kay anisotropic. Forward 패스, alpha sort.
- Phase E Stage 5 이후: Marschner R + TRT. 단 OIT 안 쓰므로 정렬 미스 일부 수용.

### 8.3 눈

간단 Custom:
- 각막 refraction (IOR 1.376)
- Iris parallax (2 layer sampling)

```
Shaders/BRDF/Character_Eye.hlsl
```

### 8.4 의상 / 갑옷

- **Anisotropic GGX** (브러시드 메탈) — 01 BRDF §3.5 확장 — Phase E Stage 1.x
- **Sheen / Charlie** (천, 벨벳) — Filament Cloth BRDF — Phase E Stage 2 이후

---

## 9. 라이트맵 베이킹 — 장기 계획

현재는 전부 실시간 (IBL + Directional + clustered). 라이트맵은 **맵이 확정된 이후 Phase E Stage 4 (Path Tracer)** 가 ground truth 를 제공할 때 부터 의미:

```
Tools/WintersLightmapBaker/    // Path Tracer 기반 오프라인 lightmap 생성
Resource/Lightmaps/
├── summoners_rift_day.dds     // atlas
├── summoners_rift_dusk.dds
└── summoners_rift_night.dds
```

런타임 교체: 시간대 시스템이 맵 진입 시 crossfade. **엘든링 §4.3 의 mechanism 그대로 이식**.

중요: 라이트맵이 들어오는 순간 Lighting PS 가 `albedo * indirect_lightmap` 를 곱해 IBL ambient 를 부분 대체 (lightmap 우선).

---

## 10. LOD + Impostor (원전 §10.3/10.4)

MOBA 맵은 작지만, 원거리 배경 (멀리 있는 산/성) 에 impostor 시스템은 여전히 유효:

```
Tools/WintersImpostorBaker/
Resource/Impostors/
├── mountain_ranges.dds    // 8 방향 × 8 각도 atlas
└── distant_castle.dds
```

LOD 규약:
| LOD | 거리 | Mesh |
|-----|------|------|
| 0 | ≤10m | full |
| 1 | 10~50m | 50% tri |
| 2 | 50~200m | 20% tri |
| 3 | 200m+ | impostor billboard |

---

## 11. 심볼 가시성 / 보안 관점 (CLAUDE.md §보안 §2)

원전 §13.1 "안 쓰는 기능의 부재" 는 **공격 표면 축소** 와도 맞물림. Winters 원칙 재확인:

- 본 시리즈의 새 클래스 (`CBRDFMaterial`, `CClusteredLighting`, `CTAAResolve`, `CIBLManager`, `CMotionVectorPass`, `CShadowCSM`, `CColorLUT` 등) 는 Engine DLL 내부.
- `WINTERS_ENGINE` export 는 해당 클래스 자체만. 내부 Impl / Helper 는 export 금지.
- Client 는 `CGameInstance::Get_*` Tier 2 Getter 로만 접근.
- 본 문서의 모든 `.cpp` 는 **Client EXE 바이너리에 포함되지 않음** — 치트 개발자의 EXE 기반 CE 스캔 저항.

---

## 12. ImGui 튜너 — Winters 는 에디터가 곧 연습모드

CLAUDE.md "연습모드가 최종 목표". FromSoft 엔진이 아티스트와 엔진 개발자가 같은 건물이듯, **Winters 는 아트가 곧 런타임 튜닝**.

본 시리즈 전체에서 도입한 ImGui 패널 통합:

```
[ImGui 패널 계층 — Scene_Editor 및 Scene_InGame 에서 동일 접근]
├─ Renderer
│   ├─ Clustered Lighting        (문서 02 §13)
│   ├─ TAA                       (문서 03 §11)
│   ├─ IBL                       (문서 04 §11)
│   ├─ Motion Vector Debug       (문서 05 §14)
│   ├─ Volumetric Fog            (본 문서 §5)
│   ├─ Shadow CSM                (본 문서 §4)
│   ├─ Color Grading (LUT)       (본 문서 §7.2)
│   └─ SSAO / SSR                (Phase E Stage 5 시점 추가)
└─ BRDF
    ├─ Material Tuner (per-actor) (문서 01 §11)
    └─ Furnace Test               (문서 01 §10)
```

**디자인 규칙**: 신규 Stage 는 ImGui 패널 없이 PR 불가 — "빌드 1번으로 모든 값 튜닝" 원칙.

---

## 13. 연습모드 × 에디터 통합 (원전 §13 의 "아이덴티티")

FromSoft 의 엔진이 아티스트를 서비스하는 방식 → Winters 연습모드:

- **챔피언 위치/체력/마나/쿨다운 실시간 조작**: ECS 컴포넌트 직접 수정 UI
- **스폰 트리거**: 정글몹 / 미니언 웨이브 버튼
- **Shader Reload**: HLSL 파일 모니터링 → 저장 시 재컴파일 (`CDX11Device::ReloadShader(path)`)
- **Time of Day**: directional light + IBL + LUT 크로스페이드 슬라이더
- **Volumetric 파라미터**: density / anisotropy 실시간

이 통합이 "Winters 아이덴티티". "UE/Unity 가 못 하는 것" 목록이 아니라 "**Winters 가 가장 잘 하는 것**" 을 분명히 하는 쪽.

---

## 14. 참고 기법이 아닌 결정의 체크리스트

원전 §14 "Winters Engine 이 참고할 교훈" 의 실행 가능한 번역:

- [x] **"안 할 것" 목록 문서화** — §0
- [x] **아트디렉션 먼저** — §7.2 LUT 시스템을 Stage 2 에 포함
- [x] **Baked + Realtime 하이브리드 원칙** — §9
- [x] **Color Pipeline 에 투자** — §7 (Tonemap + LUT + Dither)
- [x] **Volumetric Fog 우선순위 ↑** — §5, Stage 5 에 배치
- [x] **Impostor System 설계** — §10, Phase E Stage 5 에 포함
- [x] **엔진 = 결정들** — 본 문서 자체가 Winters 의 결정 기록

---

## 15. Winters vs FromSoft 대조표 (최종)

| 축 | FromSoft | Winters |
|----|---------|--------|
| API | D3D12 (엘든링 PC) | D3D11 (유지) |
| Lighting | Deferred + Clustered (추정) | Deferred + Clustered (확정, 문서 02) |
| Shadow | CSM 4 + Atlas + Contact | CSM 4 + Atlas + Contact (§4) |
| GI | Baked + Probe | Baked (장기) + IBL (문서 04) |
| Volumetric | Froxel | Froxel (§5) |
| SSR | 있음 | 있음 (Phase E Stage 5) |
| Tonemap | 필름 커브 | ACES + LUT (§7) |
| Hair | Marschner (추정) | Kajiya-Kay → Marschner 단계적 (§8.2) |
| Skin | Pre-Int SSS | Pre-Int SSS (§8.1) |
| Cloth | Sheen | Sheen (장기) |
| TAA | SOTE+ | 본격 도입 (문서 03) |
| DLSS/FSR | SOTE | Phase E Stage 8 이후 |
| Streaming | 타일 + Kraken | 단일 맵 전부 상주 (MOBA 제약) |
| LOD / Impostor | 있음 | 있음 (§10) |
| OIT | 없음 (alpha-sort) | 없음 (alpha-sort) |

---

## 16. 착수 순서 (Winters 시점)

본 시리즈 01~06 의 권장 실행 순서. Phase E 의 Stage 와 교차 매핑:

```
Stage 1 BRDF (01_WINTERS)
     ↓
Motion Vectors 전제 (05_WINTERS)                      ← TAA 에 필수
     ↓
Stage 2 PBR = IBL + Clustered Deferred (04 + 02_WINTERS)
     ↓
Stage 7 TAA (03_WINTERS)                              ← MV + GBuffer 전제
     ↓
Stage 5 Shadow/SSAO/SSR/Volumetric Fog (06_WINTERS §4~§6)
     ↓
Stage 6 FFT Bloom  (별도 문서 — 본 시리즈 범위 밖)
     ↓
Stage 4 Path Tracer  (별도 문서)
     ↓
Stage 8 DXR          (선택)
```

**반드시 이 순서**. MV 가 없으면 TAA 가 흔들리고, BRDF 가 없으면 Clustered 에 합성할 재료가 없다.

---

## 17. 참고

- Digital Foundry 엘든링/SOTE 분석
- Jimenez 2010 — Pre-Integrated SSS
- Hillaire 2015 — Frostbite Volumetric
- Kawase 2003 — Practical Bloom Kernel
- Narkowicz 2015 — ACES filmic approx
- `.md/plan/graphics/` 시리즈 (내부 엔진 Stage 별 문서)
- CLAUDE.md 게임 콘텐츠 목표 (§게임 콘텐츠 목표)

---

*문서 끝. FromSoft 가 "엔진은 결정들이다" 라고 가르쳤다면, Winters 의 결정은 본 시리즈 6 문서에 담겼다.*
