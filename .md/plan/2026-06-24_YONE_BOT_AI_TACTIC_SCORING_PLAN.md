Session - Yone Bot 전투 판단을 hard priority에서 reaction override + scored candidate로 줄인다.

1. 반영해야 하는 코드

본질:
- 요네 전투 AI의 첫 본질은 스킬을 많이 쓰는 것이 아니라 "살아 돌아올 수 있는 교전만 시작하고, 위험하면 E 2타로 복귀한다"이다.
- E 복귀는 반응형 override다. 점수 경쟁에 넣지 않는다.
- R, E engage, Q, W, BasicAttack은 후보로 보고, 현재 context에서 가장 단순한 점수 하나로 고른다.
- 이 단계는 챔피언 개별 tactic만 정리한다. 공통 AI architecture, 데이터 파일, Client visual은 건드리지 않는다.

현재 코드 증거:
- `C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp`의 `TryExecuteYoneChampionCombat(...)`는 현재 `E return -> R -> E engage -> Q -> W -> BA` 순서로 즉시 반환한다.
- 같은 파일의 `EmitSkillCommand(...)`는 스킬 랭크, 쿨다운, 타겟 생존, targetable, range를 이미 검증한다.
- `ChampionAIComponent`에는 `fChampionDecisionScore`, `fDecisionSelfHpRatio`, `fDecisionEnemyHpRatio`, `fDecisionEnemyDistance`, `fDecisionTurretDanger` 등이 이미 trace 가능한 값으로 들어간다.

1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp

`TryExecuteYoneChampionCombat(...)` 안의 E return 블록은 유지한다.

기존 코드:

```cpp
        // E recast is a stage-2 command. It must bypass the AI cooldown gate
        // and let CommandExecutor validate the active stage window.
        if (bSoulOut)
        {
            const bool_t bShouldReturn =
                pYoneState->soulTimerSec <= 0.75f ||
                ctx.selfHpRatio <= ai.reengageHpRatio ||
                ctx.bInsideEnemyTurretDanger ||
                ctx.enemyChampion == NULL_ENTITY ||
                ctx.selfHpRatio + ai.fChampionScoreMargin < ctx.enemyHpRatio;
            if (bShouldReturn)
            {
                ai.lastAction = eChampionAIAction::Retreat;
                GameCommand cmd = MakeAICommand(ai, tc, self, eCommandKind::CastSkill);
                cmd.slot = eSlot;
                cmd.itemId = 2u;
                cmd.direction = WintersMath::DirectionXZ(selfPos, pYoneState->anchorPosition);
                cmd.groundPos = pYoneState->anchorPosition;
                outCommands.push_back(cmd);
                RecordChampionAICommandDebug(ai, NULL_ENTITY, cmd.kind, cmd.slot, pYoneState->anchorPosition);
                PushChampionAIDecisionTrace(ai, tc, NULL_ENTITY);
                LogChampionAICommand("yone-e-soul-return", tc, self, ai, champion.id,
                    selfPos, pYoneState->anchorPosition, NULL_ENTITY, cmd.kind, cmd.slot);
                return true;
            }
        }
```

유지 이유:
- `itemId = 2u`는 stage-2 contract다. `EmitSkillCommand(...)`의 일반 쿨다운 gate를 통과시키면 안 된다.
- 복귀는 후보 점수보다 우선하는 생존 반응이다.

1-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp

`const EntityID target = ctx.enemyChampion;` 아래의 hard priority skill chain은 scored candidate loop로 교체한다.

기존 코드:

```cpp
        if ((ctx.enemyHpRatio <= 0.5f || bSoulOut) &&
            EmitSkillCommand(world, tc, self, ai, champion.id, selfPos, target,
                rSlot, "yone-r-champion", outCommands))
        {
            return true;
        }

        if (!bSoulOut &&
            ctx.enemyDistance > ctx.attackRange + 0.25f &&
            EmitSkillCommand(world, tc, self, ai, champion.id, selfPos, target,
                eSlot, "yone-e-soul-engage", outCommands))
        {
            return true;
        }

        if (EmitSkillCommand(world, tc, self, ai, champion.id, selfPos, target,
            qSlot, "yone-q-champion", outCommands))
        {
            return true;
        }

        if (EmitSkillCommand(world, tc, self, ai, champion.id, selfPos, target,
            wSlot, "yone-w-champion", outCommands))
        {
            return true;
        }

        return EmitBasicAttackCommand(world, tc, self, ai, champion.id, selfPos, target,
            eChampionAIAction::AttackChampion, "yone-ba-champion", outCommands);
```

아래로 교체:

```cpp
        struct YoneCombatCandidate
        {
            u8_t slot = 0u;
            f32_t score = -1.f;
            const char* reason = "";
        };

        YoneCombatCandidate candidates[4]{};
        u8_t candidateCount = 0u;
        const auto AddCandidate = [&](u8_t slot, f32_t score, const char* reason)
        {
            if (candidateCount >= static_cast<u8_t>(sizeof(candidates) / sizeof(candidates[0])))
                return;
            candidates[candidateCount++] = YoneCombatCandidate{ slot, score, reason };
        };

        const f32_t hpAdvantage =
            ctx.selfHpRatio - ctx.enemyHpRatio;
        const f32_t rangePressure =
            (ctx.enemyDistance > ctx.attackRange)
                ? WintersMath::Clamp01((ctx.enemyDistance - ctx.attackRange) / 5.f)
                : 0.f;
        const f32_t executePressure =
            WintersMath::Clamp01((0.55f - ctx.enemyHpRatio) / 0.55f);

        if (ctx.enemyHpRatio <= 0.5f || bSoulOut)
            AddCandidate(rSlot, 100.f + executePressure * 20.f, "yone-r-champion");

        if (!bSoulOut && ctx.enemyDistance > ctx.attackRange + 0.25f && hpAdvantage >= -0.10f)
            AddCandidate(eSlot, 70.f + rangePressure * 20.f + hpAdvantage * 10.f, "yone-e-soul-engage");

        AddCandidate(qSlot, 55.f + executePressure * 10.f, "yone-q-champion");
        AddCandidate(wSlot, 45.f + hpAdvantage * 10.f, "yone-w-champion");

        for (u8_t attempt = 0u; attempt < candidateCount; ++attempt)
        {
            u8_t bestIndex = 0xFFu;
            f32_t bestScore = -1.f;
            for (u8_t i = 0u; i < candidateCount; ++i)
            {
                if (candidates[i].score > bestScore)
                {
                    bestScore = candidates[i].score;
                    bestIndex = i;
                }
            }

            if (bestIndex == 0xFFu)
                break;

            const YoneCombatCandidate candidate = candidates[bestIndex];
            candidates[bestIndex].score = -1.f;
            if (EmitSkillCommand(world, tc, self, ai, champion.id, selfPos, target,
                candidate.slot, candidate.reason, outCommands))
            {
                return true;
            }
        }

        return EmitBasicAttackCommand(world, tc, self, ai, champion.id, selfPos, target,
            eChampionAIAction::AttackChampion, "yone-ba-champion", outCommands);
```

의도:
- 후보별 점수는 작고 해석 가능해야 한다.
- 실패한 후보는 `EmitSkillCommand(...)`가 block reason을 남기고, 다음 후보를 시도한다.
- R은 execute 또는 soul-out 상태에서 높은 점수다.
- E engage는 soul-out이 아니고, 적이 기본 공격 범위 밖이며, HP 불리함이 크지 않을 때만 후보가 된다.
- Q/W는 기본 교전 후보로 남긴다.
- BasicAttack은 스킬 후보가 모두 실패했을 때의 fallback이다.

중단 조건:
- 점수 구조가 `ChampionAIContext` 밖의 gameplay truth를 직접 읽기 시작하면 멈춘다.
- Yone tactic 정리를 위해 공통 AI state machine, GameCommand 구조체, CommandExecutor를 수정해야 하는 상황이면 이번 단계 범위를 넘은 것이다.
- score 계산이 champion data JSON으로 빼고 싶어질 정도로 커지면 먼저 로컬 후보 구조를 더 줄인다.

2. 검증

검증 명령:

```powershell
msbuild Shared/GameSim/Include/GameSim.vcxproj /p:Configuration=Debug /p:Platform=x64
msbuild Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64
powershell -NoProfile -ExecutionPolicy Bypass -File Tools\Harness\Run-BotAiValidation.ps1 -SkipFullPipeline
powershell -NoProfile -ExecutionPolicy Bypass -File Tools\LoLData\Verify-LoLDataDrivenPipeline.ps1 -Configuration Debug
```

성공 기준:
- Yone E stage-2 contract audit가 계속 PASS한다.
- SimLab Yone E return probe가 PASS한다.
- full pipeline이 `GameSim`, `Server`, `Client`, `SimLab`까지 PASS한다.
- Debug 로그에서 요네가 위험 조건에서 `yone-e-soul-return`을 우선 발행하고, 일반 교전에서는 `yone-r/e/q/w/ba` 중 하나를 발행한다.

핸드오프 메모:
- 이 계획은 "요네가 사람처럼 보이는 첫 단계"가 아니라 "우선순위를 점수 후보로 바꾸는 최소 단계"다.
- 후보 점수는 이후 champion JSON 또는 champion tactic table로 옮길 수 있지만, 지금은 C++ 한 함수 안에서 행동을 먼저 고정한다.
- 이 단계가 안정화된 뒤 챔피언 공통 combo contract를 별도 문서로 분리한다.
