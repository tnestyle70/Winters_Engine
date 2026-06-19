# EntityID Generation Atomic Direction

Session - EntityID Generation을 storage index와 lifetime identity의 최소 원자로 고정한다.

## 북극성

`Entity`는 게임 오브젝트가 아니다. `Entity`는 월드 안의 컴포넌트 저장소를 찾기 위한 생명 있는 슬롯이다.

가장 아래에 남는 본질은 네 개뿐이다.

```text
index      : component storage 위치
generation : slot reuse 판별 번호
alive      : 현재 slot 생존 여부
free-next  : 죽은 slot일 때 다음 free slot
```

따라서 최종 방향은 아래처럼 잡는다.

```text
EntityID     = index only
EntityHandle = packed { generation, index }
EntitySlot   = { generation, nextFree }
```

`EntityID`와 `EntityHandle`을 지금 당장 합치지 않는다. Winters의 현재 sparse component store와 많은 callsite는 `EntityID`를 storage index로 사용한다. `EntityID`는 빠른 내부 순회용 index로 유지하고, 프레임을 넘어 저장되는 owner/target/source/parent 같은 참조를 `EntityHandle`로 점진 승격한다.

## Layer 0 - Slot

가장 아래 layer는 gameplay, component, system, network를 모른다.

```cpp
struct EntitySlot
{
    EntityGeneration generation = 0;
    EntityID nextFree = NULL_FREE_ENTITY;
};
```

기존 `m_vecAlive`, `m_vecGenerations`, `m_vecFreeList`는 하나의 slot 배열과 free-list head로 줄인다.

```text
before:
- m_vecAlive
- m_vecGenerations
- m_vecFreeList
- m_iNextID

after:
- m_vecSlots
- m_iFreeHead
- m_iAliveCount
```

`alive`는 별도 배열로 두지 않는다. `generation`의 최하위 bit를 alive bit로 사용한다.

```text
odd generation  = alive
even generation = dead
0               = null/invalid
```

이렇게 하면 검증은 단일 load와 비교로 끝난다.

```cpp
bool IsAlive(EntityHandle handle) const
{
    const EntityID id = handle.GetIndex();
    return id < m_vecSlots.size()
        && m_vecSlots[id].generation == handle.GetGeneration()
        && IsAliveGeneration(m_vecSlots[id].generation);
}
```

## Layer 1 - Identity

`EntityID`는 위치다. `EntityHandle`은 수명 안전 정체성이다.

```cpp
using EntityID = uint32_t;
using EntityGeneration = uint32_t;

struct EntityHandle
{
    uint64_t value = 0;
};
```

packing 원자는 유지한다.

```text
low 32 bits  = index
high 32 bits = generation
```

이 layer의 금지 사항:

- `EntityID`를 network actor id처럼 쓰지 않는다.
- `EntityID`를 프레임을 넘는 gameplay 참조로 새로 저장하지 않는다.
- `EntityHandle`을 component storage key로 직접 쓰지 않는다.

## Layer 2 - EntityManager

`CEntityManager`의 외부 API는 유지한다.

```cpp
EntityID Create();
EntityHandle CreateHandle();
void Destroy(EntityID id);
bool Destroy(EntityHandle handle);
bool IsAlive(EntityID id) const;
bool IsAlive(EntityHandle handle) const;
EntityGeneration GetGeneration(EntityID id) const;
EntityHandle GetHandle(EntityID id) const;
bool TryResolve(EntityHandle handle, EntityID& outEntity) const;
EntityID Resolve(EntityHandle handle) const;
uint32_t GetAliveCount() const;
```

1차 구현은 외부 동작을 바꾸지 않고 내부 allocator만 바꾼다.

```text
Create:
1. freeHead가 있으면 free slot pop
2. 없으면 slot append
3. generation을 alive generation으로 만든다
4. EntityID index 반환

Destroy:
1. EntityID가 현재 alive인지 확인
2. generation을 다음 dead generation으로 bump
3. slot을 free list head에 push

TryResolve:
1. handle valid 확인
2. index bounds 확인
3. slot generation == handle generation 확인
4. alive generation 확인
5. 현재 EntityID 반환
```

핵심 회귀 방지 조건은 하나다.

```text
Destroy(A) -> Create(B)가 같은 index를 재사용해도
old handle A는 Resolve에 실패해야 한다.
```

## Layer 3 - World

`CWorld`는 entity object owner가 아니다. `CWorld`는 아래 세 owner를 묶는 state boundary다.

```text
EntityManager  : slot lifetime
ComponentStore : EntityID index -> component column
Mutation       : create/destroy/add/remove commit
```

`World::ForEach`에서 받은 `EntityID`는 현재 query 결과로 나온 index이므로 즉시 사용해도 된다.

```cpp
world.ForEach<TransformComponent>(
    [](EntityID entity, TransformComponent& transform)
    {
        // entity는 이 loop row의 storage index다.
    });
```

하지만 component 안에 저장된 owner/target/source는 다른 시간에 기록된 참조이므로 `EntityHandle`이어야 한다.

```cpp
EntityID target = world.ResolveEntity(projectile.target);
if (target == NULL_ENTITY)
    return;
```

## Layer 4 - Component Reference

`EntityID`로 남겨도 되는 값:

- 현재 `ForEach` row에서 받은 entity
- 같은 함수 안에서 즉시 resolve하고 버리는 임시 index
- component store 내부 sparse/dense index key

`EntityHandle`로 승격해야 하는 값:

- projectile owner/target
- damage source/victim
- status effect source
- skill caster/target
- selected/controlled entity
- parent/child relation
- FX attach target
- replicated event source/target/projectile

승격 순서는 위험도가 높은 순서로 잡는다.

```text
1. projectile owner/target
2. damage source/victim
3. status effect source
4. parent/child relation
5. controlled/selected entity
6. FX attach target
7. replicated event fields
```

## Layer 5 - CommandBuffer

`CommandBuffer`의 본질은 순회 중 structural mutation을 직접 하지 않는 것이다.

```text
System loop -> mutation request 기록 -> safe commit point에서 Flush
```

Entity generation 방향에서는 handle path가 우선이다.

```cpp
DeferDestroy(EntityID entity);       // legacy/index path
DeferDestroy(EntityHandle entity);   // lifetime-safe path
```

1차에서는 `Destroy(EntityHandle)`이 stale handle을 조용히 무시하고 살아 있는 새 entity를 건드리지 않는지 검증한다. create token, typed structural packet, per-worker mutation list는 그 다음 단계다.

## Layer 6 - System

System의 원자는 아래 셋이다.

```text
Query + Logic + DeferredMutation
```

규칙은 단순하다.

```text
ForEach에서 받은 EntityID       = 현재 row index이므로 즉시 사용 가능
Component 안에 저장된 entity ref = 반드시 Resolve 후 사용
```

System은 stale `EntityID`를 믿지 않는다. 장기 저장된 참조를 읽는 순간 `ResolveEntity(EntityHandle)`을 통과한다.

## Layer 7 - GameSim / Network

GameSim 내부 identity와 network identity를 섞지 않는다.

```text
EntityHandle   : 현재 GameSim world 내부 lifetime-safe reference
NetworkActorId : server/client snapshot binding용 안정 식별자
```

권위 흐름은 유지한다.

```text
Client Input
-> GameCommand
-> Server GameSim EntityHandle/EntityID
-> Snapshot/Event NetworkActorId
-> Client Visual EntityID/EntityHandle binding
```

서버의 `EntityID` 숫자가 클라이언트의 `EntityID` 숫자와 같다고 가정하지 않는다.

## Layer 8 - JobSystem / Fiber

Entity generation은 JobSystem/Fiber와 직접 결합하지 않는다.

```text
EntityManager mutation = single commit point
Component iteration    = read/write access contract
Job                    = query range work
Fiber                  = wait continuation
```

worker에서 즉시 create/destroy를 실행하지 않는다. worker는 mutation request를 만들고, `CommandBuffer` 또는 equivalent commit stage가 deterministic order로 적용한다.

## 구현 순서

1. `Engine/Public/ECS/Entity.h`의 `CEntityManager` 내부를 `EntitySlot + freeHead`로 교체한다.
2. 외부 API signature는 유지한다.
3. `EntityID` path와 `EntityHandle` path가 기존처럼 빌드되는지 확인한다.
4. stale handle 회귀 smoke를 먼저 만든다.
5. projectile/damage/status/parent처럼 프레임을 넘는 참조만 `EntityHandle`로 승격한다.
6. `CommandBuffer`의 handle destroy path를 stale-safe로 검증한다.
7. GameSim/network boundary에서 `EntityHandle`과 `NetworkActorId` 용도를 문서와 코드로 분리한다.
8. 그 뒤에 component storage/query/job range 최적화로 올라간다.

## 검증 파이프라인

매 단계는 아래 순서로만 진행한다.

```text
1. baseline 확인
2. 작은 패치 적용
3. targeted grep
4. git diff --check
5. Debug x64 full build
6. smoke 실행 또는 수동 runtime 확인
```

필수 검증 명령:

```powershell
powershell -ExecutionPolicy Bypass -File Tools\VerifyEntityGeneration.ps1
git diff --check -- Engine/Public/ECS/Entity.h
rg -n "m_vecAlive|m_vecGenerations|m_vecFreeList|m_iNextID" Engine/Public/ECS/Entity.h
rg -n "CreateHandle|TryResolve|Resolve\\(|Destroy\\(EntityHandle" Engine/Public/ECS Engine/Private/ECS
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' Winters.sln /m /p:Configuration=Debug /p:Platform=x64
```

필수 smoke 조건:

```text
Create A
GetHandle A
Destroy A
Create B
확인: A index와 B index가 같을 수 있다
확인: old handle A Resolve 실패
확인: B handle Resolve 성공
확인: Destroy(old handle A)가 B를 파괴하지 않음
확인: GetAliveCount가 create/destroy 후 정확함
```

Engine public header 변경 후에는 `EngineSDK/inc`를 직접 수정하지 않는다. 빌드 또는 `UpdateLib.bat` sync 결과로만 따라오게 한다.

## 회귀 금지 규칙

- `EntityID`를 전역 안정 ID로 취급하지 않는다.
- `EntityHandle`을 component storage key로 쓰지 않는다.
- stale handle destroy가 새로 재사용된 entity를 파괴하면 실패다.
- network snapshot/event에서 server `EntityID`를 client `EntityID`로 직접 믿으면 실패다.
- normal F5 gameplay를 숨겨서 성능이나 안정성을 증명하지 않는다.
