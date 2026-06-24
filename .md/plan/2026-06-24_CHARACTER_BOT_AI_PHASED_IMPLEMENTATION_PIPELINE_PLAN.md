Session - Character Bot AI 공통 판단/챔피언 전술/ML-ready 토대를 Phase 단위로 반영하고 검증한다.

1. 반영해야 하는 코드

본질:
- 한 번에 전체 AI를 갈아엎지 않는다.
- 각 Phase는 `계획서 -> 코드 반영 -> 빌드/검증 -> 결과 보고서`로 닫는다.
- Bot AI는 계속 서버 권위 `GameCommand` 생산자다. Transform, HP, cooldown, damage truth를 직접 변경하지 않는다.
- ML/RL 연결은 마지막 단계다. 먼저 deterministic evidence와 legal action pipeline을 고정한다.

Phase 순서:
- Phase A: 현재 기준선 고정. Yone E stage-2 runtime return probe를 SimLab에 넣어 데이터 계약이 실제 GameSim 복귀로 닫히는지 검증한다. SimLab도 생성된 서버 definition pack을 직접 물려서 legacy fallback이 아닌 JSON 기반 runtime 경로를 검증한다.
- Phase B: 공통 decision evidence 확장. 골드, 레벨, 체력 우위, 미니언/포탑 가치, 시야 위험, 오브젝트 pressure를 context/score로 모은다.
- Phase C: brain input 확장. `ChampionAIBrainInput`이 공통 evidence를 받아 intent를 결정하게 한다.
- Phase D: 챔피언별 tactic 정리. 공격/미니언 공격 시 챔피언 특성에 따른 command 후보와 combo를 적용한다.
- Phase E: debug/trace 확장. 판단 근거를 snapshot/debug UI에서 확인할 수 있게 한다.
- Phase F: ML/RL bridge. deterministic decision-frame export와 offline policy artifact 연결점을 만든다.

1-1. C:/Users/user/Desktop/Winters/Tools/SimLab/main.cpp

Phase A에서 반영한다.

추가 전제:
- SimLab은 `ServerData::GetLoLGameplayDefinitionPack()`을 `TickContext::pDefinitions`에 넣는다.
- 이 연결이 없으면 `GameplayDefinitionQuery`가 legacy fallback으로 돌아가므로, JSON -> generated server definition -> runtime GameSim 계약을 검증하지 못한다.

`MatchResult RunMatch(u64_t seed, u64_t tickCount)` 아래에 Yone E runtime probe helper를 추가한다.

기존 코드:

```cpp
        result.finalHash = finalHash;
        return result;
    }
}
```

아래에 추가:

```cpp
    TickContext MakeProbeTickContext(
        u64_t tick,
        DeterministicRng& rng,
        EntityIdMap& entityMap,
        FlatWalkable& walkable);

    void TickYoneProbe(CWorld& world, const TickContext& tc);

    f32_t DistanceSqXZLocal(const Vec3& a, const Vec3& b);

    bool_t RunYoneEReturnProbe();
```

실제 구현은 같은 anonymous namespace 안의 helper 함수 정의로 추가한다. 별도 header는 만들지 않는다.

Phase A 성공 기준:
- E 1타가 `YoneSimComponent::bSoulUnboundActive = true`를 만든다.
- E 2타가 `cmd.itemId = 2u`로 stage-2 path를 타고 `bReturning = true`를 만든다.
- `YoneGameSim::Tick` 이후 anchor로 복귀하고 `bSoulUnboundActive`, `bReturning`, `soulTimerSec`가 clear된다.

1-2. C:/Users/user/Desktop/Winters/Tools/SimLab/main.cpp

`main()`에서 기존 deterministic match 전에 `RunYoneEReturnProbe()`를 실행한다.

기존 코드:

```cpp
    RegisterAllChampionHooks();

    const MatchResult runA = RunMatch(seed, tickCount);
    const MatchResult runB = RunMatch(seed, tickCount);
    const MatchResult runC = RunMatch(seed + 1, tickCount);

    bool bPass = true;
```

아래로 교체:

```cpp
    RegisterAllChampionHooks();

    const bool_t bYoneEReturnProbePass = RunYoneEReturnProbe();

    const MatchResult runA = RunMatch(seed, tickCount);
    const MatchResult runB = RunMatch(seed, tickCount);
    const MatchResult runC = RunMatch(seed + 1, tickCount);

    bool bPass = bYoneEReturnProbePass;
```

의도:
- full data-driven pipeline이 SimLab을 실행할 때마다 Yone E runtime contract가 같이 검증된다.
- 별도 CLI 옵션을 만들지 않는다. 호출 누락을 막기 위해 기본 SimLab PASS 조건에 포함한다.

1-3. C:/Users/user/Desktop/Winters/Tools/SimLab/SimLab.vcxproj

Phase A에서 반영한다.

생성된 서버 gameplay definition pack 구현 파일을 SimLab 빌드에 포함한다.

추가:

```xml
<ClCompile Include="..\..\Server\Private\Data\Generated\LoLGameplayDefinitions.generated.cpp" />
```

의도:
- `ServerData::GetLoLGameplayDefinitionPack()` 심볼을 SimLab에서 직접 링크한다.
- SimLab deterministic run과 Yone E probe가 같은 generated definition source를 보게 한다.

1-4. Phase B 코드 계획

CONFIRM_NEEDED:
- `GoldComponent`, `ChampionScoreComponent`, `VisibilityComponent`, ward ownership/visibility field, jungle/objective component를 실제로 더 확인한 뒤 exact code block을 작성한다.

대상 파일:
- `C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp`
- `C:/Users/user/Desktop/Winters/Shared/GameSim/Components/ChampionAIComponent.h`

반영 방향:
- `ChampionAIContext`에 gold/level/health/minion/structure/objective/vision score 근거를 추가한다.
- `BuildChampionAIContext(...)`는 raw fact 수집만 한다.
- `UpdateChampionAIDecisionEvidence(...)`는 score 계산만 한다.
- score는 `0..1` 정규화로 유지한다.

1-5. Phase C 코드 계획

CONFIRM_NEEDED:
- Phase B field 확정 이후 `ChampionAIBrainInput`에 정확한 score field를 추가한다.

대상 파일:
- `C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIBrain.h`
- `C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIBrain.cpp`
- `C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp`

반영 방향:
- Brain은 world를 직접 읽지 않는다.
- Brain은 intent만 반환한다.
- 기본 difficulty 2는 기존 RuleBased 체감을 보존한다.

1-6. Phase D 코드 계획

CONFIRM_NEEDED:
- Yone/Yasuo 이후 어떤 챔피언 순서로 tactic을 정리할지 선택해야 한다.

대상 파일:
- `C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp`
- `C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h`
- `C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.cpp`

반영 방향:
- 공통 brain이 `AttackChampion` 또는 `FarmMinion` intent를 낸 뒤에만 champion tactic이 실행된다.
- champion tactic은 command 후보를 만들고 emitter를 호출한다.
- Yone E return 같은 생존형 recast는 scored candidate가 아니라 reaction override로 둔다.

1-7. Phase E 코드 계획

CONFIRM_NEEDED:
- Snapshot schema 확장까지 할지, 우선 내부 trace/report만 늘릴지 결정해야 한다.

대상 파일:
- `C:/Users/user/Desktop/Winters/Shared/GameSim/Components/ChampionAIComponent.h`
- `C:/Users/user/Desktop/Winters/Server/Private/Game/SnapshotBuilder.cpp`
- `C:/Users/user/Desktop/Winters/Client/Private/UI/AIDebugPanel.cpp`

반영 방향:
- debug UI는 판단 근거 표시만 한다.
- Client는 AI truth를 만들지 않는다.

1-8. Phase F 코드 계획

CONFIRM_NEEDED:
- decision frame export 대상이 SimLab인지 별도 Tools runner인지 정해야 한다.

대상 후보:
- `C:/Users/user/Desktop/Winters/Tools/SimLab/main.cpp`
- 신규 offline export tool

반영 방향:
- feature schema version/hash를 둔다.
- legal action mask와 선택된 command를 저장한다.
- runtime frame에서 외부 모델 호출을 block하지 않는다.
- runtime에는 검증된 deterministic policy artifact만 붙인다.

2. 검증

Phase A 검증 명령:

```powershell
msbuild Tools/SimLab/SimLab.vcxproj /p:Configuration=Debug /p:Platform=x64
Tools/Bin/Debug/SimLab.exe
powershell -NoProfile -ExecutionPolicy Bypass -File Tools\Harness\Run-BotAiValidation.ps1 -SkipFullPipeline
powershell -NoProfile -ExecutionPolicy Bypass -File Tools\LoLData\Verify-LoLDataDrivenPipeline.ps1 -Configuration Debug
git diff --check
```

Phase B 이후 공통 검증:

```powershell
msbuild Shared/GameSim/Include/GameSim.vcxproj /p:Configuration=Debug /p:Platform=x64
msbuild Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64
msbuild Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64
powershell -NoProfile -ExecutionPolicy Bypass -File Tools\Harness\Run-BotAiValidation.ps1 -SkipFullPipeline
powershell -NoProfile -ExecutionPolicy Bypass -File Tools\LoLData\Verify-LoLDataDrivenPipeline.ps1 -Configuration Debug
git diff --check
```

결과 보고서 규칙:
- 각 Phase 완료 후 `.md/build/YYYY-MM-DD_CHARACTER_BOT_AI_PHASE_<X>_REPORT.md`를 작성한다.
- 보고서에는 반영 파일, 검증 명령, PASS/WARN/FAIL, 기존 경고와 신규 경고 구분, 다음 Phase 진입 조건을 적는다.

중단 조건:
- `CommandExecutor` 검증을 우회해야 통과하는 AI 변경은 중단한다.
- normal F5 roster/map/minion/snapshot/champion/UI/FX를 숨겨서 검증하는 변경은 중단한다.
- 새로운 scorer/helper가 truth component를 직접 수정하기 시작하면 중단한다.
