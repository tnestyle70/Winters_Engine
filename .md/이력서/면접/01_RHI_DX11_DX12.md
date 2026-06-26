# 면접 대비 — RHI / DX11·DX12 backend

> 도메인 상태: **working** (DX11 production + DX12 scene-parity 검증 트랙)
> 근거 문서: `.md/이력서/WINTERS_DOMAIN_HONEST_MAP_2026-06-26.md` §1
> 마지막 갱신: 2026-06-26

---

## 0. 한 줄 본질 + 현재 상태

**한 줄 본질**: RHI(Render Hardware Interface)는 "렌더링 코드가 어떤 그래픽스 API(DX11/DX12/Vulkan) 위에서 도는지 모르게" 만드는 추상화 계층이다. 나는 *DX11을 추상화한 것이 아니라*, DX12/Vulkan의 명시적(explicit) 렌더링 모델(PSO·BindGroup·explicit barrier·frame-in-flight·fence)을 **기준선(baseline)**으로 인터페이스를 설계하고, DX11을 그 모델의 immediate-mode emulation 백엔드로 구현했다.

**현재 성숙도(정직하게)**:
- **production**: DX11 백엔드. 매 프레임 F5 인게임이 DX11로 돈다. `CDX11Device`가 `IRHIDevice`를 상속해 실제 게임 렌더가 RHI 인터페이스를 통과한다.
- **working**: DX12 백엔드(`CDX12Device`). descriptor heap·fence·ResourceBarrier·RootSignature/PSO가 실제 구현돼 빌드되고, 공용 `CRHISceneRenderer`가 IRHIDevice만으로 스냅샷 메시를 DrawIndexed한다. 단 **production 경로가 아니라 scene-parity 검증 트랙**이다.
- **planned-only**: Vulkan/Metal/Xbox/PS5는 `enum class eRHIBackend`에 값만 있고 구현 파일은 0개(`Engine/Private/RHI/Vulkan` 디렉토리 자체가 없음).

> **면접 첫 문장 가드레일**: "DX12로 게임을 돌린다"가 아니라 "DX11이 production, DX12는 `--rhi-scene-only` 게이트 뒤에서 씬 메시 parity를 검증하는 단계"라고 먼저 긋는다. 셰이더 컴파일은 DXC가 아니라 `D3DCompile`(FXC)을 쓴다 — cross-compile은 강조하지 않는다.

---

## 1. 핵심 개념 (본질)

### 1.1 왜 RHI라는 계층이 존재하는가 — first principles

그래픽스 API는 시대마다 추상화 수준이 다르다.
- **DX11 = immediate mode**: `Context->Draw()`를 부르면 드라이버가 알아서 상태 검증·메모리 배리어·동기화를 해 준다. 개발자는 "친절한" API를 쓴다. 대신 드라이버 내부에서 single-thread command translation 병목이 생기고, 멀티스레드 command recording이 어렵다.
- **DX12/Vulkan = explicit mode**: 드라이버의 "친절함"을 제거하고 그 책임을 앱으로 내린다. PSO를 미리 굽고, descriptor를 직접 heap에 쓰고, 리소스 상태 전이(barrier)를 손으로 넣고, GPU/CPU 동기화를 fence로 직접 한다. 대신 멀티스레드 command list 기록이 가능하고 드라이버 오버헤드가 사라진다.

게임 엔진이 두 모델을 동시에 지원하려면 렌더링 코드를 API에 직접 묶으면 안 된다. **RHI = 렌더 코드와 그래픽스 API 사이의 안정적 경계**다.

**핵심 설계 결정**: 추상화의 "기준선"을 무엇으로 잡느냐. DX11을 기준으로 잡으면(sokol/bgfx 스타일) modern feature 추가 시 막힌다. DX12/VK를 기준으로 잡으면(Unreal RHI/WebGPU/NVRHI 스타일) DX11은 명시적 호출을 "무시(no-op)"만 하면 된다 — 정보 손실이 아래 방향이라 안전하다. 나는 후자를 택했다.

### 1.2 면접에서 설명해야 하는 explicit-model 개념 5종

1. **PSO (Pipeline State Object)**: DX11은 셰이더·블렌드·래스터·뎁스 상태를 따로따로 바인딩한다. DX12/VK는 이 모든 상태를 하나의 immutable 객체로 미리 컴파일한다. 이유: 드라이버가 매 draw마다 상태 조합을 검증/재컴파일하던 비용을 생성 시점으로 옮긴다. 비용은 첫 프레임 PSO 컴파일 stutter(수백 ms).

2. **Descriptor Heap / BindGroup**: 셰이더가 보는 리소스(텍스처·cbuffer·sampler)를 GPU가 읽을 수 있는 메모리(descriptor)에 기술한다. DX11은 슬롯 모델(`PSSetShaderResources(slot, ...)`). DX12는 heap에 descriptor를 쓰고 root signature로 셰이더에 연결한다. 내 RHI는 **인터페이스는 슬롯 모델(BindGroup)을 따르되, DX12 백엔드가 내부적으로 heap에 기록**한다.

3. **Explicit Resource Barrier**: GPU 리소스는 "지금 어떤 용도인가"(render target / copy dest / shader read) 상태를 가진다. 용도가 바뀔 때 barrier로 전이를 알려야 캐시 flush·레이아웃 변환이 일어난다. DX11은 드라이버가 자동 처리, DX12는 `ResourceBarrier`를 직접 호출해야 한다(빠뜨리면 깨진 화면/해저드).

4. **Frame-in-flight (double/triple buffering)**: CPU가 N+1 프레임을 기록하는 동안 GPU는 N 프레임을 그린다. 같은 백버퍼/command allocator를 GPU가 아직 쓰는 중에 CPU가 덮어쓰면 안 된다. 그래서 프레임별로 자원을 분리(`m_iFrameIndex`)하고 fence로 "이 프레임 자원을 GPU가 다 썼는지" 기다린다.

5. **Fence 동기화**: GPU는 비동기다. CPU가 "GPU가 여기까지 실행했나"를 알려면 command queue에 fence value를 Signal하고, CPU는 그 value 도달을 기다린다(`SetEventOnCompletion`). DX11은 드라이버가 숨김, DX12는 직접.

### 1.3 핸들 기반 리소스 — DLL 경계 문제의 first principle

엔진이 DLL이고 게임이 EXE면, EXE에서 `new`하고 DLL에서 `delete`하면 **CRT 힙이 달라 깨진다**(또는 그 반대). `unique_ptr<IRHIBuffer>`를 DLL 경계로 반환하는 것도 같은 함정이다(소멸자가 어느 쪽 힙을 쓰는가 불명확). 그래서 자원은 **64bit 핸들(POD)**로 반환하고, 실제 객체는 Engine이 소유한다. 핸들은 그냥 숫자라 경계를 안전하게 넘는다. 추가로 핸들에 **generation 비트**를 넣어 use-after-free(소멸 후 같은 인덱스 재사용 시 stale 핸들로 접근)를 잡는다.

---

## 2. 왜 이 선택인가 — 기술 스택 선택 + Trade-off

### 2.1 추상화 기준선: DX12/VK 기준 vs DX11 기준

| 선택지 | 장점 | 단점 | 내 결정 이유 |
|---|---|---|---|
| **DX12/VK 모델 기준 (택)** | modern feature(bindless·multi-thread command·explicit barrier) 확장 가능, 산업 표준(Unreal RHI/WebGPU/NVRHI) | DX11 백엔드가 "안 쓰는 명시 호출"을 no-op으로 구현해야 함, 초기 설계 비용 큼 | LoL는 DX11로 충분하지만 EldenRing(1M~10M tri 오픈월드)은 DX12 multi-thread command가 필연. LoL 단계에서 추상화 안 해두면 ER 진입 시 렌더 코드 30~40곳을 헤집어야 함 |
| DX11 모델 기준 (sokol/bgfx) | DX11에 자연스럽고 코드 적음 | DX12 PSO/barrier/bindless를 표현할 슬롯이 없어 modern feature에서 봉착 | 미래(ER) 봉착이 확실해 기각 |

### 2.2 자원 핸들: 64bit handle vs unique_ptr

| 선택지 | 장점 | 단점 | 내 결정 이유 |
|---|---|---|---|
| **64bit handle (32 index + 32 gen) (택)** | DLL 경계 CRT 충돌 0, POD라 복사 안전, generation으로 use-after-free 차단, free-list 인덱스 재사용 | 간접 lookup 1회 비용, lookup 실패(stale) 처리 필요 | DLL(Engine)/EXE(Game) 경계가 실재하고, MOBA 다수 자원에서 stale 접근 방어가 가치 큼 |
| `unique_ptr<IRHIBuffer>` 반환 | 직관적, RAII | DLL 경계에서 delete 힙 불일치 위험(CRT 충돌), 가상 소멸자 다형성 비용 | 경계 안전성을 깨므로 기각 (계획 단계 Codex 검토 P0-5에서 명시 차단) |

> generation 폭도 trade-off였다. 초기 안은 24-bit index + 8-bit gen이었으나 256 cycle wrap이 너무 빨라(인덱스 256번 재사용이면 gen 충돌) **32+32로 확정**(plan 부록 P1-16). 실제 코드도 `(gen << 32) | index`로 64bit를 그대로 채운다(`RHIHandles.h:37`).

### 2.3 DX11 백엔드 유지 vs DX12 우선 cutover

| 선택지 | 장점 | 단점 | 내 결정 이유 |
|---|---|---|---|
| **DX11 production 유지 + DX12 parity 트랙 (택)** | 항상 돌아가는 게임 보존, DX12를 점진 검증, 회귀 0 | DX12가 production이 되기까진 두 경로 병존 | 신입 1인 프로젝트에서 "동작하는 LoL"을 깨지 않으면서 DX12를 안전하게 키우는 유일한 길. `--rhi-scene-only` 플래그로 격리 |
| DX12 즉시 cutover | 코드 단일화 | 검증 안 된 DX12로 production 깨질 위험, 디버깅 지옥 | 1인 리스크 과대 |

**왜 신입 1인 범위에서 합리적인가**: 인터페이스 설계(RH-1~4)는 LoL 단계에 미리 박아두고, 무거운 DX12 백엔드(RH-5)는 게임을 안 깨는 별도 게이트로 키운다. "안 깨지는 production + 검증되는 실험" 분리가 1인 개발의 핵심 리스크 관리다.

---

## 3. 실제 구현 (코드 근거)

### 3.1 핸들 자료구조 — `RHIHandles.h`

`RHIHandle<TTag>`는 `u64_t value` 하나를 가진 POD다(`Engine/Public/RHI/RHIHandles.h:6-47`):
- `Index()` = 하위 32bit, `Generation()` = 상위 32bit (`:17-24`)
- `Make(index, generation)`에서 `generation==0`이면 1로 보정 — gen 0을 "무효" sentinel로 예약 (`:31-39`)
- `IsValid()`는 `value != 0 && Generation() != 0` (`:11-14`)
- **타입 태그**로 컴파일 타임 구분: `RHIBufferTag`/`RHITextureTag` 등 8종, `using RHIBufferHandle = RHIHandle<RHIBufferTag>` (`:49-65`). 버퍼 핸들을 텍스처 슬롯에 넣으면 컴파일 에러 → 타입 안전.

### 3.2 리소스 테이블 — `CRHIResourceTable.h`

`CRHIResourceTable<TResource, TTag>` (`Engine/Public/RHI/CRHIResourceTable.h:9-136`):
- `m_Slots` (자원 포인터 + generation), `m_FreeList` (재사용 인덱스 스택) (`:133-134`)
- **Insert** (`:28-51`): free-list 있으면 인덱스 재사용 + `++generation`(0이면 1 보정), 없으면 새 슬롯 push. 핸들은 `Make(index, slot.generation)`.
- **Lookup** (`:53-68`): `slot.generation != handle.Generation()`이면 nullptr 반환 → **stale 핸들(소멸 후 재할당) 즉시 차단**. use-after-free의 핵심 방어.
- **Erase** (`:87-104`): `delete` 후 인덱스를 free-list로. 다음 Insert가 gen을 올리므로 옛 핸들은 무효화.
- **thread-safety policy**: `AssertRenderThread()` (`:125-131`)가 `_DEBUG`에서 생성 스레드 != 현재 스레드면 assert. lock-free인 이유는 "render thread only" 계약을 assert로 강제하기 때문(plan P1-17).

### 3.3 디바이스 인터페이스 — `IRHIDevice.h`

`IRHIDevice` (`Engine/Public/RHI/IRHIDevice.h:12-103`):
- `BeginFrame/EndFrame` (`:24-25`) — frame-in-flight 경계를 인터페이스에 노출(DX11은 swap, DX12는 fence wait + barrier).
- 자원 생성이 전부 **handle 반환**: `CreateBuffer(desc) -> RHIBufferHandle` (`:37`), `CreateTexture` (`:64`), `CreateShader` (`:51`), `CreateSampler` (`:81`). 대응 `Destroy*`.
- explicit 모델 자원: `CreatePipeline`/`CreateRenderPass`/`CreateBindGroupLayout`/`CreateBindGroup`/`UpdateBindGroup` (`:88-102`) — 순수 가상(=0)이라 모든 백엔드가 반드시 구현.
- `GetNativeHandle(eNativeHandleType)` (`:18`) — ImGui DX11 backend 같은 외부를 위한 명시 escape hatch(borrowed pointer).

### 3.4 커맨드 리스트 — `IRHICommandList.h`

`IRHICommandList` (`Engine/Public/RHI/IRHICommandList.h:7-38`): `Begin/End`, `BeginRenderPass/EndRenderPass`, `SetPipeline`, `SetBindGroup(slot, handle)`, `SetVertexBuffer/SetIndexBuffer`, `Draw/DrawIndexed`, `Dispatch`, `UpdateBuffer`, **`TransitionResource(handle, newState)`** (`:34-35`). DX11 백엔드에서 `TransitionResource`는 no-op이지만 **호출 자체는 통과** — DX12/VK 마이그 시 race를 미리 발견하려는 의도적 설계(plan §4.1).

### 3.5 DX12 백엔드 — `CDX12Device` (실제 explicit 코드)

`Engine/Private/RHI/DX12/DX12Device.cpp` (2162줄)는 문서가 아니라 진짜 DX12다:
- **frame-in-flight 더블버퍼**: `kFrameCount = 2`, 프레임별 `m_pCommandAllocators[kFrameCount]`, `m_pRenderTargets[kFrameCount]`, `m_uFrameFenceValues[kFrameCount]` (`DX12Device.h:105-127`).
- **BeginFrame** (`DX12Device.cpp:2057-2099`): `WaitForFrame(frameIndex)` → allocator/command list Reset → descriptor heap 바인딩 → **PRESENT→RENDER_TARGET barrier** → RTV/DSV 바인딩 + clear.
- **EndFrame** (`:2101-2126`): **RENDER_TARGET→PRESENT barrier** → `Close` → `ExecuteCommandLists` → `Signal(fence, value)` → `Present` → 다음 `frameIndex = GetCurrentBackBufferIndex()`.
- **fence wait** (`:2128-2147`): `GetCompletedValue() >= fenceValue`면 skip, 아니면 `SetEventOnCompletion` + `WaitForSingleObject(INFINITE)`.
- **PSO/RootSignature**: `CreateRootSignature` (`:1527`), `CreateGraphicsPipelineState` (`:1574`).
- **descriptor heap**: SRV/Sampler용 + staging heap (`:1815-1844`), RTV/DSV heap (`:1983-2009`).
- **버퍼 업로드**: upload heap에 memcpy → `CopyBufferRegion` → COPY_DEST→최종 상태 barrier (`:1146-1174`).

### 3.6 공용 씬 렌더러 — `CRHISceneRenderer` (백엔드 무관 draw)

`Engine/Private/Renderer/RHISceneRenderer.cpp`의 `Render()` (`:397-487`)가 **IRHIDevice/IRHICommandList만으로** DX11/DX12 양쪽에서 동일하게 돈다:
- `GetFrameCommandList()`로 command list 획득 (`:402`)
- 프레임 상수(viewProjection) `UpdateBuffer` (`:412`)
- 메시별: 정점 레이아웃에 따라 PSO 선택(`hStaticPipeline`/`hColorPipeline`, depth write 여부로 분기) (`:429-441`) → object 상수 update → BindGroup 4개 리소스(frame cbuffer/object cbuffer/albedo SRV/sampler) `UpdateBindGroup` (`:454-475`) → `SetPipeline`/`SetBindGroup`/`SetVertexBuffer`/`SetIndexBuffer`/`DrawIndexed` (`:476-485`).

**정직성 포인트**: 이 씬 렌더러의 static 셰이더는 진짜 라이팅이 아니라 가짜 반구광 `saturate(normal.y * 0.35 + 0.75)` (`:83`)다. 백엔드 포터빌리티 검증용 프로토타입이지 메인 PBR 렌더러가 아니다.

### 3.7 백엔드 선택

`CEngineApp.cpp`가 config로 디바이스를 고른다: `CreateDX11DeviceForWindow` → `CDX11Device::Create` (`:87-96`), `CreateDX12DeviceForWindow` → `CDX12Device::Create` (`:98-108`). 호출부는 `pDevice->GetBackend() == eRHIBackend::DX12`로 분기(Scene/FX/ImGuiLayer 등).

---

## 4. 검증 — 동작을 어떻게 증명했나

### 4.1 Scene-only parity 게이트 (S18)

핵심 검증은 **같은 씬을 legacy DX11 draw와 RHI 스냅샷 draw로 둘 다 그려 비교**하는 것이다(`.md/plan/rhi/sessions/S18_2026-06-24_RHI_SCENE_ONLY_PARITY_GATE.md`):
- `--rhi-scene-only` 커맨드라인 토큰 감지(`IsRHISceneOnlyMode`)
- 이 플래그가 켜지고 `m_pRHISceneRenderer->IsReady()`면, `Scene_InGameRender.cpp`에서 **map/champion/structure/jungle/minion/ambient의 legacy mesh draw를 전부 skip**하고 RHI 스냅샷 렌더만 남긴다.
- `WINTERS_PROFILE_COUNT("RHISceneOnly", ...)`로 게이트 진입 계측.

**성공 기준(문서 명시)**:
1. normal F5에서는 legacy draw 유지(회귀 0)
2. `--rhi-scene-only`에서 RHI scene renderer 준비 시 legacy draw 생략
3. RHI renderer 미준비면 legacy fallback 유지(빈 화면 방지)
4. `Client/Public`·`Shared`에 DX11/DX12 concrete 노출 0 (경계 audit)

### 4.2 검증 하니스

`Tools/Harness/Run-S17RhiValidation.ps1`이 runtime smoke를 돌리고, smoke 목록에 `WintersGame_rhi_scene_only`가 포함되는지 확인 + 타임스탬프 report(`.md/build/...HARNESS_REPORT.md`) 자동 생성. `git diff --check`로 공백/충돌 마커도 게이트.

### 4.3 정직한 한계

자동 픽셀 diff(frame diff < 1px)는 **plan의 RH-5 합격 기준**이지 아직 자동화돼 있지 않다. 현재 검증은 "토글 후 수동 시각 비교 + smoke 통과 + 경계 grep audit" 수준이다. 면접에서 "골든 이미지 테스트 있나?"엔 "parity 게이트와 smoke는 있고, 픽셀 단위 자동 diff는 RH-5 합격 기준으로 계획돼 있다"가 정직한 답.

---

## 5. 최적화

### 5.1 실제로 한 것

- **핸들 lookup의 lock-free화**: RHIResourceTable을 mutex 대신 "render thread only" 계약 + debug assert로 강제해 매 lookup의 락 비용 제거(`CRHIResourceTable.h:125-131`). MOBA는 매 프레임 다수 자원을 lookup하므로 락 제거가 유효.
- **DX12 더블버퍼 + fence**: CPU가 GPU를 INFINITE로 막연히 기다리지 않고, 이미 끝난 프레임은 `GetCompletedValue()` 비교로 즉시 통과(`DX12Device.cpp:2133`) — 불필요한 OS wait 회피.
- **버퍼 업로드 경로 분리**: GPU-only 버퍼는 upload heap memcpy + CopyBufferRegion으로 default heap에 올려 셰이더 read 대역폭 확보(`:1146-1174`).

### 5.2 계획 중(측정 예정)

- **PSO 디스크 캐시**: DX12 PSO 컴파일이 수백 ms라 첫 프레임 100개 PSO = 10초 stutter 위험. `ID3D12PipelineLibrary`로 셰이더 hash 기반 디스크 캐시 예정(plan §4.4). **정량 수치는 아직 측정 안 함 → "측정 예정".**
- **bindless descriptor**: 현재 인터페이스는 슬롯 모델. DX12 백엔드 내부에서 1M descriptor heap을 활용하는 bindless는 인터페이스 v2로 계획(plan §4.2).
- **multi-thread command recording**: DX12의 진짜 이득. EldenRing 오픈월드에서 command list를 병렬 기록하는 것이 RH-5 이후 목표.

> **정직성**: "300~650 FPS" 같은 수치는 최적화 마스터플랜의 *이론 추정치*이고 RHI 도메인엔 적용 안 됐다. RHI에서 내가 말할 수 있는 정량은 "측정 예정"뿐이다.

---

## 6. 구현 예정 (Planned) — 동일한 깊이로

### 6.1 RH-5: DX12를 production 경로로 승격

- **무엇을**: 현재 scene-parity 트랙인 DX12를 `--rhi-scene-only` 게이트 없이 전체 씬(라이팅·SSAO·FoW 포함)을 그리는 production 백엔드로 올린다.
- **왜**: EldenRing 오픈월드(1M~10M tri)는 DX11 single-thread command translation에서 명확히 병목. DX12 multi-thread command recording이 필요.
- **어떻게**:
  1. 현재 가짜 반구광 셰이더(`RHISceneRenderer.cpp:83`)를 메인 PBR 경로(Cook-Torrance + GTAO)로 교체.
  2. PSO 디스크 캐시(`ID3D12PipelineLibrary`)로 첫 프레임 stutter 제거.
  3. command list를 프레임당 1개에서 다수로 분할, JobSystem 워커에서 병렬 기록.
- **자료구조/단계**: 프레임별 command allocator는 이미 분리됨(`m_pCommandAllocators[kFrameCount]`). 병렬화는 워커별 allocator pool + bundle/secondary command list 추가가 필요.
- **예상 trade-off**: 명시적 barrier를 손으로 넣으므로 누락 시 깨진 화면/해저드. 완화책으로 DX11 백엔드에 이미 `TransitionResource` no-op 호출을 박아 둬서(§3.4) race를 DX11 단계에서 선발견.
- **검증**: plan의 RH-5 합격 기준 = "DX12 빌드 통과(compile-only)" + "LoL 시각 결과 frame diff < 1px(visual parity)"를 분리. parity 게이트(S18)를 픽셀 자동 diff로 강화.

### 6.2 RH-6: Vulkan 백엔드

- **무엇을**: `Engine/Private/RHI/Vulkan/*`를 신설(현재 0줄). VMA(메모리 할당) + DXC SPIR-V 파이프라인.
- **왜**: Steam Deck/Linux 타깃 결정 시 필요. enum 값(`eRHIBackend::Vulkan`)만 있고 구현이 없는 현 상태를 채운다.
- **어떻게**: IRHIDevice/IRHICommandList의 순수 가상을 Vulkan으로 구현. PSO=`VkPipeline`, BindGroup=`VkDescriptorSet`, barrier=`VkImageMemoryBarrier`, fence=`VkFence`. 셰이더는 DXC로 HLSL→SPIR-V, `register(bN/tN/sN)` 명시로 binding 충돌 방지(plan §4.3).
- **trade-off**: validation layer 디버깅 비용. `vkEnumerateInstanceLayerProperties`로 사전 availability 체크 후 fallback(plan P2-29).
- **검증**: validation layer 0 error/warning + LoL 시각 parity(선택).

> **면접 방어**: Vulkan은 "enum에 값만 있고 코드 0줄"이 정직한 현실. "인터페이스를 DX12/VK 기준으로 설계해뒀으니 Vulkan 추가는 mechanical translation"이라고 말하되, **compile-only와 visual parity를 분리**(plan P2-21)해서 "mechanical은 빌드까지, sync/barrier 디버깅은 별개"라고 과장을 스스로 자른다.

### 6.3 RH-2 잔여: Public DX11 헤더 완전 제거

- 이미 `Engine/Private/RHI/DX11/`로 이동 완료(현 디렉토리 구조가 증거). 남은 것은 `Engine/Public/` 전수 grep에서 `ID3D11Device|d3d11.h|RHI/DX11` 0 hit를 게이트로 자동 강제하는 것.

---

## 7. 면접 예상 질문 & 모범 답변

### Q1. RHI가 뭔가요? 왜 필요하죠? (기본)
A. 렌더링 코드를 DX11/DX12/Vulkan 같은 특정 그래픽스 API에 직접 묶지 않게 하는 추상화 계층입니다. 렌더 코드는 `IRHIDevice`/`IRHICommandList`만 보고, 어떤 백엔드인지는 모릅니다. 게임 한 본체로 여러 API를 지원하고, 미래 API 추가 시 렌더 코드 전체가 아니라 백엔드 한 개만 추가하면 됩니다.

### Q2. DX11과 DX12의 근본 차이는? (기본 개념)
A. DX11은 immediate mode로 드라이버가 상태 검증·barrier·동기화를 자동 처리해 "친절"하지만 single-thread command 병목이 있습니다. DX12는 explicit mode로 PSO·descriptor heap·resource barrier·fence를 앱이 직접 관리합니다. 대신 멀티스레드 command recording과 드라이버 오버헤드 제거가 가능합니다. LoL는 DX11로 충분하지만 EldenRing 오픈월드는 DX12의 multi-thread command가 필요합니다.

### Q3. 추상화 기준을 왜 DX12로 잡았나요? DX11이 production인데? (설계 의도)
A. 추상화는 정보 손실이 한 방향이어야 안전합니다. DX12/VK 모델(PSO·explicit barrier)을 기준으로 잡으면 DX11 백엔드는 명시 호출을 no-op으로 "무시"만 하면 됩니다. 반대로 DX11을 기준으로 잡으면(sokol/bgfx) DX12의 PSO/bindless를 표현할 슬롯이 없어 modern feature에서 봉착합니다. 산업 표준(Unreal RHI/WebGPU/NVRHI)도 전부 DX12/VK 기준입니다. production이 DX11이라는 건 "지금 도는 백엔드"일 뿐, 인터페이스 설계 기준과는 다른 축입니다.

### Q4. 왜 unique_ptr 대신 64bit 핸들을 반환하나요? (설계 심화)
A. 엔진이 DLL, 게임이 EXE라 CRT 힙이 분리돼 있습니다. DLL에서 만든 객체를 EXE에서 delete하면 힙 불일치로 깨집니다. `unique_ptr<IRHIBuffer>`를 경계로 넘기는 것도 소멸자가 어느 힙을 쓰는지 불명확합니다. 핸들은 POD 숫자라 경계를 안전하게 넘고, 객체는 Engine이 소유합니다. 추가로 상위 32bit에 generation을 넣어, 소멸 후 같은 인덱스가 재사용돼도 옛 핸들의 gen이 안 맞아 lookup이 nullptr를 반환합니다 — use-after-free를 런타임에 잡습니다.

### Q5. generation 비트는 왜 32bit나 쓰나요? (심화)
A. 초기 안은 24-bit index + 8-bit generation이었는데, 8비트면 같은 인덱스를 256번 재사용하면 gen이 wrap돼서 stale 핸들이 우연히 유효해 보이는 충돌이 납니다. MOBA처럼 자원 생성/파괴가 잦으면 256 cycle은 너무 빠릅니다. 그래서 64bit를 32 index + 32 generation으로 나눠 wrap 주기를 실질 무한대로 키웠습니다.

### Q6. (압박) DX12로 게임이 실제로 돌아갑니까? 아니면 그냥 만들어만 둔 건가요?
A. 솔직히 말하면 **production F5는 DX11입니다.** DX12는 `--rhi-scene-only` 플래그 뒤에서 스냅샷 씬 메시를 parity 검증하는 트랙입니다. 다만 "만들어만 둔" 수준은 아닙니다 — `CDX12Device`는 frame-in-flight 더블버퍼, fence 동기화, RootSignature/PSO, descriptor heap, ResourceBarrier가 실제로 구현돼 빌드되고, 공용 `CRHISceneRenderer`가 IRHIDevice만으로 DrawIndexed까지 돕니다. 즉 "DX12 백엔드는 동작하지만 아직 메인 렌더 경로(라이팅·SSAO·FoW)는 DX11"이 정확한 상태입니다. DX12를 production으로 올리는 게 RH-5 계획이고, 그때 가짜 반구광 셰이더를 메인 PBR로 교체합니다.

### Q7. (압박) Vulkan도 지원한다고 하셨는데, 코드를 보여주실 수 있나요?
A. 못 보여드립니다. **Vulkan은 `eRHIBackend` enum에 값만 있고 구현 파일은 0줄입니다.** 제가 한 건 "인터페이스를 DX12/VK 모델 기준으로 설계해서 Vulkan 백엔드를 추가할 때 렌더 코드를 안 건드려도 되게 경계를 그어둔 것"까지입니다. Vulkan 구현은 RH-6 계획이고, PSO=`VkPipeline`, barrier=`VkImageMemoryBarrier`, 셰이더는 DXC로 SPIR-V 변환하는 식으로 mechanical하게 채울 수 있습니다. 다만 sync/barrier 디버깅은 mechanical이 아니라서 compile-only와 visual parity를 합격 기준으로 분리해 뒀습니다.

### Q8. (압박) 그럼 그 RHI 씬 렌더러가 메인 렌더러인가요? 라이팅은 어떻게 하죠?
A. 아닙니다. `CRHISceneRenderer`는 백엔드 포터빌리티를 검증하는 프로토타입이고, 라이팅은 가짜 반구광 `saturate(normal.y*0.35 + 0.75)` 한 줄입니다(`RHISceneRenderer.cpp:83`). 진짜 렌더링은 DX11 forward 렌더러(Cook-Torrance PBR + GTAO SSAO)가 합니다. RHI 씬 렌더러의 목적은 "같은 스냅샷을 DX11/DX12 양쪽에서 IRHIDevice만으로 그릴 수 있나"를 증명하는 것이지 룩을 책임지지 않습니다.

### Q9. 셰이더는 DXC로 cross-compile하나요? (디테일 압박)
A. 아닙니다. 현재는 `D3DCompile`/`D3DCompileFromFile`, 즉 FXC를 씁니다(`RHIShaderCompiler.cpp:33`, `DX11Shader.cpp:99`). DXC로 SPIR-V cross-compile은 Vulkan(RH-6) 진입 시 도입할 계획이고, 그래서 셰이더에 `register(b0/t0/s0)`를 명시해 cross-compile 시 binding 충돌을 막도록 미리 작성해 뒀습니다. 지금 "DXC cross-compile을 한다"고 말하면 거짓입니다.

### Q10. DX11 백엔드인데 왜 TransitionResource 같은 barrier 호출이 인터페이스에 있나요? (설계 통찰)
A. 의도적입니다. DX11은 드라이버가 barrier를 자동 처리하니 `TransitionResource`는 no-op입니다. 하지만 렌더 코드가 DX11 단계부터 명시적으로 barrier를 호출하게 강제하면, DX12/Vulkan으로 갈 때 "여기서 상태 전이가 필요하다"는 정보가 이미 코드에 박혀 있습니다. DX11에서 no-op이라 비용 0인데, DX12 마이그 시 race를 미리 발견하는 보험입니다.

### Q11. parity는 어떻게 검증했나요? 골든 이미지 있나요? (검증 압박)
A. S18 게이트가 `--rhi-scene-only` 플래그로 legacy DX11 draw를 끄고 RHI 스냅샷만 그려서, normal F5(legacy)와 토글 후(RHI)를 비교합니다. PowerShell 하니스(`Run-S17RhiValidation.ps1`)가 runtime smoke와 경계 grep audit을 돌립니다. 다만 **픽셀 단위 자동 diff(frame diff < 1px)는 RH-5 합격 기준으로 계획돼 있고 아직 자동화 안 됐습니다.** 현재는 토글 후 수동 시각 비교 + smoke + 경계 audit 수준입니다.

### Q12. PSO 첫 프레임 stutter는 어떻게 막을 건가요? (확장)
A. DX12 PSO 컴파일은 셰이더 컴파일 포함 수백 ms라, 첫 프레임에 PSO 100개를 컴파일하면 수 초 stutter가 납니다. `ID3D12PipelineLibrary`로 셰이더 hash 기반 디스크 캐시를 만들어 두 번째 실행부터 디스크에서 로드할 계획입니다. 아직 측정값은 없어서 "측정 예정"입니다.

### Q13. Client/Public에 DX11/DX12 타입이 새면 안 되는 이유는? (아키텍처)
A. 의존성 역전입니다. concrete 백엔드(`ID3D11Device*`, `d3d11.h`)가 Public 헤더로 새면 Client가 특정 API에 묶여 RHI 추상화가 무의미해집니다. 그래서 `Engine/Public/`에서 `ID3D11Device|d3d11.h|RHI/DX11` grep 0 hit를 게이트로 강제하고, DX11/DX12 헤더는 `Engine/Private/RHI/<Backend>/`에만 둡니다. ImGui DX11 backend처럼 불가피한 외부는 `GetNativeHandle(eNativeHandleType)` escape hatch로만 명시 접근합니다.

---

## 8. 30초 엘리베이터 피치

"제 RHI는 DX11을 추상화한 게 아니라, **DX12/Vulkan의 명시적 렌더링 모델을 기준선으로 인터페이스를 설계하고 DX11을 그 emulation 백엔드로** 구현한 멀티백엔드 계층입니다. DLL 경계 CRT 충돌을 피하려고 자원을 unique_ptr가 아니라 32-index+32-generation 64bit 핸들로 반환해서 use-after-free까지 generation으로 잡습니다. DX11이 production이고, DX12 백엔드는 frame-in-flight 더블버퍼·fence·RootSignature/PSO·ResourceBarrier까지 실제로 구현해 공용 씬 렌더러가 IRHIDevice만으로 DrawIndexed하는 scene-parity 검증 단계입니다. Vulkan은 enum 값만 있고 코드는 0줄 — 그건 정직하게 'EldenRing 진입 시 채울 mechanical translation'이라고 말합니다. 제가 보여드리고 싶은 건 화려한 기능 목록이 아니라, **어디까지가 동작이고 어디부터가 계획인지를 게이트로 스스로 긋는 능력**입니다."
