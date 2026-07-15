# 03. 렌더링 파이프라인 — RHI · DX11 · FX

> 면접 대본 겸 지식 베이스. 코드 문법 설명은 `.md/interview/cpp/` 세트가 담당하고,
> 여기서는 렌더링 도메인의 구조와 의사결정만 다룬다. 모든 파일 경로는 repo-relative.

---

## ① 도메인 한 줄 정의

"Winters의 렌더링은 `IRHIDevice`라는 백엔드 추상화 위에 DX11(라이브 경로)과 DX12(parity 검증 트랙)를 올리고, 게임 쪽에서는 `RenderWorldSnapshot`이라는 순수 데이터 계약만 채우게 해서 — LoL 클라이언트와 Elden 클라이언트가 렌더러 클래스를 복제하지 않고 같은 렌더러를 공유하도록 설계한 파이프라인입니다."

이 한 문장에 이 챕터의 세 축이 다 들어 있다:
1. **RHI 추상화** — 인터페이스/핸들로 백엔드를 은닉
2. **DX11 라이브 + DX12 parity** — 점진 마이그레이션 전략
3. **데이터 계약(snapshot)** — 렌더러와 게임 로직의 단방향 분리

---

## ② 구조와 데이터 흐름

### 계층 구조

```text
Client (LoL / Elden)                          Engine
┌──────────────────────┐      ┌─────────────────────────────────────────┐
│ Scene / GameObject   │      │  Renderer 계층                           │
│  - 챔피언, 맵, 미니언  │      │   ModelRenderer (legacy DX11 immediate)  │
│  - FxCuePlayer 호출   │─────▶│   CRHISceneRenderer (snapshot 소비)      │
│                      │      │   NormalPass → SSAOPass → forward AO    │
│ RenderWorldSnapshot  │      │   FogOfWarRenderer / FX renderers       │
│  (meshes/fx/debug)   │      ├─────────────────────────────────────────┤
└──────────────────────┘      │  RHI 계층                                │
                              │   IRHIDevice (순수 가상 인터페이스)        │
                              │   RHIHandle<Tag> + CRHIResourceTable    │
                              │      ┌──────────────┬──────────────┐    │
                              │      │ CDX11Device  │ CDX12Device  │    │
                              │      │ (immediate,  │ (fence/frame,│    │
                              │      │  no-op 배리어)│  explicit)   │    │
                              │      └──────────────┴──────────────┘    │
                              └─────────────────────────────────────────┘
```

- `Engine/Public/RHI/IRHIDevice.h` — device/swapchain/buffer/texture/shader/sampler/pipeline/renderpass/bindgroup 생성을 전부 추상화. `eRHIBackend`는 DX11/DX12/Vulkan/Metal/Xbox/PS5 6종을 미리 선언(`Engine/Public/RHI/RHITypes.h`).
- `Engine/Public/Renderer/RenderWorldSnapshot.h` — view 행렬 + `RenderMeshItem`(world 행렬, RHI 버퍼 핸들, 텍스처/샘플러 핸들, tint, depthWrite) 벡터. **DX11/DX12 타입이 한 줄도 없다.**
- 컴퍼스 규칙(`.md/architecture/WINTERS_CODEBASE_COMPASS.md` RHI 방향): "LoL과 Elden은 renderer class hierarchy를 복제하지 않고 같은 RHI renderer에 서로 다른 world/render snapshot을 공급한다."

### 프레임 렌더 흐름 (DX11 라이브 기준)

```text
BeginFrame                          Draw                              EndFrame
──────────                          ────                              ────────
RT+뷰포트 재바인딩          컬링: BuildClipVisibilityMask         GPU end timestamp
(DISCARD 안정화)      →    (로컬→클립 변환 후 서브메시 마스크)   →   Present (VSync)
GPU begin timestamp        legacy draw: ModelRenderer            timing readback
RTV/DSV 클리어             snapshot draw: CRHISceneRenderer      (non-blocking)
                           NormalPass → SSAO → AO SRV 주입
                           FX (sprite/mesh, 알파/erode clip)
```

- **컬링**: `ModelRenderer::RenderFrustumCulled`가 `matWorld * matViewProj`로 로컬→클립 행렬을 만들고 `BuildClipVisibilityMask`로 서브메시 단위 가시성 마스크를 뽑아 그 마스크로만 그린다 (`Engine/Private/Renderer/ModelRenderer.cpp`). snapshot 경로도 같은 마스크 함수를 재사용한다(`AppendRenderSnapshotMeshesFrustumCulled`).
- **포스트**: `CNormalPass`가 depth+normal을 별도 RT에 기록 → `CSSAOPass::Execute(pDepthSRVNative, pNormalSRVNative, viewProj)`가 AO 계산 → 그 출력 SRV를 forward 패스에 주입하는 **forward + prepass 하이브리드**다 (`Engine/Public/Renderer/SSAOPass.h`). 패스 간 텍스처 전달은 전부 `void*`로만 한다 — public 헤더에 `ID3D11*`를 노출하지 않기 위한 의도적 type erasure.
- **GPU 타이밍**: disjoint+timestamp 쿼리를 4슬롯 링으로 두고 몇 프레임 지연된 슬롯만 `D3D11_ASYNC_GETDATA_DONOTFLUSH`로 non-blocking 회수 → `GPU::FrameUs` 카운터로 방출 (`Engine/Private/RHI/DX11/CDX11Device.cpp`의 `ReadGpuTimingResults`).

### 핸들 기반 리소스 모델

- `Engine/Public/RHI/RHIHandles.h` — `RHIHandle<TTag>`는 u64 하나에 하위 32비트 index, 상위 32비트 generation을 패킹. `IsValid()`는 value≠0 && generation≠0. `RHIBufferTag`/`RHITextureTag` 같은 태그 타입으로 버퍼 핸들과 텍스처 핸들이 **컴파일 타임에 섞이지 않는다**.
- `Engine/Public/RHI/CRHIResourceTable.h` — 핸들→리소스 매핑. Insert는 freelist 슬롯 재사용 시 generation을 ++해서 이전 핸들을 무효화하고, Lookup은 index 범위 + generation 일치 이중 검증 후에만 포인터를 돌려준다. 생성자에서 스레드 id를 저장하고 `_DEBUG`에서 모든 접근에 `AssertRenderThread()` — 락 대신 "렌더 스레드 단일 소유"라는 계약을 assert로 문서화했다.

### 텍스처/셰이더 관리

**셰이더** — 관리 전략이 경로별로 다르고, 각각 이유가 있다:
- legacy 경로: `ModelRenderer::Initialize`가 `.hlsl` 파일 경로를 받는다(기본 `Shaders/Mesh3D.hlsl`). 경로에 `PBR`이 포함되면 PBR 머티리얼 경로로 분기하는 규약 (`Engine/Private/Renderer/ModelRenderer.cpp`).
- RHI 경로: `CRHISceneRenderer`/`CRHIFxSpriteRenderer`는 셰이더를 **인라인 HLSL 문자열**로 소스에 내장하고 런타임 컴파일한다. 렌더러와 셰이더 계약(CB 레이아웃, 시맨틱)이 한 파일에서 같이 리뷰되고, 백엔드 caps에 따라 컴파일 타깃만 분기(vs_5_1 vs vs_5_0)하면 되기 때문이다. 대가는 셰이더 아티스트 편집 경로가 없다는 것 — 이건 FX 노드 그래프가 맡을 방향이다.
- 컴파일된 바이트코드는 `CreateShader(stage, bytecode, size, debugName)`로 핸들화되고, debugName이 실패 로그의 식별자가 된다.

**텍스처** — 로드는 두 층으로 나뉜다:
- RHI 층: `RHI_CreateTextureFromFile`(WIC 디코드, `Engine/Private/RHI/RHITextureLoader.cpp`)이 `RHITextureHandle`을 반환. 경로는 `WintersResolveContentPath`로 해석해 config별 복사본이 아닌 `Client/Bin/Resource` 기준을 지킨다.
- legacy 층: `CTexture::Create(device, path, eTexSamplerMode, eTexColorSpace)` — 샘플러 모드와 색공간을 로드 시점에 명시한다. 단, `ModelRenderer::LoadTexture`의 오버라이드는 공유 `CModel`에 걸리므로 "모든 인스턴스에 영향"이라는 경고 주석이 코드에 남아 있다 — 인스턴스별 오버라이드는 의도적으로 미룬 부채다.
- 폴백: snapshot 렌더러는 텍스처 핸들이 없으면 1x1 화이트 텍스처로 대체해 no-texture가 no-draw가 되지 않게 한다.

### ModelRenderer 책임 분해

`Engine/Private/Renderer/ModelRenderer.cpp`의 `Impl` 구조가 소유권을 명시한다:

| 데이터 | 소유/공유 | 이유 |
|---|---|---|
| 메시 셰이더/파이프라인 | CEngineApp 소유, 미소유 포인터 참조 | 전 인스턴스 공통 |
| `CModel` (메시/스켈레톤) | `shared_ptr`, ResourceCache 경유 공유 | 같은 챔피언 10마리가 메시를 복제하면 안 됨 |
| `CAnimator` (애니 시간/본 행렬) | `unique_ptr`, 인스턴스별 | 인스턴스마다 재생 시점이 다름 |
| cbPerFrame/cbPerObject, bone SRV | 인스턴스별 | 프레임/오브젝트마다 값이 다름 |

같은 모델이 그려지는 경로는 현재 **3개가 공존**한다:
1. `Render`/`RenderWithVisibility`/`RenderFrustumCulled` — legacy DX11 즉시모드
2. `RenderNormalPass*` — 외부에서 받은 셰이더로 depth+normal G-buffer 기록
3. `AppendRenderSnapshotMeshes` — snapshot에 메시 아이템을 push하는 백엔드 중립 경로

단, 3번에는 명확한 한계가 있다: `HasSkeleton()`이면 조기 return — **스킨드 메시는 아직 legacy 전용**이고, 정적 메시만 snapshot화됐다. 이건 숨길 게 아니라 마이그레이션 진행 상태로 그대로 말한다.

### FX 시스템 — 이중 트랙 + 큐 기반 재생

FX는 두 시스템이 병행한다:

**(A) 라이브 LoL 경로 (코드 프리셋 + 데이터 큐)**
- 챔피언 스킬 FX는 `Client/Private/GameObject/Champion/*/[Champ]FxPresets.cpp`가 `CFxCuePlayer::Play(world, "Zed.Q.Cast", ctx)`처럼 **이름 붙은 큐**로 재생한다 (`Client/Public/GameObject/FX/FxCuePlayer.h`). `PreloadDirectory`로 `.wfx` 에셋 디렉터리를 미리 로드하고 `FindCue`로 이름→핸들 조회.
- `.wfx` 로더는 외부 JSON 라이브러리 없이 자작 미니 파서(`Engine/Private/FX/FxAsset.cpp`의 `ExtractString`/`ExtractNumber`)로 파싱하고, 실패는 `FxAssetLoadResult{asset, bSucceeded, strError}`로 구조화해 반환. `CFxAssetRegistry`는 generation 슬롯 + name→handle 맵 + `ReloadFromFile`(핫리로드)를 제공한다 (`Engine/Public/FX/FxAsset.h`).
- `FxEmitterDesc`는 Billboard/Ribbon/Beam/GroundDecal/MeshParticle/ShockwaveRing 렌더타입과 Bone/Socket/Submesh/TargetSegment anchor, lifecycle 모드까지 담는 풍부한 기술서다.

**(B) 신규 데이터 주도 경로 (Niagara식 노드 그래프)**
- `CFxGraph`(노드/엣지/파라미터/커브, JSON 로드·세이브) → `CFxGraphValidator`(정적 검증) → `CFxGraphCompiler`(exec plan 컴파일) → `CFxParticlePool`(SoA 실행) (`Engine/Public/FX/Graph/FxGraph.h`).
- 검증(`Engine/Private/FX/Graph/FxGraphValidator.cpp`): 중복 노드 id, 끊긴 엣지, stage 역행(Update→Spawn 방향 엣지), 렌더 노드 0개/2개 이상을 이슈로 수집하고, Kahn 위상정렬로 `graph_cycle_detected`까지 잡는다. 에러가 있으면 topoOrder를 비워서 컴파일 자체를 막는다 — **런타임이 아니라 로드타임에 실패를 드러낸다**.
- 컴파일(`Engine/Private/FX/Exec/FxExecPlan.cpp`): topoOrder를 따라 노드 타입별로 캡처 람다(`std::function<void(CFxParticlePool&, const FxExecContext&)>`)를 Spawn/Init/Update stage 버킷에 쌓는다. Gravity 노드는 `velocity.y += g*dt; position += v*dt` 클로저가 되는 식.
- 파티클 저장(`Engine/Private/FX/Exec/FxParticlePool.cpp`): position/velocity/color/size/age/lifetime을 **각각 별도 vector로 두는 SoA**. `KillExpired`는 죽은 파티클을 마지막 alive와 swap 후 `--alive` — 순서를 포기하고 O(1) 제거.

---

## ③ 핵심 설계 결정과 트레이드오프

각 결정을 "왜 → 대안 → 선택 이유 → 감수한 비용" 순으로 정리한다.

### 결정 1. IRHIDevice: 순수 가상과 기본 구현을 섞은 인터페이스

- **왜**: DX11이 라이브로 도는 중에 DX12를 붙여야 했다. 인터페이스 전체를 순수 가상으로 만들면 백엔드 하나 추가할 때마다 전 메서드를 한 번에 구현해야 한다.
- **대안**: (a) 전부 순수 가상, (b) 전부 기본 구현, (c) 컴파일 타임 백엔드 선택(#ifdef).
- **선택**: `CreatePipeline`/`CreateRenderPass`/`CreateBindGroup*` 같은 **핵심 계약은 =0으로 강제**하고, `CreateSwapChain`/`CreateBuffer`/`CreateTexture` 등은 **빈 핸들을 반환하는 기본 구현**을 둬서 백엔드가 점진 구현할 수 있게 했다 (`Engine/Public/RHI/IRHIDevice.h`). 백엔드 concrete가 필요한 지점은 `GetNativeHandle(eNativeHandleType)`이 type-erased `void*`로만 뚫는다.
- **비용**: 미구현 기능이 컴파일 에러가 아니라 "빈 핸들"로 조용히 흘러갈 수 있다. 그래서 호출부는 `IsValid()` 검사가 관례가 됐고, 실패 시 bounded 로그를 남긴다.

### 결정 2. raw 포인터 대신 generational handle

- **왜**: 리소스를 포인터로 노출하면 (a) 백엔드 타입이 상위 계층에 새고, (b) 삭제 후 재사용된 슬롯을 옛 포인터가 가리키는 use-after-free를 못 잡는다.
- **대안**: shared_ptr(수명 공유), raw 포인터 + 규율.
- **선택**: index+generation을 u64에 패킹한 `RHIHandle<TTag>` + `CRHIResourceTable`. 슬롯 재사용 시 generation을 올리므로 stale 핸들은 Lookup에서 generation 불일치로 nullptr이 된다. 태그 타입으로 버퍼/텍스처 핸들 혼용을 컴파일 타임에 차단.
- **비용**: 매 접근마다 테이블 Lookup 한 번(간접 참조). 그리고 테이블이 렌더 스레드 단일 소유라는 계약이 필요해졌다 — 락 대신 `_DEBUG` assert로 계약을 강제했다.

### 결정 3. DX11 immediate context를 IRHICommandList로 "no-op 흡수"

- **왜**: DX12는 커맨드리스트/배리어/fence가 필수지만 DX11 즉시모드에는 그런 개념이 없다. 인터페이스를 DX11 기준으로 깎으면 DX12를 못 담고, DX12 기준으로 깎으면 DX11이 억지가 된다.
- **선택**: 인터페이스는 DX12 수준(BeginRenderPass/TransitionResource 포함)으로 잡고, DX11 구현(`CDX11FrameCommandList`, `Engine/Private/RHI/DX11/CDX11Device.cpp`)에서는 `Begin/End/BeginRenderPass/EndRenderPass`를 **전부 no-op**으로 흡수했다. `SetPipeline/SetBindGroup/Draw`만 실제 컨텍스트 호출로 매핑되고, `SetBindGroup`은 layout의 visibility 비트를 읽어 `VSSet*`/`PSSet*`로 분기 바인딩한다.
- **대조**: 같은 인터페이스 뒤에서 `CDX12Device`는 kFrameCount=2 프레임별 allocator/fence, `BeginFrame`의 PRESENT→RENDER_TARGET 배리어, `EndFrame`의 Close→Execute→Signal→Present로 완전 명시적으로 구현된다 (`Engine/Private/RHI/DX12/DX12Device.cpp`). 디스크립터는 "영속 bindgroup = shader-visible 힙의 연속 range를 free-range allocator로 소유, 프레임 임시 = 프레임 인덱스별 ring 영역" 구조다.
- **비용**: DX11에서는 의미 없는 호출(배리어 등)이 인터페이스에 존재한다. 추상화가 "최소 공통분모"가 아니라 "최대 요구사항" 쪽으로 기울었고, 이는 DX12/Vulkan으로 갈수록 이득이 커지는 선불 비용이다.

### 결정 4. 스왑체인: FLIP_DISCARD 목표 → DISCARD로 의도적 후퇴

- **왜**: 파일 상단 주석은 FLIP_DISCARD를 목표로 명시했지만, FLIP 모델은 백버퍼 순환/RTV 재바인딩 정책을 엄격히 지켜야 해서 초기 안정화 단계 리스크가 컸다.
- **선택**: `DXGI_SWAP_EFFECT_DISCARD + BufferCount=1`로 후퇴하고, 그 대가로 `BeginFrame`마다 `OMSetRenderTargets + RSSetViewports`를 재설정한다 (`Engine/Private/RHI/DX11/CDX11Device.cpp`의 `CreateDeviceAndSwapChain`/`BeginFrame`). "초기 안정화 단계에서는 전통적인 DISCARD 스왑체인을 사용한다"는 주석으로 이상과 현실의 괴리를 코드에 문서화했다.
- **비용**: FLIP 모델의 present 효율을 포기했고, 매 프레임 RT 재바인딩이라는 소액의 고정비를 낸다. 면접에서 "왜 FLIP 안 썼나"가 나오면 이 트레이드오프를 먼저 말한다 — 성능 이상보다 검은 화면 없는 안정성이 그 시점의 우선순위였다.

### 결정 5. RenderWorldSnapshot — 렌더러 복제 대신 데이터 계약

- **왜**: LoL 클라이언트와 Elden 클라이언트를 같은 엔진에 올려야 한다. 제품마다 렌더러 클래스 계층을 복제하면 두 배로 썩는다.
- **선택**: 렌더러는 하나(`CRHISceneRenderer`)로 두고, 제품별 차이는 **snapshot 데이터를 채우는 쪽**에서 흡수한다. `CRHISceneRenderer`는 셰이더 타깃만 백엔드에 따라 분기(DX12면 vs_5_1/ps_5_1, 아니면 5_0)하고, 정점 레이아웃 2종 × depthWrite on/off = 4개 파이프라인을 미리 만들고, 메시 수만큼 per-mesh CB+bindgroup 슬롯을 lazy 확장(`EnsureDrawSlots`)하며 그린다 (`Engine/Private/Renderer/RHISceneRenderer.cpp`). 텍스처가 없으면 1x1 화이트 폴백.
- **비용**: 매 프레임 snapshot 벡터를 채우는 복사 비용, 그리고 skinned 경로처럼 snapshot 계약에 아직 없는 기능은 legacy 경로에 남는 이원화.

### 결정 6. FX 신규 트랙은 DX12 전용 가드로 격리

- **왜**: 라이브 LoL 챔피언 FX는 매일 보는 기능이라 회귀 리스크가 가장 크다. 새 RHI 기반 FX 렌더러를 라이브에 바로 꽂으면 검증 전 리스크를 라이브가 다 뒤집어쓴다.
- **선택**: `CRHIFxSpriteRenderer::Initialize` 첫 줄에서 `GetBackend() != eRHIBackend::DX12`면 즉시 실패 반환 (`Engine/Private/Renderer/RHIFxSpriteRenderer.cpp`). 즉 신규 RHI FX는 DX12 parity 트랙에서만 켜지고, DX11 라이브 FX는 기존 경로가 계속 담당한다.
- **비용**: 같은 룩을 내는 코드가 한동안 두 벌 존재한다. 대신 데이터 쪽은 `FxEmitterSetMaterialFromLegacyFields`/`FxEmitterApplyMaterialToLegacyFields`(`Engine/Public/FX/FxAsset.h`)라는 **명명된 양방향 sync 헬퍼 2개**로 flat 필드와 신규 material 구조를 공존시켜, 신구 저작 데이터가 어느 쪽 경로로도 소비되게 했다. 필드 동기화가 산재하지 않고 중앙화된 게 포인트다.

### 결정 7. CreatePipeline의 legacy 브리지 — 핸들 유무로 data-only vs native PSO

- **왜**: RHI 이관 중에 "desc만 보관하던" 기존 호출자와 "셰이더 핸들까지 넘기는" 신규 호출자가 같은 API를 쓴다.
- **선택**: vs/ps 핸들이 유효하면 InputLayout/Rasterizer/DepthStencil/Blend까지 native 생성, 없으면 desc만 보관하는 data-only 파이프라인으로 남긴다. native 초기화가 실패하면 delete + 빈 핸들로 생성 자체를 실패 처리 (`Engine/Private/RHI/DX11/CDX11Device.cpp`의 `CreatePipeline`).
- **비용**: 하나의 API에 두 계약이 공존해 "이 핸들이 그릴 수 있는 파이프라인인가"를 런타임(`IsNativeReady`)에 물어야 한다. 마이그레이션 완료 후 제거할 부채로 명시해 둔다.

---

## ④ 어려웠던 점과 해결

### 1) 다양한 PC에서의 디바이스 초기화 실패 — 3단 폴백 체인

노트북 iGPU/dGPU 스위칭 환경과 Graphics Tools(디버그 레이어) 미설치 PC에서 초기화가 죽거나 iGPU로 도는 문제를 겪었다. 해결은 폴백 체인 + 각 단계 로깅:

1. `IDXGIFactory6::EnumAdapterByGpuPreference(HIGH_PERFORMANCE)`로 외장 GPU 우선, 실패 시 `EnumAdapters1` 첫 하드웨어 어댑터, SOFTWARE 어댑터는 배제
2. 디바이스 생성 실패 + DEBUG 플래그였으면 → 플래그 제거 후 재시도 ("Debug layer unavailable" 로그)
3. 그래도 실패면 → OS 기본 하드웨어 어댑터로 재시도

FeatureLevel도 11.1 우선 / 11.0 폴백. 어댑터 이름과 VRAM, 선택 사유를 `OutputDebugString`으로 남겨서 "어느 GPU로 떴는지"를 즉시 확인할 수 있게 했다 (`Engine/Private/RHI/DX11/CDX11Device.cpp`의 `SelectHighPerformanceDX11Adapter`/`CreateDeviceAndSwapChain`).

### 2) 본 개수 한계 — cbuffer 팔레트 → structured buffer SRV

cbuffer 기반 본 팔레트는 64KB 한계 때문에 256~512본에서 막힌다. 대형 캐릭터 rig가 이 한계에 걸려서, `D3D11_RESOURCE_MISC_BUFFER_STRUCTURED` + DYNAMIC 버퍼로 `kMaxGPUBones=1024`개 행렬을 담아 **t8 슬롯 SRV**로 바인딩하는 구조로 교체했다 (`Engine/Private/Renderer/ModelRenderer.cpp`의 `BoneMatrixSRVBuffer`). 주석에 "옛 256/512 cbuffer palette를 대체"라고 이력을 남겼다. 업데이트는 `Map(WRITE_DISCARD) + memcpy(min(size, 1024))` — 큰 캐릭터도 한 드로우로 스키닝된다.

### 3) "이펙트가 안 보임" — UV·알파 미스매치와 clip 파이프라인

LoL 원본 추출 에셋의 `render/*.png`는 메시 diffuse가 아니라 스프라이트 캡처였고, 메시 UV가 그 텍스처의 **알파 0 영역**을 가리키고 있었다. FX 셰이더는 erode clip과 alpha clip을 이중으로 걸기 때문에(`Engine/Private/Renderer/RHIFxSpriteRenderer.cpp`의 `PSMain`: erode → alpha clip 순) 전 픽셀이 조용히 버려져 "호출은 됐는데 아무것도 안 보이는" 증상이 났다.

교훈이 두 개였다:
- **CPU 디버거로 못 잡는 패턴은 RenderDoc부터** — 셰이더에서 clip으로 죽는 픽셀은 브레이크포인트가 안 잡는다. UV/alpha bbox를 데이터로 계측하고 GPU 캡처로 확인하는 게 정답이었다.
- 진짜 머티리얼 텍스처(`*_texture.png`/`*_mult.png`)를 쓰고, alpha/erode clip은 켠 채로 유지했다. clip 자체가 문제가 아니라 입력 데이터가 문제였기 때문이다.

이후 FX 룩 자체는 `CBFxParams.vStyleParams.x`(styleMode) 분기 하나로 통합했다 — brush contrast/rim/cell shading(`ApplyFxStyle`)과 노이즈 다중 샘플 + UV distortion + age 기반 dissolve의 마법진 소멸(`ApplyMagicSurface`)이 한 셰이더에 산다. 상수 패킹은 `MakeFxParamsFromMaterial`(`Engine/Public/Renderer/FxShaderConstants.h`)로 중앙화.

### 4) 챔피언 facing이 반대로 도는 버그 — 렌더 스레드 계측 훅

챔피언 body yaw가 ±PI 뒤집히는 버그(서버 권위 yaw vs 로컬 예측 yaw 충돌)를 잡을 때, 증상 튜닝 대신 **렌더 스레드에서 expected vs actual을 계측**하는 훅을 심었다. `ModelRenderer::SetYawTraceContext(snapshotTick, entity, commandSeq, expectedYaw, expectedForward)`로 컨텍스트를 주입하면, `UpdateTransform` 시점에 world 행렬에서 실제 forward/right/back 벡터와 yaw를 추출해 expected와의 dot/delta를 계산하고, `IsYawTraceHalfTurn`(tolerance 0.35)으로 반바퀴 뒤집힘을 정량 감지한다. 방출은 512회 상한 카운터로 바운드했다 (`Engine/Private/Renderer/ModelRenderer.cpp`). 버그 확정 후 로그 방출부는 정리했고 현재 코드에는 계측 골격(컨텍스트 주입 + 비교 계산)이 남아 있다 — "routine 로그는 남기지 않는다"는 팀 정책과의 절충이다.

### 5) 리소스 생성/로드 실패의 진단 정책

한때 "포맷만 해놓고 방출하지 않는 sprintf 진단"(죽은 진단)이 있었고, 이는 없는 로그보다 나쁘다는 결론을 냈다. 지금은:
- `Engine/Private/RHI/RHITextureLoader.cpp` — WIC 디코드의 **각 단계**(CoCreateInstance/CreateDecoder/GetFrame/GetSize/FormatConverter)마다 `LogTextureLoadFailure(stage, path, hr)`로 실패 지점을 특정해 방출하고 빈 핸들 반환. COM 수명은 `CScopedCOMInit` RAII.
- `CDX11Device::CreateBuffer/CreateTexture` 실패도 hr/size/debugName 포함 bounded 로그 + 즉시 정리 + 빈 핸들.
- 상수버퍼는 3중 방어: `DX11ConstantBuffer<T>`의 `static_assert(sizeof(T) % 16 == 0)`(컴파일 타임), `CBPerFrame` 등의 명시적 `_padding` 필드, 디바이스단 `(ByteWidth+15) & ~15` 올림(런타임) (`Engine/Private/RHI/DX11/DX11ConstantBuffer.h`, `Engine/Public/Renderer/FxShaderConstants.h`). HLSL cbuffer 정렬 위반은 에러가 아니라 "조용히 깨진 데이터"로 나타나기 때문에 컴파일 타임 가드로 승격했다.

### 6) Z-fighting (맵 렌더링 초기)

소환사의 협곡 맵을 처음 올릴 때 겹친 지오메트리에서 Z-fighting을 겪었다. 당시는 겹친 메시 정리와 depth 설정 조정으로 해결했다(초기 맵 파이프라인 단계의 일반적 대응이었고, 현재 코드에 전용 depth-bias 장치를 남기지는 않았다). 이후 유사 계열 사고 — Destroyed 상태 넥서스의 중첩 메시가 텍스처 진단을 오염시킨 건 — 을 겪으며 "겹친 지오메트리는 렌더 옵션 튜닝보다 데이터(메시 구성) 정리가 먼저"라는 순서를 갖게 됐다.

---

## ⑤ 향후 개선 방향

1. **스킨드 메시의 snapshot 경로** — 현재 `AppendRenderSnapshotMeshes`는 스켈레톤이 있으면 조기 return. RHI 계약에 본 팔레트(structured buffer 상당)를 명시해 스킨드까지 백엔드 중립화하는 것이 다음 큰 조각이다.
2. **FLIP_DISCARD 복귀** — DISCARD 후퇴는 안정화용이었다. RTV 재바인딩 정책이 프레임 루프에 이미 있으므로, BufferCount≥2 + FLIP 전환을 검증 트랙에서 먼저 태운다.
3. **FX legacy 프리셋 → 노드 그래프 데이터로 이관** — validator/compiler/pool 골격은 완성됐다. 챔피언 프리셋 코드(수십 개 cpp)를 `.wfx`/그래프 데이터로 옮기는 건 라이브 리스크 때문에 챔피언 단위로 점진 진행한다.
4. **ModelRenderer 책임 축소** — 헤더에 "이거도 GameInstance에 안 넣고 그냥 공개?"라는 미결 주석이 남아 있을 만큼(`Engine/Public/Renderer/ModelRenderer.h`) public API가 방대하다. snapshot 경로가 성숙하면 draw 제출 책임을 snapshot 쪽으로 이관하고 ModelRenderer는 리소스/애니 상태 보유자로 줄인다.
5. **legacy data-only 파이프라인 제거** — `CreatePipeline` 브리지의 두 계약을 native PSO 하나로 수렴.

---

## ⑥ 면접 Q&A

**Q1. 렌더 백엔드 추상화를 어떻게 설계했나?**
- 골격: `IRHIDevice` 하나에 리소스 생성 전부를 추상화하되, 핵심 계약(pipeline/renderpass/bindgroup)은 순수 가상으로 강제하고 나머지는 빈 핸들 기본 구현으로 점진 마이그레이션 가능하게 했다. 백엔드 concrete는 `GetNativeHandle`의 `void*`로만 노출.
- 꼬리질문 대비: "왜 전부 순수 가상으로 안 했나?" → DX11 라이브를 유지하며 DX12를 붙이는 중이라, 백엔드가 미구현 기능을 가진 채로도 링크/실행이 되어야 했다. 조용한 실패 리스크는 핸들 `IsValid()` 관례 + 실패 로그로 상쇄.

**Q2. 왜 raw 포인터가 아니라 핸들인가?**
- 골격: 세 가지 — (1) 백엔드 은닉: 포인터를 노출하면 `ID3D11Buffer*`가 상위 계층에 샌다. (2) generational 안전성: 삭제 후 재사용 슬롯을 옛 핸들이 접근하면 generation 불일치로 nullptr. (3) 강타입 태그: 버퍼 핸들을 텍스처 자리에 넣으면 컴파일 에러.
- 꼬리질문: "핸들이 가리키던 리소스가 삭제되면?" → Lookup이 index 범위 + generation 이중 검증으로 nullptr 반환, 크래시가 아니라 no-draw. "스레드 안전은?" → 테이블은 렌더 스레드 단일 소유 계약이고, 락 대신 `_DEBUG` assert로 계약 위반을 즉시 드러낸다. 락을 안 쓴 이유는 현재 접근 패턴이 단일 스레드라 락이 비용만 내고 계약을 흐리기 때문.

**Q3. DX11 즉시모드를 커맨드리스트 추상화에 어떻게 맞췄나?**
- 골격: 인터페이스는 DX12 수준으로 잡고 DX11 쪽에서 배리어/렌더패스를 no-op으로 흡수했다. `SetBindGroup`만 layout visibility 비트를 읽어 VS/PS 슬롯에 분기 바인딩. 같은 인터페이스의 DX12 구현은 프레임별 allocator+fence, explicit 배리어, staging→shader-visible 디스크립터 copy로 완전히 다르다.
- 꼬리질문: "DX12 디스크립터 힙은 어떻게 관리하나?" → 영속 bindgroup은 shader-visible 힙의 연속 range를 free-range allocator로 소유, 프레임 임시는 프레임 인덱스별 ring 영역에서 bump 할당 후 `ResetFrame`으로 리셋. Vulkan descriptor set 개념을 DX12 위에 직접 구현한 것.

**Q4. 같은 엔진으로 LoL과 Elden 두 클라이언트를 어떻게 올리나?**
- 골격: 렌더러 클래스 계층을 복제하지 않는다. `RenderWorldSnapshot`이라는 순수 데이터 계약(view + mesh/fx/debug 아이템, RHI 핸들만 포함)을 제품별로 다르게 채우고, `CRHISceneRenderer` 하나가 소비한다. 이 규칙은 아키텍처 컴퍼스 문서에 금지 조항("Elden renderer를 새로 복제하지 않는다")으로 박아 협업 규칙화했다.
- 꼬리질문: "snapshot 복사 비용은?" → 아이템은 행렬+핸들 수준의 POD라 프레임당 벡터 채우기 비용이고, per-mesh CB 슬롯은 lazy 확장 풀로 재사용한다. 문제가 되면 그때 계측으로 증명하고 최적화한다(성능 경계 원칙).

**Q5. GPU 프레임 타임은 어떻게 재나? GetData 블로킹 문제는?**
- 골격: disjoint+begin/end timestamp 쿼리를 4슬롯 링으로 두고, 매 프레임 현재 슬롯에 기록 후 write index를 전진. 회수는 `DONOTFLUSH` 플래그로 몇 프레임 지연된 pending 슬롯만 non-blocking 시도. disjoint이거나 frequency 0, 타임스탬프 역전이면 그 프레임은 폐기. 쿼리 생성이 실패해도 렌더링은 계속되게 ready 플래그로 게이트.
- 꼬리질문: "왜 4슬롯인가?" → GPU가 CPU보다 2~3프레임 뒤에 있을 수 있어 in-flight 쿼리를 겹치지 않게 담을 여유분이다.

**Q6. 본이 많은 캐릭터 스키닝은?**
- 골격: cbuffer 팔레트는 64KB 한계로 256~512본에서 막힌다. structured buffer SRV(t8)로 1024본을 담아 한 드로우로 스키닝한다. 실제로 대형 rig가 한계에 걸려 교체한 이력이 코드 주석에 있다.
- 꼬리질문: "다른 방법은?" → 텍스처에 본 행렬을 굽는 방식, compute pre-skinning 등이 있지만 DX11에서 structured buffer가 구현 대비 효과가 가장 컸다.

**Q7. 파티클 시스템 설계를 설명해달라.**
- 골격: 두 트랙. 라이브는 이름 붙은 큐(`CFxCuePlayer::Play(world, "Zed.Q.Cast", ctx)`) + 챔피언 프리셋 코드. 신규는 노드 그래프 → 정적 검증(중복 id/사이클/stage 역행/렌더노드 유일성, Kahn 위상정렬) → 노드별 클로저를 stage 버킷에 쌓는 exec plan 컴파일 → SoA 파티클 풀 실행. 죽은 파티클은 swap-and-pop으로 O(1) 제거하고 순서는 포기한다.
- 꼬리질문: "왜 SoA인가?" → update 스텝(Gravity/Drag/Age)이 alive 범위를 선형 순회하며 특정 속성 배열만 만지므로 캐시 라인에 필요한 데이터만 태운다. "왜 바로 그래프로 전환 안 하나?" → 라이브 챔피언 FX 회귀 리스크. 신규 RHI FX 렌더러도 DX12 백엔드 가드로 격리해 parity를 먼저 검증한다.

**Q8. 이펙트가 화면에 안 나오는 버그는 어떻게 디버깅하나?**
- 골격: war story로 답한다 — 스프라이트 캡처용 png를 diffuse로 썼다가 UV가 알파 0 영역을 가리켜 clip이 전 픽셀을 버린 사고. CPU 추론을 누적하는 대신 RenderDoc GPU 캡처와 UV/alpha bbox 데이터 계측으로 원인을 확정했다. 이후 "안 보임" 계열 증상은 셰이더 Read + 데이터 계측을 먼저 하는 디버깅 파이프라인을 팀 규칙으로 못박았다.
- 꼬리질문: "clip을 빼면 되지 않나?" → clip은 erode/dissolve 룩의 핵심이라 유지하고, 입력 데이터(진짜 머티리얼 텍스처)를 고쳤다. 증상이 아니라 원인을 고친다.

**Q9. 리소스 로드 실패는 어떻게 다루나?**
- 골격: 단계별 특정 — WIC 로더는 디코드 각 단계마다 stage/path/hr을 포함한 bounded 로그 후 빈 핸들 반환. 디바이스 리소스 생성 실패도 debugName/hr 포함 로그 + 즉시 정리. 원칙은 "방출되지 않는 진단(죽은 진단)은 없는 로그보다 나쁘다 — 실패 경로는 반드시 방출하거나 삭제".
- 꼬리질문: "예외는 왜 안 쓰나?" → 렌더 경계는 프레임마다 도는 코드라 실패를 값(빈 핸들 + result 구조체)으로 돌려주고 호출부가 폴백(1x1 화이트 텍스처 등)을 선택하게 하는 편이 흐름 제어가 명확하다.

**Q10. 상수버퍼 정렬 버그를 어떻게 예방했나?**
- 골격: 다층 방어 — `static_assert(sizeof(T) % 16 == 0)`로 컴파일 타임 강제, CB 구조체에 명시적 `_padding` 필드, 디바이스 `CreateBuffer`에서 constant 용도면 16바이트 올림. HLSL cbuffer 정렬 위반은 에러 없이 셰이더 데이터만 조용히 깨지는 부류라서, 런타임 디버깅 비용을 컴파일 타임 가드로 선지불했다.
- 꼬리질문: "패딩을 자동화할 수는 없나?" → alignas나 리플렉션 기반 검증 도구도 가능하지만, C++ 구조체와 HLSL cbuffer의 레이아웃 규칙이 다르다는 사실 자체를 코드에 드러내는 명시적 `_padding` 필드가 리뷰에서 더 안전했다.

---

## ⑦ 다른 챕터와의 연결

- **cpp/06_polymorphism_virtual** — `IRHIDevice`의 순수 가상 vs 기본 구현 분리, `dynamic_cast` 다운캐스트(CDX11PipelineState)의 비용 논의는 이 챕터의 결정 1·3의 문법적 기반.
- **cpp/07_templates_generic** — `RHIHandle<TTag>` 강타입 태그, `CRHIResourceTable<TResource, TTag>`, `DX11ConstantBuffer<T>`의 static_assert가 전부 템플릿 장치다.
- **cpp/03_memory_lifetime_raii** — ComPtr/`CScopedCOMInit`/move-only 상수버퍼 래퍼의 소유권 설계. generational handle은 "포인터 수명 문제를 값 검증으로 바꾼" 사례로 연결.
- **cpp/08_stl_containers_cache** — 파티클 풀의 SoA + swap-and-pop은 캐시 지역성 논의의 실물 예제.
- **cpp/10_error_handling** — "죽은 진단 금지, 실패 경로는 bounded 방출" 정책(`.md/architecture/WINTERS_ERROR_HANDLING_POLICY.md`)이 렌더 리소스 경계에서 어떻게 실천되는지가 이 챕터 ④-5.
- **cpp/11_architecture_ecs** — 렌더 스냅샷을 소비하는 쪽과 만드는 쪽의 단방향 데이터 흐름은 ECS의 시스템/컴포넌트 분리와 같은 사고방식이다.
- **엔진 도메인 다른 챕터** — 서버 권위 이동/yaw 챕터(스냅샷 적용과 로컬 예측)와 ④-4의 yaw 계측 훅이 맞물리고, 에셋 파이프라인 챕터(`.wmesh`/`.wfx` 포맷)와 FX 로더/레지스트리가 맞물린다.
