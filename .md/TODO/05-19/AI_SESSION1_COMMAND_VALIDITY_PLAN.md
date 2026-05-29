Session - ChampionAI Session 1이 사거리 밖 공격 명령으로 멈추지 않게 유효 명령만 생성한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAISystem.cpp

기존 코드:

```cpp
    bool_t EmitBasicAttackCommand(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        eChampion champion,
        const Vec3& selfPos,
        EntityID target,
        eChampionAIAction action,
        const char* reason,
        std::vector<GameCommand>& outCommands)
    {
        if (!IsSkillReady(world, self, static_cast<u8_t>(eSkillSlot::BasicAttack)) ||
            target == NULL_ENTITY ||
            !IsAliveTarget(world, target) ||
            !GameplayStateQuery::CanBeTargetedBy(world, self, target))
        {
            return false;
        }

        ai.lastAction = action;
        GameCommand cmd = MakeAICommand(ai, tc, self, eCommandKind::BasicAttack);
        cmd.slot = static_cast<u8_t>(eSkillSlot::BasicAttack);
        cmd.targetEntity = target;
        outCommands.push_back(cmd);

        Vec3 targetPos{};
        TryGetPosition(world, target, targetPos);
        LogChampionAICommand(reason, tc, self, ai, champion, selfPos, targetPos,
            target, cmd.kind, cmd.slot);
        return true;
    }
```

아래로 교체:

```cpp
    bool_t EmitBasicAttackCommand(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        eChampion champion,
        const Vec3& selfPos,
        EntityID target,
        eChampionAIAction action,
        const char* reason,
        std::vector<GameCommand>& outCommands)
    {
        if (!IsSkillReady(world, self, static_cast<u8_t>(eSkillSlot::BasicAttack)) ||
            target == NULL_ENTITY ||
            !IsAliveTarget(world, target) ||
            !GameplayStateQuery::CanBeTargetedBy(world, self, target))
        {
            return false;
        }

        Vec3 targetPos{};
        if (!TryGetPosition(world, target, targetPos))
            return false;

        const f32_t attackRange = ResolveAttackRange(world, self, champion);
        if (attackRange > 0.f &&
            WintersMath::DistanceSqXZ(selfPos, targetPos) > attackRange * attackRange)
        {
            return false;
        }

        ai.lastAction = action;
        GameCommand cmd = MakeAICommand(ai, tc, self, eCommandKind::BasicAttack);
        cmd.slot = static_cast<u8_t>(eSkillSlot::BasicAttack);
        cmd.targetEntity = target;
        outCommands.push_back(cmd);

        LogChampionAICommand(reason, tc, self, ai, champion, selfPos, targetPos,
            target, cmd.kind, cmd.slot);
        return true;
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

        const u8_t slot = ResolveAttackChampionSlot(champion.id);
        if (slot != static_cast<u8_t>(eSkillSlot::BasicAttack) &&
            EmitSkillCommand(world, tc, self, ai, champion.id, selfPos, target,
                slot, "lane-attack-champion-skill", outCommands))
        {
            return true;
        }

        return EmitBasicAttackCommand(world, tc, self, ai, champion.id, selfPos,
            target, eChampionAIAction::AttackChampion, "lane-attack-champion-ba", outCommands);
    }
```

아래로 교체:

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

        const u8_t slot = ResolveAttackChampionSlot(champion.id);
        if (slot != static_cast<u8_t>(eSkillSlot::BasicAttack) &&
            EmitSkillCommand(world, tc, self, ai, champion.id, selfPos, target,
                slot, "lane-attack-champion-skill", outCommands))
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

2. 검증

미검증:
- 사거리 밖 미니언 공격 명령이 서버에서 reject되지 않고 이동 분기로 이어지는지 미검증.
- 10% `AttackChampion` 선택 시 스킬/평타 사거리 밖이면 챔피언 쪽 이동 명령이 나가는지 미검증.
- 적 포탑 내부에서 적 챔피언 출현 시 `Retreat` 명령으로 바뀌는지 런타임 미검증.

검증 명령:

```powershell
git diff --check
rg -n "BotLaneAI|CBotLaneAISystem|BotLaneAIDebug|eBotLaneAI|kBotLaneAI|HarassChampion|HarrasChampion|harassChance" Shared Server Client -S
& "C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" Winters.sln /p:Configuration=Debug /p:Platform=x64 /t:Server
& "C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" Winters.sln /p:Configuration=Debug /p:Platform=x64 /t:Client
```

확인 필요:
- F5에서 AI가 `MoveToOuterTurret -> WaitForWave -> LaneCombat`으로 진입한 뒤 사거리 밖 대상 앞에서 멈추지 않는지 확인.
- 서버 로그에서 `BasicAttackReject`가 반복 스팸으로 찍히지 않는지 확인.
- `ChampionAI` debug panel에서 `AttackChampion`, `AttackMinion`, `AttackStructure`, `Retreat` 상태 전환이 의도대로 보이는지 확인.
