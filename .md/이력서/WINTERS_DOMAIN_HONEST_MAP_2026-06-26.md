# Winters Engine 도메인 정직성 지도 (이력서/면접 근거)

작성일: 2026-06-26
목적: 이력서·포트폴리오·면접에서 **"실제 구현"과 "스켈레톤/계획"을 코드 근거로 구분**한다. 과장은 면접 코드리뷰에서 즉사하고, 정직한 경계는 메타인지 강점으로 읽힌다. 17개 도메인을 코드 레벨로 감사한 결과다.

## 상태 등급

- **production**: 매 프레임/매치마다 실제로 돌고 검증(게이트/프로파일러)까지 있음
- **working**: 핵심 경로 구현·동작, 완성도/엣지케이스 부족
- **prototype**: 일부 경로만, 실험적/격리됨
- **skeleton**: 헤더/클래스 골격만, 본문 거의 없음
- **planned-only**: 문서에만 존재, 코드 0줄

## 한눈에 보는 도메인 성숙도

| # | 도메인 | 전체 상태 | 이력서 한 줄 본질 |
|---|---|---|---|
| 1 | RHI / DX11·DX12 backend | working | DX12/Vulkan 명시적 모델 기준 핸들 추상화. DX11 production + DX12 scene-parity |
| 2 | Renderer / Graphics | working | DX11 포워드 + GTAO SSAO + Cook-Torrance PBR + stylized diffuse + FoW |
| 3 | ECS / 데이터주도 정의 | working | sparse-set ECS + access-conflict 병렬 스케줄러 + JSON→codegen 불변 Def pack |
| 4 | JobSystem / Fiber / Profiler | working | Chase-Lev work-stealing JobSystem + CPU/GPU 프로파일러 (Fiber는 PoC) |
| 5 | 네트워크 (IOCP/스냅샷) | working | IOCP TCP 권위 서버 + FlatBuffers 스냅샷 복제 (UDP는 헤더만) |
| 6 | 서버권위 시뮬레이션 | working | 30Hz 결정론 ECS 권위 루프, Input→Command→Snapshot→Visual (승패종료 미완) |
| 7 | 챔피언/스킬 (GAS 등가) | working | 함수포인터 훅 레지스트리 + 15챔피언 QWER + 데이터주도 스킬 파라미터 |
| 8 | AI (HFSM/Utility/봇) | working | command-only 결정론 라인전 봇 (BT/MCTS/RL은 비권위 PoC/dead) |
| 9 | FX / Effect Tool | working | WFX 데이터주도 큐 런타임 + 서버 cue 1회 재생 (노드그래프/VM은 PoC) |
| 10 | 에셋 파이프라인 / Winters 포맷 | working | .wmesh/.wskel/.wanim/.wmat end-to-end 쿡→로드→재생 |
| 11 | EldenRing 클라/에디터/추출 | **prototype** | 원작 바이너리→Winters 바이너리 복원 파이프라인 + 쇼케이스/에디터 (게임플레이 0) |
| 12 | 성능 최적화 / 측정 인프라 | working | 프로파일러+GPU타임스탬프+JSON캡처 production, 최적화 Phase1~7은 계획 |
| 13 | UI 파이프라인 | working | DX11 배치 스프라이트 렌더러 + Atlas 매니페스트 + 데이터주도 HUD (텍스트는 ImGui) |
| 14 | 백엔드 (Go) | working | 6개 Go 마이크로서비스 + WinHTTP로 C++ 클라 실연동 (테스트/실결제 없음) |
| 15 | 보안 / 안티치트 | working | 서버권위 입력검증 1차방어 (유저모드/커널은 문서뿐) |
| 16 | 협업 / 툴링 / 빌드 | working | SimLab 결정론 골든 + PowerShell 검증 하니스 + CMake/Ninja (Perforce 없음) |
| 17 | 애니메이션 / 물리 / 오디오 | working(약점) | 스켈레탈 애니 재생 + SAT 충돌수학 라이브러리 + 2D FMOD 래퍼 |

> **이력서 본질 한 줄(맨 위)**: "엔진을 *쓰는 사람*이 아니라 *의심하고 다시 만든 사람* — 단일 스레드 학습용 엔진의 한계를 프로파일러로 증명하고, RHI·ECS·잡시스템·서버권위 네트워크를 직접 설계한 C++20 멀티스레드 자체 게임 엔진 Winters로 LoL·EldenRing을 구현."

---

## 도메인별: 쓸 수 있는 문장 / 쓰면 안 되는 표현

각 도메인은 (a) 정직하게 어필 가능한 강점, (b) **레드플래그(과장 시 즉사)**, (c) 키워드로 정리한다.

### 1. RHI / DX11·DX12 backend — working

**쓸 수 있는 문장**
- DX12/Vulkan의 명시적 모델(PSO·BindGroup·explicit barrier·frame-in-flight)을 기준으로 `IRHIDevice`/`IRHICommandList` 추상화를 설계, DX11을 immediate-mode emulation 백엔드로 구현해 단일 DLL이 런타임에 백엔드를 선택하는 멀티백엔드 RHI 구축.
- DLL 경계 CRT 충돌을 피하려 자원 생성을 unique_ptr 대신 64bit handle(32-index+32-generation) 반환으로 설계, free-list 재사용·generation mismatch use-after-free 차단·render-thread 단독 접근 assert를 갖춘 `CRHIResourceTable` 구현.
- DX12 백엔드에 descriptor heap·fence·ResourceBarrier·RootSignature/PSO를 구현, 공용 `CRHISceneRenderer`가 IRHIDevice만으로 스냅샷 메시를 PSO 바인딩 후 DrawIndexed.
- DX11 production을 유지하면서 `--rhi-scene-only` 플래그로 legacy draw를 끄고 RHI 스냅샷만 렌더해 비교하는 parity 검증 게이트(S18) 설계.

**레드플래그**
- ❌ "DX12로 게임을 돌린다" → DX12는 `--rhi-scene-only` 게이트 뒤 scene-parity 검증 트랙, production F5는 DX11. "DX12는 씬 메시 parity 검증 단계"로만.
- ❌ "멀티플랫폼 RHI / Vulkan·Metal·Console 지원" → enum 값(`eRHIBackend`)만 있고 구현 0줄. "DX11+DX12 구현, Vulkan/Console은 경계 설계만".
- ❌ "DXC/SPIR-V 크로스컴파일" → 실제는 `D3DCompile`(FXC). cross-compile 강조 금지.

**키워드**: RHI abstraction, D3D11, D3D12, handle-based resources, generation handle, PSO, RootSignature, DescriptorHeap, ResourceBarrier, BindGroup, frame-in-flight, fence sync, scene-parity gate, DLL boundary

### 2. Renderer / Graphics — working

**쓸 수 있는 문장**
- 상용 RHI 없이 C++20/HLSL DX11 포워드 렌더러를 직접 구현: Depth+Normal 프리패스 → GTAO 2-pass 컴퓨트 SSAO → 화면공간 AO를 메인 패스에 주입, 매 프레임 프로파일러 스코프와 함께 가동.
- Cook-Torrance PBR(GGX/Smith/Schlick, dir+4 point, Reinhard 톤맵·감마)과 게임 룩용 stylized diffuse(wrap ramp·top-light·grazing rim·point accent·hover outline) 두 셰이더 경로를 HLSL로 직접 작성.
- 본 팔레트를 cbuffer→1024본 StructuredBuffer SRV로 전환해 1드로우 스키닝 달성, 클립공간 가시성 마스크 CPU 프러스텀 컬링으로 드로우 절감.
- Fog of War를 R8 visibility 업로드 + 월드 오버레이 쿼드 + 미니맵 RGBA 합성으로 구현.

**레드플래그**
- ❌ "RenderGraph / GPU-driven / 실시간 GI / Nanite / Path Tracing 구현" → **전부 .md 문서뿐, 코드 0줄**. 'skeleton'도 과장이며 planned-only. (문서가 'RenderGraph.h 스켈레톤'이라 적었지만 그 헤더조차 없음.)
- ❌ "PBR 엔진" 단정 → IBL/그림자(CSM)/디퍼드/PostFX 부재(문서 자인). "forward, 단일 DX11, IBL/그림자 없음"을 선제 고지.
- ❌ `RHISceneRenderer`를 메인 렌더러로 소개 금지 → 가짜 반구 라이팅(normal.y*0.35+0.75)인 백엔드 포터블 프로토타입.
- 골든/스모크 자동검증 없음 → "프로파일러 계측+토글+수동 시각확인"으로 정직하게.

**키워드**: Forward Rendering, Cook-Torrance BRDF, GGX, GTAO, SSAO, Compute Shader, Depth-Normal Prepass, GPU Skinning, Structured Buffer, Frustum Culling, Fog of War, HLSL

### 3. ECS / 데이터주도 정의 — working

**쓸 수 있는 문장**
- sparse-set 컴포넌트 스토어(연속 메모리) + generation EntityHandle 자체 ECS와, system이 선언한 Read/Write 접근 집합으로 충돌을 정적 판정해 phase 내 비충돌 system을 JobSystem에 병렬 batch로 묶는 **데이터 접근 기반 스케줄러** 설계·구현.
- 챔피언/스킬 수치를 하드코딩 테이블에서 분리: 저작 JSON을 **오프라인 코드젠(Python)**으로 불변 Def pack(stable `DefinitionKey` + dense DefId)으로 cook, 런타임 frame 경로는 string lookup 없이 dense index만 조회.
- 결정론 contract(xorshift RNG·EntityID 정렬 순회·고정 tick) + per-tick 상태 FNV 해시로 same-seed 동일성/seed 민감도를 검증하는 헤드리스 회귀 게이트(SimLab).

**레드플래그**
- ❌ "archetype/chunk ECS" → 실제는 sparse-set(멀티컴포넌트 ForEach는 첫 store 순회 후 Has() probe). "sparse-set 선택"으로 정직하게.
- ❌ "데이터주도 cutover 완료" → 모든 resolver가 `ChampionGameDataDB` 폴백 유지(25회), StatSystem 여전히 championId/level 사용. "부분 cutover, 진행 중".
- ❌ "런타임 JSON 데이터주도 / hot reload" → offline codegen이고 hot reload 미구현.
- ❌ `CChampionGameplayFactory`·Foundation 추출 = 계획서만. "설계"로만.

**키워드**: sparse-set ECS, generation handle, system scheduler, data access conflict, parallel job batching, data-oriented design, immutable definition pack, DefinitionKey, offline codegen, deterministic simulation, state hashing

### 4. JobSystem / Fiber / Profiler — working

**쓸 수 있는 문장**
- 코어-2 워커 풀 + Chase-Lev work-stealing deque + 글로벌 MPMC 큐 하이브리드 JobSystem 구현, atomic JobCounter + help-stealing WaitForCounter로 fork-join 처리, TransformSystem/NavigationSystem을 임계값 기반 멀티스레드 fan-out(단일스레드 fallback 포함).
- 유휴 워커 yield 스핀을 1ms condition_variable 타임아웃 블로킹으로 전환해 외부 프로세스와 코어 경쟁/스터터 완화.
- CPU 계층 스코프 프로파일러(QueryPerformanceCounter, thread_local 스택) + DX11 disjoint+timestamp 4슬롯 링버퍼 GPU 타이머(non-blocking readback)를 매 프레임 구동, ImGui 오버레이 + 버전드 JSON 캡처. **이 계측으로 '스케줄러 병렬 배치가 실게임에서 발화 안 함(MaxBatchSize=1)'을 발견해 다음 최적화 단계를 정의** (측정주도 개발의 정직한 사례).

**레드플래그**
- ❌ "Fiber 기반 잡 시스템" → 기본 모드 ThreadOnly, FiberShell은 호출부 0(dead path), `CFiberPool`은 빈 클래스. "Fiber는 PoC/설계, 실가동은 스레드 풀".
- ❌ "엔진 전체를 병렬화" → 스케줄러 배치 병렬은 실측 MaxBatchSize=1(한 번도 안 돔), 실제 병렬은 Transform 루트/Nav fan-out 두 곳. "병렬 실행 경로 구현, 활용도는 부분적".
- ❌ 1만 job 스트레스/fiber leak 테스트 = 계획서만, 코드 없음.
- 방어 강점: WorkStealingDeque의 Chase-Lev 메모리 오더링, help-stealing 데드락 회피, GPU 타이머 disjoint/링버퍼는 깊게 파도 버팀.

**키워드**: work-stealing deque, Chase-Lev, job system, fork-join, help-stealing, condition_variable, QueryPerformanceCounter, D3D11 timestamp query, disjoint query, false sharing alignas64, Tracy

### 5. 네트워크 (IOCP/스냅샷) — working

**쓸 수 있는 문장**
- Windows IOCP(AcceptEx/WSARecv/WSASend) 비동기 TCP 게임 서버 구현, 다중 클라이언트 세션을 4-worker 완료포트로 처리, per-session 송신 큐 + pending-IO refcount 안전 해제.
- 16B 고정 헤더 길이접두 프레이밍 + FlatBuffers 직렬화로 Client/Server 공용 와이어 포맷 통일, 수신마다 `flatbuffers::Verifier` 검증 + 시퀀스 anti-replay/suspicion 가드.
- 고정 틱 권위 루프에서 결정론적 엔티티 정렬로 full Snapshot을 세션별 전송, 클라이언트 SnapshotApplier가 ECS에 적용하는 복제 round-trip 완성(스모크·ReplayRecorder 검증).

**레드플래그**
- ❌ "UDP를 했다" → UDP 소켓(recvfrom/sendto) 게임 코드에 0줄(Tracy 벤더뿐), `CUdpReliabilityChannel`·`CUdpClient`는 헤더 선언만 .cpp 없음. "UDP는 헤더/스키마/계획 설계 단계".
- ❌ "delta 스냅샷 / AOI" → 스키마 필드만, 항상 full 스냅샷. planned.
- ❌ "N만 동접" → 부하 테스트 수치 없음. "구조상 다중 accept 가능, 실측은 localhost 스모크 수준".

**키워드**: IOCP, AcceptEx, WSARecv/WSASend, completion port, length-prefixed framing, FlatBuffers, snapshot replication, server-authoritative, anti-replay, Verifier

### 6. 서버권위 시뮬레이션 — working

**쓸 수 있는 문장**
- IOCP 위에 ECS 시뮬 코어를 올려 30Hz 고정-dt 권위 tick 루프 구현. **Client Input→GameCommand→서버 GameSim→Snapshot/Event→Client 시각**까지 닫힌 루프로 위치/HP/쿨타임/투사체/미니언/타워를 서버에서 판정.
- 클라 신뢰 제거: 명령 issuer를 sessionId→controlledEntity로 서버 결정, 시퀀스 검증/coalescing, DeterministicEntityIterator 정렬 + DeterministicRng/Time 결정성 강제, SimLab 동일-seed A/B/C 재현 비교로 검증.
- 1500줄 비대 GameRoom을 `ServerProjectileAuthority`·`WalkabilityAuthority`·`MinionWaveRuntime` 등 순수 authority 모듈로 atomic 분해, 네트워크 의존 제거(rg 검증) + 스모크/빌드 게이트.
- 터렛/스킬/미니언 투사체를 세그먼트 충돌·타게팅·DamageRequest 큐로 서버 판정, spawn/hit ReplicatedEvent로 전파.

**레드플래그**
- ❌ "1판이 완결된다" → **넥서스 파괴 승패·매치 종료·PostGame 전이 없음**(eRoomPhase에 InGame까지만). "승패 종료 미구현, 1판 완결은 다음 단계"를 먼저 인정.
- ❌ "랙보정 적용" → 히스토리 기록만, 명령 처리 시 rewindTicks=0(미적용). "인프라는 있고 적용은 미연결".
- ❌ "클라 예측/reconciliation" → 문서만, 코드 0. "권위 스냅샷 직접 적용, 예측은 계획".

**키워드**: server-authoritative simulation, 30Hz fixed-dt tick, deterministic simulation, snapshot/event replication, projectile authority, flow-field pathfinding, NavGrid, anti-spoofing, command sequence validation

### 7. 챔피언/스킬 (GAS 등가) — working

**쓸 수 있는 문장**
- LoL류 PvP의 서버권위·결정론 요구를 충족하려 UE5 GAS를 1급 Tag/Effect 객체 없이 ECS로 재해석: champ×variant 함수포인터 디스패치 테이블(`GameplayHookRegistry`) + 단일 캐스트 게이트(`CommandExecutor::HandleCastSkill`)로 cooldown/learned-rank/CC/타겟·사거리/2단스테이지를 서버에서만 확정.
- 스킬 파라미터(cost/cooldown/range/stage/effect)를 3670줄 JSON 데이터팩으로 분리, `GameplayDefinitionQuery`로 해석하는 데이터주도 cutover 진행.
- 15개 챔피언 QWER를 서버에 배선하고 투사체·대시·넉업·슬로우·은신/무적, Viego 빙의(FormOverride)·Sylas 궁 탈취 같은 비자명 메커니즘 구현.
- 결정론 골든 러너(SimLab): 동일 seed+커맨드에서 per-tick state hash 일치 검사(divergence 시 exit 1) + accept/reject 사유 로깅.

**레드플래그**
- ❌ "GAS를 만들었다 / UGAS 포팅" → 코드에 GameplayTag/AttributeSet 명명 0건. "GAS의 통찰을 ECS로 등가 구현, Tag/Effect 1급 객체는 의도적으로 안 만듦".
- ❌ "완전 구현 15챔피언" → MasterYi는 빈 스텁, Riven은 Q/W만(E/R 없음). "15명 배선, 완성도 편차(Zed/Yasuo/Viego 풀, Riven 절반, MasterYi 스텁)".
- ❌ Ezreal/Garen을 구현 챔피언에 넣기 금지 → 클라이언트측 PoC, 서버 미배선(오히려 권위 규칙 위반 사례).
- ❌ "데이터주도 100%" → fallback 상수 잔존.

**키워드**: server-authoritative, deterministic-simulation, GAS-equivalent, ability-hook-registry, function-pointer-dispatch, cooldown-system, status-effect, crowd-control, skill-rank, AttributeSet-equivalent, FormOverride, golden-test

### 8. AI (HFSM/Utility/봇) — working

**쓸 수 있는 문장**
- 외부 AI 라이브러리 없이 서버권위·결정론 제약 하에서 LoL 라인전 봇 구현. **"봇은 truth를 직접 수정하지 않고 GameCommand만 생산하는 가짜 플레이어 입력"** 북극성 제약 + perception→Utility 스코어링(전 가치를 골드로 환산)→HFSM(7상태)→brain→command 파이프라인 (18 챔피언 profile/7 콤보).
- brain stateless 분리 + 상태를 컴포넌트에 모아 재진입 안전성 확보, RuleBased/PlayerLike brain 디스패치로 챔피언별 전술을 공용 코드 오염 없이 확장.
- 결정론 SimLab 자동 검증 + 14개 런타임 노브·결정 트레이스 링버퍼·AIDebugControl 명령 왕복으로 인게임 튜닝/디버깅.

**레드플래그**
- ❌ "BT/MCTS/RL 했다" → **즉사**. MCTS는 UCT 본문은 있으나 출력(macroGoal)을 아무도 안 읽음, BT는 AIIntent 큐에 push만(소비자 0), RLBridge는 LoadModel이 무조건 false인 stub. "PoC/격리된 비권위 실험 경로"로만.
- ❌ "난이도별 봇 / 적응형 PlayerLike brain" → difficulty는 저장만(결정에서 안 읽힘), brainType 스폰 미배선(라이브는 RuleBased만). "설계는 했으나 라이브 미배선".
- ❌ "고도화된 perception" → 문서가 스스로 '전지적 perception, 스킬샷 리드 없음'을 한계로 명시.
- ❌ MobaZero/NYPC = 논문 리뷰 노트만, 코드 없음.

**키워드**: HFSM, Utility AI, command-only, deterministic simulation, server-authoritative, decision trace, golden test, LoL bot AI

### 9. FX / Effect Tool — working

**쓸 수 있는 문장**
- Niagara식 데이터주도 FX 파이프라인 구축: 스킬 FX를 JSON(WFX) 에셋으로 외부화, cue 이름으로 multi-emitter(Billboard/Beam/Ribbon/Mesh/Decal)를 ECS 스폰하는 런타임으로 재컴파일 없이 13챔피언 112개 이펙트 구동.
- 서버권위 이벤트(projectile spawn/hit/attached)를 visual catalog로 cue에 매핑해 클라에서 1회 재생, 멀티플레이 이펙트 일관성을 서버 기준 정렬.
- WFX load/save ImGui 에디터 패널 + 에셋 카탈로그, per-frame ECS Update/Render에 드로우 카운터(Fx::Drawn) 계측.

**레드플래그**
- ❌ "노드 그래프/SoA 파티클 풀/expression VM/GPU compute"를 묶어 자랑 → 노드그래프+SoA풀은 EldenRing **에디터에서만** 호출되는 prototype(LoL 런타임 미통합), VM/GPU compute는 문서뿐(cpp 없음, bytecodeBlob 필드만).
- "데이터주도 FX 런타임"=WFX(JSON 플랫 에셋) 경로, 노드그래프와 별개임을 정확히 구분.
- JSON 파서가 수기 문자열 추출 → 견고성 질문 취약. cue 누락 시 C++ 하드코드 fallback billboard 잔존.
- 정직선: "production은 WFX 큐 런타임·서버 cue 경로·ImGui 에디터, 노드그래프/VM/GPU는 PoC·로드맵".

**키워드**: data-driven VFX, WFX JSON asset, particle cue system, server-authoritative FX, ECS particle runtime, ImGui FX editor, node graph compiler(prototype), topological sort validation, Niagara-style

### 10. 에셋 파이프라인 / Winters 포맷 — working

**쓸 수 있는 문장**
- FBX 런타임 파싱(50~200ms) 문제를 해결하려 16B 공통 헤더 기반 자체 바이너리(.wmesh/.wskel/.wanim/.wmat) 설계, Assimp 기반 오프라인 컨버터(`WintersAssetConverter.exe`)와 런타임 zero-copy 로더를 C++20으로 구현해 end-to-end 쿡→로드 파이프라인 동작.
- 메시/스켈레톤/애니의 정합성을 FNV1a skel_hash·본 카운트로 교차검증, 불일치 시 Assimp fallback 자동 전환.
- skinned 정점 stride(76B)를 런타임 입력 레이아웃 byte offset과 `static_assert`로 묶어 ABI 계약화, info 헤더 게이트 + F5 인게임 Idle/Run 재생 체크리스트로 검증.

**레드플래그**
- ❌ "무결성(SHA256/Ed25519)·압축(LZ4)" → 코드 0줄, 모든 로더가 flags≠WF_NONE면 거부하는 스텁. "MVP는 zero-parse 로드에 집중, 무결성·압축은 설계만".
- ❌ "로딩 1ms" → 설계 목표치, 실측 없음. "zero-copy 레이아웃으로 파싱 단계 제거" 구조적 근거로만.
- ❌ ".wtex/.wmap/.winters 번들" → planned-only.
- 강점 방어: writer/loader 본문이 실제 로직이고 디스크 산출물 바이트가 코드와 일치, Model.cpp에서 실제 재생까지 연결 → 까도 안 무너짐.

**키워드**: Assimp, binary asset format, zero-copy loader, skinned mesh, skeletal animation, FNV-1a hash, asset cooking pipeline, CLI converter, vertex stride ABI contract

### 11. EldenRing 클라/에디터/추출 — **prototype** (제작 예정 단계)

**쓸 수 있는 문장**
- 원작 Elden Ring 바이너리(MSB/FLVER/TPF/HKX)를 WitchyBND→Blender→자체 컨버터로 엔진 바이너리(.wmesh/.wmat/.wskel/.wanim)로 변환하는 **5,000줄+ Python 데이터주도 에셋 cook 파이프라인** 설계·구현, 단계별 retry·바이너리 info audit·무인 드라이버(hidden-console·MAX_PATH·실패 슬라이스 가드)로 Limgrave placement 3,862개 자동 복원(missing wmesh 0).
- MSB placement를 source of truth로 cooked .wmesh를 런타임 자동 로드·스폰하고 스킨드 캐릭터 애니를 순환 재생하는 DX11 Limgrave 쇼케이스 클라이언트(`WintersElden.exe`) — 게임플레이가 아닌 **에셋/공간 복원 검수 diorama**, map_closure 감사 카운터로 누락 추적.
- DX12 ImGui 도킹 에디터(`WintersEldenRingEditor.exe`): World cell 문서(JSON) + command 패턴 트랜잭션 undo/redo + FX graph compile·Sequencer·World partition cell 전이·hitbox overlap을 실제 Engine 시스템 호출로 검증하는 검수 패널.

**레드플래그**
- ❌ "Elden Ring을 만들었다" → **게임플레이 0**(플레이어 조작/lock-on/dodge/stamina/전투 상태머신 코드 grep 0건). "클립을 타이머로 순환 재생하는 쇼케이스, 입력 구동 상태머신 아님".
- ❌ "DX12 3D 에디터" → 뷰포트가 'Preview pending', JSON+텍스트 편집 수준.
- ❌ "PBR material resolver / 보스 HFSM" → 전자는 DX11 상수버퍼+텍스처 role 휴리스틱, 후자는 문서만(에디터 Boss 패널은 hitbox 기하 overlap뿐).
- ❌ "World partition 스트리밍" → dt=0 headless probe로만 구동, 실게임 루프 미연결.
- 진짜 강점: 실제 빌드되는 두 exe, end-to-end cook 파이프라인, source-of-truth placement→자동 로드→감사 카운터 데이터주도 설계.

**키워드**: WitchyBND, FLVER, MSB placement, HKX animation, Blender headless FBX, asset cook pipeline, DX12 RHI, ImGui docking editor, WorldCellDocument JSON, command-pattern undo/redo, data-driven asset reconstruction

### 12. 성능 최적화 / 측정 인프라 — working ⭐(이력서 척추)

**쓸 수 있는 문장**
- thread-local scope stack 기반 계층형 CPU 프로파일러를 매 프레임(Debug/Release 상시) 29개 모듈에서 계측, EMA 스무딩·age/prune 안정 행 뷰로 흔들리는 타이밍을 읽게 함.
- DX11 disjoint+timestamp 쿼리를 4슬롯 링으로 non-blocking 읽어 GPU 프레임 시간을 노출, CPU와 대조해 **'작은 씬임에도 완전 CPU 바운드(드로우콜 ~94)'를 측정으로 확정**.
- DLL 경계에서 동일 string literal이 다른 주소로 잡혀 카운터가 중복되던 버그를 포인터비교→strcmp로 수정, 60fps 경계 슬로모션 스터터를 만들던 16.7ms dt 하드클램프를 0.1s 스파이크 상한으로 재설계.
- 버전드 JSON 캡처(스키마+트렁케이션 플래그) + 타임스탬프 아카이브(F4) + 리미터 토글(F11)로 재현 가능한 측정 파이프라인 구축, 그 캡처를 근거로 frame budget 게이트 기반 7단계 최적화 로드맵 작성.

**레드플래그**
- ❌ "300~650 FPS 달성" → 마스터플랜 이론 추정치, Phase 1~7 코드 미반영(문서 자기명시). "측정 인프라를 만들고 최적화는 설계/로드맵 단계".
- ❌ "엔진을 병렬화해 빨라졌다" → ECS 병렬 스케줄러 코드 경로는 있으나 MaxBatchSize=1(병렬 미발동). "병렬 실행 경로 구현, 배치 튜닝 미완".
- 9.54ms/94 드로우콜은 문서 인용 → "F4로 캡처해 스키마째 보여줄 수 있다"로 방어(외워 말하지 말 것).
- **이 도메인이 핵심 차별화**: "측정 인프라를 먼저 만들고, 그 측정이 오히려 자기 한계(병렬 미발동)를 드러냈다"는 정직한 측정주도 개발 서사 = 시니어 시그널.

**키워드**: CPU profiler, hierarchical scope timing, thread-local stack, EMA smoothing, Tracy, DX11 timestamp query, disjoint query, frame budget, JSON capture schema, CPU-bound diagnosis, dt clamp, frame pacing

### 13. UI 파이프라인 — working

**쓸 수 있는 문장**
- DX11 기반 자체 배치 UI 스프라이트 렌더러(`CUIRenderer`) 설계·구현: 동적 VB, 전용 VS/PS, SRV 변경 시 flush 배칭, 전체 DX11 state save/restore, 미니맵용 원형 클립으로 ImGui immediate-mode 의존을 줄이고 드로우콜을 배치.
- 기획/디자인 값(텍스처 경로·스프라이트 atlas rect)을 JSON으로 분리하는 `UIAtlasManifest`를 자체 파서로 구현 + WIC PNG→SRV 로딩, 코드 수정 없이 HUD/상점 에셋 교체.
- Client(ECS/Snapshot)→Engine UI 단방향 상태 동기화: ActorHUDState/UIWorldHealthBarDesc로 truth를 추려 전달, 체력바·ActorHUD·미니맵·데미지폰트·킬피드·상점을 매 프레임 렌더.

**레드플래그**
- ❌ "자체 UI 프레임워크" → 텍스트/숫자/상점/스코어보드는 여전히 ImGui ImDrawList+ImGui 폰트 의존, 자체 런타임 폰트 렌더러 없음. "스프라이트/이미지 계층은 자체, 텍스트는 ImGui 어댑터".
- ❌ "retained-mode 위젯 트리(CUIRoot 등)" → 코드 없음, 아키텍처 문서만. "legacy Manager/UI 동작, 위젯 트리는 설계만".
- Lua UI는 prototype. 자동 UI 게이트 없음.

**키워드**: DX11, batched sprite renderer, UI atlas manifest, JSON-driven UI, WIC texture loading, HUD, minimap, fog of war, health bar, damage floater, ImGui adapter, ECS-to-UI sync

### 14. 백엔드 (Go) — working

**쓸 수 있는 문장**
- 게임 클라의 match 밖 영속 상태를 분리하려 인증·매칭·상점·결제·프로필·랭킹 6개를 Go 마이크로서비스(chi/pgx/go-redis/kafka-go)로 구현, PostgreSQL 트랜잭션+FOR UPDATE 행잠금으로 지갑 차감 정합성, 결제 멱등키로 중복 충전 방지.
- Kafka 이벤트(MatchCompleted/PaymentCompleted) producer→consumer로 매치 결과를 프로필 전적·Redis ZSET 랭킹에 비동기 전파.
- JWT(HS256 access+refresh) + Redis 토큰 회전, C++ 클라에 WinHTTP 비동기 SDK(워커 호출+메인 콜백 펌프)로 프레임 정지 없이 로그인·로비 동기화·매치 join 실연동.

**레드플래그**
- ❌ "결제 시스템 구축" → MockGateway만 등록(VerifyReceipt가 가짜 txID). "게이트웨이 추상화+멱등성 설계".
- ❌ "E2E 로그인 데모" → 로그인 씬 버튼이 오프라인 로그인만 호출. "SDK/서버 경로 구현, UI 배선은 오프라인 기준".
- ❌ "신뢰성 이벤트 파이프라인" → consumer가 실패 메시지 nil 스킵, DLQ 없음.
- ❌ "실서비스 매치메이킹" → matchSize=2 프로토타입.
- ❌ Social/Replay 서비스 = 계획만.
- 자동화 테스트 0개(*_test.go 없음) → "검증은 go build + 수동 실행 수준" 정직하게.

**키워드**: Go, microservices, chi, PostgreSQL, pgx, Redis, Kafka, JWT, bcrypt, idempotency-key, FOR UPDATE row lock, Redis ZSET, event-driven, docker-compose, WinHTTP async client

### 15. 보안 / 안티치트 — working

**쓸 수 있는 문장**
- 서버권위 입력검증을 안티치트 1차방어로 정의: 클라가 위치 결과 대신 이동/캐스트 '의도'만 전송하게 명령 모델을 재설계해 스피드핵·텔레포트의 공격 표면 자체를 제거(서버 주도 그리드 이동).
- 캐스트/평타에 서버가 쿨다운 소유 + 사거리(거리제곱 vs range²)·타겟 생존/타게팅·학습 여부를 매 명령 검사해 reason 코드와 거부.
- 세션-엔티티 바인딩으로 제어권을 lobby authority에서만 도출(스푸핑 차단), 입력 경계에 시퀀스 anti-replay + flatbuffers 검증, 히트판정용 200ms 바운드 랙 보상 히스토리.

**레드플래그**
- ❌ "안티치트(탐지형)" 기대 충돌 → "이건 탐지형이 아니라 권위형 1차방어. 유저모드/커널은 설계만, 코드 0줄".
- ❌ `CSpeedChecker`/`CServerValidator` 등 클래스를 "구현했다" → 그 파일들 실존 안 함(로직은 CommandExecutor에 흡수).
- ❌ "스피드핵을 막았다" → "클라가 위치를 주장 못 하게 해 스피드핵 표면을 없앴다" 정확한 표현.
- FlagSuspicious는 집계만, kick/ban 미연결.

**키워드**: server authority, anti-cheat, input validation, lag compensation, rewind, anti-replay, sequence window, flatbuffers verification, session-entity binding, cooldown authority, range check

### 16. 협업 / 툴링 / 빌드 — working

**쓸 수 있는 문장**
- 다중 머신/다중 AI 에이전트가 같은 엔진 repo를 동시 수정할 때 GameSim 결정론 붕괴를 막으려 헤드리스 5v5 시뮬 러너(SimLab)로 same-seed 재현·seed 민감도·스킬 상태기계 복귀를 FNV 해시로 검증, exit code로 회귀 게이트화.
- Client/Engine/Shared 의존성 역전(DX11/DX12 concrete의 공용 헤더 누출)을 코드리뷰가 아닌 자동 게이트로 차단: rg 경계 audit + CMake/Ninja·MSBuild 이중 빌드 + exe runtime smoke + 데이터-코드 contract 교차검증을 묶은 PowerShell 검증 하니스 2종 + 타임스탬프 report 자동 생성.
- 수기 .vcxproj.filters 편집을 없애려 CMake `source_group(TREE)` 기반 VS 필터 자동 생성 + Ninja Multi-Config preset 도입, Engine·EldenRing을 CMake/Ninja로 전환(MSBuild와 병렬 운영).

**레드플래그**
- ❌ "CMake 마이그레이션 완료" → Engine+Elden만 전환, LoL Client/Server는 vcxproj/MSBuild. "점진/병렬 운영 단계".
- ❌ **"Perforce 경험"** → **코드에 Perforce 전무. 'P4'는 'Phase 4' 약자.** 이력서에 Perforce 쓰면 즉사. 절대 넣지 말 것.
- ❌ "결정론 테스트가 실게임과 동일" → SimLab은 FlatWalkable(navgrid 없는 평면) GameSim-only 미러. "서버 시뮬 코어 결정론"으로 한정.
- ownership matrix 강제는 의존성 audit 한정, 소유권 자체는 사람 규율.

**키워드**: CMake, Ninja Multi-Config, CMakePresets, source_group(TREE), MSBuild, deterministic simulation, golden test, FNV-1a hash, PowerShell validation harness, CI gate, dependency boundary audit, headless sim runner

### 17. 애니메이션 / 물리 / 오디오 — working(약점이되 통제됨)

**쓸 수 있는 문장**
- 외부 물리/애니 미들웨어 없이 C++20/DirectXMath로 스켈레탈 애니 런타임 직접 구현: 본 계층 누적(Assimp offset 규약), 키프레임 이진탐색 + lerp/slerp, scratch 버퍼 재사용으로 매 프레임 평가, ModelRenderer + 26개 모듈에서 소비.
- Assimp 임포트를 자체 .wanim/.wskel 바이너리(magic·version·skel-hash 검증, FNV1a 본 이름 해시)로 직렬화하는 파이프라인.
- SAT 기반 yaw-OBB / sphere-box closest-point / AABB Overlap + NaN sanitize + telegraph·active·dodge 프레임 윈도우를 갖춘 격투형 히트 판정 충돌 수학 라이브러리.
- 강체·천·파괴·3D 오디오·애니 블렌딩 미구현을 챕터 문서·7단계 물리 계획으로 정직하게 진단하고 MOBA 특성에 맞춰 우선순위화.

**레드플래그**
- ❌ "물리 엔진을 만들었다" → 강체/솔버/CCD 0건(grep). "SAT 충돌 판정 라이브러리 + 프레임 윈도우".
- ❌ "전투 판정에 쓰인다" → 충돌 라이브러리 유일 소비처가 에디터 디버그 패널. "working 라이브러리, prototype 통합".
- ❌ "애니 블렌딩/StateMachine/IK" → 단일 클립 재생만(grep 0건), 챔프별 castFrame 하드코딩이 확장 한계.
- ❌ "3D 공간 오디오" → 순수 2D FMOD 래퍼. "wrapper 수준 + 로드맵".

**키워드**: Skeletal Animation, Keyframe Interpolation, Slerp, Bone Hierarchy, DirectXMath, Custom Binary Format, SAT, OBB Collision, Hitbox Frame Window, FMOD

---

## 이력서 전역 레드플래그 (한 곳에 모음 — 절대 쓰지 말 것)

| 쓰면 즉사하는 표현 | 진실 | 대체 표현 |
|---|---|---|
| Vulkan/Metal/Console RHI | enum만, 코드 0 | "DX11+DX12, 그 외는 경계 설계만" |
| RenderGraph/GI/Nanite/PathTracing | 코드 0줄 | (언급 시) "설계/로드맵 단계" |
| Fiber 기반 잡 시스템 | ThreadOnly 기본, Fiber dead | "Fiber는 PoC, 실가동은 스레드 풀" |
| UDP 넷코드/AOI/델타 | 헤더만, .cpp 없음 | "TCP 권위 복제 구현, UDP는 설계" |
| 1판 완결/승패 종료 | 넥서스 승패 없음 | "권위 루프 닫힘, 매치 종료는 다음 단계" |
| BT/MCTS/RL 봇 | 출력 미연결 dead | "HFSM+Utility 봇, BT/MCTS는 격리 PoC" |
| 결제 시스템 | MockGateway만 | "게이트웨이 추상화+멱등성 설계" |
| **Perforce 경험** | **코드에 없음(P4=Phase4)** | **언급 금지** |
| 유저모드/커널 안티치트 | 문서뿐, 코드 0 | "서버권위 입력검증 1차방어" |
| 물리 엔진 | 충돌수학 라이브러리뿐 | "SAT 충돌 판정 라이브러리" |
| 300~650 FPS 달성 | 이론 추정치, 미반영 | "측정 인프라 구축, 최적화는 로드맵" |
| CMake 완전 전환 | Engine+Elden만 | "점진 전환, LoL은 vcxproj 병행" |

## 면접에서 정직성을 무기로 바꾸는 한 문장

> "이 엔진은 제가 *어디까지 했고 어디부터 계획인지*를 README의 구현상태 표와 도메인별 검증 게이트로 스스로 그어 둡니다. 측정 인프라를 먼저 만들었고, 그 측정이 오히려 제 코드의 한계(병렬 배치가 실제로는 발화하지 않음)를 드러내 다음 작업을 정의했습니다. 저는 화려한 기능 목록보다 *문제를 정의하고 측정으로 검증하는 루프*를 보여드리고 싶습니다."
