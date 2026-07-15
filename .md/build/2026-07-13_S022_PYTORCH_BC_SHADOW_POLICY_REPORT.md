# S022 PyTorch BC Shadow Policy 검증 보고서

날짜: 2026-07-13  
상태: **S022 source freeze / Handoff**  
범위: GameSim 실측 4-macro corpus, 결정론적 PyTorch masked BC, 고정 binary artifact, Server SHA fail-closed load, C++ shadow-only 추론, Snapshot/F9/Chrono 근거 표시

## 1. 완료 경계

S022는 다음 수직 슬라이스를 실제 코드와 검증으로 연결했다.

```text
30 Hz authoritative GameSim
  -> 0.20 s decision cadence의 typed AiDecisionTraceV1
  -> Accepted AiEpisodeV1 measured corpus
  -> offline CPU PyTorch masked behavior cloning
  -> canonical JSON report + WBCPOL1 little-endian .wbc
  -> Server startup path/SHA/schema fail-closed load
  -> 같은 trace를 C++에서 shadow-only rescore
  -> Snapshot newest-row join
  -> F9/Chrono active-final vs shadow 비교
```

학습 결과는 기존 `RuleBased`/`PlayerLike` intent, command, RNG, hold timer를 변경하지 않는다. S022 artifact의 명시적 상태는 `SHADOW_ONLY`, `MEASURED_EPISODES`, `OFFLINE_HOLDOUT_ONLY_NOT_PROMOTED`다. active learned command, 인간 command adapter, DAgger aggregation, PPO, recurrent memory, 2v2/5v5 league는 완료로 주장하지 않는다.

## 2. 실제 반영 파일

### Offline data/training

- `Tools/AIResearch/TrainImitationRankingBaseline.py`
  - 기존 NumPy pairwise 경로 보존
  - 명시적 `pytorch-masked-bc` backend 추가
  - 67차원 단일 feature order, legal-only normalization, masked cross entropy, CPU float64 full-batch 수동 gradient descent
  - canonical JSON report와 860-byte `WBCPOL1` float32 little-endian runtime artifact를 atomic write
  - unsafe promotion/RL claim, split leakage, malformed/non-finite artifact fail-closed
- `Tools/AIResearch/tests/test_train_imitation_ranking_baseline.py`
  - mask/tie/split/normalizer/deterministic bytes/corrupt binary/safety-claim 회귀
- `Tools/AIResearch/tests/RunLiveAiEpisodeSmokeProbe.ps1`
  - 네 scenario family, Blue/Red mirror, repeat A/B, promotion validation, corpus manifest 생성
- `Tools/AIResearch/RunValidation.ps1`
  - trainer A/B deterministic fixture와 기존 native/static contract 연결
- `Tools/AIResearch/README.md`
  - headless collection, offline training, artifact boundary, IL/RL 후속 gate 박제

### Native GameSim/SimLab

- `Tools/SimLab/main.cpp`
  - `fight|retreat|farm|siege` measured episode exporter
  - `.wbc` decode/parity CLI
  - legal mask/tie/invalid artifact/purity/cadence/300-tick non-interference probe
- `Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h/.cpp`
  - 67-feature/4-candidate artifact와 decision 계약
  - 명시적 little-endian decoder
  - legal candidate만 평가하는 bias-free linear scorer
  - selected margin과 winner-vs-runner-up top contribution
- `Shared/GameSim/Systems/ChampionAI/ChampionAISystem.h/.cpp`
  - 실제 command가 제출된 decision tick에만 동일 typed trace를 shadow 평가
  - 결과는 transient research sidecar에만 저장
  - authoritative legacy trace에 shadow 값을 넣지 않아 checkpoint/gameplay 비간섭 유지
- `Shared/GameSim/Components/ChampionAIComponent.h`
  - shadow status, revision/SHA, 네 logit, margin, top feature, disagreement evidence
- `Shared/GameSim/Components/JaxSimComponent.h`
  - raw POD keyframe에 들어가던 다섯 3-byte implicit padding을 명시적 zero-initialized reserved field로 치환
  - `sizeof == 60`, 다음 필드 offset `4/20/32/44/56` 고정

### Server load/lifetime

- `Server/Private/main.cpp`
  - `--ai-shadow-policy=<path>`와 `--ai-shadow-policy-sha256=<lowercase 64hex>`를 쌍으로 요구
  - file/SHA/binary contract 검증 실패 시 room 생성 전에 non-zero 종료
- `Server/Public/Game/GameRoom.h`, `Server/Private/Game/GameRoom.cpp`
  - immutable shared artifact lifetime 소유
- `Server/Public/Game/ServerAICommandProducer.h`, `Server/Private/Game/ServerAICommandProducer.cpp`
  - optional const artifact pointer를 GameSim AI에 전달
- `Server/Private/Game/GameRoomChampionAI.cpp`
  - authoritative bot phase에서 artifact 연결
- `Server/Private/Game/SnapshotBuilder.cpp`
  - authoritative newest row와 transient shadow row의 `(tick, commandSequence)`가 같을 때만 join

### Snapshot/F9/Chrono evidence

- `Shared/Schemas/Snapshot.fbs`
- `Shared/Schemas/Generated/cpp/Snapshot_generated.h`
- `Shared/Schemas/Generated/go/Shared/Schema/AIDebugTraceRow.go`
- `Client/Private/Network/Client/SnapshotApplier.cpp`
- `Client/Private/UI/AIDebugPanel.cpp`

F9에는 artifact status/revision/SHA prefix, `active-final -> shadow`, agree/disagree, Retreat/Fight/Farm/Siege raw logits, selected margin, 정확한 67-feature 이름과 signed top contribution이 표시된다. softmax를 계산하지 않으며 logit을 확률로 표기하지 않는다.

## 3. Seed 배분과 corpus 계약

Measured seed는 다음 식으로 고정했다.

```text
scenarioSeed = 42 + familyIndex * 10000 + scenarioIndex
familyIndex: fight=0, retreat=1, farm=2, siege=3
scenarioIndex: 0..7
```

| Family | Seed |
|---|---:|
| Fight | 42..49 |
| Retreat | 10042..10049 |
| Farm | 20042..20049 |
| Siege | 30042..30049 |

Blue/Red는 같은 scenario id, 같은 seed, 같은 frozen split group을 공유한다. side만 episode/run identity에서 다르다. 각 side는 같은 seed로 repeat A/B를 실행해 native trace, metadata, canonical JSONL을 비교한다. 즉 32개 독립 frozen scenario group이 32개 mirror pair, 64개 Accepted record가 된다.

학습 split은 `(scenario_id, rules_hash, definition_hash)`를 SHA-256으로 고정 분할한다. Blue/Red mirror가 train/holdout 양쪽으로 새지 않는다. 최종 split은 train 24 groups/48 records, holdout 8 groups/16 records, overlap 0이다. trainer seed `1729`는 계약에 기록되지만 zero initialization, full-batch, shuffle 없음이므로 sample order RNG에 의존하지 않는다.

## 4. 학습 수식

candidate `k`의 67차원 feature를 `x_k`, train legal-row mean을 `mu`, inverse scale을 `s`, 공유 weight를 `w`라 두면 C++와 Python의 logit은 다음과 같다.

```text
z_k = sum_i ((x[k,i] - mu[i]) * s[i] * w[i])
```

illegal candidate는 logit 경쟁과 loss에서 제외한다. 선택된 expert candidate를 `y`라 두면 decision별 loss는 다음이다.

```text
L_d = -z_y + log(sum_{k in Legal(d)} exp(z_k))
L = mean_d(L_d) + lambda * ||w||^2
```

S022 설정은 zero weight, CPU, float64 학습, full-batch 400 epoch, learning rate `0.08`, L2 `0.001`, thread 1이다. export 시 mean/inverse-scale/weight를 canonical float32로 고정한다. 이 단계는 규칙봇이 실제로 Accepted한 macro를 모방하는 IL/BC이며 reward를 최적화하는 RL이 아니다.

## 5. 최종 산출물과 SHA-256

최종 산출물 root: `C:\Users\user\AppData\Local\Temp\WintersS022Final2`

| 산출물 | Bytes | SHA-256 |
|---|---:|---|
| `Tools/Bin/Debug/SimLab.exe` | 6,228,992 | `91A9BA0A5EC2C04AFED66C68DA9D025E82355BEF32879ED4141593990C836BD7` |
| corpus A/B `ai_episode_v1.jsonl` | 118,450 | `98498BF4E2E3EEFF85B5F8C164A14F9CA52144109BB0785AC93B0F43CF8A4268` |
| policy A/B JSON | 12,730 | `77D1752F1D238E8DD00D1A9F3394EF2FFB74FE60317A56F43FDE411D5238AE34` |
| policy A/B `.wbc` | 860 | `AE3F6AEAF7D67D05038220866B375AC223F2072A6FA5B2123DE3E4D6ABF53B3D` |

추가 identity:

- internal policy SHA: `6be2da8a3bf44b5c94e76e18e5caf5135a3224a6148576508623c96829b4ba29`
- feature order SHA: `9208820578df2314c3c6bd9cb30ad6140e5555c412fb59ab71ded2352528ffa0`
- definition SHA: `1998ce31c9f5ca748c50dffcea117a28a17fbe1cf99e4a87744dc30a063345d6`
- 240-tick live smoke JSONL SHA: `BCD5373B69AF17E7DAAB1979B2570323126C1C616AC4FD36BC92A339C4A3145E`
- corpus manifest A/B SHA는 absolute output path를 포함해 각각 `35DACF0168CBD68AC80DCD5304D2F87CF830838388FD1B266338EBF09D088506`, `17754C3FF5B51C0391649CAF747CA893FE0D0CF08D144B36134F85CF3C57F441`이며 corpus bytes는 동일하다.

## 6. 계측 결과

- corpus: 64 records, 32 frozen/mirrored groups, Retreat/Fight/Farm/Siege 각 16, dropped 0
- train: 48 decisions, masked CE `0.00290223767235021`, top-1/top-3 `1.0/1.0`, selected-rank regret `0`
- holdout: 16 decisions, masked CE `0.002649042565457199`, top-1/top-3 `1.0/1.0`, selected-rank regret `0`
- Python logits: `[-0.144150327784, 6.84860820909, 0, 0]`
- C++ logits: `[-0.144150332, 6.84860802, 0, 0]`
- max absolute parity delta: `1.89092278724e-7` (`<= 1e-5` PASS)
- 300-tick shadow probe: 16 evaluated decisions, 강제 disagreement 확인, command/state/keyframe 비간섭 PASS
- Debug inference benchmark: 약 `3,849.2..4,734.1 ns/evaluation`; 제품 성능 수치가 아니라 Debug contract probe 계측

작은 bootstrap corpus가 의도적으로 네 class를 분리하므로 1.0 accuracy는 pipeline 계약 증거일 뿐 실전 일반화 성능 주장이 아니다. active canary 전 목표는 최소 10,000 Accepted decisions, 100 frozen groups, 실제 lane/fight conflict와 golden scenario다.

## 7. 빌드 및 검증

### Build

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' `
  Tools\SimLab\SimLab.vcxproj /t:Build `
  /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
```

결과: exit `0`. project dependency로 Engine, GameSim, Shared boundary, FlatBuffers codegen이 순차 실행됐다. 기존 C4275 DLL-interface warning 외 compile/link error는 없다.

### SimLab

```powershell
Tools\Bin\Debug\SimLab.exe 1800 42
```

연속 3회 모두 exit `0`.

- same-seed: `DB0DC85E451999AD`
- seed+1: `57A9B2394575042A`
- AIShadow raw keyframe equality 포함 3/3 PASS
- Recall regression: `retreat -> Recall` PASS
- MidDefense deterministic hash: `CBC15147CFE16804`

### Corpus/PyTorch/parity

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File Tools/AIResearch/tests/RunLiveAiEpisodeSmokeProbe.ps1 `
  -BuildMeasuredCorpus -MeasuredSeedsPerScenario 8 -OutputRoot <corpus_a|corpus_b>

python -B Tools/AIResearch/TrainImitationRankingBaseline.py `
  --backend pytorch-masked-bc --input <corpus> `
  --output <policy.json> --runtime-output <policy.wbc> `
  --policy-id s022-measured-shadow --policy-revision 2 --minimum-groups 10

Tools\Bin\Debug\SimLab.exe --verify-ai-shadow-policy <policy.wbc> <decision_trace_v1.bin>
```

결과: corpus A/B, policy JSON A/B, `.wbc` A/B byte-identical. promotion validation과 C++ parity PASS.

### Python/static

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/AIResearch/RunValidation.ps1
```

결과: exit `0`, Python `71/71`, deterministic fixture binary, research types, AiEpisode codec, influence map, replay journal, timeline rebase, Shared boundary PASS. 이 명령에서는 제품 C++ build를 의도적으로 skip했다.

## 8. Shadow false positive와 수정

첫 통합 실행에서 shadow off/on 명령 16개, cumulative authoritative hash `FA15DD93694DA25D`, final intent/action, evaluated/disagreed evidence는 같았지만 8,982-byte keyframe의 `JaxSimComponent` padding 한 바이트가 달라졌다. 원인은 `PodSave<T>`가 trivially-copyable component의 object representation 전체를 raw 저장하면서 미초기화 alignment padding도 포함한 것이었다.

검증 gate를 약화하지 않고 `JaxSimComponent`의 다섯 implicit gap을 명시적 zero-initialized reserved bytes로 바꿨다. field offset과 60-byte ABI는 유지했고 raw keyframe byte equality를 그대로 둔 채 연속 3회 PASS했다.

## 9. 통합 handoff

- S022 source와 `Tools/SimLab/main.cpp`는 handoff 가능 상태다.
- 현재 병렬 S023 UDP/JobSystem lane과의 build lock 합의 때문에 이 최종 source 상태에서 Server/Client/solution build와 visible full-map F9 캡처는 실행하지 않았다.
- Snapshot/F9 source slice는 앞선 독립 lane에서 Client Debug x64를 통과했지만, S023 통합 후 최종 Server -> Client -> solution 단일-lane 재빌드와 visible F9/Chrono 확인은 통합 owner가 수행해야 한다.
- Server/Client/solution 미실행을 S022 PASS로 둔갑시키지 않는다. 본 보고서의 Handoff는 SimLab/corpus/PyTorch/C++ shadow 계약 완료를 뜻한다.
- 기존 dirty 변경은 보존했고 commit은 만들지 않았다.
