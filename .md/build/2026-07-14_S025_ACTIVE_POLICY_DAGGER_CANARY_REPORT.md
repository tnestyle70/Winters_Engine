# S025 Active Policy / DAgger Canary 검증 보고서

날짜: 2026-07-14
상태: **Source Freeze / Handoff**
범위: SimLab Debug 1v1 learned-macro active canary, immutable human correction provenance, corrected-only IL/DAgger dataset, 결정론적 산출물 검증

## 1. 완료 결과

이번 슬라이스는 NYPC의 `seed + state + action/policy injection` 방식을 Winters의
실제 권위 GameSim 경로에 연결했다.

```text
고정 seed와 authoritative GameState
  -> 기존 CChampionAISystem이 typed 후보/관측 생성
  -> immutable WBCPOL1 artifact가 legal 후보만 rescore
  -> checkpoint restore 후 learned macro를 one-shot Debug action으로 적용
  -> GameCommand -> CDefaultCommandExecutor
  -> 최종 trace/reward/boundary를 AiEpisodeV1로 export
  -> 동일 입력 repeat A/B의 파일 SHA-256 비교
```

Python/PyTorch가 Client frame 안에서 실행되거나 매 tick weight를 hot-swap하지
않는다. Python은 process 밖에서 dataset materialization, offline training, artifact
생성과 검증을 담당하고, runtime C++은 고정 binary artifact만 읽는다.

## 2. Active two-pass 원리

`--run-ai-active-macro-episode`는 실제 decision boundary마다 다음 순서를 지킨다.

1. world/RNG/NetId/tick keyframe을 저장한다.
2. 첫 pass의 `CChampionAISystem`이 같은 typed trace를 WBC로 평가한다.
3. 첫 pass command는 executor에 전달하지 않는다.
4. keyframe을 transactional restore하고 bot/enemy를 NetId로 다시 찾는다.
5. 제안된 `Retreat|Fight|Farm|Siege`를 one-shot `AIDebugControl`로 주입한다.
6. 두 번째 pass의 최종 command만 executor에서 실행하고 final trace만 보존한다.

따라서 preflight가 world를 한 번 더 진행시키지 않으며, learned proposal과 기존
rule-policy가 같은 상태에서 비교된다. emergency retreat, recall, action lock 같은
기존 안전 우선순위가 learned proposal보다 앞에 있으면 최종 행동을 막을 수 있고,
그 횟수는 `safety_override_count`로 별도 계수한다.

일반 runner는 agreement-only 정책도 정상 episode로 허용한다. 전용 probe만
의도적으로 Retreat를 선호하는 860-byte canary WBC를 만들어 최소 한 번의 실제
disagreement와 applied intervention을 요구한다.

## 3. 수식과 seed

후보 `k`의 67차원 feature를 `x[k,i]`, artifact normalizer를 `mu[i]`, `s[i]`,
공유 linear weight를 `w[i]`라 두면 C++ logit은 다음이다.

```text
z[k] = sum_i ((x[k,i] - mu[i]) * s[i] * w[i])
a = argmax_{k in LegalMask} z[k]
```

illegal candidate는 argmax에서 제외하고 동점은 candidate kind가 작은 쪽을 고른다.
이번 canary artifact는 성능 모델이 아니라 active 경로 검증용으로
`candidate_kind_1(Retreat)=+32`, 다른 kind one-hot은 `-32`로 고정했다.

검증 seed는 `42`다. scenario의 결정론적 변화량은 다음과 같이 만든다.

```text
seedUnit = (seed mod 17) / 16
enemyDistance = 3.5 + seedUnit
minionDistance = 4.25 + 0.5 * seedUnit
selfHpRatio = 0.78 + 0.18 * seedUnit
enemyHpRatio = 0.68 + 0.20 * (1 - seedUnit)
```

Blue/Red는 좌표 방향과 team을 mirror할 수 있으며 같은 seed가 같은 parameter
scenario를 뜻한다. 이번 probe는 Blue/42 repeat A/B를 사용했다.

decision `t`의 observable HP advantage를

```text
Vhp(t) = selfHpRatio(t) - enemyHpRatio(t)
```

라 두고 다음 실제 decision 직전까지의 interval reward를

```text
r(t) = Vhp(nextDecision) - Vhp(t)
```

로 기록한다. 마지막 decision은 terminal 또는 time-limit state까지 계산한다.
따라서 episode return은 구간 합이며 같은 경계에서는 telescope한다. 이 reward는
canary 계측값이지 아직 PPO reward contract가 아니다.

## 4. IL/DAgger provenance

원본 `AiEpisodeV1`의 선택, command, executor 결과, reward, next-state는 실제로
일어난 사실이므로 사람이 고쳐 쓰지 않는다. `AiDecisionCorrectionSidecarV1`이
source JSONL SHA, canonical record SHA, timeline identity, 원래/교정 후보를 묶고,
`ImitationDecisionV1`은 원본 record와 별도 expert label을 함께 보존한다.

```text
규칙/전문 actor의 Accepted label:
  SOURCE_ACCEPTED / SOURCE_COMMAND_ACCEPTED

사람의 반사실 교정:
  HUMAN_DEBUG_CORRECTION / UNEXECUTED_COUNTERFACTUAL
```

active learned-policy가 실행한 행동은 Accepted여도 expert가 아니다. 따라서 active
rollout에는 `MaterializeImitationDataset.py --corrected-only`를 사용한다. 이 mode는
사람이 실제로 바꾼 행만 출력하고 미검토 source action은 제외하며, 교정이 0건이면
fail-closed한다. active JSONL을 직접 BC trainer에 넣지 않는다.

## 5. 변경 파일

### SimLab

- `Tools/SimLab/main.cpp`
  - transition별 terminal/truncated 경계
  - policy full SHA-256 read/decode fail-closed
  - checkpoint two-pass active macro runner와 canary report
  - evaluation/agreement/intervention/applied/safety counter 불변식

### AIResearch data/training

- `Tools/AIResearch/AiImitationDatasetSchema.py`
- `Tools/AIResearch/MaterializeImitationDataset.py`
- `Tools/AIResearch/TrainImitationRankingBaseline.py`
- `Tools/AIResearch/fixtures/ai_decision_correction_sidecar_v1_golden.json`
- `Tools/AIResearch/tests/test_materialize_imitation_dataset.py`
- `Tools/AIResearch/tests/test_train_imitation_ranking_baseline.py`

### Probes/docs

- `Tools/AIResearch/tests/BuildActiveMacroCanaryPolicy.py`
- `Tools/AIResearch/tests/RunActiveAiPolicyEpisodeProbe.ps1`
- `Tools/AIResearch/RunValidation.ps1`
- `Tools/AIResearch/README.md`
- `Tools/AIResearch/AI_EPISODE_V1.md`

Server, Shared, Client, Engine, `Tools/Harness`, S024 work packet은 이번 슬라이스에서
수정하지 않았다.

## 6. 산출물과 SHA-256

Active output root:
`C:\Users\user\AppData\Local\Temp\WintersS025ActiveFinal-64c4413eb13447a5b972acc0ebcd8341`

| 산출물 | SHA-256 |
|---|---|
| `SimLab.exe` | `677E657C0075F92F3FD64175247605198C6FBB5E7DCAB99CD54FF3D08275477B` |
| canary WBC A/B | `51D9A1D99B8FF2D355B0AD831AC83BC8704DC038864536D4A4D3182D357C4085` |
| active native trace A/B | `95D50D2F13D40E08344AC8ACDC58989427B4A8A524A2CB11DC934C4CDA07636B` |
| active metadata A/B | `8FE400566D79D00653A344E1C1CA4240ACE4C8B74CBD7F5CA37CEE5EB15729B8` |
| active canonical JSONL A/B | `043F2AA728A80CA8F8B544C8C05668D2835642B334033FF6E0F3DCCDC5B6AD04` |
| active canary report A/B | `874A683B6A405EB749ABE79C07949FA94633B81006CA7A10CA0CDE03F758E1DB` |
| corrected-only fixture A/B | `DF7C3E3FF52C7202EF6D519F197F4C2BECF6DBB89CF7FA519E2DC9E27F26094D` |

기존 live regression output root:
`C:\Users\user\AppData\Local\Temp\WintersS025LiveRegression-647c3de2c8b44e18beda066ebf08597e`

| 산출물 | SHA-256 |
|---|---|
| live native trace A/B | `3457280FE6B9E8B98A2FAFA0BEB40E8E810A118A82D387F5055498B8DA1319D8` |
| live metadata A/B | `AF6BDAC1511C3785F7FB227082A36A8B5574DDB5BB48CAED3AEDA58F0FA32335` |
| live canonical JSONL A/B | `234C60E314AB0461FA8DF25EE1D24880F1B52B20F33ACA3379089694F7144FFE` |

## 7. 계측 결과

```text
side=blue
seed=42
tick_limit=300
completed_tick=300
transition_count=41
evaluated=41
agreement=0
intervention=41
applied_decision=21
applied_intervention=21
safety_override=20
fallback=2
rejected_command=0
episode_return=-0.0908450484
terminal=false
truncated=true
outcome=time_limit
final_state_hash=78EBB74E78272343
```

counter 불변식도 성립한다.

```text
evaluated = agreement + intervention = 41
intervention = applied_intervention + safety_override = 41
applied_intervention <= applied_decision <= evaluated
```

## 8. 빌드·검증

### SimLab Debug x64

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' `
  Tools/SimLab/SimLab.vcxproj /t:Build `
  /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
```

결과: PASS. `SimLab.exe` 6,273,536 bytes. S024와 build-lock을 명시적으로
인수·반환했으며 종료 후 `msbuild/cl/link/SimLab/python` process는 0이었다.

### Active probe

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File Tools/AIResearch/tests/RunActiveAiPolicyEpisodeProbe.ps1 `
  -Seed 42 -TickLimit 300
```

결과: 두 실행의 네 산출물 SHA 완전 일치. 859-byte truncated WBC는 자신의 정확한
SHA를 전달해도 decode에서 exit 1, output directory 미생성으로 fail-closed했다.

### 기존 회귀

- 240-tick live episode repeat A/B: 8 records, raw/promotion validation PASS
- `SimLab.exe 1800 42`: PASS
  - same seed: `DB0DC85E451999AD`
  - seed+1: `57A9B2394575042A`
  - retreat -> Recall, FOW/last-seen, MidDefense, keyframe, shadow non-interference 포함
- Python unittest: `80/80 PASS`
- PowerShell parse: PASS
- Python AST: PASS
- trailing whitespace: 0
- scoped `git diff --check`: PASS (existing LF/CRLF conversion warning만 출력)

## 9. 완료 주장 제한과 다음 gate

완료한 것은 Debug SimLab 1v1에서 learned macro가 실제 authoritative command
경로에 들어갈 수 있음을 증명한 canary와 안전한 IL correction boundary다.

완료하지 않은 것은 다음이다.

- production Server active policy selection/promotion
- runtime 자동 action-origin과 F9 correction journal
- terrain LOS/network FOW
- dense per-tick PPO/GAE trajectory와 value/hidden state
- 1v1 recurrent PPO, 2v2 curriculum, 5v5 self-play league
- 실제 Client full-map learned-policy 눈검증

다음 순서는 S024의 10-bot 54,000-tick server-authority 안정성 gate를 먼저 닫고,
그 위에 `human correction capture -> corrected-only DAgger aggregation -> shadow
candidate -> frozen/golden promotion`을 연결한 뒤 1v1 PPO로 올라가는 것이다.
