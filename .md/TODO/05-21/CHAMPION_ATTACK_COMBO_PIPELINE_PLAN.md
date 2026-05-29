Session - Fiora, Ashe, Riven champion attack combo를 Jax combo pipeline 위에 하나씩 확장한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAIPolicy.h

기존 코드:

```cpp
struct ChampionAIComboStep
{
    u8_t slot = static_cast<u8_t>(eSkillSlot::BasicAttack);
    u16_t itemId = 0;
    f32_t minRange = 0.f;
    f32_t maxRange = 0.f;
    f32_t selfHpMinRatio = 0.f;
    f32_t enemyHpMaxRatio = 1.f;
};

struct ChampionAIComboPlan
{
    ChampionAIComboStep steps[6]{};
    u8_t stepCount = 0;
};
```

아래로 교체:

```cpp
enum class eChampionAIComboTargetMode : u8_t
{
    TargetEntity,
    AwayFromTarget,
};

struct ChampionAIComboStep
{
    u8_t slot = static_cast<u8_t>(eSkillSlot::BasicAttack);
    u16_t itemId = 0;
    f32_t minRange = 0.f;
    f32_t maxRange = 0.f;
    f32_t selfHpMinRatio = 0.f;
    f32_t enemyHpMaxRatio = 1.f;
    u8_t targetMode = static_cast<u8_t>(eChampionAIComboTargetMode::TargetEntity);
};

struct ChampionAIComboPlan
{
    ChampionAIComboStep steps[8]{};
    u8_t stepCount = 0;
};
```

1-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAIPolicy.cpp

기존 코드:

```cpp
const ChampionAIComboPlan& GetChampionAIComboPlan(eChampion champion)
{
    static constexpr ChampionAIComboPlan s_Default{};
    static constexpr ChampionAIComboPlan s_Jax{
        {
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::Q), 0, 0.f, 7.0f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::W), 0, 0.f, 1.75f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::E), 0, 0.f, 2.5f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::BasicAttack), 0, 0.f, 1.75f, 0.35f, 1.00f },
        },
        4
    };

    switch (champion)
    {
    case eChampion::JAX:
        return s_Jax;
    default:
        return s_Default;
    }
}
```

아래로 교체:

```cpp
const ChampionAIComboPlan& GetChampionAIComboPlan(eChampion champion)
{
    static constexpr ChampionAIComboPlan s_Default{};

    static constexpr ChampionAIComboPlan s_Jax{
        {
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::Q), 0, 0.f, 7.0f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::W), 0, 0.f, 1.75f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::E), 0, 0.f, 2.5f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::BasicAttack), 0, 0.f, 1.75f, 0.35f, 1.00f },
        },
        4
    };

    static constexpr ChampionAIComboPlan s_Fiora{
        {
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::Q), 0, 0.f, 4.0f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::E), 0, 0.f, 1.75f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::BasicAttack), 0, 0.f, 1.75f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::BasicAttack), 0, 0.f, 1.75f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::W), 0, 0.f, 2.5f, 0.35f, 1.00f },
        },
        5
    };

    static constexpr ChampionAIComboPlan s_Ashe{
        {
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::W), 0, 0.f, 9.0f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::Q), 0, 0.f, 6.0f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::BasicAttack), 0, 0.f, 6.0f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::BasicAttack), 0, 0.f, 6.0f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::BasicAttack), 0, 0.f, 6.0f, 0.35f, 1.00f },
        },
        5
    };

    static constexpr ChampionAIComboPlan s_Riven{
        {
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::Q), 0, 0.f, 4.5f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::BasicAttack), 0, 0.f, 1.75f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::Q), 0, 0.f, 4.5f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::BasicAttack), 0, 0.f, 1.75f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::Q), 0, 0.f, 4.5f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::W), 0, 0.f, 2.5f, 0.35f, 1.00f },
            ChampionAIComboStep{
                static_cast<u8_t>(eSkillSlot::E),
                0,
                0.f,
                4.0f,
                0.35f,
                1.00f,
                static_cast<u8_t>(eChampionAIComboTargetMode::AwayFromTarget)
            },
        },
        7
    };

    switch (champion)
    {
    case eChampion::ASHE:
        return s_Ashe;
    case eChampion::FIORA:
        return s_Fiora;
    case eChampion::JAX:
        return s_Jax;
    case eChampion::RIVEN:
        return s_Riven;
    default:
        return s_Default;
    }
}
```

1-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAISystem.cpp

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
        u16_t itemId = 0u)
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
        cmd.itemId = itemId;
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
        const char* reason,
        std::vector<GameCommand>& outCommands,
        u16_t itemId = 0u,
        u8_t targetMode = static_cast<u8_t>(eChampionAIComboTargetMode::TargetEntity))
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

        Vec3 commandPos = targetPos;
        Vec3 direction = WintersMath::DirectionXZ(selfPos, targetPos);
        if (targetMode == static_cast<u8_t>(eChampionAIComboTargetMode::AwayFromTarget))
        {
            direction = WintersMath::DirectionXZ(targetPos, selfPos);
            const f32_t castDistance = range > 0.f ? range : 4.f;
            commandPos = Vec3{
                selfPos.x + direction.x * castDistance,
                selfPos.y,
                selfPos.z + direction.z * castDistance
            };
        }

        ai.lastAction = eChampionAIAction::AttackChampion;
        GameCommand cmd = MakeAICommand(ai, tc, self, eCommandKind::CastSkill);
        cmd.slot = slot;
        cmd.itemId = itemId;
        cmd.targetEntity = target;
        cmd.groundPos = commandPos;
        cmd.direction = direction;
        outCommands.push_back(cmd);

        LogChampionAICommand(reason, tc, self, ai, champion, selfPos, commandPos,
            target, cmd.kind, cmd.slot);
        return true;
    }
```

기존 코드:

```cpp
    bool_t EmitChampionAIComboStep(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        EntityID target,
        const ChampionAIComboStep& step,
        std::vector<GameCommand>& outCommands)
    {
        if (step.slot == static_cast<u8_t>(eSkillSlot::BasicAttack))
        {
            return EmitBasicAttackCommand(world, tc, self, ai, champion.id, selfPos,
                target, eChampionAIAction::AttackChampion,
                "combo-attack-champion-ba", outCommands);
        }

        return EmitSkillCommand(world, tc, self, ai, champion.id, selfPos, target,
            step.slot, "combo-attack-champion-skill", outCommands, step.itemId);
    }
```

아래로 교체:

```cpp
    bool_t EmitChampionAIComboStep(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        EntityID target,
        const ChampionAIComboStep& step,
        std::vector<GameCommand>& outCommands)
    {
        if (step.slot == static_cast<u8_t>(eSkillSlot::BasicAttack))
        {
            return EmitBasicAttackCommand(world, tc, self, ai, champion.id, selfPos,
                target, eChampionAIAction::AttackChampion,
                "combo-attack-champion-ba", outCommands);
        }

        return EmitSkillCommand(world, tc, self, ai, champion.id, selfPos, target,
            step.slot, "combo-attack-champion-skill", outCommands,
            step.itemId, step.targetMode);
    }
```

기존 코드:

```cpp
    bool_t TryEmitAttackChampionCombo(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        EntityID target,
        const ChampionAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        const ChampionAIComboPlan& combo = GetChampionAIComboPlan(champion.id);
        if (combo.stepCount == 0u)
            return false;

        if (ai.comboTarget != target)
        {
            ai.comboTarget = target;
            ai.comboStep = 0u;
        }

        const u8_t stepCount = std::min(combo.stepCount, static_cast<u8_t>(6u));
        for (u8_t attempt = 0u; attempt < stepCount; ++attempt)
        {
            const u8_t index = static_cast<u8_t>((ai.comboStep + attempt) % stepCount);
            const ChampionAIComboStep& step = combo.steps[index];
            if (!CanUseComboStep(world, self, step, ctx))
                continue;

            if (EmitChampionAIComboStep(world, tc, self, ai, champion, selfPos,
                target, step, outCommands))
            {
                const u8_t nextStep = static_cast<u8_t>((index + 1u) % stepCount);
                ai.comboStep = nextStep;
                if (nextStep == 0u)
                    ai.comboTarget = NULL_ENTITY;
                return true;
            }
        }

        return false;
    }
```

아래로 교체:

```cpp
    bool_t TryEmitAttackChampionCombo(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        EntityID target,
        const ChampionAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        const ChampionAIComboPlan& combo = GetChampionAIComboPlan(champion.id);
        if (combo.stepCount == 0u)
            return false;

        const bool_t bWasActive = ai.comboTarget == target;
        if (!bWasActive)
        {
            ai.comboTarget = target;
            ai.comboStep = 0u;
        }

        const u8_t stepCount = std::min(combo.stepCount, static_cast<u8_t>(8u));
        const u8_t index = static_cast<u8_t>(ai.comboStep % stepCount);
        const ChampionAIComboStep& step = combo.steps[index];
        if (!CanUseComboStep(world, self, step, ctx))
        {
            if (!bWasActive)
            {
                ai.comboTarget = NULL_ENTITY;
                ai.comboStep = 0u;
            }
            return bWasActive;
        }

        if (EmitChampionAIComboStep(world, tc, self, ai, champion, selfPos,
            target, step, outCommands))
        {
            const u8_t nextStep = static_cast<u8_t>((index + 1u) % stepCount);
            ai.comboStep = nextStep;
            if (nextStep == 0u)
                ai.comboTarget = NULL_ENTITY;
            return true;
        }

        if (!bWasActive)
        {
            ai.comboTarget = NULL_ENTITY;
            ai.comboStep = 0u;
        }
        return bWasActive;
    }
```

2. 검증

미검증:
- 코드 미반영
- Fiora `Q -> E -> BasicAttack -> BasicAttack -> W` 체감 미검증
- Ashe `W -> Q -> BasicAttack -> BasicAttack -> BasicAttack` 체감 미검증
- Riven `Q -> BasicAttack -> Q -> BasicAttack -> Q -> W -> E away` 체감 미검증

검증 명령:
- `git diff --check`
- `msbuild Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64`
- `msbuild Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64`

확인 필요:
- Riven 서버 GameSim 전용 파일은 현재 없으므로 Q/W/E 커맨드 수락과 서버 fallback damage/이벤트는 확인하되, E dash/shield 체감까지 필요하면 별도 Riven server skill hook 세션으로 분리한다.
- Riven Q 반복은 현재 서버 cooldown에 따라 느리게 보일 수 있으므로, 실제 체감이 목표보다 느리면 Riven Q multi-cast timing/stage 처리를 별도 세션으로 분리한다.

수동 확인:
- AI Debug 패널에서 Fiora, Ashe, Riven의 `Champion` 공격 override를 눌렀을 때 `reason=combo-attack-champion-*` 로그가 계획 순서대로 이어지는지 확인.
- combo 진행 중 현재 step이 cooldown이면 다음 step으로 건너뛰지 않고 `comboTarget`을 유지한 뒤 같은 step을 다시 시도하는지 확인.
- Riven E step에서 `cmd.direction`이 적 챔피언 반대 방향으로 찍히고, `cmd.groundPos`가 self 기준 반대 방향 위치로 찍히는지 Server 로그로 확인.
