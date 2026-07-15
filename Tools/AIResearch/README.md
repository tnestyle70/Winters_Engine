# Winters AI Research Tools

이 폴더는 NYPC에서 검증한 manifest, replay, imitation, league,
counterfactual 실험 규율을 Winters 서버 권위 GameSim에 연결하는 오프라인
도구 경계다. Python은 gameplay transition truth를 소유하지 않으며 NYPC의
Python `GameState`, referee, `deepcopy` rollout을 Winters runtime에 넣지
않는다.

## Ownership

```text
Shared/GameSim
  authoritative transition, typed observation/action/decision trace,
  executor result, state/checkpoint truth

Server
  policy selection boundary, final validation, GameCommand emission,
  snapshot/replay/timeline metadata

Tools/AIResearch
  selective bridge manifest, dataset export/validation,
  offline baseline training, promotion-report validation

Client
  read-only AI/Chrono debug presentation and timeline rebase
```

## Implemented S017 foundation

- `bridge_manifest.json`과 `ValidateBridgeManifest.py`가 선택한 NYPC 파일의
  provenance, SHA-256, disposition, target contract를 검증한다. 601개 Python
  파일을 일괄 복사하지 않는다.
- `AiEpisodeV1`은 native typed decision trace를 canonical JSONL로 내보내고,
  current observation, candidate/action mask, selected candidate, emitted
  command, final executor result, state hash와 episode boundary를 검증한다.
- promotion validation은 privileged observation, 불법 후보, unfinished
  executor, schema/hash/boundary 오류를 거절한다.
- `TrainImitationRankingBaseline.py`의 기본값은 기존 결정론적 NumPy pairwise
  supervised baseline이다. 명시적 `--backend pytorch-masked-bc`만 CPU
  float64 full-batch masked cross entropy를 사용한다. 두 경로 모두
  promotion-valid Accepted record와 `(scenario_id, rules_hash,
  definition_hash)` frozen split만 사용하며 강화학습이나 active runtime
  promotion을 수행하지 않는다.
- `PolicyPromotionGate.py`는 mirrored/repeat/frozen report의 형식과 계약을
  검증한다. 실제 policy state를 바꾸거나 실제 league를 실행하지 않는다.
- GameSim champion observation에는 팀 시야의 radial/cone/concealment filter와
  5초 last-seen memory가 연결돼 있다. terrain wall LOS와 network snapshot
  confidentiality는 아직 없다.
- 9x9 `ThreatNow`, `ThreatBelief`, `SupportEta`, `EscapeCost` influence map은
  transient research instrumentation으로 구현돼 있다. 현재 정책 입력,
  `AiEpisodeV1`, Client overlay에는 연결되지 않았다.
- snapshot의 `timelineEpoch`, `branchId`, `toolRevision`과 Client timeline
  rebase, WRPL v2 command/tool revision journal foundation이 있다. exact command
  journal re-simulation과 faithful/reactive Chrono A/B는 아직 없다.

## S022 PyTorch BC shadow flow

```text
Headless GameSim workers (30 Hz truth, 5 Hz decisions)
  -> Accepted AiEpisodeV1 measured corpus
  -> offline Python/PyTorch masked behavior cloning
  -> canonical PolicyArtifactV1 JSON + fixed little-endian .wbc + SHA-256
  -> Server startup load
  -> C++ shadow-only rescore
  -> F9/Chrono active-final vs shadow disagreement review
```

PyTorch는 게임 규칙이나 transition을 다시 구현하지 않는다. 67개 typed
observation/candidate feature를 사용해 legal candidate 네 개의 logit을
학습하는 오프라인 도구다. illegal candidate는 loss와 argmax에서 mask되고,
동점은 낮은 candidate kind가 이긴다. CUDA, AMP, DataLoader shuffle,
`torch.save` checkpoint는 이 결정론 계약에서 사용하지 않는다.

runtime artifact는 `WBCPOL1` 고정 little-endian binary이며 normalization
mean/inverse-scale과 bias 없는 67개 shared linear weight만 담는다. Server는
episode 시작 전에 명시된 SHA-256과 schema/shape/order를 검증한다. S022의
정책은 기존 RuleBased/PlayerLike command, RNG, intent를 바꾸지 않는
`SHADOW_ONLY`다. F9 trace에서 같은 decision의 active-final 선택, learned
선택, 네 logit, 차이 여부와 artifact revision을 비교한다.

게임 Client를 계속 실행해 Python에 frame을 실시간 공급하거나, 매 tick
학습 weight를 hot-swap하는 구조가 아니다. 많은 headless episode를 먼저
수집하고 process 밖에서 batch 학습한 뒤 immutable artifact를 다음
episode/branch 경계에 로드한다. Client는 사람이 disagreement와 Chrono
counterfactual을 검토하고 이후 DAgger 교정 label을 만드는 관측 도구다.

golden fixture로 trainer/binary 계약만 확인하는 예시는 다음과 같다. 이
입력의 source policy revision은 7이므로 새 revision은 반드시 더 커야 한다.

```powershell
python -B Tools/AIResearch/TrainImitationRankingBaseline.py `
  --backend pytorch-masked-bc `
  --input Tools/AIResearch/fixtures/imitation_ranking_v1_golden.jsonl `
  --output out/s022-contract.json `
  --runtime-output out/s022-contract.wbc `
  --policy-id s022-contract `
  --policy-revision 8 `
  --minimum-groups 8 `
  --fixture-contract
```

이 fixture 결과는 `CONTRACT_ONLY_NOT_MEASURED`이고 봇 실력 향상 증거가
아니다. 성능 주장은 별도 measured corpus, holdout, mirrored repeat,
frozen opponent/골든 시나리오 promotion gate를 통과한 뒤에만 가능하다.

첫 native measured corpus와 shadow artifact를 만드는 명령은 다음과 같다.
이 경로는 Client 창이 아니라 Debug SimLab의 실제 GameSim decision과
executor를 사용하며, 각 scenario를 두 번 실행해 raw/metadata/JSONL SHA를
비교한다.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File Tools/AIResearch/tests/RunLiveAiEpisodeSmokeProbe.ps1 `
  -BuildMeasuredCorpus `
  -MeasuredSeedsPerScenario 8 `
  -MeasuredCorpusOutput out/measured_ai_episode_v1.jsonl

python -B Tools/AIResearch/TrainImitationRankingBaseline.py `
  --backend pytorch-masked-bc `
  --input out/measured_ai_episode_v1.jsonl `
  --output out/s022-measured-shadow.json `
  --runtime-output out/s022-measured-shadow.wbc `
  --policy-id s022-measured-shadow `
  --policy-revision 2
```

이 32 frozen-group/32 Blue-Red mirror-pair/64-record corpus는 end-to-end 측정·학습·로딩 계약의 최소 증거이지
사람 수준 성능 corpus가 아니다. active canary 전에는 최소 10,000 Accepted
decision, 100개 이상 frozen group, holdout/골든 시나리오와 mirrored 평가가
별도로 필요하다. trainer도 episode의 `terminal/truncated` 경계가 canonical
마지막 행보다 앞서면 입력을 거절한다.

현재 measured registry는 `RETREAT_VS_ONE_MACRO_BOOTSTRAP` 범위다. 각
record는 Retreat와 Fight/Farm/Siege 중 하나가 경쟁하는 2-way legal mask이며
Fight-vs-Farm-vs-Siege 3/4-way conflict는 아직 없다. manifest의
`legal_candidate_mask_histogram`을 함께 보고, 이 작은 holdout 정확도를 실제
라인전·한타 실력으로 해석하지 않는다.

## Human/debug correction materialization

`AiEpisodeV1`은 권위 시뮬레이션에서 실제로 선택·집행된 사실 기록이다. 인간이
F9 trace나 수동 Chrono 비교를 보고 다른 macro 후보를 선호하더라도 원본
`selected_candidate_kind`, command, executor, reward, next-state를 고쳐 쓰지
않는다. 교정은 source 파일 SHA, canonical record SHA, timeline identity에 묶인
`AiDecisionCorrectionSidecarV1`로 작성하고 다음 명령으로 IL 전용 dataset을
materialize한다.

```powershell
python -B Tools/AIResearch/MaterializeImitationDataset.py `
  --input out/measured_ai_episode_v1.jsonl `
  --corrections out/human_debug_corrections_v1.json `
  --output out/imitation_decision_v1.jsonl

python -B Tools/AIResearch/TrainImitationRankingBaseline.py `
  --backend pytorch-masked-bc `
  --input out/imitation_decision_v1.jsonl `
  --input-contract imitation-decision-v1 `
  --output out/corrected-shadow.json `
  --runtime-output out/corrected-shadow.wbc `
  --policy-id corrected-shadow `
  --policy-revision 3
```

각 `ImitationDecisionV1` 행은 원본 `source_record`를 그대로 보존하고 별도의
`expert_label`만 가진다. 원래 Accepted 선택은
`SOURCE_ACCEPTED/SOURCE_COMMAND_ACCEPTED`, 인간 교정은
`HUMAN_DEBUG_CORRECTION/UNEXECUTED_COUNTERFACTUAL`이다. 후자의 source command,
executor, reward는 교정 행동의 결과가 아니며 RL transition이나 승급 증거로
사용할 수 없다. 현재 sidecar의 F9/Chrono 출처는 사람이 작성한 self-attestation
이고 runtime이 자동 증명한 provenance가 아니다. 자동 in-game DAgger capture는
별도 action-origin/runtime journal 계약이 필요하다.

learned/active 정책이 방문한 상태는 source action 자체가 expert가 아니므로 위
기본 materialization을 사용하면 안 된다. 해당 에피소드는 먼저 F9/Chrono로
검토하고, 실제로 사람이 바꿔 표시한 행만 다음처럼 추출한다.

```powershell
python -B Tools/AIResearch/MaterializeImitationDataset.py `
  --input out/active_episode_v1.jsonl `
  --corrections out/active_human_corrections_v1.json `
  --output out/dagger_corrected_only_v1.jsonl `
  --corrected-only
```

`--corrected-only`는 교정되지 않은 active source action을 dataset에서 제외하고,
교정이 하나도 없으면 fail-closed한다. 따라서 active episode를 직접
`--input-contract ai-episode-v1`로 trainer에 넣지 않는다.

## Debug active-policy canary

`RunActiveAiPolicyEpisodeProbe.ps1`은 고정 seed의 1v1 GameSim 상태에서 매
결정 직전 checkpoint를 저장하고, 첫 pass에서 `.wbc` 제안을 계산한 뒤 restore,
두 번째 pass에서 그 macro 후보를 Debug one-shot override로 집행한다. 첫 pass의
command는 executor에 전달되지 않으며 두 번째 pass의 최종 trace와 결과만
기록된다.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File Tools/AIResearch/tests/RunActiveAiPolicyEpisodeProbe.ps1
```

probe는 동일 policy/seed를 두 번 실행해 native trace, metadata, canonical JSONL,
canary report의 SHA-256 일치를 요구하고, 실제 rule-policy disagreement와 learned
intervention 적용을 최소 한 번 확인한다. 잘린 `.wbc`는 SHA가 맞더라도 decode에서
fail-closed해야 한다. 이 출력의 report는
`EVALUATION_AND_DAGGER_STATE_DISCOVERY_ONLY`이며 성능·승급·expert corpus를
주장하지 않는다.

## AiEpisodeV1

세부 wire/export/privacy 계약은
[`AI_EPISODE_V1.md`](AI_EPISODE_V1.md)를 따른다. 핵심 제한은 다음과 같다.

- V1 후보는 `Retreat`, `Fight`, `Farm`, `Siege` 네 종류뿐이다.
- 숨은 적의 current enemy field는 canonical zero다.
- live smoke의 `rules_hash`는 정확히 빌드된 `SimLab.exe`의 SHA-256이다.
- 마지막 tick은 Accepted decision을 강제로 만들고 time-limit
  `truncated=true` 경계로 끝낸다.
- 이 형식은 decision-event BC/ranking 계약이지 PPO trajectory 계약이 아니다.

## Validation

도구·계약 검증:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File Tools/AIResearch/RunValidation.ps1
```

GameSim/SimLab와 live episode smoke 포함:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File Tools/AIResearch/RunValidation.ps1 `
  -BuildCpp
```

Server/Client Debug x64까지 요청:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File Tools/AIResearch/RunValidation.ps1 `
  -FullBuild
```

명령은 검증 방법을 정의할 뿐이다. 실제 통과 여부와 artifact hash는 별도
검증 기록에 남긴다. 검증 실패 시 source를 복사하거나 policy를 승격하지
않는다.

2026-07-13 검증 결과:

- `RunValidation.ps1 -FullBuild -SimTicks 1800 -Seed 42` 통과
- Server/Client Debug x64 빌드 통과
- replay Snapshot/Event FlatBuffer fail-closed validation 반영 후 Client Debug
  x64 재빌드 통과
- 두 번의 live episode canonical JSONL SHA-256 일치:
  `BECC9A151ABD8C24E45D79C787EA37FDB7BAFFA513896723128F912C98B0130E`

이 결과는 계약·결정성·빌드 회귀 증거다. 봇 성능, 실제 학습, league 승률
또는 runtime policy promotion의 증거로 해석하지 않는다.

## Next gates

1. terrain-aware LOS와 실제 network FOW confidentiality
2. Influence를 typed observation/episode/AI Debug UI에 연결
3. branch-aware external command journal exact re-simulation과 reactive A/B
4. disagreement 기반 인간/오라클 DAgger label adapter와 재학습
5. PPO-ready trajectory contract와 recurrent PPO 1v1
6. 1v1→2v2→5v5 curriculum, frozen opponent league/self-play
7. runtime artifact promotion과 ShadowCoach

미구현 항목을 포트폴리오에서 완료로 주장하지 않는다.
