# 07. 네트워크 · 서버 권위 · 복제 (Network · Server Authority · Replication)

> **상태 동기화 (2026-07-11)**: 아래 TCP server-authority 설명은 현재 runtime과 맞다. UDP는 아직 미구현이며, 최신 목표는 Backend HTTPS를 유지한 채 WintersServer 실시간 session 전체를 UDP로 옮기는 것이다. 현재/목표/Fiber 결합을 구분할 때는 [2026-07-11 통합 감사](../../plan/2026-07-11_FULL_UDP_AND_SERVER_FIBER_INTEGRATION_AUDIT.md)를 함께 본다.

> 면접 대본 겸 지식 베이스. 코드 문법이 아니라 **도메인 구조와 의사결정**을 설명한다.
> 모든 인용은 실제 코드에서 검증된 것이며, path는 repo-relative다.

---

## ① 도메인 한 줄 정의

"Winters의 네트워크는 **IOCP 기반 TCP 서버가 30Hz 고정 틱으로 모든 게임플레이 truth를 소유**하고, 클라이언트는 **명령(Command)만 올려보내고 스냅샷/이벤트를 받아 프레젠테이션만 담당**하는 서버 권위(server-authoritative) 구조입니다. 클라 예측은 '몸통 회전(yaw)' 같은 체감 지연이 큰 부분에만 제한적으로 허용하고, 서버가 실제로 따라잡을 때까지만 보호합니다."

---

## ② 구조와 데이터 흐름

### 전체 루프 (한 프레임 / 한 틱)

```text
[클라이언트]                                  [서버]
우클릭/QWER 입력
  │ CCommandSerializer::SendMove/CastSkill
  │  seq·clientTick 부여, FlatBuffers CommandBatch
  ▼
TCP (PacketHeader 16B + payload) ──────────▶ IOCP worker (GetQueuedCompletionStatus)
                                              │ CFrameParser: NeedMore/Complete/Invalid
                                              │ CPacketDispatcher: FlatBuffers Verify
                                              │  실패 → FlagSuspicious (silent drop 금지)
                                              │ CSession::TryAcceptSequence (리플레이/폭주 거부)
                                              ▼
                                             CCommandIngress (별도 mutex 큐)
                                              │ Move 코얼레싱 + stable_sort 결정론 정렬
                            ┌─────────────────┘
                            ▼  30Hz Tick (m_stateMutex 단일 락)
              DrainCommands → ServerBotAI → ExecuteCommands
              → SimulationSystems(15챔프 GameSim 포함)
              → LagComp.RecordHistory
              → BroadcastEvents(edge) → BroadcastSnapshot(state)
                            │                    │
              ┌─────────────┘                    └─▶ ReplayRecorder(.wrpl, 방송 바이트 그대로)
              ▼
[클라이언트] CSnapshotApplier / CEventApplier
  verify → EnsureEntity → 컴포넌트 채움 → stale 수거
  → 55ms 보간, 로코모션 합성, yaw 예측 보호, ack 기반 예측 prune
```

### 와이어 포맷

- 길이 접두(length-prefix) 프레이밍. `PacketHeader`는 `magic(0x5742 'WB') / version / type / flags / payloadSize / sequence` 16바이트 고정 — `static_assert(sizeof(PacketHeader) == 16)`로 박제 (`Shared/Network/PacketEnvelope.h:34`).
- payload는 전부 FlatBuffers: `CommandBatch / Snapshot / Event / Hello / LobbyCommand / LobbyState`.
- `PacketFlag_Compressed / Encrypted` 플래그는 정의만 있고 아직 미사용 — 압축/암호화는 로드맵이다 (`Shared/Network/PacketEnvelope.h:27`).

### 계층 요약

| 계층 | 소유 코드 | 책임 |
|---|---|---|
| 트랜스포트 | `Server/Private/Network/IOCPCore.cpp`, `Session.cpp` | AcceptEx pre-post, 비동기 recv/send, 세션 수명 |
| 프레이밍 | `Server/Private/Network/FrameParser.cpp` | 스트림→메시지 절단, 손상 스트림 컷 |
| 라우팅/검증 | `Server/Private/Network/PacketDispatcher.cpp` | FlatBuffers verify, 세션→룸 매핑 |
| 인제스트 | `Server/Private/Game/CommandIngress.cpp` | 네트워크/시뮬 스레드 디커플, 코얼레싱, 결정론 정렬 |
| 시뮬레이션 | `Server/Private/Game/GameRoomTick.cpp` | 30Hz 고정 틱 페이즈 파이프라인 |
| 복제 | `Server/Private/Game/SnapshotBuilder.cpp`, `GameRoomReplication.cpp` | 상태 스냅샷 + edge 이벤트 방출 |
| 클라 적용 | `Client/Private/Network/Client/SnapshotApplier.cpp`, `EventApplier.cpp` | verify→ensure→populate→reap, dedup |
| 프레젠테이션 | `Client/Private/Scene/Scene_InGameNetwork.cpp` | 보간, 로코모션 합성, 예측 보호 |

식별자 경계는 `EntityIdMap`이 소유한다: 서버 로컬 `EntityID`와 와이어 `NetEntityId(u32, 1부터)`를 두 개의 unordered_map으로 양방향 매핑하고, `IssueNew`는 idempotent, 파괴 시 `Unbind`로 동기화한다 (`Shared/GameSim/Replication/EntityIdMap.h`). 클라와 서버가 서로의 로컬 인덱스를 절대 노출하지 않는다.

### 틱 페이즈 파이프라인 — 순서 자체가 데이터 흐름 계약

`Tick()`은 `steady_clock` 기준 33,333µs 주기를 `next += period; sleep_until(next)`로 드리프트 없이 유지하고, 진입 즉시 `m_stateMutex`를 잡아 월드 전체를 한 락 안에서 돌린다. 네트워크 워커 4개는 이 락을 절대 잡지 않는다 — CommandIngress의 별도 mutex 큐까지만 접근한다 (`Server/Private/Game/GameRoomTick.cpp:68-111`).

페이즈 순서에는 각각 인과가 실려 있다:

```text
Phase_DrainCommands        사람 명령 확정 (여기서 세션→엔티티 lazy 재해석)
Phase_ServerBotAI          봇도 GameCommand만 생산해 같은 벡터에 push
Phase_ExecuteCommands      사람+봇 명령을 같은 틱, 같은 executor로 실행
Phase_SimulationSystems    Status→Stat→Cooldown→Combat→Move→JungleAI→AttackChase
                           → (중간 ExecuteCommands: AI 시스템이 방금 만든 명령 재실행)
                           → 15챔피언 GameSim::Tick → 미니언/터렛/투사체
                           → DamageQueue 소비 → Stat 재계산 → Death → 리스폰
LagComp.RecordHistory      시뮬 확정 후·방송 전에 이력 기록 (되감기 기준점)
Phase_BroadcastEvents      edge 이벤트 방출 (+.wrpl 미러링)
Phase_BroadcastSnapshot    상태 스냅샷 방출 (+중립 스냅샷 리플레이 기록)
```

데미지를 즉시 적용하지 않고 큐 엔티티로 쌓았다가 tick 끝에 정렬 소비하는 것, 죽음/리스폰이 항상 마지막인 것이 "여러 소스가 같은 틱에 같은 타겟을 때려도 결과가 결정론적"인 이유다 (`GameRoomTick.cpp:120-163`).

---

## ③ 핵심 설계 결정과 트레이드오프

### 결정 1 — 서버 권위 + 클라 시뮬 시스템 전면 게이팅

- **왜**: MOBA는 치트에 민감하고, 피해/쿨다운/이동 truth가 두 군데 있으면 반드시 발산한다.
- **대안**: 클라 시뮬 + 서버 검증(락스텝/롤백), 혹은 P2P.
- **선택 이유**: truth를 서버 한 곳에 두면 검증·리플레이·봇이 전부 같은 경로를 탄다. 나는 이를 코드로 강제했다 — 네트워크 권위 모드면 `OnEnter`에서 Navigation/LocalUnitAI/TurretAI/StructureProjectile/BehaviorTree/MCTS 시스템 등록을 전부 `if (!m_bNetworkAuthoritativeGameplay)`로 건너뛰고, `CMinion_Manager`도 `Set_Enabled(!authoritative)`로 끈다 (`Client/Private/Scene/Scene_InGameLifecycle.cpp:512,529-581`). 클라는 위치/HP/쿨다운을 만들지 않고 스냅샷 소비 + 애니/FX 재생만 한다.
- **감수한 비용**: 모든 입력이 왕복 지연(RTT)을 먹는다. 이를 보간(55ms)과 제한적 예측(yaw)으로 흡수했고, 그 충돌 사례가 ④의 war story다.
- **계기**: 게이팅 전에는 클라 `CNavigationSystem`이 SnapshotApply와 SyncFromECS 사이에서 챔피언 yaw를 덮어써 이동이 step-like로 튀는 사고가 있었다(gotchas 2026-05-22). "클라 시뮬을 고치는" 대신 "클라 시뮬을 없애는" 구조적 해법을 택했다.

### 결정 2 — IOCP 리액터 + pending-IO 카운트 지연 소멸 + single-outstanding send

- **왜**: Windows에서 수백 세션급 비동기 I/O의 표준은 IOCP고, 취업 목표 도메인(게임 서버)의 핵심 역량이기도 하다.
- **구조**: `CreateIoCompletionPort`에 워커 수만큼 concurrency를 잡고, 리슨 소켓에 `WSAIoctl`로 AcceptEx/GetAcceptExSockaddrs 함수 포인터를 로드, 시작 시 **AcceptEx 4개를 pre-post** 한다 (`Server/Private/Network/IOCPCore.cpp:93-129`). 워커는 `GetQueuedCompletionStatus`로 깨어나 `CONTAINING_RECORD`로 IOContext를 복원하고(:216,225), 종료는 워커 수만큼 `PostQueuedCompletionStatus(..., nullptr)`로 깨워 join한다(:143).
- **세션 수명**: shared_ptr만으로는 부족하다 — OVERLAPPED가 커널에 떠 있는 동안 객체가 죽으면 use-after-free다. 그래서 매 WSARecv/WSASend 전 `AddPendingIo()`, 완료 시 `CompletePendingIo()`로 원자 카운트를 유지하고, 끊긴 세션은 `m_closingSessions`로 옮겨 카운트 0인 것만 reap한다. `Find()`는 닫히는 세션까지 뒤져 in-flight 완료가 세션을 잃지 않게 한다 (`Server/Private/Network/Session.cpp:61-67`, `Session_Manager.cpp:50-98`).
- **송신 직렬화**: 세션당 deque + `m_bSendPending` 플래그로 **동시에 WSASend는 항상 1개**만 건다. 겹친 WSASend가 바이트 순서를 깨는 문제를 원천 차단한다 (`Server/Private/Network/Session.cpp:91-160`).
- **감수한 비용**: single-outstanding 큐에는 백프레셔/드롭 정책이 없다 — 느린 클라이언트에서 브로드캐스트가 매 틱 쌓이면 큐가 무한정 자랄 수 있다. 알고 있는 미완 지점이고 ⑤에서 다룬다.

### 결정 3 — 명령 업스트림: 시퀀스 게이트 → Move 코얼레싱 → 결정론 정렬

- **왜**: 신뢰할 수 없는 입력 스트림을 (a) 보안적으로, (b) 결정론적으로, (c) 체감 좋게 소화해야 한다.
- **3단 장치**:
  1. **시퀀스 수용**: `CSession::TryAcceptSequence`가 과거/중복 seq(`seq <= last`)를 거부하고, `last+60` 초과 점프는 suspicious로 거부한다 (`Server/Private/Network/Session.cpp:26-42`). 리플레이 공격과 시퀀스 폭주가 sim에 닿기 전 네트워크 경계에서 걸러진다.
  2. **Move 코얼레싱**: `EnqueueCommand`가 같은 세션의 아직 실행 안 된 pending Move를 최신 것으로 **교체**한다 (`Server/Private/Game/CommandIngress.cpp:74-85`). 우클릭 연타가 매 틱 눈에 보이는 조향 회전으로 실행되던 문제의 해법이다. 비-Move 명령은 권위적으로 전부 보존한다.
  3. **결정론 정렬**: `DrainSorted`가 `(acceptedTick, sessionId, sequenceNum)` stable_sort로 멀티 워커가 밀어넣은 명령의 전역 실행 순서를 고정한다 (`CommandIngress.cpp:98-106`). 리플레이 재현성의 전제다.
- **대안**: 명령을 수신 즉시 실행(순서 비결정), 혹은 모든 Move를 실행(연타 시 저더).
- **감수한 비용**: 코얼레싱은 "모든 입력이 실행된다"는 단순한 계약을 깬다. 대신 gotchas에 규칙으로 박제해(2026-05-20 Move coalescing) 팀 전체가 같은 전제를 공유한다.

### 결정 4 — 스냅샷은 무상태 full-rebuild, edge는 일회성 이벤트로 이원화

- **왜**: 지속 상태(위치/HP/쿨다운)와 순간 사건(투사체 스폰/히트/킬)은 손실·중복 특성이 다르다.
- **스냅샷**: `CSnapshotBuilder::Build`가 매 틱, 세션마다 Transform 보유 전 엔티티를 **netId 오름차순 정렬**해 전체 상태를 새로 빌드한다. 세션별로 다른 것은 `yourNetId`와 `lastAckedSeq`뿐이다 (`Server/Private/Game/SnapshotBuilder.cpp:106-138`, `GameRoomReplication.cpp:148-159`). 델타/베이스라인 압축과 AOI가 없어 비용이 엔티티수×세션수로 늘지만, **무상태라 재접속·late-join이 스냅샷 하나로 완결**되고 per-클라 ack 상태 관리가 필요 없다. MVP 단계에서 옳은 단순화라고 판단했다.
  - 정렬은 미관이 아니다: netId 순서 고정이 리플레이 재현성과 클라 보간 안정성의 전제다.
- **이벤트**: `Phase_BroadcastEvents`는 (1) `ReplicatedActionComponent`를 `lastBroadcastActionSeq`와 비교해 **새 시퀀스만** ActionStart로 방송하고, (2) `ReplicatedEventComponent`를 단 일회성 이벤트 엔티티를 직렬화 후 **즉시 DestroyEntity**, 투사체 파괴 이벤트면 방송 직후 netId를 Unbind해 회수한다 (`Server/Private/Game/GameRoomReplication.cpp:78-121`).
- **클라 방어**: TCP 재전송/중복 대비로 `BuildCueKey(effectId,src,tgt,startTick,flags)`를 FNV-1a로 해싱해 seen-set으로 dedup(4096 초과 시 clear)하고, ActionStart는 `IsNewerActionSeq` 단조 증가 비교로 오래된 재생을 막는다 (`Client/Private/Network/Client/EventApplier.cpp:170-176,428-434,865-876`).
- **리플레이 보너스**: `Phase_BroadcastSnapshot`이 방송과 별개로 `yourNetId=NULL, ack=0`인 중립 스냅샷을 한 번 더 빌드해 방송 바이트 그대로 .wrpl에 기록한다 (`GameRoomReplication.cpp:126-140`). 리플레이 재생기는 라이브와 동일한 SnapshotApplier/EventApplier 경로를 재사용하므로 별도 재생 코드가 없다.

### 결정 5 — FlatBuffers + verify를 '신뢰 경계'로: 실패는 절대 조용히 버리지 않는다

- **왜**: 스키마 드리프트나 변조 패킷을 bare return으로 삼키면, 증상이 "네트워크가 멈췄다"와 구분 불가능해진다 — 실제로 이 함정을 gotchas(2026-07-09 authoritative packets)로 박제했다.
- **서버 쪽**: `DispatchCommandBatch/LobbyCommand`는 실행 전 `flatbuffers::Verifier`를 통과해야 하고, 실패 시 `FlagSuspicious()`로 세션 의심도를 올린다 (`Server/Private/Network/PacketDispatcher.cpp:71-79`). 프레임 파서의 Invalid(마법값/버전/64KB 상한 위반)는 즉시 연결 절단이다 (`Server/Private/Network/FrameParser.cpp:23-32`, `PacketDispatcher.cpp:44-48`).
- **클라 쪽**: Hello/Snapshot/Event 모든 진입점이 verify 실패를 bounded(8회) trace로 남긴다 (`Client/Private/Network/Client/SnapshotApplier.cpp:561-573`).
- **데이터 버전까지**: Hello에 `dataBuildHash`를 실어 클라 `ChampionGameDataDB::GetBuildHash()`와 대조한다. 서로 다른 생성 데이터로 빌드된 조합이 "미묘하게 다른 수치"가 아니라 **접속 시점의 명시적 로그**로 드러난다. 필드를 프로토콜 끝에 추가하고 0=구버전 검사 생략으로 하위호환을 지켰다 (`SnapshotApplier.cpp:503-519`, `Shared/GameSim/Definitions/DataPackManifest.h:17`).
- **감수한 비용**: verify는 매 패킷 CPU를 쓴다. 신뢰 경계(클라→서버, 서버→클라 진입점)에서만 수행하고 내부 계층에서는 반복하지 않는 것으로 비용을 한정했다.

### 결정 6 — TCP 단일 MVP 먼저, UDP는 헤더 설계까지만

- **현재**: IOCPCore는 `SOCK_STREAM/IPPROTO_TCP` 하나만 열고 모든 트래픽이 TCP를 탄다. 지연 최소화로 서버 accept와 클라 connect 양쪽에 `TCP_NODELAY`를 건다 (`Server/Private/Network/IOCPCore.cpp:251`, `Client/Private/Network/Client/ClientNetwork.cpp:134`).
- **왜 TCP 먼저**: 순서 보장·재전송을 커널이 대신해 주면, 나는 결정론·프레이밍·스냅샷·verify라는 **본질 문제에 먼저 집중**할 수 있다. UDP 신뢰 계층은 그 자체로 큰 프로젝트다.
- **UDP 준비 상태**: 설계 문서(`.md/논문/10_Server_네트워크.md`)는 TCP 제어면(로비/밴픽) + UDP 데이터면(인게임) 분리를 전제하고, 와이어 포맷은 이미 코드로 존재한다 — `UdpPacketHeader` v2, 24바이트, 채널 3종(ReliableOrdered/ReliableUnordered/UnreliableSequenced), `ackSeq+ackBitfield`(32틱 선택적 ack), MTU 1200, Fragment 플래그 (`Shared/Network/UdpPacketHeader.h`).
- **감수한 비용**: TCP head-of-line blocking — 패킷 하나 유실이 이후 스냅샷 전부를 지연시킨다. 30Hz 저주파 + LAN/근거리 환경 MVP에서는 허용했고, 스냅샷이 무상태 full-rebuild라 "최신 것만 쓰면 되는" 구조여서 UDP 전환 시 UnreliableSequenced 채널에 그대로 얹을 수 있다.

### 클라이언트 수신 구조 (보조 결정)

클라는 blocking recv 전용 스레드가 바이트를 누적해 프레임 단위로 잘라 뮤텍스 큐에 넣고, 메인스레드가 `PumpReceivedFrames`로 **swap-드레인**해 콜백을 호출한다 — 수신과 적용을 스레드 경계에서 분리해 게임 스레드가 소켓을 만지지 않는다 (`Client/Private/Network/Client/ClientNetwork.cpp:191-257`). Send는 "입력 시스템 단일 스레드 가정"의 단순 blocking send로 두고 주석에 MVP임을 명시했다(:167).

---

## ④ 어려웠던 점과 해결

### War story 1 — 로컬 이동 yaw 예측 보호: "ack를 믿으면 안 된다"

**증상**: 우클릭 이동 시 클라가 즉시 몸통을 돌려주는 예측을 넣었더니, 서버 스냅샷이 도착하는 순간 캐릭터가 이전 방향으로 홱 돌아갔다가 다시 도는 튐이 생겼다.

**1차 실패**: "스냅샷의 `lastAckedCommandSeq`가 내 Move seq를 지나면 예측을 버리고 서버를 따르자"로 구현했다. 그런데 **ack가 진전됐다는 것은 서버가 명령을 실행했다는 뜻이지, 그 결과 yaw가 이번 스냅샷에 반영됐다는 뜻이 아니다**. 서버 회전이 한두 틱 늦게 실리므로 여전히 튀었다(gotchas 2026-05-20 local yaw prediction).

**해결 — 상태기계**: 보호 해제를 "서버가 실제로 따라잡았는가"로 판정한다 (`Client/Private/Network/Client/SnapshotApplier.cpp:669-699`):
- `bServerCaughtProtectedYaw`: 서버 yaw가 예측 yaw의 0.20rad 이내 → 해제.
- `bServerActionLocked`: 죽음/공격 액션 플래그 → 액션이 이기므로 해제.
- `bProtectedAckGraceExpired`: ack가 커버된 뒤에도 최대 12스냅샷(`kLocalMoveYawMaxProtectedSnapshots`)까지만 기다리고 만료 → 서버 권위 복귀 (안전망).
- `bServerOpposesProtectedYaw`: 서버 yaw가 예측과 **정확히 반대(half-turn, 허용오차 0.35rad)**면 grace 만료를 유예 — 정반대 값은 "따라잡는 중"이 아니라 축 보정 문제일 가능성이 높아서 보호를 유지한다.

**연속 상태 규칙**: Transform yaw write는 항상 `MakeChampionVisualYawNear`로 현재값 근처 정규화한다(:694). 매 write마다 [-π,π]로 wrap하면 빠른 우클릭에서 +π/-π 경계를 재교차하며 한 바퀴 도는 버그가 났었다. Normalize는 wire/로그/델타 비교 전용이다(gotchas 2026-05-21 yaw ownership).

**서버 쪽 협조**: 스냅샷의 Moving 플래그도 단순 MoveTarget 유무가 아니다 — 스킬 액션 중이면 `IsMoveLockedBySnapshotAction`이 액션락 틱을 재계산해 이동 플래그를 억제한다 (`Server/Private/Game/SnapshotBuilder.cpp:80-98`). 클라 yaw 보호와 서버 플래그가 한 세트로 설계됐다.

### War story 2 — 클라 시뮬이 스냅샷을 덮어쓴 사고 → 프레젠테이션 강등

yaw 튐을 쫓다 보니 범인 중 하나는 예측 로직이 아니라 **클라에 남아 있던 CNavigationSystem**이었다. 스냅샷이 적용한 yaw를 같은 프레임의 클라 내비게이션이 다시 덮어썼다. 개별 패치 대신 결정 1의 시스템 게이팅으로 클라 시뮬 자체를 제거했고, "movement가 step-like해지면 로컬 내비를 다시 켜지 말고 보간/예측을 고쳐라"를 gotchas에 박제했다(2026-05-22).

### War story 3 — 보간 순서 의존성: 캡처는 반드시 스냅샷 적용 '전'

55ms 보간(`kNetworkActorInterpDurationSec = 0.055f`)의 start 위치는 "스냅샷이 덮어쓰기 직전의 위치"여야 한다. 그래서 `OnUpdate` 순서가 엄격하다: **① CaptureNetworkActorInterpolationStarts → ② PumpNetwork(스냅샷 적용) → ③ 새 serverTick이면 BeginNetworkActorInterpolation → … → ApplyNetworkActorInterpolation(dt)** (`Client/Private/Scene/Scene_InGame.cpp:745-809`). 캡처를 적용 뒤에 두면 start==target이 되어 보간이 무효가 된다.

예외 처리도 명시적이다 (`Client/Private/Scene/Scene_InGameNetwork.cpp:694-748`):
- 이동거리² ≥ 9(약 3m) → 텔레포트로 간주, 즉시 스냅.
- 미세변화(0.0001m²·0.0005rad 이하) → 보간 생략.
- 로컬 칼리스타 패시브 대시 중 → 보간 억제(로컬 예측이 이김).
- 외삽(dead-reckoning) 대신 스냅샷-투-스냅샷 보간을 택해 서버와의 위치 발산을 원천 차단했다. 대가는 55ms의 추가 표시 지연이다.

30Hz 스냅샷만으로 애니를 몰기 위한 로코모션 합성도 같은 파일에 있다: 위치 델타(1cm 임계) + 서버 Moving 플래그 + 포즈를 합성하고 `kMoveHoldSec = 0.12f` grace hold로 스냅샷 사이 애니 튐을 잡는다(:809-864).

### War story 4 — raw click intent 보존

초기에 클라가 보정한(walkable로 스냅한) 목표를 Move 명령에 실어 보냈더니, 플레이어의 원래 클릭 의도가 서버에 도달하기 전에 지워졌다. 지금은 `IssuePlayerMoveTarget`이 걷기 가능 검증만 하고 **SendMove에는 원시 클릭 XZ(`moveIntent = ground`)를 그대로 싣는다** — 경로 해석은 서버 nav의 소유다 (`Client/Private/Scene/Scene_InGameLocalSkills.cpp:1297-1366`). 예측 기록은 `RecordNetworkMovePrediction`으로 deque(최대 64)에 쌓고 스냅샷 ack가 오면 앞에서부터 prune한다 (`Client/Private/Scene/Scene_InGameNetwork.cpp:1157-1190`).

### War story 5 — 로비 세션 이어받기와 Hello 도착 레이스

BanPick 로비와 인게임은 같은 TCP 세션을 공유한다. 씬 진입 시 네트워크 로스터 모드 + 이미 연결돼 있으면 새 소켓을 만들지 않고 로비 세션을 그대로 물려받는다(`m_bUsingSharedNetwork`, `Client/Private/Scene/Scene_InGameNetwork.cpp:617-656`). 문제는 순서였다: 서버의 Hello(내 netId/챔피언/팀)가 **씬이 게임 프레임 콜백을 등록하기 전에** 도착할 수 있다. 해법은 세션 클라이언트가 마지막 Hello 페이로드를 캐시해 두고, 씬이 콜백 등록을 마친 뒤 `ReplayLastHelloToGameFrameCallback`으로 재생하는 것이다(`Scene_InGameNetwork.cpp:659-663`). "패킷 도착 시점과 리스너 등록 시점은 독립"이라는 비동기 통신의 고전적 레이스를, 드롭이 아니라 캐시-재생으로 푼 사례다.

### 스냅샷 적용의 부분 수거 정책

`OnSnapshot`의 마지막 단계는 "이번 스냅샷에 없는 netId" 수거인데, **미니언/Viego소울/칼리스타센티넬/와드만** DestroyEntity하고 챔피언·구조물은 유지한다 (`Client/Private/Network/Client/SnapshotApplier.cpp:1426-1461`). 챔피언은 리스폰이 있고, 구조물은 스테이지 로드 때 배치된 비주얼에 netId를 근접 매칭으로 바인딩한 것이라 파괴하면 안 되기 때문이다. "서버가 안 보낸 것 = 다 지운다"는 순진한 규칙이 도메인에서는 틀린다는 사례로 자주 인용한다.

---

## ⑤ 향후 개선 방향

1. **UDP 데이터면**: `UdpPacketHeader`(채널/ack bitfield/MTU 1200)는 설계 완료. 스냅샷을 UnreliableSequenced로, 이벤트를 ReliableUnordered로 옮기고 TCP는 로비/제어면에 남긴다.
2. **델타 압축 + AOI**: full-snapshot의 선형 비용이 엔티티/세션 증가의 병목. netId 정렬이 이미 있으므로 baseline-diff 도입이 자연스럽다.
3. **랙 보상 적용**: `CLagCompensation`은 200ms(6틱) 링버퍼 + generation 검증까지 **기록은 라이브**지만, 명령은 `cmd.rewindTicks = 0`으로 고정돼 되감기 히트판정은 꺼져 있다 (`Server/Private/Security/LagCompensation.cpp`, `Server/Private/Game/GameRoomCommands.cpp:26`). "엄폐 뒤에서 맞는" 공정성 트레이드오프라 정책 결정과 함께 신중히 켤 항목.
4. **송신 백프레셔**: single-outstanding 큐의 무제한 성장 방지 — 큐 길이 상한 + 스냅샷 최신본 교체(오래된 스냅샷 드롭) 정책.
5. **압축/암호화**: `PacketFlag_Compressed/Encrypted` 배선.
6. **멀티코어 sim**: 현재 룸당 단일 틱 스레드 + `m_stateMutex` 단일 락. 룸 수 스케일은 되지만 룸 내부 병렬화는 JobSystem 통합이 필요하다.

미완을 숨기지 않는 것이 원칙이다 — "인프라는 완성, 정책은 유보" 상태(랙 보상)와 "설계만 존재"(UDP)를 구분해 말한다.

---

## ⑥ 면접 Q&A

### Q1. 왜 서버 권위를 선택했나?
**답변 골격**: truth가 두 곳이면 반드시 발산한다. 서버가 위치/HP/쿨다운/판정을 전부 소유하고, 클라는 명령 생산 + 스냅샷 소비 + 연출만 한다. 봇 AI조차 truth를 직접 수정하지 않고 사람과 같은 GameCommand를 생산해 동일 검증 경로를 통과한다. 클라에서는 시뮬 시스템 등록 자체를 모드별로 게이팅해 구조로 강제했다.
**꼬리질문 대비**: "지연은 어떻게?" → 55ms 보간 + yaw 제한 예측 + Move 코얼레싱. "롤백 넷코드와 비교하면?" → 격투게임처럼 입력이 작고 결정론이 완전하면 롤백이 좋지만, MOBA는 관전자/리플레이/치트 방지 요구가 커서 서버 권위가 업계 표준이다.

### Q2. IOCP 서버 구조를 설명해 달라.
**답변 골격**: CreateIoCompletionPort로 워커 concurrency 설정 → WSAIoctl로 AcceptEx 함수 포인터 로드 → AcceptEx 4개 pre-post → 워커가 GetQueuedCompletionStatus로 깨어나 CONTAINING_RECORD로 IOContext 복원, completion key의 sessionId로 세션 라우팅 → Accept 완료 시 SO_UPDATE_ACCEPT_CONTEXT·TCP_NODELAY 설정 후 다음 AcceptEx 재게시. 종료는 워커 수만큼 nullptr overlapped를 post해 깨운다.
**꼬리질문 대비**: "왜 pre-post 4개?" → accept 폭주 시 backlog 대기 없이 즉시 수용할 슬롯 확보. "select/epoll 대비?" → Windows에서 완료 기반(proactor) 모델이 시스템콜 횟수와 스레드 웨이크업 면에서 유리.

### Q3. 비동기 I/O에서 세션 수명은 어떻게 안전하게 관리했나?
**답변 골격**: shared_ptr만으로는 부족하다 — OVERLAPPED가 커널에 떠 있는 동안 소켓/버퍼가 살아 있어야 한다. 매 비동기 호출 전 pending-IO 원자 카운트 증가, 완료 시 감소. 끊긴 세션은 closing 리스트로 옮기고 closesocket으로 걸린 I/O를 실패 완료시켜 카운트를 0으로 유도, reap 단계에서 카운트 0인 것만 파괴한다. Find는 closing까지 검색해 in-flight 완료가 세션을 찾게 한다.
**꼬리질문 대비**: "그냥 shared_ptr을 IOContext에 넣으면?" → 가능한 대안이지만 완료 라우팅이 sessionId 키 기반이라 카운트 방식이 소유권 흐름을 단순하게 유지한다.

### Q4. TCP 스트림을 어떻게 메시지로 잘랐나?
**답변 골격**: 16바이트 고정 헤더(magic/version/type/flags/payloadSize/sequence, static_assert로 크기 박제) + 길이 접두 프레이밍. 파서는 NeedMore/Complete/Invalid 3상태 — 헤더 미만이면 대기, magic/version 불일치나 64KB 초과면 버퍼 clear 후 Invalid, Invalid를 받은 디스패처는 즉시 연결 절단. "한 recv에 여러 프레임 / 한 프레임이 여러 recv" 양쪽을 드레인 루프로 처리한다.
**꼬리질문 대비**: "왜 절단이 기본?" → 스트림 정렬이 한 번 깨지면 이후 모든 바이트 해석이 불신 대상. 재동기화 시도보다 재접속이 안전하고, 무상태 스냅샷 덕에 재접속 비용이 싸다.

### Q5. 클라 예측과 서버 권위가 충돌하면 어떻게 하나?
**답변 골격**: yaw 예측 보호 상태기계로 답한다. 핵심 교훈은 "ack 진전 ≠ 결과 반영"이다. 서버 yaw가 예측을 0.20rad 이내로 실제로 따라잡거나, 액션락이 걸리거나, ack 이후 12스냅샷 grace가 만료될 때만 보호를 해제한다. 서버 yaw가 정확히 반대 방향(half-turn)이면 특수 처리한다. Transform yaw는 연속 상태라 write마다 현재값 근처 정규화(YawNear)를 쓰고, [-π,π] 정규화는 wire/비교 전용으로 분리했다.
**꼬리질문 대비**: "위치도 예측하나?" → 아니다. 위치는 스냅샷-투-스냅샷 보간만 하고, 예측은 체감 대비 리스크가 작은 yaw와 칼리스타 대시 같은 국소 케이스에 한정했다.

### Q6. 스냅샷을 어떻게 만들었고, 왜 델타 압축을 안 했나?
**답변 골격**: 매 틱 세션별 full-rebuild, 엔티티는 netId 오름차순 정렬, 세션별 차이는 yourNetId와 ack뿐. 무상태라 재접속/late-join이 스냅샷 하나로 완결되고, per-클라 baseline 관리가 없어 버그 표면이 작다. 대역폭·CPU가 선형 증가하는 것은 MVP 스케일에서 수용했고, 델타/AOI는 명시적 다음 단계다.
**꼬리질문 대비**: "델타로 가면 뭐가 어려운가?" → 클라별 마지막 ack된 baseline 추적, 유실 시 재동기화 경로, 그리고 UDP 전환과 결합될 때의 순서 문제. netId 정렬 순회가 이미 있어 diff 자체는 저렴하다.

### Q7. 신뢰할 수 없는 클라 패킷을 어떻게 방어하나?
**답변 골격**: 다층 게이트 — ① 프레이밍(magic/version/크기, 위반 시 절단) ② FlatBuffers verify(실패 시 silent drop이 아니라 의심도 증가 — 스키마 드리프트와 네트워크 스톨을 구분하기 위해) ③ 세션별 시퀀스 수용(과거 거부, +60 점프 의심) ④ sim 내부의 쿨다운/사거리/상태 검증. 추가로 Hello에서 데이터 빌드 해시를 대조해 서버/클라 데이터 버전 불일치를 접속 시점에 가시화한다.
**꼬리질문 대비**: "verify 비용은?" → 신뢰 경계 진입점에서 1회만. 내부 계층은 이미 검증된 버퍼를 신뢰한다.

### Q8. 순간 이벤트(FX·킬)와 지속 상태를 왜 나눴나?
**답변 골격**: 상태는 "최신본만 있으면 되는" 멱등 데이터고, edge는 "정확히 한 번 재생"이 필요한 데이터라 복제 특성이 다르다. 서버는 edge를 일회성 이벤트 엔티티로 만들어 방송 후 즉시 파괴하고, 클라는 FNV-1a cue-key dedup + actionSeq 단조성 검사 + 투사체 비주얼 수명 추적 3중 장치로 중복 재생을 막는다.
**꼬리질문 대비**: "UDP로 가면?" → 이벤트는 Reliable 채널, 스냅샷은 Unreliable Sequenced — 이 이원화가 채널 분리와 정확히 대응되도록 미리 설계했다.

### Q9. 게임 중 연결이 끊긴 플레이어는 어떻게 복귀시키나?
**답변 골격**: 인게임 중 이탈 시 슬롯을 비우지 않고 sessionId=0으로 예약 상태를 유지한다. 새 세션이 붙으면 끊긴 human 슬롯을 찾아 slot의 netId → EntityIdMap.FromNet으로 **살아있는 챔피언 엔티티를 되찾아** 세션↔엔티티를 재바인딩하고 Hello를 재송신한다 (`Server/Private/Game/GameRoomLobby.cpp:346-374`). 엔티티를 파괴하지 않는 이유는 서버 권위 상태(레벨/골드/위치)가 그 엔티티에 있기 때문이고, 무상태 스냅샷 덕에 복귀 클라는 다음 스냅샷 하나로 전체 상태를 복원한다.
**꼬리질문 대비**: "재접속 남용(강제 끊기)은?" → 현재는 정책 없음. 재접속 쿨다운/횟수 제한이 자연스러운 확장점.

### Q10. 지금 네트워크 스택의 가장 큰 한계는?
**답변 골격**: 정직하게 셋을 든다 — ① TCP 단일이라 head-of-line blocking(UDP 데이터면은 헤더 설계까지 완료) ② full-snapshot의 선형 비용(델타/AOI 미구현) ③ 랙 보상이 기록만 되고 rewindTicks=0으로 적용 유보. 구현된 것과 설계만 된 것을 구분해서 말하는 것 자체가 이 프로젝트의 문서 원칙이다.

---

### 부록 — 면접에서 바로 인용할 수 있는 수치 (전부 코드 검증됨)

| 항목 | 값 | 위치 |
|---|---|---|
| 서버 틱 주기 | 33,333µs (30Hz), sleep_until 드리프트 보정 | `Server/Private/Game/GameRoomTick.cpp:72` |
| 패킷 헤더 | 16B 고정, magic 0x5742 'WB' | `Shared/Network/PacketEnvelope.h:9,46` |
| 프레임 payload/버퍼 상한 | 64KB / 256KB (초과 시 Invalid/Clear) | `Server/Public/Network/FrameParser.h:29-30` |
| 시퀀스 점프 의심 임계 | last+60 초과 | `Server/Private/Network/Session.cpp:34` |
| AcceptEx pre-post | 4개 | `Server/Private/Network/IOCPCore.cpp:123` |
| 랙 보상 이력 | 6틱=200ms 링버퍼, 적용은 rewindTicks=0 유보 | `Server/Private/Security/LagCompensation.cpp`, `Server/Private/Game/GameRoomCommands.cpp:26` |
| 액터 보간 | 55ms SmoothStep, 텔레포트 임계 3m(9.f²) | `Client/Private/Scene/Scene_InGameNetwork.cpp:133-134` |
| 로코모션 grace | 이동 판정 1cm 임계, 0.12s hold | `Scene_InGameNetwork.cpp:811-812` |
| yaw 보호 | 최대 12스냅샷, catch 0.20rad, half-turn 허용오차 0.35rad | `Client/Private/Network/Client/SnapshotApplier.cpp:80-81,674` |
| 예측 deque | Move 예측 최대 64, ack 시 prune | `Scene_InGameNetwork.cpp:1171` |
| 이벤트 dedup | FNV-1a cue-key, seen-set 4096 초과 시 clear | `Client/Private/Network/Client/EventApplier.cpp:428,872` |
| UDP 헤더(미가동) | v2, 24B, 채널 3종, ack bitfield, MTU 1200 | `Shared/Network/UdpPacketHeader.h` |

---

## ⑦ 다른 챕터와의 연결

- **결정론 시뮬레이션(Shared GameSim)**: 이 챕터의 "명령 정렬·netId 정렬·고정 30Hz"는 결정론 3축(고정 dt / 파생 시드 RNG / 정렬 순회) 위에서만 의미가 있다. `TickContext`가 RNG·EntityIdMap·정의 팩을 모든 시스템에 주입하는 구조는 GameSim 챕터가 소유한다.
- **스킬/챔피언 아키텍처**: `HandleCastSkill`의 검증→상태변경→훅 디스패치→**복제 이벤트 방출** 4단 구조 중 마지막 단이 이 챕터의 이벤트 파이프라인으로 이어진다. 판정(서버)과 연출(EventApplier→VisualHookRegistry)의 분리도 여기서 합류한다.
- **클라이언트 씬 구조**: Scene_InGame의 책임별 TU 분할에서 "snapshot-apply(Network)와 local prediction(LocalSkills)을 절대 같은 파일에 두지 않는다"는 규칙이 이 챕터의 예측/권위 경계를 파일 구조로 표현한 것이다.
- **데이터 아키텍처**: Hello의 `dataBuildHash` 핸드셰이크는 데이터 3분할(SharedContract/ServerPrivate/ClientPublic)과 cook 파이프라인의 drift 감지 장치다.
- **봇 AI**: 봇이 GameCommand만 생산해 사람과 동일 인제스트/실행 경로를 타는 구조, 그리고 AI 결정 트레이스를 스냅샷에 실어 클라 ImGui로 시각화하는 폐루프 디버깅은 이 챕터의 복제 채널을 재사용한다.
- **에러 처리 정책**: "verify 실패 silent drop 금지", bounded 카운터 로그, 실패=std::cerr/게이트 래퍼 구분은 `WINTERS_ERROR_HANDLING_POLICY.md` 챕터의 전 계층 규약이 네트워크 경계에 적용된 사례다.
