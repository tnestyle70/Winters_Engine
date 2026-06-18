Session - 야스오 적 AI 봇을 현재 ChampionAISystem과 YasuoGameSim 기준으로 구현한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Champions/Yasuo/YasuoGameSim.cpp

기존 코드:

```cpp
    bool_t IsAirborne(CWorld& world, EntityID target)
    {
        return target != NULL_ENTITY &&
            world.IsAlive(target) &&
            world.HasComponent<YasuoAirborneComponent>(target);
    }
```

아래로 교체:

```cpp
    bool_t IsAirborne(CWorld& world, EntityID target)
    {
        if (target == NULL_ENTITY || !world.IsAlive(target))
            return false;

        if (world.HasComponent<YasuoAirborneComponent>(target))
            return true;

        return world.HasComponent<GameplayStateComponent>(target) &&
            (world.GetComponent<GameplayStateComponent>(target).stateFlags &
                kGameplayStateAirborneFlag) != 0u;
    }
```

목적:
- 야스오 토네이도로 뜬 대상뿐 아니라, 서버 범용 에어본 상태 플래그가 붙은 대상도 R 후보로 인식한다.
- 기존 `OnR`과 `FindAirborneTarget`이 같은 `IsAirborne` 경로를 쓰므로 AI/사람 입력 모두 같은 판정을 받는다.

1-2. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Definitions/MapDataFormats.h"
#include "Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"
```

아래에 추가:

```cpp
#include "Shared/GameSim/Champions/Yasuo/YasuoGameSim.h"
```

목적:
- AI가 야스오 Q stage와 에어본 R 후보를 서버 GameSim 기준으로 조회한다.

1-3. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp

기존 코드:

```cpp
    struct ChampionAIContext
    {
        EntityID enemyChampion = NULL_ENTITY;
        EntityID enemyMinion = NULL_ENTITY;
        EntityID enemyStructure = NULL_ENTITY;
        EntityID alliedWave = NULL_ENTITY;

        f32_t selfHpRatio = 1.f;
```

아래로 교체:

```cpp
    struct ChampionAIContext
    {
        EntityID enemyChampion = NULL_ENTITY;
        EntityID enemyMinion = NULL_ENTITY;
        EntityID enemyStructure = NULL_ENTITY;
        EntityID alliedWave = NULL_ENTITY;
        EntityID airborneChampion = NULL_ENTITY;
        EntityID yasuoDashMinion = NULL_ENTITY;

        f32_t selfHpRatio = 1.f;
```

기존 코드:

```cpp
        f32_t attackRange = 1.5f;
        f32_t waveDistance = 999.f;
        f32_t turretDanger = 0.f;

        bool_t bAlliedWaveNearby = false;
```

아래로 교체:

```cpp
        f32_t attackRange = 1.5f;
        f32_t waveDistance = 999.f;
        f32_t turretDanger = 0.f;
        f32_t airborneDistance = 999.f;
        f32_t yasuoDashMinionDistance = 999.f;

        u8_t yasuoQStage = 1u;
        bool_t bYasuoEActive = false;

        bool_t bAlliedWaveNearby = false;
```

목적:
- 공용 AI context에 야스오 판단 재료만 얇게 추가한다.
- 상태는 서버 컴포넌트에서 읽고, AI는 명령만 발행한다.

1-4. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp

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
        std::vector<GameCommand>& outCommands,
        u16_t itemId = 0u,
        u8_t targetMode = static_cast<u8_t>(eChampionAIComboTargetMode::TargetEntity))
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
        const char* reason,
        std::vector<GameCommand>& outCommands,
        u16_t itemId = 0u,
        u8_t targetMode = static_cast<u8_t>(eChampionAIComboTargetMode::TargetEntity),
        eChampionAIAction action = eChampionAIAction::AttackChampion)
```

기존 코드:

```cpp
        ai.lastAction = eChampionAIAction::AttackChampion;
```

아래로 교체:

```cpp
        ai.lastAction = action;
```

목적:
- 야스오 Q 파밍처럼 스킬을 미니언에게 쓰는 경우 `lastAction`을 `AttackMinion`으로 남길 수 있게 한다.
- 기존 호출은 default 값으로 유지된다.

1-5. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp

`FindEnemyMinion(...)` 함수 바로 아래에 아래 코드 추가:

```cpp
    EntityID FindYasuoDashMinionTowardChampion(
        CWorld& world,
        EntityID self,
        eTeam myTeam,
        u8_t lane,
        const Vec3& selfPos,
        EntityID targetChampion,
        f32_t eRange,
        f32_t scanRange,
        f32_t& outDistance)
    {
        outDistance = 999.f;

        Vec3 championPos{};
        if (!TryGetPosition(world, targetChampion, championPos))
            return NULL_ENTITY;

        constexpr f32_t kYasuoEDashThroughDistance = 0.75f;
        constexpr f32_t kYasuoEDashMaxDistance = 5.5f;

        const f32_t currentDistSq = WintersMath::DistanceSqXZ(selfPos, championPos);
        const f32_t eRangeSq = eRange * eRange;
        const f32_t scanRangeSq = scanRange * scanRange;
        EntityID best = NULL_ENTITY;
        f32_t bestScore = 0.f;
        f32_t bestDistSq = 999.f * 999.f;

        world.ForEach<MinionComponent, TransformComponent>(
            [&](EntityID e, MinionComponent& minion, TransformComponent& transform)
            {
                if (minion.team == myTeam ||
                    minion.laneType != lane ||
                    !IsAliveTarget(world, e) ||
                    !GameplayStateQuery::CanBeTargetedBy(world, self, e))
                {
                    return;
                }

                const Vec3 minionPos = transform.GetPosition();
                const f32_t minionDistSq = WintersMath::DistanceSqXZ(selfPos, minionPos);
                if (minionDistSq > eRangeSq || minionDistSq > scanRangeSq)
                    return;

                const Vec3 dir = WintersMath::DirectionXZ(selfPos, minionPos);
                const f32_t minionDist = std::sqrt(std::max(0.f, minionDistSq));
                const f32_t dashDistance =
                    std::min(minionDist + kYasuoEDashThroughDistance, kYasuoEDashMaxDistance);
                const Vec3 dashEnd{
                    selfPos.x + dir.x * dashDistance,
                    selfPos.y,
                    selfPos.z + dir.z * dashDistance
                };

                const f32_t afterDistSq = WintersMath::DistanceSqXZ(dashEnd, championPos);
                if (afterDistSq + 0.01f >= currentDistSq)
                    return;

                const f32_t score = currentDistSq - afterDistSq;
                if (score > bestScore)
                {
                    bestScore = score;
                    bestDistSq = minionDistSq;
                    best = e;
                }
            });

        outDistance = (best != NULL_ENTITY) ? std::sqrt(std::max(0.f, bestDistSq)) : 999.f;
        return best;
    }
```

목적:
- 야스오가 적 챔피언에게 직접 E를 못 탈 때, 적에게 가까워지는 미니언 E 후보를 고른다.
- 이 단계에서는 “같은 대상 E 재사용 제한”은 넣지 않는다. 반복 문제가 확인되면 다음 세션에서 `YasuoStateComponent` 또는 별도 서버 컴포넌트로 추가한다.

1-6. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp

기존 코드:

```cpp
        ctx.turretDanger = ComputeTurretDanger(
            world,
            champion.team,
            ai.lane,
            selfPos,
            ctx.bInsideEnemyTurretDanger);
        return ctx;
```

아래로 교체:

```cpp
        ctx.turretDanger = ComputeTurretDanger(
            world,
            champion.team,
            ai.lane,
            selfPos,
            ctx.bInsideEnemyTurretDanger);

        if (champion.id == eChampion::YASUO)
        {
            ctx.yasuoQStage = YasuoGameSim::ResolveQVariantStage(world, self);
            if (world.HasComponent<YasuoStateComponent>(self))
            {
                const auto& yasuo = world.GetComponent<YasuoStateComponent>(self);
                ctx.bYasuoEActive = yasuo.bEActive;
            }

            const f32_t rRange = GetDefaultChampionSkillRange(
                eChampion::YASUO,
                static_cast<u8_t>(eSkillSlot::R));
            ctx.airborneChampion = YasuoGameSim::FindAirborneTarget(
                world,
                self,
                champion.team,
                rRange > 0.f ? rRange : 14.f);
            if (ctx.airborneChampion != NULL_ENTITY)
            {
                Vec3 airbornePos{};
                if (TryGetPosition(world, ctx.airborneChampion, airbornePos))
                    ctx.airborneDistance =
                        std::sqrt(std::max(0.f, WintersMath::DistanceSqXZ(selfPos, airbornePos)));
            }

            if (ctx.enemyChampion != NULL_ENTITY)
            {
                const f32_t eRange = GetDefaultChampionSkillRange(
                    eChampion::YASUO,
                    static_cast<u8_t>(eSkillSlot::E));
                ctx.yasuoDashMinion = FindYasuoDashMinionTowardChampion(
                    world,
                    self,
                    champion.team,
                    ai.lane,
                    selfPos,
                    ctx.enemyChampion,
                    eRange,
                    ai.minionScanRange,
                    ctx.yasuoDashMinionDistance);
            }
        }

        return ctx;
```

목적:
- 야스오 전용 판단에 필요한 서버 상태를 context 단계에서 한 번만 수집한다.

1-7. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp

`TryEmitAttackChampion(...)` 함수 바로 위에 아래 코드 추가:

```cpp
    bool_t TryExecuteYasuoUltimate(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        const ChampionAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        if (champion.id != eChampion::YASUO ||
            ctx.airborneChampion == NULL_ENTITY ||
            !IsSkillReady(world, self, static_cast<u8_t>(eSkillSlot::R)))
        {
            return false;
        }

        return EmitSkillCommand(
            world,
            tc,
            self,
            ai,
            champion.id,
            selfPos,
            ctx.airborneChampion,
            static_cast<u8_t>(eSkillSlot::R),
            "yasuo-airborne-r",
            outCommands);
    }

    bool_t TryExecuteYasuoChampionCombat(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        const ChampionAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        if (champion.id != eChampion::YASUO)
            return false;

        if (TryExecuteYasuoUltimate(world, tc, self, ai, champion, selfPos, ctx, outCommands))
            return true;

        const EntityID target = ctx.enemyChampion;
        if (target == NULL_ENTITY)
            return false;

        if (!CanAttackChampion(ai, ctx))
            return false;

        const u8_t qSlot = static_cast<u8_t>(eSkillSlot::Q);
        const u8_t eSlot = static_cast<u8_t>(eSkillSlot::E);
        const f32_t qRange = GetDefaultChampionSkillRange(champion.id, qSlot);
        const f32_t eRange = GetDefaultChampionSkillRange(champion.id, eSlot);

        if (ctx.bYasuoEActive &&
            IsSkillReady(world, self, qSlot) &&
            (qRange <= 0.f || ctx.enemyDistance <= qRange))
        {
            return EmitSkillCommand(world, tc, self, ai, champion.id, selfPos,
                target, qSlot, "yasuo-eq-after-dash", outCommands);
        }

        if (IsSkillReady(world, self, eSlot) &&
            eRange > 0.f &&
            ctx.enemyDistance <= eRange &&
            ctx.enemyDistance > ctx.attackRange + 0.25f)
        {
            return EmitSkillCommand(world, tc, self, ai, champion.id, selfPos,
                target, eSlot, "yasuo-e-engage-champion", outCommands);
        }

        if (IsSkillReady(world, self, qSlot) &&
            (qRange <= 0.f || ctx.enemyDistance <= qRange))
        {
            return EmitSkillCommand(world, tc, self, ai, champion.id, selfPos,
                target, qSlot, "yasuo-q-champion", outCommands);
        }

        if (EmitBasicAttackCommand(world, tc, self, ai, champion.id, selfPos,
            target, eChampionAIAction::AttackChampion, "yasuo-ba-champion", outCommands))
        {
            return true;
        }

        if (IsSkillReady(world, self, eSlot) &&
            ctx.yasuoDashMinion != NULL_ENTITY)
        {
            return EmitSkillCommand(world, tc, self, ai, champion.id, selfPos,
                ctx.yasuoDashMinion, eSlot, "yasuo-e-minion-gapclose", outCommands);
        }

        Vec3 targetPos{};
        if (!TryGetPosition(world, target, targetPos))
            return false;

        return EmitMoveCommand(world, tc, self, ai, champion.id, selfPos,
            targetPos, eChampionAIAction::AttackChampion,
            "yasuo-move-champion", outCommands);
    }
```

목적:
- R 우선, E 이후 Q 우선, E 진입, Q, BA, 미니언 E 경유 접근 순서로 야스오 챔피언 전투를 얇게 구현한다.
- AI는 GameCommand만 발행하고 실제 Q/E/R 처리는 기존 `YasuoGameSim`과 `CommandExecutor`가 담당한다.

1-8. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp

`TryExecuteMinionFarm(...)` 함수 바로 위에 아래 코드 추가:

```cpp
    bool_t TryExecuteYasuoMinionFarm(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        const ChampionAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        if (champion.id != eChampion::YASUO ||
            ctx.enemyMinion == NULL_ENTITY)
        {
            return false;
        }

        Vec3 minionPos{};
        if (!TryGetPosition(world, ctx.enemyMinion, minionPos))
            return false;

        const f32_t minionDistance =
            std::sqrt(std::max(0.f, WintersMath::DistanceSqXZ(selfPos, minionPos)));
        const u8_t qSlot = static_cast<u8_t>(eSkillSlot::Q);
        const f32_t qRange = GetDefaultChampionSkillRange(champion.id, qSlot);
        const f32_t minionHpRatio = HealthRatio(world, ctx.enemyMinion);

        if (IsSkillReady(world, self, qSlot) &&
            minionHpRatio <= 0.65f &&
            (qRange <= 0.f || minionDistance <= qRange))
        {
            return EmitSkillCommand(
                world,
                tc,
                self,
                ai,
                champion.id,
                selfPos,
                ctx.enemyMinion,
                qSlot,
                "yasuo-q-farm-minion",
                outCommands,
                0u,
                static_cast<u8_t>(eChampionAIComboTargetMode::TargetEntity),
                eChampionAIAction::AttackMinion);
        }

        return false;
    }
```

기존 코드:

```cpp
        if (TryExecuteMinionFarm(world, tc, self, ai, champion, selfPos, ctx, outCommands))
            return;
```

아래로 교체:

```cpp
        if (TryExecuteYasuoMinionFarm(world, tc, self, ai, champion, selfPos, ctx, outCommands))
            return;

        if (TryExecuteMinionFarm(world, tc, self, ai, champion, selfPos, ctx, outCommands))
            return;
```

목적:
- 기존 BA 파밍을 유지하면서 야스오만 Q 파밍을 우선 시도한다.

1-9. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp

기존 코드:

```cpp
        if (TryExecuteStructureAttack(world, tc, self, ai, champion, selfPos, ctx, outCommands))
            return;

        if (TryStartChampionAttack(world, tc, self, ai, champion, selfPos, ctx, outCommands))
            return;
```

아래로 교체:

```cpp
        if (TryExecuteStructureAttack(world, tc, self, ai, champion, selfPos, ctx, outCommands))
            return;

        if (TryExecuteYasuoChampionCombat(world, tc, self, ai, champion, selfPos, ctx, outCommands))
            return;

        if (TryStartChampionAttack(world, tc, self, ai, champion, selfPos, ctx, outCommands))
            return;
```

목적:
- 공용 구조물/후퇴 판단은 유지하고, 챔피언 교전 진입 시 야스오 전용 판단을 먼저 적용한다.

1-10. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp

기존 코드:

```cpp
        u32_t mask = 0u;
        if (ctx.enemyChampion == NULL_ENTITY)
            return mask;
```

아래로 교체:

```cpp
        u32_t mask = 0u;
        if (champion == eChampion::YASUO &&
            ctx.airborneChampion != NULL_ENTITY &&
            IsSkillReady(world, self, static_cast<u8_t>(eSkillSlot::R)))
        {
            mask |= 1u << (static_cast<u8_t>(eSkillSlot::R) - 1u);
        }

        if (ctx.enemyChampion == NULL_ENTITY)
            return mask;
```

목적:
- 야스오 R 가능 여부가 AI Debug skill mask에 나타나도록 한다.

1-11. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.cpp

기존 코드:

```cpp
            {
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::Q), 0.f, 1.f },
            },
            1
```

Yasuo `MakeYasuoProfile()` 안의 위 블록을 아래로 교체:

```cpp
            {
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::Q), 0.f, 1.f },
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::E), 0.f, 0.9f },
            },
            2
```

목적:
- 야스오 디버그 패널에서 Q/E 사용 가능성이 보인다.
- 실제 전투 우선순위는 `TryExecuteYasuoChampionCombat`이 담당한다.

1-12. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Components/ChampionAIComponent.h

확인 필요:
- 이번 세션에서는 enum에 새 `Action`/`Intent`를 추가하지 않는다.
- `eChampionAIAction::AttackChampion`과 `AttackMinion`만 사용해서 action mask 폭 변경을 피한다.
- 이후 “YasuoGapClose”, “CastUltimate” 같은 전용 액션을 UI에서 명확히 보고 싶으면 `stateFlags` packing을 건드리지 말고 별도 `aiDebugFlags` snapshot 확장 세션으로 분리한다.

2. 검증

정적 확인:
- `rg -n "TryExecuteYasuoChampionCombat|TryExecuteYasuoMinionFarm|yasuo-airborne-r|yasuo-e-minion-gapclose" Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp`
- `rg -n "bool_t IsAirborne|kGameplayStateAirborneFlag|FindAirborneTarget" Shared/GameSim/Champions/Yasuo/YasuoGameSim.cpp`
- `git diff --check`

빌드:
- `msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64 /m`

런타임 확인:
- 적 AI 봇을 야스오로 배치한다.
- 미니언 파밍 중 낮은 HP 미니언에게 BA가 나가는지 확인한다.
- Q ready 상태에서 낮은 HP 미니언에게 `yasuo-q-farm-minion` 로그가 찍히는지 확인한다.
- 적 챔피언이 감지 범위 안에 있고 E 사거리 밖/안 상황을 나눠 `yasuo-e-engage-champion`, `yasuo-q-champion`, `yasuo-ba-champion` 로그 순서를 확인한다.
- 적 챔피언이 직접 E 사거리 밖이고 미니언 E 경유가 유리할 때 `yasuo-e-minion-gapclose`가 찍히는지 확인한다.
- 야스오 Q 토네이도 또는 다른 서버 에어본 상태로 적이 뜬 상태에서 `yasuo-airborne-r` 명령이 나가고, `CommandExecutor`에서 `cast-skill accept` slot R로 이어지는지 확인한다.

주의:
- 이번 계획은 서버 권한 경로만 사용한다. AI는 Transform, HP, cooldown을 직접 수정하지 않는다.
- E 동일 타겟 재사용 제한은 이번 세션에서 제외한다. 반복 대시 문제가 실제로 보이면 다음 세션에서 서버 상태 컴포넌트로 추가한다.
- AI 디버그 텍스트에 Q stage, E active, airborne target, last command reason을 띄우는 작업은 26번 통합 디버그 계획의 snapshot 확장과 같이 묶는 것이 안전하다.
