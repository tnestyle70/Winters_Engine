Session - S024 54,000-tick handoff를 독립 확인한 뒤, IL/RL 확대 전에 실제 권위 GameRoom actor와 active-policy canary의 process-isolated 1/2/4/8 처리량, 결정론, corpus/PyTorch 경계를 검증했다.

# S029 AI Actor Throughput and Multi-Room Scaling Preflight

## 1. 최종 판정

- **S024 5v5 장기 안정성 전제: GO.** 54,000 tick A/B, replay payload, world/final-state, lifecycle 및 process-zero handoff를 다시 확인했다.
- **한 호스트의 process-isolated `CGameRoom` actor farm: GO.** 1/2/4/8 actor 모두 PASS했고 8 actor aggregate throughput은 6,002.369 world-tick/s, 60,023.689 scheduled bot-step/s였다.
- **8 actor 권장치: GO for this host/preflight.** 20 logical processors에서 8 actor parallel efficiency 76.655%, 최악 p99 4.971 ms, deadline miss 0이었다. 16 actor는 측정하지 않았다.
- **same-process multi-room: NO-GO.** 이번 결과는 room당 프로세스 하나이며 shared static/hook registry와 single-room network hub를 검증하지 않는다.
- **networked multi-room: NO-GO.** external client, TCP/UDP session fanout, loss/reorder는 범위 밖이다.
- **PyTorch BC contract: GO, promotion: NO-GO.** 64-record bootstrap corpus에서 deterministic masked BC와 C++ parity를 검증했지만 `SHADOW_ONLY`, `OFFLINE_HOLDOUT_ONLY_NOT_PROMOTED`다.
- **대규모 IL/RL/PPO/self-play 시작: 아직 NO-GO.** 실제 5v5 decision-native trajectory exporter, 정확한 decision/evaluation counter, persistent actor loop, reward/league gate가 아직 없다.

보이지 않는 headless 서버 권위 simulation을 여러 개 병렬로 돌릴 계산 경로는 확인했다. 그러나 이번 실행에서 봇이 온라인으로 RL을 학습한 것은 아니다. GameSim이 데이터를 만들고, Python/PyTorch가 그 데이터를 오프라인으로 학습하고, 고정 `.wbc`를 다시 C++ shadow inference로 검증하는 단계까지만 실제 수행했다.

## 2. 검증 범위와 권위 경계

```text
Process-isolated GameRoom actor
  -> 실제 CGameRoom + 10 ChampionAI + minion/structure/combat
  -> Snapshot/Event/Replay/Checkpoint 포함
  -> 같은 seed의 replay/world/final-state hash 비교

Debug active-policy actor
  -> SimLab 1v1 checkpoint two-pass canary
  -> policy evaluate/intervention/safety override
  -> native 528-byte trace + metadata
  -> Python exporter/validator를 batch timer 밖에서 fail-closed 실행

Offline learner
  -> promotion-valid AiEpisodeV1 corpus
  -> deterministic PyTorch masked BC
  -> fixed float32 .wbc
  -> C++ shadow decode/logit parity
```

권위 flow는 계속 다음을 보존한다.

```text
Observation -> Candidate/ActionMask -> policy score
-> GameCommand -> Server GameSim executor
-> Snapshot/Event/Replay/Trace
```

Python은 transition이나 전투 결과를 계산하지 않는다. 게임 결과는 C++ GameSim만 만든다. match 도중 Python이 weight를 실시간 덮어쓰지도 않는다. 학습 산출물은 match 경계에서 SHA와 schema를 검증한 뒤 shadow/canary로만 로드한다.

## 3. S024 완료 훅 독립 감사

- Release 54,000 tick seed 42 A/B: PASS/PASS
- replay/world: `F172FA227ACA7576` / `3B12304F5110E999`
- final-state SHA-256: `EA8CBF81223AB0A31B785596CD31562F5973B100AFBC62BDD3AAD9765982C19F`
- p99: 3.440/3.551 ms, deadline miss 6/0
- replay 각 143,018 records, 1,420,783,432 bytes
- source freeze, build-lock handoff, `msbuild/cl/link/soak/SimLab/python = 0`

WRPL 전체 파일은 48-byte header의 `createdUnixMs` 때문에 byte-identical 대상이 아니다. record payload는 두 run에서 동일했다.

- payload SHA-256 A/B: `B61B29BF4186CAD37161FC37A6D928B48A903E9488FEFBE2823DCB876EE67C85`
- full WRPL SHA-256: Run 1 `CFDC2F...43C0`, Run 2 `FFD687...792CA`
- S024 report SHA-256: `2BB848F1...788D`
- S024 work packet SHA-256: `899805BE...24E3`

따라서 S029는 S024의 장기 gameplay/replay 안정성을 다시 돌리는 대신 그 위의 actor scaling preflight를 실행했다.

## 4. S029 구현 산출물

### 4.1 GameRoom actor runner

`Tools/Harness/RunGameRoomActorScalingPreflight.ps1`

- room당 Release harness 프로세스 하나, actor count `1,2,4,8`
- 동일 seed replica와 고유 room ID
- child watchdog, ExitCode, async stdout/stderr drain, 강제 종료 후 wait/dispose
- aggregate RSS/private/handle 10 ms sampled peak
- replay/world/final-state 동일성 및 exact uppercase 16-hex gate
- existing p99/max/deadline/liveness 결과 수집
- deterministic artifact와 wall-clock 성능 JSON 분리

SHA-256: `18964CECDF294FDF7D3586B6C106A590DD5D032A24E4F1A42ED7BD832F1BEA18`

### 4.2 Active-policy actor runner

`Tools/AIResearch/tests/RunActiveAiActorScalingPreflight.ps1`

- Debug SimLab active canary process `1,2,4,8`
- policy/seed/tick/scenario/revision/episode/timeline provenance gate
- transition accounting 및 final truncated boundary gate
- native trace size `transition_count * 528`
- 각 actor에 기존 `ExportAiEpisodeV1.py`와 `ValidateAiEpisode.py` 재사용
- deterministic trace/metadata/report/JSONL/final-state 비교
- JSONL export/validation은 throughput timer 밖에서 수행
- expert dataset 사용 금지를 report에 명시

SHA-256: `8BDE52AC4D55DD78B5786B6477FA5ABF64A31867DD7D65DD2B21386C410FF6E6`

두 runner는 PowerShell AST 0 error, trailing whitespace 0, 공백이 포함된 `OutputRoot` 1-actor smoke PASS, 독립 P0/P1 재검토 PASS다.

## 5. 정확한 빌드·검증 명령

### Freshness build

```powershell
MSBuild.exe Server\Include\Server.vcxproj /m:1 /nr:false /t:Build /p:Configuration=Release /p:Platform=x64 /verbosity:minimal

powershell -NoProfile -ExecutionPolicy Bypass `
  -File .\Tools\Harness\RunGameRoomBotMatchSoak.ps1 `
  -Configuration Release -TickCount 300 -Seed 42 -Runs 1 `
  -HeartbeatTicks 300 -PrivateLimitMiB 1024 -SkipServerBuild

MSBuild.exe Tools\SimLab\SimLab.vcxproj /m:1 /nr:false /t:Build /p:Configuration=Debug /p:Platform=x64 /verbosity:minimal

.\Tools\Bin\Debug\SimLab.exe 1800 42
```

### Active contract와 actor matrix

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\Tools\AIResearch\tests\RunActiveAiPolicyEpisodeProbe.ps1 `
  -Seed 42 -TickLimit 300 -OutputRoot <evidence>\active_contract_final

.\Tools\Harness\RunGameRoomActorScalingPreflight.ps1 `
  -TickCount 1800 -Seed 42 -ActorCounts '1,2,4,8' `
  -HeartbeatTicks 1800 -PrivateLimitMiB 1024 -TimeoutSeconds 120 `
  -OutputRoot <evidence>\game_room

.\Tools\AIResearch\tests\RunActiveAiActorScalingPreflight.ps1 `
  -TickLimit 300 -Seed 42 -ActorCounts '1,2,4,8' `
  -TimeoutSeconds 120 -OutputRoot <evidence>\active_scaling
```

### Contract, corpus, learner, parity

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\Tools\AIResearch\RunValidation.ps1 `
  -WintersRoot C:\Users\user\Desktop\Winters `
  -NypcRoot C:\Users\user\Desktop\NYPC

powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\Tools\AIResearch\tests\RunLiveAiEpisodeSmokeProbe.ps1 `
  -Seed 42 -BuildMeasuredCorpus -MeasuredSeedsPerScenario 8 `
  -OutputRoot <evidence>\measured_corpus `
  -MeasuredCorpusOutput <evidence>\measured_corpus\measured_ai_episode_v1.jsonl

python -B Tools/AIResearch/TrainImitationRankingBaseline.py `
  --backend pytorch-masked-bc `
  --input <measured_ai_episode_v1.jsonl> `
  --output <policy_a.json> --runtime-output <policy_a.wbc> `
  --policy-id s029-measured-shadow --policy-revision 2 --minimum-groups 10

.\Tools\Bin\Debug\SimLab.exe `
  --verify-ai-shadow-policy <policy_a.wbc> <decision_trace_v1.bin>
```

Python parity 계산은 같은 promotion-valid JSONL을 `LoadPromotionEpisodes`와 `BuildMaskedBCExamples`로 feature화하고, report의 mean/inverse-scale/weight를 float32로 양자화한 뒤 C++과 같은 float64 accumulation 및 float32 output cast를 적용했다.

## 6. Freshness와 기본 회귀 결과

- Release Server build: PASS
  - SHA-256: `92FB25176E4C1022D1E6FD273ECFEBB94A20132BAB13E618AA347BB7DFA4348E`
  - 기존 C4275 DLL-interface warning 1건, compile/link error 0
- Release GameRoom harness freshness: PASS
  - SHA-256: `9EC4C60B6F5FD232BECF712622F713DAD9F2C41E17FE23C62218DBB15F7EB5D0`
- Debug SimLab build: PASS
  - SHA-256: `E9177E894DA67838215251FE1D9210A28959CA6FFCA477AA787593B46C7F04D5`
- SimLab 1,800 tick seed 42: PASS
  - same-seed `DB0DC85E451999AD`, seed+1 `57A9B2394575042A`
  - retreat -> Recall 및 AIShadow raw keyframe non-interference PASS
- active A/B: 각 41 transitions, 41 evaluations, 21 applied interventions, 20 safety overrides
  - JSONL SHA-256 `DB6377084EBEE067A86BAD4CBFCEB19A759CAE249D8D86FF8A23BDD00A5A033C`
  - report SHA-256 `874A683B6A405EB749ABE79C07949FA94633B81006CA7A10CA0CDE03F758E1DB`
  - truncated artifact negative probe fail-closed PASS
- AIResearch contract: Python 80/80, C++ trace/codec/influence/replay/timeline/shared-boundary PASS

## 7. 실제 GameRoom 1/2/4/8 결과

각 actor는 1,800 world ticks, seed 42, 10 bots다. `bot-step/s`는 `world-tick/s * 10` scheduled value이며 실제 policy decision 수가 아니다. `command proxy/s`도 final command sequence sum을 wall time으로 나눈 값이다.

| actor | batch sec | world-tick/s | bot-step/s | command proxy/s | speedup | efficiency | sampled private MiB | max p99/max ms | miss |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 1.839 | 978.793 | 9,787.933 | 1,397.499 | 1.000 | 1.000 | 22.539 | 2.028/13.921 | 0 |
| 2 | 1.886 | 1,909.204 | 19,092.038 | 2,725.919 | 1.951 | 0.975 | 44.605 | 2.035/8.007 | 0 |
| 4 | 2.097 | 3,434.162 | 34,341.616 | 4,903.220 | 3.509 | 0.877 | 88.488 | 2.007/15.643 | 0 |
| 8 | 2.399 | 6,002.369 | 60,023.689 | 8,570.049 | 6.132 | 0.767 | 176.949 | 4.971/28.011 | 0 |

공통 deterministic result:

- replay hash `AE0D740A55082FB6`
- world hash `D12B7306C1B70685`
- final-state SHA-256 `7BDE3420172DB09CC48ECDB4A15C59490E53B393CD60078642C00C6E09BF0EDF`
- actor별 max stop 0.040 sec, max steady handle delta 8, max private growth 0.465 MiB
- aggregate throughput monotonic within 10% tolerance: true
- 50% efficiency 기준 권장 actor count: 8

1,800-tick 짧은 horizon은 S024 54,000-tick 후반의 entity/replay 누적 비용이 작다. 8 actor의 6,002 tick/s를 장기 54,000-tick 속도로 그대로 외삽하면 안 된다.

## 8. Active-policy 1/2/4/8 결과

이 수치는 Debug checkpoint two-pass canary이며 production actor 처리량이 아니다. actor 하나당 300 requested/completed ticks와 41 transitions다. exporter/validator 시간은 batch timer에서 제외했다.

| actor | batch sec | tick/s | transitions | transition/eval/s | speedup | efficiency | sampled private MiB |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 0.569 | 526.988 | 41 | 72.022 | 1.000 | 1.000 | 220.348 |
| 2 | 0.520 | 1,152.809 | 82 | 157.551 | 2.188 | 1.094 | 449.918 |
| 4 | 0.606 | 1,978.731 | 164 | 270.427 | 3.755 | 0.939 | 818.234 |
| 8 | 0.607 | 3,953.631 | 328 | 540.330 | 7.502 | 0.938 | 1,486.922 |

Deterministic hashes:

- native trace `95D50D2F13D40E08344AC8ACDC58989427B4A8A524A2CB11DC934C4CDA07636B`
- metadata `8B131FE9A9D628CEDC86926B49CDA094E2F1225C5CD9699555707D572E8C300A`
- canary report `874A683B6A405EB749ABE79C07949FA94633B81006CA7A10CA0CDE03F758E1DB`
- validated JSONL `DB6377084EBEE067A86BAD4CBFCEB19A759CAE249D8D86FF8A23BDD00A5A033C`
- final state `78ebb74e78272343`

이 episode는 `EVALUATION_AND_DAGGER_STATE_DISCOVERY_ONLY`이며 `eligible_as_imitation_expert_input=false`다. 학습 policy가 선택한 행동을 다시 expert label로 자동 사용하지 않는다.

## 9. Seed 배분과 데이터 의미

현재 isolation gate는 모든 actor에 seed `42`를 주고 room ID만 `290000 + actor_count * 100 + actor_index`로 달리했다. room ID가 달라도 authoritative hash가 동일하므로 actor index, room ID, launch order가 RNG에 섞이지 않았다.

Measured corpus seed는 다음과 같다.

```text
scenarioSeed = 42 + familyIndex * 10000 + scenarioIndex
family = fight/retreat/farm/siege
scenarioIndex = 0..7
Blue/Red mirror = same scenario seed and same frozen split group
```

다음 production actor-farm에서는 아래 계약을 제안한다. 아직 구현된 allocator는 아니다.

```text
matchSeed = Low64(SHA256(
  "WintersActorSeedV1\0" ||
  experimentManifestSha256 ||
  UInt64LE(globalEpisodeOrdinal)))
```

worker ID나 완료 순서로 seed를 만들지 않는다. 중앙 ordinal로 seed set을 고정하면 actor 수가 바뀌어도 같은 experiment가 같은 episode 집합을 가진다. policy exploration RNG는 combat RNG와 분리하고 `(matchSeed, subsystem, entityNetId, tick, decisionOrdinal, purposeTag)` named key를 사용해야 한다.

## 10. Corpus와 PyTorch 결과

Measured corpus:

- scope `RETREAT_VS_ONE_MACRO_BOOTSTRAP`
- records/groups/mirrors 64/32/32
- class retreat/fight/farm/siege 각 16
- legal mask `0x3=16`, `0x5=32`, `0x9=16`, dropped 0
- corpus SHA-256 `1A823A360D790D7ACD8A277B382C1D35179F3C6BE3C74DEA52EFC475D5C8F8C6`
- manifest SHA-256 `8A08DD2ACC449A3750F1165414FB19FD1CA6AF861AB380ECDDC99D73C3ED7038`

PyTorch masked BC A/B:

- train/holdout decisions 48/16, group overlap 0
- train top1/NCE 1.0 / 0.0027200178
- holdout top1/NCE 1.0 / 0.0046135448
- policy report A/B SHA-256 `74FA508B450ADBC4CD4101ABE442076CD4D9AD4A4BC91323B8549B5C2B220005`
- runtime `.wbc` A/B SHA-256 `20B810F375FA15C2379DD7F4DBFFC0FC6CF6914817592257D0F120FD63014022`
- canonical policy identity `45af2e735a483f2be7b2c310c972259e8d6765af698419fb8d62b3c9893dbdd1`
- train wall 7.820/7.332 sec
- Python/C++ first live decision logits `-0.522450447, 6.36946487, 0, 0`
- max absolute logit delta `0` (`<= 1e-5` PASS)

top1 1.0은 작은 2-way bootstrap fixture에서의 offline 결과다. 실제 lane/teamfight 실력이나 promotion 증거가 아니다.

## 11. 발견·수정한 runner 결함

1. 이 호스트의 `Start-Process` 반환 object가 child ExitCode를 보존하지 않았다. `System.Diagnostics.Process`와 async stdout/stderr drain으로 교체했다.
2. timeout/validation exception의 고아 process 가능성을 kill, wait, output flush, dispose와 top-level finally로 닫았다.
3. 공백 path native argv를 명시 quote하고 실제 공백 `OutputRoot` smoke로 검증했다.
4. active report 누락 property가 numeric zero cast로 통과하던 경계를 mandatory provenance/counter/boundary 검사로 닫았다.
5. raw trace 길이만 맞으면 통과하던 경계를 기존 Python codec/exporter/validator 재사용으로 닫았다.
6. GameRoom replay/world hash를 exact `[0-9A-F]{16}`으로 강제했다.

## 12. 다음 gate

1. **Decision-native 5v5 actor capture**: finalized decision/evaluation/accepted/rejected/command counter와 528-byte trace를 persistent batch로 수집한다.
2. **Persistent actor pool**: 한 process가 여러 episode를 연속 실행하고 manifest ordinal seed allocator를 적용한다.
3. **실측 IL corpus**: 최소 10,000 Accepted decisions, 100+ frozen groups. Human/Chrono correction만 sidecar expert label로 사용한다.
4. **1v1 lane commitment BC/DAgger**: Farm/Retreat/Recall/Fight와 Q/W/E/R/BA executor 결과를 분리 평가한다.
5. **2v2 support/target focus curriculum**을 거친다.
6. **5v5 teamfight rollout/self-play league**는 reward, credit assignment, frozen opponent pool, promotion gate 뒤에 PPO를 시작한다.

눈으로 보는 Client 검증은 매 episode마다 필요하지 않다. headless actor가 seed와 GameState로 corpus를 만들고, promotion 후보나 golden failure만 F9 trace/Chrono branch로 재생해 사람이 확인하는 방향이 맞다.

## 13. Evidence와 handoff

정본 evidence root:

`C:\Users\user\Desktop\Winters\.md\build\evidence\s029_ai_actor_scaling\20260714_042900_final`

- `game_room/game_room_actor_scaling_v1.json`
  - SHA-256 `CBBDA4A6C744EF19B2B728BD0C67A030C15BA0A00649FC98FE26C43B0A2CD196`
- `active_scaling/active_ai_actor_scaling_v1.json`
  - SHA-256 `86EC8608E2E62A9520A9274FB211ECE25940B04044CCA7383B771F193707001B`
- `measured_corpus/measured_ai_episode_v1.jsonl`
- `pytorch/policy_a.json`, `pytorch/policy_a.wbc`

Evidence 총 633 files, 293,816,770 bytes다. 기존 dirty 변경은 reset/revert/stage/commit하지 않았다. 최종 source는 freeze 상태이며 마지막 확인에서 `msbuild/cl/link/GameRoomBotMatchSoak/SimLab/python = 0`이었다.
