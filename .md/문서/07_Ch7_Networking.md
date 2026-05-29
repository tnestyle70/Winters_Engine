# Ch7. Networking (IOCP / AOI / Replication / Anti-cheat / Replay)

> Winters 현재: `Server/Public/Network/` IOCP 일부, `Shared/Network/PacketDef` + Snapshot, `Shared/Replay/` R0 골격.
> 사용자 master plan: **Fiber × IOCP 통합** (memory: `project_fiber_mastery_session_2026_05_11.md`).
> 레퍼런스: `UnrealEngine/Engine/Source/Runtime/Net/`, `Engine/Plugins/Runtime/ReplicationGraph/`, `Runtime/Experimental/Iris/`.

---

## 1. 기초 원리 — Network는 거짓말 잘 해야 한다

물리적 진실:
- 네트워크는 **느리다** (한국 ↔ 미국 ~150ms RTT)
- 네트워크는 **불안하다** (패킷 손실 0.1~5%)
- 네트워크는 **차례를 안 지킨다** (순서 뒤바뀜)
- 대역폭은 **한정**된다 (집 회선 10~100Mbps)

플레이어 인식:
- 100ms 안에 반응 없으면 "랙"
- 화면이 멈추면 "버그"
- 다른 플레이어와 다르게 보이면 "치트"

이 거리를 메우는 4가지 거짓말:

1. **Client prediction** — 클라가 결과를 추측해서 미리 보여줌. 서버 답 오면 보정.
2. **Server reconciliation** — 서버가 모든 진실. 클라 예측이 틀리면 rewind.
3. **Interpolation** — 다른 플레이어는 100ms 뒤로 보여주고 부드럽게.
4. **Lag compensation** — "쐈을 때 화면에 보이던 위치"로 hit 판정.

---

## 2. 핵심 — 5가지 시스템

### 2.1 Transport: TCP vs UDP

| | TCP | UDP |
|--|-----|-----|
| 순서 보장 | O | X (직접 처리) |
| 신뢰성 | O | X (직접 처리) |
| Head-of-line blocking | O (한 패킷 늦으면 전부 대기) | X |
| 적합 | 로그인/매칭/상점 | 게임 패킷 |

게임 자체는 **UDP + reliable layer (custom)**. 매칭/상점은 TCP/HTTPS.

UE5의 옛 `NetDriver`는 UDP 기반 자체 reliable. 최근 `Iris`는 압축/우선순위 재설계.

### 2.2 IOCP (Windows) / epoll (Linux) / kqueue (BSD)

수만 connection을 single thread로 다루는 OS proactor.

```cpp
// IOCP 흐름 (Server)
HANDLE iocp = CreateIoCompletionPort(...);
for each new connection:
    AcceptEx + CreateIoCompletionPort(socket, iocp, ...)
worker threads:
    while (GetQueuedCompletionStatus(iocp, ...))
        process completed I/O event
```

**Fiber × IOCP 통합** (Winters 사용자 master plan):
- 한 connection = 한 fiber
- IO 완료 시 fiber 깨움
- 동기 코드처럼 보이는 비동기 흐름
- 컨텍스트 스위치 비용 < OS thread

```cpp
// Winters server 의사 코드 (사용자 master plan)
fiber_loop(connection) {
    while (alive) {
        Packet p = co_await recv();   // IOCP 완료 시 자동 resume
        Handle(p);
        co_await send(reply);
    }
}
```

### 2.3 Snapshot Replication

UE5와 옛 LoL 방식: **서버가 매 N ms마다 actor 상태 스냅샷을 보냄**.

```text
Snapshot t=0:
  Actor 1: pos(10,0,5), hp=100, mp=50
  Actor 2: pos(20,0,5), hp=80,  mp=20
  ...

Snapshot t=33ms (delta):
  Actor 1: pos(11,0,5)  ← 변경만
  Actor 2: hp=75        ← 변경만
```

문제: actor 1000개에 plyr 50명 = 50000 채널. 매 tick 1000 actor × 50 conn = 50K 비교.

해결책: **Replication Graph** (아래).

### 2.4 Replication Graph

`UnrealEngine/Engine/Plugins/Runtime/ReplicationGraph/Source/Public/ReplicationGraph.h:13~27`:

```cpp
/*
High level overview of ReplicationGraph:
    * The graph is a collection of nodes which produce replication lists for each network connection.
      The graph essentially maintains persistent lists of actors to replicate and feeds them to connections.

    * This allows for more work to be shared and greatly improves the scalability of the system with
      respect to number of actors * number of connections.

    * For example, one node on the graph is the spatialization node. All actors that essentially use
      distance based relevancy will go here. There are also always relevant nodes. Nodes can be global,
      per connection, or shared (E.g, "Always relevant for team" nodes).

    * Instead there are essentially three ways for game code to affect this part of replication:
        * The graph itself. Adding new UReplicationNodes or changing the way an actor is placed in the graph.
        * FGlobalActorReplicationInfo: The associative data the replication graph keeps, globally, about each actor.
        * FConnectionReplicationActorInfo: The associative data that the replication keeps, per connection, about each actor.
*/

UCLASS(MinimalAPI, abstract, transient, config=Engine)
class UReplicationGraphNode : public UObject
{
    GENERATED_BODY()
public:
    UE_API UReplicationGraphNode();
    virtual void NotifyAddNetworkActor(const FNewReplicatedActorInfo& Actor) PURE_VIRTUAL(...);
    // ...
};
```

**핵심 아이디어**: actor가 "어느 노드"에 속하느냐로 replicate priority 결정.

```text
Replication Graph nodes:
   AlwaysRelevant         (게임 상태, 매치 정보)
   AlwaysRelevantForTeam  (팀 정보)
   Spatialization         (위치 기반 — grid + AOI)
   Dormancy               (안 움직이는 actor)
   ConnectionGraphPlayer  (각 conn의 플레이어 본인 우선)
```

매 tick 각 connection은 graph를 traverse → relevant actor list → priority sort → bandwidth budget 내에서 보냄.

### 2.5 Iris (UE5 next-gen)

`Runtime/Net/Iris/Core/...` — replication을 **데이터 oriented** 재설계.
- 비트 단위 패킹
- 우선순위/dormancy를 graph 없이 component flag로
- Mass / large scale 지원

Winters는 일단 ReplicationGraph 등가로 시작하고 Iris 패턴은 메모.

---

## 3. 심화

### 3.1 AOI (Area of Interest)

플레이어가 보는 영역 안 actor만 replicate.

```text
Grid:                BVH:
+-+-+-+-+           AOI radius 100m
|.|.|P|.|           → BVH query → 안에 있는 N개만
|.|x|x|.|
+-+-+-+-+
P = 플레이어, x = 가시
```

LoL 5v5는 AOI 거의 안 씀 (10 player + 30 minion + 8 turret). GTA6/MMO는 필수.

Winters: `Server/Public/Network/AOIManager.h` 추가 권장 (Ch7 Stage 5+).

### 3.2 Client Prediction + Server Reconciliation

```text
t=0   클라: 이동 입력 (frame 100). 즉시 캐릭 이동 (예측).
            서버에 (frame 100, input) 전송.
t=50  서버: frame 100 input 처리. 새 위치 계산. snapshot 전송.
t=100 클라: snapshot 수신. 자신의 frame 100 예측과 비교.
            예측 == 진실: OK.
            예측 != 진실: rewind to frame 100, replay frame 100~current with corrected state.
```

UE5: `UCharacterMovementComponent::ClientUpdatePositionAfterServerUpdate`.

### 3.3 Lag Compensation

```text
플레이어 A의 입장:
  t=0    플레이어 B가 (10,0,0)에 보임 (interpolated, 100ms delayed)
  t=0    A가 발사. server에 (t=0, ray) 전송.

서버:
  t=0+RTT/2  도착. A의 화면에서 B는 100ms 전 위치였으니
             100ms 전 simulation snapshot을 rewind → ray 판정.
             맞으면 hit. 현재 시점에 데미지 적용.
```

UE5: `Engine/Source/Runtime/Engine/Public/Engine/NetworkObjectList.h` + lag compensation history.

### 3.4 Anti-cheat

- **Server sanity**: 속도/위치 jump / 쿨다운 위반 / 명백히 invalid input 검출
- **Timing check**: 클라 frame > N fps 위반
- **Binary integrity**: 게임 본체 hash 검증
- **Process scan**: BattlEye / Easy Anti-Cheat 같은 kernel-level 검사

ClientInput → ServerCommand 변환 시점에 sanity 1차. 통과해도 server simulation에서 결과가 invalid면 reject.

Winters AGENTS.md 박제: **Bot AI = GameCommand producer**. 즉 AI도 사람 input과 같은 경로를 거친다. anti-cheat 적용 동일.

### 3.5 Replay

`Shared/Replay/` 골격에 박힘. R0~R3 계획:
- R0: 로컬 저장 (snapshot + event timeline)
- R1: 서버 recorder
- R2: backend ingest / download
- R3: user replay library

UE5: `Source/Runtime/Engine/Classes/Engine/DemoNetDriver.h`. 네트워크 패킷을 그대로 파일에 기록 → 재생은 같은 패킷을 클라가 다시 소비.

Winters는 snapshot+event(`Shared/Network/Snapshot.fbs` 등) 기준이라 같은 접근 가능.

### 3.6 Reliable / Ordered / Unordered

UDP 위에 reliable layer:

```cpp
enum class eChannelMode {
    Unreliable,         // 한 번만 시도, drop 무시 (position snapshot)
    UnreliableSequenced,// 순서 보장, drop 무시 (가장 최신만 의미)
    Reliable,           // 무조건 도착 (login/ability cast)
    ReliableOrdered,    // 도착 + 순서 (chat)
};
```

ENet, Steam Networking Sockets, GameNetworkingSockets 같은 라이브러리가 다 제공. UE5 자체 NetDriver도 채널 모델.

---

## 4. Winters 매핑

### 4.1 현재 상태

- `Server/Public/Network/`: IOCP 일부 진행 중 (사용자 Fiber master plan)
- `Shared/Network/PacketDef`: FlatBuffers 기반 스키마
- `Shared/Network/Snapshot*`: 서버 권위 snapshot
- `Shared/Replay/`: R0 골격

### 4.2 Ch7 추가/확장 헤더 (제안)

```cpp
// Server/Public/Network/IocpFiberServer.h  (사용자 master plan)
class CIocpFiberServer
{
public:
    bool Initialize(const ServerDesc& desc);
    void Run();
    void Shutdown();
private:
    HANDLE m_iocp;
    std::vector<FiberWorker> m_workers;
};

// Server/Public/Network/AOIManager.h
class CAOIManager
{
public:
    void RegisterEntity(EntityID id, const Vec3& pos);
    void UpdateEntity(EntityID id, const Vec3& pos);
    void Query(EntityID viewer, f32_t radius, std::vector<EntityID>& outVisible) const;
private:
    CSpatialGrid m_grid;   // 또는 BVH
};

// Server/Public/Network/ReplicationGraph.h
class CReplicationGraph
{
public:
    void AddNode(std::unique_ptr<IReplicationNode> node);
    void GatherActorsForConnection(ConnectionID conn, std::vector<EntityID>& out) const;

    // 노드 종류
    //   CRN_AlwaysRelevant
    //   CRN_AlwaysRelevantForTeam
    //   CRN_Spatialization (AOI)
    //   CRN_Dormancy
    //   CRN_ConnectionPlayer
};

// Shared/Network/DeltaCompression.h
void EncodeSnapshotDelta(const Snapshot& prev, const Snapshot& curr, BitWriter& out);
void DecodeSnapshotDelta(const Snapshot& prev, BitReader& in, Snapshot& out);

// Shared/Network/RollbackBuffer.h
class CRollbackBuffer
{
public:
    void Push(u32_t frame, const Snapshot& snap);
    bool Rewind(u32_t frame);
};

// Shared/Network/AntiCheat.h
struct AntiCheatVerdict { bool valid; const char* reason; };
AntiCheatVerdict ValidateGameCommand(const GameCommand& cmd, const PlayerState& state);
```

### 4.3 Bot AI 불변식 (재확인)

CLAUDE.md 2026-05-12 박제:
> Bot AI는 GameCommand producer일 뿐 gameplay 결과를 직접 수정하지 않는다.

이게 networking에서 더 중요해진다:
- AI Command는 사람 Command와 같은 anti-cheat 경로를 거친다 (단순 sanity만)
- AI Command는 같은 server simulation을 거친다
- AI 결과(피해/이동)는 같은 snapshot을 통해 클라에 도착
- AI는 replication graph의 entry가 아니다 (server-only)

### 4.4 단계별

```text
Ch7-Stage1  IOCP × Fiber 통합 ping-pong (현재 사용자 master plan)
Ch7-Stage2  Reliable UDP layer
Ch7-Stage3  Snapshot replication + delta compression
Ch7-Stage4  Client prediction + reconciliation
Ch7-Stage5  AOI manager (Ch3 streaming source 연동)
Ch7-Stage6  Replication Graph (always / spatial / dormancy node)
Ch7-Stage7  Lag compensation (server rewind)
Ch7-Stage8  Anti-cheat sanity layer
Ch7-Stage9  Replay R1~R3 (server recorder + backend + library)
Ch7-Stage10 Iris-tier 압축/우선순위 (large-scale 진입 시)
```

### 4.5 게임별 적용

| 게임 | 필요 Stage |
|------|-----------|
| LoL (현재) | Stage 1~4, 8, 9 |
| 로아 레이드 | Stage 1~7, 8, 9 |
| 엘든링 P2P (멀티 4인) | 별도 모델 (호스트 마이그) |
| GTA6 / MMO | Stage 1~10 + region routing |

---

## 5. 검증 명령

```powershell
# Server smoke
.\Server\Bin\Debug\WintersServer.exe --bind=0.0.0.0:7777 --maxconn=10000

# 기대 로그
# [Net] IOCP server up on 0.0.0.0:7777 workers=8
# [Net] connection accepted from 192.168.1.50:54321 (fiber#42)
# [Net] snapshot tx conn=42 actors=12 bytes=234 (delta from t=1233)
# [AOI] viewer=42 radius=100m visible=8 of 152 total
```

---

## 6. 다음 챕터로

Ch7 Stage 3까지 가야 **Ch8 GAS**의 cooldown/cost가 서버 권위로 동작. Ch7 Stage 5 (AOI)는 Ch3 (WorldPartition streaming source)와 묶여 진화.
