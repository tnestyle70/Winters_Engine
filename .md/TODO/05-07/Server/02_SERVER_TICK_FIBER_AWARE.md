# Stage 2 — Server Tick Fiber-Aware 박제 (h/cpp 전문)

작성일: 2026-05-07
상위: [`00_INDEX_SERVER_FIBER_INTEGRATION.md`](00_INDEX_SERVER_FIBER_INTEGRATION.md)
전제: Stage 1 통과 ([`01_SERVER_FIBER_SKELETON.md`](01_SERVER_FIBER_SKELETON.md))
목표: **Tick 안에서 `Submit` + `WaitForCounter` 호출이 가능한 컨텍스트를 helper 로 정리. Per-session SnapshotBuilder 호출 표면 (`PerSessionSnapshotInput` POD) 를 SnapshotBuilder 헤더에 추가. Phase 분해 X. 행동 byte-identical 보존.**

---

## 1. Preflight — 사실 확정 표 (TODO 0)

| 항목 | 확정값 | 출처 |
|---|---|---|
| `CSnapshotBuilder::Build` 시그니처 | `flatbuffers::DetachedBuffer Build(CWorld&, const EntityIdMap&, u64_t serverTick, u64_t rngState, u32_t lastAckedSeq, NetEntityId yourNetId);` | [SnapshotBuilder.h:17-23](../../../Server/Public/Game/SnapshotBuilder.h:17) |
| `CSnapshotBuilder` 의 멤버 변수 | 없음 (default ctor only, 멤버 0). Build 는 fbb 를 stack 에 생성 후 fbb.Release(). | [SnapshotBuilder.h:25-27](../../../Server/Public/Game/SnapshotBuilder.h:25), [SnapshotBuilder.cpp:17-274](../../../Server/Private/Game/SnapshotBuilder.cpp:17) |
| `Phase_BroadcastSnapshot` 본문 | `for (u32_t sid : m_sessionIds)` N session 루프 + 각 session 마다 `m_pSnapBuilder->Build(...)` + `WrapEnvelope` + `pSession->Send(std::move(packet))` | [GameRoom.cpp:934-969](../../../Server/Private/Game/GameRoom.cpp:934) |
| `Phase_BroadcastEvents` 본문 | `m_world.ForEach<NetAnimationComponent>` collect → broadcast + `DeterministicEntityIterator<ReplicatedEventComponent>::CollectSorted(m_world)` → broadcast → **DestroyEntity(entity)** | [GameRoom.cpp:857-932](../../../Server/Private/Game/GameRoom.cpp:857) |
| `m_pSnapBuilder` 소유 | `std::unique_ptr<CSnapshotBuilder> m_pSnapBuilder;` GameRoom 멤버 | [GameRoom.h:143](../../../Server/Public/Game/GameRoom.h:143) |
| `WaitForCounter` 시그니처 | `void WaitForCounter(CJobCounter* pCounter, std::uint32_t iTarget = 0);` | [JobSystem.h:48](../../../Engine/Public/Core/JobSystem.h:48) |
| `Submit` 시그니처 (function 버전) | `void Submit(std::function<void()> job, CJobCounter* pCounter);` | [JobSystem.h:42](../../../Engine/Public/Core/JobSystem.h:42) |
| Tick 의 stateMutex | `std::lock_guard stateLock(m_stateMutex);` Tick 시작에서 (L306). Tick 전체를 직렬화. | [GameRoom.cpp:306](../../../Server/Private/Game/GameRoom.cpp:306) |

**TODO 0 확인**: 위 모든 행에 "필요"/"추정"/"TBD" 0.

---

## 2. 변경 파일 목록 (4 파일, 모두 수정)

| 종류 | 파일 |
|---|---|
| 수정 .h | `Server/Public/Game/SnapshotBuilder.h` (per-session Input POD 추가) |
| 수정 .cpp | `Server/Private/Game/SnapshotBuilder.cpp` (오버로드 추가 — 기존 시그니처는 그대로 호출 위임) |
| 수정 .h | `Server/Public/Game/GameRoom.h` (private helper 선언 추가) |
| 수정 .cpp | `Server/Private/Game/GameRoom.cpp` (helper 정의 + Phase_BroadcastSnapshot 본문 정리, 분해 X) |

**Stage 2 의 약속**:
- `CJobSystem::Submit` 호출 0 (Stage 3 에서 호출)
- `CJobSystem::WaitForCounter` 호출 0 (Stage 3 에서 호출)
- 행동 byte-identical 보존
- Stage 3 가 변경할 위치만 helper 로 추출

---

## 3. 수정 파일 1 — `Server/Public/Game/SnapshotBuilder.h`

### 3.1 경로

`C:\Users\user\Desktop\Winters\Server\Public\Game\SnapshotBuilder.h`

### 3.2 수정 전 (전문, [SnapshotBuilder.h:1-28](../../../Server/Public/Game/SnapshotBuilder.h:1))

```cpp
#pragma once

#include "ECS/Entity.h"
#include "Shared/GameSim/EntityIdMap.h"
#include "WintersTypes.h"

#include <flatbuffers/flatbuffers.h>
#include <memory>

class CWorld;

class CSnapshotBuilder final
{
public:
    static std::unique_ptr<CSnapshotBuilder> Create();

    flatbuffers::DetachedBuffer Build(
        CWorld& world,
        const EntityIdMap& entityMap,
        u64_t serverTick,
        u64_t rngState,
        u32_t lastAckedSeq,
        NetEntityId yourNetId);

private:
    CSnapshotBuilder() = default;
};
```

### 3.3 수정 후 (전문, +25 줄)

```cpp
#pragma once

#include "ECS/Entity.h"
#include "Shared/GameSim/EntityIdMap.h"
#include "WintersTypes.h"

#include <flatbuffers/flatbuffers.h>
#include <memory>

class CWorld;

// ─────────────────────────────────────────────────────────────
//  PerSessionSnapshotInput  |  N session 동시 빌드용 POD
//
//  Stage 3 에서 Phase_BroadcastSnapshot 가 N 개를 한 번에
//  vector<PerSessionSnapshotInput> 으로 모은 후 Job 별로 Build 호출.
//  멤버는 모두 read-only / value type — race 0.
// ─────────────────────────────────────────────────────────────
struct PerSessionSnapshotInput
{
    u32_t       sessionId    = 0;            // session 식별 (Send 시 다시 사용)
    NetEntityId yourNetId    = NULL_NET_ENTITY;
    u32_t       lastAckedSeq = 0;
};

class CSnapshotBuilder final
{
public:
    static std::unique_ptr<CSnapshotBuilder> Create();

    // 기존 시그니처 (Stage 1~2 호출 경로 그대로 유지)
    flatbuffers::DetachedBuffer Build(
        CWorld& world,
        const EntityIdMap& entityMap,
        u64_t serverTick,
        u64_t rngState,
        u32_t lastAckedSeq,
        NetEntityId yourNetId);

    // ★ Stage 2 추가 — POD input 받는 오버로드.
    //   기존 Build 와 byte-identical 결과 보장 (위임).
    //   Stage 3 의 Job 진입점에서 호출.
    flatbuffers::DetachedBuffer BuildForSession(
        CWorld& world,
        const EntityIdMap& entityMap,
        u64_t serverTick,
        u64_t rngState,
        const PerSessionSnapshotInput& input);

private:
    CSnapshotBuilder() = default;
};
```

### 3.4 검증 포인트

- 기존 `Build` 시그니처 변경 0 — Stage 1 의 호출자 (Phase_BroadcastSnapshot L947) 회귀 0.
- `PerSessionSnapshotInput` 은 POD — copy 안전, race 0 (P-15 통과: `u32_t`/`NetEntityId` 모두 `WintersTypes.h` 정의, 본 헤더 이미 include).
- `BuildForSession` 본체는 기존 `Build` 위임 — byte-identical 보장.

---

## 4. 수정 파일 2 — `Server/Private/Game/SnapshotBuilder.cpp`

### 4.1 경로

`C:\Users\user\Desktop\Winters\Server\Private\Game\SnapshotBuilder.cpp`

### 4.2 수정 = `BuildForSession` 정의 추가 (파일 끝, 기존 본문 변경 0)

기존 [SnapshotBuilder.cpp:1-274](../../../Server/Private/Game/SnapshotBuilder.cpp:1) 의 본문은 그대로. **L274 `}` 직후에 추가**:

```cpp
// ★ Stage 2 추가 — PerSessionSnapshotInput 위임 오버로드.
//   기존 Build 와 byte-identical 결과 보장.
flatbuffers::DetachedBuffer CSnapshotBuilder::BuildForSession(
    CWorld& world,
    const EntityIdMap& entityMap,
    u64_t serverTick,
    u64_t rngState,
    const PerSessionSnapshotInput& input)
{
    return Build(
        world,
        entityMap,
        serverTick,
        rngState,
        input.lastAckedSeq,
        input.yourNetId);
}
```

### 4.3 검증 포인트

- 위임 패턴 — `Build` 본체 0 변경 = byte-identical 결과.
- 인라이닝 가능 (단일 호출) — 컴파일러 optimization 으로 추가 비용 0.

---

## 5. 수정 파일 3 — `Server/Public/Game/GameRoom.h`

### 5.1 경로

`C:\Users\user\Desktop\Winters\Server\Public\Game\GameRoom.h`

### 5.2 수정 = private 영역에 helper 선언 + include 추가

**Hunk A — include 추가** (현재 L7-L12 사이):

수정 전 ([GameRoom.h:7-12](../../../Server/Public/Game/GameRoom.h:7)):
```cpp
#include "Shared/GameSim/DeterministicRng.h"
#include "Shared/GameSim/DeterministicTime.h"
#include "Shared/GameSim/EntityIdMap.h"
#include "Shared/GameSim/Systems/ICommandExecutor.h"
#include "WintersMath.h"
#include "WintersTypes.h"
```

수정 후:
```cpp
#include "Shared/GameSim/DeterministicRng.h"
#include "Shared/GameSim/DeterministicTime.h"
#include "Shared/GameSim/EntityIdMap.h"
#include "Shared/GameSim/Systems/ICommandExecutor.h"
#include "WintersMath.h"
#include "WintersTypes.h"

// ★ Stage 2 — PerSessionSnapshotInput 표면 사용 (멤버 X, helper 시그니처 only).
//   forward declaration 으로 충분하지만, struct 정의를 보유한 헤더가 작아 직접 include
//   (P-15 회피: 헤더 외부 의존 모두 직접 include).
#include "Game/SnapshotBuilder.h"
```

**Hunk B — private 메서드 선언 추가** (현재 L100-L114 사이, [GameRoom.h:100](../../../Server/Public/Game/GameRoom.h:100)):

수정 전:
```cpp
    void Phase_DrainCommands(TickContext& tc);
    void Phase_ExecuteCommands(TickContext& tc);
    void Phase_SimulationSystems(TickContext& tc);
    void Phase_ServerMinionWave(TickContext& tc);
    void Phase_ServerMinionAI(TickContext& tc);
    void Phase_ServerTurretAI(TickContext& tc);
    void Phase_ServerProjectiles(TickContext& tc);
    void Phase_BroadcastEvents(TickContext& tc);
    void Phase_BroadcastSnapshot(TickContext& tc);
    void BroadcastEventPayload(const u8_t* payload, u32_t payloadSize, u32_t sequence);
    void BroadcastReplicatedEvent(const ReplicatedEventComponent& event, TickContext& tc);
```

수정 후:
```cpp
    void Phase_DrainCommands(TickContext& tc);
    void Phase_ExecuteCommands(TickContext& tc);
    void Phase_SimulationSystems(TickContext& tc);
    void Phase_ServerMinionWave(TickContext& tc);
    void Phase_ServerMinionAI(TickContext& tc);
    void Phase_ServerTurretAI(TickContext& tc);
    void Phase_ServerProjectiles(TickContext& tc);
    void Phase_BroadcastEvents(TickContext& tc);
    void Phase_BroadcastSnapshot(TickContext& tc);
    void BroadcastEventPayload(const u8_t* payload, u32_t payloadSize, u32_t sequence);
    void BroadcastReplicatedEvent(const ReplicatedEventComponent& event, TickContext& tc);

    // ★ Stage 2 helper — Phase_BroadcastSnapshot 가 호출.
    //   Stage 2 본체는 기존 직렬 루프 그대로 (helper 호출만 추가).
    //   Stage 3 에서 이 helper 들이 Job Submit 진입점으로 변환됨.
    //
    //   CollectBroadcastInputs: m_sessionIds 순회 + sessionToEntity lookup +
    //                           PerSessionSnapshotInput 채워 vector 반환.
    //   BroadcastSnapshotForInput: 단일 input 에 대해 Build + WrapEnvelope + Send.
    //                              Stage 3 의 Job 진입점이 이 함수 호출.
    void CollectBroadcastInputs(
        std::vector<PerSessionSnapshotInput>& outInputs) const;

    void BroadcastSnapshotForInput(
        const PerSessionSnapshotInput& input,
        const TickContext& tc);
```

### 5.3 검증 포인트

- Helper 는 **모두 const-correct or read-only intent** — `CollectBroadcastInputs` 는 `m_sessionIds`/`m_sessionToEntity`/`m_entityMap` read-only. `BroadcastSnapshotForInput` 은 `m_pSnapBuilder->BuildForSession` (m_world read) + `pSession->Send` (네트워크 송신) — Tick 안에서 직렬 호출 시 행동 보존.
- `m_pSnapBuilder` 가 stateless (§1 Preflight 4행) — N thread 동시 호출 안전 (Stage 3 의 전제).
- forward declaration 충분하지만 `PerSessionSnapshotInput` 정의 노출이 helper 시그니처에 있어 헤더 include 필요 (P-15 회피).

---

## 6. 수정 파일 4 — `Server/Private/Game/GameRoom.cpp`

### 6.1 경로

`C:\Users\user\Desktop\Winters\Server\Private\Game\GameRoom.cpp`

### 6.2 변경 = (a) `Phase_BroadcastSnapshot` 본문을 helper 호출로 교체, (b) helper 정의 추가. **행동 byte-identical 보존**.

#### 6.2.1 Hunk A — `Phase_BroadcastSnapshot` 본문 교체 (현재 L934-L969)

수정 전 ([GameRoom.cpp:934-969](../../../Server/Private/Game/GameRoom.cpp:934)):
```cpp
void CGameRoom::Phase_BroadcastSnapshot(TickContext& tc)
{
    u32_t sentCount = 0;
    for (u32_t sid : m_sessionIds)
    {
        auto pSession = CSession_Manager::Get()->Find(sid);
        if (!pSession)
            continue;

        auto entityIt = m_sessionToEntity.find(sid);
        if (entityIt == m_sessionToEntity.end() || entityIt->second == NULL_ENTITY)
            continue;

        auto snapshot = m_pSnapBuilder->Build(
            m_world,
            m_entityMap,
            tc.tickIndex,
            m_rng.GetState(),
            0,
            m_entityMap.ToNet(entityIt->second));

        auto packet = WrapEnvelope(
            ePacketType::Snapshot,
            static_cast<u32_t>(tc.tickIndex),
            snapshot.data(),
            static_cast<u32_t>(snapshot.size()));
        pSession->Send(std::move(packet));
        ++sentCount;
    }

    if (sentCount > 0 && (tc.tickIndex % 30ull) == 0ull)
    {
        std::cout << "[Tick #" << tc.tickIndex << "] snap broadcast to "
            << sentCount << " sids\n";
    }
}
```

수정 후:
```cpp
void CGameRoom::Phase_BroadcastSnapshot(TickContext& tc)
{
    // ★ Stage 2 — helper 호출로 분리. 행동 byte-identical 보존.
    //   - CollectBroadcastInputs: 기존 m_sessionIds 순회 + sessionToEntity lookup
    //   - BroadcastSnapshotForInput: 기존 Build + WrapEnvelope + Send
    //   Stage 3 에서 이 직렬 for 루프가 N Job Submit 으로 변환됨.

    std::vector<PerSessionSnapshotInput> inputs;
    inputs.reserve(m_sessionIds.size());
    CollectBroadcastInputs(inputs);

    u32_t sentCount = 0;
    for (const PerSessionSnapshotInput& input : inputs)
    {
        BroadcastSnapshotForInput(input, tc);
        ++sentCount;
    }

    if (sentCount > 0 && (tc.tickIndex % 30ull) == 0ull)
    {
        std::cout << "[Tick #" << tc.tickIndex << "] snap broadcast to "
            << sentCount << " sids\n";
    }
}
```

#### 6.2.2 Hunk B — helper 정의 추가 (`Phase_BroadcastSnapshot` 정의 직후)

`Phase_BroadcastSnapshot` 종료 위치 (수정 후 본문 끝, 위 Hunk A 의 닫는 `}` 다음 줄) 에 추가:

```cpp
// ★ Stage 2 helper — Tick 안에서 호출. m_stateMutex 잡힌 상태 (Tick 직렬화).
//   read-only: m_sessionIds, m_sessionToEntity, m_entityMap, CSession_Manager::Find.
//   side-effect: 0 (vector push_back 만).
void CGameRoom::CollectBroadcastInputs(
    std::vector<PerSessionSnapshotInput>& outInputs) const
{
    for (u32_t sid : m_sessionIds)
    {
        auto pSession = CSession_Manager::Get()->Find(sid);
        if (!pSession)
            continue;

        auto entityIt = m_sessionToEntity.find(sid);
        if (entityIt == m_sessionToEntity.end() || entityIt->second == NULL_ENTITY)
            continue;

        PerSessionSnapshotInput input{};
        input.sessionId    = sid;
        input.yourNetId    = m_entityMap.ToNet(entityIt->second);
        input.lastAckedSeq = 0;   // Stage 1~3 동안 0 — Sim-10 v2 M2 reliability 진입 시 채움
        outInputs.push_back(input);
    }
}

// ★ Stage 2 helper — 단일 input 에 대해 Build + WrapEnvelope + Send.
//   Stage 3 의 Job 진입점이 이 함수 호출.
//   주의: 본 함수는 m_pSnapBuilder->BuildForSession (m_world read) + pSession->Send
//          를 호출. m_world read 는 thread-safe (ECS sparse set read-only) 가정 —
//          Stage 3 의 ECS Read Concurrency 검증 항목으로 명시.
void CGameRoom::BroadcastSnapshotForInput(
    const PerSessionSnapshotInput& input,
    const TickContext& tc)
{
    auto pSession = CSession_Manager::Get()->Find(input.sessionId);
    if (!pSession)
        return;

    auto snapshot = m_pSnapBuilder->BuildForSession(
        m_world,
        m_entityMap,
        tc.tickIndex,
        m_rng.GetState(),
        input);

    auto packet = WrapEnvelope(
        ePacketType::Snapshot,
        static_cast<u32_t>(tc.tickIndex),
        snapshot.data(),
        static_cast<u32_t>(snapshot.size()));
    pSession->Send(std::move(packet));
}
```

### 6.3 검증 포인트

- 직렬 for 루프 → `inputs` collect → 다시 직렬 for 루프 = 행동 byte-identical (Stage 2 약속).
- Helper 분리만 — Submit 호출 0, WaitForCounter 호출 0.
- `BuildForSession` 위임 (§4) → 기존 `Build` 호출 = byte-identical.
- `lastAckedSeq = 0` 은 기존 [GameRoom.cpp:952](../../../Server/Private/Game/GameRoom.cpp:952) 의 `0` literal 그대로.

### 6.4 변경 라인 수

- Phase_BroadcastSnapshot 본체: 36 줄 → 18 줄 (helper 사용)
- 신규 helper 2개: 약 35 줄
- 총 +17 줄. 행동 변화 0.

---

## 7. 빌드/검증

### 7.1 빌드 절차

Stage 1 §8.1 과 동일.

### 7.2 30초 smoke (Stage 1 통과 + 추가)

```powershell
Server\Bin\Debug\WintersServer.exe --smoke-seconds=30
```

### 7.3 합격 기준

```text
[OK] Stage 1 의 모든 합격 기준 (Initialize/Tick Fiber shell/Shutdown 로그)
[OK] [Tick #N] snap broadcast to M sids 로그 — Stage 1 과 동일 형식 + sids count 같음
[OK] Tick maxJitter Stage 1 baseline + 100us 이내 (helper 호출 overhead 무시 가능)
[NO_REGRESSION] grep -rn "Phase_BroadcastSnapshot" Server/ 결과: GameRoom.cpp 의 정의 1개 + 호출 1개 (Tick) — 변경 0
```

### 7.4 Byte-identical 검증 (수동 또는 자동)

Stage 1 baseline 과 Stage 2 의 snapshot bytes 비교 — 동일 GameRoom 상태에서 같은 input 으로 같은 결과.

**수동 절차** (8 session 모킹은 Stage 3 에서, 여기서는 1 session):

1. Stage 1 코드 + 1 session 연결 + 5 tick 후 Snapshot bytes 캡처 (Server 로그에 hex dump 추가 가능)
2. Stage 2 코드 + 동일 절차
3. Stage 1 vs Stage 2 의 5 tick snapshot bytes 비교 — 동일

**자동 검증 옵션** (선택, 추후 박제):
```cpp
// Tick 안에 Stage 2 helper 호출 후 결과를 Stage 1 직접 호출 결과와 hash 비교
// 차이 시 LOG + assert
```

### 7.5 Acceptance

```text
[ ] Stage 1 의 모든 acceptance 항목 통과 유지
[ ] SnapshotBuilder.h: PerSessionSnapshotInput POD 추가, 기존 Build 시그니처 보존
[ ] SnapshotBuilder.cpp: BuildForSession 위임 함수만 추가, 본체 변경 0
[ ] GameRoom.h: include +1 + private helper 2개 선언
[ ] GameRoom.cpp: Phase_BroadcastSnapshot 본체 helper 호출로 교체 + helper 정의 추가
[ ] Stage 1 vs Stage 2 의 5 tick snapshot bytes 비교 결과 동일 (수동 또는 자동)
[ ] grep -rn "Submit\|WaitForCounter" Server/ 결과 0 (Stage 2 약속 — Submit 호출 0)
[ ] Tick maxJitter Stage 1 baseline + <100us 이내
[ ] git diff Server/ 변경량: SnapshotBuilder.h +25L / SnapshotBuilder.cpp +18L /
    GameRoom.h +20L / GameRoom.cpp +17L
```

---

## 8. 19 함정 회피 보고 (Stage 2)

| 함정 | 회피 |
|---|---|
| P-1 데이터 미검증 | §1 Preflight 표 모든 행 확정값. SnapshotBuilder.h/cpp 와 GameRoom.h/cpp 의 모든 인용 줄 번호 + 직접 인용. |
| P-2 PIMPL 추측 | `CSnapshotBuilder` 은 PIMPL 아님 — 빈 클래스. `m_pSnapBuilder->Build` 호출 시그니처는 헤더 인용. |
| P-3 호출 경로 단일 가정 | Phase_BroadcastSnapshot 호출자 = Tick L324 1 곳. Phase_BroadcastSnapshot 의 호출 대상 = `m_pSnapBuilder->Build`, `WrapEnvelope`, `pSession->Send` 3 곳 — 모두 helper 안으로 옮김 (행동 보존). |
| P-4 ECS / Scene 결합 | `CSnapshotBuilder` 는 Scene 의존 0. ECS World 만 의존. |
| P-5 유령 의존 | 모든 인용 .h/.cpp 줄 번호 + 직접 인용. |
| P-6 TODO 박제 진입 | §1 Preflight TODO 0. |
| P-7 자료형 미래 사례 | `PerSessionSnapshotInput` 의 `lastAckedSeq` 는 `u32_t` — Sim-10 v2 M2 reliability 의 sequence 한도 (4G) 충분. |
| P-8 인용 의미 반전 | "Stage 2 = helper 분리 + 행동 byte-identical" — 00_INDEX §4 의 Stage 2 정의 와 일치. |
| P-9 Scheduler 동시성 | Stage 2 는 phase 추가 0. 직렬 보존. |
| P-10 Owner Scope | `m_pSnapBuilder` 는 GameRoom 멤버 (멀티 GameRoom 시 GameRoom 별 1개). `PerSessionSnapshotInput` 은 함수 로컬 vector. Owner 매트릭스 일치. |
| P-11 도메인 상수 | `lastAckedSeq = 0` 은 GameRoom.cpp L952 기존값 그대로. 신규 도메인 상수 0. |
| P-12 음수 좌표 | 본 박제는 좌표 계산 0. |
| P-13 미존재 API | `m_pSnapBuilder->Build` 호출 = SnapshotBuilder.h L17-23 시그니처 검증. `BuildForSession` 은 본 박제가 신설 (정의도 본 박제 안). `Submit/WaitForCounter` 호출 0 — 미존재 API 0. |
| P-14 행동 변경 | byte-identical 명시 (BuildForSession 위임). 직렬 루프 보존. |
| P-15 헤더 외부 의존 미include | GameRoom.h 의 `PerSessionSnapshotInput` 사용 — `Game/SnapshotBuilder.h` 직접 include 추가. |
| P-16 산술 검증 | 본 박제는 비트 폭/메모리 한도 0. |
| P-17 Typedef 일괄 변경 | typedef 변경 0. |
| P-18 RHI/Engine 인프라 미인지 | `CSnapshotBuilder` 재사용 (위임). 신설 클래스 0. |
| P-19 Render/Sim 결합 | Server 에 Render 없음. SnapshotBuilder 는 sim read-only. |

---

## 9. ECS World Read Concurrency 검증 (Stage 3 진입 전제)

본 Stage 2 는 `BuildForSession` 을 직렬 호출만 함 (행동 보존). Stage 3 진입 전 다음 검증 필요:

- [ ] `CWorld::ForEach<T>` / `GetComponent<T>` / `HasComponent<T>` / `IsAlive(EntityID)` 가 **read-only thread-safe** 인지 ECS 헤더/소스 확인
- [ ] N session 동시 호출 시 m_world 의 sparse set 이 read-only consistent 한지 (write 0 인 경우만)
- [ ] `EntityIdMap::ToNet` 이 thread-safe 인지 확인
- [ ] `CSession_Manager::Find` 가 thread-safe 인지 확인 (현재 Stage 1~2 에서는 lock_guard 안에서만 호출)
- [ ] `pSession->Send(std::move(packet))` 의 thread-safe 여부 확인 (CSession 내부 send queue 모델 — IOCP `WSASend` 직렬 큐 가정 시 안전)

위 5 항목 확인이 어려우면 Stage 3 의 병렬화는 **`m_pSnapBuilder->BuildForSession` 만 병렬** + `pSession->Send` 는 직렬 collect 후 직렬 송신 으로 conservative 박제.

---

## 10. Stage 3 진입 조건

본 Stage 2 통과 후 [`03_SERVER_PARALLEL_PHASES.md`](03_SERVER_PARALLEL_PHASES.md) 진입.

선결:
- 위 §7.5 acceptance 모든 [x] 체크 완료
- §9 ECS Read Concurrency 5 항목 검증 통과 (또는 conservative fallback 채택)
- Engine 측 `CJobSystem::PushToSomeDeque` Main-push race fix 확인 (CLAUDE.md §1.C)

---

**END OF STAGE 2**
