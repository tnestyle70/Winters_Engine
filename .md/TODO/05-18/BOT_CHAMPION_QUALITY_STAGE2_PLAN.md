Session - Jax/Ezreal/Fiora/Ashe를 BotAI 품질 기준 챔피언으로 다듬는다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/BotLaneAISystem.cpp

`TryEmitJaxFightBT(...)` 안에서 기존 코드:

```cpp
        if (qRange > 0.f &&
            ctx.selfHpRatio > 0.45f &&
            ctx.turretDanger < 0.90f &&
            ctx.enemyDistance > ctx.attackRange * 0.85f &&
            ctx.enemyDistance <= qRange &&
            EmitSkillCommand(world, tc, self, ai, champion.id, selfPos, target, q,
                "bt-jax-q-engage", outCommands))
```

아래로 교체:

```cpp
        if (qRange > 0.f &&
            ctx.selfHpRatio > 0.45f &&
            ctx.turretDanger < 0.90f &&
            (ctx.minionAdvantage >= -1.f || ctx.enemyHpRatio < 0.50f) &&
            ctx.enemyDistance > ctx.attackRange * 0.85f &&
            ctx.enemyDistance <= qRange &&
            EmitSkillCommand(world, tc, self, ai, champion.id, selfPos, target, q,
                "bt-jax-q-engage", outCommands))
```

`TryEmitFioraFightBT(...)` 안에서 기존 코드:

```cpp
        if (ctx.selfHpRatio < 0.50f &&
            EmitSkillCommand(world, tc, self, ai, champion.id, selfPos, target, w,
                "bt-fiora-w-riposte", outCommands))
```

아래로 교체:

```cpp
        const f32_t wRange = GetDefaultChampionSkillRange(champion.id, w);
        if (wRange > 0.f &&
            ctx.selfHpRatio < 0.50f &&
            ctx.enemyDistance <= wRange &&
            EmitSkillCommand(world, tc, self, ai, champion.id, selfPos, target, w,
                "bt-fiora-w-riposte", outCommands))
```

`TryEmitAsheFightBT(...)` 안에서 기존 코드:

```cpp
        if (rRange > 0.f &&
            ctx.enemyHpRatio < 0.35f &&
            ctx.enemyDistance <= rRange &&
            EmitSkillCommand(world, tc, self, ai, champion.id, selfPos, target, r,
                "bt-ashe-r-finish", outCommands))
```

아래로 교체:

```cpp
        if (rRange > 0.f &&
            (ctx.enemyHpRatio < 0.35f ||
                (ctx.selfHpRatio < 0.45f && ctx.enemyDistance <= ctx.attackRange + 1.5f)) &&
            ctx.enemyDistance <= rRange &&
            EmitSkillCommand(world, tc, self, ai, champion.id, selfPos, target, r,
                "bt-ashe-r-finish-or-peel", outCommands))
```

`TryEmitEzrealFightBT(...)` 안에서 기존 코드:

```cpp
        if (qRange > 0.f &&
            ctx.selfHpRatio > 0.35f &&
            ctx.turretDanger < 1.00f &&
            ctx.enemyDistance <= qRange &&
            EmitSkillCommand(world, tc, self, ai, champion.id, selfPos, target, q,
                "bt-ezreal-q-poke", outCommands))
```

아래로 교체:

```cpp
        if (qRange > 0.f &&
            ctx.selfHpRatio > 0.35f &&
            ctx.turretDanger < 1.00f &&
            (ctx.enemyDistance > ctx.attackRange + 0.25f || ctx.enemyHpRatio < 0.55f) &&
            ctx.enemyDistance <= qRange &&
            EmitSkillCommand(world, tc, self, ai, champion.id, selfPos, target, q,
                "bt-ezreal-q-poke", outCommands))
```

`TryEmitChampionFightBT(...)` 바로 아래에 추가:

```cpp
    bool_t TryEmitChampionRetreatBT(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        BotLaneAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        const BotLaneAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        const EntityID target = ctx.enemyChampion;
        if (target == NULL_ENTITY)
            return false;

        switch (champion.id)
        {
        case eChampion::JAX:
        {
            const u8_t e = static_cast<u8_t>(eSkillSlot::E);
            if (ctx.enemyDistance <= ctx.attackRange + 1.25f &&
                (ctx.selfHpRatio < 0.65f || ctx.enemyMinionCount > ctx.allyMinionCount + 1.f) &&
                EmitSkillCommand(world, tc, self, ai, champion.id, selfPos, target, e,
                    "bt-retreat-jax-e-counter", outCommands))
            {
                return true;
            }
            break;
        }
        case eChampion::FIORA:
        {
            const u8_t w = static_cast<u8_t>(eSkillSlot::W);
            const f32_t wRange = GetDefaultChampionSkillRange(champion.id, w);
            if (wRange > 0.f &&
                ctx.selfHpRatio < 0.58f &&
                ctx.enemyDistance <= wRange &&
                EmitSkillCommand(world, tc, self, ai, champion.id, selfPos, target, w,
                    "bt-retreat-fiora-w-riposte", outCommands))
            {
                return true;
            }
            break;
        }
        case eChampion::ASHE:
        {
            const u8_t w = static_cast<u8_t>(eSkillSlot::W);
            const f32_t wRange = GetDefaultChampionSkillRange(champion.id, w);
            if (wRange > 0.f &&
                ctx.enemyDistance <= wRange &&
                ctx.enemyDistance <= ctx.attackRange + 2.0f &&
                EmitSkillCommand(world, tc, self, ai, champion.id, selfPos, target, w,
                    "bt-retreat-ashe-w-peel", outCommands))
            {
                return true;
            }
            break;
        }
        case eChampion::EZREAL:
        {
            const u8_t q = static_cast<u8_t>(eSkillSlot::Q);
            const f32_t qRange = GetDefaultChampionSkillRange(champion.id, q);
            if (qRange > 0.f &&
                ctx.enemyDistance <= qRange &&
                ctx.enemyDistance > ctx.attackRange &&
                EmitSkillCommand(world, tc, self, ai, champion.id, selfPos, target, q,
                    "bt-retreat-ezreal-q-space", outCommands))
            {
                return true;
            }
            break;
        }
        default:
            break;
        }

        return false;
    }
```

기존 코드:

```cpp
        case eBotLaneAIIntent::Retreat:
        {
            ai.lockedChampion = NULL_ENTITY;
            const Vec3 goal = ResolveRetreatGoal(world, ai, ctx, selfPos);
            return EmitMoveCommand(world, tc, self, ai, champion.id, selfPos, goal,
                "utility-retreat", outCommands);
        }
```

아래로 교체:

```cpp
        case eBotLaneAIIntent::Retreat:
        {
            ai.lockedChampion = ctx.enemyChampion;
            if (TryEmitChampionRetreatBT(world, tc, self, ai, champion, selfPos,
                ctx, outCommands))
            {
                return true;
            }

            ai.lockedChampion = NULL_ENTITY;
            const Vec3 goal = ResolveRetreatGoal(world, ai, ctx, selfPos);
            return EmitMoveCommand(world, tc, self, ai, champion.id, selfPos, goal,
                "utility-retreat", outCommands);
        }
```

2. 검증

미검증:
- 이 문서는 다음 단계 구현 계획만 작성했다. 코드 변경과 빌드는 아직 수행하지 않았다.

검증 명령:
- `git diff --check`
- `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1`
- `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1`

수동 확인:
- Custom Scene에서 Jax/Ezreal/Fiora/Ashe 봇을 각각 선택하고 AI Debug Panel로 `FightChampion`, `Kite`, `Retreat` 전환을 확인.
- Jax가 미니언 불리함이 심한 상태에서 Q로 무리하게 진입하지 않는지 확인.
- Fiora W가 대상과 너무 멀 때 허공 대응기로 낭비되지 않는지 확인.
- Ashe가 저체력 근접 위협에서 R 또는 W로 peel을 시도하는지 확인.
- Ezreal이 BA 거리 안에서는 무조건 Q만 반복하지 않고, 거리 벌리기/마무리 상황에서 Q를 우선 쓰는지 확인.
