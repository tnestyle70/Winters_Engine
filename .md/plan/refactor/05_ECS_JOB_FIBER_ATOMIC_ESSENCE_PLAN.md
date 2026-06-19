Session - ECS/JobSystem/Fiber를 identity, storage, query, mutation, work, wait, truth, view 원자로 다시 자른다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/.md/architecture/WINTERS_CODEBASE_COMPASS.md

`### Engine` 섹션의 `의존 규칙:` 블록에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```md
- Engine public header 변경 시 `EngineSDK/inc` 동기화가 필요할 수 있으므로 검증 항목에 `UpdateLib.bat` 또는 빌드 전 SDK sync를 남긴다.
```

아래에 추가:

```md
- Engine ECS의 원자는 gameplay object가 아니다. Engine에 남길 원자는 identity, storage, query, structural mutation, schedule, generic debug뿐이다.
- `Entity`는 수명 있는 id다. `Component`는 타입이 붙은 state column이다. `System`은 read/write set을 가진 state transition이다. `World`는 state boundary다.
- JobSystem의 원자는 work packet, worker queue, counter, wait policy뿐이다. Champion, minion, turret, skill, status 같은 gameplay 의미를 JobSystem API나 Fiber API에 넣지 않는다.
- Fiber는 gameplay 기능이 아니라 wait continuation이다. Worker fiber context의 wait은 `CJobSystem::WaitForCounter` 안에서만 yield/resume 정책으로 처리한다.
```

`### Shared / GameSim` 섹션의 `의존 규칙:` 블록에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```md
- gameplay 결과는 Shared/Server에서 만들어지고 Client는 presentation으로 소비한다.
```

아래에 추가:

```md
- Shared/GameSim의 원자는 gameplay fact와 deterministic transition이다. HP, mana, cooldown, skill state, projectile, minion, turret, status, replicated pose/action은 Shared/GameSim 소유다.
- Shared/GameSim은 Engine gameplay header를 truth owner로 재사용하지 않는다. `Engine/Public/ECS/Components/GameplayComponents.h` 의존은 migration target이다.
```

1-2. C:/Users/tnest/Desktop/Winters/Engine/README.md

기존 코드:

```md
- 제품 중립 runtime primitive
```

아래에 추가:

```md
- ECS identity/storage/query/scheduler/structural mutation primitive
- JobSystem/Fiber work packet, worker queue, counter dependency, wait/resume policy
```

기존 코드:

```md
- champion, skill, item, quest, boss, minion, turret
```

아래에 추가:

```md
- gameplay fact component: HP, mana, cooldown, team, status, projectile, minion, turret, champion state
- gameplay transition system: damage, skill validation, projectile hit, minion AI, turret AI, status effect tick
```

1-3. C:/Users/tnest/Desktop/Winters/Shared/README.md

기존 코드:

```md
- `Shared/GameSim/`: deterministic gameplay truth
```

아래에 추가:

```md
- GameSim component/system truth: health, mana, cooldown, movement, projectile, minion, structure, champion, status, replicated pose/action
```

기존 코드:

```md
- renderer, UI, ImGui, DX type
```

아래에 추가:

```md
- Engine gameplay component/system headers as source of truth
```

1-4. C:/Users/tnest/Desktop/Winters/Engine/Public/ECS/World.h

기존 코드:

```cpp
#include <functional>
```

삭제할 코드:

```cpp
#include <functional>
```

기존 코드:

```cpp
	template<typename T>
	void ForEach(std::function<void(EntityID, T&)> fn)
	{
		auto& s = GetOrCreateStore<T>();
		for (uint32_t i = 0; i < s.Count(); ++i)
		{
			fn(s.Entities()[i], s.Data()[i]);
		}
	}

	template<typename T1, typename T2>
	void ForEach(std::function<void(EntityID, T1&, T2&)> fn)
	{
		auto& s1 = GetOrCreateStore<T1>();
		auto& s2 = GetOrCreateStore<T2>();
		for (uint32_t i = 0; i < s1.Count(); ++i)
		{
			EntityID e = s1.Entities()[i];
			if (s2.Has(e))
				fn(e, s1.Data()[i], s2.Get(e));
		}
	}

	template<typename T1, typename T2, typename T3>
	void ForEach(std::function<void(EntityID, T1&, T2&, T3&)> fn)
	{
		auto& s1 = GetOrCreateStore<T1>();
		auto& s2 = GetOrCreateStore<T2>();
		auto& s3 = GetOrCreateStore<T3>();

		for (uint32_t i = 0; i < s1.Count(); ++i)
		{
			EntityID e = s1.Entities()[i];
			if (s2.Has(e) && s3.Has(e))
				fn(e, s1.Data()[i], s2.Get(e), s3.Get(e));
		}
	}
```

아래로 교체:

```cpp
	template<typename T, typename Fn>
	void ForEach(Fn&& fn)
	{
		auto* s = TryGetStore<T>();
		if (!s)
			return;

		for (uint32_t i = 0; i < s->Count(); ++i)
			fn(s->Entities()[i], s->Data()[i]);
	}

	template<typename T1, typename T2, typename Fn>
	void ForEach(Fn&& fn)
	{
		auto* s1 = TryGetStore<T1>();
		auto* s2 = TryGetStore<T2>();
		if (!s1 || !s2)
			return;

		for (uint32_t i = 0; i < s1->Count(); ++i)
		{
			EntityID e = s1->Entities()[i];
			if (s2->Has(e))
				fn(e, s1->Data()[i], s2->Get(e));
		}
	}

	template<typename T1, typename T2, typename T3, typename Fn>
	void ForEach(Fn&& fn)
	{
		auto* s1 = TryGetStore<T1>();
		auto* s2 = TryGetStore<T2>();
		auto* s3 = TryGetStore<T3>();
		if (!s1 || !s2 || !s3)
			return;

		for (uint32_t i = 0; i < s1->Count(); ++i)
		{
			EntityID e = s1->Entities()[i];
			if (s2->Has(e) && s3->Has(e))
				fn(e, s1->Data()[i], s2->Get(e), s3->Get(e));
		}
	}
```

private 영역에서 아래 기존 코드 바로 위에 추가:

기존 코드:

```cpp
	template<typename T>
	CComponentStore<T>& GetOrCreateStore()
```

아래에 추가:

```cpp
	template<typename T>
	CComponentStore<T>* TryGetStore()
	{
		auto it = m_mapStores.find(std::type_index(typeid(T)));
		if (it == m_mapStores.end())
			return nullptr;
		return &static_cast<CComponentStoreWrapper<T>*>(it->second.get())->GetStore();
	}

	template<typename T>
	const CComponentStore<T>* TryGetStore() const
	{
		auto it = m_mapStores.find(std::type_index(typeid(T)));
		if (it == m_mapStores.end())
			return nullptr;
		return &static_cast<const CComponentStoreWrapper<T>*>(it->second.get())->GetStore();
	}
```

1-5. C:/Users/tnest/Desktop/Winters/Engine/Public/ECS/Entity.h

CONFIRM_NEEDED:
- 현재 `EntityID`는 storage index이고 `EntityHandle`은 lifetime identity다. 이 둘을 바로 typedef 통합하지 않는다.
- 다음 세부 계획에서 `EntityID` callsite를 storage index, network id, gameplay target id, debug id로 분류한다.
- `EntityHandle`에 world boundary가 필요한지 `CWorld`/GameRoom/Scene/Editor preview owner를 확인한 뒤 결정한다.
- 완료 전까지 새 gameplay code는 `EntityID`를 장기 보관하지 않고 snapshot/event/network id와 구분한다.

1-6. C:/Users/tnest/Desktop/Winters/Engine/Public/ECS/ComponentStore.h

CONFIRM_NEEDED:
- 현재 storage 원자는 sparse set이다. archetype/chunk로 바로 갈아타지 않는다.
- 먼저 hot path query에서 `std::function`, 없는 store 생성, 반복 `type_index` lookup을 제거한다.
- 다음 세부 계획에서 `ComponentSlice<T>` 또는 `QueryView<T...>`를 추가할 때는 전체 새 파일 본문을 작성한다.

1-7. C:/Users/tnest/Desktop/Winters/Engine/Public/ECS/Components/GameplayComponents.h

삭제할 범위:
최종적으로 파일 전체 삭제.

삭제 전 필수 이관:
- `eTeam`, `ChampionComponent`, `MinionComponent`, `TurretComponent`, `StructureComponent`, `JungleComponent`, `SkillStateComponent`, `StatusEffectComponent`, `GameplayStateComponent`는 `Shared/GameSim`으로 이동한다.
- `LocalPlayerTag`, `CommandQueueComponent`, visual-only 또는 client-input-only marker는 `Client` 소유로 분리한다.
- `MapObjectComponent`는 runtime gameplay truth인지 editor/presentation placement인지 확인한 뒤 `Shared/GameSim`, `Client`, `Data`, `Tools` 중 하나로만 이동한다.

1-8. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Core/World/World.h

기존 코드:

```cpp
#include "WintersAPI.h"
#include "ECS/World.h"

namespace SharedSim
{
    // Temporary adapter boundary for Phase 7F.
    // GameSim code includes this file instead of reaching into Engine ECS
    // directly; replacing the backing world becomes a one-file change later.
    using World = ::CWorld;
}
```

CONFIRM_NEEDED:
- 이 adapter는 최종 원자가 아니다. Shared/GameSim이 Engine world implementation에 묶여 있는 임시 bridge다.
- 1차는 이 파일을 유지하고 `Shared/GameSim`에서 `ECS/Components/GameplayComponents.h` 의존을 먼저 제거한다.
- 2차에서 `SharedSim::World`를 GameSim 전용 world/query interface 또는 Engine core-only interface로 바꾼다.
- 최종 교체 코드는 `Shared/GameSim` callsite 전수 inspect 뒤 작성한다.

1-9. C:/Users/tnest/Desktop/Winters/Engine/Public/Core/Fiber/FiberTypes.h

기존 코드:

```cpp
enum class eJobExecutionMode : u8_t
{
	ThreadOnly = 0,
	FiberShell,
};
```

아래로 교체:

```cpp
enum class eJobExecutionMode : u8_t
{
	ThreadOnly = 0,
	FiberShell,
	FiberPool,
	FiberFull,
};
```

1-10. C:/Users/tnest/Desktop/Winters/Engine/Public/Core/JobSystem.h

CONFIRM_NEEDED:
- 현재 `WorkItem`의 원자는 `std::function<void()>`와 `CJobCounter*`가 섞인 compatibility packet이다.
- 최종 hot path 원자는 `JobFn`, `void* pData`, `CJobCounter*`, debug name, optional range다.
- `std::function<void()>` Submit은 legacy compatibility로 유지하되 scheduler hot path는 `JobDecl` 또는 새 range packet으로 옮긴다.
- `FiberShellCall`은 FiberShell mode 전용으로 가둔다.
- FiberPool/FiberFull에 필요한 waiter map, ready fiber queue, fiber pool member는 `CJobSystem` 내부가 소유한다. `CJobCounter`에 wait list를 넣지 않는다.

1-11. C:/Users/tnest/Desktop/Winters/Engine/Private/Core/JobSystem.cpp

CONFIRM_NEEDED:
- `TryExecuteItemOnFiber`의 per-job `CreateFiber`/`DeleteFiber`는 FiberShell 검증용으로만 남긴다.
- `FiberPool` mode에서는 Initialize 시점에 fiber를 만들고 Shutdown 시점에 삭제한다.
- `FiberFull` mode에서는 worker fiber context의 `WaitForCounter`가 help-stealing을 하지 않고 waiter map에 current fiber를 등록한 뒤 thread fiber로 yield한다.
- main thread는 fiber화하지 않는다. main/external `WaitForCounter`는 global drain과 steal을 유지한다.
- `Get_WorkerSlot()`은 현재 resume된 worker thread의 slot이다. yield 가능한 구간에서 slot을 캐시하지 않는다.

1-12. C:/Users/tnest/Desktop/Winters/Engine/Public/ECS/SystemScheduler.h

기존 코드:

```cpp
    // Phase = 시스템 실행 순서 그룹.
    // 같은 Phase 는 JobSystem 으로 병렬 실행 가능.
```

아래로 교체:

```cpp
    // Phase = coarse order. Actual parallelism is allowed only inside
    // non-conflicting read/write access batches.
```

1-13. C:/Users/tnest/Desktop/Winters/Engine/Private/ECS/SystemScheduler.cpp

CONFIRM_NEEDED:
- 현재 원자는 system batch다. 최종 원자는 query range job이다.
- 1차는 access conflict batch를 유지하고 unknown access는 직렬화한다.
- 2차에서 `Execute(CWorld&, float)` 안에서 system 전체 Submit을 query chunk/range Submit으로 낮춘다.
- scheduler가 gameplay phase 의미를 갖지 않도록 Champion/Minion/Turret 이름의 branch를 추가하지 않는다.

1-14. C:/Users/tnest/Desktop/Winters/Engine/Public/ECS/CCommandBuffer.h

CONFIRM_NEEDED:
- 현재 mutation 원자는 `std::function` command다. 최종 원자는 typed structural mutation packet이다.
- `DeferCreate(std::function<...>)`와 `DeferCommand(std::function<...>)`는 compatibility path로만 남긴다.
- worker hot path는 `CJobSystem::Get_WorkerSlot()` 기준 per-slot create/destroy/add/remove packet list를 사용한다.
- slot 0은 main, slot 1..N은 worker다.

1-15. C:/Users/tnest/Desktop/Winters/Engine/Private/ECS/CommandBuffer.cpp

CONFIRM_NEEDED:
- `std::lock_guard<std::mutex>`로 모든 worker mutation을 공유 vector에 넣는 구조는 최종 hot path가 아니다.
- Flush는 모든 writer job 완료 후 main thread에서만 실행한다.
- per-slot list merge 순서는 deterministic해야 한다. 같은 frame replay에서 create/destroy 순서가 달라지면 실패다.

1-16. C:/Users/tnest/Desktop/Winters/Engine/Public/ECS/Systems/MinionAISystem.h 및 C:/Users/tnest/Desktop/Winters/Engine/Private/ECS/Systems/MinionAISystem.cpp

삭제할 범위:
최종적으로 Engine 경로의 MinionAISystem 파일 전체 삭제.

삭제 전 필수 이관:
- read-only decision pass는 `Shared/GameSim/Systems/MinionAI`로 이동한다.
- component write apply pass는 main thread 또는 deterministic reduce stage에서 실행한다.
- worker thread에서 cross-entity HP/state를 직접 쓰지 않는다.

1-17. C:/Users/tnest/Desktop/Winters/Engine/Public/ECS/Systems/TurretAISystem.h 및 C:/Users/tnest/Desktop/Winters/Engine/Private/ECS/Systems/TurretAISystem.cpp

삭제할 범위:
최종적으로 Engine 경로의 TurretAISystem 파일 전체 삭제.

삭제 전 필수 이관:
- target selection, aggro, projectile spawn request는 `Shared/GameSim/Systems` 소유다.
- Client visual projectile playback은 Client event/cue 소비 경로가 소유한다.

1-18. C:/Users/tnest/Desktop/Winters/Engine/Public/ECS/Systems/StatusEffectSystem.h 및 C:/Users/tnest/Desktop/Winters/Engine/Private/ECS/Systems/StatusEffectSystem.cpp

삭제할 범위:
최종적으로 Engine 경로의 StatusEffectSystem 파일 전체 삭제.

삭제 전 필수 이관:
- status tick, stacking, gameplay state flag 계산은 `Shared/GameSim/Systems/StatusEffect` 소유다.
- UI debuff icon, animation restriction display, debug panel은 Client view state 소유다.

2. 검증

미검증:
- 계획서 작성만 수행.
- 코드 변경 미실행.
- 빌드 미실행.

검증 명령:
- git diff --check
- rg -n "GameplayComponents|MinionAISystem|TurretAISystem|TurretProjectileSystem|StatusEffectSystem" Engine/Public Engine/Private
- rg -n "#include \"ECS/Components/GameplayComponents.h\"" Shared/GameSim Server Client Tools
- rg -n "std::function<void\\(\\)>|CreateFiber|DeleteFiber|WaitForCounter|FiberShell" Engine/Public/Core Engine/Private/Core
- rg -n "ForEach\\(std::function" Engine/Public/ECS Engine/Private/ECS
- msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64

성능 검증:
- JSON counter scope: `Scheduler::SequentialBatches`, `Scheduler::ParallelBatches`, `Scheduler::SubmittedJobs`, `Scheduler::MaxBatchSize`
- 추가 counter 필요: `ECS::QueryRows`, `ECS::MissingStoreSkips`, `ECS::StructuralCommands`, `JobSystem::Submitted`, `JobSystem::Completed`, `JobSystem::WaitHelpSteal`, `Fiber::WaitMapInsert`, `Fiber::ReadyResume`
- 목표 budget: 10k entity 3-component query 0.2ms 이하, 100k entity chunk/range query 0.8ms 이하, 10k no-op job submit/wait deadlock 0.

확인 필요:
- `Shared/GameSim`이 Engine core-only ECS interface를 써도 되는지, 아니면 GameSim 전용 world/query를 분리할지 결정.
- 새로 추가하는 `.h/.cpp` 파일이 빌드 프로젝트에 포함되는지 확인.
- Engine public header 변경 후 `UpdateLib.bat` 실행 필요.
