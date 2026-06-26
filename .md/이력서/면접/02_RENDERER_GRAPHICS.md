# 02. Renderer / Graphics 면접 대비 (Winters Engine)

> 도메인: SSAO/PBR/stylized/FoW/스키닝 — DX11 포워드 렌더러
> 정직성 등급: **working** (production 경로 + planned 경로 혼재)
> 근거: `.md/이력서/WINTERS_DOMAIN_HONEST_MAP_2026-06-26.md` ### 2.

---

## 0. 한 줄 본질 + 현재 상태

**본질**: 상용 렌더링 미들웨어(Unreal RHI / Filament / bgfx) 없이, C++20 + HLSL로 DX11 위에 **포워드 렌더러를 직접 구현**한 도메인이다. "화면에 보이는 모든 픽셀의 라이팅을 내가 셰이더로 짰다"가 핵심.

**현재 성숙도(정직하게)**:
- **production(매 프레임 실제 가동)**: Depth+Normal 프리패스 → GTAO 2-pass 컴퓨트 SSAO → 메인 패스 stylized diffuse 라이팅, GPU 스키닝(StructuredBuffer 본 팔레트), CPU 클립공간 프러스텀 컬링, Fog of War 오버레이.
- **working(구현됐으나 메인 경로는 아님)**: Cook-Torrance PBR 셰이더 경로(`Mesh3D_PBR.hlsl` / `Skinned3D_PBR.hlsl`) — 컴파일·바인딩되지만 LoL 룩은 stylized를 디폴트로 쓴다.
- **planned-only(코드 0줄, 문서만)**: RenderGraph, 실시간 GI(SSR/VXGI/DDGI), Nanite, Path Tracing, IBL, Shadow Map(CSM), Deferred, PostFX, TAA.

> **선제 고지 원칙**: "PBR 엔진"이라고 단정하지 않는다. IBL/그림자/디퍼드/PostFX가 없는 forward, 단일 DX11이며, 골든/스모크 자동검증이 아니라 **프로파일러 계측 + 토글 + 수동 시각확인**으로 검증한다 — 이걸 먼저 말한다.

---

## 1. 핵심 개념 (본질)

### 1-1. Forward Rendering — 왜 이 구조인가 (first principles)
라이팅 방정식은 본질적으로 "각 표면 픽셀에서, 모든 광원에 대해 BRDF를 적분"하는 일이다. 이걸 처리하는 두 패러다임:
- **Forward**: 지오메트리를 그리면서 그 자리에서 라이팅을 계산. 셰이더가 `for(light)`를 돈다. 광원 N개 × 픽셀 M개 = N×M (오버드로우 포함). 장점: MSAA·투명·머티리얼 다양성에 자연스럽다. 단점: 광원 많으면 폭발.
- **Deferred**: 먼저 G-Buffer(albedo/normal/depth)를 깔고, 라이팅을 화면공간에서 한 번만. 광원 많을 때 유리하지만 MSAA·투명이 까다롭고 대역폭을 먹는다.

**내 선택은 Forward**. 이유는 1차 원리에서 나온다: **MOBA는 동시 광원이 적다(주광 1 + 포인트 4)**. Deferred의 이득(많은 광원)이 거의 없고, G-Buffer 대역폭·복잡도 비용만 떠안는다. 즉 "범용 엔진의 디폴트(Deferred)"가 아니라 "이 게임의 라이팅 부하"에서 역산한 결정이다.

### 1-2. SSAO / GTAO — Ambient Occlusion이 왜 필요한가
디렉셔널 라이트만으로는 "물체가 바닥에 닿은 틈, 주름 안쪽"이 어둡게 안 깔린다. 그건 **주변광이 기하학적으로 가려지는(occluded)** 현상이고, 풀로 풀면 반구 적분이다. 실시간엔 비싸니 **화면공간 근사(Screen-Space AO)**를 쓴다.

- **핵심 아이디어**: depth/normal 버퍼만 있으면, 각 픽셀 주변을 샘플해 "내 위 반구가 얼마나 막혔나"를 추정할 수 있다. 월드 지오메트리를 다시 안 봐도 된다 — 이미 래스터된 깊이가 곧 가시 표면이다.
- **GTAO(Ground-Truth AO) 계열**: 단순 무작위 샘플(Crytek SSAO)보다, **수평선각(horizon angle)**을 방향별로 적분해 ground-truth 반구 가시성에 가깝게 만드는 방식. 내 구현은 8 방향 × 4 스텝으로 각 방향의 가림(occlusion)을 누적한다.
- **왜 2-pass인가**: 1패스는 노이즈가 많다(샘플 수 한계). 그래서 2패스에서 **bilateral blur**(공간 가중 × 깊이 가중)로 엣지를 보존하며 노이즈만 지운다. 단순 가우시안이면 물체 경계가 번지므로 depth weight로 경계를 살린다.

### 1-3. Cook-Torrance PBR — microfacet 이론
PBR의 1차 원리: 표면을 거울 같은 미세면(microfacet)들의 통계로 모델링. 스펙큘러 BRDF = `D·F·G / (4·NoV·NoL)`.
- **D (NDF, GGX/Trowbridge-Reitz)**: 미세면 법선이 하프벡터 H에 정렬된 비율. roughness가 분포 폭을 정한다. (Disney 관례 `a = roughness²`)
- **F (Fresnel, Schlick)**: 시야각이 스칠수록 반사가 강해진다. 금속/비금속을 `F0`로 구분.
- **G (Geometry, Smith/Schlick-GGX)**: 미세면끼리의 그림자/마스킹.
- **에너지 보존**: `kD = (1-F)(1-metallic)` — 반사된 만큼 디퓨즈를 빼야 1을 안 넘는다.

### 1-4. GPU Skinning — 본 팔레트가 왜 cbuffer→StructuredBuffer로 갔나
스키닝은 정점마다 "영향 본 4개의 변환을 가중 합성(LBS, Linear Blend Skinning)"해 정점을 움직인다. 본 행렬 팔레트를 GPU에 올려야 하는데:
- **cbuffer 한계**: D3D11 상수버퍼는 64KB. float4x4(64B) × 1024본 = 64KB로 **딱 한계**. Elden Ring 보스 같은 1024본 리그가 들어오면 터진다.
- **StructuredBuffer SRV**: 크기 제약이 사실상 없고, VS에서 인덱스 접근. 그래서 본 팔레트를 SRV(t8)로 옮겼다. `row_major`를 명시해 기존 cbuffer 메모리 레이아웃을 그대로 보존(ABI 안전).

### 1-5. Fog of War — MOBA 가시성의 본질
FoW는 "내 시야 밖은 안 보임"을 렌더링으로 구현. 본질은 **per-cell 가시성 값(R8)을 텍스처로 업로드 → 월드/미니맵에 합성**. 게임플레이 truth(어디가 보이나)는 ECS/서버가 정하고, 렌더러는 그 값을 시각화만 한다(권위 분리).

---

## 2. 왜 이 선택인가 — 기술 스택 + Trade-off

### 2-1. Forward vs Deferred
| 선택지 | 장점 | 단점 | 내 결정 |
|---|---|---|---|
| **Forward (선택)** | 구현 단순, 투명/MSAA 자연, 적은 광원에 효율적 | 광원 많으면 폭발, 오버드로우 | MOBA는 광원 1+4. Deferred 이득이 없고 G-Buffer 비용만. |
| Deferred | 많은 광원, 라이팅 화면공간 1회 | G-Buffer 대역폭·복잡도, 투명 난해 | 이 게임 부하에 오버스펙 |

### 2-2. Stylized diffuse vs full PBR (디폴트 룩)
| 선택지 | 장점 | 단점 | 내 결정 |
|---|---|---|---|
| **stylized diffuse (디폴트)** | LoL/Riot 룩, 톱라이트·림·wrap ramp로 가독성↑, 가벼움 | 물리적 정확성 X | MOBA는 챔피언 **실루엣 가독성**이 사실성보다 중요. 디폴트로 채택. |
| Cook-Torrance PBR | 물리 정확, 금속/거칠기 표현 | IBL/그림자 없으면 어둡고 밋밋, 무거움 | 셰이더는 짜뒀으나(`*_PBR.hlsl`) 룩 디폴트는 아님 |

> 정직성: PBR 셰이더는 **존재하고 빌드/바인딩된다**(BRDF_GGX.hlsli + Mesh3D_PBR). 하지만 IBL/그림자가 없어 "production PBR 룩"이라 말하면 과장. "두 라이팅 경로를 모두 구현, 디폴트는 가독성 우선 stylized"가 정확.

### 2-3. GTAO 2-pass vs 단순 SSAO / HBAO
| 선택지 | 장점 | 단점 | 내 결정 |
|---|---|---|---|
| **GTAO horizon + bilateral blur (선택)** | ground-truth에 근접, 엣지 보존 | 방향×스텝 샘플 비용 | 적은 샘플(8×4)로 품질 확보, 컴퓨트로 가속 |
| Crytek SSAO (반구 무작위 샘플) | 단순 | 노이즈·뱅딩, 물리근거 약함 | 품질 부족 |
| HBAO+/풀 GTAO(멀티바운스) | 더 정확 | 무거움, 오버엔지니어링 | MOBA 부하에 과함 |

### 2-4. cbuffer vs StructuredBuffer 본 팔레트
| 선택지 | 장점 | 단점 | 내 결정 |
|---|---|---|---|
| cbuffer (이전) | 빠른 상수 접근 | **64KB = 1024본 한계**, Elden 보스 불가 | 한계로 폐기 |
| **StructuredBuffer SRV (선택)** | 큰 팔레트, 1드로우 스키닝 | SRV 슬롯·바인딩 관리 | 1024본 리그 수용 위해 전환 |

### 2-5. 왜 "신입 1인 프로젝트 범위"에서 이게 합리적인가
RenderGraph/GI/디퍼드를 먼저 깔면 **자유도 3배에 디버깅 3배**다. 정직성 지도(roadmap)에 명시했듯, 지금 Winters 렌더 문제의 핵심은 고급 기법 부재가 아니라 **색공간·diffuse ramp·AO/contact·UI unlit 분리·FoW 팔레트 불일치**다. 그래서 "DX11에서 납득 가능한 기준 이미지를 먼저 고정"하는 순서를 택했다. 백엔드/기법을 늘리기 전에 한 경로를 완성하는 것이 1인 범위에서 합리적이다.

---

## 3. 실제 구현 (코드 근거)

### 3-1. 패스 오케스트레이션 (call path)
`Client/Private/Scene/Scene_InGameRender.cpp`:
- L357 `if (!bRHISceneOnly && bUseDX11RHI && m_pNormalPass && bSSAOEnabled)` 가드.
- L360 `m_pNormalPass->Begin(pDevice)` → 맵/액터를 normal+depth로 그림(L364, L386) → L404 `End`.
- L409 `m_pSSAOPass->Execute(...)`에 normal/depth SRV를 넘김(L411–412).
- L415 `GetOutputSRVNative()` → L434 `m_Map.SetAmbientOcclusionSRV(...)`, L490 액터에도 동일 SRV 전달.
- 메인 패스에서 t5로 바인딩되어 stylized diffuse가 화면공간 AO를 곱한다.

### 3-2. NormalPass (`Engine/Private/Renderer/NormalPass.cpp`)
- L86 Normal RT는 `R16G16B16A16_FLOAT`(법선 정밀도), L105 Depth는 `R24G8_TYPELESS`로 만들고 **DSV(D24_UNORM)와 SRV(R24_UNORM_X8)를 같은 텍스처에 둘 다** 생성(L113–128) — depth를 SSAO 입력으로 재사용하는 핵심.
- L134 static / L146 skinned 두 파이프라인(`NormalOnly.hlsl` / `SkinnedNormalOnly.hlsl`).
- L173 clear normal `{0.5,0.5,1,1}`(인코딩된 +Z 법선). Begin이 이전 RTV/뷰포트를 저장(L167)하고 End가 복원(L189) — 패스 격리.

### 3-3. SSAO 2-pass 컴퓨트 (`Engine/Private/Renderer/SSAOPass.cpp` + `Shaders/SSAO/`)
- raw/filtered 두 AO 텍스처를 `R16_FLOAT` UAV+SRV로 생성(L135–161). raw에 GTAO를 쓰고, filtered에 blur 결과.
- **GTAO 패스**(`GTAO_CS.hlsl`): depth로 월드포즈 복원(`ReconstructWorldPos`, viewProjInv 사용, L20–27), 8방향×4스텝(L47–48), 각 샘플의 `ndotDir`·거리가중·thickness heuristic으로 occlusion 누적(L80–86), 최종 `pow(ao, intensity)`(L92).
- **Blur 패스**(`GTAO_Blur_CS.hlsl`): 5×5 **bilateral** — `spatialWeight = exp(-0.5·r²)` × `depthWeight = exp(-|Δdepth|·80)`(L33–35)로 엣지 보존.
- 디스패치는 `(w+7)/8, (h+7)/8`(8×8 스레드그룹, SSAOPass.cpp L258, L287). 패스 후 SRV/UAV 언바인드로 RW 해저드 차단(L260–263).

### 3-4. Stylized diffuse 라이팅 (`Shaders/Mesh3D.hlsl`, `Skinned3D.hlsl`)
- `StylizedMapRamp` / `StylizedChampionRamp`: NdotL에 wrap(0.34/0.42)을 더해 그림자 경계를 부드럽게(Mesh3D L68, Skinned3D L78).
- `ApplyStylizedDiffuse`: key(주광 ramp) + top-light(N.y) + grazing/rim + 포인트 라이트 accent + **화면공간 AO 곱**(Mesh3D L123–124, Skinned3D L135–136).
- 셰이더-로컬 sRGB: `SrgbToLinearApprox`/`LinearToSrgbApprox`(pow 2.2)로 라이팅을 선형공간에서 하고 마지막에 감마(L163–168) — double gamma 방지(roadmap Session 12).
- `SampleScreenAO`: SV_POSITION.xy / screenSize로 AO UV 복원(Mesh3D L61–66).

### 3-5. Cook-Torrance PBR 경로 (`Shaders/BRDF/BRDF_GGX.hlsli`)
- `D_GGX`/`G_Smith`/`F_Schlick`/`BRDF_CookTorrance` 전부 직접 구현(L7–63). `Mesh3D_PBR.hlsl`이 dir 1 + point 4(L119–123), Reinhard 톤맵 `c/(c+1)` + 감마(L129–130).

### 3-6. GPU 스키닝 본 팔레트 (`Engine/Private/Renderer/ModelRenderer.cpp`)
- L30 `kMaxGPUBones = 1024`, L40 `ByteWidth = sizeof(XMFLOAT4X4)*1024`, L53 `NumElements=1024`인 StructuredBuffer + SRV.
- L483/L617 `bonesSRV.Update(...)`로 애니메이터 최종 본행렬 업로드, L503 `BindVS(pContext, 8)`로 t8 바인딩.
- 셰이더 측 `Skinned3D.hlsl` L33–37 `StructuredBuffer<SkinBoneMatrix>`, L157–161 LBS 4본 가중합.

### 3-7. 프러스텀 컬링 (`ModelRenderer.cpp`)
- L447 `matLocalToClip = world × viewProj`, L453 `BuildClipVisibilityMask(matLocalToClip, &bAnyVisible)` — **클립공간 가시성 마스크**를 CPU에서 빌드해 submesh 단위로 드로우 스킵. L455 전부 안 보이면 early-out.

### 3-8. Fog of War (`Engine/Private/Renderer/FogOfWarRenderer.cpp`)
- L192 가시성 텍스처 `R8_UNORM`, 월드 오버레이 리소스(L217) + L232 미니맵 오버레이 `R8G8B8A8_UNORM`(CPU write). 월드 쿼드 합성 + 미니맵 RGBA 합성.

---

## 4. 검증 — 동작을 어떻게 증명했나

> **정직선**: 이 도메인은 **골든/스모크 자동검증이 없다.** 검증은 (1) 프로파일러 계측 (2) 런타임 토글 (3) 수동 시각확인 (4) fxc 셰이더 컴파일 게이트 (5) 빌드 게이트.

- **프로파일러 스코프**: `WINTERS_PROFILE_SCOPE("Render::NormalPass")`, `"ModelRenderer::RenderSkinned"`, `"BuildClipVisibilityMask"` 등으로 매 프레임 각 패스 비용을 계측. 이 계측이 "작은 씬인데도 완전 CPU 바운드(드로우콜 ~94)"를 드러냈다(정직성 지도 #12).
- **토글 검증**: `m_pSSAOPass->SetEnabled(false)`로 AO on/off 비교, `bRHISceneOnly` 플래그로 RHI/legacy 비교. SSAO 파라미터(radius/intensity/thickness)는 ImGui로 실시간 튜닝(SSAOPass.h L32–42).
- **셰이더 컴파일 게이트**: `fxc`로 VS/PS/CS 검증(roadmap §2: ContactShadowPlane, Engine/Client/Server Debug x64 빌드 통과 기록).
- **F5 시각 게이트(체크리스트)**: 같은 맵/카메라/챔피언 비교. 실패 기준 명문화 — 챔피언 하이라이트가 "얼음처럼" 뜨면 실패, UI/HP bar/AttackRange가 월드 조명·fog에 물들면 unlit 회귀, ContactShadow가 UI 위로 뜨면 실패(roadmap §2).
- **색공간 회귀 검증**: Session 12에서 텍스처 로더 sRGB 정책을 `ShaderLocalSRGB`로 고정해 double-gamma 회귀를 차단, 빌드 통과 확인.

---

## 5. 최적화

### 실제로 한 것
- **본 팔레트 cbuffer→StructuredBuffer**: 64KB 한계 제거 + **1드로우 스키닝**(메시 분할 없이 1024본). 코드 근거 ModelRenderer L30–64.
- **클립공간 프러스텀 컬링**: submesh 단위 가시성 마스크로 드로우콜 절감, 전부 비가시면 early-out(L455).
- **SSAO 8×8 컴퓨트 디스패치**: GTAO를 픽셀셰이더가 아니라 컴퓨트로, 8×8 타일·point sampler·RW 해저드 언바인드.
- **AO 텍스처 R16_FLOAT, normal R16G16B16A16**: 필요 정밀도만 — depth는 24bit, AO는 half로 대역폭 절약.
- **SSAO 가드**: AO 비활성/SRV 없을 때 normal 프리패스 자체를 스킵(Scene_InGameRender L357) — 불필요 패스 제거.

> **정량 수치는 정직하게**: 9.54ms/94 드로우콜은 **F4로 캡처한 JSON에서 보여줄 수 있는 값**이지 외워 말하는 마케팅 수치가 아니다. SSAO/스키닝 최적화의 before/after FPS 수치는 **측정 예정**(별도 골든 캡처 없음).

### 계획 중인 최적화
- static mesh **batching/instancing**(맵 드로우콜 ~94의 주범 해소 — 정직성 지도 Map 게이트 룰: F5 숨김이 아니라 Engine generic batching으로).
- AO **half-res 렌더 + upsample**(현재 풀해상도).
- 본 팔레트 **double-buffer/persist-map**으로 매 프레임 Update 비용 감소.

---

## 6. 구현 예정 (Planned) — 동일한 깊이로

> 이건 실제로 구현할 항목이다. 면접에서 "아직 안 했죠? 어떻게 할 건가"에 막힘없이 답하기 위해 무엇/왜/어떻게/Trade-off/검증을 적는다.

### 6-1. Shadow Map (CSM) — 가장 시급
- **무엇/왜**: 현재 그림자가 없어 "물체가 바닥에 닿은" 느낌이 SSAO contact AO에만 의존한다. 방향광 그림자(Cascaded Shadow Map)로 드라마틱한 셀프섀도/드롭섀도를 만든다.
- **어떻게**: 라이트 시점 depth-only 패스로 shadow atlas 렌더 → 카메라 프러스텀을 N개 cascade로 분할 → 메인 PS에서 라이트공간 변환 후 PCF 비교. 자료구조: cascade별 viewProj 배열 + depth array texture.
- **Trade-off**: cascade 분할 경계 깜빡임(stabilization 필요), peter-panning/acne(depth bias·normal-offset로). 메모리 vs 품질.
- **검증**: 그림자 on/off 토글 + F5 시각 게이트(발밑 접촉, 경계 acne 없음) + 프로파일러로 shadow 패스 비용 계측.

### 6-2. IBL (Image-Based Lighting)
- **무엇/왜**: PBR 셰이더가 있어도 환경광(ambient)이 상수(0.03)라 금속/거칠기 표현이 죽는다. 큐브맵 기반 diffuse irradiance + specular prefilter + BRDF LUT(split-sum, Karis 2013)로 PBR을 "살린다".
- **어떻게**: 오프라인에 irradiance map(cosine 가중 적분) + prefiltered env map(roughness별 mip) + BRDF integration LUT 생성 → PS에서 `prefilter·(F·scale+bias)`. cbuffer에 mip 카운트.
- **Trade-off**: 프리컴퓨트 비용·VRAM(큐브맵 mip chain) vs 룩 품질. 동적 환경엔 캡처 갱신 필요.
- **검증**: ground-truth path tracer(로드맵 Stage 4) 스크린샷과 대조 — 이게 GI 검증의 골든.

### 6-3. RenderGraph (멀티패스 관리)
- **무엇/왜**: 지금은 Scene 코드가 패스를 손으로 엮는다(Begin/Execute/End 수동). 패스가 늘면(shadow+SSAO+bloom+TAA) 리소스 수명·배리어 관리가 폭발한다. RenderGraph는 패스를 노드, 리소스를 엣지로 선언하면 **transient 리소스 풀링·자동 배리어·dead pass culling**을 해준다.
- **어떻게**: 패스가 read/write 리소스를 선언 → DAG 위상정렬 → 리소스 aliasing(생명주기 안 겹치면 메모리 재사용) → 실행. (정직성 지도가 명시: 현재 `RenderGraph.h` 헤더조차 없음 = planned-only. 'skeleton'도 과장.)
- **Trade-off**: 추상화 복잡도 vs 멀티패스 확장성. 1인 프로젝트엔 패스가 4~5개 넘어가는 시점에 도입이 맞다.
- **검증**: 기존 수동 패스를 RenderGraph로 포팅해 **픽셀 동일성**(같은 프레임 캡처 diff)으로 회귀 확인.

### 6-4. 실시간 GI (SSR → SSGI/DDGI)
- **무엇/왜**: 간접광/반사 부재. 가장 싼 SSR(화면공간 반사)부터.
- **어떻게**: depth 버퍼 ray march(이미 SSAO에서 depth 재구성 인프라 있음) → hit 픽셀 컬러 재투영. 그다음 DDGI(프로브 기반 diffuse GI)로 확장.
- **Trade-off**: 화면공간 한계(화면 밖 정보 없음, disocclusion) vs 비용. DDGI는 프로브 업데이트 예산.
- **검증**: path tracer 대조 + SSR on/off 토글.

### 6-5. TAA (Temporal Anti-Aliasing)
- **무엇/왜**: 현재 MSAA/AA 없음 → 챔피언 에지·스킬 인디케이터 지글거림.
- **어떻게**: per-pixel jitter(Halton) + motion vector 버퍼 + history reproject + neighborhood clamp(ghosting 방지).
- **Trade-off**: ghosting/blur vs 안정성. motion vector 패스 추가 비용.
- **검증**: 정지/이동 양쪽 F5 시각 + history on/off 비교.

---

## 7. 면접 예상 질문 & 모범 답변

### Q1. (기본) Forward와 Deferred의 차이, 왜 Forward를 택했나?
Forward는 지오메트리를 그리며 그 자리에서 라이팅을 계산하고, Deferred는 G-Buffer를 먼저 깐 뒤 화면공간에서 라이팅을 한 번에 합니다. Deferred는 광원이 많을 때 유리하지만 G-Buffer 대역폭·투명·MSAA 비용이 듭니다. 제 게임은 MOBA라 동시 광원이 주광 1 + 포인트 4 수준이라 Deferred의 이득이 거의 없고 복잡도만 떠안습니다. 그래서 "범용 디폴트"가 아니라 **이 게임의 라이팅 부하에서 역산해** Forward를 택했습니다.

### Q2. (기본) SSAO가 무엇이고, 왜 2-pass인가?
SSAO는 주변광이 기하학적으로 가려지는 정도를 depth/normal 버퍼만으로 화면공간에서 근사하는 기법입니다. 제 구현은 GTAO 계열로 8방향×4스텝 수평선각을 적분합니다. 1패스만 하면 샘플 수 한계로 노이즈가 심해서, 2패스에서 bilateral blur — 공간 가중 × 깊이 가중 — 로 물체 경계를 살리며 노이즈만 제거합니다. 코드로는 `GTAO_CS.hlsl`이 raw AO를, `GTAO_Blur_CS.hlsl`의 `exp(-|Δdepth|·80)` depth weight가 엣지 보존 블러를 합니다.

### Q3. (기본) 본 팔레트를 왜 cbuffer에서 StructuredBuffer로 바꿨나?
D3D11 상수버퍼는 64KB 한계인데 float4x4 64B × 1024본이면 정확히 64KB로 꽉 찹니다. Elden Ring 보스 같은 1024본 리그가 들어오면 cbuffer로는 한 드로우에 못 올립니다. StructuredBuffer SRV는 크기 제약이 사실상 없어 1드로우 스키닝이 됩니다. 옮길 때 `row_major`를 명시해 기존 cbuffer 메모리 레이아웃을 그대로 보존했습니다 — ABI 안전을 위해서요.

### Q4. (설계) stylized와 PBR 두 경로를 다 둔 이유는?
MOBA에서는 물리적 사실성보다 **챔피언 실루엣 가독성**이 우선입니다. 그래서 디폴트는 wrap ramp·top-light·rim으로 형태를 또렷하게 만드는 stylized diffuse입니다. 다만 microfacet 이론을 이해하고 구현했다는 걸 보이려 Cook-Torrance(GGX/Smith/Schlick) 경로도 `BRDF_GGX.hlsli`에 직접 작성해 두 셰이더가 모두 빌드·바인딩됩니다. 룩 디폴트가 stylized인 것이지 PBR이 미구현은 아닙니다.

### Q5. (설계) GTAO에서 월드 포지션을 어떻게 복원하나? 정확도 문제는?
depth 값과 viewProjInv를 곱해 클립공간에서 월드공간으로 역변환합니다(`ReconstructWorldPos`). NDC y 뒤집기·w 나누기를 하고, w가 0에 가까울 때 `max(w,1e-5)`로 방어합니다. 정확도 이슈는 depth 정밀도(저는 24bit)와 멀리서 z-fighting인데, 현재 작은 MOBA 씬이라 충분합니다. 큰 씬으로 가면 view-space reconstruction이나 linear depth로 바꿀 계획입니다.

### Q6. (압박/adversarial) "PBR 엔진이라고 하셨는데, IBL도 그림자도 없잖아요? 그게 PBR인가요?"
정확한 지적이고, 그래서 저는 "PBR 엔진"이라 단정하지 않습니다. **forward, 단일 DX11, IBL·그림자·디퍼드·PostFX 없음**을 먼저 고지합니다. PBR로 제가 구현한 건 Cook-Torrance microfacet BRDF(D·F·G, 에너지 보존 kD) 셰이더 경로까지입니다. 환경광이 상수(0.03)라 PBR이 "살아나려면" IBL이 필수인데 그건 planned입니다. 다음 단계로 split-sum IBL(irradiance map + prefilter + BRDF LUT)을 path tracer를 ground-truth 삼아 검증하며 붙일 계획입니다.

### Q7. (압박/adversarial) "RenderGraph / 실시간 GI / Nanite 구현했다고 문서에 있던데요?"
그건 **로드맵 문서이고 코드는 0줄**입니다. 정직하게 말하면 `RenderGraph.h` 헤더조차 없어 'skeleton'이라고도 못 하고 planned-only입니다. 제 판단은, 지금 Winters 렌더의 실제 문제가 고급 기법 부재가 아니라 색공간·diffuse ramp·AO·UI unlit 분리·FoW 팔레트 같은 **기준 이미지 고정**이라는 것이었습니다. 백엔드·기법을 먼저 늘리면 디버깅이 3배가 되니, DX11 한 경로를 완성하는 순서를 택했습니다. RenderGraph는 패스가 4~5개를 넘는 시점(shadow+SSAO+bloom+TAA)에 도입하는 게 맞다고 봅니다.

### Q8. (압박/adversarial) "이 렌더링이 맞게 동작한다는 걸 어떻게 증명하죠? 골든 테스트 있나요?"
이 도메인은 **자동 골든/스모크 테스트가 없습니다** — 그게 솔직한 현황입니다. 대신 (1) 매 프레임 프로파일러 스코프로 각 패스 비용을 계측하고 F4로 JSON 캡처를 남깁니다. (2) SSAO/RHI를 런타임 토글해 on/off를 비교합니다. (3) `fxc`로 셰이더 컴파일을 게이트하고 Engine/Client/Server 빌드를 게이트합니다. (4) F5 시각 게이트에 실패 기준을 명문화했습니다 — 챔피언 하이라이트가 얼음처럼 뜨면 실패, UI가 월드 조명에 물들면 unlit 회귀. 픽셀 단위 자동 골든(reference image diff)은 다음에 추가할 검증입니다.

### Q9. (압박) "RHISceneRenderer가 메인 렌더러 아닌가요?"
아닙니다. `RHISceneRenderer`는 **백엔드 포터블 프로토타입**이고 라이팅이 `saturate(normal.y*0.35+0.75)`인 가짜 반구 라이팅입니다(코드 L83). DX11/DX12 parity를 검증하려는 트랙이지 production 렌더 경로가 아닙니다. 실제 게임 룩은 `Mesh3D.hlsl`/`Skinned3D.hlsl`의 stylized diffuse가 만듭니다.

### Q10. (심화) bilateral blur의 depth weight 상수(80)는 어떻게 정했나?
`exp(-|Δdepth|·80)`은 깊이 차이가 클수록 가중을 급격히 죽여 물체 경계를 보존합니다. 80은 제 depth 스케일(정규화 depth)에서 "경계는 자르고 같은 표면은 섞는" 균형으로 튜닝한 값입니다. 원리상 이건 depth 분포에 의존하므로, linear depth로 바꾸거나 씬 스케일이 변하면 재튜닝해야 합니다. 하드코딩이라 cbuffer 파라미터로 빼는 게 다음 개선입니다.

### Q11. (심화) 패스 사이 read/write 해저드는 어떻게 막나?
컴퓨트 패스가 끝나면 SRV와 UAV를 명시적으로 null 언바인드합니다(SSAOPass.cpp L260–263, L289–291). 안 그러면 다음 패스가 같은 텍스처를 SRV로 읽을 때 D3D11이 "여전히 UAV로 바인딩됨" 해저드를 경고하고 read가 막힙니다. NormalPass도 Begin에서 이전 RTV/뷰포트를 저장하고 End에서 복원해 패스를 격리합니다.

### Q12. (심화) Fog of War가 게임플레이 truth를 정하나?
아닙니다. 가시성 truth(어디가 보이나)는 ECS/서버가 정하고, FogOfWarRenderer는 그 per-cell R8 가시성 값을 업로드받아 월드 오버레이 쿼드와 미니맵 RGBA로 **시각화만** 합니다(권위 분리). 렌더러가 가시성을 결정하면 클라마다 달라져 권위 모델이 깨집니다.

---

## 8. 30초 엘리베이터 피치

"상용 RHI 없이 DX11 위에 포워드 렌더러를 직접 짰습니다. 핵심 경로는 Depth+Normal 프리패스 → GTAO 2-pass 컴퓨트 SSAO → stylized diffuse 메인 패스로, 매 프레임 프로파일러로 계측하면서 돕니다. 본 팔레트는 cbuffer 64KB 한계를 StructuredBuffer SRV로 넘겨 1024본 1드로우 스키닝을 했고, Cook-Torrance PBR 셰이더도 GGX/Smith/Schlick까지 직접 구현해 두 라이팅 경로를 가집니다. 중요한 건, IBL·그림자·RenderGraph·GI가 아직 없다는 걸 제가 로드맵에 정직하게 그어 뒀다는 겁니다 — MOBA에선 사실성보다 가독성이 먼저라 기준 이미지부터 고정하는 순서를 택했고, 다음은 CSM 그림자와 split-sum IBL을 path tracer를 ground-truth 삼아 붙이는 겁니다."
