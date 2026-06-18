Session - Viego passive soul, possession, QWE/BA steal, R 유지 복구 로직을 완성한다.

1. 반영해야 하는 코드

이번 세션은 비에고만 대상으로 한다. 성공 기준은 적 처치 시 영혼이 남고, 비에고가 우클릭으로 영혼을 소비하면 시각 모델과 Q/W/E/기본공격은 대상 챔피언으로 바뀌며 R은 비에고 R 그대로 남고, R 사용 또는 타이머 종료 시 원상 복구되는 것이다.

1-1. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Components/ViegoSoulComponent.h

기존 코드:

```cpp
#include "ECS/Entity.h"
#include "ECS/Components/GameplayComponents.h"
#include "GameContext.h"
#include "WintersTypes.h"
```

아래로 교체:

```cpp
#include "ECS/Entity.h"
#include "ECS/Components/GameplayComponents.h"
#include "GameContext.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "WintersTypes.h"
```

기존 코드:

```cpp
struct ViegoSoulComponent
{
	EntityID deadChampion = NULL_ENTITY;
	eChampion champion = eChampion::END;
	eTeam eligibleTeam = eTeam::TEAM_END;
	f32_t fRemainingSec = 5.f;
};
```

아래로 교체:

```cpp
struct ViegoSoulComponent
{
	EntityID deadChampion = NULL_ENTITY;
	eChampion champion = eChampion::END;
	eTeam eligibleTeam = eTeam::TEAM_END;
	u8_t skillRanks[SkillRankComponent::kSlotCount] = {};
	StatComponent stolenStat{};
	f32_t fRemainingSec = 5.f;
	bool_t bHasSkillRanks = false;
	bool_t bHasStolenStat = false;
};
```

1-2. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Components/ViegoSimComponent.h

기존 코드:

```cpp
#include "WintersTypes.h"
#include "ECS/Entity.h"
```

아래로 교체:

```cpp
#include "WintersTypes.h"
#include "ECS/Entity.h"
#include "GameContext.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
```

기존 코드:

```cpp
    bool_t bPossessionActive = false;
    EntityID possessedTarget = NULL_ENTITY;
    f32_t possessionTimerSec = 0.f;
    f32_t possessionDurationSec = 6.f;
```

아래로 교체:

```cpp
    bool_t bPossessionActive = false;
    EntityID possessedTarget = NULL_ENTITY;
    eChampion possessionChampion = eChampion::END;
    f32_t possessionTimerSec = 0.f;
    f32_t possessionDurationSec = 6.f;

    SkillRankComponent originalSkillRanks{};
    StatComponent originalStat{};
    bool_t bHasOriginalSkillRanks = false;
    bool_t bHasOriginalStat = false;
```

1-3. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Champions/Viego/ViegoGameSim.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Components/ViegoSoulComponent.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Components/ViegoSoulComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
```

기존 코드:

```cpp
        std::cout << "[ViegoSim] R dash caster=" << ctx.casterEntity << "\n";
    }
}
```

아래로 교체:

```cpp
        std::cout << "[ViegoSim] R dash caster=" << ctx.casterEntity << "\n";
    }

    void RestoreViegoPossession(CWorld& world, EntityID entity)
    {
        if (entity == NULL_ENTITY || !world.HasComponent<ViegoSimComponent>(entity))
            return;

        auto& viego = world.GetComponent<ViegoSimComponent>(entity);
        if (!viego.bPossessionActive &&
            !world.HasComponent<FormOverrideComponent>(entity))
        {
            return;
        }

        if (viego.bHasOriginalSkillRanks &&
            world.HasComponent<SkillRankComponent>(entity))
        {
            world.GetComponent<SkillRankComponent>(entity) = viego.originalSkillRanks;
        }

        if (viego.bHasOriginalStat &&
            world.HasComponent<StatComponent>(entity))
        {
            world.GetComponent<StatComponent>(entity) = viego.originalStat;
        }

        if (world.HasComponent<FormOverrideComponent>(entity))
            world.RemoveComponent<FormOverrideComponent>(entity);

        viego.bPossessionActive = false;
        viego.possessedTarget = NULL_ENTITY;
        viego.possessionChampion = eChampion::END;
        viego.possessionTimerSec = 0.f;
        viego.bHasOriginalSkillRanks = false;
        viego.bHasOriginalStat = false;
    }

    void ApplyViegoSoulRuntime(CWorld& world, EntityID viegoEntity, const ViegoSoulComponent& soul)
    {
        auto& viego = world.HasComponent<ViegoSimComponent>(viegoEntity)
            ? world.GetComponent<ViegoSimComponent>(viegoEntity)
            : world.AddComponent<ViegoSimComponent>(viegoEntity, ViegoSimComponent{});

        if (!viego.bPossessionActive)
        {
            if (world.HasComponent<SkillRankComponent>(viegoEntity))
            {
                viego.originalSkillRanks = world.GetComponent<SkillRankComponent>(viegoEntity);
                viego.bHasOriginalSkillRanks = true;
            }
            if (world.HasComponent<StatComponent>(viegoEntity))
            {
                viego.originalStat = world.GetComponent<StatComponent>(viegoEntity);
                viego.bHasOriginalStat = true;
            }
        }

        viego.bPossessionActive = true;
        viego.possessedTarget = soul.deadChampion;
        viego.possessionChampion = soul.champion;
        viego.possessionDurationSec = 5.f;
        viego.possessionTimerSec = 5.f;

        if (soul.bHasSkillRanks && world.HasComponent<SkillRankComponent>(viegoEntity))
        {
            auto& ranks = world.GetComponent<SkillRankComponent>(viegoEntity);
            for (u8_t slot = 0; slot < SkillRankComponent::kSlotCount; ++slot)
            {
                if (slot == static_cast<u8_t>(eSkillSlot::R))
                    continue;
                ranks.ranks[slot] = soul.skillRanks[slot];
            }
        }

        if (soul.bHasStolenStat && world.HasComponent<StatComponent>(viegoEntity))
        {
            auto& stat = world.GetComponent<StatComponent>(viegoEntity);
            const eChampion originalChampion = stat.championId;
            stat = soul.stolenStat;
            stat.championId = originalChampion;
            stat.bDirty = true;
        }
    }
}
```

기존 코드:

```cpp
namespace ViegoGameSim
{
    void TrySpawnSoulForKill(CWorld& world, const TickContext& tc,
```

아래로 교체:

```cpp
namespace ViegoGameSim
{
    void ApplySoulPossessionRuntime(CWorld& world, EntityID viegoEntity,
        const ViegoSoulComponent& soul)
    {
        ApplyViegoSoulRuntime(world, viegoEntity, soul);
    }

    void RestorePossession(CWorld& world, EntityID viegoEntity)
    {
        RestoreViegoPossession(world, viegoEntity);
    }

    void TrySpawnSoulForKill(CWorld& world, const TickContext& tc,
```

기존 코드:

```cpp
        const Vec3 origin = ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        const Vec3 dir = ResolveDirection(ctx);
```

아래로 교체:

```cpp
        RestoreViegoPossession(*ctx.pWorld, ctx.casterEntity);

        const Vec3 origin = ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        const Vec3 dir = ResolveDirection(ctx);
```

기존 코드:

```cpp
        soul.deadChampion = deadChampion;
        soul.champion = dead.id;
        soul.eligibleTeam = eligibleTeam;
        soul.fRemainingSec = kViegoSoulLifetimeSec;
        world.AddComponent<ViegoSoulComponent>(soulEntity, soul);
```

아래로 교체:

```cpp
        soul.deadChampion = deadChampion;
        soul.champion = dead.id;
        soul.eligibleTeam = eligibleTeam;
        soul.fRemainingSec = kViegoSoulLifetimeSec;
        if (world.HasComponent<SkillRankComponent>(deadChampion))
        {
            const auto& deadRanks = world.GetComponent<SkillRankComponent>(deadChampion);
            for (u8_t slot = 0; slot < SkillRankComponent::kSlotCount; ++slot)
                soul.skillRanks[slot] = deadRanks.ranks[slot];
            soul.bHasSkillRanks = true;
        }
        if (world.HasComponent<StatComponent>(deadChampion))
        {
            soul.stolenStat = world.GetComponent<StatComponent>(deadChampion);
            soul.bHasStolenStat = true;
        }
        world.AddComponent<ViegoSoulComponent>(soulEntity, soul);
```

기존 코드:

```cpp
                [&](EntityID, ViegoSimComponent& state)
                {
                    if (state.bMistActive)
```

아래로 교체:

```cpp
                [&](EntityID entity, ViegoSimComponent& state)
                {
                    if (state.bMistActive)
```

기존 코드:

```cpp
                        if (state.possessionTimerSec <= 0.f)
                        {
                            state.bPossessionActive = false;
                            state.possessedTarget = NULL_ENTITY;
                        }
```

아래로 교체:

```cpp
                        if (state.possessionTimerSec <= 0.f)
                        {
                            RestoreViegoPossession(world, entity);
                        }
```

1-4. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp

기존 코드:

```cpp
        auto& viego = world.HasComponent<ViegoSimComponent>(cmd.issuerEntity)
            ? world.GetComponent<ViegoSimComponent>(cmd.issuerEntity)
            : world.AddComponent<ViegoSimComponent>(cmd.issuerEntity, ViegoSimComponent{});
        viego.bPossessionActive = true;
        viego.possessedTarget = soul.deadChampion;
        viego.possessionDurationSec = 5.f;
        viego.possessionTimerSec = 5.f;

        FormOverrideComponent form{};
        form.visualChampion = soul.champion;
        form.skillChampion = soul.champion;
        form.skillSlotMask = FormOverrideComponent{}.skillSlotMask;
        form.fRemainingSec = 5.f;
        form.bActive = true;
```

아래로 교체:

```cpp
        ViegoGameSim::ApplySoulPossessionRuntime(world, cmd.issuerEntity, soul);

        FormOverrideComponent form{};
        form.baseChampion = eChampion::VIEGO;
        form.visualChampion = soul.champion;
        form.skillChampion = soul.champion;
        form.skillSlotMask = static_cast<u8_t>(
            (1u << static_cast<u8_t>(eSkillSlot::BasicAttack)) |
            (1u << static_cast<u8_t>(eSkillSlot::Q)) |
            (1u << static_cast<u8_t>(eSkillSlot::W)) |
            (1u << static_cast<u8_t>(eSkillSlot::E)));
        form.fRemainingSec = 5.f;
        form.bActive = true;
```

CONFIRM_NEEDED: 위 교체 블록은 `ViegoGameSim::ApplySoulPossessionRuntime` public wrapper를 쓰는 버전이다. helper를 `CommandExecutor.cpp` 안에 복제하지 말고 ViegoGameSim 네임스페이스로 공개한다.

1-5. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Champions/Viego/ViegoGameSim.h

기존 코드:

```cpp
#include "ECS/Entity.h"
```

아래로 교체:

```cpp
#include "ECS/Entity.h"
#include "Shared/GameSim/Components/ViegoSoulComponent.h"
```

기존 코드:

```cpp
    void TrySpawnSoulForKill(CWorld& world, const TickContext& tc,
        EntityID killer, EntityID deadChampion);
```

아래에 추가:

```cpp
    void ApplySoulPossessionRuntime(CWorld& world, EntityID viegoEntity,
        const ViegoSoulComponent& soul);
    void RestorePossession(CWorld& world, EntityID viegoEntity);
```

2. 검증

적 챔피언 처치 후 생성된 영혼 엔티티가 `ViegoSoulComponent.skillRanks`, `stolenStat`, `bHasSkillRanks`, `bHasStolenStat`을 보존하는지 Debug 출력으로 확인한다.

비에고가 영혼 우클릭 후 Q/W/E/기본공격을 사용하면 `hookChampion == soul.champion`이고, R 사용 시 `hookChampion == VIEGO`인지 확인한다.

비에고 R 사용 직전, 그리고 5초 타이머 종료 시 `FormOverrideComponent`가 제거되고 원래 `SkillRankComponent`/`StatComponent`가 복구되는지 확인한다.

`git diff --check`를 실행한다.

Server와 Client를 빌드한다.
