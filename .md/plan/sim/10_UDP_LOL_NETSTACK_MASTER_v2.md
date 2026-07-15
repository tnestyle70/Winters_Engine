# Phase Sim-10 v2 — UDP LoL-Authoritative NetStack Master Plan (Codex 보정)

> [!IMPORTANT]
> **Historical design.** 아래 본문을 현재 구현 상태로 사용하지 않는다. 최신 기준은 [2026-07-13 canonical implementation plan](../2026-07-13_UDP_JOB_SYSTEM_CHASE_LEV_FIBER_IMPLEMENTATION_PLAN.md)과 [S023 결과 보고서](../../build/2026-07-13_UDP_JOB_SYSTEM_CHASE_LEV_FIBER_RESULT.md)다.
> As-built delta: JobSystem Submit race, Chase-Lev deque, FiberFull 및 stress 구현은 완료되었고, UDP v3 generic vertical slice와 server hub/client facade가 구현되었다. main F5 통합과 최종 build 상태는 S023 결과 보고서를 따른다. 6주 Fiber mastery 프로그램은 미착수이며, 현재 상태는 production UDP cutover가 아니다.
> 이 문서의 UDP v2 수치인 **24 B header / 10 B fragment header / 1 MiB logical payload**는 historical design이다. 실제 v3 상수는 **40 B header / 16 B fragment header / 1,200 B datagram / 64 KiB logical payload**다.

> **2026-07-11 historical snapshot:** 당시의 “TCP-only / UDP 선언-only” 판정은 위 2026-07-13 as-built delta로 대체됐다. 아래 단계와 수치는 설계 진화 기록으로만 읽고, 현재 코드·검증·남은 production gate는 위 canonical plan과 S023 결과 보고서를 따른다.

**작성일**: 2026-04-30
**v1 → v2 변경 사유**: Codex 리뷰 7건 + 마일스톤 조정. 큰 축 (UDP/Reliability/Delta/AOI/LagComp/Prediction/Replay) 유지. 구현 안전성을 위해 인터페이스 정확화 + 결정성 가드 M1 부터 적용 + Encryption 별도 Phase.
**범위**: Server/Shared/Client 의 네트워크 + 결정성 + Snapshot/AOI + Prediction 전 영역
**전제**: ① v1.2 `04_IOCP_GAMEROOM.md` (TCP IOCP) 는 1차 prototype 으로 남기고, 본 계획서가 production 방향 ② Backend Phase 0-8 동작 ③ Shared/GameSim + Schemas 박제 완료 ④ Server 코드 sketch 상태

**한 줄 명제**: **TCP→UDP 전환 + 자체 Reliability Layer + Snapshot Delta + AOI + Lag Compensation + Client Prediction + 결정성 보강 = LoL 본체의 5%→30%. 6 마일스톤 (M1-M6), Encryption 은 별도 Sim-11. 결정성 가드는 M1 부터 적용.**

---

## 0. v1 → v2 변경 정리 (Codex 7건 보정)

| # | v1 | v2 |
|---|---|---|
| **C1** | `UdpPacketHeader.ackBase = last contiguous` + `ackBitfield = base 이전 32` | **`ackSeq = newest received`** + `ackBitfield = ackSeq 이전 32 수신 마스크` (selective ack 의미 명확) |
| **C2** | Fragment = `fragmentInfo:u16 (8/8)` 인-band | **별도 `UdpFragmentHeader`** (`messageId`, `fragmentIndex:u16`, `fragmentCount:u16`, `fragmentPayloadSize:u16`) + MTU payload 1200 byte |
| **C3** | `Snapshot.fbs` 에 `SnapshotDelta` table 단순 추가 | **`SnapshotEnvelope` root + `SnapshotKind { Full, Delta }`** discriminator. FlatBuffers root verify 안전 |
| **C4** | Command ordering 은 M6 결정성 보강에서 도입 | **M1 부터 stable_sort + tie-breaker** (`acceptedTick, sessionId, sequenceNum`). `unordered_map` 순회 금지 — Server first day 부터 |
| **C5** | `/fp:precise` 는 M6 결정성 보강 | **M1 부터 Server `/fp:precise` 강제. Engine/Client 는 현재 Precise 유지 확인.** Engine/Shared sim 경계 `/fp:fast` 금지 문서화 |
| **C6** | Prediction world = `SharedSim::World = ::CWorld` 전체 복제 | **Prediction = sim-only component subset 명시.** `RenderComponent` / `ModelRenderer` / `FxBillboardComponent` 등 비결정 상태 제외 |
| **C7** | grep guard 종료 조건을 전부 `0 hit` 로 둠 | **allowlist 외 0 hit** 로 보정. 현재 repo 의 lookup-only registry / `EntityIdMap` `unordered_map`, unique `EntityID` 정렬은 문서화된 예외 |

**마일스톤 조정**:
- **M1 축소**: UDP transport + hello/session + CommandBatch + **full** snapshot loop 까지만. AntiCheat 는 range/cooldown 최소. crypto/reliability/delta 미포함. 단 **결정성 가드 (stable_sort + /fp:precise)** 는 M1 부터.
- **M2 제한**: 3-channel reliability + ack + retransmit + fragment/reassembly. **ChaCha20/ECDH 제거 → Sim-11 또는 M6 이후**.
- **M3**: SnapshotEnvelope full↔delta 전환 + baseline ack + AOI + full-resync fallback. **ack-only heartbeat** (idle client 도 baseline ack 가능).
- **M4**: `kMaxRewindMs = 200` 단일화 (200ms 초과 reject + suspicion).
- **M5**: predicted world = **sim-only state** 만 재실행 (RenderComponent 등 제외).
- **M6**: replay logger + grep guard + static_assert + cross-process bit-equal. 결정성 가드 일부는 M1 에 이미 있음.

**Public Interface 명시 (v2 신규)**:
- Shared network: `eUdpChannel`, `UdpPacketHeader`, `UdpFragmentHeader`, `SeqGreater`/`SeqDistance` (wrap-around safe)
- Schema: `SnapshotEnvelope` root + `SnapshotKind { Full, Delta }`, `SnapshotDelta { baselineTick, added/changed/removed }`
- Server M1: `CUdpCore`, `CUdpSession`, `CUdpSessionManager`, `CUdpPacketDispatcher`, **`CGameRoom::EnqueueCommand(sessionId, GameCommandWire, acceptedTick)`**
- Client M1: `CUdpClient`, **`CCommandSerializer::BuildCommandBatch(...)`**, `CSnapshotApplier` (full first; delta M3)

**Assumptions (v2)**:
- M1 transport: **Windows UDP IOCP with `WSARecvFrom`/`WSASendTo`** (not RIO)
- Encryption: M1-M2 미포함. transport semantics 검증 후 Sim-11 또는 M6 이후 hardening
- Backend (Auth/Match/Shop/Profile/Payment): TCP/HTTP 그대로

---

## 1. 종합 진단 (v1 동일 — 변경 없음)

### 1.1 기존 markdown 인벤토리

**sim 폴더 (4개)**:
- `00_SHAREDGAMESIM_MASTER_PLAN.md`, `03_SCHEMA_FLATBUFFERS.md`, `04_IOCP_GAMEROOM.md` (v1.2 TCP), `05_TO_09_REMAINING_OUTLINE.md`

**backend 폴더 (8개)**: Phase 0-8 동작 ✅
**security 폴더 (8개)**: Level 0-5 박제
**architecture 폴더 (4개)**: Final + Conventions + Gameplay + Class Day8

### 1.2 코드베이스 현재 상태

**Shared/ ✅**: PacketEnvelope (TCP framing 가정), EntityIdMap, DeterministicRng (sub-stream 박제), ICommandExecutor, Components, Schemas (Command/Snapshot/Event)

**Server/ ⚠️**: IOCPCore 77줄 (sketch), FrameParser 39줄 (consume 미완성), Session 40줄 (멤버만), 나머지 0줄. main.cpp placeholder.

**Client Network ⚠️**: 6 HTTP client + CommandSerializer + SnapshotApplier 모두 stub. UDP/Prediction 미존재.

### 1.3 LoL 토폴로지 7-8 레이어 매핑

```
L8  Anti-cheat / Replay / 분쟁          ← Sim-9 + Security Level 5
L7  Region routing / Service mesh        ← 향후
L6  Matchmaking + Auth + Shop + ...      ← Backend Phase 1-7 ✅
L5  Server Orchestration (K8s + Agones)  ← 향후
L4  UDP + Reliability + Delta + AOI      ← ★ 본 계획서 M1-M3
L3  Lag compensation + Prediction        ← ★ 본 계획서 M4-M5
L2  결정론적 시뮬레이션                    ← ★ M1 기본가드 + M6 grep+replay
L1  GameRoom Tick / Dispatcher / IOCP    ← v1.2 04 + M1 본격
```

---

## 2. UDP 전환 (v1 §2 동일)

### 2.1 TCP HoL Blocking
TCP in-order delivery → 패킷 1개 손실 = 그 뒤 모든 패킷 stall. 30Hz tick 환경에서 100ms RTT × 1% loss → 1-3 tick frozen.

### 2.2 LoL 실제 구조
ENet 기반 자체 UDP reliability layer. **3 channel** (입력 reliable ordered / 스킬 reliable unordered / 위치 unreliable sequenced) per-channel sequence + ack.

### 2.3 UDP 도입 효과
HoL blocking 제거 / snapshot 손실 허용 / 입력 retransmit only / MTU-aware fragment / congestion control.

### 2.4 TCP 유지 영역
Hello/Auth/Login (Backend Phase 1-7 그대로) + Match lock-in + In-game chat. **하이브리드**.

---

## 3. UDP Reliability Layer 설계 (★ M1-M2)

### 3.1 3-channel 분리

```cpp
// Shared/Network/UdpChannel.h (★ Sim-10 신규)
enum class eUdpChannel : u8_t
{
    ReliableOrdered     = 0,  // 입력 (CommandBatch) — 순서 보장 + 재전송
    ReliableUnordered   = 1,  // Event (kill, level up) — 재전송, 순서 무관
    UnreliableSequenced = 2,  // Snapshot — 최신만 (오래된 무시), 재전송 X
};
```

### 3.2 패킷 envelope (★ v2 — ack 의미 변경)

```cpp
// Shared/Network/UdpPacketHeader.h (★ Sim-10 신규)
#pragma pack(push, 1)
struct UdpPacketHeader  // 24 bytes
{
    uint16_t magic;            // 0x5742
    uint8_t  version;          // 2 (TCP envelope=1 와 구분)
    uint8_t  channel;          // eUdpChannel
    uint16_t type;             // ePacketType (CommandBatch/Snapshot/Event/Hello/Heartbeat/AckOnly)
    uint16_t flags;            // Compressed / Encrypted / Fragment(=별도 header 따라옴) / AckOnly
    uint32_t channelSeq;       // ★ 이 packet 의 per-channel sequence
    uint32_t ackSeq;           // ★ v2 — 가장 최근 받은 packet 의 seq (newest received)
    uint32_t ackBitfield;      // ★ v2 — ackSeq 이전 32개의 수신 마스크 (bit i = ackSeq-(i+1) 받았는지)
    uint16_t payloadSize;      // bytes (fragment 시: 본 packet 의 fragment payload size)
    uint16_t reserved;         // 0
};
#pragma pack(pop)
static_assert(sizeof(UdpPacketHeader) == 24);
```

**ack 의미 (v2 명확화)**:
- `ackSeq` = 수신 측이 본 가장 큰 seq (꼭 contiguous 일 필요 X)
- `ackBitfield` bit `i` = `ackSeq - (i+1)` 수신 여부
- Sender 는 `ackSeq` 위로 새 packet 이 있어도 `ackBitfield` 의 0 bit 는 hole = 손실 후보 → RTO 후 재전송
- v1 의 "last contiguous" 는 selective ack 가 안 되어 효율 ↓

### 3.3 Fragment header (★ v2 별도 header)

```cpp
// Shared/Network/UdpFragmentHeader.h (★ Sim-10 신규)
#pragma pack(push, 1)
struct UdpFragmentHeader  // 10 bytes — UdpPacketHeader 다음, payload 앞
{
    uint32_t messageId;            // ★ 같은 message 의 fragment 들이 공유
    uint16_t fragmentIndex;        // ★ v2 — u16 (256 → 65536 fragment 까지 OK)
    uint16_t fragmentCount;        // ★ v2 — u16
    uint16_t fragmentPayloadSize;  // 본 fragment 의 payload bytes
};
#pragma pack(pop)
static_assert(sizeof(UdpFragmentHeader) == 10);

// MTU 정책
constexpr uint32_t kMtuPayloadBytes = 1200;   // ★ v2 — IPv4 헤더 20 + UDP 8 + UdpPacketHeader 24 + UdpFragmentHeader 10 = 62 byte 헤더 → 안전 MTU 1280 이내
constexpr uint32_t kMaxMessageBytes = 1024 * 1024;  // 1MB (1MB / 1200 ≈ 873 fragment, u16 한도 65535 충분)
```

**Fragment 정책**:
- payload > `kMtuPayloadBytes` → split
- 각 fragment 가 `UdpPacketHeader.flags` 에 `Fragment` bit set + `UdpFragmentHeader` 동봉
- Receive 측: `messageId` 별 reassembly buffer + bitset (어떤 fragment 받았는지)
- 모든 fragment 도착 시 reassemble + 원래 packet 처리
- Timeout (e.g. 5 sec) 후 메모리 회수

### 3.4 Sequence wrap-around helpers (★ v2 명시)

```cpp
// Shared/Network/SeqMath.h (★ Sim-10 신규)
inline bool SeqGreater(uint32_t a, uint32_t b)
{
    // 32-bit wrap-around safe (RFC 1982)
    return (a > b && a - b < 0x80000000u) ||
           (a < b && b - a > 0x80000000u);
}

inline int32_t SeqDistance(uint32_t a, uint32_t b)
{
    // signed distance (a - b)
    return static_cast<int32_t>(a - b);
}
```

### 3.5 Re-transmit + Congestion Control (1차)

**RTO**: SRTT + max(50ms, 4×RTTVAR). 최소 50ms / 최대 500ms.

**CWND**: 1차는 fixed (32 packet). 게임 입력 30Hz × 32B ≈ 1KB/s 라 congestion 거의 안 남. 추후 BBR/AIMD.

**정책**:
- `ReliableOrdered`: stall — 손실 발견 시 재전송 + 그 뒤 packet 유보
- `ReliableUnordered`: 손실 발견 시 재전송 + 그 뒤 packet 즉시 전달
- `UnreliableSequenced`: 손실 무시 + 더 새 packet 받으면 ok

### 3.6 Encryption (★ v2 — Sim-11 별도)

v1 은 M2 후반에 ChaCha20+ECDH. **v2 는 M2 에서 제외**. 사유:
- Transport semantics (reliability + fragment + ack) 검증이 우선
- Cipher 결함 (nonce reuse 등) 은 transport 검증과 별개 디버깅
- LAN demo (M1) / external test (M3-M5) 까지는 plaintext OK

**Sim-11**: DTLS 1.3 통합 (mbedTLS) 또는 ChaCha20-Poly1305 + ECDH 자체 박제. **별도 사이클**.

---

## 4. 결정성 보강 (★ M1 부터 적용 — v2 변경)

### 4.1 `/fp:precise` 강제 (★ v2 — M1 부터)

**Server M1 시작 시점**:
- `Server/Include/Server.vcxproj` `<FloatingPointModel>Precise</FloatingPointModel>` 강제
- `<FloatingPointModel>Fast</FloatingPointModel>` 절대 X
- `Engine/Include/Engine.vcxproj` 는 현재 Debug/Release 모두 `Precise` 확인됨 — M1 에서 유지/회귀 가드

**Client M5 시작 시점**:
- `Client/Include/Client.vcxproj` 는 현재 Debug/Release 모두 `Precise` 확인됨 — M5 에서 prediction guard 로 재검증
- prediction 시 server 결과와 bit-equal 위해 `Fast` 회귀만 차단

**`Engine/Shared` sim 경계 정책**:
- Sim 코드 (`Shared/GameSim/`, `Server/Private/Game/`, `Server/Private/Security/`) 는 `/fp:fast` 절대 X
- 비-sim 코드 (`Client/Private/UI/`, `Client/Private/Renderer/`) 는 `Precise` 권장이나 미강제

**회귀 가드 (CI)**:
```bash
grep -rn "FloatingPointModel.*Fast\|/fp:fast" \
    Engine/Include/ Server/Include/ Client/Include/
# 결과 발견 시 즉시 fail
```

### 4.2 Container 결정성 (★ v2 — M1 부터)

| 컨테이너 | 결정성 | sim 사용 가능 |
|---|---|---|
| `std::vector<T>` | ✅ insert 순서 | ✅ |
| `std::map<K,V>` | ✅ key 정렬 | ✅ |
| `std::unordered_map<K,V>` | ❌ hash 순서 | **❌ M1 부터 금지** |
| `std::set<T>` | ✅ key 정렬 | ✅ |
| `std::unordered_set<T>` | ❌ hash 순서 | **❌ M1 부터 금지** |

**예외**: sim 외 (lookup-only cache, ECS internal storage) 는 OK. 단 sim 내부 순회 (Server `GameRoom` Tick 안) 는 항상 `std::map` 또는 sorted `std::vector`.

**현재 repo allowlist (M1 기준)**:
- `Shared/GameSim/Registries/*Registry.h` 의 `unordered_map` 은 id/name → definition lookup-only 용도로 허용. 단 `ForEach...` 결과를 sim effect 순서로 쓰면 금지. 필요 시 sorted vector 로 복사 후 처리.
- `Shared/GameSim/EntityIdMap.h` 의 `unordered_map` 은 `NetEntityId` ↔ `EntityID` 양방향 lookup 용도로 허용. 순회 API 를 추가해서 sim effect 순서로 쓰는 것은 금지.
- ECS internal store 는 본 grep 의 직접 fail 대상이 아님. System 실행 전 `DeterministicEntityIterator` 같은 sorted collector 를 거친다.

**회귀 가드 (M1 부터)**:
```bash
grep -rn "unordered_map\|unordered_set" \
    Shared/GameSim/ Server/Private/Game/ Server/Private/Security/
# allowlist 외 hit 발견 시 fail. allowlist hit 는 정당성 문서화.
```

### 4.3 Command ordering (★ v2 — M1 부터)

**`CGameRoom` Tick 의 Phase_DrainCommands 첫 줄부터**:
```cpp
// ★ v2 — M1 부터 stable_sort + tie-breaker. unordered iteration 절대 X.
std::stable_sort(drained.begin(), drained.end(),
    [](const PendingCommand& a, const PendingCommand& b) {
        if (a.acceptedTick != b.acceptedTick)
            return a.acceptedTick < b.acceptedTick;
        if (a.sessionId != b.sessionId)
            return a.sessionId < b.sessionId;
        return a.sequenceNum < b.sequenceNum;
    });
```

**왜 `std::sort` 가 아닌 `std::stable_sort`**: 동일 key 케이스에서 입력 순서 보장. `std::sort` 는 동일 key 순서 미보장 → process 마다 다른 결과 가능.

**예외**: `DeterministicEntityIterator::CollectSorted` 처럼 key 가 유일한 `EntityID` 단일 정렬은 `std::sort` 허용. 동일 key 가능성이 있는 command/event 정렬은 `stable_sort + tie-breaker` 필수.

### 4.4 RNG sub-stream 강제

`DeterministicRng::MakeSubSeed(tickIndex, sourceEntity, skillId)` 박제됨. System 들이 직접 `tc.pRng->NextU*()` 호출 금지.

**회귀 가드**:
```bash
grep -rn "tc\.pRng->Next\|pRng->Next" Shared/GameSim/Systems/ \
    | grep -v "MakeSubSeed"
```

### 4.5 Logical time 만 사용

Sim 내부에서 `std::chrono::now()`, `time(nullptr)`, `GetTickCount()` 절대 X. 항상 `tc.tickIndex` 또는 `DeterministicTime::TickToSec(tickIndex)`.

**회귀 가드**:
```bash
grep -rn "chrono::\|GetTickCount\|time(0\|time(nullptr\|QueryPerformance" \
    Shared/GameSim/ Server/Private/Game/ Server/Private/Security/
```

### 4.6 Fix-point 수학 (장기 — Sim-12 후속)

`Shared/GameSim/FixedMath.h` 신규 (Sim-12) — q16.16 add/sub/mul/div + LUT-based sin/cos/sqrt. **M1-M6 범위 밖**.

---

## 5. Snapshot Delta + AOI (★ M3 — v2 envelope 갱신)

### 5.1 ★ v2 — `SnapshotEnvelope` root

**v1 의 단순 추가 X** — FlatBuffers root verify 안전을 위해 envelope discriminator.

```fbs
// Shared/Schemas/Snapshot.fbs (v2 갱신)
include "Command.fbs";

namespace Shared.Schema;

enum SnapshotKind : ubyte
{
    Full  = 0,
    Delta = 1,
}

table EntitySnapshot {
    netId:uint;
    championId:ubyte;
    team:ubyte;
    level:ubyte;
    hp:float;
    mana:float;
    posX:float; posY:float; posZ:float;
    yaw:float;
    moveSpeed:float;
    animId:ushort;
    animPhaseFrame:ushort;
    skillCooldowns:[float];
    skillRanks:[ubyte];
    buffMask:uint;
    statHash:uint;
}

table SnapshotFull {
    serverTick:ulong;
    serverTimeMs:ulong;
    rngState:ulong;
    entities:[EntitySnapshot];
    lastAckedCommandSeq:uint;
    yourNetId:uint;
}

table EntitySnapshotDelta {
    netId:uint;
    fieldMask:uint;          // bit 0 = hp, 1 = mana, 2 = pos, 3 = yaw, ...
    hp:float;
    mana:float;
    posX:float; posY:float; posZ:float;
    yaw:float;
    moveSpeed:float;
    animId:ushort; animPhaseFrame:ushort;
    skillCooldowns:[float];  // 길이 0 = 변경 없음
    buffMask:uint;
    statHash:uint;
}

table SnapshotDelta {
    serverTick:ulong;
    serverTimeMs:ulong;
    rngState:ulong;
    baselineTick:ulong;                        // ack 받은 baseline (0 X)
    addedEntities:[EntitySnapshot];
    changedEntities:[EntitySnapshotDelta];
    removedNetIds:[uint];
    lastAckedCommandSeq:uint;
    yourNetId:uint;
}

// ★ v2 — root discriminator
table SnapshotEnvelope {
    kind:SnapshotKind;
    full:SnapshotFull;       // kind=Full 일 때만
    delta:SnapshotDelta;     // kind=Delta 일 때만
}

root_type SnapshotEnvelope;
```

**Recv 흐름**:
```cpp
auto* env = Shared::Schema::GetSnapshotEnvelope(buf);
if (env->kind() == SnapshotKind::Full)
    ApplyFull(env->full(), world);
else
    ApplyDelta(env->delta(), world);
```

### 5.2 Baseline ack 메커니즘 (★ v2 — ack-only heartbeat 추가)

**Server side**:
- 매 client 별 `m_acknowledgedBaselineTick` 추적
- snapshot 송신 시 `baselineTick = m_acknowledgedBaselineTick`
- Client ack (다음 CommandBatch 의 piggyback `lastAcknowledgedSnapshotTick` 필드) 받으면 갱신

**Client side**:
- snapshot 수신 시 `m_lastReceivedSnapshotTick = env.serverTick()`
- 다음 CommandBatch piggyback ack
- **★ v2 — idle client (입력 없음)** 는 5초마다 **`AckOnlyHeartbeat`** 송신 → `lastAcknowledgedSnapshotTick` piggyback

### 5.3 AOI (50m grid + LOD)

**1차 (M3)**: 50m grid bucketing. `CAOI::Rebuild(world)` 매 tick / `ComputeVisibleSet(self, world, outSet)` 3×3 cell 범위.

**2차 (M3 후반)**:
- 시야 차폐 (Vision System — 와드/부쉬/벽)
- 정밀도 LOD: 가까운 entity 정밀 / 멀리 quantized 8-bit

### 5.4 Bandwidth 목표

| 시나리오 | 1 client 입력 | 1 client 수신 | 5v5 / 서버 | 100 동접 (10 룸) |
|---|---|---|---|---|
| 30Hz delta + AOI | 0.3 KB/s | 4 KB/s | 40 KB/s | 400 KB/s = 3.2 Mbps |
| Full snapshot (현 v1.2) | 0.3 KB/s | 19 KB/s | 190 KB/s | 1.9 MB/s = 15 Mbps |

**80% 대역폭 절감**.

### 5.5 Full-resync fallback (★ v2 명시)

Client 가 baselineTick 을 잃거나 (장시간 disconnect 후 reconnect) ack 가 늦으면:
- Server: `acknowledgedBaselineTick = 0` → 다음 snapshot 을 `kind=Full` 로 송신
- Client: 받은 `Full` 로 PredictionWorld + RenderWorld 전체 reset

---

## 6. Lag Compensation + Client Prediction (★ M4-M5)

### 6.1 Server-side rewind (★ v2 — kMaxRewindMs = 200 단일화)

```cpp
// Server/Public/Security/LagCompensation.h
class CLagCompensation
{
public:
    // ★ v2 — 단일 한도 (v1 의 kMaxHistoryFrames=6 와 200ms 두 개 중첩 X)
    static constexpr u32_t kMaxRewindMs = 200;
    static constexpr u32_t kMaxHistoryFrames = (kMaxRewindMs * 30) / 1000;  // 30Hz 기준 6 frame

    void RecordHistory(CWorld& world, u64_t tickIndex, u64_t serverTimeMs);
    bool TryGetHistoricalState(EntityID e, u64_t pastServerTimeMs, EntityState& outState) const;

    // 200ms 초과 시 false 반환 → caller 가 reject + suspicion ++
    bool IsWithinRewindWindow(u64_t pastServerTimeMs, u64_t currentServerTimeMs) const
    {
        return (currentServerTimeMs - pastServerTimeMs) <= kMaxRewindMs;
    }
};
```

### 6.2 Client Prediction (★ v2 — sim-only subset)

**v1 의 위험**: `SharedSim::World = ::CWorld` → 전체 ECS 복제. `RenderComponent`, `ModelRenderer`, `FxBillboardComponent` 등 비결정 상태도 복제 → mispredict 시 시각 깜빡임 + 메모리 폭발.

**v2 해결**: predicted world = **sim-only component subset**. 명시적 list:

```cpp
// Client/Public/Network/PredictionWorld.h (★ v2)
class CPredictionWorld
{
public:
    // ★ v2 — sim-only component types (whitelist)
    //   Render/FX/Audio component 는 절대 복제 X
    using SimComponents = std::tuple<
        TransformComponent,
        StatComponent,
        HealthComponent,
        ManaComponent,
        SkillStateComponent,
        SkillRankComponent,
        BuffComponent,
        PendingHitComponent,
        ProjectileKindComponent,
        ChampionComponent,
        NetEntityIdComponent
        // ★ 추가 시 결정성 검증 필요
    >;

    // 위 list 만 복제 + 재실행
    void ExecuteCommand(const GameCommand& cmd, const TickContext& tc);
    void ApplySnapshot(const Shared::Schema::SnapshotEnvelope* env);
    void Rollback(u32_t lastAckedSeq, const ClientInputBuffer& inputs);
};
```

**Render 측**:
- `CRenderInterpolator` 가 PredictionWorld 의 Transform 만 읽어서 RenderWorld 의 ModelRenderer 에 lerp 반영
- RenderComponent 자체는 prediction 대상 X

### 6.3 Client Input Buffer (Sim-5 §5.2 동일)

```cpp
struct ClientInputBuffer
{
    static constexpr u32_t kCapacity = 120;   // 4초 @ 30Hz
    GameCommandWire commands[kCapacity];
    u64_t commandTicks[kCapacity];
    u32_t head = 0;
    u32_t tail = 0;
    u32_t nextSeq = 1;

    u32_t Push(const GameCommandWire& cmd, u64_t clientTick);
    void  Drop(u32_t ackedSeq);
    bool  ForEachAfter(u32_t startSeq,
        std::function<void(u32_t, const GameCommandWire&, u64_t)> fn) const;
};
```

### 6.4 Render Interpolation

- Snapshot 수신 시 `RenderInterpolator.BeginInterpolation(predictedWorld)` 로 truth 기록
- Render frame (144Hz) 마다 `Tick(visualWorld, dt)` — 100ms lerp
- Mispredict 시 snap-to 가 아니라 lerp → 시각 jitter < 2 frame

### 6.5 결정성 동기화

매 snapshot 수신 시 `m_localRng.SetState(env.full().rngState() or env.delta().rngState())`. 입력 재실행 시 server 와 bit-equal RNG → mispredict 0 (이상적).

---

## 7. 변경/신규 파일 list (★ v2 갱신)

### 7.1 Shared 측

| 파일 | 종류 | M | 비고 |
|---|---|---|---|
| `Shared/Network/UdpChannel.h` | 신규 | M1 | `eUdpChannel` enum |
| `Shared/Network/UdpPacketHeader.h` | 신규 | M1 | 24 bytes, **ackSeq=newest** (v2) |
| `Shared/Network/UdpFragmentHeader.h` | 신규 | M2 | 10 bytes, **u16/u16 + messageId** (v2) |
| `Shared/Network/SeqMath.h` | 신규 | M1 | `SeqGreater`/`SeqDistance` |
| `Shared/Schemas/Snapshot.fbs` | 갱신 | M3 | **`SnapshotEnvelope` root + Full + Delta** (v2) |
| `Shared/Schemas/Event.fbs` | 갱신 | M3 | eEventKind 확장 |
| `Shared/GameSim/Snapshot/DeltaBuilder.h/.cpp` | 신규 | M3 | server side |
| `Shared/GameSim/Snapshot/DeltaApplier.h/.cpp` | 신규 | M3 | client side |
| `Shared/GameSim/FixedMath.h` | 신규 | Sim-12 | M6 범위 밖 |

### 7.2 Server 측

| 파일 | 종류 | M | 비고 |
|---|---|---|---|
| `Server/Public/Network/UdpCore.h/.cpp` | 신규 | M1 | WSARecvFrom/SendTo + IOCP |
| `Server/Public/Network/UdpSession.h/.cpp` | 신규 | M1 | sourceAddr 기반 lookup |
| `Server/Public/Network/UdpSession_Manager.h/.cpp` | 신규 | M1 | |
| `Server/Public/Network/UdpPacketDispatcher.h/.cpp` | 신규 | M1 | envelope verify → CommandBatch verify → room |
| `Server/Public/Network/UdpReliabilityChannel.h/.cpp` | 신규 | M2 | per-channel sequence/ack/RTO |
| `Server/Public/Network/UdpFragmenter.h/.cpp` | 신규 | M2 | MTU split + reassemble |
| `Server/Public/Game/GameRoom.h/.cpp` | v1.2 04 본격 | M1 | **`EnqueueCommand(sessionId, GameCommandWire, acceptedTick)` API** + **stable_sort drain** (v2 M1) |
| `Server/Public/Game/SnapshotBuilder.h/.cpp` | 헤더 분리 + delta | M3 | |
| `Server/Public/Game/AOI.h/.cpp` | 본격 | M3 | LOD 포함 |
| `Server/Public/Security/AntiCheatServer.h/.cpp` | range/cooldown 최소 | M1 | targetMode 검증은 M3 |
| `Server/Public/Security/LagCompensation.h/.cpp` | 본격 | M4 | **kMaxRewindMs = 200 단일** (v2) |
| `Server/Public/Game/ReplayLogger.h/.cpp` | 신규 | M6 | input log → 파일 |
| `Server/Private/main.cpp` | placeholder → bootstrap | M1 | |
| `Server/Include/Server.vcxproj` | **`/fp:precise` + Mswsock.lib + Engine ref** | M1 | (v2 — M1 부터) |

### 7.3 Client 측

| 파일 | 종류 | M | 비고 |
|---|---|---|---|
| `Client/Public/Network/UdpClient.h/.cpp` | 신규 | M1 | UDP send/recv + reliability mirror |
| `Client/Public/Network/CommandSerializer.cpp` | stub → 본격 | M1 | **`BuildCommandBatch(...)` API 명시** (v2) |
| `Client/Public/Network/SnapshotApplier.cpp` | full first | M1 | delta 는 M3 |
| `Client/Public/Network/ClientInputBuffer.h/.cpp` | 신규 | M5 | |
| `Client/Public/Network/PredictionWorld.h/.cpp` | 신규 | M5 | **sim-only subset** (v2) |
| `Client/Public/Network/RollbackEngine.h/.cpp` | 신규 | M5 | |
| `Client/Public/Network/RenderInterpolator.h/.cpp` | 신규 | M5 | |
| `Client/Include/Client.vcxproj` | **`/fp:precise` 유지/검증** | M5 | 현재 Precise 확인됨 — Fast 회귀 차단 |

### 7.4 Backend (Go) — 변경 없음

UDP 게임 서버는 Backend HTTP 와 별개.

---

## 8. Phase 분할 (★ v2 — M1/M2/M5 조정)

### M1 — UDP Transport Baseline (★ v2 축소: 60h → 50h)

**목표**: TCP IOCP (v1.2) → UDP 로 포팅. **단일 client 1 packet hello/session/CommandBatch + full snapshot loop**.

**작업** (★ v2 명시):
- `UdpCore` — WSARecvFrom/SendTo + IOCP (RIO X)
- `UdpSession` (sessionId 발급, sourceAddr 기반 lookup)
- `UdpSessionManager`
- `UdpPacketDispatcher` (envelope verify → CommandBatch FlatBuffers verify → room queue)
- `GameRoom` — v1.2 04 §5 본격, **`EnqueueCommand(...)` API**, **stable_sort drain** (v2)
- `Server/main.cpp` bootstrap
- `Client/Public/Network/UdpClient` 신규
- `Client/Public/Network/CommandSerializer::BuildCommandBatch(...)` 본격
- `Client/Public/Network/SnapshotApplier` — **full snapshot path 만** (delta 는 M3)
- AntiCheat **range/cooldown 최소만** (targetMode/issuer 는 M3)
- ★ **`/fp:precise` 강제** (Server.vcxproj; Engine/Client 는 현재 Precise 유지 확인)
- ★ **결정성 회귀 가드 grep CI** (unordered_map/chrono/std::sort/fp:fast 4 항목)

**M1 미포함**: crypto, reliability (3-channel), retransmit, fragment, delta, AOI, lag comp, prediction.

**종료 조건** (★ v2):
- localhost 1 client connect → 1 CommandBatch send → server tick → **full snapshot** broadcast → client recv. 손실 0.
- 5분 30Hz tick jitter < 5ms.
- UDP localhost RTT < 1ms.
- ★ 회귀 가드 4항목 allowlist 외 0 hit.
- ★ `Server.vcxproj` `Precise` 설정 확인.

**별도 sub-plan**: `.md/plan/sim/10_v2_M1_UDP_TRANSPORT.md`.

### M2 — Reliability Channel (★ v2 축소: 40h → 30h. crypto 제거)

**목표**: 3 channel + sequence/ack + retransmit + fragment/reassembly. **5% loss + 100ms RTT 환경 입력 stable**.

**작업** (★ v2):
- `UdpReliabilityChannel` (per-channel ring + ackBitfield 32 + RTO)
- `UdpFragmenter` (>1200B 분할/재조립, **u16 fragment count**)
- `SeqMath` (wrap-around safe)
- 회귀 시나리오: clumsy net emulator 5%/10%/20% loss + 100ms RTT

**M2 제외 (★ v2)**:
- ChaCha20/ECDH crypto → **Sim-11** 또는 M6 이후 별도

**종료 조건**:
- 5% loss + 100ms RTT 에서 ReliableOrdered 100% 도달
- UnreliableSequenced 손실 그대로 통과
- **256KB + 1MB payload reassembly OK** (★ v2 — u16 한도 검증)
- nonce reuse 검증은 Sim-11 로 위임

**별도 sub-plan**: `.md/plan/sim/10_v2_M2_UDP_RELIABILITY.md`.

### M3 — Snapshot Delta + AOI (50h)

**목표**: 대역폭 80% 절감. 5v5 = 4 KB/s/client.

**작업** (★ v2):
- `Shared/Schemas/Snapshot.fbs` 갱신 — **`SnapshotEnvelope` root + `SnapshotKind`**
- `Shared/GameSim/Snapshot/DeltaBuilder` (server)
- `Shared/GameSim/Snapshot/DeltaApplier` (client)
- `CAOI` 본격 (50m grid + 3×3 cell)
- LOD (정밀도 quantization)
- Baseline ack — Client → Server piggyback `lastAcknowledgedSnapshotTick`
- ★ **`AckOnlyHeartbeat`** (idle client 5초마다)
- ★ **Full-resync fallback** (baseline 미일치 시 자동)
- AntiCheat targetMode/issuer spoof 검증 추가

**종료 조건**:
- 5v5 룸 평균 snapshot < 200B
- baseline 미일치 시 자동 full snapshot (resync 무봉)
- 100 동접 10 룸 부하 시 서버 outbound < 5 Mbps
- idle client 5초 후 baseline ack 정상

**별도 sub-plan**: `.md/plan/sim/10_v2_M3_DELTA_AOI_VISION.md`.

### M4 — Lag Compensation (30h)

**목표**: 100ms RTT 클라가 BA hit 보고 시 서버가 rewind 검증.

**작업** (★ v2):
- `CLagCompensation::RecordHistory` 본격 (200ms = 6 frame, position + hp + bIsDead)
- `TryGetHistoricalState(entity, pastServerTimeMs)` API
- ★ **`kMaxRewindMs = 200` 단일화** (v1 의 6 frame + 200ms 두 한도 → 하나로)
- `IsWithinRewindWindow(pastMs, currentMs)` guard
- BA hit validation 통합

**종료 조건**:
- 100ms RTT BA → 클라 본 시점 target 위치 hit 인정
- **>200ms RTT → reject + suspicion ++** (★ v2 단일 한도)
- 6 frame 보존 메모리 < 5 KB / 룸

**별도 sub-plan**: `.md/plan/sim/10_v2_M4_LAG_COMPENSATION.md`.

### M5 — Client Prediction + Reconciliation (★ v2 변경 — sim-only)

**목표**: 100ms RTT perceived latency = 0. visual jitter < 2 frame.

**작업** (★ v2):
- `ClientInputBuffer` (120 frame ring)
- `CPredictionWorld` — **★ sim-only component subset** (Render/FX/Audio component 제외)
- `CRollbackEngine` (snapshot 수신 → lastAckedSeq 이후 입력 재실행, sim-only state 만)
- `CRenderInterpolator` (snap-to 회피, 100ms lerp, RenderComponent 만 갱신)
- RNG 동기화 — `m_localRng.SetState(snap.rngState())`
- ★ **`Client.vcxproj` `/fp:precise`** 유지 검증 (M5 진입 시)

**종료 조건**:
- 100ms RTT → 캐스트 즉시 반응
- 200ms 후 server authoritative 결과 무봉 보정 (lerp)
- 250ms RTT → mispredict snap-to 없음 (lerp)
- mispredict 비율 < 5%
- bit-equal — 같은 input + seed → client predicted = server authoritative
- ★ Render/FX component 가 prediction world 에 들어가지 않음 (검증 grep)

**별도 sub-plan**: `.md/plan/sim/10_v2_M5_CLIENT_PREDICTION.md`.

### M6 — Replay + Determinism Hardening (★ v2 축소: 30h → 20h)

**목표**: replay 도구 + cross-process determinism + grep guard 자동화. (결정성 가드 일부는 이미 M1 에 있음 — v2)

**작업** (★ v2):
- `Server/Public/Game/ReplayLogger` (input log → 파일, server tick 마다)
- replay 도구: input log + seed → 시뮬 재실행 → hp/pos/rng/snapshot hash 비교
- ★ M1 의 회귀 가드 4 항목을 12 항목으로 확장:
  1. `unordered_map`/`unordered_set` in sim — lookup-only allowlist 제외
  2. `chrono::`/`GetTickCount`/`time(0)` in sim
  3. `tc.pRng->Next` 직접 호출 (`MakeSubSeed` 미경유)
  4. `std::sort` (sim 내) — 동일 key 가능 정렬은 `std::stable_sort + tie-breaker` 필수. unique `EntityID` 정렬은 allowlist
  5. `/fp:fast` 또는 `FloatingPointModel.*Fast`
  6. `Shared/GameSim/Components/*.h` 전수 `static_assert(std::is_trivially_copyable_v<T>)` 누락
  7. `Shared/GameSim/` 에서 `Client/` include
  8. `m_acceptSocket` AcceptEx 없는데 `IOContext` 에 누락
  9. `make_unique` private ctor 클래스
  10. `WINTERS_API`/`ENGINE_DLL` 마크 안 된 internal 클래스 export
  11. namespace Engine 밖 헤더에서 `bool_t`/`u8_t` alias
  12. unqualified `vector`/`function` (using namespace std 가정)

**종료 조건**:
- 같은 seed + 같은 input log → 2 process 5분 시뮬 hp/pos/rngState/snapshot hash 100% 일치
- 회귀 grep 12종 allowlist 외 0 hit
- replay tool 로 종료된 게임 재시뮬 OK
- (선택) fixed-point 도입 후 cross-platform replay → Sim-12

**별도 sub-plan**: `.md/plan/sim/10_v2_M6_DETERMINISM_REPLAY.md`.

---

## 9. 의존 그래프 (★ v2 — Encryption 별도 Sim-11)

```
Backend Phase 1-7 (HTTP) ✅
        │
        ▼
   Sim-1 ~ Sim-3 ✅ 부분
        │
        ▼
   ┌────────────────────────────────┐
   │  M1 — UDP Transport            │
   │   + stable_sort + /fp:precise  │  ★ v2 결정성 가드 M1 부터
   └────────────────────────────────┘
        │
        ▼
   ┌────────────────────────────────┐
   │  M2 — Reliability + Fragment   │  (★ v2 — Encryption 제거)
   └────────────────────────────────┘
        │
   ┌────┴────┐
   ▼         ▼
M3 Delta   M4 LagComp
+ AOI      (병렬 가능)
   │         │
   └────┬────┘
        ▼
   ┌────────────────────────────────┐
   │  M5 — Client Prediction        │
   │   (★ v2 sim-only subset)       │
   └────────────────────────────────┘
        │
        ▼
   ┌────────────────────────────────┐
   │  M6 — Replay + Hardening       │
   └────────────────────────────────┘
        │
        ├──────────────────────────────┐
        ▼                              ▼
   Sim-7/8/9                      Sim-11 — Encryption
   (Bot/MCTS/RL)                  (DTLS 또는 ChaCha20+ECDH)
```

**병렬 가능**: M3 ↔ M4. Sim-11 은 M6 와 독립.

---

## 10. 검증 시나리오 (★ v2)

### M1 합격 (★ v2)
- localhost 1 client connect → 30Hz tick → **full snapshot** loop. 5분 jitter < 5ms.
- ★ `Server.vcxproj` `Precise` 확인.
- ★ 회귀 grep 4 항목 (unordered_map / chrono / std::sort / fp:fast) allowlist 외 0 hit.

### M2 합격 (★ v2)
- clumsy 5% loss + 100ms RTT → ReliableOrdered 100%, UnreliableSequenced 95% 도달.
- **256KB + 1MB payload 분할 송수신** (★ v2 — u16 fragment count 검증).

### M3 합격 (★ v2)
- 5v5 룸 평균 snapshot < 200B (delta + AOI).
- AOI 다른 라인 entity Snapshot 미포함.
- baseline 미일치 → 자동 full snapshot (resync 무봉).
- ★ idle client 5초 후 `AckOnlyHeartbeat` → server baseline 갱신.

### M4 합격 (★ v2)
- 100ms RTT BA 인정 / **>200ms RTT reject** (kMaxRewindMs=200 단일).
- replay 시 hit 결과 동일.

### M5 합격 (★ v2)
- 100ms RTT → 입력 즉시 반응.
- mispredict snap-to 없음 (lerp).
- RNG bit-equal.
- ★ predicted world 에 RenderComponent/FxBillboardComponent 없음 (grep 검증).
- ★ `Client.vcxproj` `Precise` 유지 확인.

### M6 합격
- 같은 seed + input log → 2 process 5분 시뮬 hp/pos bit-equal.
- 회귀 grep 12종 allowlist 외 0 hit.
- replay tool 로 종료된 게임 재시뮬 OK.

### Demo (M1 합격 후)
- 2 client (PC + 노트북 LAN) → 한 쪽 이동/스킬 → 다른 쪽 화면 reflected.

### Alpha (M3 합격 후)
- 5v5 LAN 부하.

### Beta (M5 합격 후)
- 100ms+ RTT external user.

---

## 11. Anti-pattern (★ v2 — 추가 항목)

1. **TCP retain → UDP 위 reliable ordered 만**: 그냥 TCP. **3 channel 분리** 본질.
2. **Snapshot 매 tick full state**: 30Hz × 19KB = 4.5 Mbps / client. 100 동접 시 450 Mbps.
3. **Lag comp 무한 rewind**: lag exploit. **kMaxRewindMs=200 (v2 단일)** + suspicion.
4. **Client predicted = server 무시**: server authoritative 결과 우선.
5. **`std::unordered_map` sim 내부**: ★ M1 부터 grep guard. lookup-only allowlist 외 사용 금지.
6. **`time(nullptr)` sim 내부**: ★ M1 부터 grep guard.
7. **`/fp:fast`**: ★ M1 부터 grep guard.
8. **`std::sort` sim 내부 (tie-breaker 없이)**: ★ v2 — 동일 key 가능 정렬은 `std::stable_sort` + tie-breaker. unique `EntityID` 정렬만 allowlist.
9. **새 alias 도입** (CONVENTIONS §1.4 위반).
10. **`std::vector<T> Get()` DLL export 반환**: C4251.
11. **`make_unique` private ctor 클래스**: CONVENTIONS §5.2 위반.
12. **★ v2 — Prediction world 에 RenderComponent 복제**: 비결정 + 메모리 폭발. sim-only subset 만.
13. **★ v2 — Snapshot.fbs 에 SnapshotDelta 단순 추가** (root verify 안전 X). `SnapshotEnvelope` discriminator 필수.
14. **★ v2 — Fragment u8/u8** (256 한도): 1MB payload (~870 fragment) 처리 못함. **u16/u16**.
15. **★ v2 — Ack base = last contiguous**: selective ack 안 됨. **ackSeq = newest received** + bitfield.

---

## 12. 향후 확장 (★ v2 — Sim-11 Projectile + Encryption 분리)

| Phase | 영역 | sub-plan | 비고 |
|---|---|---|---|
| **Sim-11** | **Projectile System** | `.md/plan/sim/11_PROJECTILE_SYSTEM.md` | ★ Gameplay — server 권위 투사체 (Ezreal Q / Lux Q / Yasuo Q3 등) |
| **Sim-19** | **Encryption** (ChaCha20+ECDH 또는 DTLS 1.3) | (추후) | ★ v2 — M2 에서 분리. 11 슬롯이 Projectile 로 점유되어 19 로 이동 |
| Sim-12 | Fixed-point math (q16.16) | (추후) | cross-platform replay |
| Sim-13 | BBR / AIMD congestion | (추후) | M2 의 fixed CWND 대체 |
| Sim-14 | Server Orchestration (Agones + K8s) | (추후) | LoL L5 |
| Sim-15 | Region routing + Service mesh | (추후) | LoL L7 |
| Sim-16 | Replay-based detection (ML) | (추후) | wallhack/aimbot 사후 분석 |
| Sim-17 | Behavioral fingerprinting | (추후) | ban evasion |
| Sim-18 | Vanguard-style kernel anti-cheat | (추후) | Security PART4 |

---

## 13. v1 → v2 변경 한 줄

**v1 의 큰 축 (UDP/Reliability/Delta/AOI/LagComp/Prediction/Replay) 유지. Codex 보정 7건: ① ackSeq=newest (selective ack 명확) ② Fragment 별도 header + u16/u16 + 1MB 지원 ③ SnapshotEnvelope root discriminator (FlatBuffers verify 안전) ④ stable_sort + /fp:precise 등 결정성 가드를 M6 → M1 로 전진 ⑤ Encryption M2 에서 빼고 Sim-11 별도 ⑥ Prediction world = sim-only subset (Render/FX 제외) ⑦ repo 기준 lookup-only allowlist 외 0 hit 로 grep guard 현실화. 마일스톤 시간: M1 60→50h, M2 40→30h (crypto 제거), M6 30→20h. 2-client demo 는 M1 합격 후 가능, 5v5 LAN alpha 는 M3 합격 후, external beta 는 M5 합격 후.**
