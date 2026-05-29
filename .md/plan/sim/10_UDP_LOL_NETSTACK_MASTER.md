# Phase Sim-10 — UDP LoL-Authoritative NetStack Master Plan

> ⚠️ **DEPRECATED — 2026-04-30**
>
> 본 v1 문서는 **Codex 7건 보정 후 v2 로 대체**. 신규 작업은 **`10_UDP_LOL_NETSTACK_MASTER_v2.md`** 참조.
>
> v1 의 **sub-plan 링크 (10a/10b/10c/10d/10e/10f) 는 v2 의 `10_v2_M{N}_*` 로 모두 rename** 됨:
> - `10a_M1_UDP_TRANSPORT.md` → `10_v2_M1_UDP_TRANSPORT.md`
> - `10b_M2_UDP_RELIABILITY.md` → `10_v2_M2_UDP_RELIABILITY.md`
> - `10c_M3_DELTA_AOI.md` → `10_v2_M3_DELTA_AOI_VISION.md`
> - `10d_M4_LAG_COMP.md` → `10_v2_M4_LAG_COMPENSATION.md`
> - `10e_M5_PREDICTION.md` → `10_v2_M5_CLIENT_PREDICTION.md`
> - `10f_M6_DETERMINISM_REPLAY.md` → `10_v2_M6_DETERMINISM_REPLAY.md`
>
> 본 문서는 **history 보존용**. 작업 진행 시 v2 master 우선.

**작성일**: 2026-04-30 (deprecated 2026-04-30)
**범위**: Server/Shared/Client 의 네트워크 + 결정성 + Snapshot/AOI + Prediction 전 영역
**전제**: ① v1.2 `04_IOCP_GAMEROOM.md` (TCP IOCP) 는 1차 prototype 으로 남기고, 본 계획서가 production 방향 ② Backend Phase 0-8 동작 (Auth/Match/Profile/Shop/Payment/Leaderboard) ③ Shared/GameSim + Schemas 박제 완료 ④ Server 코드는 sketch 상태 (FrameParser 39줄, IOCPCore 77줄, 나머지 0)

**한 줄 명제**: **TCP→UDP 전환 + 자체 Reliability Layer + Snapshot Delta + AOI + Lag Compensation + Client Prediction + 결정성 보강 = LoL 본체의 5%→30% 도약. 6 마일스톤 (M1-M6), Demo→Alpha→Beta 단계별 종료 조건.**

---

## 0. 이번 계획서가 v1.2 와 다른 점

| 영역 | v1.2 (`04_IOCP_GAMEROOM.md`) | 본 계획서 (Sim-10) |
|---|---|---|
| Transport | TCP IOCP | **UDP + 자체 Reliability** (LoL 실제 구조) |
| Channel | 단일 stream | **3 channel** (Reliable Ordered / Reliable Unordered / Unreliable Sequenced) |
| Snapshot | full state | **delta 압축 + baseline ack** |
| AOI | 50m grid (1차) | **grid + interest management + 정밀도 LOD** |
| Lag comp | history record + 200ms reject | **rewind hit validation** |
| Prediction | 미언급 | **Client prediction + Server reconciliation + render lerp** |
| 결정성 | 단일 thread tick | **+ fixed-point or /fp:precise + RNG sub-stream + sorted iteration** |
| Replay | 미언급 | **input log + tick replay** (anti-cheat 기반) |
| 분량 목표 | 360줄 outline | **본 마스터 + M1-M6 별 박제 시점에 sub plan** |

**v1.2 의 위치**: M1 (Transport baseline) 의 reference. M1 진입 시 v1.2 의 Session/Manager/Dispatcher/GameRoom 골격을 UDP 로 포팅. 단 4G LagComp / 4F SnapshotBuilder 는 본 계획서의 M3/M4 가 본격화.

---

## 1. 종합 진단 — 어디까지 왔고 무엇이 빠졌나

### 1.1 기존 markdown 인벤토리

**sim 폴더 (4개)**:
- `00_SHAREDGAMESIM_MASTER_PLAN.md` — Sim-1 Determinism / Sim-2 5축 / Sim-3 Schema / Sim-4 IOCP / Sim-5~9 마스터
- `03_SCHEMA_FLATBUFFERS.md` — FlatBuffers Command/Snapshot/Event 박제
- `04_IOCP_GAMEROOM.md` — v1.2 TCP IOCP (360줄)
- `05_TO_09_REMAINING_OUTLINE.md` — Prediction / Backend Skin / Bot / MCTS / RL

**backend 폴더 (8개)**:
- `00_BACKEND_PLAN_INDEX.md` 외 Phase 0-8 — **이미 동작 확인** (포트 8081-8086)

**security 폴더 (8개)**:
- `00_SECURITY_INDEX.md` + Level 0-5 + Red Team — 향후 anti-cheat 통합 reference

**architecture 폴더 (4개)**:
- `WINTERS_ENGINE_ARCHITECTURE_FINAL.md`, `WINTERS_ENGINE_CONVENTIONS.md`, `WINTERS_GAMEPLAY_ARCHITECTURE.md`, `CLASS_DAY8_VS_WINTERS.md`

### 1.2 코드베이스 현재 상태

**Shared/ — 잘 박제됨 ✅**
| 파일 | 상태 |
|---|---|
| `Shared/Network/PacketEnvelope.h` | 16바이트 header, magic=0x5742, ePacketType 7종, `WrapEnvelope`/`TryExtractFrame` 헬퍼 — TCP framing 가정 |
| `Shared/GameSim/EntityIdMap.h` | NetEntityId↔EntityID 양방향 |
| `Shared/GameSim/DeterministicRng.h` | XorShift64* + sub-stream `MakeSubSeed` |
| `Shared/GameSim/Systems/ICommandExecutor.h` | TickContext, GameCommandWire, GameCommand, BuildServerCommand, CDefaultCommandExecutor |
| `Shared/GameSim/Components/*` | Stat/Health/Skill/Buff/PendingHit 등 결정성 component 박제 |
| `Shared/Schemas/Command.fbs` | CommandKind 9종, CommandPacket, CommandBatch |
| `Shared/Schemas/Snapshot.fbs` | EntitySnapshot 16 필드, **`deltaBaseTick:ulong` 자리만 있고 본격 미구현** |
| `Shared/Schemas/Event.fbs` | (있음, 본격 emit 미구현) |
| `Shared/Schemas/Generated/cpp/` + `go/` | 양쪽 codegen 산출물 OK |

**Server/ — 대부분 stub ⚠️**
| 파일 | 줄수 | 상태 |
|---|---|---|
| `Server/Public/Network/IOCPCore.h` | ~50 | sketch + typo (`workterCount`) + IOContext에 `acceptSocket` 없음 |
| `Server/Private/Network/IOCPCore.cpp` | 77 | 부분 구현 |
| `Server/Public/Network/FrameParser.h` | 22 | `TryExtract(ParsedFrame&, bool&)` — consume 정책 미완성 |
| `Server/Private/Network/FrameParser.cpp` | 39 | 부분 구현 |
| `Server/Public/Network/Session.h` | 40 | 멤버만, public API 없음 |
| `Server/Private/Network/Session.cpp` | 0 | 빈 파일 |
| `Server/Public/Network/Session_Manager.h` | 0 | 빈 |
| `Server/Public/Network/PacketDispatcher.h` | 0 | 빈 |
| `Server/Public/Game/GameRoom.h` | ~50 | sketch + typo (`m_tickIndezx`) + raw `pSession` |
| `Server/Public/Game/GameLogic.h` / `ServerWorld.h` / `AOI.h` | 0 | 빈 |
| `Server/Public/Game/SnapshotBuilder.cpp` | 20 | cpp 안에 class 박힘 (헤더 없음) |
| `Server/Public/Security/AntiCheatServer.h` / `LagCompensation.h` | 0 | 빈 |
| `Server/Private/main.cpp` | 39 | placeholder (콘솔 출력만) |
| `Server/Include/Server.vcxproj` | 존재 | `/fp:precise` / `Mswsock.lib` / Engine link 미설정 |

**Client/ Network — 거의 stub ⚠️**
| 파일 | 상태 |
|---|---|
| `Client/Public/Network/AuthClient.h` 등 6개 HTTP client | stub (Backend 와 통신 미박제) |
| `Client/Public/Network/CommandSerializer.h` | stub (input → wire 변환) |
| `Client/Public/Network/SnapshotApplier.h` | stub (snapshot 수신 → world 반영) |
| Client UDP / Prediction / Reconciliation | **존재 안함** |

### 1.3 LoL 토폴로지 7-8 레이어 매핑

```
┌──────────────────────────────────────────────────────┐
│ L8  Anti-cheat 통합 / Replay / 분쟁 시스템             │ ← Sim-9 + Security Level 5
├──────────────────────────────────────────────────────┤
│ L7  Region routing / Service mesh / Observability     │ ← 향후
├──────────────────────────────────────────────────────┤
│ L6  Matchmaking + Auth + Profile + Shop + Ranking     │ ← Backend Phase 1-7 ✅
├──────────────────────────────────────────────────────┤
│ L5  Server Orchestration (K8s + Agones)               │ ← 향후
├──────────────────────────────────────────────────────┤
│ L4  UDP + Reliability + Delta + AOI 고급              │ ← ★ 본 계획서 M1-M3
├──────────────────────────────────────────────────────┤
│ L3  Lag compensation + Client prediction              │ ← ★ 본 계획서 M4-M5
├──────────────────────────────────────────────────────┤
│ L2  결정론적 시뮬레이션 (fixed-point, container)        │ ← ★ 본 계획서 M6 (or 병행)
├──────────────────────────────────────────────────────┤
│ L1  GameRoom Tick (단일 thread, 결정성)                │ ← v1.2 04 ▲
│ L1  PacketDispatcher (framing, verify, route)         │ ← v1.2 04 ▲
│ L1  Session / SessionManager                          │ ← v1.2 04 ▲
│ L1  IOCP/UDP Worker Pool                              │ ← v1.2 04 ▲
└──────────────────────────────────────────────────────┘
```

**현재 위치**: L1 sketch + L6 동작. **목표**: L1 본격 (UDP 로) + L2 + L3 + L4 + L5 본격 = LoL 본체의 ~30%.

---

## 2. UDP 전환 — 왜 TCP 로는 LoL 못 되나

### 2.1 TCP 의 치명적 한계 — Head-of-Line Blocking

TCP 는 in-order delivery 를 보장한다. 패킷 1개 손실 → ACK 안 옴 → 그 뒤 모든 패킷이 OS recv 버퍼에 쌓이지만 **app 에 전달 안됨**. 100ms RTT 환경에서 1% packet loss 만 있어도:
- 평균 stall time = `~RTT × loss_rate × 후속_패킷_수` = 수백 ms
- 30Hz tick = 33ms 주기. **TCP 한 번 막히면 1-3 tick 동안 입력이 안 들어옴 = 캐릭터 frozen**

### 2.2 LoL 실제 구조

LoL 은 **ENet 기반 자체 UDP reliability layer** 사용 (공개 분석). 핵심:
- **3 channel 분리**: 입력 (reliable ordered) / 스킬 cast (reliable unordered) / 위치 broadcast (unreliable sequenced)
- **per-channel sequence + ack** — 한 channel 손실이 다른 channel 막지 않음
- **선택적 retransmit** — unreliable channel 은 손실 시 그냥 폐기 (다음 snapshot 이 덮어씀)
- **DTLS or 자체 stream cipher** — 위변조 방지

### 2.3 UDP 도입 시 얻는 것

| 효과 | 메커니즘 |
|---|---|
| HoL blocking 제거 | per-channel sequence 분리 |
| 30Hz snapshot 손실 허용 | 다음 snapshot 이 모든 entity 덮어씀 (불멸 메모리 X) |
| 입력 retransmit | reliable channel 만 재전송 (전체 stream X) |
| MTU-aware fragment | 1400 byte 단위 자체 분할 (TCP 의 nagle / delayed ACK 회피) |
| congestion control | 자체 RTO/CWND — 게임 워크로드에 맞게 |

### 2.4 TCP 유지가 합리적인 영역

- **Hello / Auth / 로그인** — 1회성, 큰 payload OK, 신뢰 필수 → TCP 또는 HTTP (이미 Backend Phase 1-7 이 HTTP)
- **Match lock-in / Game session config** — Backend → Game Server 통신
- **In-game chat (전체 채팅)** — reliable 필수, latency 덜 민감

→ **하이브리드**: 게임 진입 전 TCP/HTTP, 게임 진입 후 UDP. 현재 Backend Phase 1-7 가 이미 TCP/HTTP 사용 중이므로 **그대로 유지**.

---

## 3. UDP Reliability Layer 설계 (★ M1-M2 핵심)

### 3.1 3-channel 분리

```cpp
enum class eUdpChannel : u8_t
{
    ReliableOrdered    = 0,  // 입력 (CommandBatch) — 순서 보장 + 재전송
    ReliableUnordered  = 1,  // Event (kill, level up) — 재전송, 순서 무관
    UnreliableSequenced= 2,  // Snapshot — 최신만 (오래된 무시), 재전송 X
};
```

### 3.2 패킷 envelope 갱신 (Shared/Network/PacketEnvelope.h)

기존 `PacketHeader` (16 bytes) 에 channel + per-channel seq 필드 추가:
```cpp
#pragma pack(push, 1)
struct UdpPacketHeader  // ★ Sim-10 신규 — TCP envelope 와 별개
{
    uint16_t magic;           // 0x5742
    uint8_t  version;         // 2 (TCP=1 와 구분)
    uint8_t  channel;         // eUdpChannel
    uint16_t type;            // ePacketType (CommandBatch/Snapshot/Event/...)
    uint16_t flags;           // Compressed/Encrypted/Fragment/AckOnly
    uint32_t channelSeq;      // per-channel sequence (overflow safe wrap)
    uint32_t ackBase;         // last contiguously-received seq (sliding window 시작)
    uint32_t ackBitfield;     // 32 bit — base 이전 32개 패킷 ack 마스크
    uint16_t payloadSize;     // bytes
    uint16_t fragmentInfo;    // fragmentIndex(8) | fragmentCount(8) — 0xFFFF=non-frag
};                            // = 24 bytes
#pragma pack(pop)
static_assert(sizeof(UdpPacketHeader) == 24);
```

### 3.3 Sequence + Ack (sliding window)

```
세션 별 send 측:
  - nextSendSeq[3 channel] = {1, 1, 1}
  - sentBuffer[channel] = circular ring (256 entries) — 재전송 대기

세션 별 recv 측:
  - lastRecvSeq[3 channel]
  - recvBitfield[channel] = uint32 (지난 32 packet)
  - 매 send 시 piggyback: header.ackBase + ackBitfield
```

**Ack 처리**:
- Sender 가 packet 보낼 때 piggyback ack 박는다.
- Recv 시 ackBase + bitfield 로 sentBuffer 의 해당 entry 제거.
- RTO 초과 (200ms) entry 는 재전송 (reliable channel only).

### 3.4 Re-transmit + Congestion Control (1차)

**RTO**: SRTT (smoothed RTT) + 4×RTTVAR (TCP 와 동일 공식) — `(SRTT + max(50ms, 4*RTTVAR))`. 최소 50ms / 최대 500ms.

**CWND**: 1차는 fixed window (32 packet) — 게임 입력은 30Hz × 32 byte ≈ 1KB/s 라 congestion 거의 안 남. 추후 BBR or AIMD 도입.

**Re-transmit 정책**:
- ReliableOrdered: stall — 손실 발견 시 재전송 + 그 뒤 packet 유보
- ReliableUnordered: 손실 발견 시 재전송 + 그 뒤 packet 즉시 전달
- UnreliableSequenced: 손실 무시 + 다음 더 새 packet 받으면 ok

### 3.5 Fragment + Reassembly

Snapshot 큰 경우 (>1400 byte) — MTU-aware fragment 필요. fragmentInfo 8/8 분할.

**위험**: UDP fragment 는 IP layer fragment 가 아닌 **app layer fragment**. IP fragment 는 NAT/방화벽 손실 위험 → 항상 app layer 분할.

### 3.6 Encryption

- 1차 (M1): plaintext (loopback test 위주)
- 2차 (M2 후반): 자체 ChaCha20 stream cipher + per-session key (handshake 시 ECDH 교환)
- 3차 (Beta): DTLS 1.3 통합 (libdtls 또는 mbedTLS)

**WARNING**: 자체 cipher 는 nonce reuse 시 catastrophic. ChaCha20 nonce = `(sessionId, channel, channelSeq)` 조합 — 절대 reuse X.

---

## 4. 결정성 보강 (★ M6 — Sim-1F grep 12종 이후)

### 4.1 Float vs Fixed-point

LoL replay 는 픽셀 단위 동일 결과 보장. `float` 은 CPU/컴파일러/optimization level 에 따라 ULP 차이 발생.

**옵션 A** (1차 채택): **strict IEEE 754 + `/fp:precise`**
- Server.vcxproj / Engine.vcxproj 모두 `/fp:precise` 강제
- `/fp:fast` 절대 X (80-bit register 정밀도 → memory truncate 시 비결정)
- SSE2 intrinsic 사용 시 `_mm_set_round_mode(MM_ROUND_NEAREST)` 명시
- `pow/sin/cos` 같은 transcendental 은 vendor lib 차이 → 자체 lookup table 또는 polynomial approx

**옵션 B** (장기): **fixed-point** (e.g. q24.8 또는 q16.16)
- 모든 게임플레이 좌표/속도/hp 를 i32 로
- 변환 cost 큼 → 1차는 옵션 A, ML/replay 본격 시 B 도입

### 4.2 RNG sub-stream 강제

`DeterministicRng` 는 이미 `MakeSubSeed(tickIndex, sourceEntity, skillId)` 박제됨. 단 강제 메커니즘 부재 → System 들이 자발적으로 sub-stream 호출.

**회귀 가드**:
```bash
# CI 체크: tc.pRng->NextU* 직접 호출 = fail
grep -rn "tc\.pRng->Next" Shared/GameSim/Systems/ \
    | grep -v "MakeSubSeed"
```

### 4.3 Container 순회 순서

| 컨테이너 | 결정성 | 사용 |
|---|---|---|
| `std::vector<T>` | ✅ insert 순서 | sim state |
| `std::map<K,V>` | ✅ key 정렬 | id-keyed lookup |
| `std::unordered_map<K,V>` | ❌ hash 순서 | sim 외 (cache, lookup-only) |
| `std::set<T>` | ✅ key 정렬 | active entity set |
| `std::unordered_set<T>` | ❌ hash 순서 | sim 외 |

**회귀 가드**:
```bash
grep -rn "unordered_map\|unordered_set" Shared/GameSim/ Server/
# 결과 발견 시 sim state 인지 검토
```

`DeterministicEntityIterator<T>::CollectSorted` 는 이미 박제 — System 진입 시 항상 사용.

### 4.4 Logical time 만 사용

Sim 내부에서 `std::chrono::now()`, `time(nullptr)`, `GetTickCount()` 호출 금지. 항상 `tc.tickIndex` 또는 `DeterministicTime::TickToSec(tickIndex)`.

**회귀 가드**:
```bash
grep -rn "chrono::\|GetTickCount\|time(0\|time(nullptr" \
    Shared/GameSim/ Server/Private/Game/ Server/Private/Security/
```

### 4.5 std::sort tie-breaker

`std::sort` 는 동일 key 순서 미보장. Sim 내부 정렬은 항상 `std::stable_sort` + tie-breaker:
```cpp
std::stable_sort(commands.begin(), commands.end(),
    [](const auto& a, const auto& b) {
        if (a.serverAcceptedTick != b.serverAcceptedTick)
            return a.serverAcceptedTick < b.serverAcceptedTick;
        if (a.sessionId != b.sessionId)
            return a.sessionId < b.sessionId;
        return a.sequenceNum < b.sequenceNum;
    });
```

### 4.6 Fix-point 수학 헬퍼 (장기 — 본 계획서 후속)

`Shared/GameSim/FixedMath.h` 신규 (Sim-11 후속) — q16.16 add/sub/mul/div + `FixedSin/Cos/Sqrt` LUT.

---

## 5. Snapshot Delta + AOI (★ M3)

### 5.1 Baseline ack 방식

**현 Snapshot.fbs 의 `deltaBaseTick:ulong`** 은 자리만 있음. 본격 박제:
- Server 가 매 client 별로 **마지막 ack-된 baseline tick** 추적
- Snapshot 송신 시 baseline 과의 **diff 만** 전송 (변경된 EntitySnapshot 필드 + 사라진 entity list)
- Client 가 snapshot 수신 시 ack 응답 (channel: ReliableOrdered piggyback)
- Server 가 ack 받으면 해당 baseline 까지 메모리 회수

### 5.2 Delta payload 구조 (Snapshot.fbs 갱신)

```
namespace Shared.Schema;

table EntitySnapshotDelta {
    netId:uint;
    fieldMask:uint;                // bit 0 = hp, 1 = mana, 2 = posXYZ, 3 = yaw, ...
    hp:float;                      // (optional, fieldMask bit 가 1 이면 valid)
    mana:float;
    posX:float; posY:float; posZ:float;
    yaw:float;
    moveSpeed:float;
    animId:ushort; animPhaseFrame:ushort;
    skillCooldowns:[float];        // 길이 0 = 변경 없음
    ...
}

table SnapshotDelta {
    serverTick:ulong;
    serverTimeMs:ulong;
    rngState:ulong;
    baselineTick:ulong;            // 0 = full snapshot
    addedEntities:[EntitySnapshot];   // baseline 에 없던 새 entity
    changedEntities:[EntitySnapshotDelta];
    removedNetIds:[uint];
    lastAckedCommandSeq:uint;
    yourNetId:uint;
}
```

**대역폭 추정**:
- 5v5 = 10 entity × 64 bytes (full) ≈ 640B / snapshot
- delta = 평균 30% entity 변경 × 32 bytes ≈ 100B / snapshot
- **6.4× 대역폭 절감**

### 5.3 AOI (Area of Interest) — grid + interest management

**1차** (M3): 50m grid bucketing — `CAOI::Rebuild(world)` 매 tick / `ComputeVisibleSet(self, world, outSet)` 3×3 cell 범위.

**2차** (M3 후반):
- **시야 차폐** — Vision System (와드/부쉬/벽). 1차 grid 결과를 vision filter 로 한 번 더.
- **정밀도 LOD**:
  - 가까운 entity (< 10m): 모든 필드 정밀
  - 중간 (10-30m): pos quantized 8-bit per axis
  - 멀리 (> 30m): pos quantized 4-bit per axis + animId 만

### 5.4 bandwidth 목표

| 시나리오 | 1 client 입력 | 1 client 수신 | 5v5 룸 / 서버 | 100 동접 (10 룸) |
|---|---|---|---|---|
| 30Hz delta + AOI | 0.3 KB/s | 4 KB/s | 40 KB/s | 400 KB/s = 3.2 Mbps |
| Full snapshot (현 v1.2) | 0.3 KB/s | 19 KB/s | 190 KB/s | 1.9 MB/s = 15 Mbps |

→ **delta + AOI 로 80% 대역폭 절감**.

---

## 6. Lag Compensation + Client Prediction (★ M4-M5)

### 6.1 Server-side rewind (M4)

기존 v1.2 04 §9 의 `CLagCompensation::RecordHistory` 는 position 만. Sim-10 M4 에서 확장:
- `Frame` 에 position + hp + bIsDead 저장
- `GetHistoricalState(entity, pastTickIndex)` 로 rewind
- **Hit validation**: 클라가 BA hit 보고 시 `clientTick` 시점의 target 위치/hp 로 검증
- **200ms 한도**: 그 이상 = lag exploit reject (v1.2 동일)

### 6.2 Client Prediction (M5 — `05_TO_09_REMAINING_OUTLINE.md` Sim-5 본격화)

**목표**: 100ms RTT 환경에서 사용자 perceived latency = 0.

**구조**:
```
입력 (W 키) → ClientInputBuffer.Push(wire, clientTick)
            → CPredictionWorld.ExecuteCommand(...)         // 즉시 시각 반영
            → Network.Send(CommandBatch, channel=ReliableOrdered)
            
Snapshot 수신 → CSnapshotApplier.Apply(snap, predictedWorld)
             → m_localRng.SetState(snap.rngState)          // 결정성 동기화
             → CRollbackEngine.Rebuild(lastAckedSeq, ...)   // 입력 재실행
             → CRenderInterpolator.BeginInterpolation(...)  // 시각 jitter 마스킹
```

### 6.3 Render Interpolation (M5)

**목표**: Server snapshot 30Hz → Client render 144Hz 부드럽게.

- Snapshot 수신 시 `RenderInterpolator` 에 truth 기록
- Render frame 마다 `Tick(visualWorld, dt)` — 100ms lerp 로 visualWorld 갱신
- **Mispredict 시 snap-to 가 아니라 lerp** — 사용자 시각 jitter < 2 frame

### 6.4 결정성 동기화

Client 의 `m_localRng` 는 매 snapshot 마다 `SetState(snap.rngState)` 로 server 와 bit-equal 동기화. 입력 재실행 시 같은 RNG 시퀀스 → 같은 결과 → mispredict 0 (이상적).

---

## 7. 변경/신규 파일 list

### 7.1 Shared 측

| 파일 | 종류 | M |
|---|---|---|
| `Shared/Network/UdpPacketHeader.h` | 신규 | M1 |
| `Shared/Network/UdpChannel.h` | 신규 (eUdpChannel enum) | M1 |
| `Shared/Schemas/Snapshot.fbs` | 갱신 — `SnapshotDelta` table 추가 | M3 |
| `Shared/Schemas/Event.fbs` | 갱신 — eEventKind 확장 (Kill/Level/Item) | M3 |
| `Shared/GameSim/FixedMath.h` | 신규 (q16.16) | M6 (선택) |
| `Shared/GameSim/Snapshot/DeltaBuilder.h/.cpp` | 신규 — baseline diff 계산 | M3 |
| `Shared/GameSim/Snapshot/DeltaApplier.h/.cpp` | 신규 — Client side delta merge | M3 |

### 7.2 Server 측

| 파일 | 종류 | M |
|---|---|---|
| `Server/Public/Network/UdpCore.h/.cpp` | 신규 (대체 IOCPCore — UDP 전용) | M1 |
| `Server/Public/Network/UdpReliabilityChannel.h/.cpp` | 신규 — per-channel sequence/ack | M2 |
| `Server/Public/Network/UdpSession.h/.cpp` | 신규 (대체 Session) | M1 |
| `Server/Public/Network/UdpSession_Manager.h/.cpp` | 신규 | M1 |
| `Server/Public/Network/UdpPacketDispatcher.h/.cpp` | 신규 | M1 |
| `Server/Public/Network/UdpFragmenter.h/.cpp` | 신규 — MTU-aware split/reassemble | M2 |
| `Server/Public/Network/UdpEncryption.h/.cpp` | 신규 — ChaCha20 + ECDH handshake | M2 후반 |
| `Server/Public/Game/GameRoom.h/.cpp` | v1.2 04 본격 (UDP 로 포팅) | M1 |
| `Server/Public/Game/SnapshotBuilder.h/.cpp` | header 분리 + delta 본격 | M3 |
| `Server/Public/Game/AOI.h/.cpp` | v1.2 04 §7 본격 + LOD | M3 |
| `Server/Public/Security/AntiCheatServer.h/.cpp` | v1.2 04 §8 본격 | M1 |
| `Server/Public/Security/LagCompensation.h/.cpp` | v1.2 04 §9 본격 + hit validation | M4 |
| `Server/Public/Game/ReplayLogger.h/.cpp` | 신규 — input log → 파일 | M6 |
| `Server/Private/main.cpp` | placeholder → bootstrap | M1 |
| `Server/Include/Server.vcxproj` | `/fp:precise` + `Mswsock.lib` + Engine ref | M1 |

### 7.3 Client 측

| 파일 | 종류 | M |
|---|---|---|
| `Client/Public/Network/UdpClient.h/.cpp` | 신규 — UDP send/recv + reliability mirror | M1 |
| `Client/Public/Network/CommandSerializer.cpp` | stub → 본격 (input → wire → batch) | M1 |
| `Client/Public/Network/SnapshotApplier.cpp` | stub → delta merge 본격 | M3 |
| `Client/Public/Network/ClientInputBuffer.h/.cpp` | 신규 (Sim-5 §5.2) | M5 |
| `Client/Public/Network/PredictionWorld.h/.cpp` | 신규 (Sim-5 §5.3) | M5 |
| `Client/Public/Network/RollbackEngine.h/.cpp` | 신규 (Sim-5 §5.4) | M5 |
| `Client/Public/Network/RenderInterpolator.h/.cpp` | 신규 (Sim-5 §5.5) | M5 |

### 7.4 Backend (Go) — 변경 없음 (이미 Phase 1-7 동작)

UDP 게임 서버는 Backend HTTP 와 별개. Match lock-in 시 `gameServerHost:port` 를 클라에 알려주면 끝.

---

## 8. Phase 분할 (M1-M6)

각 마일스톤은 **별도 sub-plan .md** 로 박제 (진입 시점에). 본 마스터는 **인터페이스 + 종료 조건** 만.

### M1 — UDP Transport Baseline (60 hour 추정)

**목표**: TCP IOCP (v1.2) → UDP 로 포팅. 단일 client 1 packet 송수신.

**작업**:
- `UdpCore` 박제 (WSARecvFrom/WSASendTo + IOCP 또는 RIO)
- `UdpSession` (sessionId 발급, sourceAddr 기반 lookup)
- `UdpSession_Manager` 
- `UdpPacketDispatcher` (envelope verify → CommandBatch FlatBuffers verify → room queue)
- `GameRoom` v1.2 04 §5 본격 (30Hz tick + Phase_Drain/Execute/Snapshot)
- `Server/main.cpp` bootstrap
- `Client/Public/Network/UdpClient` 신규
- AntiCheat 1차 (Move/Cooldown/Range/targetMode)

**종료 조건**:
- 1 client connect → 1 CommandBatch send → server tick → snapshot broadcast → client recv. 손실 0.
- 5분 30Hz tick jitter < 5ms.
- UDP localhost RTT < 1ms.

**별도 sub-plan**: `.md/plan/sim/10a_M1_UDP_TRANSPORT.md` (M1 진입 시 박제).

### M2 — Reliability Channel + Encryption (40 hour)

**목표**: 3 channel + sequence/ack + retransmit. 손실 5% / 지연 100ms 환경에서 입력 stable.

**작업**:
- `UdpReliabilityChannel` 박제 (per-channel ring + ack bitfield + RTO)
- `UdpFragmenter` (>1400B 분할/재조립)
- `UdpEncryption` ChaCha20 + ECDH handshake (M2 후반)
- 회귀 시나리오: clumsy 같은 net emulator 로 5%/10%/20% loss + 100ms RTT 시뮬

**종료 조건**:
- 5% loss + 100ms RTT 에서 ReliableOrdered channel 100% 도달 (재전송)
- UnreliableSequenced 는 손실 그대로 통과 (다음 snapshot 덮어씀)
- 1MB Snapshot 분할 송수신 OK
- 같은 sessionId + channel + seq 조합 nonce reuse 0건 (cipher 안전성)

**별도 sub-plan**: `.md/plan/sim/10b_M2_UDP_RELIABILITY.md`.

### M3 — Snapshot Delta + AOI (50 hour)

**목표**: 대역폭 80% 절감. 5v5 = 4 KB/s/client.

**작업**:
- `Shared/Schemas/Snapshot.fbs` 갱신 (`SnapshotDelta` 추가)
- `Shared/GameSim/Snapshot/DeltaBuilder` (server side)
- `Shared/GameSim/Snapshot/DeltaApplier` (client side)
- `CAOI` 본격 (50m grid + 3×3 cell 범위)
- LOD (정밀도 quantization — 거리 기반)
- baseline ack 메커니즘 (client → server, ReliableOrdered piggyback)

**종료 조건**:
- 5v5 룸 평균 snapshot < 200B (delta + AOI)
- baseline 미일치 시 자동 full snapshot 회귀 (resync)
- 100 동접 10 룸 부하 시 서버 outbound < 5 Mbps

**별도 sub-plan**: `.md/plan/sim/10c_M3_DELTA_AOI.md`.

### M4 — Lag Compensation (30 hour)

**목표**: 100ms RTT 클라가 BA hit 보고 시 서버가 rewind 검증. 250ms 초과 reject.

**작업**:
- `CLagCompensation::RecordHistory` 본격 (200ms = 6 frame, position + hp + bIsDead)
- `GetHistoricalState(entity, pastTickIndex)` API
- `IsTooOld(pastTick, currentTick)` guard
- BA hit validation 통합 (서버 권위)

**종료 조건**:
- 100ms RTT BA → 클라 본 시점 target 위치 hit 인정
- 250ms RTT → reject + suspicion ++
- 6 frame 보존 메모리 < 5 KB / 룸

**별도 sub-plan**: `.md/plan/sim/10d_M4_LAG_COMP.md`.

### M5 — Client Prediction + Reconciliation (60 hour)

**목표**: 100ms RTT 에서 사용자 perceived latency = 0. visual jitter < 2 frame.

**작업** (`05_TO_09_REMAINING_OUTLINE.md` Sim-5 본격):
- `ClientInputBuffer` (120 frame ring)
- `CPredictionWorld` (local copy of server world)
- `CRollbackEngine` (snapshot 수신 → lastAckedSeq 이후 입력 재실행)
- `CRenderInterpolator` (snap-to 회피, 100ms lerp)
- RNG 동기화 — `m_localRng.SetState(snap->rngState())`

**종료 조건**:
- 100ms RTT → 캐스트 즉시 반응 (client predicted)
- 200ms 후 server authoritative 결과 무봉 보정
- 250ms RTT → rollback 시각 jitter < 2 frame
- mispredict 비율 < 5% (정상 환경)
- bit-equal — 같은 input + seed → client predicted = server authoritative

**별도 sub-plan**: `.md/plan/sim/10e_M5_PREDICTION.md`.

### M6 — 결정성 보강 + Replay (30 hour)

**목표**: 같은 seed + input log → 5분 시뮬 hp/pos bit-equal. anti-cheat 회귀 가드 12종.

**작업**:
- `/fp:precise` 강제 (Engine + Server vcxproj)
- `Shared/GameSim/Components/*.h` 전수 `static_assert(std::is_trivially_copyable_v<T>)` (Sim-2 약속)
- RNG sub-stream 강제 grep (CI hook)
- container 결정성 grep
- logical time 강제 grep
- `std::stable_sort` + tie-breaker 강제
- `Server/Public/Game/ReplayLogger` (input log → 파일)
- replay 도구: input log + seed → 시뮬 재실행 → 결과 비교

**종료 조건**:
- 같은 seed + 같은 input log → 2 룸 5분 시뮬 hp/pos/rngState/snapshot hash 100% 일치
- 회귀 grep 12종 0 hit
- replay tool 로 종료된 게임 재시뮬 OK
- (선택) fixed-point 도입 후 cross-platform replay

**별도 sub-plan**: `.md/plan/sim/10f_M6_DETERMINISM_REPLAY.md`.

---

## 9. 의존 그래프

```
Backend Phase 1-7 (HTTP) ✅
        │
        ▼
   Sim-1 ~ Sim-3 (Determinism + 5축 + Schema) ✅ 부분
        │
        ▼
   ┌────────────────────────────┐
   │       M1 — UDP Transport   │   v1.2 04 의 TCP 골격을 UDP 로 포팅
   └────────────────────────────┘
        │
        ▼
   ┌────────────────────────────┐
   │   M2 — Reliability + Crypto│
   └────────────────────────────┘
        │
   ┌────┴────┐
   ▼         ▼
M3 Delta   M4 Lag Comp
+ AOI      (병렬 가능)
   │         │
   └────┬────┘
        ▼
   ┌────────────────────────────┐
   │   M5 — Client Prediction   │
   └────────────────────────────┘
        │
        ▼
   ┌────────────────────────────┐
   │   M6 — Determinism + Replay│
   └────────────────────────────┘
        │
        ▼
   ┌────────────────────────────┐
   │   Sim-7/8/9 (Bot/MCTS/RL)  │   결정성 + Prediction 전제
   └────────────────────────────┘
```

**병렬 가능**: M3 ↔ M4 (서로 독립). 단 M5 는 M3 (delta apply) + M4 (rewind 검증) 결과 동시 활용.

---

## 10. 검증 시나리오 (단계별 통합)

### M1 합격
- localhost 1 client connect → 30Hz tick → snapshot loop. 5분 jitter < 5ms.

### M2 합격
- clumsy 5% loss + 100ms RTT → ReliableOrdered 100%, UnreliableSequenced 95% 도달 (5% 손실 OK).
- 1MB snapshot 분할 송수신.

### M3 합격
- 5v5 룸 평균 snapshot < 200B.
- AOI 다른 라인 entity Snapshot 미포함.
- baseline ack 정상 → full resync 자동.

### M4 합격
- 100ms RTT BA 인정 / 250ms RTT reject.
- replay 시 hit 결과 동일.

### M5 합격
- 100ms RTT → 입력 즉시 반응.
- mispredict snap-to 없음 (lerp).
- RNG bit-equal.

### M6 합격
- 같은 seed + input log → 2 process 5분 시뮬 hp/pos bit-equal.
- 회귀 grep 12종 (RNG 직접 / unordered iteration / chrono 사용 / stable_sort 누락 등) 0 hit.

### Demo (M1 합격 후)
- 2 client (PC + 노트북 LAN) connect → 한 쪽 이동/스킬 → 다른 쪽 화면 reflected.

### Alpha (M3 합격 후)
- 5v5 외부 LAN 부하 테스트.
- AOI / delta 효과 측정.

### Beta (M5 합격 후)
- 100ms+ RTT 환경 외부 user 테스트.
- Anti-cheat 1차 통합.

---

## 11. Anti-pattern (피할 것)

1. **TCP retain → UDP 위 reliable ordered 만 사용**: 이러면 그냥 TCP 다. **3 channel 분리** 본질이 핵심.
2. **Snapshot 매 tick full state**: 30Hz × 19KB = 4.5 Mbps / client. 100 동접 시 450 Mbps. delta 필수.
3. **Lag comp 무한 rewind**: lag exploit 의 정의. 200ms 한도 + suspicion.
4. **Client predicted 결과 = server 무시**: 이즈리얼 Q 가 클라에서는 hit / server 에서는 miss → 데미지 미적용. **server authoritative 결과 우선**.
5. **`std::unordered_map` sim 내부 사용**: order = hash 의존 = compiler/STL impl 의존 = 비결정. `std::map` 또는 `std::vector + std::sort`.
6. **`time(nullptr)` sim 내부 사용**: real time = wall clock = process 마다 다름. `tickIndex` 만 사용.
7. **`/fp:fast`**: 결정성 깨짐. 절대 X.
8. **새 alias 도입** (`AcceptEx_FxRoutine_t` 같은 typedef): WINTERS_ENGINE_CONVENTIONS §1.4 위반. 기존 alias 만 사용.
9. **`std::vector<T> Get()` DLL export 반환**: C4251. opaque handle 또는 out 파라미터.
10. **`make_unique` private ctor 클래스**: WINTERS_ENGINE_CONVENTIONS §5.2 위반. `unique_ptr<T>(new T())`.

---

## 12. 향후 확장 (M6 이후)

| Phase | 영역 | 비고 |
|---|---|---|
| Sim-11 | DTLS 1.3 통합 | mbedTLS or libdtls. M2 의 자체 cipher 대체 |
| Sim-12 | BBR / AIMD congestion | M2 의 fixed CWND 대체 |
| Sim-13 | Server Orchestration (Agones + K8s) | LoL L5. game server pool 동적 관리 |
| Sim-14 | Region routing + Service mesh | LoL L7. 한국 user → 한국 서버 latency |
| Sim-15 | Replay-based detection (ML) | 끝난 게임 사후 분석 wallhack/aimbot |
| Sim-16 | Behavioral fingerprinting | ban evasion 추적 |
| Sim-17 | Vanguard-style anti-cheat (kernel) | Security PART4 와 통합 |

---

## 13. 한 줄

**Sim-10 = TCP IOCP (v1.2 04) → UDP + 자체 Reliability Layer + Snapshot Delta + AOI + Lag Compensation + Client Prediction + 결정성 보강. 6 마일스톤 (M1 Transport / M2 Reliability / M3 Delta+AOI / M4 LagComp / M5 Prediction / M6 Determinism). M1 합격 = 2-client demo 가능. M3 합격 = 5v5 LAN alpha. M5 합격 = 100ms RTT external beta. LoL 본체 5%→30% 도약. 각 M 진입 시 별도 sub-plan .md 로 본격 박제.**
