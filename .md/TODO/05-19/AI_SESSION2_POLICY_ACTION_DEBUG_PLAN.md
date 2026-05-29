Session - Champion AI Session 2에서 AttackChampion 스킬 선택을 정책화하고 F9 패널에 State/Action/Target 기준선을 연결한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/ChampionAIComponent.h

기존 코드:

```cpp
inline constexpr u32_t kChampionAIDebugPresentFlag = 1u << 7;
inline constexpr u32_t kChampionAIStateShift = 8u;
inline constexpr u32_t kChampionAIStateMask = 0xFu << kChampionAIStateShift;
```

아래로 교체:

```cpp
inline constexpr u32_t kChampionAIDebugPresentFlag = 1u << 7;
inline constexpr u32_t kChampionAIStateShift = 8u;
inline constexpr u32_t kChampionAIStateMask = 0xFu << kChampionAIStateShift;
inline constexpr u32_t kChampionAIActionShift = 12u;
inline constexpr u32_t kChampionAIActionMask = 0xFu << kChampionAIActionShift;
```

기존 코드:

```cpp
struct ChampionAIDebugComponent
{
	bool_t bPresent = false;
	eChampionAIState state = eChampionAIState::MoveToOuterTurret;
	u32_t targetNetId = 0;
	f32_t moveSpeed = 0.f;
	Vec3 snapshotPos{ 0.f, 0.f, 0.f };
};
```

아래로 교체:

```cpp
struct ChampionAIDebugComponent
{
	bool_t bPresent = false;
	eChampionAIState state = eChampionAIState::MoveToOuterTurret;
	eChampionAIAction action = eChampionAIAction::MoveToSafeAnchor;
	u32_t targetNetId = 0;
	f32_t moveSpeed = 0.f;
	Vec3 snapshotPos{ 0.f, 0.f, 0.f };
};
```

1-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAIPolicy.cpp

`MakeAsheProfile` 안에서 아래 기존 코드:

```cpp
            {
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::R), 0.f, 1.f },
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::W), 0.f, 1.f },
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::Q), 0.f, 1.f },
            },
            3
```

아래로 교체:

```cpp
            {
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::W), 0.f, 1.f },
            },
            1
```

`MakeFioraProfile` 안에서 아래 기존 코드:

```cpp
            {
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::W), 0.f, 1.f },
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::R), 0.f, 1.f },
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::Q), 0.f, 1.f },
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::E), 0.f, 1.f },
            },
            4
```

아래로 교체:

```cpp
            {
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::Q), 0.f, 1.f },
            },
            1
```

`MakeJaxProfile` 안에서 아래 기존 코드:

```cpp
            {
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::Q), 0.f, 1.f },
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::W), 0.f, 1.f },
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::E), 0.f, 1.f },
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::R), 0.f, 1.f },
            },
            4
```

아래로 교체:

```cpp
            {
                ChampionAISkillRule{ static_cast<u8_t>(eSkillSlot::Q), 0.f, 1.f },
            },
            1
```

1-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAISystem.cpp

기존 코드:

```cpp
    u8_t ResolveAttackChampionSlot(eChampion champion)
    {
        switch (champion)
        {
        case eChampion::JAX:
        case eChampion::FIORA:
        case eChampion::MASTERYI:
            return static_cast<u8_t>(eSkillSlot::Q);
        case eChampion::ASHE:
            return static_cast<u8_t>(eSkillSlot::W);
        default:
            return static_cast<u8_t>(eSkillSlot::BasicAttack);
        }
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
                rule.slot, "lane-attack-champion-skill", outCommands))
            {
                return true;
            }
        }

        return false;
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

1-4. C:/Users/user/Desktop/Winters/Server/Private/Game/SnapshotBuilder.cpp

기존 코드:

```cpp
        if (world.HasComponent<ChampionAIComponent>(entity))
        {
            const auto& ai = world.GetComponent<ChampionAIComponent>(entity);
            stateFlags |= kChampionAIDebugPresentFlag;
            stateFlags |= (static_cast<u32_t>(ai.state) << kChampionAIStateShift) & kChampionAIStateMask;

            EntityID target = ai.lockedChampion;
            if (target == NULL_ENTITY)
                target = ai.targetMinion;
            if (target == NULL_ENTITY)
                target = ai.targetStructure;
            if (target == NULL_ENTITY)
                target = ai.alliedWave;

            if (target != NULL_ENTITY)
                ownerNet = entityMap.ToNet(target);
        }
```

아래로 교체:

```cpp
        if (world.HasComponent<ChampionAIComponent>(entity))
        {
            const auto& ai = world.GetComponent<ChampionAIComponent>(entity);
            stateFlags |= kChampionAIDebugPresentFlag;
            stateFlags |= (static_cast<u32_t>(ai.state) << kChampionAIStateShift) & kChampionAIStateMask;
            stateFlags |= (static_cast<u32_t>(ai.lastAction) << kChampionAIActionShift) & kChampionAIActionMask;

            EntityID target = ai.lockedChampion;
            if (target == NULL_ENTITY)
                target = ai.targetMinion;
            if (target == NULL_ENTITY)
                target = ai.targetStructure;
            if (target == NULL_ENTITY)
                target = ai.alliedWave;

            if (target != NULL_ENTITY)
                ownerNet = entityMap.ToNet(target);
        }
```

1-5. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp

기존 코드:

```cpp
                auto& debug = world.GetComponent<ChampionAIDebugComponent>(e);
                debug.bPresent = true;
                debug.state = static_cast<eChampionAIState>(
                    (es->stateFlags() & kChampionAIStateMask) >> kChampionAIStateShift);
                debug.targetNetId = es->ownerNet();
                debug.moveSpeed = es->moveSpeed();
                debug.snapshotPos = Vec3{ es->posX(), es->posY(), es->posZ() };
```

아래로 교체:

```cpp
                auto& debug = world.GetComponent<ChampionAIDebugComponent>(e);
                debug.bPresent = true;
                debug.state = static_cast<eChampionAIState>(
                    (es->stateFlags() & kChampionAIStateMask) >> kChampionAIStateShift);
                debug.action = static_cast<eChampionAIAction>(
                    (es->stateFlags() & kChampionAIActionMask) >> kChampionAIActionShift);
                debug.targetNetId = es->ownerNet();
                debug.moveSpeed = es->moveSpeed();
                debug.snapshotPos = Vec3{ es->posX(), es->posY(), es->posZ() };
```

1-6. C:/Users/user/Desktop/Winters/Client/Private/UI/AIDebugPanel.cpp

기존 코드:

```cpp
	const char* ChampionAIStateName(eChampionAIState state)
	{
		switch (state)
		{
		case eChampionAIState::MoveToOuterTurret: return "MoveToOuterTurret";
		case eChampionAIState::WaitForWave: return "WaitForWave";
		case eChampionAIState::LaneCombat: return "LaneCombat";
		case eChampionAIState::Retreat: return "Retreat";
		case eChampionAIState::Dead: return "Dead";
		default: return "Unknown";
		}
	}
```

아래에 추가:

```cpp
	const char* ChampionAIActionName(eChampionAIAction action)
	{
		switch (action)
		{
		case eChampionAIAction::MoveToSafeAnchor: return "MoveToSafeAnchor";
		case eChampionAIAction::FollowWave: return "FollowWave";
		case eChampionAIAction::AttackMinion: return "AttackMinion";
		case eChampionAIAction::AttackChampion: return "AttackChampion";
		case eChampionAIAction::AttackStructure: return "AttackStructure";
		case eChampionAIAction::Retreat: return "Retreat";
		default: return "Unknown";
		}
	}
```

기존 코드:

```cpp
			ImGui::Text("State: %s", ChampionAIStateName(debug.state));
			ImGui::Text("Target Net: %u", debug.targetNetId);
```

아래로 교체:

```cpp
			ImGui::Text("State: %s", ChampionAIStateName(debug.state));
			ImGui::Text("Action: %s", ChampionAIActionName(debug.action));
			ImGui::Text("Target Net: %u", debug.targetNetId);
```

2. 검증

미검증:
- `AttackChampion` 선택 시 Jax/Fiora/MasterYi는 Q, Ashe는 W를 먼저 시도하는지 런타임 미검증.
- 스킬 쿨타임 또는 사거리 실패 후 평타, 이동 명령으로 자연스럽게 내려가는지 미검증.
- F9 `AI Debug` 패널에서 서버 스냅샷 기준 `State`, `Action`, `Target Net`이 함께 갱신되는지 미검증.

검증 명령:

```powershell
git diff --check
rg -n "ResolveAttackChampionSlot|HarrasChampion|HarassChampion|BotLaneAI|CBotLaneAISystem|eBotLaneAI|kBotLaneAI" Shared Server Client -S
& "C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" Winters.sln /p:Configuration=Debug /p:Platform=x64 /t:Server
& "C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" Winters.sln /p:Configuration=Debug /p:Platform=x64 /t:Client
```

확인 필요:
- F5 서버/클라 실행 후 봇이 `MoveToOuterTurret -> WaitForWave -> LaneCombat` 순서로 들어가는지 확인.
- 미니언 교전 중 적 챔피언이 근처에 있을 때 F9 패널의 `Action`이 `AttackMinion` 위주로 유지되다가 확률적으로 `AttackChampion`으로 바뀌는지 확인.
- 적 챔피언이 없는 상태에서 아군 미니언이 적 포탑 내부에 있을 때 `AttackStructure`가 보이는지 확인.
- 적 포탑 내부에서 적 챔피언이 접근하면 `Retreat`로 바뀌는지 확인.
