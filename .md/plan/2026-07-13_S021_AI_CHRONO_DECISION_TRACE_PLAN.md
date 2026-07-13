Session - S020 Claude lane을 기능 단위로 감사하고, 승인 기반 스킬 간격·retreat→recall 전이·선택 근거/executor 결과·F9 Chrono branch timeline을 S015/S017 기반에 반영한다.

날짜: 2026-07-13. 작업 packet: `2026-07-13_s021_ai_chrono_decision_trace`. 기준: `main@9110091`, 다수의 기존 dirty 변경을 보존하며 commit은 만들지 않는다.

현재 코드 증거는 다음과 같다. S020의 미니맵 기저는 직교·등길이이고, F9 접힘 UI와 5초 fresh-cast gate는 문서의 Debug x64/SimLab 결과 및 산출물 시각과 일치한다. 그러나 `ChampionAISystem.cpp`는 executor 결과 전에 interval timer를 장전하고, runtime cooldown과 policy interval을 같은 `SkillCooldown`으로 기록한다. Retreat 도달 Recall은 emergency low-HP return 뒤에 있어 도달 후에도 계속 Move만 제출한다. F9는 모든 봇의 16행 legacy trace를 매 snapshot 복제하지만 Retreat 점수, command sequence, executor 결과, epoch/branch가 없다. S015 Chrono는 30-tick keyframe을 transactional restore하고 미래 keyframe을 버린 뒤 epoch/branch를 증가시키므로 기반은 재사용한다.

## 1. 반영해야 하는 코드

### 1-1. `Shared/GameSim/Components/ChampionAIComponent.h`

`eChampionAIDecisionBlockReason`의 기존 tail:

```cpp
	InvalidPath,
	CommandRejected,
```

아래에 append-only로 추가한다.

```cpp
	PolicyCastInterval,
	RuntimeSkillCooldown,
```

`eChampionAITuningId`의 기존 tail:

```cpp
	DiveScanRange,
	DiveExtraBAWindow,
	Count,
```

아래로 교체한다.

```cpp
	DiveScanRange,
	DiveExtraBAWindow,
	SkillCastMinInterval,
	Count,
```

`ChampionAIDecisionTraceEntry`의 점수/관측 tail에 Retreat, candidate legality, command identity, executor 결과, combo step, interval evidence를 append한다. 기존 16-entry bounded ring과 `AiEpisodeV1` ABI는 유지한다.

```cpp
	f32_t turretDanger = 0.f;
	f32_t retreatScore = 0.f;
	f32_t skillCastIntervalSec = 5.f;
	f32_t skillCastIntervalRemainingSec = 0.f;
	u32_t legalCandidateMask = 0u;
	u32_t illegalCandidateMask = 0u;
	u32_t commandSequence = 0u;
	u16_t executorReason = 0u;
	u8_t executorState = static_cast<u8_t>(AiExecutorStateV1::Unknown);
	u8_t comboStep = 0u;
```

`ChampionAITuning`의 `diveExtraBAWindow` 아래에 15번째 runtime tuning을 추가한다.

```cpp
	ChampionAITuningParam skillCastMinInterval{ 5.f, 5.f, 0.f, 15.f, false };
```

`ChampionAIComponent::fSkillCastCooldownTimer` 위에 effective 값 owner를 추가한다.

```cpp
	f32_t fSkillCastMinInterval = 5.f;
```

`ChampionAIDebugComponent`는 wire에서 최신 1행을 받은 뒤 패널이 바로 읽을 current summary를 가진다.

```cpp
	f32_t fRetreatDecisionScore = 0.f;
	f32_t fSkillCastMinInterval = 5.f;
	f32_t fSkillCastCooldownTimer = 0.f;
	u32_t lastCommandSequence = 0u;
	u16_t lastExecutorReason = 0u;
	u8_t lastExecutorState = static_cast<u8_t>(AiExecutorStateV1::Unknown);
```

### 1-2. `Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp`

S020의 file-local `kChampionAISkillCastMinInterval` 상수 블록은 삭제하고 effective tuning field를 사용한다.

`EmitSkillCommand`의 policy/runtime cooldown block:

```cpp
if (!bStage2 && !bCommittedSequence &&
    ai.fSkillCastCooldownTimer > 0.f)
{
    SetChampionAIBlockReason(ai, eChampionAIDecisionBlockReason::SkillCooldown);
    return false;
}
if (!bStage2 && !IsSkillReady(world, self, slot))
{
    SetChampionAIBlockReason(ai, eChampionAIDecisionBlockReason::SkillCooldown);
    return false;
}
```

아래로 교체한다.

```cpp
if (!bStage2 && !bCommittedSequence &&
    ai.fSkillCastCooldownTimer > 0.f)
{
    SetChampionAIBlockReason(
        ai,
        eChampionAIDecisionBlockReason::PolicyCastInterval);
    return false;
}
if (!bStage2 && !IsSkillReady(world, self, slot))
{
    SetChampionAIBlockReason(
        ai,
        eChampionAIDecisionBlockReason::RuntimeSkillCooldown);
    return false;
}
```

동일 의미의 stage-window와 basic-attack cooldown reason도 `RuntimeSkillCooldown`, fresh combo gate는 `PolicyCastInterval`로 분리한다.

명령 push 직후의 아래 코드는 삭제한다. Timer는 executor Accepted에서만 commit한다.

```cpp
if (!bStage2)
    ai.fSkillCastCooldownTimer = kChampionAISkillCastMinInterval;
```

`PushChampionAIDecisionTrace`에서 기존 점수 복사 아래에 다음 evidence를 추가하고, command submitted이면 sequence/state를 `Submitted`로 기록한다.

```cpp
entry.retreatScore = ai.fRetreatDecisionScore;
entry.skillCastIntervalSec = ai.fSkillCastMinInterval;
entry.skillCastIntervalRemainingSec = ai.fSkillCastCooldownTimer;
entry.comboStep = ai.comboStep;
entry.commandSequence = bCommandSubmitted ? ai.nextCommandSequence - 1u : 0u;
entry.executorState = static_cast<u8_t>(
    bCommandSubmitted ? AiExecutorStateV1::Submitted : AiExecutorStateV1::Unknown);
entry.executorReason = 0u;
```

typed draft를 확정한 뒤 legacy entry에도 다음을 복사한다.

```cpp
entry.legalCandidateMask = research.actionMask.legalCandidateMask;
entry.illegalCandidateMask = research.actionMask.illegalCandidateMask;
```

`ApplyChampionAIProfileAndTuning`의 `diveExtraBAWindow` 대입 아래에 추가한다.

```cpp
ai.fSkillCastMinInterval = ResolveChampionAITuningParam(
    ai.tuning.skillCastMinInterval, 5.f, bOverrideProfile);
```

action-lock 처리 뒤, emergency low-HP return 앞에 Retreat 도달 전이를 추가한다. Recall executor가 거절할 상태라면 상태를 미리 `Recalling`으로 바꾸지 않고 `StateBlocked` trace를 남긴다.

```cpp
if (ai.state == eChampionAIState::Retreat &&
    HasReachedGoal(selfPos, ai.retreatGoal, 1.5f))
{
    if (ctx.bCanCast)
    {
        EmitRecall(world, tc, self, ai, champion.id, selfPos,
            "retreat-arrived-recall", outCommands);
    }
    else
    {
        SetChampionAIBlockReason(
            ai,
            eChampionAIDecisionBlockReason::StateBlocked);
        PushChampionAIDecisionTrace(world, self, ai, tc, NULL_ENTITY, false);
    }
    return;
}
```

아래쪽의 중복 `Retreat` 도달 Recall block은 삭제하고, 미도달 재후퇴 조건만 유지한다.

### 1-3. `Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`

`ResolveChampionAITuningParamById`의 `DiveExtraBAWindow` case 아래에 추가한다.

```cpp
case eChampionAITuningId::SkillCastMinInterval:
    return &ai.tuning.skillCastMinInterval;
```

`FinalizeChampionAICommandTrace`는 rich research component가 없어도 authoritative AI component의 matching legacy row를 닫는다. 기존 validation 뒤 다음 순서로 처리한다.

```cpp
auto& ai = world.GetComponent<ChampionAIComponent>(cmd.issuerEntity);
const u8_t executorState = static_cast<u8_t>(
    result.state == eCommandExecutionState::Accepted
        ? AiExecutorStateV1::Accepted
        : AiExecutorStateV1::Rejected);
const u16_t executorReason = static_cast<u16_t>(result.reason);

ChampionAIDecisionTraceEntry* pLegacyTrace = nullptr;
for (u8_t offset = 0u; offset < traceCount; ++offset)
{
    ChampionAIDecisionTraceEntry& trace = ai.debugDecisionTrace[index];
    if (trace.commandSequence == cmd.sequenceNum &&
        trace.executorState == static_cast<u8_t>(AiExecutorStateV1::Submitted))
    {
        trace.executorState = executorState;
        trace.executorReason = executorReason;
        pLegacyTrace = &trace;
        break;
    }
}

if (result.state == eCommandExecutionState::Accepted &&
    cmd.kind == eCommandKind::CastSkill && cmd.itemId != 2u)
{
    ai.fSkillCastCooldownTimer = ai.fSkillCastMinInterval;
}
else if (result.state == eCommandExecutionState::Rejected && pLegacyTrace)
{
    pLegacyTrace->blockReason = eChampionAIDecisionBlockReason::CommandRejected;
    if (ai.comboTarget != NULL_ENTITY)
        ai.comboStep = pLegacyTrace->comboStep;
}
```

그 뒤 기존 `ChampionAIResearchDebugComponent` trace/draft matching update를 그대로 수행한다. unmatched sequence와 Unknown 결과는 어떤 row/timer/combo도 바꾸지 않는다.

### 1-4. `Shared/Schemas/Snapshot.fbs`, generated C++/Go, Server Builder, Client Applier

`AIDebugTraceRow` tail의 `turretDanger` 아래에 append-only field를 추가한다.

```fbs
    retreatScore:float;
    skillCastIntervalSec:float;
    skillCastIntervalRemainingSec:float;
    legalCandidateMask:uint;
    illegalCandidateMask:uint;
    commandSequence:uint;
    executorReason:ushort;
    executorState:ubyte;
    comboStep:ubyte;
```

`Shared/Schemas/run_codegen.bat`로 generated C++/Go를 재생성하며 generated 파일은 수동 편집하지 않는다.

`SnapshotBuilder.cpp`의 AI trace serialization은 기존 `count`개 loop 전체를 아래 정책으로 교체한다.

```cpp
const u8_t count = std::min<u8_t>(
    ai.debugDecisionTraceCount,
    kChampionAIDebugTraceCapacity);
if (count > 0u)
{
    const u8_t index = static_cast<u8_t>(
        (ai.debugDecisionTraceHead + kChampionAIDebugTraceCapacity - 1u) %
        kChampionAIDebugTraceCapacity);
    // CreateAIDebugTraceRow(... existing fields ..., appended evidence fields)
}
```

즉 서버 ring은 16행을 유지하되 wire는 모든 봇에 최신 1행만 보낸다. 기존 최대 16행/봇/snapshot에서 1행/봇/snapshot으로 상세 복제량을 93.75% 줄이고, 선택 bot의 긴 history는 Client F9가 panel-open cadence로만 축적한다.

`SnapshotApplier.cpp`의 row mapping에 append field를 복사하고, 최신 row에서 `ChampionAIDebugComponent` current summary를 갱신한다.

```cpp
dst.retreatScore = pRow->retreatScore();
dst.skillCastIntervalSec = pRow->skillCastIntervalSec();
dst.skillCastIntervalRemainingSec = pRow->skillCastIntervalRemainingSec();
dst.legalCandidateMask = pRow->legalCandidateMask();
dst.illegalCandidateMask = pRow->illegalCandidateMask();
dst.commandSequence = pRow->commandSequence();
dst.executorReason = pRow->executorReason();
dst.executorState = pRow->executorState();
dst.comboStep = pRow->comboStep();
```

### 1-5. `Server/Private/Game/GameRoomCommands.cpp`

`RewindSimulationSeconds`의 정수 seconds truncate block:

```cpp
if (!std::isfinite(cmd.practiceValue) ||
    cmd.practiceValue < 1.f || cmd.practiceValue > 60.f)
```

아래로 교체하고, tick 계산은 seconds 선 truncate가 아니라 곱한 뒤 반올림한다.

```cpp
constexpr f32_t kMinRewindSeconds =
    1.f / static_cast<f32_t>(DeterministicTime::kTicksPerSecond);
if (!std::isfinite(cmd.practiceValue) ||
    cmd.practiceValue < kMinRewindSeconds || cmd.practiceValue > 60.f)
{
    return Finish(false, "rewind-seconds-out-of-range");
}
const u64_t rewindTicks = std::max<u64_t>(
    1ull,
    static_cast<u64_t>(std::llround(
        static_cast<f64_t>(cmd.practiceValue) *
        static_cast<f64_t>(DeterministicTime::kTicksPerSecond))));
```

이는 tick 단위 요청을 허용하지만 restore는 여전히 target 이하의 가장 가까운 30-tick keyframe에 착지한다. exact target까지 journal re-sim을 했다고 주장하지 않는다.

### 1-6. `Client/Private/UI/AIDebugPanel.cpp`

`SnapshotApplier.h`, `ICommandExecutor.h`, `<algorithm>`을 include하고, anonymous namespace의 `s_SelectedAINetId` 아래에 bounded client-only history를 추가한다.

```cpp
struct AIChronoDecisionSample
{
    u64_t serverTick = 0u;
    u64_t timelineEpoch = 0u;
    u64_t branchId = 0u;
    u64_t toolRevision = 0u;
    NetEntityId netId = NULL_NET_ENTITY;
    eChampion champion = eChampion::END;
    eTeam team = eTeam::Neutral;
    ChampionAIDecisionTraceEntry decision{};
};

std::vector<AIChronoDecisionSample> s_ChronoDecisionHistory;
u32_t s_ChronoSampleIntervalTicks = 30u;
u32_t s_ChronoMaxSamples = 180u;
u32_t s_ChronoRewindTicks = 150u;
u64_t s_LastChronoSampleTick = ~0ull;
u64_t s_LastChronoEpoch = ~0ull;
u64_t s_LastChronoBranch = ~0ull;
NetEntityId s_LastChronoNetId = NULL_NET_ENTITY;
int s_SelectedChronoSample = -1;
```

sampling은 `Render`가 호출될 때만, 선택 bot에 최신 decision row가 있고 `(epoch, branch, netId, tick)`이 새로우며 interval을 만족할 때만 1행 append한다. max samples는 16..512, sample interval은 1..300 tick으로 clamp하고 오래된 앞쪽 row를 erase한다. branch가 바뀌면 이전 branch history를 보존하고 restored tick의 새 branch row부터 다시 쌓는다.

F9 `Selected AI`에는 다음 evidence를 표시한다.

```text
Scores: retreat / champion / farm / structure
Why: hard block, legal/illegal candidate mask, HP/threshold, distance/range, turret/threshold
Command: sequence, kind/slot/target, executor Accepted|Rejected, exact executor reason
Skill cadence: configured interval, remaining policy timer
```

`Runtime Tuning`에는 아래 15번째 slider를 추가한다.

```cpp
RenderTuningSlider(
    pScene,
    debug.netId,
    "Skill Cast Min Interval",
    eChampionAITuningId::SkillCastMinInterval,
    debug.fSkillCastMinInterval,
    0.f,
    15.f);
```

`Decision Trace` table은 latest server evidence를 표시하고, 새 `Chrono Decision Timeline` header는 다음을 제공한다.

```text
current server tick / epoch / branch / toolRevision / paused / speed
sample interval ticks [1,300]
max local samples [16,512]
rewind ticks [1,1800] (1/30s..60s)
Rewind 버튼 -> SendPracticeControl(RewindSimulationSeconds, ticks/30.f)
branch-tagged history table + selected row의 scores/why/executor details
```

Rewind는 authoritative network session, 과거 tick, 60초 범위일 때만 enable한다. UI에는 `requested target`과 restore가 `nearest <= target 30-tick checkpoint`임을 함께 적는다.

### 1-7. `Tools/SimLab/main.cpp`

`RunCommandExecutionOutcomeTraceProbe`를 확장해 typed trace와 legacy trace가 같은 sequence만 닫는지, accepted fresh CastSkill만 configured interval을 장전하는지, rejected CastSkill은 timer를 소비하지 않고 comboStep을 submission step으로 복구하는지 검증한다. 기존 Move accept/reject/unmatched/Unknown assertions는 유지한다.

기존 `RunChampionAIStateGateCommitmentProbe`에 low HP인 bot을 `Retreat` state와 `retreatGoal` 도달 위치에 두는 fixture를 추가한다. AI 1 tick이 Move가 아니라 Recall command 하나를 제출하는지, executor가 Accepted 후 `RecallComponent::bActive`가 생기는지 검증한다.

### 1-8. 완료 경계와 30% ceiling

이번 slice는 현재 foundation의 신뢰도를 높이는 30% ceiling 작업으로 제한한다. 완료로 주장하는 것은 latest summary wire, client bounded branch history, executor 연결, Recall bug, tick-unit rewind request까지다.

다음은 후속 `exact Chrono A/B` packet이다: keyframe→arbitrary target journal replay/re-sim, branch-aware WRPL segment 또는 replay format, 선택 bot subscribe/unsubscribe observation op, server keyframe interval/window/capture μs·bytes 계측. 다음은 그 뒤의 `DecisionLedger V2` packet이다: valuation의 각 raw×weight contribution을 단일 계산 경로에서 산출하고 early return까지 decision당 정확히 1행 close. `AiEpisodeV1`과 PyTorch BC/DAgger/PPO 계약은 이번 변경으로 확장하지 않는다.

## 2. 검증

1. Schema/codegen:

```text
Shared\Schemas\run_codegen.bat
```

2. 관련 프로젝트를 의존 순서로 Debug x64 빌드한다. generated header 병렬 race를 피하기 위해 schema codegen 뒤 GameSim→SimLab→Server→Client 순서로 실행한다.

```text
msbuild Shared\GameSim\Include\GameSim.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
msbuild Tools\SimLab\SimLab.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
msbuild Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
msbuild Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
```

3. SimLab 기본 1,800 tick/seed 42를 실행한다. 모든 기존 probe, 새 Recall/accepted interval/rejected no-consume probe, same-seed hash 일치, seed+1 민감도, keyframe restore replay가 PASS여야 한다.

```text
Tools\Bin\Debug\SimLab.exe 1800 42
```

4. Python AI Research 회귀를 실행해 `AiEpisodeV1` golden/validator/baseline 계약이 그대로인지 확인한다.

```text
python -m unittest discover -s Tools\AIResearch\tests -p "test_*.py"
```

5. owned-path whitespace/schema diff와 전체 dirty 보존을 확인한다.

```text
git diff --check -- Shared/GameSim/Components/ChampionAIComponent.h Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp Shared/Schemas/Snapshot.fbs Shared/Schemas/Generated/cpp/Snapshot_generated.h Shared/Schemas/Generated/go/Shared/Schema/AIDebugTraceRow.go Server/Private/Game/GameRoomCommands.cpp Server/Private/Game/SnapshotBuilder.cpp Client/Private/Network/Client/SnapshotApplier.cpp Client/Private/UI/AIDebugPanel.cpp Tools/SimLab/main.cpp .md/plan/2026-07-13_S021_AI_CHRONO_DECISION_TRACE_PLAN.md
```

6. 최종 보고서는 `.md/build/2026-07-13_S021_AI_CHRONO_DECISION_TRACE_REPORT.md`에 실제 명령, PASS/FAIL, hash, 남은 C4251/C4275 경고, exact Chrono/WRPL/DecisionLedger 경계를 기록하고 packet을 `Active -> Handoff`로 바꾼다. 창을 띄우지 않는 자동 검증 범위는 빌드/SimLab까지이며, F9 수동 UX gate는 별도 항목으로 남긴다.
