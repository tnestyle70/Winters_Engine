# 02. 그래픽스 렌더링 — 박사 연구 심화

> 이 문서는 `00_PHD_Paper_Guide.md`의 개념 틀을 전제로 한다.
> 특히 §1 "구현 vs 기여", §3 thesis statement 형식
> (`"[기법 X]를 쓰면 [제약 C] 하에서 [목표 Y]를 [SOTA 대비 개선 Z]로 달성한다"`),
> §4 표준 구조, §7 평가(PSNR/SSIM/FLIP, frame time ms, memory MB, 시간적 안정성)를
> 모든 세부주제에서 일관되게 적용한다.
> 읽는 이는 Winters 엔진(DX12 메인 RHI + Vulkan/콘솔/모바일 전략, LoL식 MOBA + 오픈월드)을
> 직접 만든 숙련 C++ 개발자다. 따라서 "어떻게 구현하나"가 아니라
> **"이 분야에서 박사 기여는 어디서 나오나"**에 초점을 둔다.

---

## 0. 이 분야를 박사로 본다는 것

### 0.1 구현 항목 vs 기여 후보

그래픽스/실시간 렌더링은 게임 엔진 분야 중 **박사 논문이 가장 많이 나오는** 영역이다.
이유는 명확하다. (a) 정량 평가가 잘 정의돼 있고(ground-truth path tracer와의 오차),
(b) 실시간 제약(frame budget)이 자연스러운 constraint를 만들며,
(c) 인지(perception) 기반 품질 지표가 성숙해 "더 좋다"를 측정할 수 있다.

하지만 가이드 §1의 함정은 여기서도 그대로다.

| 이건 구현이다 (석사·포트폴리오) | 이건 기여 후보다 (박사) |
|--------------------------------|--------------------------|
| "Deferred renderer를 만들었다" | "G-buffer 대역폭을 머티리얼 ID 기반 가변 압축으로 줄여, 모바일 tile GPU에서 deferred를 forward 동급 대역폭으로 돌린다" |
| "PBR을 구현했다" | "실시간 multiscatter GGX를 LUT 1회 fetch로 근사해, 거친 금속에서 에너지 손실을 ΔE<1로 제거한다" |
| "VXGI/DDGI를 붙였다" | "동적 파괴 장면에서 probe 가시성을 점진 갱신해 light leak 없이 1ms 이내로 diffuse GI를 업데이트한다" |
| "RTX로 반사를 켰다" | "1spp 경로추적 + 시간적 재투영에서 disocclusion ghosting을 곡률 기반 가중으로 억제해 SVGF 대비 temporal lag을 절반으로 줄인다" |

핵심: **"렌더링 기능을 구현"한 것은 비교 대상이 없다.**
박사는 항상 *기존 최첨단(SOTA) 또는 ground truth와 비교해 정량적으로 무엇이 나아졌는가*를 말한다.

### 0.2 Top Venue

가이드 §9의 그래픽스 행을 펼친다. **어디에 내느냐가 곧 기여의 성격**을 규정한다.

| 무대 | 성격 | 이 문서 주제와의 정렬 |
|------|------|----------------------|
| **SIGGRAPH / SIGGRAPH Asia** | 분야 최정점. 새 알고리즘·이론. ACM TOG 저널 트랙과 통합 | GI, PBR, RT 전반의 "획기적 한 방" |
| **EGSR** (Eurographics Symposium on Rendering) | 렌더링 특화. 라이트 트랜스포트·샘플링의 본진 | ReSTIR, path guiding, 디노이징 |
| **HPG** (High-Performance Graphics) | GPU·실시간·하드웨어 친화. 자료구조·성능 | BVH, light culling, froxel, GPU-driven |
| **I3D** (ACM Symp. on Interactive 3D Graphics and Games) | 인터랙티브·게임 친화. 실시간 기법 | Forward+, 실시간 GI 근사, 게임 워크로드 |
| **ACM TOG** | 저널 (SIGGRAPH가 여기로 출판) | 이론·완결성 높은 기여 |
| **IEEE TVCG** | 저널. 시각화·렌더링·인지 | 품질 지표·user study가 강한 기여 |
| (산업, 비학술) GDC / Digital Dragons | Lumen·Nanite 같은 산업 SOTA의 1차 출처 | 인용·동기·baseline 근거로만 사용 |

> 주의(가이드 §9): GDC/UE 기술 블로그는 **동료심사 학술 출판이 아니다.**
> Lumen, Nanite, Frostbite 강연은 "산업 현황과 open problem의 증거"로 인용하되,
> 기여의 학술적 정당화는 SIGGRAPH/EGSR/HPG/I3D 논문에 건다.

### 0.3 렌더링 박사의 공통 무기: ground truth와 frame budget

이 분야에서 거의 모든 기여 챕터는 두 축 사이의 trade-off를 정량화한다.

```text
                품질 (vs offline path-traced ground truth)
                  ↑  PSNR / SSIM / FLIP / ΔE
                  │
   offline PT ────●  (ground truth, 수 초/프레임)
                  │
   내 기법 ───────●  ← "이 점을 좌상단(품질↑·시간↓)으로 민다"
                  │
   naive 근사 ────●
                  └──────────────────────────→ 비용 (ms / MB / W)
                                                 frame budget(예: 16.6ms@60fps,
                                                 모바일 8.3ms@120fps)
```

박사 기여 = "이 Pareto frontier를 특정 제약(동적 장면 / 저사양 GPU / 투명 / 대역폭) 하에서 바깥으로 민다."
나머지(§1~§6)는 모두 이 한 장의 그림을 각 분야에서 구체화한 것이다.

---

## 1. Global Illumination (실시간 전역조명)

실시간 GI는 현재 그래픽스 박사의 **가장 뜨거운 전장**이다.
Lumen(UE5)·DDGI·ReSTIR GI가 2019~2023에 쏟아졌고, "동적 장면 / 저사양 / light leak"이라는
open problem이 여전히 살아 있다.

### 1.1 핵심 원리(이론/수학)

모든 GI의 출발점은 **렌더링 방정식(rendering equation, Kajiya 1986)**이다.
점 `x`에서 방향 `ω_o`로 나가는 radiance:

```text
L_o(x, ω_o) = L_e(x, ω_o)
            + ∫_{Ω} f_r(x, ω_i, ω_o) · L_i(x, ω_i) · (n · ω_i) dω_i
```

- `L_e` : 자발광(emission)
- `f_r` : BRDF (§3에서 다룸)
- `L_i(x, ω_i)` : 입사 radiance — **이 항이 다른 표면에서 온 빛(=간접광)을 포함**하므로 방정식이 재귀·적분이 된다.
- `(n · ω_i)` : 코사인 항 (반구 `Ω` 적분)

직접광(direct)만 풀면 deferred/forward의 라이팅이고,
`L_i`의 간접 성분까지 푸는 것이 GI다. 이 적분은 **고차원·재귀**라 닫힌 해가 없어
몬테카를로 추정(path tracing) 또는 캐싱/사전계산(radiosity, probe)으로 근사한다.

실시간 GI의 핵심 분해는 **diffuse(저주파, 캐싱 가능) vs specular(고주파, 거의 ray가 필요)**:

```text
L_indirect ≈ L_diffuse-GI (probe/SH로 캐싱)  +  L_specular-GI (SSR/RT reflection)
```

diffuse는 공간적으로 부드러워 **irradiance를 sparse하게 캐싱**(probe, SH 9계수)할 수 있다는 것이
실시간 GI의 거의 모든 트릭의 뿌리다.

### 1.2 대표 기존 연구(SOTA)

오프라인 계보:
- **Goral et al. 1984 / Cohen & Greenberg 1985** — Radiosity. 확산면 간 form factor 행렬을 풀어 diffuse GI. 동적 장면에 약함.
- **Kajiya 1986** — Path tracing + 렌더링 방정식. 모든 GI의 ground truth.
- **Jensen 1996** — Photon Mapping. caustics·SSS에 강한 two-pass(광자 분사 → density estimation).
- **Keller 1997** — Instant Radiosity / VPL. 광자 경로 끝에 가상 점광원(VPL)을 심어 간접광을 직접광 합으로 근사. 후대 many-light·ReSTIR의 조상.

실시간 계보:
- **Crassin et al. 2011** — *Interactive Indirect Illumination Using Voxel Cone Tracing* (VXGI). 장면을 sparse voxel octree로 복셀화 → 표면에서 cone을 쏴 사전필터된 복셀에서 간접광 적분. NVIDIA VXGI의 기반.
- **Majercik, Guertin, Nowrouzezahrai, McGuire 2019** — *Dynamic Diffuse Global Illumination with Ray-Traced Irradiance Fields* (DDGI). irradiance probe grid + **per-probe depth(가시성) octahedral map**으로 light leak을 억제. 실시간 동적 GI의 사실상 표준.
- **Epic Games (Wright et al.) 2021~** — **Lumen** (UE5). software ray tracing(SDF + mesh card) + hardware RT 하이브리드 + **surface cache**(메시 표면에 라이팅 캐시) + screen-space + world-space probe 다단 구성. GDC/SIGGRAPH course로 공개. 학술 단일 논문이 아니라 시스템.
- **Ouyang, Liu, Kettunen, Pharr, Lehtinen 2021** — *ReSTIR GI*. spatiotemporal reservoir resampling을 간접광 경로에 적용해 1~수 spp로 다수의 간접 경로를 사실상 재사용. (선행: Bitterli et al. 2020 ReSTIR — §6.)
- **SDFGI** (Godot 등) / **signed distance field cone tracing** — voxel 대신 SDF에서 cone trace. 동적 갱신·메모리 측면 변형.

### 1.3 알고리즘/파이프라인(의사코드)

DDGI를 대표로(가장 박사 연구의 baseline이 되기 좋음):

```text
# --- 사전: probe grid 배치 (월드 공간 규칙 격자 또는 적응 격자) ---
for each frame:
    # 1) probe 갱신 (예산: probe N개 중 일부만, 또는 매 프레임 일부 방향)
    for each active probe p:
        for k rays in sphere(p):                 # 보통 64~256 ray/probe
            hit = trace_ray(p.pos, rand_dir_k)   # RT 또는 SDF/voxel trace
            radiance_k = shade(hit)              # hit점 직접광 + 이전 프레임 probe에서 1-bounce
        # octahedral map에 누적
        p.irradiance_oct = blend(p.irradiance_oct, integrate(radiance_k), hysteresis)
        p.depth_oct      = blend(p.depth_oct, hit_distances_k, hysteresis)  # 가시성

    # 2) 화면 픽셀 셰이딩 시 probe 보간
    for each visible pixel x with normal n:
        probes8 = surrounding_cage(x)            # 8개 코너 probe
        w = trilinear_weight(x)
        for p in probes8:
            # leak 방지: probe→x 가시성 체크 (Chebyshev, depth_oct의 분산 이용)
            vis = chebyshev_visibility(p.depth_oct, dir(p→x), dist(p→x))
            w_p = trilinear * vis * max(0, dot(n, dir(p→x)))
        irradiance(x) = Σ w_p · sample_oct(p.irradiance_oct, n) / Σ w_p
        L_diffuse_GI(x) = albedo(x)/π · irradiance(x)
```

`chebyshev_visibility`(probe depth의 평균·분산으로 차폐 확률 추정)가 DDGI의 **light leak 억제 핵심**이다.
VXGI라면 위 1)이 "복셀화 + cone trace"로, Lumen이라면 "SDF trace + surface cache lookup"으로 바뀐다.

### 1.4 박사급 novel 기여 각도 (open problems)

- **동적 장면 갱신 비용**: 파괴·문 열림·이동 지오메트리에서 voxel/SDF/surface cache 재구축이 비싸다.
  → "변화 영역만 점진(incremental) 갱신하는 자료구조 + 우선순위 스케줄링"이 기여 후보. (자료구조형, HPG/I3D 정렬)
- **Light leaking**: probe 기반의 고질병. DDGI의 Chebyshev도 얇은 벽·코너에서 샌다.
  → "곡률/두께 인지 가시성 모델" 또는 "표면 정렬 probe(surface-aligned)"가 각도.
- **Temporal stability**: probe hysteresis가 동적 광원에 늦게 반응(lag) ↔ 빠르면 flicker.
  → "콘텐츠 적응 hysteresis"나 "변화 검출 기반 가변 갱신율".
- **저사양 이식(downport)**: RT 하드웨어 없는 모바일/구형 GPU에서 GI.
  → "SDF/voxel 기반 software trace의 모바일 tile GPU 최적화" — Winters의 모바일 전략과 직결.
- **메모리**: probe·voxel·surface cache 메모리 폭증.
  → "지각적으로 중요한 곳에만 밀도를 주는 적응 probe(content-adaptive density)" + DDGI 대비 메모리 N%↓·품질 동등.

가이드 §5 기준 기여 유형: **1(새 알고리즘) + 2(새 자료구조)** 조합이 전형. 4(수렴/오차 한계 증명)가 붙으면 강해진다.

### 1.5 Thesis statement 예시

> "Probe별 가시성 분산을 곡률 가중으로 보정하고 변화 영역만 점진 갱신하는 irradiance field는,
> 동적 파괴 장면에서 light leak 없이(ΔE < 2 vs offline PT) **1ms 이내**로 diffuse GI를 갱신하며,
> DDGI 대비 동일 품질에서 probe 메모리를 **40% 절감**한다."

(가이드 §3 형식: 기법 X = 곡률 보정 + 점진 갱신, 제약 C = 동적 파괴 60fps, 목표 Y = leak 없는 diffuse GI, 개선 Z = 1ms·메모리 40%↓.)

### 1.6 평가 방법

- **품질**: offline path tracer(예: 자체 PT 또는 Mitsuba/PBRT) ground truth와 **PSNR/SSIM/FLIP, ΔE**. diffuse irradiance 맵 단위로도 비교.
- **성능**: probe 갱신 ms, 셰이딩 ms, 총 GI ms (RTX 4070 / 모바일 Adreno 등 명시 — 가이드 §7 재현성).
- **메모리**: probe·voxel·SDF·surface cache MB.
- **시간적 안정성**: 카메라·광원 이동 시퀀스에서 **temporal flicker**(연속 프레임 차분), TAA off로 측정.
- **Ablation**: 가시성 보정 on/off, 점진 갱신 on/off, hysteresis 고정 vs 적응 — "효과의 원천" 분리.
- **Baseline**: DDGI(동일 probe 수), VXGI, (가능하면) Lumen software path. + naive(매 프레임 full 재계산).
- **장면 다양성**: 정적/동적/파괴/실외 여러 시드·장면(가이드 §7 통계).

### 1.7 Winters 연결점

- **두 게임 모드의 워크로드 대비가 천연 testbed**: MOBA 탑다운(카메라 고정·고각, 동적 광원은 스킬 FX 위주)
  vs 오픈월드(Elden Ring 자산, 자유 카메라·낮밤·실내외). 같은 GI 기법을 두 워크로드에서 평가하면
  "카메라/장면 통계가 기법 우열을 어떻게 바꾸는가"라는 일반화 가능한 발견(가이드 §5-5)이 나온다.
- **LoL식 FX는 값싼 GI로 충분**: 탑다운 MOBA는 full GI보다 "스킬·미니언 발광이 바닥에 번지는 정도"의
  저비용 간접광이면 된다 → **모드별 GI LOD/예산 정책**이 시스템 기여(§5-3) 각도.
- **fog of war와 혼동 금지**: Winters의 `fogofwaroverlay` 류는 **게임플레이 시야 마스크**(2D 오버레이)이지
  체적 산란(§5)이 아니다. GI/볼류메트릭 논문에서 "fog of war"는 라이팅이 아니라고 명시해야 심사 혼선이 없다.
- **RHI 전략과 직결**: `RHI_BACKEND_STRATEGY` §6은 모바일에서 "deferred+heavy SSAO/GTAO 옵션화, bindless는 capability check"라 못박았다.
  GI 기여를 **RT 없는 Vulkan/모바일 path**로 downport하는 것이 곧 Winters의 cross-platform 목표와 한 몸이다.
- `Engine/Public/Renderer/ModelRenderer.h`는 이미 `SetAmbientOcclusionSRV()`를 노출 — AO/간접광 입력 훅이 존재하므로 probe irradiance를 여기에 주입하는 통합 실험이 가능.

---

## 2. Deferred Rendering (지연 셰이딩)

### 2.1 핵심 원리(이론/수학)

forward shading은 라이트 `L`개·픽셀 `P`개일 때 (오버드로 포함) 셰이딩 비용이 `O(P · L)`로 폭증한다.
deferred는 **기하/머티리얼 단계와 라이팅 단계를 분리**한다.

```text
Geometry pass:  픽셀당 표면 속성을 G-buffer에 기록 (라이팅 안 함)
                → albedo, normal, roughness, metallic, depth, ...
Lighting pass:  화면공간에서 G-buffer를 읽어 라이트마다 셰이딩
                → 비용 ≈ O(P · L) 이지만 가시 픽셀만, 오버드로 없이
```

핵심 이득: **라이팅이 "보이는 픽셀당 1회"**로 수렴(오버드로 제거)하고, 라이트를 화면공간에서
**볼륨/타일 컬링**할 수 있다. 대가는 **G-buffer 대역폭**(여러 RT를 매 픽셀 write/read)과
**투명·MSAA·머티리얼 다양성** 문제다.

라이트 컬링의 수학적 핵심은 화면을 타일/클러스터로 나눠 각 타일의 **min/max depth로 절두체 슬랩**을
만들고, 라이트 볼륨(구·원뿔)과 교차하는 라이트만 그 타일의 리스트에 넣는 것:

```text
tile_frustum = build_frustum(tile_xy, z_min, z_max)
light ∈ tile.list  ⟺  sphere(light.pos, light.radius) ∩ tile_frustum ≠ ∅
```

### 2.2 대표 기존 연구(SOTA)

- **Deering et al. 1988** — *The Triangle Processor and Normal Vector Shader*. deferred 셰이딩 개념의 효시(기하와 셰이딩 분리).
- **Saito & Takahashi 1990** — *Comprehensible Rendering of 3-D Shapes* (G-buffer 용어의 기원, geometric buffer).
- **Olsson, Billeter, Assarsson 2012** — *Clustered Deferred and Forward Shading* (HPG). 화면 2D 타일을 깊이까지 확장한 **3D 클러스터**로 라이트를 분류. 타일 deferred의 깊이 불연속 문제(한 타일에 멀고 가까운 면이 섞임)를 해결. 현대 클러스터드의 표준.
- **Andersson 2009 (Frostbite, GDC) / Lauritzen 2010** — tiled deferred(compute 기반 light culling)의 산업·기술 발표.
- **Pesce / Pettineo (산업 블로그)** — G-buffer 레이아웃·packing 실무 SOTA(인용·동기용).

### 2.3 알고리즘/파이프라인(의사코드)

클러스터드 deferred:

```text
# 1) Geometry pass
for each triangle:
    write G-buffer(albedo, packed_normal, roughness, metallic, depth, [motion])

# 2) Cluster assignment (compute)
divide screen into (Tx, Ty) tiles × Zslices  (z는 보통 지수 분할)
for each cluster c:
    c.light_list = []
    for each light l:
        if intersects(light_volume(l), cluster_frustum(c)):
            c.light_list.append(l)

# 3) Lighting pass (full-screen 또는 compute)
for each pixel x:
    surf = unpack_Gbuffer(x)
    c = cluster_of(x)                       # x의 화면좌표+depth → 클러스터
    Lo = 0
    for l in c.light_list:                  # 그 클러스터의 라이트만
        Lo += BRDF(surf, l) · NdotL · shadow(l, x)
    output(x) = Lo + emissive + ambient/GI
```

### 2.4 박사급 novel 기여 각도 (open problems)

- **대역폭(특히 모바일 tile GPU)**: G-buffer write/read가 메모리 대역폭을 잡아먹는다.
  → "머티리얼 ID 기반 가변 G-buffer 압축" 또는 "tile-based GPU의 on-chip tile memory에 G-buffer를 유지(merge geometry+lighting subpass)해 main memory write 회피". (HPG/I3D, 자료구조+시스템)
- **머티리얼 다양성**: deferred는 고정 G-buffer 레이아웃이라 다층/이방성/SSS·clear-coat 머티리얼을 표현하기 어렵다(이게 forward의 장점).
  → "머티리얼 분류 후 분기 셰이딩(material classification + indirect dispatch)"의 효율적 새 스케줄링.
- **VRS(Variable Rate Shading) 통합**: 어디를 거칠게 셰이딩할지 결정.
  → "G-buffer 통계(법선 분산·머티리얼)로 per-tile shading rate를 정해 품질 손실 없이 셰이딩 픽셀 수↓". (인지 품질 지표로 평가)
- **투명·MSAA**: deferred의 영원한 약점. → 하이브리드(불투명 deferred + 투명 forward) 스케줄링 최적화.

### 2.5 Thesis statement 예시

> "G-buffer를 머티리얼 ID에 따라 가변 비트폭으로 압축하고 tile GPU의 on-chip 메모리에 유지하면,
> 모바일 클러스터드 deferred를 **forward+ 동급 대역폭(KB/tile)**으로 수행하면서
> 64+ 동적 라이트에서 품질 손실(FLIP < 0.5) 없이 frame time을 **30% 단축**한다."

### 2.6 평가 방법

- **대역폭**: G-buffer write/read **KB/frame, KB/tile**(GPU 카운터). ← 이 분야의 1차 지표.
- **성능**: geometry/culling/lighting 단계별 ms, 라이트 수 스윕(가이드 §7 확장성 곡선).
- **품질**: 압축·VRS 적용 시 FLIP/SSIM vs 무압축·full-rate.
- **Baseline**: tiled deferred(Lauritzen), 클러스터드(Olsson 2012), forward+(§4). + naive forward.
- **Ablation**: 압축 on/off, 클러스터 z분할 방식, VRS on/off.
- **HW 명시**: 데스크톱 IMR(immediate-mode) GPU vs 모바일 TBDR(tile-based deferred) — 결론이 아키텍처에 의존하므로 둘 다(가이드 §7 threats to validity).

### 2.7 Winters 연결점

- **모바일 deferred가 핵심 긴장점**: `RHI_BACKEND_STRATEGY` §6 "deferred+heavy SSAO/GTAO 옵션화, tile-based GPU 고려: render pass/load-store 명확화"가 곧 이 절의 open problem(대역폭·tile memory)이다.
  Vulkan **dynamic rendering / subpass**(§5 V4)로 G-buffer를 on-chip에 유지하는 실험이 Winters에서 자연스럽다.
- **DX12 메인 vs 모바일 대비 testbed**: 같은 클러스터드 deferred를 데스크톱 DX12(IMR)와 Vulkan 모바일(TBDR)에서 돌려
  "대역폭 병목이 아키텍처별로 어떻게 다른가"를 측정 → 가이드 §5-5(경험적 발견)형 기여.
- **MOBA vs 오픈월드 라이트 분포**: MOBA는 라이트가 적고 탑다운이라 클러스터 z분할 효용이 작지만,
  오픈월드(많은 점광원·실내)는 클러스터드 이득이 크다 → 모드별 컬링 전략 비교가 ablation 소재.
- `ModelRenderer.h`의 `RenderNormalPass(...)`가 이미 mesh/skinned 셰이더·파이프라인을 분리 호출 — G-buffer geometry pass의 자연스러운 hook 지점.

---

## 3. Physically Based Rendering (PBR)

### 3.1 핵심 원리(이론/수학)

PBR의 BRDF는 **microfacet 이론**에 선다. 표면을 무수한 미세거울(microfacet)의 통계 분포로 보고,
Cook-Torrance 형태로 specular를 모델링한다:

```text
f_specular(ω_i, ω_o) =  D(h) · F(ω_o, h) · G(ω_i, ω_o)
                       ───────────────────────────────
                          4 · (n·ω_i) · (n·ω_o)

  h = normalize(ω_i + ω_o)            (half vector)
  D : Normal Distribution Function    (microfacet 법선이 h에 정렬된 비율)
  F : Fresnel                          (각도별 반사율, Schlick 근사 F0+(1−F0)(1−cosθ)^5)
  G : Geometry / masking-shadowing     (미세거울 간 가림·그림자)
```

가장 널리 쓰는 **D = GGX(Trowbridge-Reitz)**:

```text
D_GGX(h) = α² / ( π · ((n·h)² (α²−1) + 1)² ),   α = roughness²
```

긴 꼬리(long tail)가 실제 거친 표면의 하이라이트와 잘 맞아 표준이 됐다.
**G**는 GGX와 **수학적으로 일관된** Smith masking-shadowing을 써야 에너지가 맞는다(Heitz 2014).

diffuse는 보통 Lambert(`albedo/π`) 또는 Disney diffuse. 전체:

```text
f_r = (1 − metallic)·albedo/π  +  f_specular
```

**에너지 보존**(반사 에너지 ≤ 입사)이 PBR의 정의적 제약이다. 단일산란 microfacet은
거친 표면에서 **에너지를 잃는다**(다중 반사된 빛을 빼먹음) → multiscatter 보정이 필요(§3.4).

### 3.2 대표 기존 연구(SOTA)

- **Cook & Torrance 1982** — *A Reflectance Model for Computer Graphics*. microfacet BRDF의 고전, D·F·G 구조의 기원.
- **Walter, Marschner, Li, Torrance 2007** — *Microfacet Models for Refraction through Rough Surfaces*. **GGX** 분포를 그래픽스에 도입(반사·굴절).
- **Burley 2012** — *Physically-Based Shading at Disney* (SIGGRAPH course). **Disney "principled" BRDF**. 아티스트 친화 파라미터(baseColor, roughness, metallic, …)로 표준화 → 게임 PBR의 사실상 입력 규약.
- **Karis 2013** — *Real Shading in Unreal Engine 4* (SIGGRAPH course). **split-sum approximation**으로 IBL(image-based lighting)을 실시간화: 환경광 적분을 사전적분 cubemap × BRDF LUT 두 항의 곱으로 분리.
- **Heitz 2014** — *Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs* (JCGT). G항(Smith)의 올바른 유도·height-correlated 형태. 에너지 정확도의 기준.
- **Heitz, Hanika, d'Eon, Dachsbacher 2016** — *Multiple-Scattering Microfacet BSDFs with the Smith Model*. microfacet **다중산란** — 거친 표면의 에너지 손실/채도 저하를 해결(ground truth).
- (산업) **Lagarde & de Rousiers 2014** — *Moving Frostbite to PBR* (SIGGRAPH course). 산업 통합 SOTA, 인용·동기용.

### 3.3 알고리즘/파이프라인(의사코드)

직접광 + split-sum IBL:

```text
# --- 사전계산 (오프라인 또는 로드시) ---
prefiltered_env[mip]  = ∫ L_env(ω) · D(roughness=mip) dω      # roughness별 흐린 환경맵
brdf_LUT(NdotV, rough)= (∫ ... ) → (scale, bias)              # 2D LUT, split-sum의 BRDF 항

# --- 런타임 픽셀 셰이딩 ---
for each pixel:
    F0 = lerp(0.04, albedo, metallic)
    # (a) analytic lights
    for l in lights:
        h = normalize(V + L_dir)
        D = D_GGX(NdotH, roughness)
        F = F0 + (1−F0)·(1−VdotH)^5
        G = G_SmithHeightCorrelated(NdotV, NdotL, roughness)
        spec = D·F·G / (4·NdotV·NdotL)
        diff = (1−metallic)·albedo/π
        Lo += (diff + spec) · radiance(l) · NdotL
    # (b) IBL (split-sum)
    prefiltered = sample(prefiltered_env, R, roughness)
    (s, b)      = sample(brdf_LUT, NdotV, roughness)
    Lo += prefiltered · (F0·s + b)        # specular IBL
    Lo += irradiance(N) · albedo·(1−metallic)   # diffuse IBL (probe/SH)
    # (c) multiscatter 보정 (§3.4)
    Lo *= energy_compensation(F0, roughness, brdf_LUT)
    output = Lo
```

### 3.4 박사급 novel 기여 각도 (open problems)

- **실시간 multiscatter**: Heitz 2016은 stochastic(오프라인). 실시간은 Kulla-Conty/Fdez-Aguera류 LUT 근사를 쓰지만 근사 오차가 있다.
  → "단일 LUT fetch로 다층/이방성까지 포함하는 에너지 보존 multiscatter 근사" + ground truth(Heitz 2016) 대비 ΔE 한계 분석. (새 알고리즘+이론)
- **다층 머티리얼(layered)**: clear-coat·car paint·피부(SSS)·천(sheen)의 물리적 층 결합.
  → "실시간 layered BRDF의 효율적 importance 분배·에너지 보존" — open problem이 분명한 영역.
- **이방성·실 단위(fabric/hair)**: 게임에서 비싸고 근사가 거침.
- **색역/스펙트럴 정확도**: metal의 색 있는 Fresnel, ΔE 기반 품질.

가이드 §5: 여기는 **이론 기여(4)**가 특히 잘 붙는다(에너지 보존은 증명 가능한 성질).

### 3.5 Thesis statement 예시

> "거친 도전체(rough conductor)의 multiscatter 에너지 보상을 Fresnel·roughness의 2D LUT **1회 fetch**로 근사하는 모델은,
> Heitz 2016 stochastic ground truth 대비 알베도 오차를 **ΔE < 1**로 유지하면서
> 픽셀당 추가 비용을 **1 texture fetch**로 제한해 실시간 60fps에서 에너지 손실을 제거한다."

### 3.6 평가 방법

- **정확도**: white furnace test(균일 환경광에서 반사 에너지=입사여야 함)로 에너지 보존을 직접 검증, roughness 스윕에서 **에너지 손실 곡선**.
- **품질**: Heitz 2016 / path-traced multiscatter ground truth와 **ΔE, FLIP, SSIM**. roughness·metallic·각도 그리드로.
- **성능**: 픽셀당 ALU·texture fetch, frame time ms.
- **Ablation**: 단일산란만 / +LUT 보상 / full multiscatter — 시각·수치 차이.
- **Baseline**: Karis split-sum(보상 없음), Kulla-Conty 근사, ground truth.

### 3.7 Winters 연결점

- `ModelRenderer.h`에 **이미 `UsesPBR()`**가 있다 — PBR/non-PBR 경로가 공존하는 실제 코드. 새 BRDF 모델을 PBR 경로에 끼워 두 모드(MOBA 양식화 vs 오픈월드 사실주의)에서 같은 머티리얼이 어떻게 보이는지 A/B 가능.
- **양식화 vs 사실주의의 긴장**: LoL식 MOBA는 종종 **non-PBR/stylized**(채도·가독성 우선)인데 오픈월드(Elden Ring 자산)는 사실적 PBR을 원한다.
  → "동일 자산을 양식화/사실 두 셰이딩으로 일관되게 렌더하는 파라미터 매핑"은 I3D/TVCG형(인지 품질) 기여 각도.
- **모바일 셰이더 permutation**: `RHI_BACKEND_STRATEGY` §6 "shader permutation 줄이기"와 직결 — multiscatter LUT 1-fetch 근사는 permutation/ALU를 안 늘리므로 모바일 친화. 기여의 "왜 중요한가"(§Heilmeier)가 Winters에서 자명.
- **HLSL authoring 유지**(§5): DXC→DXIL(DX12)·DXC→SPIR-V(Vulkan)로 같은 BRDF 셰이더가 두 backend에 나가므로, BRDF 기여를 두 RHI에서 동일 코드로 평가 가능.

---

## 4. Forward+ (Tiled / Clustered Forward)

### 4.1 핵심 원리(이론/수학)

Forward+는 **forward의 머티리얼 자유·투명·MSAA 친화성**을 지키면서
**deferred의 라이트 컬링**을 빌려온다. 핵심은 **depth prepass + per-tile light list**다.

```text
1) Depth prepass : 깊이만 먼저 그림 → 타일별 z_min/z_max 확보(+ 후속 오버드로 차단)
2) Light culling : 화면을 타일(또는 클러스터)로 나눠 각 타일의 절두체와
                   교차하는 라이트 인덱스 리스트를 compute로 생성
3) Forward shading: 각 픽셀이 자기 타일의 light list만 순회하며 셰이딩
```

deferred와 달리 **G-buffer가 없다** → 대역폭이 작고, 머티리얼별 셰이더 분기·투명·MSAA가 자연스럽다.
비용은 **컬링 오버헤드 + (얕은) 오버드로**로 옮겨간다. 라이팅 복잡도는 deferred와 유사한
`O(P · L_tile)`로, full forward의 `O(P · L)`을 타일 컬링으로 깎은 것이다.

### 4.2 대표 기존 연구(SOTA)

- **Harada, McKee, Yang 2012** — *Forward+: Bringing Deferred Lighting to the Next Level* (Eurographics short / AMD). **Forward+** 명명. depth prepass + tiled light culling.
- **Olsson, Billeter, Assarsson 2012** — *Clustered Deferred and Forward Shading* (HPG). **클러스터드 forward**도 같은 논문에서 제시(타일을 z로 확장). Forward+의 깊이 불연속 약점을 해결.
- **Drobot 2017 (Activision) / Pettineo (Bindless Deferred 비교)** — 산업 비교·실무 SOTA(인용·동기용).
- (대비축) §2의 deferred 계보와 항상 짝으로 논의된다.

### 4.3 알고리즘/파이프라인(의사코드)

```text
# 1) Depth prepass
for each opaque triangle:
    write depth only

# 2) Light culling (compute, 타일당 1 thread group)
for each tile t:
    z_min, z_max = reduce_depth(t)
    frustum = build_tile_frustum(t.xy, z_min, z_max)
    t.light_list = []
    for l in lights:                      # 또는 BVH/클러스터로 가속
        if sphere(l) ∩ frustum: t.light_list.append(l.index)

# 3) Forward shading (실제 지오메트리 다시 그림)
for each pixel x (with full material shader):
    surf = evaluate_material(x)           # 머티리얼별 자유 셰이더
    t = tile_of(x)
    Lo = 0
    for li in t.light_list:               # 타일 라이트만
        Lo += BRDF(surf, light[li]) · NdotL · shadow
    output = Lo + IBL/GI
# 투명: light_list 재사용해 같은 패스 구조로 OIT 없이도 정상 블렌딩
```

### 4.4 박사급 novel 기여 각도 (open problems)

- **라이트 수 확장 + 컬링 비용**: 라이트가 수천~수만이면 per-tile brute-force 컬링이 병목.
  → "라이트 BVH / 계층 컬링 / 라이트 클러스터링 자료구조"로 컬링을 `O(L)`→`O(log L)`급으로. (HPG, 자료구조형)
- **타일 vs 클러스터 적응 선택**: 장면 깊이 분포에 따라 타일이 나을 때/클러스터가 나을 때가 갈린다.
  → "장면 통계 기반 적응 컬링 구조 선택" + 두 워크로드(MOBA/오픈월드)에서 경험적 법칙(§5-5).
- **투명 라이팅 일관성**: forward+의 장점인 투명을 GI·그림자와 정합.
- **모바일 컬링 비용**: tile GPU에서 compute 컬링 오버헤드.

### 4.5 Thesis statement 예시

> "라이트를 화면공간 BVH로 계층 컬링하는 clustered forward는,
> 10,000개 동적 점광원에서 타일 brute-force 대비 컬링 시간을 **5×** 줄이고,
> forward의 투명·MSAA 이점을 유지한 채 deferred 대비 G-buffer 대역폭을 **0(없음)**으로 만든다."

### 4.6 평가 방법

- **확장성(핵심)**: **라이트 수 vs 컬링 ms / 셰이딩 ms** 곡선(가이드 §7 scaling). 라이트 100→10k 스윕.
- **대역폭**: forward+(G-buffer 0) vs deferred 직접 비교(KB/frame).
- **투명/MSAA**: 투명 레이어 수·MSAA 샘플 수 대비 비용·품질.
- **Baseline**: tiled forward+(Harada 2012), clustered forward(Olsson 2012), **clustered deferred(§2)**, full forward(naive).
- **Ablation**: 타일 vs 클러스터, brute-force vs BVH 컬링, depth prepass on/off.

### 4.7 Winters 연결점

- **MOBA = forward+의 이상적 케이스**: 라이트 수가 적당하고 **투명 FX(스킬 이펙트)가 많다** → MSAA·투명 친화의 forward+가 자연스럽다.
  반면 오픈월드는 라이트가 많아 클러스터드/деferred가 유리할 수 있다 → **모드별 forward/deferred 선택**이 시스템 기여(§5-3) 각도이자 천연 ablation.
- **모바일 기본 경로 후보**: `RHI_BACKEND_STRATEGY` §6이 deferred를 "옵션화"로 둔 만큼, 모바일 **기본은 forward+**가 합리적 → 컬링 비용 최적화가 Winters 모바일 목표와 직결.
- **FX 투명 부하 측정**: LoL식 대량 파티클/빔(`FxBeamSystem`, `FxMeshSystem` 등 클라이언트 FX)이 투명 오버드로를 만든다 → forward+ 투명 라이팅의 실제 워크로드 testbed.
- DX12·Vulkan 양쪽에서 compute 컬링(§5 V3/V4)을 동일 RHI 추상으로 평가 가능.

---

## 5. Volumetric Fog (체적 안개 · 대기 산란)

> ⚠️ Winters의 **fog of war**(게임플레이 시야 마스크, `fogofwaroverlay*`)와는 **전혀 다른 주제**다.
> 여기서 fog는 **참여 매질(participating media) 안의 빛 산란**(물리)을 말한다. 논문에서 반드시 구분 명시.

### 5.1 핵심 원리(이론/수학)

빛이 안개·연기·대기를 지날 때 **흡수(absorption)·산란(scattering)·방출(emission)**이 일어난다.
시선(ray)을 따라가는 **radiative transfer equation(RTE)**의 실시간 형태:

```text
L(camera) = ∫_{0}^{D}  T(0,s) · σ_s(s) · ∫_{4π} p(θ) · L_light(s, ω) dω  ds
          + T(0,D) · L_background

  T(a,b) = exp( − ∫_a^b σ_t(t) dt )      투과율(transmittance), σ_t = σ_a + σ_s (Beer-Lambert)
  σ_s    : scattering coefficient
  σ_t    : extinction coefficient
  p(θ)   : phase function (산란 방향 분포)
```

위상함수는 보통 **Henyey-Greenstein**:

```text
p_HG(θ) = (1 − g²) / ( 4π · (1 + g² − 2g·cosθ)^{3/2} ),   g ∈ (−1,1)
  g>0 전방 산란(안개), g≈0 등방, g<0 후방
```

실시간 구현의 핵심 자료구조는 **froxel(frustum-aligned voxel) 3D 텍스처**다.
카메라 절두체를 (x,y) 화면 타일 × z 슬라이스로 나눠 각 froxel에 산란/투과 정보를 적분한다.

### 5.2 대표 기존 연구(SOTA)

- **Wronski 2014** — *Volumetric Fog: Unified Compute Shader-Based Solution to Atmospheric Scattering* (SIGGRAPH talk, Assassin's Creed 4). **froxel 기반 실시간 체적 안개**의 표준. (1) 매질 속성 froxel 기록 → (2) 라이트 산란 froxel 계산 → (3) z방향 누적 적분 → (4) 픽셀에서 froxel 샘플.
- **Hillaire 2015 / 2020** — *Physically Based Sky, Atmosphere and Cloud Rendering* (Frostbite) 및 *A Scalable and Production Ready Sky and Atmosphere*. 대기 산란(Rayleigh/Mie)의 실시간 LUT 기법. SIGGRAPH course.
- **Bruneton & Neyret 2008** — *Precomputed Atmospheric Scattering*. 대기 산란 사전계산의 고전.
- **Fong et al. 2017** — *Production Volume Rendering* (SIGGRAPH course). 오프라인 ground truth·이론 레퍼런스.
- (구름) **Schneider & Vos 2015** (Horizon, GDC), **Hillaire** — volumetric cloud, 인용·동기용.

### 5.3 알고리즘/파이프라인(의사코드)

Wronski식 froxel 파이프라인:

```text
# 3D froxel texture: (tilesX, tilesY, Zslices), z는 보통 지수 분포
# 1) 매질 속성 기록 (compute)
for each froxel f:
    f.σ_s, f.σ_a = sample_media(f.worldpos)   # 안개 밀도/색
    f.emissive   = ...

# 2) in-scattering 계산 (compute)
for each froxel f:
    Lscat = 0
    for l in lights:
        vis   = shadow(l, f.worldpos)         # 그림자 → "god ray"
        phase = HG(angle(view, l), g)
        Lscat += l.radiance · phase · vis · f.σ_s
    f.scatter      = Lscat
    f.extinction   = f.σ_a + f.σ_s
    # temporal reprojection: 이전 프레임 froxel과 블렌딩 (노이즈↓)
    f.scatter = lerp(history(f), f.scatter, α)

# 3) z방향 누적 적분 (앞→뒤로 ray march, scattering integration)
accum_scatter = 0; transmittance = 1
for z in 0..Zslices:
    s = froxel[x,y,z]
    accum_scatter += transmittance · s.scatter · slice_depth
    transmittance *= exp(−s.extinction · slice_depth)
    froxel_integrated[x,y,z] = (accum_scatter, transmittance)

# 4) 픽셀 적용
for each pixel x:
    (scatter, T) = sample3D(froxel_integrated, x.uv, linearize(x.depth))
    output = scatter + T · scene_color(x)
```

### 5.4 박사급 novel 기여 각도 (open problems)

- **노이즈 vs froxel 해상도**: froxel은 저해상도(메모리·성능) ↔ 고주파 그림자 산란(god ray)에서 aliasing/노이즈.
  → "blue-noise jitter + temporal 누적의 안정화" 또는 "적응 froxel 해상도(중요 영역만 조밀)". (알고리즘+자료구조)
- **그림자 산란(volumetric shadows) 비용**: froxel마다 shadow 샘플이 비싸다.
  → "froxel용 저비용 가시성 캐시 / epipolar 샘플링의 실시간 변형".
- **Temporal 안정성**: 카메라·안개 이동 시 reprojection ghosting.
  → "체적 매질의 disocclusion-aware reprojection".
- **다중 산란(multiple scattering)**: 단일산란만 하면 짙은 구름/안개가 어둡고 평평. 실시간 다중산란 근사.
- **성능 예산**: 모바일에서 froxel 자체가 사치 → 저사양 근사.

### 5.5 Thesis statement 예시

> "Froxel in-scattering을 blue-noise 시간적 분산으로 적분하고 disocclusion을 매질 깊이로 보정하면,
> 동적 카메라에서 ghosting·flicker 없이(temporal error **50%↓** vs naive reprojection)
> 64³ froxel만으로 god-ray 품질(SSIM **0.95+** vs 고해상 ground truth)을 16ms 예산 안에 달성한다."

### 5.6 평가 방법

- **품질**: 고해상도/오프라인 ray-march ground truth와 **SSIM/FLIP/PSNR**, god-ray·구름 경계 집중 비교.
- **시간적 안정성(핵심)**: 카메라 이동 시퀀스의 **프레임 차분(temporal flicker)**, ghosting 정성·정량.
- **성능**: froxel 단계별 ms, froxel 해상도 스윕(32³~128³) vs 품질/시간.
- **메모리**: froxel 3D 텍스처 MB.
- **Ablation**: temporal on/off, blue-noise on/off, 적응 해상도 on/off, 단일 vs 다중산란.
- **Baseline**: Wronski 2014 froxel(naive reprojection), Hillaire 대기.

### 5.7 Winters 연결점

- **fog of war ≠ volumetric fog 명시(재강조)**: Winters는 `fogofwaroverlay`(2D 시야 마스크)를 이미 가진다. 박사 문서·코드에서 **체적 안개(라이팅)와 게임플레이 안개(가시성 규칙)를 이름·시스템 모두 분리**해야 심사·협업 혼선이 없다(가이드 §11 clarity).
- **오픈월드에서만 가치**: 탑다운 MOBA는 카메라가 매질을 거의 통과하지 않아 froxel 이득이 작다. 체적 안개는 **오픈월드(Elden Ring 자산)의 실외 분위기**에 효과적 → "두 모드 워크로드 대비"에서 froxel의 ROI가 모드별로 갈리는 것을 보이는 게 좋은 ablation/경험 기여(§5-5).
- **LoL FX와의 구분**: 클라이언트의 안개류 파티클(`*_fog*.tex`, `sru_*fog*`)은 **빌보드 파티클 FX**이지 froxel 산란이 아니다. "값싼 양식화 안개(FX) vs 물리 froxel 안개"의 품질/비용 대비가 Winters만의 testbed 강점.
- **모바일 예산**: `RHI_BACKEND_STRATEGY` §6 기조상 froxel은 모바일에서 옵션/저해상 → 저사양 근사 기여의 동기가 Winters에 내장.

---

## 6. Real-Time Ray Tracing

### 6.1 핵심 원리(이론/수학)

ray tracing은 §1.1 렌더링 방정식의 적분을 **광선 추적 + 몬테카를로**로 직접 추정한다.
픽셀 radiance는 경로(path)들의 기대값:

```text
L(x, ω_o) ≈ (1/N) Σ_{k=1}^{N}  f_r·L_i·(n·ω_i) / pdf(ω_i)     (importance sampling)
```

실시간의 두 기둥:
1. **가속 구조 BVH(Bounding Volume Hierarchy)** — `O(log n)` 광선-삼각형 교차. HW(RTX)가 BVH traversal·intersection을 고정함수로 가속(DXR의 TLAS/BLAS).
2. **극저 샘플 수(1 spp 수준) + 디노이징** — 실시간은 픽셀당 1~수 경로만 → 분산(노이즈)이 큼 → **시공간 디노이저**로 분산을 줄여야 한다.

DXR 셰이더 스테이지: ray generation → (traverse BVH) → closest-hit / any-hit / miss.
실시간은 보통 **하이브리드**: G-buffer는 raster(§2)로 만들고, 반사·그림자·GI만 ray로 보강.

### 6.2 대표 기존 연구(SOTA)

- **Whitted 1980** — *An Improved Illumination Model for Shaded Display*. 재귀 ray tracing(반사·굴절·그림자)의 효시.
- **Kajiya 1986** — path tracing(§1). 몬테카를로 라이트 트랜스포트.
- **Bitterli, Wyman, Pharr, Shirley, Lefohn, Jarosz 2020** — *Spatiotemporal Reservoir Resampling for Real-Time Ray Tracing with Dynamic Direct Lighting* (**ReSTIR**, SIGGRAPH/TOG). reservoir 기반 RIS(resampled importance sampling)를 시공간으로 재사용해 수천 광원의 직접광을 1 spp급으로. → **ReSTIR GI**(§1, Ouyang 2021), **ReSTIR PT**(Lin et al. 2022)로 확장.
- **Schied et al. 2017** — *Spatiotemporal Variance-Guided Filtering* (**SVGF**, HPG). 1spp 경로추적을 분산 추정으로 안내하는 시공간 디노이저. 실시간 디노이징의 기준선. 후속 **A-SVGF**(2018).
- **NVIDIA RTXGI / NRD(Real-Time Denoisers) / ReBLUR, ReLAX** — 산업 디노이저 SOTA(인용·동기).
- **Wright et al. (Lumen)** — HW RT + SW trace 하이브리드(§1), 산업 SOTA.

### 6.3 알고리즘/파이프라인(의사코드)

하이브리드 1spp + 디노이즈:

```text
# 1) G-buffer: raster (§2)로 primary visibility (BVH primary ray 불필요)
write G-buffer(pos, normal, roughness, motion_vector, ...)

# 2) ray pass (DXR), 픽셀당 1 spp
for each pixel x:
    surf = Gbuffer(x)
    # 반사/그림자/GI 중 필요한 것만
    ω_i  = sample_BRDF_or_light(surf)           # ReSTIR이면 reservoir에서 후보 재사용
    hit  = TraceRay(TLAS, surf.pos, ω_i)
    raw[x] = shade(hit) / pdf                    # 노이즈 큰 1spp 결과

# 3) 디노이즈 (SVGF류)
moments  = temporal_accumulate(raw, history, motion_vector)   # 재투영 누적
variance = estimate_variance(moments)
denoised = edge_aware_atrous(raw, variance,
                             weights=f(depth, normal, luma))   # 분산↑일수록 더 흐림
# 4) 합성
output = composite(Gbuffer_direct, denoised_reflection/GI)
output = TAA(output, history)
```

### 6.4 박사급 novel 기여 각도 (open problems)

- **디노이징 시간적 안정성(핵심 open problem)**: 1spp + reprojection은 disocclusion(가려졌다 드러난 영역)에서
  ghosting·boiling·lag이 생긴다. SVGF도 빠른 모션·얇은 구조에서 약함.
  → "곡률/머티리얼 인지 가중 누적", "분산 기반 적응 history length", "ReSTIR 재사용과 디노이저의 공동 설계". (알고리즘+이론, EGSR/HPG)
- **동적 BVH 갱신 비용**: 스키닝·파괴·대규모 동적 장면에서 BLAS/TLAS refit/rebuild가 비싸다.
  → "변형 정도 기반 적응 refit vs rebuild 스케줄링", "LOD-aware BVH". (자료구조, HPG)
- **소비자 HW 예산**: RTX는 여전히 비싸고, 미드레인지·모바일은 RT 코어가 없거나 약함.
  → "RT 없는 GPU의 SW trace 하이브리드", "RT를 거친 GI에만 쓰고 나머지는 raster" 예산 배분. ← Winters 모바일/콘솔 전략과 직결.
- **하이브리드 raster+RT 정합**: raster G-buffer와 RT 결과의 일관성(샘플링·MSAA·투명).

### 6.5 Thesis statement 예시

> "ReSTIR 재사용 통계와 디노이저 history를 공동 설계해 disocclusion을 곡률로 가중하면,
> 1spp 하이브리드 반사에서 ghosting 없이(temporal error **−50%** vs SVGF) RTX 4070 기준 **2ms** 디노이징으로
> 동적 장면 반사 품질(FLIP **<0.5** vs offline PT)을 60fps 예산 안에 유지한다."

### 6.6 평가 방법

- **품질**: offline path tracer(수천 spp) ground truth와 **PSNR/SSIM/FLIP**; 반사·그림자·GI 분리 측정.
- **시간적 안정성(핵심)**: 동적 카메라·오브젝트 시퀀스에서 **temporal error / ghosting**(연속 프레임), disocclusion 영역 집중.
- **성능**: ray pass ms, 디노이즈 ms, **BVH build/refit ms**(동적 장면), 총 RT ms. HW 명시(RTX 세대 — 가이드 §7 재현성).
- **Ablation**: ReSTIR on/off, temporal on/off, 가중치 항(depth/normal/곡률) 제거, refit vs rebuild.
- **Baseline**: SVGF / A-SVGF, NRD(ReBLUR/ReLAX), naive temporal, (가능시) Lumen HW path.
- **샘플 예산 스윕**: 1·2·4 spp vs 품질/시간.

### 6.7 Winters 연결점

- **하이브리드가 Winters 전략의 핵심**: `RHI_BACKEND_STRATEGY`는 DX12 메인(M9에 compute/async queue)·Vulkan을 두고, 모바일/구형은 RT 없음을 전제 → **"RT는 고사양 옵션, raster가 baseline"**이라는 예산 배분이 곧 §6.4 open problem.
  DX12 **DXR vs Vulkan ray tracing(VK_KHR_ray_tracing)**을 같은 RHI 추상으로 두고 가용성에 따라 fallback하는 설계가 자연스럽다.
- **두 모드 = 동적 BVH 워크로드 대비**: MOBA는 스키닝 챔피언·미니언이 많지만 장면이 작고 카메라 고정(반사 ROI 낮음) ↔ 오픈월드는 거대·자유 카메라(반사·그림자 RT ROI 높음). **동적 BVH 갱신 비용이 모드별로 어떻게 다른가**가 천연 실험(가이드 §5-5).
- **MOBA에 RT가 과한가**: 탑다운 가독성 위주 MOBA에선 풀 RT GI가 사치 → "RT를 거울 같은 specular 반사에만 선택 적용" 같은 예산 정책이 Winters식 기여 각도. (§1의 "LoL식 FX는 값싼 GI" 기조와 일관)
- **측정 인프라 강점**(가이드 §7): CLAUDE.md의 "inspectable debug overlay + bounded trace" 문화 → ray/디노이즈 단계별 ms·분산 맵·history 시각화를 띄우면 그 자체가 박사 평가용 측정 인프라가 된다.

---

## 종합. 통합 학위논문 구조 예시

가이드 §4의 "Three Papers Make a Thesis"를 그래픽스에 적용한다.
**한 분야 안의 인접한 3개 문제**를 골라 하나의 thesis statement로 묶는다.

### 예시 학위논문: "동적 장면을 위한 실시간 전역조명: 안정성·이식성·예산"

> **학위논문 thesis statement:**
> "Probe 가시성의 점진 갱신과 곡률 인지 보정, 그리고 RT 유무에 무관한 trace 추상화를 결합하면,
> 동적 장면에서 light leak·temporal flicker 없이 diffuse GI를 데스크톱 RTX와 모바일 tile GPU 양쪽에서
> frame budget 안에 갱신할 수 있다."

```text
Ch 1. 서론
  - 동기: 실시간 GI는 정적엔 풀렸으나 동적·저사양·안정성이 미해결(§1.4)
  - thesis statement(위)
  - 기여 3가지 = 논문 3편

Ch 2. 배경 — 렌더링 방정식(§1.1), microfacet/PBR(§3), deferred/forward 라이팅(§2,§4),
       GI 계보(radiosity→VPL→VXGI→DDGI→ReSTIR GI→Lumen), 디노이징(§6) 비판적 정리 + gap

Ch 3. [논문 1 — I3D/HPG]  "점진 갱신 irradiance field"
  - 변화 영역만 갱신하는 자료구조 + 우선순위 스케줄러 (§1.4 동적 갱신비용)
  - 평가: DDGI 대비 동적 장면 갱신 ms↓, 메모리↓, 품질 동등(§1.6)

Ch 4. [논문 2 — EGSR]  "곡률 인지 가시성으로 light leak·flicker 제거"
  - Chebyshev 가시성의 곡률/두께 보정 + 적응 hysteresis (§1.4 leak·temporal)
  - 평가: leak 정량(ΔE), temporal flicker, ground-truth PT 대비(§1.6)

Ch 5. [논문 3 — HPG/I3D]  "RT 유무 무관 trace 추상화로 모바일 이식"
  - HW RT(DXR/VK) ↔ SW SDF/voxel trace 통합 인터페이스 + 예산 배분 (§6.4, §1.4 저사양)
  - 평가: 데스크톱 vs 모바일 TBDR 성능·품질·대역폭, 두 게임 모드 워크로드 대비

Ch 6. 종합 평가
  - 세 기여를 합친 end-to-end: MOBA vs 오픈월드(Winters testbed) 양쪽
  - baseline: DDGI, VXGI, (가능시) Lumen SW path / naive 재계산
  - ablation: 세 기여를 하나씩 끄며 효과 분리
  - threats to validity: HW 세대 의존, 장면 다양성, ground truth의 한계

Ch 7. 논의 — 일반화(다른 엔진·장르), specular GI로의 확장 가능성

Ch 8. 결론 + future work — specular/glossy GI, 다중산란 매질(§5)과의 통합
```

이 구조의 강점(가이드 §12 루브릭 대응):
- **Novelty**: 세 기여 각각이 DDGI/SVGF 대비 명확히 새롭다.
- **Rigor**: 모든 챕터가 ground-truth PT 대비 + ablation + 두 워크로드.
- **Significance**: "동적·저사양 실시간 GI"는 산업 미해결(Lumen조차 무겁다) → Winters의 모바일/콘솔 전략에 직접 기여.
- **Clarity**: 한 thesis statement(안정성·이식성·예산)로 세 논문이 묶인다.

### 다른 묶음 후보(참고)
- "모바일 tile GPU를 위한 대역폭 최소 렌더링": deferred 압축(§2) + forward+ 컬링(§4) + froxel 저사양 안개(§5) → 한 thesis("on-chip tile memory를 떠나지 않는 렌더링 파이프라인").
- "실시간 라이트 트랜스포트의 시간적 안정성": ReSTIR GI(§1,§6) + 디노이징(§6) + 체적 산란 temporal(§5) → 한 thesis("disocclusion-aware 시공간 재사용").

---

## 참고문헌

(저자/연도 — 확실한 대표 문헌만. 산업 발표는 학술 인용과 구분.)

**전역조명 / 라이트 트랜스포트**
- Kajiya, J. T. 1986. *The Rendering Equation.* SIGGRAPH.
- Goral, C., Torrance, K., Greenberg, D., Battaile, B. 1984. *Modeling the Interaction of Light Between Diffuse Surfaces.* SIGGRAPH. (Radiosity)
- Cohen, M. F., Greenberg, D. P. 1985. *The Hemi-Cube: A Radiosity Solution for Complex Environments.* SIGGRAPH.
- Jensen, H. W. 1996. *Global Illumination Using Photon Maps.* Eurographics Rendering Workshop.
- Keller, A. 1997. *Instant Radiosity.* SIGGRAPH. (VPL)
- Crassin, C., Neyret, F., Sainz, M., Green, S., Eisemann, E. 2011. *Interactive Indirect Illumination Using Voxel Cone Tracing.* Pacific Graphics / Computer Graphics Forum. (VXGI)
- Majercik, Z., Guertin, J.-P., Nowrouzezahrai, D., McGuire, M. 2019. *Dynamic Diffuse Global Illumination with Ray-Traced Irradiance Fields.* JCGT. (DDGI)
- Ouyang, Y., Liu, S., Kettunen, M., Pharr, M., Lehtinen, J. 2021. *ReSTIR GI: Path Resampling for Real-Time Path Tracing.* Computer Graphics Forum (HPG).

**Deferred / Forward+ / 라이트 컬링**
- Deering, M., Winner, S., Schediwy, B., Duffy, C., Hunt, N. 1988. *The Triangle Processor and Normal Vector Shader.* SIGGRAPH.
- Saito, T., Takahashi, T. 1990. *Comprehensible Rendering of 3-D Shapes.* SIGGRAPH. (G-buffer)
- Olsson, O., Billeter, M., Assarsson, U. 2012. *Clustered Deferred and Forward Shading.* HPG.
- Harada, T., McKee, J., Yang, J. C. 2012. *Forward+: Bringing Deferred Lighting to the Next Level.* Eurographics (short).

**PBR / BRDF**
- Cook, R. L., Torrance, K. E. 1982. *A Reflectance Model for Computer Graphics.* ACM TOG.
- Walter, B., Marschner, S. R., Li, H., Torrance, K. E. 2007. *Microfacet Models for Refraction through Rough Surfaces.* EGSR. (GGX)
- Burley, B. 2012. *Physically-Based Shading at Disney.* SIGGRAPH Course. (Disney principled BRDF)
- Karis, B. 2013. *Real Shading in Unreal Engine 4.* SIGGRAPH Course. (IBL split-sum)
- Heitz, E. 2014. *Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs.* JCGT.
- Heitz, E., Hanika, J., d'Eon, E., Dachsbacher, C. 2016. *Multiple-Scattering Microfacet BSDFs with the Smith Model.* ACM TOG (SIGGRAPH).
- Lagarde, S., de Rousiers, C. 2014. *Moving Frostbite to Physically Based Rendering.* SIGGRAPH Course. (산업)

**Volumetric / 대기 산란**
- Wronski, B. 2014. *Volumetric Fog: Unified Compute Shader-Based Solution to Atmospheric Scattering.* SIGGRAPH Talk. (froxel, AC4)
- Bruneton, E., Neyret, F. 2008. *Precomputed Atmospheric Scattering.* EGSR.
- Hillaire, S. 2020. *A Scalable and Production Ready Sky and Atmosphere Rendering Technique.* Computer Graphics Forum (EGSR).
- Fong, J., Wrenninge, M., Kulla, C., Habel, R. 2017. *Production Volume Rendering.* SIGGRAPH Course.

**Ray Tracing / 디노이징**
- Whitted, T. 1980. *An Improved Illumination Model for Shaded Display.* CACM.
- Bitterli, B., Wyman, C., Pharr, M., Shirley, P., Lefohn, A., Jarosz, W. 2020. *Spatiotemporal Reservoir Resampling for Real-Time Ray Tracing with Dynamic Direct Lighting.* ACM TOG (SIGGRAPH). (ReSTIR)
- Schied, C., et al. 2017. *Spatiotemporal Variance-Guided Filtering: Real-Time Reconstruction for Path-Traced Global Illumination.* HPG. (SVGF)
- Lin, D., Kettunen, M., Bitterli, B., Pantaleoni, J., Yuksel, C., Wyman, C. 2022. *Generalized Resampled Importance Sampling.* ACM TOG. (ReSTIR PT)

**산업 SOTA (비학술, 인용·동기·baseline 근거용 — 가이드 §9 주의)**
- Wright, D., et al. (Epic Games). *Lumen: Real-Time Global Illumination in Unreal Engine 5.* SIGGRAPH Course / GDC.
- NVIDIA. *RTXGI / NRD (Real-Time Denoisers: ReBLUR, ReLAX).* 기술 문서.
- Andersson, J. 2009. *Parallel Graphics in Frostbite.* SIGGRAPH Course. (tiled deferred 산업)
