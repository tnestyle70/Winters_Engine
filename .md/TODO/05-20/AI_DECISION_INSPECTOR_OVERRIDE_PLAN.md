Session - AI 패널에 서버 AI 판단 후보를 노출하고 선택한 봇의 다음 행동을 서버 권위 one-shot override로 강제한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/ChampionAIComponent.h

기존 코드:

```cpp
enum class eChampionAIIntent : u8_t
{
	FarmMinion,
	HarassChampion,
	SiegeStructure,
	Retreat,
};
```

아래에 추가:

```cpp
inline constexpr u32_t kChampionAIActionBitMoveToSafeAnchor = 1u << 0;
inline constexpr u32_t kChampionAIActionBitFollowWave = 1u << 1;
inline constexpr u32_t kChampionAIActionBitAttackMinion = 1u << 2;
inline constexpr u32_t kChampionAIActionBitAttackChampion = 1u << 3;
inline constexpr u32_t kChampionAIActionBitAttackStructure = 1u << 4;
inline constexpr u32_t kChampionAIActionBitRetreat = 1u << 5;

inline constexpr u32_t kChampionAIAvailableActionShift = 20u;
inline constexpr u32_t kChampionAIAvailableActionMask = 0x3Fu << kChampionAIAvailableActionShift;
inline constexpr u32_t kChampionAIAvailableSkillShift = 26u;
inline constexpr u32_t kChampionAIAvailableSkillMask = 0xFu << kChampionAIAvailableSkillShift;
inline constexpr u32_t kChampionAIDebugOverrideFlag = 1u << 30;
inline constexpr u16_t kChampionAIDebugClearOverrideItemId = 0xFFFFu;
```

기존 코드:

```cpp
	bool_t bWaveJoined = false;
	bool_t bStructureWaveTanking = false;
	bool_t bInsideEnemyTurretDanger = false;
};
```

아래로 교체:

```cpp
	bool_t bWaveJoined = false;
	bool_t bStructureWaveTanking = false;
	bool_t bInsideEnemyTurretDanger = false;

	u32_t debugAvailableActionMask = 0;
	u32_t debugAvailableSkillMask = 0;
	eChampionAIAction debugForcedAction = eChampionAIAction::FollowWave;
	u8_t debugForcedSkillSlot = 0;
	u8_t debugForcedDecisionCount = 0;
};
```

기존 코드:

```cpp
	u32_t targetNetId = 0;
	f32_t moveSpeed = 0.f;
	Vec3 snapshotPos{ 0.f, 0.f, 0.f };
};
```

아래로 교체:

```cpp
	u32_t netId = 0;
	u32_t targetNetId = 0;
	u32_t availableActionMask = 0;
	u32_t availableSkillMask = 0;
	bool_t bOverridePending = false;
	f32_t moveSpeed = 0.f;
	Vec3 snapshotPos{ 0.f, 0.f, 0.f };
};
```

1-2. C:/Users/user/Desktop/Winters/Shared/Schemas/Command.fbs

기존 코드:

```cpp
    Recall = 7,
    RecallCancel = 8,
    Ping = 9
```

아래로 교체:

```cpp
    Recall = 7,
    RecallCancel = 8,
    AIDebugControl = 9
```

1-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ICommandExecutor.h

기존 코드:

```cpp
    Recall = 7,
    RecallCancel = 8,
};
```

아래로 교체:

```cpp
    Recall = 7,
    RecallCancel = 8,
    AIDebugControl = 9,
};
```

기존 코드:

```cpp
    void HandleRecall(CWorld&, const TickContext&, const GameCommand&);
    void HandleRecallCancel(CWorld&, const TickContext&, const GameCommand&);
```

아래에 추가:

```cpp
    void HandleAIDebugControl(CWorld&, const TickContext&, const GameCommand&);
```

1-4. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Components/ChampionComponent.h"
```

아래에 추가:

```cpp
#include "Shared/GameSim/Components/ChampionAIComponent.h"
```

기존 코드:

```cpp
    case eCommandKind::RecallCancel:
        HandleRecallCancel(world, tc, cmd);
        break;
```

아래에 추가:

```cpp
    case eCommandKind::AIDebugControl:
        HandleAIDebugControl(world, tc, cmd);
        break;
```

`CDefaultCommandExecutor::HandleRecallCancel` 바로 아래에 추가:

```cpp
void CDefaultCommandExecutor::HandleAIDebugControl(CWorld& world,
    const TickContext& tc, const GameCommand& cmd)
{
    (void)tc;

#if defined(_DEBUG)
    if (cmd.targetEntity == NULL_ENTITY ||
        !world.HasComponent<ChampionAIComponent>(cmd.targetEntity))
    {
        return;
    }

    auto& ai = world.GetComponent<ChampionAIComponent>(cmd.targetEntity);
    if (cmd.itemId == kChampionAIDebugClearOverrideItemId)
    {
        ai.debugForcedDecisionCount = 0;
        ai.debugForcedSkillSlot = 0;
        return;
    }

    const auto action = static_cast<eChampionAIAction>(cmd.itemId);
    if (static_cast<u8_t>(action) > static_cast<u8_t>(eChampionAIAction::Retreat))
        return;

    ai.debugForcedAction = action;
    ai.debugForcedSkillSlot = cmd.slot;
    ai.debugForcedDecisionCount = 1;
    ai.decisionTimer = 0.f;
#else
    (void)world;
    (void)cmd;
#endif
}
```

1-5. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAISystem.cpp

`CanHarassChampion` 바로 아래에 추가:

```cpp
u32_t BuildChampionAIAvailableActionMask(const ChampionAIComponent& ai, const ChampionAIContext& ctx)
```

`BuildChampionAIAvailableActionMask` 아래에 추가:

```cpp
u32_t BuildChampionAIAvailableSkillMask(
    CWorld& world,
    EntityID self,
    eChampion champion,
    const ChampionAIProfile& profile,
    const ChampionAIContext& ctx)
```

`ExecuteWaitForWave` 바로 아래에 추가:

```cpp
bool_t TryConsumeChampionAIDebugOverride(
    CWorld& world,
    const TickContext& tc,
    EntityID self,
    ChampionAIComponent& ai,
    ChampionComponent& champion,
    const Vec3& selfPos,
    const ChampionAIContext& ctx,
    std::vector<GameCommand>& outCommands)
```

기존 코드:

```cpp
            const ChampionAIContext ctx =
                BuildChampionAIContext(world, self, ai, champion, selfPos);
```

아래에 추가:

```cpp
            ai.debugAvailableActionMask = BuildChampionAIAvailableActionMask(ai, ctx);
            ai.debugAvailableSkillMask =
                BuildChampionAIAvailableSkillMask(world, self, champion.id, profile, ctx);

            if (ai.debugForcedDecisionCount > 0)
            {
                --ai.debugForcedDecisionCount;
                if (TryConsumeChampionAIDebugOverride(
                    world, tc, self, ai, champion, selfPos, ctx, outCommands))
                {
                    return;
                }
            }
```

1-6. C:/Users/user/Desktop/Winters/Server/Private/Game/SnapshotBuilder.cpp

기존 코드:

```cpp
            stateFlags |= (static_cast<u32_t>(ai.intent) << kChampionAIIntentShift) & kChampionAIIntentMask;
```

아래에 추가:

```cpp
            stateFlags |= (ai.debugAvailableActionMask << kChampionAIAvailableActionShift) & kChampionAIAvailableActionMask;
            stateFlags |= (ai.debugAvailableSkillMask << kChampionAIAvailableSkillShift) & kChampionAIAvailableSkillMask;
            if (ai.debugForcedDecisionCount > 0)
                stateFlags |= kChampionAIDebugOverrideFlag;
```

1-7. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp

기존 코드:

```cpp
                debug.bPresent = true;
```

아래에 추가:

```cpp
                debug.netId = es->netId();
```

기존 코드:

```cpp
                debug.targetNetId = es->ownerNet();
                debug.moveSpeed = es->moveSpeed();
```

아래로 교체:

```cpp
                debug.targetNetId = es->ownerNet();
                debug.availableActionMask =
                    (es->stateFlags() & kChampionAIAvailableActionMask) >> kChampionAIAvailableActionShift;
                debug.availableSkillMask =
                    (es->stateFlags() & kChampionAIAvailableSkillMask) >> kChampionAIAvailableSkillShift;
                debug.bOverridePending = (es->stateFlags() & kChampionAIDebugOverrideFlag) != 0u;
                debug.moveSpeed = es->moveSpeed();
```

1-8. C:/Users/user/Desktop/Winters/Client/Public/Network/Client/CommandSerializer.h

기존 코드:

```cpp
struct GameCommandWire;
```

아래에 추가:

```cpp
enum class eChampionAIAction : u8_t;
```

기존 코드:

```cpp
	void SendRecall(CClientNetwork& net);
	void SendLevelSkill(CClientNetwork& net, u8_t slot);
```

아래에 추가:

```cpp
	void SendAIDebugControl(CClientNetwork& net, NetEntityId targetNet,
		eChampionAIAction action, u8_t skillSlot = 0);
	void SendAIDebugClear(CClientNetwork& net, NetEntityId targetNet);
```

1-9. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/CommandSerializer.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Systems/ICommandExecutor.h"
```

아래에 추가:

```cpp
#include "Shared/GameSim/Components/ChampionAIComponent.h"
```

`SendRecall` 바로 아래에 `SendAIDebugControl`과 `SendAIDebugClear`를 추가한다.

1-10. C:/Users/user/Desktop/Winters/Client/Public/UI/AIDebugPanel.h

기존 코드:

```cpp
class CWorld;
```

아래에 추가:

```cpp
class CScene_InGame;
```

기존 코드:

```cpp
		static void Render(CWorld& world);
```

아래로 교체:

```cpp
		static void Render(CWorld& world, CScene_InGame* pScene);
```

1-11. C:/Users/user/Desktop/Winters/Client/Private/UI/AIDebugPanel.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Components/NetAnimationComponent.h"
```

아래에 추가:

```cpp
#include "Network/Client/ClientNetwork.h"
#include "Network/Client/CommandSerializer.h"
#include "Scene/Scene_InGame.h"
#include "Shared/GameSim/Systems/ChampionAIPolicy.h"
```

기존 코드:

```cpp
void UI::CAIDebugPanel::Render(CWorld& world)
```

아래로 교체:

```cpp
void UI::CAIDebugPanel::Render(CWorld& world, CScene_InGame* pScene)
```

`Champion AI` 출력 범위를 선택 가능한 table, profile/range readout, action button, skill button, clear button으로 교체한다.

1-12. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

기존 코드:

```cpp
    if (m_bShowAIDebug)
        UI::CAIDebugPanel::Render(m_World);
```

아래로 교체:

```cpp
    if (m_bShowAIDebug)
        UI::CAIDebugPanel::Render(m_World, this);
```

2. 검증

검증 명령:
- `Shared/Schemas/run_codegen.bat`
- `git diff --check`
- `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`
- `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`

수동 확인:
- F9 AI 패널에서 AI 챔피언 row 선택 가능.
- 선택한 AI의 현재 state, intent, action, 가능 행동 mask, 가능 스킬 mask, profile range가 보임.
- 버튼 클릭은 클라 월드 직접 변경 없이 `AIDebugControl` command로 서버에 전달됨.
- 서버 AI tick에서 one-shot override가 기존 AI command emission 경로로 Move, BasicAttack, CastSkill을 발행함.
- override 후 다음 snapshot에서 Action, Intent, Target 값이 바뀌는지 확인.

확인 필요:
- `Command.fbs` 변경 후 generated `Command_generated.h`가 갱신되는지 확인.
- `_DEBUG` 서버에서만 `AIDebugControl`을 소비하고 Release에서는 무시되는지 확인.
