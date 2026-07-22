Session - 비에고 W·원거리 미니언·F4/F9 사거리 튜닝을 교정하고 애쉬 검은 쿼드와 사일러스 Q/W 권위 로직까지 완결한다.
좌표: 신규 좌표 후보 · 축: C7 권위와 정합성, C8 검증이 병목
관련: `2026-07-18_CHAMPION_AI_AGGRESSION_MID_GROUP_YONE_COMBO_MINION_STUCK_RESULT.md`, `2026-07-18_CHAMPION_AI_LIVE_SAFE_TURRET_RECALL_ANCHOR_RESULT.md`, `2026-07-18_F4_BALANCE_TUNER_DAMAGE_RESPAWN_HUD_RESULT.md`

## 1. 결정 기록

① 문제·제약: W 최대 대시 5.0을 12.5로 바꾸고, F4가 lane role 0..3의 `attackRange`를 편집·즉시 갱신하며, 0.20초 AI cadence 안에서 `FarmMinion`인데 `MoveToSafeAnchor`로 멈추는 상태를 제거한다.
① 문제·제약: 현재 앵커 상태는 적 챔피언 또는 8.0 이내 아군 웨이브만 `LaneCombat`으로 넘겨 적 미니언/80.0 이내 추종 웨이브를 이미 찾고도 정체할 수 있고, `NoTarget`은 챔피언 부재만으로 기록돼 미니언 target과 모순된다.
② 순진한 해법의 실패: `aggression`/`minionScanRange`만 올리면 상태 전이문이 닫힌 채이고, AI JSON 저장을 선택 봇 튜닝으로 쓰면 같은 챔피언 전체가 함께 바뀌므로 요구한 per-bot 실시간 검증이 아니다.
③ 메커니즘: canonical gameplay 수치는 기존 JSON→runtime overlay/cook 경로로, AI 선택 봇 값은 append-only tuning id→typed command→Server/GameSim override→snapshot echo 한 경로로 적용한다.
③ 메커니즘: 아군 웨이브 search와 적 미니언 인지를 앵커→라인 전이 조건으로 승격하고, `NoTarget`은 실제 target 필요 행동 실패에서만 남긴다.
④ 대조: F4는 디스크에 남는 authoring mode, AI Live Tuning은 선택 entity에만 남는 practice overlay mode로 분리한다. 둘을 같은 Save 버튼으로 합치지 않는다.
⑤ 대가: Follow Wave Search를 크게 하면 멀리 있는 웨이브를 따라 전진하며, Farm Priority를 높이면 교전 진입이 늦어진다. 실제 5v5 체감과 화면 캡처는 수동 F5 검증이 필요하다.

## 2. 사용자 작업 계약과 화면

```text
사용자 작업: 디버그 매치의 방장이 봇 하나를 선택해 현재 목표·이동·차단 근거를 읽고, 라인/파밍/위험 수치를 조절해 다음 서버 스냅샷부터 행동 변화를 확인한다.
대상 범위: 서버 스냅샷에 존재하는 모든 ChampionAI 봇 중 선택한 1개.
필수 데이터: state/intent/action, champion/minion/wave target, last move goal/executor, Retreat/Fight/Farm/Structure의 점수순 후보 목록, 후보별 raw×weight=contribution 항, legality/selected, 현재 관측값과 tuning threshold.
핵심 행동: 각 DragFloat를 드래그하거나 더블클릭해 선택 봇에 즉시 적용, Reset Selected Bot으로 JSON profile 값 복원.
제외: Release mutation, AI JSON 영구 저장 UI, ML/shadow policy 편집, navmesh/MoveSystem 전면 개편.
권위/저장: Client drag draft -> typed AIDebugControl -> Debug host Server -> GameSim component override -> snapshot effective-value echo. 파일 저장 없음.
완료 증거: Debug F5/F9에서 첫 bot 자동 선택, 10초 안에 Follow Wave/Farm/Turret 값 하나를 변경·commit, 값 echo와 state/action/last move 변화 캡처.
```

| 카테고리 | 필수 값 | 의미/근거 |
|---|---|---|
| Observe | state·intent·action·scores | 현재 선택 결과 |
| Decision Ranking | Retreat·Fight·Farm·Structure 순위, legal/selected, 항별 raw×weight | 서버가 왜 1등/실제 행동을 골랐는지 |
| Targets & Move | enemy minion, allied wave, wave distance, last move goal, executor | `FarmMinion`과 실제 이동의 모순 진단 |
| Lane | Champion Scan, Minion Scan, Follow Wave Search, Structure Scan, Leash | 관측 후보 범위 |
| Choice | Farm Priority, Champion Margin, Turret Danger Limit | farm score 배율, fight 요구 마진, hard retreat threshold |
| Survival | Retreat HP, Reengage HP, Skill Cast Interval | 퇴각/복귀/명령 cadence |
| F4 Minions | Max Health, Attack Damage, Attack Range | role 0..3 canonical spawn data |

```text
┌─ AI Debug ──────────────────────────────────────────────┐
│ [LeeSin (Blue) ▼]  [Observe] [Live Tuning]             │
│ Observe: State | Intent | Action                        │
│ 1 Farm 0.72 [SELECTED]   2 Fight 0.61 [LEGAL]           │
│   FarmOpportunity 0.60 × 1.20 = +0.72                  │
│   raw 1등과 실제 선택이 다르면 hold/margin/gate 표시    │
│ Targets: minion / allied wave / wave distance           │
│ Move: last goal | executor | actual block reason        │
│ [Decision Trace ▸]                                      │
│                                                        │
│ Live Tuning: [Lane] [Choice] [Survival]                 │
│ Follow Wave Search (m)       80.0  drag/double-click    │
│ Farm Priority (x)             1.0  drag/double-click    │
│ Turret Danger Limit (0..1)    0.85 drag/double-click    │
│ [Test Actions ▸] [Reset Selected Bot]  server snapshot  │
└─────────────────────────────────────────────────────────┘
```

- 첫 bot 자동 선택을 유지하고 `Live Tuning`을 기본 open으로 둔다. Follow Wave/Farm/Turret/Reset은 1920×1080 현재 DPI 첫 viewport 상단에 배치해 F9 진입부터 첫 commit까지 10초 안에 끝나게 한다.
- 기본 mutation은 값 편집 자체의 즉시 live apply 하나다. Secondary는 `Reset Selected Bot` 하나이며 Force 버튼은 접힌 `Test Actions`에 둔다.
- 기존 기본 화면의 `Force` 5버튼과 설명 없는 `Core Tuning` 세로열은 삭제·통합한다. `#if 0` legacy surface는 이번 동작 경로가 아니므로 건드리지 않는다.
- 수동 기준 executable/scene/shortcut은 Debug Client F5/InGame/F9다. 공식 최소 해상도·DPI는 `CONFIRM_NEEDED`; 성공·empty/권한실패 캡처 artifact는 RESULT에 경로를 기록한다.

## 3. 반영해야 하는 코드

### 3.1 `C:/Users/user/Desktop/Winters/Data/Gameplay/ChampionGameData/champions.json`

VIEGO slot 2의 기존 코드를 아래로 교체한다. charge `rangeScale [0.5, 1.0]`은 유지되어 최소/최대가 6.25/12.5가 된다.

```json
"rangeMax": 5.0,
```

```json
"rangeMax": 12.5,
```

### 3.2 `C:/Users/user/Desktop/Winters/Shared/GameSim/Components/ChampionAIComponent.h`

`eChampionAITuningId`의 기존 말미를 append-only로 교체한다.

```cpp
    SkillCastMinInterval,
    Count,
```

```cpp
    SkillCastMinInterval,
    FollowWaveSearchRange,
    FarmPriority,
    Count,
```

`ChampionAITuning` 말미에 아래를 추가한다.

```cpp
    ChampionAITuningParam followWaveSearchRange{ 80.f, 80.f, 10.f, 120.f, false };
    ChampionAITuningParam farmPriority{ 1.f, 1.f, 0.f, 3.f, false };
```

기존 id는 재배치하지 않는다. `SkillCastMinInterval`, `FollowWaveSearchRange`, `FarmPriority`, `Count`의 숫자를 static assert로 고정하고, `0..Count-1` resolver non-null/`Count` null 계약으로 wire `u8 slot` 회귀를 막는다.

같은 header에 mutable/const 단일 resolver를 추가해 Server validation과 CommandExecutor가 같은 id→param 매핑을 소비하게 한다. 기존 두 cpp의 중복 switch는 삭제한다.

```cpp
inline ChampionAITuningParam* ResolveChampionAITuningParam(
    ChampionAITuning& tuning,
    eChampionAITuningId tuningId);
inline const ChampionAITuningParam* ResolveChampionAITuningParam(
    const ChampionAITuning& tuning,
    eChampionAITuningId tuningId);
```

`ChampionAIComponent`의 resolved tuning/evidence에 아래를 추가한다.

```cpp
    f32_t followWaveSearchRange = 80.f;
    f32_t fFarmPriority = 1.f;
    f32_t fDecisionWaveDistance = 999.f;
```

`ChampionAIDebugComponent`에는 snapshot echo를 추가한다.

```cpp
    u32_t enemyMinionNetId = 0u;
    u32_t alliedWaveNetId = 0u;
    f32_t fWaveDistance = 999.f;
    f32_t fWaveSupportRange = 0.f;
    f32_t fFollowWaveSearchRange = 0.f;
    f32_t fFarmPriority = 0.f;
```

구조체 size/offset/static trivially-copyable assert는 실측 layout으로 갱신하되 기존 필드 순서는 바꾸지 않는다. `fDecisionWaveDistance`는 기존 authoritative decision evidence 관례에 맞춰 component에 남긴다. 그 결과 hash/checkpoint 크기가 바뀔 수 있음을 RESULT에 기록하고 keyframe round-trip으로 보상한다.

### 3.3 `C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp`

`ApplyChampionAIProfileAndTuning` 말미에 effective 값을 해석한다.

```cpp
        ai.followWaveSearchRange = ResolveChampionAITuningParam(
            ai.tuning.followWaveSearchRange, 80.f, bOverrideProfile);
        ai.fFarmPriority = ResolveChampionAITuningParam(
            ai.tuning.farmPriority,
            profile.minionPressureWeight * profile.lastHitWeight,
            bOverrideProfile);
```

`FindAlliedLaneMinion` 호출 범위를 아래로 교체한다.

```cpp
            std::max(80.f, ai.structureScanRange + ai.minionScanRange),
```

```cpp
            ai.followWaveSearchRange,
```

`UpdateChampionAIDecisionEvidence`는 current perception만 매 tick 기록한다. 챔피언 부재만으로 `NoTarget`을 쓰는 두 줄은 삭제한다. block reason clear는 실제 새 decision/action attempt가 시작되는 경계에서만 수행해 decision timer로 판단을 건너뛴 tick에도 직전 실패 증거가 유지되게 한다.

```cpp
        ai.targetMinion = ctx.enemyMinion;
        ai.alliedWave = ctx.alliedWave;
        ai.fDecisionWaveDistance = ctx.waveDistance;
```

`BuildChampionAIValueInput`의 farm weight를 아래로 교체한다.

```cpp
        vin.farmUtilityWeight =
            profile.minionPressureWeight * profile.lastHitWeight;
```

```cpp
        vin.farmUtilityWeight = ai.fFarmPriority;
```

`ExecuteMoveToOuterTurret`와 `ExecuteWaitForWave`의 라인 진입 조건을 아래로 교체한다.

```cpp
        if (ctx.bAlliedWaveNearby)
```

```cpp
        if (ctx.enemyMinion != NULL_ENTITY || ctx.alliedWave != NULL_ENTITY)
```

적 챔피언 우선 분기는 그대로 유지한다. 적 미니언이 있으면 `TryExecuteMinionFarm`, 없고 웨이브가 있으면 `TryExecuteFollowWave`로 내려간다.

### 3.4 `C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`

로컬 `ResolveChampionAITuningParamById` switch를 삭제하고 아래 호출로 교체한다.

```cpp
        ChampionAITuningParam* pParam =
            ResolveChampionAITuningParam(ai.tuning, tuningId);
```

### 3.5 `C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomCommands.cpp`

로컬 validation switch를 삭제하고 아래 호출로 교체한다. 이로써 기존 누락된 `SkillCastMinInterval`과 신규 id도 같은 clamp를 사용한다.

```cpp
        const ChampionAITuningParam* pParam =
            ResolveChampionAITuningParam(ai.tuning, tuningId);
```

SpawnObject hot-load의 living minion refresh에 아래를 추가한다.

```cpp
                state.attackRange = combat.attackRange;
```

### 3.6 `C:/Users/user/Desktop/Winters/Shared/Schemas/Snapshot.fbs`

`EntitySnapshot` 말미 `respawnDurationSec` 아래에 append-only로 추가한다.

```fbs
    aiDebugEnemyMinionNet:uint;
    aiDebugAlliedWaveNet:uint;
    aiDebugWaveDistance:float = 999.0;
    aiDebugWaveSupportRange:float;
    aiDebugFollowWaveSearchRange:float;
    aiDebugFarmPriority:float;
```

`Shared/Schemas/run_codegen.bat`으로 C++/Go 생성물을 갱신한다.

추가 사용자 계약은 nested table 4×4를 매 bot/30 Hz 전송하지 않고 compact parallel V1 vectors로 보낸다. `EntitySnapshot` 말미에 `aiDebugCandidateTick`, `aiDebugSelectionTick`, 후보 kind/flags/target/score/termCount 각 vector와 16칸 term featureId/raw/weight/contribution vector를 append-only로 둔다. candidate index와 `candidate*4+term` index가 positional contract다. 이 값은 Client가 공식을 재계산한 값이 아니라 서버의 기존 `ChampionAIResearchDebugComponent::decisionDraft`에서 직렬화한 정확한 valuation evidence다.

### 3.7 `C:/Users/user/Desktop/Winters/Server/Private/Game/SnapshotBuilder.cpp`

`ChampionAIComponent`에서 target net id와 effective 값을 채우고 `CreateEntitySnapshot` append 인수로 전달한다.

```cpp
            aiDebugEnemyMinionNet = ai.targetMinion != NULL_ENTITY
                ? entityMap.ToNet(ai.targetMinion) : NULL_NET_ENTITY;
            aiDebugAlliedWaveNet = ai.alliedWave != NULL_ENTITY
                ? entityMap.ToNet(ai.alliedWave) : NULL_NET_ENTITY;
            aiDebugWaveDistance = ai.fDecisionWaveDistance;
            aiDebugWaveSupportRange = ai.waveJoinRange;
            aiDebugFollowWaveSearchRange = ai.followWaveSearchRange;
            aiDebugFarmPriority = ai.fFarmPriority;
```

snapshot 직전 AI tick에서 갱신된 transient `ChampionAIResearchDebugComponent::decisionDraft`를 사용한다. command 제출 때만 쌓이는 decision ring을 evidence source로 쓰면 equivalent move 재사용 중 stale해질 수 있으므로 쓰지 않는다. `decisionDraft.tick == serverTick`이고 schema/count가 유효할 때만 vectors를 채우며, dead/미실행/stale bot은 candidate vectors와 selection tick을 비운다. current score/legal은 매 tick pre-action evidence이므로 현재 `ai.intent`에는 `ACTIVE/HELD INTENT`만 표시한다. 같은 tick의 최신 authoritative debug trace가 실제 decision/action attempt를 기록했을 때만 `aiDebugSelectionTick = serverTick`과 selected flag를 주입하고 `SELECTED THIS TICK`으로 표시한다. 같은 tick trace/command sequence가 없으면 `no command this tick / held`로 분리한다. Recall/DefendMid처럼 V1 후보 집합 밖의 intent는 거짓 selected를 만들지 않는다. authoritative `ChampionAIComponent`/checkpoint/hash에는 설명 전용 배열을 추가하지 않는다.

compact encoding은 최대 4후보×4term에서 payload field data 약 336 bytes/bot이며 실제 FlatBuffer 측정 gate는 empty entity 대비 10-bot delta 6 KiB 이하로 둔다. 초과하면 완료로 처리하지 않고 Debug 선택-bot 요청 경로 또는 더 압축된 struct로 재설계한다.

### 3.8 `C:/Users/user/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp`

신규 FlatBuffer accessor를 `ChampionAIDebugComponent`에 복사한다. Client는 값을 계산하거나 gameplay truth를 만들지 않는다.

`ChampionAIDebugComponent`의 client-only `AiDecisionTraceV1 utilityDecision`에 후보/항을 복사한다. SnapshotApplier는 적용 시작마다 이전 utility evidence/tick을 먼저 clear하고, candidate tick이 현재 snapshot server tick과 같고 모든 vector 길이/term count가 유효할 때만 채운다. absent/invalid/older vector는 이전 화면 값을 남기지 않는다. selected/legal flag도 서버 값을 그대로 사용한다.

### 3.8.1 새 파일: `C:/Users/user/Desktop/Winters/Client/Public/Network/Client/AIDebugEvidenceDecoder.h`

SnapshotApplier와 SimLab이 동일한 clear-before-parse 코드를 직접 소비하도록 아래 전체 body를 추가한다.

```cpp
#pragma once

#include "Shared/GameSim/Components/ChampionAIComponent.h"
#include "Shared/Schemas/Generated/cpp/Snapshot_generated.h"

namespace Client
{
    inline bool_t DecodeAIDebugUtilityEvidence(
        const Shared::Schema::EntitySnapshot& snapshot,
        u64_t serverTick,
        ChampionAIDebugComponent& outDebug)
    {
        outDebug.utilityCandidateTick = 0u;
        outDebug.utilitySelectionTick = 0u;
        outDebug.utilityDecision = ChampionAIResearch::MakeDecisionTraceV1();

        const auto* pKinds = snapshot.aiDebugCandidateKinds();
        const auto* pFlags = snapshot.aiDebugCandidateFlags();
        const auto* pTargets = snapshot.aiDebugCandidateTargets();
        const auto* pScores = snapshot.aiDebugCandidateScores();
        const auto* pTermCounts = snapshot.aiDebugCandidateTermCounts();
        const auto* pFeatureIds = snapshot.aiDebugCandidateTermFeatureIds();
        const auto* pRawValues = snapshot.aiDebugCandidateTermRawValues();
        const auto* pWeights = snapshot.aiDebugCandidateTermWeights();
        const auto* pContributions = snapshot.aiDebugCandidateTermContributions();
        constexpr u32_t kCandidateCount = kAiDecisionCandidateCapacityV1;
        constexpr u32_t kTermCount =
            kCandidateCount * kAiFeatureContributionCapacityV1;
        if (snapshot.aiDebugCandidateTick() != serverTick ||
            pKinds == nullptr || pKinds->size() != kCandidateCount ||
            pFlags == nullptr || pFlags->size() != kCandidateCount ||
            pTargets == nullptr || pTargets->size() != kCandidateCount ||
            pScores == nullptr || pScores->size() != kCandidateCount ||
            pTermCounts == nullptr || pTermCounts->size() != kCandidateCount ||
            pFeatureIds == nullptr || pFeatureIds->size() != kTermCount ||
            pRawValues == nullptr || pRawValues->size() != kTermCount ||
            pWeights == nullptr || pWeights->size() != kTermCount ||
            pContributions == nullptr || pContributions->size() != kTermCount)
        {
            return false;
        }

        AiDecisionTraceV1& decision = outDebug.utilityDecision;
        decision.tick = serverTick;
        decision.candidateCount = static_cast<u8_t>(kCandidateCount);
        const bool_t bSelectedThisTick =
            snapshot.aiDebugSelectionTick() == serverTick;
        outDebug.utilityCandidateTick = serverTick;
        outDebug.utilitySelectionTick = bSelectedThisTick ? serverTick : 0u;
        for (u32_t candidateIndex = 0u;
            candidateIndex < kCandidateCount;
            ++candidateIndex)
        {
            AiCandidateEvidenceV1& candidate = decision.candidates[candidateIndex];
            candidate.candidateKind = pKinds->Get(candidateIndex);
            candidate.flags = pFlags->Get(candidateIndex);
            if (!bSelectedThisTick)
                candidate.flags &= static_cast<u8_t>(~kAiCandidateSelectedFlagV1);
            candidate.targetNetEntityId = pTargets->Get(candidateIndex);
            candidate.score = pScores->Get(candidateIndex);
            candidate.contributionCount = pTermCounts->Get(candidateIndex);
            if (candidate.contributionCount > kAiFeatureContributionCapacityV1)
            {
                outDebug.utilityCandidateTick = 0u;
                outDebug.utilitySelectionTick = 0u;
                outDebug.utilityDecision = ChampionAIResearch::MakeDecisionTraceV1();
                return false;
            }
            for (u32_t termIndex = 0u;
                termIndex < candidate.contributionCount;
                ++termIndex)
            {
                const u32_t flatIndex = candidateIndex *
                    kAiFeatureContributionCapacityV1 + termIndex;
                AiFeatureContributionV1& term = candidate.contributions[termIndex];
                term.featureId = pFeatureIds->Get(flatIndex);
                term.rawValue = pRawValues->Get(flatIndex);
                term.weight = pWeights->Get(flatIndex);
                term.contribution = pContributions->Get(flatIndex);
            }
            if ((candidate.flags & kAiCandidateSelectedFlagV1) != 0u)
                decision.selectedCandidateKind = candidate.candidateKind;
        }
        return true;
    }
}
```

### 3.9 `C:/Users/user/Desktop/Winters/Client/Private/UI/AIDebugPanel.cpp`

`RenderTuningSlider`를 설명 가능한 DragFloat helper로 교체한다. 선택 bot net id와 tuning id를 key로 하는 client-local draft/last-sent cache를 두며, 서버 snapshot은 해당 control을 편집 중이 아닐 때만 draft를 동기화한다.

```cpp
    void RenderTuningDrag(
        CScene_InGame* pScene,
        u32_t targetNetId,
        const char* pLabel,
        const char* pHelp,
        eChampionAITuningId tuningId,
        f32_t value,
        f32_t speed,
        f32_t minValue,
        f32_t maxValue,
        const char* pFormat);
```

helper는 `ImGui::DragFloat(..., ImGuiSliderFlags_AlwaysClamp)`를 사용한다. 드래그/더블클릭 편집 중에는 local draft만 바꾸고 `ImGui::IsItemDeactivatedAfterEdit()`에서 최종값을 한 번 전송한다. 같은 `(bot, tuning id, value)`는 last-sent 비교로 재전송하지 않는다. 이 방식은 매 frame command/replay mutation과 편집 중 stale snapshot 덮어쓰기를 막는다. label hover tooltip에는 값의 식·높을 때/낮을 때 효과를 쓴다.

전송 뒤 snapshot echo가 `lastSent`와 epsilon 이내로 일치하면 pending을 해제한다. bounded snapshot timeout 안에 echo가 없으면 pending을 취소하고 서버값으로 재동기화하며 해당 control에 rejection/timeout status를 표시한다. Reset은 선택 bot의 모든 draft/pending/last-sent cache를 비우고 다음 snapshot profile 값으로 다시 초기화한다.

active `Render`의 Bot selector 아래를 §2 wireframe으로 교체한다. `Observe`는 mutation 없이 current target/move/block/score를, `Live Tuning`은 Lane/Choice/Survival 값과 접힌 Test Actions/Reset을 렌더한다. 화면에 아래 문구를 둔다.

```text
Server snapshot values. Drag or double-click for an exact value.
Session-only override for the selected bot; Reset or room restart restores its JSON profile.
```

`Observe` 상단에는 서버 후보를 score 내림차순으로 정렬한 Decision Ranking table을 둔다. 각 행은 rank/kind/score/legal/target을 보여주고 펼치면 feature별 `raw × weight = contribution`을 표시한다. selection tick이 current candidate tick과 같을 때만 `SELECTED THIS TICK`, 그 외에는 현재 intent에 `ACTIVE/HELD`를 표시한다. candidate age가 0이 아니면 표를 gray/stale 처리하며 서버가 vectors를 비운 경우 `No current candidate evidence`를 보인다. 단순 raw score 1등과 실제 active/selected가 다르면 Retreat 0.65 hard gate, post-combo, intent hold, Champion Margin, severe HP gate 같은 brain 선택 규칙이 개입할 수 있음을 별도 문구로 명시한다. UI는 점수를 재계산하거나 selected를 추정하지 않는다.

### 3.10 `C:/Users/user/Desktop/Winters/Client/Private/UI/ChampionTuner.cpp`

`ValidateBalanceDraft`와 UI가 공유하는 `kMinionAttackRangeMin = 0.1f`, `kMinionAttackRangeMax = 100.f` 상수를 추가한다. lane role 검증과 Minions 탭 WFX형 drag 편집 경로가 같은 범위를 사용하게 한다.

```cpp
                    EditDragFloat(
                        *pMinion, "attackRange", "Attack Range",
                        0.1f, kMinionAttackRangeMin, kMinionAttackRangeMax,
                        "%.1f", draft.bSpawnObjectDirty);
```

### 3.11 `C:/Users/user/Desktop/Winters/Tools/SimLab/main.cpp`

기존 Champion AI probe에 다음 deterministic fixture를 추가한다.

```text
MoveToOuterTurret + enemy minion only -> LaneCombat/FarmMinion + BA 또는 minion 이동 command
WaitForWave + allied wave 8보다 멀고 FollowWaveSearch 이내 -> LaneCombat/FollowWave + wave target Move
enemy champion 부재 + valid minion/wave 성공 path -> block reason == None
실제 target-required 행동 실패 -> block reason == NoTarget
decision timer로 새 판단을 건너뛴 tick -> 직전 실패 block reason 유지
FarmPriority override -> 동일 입력의 farm score가 profile baseline보다 증가
동일 profile bot A/B -> A에만 Follow/Farm/Turret override, B는 불변
A Reset -> 현재 runtime JSON profile 값 복원
keyframe save/restore -> A override flag와 값 보존
Follow range -> 탐지/행동 변경, Farm priority -> 비포화 score와 선택 변경, Turret limit -> 동일 danger에서 행동 전환
research decision candidate별 contribution 합 == authoritative candidate score
candidate ranking UI는 서버 selected/legal flag와 score를 보존
candidateTick mismatch/absent -> server vector omit, client previous evidence clear, UI stale/empty
FlatBuffer verifier build→read round-trip + contribution epsilon + 10-bot serialized delta <= 6 KiB
```

Viego probe에는 canonical W `rangeMax == 12.5`, full charge release 후 walkable clamp가 없는 fixture의 dash end X≈12.5와 dash 완료 뒤 transform 이동 거리≈12.5, minimum charge 거리≈6.25를 고정한다.

SimLab은 `ChampionAIValuation::BuildUtilityBreakdown`의 각 contribution 합과 score epsilon, 중앙 resolver/per-bot override/reset/keyframe, 그리고 `Client::DecodeAIDebugUtilityEvidence`의 valid decode 후 absent/stale/invalid 입력 clear-before-parse를 직접 검증한다. SimLab이 링크하지 않는 Server SnapshotBuilder나 Client SnapshotApplier 전체 경로를 검증했다고 주장하지 않는다.

### 3.11.1 `C:/Users/user/Desktop/Winters/Tools/Harness/GameRoomBotMatchSoak.cpp`

이미 Server object를 링크하는 harness에 `CSnapshotBuilder` candidate evidence integration probe를 추가한다. 동일한 10-bot world와 동일 serverTick에서 research candidate vectors OFF/ON 두 snapshot을 만들고, 두 buffer 모두 FlatBuffer verifier 통과, ON read-back candidateTick/selectionTick/4×4 contribution 합 통과, stale draft tick snapshot은 vectors omit, `on.GetSize() - off.GetSize() <= 6 KiB`를 단언한다. 기본 EntitySnapshot 비용이 delta에 섞이지 않게 world/roster/fields는 동일하고 research draft 유효성만 전환한다.

### 3.12 `C:/Users/user/Desktop/Winters/Tools/LoLData/Test-F4BalanceContracts.py`

기존 이렐리아/비에고 cooldown·damage 고정 숫자 단언은 제거한다. 이 값들은 F4의 정상 authoring 대상이라 사용자의 유효 변경을 회귀로 오인한다. 대신 canonical champion JSON과 generated `SkillGameplayDefs.json`의 slot/rank/range parity, lane role 0..3 attackRange 존재, F4 Attack Range drag control, hot-load `state.attackRange = combat.attackRange`, AI tuning resolver의 모든 enum id 매핑과 active F9 surface의 Follow Wave/Farm/Turret 설명을 계약으로 추가한다. 이번 Viego W 12.5 결과 자체는 SimLab과 RESULT 실측으로 고정한다.

### 3.13 생성 산출물

`Build-LoLDefinitionPack.py`를 source mode로 실행해 `ChampionGameplayDefs.json`, `SkillGameplayDefs.json`, Server/Shared generated C++와 manifest/parity를 현재 canonical JSON에서 재생성한다. 생성기 산출물은 수동 수정하지 않는다.

## 4. 서브 에이전트 비평

비평 주체: `f4_slider_hotload_critic` (독립 read-only)

- 1차 상태: `재검토 필요`, P0 없음.
- P1-1 수용: block reason을 evidence tick마다 clear하지 않고 champion-only `NoTarget`만 제거한다. 실제 attempt·성공·non-decision persistence를 분리 테스트한다.
- P1-2 수용: per `(bot,id)` local draft/pending/last-sent, edit-deactivation 단일 send, echo/timeout/reset 동기화를 UI 계약에 추가했다.
- P1-3 수용: `!= NoTarget` 약한 단언을 `None` 성공, `NoTarget` 실제 실패, non-decision persistence, 성공 후 clear로 강화했다.
- P1-4 수용: batch codegen 2회 결정성, 생성 accessor 계약, Services Go 테스트, Debug/Release 전체 harness를 검증에 명시했다. generated Go는 Services module 미참조라 별도 compile을 가장하지 않고 flatc 성공·2회 동일성·accessor 존재로 검증한다.
- P1-5 수용: 전체 resolver id, per-bot 격리/reset, keyframe, Follow/Farm/Turret 실제 행동 전환 fixture를 추가했다.
- P1-6 수용: 첫 bot 자동 선택, Live Tuning 기본 open, 핵심 3개+Reset 상단, 10초 timed manual QA를 추가했다.
- P1-7 수용: attack range UI/validation 범위를 공유 상수 0.1..100.0으로 통일했다.
- P2-1 수용: POD size/offset/trivially-copyable와 keyframe round-trip을 검증한다.
- P2-2 수용: tuning id 말미 숫자를 static assert로 고정한다.
- P2-3 수용: wave distance는 기존 decision evidence 관례대로 authoritative component에 두되 hash/checkpoint 비용을 RESULT에 명시한다.
- P2-4 수용: living minion attackRange는 자동화 가능한 integration을 우선하고, 불가능하면 F5 수동 검증을 완료 gate로 남긴다.
- P2-5 수용: Viego dash end뿐 아니라 완료 transform과 minimum charge 6.25도 검증한다.
- 재검토 보완: P1-4의 2회 codegen 결정성은 C++ `Snapshot_generated.h`와 Go `EntitySnapshot.go` SHA-256을 첫/두 번째 실행 뒤 비교하고 차이가 있으면 throw하는 실제 gate로 강화했다.
- 재검토 상태: `PASS` — C++/Go 2회 SHA-256 비교 gate까지 확인됐으며 미해결 P0/P1 없음. 소스 구현 gate 통과.
- 추가 사용자 계약 1차 비평 P1-1 수용: current pre-action score와 held intent를 분리하고 same-tick trace가 있을 때만 `SELECTED THIS TICK`을 표시한다.
- 추가 사용자 계약 1차 비평 P1-2 수용: stale server omit, client clear-before-parse, vector 유효성, age/gray/empty 계약을 추가했다.
- 추가 사용자 계약 1차 비평 P1-3 수용: compact parallel vectors, 실제 FlatBuffer round-trip/epsilon, 10-bot delta 6 KiB ceiling을 추가했다.
- 추가 사용자 계약 1차 비평 P1-4 수용: §3.6/§3.7을 `decisionDraft`로 통일했다. 신규 table/Go 파일을 만들지 않고 EntitySnapshot vector accessors만 생성하므로 기존 C++/Go EntitySnapshot hash 목록을 유지하고 accessor gate를 확장한다.
- 추가 사용자 계약 2차 비평 P1 수용: 검증 소유를 (1) SimLab valuation contribution/epsilon + shared inline client decoder clear-before-parse 직접 테스트, (2) GameRoomBotMatchSoak Server-linked SnapshotBuilder stale omit/Verifier/동일 10-bot off-on buffer delta, (3) Client Debug/Release build로 분리했다.
- 추가 사용자 계약 재검토 상태: `PASS` — shared inline decoder, Server-linked 10-bot OFF/ON snapshot probe, Client build 소유 분리를 확인했으며 미해결 P0/P1 없음.

## 5. 검증 — 예측이 먼저다

예측:
- canonical source와 generated/runtime query에서 Viego W 최대 range는 12.5이며 full charge dash는 장애물이 없으면 12.5, half scale은 6.25다.
- F4 Ranged 선택에서 Attack Range가 drag/double-click으로 편집되고 Save & Hot Load 뒤 living ranged minion `MinionStateComponent.attackRange`도 같은 authoritative tick에 바뀐다.
- 적 미니언만 있는 `MoveToOuterTurret`는 `LaneCombat/FarmMinion`으로, 8보다 먼 아군 웨이브만 있는 `WaitForWave`는 `LaneCombat/FollowWave`로 전환해 Move/BA command를 낸다.
- champion target이 없어도 farm/follow가 가능한 tick은 `NoTarget`을 표시하지 않는다. 실제 target-required 행동 실패만 `NoTarget`을 쓴다.
- 선택 봇 tuning은 다른 봇과 JSON을 바꾸지 않고 snapshot effective value로 echo된다. Reset은 해당 봇을 현 runtime JSON profile로 돌린다.
- Observe는 서버가 산출한 4개 후보를 점수순으로 보이고 selected/legal 및 각 contribution 합이 authoritative score와 일치한다. raw 1등과 actual selected가 다르면 brain gate/hold 개입을 숨기지 않는다.
- Bot AI는 GameCommand 생산자이며 게임플레이 진실을 직접 변경하지 않는다. 이동/공격 적용은 기존 CommandExecutor/Move/Damage 권위 경계를 유지한다.
- AI tuning struct/schema 변경으로 결정론 hash 숫자는 바뀔 수 있으나 same-seed A==B, keyframe restore, generated schema parity는 유지된다.

검증 명령:
```powershell
python Tools/LoLData/Test-F4BalanceContracts.py --root .
python Tools/LoLData/Build-LoLDefinitionPack.py --root .
python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check
powershell -ExecutionPolicy Bypass -File Tools/Harness/Check-SharedBoundary.ps1
cmd /c Shared\Schemas\run_codegen.bat
git diff -- Shared/Schemas/Snapshot.fbs Shared/Schemas/Generated
$schemaGenerated = @(
    'Shared/Schemas/Generated/cpp/Snapshot_generated.h',
    'Shared/Schemas/Generated/go/Shared/Schema/EntitySnapshot.go'
)
$schemaHash1 = $schemaGenerated | ForEach-Object { "$_|$((Get-FileHash -Algorithm SHA256 -LiteralPath $_).Hash)" }
cmd /c Shared\Schemas\run_codegen.bat
$schemaHash2 = $schemaGenerated | ForEach-Object { "$_|$((Get-FileHash -Algorithm SHA256 -LiteralPath $_).Hash)" }
if (Compare-Object -ReferenceObject $schemaHash1 -DifferenceObject $schemaHash2) {
    throw 'Schema codegen is not deterministic across consecutive runs.'
}
rg -n "AiDebugEnemyMinionNet|AiDebugFollowWaveSearchRange|AiDebugFarmPriority|AiDebugCandidateTick|AiDebugCandidateKinds|AiDebugCandidateTermContributions" Shared/Schemas/Generated
powershell -ExecutionPolicy Bypass -File Tools/Harness/Run-BotAiValidation.ps1 -Configuration Debug
powershell -ExecutionPolicy Bypass -File Tools/Harness/Run-BotAiValidation.ps1 -Configuration Release
& 'Tools/Bin/Debug/SimLab.exe' 600 1234
powershell -ExecutionPolicy Bypass -File Tools/Harness/RunGameRoomBotMatchSoak.ps1 -TickCount 1800 -Seed 42 -Runs 1 -Configuration Debug
Push-Location Services; go test ./...; Pop-Location
git diff --check
```

`Run-BotAiValidation.ps1`이 GameSim/Server/Client/SimLab을 포함하므로 Debug와 Release를 모두 실행한다. 모든 MSBuild는 harness 내부 포함 `/m:1` 순차 빌드다.

미검증:
- 계획 시점에는 실제 F5/F9 화면의 잘림·HUD 겹침·드래그 체감과 LeeSin 실맵 이동을 확인하지 않았다.
- 실제 map/nav obstruction에서 FollowWave 목적지까지의 path 품질은 MoveSystem 소유이며 이번 fixture의 flat walkable과 별도다.
- 계획 전 baseline에서 F4 계약은 사용자가 편집한 Irelia Q 5.0을 stale expected 10.0으로 오인했고, definition pack `--check`는 현재 canonical F4 편집이 아직 cook되지 않아 6개 산출물을 STALE로 보고했다. 구현 후 parity 기반 계약과 source generation으로 닫는다.

확인 필요:
- Debug F5/F9 1920x1080 현재 DPI에서 Observe/Live Tuning, no-bot empty, Release/read-only 상태 캡처.
- LeeSin에서 Minion Scan/Follow Wave Search/Farm Priority/Turret Limit 변경 후 server snapshot 값과 state/action/last move가 같은 선택 bot에만 바뀌는지 수동 캡처.
- RESULT artifact 이름: `.md/build/2026-07-19_AI_DEBUG_OBSERVE.png`, `.md/build/2026-07-19_AI_DEBUG_LIVE_TUNING.png`, `.md/build/2026-07-19_AI_DEBUG_EMPTY_OR_READONLY.png`.

## 6. 2026-07-19 보정 재개 — 범위와 성공 조건

이번 재개는 완료된 AI 판단/튜닝 경로를 다시 설계하지 않는다. 실제 화면에서 확인된 다섯 회귀만 같은 PLAN/RESULT 쌍에 이어서 닫는다.

이 절부터의 §6~§10은 같은 문서 앞부분의 Viego W `12.5/6.25` 목표와 그 고정값 검증을 명시적으로 폐기하고 대체한다. 앞부분 값은 문제를 만든 과거 구현 기준으로만 보존하며, 이번 구현·테스트·RESULT의 유효 목표는 `rangeMax=5.0`, 반충전 `2.5`다.

① 문제·제약: F9 `RenderTuningDrag`가 입력 폭을 `-1`로 잡아 label을 창 밖에 그린다. Viego W `rangeMax=12.5`는 최소/최대 대시를 6.25/12.5로 만들고 피해·기절 선분 및 AI 사거리까지 함께 확대한다.
① 문제·제약: Ranged minion authored attack range 8.0은 요청한 0.7배가 아니며, Ashe Q diffuse alpha quad는 불투명한 어두운 텍스처를 AlphaBlend 지면 평면으로 붙인다.
① 문제·제약: Sylas Q/W canonical 피해는 모두 0이고 Q/W gameplay hook이 없다. Q는 ground target이라 fallback도 못 타고, W fallback은 canonical 0 공식으로 다시 덮인다.
② 순진한 해법의 실패: 창 폭만 키우면 DPI/리사이즈에서 다시 잘린다. 모든 radius/width/acquireRange를 하나의 range로 합치면 서로 다른 형상을 훼손한다. WFX `start_delay`만 늘리면 서버 피해와 화면 폭발이 다른 시점이 된다.
③ 메커니즘: F9는 label/value 2열, F4는 기존 canonical `skills[].rangeMax`만 노출한다. Q는 정수 detonate tick에 서버 범위 피해와 stage-2 cue를 함께 내고, W는 post-mitigation 실제 HP 감소량에 data ratio를 곱해 회복한다.
④ 대조: Viego W는 검증된 원래 authored 5.0으로 복원한 뒤 F4에서 조절한다. 임의의 중간값을 새 기본값으로 만들지 않는다. Q는 요청한 지연 폭발 한 번만 구현하며 첫 사슬 피해를 추가로 발명하지 않는다.
⑤ 대가: F4의 `Skill Range`는 cast/dash 최대 거리 하나만 편집한다. 원형 반경·직사각형 폭·탐색 거리까지 같은 필드로 통합하는 전면 geometry 리팩터는 이번 범위 밖이다.

완료 기준:

- F9 최소 폭 560/default 700에서 모든 튜닝 label이 완전히 보이고 기존 drag/double-click/echo가 유지된다.
- Viego W canonical max는 5.0, min charge는 2.5이며 F4 `Skill Range (m)` 저장·hot load가 다음 cast와 AI query에 반영된다.
- Ranged minion canonical `attackRange`는 `8.0 * 0.7 = 5.6`이고 living refresh 경로를 유지한다.
- Ashe Q 뒤 검은 지면 quad가 사라지고 additive glow/Q mesh는 남는다. `w_hit.wfx`의 실수로 0이 된 폭/높이는 1.2로 복원한다.
- Sylas Q는 0.5초(30 Hz에서 15 tick) 뒤 반경 1.65 안에만 Magic 피해를 주며 같은 tick에 폭발 cue를 낸다. W는 실제 HP 피해의 50%를 최대 HP까지 회복한다.

### 6.1 사용자 작업 계약·화면 범위·행동 예산

| 표면 | 필수 데이터/행동 | 범위와 의미 | mutation owner |
|---|---|---|---|
| F9 Live Tuning | label + authoritative value, drag/double-click | 기존 tuning min/max 그대로 | Debug Server per-bot override |
| F4 Skills | `Skill Range (m)` | 0..500, canonical `rangeMax`; 0은 Self 허용 | champion JSON + authoritative reload |
| F4 Minions | Ranged Attack Range | authored 5.6; 실제 center threshold는 양쪽 gameplay radius가 더해짐 | spawn JSON + living refresh |
| Sylas Q | delay 0.5, radius 1.65, damage 70/95/120/145/170 | 원형 중심 판정, main decal 3.3×3.3 | Server/GameSim + canonical effect JSON |
| Sylas W | damage 75/100/125/150/175, heal ratio 0.50 | post-mitigation `finalAmount × ratio` | DamageQueue callback + canonical effect JSON |

```text
┌─ AI Debug / Live Tuning (responsive) ──────────────────┐
│ Follow Wave Search (m) │ [            80.0            ] │
│ Farm Priority (x)      │ [             1.00           ] │
│ Turret Danger          │ [             0.85           ] │
│                        │ waiting / timeout status       │
└─────────────────────────────────────────────────────────┘

┌─ F4 / Skills ───────────────────────────────────────────┐
│ Champion [VIEGO ▼]  Skill [W ▼]                         │
│ Skill Range (m)        [ 5.0 ]  drag / double-click     │
│ [ranked cooldown and damage table]                      │
│ [Save & Hot Load] authoritative JSON reload             │
└─────────────────────────────────────────────────────────┘
```

- F9는 control당 primary action 1개(값 편집), secondary action은 기존 Reset뿐이다. label 자체는 mutation하지 않는다.
- F4는 기존 `Save & Hot Load` 하나만 commit action으로 유지한다. range 전용 Save나 두 번째 data owner를 만들지 않는다.
- 70% 바닥 예산은 구현·결정론·Debug/Release 검증, 30% 천장 예산은 실제 5v5 Debug F5에서 Viego/F4·Sylas Q/W·Ashe Q를 한 영상으로 촬영하는 데 고정한다. 다음 신규 감사 전에 2026-07-19 안에 이 캡처를 공개 가능한 데모 조각으로 환전한다.

## 7. 보정 구현 명세

### 7.1 `Client/Private/UI/AIDebugPanel.cpp`

`RenderTuningDrag`의 `ImGui::BeginDisabled(!bCanSend);`부터 대응하는 `ImGui::EndDisabled();`까지를 아래 완전한 block으로 교체한다.

```cpp
bool_t bCommit = false;
ImGui::PushID(static_cast<int>(targetNetId));
ImGui::PushID(static_cast<int>(tuningId));
ImGui::BeginDisabled(!bCanSend);
if (ImGui::BeginTable(
        "##TuningRow", 2,
        ImGuiTableFlags_SizingStretchProp |
            ImGuiTableFlags_NoSavedSettings))
{
    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch, 0.42f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.58f);
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(pLabel);
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-1.f);
    if (ImGui::DragFloat(
        "##Value",
        &state.fDraft,
        dragSpeed,
        minValue,
        maxValue,
        pFormat,
        ImGuiSliderFlags_AlwaysClamp) && std::isfinite(state.fDraft))
    {
        state.bDirty = true;
        state.bEchoTimedOut = false;
    }
    bCommit = ImGui::IsItemDeactivatedAfterEdit();
    if (ImGui::IsItemHovered() && pHelp)
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(430.f);
        ImGui::TextUnformatted(pHelp);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
    ImGui::EndTable();
}
ImGui::EndDisabled();
ImGui::PopID();
ImGui::PopID();

if (bCommit && state.bDirty &&
    (!state.bHasLastSent ||
        std::fabs(state.fDraft - state.fLastSent) > epsilon))
{
    pScene->GetCommandSerializer()->SendAIDebugTune(
        *pScene->GetNetworkView(), targetNetId,
        static_cast<u8_t>(tuningId), state.fDraft);
    state.fLastSent = state.fDraft;
    state.bHasLastSent = true;
    state.bPending = true;
    state.bDirty = false;
    state.sentAtSec = ImGui::GetTime();
}
else if (bCommit)
{
    state.bDirty = false;
}
```

pending/timeout 문구는 이 교체 block 아래의 기존 코드를 그대로 사용해 table 전체 폭 아래에 유지한다. `bCommit`은 disabled item에서는 생기지 않으므로 server send 권한 계약도 유지된다.

### 7.2 `Client/Private/UI/ChampionTuner.cpp`

선택한 skill의 ranked table 앞에 아래 canonical scalar editor를 추가하고 `ValidateBalanceDraft`의 각 skill에 같은 0..500 검증을 추가한다.

```cpp
EditDragFloat(
    *pSkill, "rangeMax", "Skill Range (m)",
    0.1f, 0.f, 500.f, "%.1f m", draft.bChampionDirty);
```

```cpp
if (!ValidateNumber(
        skill, "rangeMax", 0.f, 500.f, outError,
        name + "." + SkillSlotToken(slot)))
{
    return false;
}
```

이는 모든 17 champion Q/W/E/R의 `rangeMax`를 노출하지만 `Radius`, `RectLength`, `RectWidth`, `AcquireRange`는 합치지 않는다.

### 7.3 canonical range data와 test

`Data/Gameplay/ChampionGameData/champions.json`의 VIEGO W를 `12.5 -> 5.0`, `Data/LoL/ServerPrivate/Gameplay/SpawnObjectGameplayDefs.json`의 roleType 1 attackRange를 `8.0 -> 5.6`으로 교체한다.

`Tools/SimLab/main.cpp`의 Viego charge probe는 12.5/6.25를 새 고정 숫자 5.0/2.5로 바꾸지 않는다. 현재 pack의 `rangeMax`, `minRangeScale`, `maxRangeScale`에서 expected min/max/end를 계산해 F4 authoring을 회귀로 오인하지 않게 한다. `Tools/LoLData/Test-F4BalanceContracts.py`는 F4 range control/validation, canonical-generated parity, Ranged 5.6을 검사한다.

### 7.4 `Client/Private/GameObject/Champion/Ashe/Ashe_FxPresets.cpp`와 Ashe WFX

`SpawnQBuffActive`의 `kPathQDiffuseTex`, `bBillboard=false`, `AlphaBlend`인 두 번째 block만 삭제한다. 첫 additive `kPathQBuffTex` block과 `SpawnQBuffMesh`는 유지한다.

`Data/LoL/FX/Champions/Ashe/w_hit.wfx`의 현재 값만 아래로 교체하며 그 밖의 사용자가 편집한 WFX 필드는 보존한다.

```json
"width": 1.2000,
"height": 1.2000,
```

### 7.5 Sylas canonical effect data와 append-only param

`SkillEffectGameplayDefs.json`의 Q/W zero Physical formula를 아래 의미로 교체한다. 생성 projection과 generated C++는 생성기로만 갱신한다.

```json
{
  "key": "skill.sylas.q",
  "params": { "formationDelaySec": 0.5, "radius": 1.65 },
  "damage": {
    "type": "Magic",
    "flatByRank": [70.0, 95.0, 120.0, 145.0, 170.0]
  }
}
```

```json
{
  "key": "skill.sylas.w",
  "params": { "healDamageRatio": 0.5 },
  "damage": {
    "type": "Magic",
    "flatByRank": [75.0, 100.0, 125.0, 150.0, 175.0]
  }
}
```

나머지 damage ratio 배열은 기존 schema대로 0 다섯 개를 유지한다. `eSkillEffectParamId` 말미에 `HealDamageRatio`를 append해 기존 enum 숫자를 이동시키지 않고 아래 1:1 owner를 함께 갱신한다.

```cpp
DamageFlatOverride,
HealDamageRatio,
```

- `Tools/LoLData/Build-LoLDefinitionPack.py`: `"healDamageRatio": "HealDamageRatio"`
- `Server/Private/Data/RuntimeGameplayDefinitionOverlay.cpp`: token→enum map
- `Client/Private/UI/ChampionTuner.cpp`: practice param option 및 present scalar authoring 표시
- `Server/Private/Game/GameRoomCommands.cpp`: `IsValidPracticeEffectParam`의 enum 말미 상한을 `HealDamageRatio`로 갱신하고 ratio 값은 0..5로 검증

### 7.6 Sylas pending Q, W damage, post-damage heal

`Shared/GameSim/Components/SylasSimComponent.h`에 아래 trivially-copyable pending component를 추가하고 `WorldKeyframe.cpp`에 등록한다.

```cpp
struct SylasQExplosionComponent
{
    EntityHandle hCaster = NULL_ENTITY_HANDLE;
    Vec3 vCenter{};
    u64_t uDetonateTick = 0u;
    u8_t uRank = 1u;
};
```

`SylasGameSim.cpp`는 Q/W hooks를 등록한다. `OnQ`는 `SylasQExplosionComponent`를 caster entity 자체에 add/replace하고 command ground point·rank와 `castTick + ceil(formationDelaySec * 30)`을 저장한다. `Tick`은 component가 있는 caster를 entity id 순으로 처리한다. due component는 먼저 local copy한 뒤 world에서 제거하여 재진입/다음 tick 재폭발을 막고, handle이 같은 살아 있는 caster인지 확인한다. 그 뒤 target을 `DeterministicEntityIterator<HealthComponent>::CollectSorted` 순서로 순회해 반경 1.65 안의 적만 canonical Q `DamageRequest`로 enqueue하고 같은 tick/position에 Q cast-frame id, stage 2, target null인 `EffectTrigger`를 정확히 한 번 emit한다.

`OnW`는 유효 unit target에 `GameplayDefinitionQuery::BuildSkillDamageRequest(... SYLAS, W ...)`를 사용한다. 새 대시나 고정 회복은 추가하지 않는다.

`SylasGameSim.h`에 아래 callback을 선언하고 `DamageQueueSystem.cpp`에서 `ApplyDamageRequest` 직후 기존 Fiora/Irelia/Yone callback 옆에 호출한다.

```cpp
struct DamageRequest;
struct DamageResult;

void OnDamageResolved(
    CWorld& world,
    const TickContext& tc,
    const DamageRequest& request,
    const DamageResult& result);
```

callback은 `request.skillId`의 champion/slot이 SYLAS/W이고 `result.finalAmount > 0`일 때만 `HealDamageRatio`를 resolve한다. source `HealthComponent.fCurrent`를 `min(fMaximum, current + finalAmount * ratio)`로 갱신하고 `ChampionComponent.hp`를 mirror한다. shield/reduction/health-floor 뒤 실제 감소량이므로 0 damage는 0 heal이다.

### 7.7 Sylas visual stage와 footprint

`Client/Private/GameObject/Champion/Sylas/SylasSkills.cpp`의 Q hook을 아래로 교체한다.

```cpp
void OnQCastFrame(VisualHookContext& ctx)
{
    if (ctx.skillStage >= 2u)
    {
        if (ctx.bAuthoritativeEvent)
            PlaySylasCueAt(ctx, "Sylas.Q.Explosion", ResolveEffectPosition(ctx), false);
        return;
    }
    PlaySylasCue(ctx, "Sylas.Q.Cast", true);
}
```

`EmitSylasEffect`의 non-R=BA mapping은 slot별 Q/W/E/R cast-frame variant switch로 교체한다. `EventApplier::ShouldKeepEffectEventPosition`에는 Sylas Q를 추가해 delayed ground event가 caster 위치로 대체되지 않게 한다.

`Data/LoL/FX/Champions/Sylas/q_explosion.wfx`의 main `q_explosion_dark_crack` width/height를 3.3/3.3으로 맞춘다. 이는 server radius 1.65의 지름이다. flash/electric accent는 독립 시각 레이어로 유지하고 WFX 전체 `start_delay`에는 0.5를 중복 적용하지 않는다.

### 7.8 Fiora W 현재 구조와 단일 owner 교정

현재 Fiora W는 네 값이 분리돼 있다.

| 현재 owner | 값 | 실제 사용 | 문제 |
|---|---:|---|---|
| `champions.json skills[W].rangeMax` | 0.0 | cast UI/일부 query | W hit 판정은 읽지 않으며 그대로 전환하면 사거리 0이 됨 |
| `skill.fiora.w.params.range` | 6.0 | `state.riposteRange` | rangeMax와 중복 |
| `skill.fiora.w.params.radius` | 0.8 | 최대 거리 보정에만 사용 | 횡폭 판정이 아님 |
| W damage formula | 0 | DamageQueue formula | `skillId=0`으로 우회해 고정 80 |
| `w_cast.wfx` | 6.8×1.9, Z=3.4 | active indicator | anchor offset이 방향 회전 없이 world +Z로 적용 |
| `w_release.wfx` | length 6.0, width 1.0 | release beam | server/WFX active와 불일치 |

`ResolveCommandOrFacingDirection`은 마우스에서 온 `GameCommand.direction`을 정상 보존하지만 `OnW`는 Q와 달리 caster yaw를 갱신하지 않는다. `FindEnemyInCone`은 `dot >= 0.5`라 WFX 직사각형이 아닌 약 120도 cone이다. 따라서 현재 구현은 “피오라가 마우스 방향으로 돌아 그 WFX 크기만큼 공격”하는 구조가 아니다.

교정 후 owner는 다음 하나씩이다.

- 길이: `champions.json W.rangeMax`를 `0.0 -> 6.5`로 명시 변경하고 F4 `Skill Range (m)`에서 편집한다. 생성 pack도 다시 만들어 canonical/generated parity를 검사한다.
- 반폭: `skill.fiora.w.params.radius` 0.8, F4 present mechanics scalar.
- 피해: `skill.fiora.w.damage.flatByRank`, 기본 동작 보존을 위해 `[80,80,80,80,80]` Physical.
- 쿨타임: 기존 `cooldownSecByRank`, F4 ranked row.
- 표현: WFX는 presentation template이며 서버 effect event의 현재 length/half-width로 size를 override한다.

### 7.9 Fiora W 서버 방향·직사각형 판정·F4 damage

`FioraGameSim.cpp`의 W는 canonical `rangeMax`를 먼저 `0.0 -> 6.5`로 authoring한 뒤 `ResolveSkillRange(... FIORA, W)`를 사용하고 effect param `range`는 삭제한다. 방향 저장 직후 Q와 동일한 `ResolveChampionVisualYawNear`로 caster transform을 회전한다.

W release 전용 판정은 기존 cone을 건드리지 않고 아래 oriented strip helper를 새로 사용한다. Q의 기존 cone 동작은 유지한다.

```cpp
EntityID FindEnemyInDirectionalStrip(
    CWorld& world,
    EntityID caster,
    eTeam casterTeam,
    const Vec3& origin,
    const Vec3& direction,
    f32_t length,
    f32_t halfWidth)
{
    const Vec3 forward = WintersMath::NormalizeXZ(
        direction, Vec3{ 0.f, 0.f, 1.f });
    const Vec3 right{ forward.z, 0.f, -forward.x };
    EntityID best = NULL_ENTITY;
    f32_t bestDistanceSq = (std::numeric_limits<f32_t>::max)();
    for (EntityID entity :
        DeterministicEntityIterator<ChampionComponent>::CollectSorted(world))
    {
        if (entity == caster ||
            !world.IsAlive(entity) ||
            !world.HasComponent<TransformComponent>(entity) ||
            !world.HasComponent<HealthComponent>(entity))
        {
            continue;
        }
        const ChampionComponent& champion =
            world.GetComponent<ChampionComponent>(entity);
        const HealthComponent& health =
            world.GetComponent<HealthComponent>(entity);
        if (champion.team == casterTeam ||
            health.bIsDead || health.fCurrent <= 0.f ||
            !GameplayStateQuery::CanBeTargetedBy(world, caster, entity) ||
            !GameplayStateQuery::CanReceiveDamage(world, caster, entity))
        {
            continue;
        }
        const Vec3 enemyPos = ResolveEntityPosition(world, entity);
        const Vec3 delta{
            enemyPos.x - origin.x, 0.f, enemyPos.z - origin.z };
        const f32_t forwardDistance = delta.x * forward.x + delta.z * forward.z;
        const f32_t lateralDistance = std::fabs(
            delta.x * right.x + delta.z * right.z);
        if (forwardDistance < 0.f || forwardDistance > length ||
            lateralDistance > halfWidth)
        {
            continue;
        }
        const f32_t distanceSq = delta.x * delta.x + delta.z * delta.z;
        if (distanceSq < bestDistanceSq)
        {
            bestDistanceSq = distanceSq;
            best = entity;
        }
    }
    return best;
}
```

`EnqueuePhysicalDamage`의 W `skillId=0` 예외를 삭제해 모든 skill이 champion/slot skillId를 갖게 한다. `SkillEffectGameplayDefs.json`의 Fiora W formula를 Physical `[80,80,80,80,80]`로 authoring하고 `ChampionTuner.cpp`의 `kCustomFlatSkills`에서 `skill.fiora.w`를 제거한다. 그러면 F4 `Flat Damage`가 canonical formula를 편집하고 DamageQueue가 같은 값을 사용한다.

F4의 present mechanics scalar section은 실제 params가 존재할 때만 아래를 노출한다.

```text
radius             -> Effect Radius / Half Width (m), 0..100
formationDelaySec  -> Delay (sec), 0..10
healDamageRatio    -> Heal / Damage Ratio, 0..5
```

따라서 Fiora W half-width, Sylas Q radius/delay, Sylas W heal ratio가 기존 canonical params를 저장·hot load한다.

### 7.10 authoritative effect geometry와 Fiora WFX

F4 hot load 뒤 gameplay만 바뀌고 WFX가 옛 크기로 남지 않도록 `EffectTriggerEvent` 말미에 append-only geometry 두 필드를 추가한다.

```fbs
table EffectTriggerEvent {
    // 기존 필드 유지
    effectLength:float;
    effectHalfWidth:float;
}
```

`ReplicatedEventComponent`에는 같은 의미의 `effectLength`, `effectHalfWidth`를, `VisualHookContext`에는 read-only `fEffectLength`, `fEffectHalfWidth`를 추가한다. `ReplicatedEventSerializer`가 두 값을 serialize하고 `EventApplier`가 visual context로 복사한다. 기존 effect는 기본 0이라 동작이 변하지 않는다.

`EmitFioraEffect`에는 default 0인 두 인자를 말미에 추가한다. W cast/parry-success/recovery만 현재 `state.riposteRange`와 `state.riposteRadius`를 싣고 `position`은 아래 endpoint로 보낸다.

```cpp
const Vec3 endpoint{
    origin.x + state.riposteDirection.x * state.riposteRange,
    origin.y,
    origin.z + state.riposteDirection.z * state.riposteRange
};
```

CommandExecutor가 함께 보내는 generic W cue는 geometry가 0이다. Fiora W visual hook은 authoritative event 중 `fEffectLength <= 0`인 generic duplicate를 무시하고 custom authoritative cue 하나만 사용한다.

`SpawnWParryActive/Success`는 glow/flash를 caster에 붙인 채, 생성된 `GroundDecal`만 world anchor로 전환해 `caster + direction * length/2`에 놓고 width=`length`, height=`halfWidth*2`로 설정한다. GroundDecal의 길이 축은 local X이므로 두 cue 모두 `fYaw = YawFromDirectionXZ(direction) - π/2`로 통일한다. 이는 cast cue의 authored `-90°`와 같은 결과이며, 별도 yaw가 없던 parry-success cue도 같은 축으로 맞춘다. 이로써 entity anchor의 world-axis offset 문제와 90도 축 어긋남을 함께 피한다. `SpawnWRelease`는 attached beam이 아니라 caster의 현재 위치부터 event endpoint까지 `bOverrideEndWorldPos` world segment로 재생한다.

authored WFX fallback도 같은 기본값으로 맞춘다.

```json
// w_cast.wfx / w_cc_indicator_yellow
"width": 6.5000,
"height": 1.6000,
"attach_offset": [0.0000, 0.0500, 3.2500]
```

```json
// w_release.wfx / w_sword_sharpflash_blue
"width": 1.6000,
"end_offset": [0.0000, 1.0500, 6.5000]
```

W의 시간 순서는 유지한다: cast에서 방향 고정·0.75초 parry active, timer 종료에 같은 방향 strip 판정·release beam·피해, hard CC를 막았으면 hit target stun, 아니면 slow다. “즉시 찌르기”로 바꾸지 않는다.

Fiora SimLab fixture는 다음을 고정한다.

- mouse +X command 후 caster yaw가 +X를 향함.
- cast cue geometry 6.5/0.8과 endpoint가 +X 6.5.
- release 전 피해 0, timer 종료 tick에 끝 모서리 내부 `(x=6.4,z=0.79)` hit, 횡폭 외부 `(6.4,0.81)` miss, 정확한 endpoint `(6.5,0)` hit, 뒤쪽 target miss.
- F4-style runtime definition range/radius/damage 변경 후 다음 W의 hit strip, event geometry, final damage가 함께 변경.
- EffectTrigger FlatBuffer old-default 0과 Fiora nonzero geometry round-trip.

## 8. 보정 비평 게이트

초안 뒤 독립 read-only sub-agent가 다음을 확인한다.

- F9 table이 drag item의 commit/tooltip 순서를 깨지 않는지.
- generic F4 range가 실제 JSON owner와 runtime query 한 경로만 쓰는지.
- Q pending state가 integer tick, deterministic iteration, keyframe 복원을 지키는지.
- Q visual과 damage가 같은 authoritative tick인지, W heal이 pre-mitigation이 아닌 `finalAmount` 기준인지.
- 기존 dirty WFX/데이터를 요청 범위 밖에서 덮어쓰지 않는지.

초기 비평 disposition:

- `P1 Viego 목표 충돌` 수용: §6 첫 문단에서 앞부분 `12.5/6.25`를 역사적 기준으로 한정하고 §6~§10의 `5.0/2.5`가 대체함을 명시했다.
- `P1 Sylas Q pending 소유/제거 순서` 수용: caster entity의 add/replace, due 시 local copy 후 선제 제거, 동일 handle·alive 검사를 §7.6에 고정했다.
- `P1 practice param 상한 누락` 수용: `GameRoomCommands.cpp`의 enum 상한과 0..5 검증을 §7.5에 추가했다.
- `P1 HealthComponent 필드명` 수용: 실제 필드 `fMaximum`으로 교정했다.
- `P1 build 경로` 수용: 실제 `Server/Include/Server.vcxproj`, `Client/Include/Client.vcxproj`로 교정했다.
- `P1 header 전방 선언` 수용: `DamageRequest`, `DamageResult` 전방 선언을 §7.6의 선언 block에 포함했다.
- `P1 계획 code block 불완전` 수용: F9 교체 block을 완전한 실행 코드로 확장하고 Fiora strip의 guard를 정확히 적었다. Sylas/Fiora의 나머지 변경은 아래 §8.1의 파일별 anchor 계약으로 보강한다.

Fiora 확장 뒤 2차 비평 disposition:

- `P1 Fiora range owner 실제값 0` 수용: canonical FIORA W `rangeMax 0.0 -> 6.5` 변경과 generated parity를 §7.8/§8.1에 명시했다.
- `P1 strip 끝 모서리 누락` 수용: 원형 제한이 되던 `length*length` 초기값을 `numeric_limits::max()`로 바꾸고 `(6.4,0.79)`와 endpoint hit fixture를 추가했다.
- `P1 WFX 90도 축/호출 경로` 수용: authored `-90°`가 포함된 post-spawn `fYaw`를 보존하고 preset header, Fiora skills, F4 tuner 인자·호출 변경을 §7.10/§8.1에 추가했다.
- `P1 source anchor 오류` 수용: 실제 Checkpoint 경로, stable registration name, `RegisterHooks()`, static-assert 비존재를 반영했다.
- `P1 schema codegen 누락` 수용: build 전 `run_codegen.bat`를 §9 자동 gate에 추가했다.
- `P1 source-ready 필수 파일 누락` 수용: Sylas header/visual/WFX, canonical Fiora·Sylas effect JSON, Fiora cast/release WFX에 실제 symbol/emitter/key anchor와 정확한 추가·교체·삭제 값을 §8.1에 보강했다.

최종 독립 재비평: `P0 없음 / P1 없음 / GO`. 아래 source edit를 시작할 수 있다.

### 8.1 파일별 source-ready anchor 계약

아래는 앞 절의 의미 명세를 실제 편집 위치로 제한한다. 새 파일은 만들지 않는다.

- `Shared/GameSim/Components/SylasSimComponent.h`: `SylasDashComponent` 아래에 §7.6의 `SylasQExplosionComponent`를 추가한다. 이 header에는 기존 static-assert 구역이 없으므로 새 assert/include는 만들지 않고, trivially-copyable 계약은 SimLab의 keyframe round-trip으로 검증한다.
- `Shared/GameSim/Champions/Sylas/SylasGameSim.h`: `struct TickContext;` 아래에 `struct DamageRequest;`, `struct DamageResult;`를 추가한다. namespace 말미의 `ApplyChainHit(...)` 선언 아래에 §7.6의 `OnDamageResolved(...)` 선언을 추가한다.
- `Shared/GameSim/Core/Checkpoint/WorldKeyframe.cpp`: `reg.Register<SylasSimComponent>("SylasSimComponent");` 바로 다음에 `reg.Register<SylasQExplosionComponent>("SylasQExplosionComponent");`를 추가한다.
- `Shared/GameSim/Champions/Sylas/SylasGameSim.cpp`: `EmitSylasEffect`의 hook variant 선택식을 slot별 `Q_CastFrame/W_CastFrame/E_CastFrame/R_CastFrame` switch로 교체한다. 실제 `RegisterHooks()`의 E/R 등록 바로 위에 Q/W 등록을 추가한다. 기존 `Tick`의 passive/dash 처리 전에 due Q explosion processor를 호출한다.
- `Client/Private/GameObject/Champion/Sylas/SylasSkills.cpp`: 기존 `OnQCastFrame` 전체를 §7.7의 stage 분기 함수로 교체한다. stage 1은 `Sylas.Q.Cast`만, authoritative stage 2는 `Sylas.Q.Explosion`만 재생한다.
- `Data/LoL/FX/Champions/Sylas/q_explosion.wfx`: emitter name `q_explosion_dark_crack` 안의 `"width": 4.9000`, `"height": 2.2000` 두 줄만 각각 `3.3000`으로 교체한다. 나머지 flash/electric emitter는 보존한다.
- `Shared/GameSim/Systems/Damage/DamageQueueSystem.cpp`: `ApplyDamageRequest` 결과가 나온 뒤 champion별 callback 묶음에 `SylasGameSim::OnDamageResolved(world, tc, request, result);` 한 줄을 추가한다.
- `Shared/GameSim/Champions/Fiora/FioraGameSim.cpp`: `FindEnemyInCone`은 Q 전용으로 보존하고 그 아래에 §7.9의 완전한 strip helper를 추가한다. `ReleaseRiposte`의 호출만 새 helper로 교체한다. `OnW`에서 `range` param resolve 한 줄을 `ResolveSkillRange(...FIORA,W)`로 교체하고 direction 저장 직후 현재 transform yaw를 `ResolveChampionVisualYawNear`로 갱신한다.
- `Data/Gameplay/ChampionGameData/champions.json`: FIORA slot 2 `rangeMax`를 `0.0 -> 6.5`로 교체한다. VIEGO slot 2는 별도로 `12.5 -> 5.0`으로 교체한다.
- `Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json`: `skill.fiora.w.params`에서 `"range": 6.0` 한 필드를 삭제하고 `flatByRank`의 다섯 0을 다섯 80으로 교체한다. `skill.sylas.q`의 빈 params를 `{ "formationDelaySec": 0.5, "radius": 1.65 }`, damage type을 Magic, flat을 `[70,95,120,145,170]`으로 교체한다. `skill.sylas.w`의 빈 params를 `{ "healDamageRatio": 0.5 }`, damage type을 Magic, flat을 `[75,100,125,150,175]`로 교체한다. 각 항목의 나머지 ratio 배열은 그대로 둔다.
- `Shared/Schemas/Event.fbs`: `EffectTriggerEvent`의 마지막 기존 필드 아래에 `effectLength:float;`, `effectHalfWidth:float;`를 이 순서로 추가한다. generated 파일은 직접 편집하지 않고 `run_codegen.bat`로만 재생성한다.
- `Shared/GameSim/Components/ReplicatedEventComponent.h`: `durationMs`/effect payload scalar 구역 말미에 기본값 0인 `f32_t effectLength`, `f32_t effectHalfWidth`를 추가한다.
- `Client/Public/GamePlay/VisualHookRegistry.h`: `VisualHookContext`의 effect scalar 구역 말미에 기본값 0인 `f32_t fEffectLength`, `f32_t fEffectHalfWidth`를 추가한다.
- `Shared/GameSim/Systems/ReplicatedEventSerializer/ReplicatedEventSerializer.cpp`: `CreateEffectTriggerEvent` 호출의 말미에 component의 두 geometry 값을 추가한다.
- `Client/Private/Network/Client/EventApplier.cpp`: EffectTrigger decode 시 두 accessor를 `VisualHookContext`의 대응 필드에 복사한다.
- `Client/Public/GameObject/Champion/Fiora/Fiora_FxPresets.h`: `SpawnWParryActive`, `SpawnWParrySuccess`에 `length`, `halfWidth`를, `SpawnWRelease`에 `endWorldPos`, `halfWidth`를 lifetime 앞 인자로 추가한다.
- `Client/Private/GameObject/Champion/Fiora/Fiora_Skills.cpp`: authoritative Fiora W event 중 `ctx.fEffectLength <= 0.f`인 generic duplicate는 반환한다. cast/success에는 `ctx.fEffectLength`, `ctx.fEffectHalfWidth`를, recovery에는 `ctx.pCommand->groundPos`, `ctx.fEffectHalfWidth`를 새 preset 인자로 전달한다.
- `Client/Private/GameObject/Champion/Fiora/Fiora_FxPresets.cpp`: W active/success에서 preset spawn 결과 중 `GroundDecal`만 world anchor, midpoint, runtime length/width로 override하고 이미 계산된 `fYaw`를 보존한다. release는 runtime endpoint를 `bOverrideEndWorldPos=true`인 world segment로 넘기고 width를 `halfWidth*2`로 override한다. glow/flash/다른 스킬 preset은 수정하지 않는다.
- `Data/LoL/FX/Champions/Fiora/w_cast.wfx`: emitter `w_cc_indicator_yellow` 안에서만 width `6.8000 -> 6.5000`, height `1.9000 -> 1.6000`, legacy `attach_offset` Z `3.4000 -> 3.2500`, anchor.offset Z `3.4000 -> 3.2500`를 교체한다. authored yaw `-1.5708`은 보존한다.
- `Data/LoL/FX/Champions/Fiora/w_release.wfx`: emitter `w_sword_sharpflash_blue`의 width `1.0 -> 1.6`, end_offset Z `6.0 -> 6.5`만 교체한다.
- `Client/Private/UI/ChampionTuner.cpp`: skill ranked table 앞에 `rangeMax` editor를 추가하고, params object에 존재하는 `radius`, `formationDelaySec`, `healDamageRatio`만 present-mechanics editor로 노출한다. `kCustomFlatSkills`에서 `skill.fiora.w`를 삭제한다.

재비평에서 미해결 P0/P1이 있으면 source edit 전에 다시 계획을 고친다.

## 9. 보정 검증 — 예측이 먼저다

예측:

- F9 label은 입력 왼쪽 열 안에서 완전히 보이고 값 frame은 남은 폭을 사용한다.
- Viego W max/min은 data-derived 5.0/2.5이며 F4에서 유효값을 저장하면 새 cast/AI range가 reload 값을 읽는다.
- Ranged authored range는 5.6이다. gameplay radius가 더해지는 최종 중심 거리까지 정확히 0.7배라고 주장하지 않는다.
- Ashe Q dark alpha ground quad만 사라지고 additive glow는 남는다.
- Sylas Q는 cast tick에 피해/폭발이 없고 tick+15에 inside target 피해와 stage-2 cue가 정확히 한 번 생긴다. outside target은 변하지 않는다.
- Sylas W heal은 방어/실드 뒤 실제 HP 감소량의 0.5이며 max HP를 넘지 않고 zero damage에는 0이다.
- Bot AI는 계속 GameCommand 생산자다. 이번 champion gameplay truth는 Command→hook→DamageRequest→DamageQueue→snapshot/event 경계를 유지한다.

자동 gate:

```powershell
python Tools/LoLData/Build-LoLDefinitionPack.py --root .
python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check
python Tools/LoLData/Test-F4BalanceContracts.py --root .
cmd /c Shared\Schemas\run_codegen.bat
msbuild Shared/GameSim/Include/GameSim.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
msbuild Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
msbuild Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
msbuild Tools/SimLab/SimLab.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
Tools/Bin/Debug/SimLab.exe --sylas-qw-only
Tools/Bin/Debug/SimLab.exe --fiora-w-only
Tools/Bin/Debug/SimLab.exe --stage-input-only
Tools/Bin/Debug/SimLab.exe --f4-balance-only
powershell -ExecutionPolicy Bypass -File Tools/Harness/Run-BotAiValidation.ps1 -Configuration Debug
powershell -ExecutionPolicy Bypass -File Tools/Harness/Run-BotAiValidation.ps1 -Configuration Release
git diff --check
```

SimLab `--sylas-qw-only`은 Q pre-delay/exact tick/inside-outside/one cue/keyframe, W post-mitigation ratio/zero/cap/mirror를 검증한다. F4 contract는 Q/W data, heal param mapping, Q main decal diameter `2*radius`, F9/F4 UI anchors, Ranged 5.6을 검증한다.

SimLab `--fiora-w-only`는 +X facing, pre-release zero damage, 직사각형 inside/outside/behind, canonical damage, runtime range/radius override, event geometry를 검증한다. schema codegen 뒤 기존 effect의 geometry default 0과 Fiora custom event의 nonzero round-trip도 검사한다.

수동 Debug F5 gate:

- F9 width 560/700/넓힘에서 label·value·pending 문구 캡처.
- F4 Viego W range 5.0→테스트값→Save & Hot Load 후 다음 cast 변화, 원복.
- Sylas Q cursor 위치에서 cast와 0.5초 뒤 폭발 분리, 같은 순간 damage text/HP, 경계 안/밖 비교.
- Sylas W의 observed damage와 회복량 50%, max-HP clamp.
- Ashe Q 5초 동안 카메라 회전 시 검은 쿼드 없음, glow/mesh 있음; W hit 보임.
- Fiora W를 +X/+Z/대각선으로 각각 사용해 캐릭터 회전, 중심선에 붙은 6.5×1.6 indicator, 0.75초 뒤 endpoint release, 표시 안/밖 피해를 영상으로 비교한다. F4에서 range/half-width/damage를 임시 변경한 뒤 세 값이 다음 cast의 표시와 판정에 함께 반영되고 원복되는지 확인한다.
