Session - NYPC Python 자산을 선택적 bridge manifest로 봉인하고 Winters AI observation·Chrono A/B·IL/RL 수직 슬라이스의 적용 순서를 확정한다.

## 1. 반영해야 하는 코드

### 1-1. C:/Users/user/Desktop/Winters/Tools/AIResearch/README.md

새 파일:

```markdown
# Winters AI Research Tools

이 폴더는 NYPC에서 검증한 replay, manifest, imitation, league, counterfactual 패턴을 Winters 서버 권위 GameSim에 연결하는 오프라인 도구 경계다.

## Ownership

```text
Shared/GameSim
  authoritative transition, observation, action mask, state hash

Server
  policy artifact load, final candidate validation, GameCommand emission

Tools/AIResearch
  manifest, dataset validation, training, league, reports

Client
  read-only AI Debug and Chrono comparison visualization
```

Python은 gameplay transition truth를 소유하지 않는다. NYPC의 Python `GameState`, bot, referee, `deepcopy` rollout을 Winters runtime에 넣지 않는다.

## First packet

1. `bridge_manifest.json`으로 NYPC source의 provenance, SHA-256, disposition, target contract를 고정한다.
2. `ValidateBridgeManifest.py`로 source drift와 금지 복사를 차단한다.
3. GameSim의 versioned observation/action/trace schema가 확정된 뒤 episode schema와 exporter를 추가한다.
4. 한 챔피언·한 라인 fixture에서 pairwise imitation baseline을 만든다.
5. ChronoBreak의 faithful/reaction branch가 닫힌 뒤 BC, DAgger, PPO 순서로 확장한다.

## Validation

```powershell
python Tools/AIResearch/ValidateBridgeManifest.py `
  --manifest Tools/AIResearch/bridge_manifest.json `
  --nypc-root C:/Users/user/Desktop/NYPC
```

검증이 실패한 source는 복사하거나 승격하지 않는다. NYPC worktree가 dirty이므로 repository revision만으로 provenance를 대표하지 않고 각 파일 SHA-256을 함께 사용한다.
```

### 1-2. C:/Users/user/Desktop/Winters/Tools/AIResearch/bridge_manifest.json

새 파일:

```json
{
  "SchemaVersion": 1,
  "GeneratedAt": "2026-07-12",
  "SourceRepository": {
    "PathHint": "C:/Users/user/Desktop/NYPC",
    "Revision": "891f44c1091a7aed0ce15f657b98b7055ec6b429",
    "WorktreeDirty": true,
    "Rule": "Revision and per-file SHA-256 must both match before extraction."
  },
  "Entries": [
    {
      "SourcePath": "mushroom/scripts/position_loop_manifest.py",
      "SourceSha256": "5e7a0ba1505e5b7c43db40af7d9c1ecb58d20f18c8fd98098a6982c821552dbc",
      "ProvenanceAndLicense": "User-authored local NYPC repository; external distribution license review required.",
      "Disposition": "Extract",
      "ReusableSymbol": "ArtifactManifest",
      "TargetOwner": "Tools/AIResearch",
      "TargetContract": "ArtifactManifestV1",
      "ForbiddenDependencies": ["hard-coded ARTIFACTS", "absolute experiment paths", "Mushroom schema"],
      "ObservationSchemaVersion": 0,
      "ActionSchemaVersion": 0,
      "PolicyRevision": 0,
      "DeterministicFixture": "CONFIRM_NEEDED: canonical manifest fixture",
      "GoldenOutputHash": "CONFIRM_NEEDED",
      "PromotionGate": "Same inputs produce byte-identical canonical manifest output.",
      "RollbackOrDeletePath": "Tools/AIResearch/ArtifactManifest.py"
    },
    {
      "SourcePath": "mushroom/scripts/promotion_gate_codex.py",
      "SourceSha256": "fc3e78a5851245490e3a03bf346f86a8944636e0dac308fdc61de57dfc6af43e",
      "ProvenanceAndLicense": "User-authored local NYPC repository; external distribution license review required.",
      "Disposition": "Adapt",
      "ReusableSymbol": "PolicyPromotionGate",
      "TargetOwner": "Tools/AIResearch",
      "TargetContract": "PolicyPromotionGateV1",
      "ForbiddenDependencies": ["head2head.play", "g++ path", "Mushroom boards"],
      "ObservationSchemaVersion": 1,
      "ActionSchemaVersion": 1,
      "PolicyRevision": 1,
      "DeterministicFixture": "CONFIRM_NEEDED: mirrored Winters scenario set",
      "GoldenOutputHash": "CONFIRM_NEEDED",
      "PromotionGate": "Repeat determinism, both teams, frozen baseline, holdout non-regression, invalid command zero.",
      "RollbackOrDeletePath": "Tools/AIResearch/PolicyPromotionGate.py"
    },
    {
      "SourcePath": "mushroom/scripts/train_codex_action_model.py",
      "SourceSha256": "ac5aa00bc06d9daa06f7fdcea3037aec95ac27acdcc36ef4495a7c623ca10c35",
      "ProvenanceAndLicense": "User-authored local NYPC repository; external distribution license review required.",
      "Disposition": "Adapt",
      "ReusableSymbol": "ImitationRankingBaseline",
      "TargetOwner": "Tools/AIResearch",
      "TargetContract": "ImitationRankingArtifactV1",
      "ForbiddenDependencies": ["Mushroom FEATURES", "CDX1 binary", "position leakage"],
      "ObservationSchemaVersion": 1,
      "ActionSchemaVersion": 1,
      "PolicyRevision": 1,
      "DeterministicFixture": "CONFIRM_NEEDED: one champion one lane grouped split",
      "GoldenOutputHash": "CONFIRM_NEEDED",
      "PromotionGate": "Group holdout leakage zero, top-1 and regret reported, Python/C++ parity.",
      "RollbackOrDeletePath": "Tools/AIResearch/TrainImitationRankingBaseline.py"
    },
    {
      "SourcePath": "battlefield/tools/belief_fact_ledger.py",
      "SourceSha256": "936ff9449c466efef2804d4fdd30d7a70024bd07592d4e2eec009114ba3c6c13",
      "ProvenanceAndLicense": "User-authored local NYPC repository; external distribution license review required.",
      "Disposition": "Adapt",
      "ReusableSymbol": "PerceptionCalibrationLedger",
      "TargetOwner": "Tools/AIResearch",
      "TargetContract": "PerceptionCalibrationV1",
      "ForbiddenDependencies": ["WAR_TRACE regex", "battlefield zone semantics", "exact opponent state leakage"],
      "ObservationSchemaVersion": 1,
      "ActionSchemaVersion": 1,
      "PolicyRevision": 0,
      "DeterministicFixture": "CONFIRM_NEEDED: typed FP MISS OK JSONL fixture",
      "GoldenOutputHash": "CONFIRM_NEEDED",
      "PromotionGate": "Belief tick aligns with future authoritative fact without privileged feature leakage.",
      "RollbackOrDeletePath": "Tools/AIResearch/PerceptionCalibrationLedger.py"
    },
    {
      "SourcePath": "mushroom/scripts/run_s338_forced_branch_counterfactual.py",
      "SourceSha256": "c816b61f5b76076a393e8b2e368c0166b8a3bc471057c79e74841e0b3cb5fa38",
      "ProvenanceAndLicense": "User-authored local NYPC repository; external distribution license review required.",
      "Disposition": "Adapt",
      "ReusableSymbol": "ChronoCounterfactualRunner",
      "TargetOwner": "Tools/AIResearch",
      "TargetContract": "ChronoCounterfactualExperimentV1",
      "ForbiddenDependencies": ["Mushroom SETSTATE", "board grid", "fixed-turn continuation"],
      "ObservationSchemaVersion": 1,
      "ActionSchemaVersion": 1,
      "PolicyRevision": 1,
      "DeterministicFixture": "CONFIRM_NEEDED: GameSim keyframe plus external journal",
      "GoldenOutputHash": "CONFIRM_NEEDED",
      "PromotionGate": "Faithful branch is identical and alternate branch reports exact first divergence tick.",
      "RollbackOrDeletePath": "Tools/AIResearch/RunChronoCounterfactual.py"
    },
    {
      "SourcePath": "battlefield/tools/trace_match.py",
      "SourceSha256": "d3bf9b24fbcae264cd25bd044858a384c337f64e3af946f14c787384630e73c4",
      "ProvenanceAndLicense": "User-authored local NYPC repository; external distribution license review required.",
      "Disposition": "Adapt",
      "ReusableSymbol": "EpisodeExporterPattern",
      "TargetOwner": "Server and Tools/AIResearch",
      "TargetContract": "AiEpisodeV1",
      "ForbiddenDependencies": ["Python GameState", "stdin referee", "wall-clock transition truth"],
      "ObservationSchemaVersion": 1,
      "ActionSchemaVersion": 1,
      "PolicyRevision": 1,
      "DeterministicFixture": "CONFIRM_NEEDED: GameSim scenario replay",
      "GoldenOutputHash": "CONFIRM_NEEDED",
      "PromotionGate": "Episode replay final hash equals live GameSim final hash.",
      "RollbackOrDeletePath": "Tools/AIResearch/EpisodeExporter.py"
    },
    {
      "SourcePath": "battlefield/tools/claude_turn_inspect.py",
      "SourceSha256": "11d500c5637d317215487aa28e6275da07a04f4ff5453b46ff797831fe0e88b0",
      "ProvenanceAndLicense": "User-authored local NYPC repository; external distribution license review required.",
      "Disposition": "Reference",
      "ReusableSymbol": "DecisionInspectionLayout",
      "TargetOwner": "Client AI Debug",
      "TargetContract": "FactPerceptionCandidateOutcomeViewModel",
      "ForbiddenDependencies": ["tagged text parser", "WAR_TRACE", "battlefield command names"],
      "ObservationSchemaVersion": 1,
      "ActionSchemaVersion": 1,
      "PolicyRevision": 0,
      "DeterministicFixture": "CONFIRM_NEEDED: golden typed trace",
      "GoldenOutputHash": "CONFIRM_NEEDED",
      "PromotionGate": "Selected trace row resolves to source observation, formula, command and outcome.",
      "RollbackOrDeletePath": "Client AI Debug view model additions"
    },
    {
      "SourcePath": "battlefield/tools/lab_selfplay.py",
      "SourceSha256": "41c59357aacee373d1dde27613c838fb6107fbbc15b76725b6106042352cf3c3",
      "ProvenanceAndLicense": "User-authored local NYPC repository; external distribution license review required.",
      "Disposition": "Adapt",
      "ReusableSymbol": "OfflineLeagueScheduler",
      "TargetOwner": "Tools/AIResearch",
      "TargetContract": "OfflineLeagueV1",
      "ForbiddenDependencies": ["battlefield subprocess protocol", "ThreadPool referee", "map names"],
      "ObservationSchemaVersion": 1,
      "ActionSchemaVersion": 1,
      "PolicyRevision": 1,
      "DeterministicFixture": "CONFIRM_NEEDED: mirrored scenario matrix",
      "GoldenOutputHash": "CONFIRM_NEEDED",
      "PromotionGate": "Every result records seed, side, policy hash, fault count and confidence interval.",
      "RollbackOrDeletePath": "Tools/AIResearch/RunOfflineLeague.py"
    },
    {
      "SourcePath": "battlefield/tools/claude_rl_tune.py",
      "SourceSha256": "42d3a3d006c72a471adb8024ef39e628894581ac5e60df95434f63de652cdb60",
      "ProvenanceAndLicense": "User-authored local NYPC repository; external distribution license review required.",
      "Disposition": "Reference",
      "ReusableSymbol": "BlackBoxTuningBaseline",
      "TargetOwner": "Tools/AIResearch",
      "TargetContract": "EsGaBaselineV1",
      "ForbiddenDependencies": ["RL naming", "battlefield env", "battlefield opponents"],
      "ObservationSchemaVersion": 0,
      "ActionSchemaVersion": 0,
      "PolicyRevision": 0,
      "DeterministicFixture": "CONFIRM_NEEDED: analytic objective smoke",
      "GoldenOutputHash": "CONFIRM_NEEDED",
      "PromotionGate": "Reported only as coordinate-ascent/ES/GA baseline, never as reinforcement learning.",
      "RollbackOrDeletePath": "Tools/AIResearch/BlackBoxTuningBaseline.py"
    },
    {
      "SourcePath": "mushroom/scripts/run_s*.py and battlefield/tmp*/scratch*",
      "SourceSha256": "NOT_APPLICABLE_PATTERN",
      "ProvenanceAndLicense": "Session, patch, temp and scratch history.",
      "Disposition": "DoNotCopy",
      "ReusableSymbol": "None",
      "TargetOwner": "None",
      "TargetContract": "RejectedByPolicy",
      "ForbiddenDependencies": ["all source content"],
      "ObservationSchemaVersion": 0,
      "ActionSchemaVersion": 0,
      "PolicyRevision": 0,
      "DeterministicFixture": "Not applicable",
      "GoldenOutputHash": "Not applicable",
      "PromotionGate": "Must remain absent from Winters runtime and AIResearch source.",
      "RollbackOrDeletePath": "Any copied matching file"
    }
  ]
}
```

### 1-3. C:/Users/user/Desktop/Winters/Tools/AIResearch/ValidateBridgeManifest.py

새 파일:

```python
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


RequiredEntryFields = (
    "SourcePath",
    "SourceSha256",
    "ProvenanceAndLicense",
    "Disposition",
    "ReusableSymbol",
    "TargetOwner",
    "TargetContract",
    "ForbiddenDependencies",
    "ObservationSchemaVersion",
    "ActionSchemaVersion",
    "PolicyRevision",
    "DeterministicFixture",
    "GoldenOutputHash",
    "PromotionGate",
    "RollbackOrDeletePath",
)

AllowedDispositions = {"Extract", "Adapt", "Reference", "DoNotCopy"}


def ComputeSha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def IsPatternEntry(entry: dict[str, Any]) -> bool:
    return entry.get("SourceSha256") == "NOT_APPLICABLE_PATTERN"


def ValidateEntry(
    entry: dict[str, Any],
    index: int,
    nypc_root: Path,
) -> list[str]:
    errors: list[str] = []
    for field in RequiredEntryFields:
        if field not in entry:
            errors.append(f"Entries[{index}] missing field: {field}")

    disposition = entry.get("Disposition")
    if disposition not in AllowedDispositions:
        errors.append(
            f"Entries[{index}] invalid Disposition: {disposition!r}"
        )

    forbidden = entry.get("ForbiddenDependencies")
    if not isinstance(forbidden, list) or not all(
        isinstance(value, str) and value for value in forbidden
    ):
        errors.append(
            f"Entries[{index}] ForbiddenDependencies must be non-empty strings"
        )

    if disposition == "DoNotCopy":
        if entry.get("TargetOwner") != "None":
            errors.append(
                f"Entries[{index}] DoNotCopy TargetOwner must be 'None'"
            )
        return errors

    if IsPatternEntry(entry):
        errors.append(
            f"Entries[{index}] pattern hash is only valid for DoNotCopy"
        )
        return errors

    source_path = entry.get("SourcePath")
    if not isinstance(source_path, str) or not source_path:
        errors.append(f"Entries[{index}] SourcePath must be a non-empty string")
        return errors

    source = nypc_root / Path(source_path)
    if not source.is_file():
        errors.append(f"Entries[{index}] source missing: {source}")
        return errors

    expected_hash = entry.get("SourceSha256")
    actual_hash = ComputeSha256(source)
    if actual_hash != expected_hash:
        errors.append(
            f"Entries[{index}] SHA-256 mismatch: {source_path} "
            f"expected={expected_hash} actual={actual_hash}"
        )

    return errors


def Main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate the selective NYPC to Winters AI bridge manifest."
    )
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--nypc-root", required=True, type=Path)
    args = parser.parse_args()

    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    errors: list[str] = []

    if manifest.get("SchemaVersion") != 1:
        errors.append("SchemaVersion must be 1")

    entries = manifest.get("Entries")
    if not isinstance(entries, list) or not entries:
        errors.append("Entries must be a non-empty list")
    else:
        for index, value in enumerate(entries):
            if not isinstance(value, dict):
                errors.append(f"Entries[{index}] must be an object")
                continue
            errors.extend(ValidateEntry(value, index, args.nypc_root))

    if errors:
        print("NYPC bridge manifest validation FAILED")
        for error in errors:
            print(f"- {error}")
        return 1

    print(
        "NYPC bridge manifest validation PASS: "
        f"entries={len(entries)} sourceRoot={args.nypc_root}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(Main())
```

### 1-4. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIResearchTypes.h

`CONFIRM_NEEDED`: 새 파일이다. `AiObservationV1`, `AiActionMaskV1`, `AiCandidateEvidenceV1`, `AiDecisionTraceV1`의 complete body는 다음 조건을 먼저 확인한 뒤 별도 code-preview packet에서 작성한다.

- `NetEntityId`와 process-local `EntityID` 중 dataset identity 소유권.
- fixed-size POD keyframe 대상과 streaming-only variable record의 분리.
- candidate feature ID와 unit/range/default metadata owner.
- `ObservationSchemaVersion`, `ActionSchemaVersion`, rules/data/policy hash의 실제 타입.
- Shared boundary 검사에서 Engine/Client/JSON/ImGui/IO 의존이 0인지 확인.

### 1-5. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h

`struct TickContext`의 아래 기존 코드를 기준으로 `IChampionAIEnvironmentQuery`와 `IChampionAIDecisionSink` 포인터를 추가하는 변경은 `CONFIRM_NEEDED`다.

기존 코드:

```cpp
struct TickContext
{
    uint64_t tickIndex = 0;
    f32_t fDt = DeterministicTime::kFixedDt;
    f64_t fSimulatedTimeSec = 0;
    DeterministicRng* pRng = nullptr;
    EntityIdMap* pEntityMap = nullptr;
    EntityID localPlayer = NULL_ENTITY;
    const IWalkableQuery* pWalkable = nullptr;
    const ILagCompensationQuery* pLagCompensation = nullptr;
    const GameplayDefinitionPack* pDefinitions = nullptr;
};
```

확인 전에는 포인터를 추가하지 않는다. 다음 code-preview packet에서 모든 aggregate initializer와 SimLab adapter를 조사해 complete replacement를 작성한다.

### 1-6. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.cpp

`GameplayStateQuery::CanBeSeenBy`는 현재 invisible flag만 검사한다. `VisibilityComponent::teamVisibilityMask`를 권위 truth로 사용하도록 교체해야 하지만 Server가 현재 `BuildServerVisibleToAll()`을 사용하므로 이 함수만 단독 수정하면 안 된다. 다음 세 파일과 같은 packet으로 작성할 때까지 `CONFIRM_NEEDED`다.

- `Server/Private/Game/GameRoomSpawn.cpp`
- Server team vision owner 신규 파일
- `Shared/GameSim/Systems/ChampionAI/ChampionAIPerception.h`

### 1-7. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIPerception.h

`CONFIRM_NEEDED`: snapshot-only target 구조를 Observation + Belief summary로 확장한다. exact code는 server visibility truth와 keyframe registration을 함께 확인한 뒤 작성한다.

필수 결과:

- hidden enemy current position/gold/level 직접 읽기 제거.
- lastSeenTick/position/velocity, confidence, ETA envelope.
- fact tick과 observation tick 분리.
- process-local pointer/heap container 없이 checkpoint 가능한 state.

### 1-8. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIValuation.h

### 1-9. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIValuation.cpp

`CONFIRM_NEEDED`: 현재 최종 `UtilityScores`만 반환하는 경로를 후보별 raw feature, weight, contribution, risk, opportunity, commit cost가 남는 단일 평가 함수로 교체한다. 기존 `BuildUtilityScores`와 두 번째 평가식을 병렬 유지하지 않는다. Read-only re-score와 live decision이 같은 함수와 weight revision을 사용해야 한다.

### 1-10. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp

`CONFIRM_NEEDED`: 아래 현재 경로를 보존하면서 decision sink와 multi-rate cache를 추가한다.

기존 코드:

```cpp
ChampionAIContext ctx =
    BuildChampionAIContext(world, self, ai, champion, tc, selfPos);

UpdateChampionAIDecisionEvidence(ai, tc, ctx, profile);
ai.debugAvailableActionMask =
    BuildChampionAIAvailableActionMask(world, self, ai, ctx);
ai.debugAvailableSkillMask =
    BuildChampionAIAvailableSkillMask(world, self, champion.id, tc, profile, ctx);
```

complete replacement 전 확인할 것:

- 30 Hz capability/emergency와 5~10 Hz micro, 1~2 Hz macro cache owner.
- 모든 early return에서 정확히 한 decision/result record가 닫히는 방식.
- command가 다음 phase에서 executor에 의해 거절될 때 결과를 같은 record에 연결하는 ID.
- selected bot만 상세 trace를 구독하는 서버 subscription.

### 1-11. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomCommands.cpp

`CGameRoom::PerformPendingRewind`는 S015 Handoff 상태이므로 현재 파일을 바로 수정하지 않는다. 아래 기존 함수 전체를 기준으로 후속 Chrono branch packet을 작성할 때까지 `CONFIRM_NEEDED`다.

필수 결과:

- nearest keyframe 복원 후 target tick까지 eligible journal re-sim.
- Faithful과 ReactiveCounterfactual 모드 분리.
- `timelineEpoch`, `branchId`, policy/data/RNG manifest.
- ControlPlane/ObservationOnly command 제외.
- single mutable AI RNG 대신 named random key.
- branch recorder와 full authoritative snapshot.

### 1-12. C:/Users/user/Desktop/Winters/Client/Private/UI/AIDebugPanel.cpp

`CONFIRM_NEEDED`: 현재 16행 최종 trace 위에 두 번째 AI Debug panel을 만들지 않는다. 기존 F9를 다음 탭으로 확장한다.

- Fact/Observation/Belief
- Influence layers
- Candidates and contribution
- Commitment and executor result
- Read-only re-score A/B
- Chrono branch comparison
- ShadowCoach top-k guide

typed snapshot/result schema와 selected-bot subscription이 먼저 확정된 뒤 exact replacement를 작성한다.

### 1-13. C:/Users/user/Desktop/Winters/Tools/SimLab/main.cpp

`CONFIRM_NEEDED`: S015 keyframe probe를 보존하고 한 챔피언·한 라인 episode fixture를 추가한다. complete code-preview 전 다음을 확정한다.

- authoritative observation/action/candidate record serializer.
- same-seed episode hash.
- hidden enemy privileged feature leakage probe.
- faithful branch identical hash.
- alternate policy first divergence tick.
- invalid command/action mask zero.

### 1-14. C:/Users/user/Desktop/Winters/Tools/AIResearch/TrainImitationRankingBaseline.py

`CONFIRM_NEEDED`: 새 파일이다. `AiEpisodeV1` JSONL과 split key가 확정된 뒤 Mushroom feature/CDX1 의존 없이 complete body를 작성한다. 첫 trainer는 pairwise ranking, group holdout, top-1/top-3/regret, artifact manifest, deterministic seed를 포함한다.

### 1-15. C:/Users/user/Desktop/Winters/Tools/AIResearch/TrainRecurrentPpo.py

`CONFIRM_NEEDED`: 새 파일이다. BC/DAgger baseline이 holdout gate를 통과하기 전에는 작성하지 않는다. action masking, GAE, clipped objective, value/entropy loss, truncated sequence, NaN guard, reward audit, checkpoint manifest 전체가 확인된 뒤 complete body를 작성한다.

## 2. 검증

현재 문서 packet 검증:

```powershell
git diff --check -- .md/architecture/WINTERS_NYPC_HUMANLIKE_AI_RESEARCH_ARCHITECTURE.md .md/plan/2026-07-12_S017_NYPC_NEXT_NATION_TO_WINTERS_AI_CHRONOBREAK_PLAN.md .md/plan/ai/14_NYPC_COMPETITION_ML_LAB_BRIDGE.md .md/architecture/WINTERS_CODEBASE_COMPASS.md .md/collab/ACTIVE_WORK_PACKETS.md
```

P0 bridge manifest 적용 후 검증:

```powershell
python Tools/AIResearch/ValidateBridgeManifest.py `
  --manifest Tools/AIResearch/bridge_manifest.json `
  --nypc-root C:/Users/user/Desktop/NYPC
```

P1 observation/trace 적용 후 검증:

```powershell
powershell -ExecutionPolicy Bypass -File Tools/Harness/Check-SharedBoundary.ps1
& 'C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe' Shared/GameSim/Include/GameSim.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
& 'C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe' Tools/SimLab/SimLab.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
```

P2 Server/Client/Chrono 적용 후 검증:

```powershell
& 'C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe' Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
& 'C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe' Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
```

자동 수용 기준:

```text
PASS bridge source path and SHA-256 validation
PASS DoNotCopy entries never create a Winters target
PASS same seed/input/policy revision episode hash
PASS team vision outside current fact leakage zero
PASS candidate hard mask rejects illegal actions
PASS decision record includes executor accepted/rejected result
PASS faithful Chrono branch state hash identical
PASS reactive branch reports exact first divergence tick
PASS Python/C++ policy inference parity
PASS holdout league invalid command zero
```

수동 인게임 확인:

- F9에서 선택 bot의 Observation과 Belief를 구분해 본다.
- Influence layer 하나씩 켰을 때 source, ETA, confidence가 설명된다.
- Read-only re-score는 world를 바꾸지 않는다.
- 같은 checkpoint에서 policy A/B branch가 상대 재반응을 포함해 달라진다.
- ShadowCoach는 명령을 내리지 않고 top-k 행동과 이유만 표시한다.

## 3. 2026-07-13 authoritative implementation status

이 절은 위 code-preview와 각 `CONFIRM_NEEDED`의 역사적 계획 상태를
대체한다. 위 블록은 당시 설계 근거와 의도된 계약을 보존하기 위해 삭제하지
않는다. 3-1은 코드 반영 범위이고, 빌드·테스트 성공 주장은 날짜와 명령을
고정한 3-4의 validation 결과로만 한정한다.

### 3-1. 반영됨

- `Tools/AIResearch/bridge_manifest.json`과 strict validator/test가 선택한
  NYPC source의 SHA-256, provenance, disposition, forbidden copy를 봉인한다.
- `ChampionAIResearchTypes.h`의 typed POD observation, action mask,
  Retreat/Fight/Farm/Siege candidate, command, executor provenance/result와
  transient research trace ring이 GameSim에 연결됐다.
- command executor가 Move/BasicAttack/CastSkill/Recall/RecallCancel/Flash의
  Accepted/Rejected와 stable reason을 command sequence로 최종 trace에
  되돌려 쓴다.
- `AiEpisodeV1` native capture, canonical JSONL exporter, semantic validator,
  promotion validation과 live GameSim smoke 경로가 있다.
- live smoke의 `rules_hash`는 정확히 빌드된 `SimLab.exe` SHA-256이다.
  240 tick에서 Accepted decision을 강제로 만들어 마지막 record를
  `truncated=true`, `terminal=false`로 닫는다.
- 숨은 적의 current NetEntityId/level/HP/inventory value/distance는 episode에서
  canonical zero이고, current wallet gold는 observation으로 내보내지 않는다.
- champion AI observation에는 allied vision source의 반경·원뿔·concealment
  filter와 5초 last-seen memory가 있다.
- 9x9 `ThreatNow`, `ThreatBelief`, `SupportEta`, `EscapeCost` influence map이
  transient GameSim research instrumentation으로 계산된다.
- `PolicyPromotionGate.py`는 mirrored/repeat/frozen report 계약을 검증한다.
  실제 runtime policy를 승격하거나 league를 실행하지 않는다.
- `TrainImitationRankingBaseline.py`는 promotion-valid Accepted episode만
  소비하고 `(scenario_id, rules_hash, definition_hash)` frozen group split을
  사용하는 결정론적 NumPy pairwise supervised baseline이다. PyTorch/RL이
  아니고 runtime policy를 수정하지 않는다.
- checkpoint restore는 staged validation을 사용하고 research debug component는
  transient로 제외된다. snapshot에는 `timelineEpoch`, `branchId`,
  `toolRevision`이 추가됐으며 성공한 rewind가 timeline identity를 바꾼다.
- Client는 epoch/branch 변경을 timeline rebase로 처리하고, WRPL v2는 command
  payload domain과 tool revision journal foundation을 가진다.

### 3-2. 명시적으로 미완료

- terrain/nav wall line-of-sight와 team별 network snapshot FOW
- Influence layer의 Brain/Valuation 소비, `AiEpisodeV1` 직렬화, Client F9 UI
- target tick까지 external command journal을 재실행하는 exact faithful branch
- 상대가 다시 판단하는 reactive A/B와 first-divergence report
- named/counter-based AI RNG substream
- measured champion corpus, 실제 PyTorch BC/DAgger, PPO-ready trajectory,
  recurrent PPO, self-play와 frozen league runner
- C++ learned policy artifact inference/promotion과 `ShadowCoach`

`AiEpisodeV1`은 현재 decision-event BC/ranking 계약이다. dense per-tick
observation, explicit next observation, elapsed-step discount, recurrent state가
없으므로 PPO trajectory라고 부르지 않는다. `Decision` brain도 여전히
RuleBased fallback이므로 learned runtime control을 주장하지 않는다.

### 3-3. 다음 실행 gate

1. terrain/network FOW와 episode leakage fixture를 닫는다.
2. branch-aware external journal exact replay를 만든 뒤 faithful/reactive A/B를
   분리한다.
3. 실제 측정 corpus로 PyTorch BC를 먼저 닫고 DAgger, PPO 순서로 진행한다.
4. frozen opponent league와 runtime promotion을 통과한 동일 policy만
   `LearnedControl`/`ShadowCoach`에 연결한다.

### 3-4. 확인된 validation

2026-07-13에 다음이 확인됐다.

- `RunValidation.ps1 -FullBuild -SimTicks 1800 -Seed 42` 통과
- Server/Client Debug x64 빌드 통과
- replay Snapshot/Event FlatBuffer fail-closed validation 반영 후 Client Debug
  x64 재빌드 통과
- live episode 두 run의 native/metadata/JSONL 결정성 검증과 raw/promotion
  validation 통과
- canonical live episode SHA-256:
  `BECC9A151ABD8C24E45D79C787EA37FDB7BAFFA513896723128F912C98B0130E`

이 검증은 현재 foundation이 빌드되고 계약 probe를 통과했다는 뜻이다.
미완료 목록의 PyTorch 학습, Chrono reactive A/B, league 성능을 대신하지
않는다.
