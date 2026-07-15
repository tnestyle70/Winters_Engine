# Server Fiber Refactor Steps

> **상태 동기화 (2026-07-11 — SUPERSEDED ORDER)**: 아래 stage는 2026-05-07 계획이다. 현재 `ServerEntry`는 이미 파일/프로젝트에 있으나 비동작 stub이고, 기존 SnapshotBuilder의 동시 호출 안전 및 TickThread Fiber shell 전제는 유효하지 않다. `ThreadOnly stress -> Server lifetime -> RoomIngress -> immutable DTO -> ThreadOnly jobify -> FiberShell lab -> Engine FiberFull` 순서는 [2026-07-11 UDP/Fiber 통합 감사](../../../plan/2026-07-11_FULL_UDP_AND_SERVER_FIBER_INTEGRATION_AUDIT.md)를 우선한다.

작성일: 2026-05-07  
목표: 사용자가 직접 step by step 으로 Server Fiber refactor 를 진행할 수 있게 한다.

---

## 0. 진행 방식

각 Stage는 다음 형식으로 진행한다.

```text
1. 코드 읽기
2. 변경 의도 말로 설명
3. 작은 patch
4. 빌드
5. smoke
6. grep gate
7. 다음 Stage 로 이동
```

한 번에 Stage 3~5를 같이 하지 않는다.  
Fiber는 작은 성공 단위를 쌓는 편이 훨씬 안전하다.

---

## Stage 0. Baseline Freeze

목표:

```text
현재 서버가 정상 실행되는 baseline 확보
```

명령:

```powershell
msbuild Engine\Include\Engine.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
msbuild Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
Server\Bin\Debug\WintersServer.exe --smoke-seconds=30
```

기록:

```text
exit code
Tick count
maxJitter
server logs
```

학습 질문:

```text
Q. main thread 와 tick thread 는 어디서 갈라지는가?
Q. IOCP worker 는 언제 시작되는가?
Q. room->Stop 이 tick thread join 을 보장하는가?
```

완료 조건:

```text
[ ] Server 30s smoke 통과
[ ] rg "m_stateMutex" 결과 파악
[ ] rg "Submit|WaitForCounter" Server 결과 파악
```

---

## Stage 1. Engine CJobSystem Export

목표:

```text
Server EXE 가 CJobSystem 을 직접 소유할 수 있게 한다.
```

변경:

```cpp
// Engine/Public/Core/JobSystem.h
#include "WintersAPI.h"

class WINTERS_ENGINE CJobSystem
{
    ...
};
```

주의:

```text
Public header 이므로 include 경로 규칙 준수
WINTERS_ENGINE_EXPORTS 는 Engine project 에서 이미 정의됨
Server/Client 에서는 dllimport
```

검증:

```powershell
msbuild Engine\Include\Engine.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
msbuild Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

완료 조건:

```text
[ ] Engine build 통과
[ ] Server build 통과
[ ] CJobSystem unresolved external 없음
```

---

## Stage 2. CServerEntry Lifetime

목표:

```text
Server process 당 CJobSystem 1개를 명시적으로 소유한다.
```

신규 파일:

```text
Server/Public/Game/ServerEntry.h
Server/Private/Game/ServerEntry.cpp
```

역할:

```text
CServerEntry::Initialize(workerCount, mode)
CServerEntry::Shutdown()
CServerEntry::Get_JobSystem()
```

핵심 설계:

```text
CServerEntry = process singleton owner
CGameRoom = owner 아님, 필요할 때 Get_JobSystem
multi GameRoom 미래에도 JobSystem 은 공유
```

main.cpp 적용 순서:

```text
WSAStartup
CServerEntry::Initialize
CGameRoom::Create
room->Start
CIOCPCore::Create/Start
...
core->Shutdown
room->Stop
CServerEntry::Shutdown
WSACleanup
```

중요:

```text
GameRoom create 실패 path
IOCP start 실패 path
모든 early return 에 CServerEntry::Shutdown 이 있어야 한다.
```

권장 RAII:

```cpp
struct ServerEntryGuard
{
    bool active = false;
    ~ServerEntryGuard()
    {
        if (active)
            CServerEntry::Shutdown();
    }
};
```

완료 조건:

```text
[ ] ServerEntry.h/cpp vcxproj 등록
[ ] filters 등록
[ ] Initialize log 1회
[ ] Shutdown log 1회
[ ] 실패 path 에도 Shutdown 누락 없음
```

---

## Stage 3. TickThread Fiber Shell

목표:

```text
GameRoom tick thread 를 Fiber API 에 익숙해지는 shell 로 변환한다.
```

변경 위치:

```text
Server/Private/Game/GameRoom.cpp
CGameRoom::TickThread
```

패턴:

```cpp
LPVOID hThreadFiber = nullptr;
if (!IsThreadAFiber())
    hThreadFiber = ConvertThreadToFiber(nullptr);

while (m_bRunning.load(...))
{
    Tick();
    sleep_until(next);
}

if (hThreadFiber && IsThreadAFiber())
    ConvertFiberToThread();
```

주의:

```text
Stage 3 에서는 Submit 0
Tick logic 변경 0
ConvertThreadToFiber 실패 시 fallback
```

완료 조건:

```text
[ ] Fiber shell entered log 1회
[ ] Fiber shell exited log 1회
[ ] 30s smoke 통과
[ ] Tick jitter baseline +200us 이내
[ ] rg "Submit|WaitForCounter" Server 는 아직 0
```

학습 질문:

```text
Q. Tick thread 를 fiber 로 바꿨는데 왜 성능이 바로 좋아지지 않는가?
Q. ConvertFiberToThread 는 왜 thread 종료 전에 호출하는가?
```

---

## Stage 4. Snapshot Helper Split

목표:

```text
행동 변화 없이 Phase_BroadcastSnapshot 을 collect/build/send helper 로 나눈다.
```

추가 타입:

```cpp
struct PerSessionSnapshotInput
{
    u32_t sessionId = 0;
    NetEntityId yourNetId = NULL_NET_ENTITY;
    u32_t lastAckedSeq = 0;
};
```

추가 함수:

```cpp
void CollectSnapshotInputs(std::vector<PerSessionSnapshotInput>& outInputs) const;
void BuildSnapshotForInput(...);
void SendSnapshotOutput(...);
```

Stage 4 에서는 여전히 직렬이다.

```text
collect 직렬
build 직렬
send 직렬
```

완료 조건:

```text
[ ] Submit/WaitForCounter 호출 0
[ ] 1 client snapshot byte-identical
[ ] "snap broadcast to N sids" count 동일
```

---

## Stage 5. Snapshot DTO / Encode Split

목표:

```text
CWorld 병렬 read 검증 전에도 안전하게 병렬화할 수 있는 구조로 만든다.
```

왜 필요한가:

```text
CWorld::GetComponent/ForEach 가 const read API 로 분리되어 있지 않음
따라서 world 를 N job 이 직접 읽는 것은 보수적으로 보류
```

구조:

```text
Tick thread:
  CWorld -> SnapshotEntityDTO vector 직렬 수집

JobSystem:
  DTO vector -> FlatBuffer encode 병렬

Tick thread:
  Send 직렬
```

예상 타입:

```cpp
struct SnapshotEntityDTO
{
    NetEntityId netId;
    Vec3 pos;
    f32_t hp;
    f32_t maxHp;
    u8_t team;
    u8_t championId;
    ...
};

struct PerSessionSnapshotBuildInput
{
    u32_t sessionId;
    NetEntityId yourNetId;
    u32_t lastAckedSeq;
    u64_t serverTick;
    u64_t rngState;
    const std::vector<SnapshotEntityDTO>* pEntities;
};
```

완료 조건:

```text
[ ] DTO collect 직렬
[ ] encode jobs 에서 m_world 접근 0
[ ] Send 직렬
[ ] 1 client byte-identical
[ ] 8 session byte-identical
[ ] Submit/WaitForCounter GameRoom.cpp 안 정확히 1 경로
```

---

## Stage 6. Snapshot Encode Jobify

목표:

```text
Per session FlatBuffer encode 를 CJobSystem 에 submit한다.
```

정답 counter 사용:

```cpp
CJobCounter counter;
for (size_t i = 0; i < inputs.size(); ++i)
{
    pJob->Submit([&, i]()
    {
        try
        {
            outputs[i] = EncodeSnapshot(inputs[i]);
            outputs[i].bValid = true;
        }
        catch (...)
        {
            outputs[i].bValid = false;
        }
    }, &counter);
}
pJob->WaitForCounter(&counter);
```

금지:

```cpp
counter.Increment(inputs.size());
```

fallback:

```text
CServerEntry::Get_JobSystem() == nullptr
inputs.size() <= 1
worker count == 0
```

일 때는 직렬 encode.

완료 조건:

```text
[ ] 30s smoke
[ ] 8 session mock
[ ] no deadlock
[ ] counter reaches zero
[ ] encode output byte-identical
```

---

## Stage 7. Phase Classification Before More Parallelism

목표:

```text
다른 phase 를 병렬화하기 전에 phase 별 read/write 표를 갱신한다.
```

작업:

```text
Phase_ServerMinionAI read/write 재작성
Phase_ServerProjectiles read/write 재작성
Phase_BroadcastEvents read/write 재작성
```

출력:

```text
각 phase 별:
  read set
  write set
  order dependency
  deterministic key
  possible Decision output
  Apply order
```

완료 조건:

```text
[ ] direct parallel 금지 phase 명확화
[ ] Decision/Apply 가능한 phase 후보 1개 선정
```

---

## Stage 8. MinionAI Decision / Apply

목표:

```text
ServerMinionAI 를 직접 write loop 에서 Decision/Apply 로 분리한다.
```

패턴:

```text
Collect minion ids sorted

Decision jobs:
  read world snapshot
  produce MinionDecision[i]

Apply serial:
  sorted decision order
  mutate state/transform/damage/event
```

Decision 예:

```cpp
struct ServerMinionDecision
{
    EntityID entity;
    EntityID target;
    Vec3 nextPos;
    bool_t bMove;
    bool_t bAttack;
    DamageRequest damage;
    eNetAnimId anim;
};
```

Apply 규칙:

```text
entity id sorted order 유지
DamageRequest enqueue 직렬
NetAnimation AddComponent 직렬
Transform write 직렬
```

완료 조건:

```text
[ ] ThreadOnly 직렬 vs Decision/Apply 결과 동일
[ ] 1000 tick deterministic hash 동일
[ ] minion attack count 동일
[ ] damage queue count 동일
[ ] no CWorld write inside decision job
```

---

## Stage 9. FiberFull Wait Stress

목표:

```text
Server job 이 nested wait 를 안전하게 할 수 있는지 확인한다.
```

stress:

```text
Parent job submits child jobs
Parent job WaitForCounter
Child jobs complete in random order
Parent resumes
Repeat 10000
```

완료 조건:

```text
[ ] no deadlock
[ ] all parent resumed
[ ] counter waiter map leak 0
[ ] fiber pool acquired == released before shutdown
[ ] created == deleted after shutdown
```

---

## 10. 최종 순서 요약

```text
0. baseline freeze
1. CJobSystem export
2. CServerEntry lifetime
3. Tick fiber shell
4. Snapshot helper split
5. Snapshot DTO / encode split
6. Snapshot encode jobify
7. Phase classification
8. MinionAI Decision/Apply
9. FiberFull nested wait stress
```

이 순서대로 가면, 너는 Fiber를 "적용"하는 게 아니라 "통제"하게 된다.
