Session - Champion AI를 HFSM root state, LaneCombat Behavior Tree, ChampionAIComponent personal Blackboard 형태로 작게 정리하고 combo 완료 후 FarmMinion 복귀를 명시한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAISystem.cpp

`CanUseComboStep` 함수 바로 아래에 `Behavior Tree` leaf 결과와 personal Blackboard helper를 추가한다.

기존 코드:

```cpp
    bool_t CanUseComboStep(
        CWorld& world,
        EntityID self,
        const ChampionAIComboStep& step,
        const ChampionAIContext& ctx)
    {
        if (step.slot >= static_cast<u8_t>(eSkillSlot::SLOT_END))
            return false;
        if (step.selfHpMinRatio > 0.f && ctx.selfHpRatio + 0.001f < step.selfHpMinRatio)
            return false;
        if (step.enemyHpMaxRatio < 0.999f && ctx.enemyHpRatio > step.enemyHpMaxRatio)
            return false;
        if (ctx.enemyDistance + 0.001f < step.minRange)
            return false;
        if (step.maxRange > 0.f && ctx.enemyDistance > step.maxRange)
            return false;
        return IsSkillReady(world, self, step.slot);
    }
```

아래에 추가:

```cpp
    enum class eChampionAIBehaviorStatus : u8_t
    {
        Failure,
        Success,
        Running,
    };

    void SetChampionAIIntent(
        ChampionAIComponent& ai,
        eChampionAIIntent intent,
        bool_t bHoldIntent = false)
    {
        ai.intent = intent;
        if (bHoldIntent)
            ai.intentHoldTimer = ai.intentHoldDuration;
    }

    void ClearChampionAICombo(ChampionAIComponent& ai)
    {
        ai.comboTarget = NULL_ENTITY;
        ai.comboStep = 0u;
    }

    void CompleteChampionAICombo(ChampionAIComponent& ai)
    {
        ClearChampionAICombo(ai);
        SetChampionAIIntent(ai, eChampionAIIntent::FarmMinion, true);
        ai.lastDecisionRoll = 1.f;
    }
```

의도:
- `eChampionAIBehaviorStatus`는 LaneCombat 안의 작은 BT leaf 결과다.
- `ChampionAIComponent`는 당장 별도 BlackboardComponent로 분리하지 않고 personal Blackboard로 쓴다.
- combo 완료 시 `FarmMinion` intent를 hold해서 다음 판단 틱에 바로 Harass 재추첨으로 튀는 일을 줄인다.

1-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAISystem.cpp

`TryEmitAttackChampionCombo` 함수를 아래로 교체한다.

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
                ClearChampionAICombo(ai);
            return bWasActive;
        }

        if (EmitChampionAIComboStep(world, tc, self, ai, champion, selfPos,
            target, step, outCommands))
        {
            const u8_t nextStep = static_cast<u8_t>((index + 1u) % stepCount);
            ai.comboStep = nextStep;
            if (nextStep == 0u)
                CompleteChampionAICombo(ai);
            return true;
        }

        if (!bWasActive)
            ClearChampionAICombo(ai);
        return bWasActive;
    }
```

의도:
- combo가 진행 중인데 현재 step을 못 쓰는 경우는 기존처럼 active combo를 유지한다.
- combo 마지막 step을 emit한 뒤에는 `CompleteChampionAICombo`로 Farm 복귀 intent를 명시한다.

1-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAISystem.cpp

`ClearChampionAITargets` 함수 안의 combo 초기화를 helper 호출로 교체한다.

기존 코드:

```cpp
    void ClearChampionAITargets(ChampionAIComponent& ai)
    {
        ai.lockedChampion = NULL_ENTITY;
        ai.targetMinion = NULL_ENTITY;
        ai.targetStructure = NULL_ENTITY;
        ai.alliedWave = NULL_ENTITY;
        ai.comboTarget = NULL_ENTITY;
        ai.comboStep = 0u;
        ai.bStructureWaveTanking = false;
        ai.bInsideEnemyTurretDanger = false;
    }
```

아래로 교체:

```cpp
    void ClearChampionAITargets(ChampionAIComponent& ai)
    {
        ai.lockedChampion = NULL_ENTITY;
        ai.targetMinion = NULL_ENTITY;
        ai.targetStructure = NULL_ENTITY;
        ai.alliedWave = NULL_ENTITY;
        ClearChampionAICombo(ai);
        ai.bStructureWaveTanking = false;
        ai.bInsideEnemyTurretDanger = false;
    }
```

1-4. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAISystem.cpp

`ExecuteRecalling` 함수 바로 아래, `ExecuteLaneCombat` 함수 바로 위에 LaneCombat BT leaf helper들을 추가한다.

기존 코드:

```cpp
    void ExecuteRecalling(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        std::vector<GameCommand>& outCommands)
    {
        ai.state = eChampionAIState::Recalling;
        ai.intent = eChampionAIIntent::Recall;
        ai.lastAction = eChampionAIAction::Recall;
        ClearChampionAITargets(ai);

        if (HasActiveRecall(world, self))
            return;

        ai.state = eChampionAIState::MoveToOuterTurret;
        ai.intent = eChampionAIIntent::FarmMinion;
        ai.bWaveJoined = false;
        EmitMoveCommand(world, tc, self, ai, champion.id, selfPos,
            ai.safeAnchor, eChampionAIAction::MoveToSafeAnchor,
            "recall-return-to-lane", outCommands);
    }
```

아래에 추가:

```cpp
    eChampionAIBehaviorStatus TickActiveChampionCombo(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        const ChampionAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        if (ai.comboTarget != ctx.enemyChampion ||
            ai.comboTarget == NULL_ENTITY ||
            !CanHarassChampion(ai, ctx))
        {
            return eChampionAIBehaviorStatus::Failure;
        }

        const size_t commandCountBefore = outCommands.size();
        if (!TryEmitAttackChampionCombo(world, tc, self, ai, champion, selfPos,
            ctx.enemyChampion, ctx, outCommands))
        {
            return eChampionAIBehaviorStatus::Failure;
        }

        return outCommands.size() > commandCountBefore
            ? eChampionAIBehaviorStatus::Success
            : eChampionAIBehaviorStatus::Running;
    }

    bool_t TryExecuteStructureAttack(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        const ChampionAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        if (ctx.enemyChampion != NULL_ENTITY ||
            ctx.enemyStructure == NULL_ENTITY ||
            !ctx.bStructureWaveTanking)
        {
            return false;
        }

        SetChampionAIIntent(ai, eChampionAIIntent::SiegeStructure);
        if (EmitBasicAttackCommand(world, tc, self, ai, champion.id, selfPos,
            ctx.enemyStructure, eChampionAIAction::AttackStructure,
            "lane-attack-structure-ba", outCommands))
        {
            return true;
        }

        return EmitMoveToTarget(world, tc, self, ai, champion.id, selfPos,
            ctx.enemyStructure, eChampionAIAction::AttackStructure,
            "lane-attack-structure-move", outCommands);
    }

    bool_t TryStartChampionHarass(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        const ChampionAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        return ShouldAttackChampion(tc, self, ai, ctx) &&
            TryEmitAttackChampion(world, tc, self, ai, champion, selfPos, ctx, outCommands);
    }

    bool_t TryExecuteMinionFarm(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        const ChampionAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        if (ctx.enemyMinion == NULL_ENTITY)
            return false;

        if (EmitBasicAttackCommand(world, tc, self, ai, champion.id, selfPos,
            ctx.enemyMinion, eChampionAIAction::AttackMinion,
            "lane-attack-minion-ba", outCommands))
        {
            return true;
        }

        return EmitMoveToTarget(world, tc, self, ai, champion.id, selfPos,
            ctx.enemyMinion, eChampionAIAction::AttackMinion,
            "lane-attack-minion-move", outCommands);
    }

    bool_t TryExecuteFollowWave(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        const ChampionAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        if (ctx.alliedWave != NULL_ENTITY &&
            EmitMoveToTarget(world, tc, self, ai, champion.id, selfPos,
                ctx.alliedWave, eChampionAIAction::FollowWave,
                "lane-follow-wave", outCommands))
        {
            return true;
        }

        return EmitMoveCommand(world, tc, self, ai, champion.id, selfPos,
            ai.laneGoal, eChampionAIAction::FollowWave, "lane-goal", outCommands);
    }
```

의도:
- `ExecuteLaneCombat`의 긴 if-chain을 작은 BT leaf로 읽히게 한다.
- `TickActiveChampionCombo`는 active combo가 아직 끝나지 않았으면 `Running`으로 Farm leaf 진입을 막는다.

1-5. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAISystem.cpp

`ExecuteLaneCombat` 함수를 아래로 교체한다.

기존 코드:

```cpp
    void ExecuteLaneCombat(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        const ChampionAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        ai.state = eChampionAIState::LaneCombat;
        ai.lockedChampion = ctx.enemyChampion;
        ai.targetMinion = ctx.enemyMinion;
        ai.targetStructure = ctx.enemyStructure;
        ai.alliedWave = ctx.alliedWave;
        ai.bStructureWaveTanking = ctx.bStructureWaveTanking;
        ai.bInsideEnemyTurretDanger = ctx.bInsideEnemyTurretDanger;

        if (ctx.selfHpRatio <= ai.retreatHpRatio ||
            (ctx.bInsideEnemyTurretDanger && ctx.enemyChampion != NULL_ENTITY) ||
            (ctx.turretDanger > 0.85f && !ctx.bStructureWaveTanking))
        {
            EmitRetreat(world, tc, self, ai, champion, selfPos, outCommands);
            return;
        }

        if (ctx.enemyChampion == NULL_ENTITY &&
            ctx.enemyStructure != NULL_ENTITY &&
            ctx.bStructureWaveTanking)
        {
            ai.intent = eChampionAIIntent::SiegeStructure;
            if (EmitBasicAttackCommand(world, tc, self, ai, champion.id, selfPos,
                ctx.enemyStructure, eChampionAIAction::AttackStructure,
                "lane-attack-structure-ba", outCommands))
            {
                return;
            }

            if (EmitMoveToTarget(world, tc, self, ai, champion.id, selfPos,
                ctx.enemyStructure, eChampionAIAction::AttackStructure,
                "lane-attack-structure-move", outCommands))
            {
                return;
            }
        }

        if (ai.comboTarget == ctx.enemyChampion &&
            ai.comboTarget != NULL_ENTITY &&
            CanHarassChampion(ai, ctx) &&
            TryEmitAttackChampionCombo(world, tc, self, ai, champion, selfPos,
                ctx.enemyChampion, ctx, outCommands))
        {
            return;
        }

        if (ShouldAttackChampion(tc, self, ai, ctx) &&
            TryEmitAttackChampion(world, tc, self, ai, champion, selfPos, ctx, outCommands))
        {
            return;
        }

        ai.intent = eChampionAIIntent::FarmMinion;
        ai.comboTarget = NULL_ENTITY;
        ai.comboStep = 0u;

        if (ctx.enemyMinion != NULL_ENTITY)
        {
            if (EmitBasicAttackCommand(world, tc, self, ai, champion.id, selfPos,
                ctx.enemyMinion, eChampionAIAction::AttackMinion,
                "lane-attack-minion-ba", outCommands))
            {
                return;
            }

            if (EmitMoveToTarget(world, tc, self, ai, champion.id, selfPos,
                ctx.enemyMinion, eChampionAIAction::AttackMinion,
                "lane-attack-minion-move", outCommands))
            {
                return;
            }
        }

        if (ctx.alliedWave != NULL_ENTITY &&
            EmitMoveToTarget(world, tc, self, ai, champion.id, selfPos,
                ctx.alliedWave, eChampionAIAction::FollowWave,
                "lane-follow-wave", outCommands))
        {
            return;
        }

        EmitMoveCommand(world, tc, self, ai, champion.id, selfPos,
            ai.laneGoal, eChampionAIAction::FollowWave, "lane-goal", outCommands);
    }
```

아래로 교체:

```cpp
    void ExecuteLaneCombat(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        const ChampionAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        ai.state = eChampionAIState::LaneCombat;
        ai.lockedChampion = ctx.enemyChampion;
        ai.targetMinion = ctx.enemyMinion;
        ai.targetStructure = ctx.enemyStructure;
        ai.alliedWave = ctx.alliedWave;
        ai.bStructureWaveTanking = ctx.bStructureWaveTanking;
        ai.bInsideEnemyTurretDanger = ctx.bInsideEnemyTurretDanger;

        if (ctx.selfHpRatio <= ai.retreatHpRatio ||
            (ctx.bInsideEnemyTurretDanger && ctx.enemyChampion != NULL_ENTITY) ||
            (ctx.turretDanger > 0.85f && !ctx.bStructureWaveTanking))
        {
            EmitRetreat(world, tc, self, ai, champion, selfPos, outCommands);
            return;
        }

        const eChampionAIBehaviorStatus comboStatus =
            TickActiveChampionCombo(world, tc, self, ai, champion, selfPos, ctx, outCommands);
        if (comboStatus != eChampionAIBehaviorStatus::Failure)
            return;

        if (TryExecuteStructureAttack(world, tc, self, ai, champion, selfPos, ctx, outCommands))
            return;

        if (TryStartChampionHarass(world, tc, self, ai, champion, selfPos, ctx, outCommands))
            return;

        SetChampionAIIntent(ai, eChampionAIIntent::FarmMinion);
        ClearChampionAICombo(ai);

        if (TryExecuteMinionFarm(world, tc, self, ai, champion, selfPos, ctx, outCommands))
            return;

        TryExecuteFollowWave(world, tc, self, ai, champion, selfPos, ctx, outCommands);
    }
```

결과 구조:

```text
LaneCombatSelector
1. EmergencyRetreat
2. ContinueActiveChampionCombo
3. AttackStructureIfWaveTanking
4. StartChampionHarassByRoll
5. FarmMinion
6. FollowWave
```

1-6. C:/Users/user/Desktop/Winters/.md/TODO/05-21/CHAMPION_AI_HFSM_BT_BLACKBOARD_PLAN.md

기존 문서의 combo 완료 설명을 `intentHoldTimer = 0`에서 `intentHoldTimer = intentHoldDuration`으로 고친다.

기존 코드:

```text
comboTarget = NULL_ENTITY
comboStep = 0
intent = FarmMinion
intentHoldTimer = 0
```

아래로 교체:

```text
comboTarget = NULL_ENTITY
comboStep = 0
intent = FarmMinion
intentHoldTimer = intentHoldDuration
```

의도:
- combo 완료 직후 한 판단 구간은 FarmMinion으로 복귀한다.
- 이후 다시 10% AttackChampion roll을 통해 새 combo를 시작할 수 있다.

2. 검증

미검증:
- 코드 미반영
- combo 완료 후 FarmMinion intent hold 체감 미검증
- LaneCombat leaf 분리 후 로그 순서 미검증

검증 명령:
- `git diff --check`
- `msbuild Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64`
- `msbuild Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64`

수동 확인:
- AI Debug override로 AttackChampion을 강제했을 때 combo 도중 FarmMinion으로 넘어가지 않는지 확인.
- combo 마지막 step 이후 `comboTarget=NULL_ENTITY`, `comboStep=0`, `intent=FarmMinion`으로 정리되는지 확인.
- combo 완료 직후 바로 새 Harass가 아니라 Farm/FollowWave leaf로 내려가는지 확인.
- Retreat/Recalling은 여전히 combo보다 우선해서 위험 상황에서 combo를 끊는지 확인.
- Server 로그에서 `reason=combo-attack-champion-*` 이후 `lane-attack-minion-*` 또는 `lane-follow-wave`로 자연스럽게 복귀하는지 확인.
