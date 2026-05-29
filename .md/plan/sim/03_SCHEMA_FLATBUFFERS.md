# Phase Sim-3 — Schema FlatBuffers (Command / Snapshot / Event)

**작성일**: 2026-04-29
**v1.1 전제**: NetEntityId vs EntityID 분리 (C-2) + sequenceNum 무결성 + RNG state 직렬화 (결정성)
**진입 전제**: Sim-2 5 축 합격 — `StatComponent / DamageRequest / SkillRankComponent` 모두 `is_trivially_copyable + standard_layout` static_assert 통과
**목적**: 클라/서버/리플레이/AI 가 같은 직렬화 경계를 공유하는 3 채널 Schema 박제 — Command (Client→Server) / Snapshot (Server→Client 30Hz) / Event (Server→Client 즉시 비동기)

---

## 0. 본 사이클의 진짜 목표

직렬화는 **결정성의 1차 검증 도구**. Schema 가 박히면:
1. **클라/서버 sim 동일성** — 같은 byte 열을 양쪽이 읽고, 같은 결과
2. **리플레이 = Command 시퀀스 + 초기 RNG seed** 만 저장 (10MB / 60분)
3. **MCTS 의 World Clone** = Snapshot 직렬화 + 디시리얼라이즈로 구현 가능
4. **RL Env** = Snapshot 을 observation feature 로 변환

★ **POD 강제** = Sim-2 의 `is_trivially_copyable` static_assert 가 본 사이클에서 **자동 회귀 보장**. 누가 Component 에 pointer/virtual 추가 시도 → 빌드 실패 → Schema 박힘 보장.

---

## 1. 신규 인프라

| # | 인프라 | 위치 | 책임 |
|---|--------|------|------|
| I-1 | `flatc` 도구 | `Tools/Bin/flatc.exe` (Windows) | .fbs → .h / .go 코드젠 |
| I-2 | Schema 정의 | `Shared/Schemas/*.fbs` | 3 채널 정의 |
| I-3 | 코드젠 산출물 | `Shared/Schemas/Generated/<lang>/` | C++ 헤더 + Go 패키지 |
| I-4 | Codegen Pre-build event | `Shared/Schemas/run_codegen.bat` | MSBuild 통합 — 빌드 시 자동 재생성 |
| I-5 | `CSnapshotBuilder` | `Server/Game/SnapshotBuilder.h/.cpp` | EntityIdMap + visibleSet → flatbuffer 빌드 |
| I-6 | `CSnapshotApplier` | `Client/Private/Network/SnapshotApplier.h/.cpp` | flatbuffer → World 적용 + EntityIdMap 갱신 |
| I-7 | `CCommandSerializer` | `Client/Private/Network/CommandSerializer.h/.cpp` | GameCommandWire → flatbuffer 송신 |
| I-8 | `CCommandDispatcher` | `Server/Game/CommandDispatcher.h/.cpp` | flatbuffer → GameCommandWire → BuildServerCommand |
| I-9 | Schema Smoke | `Client/Private/GamePlay/SchemaSmoke.cpp` | round-trip + sequenceNum + AOI prep static_assert |
| I-10 | `PacketHeader` POD | `Shared/Network/PacketEnvelope.h` | ★ TCP framing — magic/version/type/flags/payloadSize/sequence |
| I-11 | `CFrameParser` | `Server/Network/FrameParser.h/.cpp` | per-session recv buffer + complete-frame extraction |

---

## 1.5 PacketEnvelope (★ Codex P0 보정 — TCP framing 1차 박제)

**문제**: TCP recv 는 FlatBuffer 1개 단위로 오지 않음. 임의 byte chunk (partial / sticky packet) 로 도착. raw bytes 를 곧장 `VerifyCommandBatchBuffer` 에 넣으면 첫날부터 "가끔 된다" 류 버그.

**해결**: PacketHeader (고정 16 bytes) + payload (FlatBuffer) 구조. session 별 recv buffer 누적 → header 길이 확인 → payload complete 확인 → verifier → dispatcher.

### 1.5.1 `Shared/Network/PacketEnvelope.h` (POD, FlatBuffer 외부)

```cpp
#pragma once
#include "WintersTypes.h"
#include <cstdint>

constexpr uint16_t kPacketMagic = 0x5742;     // 'WB' (Winters Binary)
constexpr uint16_t kPacketVersion = 1;

enum class ePacketType : uint16_t
{
    None = 0,
    CommandBatch = 1,     // Client -> Server (FlatBuffer payload)
    Snapshot = 2,         // Server -> Client 30Hz (FlatBuffer payload)
    Event = 3,            // Server -> Client (즉시 비동기, FlatBuffer payload)
    Hello = 10,           // 인증 / sessionId 발급
    Heartbeat = 11,
    Disconnect = 12,
};

enum ePacketFlags : uint16_t
{
    PacketFlag_None = 0,
    PacketFlag_Compressed = 1u << 0,    // 향후 LZ4
    PacketFlag_Encrypted = 1u << 1,     // 향후 TLS layer
};

#pragma pack(push, 1)
struct PacketHeader
{
    uint16_t magic;          // == kPacketMagic
    uint16_t version;        // == kPacketVersion (mismatch = reject)
    uint16_t type;           // ePacketType
    uint16_t flags;          // ePacketFlags
    uint32_t payloadSize;    // FlatBuffer byte 길이 (header 제외)
    uint32_t sequence;       // session 단조 증가 (Heartbeat 포함)
};
#pragma pack(pop)

static_assert(sizeof(PacketHeader) == 16, "PacketHeader must be 16 bytes for wire stability");
```

### 1.5.2 송신 흐름 (Server / Client 공통)

```cpp
// 송신: payload (FlatBuffer) 빌드 → header 채움 → 단일 send (header + payload)
inline std::vector<u8_t> WrapEnvelope(ePacketType type, u32_t sequence,
                                       const u8_t* payload, u32_t payloadLen)
{
    std::vector<u8_t> packet(sizeof(PacketHeader) + payloadLen);
    PacketHeader* hdr = reinterpret_cast<PacketHeader*>(packet.data());
    hdr->magic = kPacketMagic;
    hdr->version = kPacketVersion;
    hdr->type = static_cast<u16_t>(type);
    hdr->flags = PacketFlag_None;
    hdr->payloadSize = payloadLen;
    hdr->sequence = sequence;
    std::memcpy(packet.data() + sizeof(PacketHeader), payload, payloadLen);
    return packet;
}
```

### 1.5.3 수신 프레임 추출 (★ Sim-4 의 `CFrameParser` 가 사용)

```cpp
// recv 누적 buffer 에서 완전한 envelope 1개씩 추출. partial 은 보존.
struct ParsedFrame {
    ePacketType type;
    u32_t sequence;
    const u8_t* payload;
    u32_t payloadSize;
};

bool TryExtractFrame(const u8_t* buffer, u32_t available, ParsedFrame& out, u32_t& consumed);
//   return false  → partial (consumed = 0, 더 받기 대기)
//   return true   → 1 frame 추출. consumed = sizeof(header) + payloadSize
//   header 검증 실패 (magic/version/payloadSize 한도 초과) → connection 종료 (호출 측 책임)
```

★ **payloadSize 상한**: 64 KB (CommandBatch 충분). 초과 시 즉시 disconnect — DoS 1차 방어.

### 1.5.4 회귀 검증 (Sim-3F 단계 1차)

| 검증 | 합격 |
|------|------|
| sizeof(PacketHeader) == 16 | static_assert |
| 1 byte 씩 chunk 로 분할 송신 | 수신 측 정상 재조립 |
| 2 envelope 한 chunk 에 sticky | 수신 측 2개 모두 추출 |
| magic mismatch | TryExtractFrame false + disconnect 트리거 |
| payloadSize > 64KB | 즉시 disconnect |

---

## 2. Schema 본격

### 2.1 `Shared/Schemas/Command.fbs` (Client → Server)

```fbs
namespace Shared.Schema;

// ★ v1.1 — issuerEntity 송신 X. 서버가 sessionId 기반 결정
enum CommandKind : ubyte {
    None = 0,
    Move = 1,
    CastSkill = 2,
    BasicAttack = 3,
    LevelSkill = 4,
    BuyItem = 5,
    UseItem = 6,
    Recall = 7,
    Recall_Cancel = 8,
    Ping = 9,
}

struct Vec3 { x:float; y:float; z:float; }

table CommandPacket {
    kind:CommandKind;
    sequenceNum:uint;          // ★ 클라 입력 단조 증가 — 서버가 갭 시 reject
    clientTick:ulong;          // 클라 추정 시간 (대기시간 측정)
    slot:ubyte;                // 0=BA, 1=Q, 2=W, 3=E, 4=R
    targetNet:uint;            // ★ NetEntityId only (EntityID 송신 X)
    groundPos:Vec3;
    direction:Vec3;
    itemId:ushort;
    _pad:ushort;
}

// 배치 — 1 packet 에 N command (네트워크 오버헤드 감소)
table CommandBatch {
    commands:[CommandPacket];
    clientTimestampMs:ulong;
}

root_type CommandBatch;
```

### 2.2 `Shared/Schemas/Snapshot.fbs` (Server → Client, 30Hz)

```fbs
namespace Shared.Schema;

include "Command.fbs";   // Vec3 재사용

// ── Entity 단일 단편 ──
table EntitySnapshot {
    netId:uint;                // ★ NetEntityId (결정성)
    championId:ubyte;          // eChampion (= u8)
    team:ubyte;                // eTeam
    level:ubyte;
    hp:float;
    mana:float;
    posX:float;
    posY:float;
    posZ:float;
    yaw:float;
    moveSpeed:float;           // sim 결과 — 클라가 스탯 재계산 안 함
    animId:ushort;             // ★ Codex P1 보정 — string → u16 ID. 30Hz 대역폭/할당 비용 감소
    animPhaseFrame:ushort;     // 현재 anim 의 frame index (보간 단서)
    skillCooldowns:[float];    // [0..4] BA/Q/W/E/R
    skillRanks:[ubyte];        // [0..4]
    buffMask:uint;             // 활성 buff 비트마스크 (32종)
    statHash:uint;             // ★ 클라 예측 검증용 — StatComponent hash
}

// ── 1 frame snapshot ──
table Snapshot {
    serverTick:ulong;
    serverTimeMs:ulong;
    rngState:ulong;            // ★ 결정성 — 클라가 같은 seed 로 rollback
    entities:[EntitySnapshot]; // visibleSet 만 (AOI 컬링됨)
    lastAckedCommandSeq:uint;  // 본 클라가 처리한 마지막 seq
    yourNetId:uint;            // 본 클라의 own entity netId
    deltaBaseTick:ulong;       // 0 = full snapshot, 그 외 = delta from base
}

root_type Snapshot;
```

★ **delta 압축 (옵션)**: full snapshot 매 30 frame, 그 사이 27 frame 은 delta. 1차 박제는 full only — Sim-4 검증 통과 후 delta 도입.

### 2.3 `Shared/Schemas/Event.fbs` (Server → Client, 즉시 비동기)

```fbs
namespace Shared.Schema;

include "Command.fbs";

enum EventKind : ubyte {
    None = 0,
    Damage = 1,
    Heal = 2,
    Death = 3,
    Respawn = 4,
    BuffApply = 5,
    BuffExpire = 6,
    ProjectileSpawn = 7,
    ProjectileHit = 8,
    SkillCast = 9,
    SkillRankUp = 10,
    ItemBuy = 11,
    ItemSell = 12,
    LevelUp = 13,
    GameStart = 14,
    GameEnd = 15,
    KillFeed = 16,
    Chat = 17,
}

table DamageEvent {
    sourceNet:uint;
    targetNet:uint;
    amount:float;
    type:ubyte;               // eDamageType
    bWasCrit:bool;
    bKilled:bool;
    skillId:ushort;
}

table BuffApplyEvent {
    targetNet:uint;
    sourceNet:uint;
    buffDefId:uint;
    duration:float;
    stackCount:ubyte;
}

table ProjectileSpawnEvent {
    netId:uint;
    ownerNet:uint;
    kind:ushort;              // eProjectileKind
    startX:float; startY:float; startZ:float;
    dirX:float; dirY:float; dirZ:float;
    speed:float;
    maxDist:float;
}

table SkillCastEvent {
    casterNet:uint;
    slot:ubyte;
    rank:ubyte;
    targetNet:uint;
}

table EventPacket {
    kind:EventKind;
    serverTick:ulong;
    damage:DamageEvent;
    buffApply:BuffApplyEvent;
    projectile:ProjectileSpawnEvent;
    skillCast:SkillCastEvent;
}

root_type EventPacket;
```

★ **union vs nullable tables**: FlatBuffers union 이 더 효율적이지만 Go codegen 호환 단순화 위해 nullable tables 채택. 향후 union 으로 교체 가능.

---

## 3. flatc 도구 통합

### 3.1 `Tools/Bin/flatc.exe` 설치

```
1. https://github.com/google/flatbuffers/releases 에서 flatc-windows.exe 다운
2. Tools/Bin/flatc.exe 로 배치
3. .gitignore 에 *.exe 제외 (필요 시 별도 빌드)
```

### 3.2 `Shared/Schemas/run_codegen.bat`

```bat
@echo off
REM ASCII-only — winters 컨벤션
setlocal
set FLATC=%~dp0..\..\Tools\Bin\flatc.exe
set OUT_CPP=%~dp0Generated\cpp
set OUT_GO=%~dp0Generated\go

if not exist "%OUT_CPP%" mkdir "%OUT_CPP%"
if not exist "%OUT_GO%" mkdir "%OUT_GO%"

"%FLATC%" --cpp -o "%OUT_CPP%" "%~dp0Command.fbs" "%~dp0Snapshot.fbs" "%~dp0Event.fbs"
if errorlevel 1 (
    echo [ERROR] flatc cpp codegen failed
    exit /b 1
)

"%FLATC%" --go -o "%OUT_GO%" "%~dp0Command.fbs" "%~dp0Snapshot.fbs" "%~dp0Event.fbs"
if errorlevel 1 (
    echo [ERROR] flatc go codegen failed
    exit /b 1
)

echo [OK] flatc codegen complete
endlocal
exit /b 0
```

### 3.3 MSBuild Pre-build Event 통합

`Client.vcxproj` Pre-build:
```xml
<Target Name="FlatcCodegen" BeforeTargets="ClCompile">
  <Exec Command="$(SolutionDir)Shared\Schemas\run_codegen.bat" />
</Target>
```

`Engine.vcxproj` 동일 (Engine 도 Schema 헤더 의존 가능).

★ **gotcha**: `run_codegen.bat` 는 **ASCII only** (한글 주석 금지) — CLAUDE.md gotchas .bat 정책. 한글 주석 시 cmd.exe 가 CP949 로 해석해서 임의 위치에서 파싱 실패.

---

## 4. 코드젠 사용처

### 4.1 C++ Client — Snapshot 적용

```cpp
// Client/Private/Network/SnapshotApplier.cpp
#include "Shared/Schemas/Generated/cpp/Snapshot_generated.h"
#include "Shared/GameSim/EntityIdMap.h"

class CSnapshotApplier
{
public:
    void Apply(const Shared::Schema::Snapshot* snap, CWorld& world,
                EntityIdMap& map, DeterministicRng& rng);

    // 적용 흐름:
    //   1. NetEntityId → 로컬 EntityID 매핑 갱신 (없으면 신규 entity 생성)
    //   2. EntitySnapshot 의 hp/pos/anim 등 적용
    //   3. RNG state 동기화 (snap->rngState() → rng.SetState)
    //   4. yourNetId = 클라 own entity 식별
    //   5. lastAckedCommandSeq = 예측 rollback 기준점
};
```

### 4.2 C++ Server — Command 디시리얼라이즈

```cpp
// Server/Game/CommandDispatcher.cpp
#include "Shared/Schemas/Generated/cpp/Command_generated.h"
#include "Shared/GameSim/Systems/ICommandExecutor.h"

class CCommandDispatcher
{
public:
    void Dispatch(u32_t sessionId, const Shared::Schema::CommandBatch* batch,
                   CGameRoom& room);

    // 처리:
    //   1. CommandPacket 순회 (sequenceNum 무결성 검증)
    //   2. wire → GameCommandWire 변환
    //   3. BuildServerCommand(wire, sessionId, controlledEntity, map) 호출
    //   4. room.QueueCommand(cmd) — Tick 시 ExecuteCommand
};
```

### 4.3 Go Backend — GameSession 패스스루

```go
// Services/internal/gamesession/handler.go
import schema "winters/services/shared/schemas/generated/go/Shared/Schema"

func (h *Handler) ProcessCommand(rawBytes []byte) error {
    cmd := schema.GetRootAsCommandBatch(rawBytes, 0)
    // 백엔드는 game logic 모름. 검증만:
    //   - sessionId 유효성
    //   - 본 user 가 해당 NetEntityId 의 owner 인지
    //   - 그대로 IOCP 룸으로 전달
}
```

★ **백엔드 책임 한계**: 백엔드는 sim 로직 0 — 라우팅 / 인증 / Skin 소유권 검증만. 실제 hp/cd/range 검증은 GameServer (Sim-4) 담당.

---

## 5. EntityIdMap 통합 — Client 측

```cpp
// Client/Private/Scene/Scene_InGame.cpp 의 OnEnter:
void CScene_InGame::OnEnter()
{
    m_pEntityIdMap = std::make_unique<EntityIdMap>();
    m_pSnapshotApplier = std::make_unique<CSnapshotApplier>();
    m_pCommandSerializer = std::make_unique<CCommandSerializer>();
    // ...
}

// Snapshot 수신:
void CScene_InGame::OnSnapshot(const u8_t* bytes, u32_t len)
{
    auto* snap = Shared::Schema::GetSnapshot(bytes);
    m_pSnapshotApplier->Apply(snap, m_World, *m_pEntityIdMap, m_localRng);

    // 예측 rollback (Sim-5 박제 영역 — Sim-3 에선 placeholder)
    if (m_pPredictionBuffer)
        m_pPredictionBuffer->Rebuild(snap->lastAckedCommandSeq(), m_World);
}
```

---

## 6. AOI 컬링 prep (Sim-4 와 연결)

```cpp
// Server/Game/SnapshotBuilder.cpp
class CSnapshotBuilder
{
public:
    flatbuffers::DetachedBuffer Build(const CWorld& world,
        const EntityIdMap& map,
        const std::unordered_set<EntityID>& visibleSet,   // ★ AOI 결과
        u64_t serverTick, u64_t rngState);

    // visibleSet 안의 entity 만 EntitySnapshot 으로 직렬화
    // 5v5 = 10 entity + 미니언 ~30 + 타워 11 + 정글몹 ~10 = ~60 max
    //   AOI 50m grid 컬링 후 클라 시야: ~20 entity
};
```

### 6.1 sizeof 추정 (1 frame snapshot)

```
EntitySnapshot 1개 ≈ 80 bytes (FlatBuffers offset 포함)
20 entity × 80 = 1600 bytes
+ Snapshot header ≈ 40 bytes
≈ 1.6 KB / frame
30 frame/sec × 1.6 KB = 48 KB/sec/client
10 client × 48 KB = 480 KB/sec — 합리
```

---

## 7. sequenceNum 무결성 검증

```cpp
// Server/Game/CommandDispatcher.cpp
void CCommandDispatcher::Dispatch(u32_t sessionId, const CommandBatch* batch, CGameRoom& room)
{
    auto* pSession = CSession_Manager::Get()->Find(sessionId);
    if (!pSession) return;

    for (const auto* cmd : *batch->commands())
    {
        const u32_t seq = cmd->sequenceNum();

        // ★ 갭 검증
        if (seq <= session.lastProcessedSeq)
        {
            // 중복 — 무시 (재전송 대응)
            continue;
        }
        if (seq > session.lastProcessedSeq + 60)   // 60 frame = 2초
        {
            // 너무 큰 갭 — 의심 / 연결 종료 또는 reject
            session.flagSuspicious();
            continue;
        }

        // ★ Lag compensation 1차 키 (Sim-4 §8 박제)
        room.QueueCommand(cmd, seq, session.lastProcessedSeq);
        session.lastProcessedSeq = seq;
    }
}
```

---

## 8. SchemaSmoke 박제

```cpp
// Client/Private/GamePlay/SchemaSmoke.cpp
#include "Shared/Schemas/Generated/cpp/Command_generated.h"
#include "Shared/Schemas/Generated/cpp/Snapshot_generated.h"
#include "Shared/Schemas/Generated/cpp/Event_generated.h"

#include "Shared/GameSim/EntityIdMap.h"

#include <type_traits>

namespace
{
    // ★ 코드젠 헤더가 컴파일 통과하는지
    using CmdKind = Shared::Schema::CommandKind;
    using EvKind  = Shared::Schema::EventKind;

    static_assert(static_cast<int>(CmdKind::CastSkill) == 2);
    static_assert(static_cast<int>(EvKind::Damage) == 1);

    // ★ NetEntityId 와 schema 의 netId 타입 일치
    static_assert(sizeof(NetEntityId) == sizeof(uint32_t));

    // ★ Round-trip 컴파일 검증 (런타임 Smoke 함수)
    void RoundTripSmoke()
    {
        // Command — 빌드
        flatbuffers::FlatBufferBuilder builder(1024);
        Shared::Schema::Vec3 origin(0.f, 1.f, 0.f);
        Shared::Schema::Vec3 dir(1.f, 0.f, 0.f);

        auto cmd = Shared::Schema::CreateCommandPacket(
            builder, CmdKind::CastSkill, /*seq*/ 42, /*tick*/ 100,
            /*slot*/ 1, /*targetNet*/ 7, &origin, &dir, /*item*/ 0, 0);

        builder.Finish(cmd);

        // 디코드
        const auto* parsed = Shared::Schema::GetCommandPacket(builder.GetBufferPointer());
        (void)parsed;
        // 런타임 assert 는 unit test 에서
    }

    // ★ AOI sizeof 추정 검증
    static_assert(sizeof(Shared::Schema::Vec3) == 12);   // 3 × float
}
```

---

## 9. Sim-3 6 단계 마일스톤

| Phase | 내용 | 시간 | 출력 |
|-------|------|------|------|
| **3A** | flatc 도구 설치 + Tools/Bin/flatc.exe + run_codegen.bat | 15분 | 수동 코드젠 통과 |
| **3B** | Command.fbs 작성 + cpp/go codegen 통과 | 20분 | `Generated/cpp/Command_generated.h` |
| **3C** | Snapshot.fbs + EntitySnapshot + 코드젠 | 25분 | `Generated/cpp/Snapshot_generated.h` |
| **3D** | Event.fbs + 17 EventKind + 코드젠 | 20분 | `Generated/cpp/Event_generated.h` |
| **3E** | MSBuild Pre-build 통합 + SchemaSmoke 박제 | 25분 | 빌드 통과 + 회귀 0 |
| **3F** | 회귀 + sequenceNum 검증 unit test + 결정성 grep 13종 | 15분 | gotcha + MEMORY 박제 |
| **합계** | | **120분** | |

---

## 10. 검증 마일스톤

| 검증 | 합격 |
|------|------|
| flatc cpp/go codegen | 통과 |
| 1 frame snapshot 크기 | < 2 KB (20 entity 기준) |
| 직렬화 round-trip | **semantic equality** (역직렬화 후 필드 값 동일 — byte-identical 은 보장 X / FlatBuffers offset alignment 차이 정상) |
| 원본 buffer 보존 | verifier 통과 후 그대로 저장 → 그 buffer 의 byte 비교만 bit-perfect 가능 |
| sequenceNum 갭 | reject (60 frame 초과) |
| sequenceNum 중복 | silent ignore |
| `sizeof(NetEntityId) == sizeof(uint32_t)` | static_assert |
| `Vec3` POD | static_assert (sizeof == 12) |
| 결정성 grep 추가 | `grep std::chrono Shared/Schemas/` = 0 |
| Pre-build event | 빌드 시 자동 .fbs → .h 재생성 |
| MSBuild incremental | .fbs 변경 시만 코드젠 재실행 (속도) |

---

## 11. 사이클 종료 후 갱신

1. **CLAUDE.md** — Phase 진행 갱신 + gotcha:
   - **G-Sim9**: Schema 는 NetEntityId only. EntityID 직접 직렬화 절대 금지
   - **G-Sim10**: `.fbs` 변경 시 cpp + go 양쪽 codegen 필수. 한쪽만 갱신 시 클라/서버 mismatch
   - **G-Sim11**: `run_codegen.bat` 는 ASCII only (한글 주석 금지)
2. MEMORY: `project_phase_sim3_complete.md`
3. WINTERS_GAMEPLAY_ARCHITECTURE.md §12: "⑨ Schema = 직렬화 경계. POD trivially_copyable static_assert + Schema NetEntityId 둘 다 통과해야 sim 통과"

---

## 12. 의존 그래프

```
Sim-2 (5 축 trivially_copyable)
            │
            ▼
Sim-3A (flatc 도구)
            │
   ┌────────┼────────┐
   ▼        ▼        ▼
Sim-3B   Sim-3C   Sim-3D
(Command) (Snapshot) (Event)
   └────────┼────────┘
            ▼
Sim-3E (MSBuild + Smoke)
            │
            ▼
Sim-3F (회귀 + 결정성)
            │
            ▼
        Sim-4 진입 가능 (IOCP 가 Schema 를 직접 사용)
```

---

## 한 줄

**v1.1 Sim-3 = 직렬화 경계 박제 = NetEntityId only 송신 / sequenceNum 무결성 / RNG state Snapshot 동기화 / POD trivially_copyable 자동 회귀 / flatc cpp+go 양쪽 codegen / AOI sizeof 1.6KB/frame 합리. 120 분 6 단계. Sim-4 IOCP 가 Schema 를 직접 사용 — Sim-3 합격 없이 Sim-4 진입 X.**
