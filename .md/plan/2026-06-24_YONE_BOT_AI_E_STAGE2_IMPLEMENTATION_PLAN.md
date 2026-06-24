Session - Yone Bot AI E return stage-2 contract를 원천 데이터 한 곳에서 닫는다.

1. 반영해야 하는 코드

본질:
- AI C++은 이미 `cmd.itemId = 2u`로 Yone E return을 stage-2 명령으로 발행한다.
- `CommandExecutor`도 이미 `cmd.itemId == 2u`를 stage-2 요청으로 검증한다.
- 현재 실패 지점은 데이터 계약뿐이다.
- 그러므로 C++ 판단 코드를 늘리지 않고, 원천 champion data의 Yone E stage 계약만 고친다.

전제:
- `Data/Gameplay/ChampionGameData/champions.json`, `Data/LoL/ServerPrivate/Gameplay/SkillGameplayDefs.json`, `Server/Private/Data/Generated/LoLGameplayDefinitions.generated.cpp`는 이미 dirty 상태다.
- 실제 반영 시 기존 cooldown/range/generated 변경은 보존한다.
- 아래 변경은 Yone E의 `stageCount`와 `stageWindowSec`만 추가로 바꾼다.
- `SkillGameplayDefs.json`과 `LoLGameplayDefinitions.generated.cpp`는 직접 편집하지 않고 `Tools/LoLData/Build-LoLDefinitionPack.py`로 생성한다.

1-1. C:/Users/user/Desktop/Winters/Data/Gameplay/ChampionGameData/champions.json

`"champion": "YONE"`의 `"slot": 3` 스킬 블록에서 아래 기존 코드만 교체한다.

기존 코드:

```json
        {
          "slot": 3,
          "targetMode": "Conditional",
          "stageCount": 1,
          "stageWindowSec": 0.0,
          "cooldownSec": 1.0,
          "rangeMax": 4.0,
          "manaCost": 0.0,
          "skillId": 0,
          "scalingTableId": 0,
          "gameplayPolicyId": 0,
          "visualCueId": 0,
          "stages": [
            {
              "lockDurationSec": 0.75,
              "animPlaySpeed": 1.0,
              "castFrame": 0.0,
              "recoveryFrame": 0.0
            }
          ]
        },
```

아래로 교체:

```json
        {
          "slot": 3,
          "targetMode": "Conditional",
          "stageCount": 2,
          "stageWindowSec": 5.0,
          "cooldownSec": 1.0,
          "rangeMax": 4.0,
          "manaCost": 0.0,
          "skillId": 0,
          "scalingTableId": 0,
          "gameplayPolicyId": 0,
          "visualCueId": 0,
          "stages": [
            {
              "lockDurationSec": 0.75,
              "animPlaySpeed": 1.0,
              "castFrame": 0.0,
              "recoveryFrame": 0.0
            }
          ]
        },
```

이유:
- `stageCount = 2`는 `GameplayDefinitionQuery::IsSkillTwoStage(...)`를 true로 만들기 위한 핵심이다.
- `stageWindowSec = 5.0`은 Yone E의 `effectDurationSec = 5.0`과 같은 시간축이다.
- 두 번째 stage lock은 생성기의 기본 stage 값 `0.6`으로 이미 채워진다. 별도 비기본 lock이 필요해질 때만 source stages에 두 번째 항목을 명시한다.

1-2. C:/Users/user/Desktop/Winters/Data/LoL/ServerPrivate/Gameplay/SkillGameplayDefs.json

직접 편집 금지. 1-1 반영 후 아래 명령으로 생성한다.

```powershell
python Tools\LoLData\Build-LoLDefinitionPack.py
```

생성 후 `skill.yone.e` 블록은 아래 계약을 만족해야 한다.

기존 코드:

```json
      "stage": {
        "count": 1,
        "windowSeconds": 0.0,
        "lockSeconds": [
          0.75,
          0.6
        ]
      },
```

아래로 교체:

```json
      "stage": {
        "count": 2,
        "windowSeconds": 5.0,
        "lockSeconds": [
          0.75,
          0.6
        ]
      },
```

1-3. C:/Users/user/Desktop/Winters/Server/Private/Data/Generated/LoLGameplayDefinitions.generated.cpp

직접 편집 금지. 1-1 반영 후 `Build-LoLDefinitionPack.py`가 생성해야 한다.

`MakeSkill_YONE_E()` 내부에서 아래 계약을 만족해야 한다.

기존 코드:

```cpp
        def.range.rangeMax = 4.f;
        def.stage.stageCount = 1u;
        def.stage.stageWindowSec = 0.f;
        def.effect.scalingTableId = 0u;
```

아래로 교체:

```cpp
        def.range.rangeMax = 4.f;
        def.stage.stageCount = 2u;
        def.stage.stageWindowSec = 5.f;
        def.effect.scalingTableId = 0u;
```

1-4. C++ 판단 코드 변경 없음

아래 코드는 이미 원하는 본질을 만족하므로 이번 구현에서 수정하지 않는다.

`Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp`

```cpp
cmd.slot = eSlot;
cmd.itemId = 2u;
cmd.direction = WintersMath::DirectionXZ(selfPos, pYoneState->anchorPosition);
cmd.groundPos = pYoneState->anchorPosition;
```

`Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`

```cpp
const bool_t bStage2 =
    bRequestedStage2 &&
    slot.currentStage == 1 &&
    slot.stageWindow > 0.f &&
    GameplayDefinitionQuery::IsSkillTwoStage(
        world,
        cmd.issuerEntity,
        tc,
        hookChampion,
        hookSlot);
```

`Shared/GameSim/Champions/Yone/YoneGameSim.cpp`

```cpp
if (state.bSoulUnboundActive)
{
    StartSoulReturn(world, ctx.casterEntity, ctx.pTickCtx, false);
    std::cout << "[YoneSim] E return caster=" << ctx.casterEntity << "\n";
    return;
}
```

2. 검증

미검증:
- 아직 코드/data 반영 전 계획서 단계다.
- 실제 반영 후 아래 명령 순서로 확인한다.

검증 명령:

```powershell
python Tools\LoLData\Build-LoLDefinitionPack.py
python Tools\LoLData\Build-LoLDefinitionPack.py --check
powershell -NoProfile -ExecutionPolicy Bypass -File Tools\Harness\Run-BotAiValidation.ps1 -SkipFullPipeline
powershell -NoProfile -ExecutionPolicy Bypass -File Tools\LoLData\Verify-LoLDataDrivenPipeline.ps1 -Configuration Debug
```

성공 기준:
- `Build-LoLDefinitionPack.py --check`가 PASS한다.
- `Run-BotAiValidation.ps1 -SkipFullPipeline`에서 Yone E stage-2 contract audit가 PASS한다.
- `Verify-LoLDataDrivenPipeline.ps1 -Configuration Debug`가 `GameSim`, `Server`, `Client`, `SimLab` build와 deterministic regression까지 PASS한다.
- `CommandExecutor`에서 Yone E return이 더 이상 `stage2-window`로 거절되지 않는다.

수동 확인:
- 정상 F5/F9 흐름에서 roster, map, minion, snapshot, champion, UI, FX를 숨기지 않는다.
- Yone Bot이 E로 들어간 뒤 timer, 체력 불리, 포탑 위험, target lost 조건 중 하나에서 E return 명령을 발행하는지 확인한다.
- 복귀 결과는 `YoneGameSim::OnE -> StartSoulReturn -> Snapshot/Event -> Client Visual` 흐름으로 확인한다.

중단 조건:
- 생성기가 Yone E stage 계약 외의 예상하지 못한 대규모 diff를 만들면 즉시 멈추고 diff를 먼저 검토한다.
- 기존 dirty 변경을 되돌려야 하는 상황이 생기면 직접 되돌리지 않고 사용자 확인을 받는다.
