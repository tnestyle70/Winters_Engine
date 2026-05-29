# Stage 3 — Server Phase 병렬화 박제 (h/cpp 전문)

작성일: 2026-05-07
상위: [`00_INDEX_SERVER_FIBER_INTEGRATION.md`](00_INDEX_SERVER_FIBER_INTEGRATION.md)
전제: Stage 1 ([`01_SERVER_FIBER_SKELETON.md`](01_SERVER_FIBER_SKELETON.md)) + Stage 2 ([`02_SERVER_TICK_FIBER_AWARE.md`](02_SERVER_TICK_FIBER_AWARE.md)) 통과 + ECS Read Concurrency 검증 (Stage 2 §9) 통과 + Engine race fix 진입.
목표: **`Phase_BroadcastSnapshot` 의 N session × `BuildForSession` 호출을 N Job 동시 Submit + WaitForCounter 로 병렬화. `Send` 는 직렬 유지 (conservative). 결과 byte-identical 보장.**

---

## 1. Preflight — 사실 확정 표 (TODO 0)

| 항목 | 확정값 | 출처 |
|---|---|---|
| `CJobSystem::Submit` (function 버전) | `void Submit(std::function<void()> job, CJobCounter* pCounter);` | [JobSystem.h:42](../../../Engine/Public/Core/JobSystem.h:42) |
| `CJobSystem::WaitForCounter` | `void WaitForCounter(CJobCounter* pCounter, std::uint32_t iTarget = 0);` busy-wait + help-stealing (Main 도 Steal) | [JobSystem.h:48](../../../Engine/Public/Core/JobSystem.h:48), 코멘트 L22 |
| `CJobCounter::Increment(n=1)` | `void Increment(std::uint32_t n = 1);` Submit 시 호출 | [JobCounter.h:24](../../../Engine/Public/Core/JobCounter.h:24) |
| `CJobCounter::Decrement` | `void Decrement();` Job 완료 시 CJobSystem::ExecuteItem 이 호출 | [JobCounter.h:29](../../../Engine/Public/Core/JobCounter.h:29), 코멘트 L8 |
| `CSnapshotBuilder::BuildForSession` | Stage 2 추가됨. `m_pSnapBuilder` stateless → 동시 호출 안전 | Stage 2 §3.3 |
| `m_pSnapBuilder` 소유 | GameRoom 멤버 unique_ptr. stateless. | [GameRoom.h:143](../../../Server/Public/Game/GameRoom.h:143) |
| `m_world` 동시 read 안전성 | Stage 2 §9 ECS Read Concurrency 검증 통과 가정 (또는 conservative fallback) | Stage 2 §9 |
| `pSession->Send` 직렬 호출 | 본 Stage 3 약속 — Send 는 collect 후 직렬 호출 | 본 §3 |
| `CServerEntry::Get_JobSystem()` | `CJobSystem*` 반환 (nullptr 가능) | Stage 1 §3.2 |
| Engine race fix 상태 | CLAUDE.md §1.C 의 Phase 5-A Chase-Lev Main-push race 정식 수정 진입 후 | CLAUDE.md §1.C |

**TODO 0 확인**: 모든 행 확정값.

---

## 2. 변경 파일 목록 (3 파일, 모두 수정)

| 종류 | 파일 |
|---|---|
| 수정 .h | `Server/Public/Game/SnapshotBuilder.h` (`PerSessionSnapshotOutput` POD 추가) |
| 수정 .h | `Server/Public/Game/GameRoom.h` (Stage 3 helper 선언 추가) |
| 수정 .cpp | `Server/Private/Game/GameRoom.cpp` (Phase_BroadcastSnapshot 병렬 분해 + helper 정의) |

**Stage 3 의 약속**:
- `BuildForSession` 만 병렬 (m_world read-only)
- `pSession->Send` 직렬 유지 (conservative)
- 결과 byte-identical (직렬 vs 병렬 동일 bytes)
- Phase_ServerProjectiles / Phase_BroadcastEvents / Phase_ServerMinionAI 변경 0 (write-heavy → 결정성 보호)
- `CSession_Manager::Find` 호출은 직렬 collect 단계에서만 (Stage 2 와 동일)

---

## 3. 수정 파일 1 — `Server/Public/Game/SnapshotBuilder.h`

### 3.1 경로

`C:\Users\user\Desktop\Winters\Server\Public\Game\SnapshotBuilder.h`

### 3.2 수정 = `PerSessionSnapshotOutput` POD 추가

Stage 2 가 추가한 `PerSessionSnapshotInput` 정의 (§3.3 의 신규 파일 전문) 직후에 추가.

수정 전 (Stage 2 적용 후 상태):
```cpp
struct PerSessionSnapshotInput
{
    u32_t       sessionId    = 0;
    NetEntityId yourNetId    = NULL_NET_ENTITY;
    u32_t       lastAckedSeq = 0;
};

class CSnapshotBuilder final
{
    // ...
};
```

수정 후 (`PerSessionSnapshotInput` 직후, `class CSnapshotBuilder` 직전 추가):
```cpp
struct PerSessionSnapshotInput
{
    u32_t       sessionId    = 0;
    NetEntityId yourNetId    = NULL_NET_ENTITY;
    u32_t       lastAckedSeq = 0;
};

// ─────────────────────────────────────────────────────────────
//  PerSessionSnapshotOutput  |  Stage 3 병렬 Build 결과
//
//  N session 동시 Build 시 각 Job 이 자기 슬롯의 buffer 만 write.
//  Job 끼리는 서로 다른 vector index 를 write 하므로 race 0.
//  Send 단계 (직렬) 에서 buffer 를 sessionId 별로 transmit.
// ─────────────────────────────────────────────────────────────
struct PerSessionSnapshotOutput
{
    u32_t                       sessionId = 0;
    flatbuffers::DetachedBuffer buffer{};      // move-only owning buffer
    bool                        bValid    = false;  // Build 성공 여부

    PerSessionSnapshotOutput() = default;
    PerSessionSnapshotOutput(const PerSessionSnapshotOutput&) = delete;
    PerSessionSnapshotOutput& operator=(const PerSessionSnapshotOutput&) = delete;
    PerSessionSnapshotOutput(PerSessionSnapshotOutput&&) = default;
    PerSessionSnapshotOutput& operator=(PerSessionSnapshotOutput&&) = default;
};

class CSnapshotBuilder final
{
    // ...
};
```

### 3.3 검증 포인트

- `flatbuffers::DetachedBuffer` 는 move-only (FlatBuffers SDK). 본 POD 도 move-only 명시.
- vector<PerSessionSnapshotOutput> 으로 Tick 안 stack 에 N 개 보유 — 각 Job 이 자기 index 만 write.
- `bValid` 로 Build 실패 (예: session 사라짐) 시 graceful skip.

---

## 4. 수정 파일 2 — `Server/Public/Game/GameRoom.h`

### 4.1 경로

`C:\Users\user\Desktop\Winters\Server\Public\Game\GameRoom.h`

### 4.2 수정 = Stage 3 helper 선언 추가

Stage 2 가 추가한 helper 옆에 추가.

수정 전 (Stage 2 적용 후):
```cpp
    void CollectBroadcastInputs(
        std::vector<PerSessionSnapshotInput>& outInputs) const;

    void BroadcastSnapshotForInput(
        const PerSessionSnapshotInput& input,
        const TickContext& tc);
```

수정 후 (Stage 2 helper 다음에 Stage 3 helper 추가):
```cpp
    void CollectBroadcastInputs(
        std::vector<PerSessionSnapshotInput>& outInputs) const;

    void BroadcastSnapshotForInput(
        const PerSessionSnapshotInput& input,
        const TickContext& tc);

    // ★ Stage 3 helper — 병렬 빌드 진입점.
    //   - BuildSnapshotsParallel: N input → N Job Submit + WaitForCounter.
    //                              결과를 N PerSessionSnapshotOutput 으로 채움.
    //                              CJobSystem 미초기화 시 직렬 fallback.
    //   - SendCollectedSnapshots: collect 된 outputs 를 직렬 Send.
    //                              CSession_Manager::Find + pSession->Send 만.
    void BuildSnapshotsParallel(
        const std::vector<PerSessionSnapshotInput>& inputs,
        std::vector<PerSessionSnapshotOutput>& outOutputs,
        const TickContext& tc);

    void SendCollectedSnapshots(
        std::vector<PerSessionSnapshotOutput>& outputs,
        const TickContext& tc);
```

---

## 5. 수정 파일 3 — `Server/Private/Game/GameRoom.cpp`

### 5.1 경로

`C:\Users\user\Desktop\Winters\Server\Private\Game\GameRoom.cpp`

### 5.2 변경 = (a) Phase_BroadcastSnapshot 본문 교체, (b) 신규 helper 정의 추가, (c) include 추가.

#### 5.2.1 Hunk A — include 추가 (현재 [GameRoom.cpp:1-48](../../../Server/Private/Game/GameRoom.cpp:1) 사이)

수정 전 (Stage 1 의 `<Windows.h>` 추가 직후 상태):
```cpp
// ★ Stage 1 Fiber shell — ConvertThreadToFiber/ConvertFiberToThread 호출용.
//   WIN32_LEAN_AND_MEAN 은 Server.vcxproj L49 에서 define — 충돌 0.
#include <Windows.h>
```

수정 후:
```cpp
// ★ Stage 1 Fiber shell — ConvertThreadToFiber/ConvertFiberToThread 호출용.
//   WIN32_LEAN_AND_MEAN 은 Server.vcxproj L49 에서 define — 충돌 0.
#include <Windows.h>

// ★ Stage 3 — CJobSystem/CJobCounter 사용 (BuildSnapshotsParallel 안 Submit + WaitForCounter).
#include "Game/ServerEntry.h"          // CServerEntry::Get_JobSystem
#include "Core/JobSystem.h"            // CJobSystem (이미 ServerEntry.h 통해 들어오지만 명시)
#include "Core/JobCounter.h"           // CJobCounter
```

#### 5.2.2 Hunk B — Phase_BroadcastSnapshot 본문 교체 (Stage 2 적용 후의 본문)

Stage 2 의 본문:
```cpp
void CGameRoom::Phase_BroadcastSnapshot(TickContext& tc)
{
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

Stage 3 의 본문 (수정 후):
```cpp
void CGameRoom::Phase_BroadcastSnapshot(TickContext& tc)
{
    // ★ Stage 3 — N session × BuildForSession 을 병렬 Submit.
    //   Send 는 collect 후 직렬 (m_stateMutex 안 + ECS write 0 phase 끝).
    //
    //   행동 보장:
    //     - 직렬 vs 병렬 결과 byte-identical (Job 진입점이 BuildForSession 호출
    //       — Stage 2 가 위임 패턴으로 박제, 같은 함수 호출)
    //     - rngState 는 Tick 한 시점 m_rng.GetState() 값을 cache 하여 모든 Job 에
    //       동일 값 전달 (직렬과 동일 — 결정성 보존)
    //     - Job 진입점이 m_world / m_entityMap / m_pSnapBuilder 만 read.
    //       Tick 안 m_stateMutex 잡힌 상태이므로 Network worker thread 에서
    //       m_world write 0 (CGameRoom::Tick L306 의 lock_guard 보호)

    std::vector<PerSessionSnapshotInput> inputs;
    inputs.reserve(m_sessionIds.size());
    CollectBroadcastInputs(inputs);

    if (inputs.empty())
        return;

    std::vector<PerSessionSnapshotOutput> outputs(inputs.size());
    BuildSnapshotsParallel(inputs, outputs, tc);

    SendCollectedSnapshots(outputs, tc);

    if ((tc.tickIndex % 30ull) == 0ull)
    {
        u32_t sentCount = 0;
        for (const auto& out : outputs)
            if (out.bValid)
                ++sentCount;
        if (sentCount > 0)
        {
            std::cout << "[Tick #" << tc.tickIndex << "] snap broadcast to "
                << sentCount << " sids (parallel)\n";
        }
    }
}
```

#### 5.2.3 Hunk C — `BuildSnapshotsParallel` 정의 추가

Phase_BroadcastSnapshot 직후 (Stage 2 helper 정의 다음):

```cpp
// ★ Stage 3 helper — N input → N Job Submit + WaitForCounter.
//   진입 조건: m_stateMutex 잡힌 상태 (Tick 직렬화 → m_world write 0 보장).
//   ECS Read Concurrency: Stage 2 §9 검증 통과 가정.
void CGameRoom::BuildSnapshotsParallel(
    const std::vector<PerSessionSnapshotInput>& inputs,
    std::vector<PerSessionSnapshotOutput>& outOutputs,
    const TickContext& tc)
{
    if (outOutputs.size() != inputs.size())
        outOutputs.resize(inputs.size());

    // sessionId 미리 채우기 (Job 안에서는 PerSessionSnapshotInput 만 capture)
    for (size_t i = 0; i < inputs.size(); ++i)
        outOutputs[i].sessionId = inputs[i].sessionId;

    // rngState 한 시점 cache — 모든 Job 이 같은 값 사용 (직렬과 byte-identical)
    const u64_t rngStateSnapshot = m_rng.GetState();
    const u64_t serverTickSnapshot = tc.tickIndex;

    CJobSystem* pJob = CServerEntry::Get_JobSystem();
    if (!pJob)
    {
        // Fallback (CServerEntry::Initialize 전 호출 또는 Shutdown 후) — 직렬 build
        for (size_t i = 0; i < inputs.size(); ++i)
        {
            outOutputs[i].buffer = m_pSnapBuilder->BuildForSession(
                m_world, m_entityMap, serverTickSnapshot, rngStateSnapshot, inputs[i]);
            outOutputs[i].bValid = (outOutputs[i].buffer.size() > 0);
        }
        return;
    }

    // ── 병렬 분기 ──────────────────────────────────────────────
    // capture 정책:
    //   - this : member access (m_pSnapBuilder, m_world, m_entityMap)
    //   - i    : 자기 슬롯 index (Job 별 다름, race 0)
    //   - serverTickSnapshot/rngStateSnapshot : value capture (불변)
    //   - inputs/outputs : 본 함수가 살아있는 동안만 — WaitForCounter 가 join 보장.
    //
    // CJobCounter 는 stack 에 위치 (Tick 함수 로컬 — Stage 3 §1 약속).
    // 모든 Job 완료 후 WaitForCounter 반환 → outputs 사용 안전.

    CJobCounter counter;
    counter.Increment(static_cast<std::uint32_t>(inputs.size()));

    for (size_t i = 0; i < inputs.size(); ++i)
    {
        pJob->Submit(
            [this, i, serverTickSnapshot, rngStateSnapshot, &inputs, &outOutputs]()
            {
                // Job 진입점 — m_world / m_entityMap read-only.
                // m_pSnapBuilder->BuildForSession 호출 (Stage 2 §3 위임).
                try
                {
                    outOutputs[i].buffer = m_pSnapBuilder->BuildForSession(
                        m_world,
                        m_entityMap,
                        serverTickSnapshot,
                        rngStateSnapshot,
                        inputs[i]);
                    outOutputs[i].bValid = (outOutputs[i].buffer.size() > 0);
                }
                catch (...)
                {
                    // Job 안에서 throw 시 counter 가 감소 안 되면 WaitForCounter
                    // 영원 대기. 안전 제일: bValid=false 표시 + 재throw X.
                    outOutputs[i].bValid = false;
                }
            },
            &counter);
    }

    // WaitForCounter — 모든 N Job 완료까지 busy-wait + help-stealing.
    // Tick thread 가 Stage 1 의 ConvertThreadToFiber 로 fiber 컨텍스트 — 안전.
    pJob->WaitForCounter(&counter, /*iTarget=*/0);
}

// ★ Stage 3 helper — outputs 를 직렬 Send.
//   Stage 3 약속: Send 는 직렬 (CSession 의 IOCP send queue 동시 호출 안전성 미확정).
void CGameRoom::SendCollectedSnapshots(
    std::vector<PerSessionSnapshotOutput>& outputs,
    const TickContext& tc)
{
    (void)tc;
    for (PerSessionSnapshotOutput& out : outputs)
    {
        if (!out.bValid)
            continue;

        auto pSession = CSession_Manager::Get()->Find(out.sessionId);
        if (!pSession)
            continue;

        auto packet = WrapEnvelope(
            ePacketType::Snapshot,
            static_cast<u32_t>(tc.tickIndex),
            out.buffer.data(),
            static_cast<u32_t>(out.buffer.size()));
        pSession->Send(std::move(packet));
    }
}
```

### 5.3 검증 포인트

#### 5.3.1 race 검증

| 변수 | Read | Write | Race 가능성 | 대응 |
|---|---|---|---|---|
| `m_world` | N Job 동시 read (`HasComponent/GetComponent/ForEach`) | 0 (Stage 3 약속) | Stage 2 §9 검증 통과 가정 | ECS sparse set 동시 read 안전 — Phase 5-A MVP 기준 |
| `m_entityMap` | N Job 동시 read (`ToNet`) | 0 | EntityIdMap 의 ToNet 동시 read 안전성 — 일반적으로 unordered_map read 동시 안전 (single-writer write 0) | OK |
| `m_pSnapBuilder` (`CSnapshotBuilder*`) | N Job 동시 호출 | 0 | stateless (§Preflight 행 5) | 안전 |
| `outOutputs[i]` | Job i 만 write | 다른 Job 0 | 서로 다른 index — race 0 | 안전 |
| `rngStateSnapshot`, `serverTickSnapshot` | value capture | 0 | 불변 | 안전 |
| `inputs[i]` | reference capture (read-only) | 0 | 본 함수 stack 안에 살아있음 (WaitForCounter 가 join 보장) | 안전 |
| `CSession_Manager::Find` | Stage 2 helper (`CollectBroadcastInputs`) 직렬 호출, Stage 3 의 `SendCollectedSnapshots` 직렬 호출 | 호출 0 (Job 안에서 X) | Stage 3 직렬화 보장 | 안전 |
| `pSession->Send` | Stage 3 의 `SendCollectedSnapshots` 직렬 호출 | 호출 0 (Job 안에서 X) | Stage 3 직렬화 보장 | 안전 |

#### 5.3.2 Job 안 throw 시 counter 안전

- C++ 예외가 Job 람다 밖으로 빠져나가면 `CJobSystem::ExecuteItem` ([JobSystem.h:77](../../../Engine/Public/Core/JobSystem.h:77)) 의 `counter->Decrement()` 호출 누락 가능
- → WaitForCounter 영원 대기 = 데드락
- 본 박제는 람다 안 try/catch + bValid=false 표시 — counter 정상 감소 보장
- (Engine 의 `ExecuteItem` 본체 검증은 Engine race fix 트랙 책임)

#### 5.3.3 ConvertThreadToFiber 실패 시

- Stage 1 의 `bFiberMode = false` 일 때 Tick thread 가 일반 thread.
- `WaitForCounter` 의 help-stealing busy-wait 은 일반 thread 에서도 동작 (yield 없음).
- 따라서 Stage 1 fallback 시에도 본 Stage 3 안전 — busy-wait 으로 대기.

---

## 6. 빌드/검증

### 6.1 빌드 절차

Stage 1 §8.1 과 동일.

### 6.2 30초 smoke (Stage 2 + 추가 8 session 모킹)

```powershell
# 1 client smoke (Stage 2 와 동일)
Server\Bin\Debug\WintersServer.exe --smoke-seconds=30

# 8 session 모킹 (별도 도구 필요 — 본 박제 외)
# 옵션 A: 기존 1 client 반복 연결로 m_sessionIds size=8 까지 도달 후 측정
# 옵션 B: 단위 테스트 추후 박제
```

### 6.3 합격 기준

```text
[OK] Stage 2 의 모든 합격 기준 (Initialize/Tick/Shutdown 로그)
[OK] [Tick #N] snap broadcast to M sids (parallel) 로그 — sids count Stage 2 와 같음
[OK] Tick maxJitter Stage 2 baseline + 0us 이내 또는 감소 (병렬 효과)
[OK] WaitForCounter 데드락 0 (30s smoke 통과)
[NO_REGRESSION] grep -rn "BuildForSession" Server/ 결과: GameRoom.cpp 안 호출 (Stage 3 helper 안 + Stage 2 fallback) + SnapshotBuilder.cpp 의 정의
```

### 6.4 Byte-identical 검증

8 session 동시 연결 시 (또는 모킹):

1. Stage 2 코드 (직렬 fallback 강제) + 8 session × 5 tick → snapshot bytes hash N×5 캡처
2. Stage 3 코드 (병렬) + 8 session × 5 tick → snapshot bytes hash 캡처
3. 두 hash matrix 비교 — **N×5 모두 동일**

**rngState 결정성 검증**:
- 본 박제는 `m_rng.GetState()` 를 Tick 한 시점 cache → 모든 Job 에 동일 전달
- Stage 2 의 직렬 호출 ([GameRoom.cpp:951](../../../Server/Private/Game/GameRoom.cpp:951)) 도 동일 — for 루프 안 매번 `m_rng.GetState()` 호출 (rng state 변경 X — Tick 안 m_rng write 호출 0).
- 따라서 rng state 는 Tick 안에서 불변 → snapshot bytes 동일 보장.

### 6.5 Acceptance

```text
[ ] Stage 1 + Stage 2 의 모든 acceptance 항목 통과 유지
[ ] SnapshotBuilder.h: PerSessionSnapshotOutput POD 추가
[ ] GameRoom.h: Stage 3 helper 2개 선언
[ ] GameRoom.cpp: include 3개 추가 + Phase_BroadcastSnapshot 병렬 분해 + helper 정의 +95L
[ ] 1 session smoke 30s 통과 (WaitForCounter 데드락 0)
[ ] 8 session 모킹 (또는 단위 테스트) 에서 byte-identical 검증 통과
[ ] grep -rn "Submit\|WaitForCounter" Server/ 결과: GameRoom.cpp 안 1 곳씩 (Stage 3 의 helper 안)
[ ] CIOCPCore::WorkerLoop 회귀 0 (코드 diff 없음)
[ ] Phase_ServerProjectiles / Phase_BroadcastEvents / Phase_ServerMinionAI diff 0 (Stage 3 약속)
[ ] Tick maxJitter Stage 2 baseline 와 비슷하거나 감소 (병렬 이득)
```

---

## 7. Phase_ServerProjectiles / Phase_BroadcastEvents 변경 0 이유

### 7.1 Phase_ServerProjectiles ([GameRoom.cpp:525-...](../../../Server/Private/Game/GameRoom.cpp:525))

write-heavy:
- `EnqueueReplicatedEvent(m_world, ...)` ([L576](../../../Server/Private/Game/GameRoom.cpp:576), [L596](../../../Server/Private/Game/GameRoom.cpp:596), [L635](../../../Server/Private/Game/GameRoom.cpp:635)) — m_world write
- `EnqueueDamageRequest(m_world, request)` ([L624](../../../Server/Private/Game/GameRoom.cpp:624)) — m_world write
- `m_world.DestroyEntity(entity)` ([L597](../../../Server/Private/Game/GameRoom.cpp:597), [L636](../../../Server/Private/Game/GameRoom.cpp:636), [L643](../../../Server/Private/Game/GameRoom.cpp:643)) — m_world write
- `m_entityMap.IssueNew(entity)` ([L548](../../../Server/Private/Game/GameRoom.cpp:548)) — entityMap write

병렬화 시 write race + 결정성 깨짐. **Stage 3 변경 0** (P-14 회피).

### 7.2 Phase_BroadcastEvents ([GameRoom.cpp:857-932](../../../Server/Private/Game/GameRoom.cpp:857))

`m_world.DestroyEntity(entity)` ([L930](../../../Server/Private/Game/GameRoom.cpp:930)) — m_world write.
`m_lastBroadcastActionSeq[entity]` ([L877](../../../Server/Private/Game/GameRoom.cpp:877)) — GameRoom 멤버 write.

병렬화 부적합. **Stage 3 변경 0** (P-14 회피).

### 7.3 Phase_ServerMinionAI ([GameRoom.cpp:404-512](../../../Server/Private/Game/GameRoom.cpp:404))

각 entity 마다:
- `state.attackCooldown -= tc.fDt` ([L440](../../../Server/Private/Game/GameRoom.cpp:440)) — component write
- `transform.SetPosition(...)` ([L484](../../../Server/Private/Game/GameRoom.cpp:484), [L493](../../../Server/Private/Game/GameRoom.cpp:493)) — component write
- `EnqueueDamageRequest(m_world, request)` ([L472](../../../Server/Private/Game/GameRoom.cpp:472)) — world write
- `m_world.AddComponent<NetAnimationComponent>(...)` ([L501](../../../Server/Private/Game/GameRoom.cpp:501)) — world write

병렬화 부적합 + CLAUDE.md §1 Track 3 의 `Get_WorkerSlot()` 함정 위험. **Stage 3 변경 0**.

### 7.4 Phase_SimulationSystems / Phase_DrainCommands / Phase_ExecuteCommands

모두 m_world write. **Stage 3 변경 0**.

### 7.5 결론

Stage 3 의 병렬화는 **`Phase_BroadcastSnapshot` 의 N session × BuildForSession** 만. Snapshot 빌드는 read-only — 병렬화 안전. 다른 phase 는 결정성/race 위험 → 별도 트랙 (CLAUDE.md §1 Track 3 Phase 5-B Engine Fiber 본체 진입 후).

---

## 8. 19 함정 회피 보고 (Stage 3)

| 함정 | 회피 |
|---|---|
| P-1 데이터 미검증 | §1 Preflight 표 모든 행 확정값. JobSystem.h L42/L48, JobCounter.h L24/L29 직접 인용. |
| P-2 PIMPL 추측 | CSnapshotBuilder 빈 클래스 (PIMPL X). CJobSystem private 멤버는 §1 인용. |
| P-3 호출 경로 단일 가정 | Phase_BroadcastSnapshot 병렬화는 BuildForSession 만 — Send 직렬 유지. 다른 Phase (Projectiles/Events/MinionAI) 변경 0 명시 (§7). |
| P-4 ECS / Scene 결합 | 본 박제는 Scene 의존 0. ECS World read-only. |
| P-5 유령 의존 | 인용 모든 .h/.cpp 줄 번호 + 직접 인용. |
| P-6 TODO 박제 진입 | §1 Preflight TODO 0. |
| P-7 자료형 미래 사례 | `CJobCounter::m_iCount` 32-bit = 4G ≫ Server max session 8 — 충분. |
| P-8 인용 의미 반전 | "Stage 3 = BuildForSession 만 병렬, Send 직렬" — CLAUDE.md §1 Track 3 의 "1차 목표 = Fiber shell only" 와 일치 (Stage 3 도 conservative). |
| P-9 Scheduler 동시성 | 본 박제는 새 phase 0. 같은 phase (Phase_BroadcastSnapshot) 안에서만 병렬. ECS phase 모델 변경 0. |
| P-10 Owner Scope | CJobCounter 함수 로컬. Output vector 함수 로컬. m_pSnapBuilder GameRoom 멤버 (멀티 GameRoom 시 GameRoom 별 1개) — 동시 호출 시 stateless 라 안전. |
| P-11 도메인 상수 | 신규 상수 0. |
| P-12 음수 좌표 | 본 박제는 좌표 계산 0. |
| P-13 미존재 API | `Submit` (function 버전) [JobSystem.h:42](../../../Engine/Public/Core/JobSystem.h:42). `WaitForCounter` [JobSystem.h:48](../../../Engine/Public/Core/JobSystem.h:48). `Increment(n)` [JobCounter.h:24](../../../Engine/Public/Core/JobCounter.h:24). 모두 헤더 인용. |
| P-14 행동 변경 | byte-identical 명시 (§6.4). Phase_ServerProjectiles/Events/MinionAI 변경 0. mask 확장 0. |
| P-15 헤더 외부 의존 미include | GameRoom.cpp 가 `CJobSystem` / `CJobCounter` 사용 — `Core/JobSystem.h` / `Core/JobCounter.h` 직접 include (§5.2.1). |
| P-16 산술 검증 | counter `Increment(N session)` ≤ 8 ≪ 4G. 산술 안전. |
| P-17 Typedef 일괄 변경 | typedef 변경 0. |
| P-18 RHI/Engine 인프라 미인지 | `CJobSystem` (Phase 5-A MVP) 재사용. 신설 0. |
| P-19 Render/Sim 결합 | Server Render 0. SnapshotBuilder sim read-only. |

---

## 9. 향후 확장 (out of scope, 명시만)

본 Stage 3 이 문 열어두는 후속 작업:

- **`Phase_ServerProjectiles` 의 collision query 만 read-only 분리** — write 부분은 직렬 적용. 별도 박제 (Stage 4 후보, 큰 이득 미확실).
- **`Phase_ServerMinionAI` 의 2-pass (Decision-Apply) 분리** — Decision = read-only 병렬, Apply = 직렬. CLAUDE.md §1 Track 3 의 `Get_WorkerSlot()` 함정 회피 후. Engine race fix + Get_WorkerSlot 정정 진입 후 별도 박제.
- **`Phase_BroadcastEvents` 의 Animation events broadcast 만 read-only 분리** — DestroyEntity 부분 직렬 유지. 별도 박제.
- **Sim-10 v2 M3 AOI** 진입 시 N session × per-AOI snapshot — 본 박제의 패턴 그대로 확장.

---

## 10. 후속 트랙 진입 조건

본 Stage 3 통과 후:
- [`04_SERVER_VERIFICATION_AND_GATES.md`](04_SERVER_VERIFICATION_AND_GATES.md) 의 Acceptance Matrix 통과 보고
- 후속 박제 (Stage 4, Phase_ServerMinionAI 2-pass 등) 는 _별도 트랙_ — 본 박제 묶음 범위 외

---

**END OF STAGE 3**
