# Server Phase Jobification Playbook

작성일: 2026-05-07  
목표: GameRoom phase 를 Fiber JobSystem 으로 병렬화할 때의 판단 기준과 패턴을 정리한다.

---

## 1. 병렬화 판단 질문 7개

어떤 phase 를 병렬화하기 전에 반드시 답한다.

```text
1. 이 phase 는 m_world 를 쓰는가?
2. EntityIdMap 을 쓰는가, 쓰면 read 인가 write 인가?
3. output 순서가 네트워크 packet bytes 에 영향을 주는가?
4. 같은 entity 를 둘 이상의 job 이 쓸 수 있는가?
5. job 안에서 mutex 를 잡는가?
6. job 안에서 Send / IO / blocking API 를 부르는가?
7. 결과를 byte-identical 하게 비교할 기준이 있는가?
```

하나라도 답이 흐리면 바로 병렬화하지 않는다.

---

## 2. Access Pattern 분류

### Type A. Pure read / pure compute

예:

```text
DTO -> FlatBuffer encode
math-only visibility calculation
read-only distance query with immutable input
```

병렬화 가능.

패턴:

```text
input immutable vector
output per-index vector
counter wait
serial consume
```

### Type B. Read world / write local output

예:

```text
MinionAI decision
projectile collision query decision
AOI recipient list build
```

조건부 가능.

전제:

```text
CWorld read-only 동시 접근 보장
또는 world snapshot DTO 를 먼저 직렬 생성
```

### Type C. Write world

예:

```text
Transform update
Health update
DamageQueue enqueue
DestroyEntity
AddComponent
EntityIdMap IssueNew/Unbind
```

직접 병렬화 금지.

패턴:

```text
Decision parallel
Apply serial
```

### Type D. IO side effect

예:

```text
pSession->Send
WSASend
log-heavy output
```

초기 단계에서는 직렬 유지.

---

## 3. Standard Jobify Pattern

```cpp
std::vector<Input> inputs;
CollectInputsSerial(inputs);

std::vector<Output> outputs(inputs.size());

CJobSystem* pJob = CServerEntry::Get_JobSystem();
if (!pJob || inputs.size() <= 1)
{
    for (size_t i = 0; i < inputs.size(); ++i)
        outputs[i] = DoWork(inputs[i]);
}
else
{
    CJobCounter counter;
    for (size_t i = 0; i < inputs.size(); ++i)
    {
        pJob->Submit([&, i]()
        {
            try
            {
                outputs[i] = DoWork(inputs[i]);
                outputs[i].bValid = true;
            }
            catch (...)
            {
                outputs[i].bValid = false;
            }
        }, &counter);
    }
    pJob->WaitForCounter(&counter);
}

ApplyOutputsSerial(outputs);
```

중요:

```text
outputs[i] 는 job i 만 write
inputs 는 immutable
counter 수동 Increment 금지
ApplyOutputsSerial 이 최종 순서 결정
```

---

## 4. Phase_BroadcastSnapshot Playbook

초기 구조:

```text
for sid in sessionIds:
  Find session
  entity lookup
  Build(world)
  WrapEnvelope
  Send
```

목표 구조:

```text
CollectSnapshotInputsSerial
CollectSnapshotDTOsSerial
BuildFlatBuffersParallel
SendSnapshotsSerial
```

### SnapshotInput

```cpp
struct PerSessionSnapshotInput
{
    u32_t sessionId = 0;
    NetEntityId yourNetId = NULL_NET_ENTITY;
    u32_t lastAckedSeq = 0;
};
```

### SnapshotDTO

```cpp
struct SnapshotEntityDTO
{
    NetEntityId netId = NULL_NET_ENTITY;
    Vec3 pos{};
    Vec3 rot{};
    f32_t hp = 0.f;
    f32_t maxHp = 0.f;
    f32_t mana = 0.f;
    f32_t maxMana = 0.f;
    f32_t moveSpeed = 0.f;
    u16_t animId = 0;
    u16_t animPhaseFrame = 0;
    u64_t animStartTick = 0;
    u32_t actionSeq = 0;
    u8_t championId = 0;
    u8_t team = 0;
    u8_t level = 1;
    // schema fields 계속 추가
};
```

### SnapshotOutput

```cpp
struct PerSessionSnapshotOutput
{
    u32_t sessionId = 0;
    flatbuffers::DetachedBuffer buffer{};
    bool_t bValid = false;
};
```

### 왜 DTO 인가

```text
CWorld read parallel 을 검증하기 전에도 FlatBuffer encode 병렬화를 시작할 수 있음
네트워크 packet bytes 비교가 쉬움
world access 와 encode access 를 분리해 디버깅 쉬움
```

---

## 5. Phase_ServerMinionAI Playbook

현재 문제:

```text
loop 안에서 read/query/write/damage/event/animation 이 모두 섞여 있음
```

목표:

```text
Read snapshot
Decision parallel
Apply serial
```

### Step 1. Collect sorted minions

```cpp
std::vector<EntityID> minions =
    DeterministicEntityIterator<MinionStateComponent>::CollectSorted(m_world);
```

### Step 2. Read-only snapshot

처음에는 직렬로 필요한 정보를 DTO에 담는다.

```cpp
struct CombatEntityView
{
    EntityID entity;
    eTeam team;
    Vec3 pos;
    bool_t bAlive;
    f32_t hp;
};

struct MinionThinkInput
{
    EntityID entity;
    eTeam team;
    Vec3 pos;
    MinionStateComponent state;
    MinionComponent minion;
    std::span<const CombatEntityView> targets; // 구현 시 vector ref 등으로 대체
};
```

### Step 3. Decision parallel

```cpp
struct MinionDecision
{
    EntityID entity = NULL_ENTITY;
    bool_t bValid = false;
    bool_t bMove = false;
    bool_t bAttack = false;
    Vec3 nextPos{};
    EntityID attackTarget = NULL_ENTITY;
    DamageRequest damage{};
    eNetAnimId anim = eNetAnimId::Idle;
    f32_t nextAttackCooldown = 0.f;
};
```

Decision job 금지:

```text
m_world.GetComponent non-const 접근
m_world.AddComponent
Transform.SetPosition
EnqueueDamageRequest
StartReplicatedAnimation
```

### Step 4. Apply serial

```text
for decision in entity sorted order:
  write MinionState
  write Transform
  enqueue DamageRequest
  update NetAnimation
```

완료 기준:

```text
old loop vs Decision/Apply 1000 tick state hash 동일
```

---

## 6. Phase_ServerProjectiles Playbook

현재 문제:

```text
projectile movement
hit detection
damage enqueue
event enqueue
entity destroy
entityMap IssueNew
```

전부 섞여 있다.

분리:

```text
ProjectileQueryInput collect serial
ProjectileDecision parallel
ProjectileApply serial
```

Decision:

```cpp
struct ProjectileDecision
{
    EntityID projectile = NULL_ENTITY;
    bool_t bDestroy = false;
    bool_t bHit = false;
    Vec3 nextPos{};
    DamageRequest damage{};
    ReplicatedEventComponent event{};
};
```

Apply serial:

```text
IssueNew
EnqueueReplicatedEvent
EnqueueDamageRequest
Transform write
DestroyEntity
```

초기 우선순위:

```text
낮음. Snapshot/MinionAI 이후.
```

---

## 7. Phase_BroadcastEvents Playbook

직접 병렬화 금지 이유:

```text
m_lastBroadcastActionSeq write
m_world.DestroyEntity
entityMap Unbind 가능
network event ordering 중요
```

가능한 부분:

```text
event payload serialization 만 병렬 후보
```

패턴:

```text
Collect events serial
Serialize payload parallel
Send + Destroy serial
```

초기 우선순위:

```text
낮음. Snapshot 이후, Projectiles 이후.
```

---

## 8. TurretAI / SpatialHash

현재 `Phase_ServerTurretAI`:

```cpp
if (m_pSpatialSystem)
    m_pSpatialSystem->Execute(m_world, tc.fDt);
if (m_pTurretAI)
    m_pTurretAI->Execute(m_world, tc.fDt);
```

이 둘은 Engine `ISystem`이다.  
Server가 직접 fiber job으로 쪼개기보다 Engine system의 access contract를 먼저 확인해야 한다.

규칙:

```text
Engine ISystem 을 Server에서 무리하게 내부 병렬화하지 않는다.
System 자체가 JobSystem 을 받는 구조면 해당 system 계획서에서 처리한다.
```

---

## 9. Determinism Rule

병렬화 후에도 외부 관찰 순서는 동일해야 한다.

정렬 기준:

```text
EntityID ascending
sessionId ascending
netId ascending
acceptedTick, sessionId, sequenceNum
```

Apply는 항상 정렬된 output을 직렬 처리한다.

```cpp
std::sort(outputs.begin(), outputs.end(),
    [](const Output& a, const Output& b)
    {
        return a.entity < b.entity;
    });
```

---

## 10. Phase priority

| 우선순위 | Phase | 이유 |
|---:|---|---|
| 1 | BroadcastSnapshot encode | read/compute-heavy, side effect 분리 쉬움 |
| 2 | MinionAI Decision | 성능 가치 큼, 구조 개선 가치 큼 |
| 3 | Projectile query | 가능하나 write/event 많음 |
| 4 | BroadcastEvents serialize | 네트워크 순서 리스크 |
| 5 | ExecuteCommands | deterministic command order 때문에 금지 |

---

## 11. Exit Criteria

어떤 phase 를 jobify 했다고 말하려면:

```text
[ ] read/write 표 작성
[ ] direct world write job 안에 0
[ ] output per-index write
[ ] apply serial
[ ] ThreadOnly old path fallback
[ ] byte-identical 또는 deterministic hash 검증
[ ] 30s smoke
[ ] 1000 tick stress
```

