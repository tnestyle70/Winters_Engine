Session - Jax combo 런타임 검증을 위해 AI Debug에 강제 AttackChampion 버튼을 추가하고 champion attack 확률을 30%로 올린다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/ChampionAIComponent.h

기존 코드:

```cpp
inline constexpr u32_t kChampionAIDebugOverrideFlag = 1u << 30;
inline constexpr u16_t kChampionAIDebugClearOverrideItemId = 0xFFFFu;
```

아래에 추가:

```cpp
inline constexpr u8_t kChampionAIDebugForceActionSkillSlot = 0xFFu;
```

기존 코드:

```cpp
	f32_t attackChampionChance = 0.10f;
```

아래로 교체:

```cpp
	f32_t attackChampionChance = 0.30f;
```

기존 코드:

```cpp
	eChampionAIAction debugForcedAction = eChampionAIAction::FollowWave;
	u8_t debugForcedSkillSlot = 0;
	u8_t debugForcedDecisionCount = 0;
};
```

아래로 교체:

```cpp
	eChampionAIAction debugForcedAction = eChampionAIAction::FollowWave;
	u8_t debugForcedSkillSlot = 0;
	u8_t debugForcedDecisionCount = 0;
	bool_t bDebugForceAction = false;
};
```

1-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor.cpp

`CDefaultCommandExecutor::HandleAIDebugControl` 안에서 아래 코드를 교체한다.

기존 코드:

```cpp
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
```

아래로 교체:

```cpp
    auto& ai = world.GetComponent<ChampionAIComponent>(cmd.targetEntity);
    if (cmd.itemId == kChampionAIDebugClearOverrideItemId)
    {
        ai.debugForcedDecisionCount = 0;
        ai.debugForcedSkillSlot = 0;
        ai.bDebugForceAction = false;
        return;
    }

    const auto action = static_cast<eChampionAIAction>(cmd.itemId);
    if (static_cast<u8_t>(action) > static_cast<u8_t>(eChampionAIAction::Retreat))
        return;

    const bool_t bForceAction = cmd.slot == kChampionAIDebugForceActionSkillSlot;
    ai.debugForcedAction = action;
    ai.debugForcedSkillSlot = bForceAction ? 0u : cmd.slot;
    ai.bDebugForceAction = bForceAction;
    ai.debugForcedDecisionCount = 1;
    ai.decisionTimer = 0.f;
```

1-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAISystem.cpp

`TryConsumeChampionAIDebugOverride` 안에서 아래 코드를 교체한다.

기존 코드:

```cpp
        const eChampionAIAction action = ai.debugForcedAction;
        const u8_t skillSlot = ai.debugForcedSkillSlot;
        ai.debugForcedSkillSlot = 0u;

        if (!IsActionAvailable(ai.debugAvailableActionMask, action))
            return false;
```

아래로 교체:

```cpp
        const eChampionAIAction action = ai.debugForcedAction;
        const u8_t skillSlot = ai.debugForcedSkillSlot;
        const bool_t bForceAction = ai.bDebugForceAction;
        ai.debugForcedSkillSlot = 0u;
        ai.bDebugForceAction = false;

        if (!bForceAction && !IsActionAvailable(ai.debugAvailableActionMask, action))
            return false;
```

1-4. C:/Users/user/Desktop/Winters/Client/Private/UI/AIDebugPanel.cpp

기존 코드:

```cpp
	void RenderActionButton(
		CScene_InGame* pScene,
		u32_t targetNetId,
		const char* pLabel,
		eChampionAIAction action,
		bool_t bAvailable,
		u8_t skillSlot = 0u)
	{
		const bool_t bCanSend =
			bAvailable &&
			targetNetId != NULL_NET_ENTITY &&
			CanSendAIDebugCommand(pScene);
		ImGui::BeginDisabled(!bCanSend);
		if (ImGui::Button(pLabel))
		{
			pScene->GetCommandSerializer()->SendAIDebugControl(
				*pScene->GetNetworkView(),
				targetNetId,
				action,
				skillSlot);
		}
		ImGui::EndDisabled();
	}
```

아래에 추가:

```cpp
	void RenderForceActionButton(
		CScene_InGame* pScene,
		u32_t targetNetId,
		const char* pLabel,
		eChampionAIAction action)
	{
		const bool_t bCanSend =
			targetNetId != NULL_NET_ENTITY &&
			CanSendAIDebugCommand(pScene);
		ImGui::BeginDisabled(!bCanSend);
		if (ImGui::Button(pLabel))
		{
			pScene->GetCommandSerializer()->SendAIDebugControl(
				*pScene->GetNetworkView(),
				targetNetId,
				action,
				kChampionAIDebugForceActionSkillSlot);
		}
		ImGui::EndDisabled();
	}
```

기존 코드:

```cpp
		RenderActionButton(pScene, debug.netId, "Champion", eChampionAIAction::AttackChampion,
			HasChampionAIAction(debug, eChampionAIAction::AttackChampion));
		ImGui::SameLine();
		RenderActionButton(pScene, debug.netId, "Structure", eChampionAIAction::AttackStructure,
			HasChampionAIAction(debug, eChampionAIAction::AttackStructure));
```

아래로 교체:

```cpp
		RenderActionButton(pScene, debug.netId, "Champion", eChampionAIAction::AttackChampion,
			HasChampionAIAction(debug, eChampionAIAction::AttackChampion));
		ImGui::SameLine();
		RenderForceActionButton(pScene, debug.netId, "Force Champion",
			eChampionAIAction::AttackChampion);
		ImGui::SameLine();
		RenderActionButton(pScene, debug.netId, "Structure", eChampionAIAction::AttackStructure,
			HasChampionAIAction(debug, eChampionAIAction::AttackStructure));
```

2. 검증

미검증:
- 코드 미반영.
- ImGui `Force Champion` 버튼으로 Jax가 AttackChampion override를 받는지 미검증.
- `attackChampionChance = 0.30f`에서 자연 roll로 Jax combo가 더 자주 시작되는지 미검증.

검증 명령:
- `git diff --check`
- `msbuild Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64`
- `msbuild Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64`

수동 확인:
- AI Debug 패널에서 Jax를 선택했을 때 `Force Champion` 버튼이 항상 보이는지 확인.
- 적 챔피언이 scan range 안에 있을 때 `Force Champion`을 누르면 `reason=combo-attack-champion-*` 로그가 시작되는지 확인.
- Jax combo가 진행 중일 때 FarmMinion으로 새지 않는지 확인.
- Jax combo 완료 후 `lane-attack-minion-*` 또는 `lane-follow-wave`로 복귀하는지 확인.
- 적 챔피언이 없을 때 `Force Champion`을 눌러도 서버가 gameplay truth를 직접 조작하지 않고 command emit 실패로 끝나는지 확인.
