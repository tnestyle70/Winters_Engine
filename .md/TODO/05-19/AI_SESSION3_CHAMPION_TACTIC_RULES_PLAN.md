Session - Champion AI Session 3에서 공통 LaneCombat 위에 챔피언별 Farm/Trade 스킬 규칙을 얹는다.

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
```

기존 코드:

```cpp
    ChampionAISkillRule skillRules[4]{};
    u8_t skillRuleCount = 0;
```

아래로 교체:

```cpp
    ChampionAISkillRule skillRules[4]{};
    u8_t skillRuleCount = 0;
    f32_t attackChampionChance = 0.10f;
    ChampionAISkillRule farmSkillRules[4]{};
    u8_t farmSkillRuleCount = 0;
```

1-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAIPolicy.cpp

기존 코드:

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
            0.10f,
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

기존 코드:

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
            0.10f,
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
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
```

기존 코드:

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
            return 1u;
        }

        const u8_t rank = world.GetComponent<SkillRankComponent>(self).ranks[slot];
        return rank == 0u ? 1u : rank;
    }

    bool_t TryGetTargetHealth(CWorld& world, EntityID target, f32_t& outHealth)
    {
        if (target == NULL_ENTITY || !world.IsAlive(target))
            return false;

        if (world.HasComponent<HealthComponent>(target))
        {
            outHealth = world.GetComponent<HealthComponent>(target).fCurrent;
            return outHealth > 0.f;
        }

        if (world.HasComponent<MinionComponent>(target))
        {
            outHealth = world.GetComponent<MinionComponent>(target).hp;
            return outHealth > 0.f;
        }

        if (world.HasComponent<ChampionComponent>(target))
        {
            outHealth = world.GetComponent<ChampionComponent>(target).hp;
            return outHealth > 0.f;
        }

        if (world.HasComponent<StructureComponent>(target))
        {
            outHealth = world.GetComponent<StructureComponent>(target).hp;
            return outHealth > 0.f;
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

    bool_t CanSkillLastHitTarget(CWorld& world, EntityID self, eChampion champion, EntityID target, u8_t slot)
    {
        f32_t targetHealth = 0.f;
        if (!TryGetTargetHealth(world, target, targetHealth))
            return false;

        const u8_t rank = ResolveAISkillRank(world, self, slot);
        const f32_t damage = ResolveChampionAISkillDamage(champion, slot, rank);
        return damage > 0.f && targetHealth <= damage;
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

        ai.lastAction = action;
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

기존 코드:

```cpp
    bool_t TryEmitAttackChampionSkill(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        EntityID target,
        const ChampionAIProfile& profile,
        const ChampionAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        const u8_t count = std::min(profile.skillRuleCount, static_cast<u8_t>(4));
        for (u8_t i = 0; i < count; ++i)
        {
            const ChampionAISkillRule& rule = profile.skillRules[i];
            if (rule.score <= 0.f ||
                rule.slot == static_cast<u8_t>(eSkillSlot::BasicAttack) ||
                ctx.enemyDistance + 0.001f < rule.minRange)
            {
                continue;
            }

            if (EmitSkillCommand(world, tc, self, ai, champion.id, selfPos, target,
                rule.slot, "lane-attack-champion-skill", outCommands))
            {
                return true;
            }
        }

        return false;
    }
```

아래로 교체:

```cpp
    bool_t TryEmitAttackChampionSkill(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        EntityID target,
        const ChampionAIProfile& profile,
        const ChampionAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        const u8_t count = std::min(profile.skillRuleCount, static_cast<u8_t>(4));
        for (u8_t i = 0; i < count; ++i)
        {
            const ChampionAISkillRule& rule = profile.skillRules[i];
            if (rule.score <= 0.f ||
                rule.slot == static_cast<u8_t>(eSkillSlot::BasicAttack) ||
                ctx.enemyDistance + 0.001f < rule.minRange)
            {
                continue;
            }

            if (EmitSkillCommand(world, tc, self, ai, champion.id, selfPos, target,
                rule.slot, eChampionAIAction::AttackChampion,
                "lane-attack-champion-skill", outCommands))
            {
                return true;
            }
        }

        return false;
    }
```

기존 코드:

```cpp
    bool_t RollAttackChampion(const TickContext& tc, EntityID self, const ChampionAIComponent& ai)
    {
        const f32_t chance = Clamp01(ai.attackChampionChance);
        u32_t x = static_cast<u32_t>(tc.tickIndex) ^
            static_cast<u32_t>(tc.tickIndex >> 32) ^
            (static_cast<u32_t>(self) * 747796405u) ^
            (ai.nextCommandSequence * 2891336453u);
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        return (x & 0xFFFFu) < static_cast<u32_t>(chance * 65535.f);
    }
```

아래로 교체:

```cpp
    bool_t RollChampionAIChance(
        const TickContext& tc,
        EntityID self,
        const ChampionAIComponent& ai,
        f32_t chance,
        u32_t salt)
    {
        const f32_t clampedChance = Clamp01(chance);
        u32_t x = static_cast<u32_t>(tc.tickIndex) ^
            static_cast<u32_t>(tc.tickIndex >> 32) ^
            (static_cast<u32_t>(self) * 747796405u) ^
            (ai.nextCommandSequence * 2891336453u) ^
            salt;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        return (x & 0xFFFFu) < static_cast<u32_t>(clampedChance * 65535.f);
    }

    bool_t RollAttackChampion(const TickContext& tc, EntityID self, const ChampionAIComponent& ai)
    {
        return RollChampionAIChance(tc, self, ai, ai.attackChampionChance, 0xA17C9u);
    }
```

기존 코드:

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
        EntityID self,
        const ChampionAISkillRule& rule,
        EntityID& outTarget)
    {
        switch (rule.target)
        {
        case ChampionAITacticTarget::EnemyChampion:
            outTarget = ctx.enemyChampion;
            return outTarget != NULL_ENTITY;
        case ChampionAITacticTarget::EnemyMinion:
            outTarget = ctx.enemyMinion;
            return outTarget != NULL_ENTITY;
        case ChampionAITacticTarget::Self:
            outTarget = self;
            return outTarget != NULL_ENTITY;
        default:
            outTarget = NULL_ENTITY;
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
            if (!TryResolveAITacticTarget(ctx, self, rule, target))
                continue;

            if (rule.condition == ChampionAITacticCondition::LastHit &&
                !CanSkillLastHitTarget(world, self, champion.id, target, rule.slot))
            {
                continue;
            }

            if (!RollChampionAIChance(
                tc,
                self,
                ai,
                rule.chance,
                0xF00D1000u + static_cast<u32_t>(rule.slot) + static_cast<u32_t>(i)))
            {
                continue;
            }

            if (EmitSkillCommand(world, tc, self, ai, champion.id, selfPos,
                target, rule.slot, eChampionAIAction::AttackMinion,
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

기존 코드:

```cpp
            ai.leashRange = profile.leashRange;
            ai.retreatHpRatio = profile.retreatHpRatio;
            ai.reengageHpRatio = profile.reengageHpRatio;
```

아래로 교체:

```cpp
            ai.leashRange = profile.leashRange;
            ai.retreatHpRatio = profile.retreatHpRatio;
            ai.reengageHpRatio = profile.reengageHpRatio;
            ai.attackChampionChance = profile.attackChampionChance;
```

2. 검증

미검증:
- Jax가 미니언 교전 중 `farmSkillRules`의 10% 확률로 W를 먼저 사용하고 이후 평타로 이어지는지 런타임 미검증.
- Irelia가 적 미니언 체력이 Q 피해량 이하일 때 Q로 막타를 시도하는지 런타임 미검증.
- 기존 `AttackChampion` 분기에서 Jax/Fiora/MasterYi Q, Ashe W 정책이 유지되는지 런타임 미검증.

검증 명령:

```powershell
git diff --check
rg -n "ResolveAttackChampionSlot|HarrasChampion|HarassChampion|BotLaneAI|CBotLaneAISystem|eBotLaneAI|kBotLaneAI" Shared Server Client -S
& "C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" "Server/Include/Server.vcxproj" /p:Configuration=Debug /p:Platform=x64
& "C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" "Client/Include/Client.vcxproj" /p:Configuration=Debug /p:Platform=x64
```

확인 필요:
- F5에서 F9 `AI Debug`를 열고 Jax가 `LaneCombat / AttackMinion` 중 W `CastSkill` 로그를 간헐적으로 내는지 확인.
- Irelia가 막타 가능 미니언 앞에서 Q `CastSkill` 로그를 내고 사거리 밖이면 이동으로 빠지는지 확인.
- 포탑 내부, 적 챔피언 접근, 후퇴 분기가 Session 2 반영 뒤에도 유지되는지 확인.
