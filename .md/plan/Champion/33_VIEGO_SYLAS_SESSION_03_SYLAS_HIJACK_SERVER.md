Session - Sylas R Hijack을 서버 권한 SpellbookOverride로 구현한다.

1. 반영해야 하는 코드

이번 세션은 사일러스가 R을 눌렀을 때 대상 챔피언의 궁극기를 저장하고, 다음 R 입력에서 저장된 궁극기를 `hookChampion/sourceSlot`으로 실행하게 만드는 서버 구현이다. 성공 기준은 첫 R은 훔치기이고 쿨다운을 먹지 않으며, 두 번째 R은 훔친 궁극기를 실행하고 Sylas R 슬롯 쿨다운을 적용하며 SpellbookOverride를 제거하는 것이다.

1-1. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Champions/Sylas/SylasGameSim.h

기존 코드:

```cpp
	void RegisterHooks();
	void Tick(CWorld& world, const TickContext& tc);
	void ApplyChainHit(CWorld& world, const TickContext& tc, EntityID source, EntityID target);
```

아래로 교체:

```cpp
	void RegisterHooks();
	void Tick(CWorld& world, const TickContext& tc);
	bool_t CanHijackUltimate(CWorld& world, EntityID caster, EntityID target);
	void ApplyChainHit(CWorld& world, const TickContext& tc, EntityID source, EntityID target);
```

1-2. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Champions/Sylas/SylasGameSim.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Components/SylasSimComponent.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/SpellbookOverrideComponent.h"
#include "Shared/GameSim/Components/SylasSimComponent.h"
```

기존 코드:

```cpp
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectRequests.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectRequests.h"
```

기존 코드:

```cpp
    void OnE(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand)
            return;

        if (ctx.pCommand->itemId == 2u)
            SpawnChainProjectile(ctx);
        else
            StartDirectionalDash(*ctx.pWorld, ctx.casterEntity, ResolveCommandDirection(ctx));
    }
}
```

아래로 교체:

```cpp
    bool_t IsValidChampion(eChampion champion)
    {
        return champion != eChampion::NONE && champion != eChampion::END;
    }

    bool_t IsAliveChampion(CWorld& world, EntityID entity)
    {
        if (entity == NULL_ENTITY ||
            !world.IsAlive(entity) ||
            !world.HasComponent<ChampionComponent>(entity))
        {
            return false;
        }

        if (world.HasComponent<HealthComponent>(entity))
        {
            const auto& health = world.GetComponent<HealthComponent>(entity);
            if (health.bIsDead || health.fCurrent <= 0.f)
                return false;
        }

        return true;
    }

    eChampion ResolveHijackSourceChampion(CWorld& world, EntityID target)
    {
        if (!world.HasComponent<ChampionComponent>(target))
            return eChampion::END;

        return world.GetComponent<ChampionComponent>(target).id;
    }

    u8_t ResolveHijackRank(CWorld& world, EntityID caster, EntityID target)
    {
        const u8_t rSlot = static_cast<u8_t>(eSkillSlot::R);
        if (world.HasComponent<SkillRankComponent>(caster))
        {
            const auto& casterRanks = world.GetComponent<SkillRankComponent>(caster);
            if (casterRanks.ranks[rSlot] > 0u)
                return casterRanks.ranks[rSlot];
        }
        if (world.HasComponent<SkillRankComponent>(target))
        {
            const auto& targetRanks = world.GetComponent<SkillRankComponent>(target);
            if (targetRanks.ranks[rSlot] > 0u)
                return targetRanks.ranks[rSlot];
        }
        return 1u;
    }

    bool_t CanHijackUltimateInternal(CWorld& world, EntityID caster, EntityID target)
    {
        if (!IsAliveChampion(world, caster) || !IsAliveChampion(world, target))
            return false;
        if (!world.HasComponent<TransformComponent>(caster) ||
            !world.HasComponent<TransformComponent>(target))
        {
            return false;
        }
        if (!GameplayStateQuery::CanBeTargetedBy(world, caster, target))
            return false;

        const auto& casterChampion = world.GetComponent<ChampionComponent>(caster);
        const auto& targetChampion = world.GetComponent<ChampionComponent>(target);
        if (casterChampion.id != eChampion::SYLAS)
            return false;
        if (casterChampion.team == targetChampion.team &&
            casterChampion.team != eTeam::Neutral)
        {
            return false;
        }

        const eChampion stolenChampion = ResolveHijackSourceChampion(world, target);
        if (!IsValidChampion(stolenChampion) || stolenChampion == eChampion::SYLAS)
            return false;

        f32_t range = ChampionGameDataDB::ResolveSkillRange(
            eChampion::SYLAS,
            static_cast<u8_t>(eSkillSlot::R));
        if (range <= 0.f)
            range = 10.f;
        const f32_t effectiveRange =
            range +
            GameplayStateQuery::ResolveGameplayRadius(world, caster) +
            GameplayStateQuery::ResolveGameplayRadius(world, target);
        return WintersMath::DistanceSqXZ(
            world.GetComponent<TransformComponent>(caster).GetPosition(),
            world.GetComponent<TransformComponent>(target).GetPosition()) <=
            effectiveRange * effectiveRange;
    }

    void OnR(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand)
            return;

        CWorld& world = *ctx.pWorld;
        const EntityID caster = ctx.casterEntity;
        const EntityID target = ctx.pCommand->targetEntity;

        if (world.HasComponent<SpellbookOverrideComponent>(caster))
            return;
        if (!CanHijackUltimateInternal(world, caster, target))
            return;

        SpellbookOverrideComponent spellbook{};
        spellbook.sourceChampion = ResolveHijackSourceChampion(world, target);
        spellbook.sourceSlot = static_cast<u8_t>(eSkillSlot::R);
        spellbook.localSlot = static_cast<u8_t>(eSkillSlot::R);
        spellbook.sourceRank = ResolveHijackRank(world, caster, target);
        spellbook.fRemainingSec = 45.f;
        spellbook.bActive = true;

        if (world.HasComponent<SpellbookOverrideComponent>(caster))
            world.GetComponent<SpellbookOverrideComponent>(caster) = spellbook;
        else
            world.AddComponent<SpellbookOverrideComponent>(caster, spellbook);

#if defined(_DEBUG)
        char msg[192]{};
        sprintf_s(msg,
            "[SylasHijack] caster=%u target=%u stolenChampion=%u rank=%u\n",
            static_cast<u32_t>(caster),
            static_cast<u32_t>(target),
            static_cast<u32_t>(spellbook.sourceChampion),
            static_cast<u32_t>(spellbook.sourceRank));
        WintersOutputAIDebugStringA(msg);
#endif
    }
}
```

CONFIRM_NEEDED: `WintersMath::DistanceSqXZ(Vec3, Vec3)` overload가 현재 include 조합에서 보이는지 확인한다. 보이지 않으면 기존 CommandExecutor의 `DistanceSqXZ(world, a, b)`와 같은 local helper를 SylasGameSim.cpp에 추가한다.

기존 코드:

```cpp
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::SYLAS, GameplayHookVariant::E_CastFrame), &OnE);

        s_bRegistered = true;
```

아래로 교체:

```cpp
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::SYLAS, GameplayHookVariant::E_CastFrame), &OnE);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::SYLAS, GameplayHookVariant::R_CastFrame), &OnR);

        s_bRegistered = true;
```

기존 코드:

```cpp
    void ApplyChainHit(CWorld& world, const TickContext& tc, EntityID source, EntityID target)
```

아래로 교체:

```cpp
    bool_t CanHijackUltimate(CWorld& world, EntityID caster, EntityID target)
    {
        return CanHijackUltimateInternal(world, caster, target);
    }

    void ApplyChainHit(CWorld& world, const TickContext& tc, EntityID source, EntityID target)
```

1-3. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Champions/Kindred/KindredGameSim.h"
#include "Shared/GameSim/Champions/Yone/YoneGameSim.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Champions/Kindred/KindredGameSim.h"
#include "Shared/GameSim/Champions/Sylas/SylasGameSim.h"
#include "Shared/GameSim/Champions/Yone/YoneGameSim.h"
```

기존 코드:

```cpp
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Components/SpellbookOverrideComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
```

기존 코드:

```cpp
    if (bRequestedStage2 && !bStage2)
    {
        LogCastSkill("reject", "stage2-window", cmd, hookChampion, slot.stageWindow);
        return;
    }

    if (!bStage2 && slot.cooldownRemaining > 0.f)
```

아래로 교체:

```cpp
    if (bRequestedStage2 && !bStage2)
    {
        LogCastSkill("reject", "stage2-window", cmd, hookChampion, slot.stageWindow);
        return;
    }

    const bool_t bSylasHijackCapture =
        champion == eChampion::SYLAS &&
        cmd.slot == static_cast<u8_t>(eSkillSlot::R) &&
        hookChampion == eChampion::SYLAS &&
        hookSlot == static_cast<u8_t>(eSkillSlot::R) &&
        !world.HasComponent<SpellbookOverrideComponent>(cmd.issuerEntity);
    if (bSylasHijackCapture &&
        !SylasGameSim::CanHijackUltimate(world, cmd.issuerEntity, effectiveCmd.targetEntity))
    {
        LogCastSkill("reject", "no-hijack-target", cmd, hookChampion, 0.f);
        return;
    }

    if (!bStage2 && slot.cooldownRemaining > 0.f)
```

기존 코드:

```cpp
    if (bStage2)
    {
        slot.currentStage = 0;
        slot.stageWindow = 0.f;
    }
    else
    {
        slot.cooldownRemaining = cooldown;
        slot.cooldownDuration = cooldown;
        if (ChampionGameDataDB::IsSkillTwoStage(hookChampion, hookSlot))
```

아래로 교체:

```cpp
    if (bStage2)
    {
        slot.currentStage = 0;
        slot.stageWindow = 0.f;
    }
    else if (!bSylasHijackCapture)
    {
        slot.cooldownRemaining = cooldown;
        slot.cooldownDuration = cooldown;
        if (ChampionGameDataDB::IsSkillTwoStage(hookChampion, hookSlot))
```

기존 코드:

```cpp
        }
    }

    if (skillIdentity.bConsumeSpellbookOnAccept)
```

아래로 교체:

```cpp
        }
    }
    else
    {
        slot.cooldownRemaining = 0.f;
        slot.cooldownDuration = 0.f;
    }

    if (skillIdentity.bConsumeSpellbookOnAccept)
```

기존 코드:

```cpp
    const u8_t rank = ResolveSkillRank(world, effectiveCmd.issuerEntity, skillIdentity.localSlot);
```

아래로 교체:

```cpp
    u8_t rank = ResolveSkillRank(world, effectiveCmd.issuerEntity, skillIdentity.localSlot);
    if (skillIdentity.sourceRank > 0u)
        rank = skillIdentity.sourceRank;
```

1-4. C:/Users/tnest/Desktop/Winters/Data/Gameplay/ChampionGameData/champions.json

기존 코드:

```json
        {
          "slot": 4,
          "targetMode": "Conditional",
          "stageCount": 1,
```

아래로 교체:

```json
        {
          "slot": 4,
          "targetMode": "UnitTarget",
          "stageCount": 1,
```

CONFIRM_NEEDED: 위 JSON anchor는 SYLAS 블록 내부의 slot 4에만 적용한다. 다른 챔피언의 R targetMode를 바꾸지 않는다.

2. 검증

사일러스가 적 챔피언을 R로 타겟하면 `SpellbookOverrideComponent.sourceChampion`이 대상 챔피언이고 `sourceSlot == R`, `localSlot == R`인지 확인한다.

첫 R 훔치기 입력 직후 Sylas R 슬롯 쿨다운은 0으로 남아야 한다.

다음 R 입력은 `ResolveSkill`에서 `hookChampion == sourceChampion`, `hookSlot == R`, `cooldownChampion == SYLAS`, `cooldownSlot == R`이 되어야 하고, 시전 수락 후 `SpellbookOverrideComponent`가 제거되어야 한다.

`Data/Gameplay/ChampionGameData/champions.json` 수정 후 ChampionGameData 생성/검증 스크립트를 실행한다. 경로는 `Tools/ChampionData/build_champion_game_data.py`를 기준으로 현재 데이터 파이프라인 명령을 확인한다.

`git diff --check`를 실행한다.

Server와 Client를 빌드한다.
