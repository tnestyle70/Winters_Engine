Session - Champion AI Session 4에서 공통 Stage1 위에 챔피언별 얇은 farm tactic 규칙을 얹는다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAIPolicy.h

기존 코드:

```cpp
struct ChampionAISkillRule
{
    u8_t slot = static_cast<u8_t>(eSkillSlot::BasicAttack);
    f32_t minRange = 0.f;
    f32_t score = 0.f;
};

struct ChampionAIProfile
{
    eChampion champion = eChampion::END;
    f32_t preferredRange = 1.5f;
    f32_t championScanRange = 6.f;
    f32_t minionScanRange = 10.f;
    f32_t structureScanRange = 18.f;
    f32_t leashRange = 14.f;
    f32_t aggression = 1.f;
    f32_t kiteBias = 0.f;
    f32_t retreatHpRatio = 0.35f;
    f32_t reengageHpRatio = 0.55f;
    f32_t minionPressureWeight = 1.f;
    f32_t turretRiskWeight = 1.f;
    f32_t lastHitWeight = 1.f;
    f32_t siegeWeight = 1.f;
    ChampionAISkillRule skillRules[4]{};
    u8_t skillRuleCount = 0;
};
```

아래로 교체:

```cpp
enum class ChampionAITacticTarget : u8_t
{
    EnemyChampion,
    EnemyMinion,
    Self,
};

enum class ChampionAITacticCondition : u8_t
{
    Always,
    LastHit,
};

struct ChampionAISkillRule
{
    u8_t slot = static_cast<u8_t>(eSkillSlot::BasicAttack);
    f32_t minRange = 0.f;
    f32_t score = 0.f;
    f32_t chance = 1.f;
    ChampionAITacticTarget target = ChampionAITacticTarget::EnemyChampion;
    ChampionAITacticCondition condition = ChampionAITacticCondition::Always;
};

struct ChampionAIProfile
{
    eChampion champion = eChampion::END;
    f32_t preferredRange = 1.5f;
    f32_t championScanRange = 6.f;
    f32_t minionScanRange = 10.f;
    f32_t structureScanRange = 18.f;
    f32_t leashRange = 14.f;
    f32_t aggression = 1.f;
    f32_t kiteBias = 0.f;
    f32_t retreatHpRatio = 0.35f;
    f32_t reengageHpRatio = 0.55f;
    f32_t minionPressureWeight = 1.f;
    f32_t turretRiskWeight = 1.f;
    f32_t lastHitWeight = 1.f;
    f32_t siegeWeight = 1.f;
    ChampionAISkillRule skillRules[4]{};
    u8_t skillRuleCount = 0;
    ChampionAISkillRule farmSkillRules[4]{};
    u8_t farmSkillRuleCount = 0;
};
```

1-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAIPolicy.cpp

`MakeJaxProfile` 안에서 아래 기존 코드 전체를:

```cpp
    constexpr ChampionAIProfile MakeJaxProfile()
    {
        return ChampionAIProfile{
            eChampion::JAX,
            1.5f,
            7.f,
            10.f,
            18.f,
            14.f,
            1.25f,
            0.00f,
            0.50f,
            0.65f,
            0.80f,
            0.90f,
            1.00f,
            1.05f,
            {
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::Q), 0.f, 1.f },
            },
            1
        };
    }
```

아래로 교체:

```cpp
    constexpr ChampionAIProfile MakeJaxProfile()
    {
        return ChampionAIProfile{
            eChampion::JAX,
            1.5f,
            7.f,
            10.f,
            18.f,
            14.f,
            1.25f,
            0.00f,
            0.50f,
            0.65f,
            0.80f,
            0.90f,
            1.00f,
            1.05f,
            {
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::Q), 0.f, 1.f },
            },
            1,
            {
                ChampionAISkillRule{
                    static_cast<u8_t>(eSkillSlot::W),
                    0.f,
                    1.f,
                    0.10f,
                    ChampionAITacticTarget::Self,
                    ChampionAITacticCondition::Always },
            },
            1
        };
    }
```

`MakeIreliaProfile` 안에서 아래 기존 코드 전체를:

```cpp
    constexpr ChampionAIProfile MakeIreliaProfile()
    {
        return ChampionAIProfile{
            eChampion::IRELIA,
            1.75f,
            7.f,
            10.f,
            18.f,
            14.f,
            1.15f,
            0.10f,
            0.42f,
            0.60f,
            0.95f,
            1.00f,
            1.05f,
            1.00f,
            {
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::Q), 0.f, 1.f },
            },
            1
        };
    }
```

아래로 교체:

```cpp
    constexpr ChampionAIProfile MakeIreliaProfile()
    {
        return ChampionAIProfile{
            eChampion::IRELIA,
            1.75f,
            7.f,
            10.f,
            18.f,
            14.f,
            1.15f,
            0.10f,
            0.42f,
            0.60f,
            0.95f,
            1.00f,
            1.05f,
            1.00f,
            {
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::Q), 0.f, 1.f },
            },
            1,
            {
                ChampionAISkillRule{
                    static_cast<u8_t>(eSkillSlot::Q),
                    0.f,
                    1.f,
                    1.f,
                    ChampionAITacticTarget::EnemyMinion,
                    ChampionAITacticCondition::LastHit },
            },
            1
        };
    }
```

1-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAISystem.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
```

`IsSkillReady` 함수 바로 아래 기존 코드:

```cpp
    bool_t IsSkillReady(CWorld& world, EntityID self, u8_t slot)
    {
        if (!world.HasComponent<SkillStateComponent>(self))
            return true;
        if (slot >= static_cast<u8_t>(eSkillSlot::SLOT_END))
            return false;
        return world.GetComponent<SkillStateComponent>(self).slots[slot].cooldownRemaining <= 0.f;
    }
```

아래에 추가:

```cpp
    u8_t ResolveAISkillRank(CWorld& world, EntityID self, u8_t slot)
    {
        if (!world.HasComponent<SkillRankComponent>(self) ||
            slot >= SkillRankComponent::kSlotCount)
        {
            return 1;
        }

        const u8_t rank = world.GetComponent<SkillRankComponent>(self).ranks[slot];
        return (rank == 0) ? 1 : rank;
    }

    bool_t TryGetTargetHealth(CWorld& world, EntityID target, f32_t& outHp)
    {
        if (target == NULL_ENTITY || !world.IsAlive(target))
            return false;

        if (world.HasComponent<HealthComponent>(target))
        {
            outHp = world.GetComponent<HealthComponent>(target).fCurrent;
            return true;
        }

        if (world.HasComponent<MinionComponent>(target))
        {
            outHp = world.GetComponent<MinionComponent>(target).hp;
            return true;
        }

        if (world.HasComponent<ChampionComponent>(target))
        {
            outHp = world.GetComponent<ChampionComponent>(target).hp;
            return true;
        }

        if (world.HasComponent<StructureComponent>(target))
        {
            outHp = world.GetComponent<StructureComponent>(target).hp;
            return true;
        }

        return false;
    }

    f32_t ResolveChampionAISkillDamage(eChampion champion, u8_t slot, u8_t rank)
    {
        if (champion == eChampion::IRELIA &&
            slot == static_cast<u8_t>(eSkillSlot::Q))
        {
            return 45.f + 25.f * static_cast<f32_t>(rank);
        }

        return 0.f;
    }

    bool_t CanSkillLastHitTarget(
        CWorld& world,
        eChampion champion,
        u8_t slot,
        u8_t rank,
        EntityID target)
    {
        f32_t hp = 0.f;
        if (!TryGetTargetHealth(world, target, hp))
            return false;

        const f32_t damage = ResolveChampionAISkillDamage(champion, slot, rank);
        return damage > 0.f && hp <= damage;
    }
```

기존 코드:

```cpp
    bool_t EmitSkillCommand(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        eChampion champion,
        const Vec3& selfPos,
        EntityID target,
        u8_t slot,
        const char* reason,
        std::vector<GameCommand>& outCommands)
    {
        if (!IsSkillReady(world, self, slot) ||
            target == NULL_ENTITY ||
            !IsAliveTarget(world, target) ||
            !GameplayStateQuery::CanBeTargetedBy(world, self, target))
        {
            return false;
        }

        Vec3 targetPos{};
        if (!TryGetPosition(world, target, targetPos))
            return false;

        const f32_t range = GetDefaultChampionSkillRange(champion, slot);
        if (range > 0.f &&
            WintersMath::DistanceSqXZ(selfPos, targetPos) > range * range)
        {
            return false;
        }

        ai.lastAction = eChampionAIAction::AttackChampion;
        GameCommand cmd = MakeAICommand(ai, tc, self, eCommandKind::CastSkill);
        cmd.slot = slot;
        cmd.targetEntity = target;
        cmd.groundPos = targetPos;
        cmd.direction = WintersMath::DirectionXZ(selfPos, targetPos);
        outCommands.push_back(cmd);

        LogChampionAICommand(reason, tc, self, ai, champion, selfPos, targetPos,
            target, cmd.kind, cmd.slot);
        return true;
    }
```

아래로 교체:

```cpp
    bool_t EmitSkillCommand(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        eChampion champion,
        const Vec3& selfPos,
        EntityID target,
        u8_t slot,
        eChampionAIAction action,
        const char* reason,
        std::vector<GameCommand>& outCommands)
    {
        if (!IsSkillReady(world, self, slot))
            return false;

        Vec3 targetPos = selfPos;
        if (target != NULL_ENTITY)
        {
            if (!IsAliveTarget(world, target) ||
                !GameplayStateQuery::CanBeTargetedBy(world, self, target) ||
                !TryGetPosition(world, target, targetPos))
            {
                return false;
            }

            const f32_t range = GetDefaultChampionSkillRange(champion, slot);
            if (range > 0.f &&
                WintersMath::DistanceSqXZ(selfPos, targetPos) > range * range)
            {
                return false;
            }
        }

        ai.lastAction = action;
        GameCommand cmd = MakeAICommand(ai, tc, self, eCommandKind::CastSkill);
        cmd.slot = slot;
        cmd.targetEntity = target;
        cmd.groundPos = targetPos;
        cmd.direction = (target != NULL_ENTITY)
            ? WintersMath::DirectionXZ(selfPos, targetPos)
            : Vec3{};
        outCommands.push_back(cmd);

        LogChampionAICommand(reason, tc, self, ai, champion, selfPos, targetPos,
            target, cmd.kind, cmd.slot);
        return true;
    }
```

기존 코드:

```cpp
            if (EmitSkillCommand(world, tc, self, ai, champion.id, selfPos, target,
                rule.slot, "lane-attack-champion-skill", outCommands))
```

아래로 교체:

```cpp
            if (EmitSkillCommand(world, tc, self, ai, champion.id, selfPos, target,
                rule.slot, eChampionAIAction::AttackChampion,
                "lane-attack-champion-skill", outCommands))
```

`TryEmitAttackChampion` 함수 바로 아래 기존 코드:

```cpp
    bool_t TryEmitAttackChampion(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        const ChampionAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        const EntityID target = ctx.enemyChampion;
        if (target == NULL_ENTITY)
            return false;

        const ChampionAIProfile& profile = GetChampionAIProfile(champion.id);
        if (TryEmitAttackChampionSkill(world, tc, self, ai, champion, selfPos,
            target, profile, ctx, outCommands))
        {
            return true;
        }

        if (EmitBasicAttackCommand(world, tc, self, ai, champion.id, selfPos,
            target, eChampionAIAction::AttackChampion, "lane-attack-champion-ba", outCommands))
        {
            return true;
        }

        Vec3 targetPos{};
        if (!TryGetPosition(world, target, targetPos))
            return false;

        return EmitMoveCommand(world, tc, self, ai, champion.id, selfPos,
            targetPos, eChampionAIAction::AttackChampion,
            "lane-attack-champion-move", outCommands);
    }
```

아래에 추가:

```cpp
    bool_t TryResolveAITacticTarget(
        const ChampionAIContext& ctx,
        ChampionAITacticTarget targetKind,
        EntityID& outTarget)
    {
        outTarget = NULL_ENTITY;
        switch (targetKind)
        {
        case ChampionAITacticTarget::EnemyChampion:
            outTarget = ctx.enemyChampion;
            return outTarget != NULL_ENTITY;
        case ChampionAITacticTarget::EnemyMinion:
            outTarget = ctx.enemyMinion;
            return outTarget != NULL_ENTITY;
        case ChampionAITacticTarget::Self:
            return true;
        default:
            return false;
        }
    }

    bool_t RollChampionAITacticChance(
        const TickContext& tc,
        EntityID self,
        const ChampionAIComponent& ai,
        const ChampionAISkillRule& rule,
        u8_t index)
    {
        const f32_t chance = Clamp01(rule.chance);
        if (chance >= 1.f)
            return true;
        if (chance <= 0.f)
            return false;

        const u32_t roll = (MakeChampionAIRoll(tc, self, ai) ^
            (static_cast<u32_t>(rule.slot) * 2654435761u) ^
            (static_cast<u32_t>(index) * 2246822519u)) & 0xFFFFu;
        return static_cast<f32_t>(roll) / 65535.f < chance;
    }

    bool_t ShouldPassAITacticCondition(
        CWorld& world,
        eChampion champion,
        EntityID self,
        const ChampionAISkillRule& rule,
        EntityID target)
    {
        switch (rule.condition)
        {
        case ChampionAITacticCondition::Always:
            return true;
        case ChampionAITacticCondition::LastHit:
            return CanSkillLastHitTarget(
                world,
                champion,
                rule.slot,
                ResolveAISkillRank(world, self, rule.slot),
                target);
        default:
            return false;
        }
    }

    bool_t TryEmitFarmSkill(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        const ChampionAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        const ChampionAIProfile& profile = GetChampionAIProfile(champion.id);
        const u8_t count = std::min(profile.farmSkillRuleCount, static_cast<u8_t>(4));
        for (u8_t i = 0; i < count; ++i)
        {
            const ChampionAISkillRule& rule = profile.farmSkillRules[i];
            if (rule.score <= 0.f ||
                rule.slot == static_cast<u8_t>(eSkillSlot::BasicAttack))
            {
                continue;
            }

            EntityID target = NULL_ENTITY;
            if (!TryResolveAITacticTarget(ctx, rule.target, target))
                continue;

            if (!ShouldPassAITacticCondition(world, champion.id, self, rule, target))
                continue;

            if (!RollChampionAITacticChance(tc, self, ai, rule, i))
                continue;

            if (EmitSkillCommand(world, tc, self, ai, champion.id, selfPos, target,
                rule.slot, eChampionAIAction::AttackMinion,
                "lane-farm-skill", outCommands))
            {
                return true;
            }
        }

        return false;
    }
```

기존 코드:

```cpp
        if (ctx.enemyMinion != NULL_ENTITY)
        {
            if (EmitBasicAttackCommand(world, tc, self, ai, champion.id, selfPos,
                ctx.enemyMinion, eChampionAIAction::AttackMinion,
                "lane-attack-minion-ba", outCommands))
            {
                return;
            }
```

아래로 교체:

```cpp
        if (ctx.enemyMinion != NULL_ENTITY)
        {
            if (TryEmitFarmSkill(world, tc, self, ai, champion, selfPos, ctx, outCommands))
                return;

            if (EmitBasicAttackCommand(world, tc, self, ai, champion.id, selfPos,
                ctx.enemyMinion, eChampionAIAction::AttackMinion,
                "lane-attack-minion-ba", outCommands))
            {
                return;
            }
```

2. 검증

검증 명령:

```powershell
git diff --check
rg -n "ResolveAttackChampionSlot|HarrasChampion|BotLaneAI|CBotLaneAISystem|eBotLaneAI|kBotLaneAI" Shared Server Client -S
MSBuild.exe "Server/Include/Server.vcxproj" /p:Configuration=Debug /p:Platform=x64
MSBuild.exe "Client/Include/Client.vcxproj" /p:Configuration=Debug /p:Platform=x64
```

수동 확인:
- F9 AI 패널에서 `Intent=FarmMinion` 상태의 `Action=AttackMinion` / `cmd=CastSkill` / `slot=2` 로그가 Jax W farm tactic으로 낮은 빈도 출력되는지 확인.
- Irelia가 적 미니언 체력이 Q 추정 피해 이하일 때 `slot=1` `lane-farm-skill`을 먼저 emit하고, 그 외에는 기존 basic attack farm으로 떨어지는지 확인.
- 공통 Stage1의 10% `AttackChampion`, 포탑 공격, 위험 후퇴 로그가 Session 3 이후 동작과 동일하게 유지되는지 확인.
