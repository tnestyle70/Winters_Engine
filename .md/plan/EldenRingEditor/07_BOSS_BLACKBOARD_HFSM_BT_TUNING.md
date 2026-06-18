Session - Boss Blackboard HFSM Behavior Tree tuning

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Components/BossAIComponent.h

새 파일:

```cpp
CONFIRM_NEEDED - ChampionAIComponent.h의 runtime tuning/debug trace 구조와 Shared/GameSim component registration 방식을 확인한 뒤 전체 파일 본문을 작성한다.

책임:
- boss kind: Margit, TreeGuard, GenericMob.
- blackboard keys: target entity, distance, hp ratio, phase, stagger, arena anchor, last attack id.
- HFSM state: Idle, Patrol, Engage, Combo, Recover, Retreat, Staggered, Dead.
- behavior tree decision result는 GameCommand 후보로만 나간다.
- tuning param은 JSON default + runtime override를 모두 지원한다.
```

1-2. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/BossAI/BossAISystem.h

새 파일:

```cpp
CONFIRM_NEEDED - CChampionAISystem::Execute signature와 GameCommand producer 패턴 확인 후 전체 파일 본문을 작성한다.

책임:
- static void Execute(CWorld& world, const TickContext& tc, std::vector<GameCommand>& outCommands)
- BossAIComponent와 Transform/Combat 상태를 읽는다.
- gameplay 결과를 직접 mutate하지 않고 outCommands만 생산한다.
```

1-3. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/BossAI/BossAISystem.cpp

새 파일:

```cpp
CONFIRM_NEEDED - boss skill command schema, entity query helpers, existing ChampionAISystem command enqueue helpers를 확인한 뒤 전체 구현 본문을 작성한다.
구현 기준:
- ChampionAISystem의 debug trace/tuning 방식을 재사용한다.
- BehaviorTree는 Engine/Public/AI/BehaviorTree.h contract를 따르되, 서버 GameSim 안에서는 deterministic data만 사용한다.
- HFSM transition은 cooldown, distance, phase hp ratio, action lock을 기준으로 한다.
- output command는 m_pendingExecCommands로 들어가 기존 CommandExecutor가 처리하게 한다.
```

1-4. C:/Users/tnest/Desktop/Winters/Server/Public/Game/GameRoom.h

기존 코드:

```cpp
    void Phase_ServerBotAI(TickContext& tc);
```

아래에 추가:

```cpp
    void Phase_ServerBossAI(TickContext& tc);
```

1-5. C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoomBossAI.cpp

새 파일:

```cpp
CONFIRM_NEEDED - GameRoomChampionAI.cpp include/style을 확인한 뒤 전체 파일 본문을 작성한다.
의도한 구조:
- #include "Game/GameRoom.h"
- #include "Shared/GameSim/Systems/BossAI/BossAISystem.h"
- CGameRoom::Phase_ServerBossAI(TickContext& tc)에서 CBossAISystem::Execute(m_world, tc, m_pendingExecCommands)를 호출한다.
```

1-6. C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoomTick.cpp

기존 코드:

```cpp
	Phase_ServerBotAI(tc);
```

아래에 추가:

```cpp
	Phase_ServerBossAI(tc);
```

의도:
- Champion/Jungle/Attack AI처럼 서버 tick에서 GameCommand producer로만 참여한다.
- 실행 순서는 `Phase_ServerBotAI(tc)` 이후, `m_pendingExecCommands`가 drain/execute되기 전이어야 한다.

1-7. C:/Users/tnest/Desktop/Winters/Client/Private/UI/AIDebugPanel.cpp

기존 코드:

```cpp
		ImGui::SeparatorText("Runtime Tuning");
```

아래에 추가:

```cpp
        CONFIRM_NEEDED - BossAI debug snapshot schema가 생긴 뒤 Boss Runtime Tuning 섹션을 추가한다.
        ChampionAI tuning slider와 같은 네트워크 debug command path를 쓰되,
        boss entity netId, tuning id, value만 boss 전용 enum으로 분리한다.
```

1-8. C:/Users/tnest/Desktop/Winters/Client/Bin/Resource/EldenRing/Boss/Margit/boss_tuning.json

새 파일:

```json
{
  "schema": "winters.eldenring.boss_tuning.v1",
  "bossId": "c2130_margit",
  "blackboard": {
    "scanRange": 32.0,
    "leashRange": 42.0,
    "meleeRange": 4.5,
    "midRange": 12.0,
    "phase2HpRatio": 0.55
  },
  "hfsm": {
    "engageCooldown": 0.35,
    "comboRecover": 0.75,
    "staggerRecover": 1.40
  },
  "behaviorTree": {
    "preferGapCloseDistance": 10.0,
    "retreatHpRatio": 0.10,
    "comboScoreMargin": 0.15
  }
}
```

2. 검증

2-1. 검증 명령

```powershell
msbuild Winters.sln /t:Server /p:Configuration=Debug /p:Platform=x64
msbuild Winters.sln /t:Client /p:Configuration=Debug /p:Platform=x64
```

2-2. 서버 권위 확인

```text
1. Boss AI는 Client visual path에서 gameplay 결과를 직접 만들지 않는다.
2. Boss decision은 GameCommand로 m_pendingExecCommands에 들어간다.
3. Snapshot/Event가 client로 전달된 뒤 animation/FX/UI만 client에서 재생한다.
4. Debug panel tuning은 서버에 command로 전송되고, snapshot/debug trace로 되돌아온 값을 표시한다.
```

2-3. 런타임 확인

```text
1. boss blackboard에 target, distance, hp ratio, phase, selected action, block reason이 보인다.
2. HFSM transition trace가 bounded ring buffer로 남는다.
3. BehaviorTree leaf 실패 사유가 "cooldown/range/action locked/no target"처럼 분리된다.
4. tuning json을 바꾼 뒤 재로드하거나 runtime slider로 바꿨을 때 서버 decision이 달라진다.
```

2-4. 다음 세션 게이트

```text
멀기트 c2130 skeletal animation baking이 막힌 상태에서도 Boss AI tuning은 placeholder/static proxy entity로 먼저 검증할 수 있다.
단, 실제 boss animation montage/FX cue 매핑은 c2130 wskel reload와 animation baking 문제가 풀린 뒤 연결한다.
```
