Session - Champion AI 공통 Stage1을 미니언 주 공격, 10% Harass, 포탑 공격, 위험 후퇴 기준으로 검증 가능하게 고정한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/ChampionAIComponent.h

기존 코드:

```cpp
enum class eChampionAIAction : u8_t
{
	MoveToSafeAnchor,
	FollowWave,
	AttackMinion,
	AttackChampion,
	AttackStructure,
	Retreat,
};
```

아래에 추가:

```cpp
enum class eChampionAIIntent : u8_t
{
	FarmMinion,
	HarassChampion,
	SiegeStructure,
	Retreat,
};
```

기존 코드:

```cpp
inline constexpr u32_t kChampionAIDebugPresentFlag = 1u << 7;
inline constexpr u32_t kChampionAIStateShift = 8u;
inline constexpr u32_t kChampionAIStateMask = 0xFu << kChampionAIStateShift;
inline constexpr u32_t kChampionAIActionShift = 12u;
inline constexpr u32_t kChampionAIActionMask = 0xFu << kChampionAIActionShift;
```

아래로 교체:

```cpp
inline constexpr u32_t kChampionAIDebugPresentFlag = 1u << 7;
inline constexpr u32_t kChampionAIStateShift = 8u;
inline constexpr u32_t kChampionAIStateMask = 0xFu << kChampionAIStateShift;
inline constexpr u32_t kChampionAIActionShift = 12u;
inline constexpr u32_t kChampionAIActionMask = 0xFu << kChampionAIActionShift;
inline constexpr u32_t kChampionAIIntentShift = 16u;
inline constexpr u32_t kChampionAIIntentMask = 0xFu << kChampionAIIntentShift;
```

기존 코드:

```cpp
	eChampionAIState state = eChampionAIState::MoveToOuterTurret;
	eChampionAIAction lastAction = eChampionAIAction::MoveToSafeAnchor;
```

아래로 교체:

```cpp
	eChampionAIState state = eChampionAIState::MoveToOuterTurret;
	eChampionAIAction lastAction = eChampionAIAction::MoveToSafeAnchor;
	eChampionAIIntent intent = eChampionAIIntent::FarmMinion;
```

기존 코드:

```cpp
	f32_t decisionTimer = 0.f;
	f32_t decisionInterval = 0.20f;
	f32_t championScanRange = 9.f;
	f32_t minionScanRange = 12.f;
	f32_t structureScanRange = 18.f;
	f32_t waveJoinRange = 8.f;
	f32_t leashRange = 14.f;
	f32_t attackChampionChance = 0.10f;
	f32_t retreatHpRatio = 0.35f;
	f32_t reengageHpRatio = 0.55f;
	u32_t nextCommandSequence = 1;
```

아래로 교체:

```cpp
	f32_t decisionTimer = 0.f;
	f32_t decisionInterval = 0.20f;
	f32_t intentHoldTimer = 0.f;
	f32_t intentHoldDuration = 0.80f;
	f32_t championScanRange = 9.f;
	f32_t minionScanRange = 12.f;
	f32_t structureScanRange = 18.f;
	f32_t waveJoinRange = 8.f;
	f32_t leashRange = 14.f;
	f32_t attackChampionChance = 0.10f;
	f32_t lastDecisionRoll = 1.f;
	f32_t retreatHpRatio = 0.35f;
	f32_t reengageHpRatio = 0.55f;
	u32_t nextCommandSequence = 1;
```

기존 코드:

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

아래로 교체:

```cpp
struct ChampionAIDebugComponent
{
	bool_t bPresent = false;
	eChampionAIState state = eChampionAIState::MoveToOuterTurret;
	eChampionAIAction action = eChampionAIAction::MoveToSafeAnchor;
	eChampionAIIntent intent = eChampionAIIntent::FarmMinion;
	u32_t targetNetId = 0;
	f32_t moveSpeed = 0.f;
	Vec3 snapshotPos{ 0.f, 0.f, 0.f };
};
```

1-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAISystem.cpp

기존 코드:

```cpp
    void LogChampionAICommand(
        const char* reason,
        const TickContext& tc,
        EntityID self,
        const ChampionAIComponent& ai,
        eChampion champion,
        const Vec3& selfPos,
        const Vec3& commandPos,
        EntityID target,
        eCommandKind kind,
        u8_t slot)
    {
        static u32_t s_logCount = 0;
        if (s_logCount >= 512u)
            return;

        char msg[384]{};
        sprintf_s(msg,
            "[ChampionAI] tick=%llu entity=%u champ=%u team=%u lane=%u state=%u action=%u cmd=%s slot=%u reason=%s target=%u pos=(%.2f,%.2f,%.2f) cmdPos=(%.2f,%.2f,%.2f)\n",
            static_cast<unsigned long long>(tc.tickIndex),
            static_cast<u32_t>(self),
            static_cast<u32_t>(champion),
            static_cast<u32_t>(ai.team),
            static_cast<u32_t>(ai.lane),
            static_cast<u32_t>(ai.state),
            static_cast<u32_t>(ai.lastAction),
            CommandName(kind),
            static_cast<u32_t>(slot),
            reason ? reason : "-",
            static_cast<u32_t>(target),
            selfPos.x,
            selfPos.y,
            selfPos.z,
            commandPos.x,
            commandPos.y,
            commandPos.z);
        WintersOutputAIDebugStringA(msg);
        ++s_logCount;
    }
```

아래로 교체:

```cpp
    void LogChampionAICommand(
        const char* reason,
        const TickContext& tc,
        EntityID self,
        const ChampionAIComponent& ai,
        eChampion champion,
        const Vec3& selfPos,
        const Vec3& commandPos,
        EntityID target,
        eCommandKind kind,
        u8_t slot)
    {
        static u32_t s_logCount = 0;
        if (s_logCount >= 512u)
            return;

        char msg[448]{};
        sprintf_s(msg,
            "[ChampionAI] tick=%llu entity=%u champ=%u team=%u lane=%u state=%u intent=%u action=%u roll=%.3f cmd=%s slot=%u reason=%s target=%u pos=(%.2f,%.2f,%.2f) cmdPos=(%.2f,%.2f,%.2f)\n",
            static_cast<unsigned long long>(tc.tickIndex),
            static_cast<u32_t>(self),
            static_cast<u32_t>(champion),
            static_cast<u32_t>(ai.team),
            static_cast<u32_t>(ai.lane),
            static_cast<u32_t>(ai.state),
            static_cast<u32_t>(ai.intent),
            static_cast<u32_t>(ai.lastAction),
            static_cast<double>(ai.lastDecisionRoll),
            CommandName(kind),
            static_cast<u32_t>(slot),
            reason ? reason : "-",
            static_cast<u32_t>(target),
            selfPos.x,
            selfPos.y,
            selfPos.z,
            commandPos.x,
            commandPos.y,
            commandPos.z);
        WintersOutputAIDebugStringA(msg);
        ++s_logCount;
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

    bool_t ShouldAttackChampion(
        const TickContext& tc,
        EntityID self,
        const ChampionAIComponent& ai,
        const ChampionAIContext& ctx)
    {
        if (ctx.enemyChampion == NULL_ENTITY)
            return false;
        if (ctx.selfHpRatio <= ai.retreatHpRatio + 0.10f)
            return false;
        if (ctx.enemyHpRatio <= 0.30f && ctx.selfHpRatio >= 0.50f)
            return true;
        return RollAttackChampion(tc, self, ai);
    }
```

아래로 교체:

```cpp
    u32_t MakeChampionAIRoll(const TickContext& tc, EntityID self, const ChampionAIComponent& ai)
    {
        u32_t x = static_cast<u32_t>(tc.tickIndex) ^
            static_cast<u32_t>(tc.tickIndex >> 32) ^
            (static_cast<u32_t>(self) * 747796405u) ^
            (ai.nextCommandSequence * 2891336453u);
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        return x & 0xFFFFu;
    }

    bool_t CanHarassChampion(const ChampionAIComponent& ai, const ChampionAIContext& ctx)
    {
        if (ctx.enemyChampion == NULL_ENTITY)
            return false;
        if (ctx.selfHpRatio <= ai.retreatHpRatio + 0.10f)
            return false;
        if (ctx.bInsideEnemyTurretDanger)
            return false;
        if (ctx.turretDanger > 0.85f && !ctx.bStructureWaveTanking)
            return false;
        return true;
    }

    void SampleLaneCombatIntent(
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        const ChampionAIContext& ctx)
    {
        ai.intentHoldTimer = std::max(0.f, ai.intentHoldTimer - tc.fDt);
        if (ai.intentHoldTimer > 0.f)
        {
            if (ai.intent != eChampionAIIntent::HarassChampion ||
                CanHarassChampion(ai, ctx))
            {
                return;
            }
        }

        ai.intentHoldTimer = ai.intentHoldDuration;

        if (!CanHarassChampion(ai, ctx))
        {
            ai.intent = eChampionAIIntent::FarmMinion;
            ai.lastDecisionRoll = 1.f;
            return;
        }

        const u32_t roll = MakeChampionAIRoll(tc, self, ai);
        ai.lastDecisionRoll = static_cast<f32_t>(roll) / 65535.f;
        ai.intent = (ai.lastDecisionRoll < Clamp01(ai.attackChampionChance))
            ? eChampionAIIntent::HarassChampion
            : eChampionAIIntent::FarmMinion;
    }

    bool_t ShouldAttackChampion(
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        const ChampionAIContext& ctx)
    {
        SampleLaneCombatIntent(tc, self, ai, ctx);
        return ai.intent == eChampionAIIntent::HarassChampion &&
            CanHarassChampion(ai, ctx);
    }
```

기존 코드:

```cpp
    bool_t EmitRetreat(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        std::vector<GameCommand>& outCommands)
    {
        ai.state = eChampionAIState::Retreat;
        ai.lockedChampion = NULL_ENTITY;
        ai.targetMinion = NULL_ENTITY;
        ai.targetStructure = NULL_ENTITY;
        return EmitMoveCommand(world, tc, self, ai, champion.id, selfPos,
            ai.retreatGoal, eChampionAIAction::Retreat, "lane-retreat", outCommands);
    }
```

아래로 교체:

```cpp
    bool_t EmitRetreat(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        std::vector<GameCommand>& outCommands)
    {
        ai.state = eChampionAIState::Retreat;
        ai.intent = eChampionAIIntent::Retreat;
        ai.lockedChampion = NULL_ENTITY;
        ai.targetMinion = NULL_ENTITY;
        ai.targetStructure = NULL_ENTITY;
        return EmitMoveCommand(world, tc, self, ai, champion.id, selfPos,
            ai.retreatGoal, eChampionAIAction::Retreat, "lane-retreat", outCommands);
    }
```

`ExecuteLaneCombat` 안에서 아래 기존 코드:

```cpp
        if (ctx.enemyChampion == NULL_ENTITY &&
            ctx.enemyStructure != NULL_ENTITY &&
            ctx.bStructureWaveTanking)
        {
            if (EmitBasicAttackCommand(world, tc, self, ai, champion.id, selfPos,
                ctx.enemyStructure, eChampionAIAction::AttackStructure,
                "lane-attack-structure-ba", outCommands))
            {
                return;
            }
```

아래로 교체:

```cpp
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
```

`ExecuteLaneCombat` 안에서 아래 기존 코드:

```cpp
        if (ShouldAttackChampion(tc, self, ai, ctx) &&
            TryEmitAttackChampion(world, tc, self, ai, champion, selfPos, ctx, outCommands))
        {
            return;
        }
```

아래로 교체:

```cpp
        if (ShouldAttackChampion(tc, self, ai, ctx) &&
            TryEmitAttackChampion(world, tc, self, ai, champion, selfPos, ctx, outCommands))
        {
            return;
        }

        ai.intent = eChampionAIIntent::FarmMinion;
```

1-3. C:/Users/user/Desktop/Winters/Server/Private/Game/SnapshotBuilder.cpp

기존 코드:

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

아래로 교체:

```cpp
        if (world.HasComponent<ChampionAIComponent>(entity))
        {
            const auto& ai = world.GetComponent<ChampionAIComponent>(entity);
            stateFlags |= kChampionAIDebugPresentFlag;
            stateFlags |= (static_cast<u32_t>(ai.state) << kChampionAIStateShift) & kChampionAIStateMask;
            stateFlags |= (static_cast<u32_t>(ai.lastAction) << kChampionAIActionShift) & kChampionAIActionMask;
            stateFlags |= (static_cast<u32_t>(ai.intent) << kChampionAIIntentShift) & kChampionAIIntentMask;

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

1-4. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp

기존 코드:

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

아래로 교체:

```cpp
                auto& debug = world.GetComponent<ChampionAIDebugComponent>(e);
                debug.bPresent = true;
                debug.state = static_cast<eChampionAIState>(
                    (es->stateFlags() & kChampionAIStateMask) >> kChampionAIStateShift);
                debug.action = static_cast<eChampionAIAction>(
                    (es->stateFlags() & kChampionAIActionMask) >> kChampionAIActionShift);
                debug.intent = static_cast<eChampionAIIntent>(
                    (es->stateFlags() & kChampionAIIntentMask) >> kChampionAIIntentShift);
                debug.targetNetId = es->ownerNet();
                debug.moveSpeed = es->moveSpeed();
                debug.snapshotPos = Vec3{ es->posX(), es->posY(), es->posZ() };
```

1-5. C:/Users/user/Desktop/Winters/Client/Private/UI/AIDebugPanel.cpp

기존 코드:

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

아래에 추가:

```cpp
	const char* ChampionAIIntentName(eChampionAIIntent intent)
	{
		switch (intent)
		{
		case eChampionAIIntent::FarmMinion: return "FarmMinion";
		case eChampionAIIntent::HarassChampion: return "HarassChampion";
		case eChampionAIIntent::SiegeStructure: return "SiegeStructure";
		case eChampionAIIntent::Retreat: return "Retreat";
		default: return "Unknown";
		}
	}
```

기존 코드:

```cpp
			ImGui::Text("State: %s", ChampionAIStateName(debug.state));
			ImGui::Text("Action: %s", ChampionAIActionName(debug.action));
			ImGui::Text("Target Net: %u", debug.targetNetId);
```

아래로 교체:

```cpp
			ImGui::Text("State: %s", ChampionAIStateName(debug.state));
			ImGui::Text("Intent: %s", ChampionAIIntentName(debug.intent));
			ImGui::Text("Action: %s", ChampionAIActionName(debug.action));
			ImGui::Text("Target Net: %u", debug.targetNetId);
```

2. 검증

미검증:
- `LaneCombat`에서 `intentHoldDuration` 동안 Farm/Harass intent가 흔들리지 않는지 미검증.
- `HarassChampion` intent가 적 챔피언 없음, 낮은 HP, 포탑 위험 상태에서 Farm으로 떨어지는지 미검증.
- 적 챔피언이 없고 웨이브가 포탑을 받아줄 때 `SiegeStructure` intent와 `AttackStructure` action이 보이는지 미검증.
- 위험 상태에서 `Retreat` intent/action이 보이고 안전 지점으로 이동하는지 미검증.

검증 명령:

```powershell
git diff --check
rg -n "ResolveAttackChampionSlot|HarrasChampion|HarassChampion|BotLaneAI|CBotLaneAISystem|eBotLaneAI|kBotLaneAI" Shared Server Client -S
& "C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" "Server/Include/Server.vcxproj" /p:Configuration=Debug /p:Platform=x64
& "C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" "Client/Include/Client.vcxproj" /p:Configuration=Debug /p:Platform=x64
```

확인 필요:
- F5에서 F9 `AI Debug`를 열고 `State`, `Intent`, `Action`, `Target Net`을 함께 확인.
- 서버 로그에서 `intent`, `roll`, `reason`을 보고 `HarassChampion` 비율이 decision tick 기준으로 낮게 찍히는지 확인.
- 적 미니언이 있을 때 기본적으로 `FarmMinion / AttackMinion` 또는 `FarmMinion / FollowWave`가 유지되는지 확인.
- 적 챔피언 접근 시 낮은 빈도로 `HarassChampion / AttackChampion`이 보이는지 확인.
- 아군 미니언 없이 적 포탑 내부로 깊게 들어가지 않는지 확인.
