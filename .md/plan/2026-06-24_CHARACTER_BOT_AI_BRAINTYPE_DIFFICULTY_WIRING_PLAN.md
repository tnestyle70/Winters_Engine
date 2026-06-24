Session - Character Bot brainType과 difficulty를 기본 동작 보존 상태로 연결한다.

1. 반영해야 하는 코드

본질:
- 공통 Character Bot AI의 본질은 "어떤 brain을 쓸지"와 "그 brain이 어떤 GameCommand를 만들지"를 분리하는 것이다.
- `ChampionAIComponent::brainType`은 이미 존재하고, `ResolveChampionAIBrain(...)`도 이미 있다.
- 현재 빠진 본질은 스폰 시점에서 difficulty를 brainType으로 연결하는 얇은 wiring이다.
- 기본 F5/F9 bot behavior를 갑자기 바꾸면 안 된다. 기본 difficulty 2는 기존 `RuleBased`를 유지한다.

현재 코드 증거:
- `C:/Users/user/Desktop/Winters/Shared/GameSim/Components/ChampionAIComponent.h`에 `difficulty`와 `brainType = RuleBased`가 있다.
- `C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIBrain.cpp`는 `RuleBased`, `PlayerLike`, `Decision` brain을 resolve한다.
- `C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp`는 `ResolveChampionAIBrain(ai.brainType)`로 lane combat intent를 결정한다.
- `C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomSpawn.cpp`는 `ai.difficulty = slot.botDifficulty;`만 설정하고 `brainType`은 기본값에 맡긴다.
- `C:/Users/user/Desktop/Winters/Server/Private/Game/LobbyAuthority.cpp`는 difficulty 0을 2로 정규화한다. 즉 기본값은 2다.

1-1. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomSpawn.cpp

anonymous namespace 안에서 `kChampionAIInitialDecisionDelaySec` 바로 아래에 helper를 추가:

기존 코드:

```cpp
    constexpr int32_t kStageChampionSpawnWalkableSearchRadius = 16;
    constexpr f32_t kChampionAIInitialDecisionDelaySec = 0.35f;

    VisibilityComponent BuildServerVisibleToAll()
```

아래에 추가:

```cpp
    eChampionAIBrainType ResolveInitialChampionAIBrainType(u8_t difficulty)
    {
        return difficulty >= 3u
            ? eChampionAIBrainType::PlayerLike
            : eChampionAIBrainType::RuleBased;
    }
```

이유:
- difficulty 1, 2는 기존 RuleBased를 유지한다.
- difficulty 3 이상만 PlayerLike로 보낸다.
- `Decision` brain은 아직 외부 의사결정 모듈 입력이 없으므로 여기서 연결하지 않는다.

1-2. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomSpawn.cpp

AI component 초기화에서 difficulty 설정 직후 brainType을 설정:

기존 코드:

```cpp
        ai.difficulty = slot.botDifficulty;
        ai.lane = static_cast<u8_t>(slot.lane);
        ai.decisionTimer = kChampionAIInitialDecisionDelaySec;
```

아래로 교체:

```cpp
        ai.difficulty = slot.botDifficulty;
        ai.brainType = ResolveInitialChampionAIBrainType(ai.difficulty);
        ai.lane = static_cast<u8_t>(slot.lane);
        ai.decisionTimer = kChampionAIInitialDecisionDelaySec;
```

의도:
- AI brain 선택 책임은 Server spawn이 갖는다.
- Shared/GameSim은 선택된 `brainType`을 실행만 한다.
- Client는 이 값으로 gameplay truth를 만들지 않는다.

중단 조건:
- lobby/editor에서 brainType을 직접 선택하는 UI가 필요해지면 이번 wiring 단계에서 멈추고 별도 client/server config contract 문서로 분리한다.
- difficulty 2 기본 bot의 행동이 변하면 즉시 rollback하고 mapping을 재검토한다.
- `Decision` brain을 연결하려면 먼저 입력/출력 artifact, deterministic replay, fallback policy가 확정되어야 한다.

2. 검증

검증 명령:

```powershell
msbuild Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64
powershell -NoProfile -ExecutionPolicy Bypass -File Tools\Harness\Run-BotAiValidation.ps1 -SkipFullPipeline
powershell -NoProfile -ExecutionPolicy Bypass -File Tools\LoLData\Verify-LoLDataDrivenPipeline.ps1 -Configuration Debug
```

성공 기준:
- 기본 bot difficulty 2에서 기존 RuleBased behavior가 유지된다.
- difficulty 3 이상의 bot만 PlayerLike brain을 사용한다.
- full pipeline이 `GameSim`, `Server`, `Client`, `SimLab`까지 PASS한다.
- `ChampionAI dependency boundary audit`가 PASS한다.

핸드오프 메모:
- 이 계획은 brainType wiring만 한다. PlayerLike brain 자체의 판단 고도화는 별도 계획에서 다룬다.
- 이 wiring이 끝나면 기획자는 difficulty로 bot 성향을 선택할 수 있고, 개발자는 brain 구현을 Shared/GameSim 안에서만 확장할 수 있다.
- 디자이너가 확인해야 할 것은 "difficulty 3 이상에서 더 공격적으로 보이는가"이며, gameplay truth는 Server/GameSim 검증으로 판단한다.
