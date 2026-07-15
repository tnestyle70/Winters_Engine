Session - S017/S021의 typed decision trace와 Chrono timeline 위에 실제 GameSim 실측 corpus, 결정론적 PyTorch masked behavior cloning, versioned runtime artifact, Server startup SHA 검증, C++ shadow 추론, F9 선택 근거 비교를 하나의 비간섭 수직 슬라이스로 연결한다.

날짜: 2026-07-13. 작업 packet: `2026-07-13_s022_pytorch_bc_shadow_policy`. 기준: `main@9110091`. S021과 그 이전의 대규모 dirty 변경을 reset/revert하지 않고 현재 exact anchor를 인수하며 commit은 만들지 않는다.

현재 코드 증거는 다음과 같다. `Shared/GameSim`은 30 Hz authoritative transition을 소유하고 Champion AI의 high-level decision cadence는 0.20초(5 Hz)다. `AiEpisodeV1`은 Retreat/Fight/Farm/Siege 네 macro candidate, legal mask, Accepted executor 결과를 canonical JSONL로 보존한다. `TrainImitationRankingBaseline.py`에는 67차원 누수 방지 feature와 frozen scenario group split이 있지만 현재 NumPy pairwise report-only baseline이며 PyTorch와 runtime policy가 아니다. 240-tick live smoke는 계약 검증을 위해 마지막 tick을 강제 Accepted로 만드는 1개 고정 scenario라 measured corpus로 사용하면 안 된다. `ChampionAIResearchDebugComponent`는 checkpoint에서 제외되는 transient sidecar이고 S021 wire는 authoritative 16행 ring 중 최신 1행만 전송하여 F9가 branch별 bounded history를 쌓는다. 따라서 학습 결과를 authoritative command에 연결하기 전에 이 sidecar에서 exact same typed observation을 decision당 한 번 rescore하는 것이 최소 안전 경계다.

## 1. 반영해야 하는 코드

### 1-1. `Tools/AIResearch/TrainImitationRankingBaseline.py`

기존 NumPy pairwise 기본 경로와 artifact를 그대로 보존하고 `--backend pytorch-masked-bc`를 명시한 경우에만 PyTorch 경로를 연다. module import 시 torch를 강제하지 않고 PyTorch backend 함수 안에서 import하여 기존 계약 검증은 torch가 없는 환경에서도 계속 작동하게 한다.

현재 `FeatureOrder` 67개와 `BuildFeatureVector`를 단일 source of truth로 재사용한다. decision `d`, candidate `k`의 식은 다음으로 고정한다.

```text
x[d,k] in R^67
x_hat[d,k] = (x[d,k] - mean_train_legal) / scale_train_legal
z[d,k] = dot(w, x_hat[d,k])
z[d,k] = -infinity, if candidate k is illegal
L_BC = mean_d(-log softmax(z[d,*])[expert_d]) + (lambda / 2) * ||w||^2
```

`w`는 bias 없는 shared linear candidate scorer다. candidate kind one-hot과 kind-conditioned context block 때문에 action별 다른 선형식이면서도 기존 feature contract와 Python/C++가 정확히 일치한다. authored candidate score/contribution, selected flag, command, executor, reward, next-state hash, raw NetId 수치는 입력하지 않는다.

`TrainingConfig` 아래에 masked example/tensor 자료형을 추가한다.

```python
@dataclass(frozen=True)
class MaskedBCExample:
    split_group: SplitGroup
    decision_group: tuple[Any, ...]
    features: np.ndarray
    legal_mask: np.ndarray
    selected_index: int


@dataclass(frozen=True)
class MaskedBCRuntimeArtifact:
    report: dict[str, Any]
    binary_bytes: bytes
    binary_sha256: str
```

`BuildDecisionExamples` 아래에 `BuildMaskedBCExamples`, `FitMaskedNormalizer`, `BuildMaskedTensors`, `TrainTorchMaskedBC`, `EvaluateTorchMaskedBC`, `BuildTorchPolicyArtifact`, `WriteRuntimePolicyBinary`를 추가한다. 결정론 범위는 동일 input/config + 동일 Python/NumPy/PyTorch 버전 + CPU로 한정한다.

```python
torch.use_deterministic_algorithms(True, warn_only=False)
torch.set_num_threads(1)
torch.manual_seed(config.seed)

weights = torch.zeros(len(FeatureOrder), dtype=torch.float64, device="cpu",
                      requires_grad=True)
for _ in range(config.epochs):
    logits = torch.einsum("dcf,f->dc", normalized_features, weights)
    logits = logits.masked_fill(~legal_mask, -torch.inf)
    loss = torch.nn.functional.cross_entropy(logits, labels)
    loss = loss + 0.5 * config.l2 * torch.dot(weights, weights)
    gradient, = torch.autograd.grad(loss, weights)
    with torch.no_grad():
        weights -= config.learning_rate * gradient
```

DataLoader/shuffle/AMP/CUDA/optimizer state/pickle/`torch.save`는 사용하지 않는다. zero initialization, full-batch manual update, legal-only normalizer, 낮은 candidate kind tie-break를 artifact에 기록한다.

PyTorch report는 canonical JSON `PolicyArtifactV1`로 쓴다. 필수 top-level과 안전 claim은 다음과 같다.

```python
{
    "schema_version": 1,
    "artifact_type": "PolicyArtifactV1",
    "policy_id": args.policy_id,
    "policy_revision": args.policy_revision,
    "policy_sha256": "canonical JSON에서 이 field를 제외한 SHA-256",
    "evidence_kind": "MEASURED_EPISODES | GOLDEN_CONTRACT_FIXTURE",
    "performance_claim": "OFFLINE_HOLDOUT_ONLY_NOT_PROMOTED | CONTRACT_ONLY_NOT_MEASURED",
    "runtime_contract": {
        "trace_schema_version": 1,
        "observation_schema_version": 1,
        "action_schema_version": 1,
        "candidate_order": [1, 2, 3, 4],
        "feature_count": 67,
        "legal_mask_required": True,
        "tie_break": "LOWEST_CANDIDATE_KIND"
    },
    "source": {},
    "training": {},
    "split": {},
    "features": {},
    "model": {"weight": []},
    "metrics": {},
    "runtime_binary": {},
    "safety": {
        "pytorch_used": True,
        "reinforcement_learning_used": False,
        "python_transition_model_used": False,
        "runtime_mode": "SHADOW_ONLY",
        "eligible_for_runtime_promotion": False,
        "policy_state_modified": False,
        "policy_promotion_gate_required": True
    }
}
```

runtime binary는 PyTorch checkpoint가 아니라 고정 little-endian `ChampionAIShadowPolicyBinaryV1`이다. header와 payload를 아래 순서로 seal한다.

```text
magic[8] = "WBCPOL1\0"
artifact_schema_version:u16 = 1
header_bytes:u16
file_bytes:u32
trace_schema_version:u16 = 1
observation_schema_version:u16 = 1
action_schema_version:u16 = 1
feature_count:u16 = 67
candidate_count:u16 = 4
scalar_type:u16 = 1 (IEEE754 float32)
policy_revision:u64
source_policy_revision:u64
feature_order_sha256_prefix:u64
reserved:u32 = 0
normalization_mean[67]:float32
normalization_inverse_scale[67]:float32
weight[67]:float32
```

subnormal은 0으로 canonicalize하고 NaN/Inf, scale<=0, shape/schema/order mismatch를 export 전에 거절한다. JSON은 runtime binary 전체 SHA-256을 포함한다. CLI는 기존 옵션에 다음을 append한다.

```python
parser.add_argument(
    "--backend",
    choices=("numpy-pairwise", "pytorch-masked-bc"),
    default="numpy-pairwise",
)
parser.add_argument("--runtime-output", type=Path)
parser.add_argument("--policy-id")
parser.add_argument("--policy-revision", type=int)
```

PyTorch backend에서는 `--runtime-output`, non-empty policy id, `policy_revision > source policy_revision`을 필수로 하고 JSON과 binary를 각각 temporary+fsync+replace로 atomic write한다. 기본 backend 출력과 기존 테스트 artifact byte는 변하지 않아야 한다.

### 1-2. `Tools/AIResearch/tests/test_train_imitation_ranking_baseline.py`, `Tools/AIResearch/RunValidation.ps1`

기존 NumPy 테스트에 masked BC와 binary contract를 같은 파일의 별도 test class로 추가한다. golden fixture는 8-record contract-only 경로에만 재사용하고 mask 검사는 record deepcopy로 legal/illegal bit와 candidate flag를 함께 바꾼다.

검증 항목은 다음으로 고정한다.

```text
fixture: 8 decisions x 4 candidates x 67 features
split: train 6 / holdout 2 / overlap 0
PyTorch: CPU float64, deterministic algorithms, one thread, zero init
same input/config/env: report bytes, binary bytes, JSON SHA, binary SHA 동일
illegal candidate: logit/argmax에서 완전 제외
selected illegal, legal candidate < 2, pending/rejected executor: fail closed
train legal rows만 normalizer에 포함; holdout 변형이 mean/weight를 바꾸지 않음
binary: bad magic/version/size/schema/order hash/NaN/Inf/invScale<=0/trailing bytes reject
policy safety claim을 active/RL/promoted로 올리면 validator reject
```

`RunValidation.ps1`의 기존 unittest discovery 뒤에 fixture PyTorch BC CLI를 두 번 실행하여 SHA equality를 확인하는 smoke를 추가한다. CUDA가 있어도 golden 경로는 CPU만 사용한다. 현재 환경 버전 Python 3.14.4, NumPy 2.4.4, PyTorch 2.13.0+cu126은 artifact에 기록하되 다른 PC에 대한 bit identity로 과장하지 않는다.

### 1-3. `Tools/SimLab/main.cpp`, `Tools/AIResearch/tests/RunLiveAiEpisodeSmokeProbe.ps1`

기존 `--export-ai-research-smoke`와 240-tick forced boundary는 계약 smoke로 그대로 유지한다. 그 함수를 corpus라고 이름 바꾸거나 seed 반복으로 부풀리지 않는다.

`WriteLiveAiResearchMetadata`의 scenario/policy identity를 인자로 받을 수 있게 하고 기존 smoke는 현재 고정값을 전달한다. 별도 CLI를 추가한다.

```text
SimLab.exe --export-ai-research-episode \
  <out-dir> <scenario-family> <scenario-id> <side> \
  <rules-sha256> <definition-sha256> <seed>
```

scenario registry의 최소 family는 `fight`, `retreat`, `farm`, `siege`다. 각 episode는 실제 `CChampionAISystem -> GameCommand -> CDefaultCommandExecutor` 1-decision transition을 실행한다.

```text
fight:   관측 가능한 적 champion, self HP 우세/적 HP 열세, Retreat+Fight legal
retreat: self HP 4~9%, 관측 가능한 강한 적 champion, Retreat+Fight legal
farm:    적 champion은 시야 밖, enemy lane minion 근접, Retreat+Farm legal
siege:   적 outer turret + 사거리 안 allied lane minion, Retreat+Siege legal
```

seed는 관측 가능한 HP/distance/level과 minion/structure HP geometry를 실제로 바꾸고 side는 Blue/Red 좌표와 team을 mirror한다. 상대 wallet은 관측 피처가 아니므로 fake gold 다양성으로 세지 않는다. 모든 spawned entity는 `EntityIdMap`의 NetEntityId를 받고 state hash는 scenario-owned champion/minion/structure/wave를 NetId 정렬로 포함한다. selected candidate가 family 기대값과 다르거나 executor가 Accepted가 아니거나 legal candidate가 2개 미만이면 그 episode는 corpus에 쓰지 않고 fail한다. 마지막 retained decision 하나만 `truncated=true`이며 tick-240 intent 강제 reset은 사용하지 않는다.

이 32-group corpus의 범위는 `RETREAT_VS_ONE_MACRO_BOOTSTRAP`이다. 각 family는 Retreat와 해당 macro의 2-way legal mask를 검증하며 Fight/Farm/Siege의 3-way 또는 4-way conflict는 포함하지 않는다. manifest에 legal-mask histogram을 기록하고, 높은 holdout top1을 production lane 판단 품질이나 실력 향상으로 해석하지 않는다.

PowerShell probe에 다음 선택 인자를 append한다.

```powershell
[switch]$BuildMeasuredCorpus,
[int]$MeasuredSeedsPerScenario = 8,
[string]$MeasuredCorpusOutput = ""
```

활성화 시 4 family x 8 parameterized scenario = 32 frozen scenario group을 Blue/Red mirror 두 행으로 만들고, 각 행을 다시 두 번 실행한다. mirror pair는 같은 scenario id와 seed를 공유하고 episode/run id만 side를 포함하므로 split에서 서로 갈라지지 않는다. raw capture, metadata, canonical episode JSONL 세 파일 SHA가 repeat 사이 동일해야 하고 각 episode와 merged corpus에 promotion validator를 실행한다. merged JSONL과 manifest는 atomic write하며 manifest는 rules/definition/SimLab SHA, 32 group/32 mirrored pair/64 record/class count, drop reason, corpus SHA를 가진다. scenario id는 실제 parameter set을 포함하여 `(scenario_id,rules_hash,definition_hash)` split이 진짜 frozen group을 형성한다.

### 1-4. `Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h`, `ChampionAIPolicy.cpp`

기존 authored profile/combo owner를 보존하고 파일 tail에 read-only learned shadow contract를 추가한다. Shared는 filesystem, BCrypt, Python, torch, Server를 include하지 않는다.

```cpp
inline constexpr u16_t kChampionAIShadowPolicySchemaVersionV1 = 1u;
inline constexpr u16_t kChampionAIShadowFeatureCountV1 = 67u;
inline constexpr u16_t kChampionAIShadowCandidateCountV1 = 4u;
inline constexpr u16_t kChampionAIShadowInvalidFeatureIndexV1 = 0xFFFFu;

enum class eChampionAIShadowStatusV1 : u8_t
{
    Disabled = 0u,
    Evaluated,
    InvalidArtifact,
    InvalidTrace,
    InsufficientLegalCandidates,
};

struct ChampionAIShadowPolicyArtifactV1
{
    u64_t policyRevision = 0u;
    u64_t sourcePolicyRevision = 0u;
    u64_t featureOrderSha256Prefix = 0u;
    u64_t binarySha256Prefix = 0u;
    f32_t normalizationMean[kChampionAIShadowFeatureCountV1]{};
    f32_t normalizationInverseScale[kChampionAIShadowFeatureCountV1]{};
    f32_t weights[kChampionAIShadowFeatureCountV1]{};
};

struct ChampionAIShadowDecisionV1
{
    eChampionAIShadowStatusV1 status = eChampionAIShadowStatusV1::Disabled;
    u8_t activeCandidateKind = 0u;
    u8_t shadowCandidateKind = 0u;
    bool_t bDisagreed = false;
    u32_t legalCandidateMask = 0u;
    f32_t logits[kChampionAIShadowCandidateCountV1]{};
    f32_t selectedMargin = 0.f;
    u16_t topFeatureIndex = kChampionAIShadowInvalidFeatureIndexV1;
    f32_t topFeatureContribution = 0.f;
};

bool_t DecodeChampionAIShadowPolicyArtifactV1(
    const u8_t* bytes,
    size_t byteCount,
    ChampionAIShadowPolicyArtifactV1& outArtifact);

ChampionAIShadowDecisionV1 EvaluateChampionAIShadowPolicyV1(
    const ChampionAIShadowPolicyArtifactV1* artifact,
    const AiDecisionTraceV1& trace);
```

decoder는 explicit little-endian read와 `memcpy`만 사용하고 magic/version/header/file size/schema/dim/scalar/order hash/reserved/truncation/trailing/finiteness/inverse scale를 모두 검증한다. struct reinterpret는 하지 않는다.

feature builder는 Python과 동일하게 candidate kind 4 one-hot, target relation 7 one-hot, candidate-kind별 14 context block을 만든다. scorer는 double intermediate의 고정 index loop로 `(x-mean)*invScale*weight`를 누적해 float logit으로 저장한다. legal candidate만 argmax하고 exact tie는 낮은 candidate kind다. legal이 2개 미만이면 비교 불가 상태로 닫는다. selected shadow candidate에서 절대 기여값이 가장 큰 feature index/contribution을 기록하여 F9에서 왜 그 logit이 컸는지 바로 볼 수 있게 한다. softmax는 calibration 증거가 아니므로 runtime과 wire에 넣지 않는다.

### 1-5. `Server/Private/main.cpp`, `Server/Public/Game/GameRoom.h`, `Server/Private/Game/GameRoom.cpp`, `Server/Public|Private/Game/ServerAICommandProducer.*`, `Server/Private/Game/GameRoomChampionAI.cpp`

Server CLI에 다음 두 옵션을 함께 요구한다.

```text
--ai-shadow-policy=<absolute-or-working-dir-path>
--ai-shadow-policy-sha256=<lowercase 64hex>
```

둘 다 없으면 shadow disabled로 기존 gameplay가 byte-identical하게 시작한다. 하나만 있거나 파일 누락/read 실패/SHA mismatch/decode 실패면 room 생성 전 non-zero로 fail한다. 명시적으로 잘못 준 정책을 조용히 RuleBased로 fallback하지 않는다.

`main.cpp` anonymous namespace에 file read와 Windows BCrypt SHA-256 helper를 추가하고 `#pragma comment(lib, "bcrypt.lib")`로 Server-private dependency만 둔다. 성공한 artifact에는 실제 binary SHA prefix를 runtime identity로 넣고 immutable shared owner를 만든다.

```cpp
std::shared_ptr<const ChampionAIShadowPolicyArtifactV1> shadowPolicy;
if (!LoadChampionAIShadowPolicyOptions(argc, argv, shadowPolicy))
    return 4;

auto room = CGameRoom::Create(1, std::move(shadowPolicy));
```

`CGameRoom::Create`와 constructor는 default empty shared_ptr를 받아 기존 integration probe를 보존하고 room member가 lifetime을 소유한다. `Phase_ServerBotAI`가 raw const pointer를 `CServerAICommandProducer::Execute`로 전달하고 producer가 `CChampionAISystem::Execute`의 optional 마지막 인자로 전달한다. active brain type, intent, hold timer, command, RNG는 shadow 결과로 절대 수정하지 않는다.

### 1-6. `Shared/GameSim/Components/ChampionAIComponent.h`, `Shared/GameSim/Systems/ChampionAI/ChampionAISystem.h/.cpp`

`ChampionAIResearchDebugComponent`는 checkpoint transient라는 기존 경계를 유지하며 현재 tick 동안만 immutable artifact pointer와 최신 shadow evidence 한 행을 가진다. `ChampionAIComponent`의 legacy ring은 keyframe에 들어가므로 learned policy on/off에 따라 그 바이트가 달라져서는 안 된다.

```cpp
ChampionAIDecisionTraceEntry shadowDecision{};
bool_t bShadowDecisionPresent = false;
const ChampionAIShadowPolicyArtifactV1* pShadowPolicy = nullptr;
```

`ChampionAIDecisionTraceEntry` tail에 decision-tick 결속 결과를 append한다.

```cpp
u64_t shadowPolicyRevision = 0u;
u64_t shadowPolicySha256Prefix = 0u;
f32_t shadowLogits[kAiDecisionCandidateCapacityV1]{};
f32_t shadowSelectedMargin = 0.f;
f32_t shadowTopFeatureContribution = 0.f;
u32_t shadowLegalCandidateMask = 0u;
u16_t shadowTopFeatureIndex = kChampionAIShadowInvalidFeatureIndexV1;
u8_t shadowStatus = static_cast<u8_t>(eChampionAIShadowStatusV1::Disabled);
u8_t shadowActiveCandidateKind = 0u;
u8_t shadowSelectedCandidateKind = 0u;
bool_t bShadowDisagreed = false;
```

`CChampionAISystem::Execute`는 optional policy pointer를 transient research state에 먼저 설정한다. cadence gate 전의 `BuildResearchDecisionTrace`에서 inference하지 않는다. `PushChampionAIDecisionTrace`가 typed trace의 selected flag, command, mask를 확정한 직후 exactly once 평가하여 `researchState.shadowDecision`에 보관한다. 같은 타입을 쓰는 legacy ring에는 shadow tail을 모두 기본값으로 지운 행만 기록한다. 이는 high-level brain뿐 아니라 emergency/safety가 만든 최종 active candidate도 비교하므로 F9에서는 `active final`이라고 명시하고 block reason/intent와 함께 해석한다. trace push가 없는 30 Hz frame, recall 유지 tick, action lock tick에는 평가가 없다.

null policy와 enabled policy의 authoritative state/command hash는 매 tick 완전히 같아야 한다. rewind restore는 research component를 제거하므로 old pointer/trace는 남지 않고 복원 뒤 첫 실제 decision에서 다시 계산된다. `AiEpisodeV1` 528-byte ABI와 authored candidate score는 변경하지 않는다.

### 1-7. `Shared/Schemas/Snapshot.fbs`, generated code, `Server/Private/Game/SnapshotBuilder.cpp`, `Client/Private/Network/Client/SnapshotApplier.cpp`, `Client/Private/UI/AIDebugPanel.cpp`

`AIDebugTraceRow` tail에 shadow field를 append-only로 추가한다.

```fbs
    shadowPolicyRevision:ulong;
    shadowPolicySha256Prefix:ulong;
    shadowLogitRetreat:float;
    shadowLogitFight:float;
    shadowLogitFarm:float;
    shadowLogitSiege:float;
    shadowSelectedMargin:float;
    shadowTopFeatureContribution:float;
    shadowLegalCandidateMask:uint;
    shadowTopFeatureIndex:ushort = 65535;
    shadowStatus:ubyte;
    shadowActiveCandidateKind:ubyte;
    shadowSelectedCandidateKind:ubyte;
    shadowDisagreed:bool;
```

`Shared/Schemas/run_codegen.bat`로 generated C++/Go를 재생성하고 generated 파일은 수동 편집하지 않는다. Server는 기존 authoritative newest legacy row의 `(tick, commandSequence)`와 transient `shadowDecision`이 일치할 때만 shadow tail을 join하여 한 행을 전송한다. 불일치/미평가 행은 기본값으로 fail closed한다. Client applier는 `ChampionAIDecisionTraceEntry`로 값 복사하므로 F9 local Chrono sample이 당시 artifact revision/SHA/logit을 값으로 freeze한다.

F9 Selected AI에는 아래를 추가한다.

```text
Shadow BC: Disabled | Evaluated | Invalid...  artifact rN sha:16hex
active-final Retreat|Fight|Farm|Siege -> shadow ...  AGREE|DISAGREE
logits: R / F / Farm / Siege, selected margin
top contribution: exact 67-feature name, signed contribution
```

Decision Trace와 Chrono Decision Timeline에도 `active->shadow/status`와 artifact revision/SHA를 표시한다. 서로 다른 artifact revision의 branch row는 revision을 숨기고 직접 비교하지 않는다. softmax를 표시하지 않으며 logit을 확률로 부르지 않는다. UI/frame/snapshot builder에서 재추론하지 않는다.

### 1-8. `Tools/AIResearch/README.md`, 구현 완료 경계와 다음 IL/RL 단계

README에 실제 실행 흐름을 다음처럼 박제한다.

```text
Headless GameSim workers (30 Hz truth, 5 Hz decisions)
  -> Accepted AiEpisodeV1 measured corpus
  -> offline Python/PyTorch masked BC
  -> immutable JSON report + fixed binary artifact + SHA
  -> Server startup load
  -> C++ shadow-only rescore
  -> F9/Chrono disagreement review
```

게임 Client를 계속 띄워 Python에 frame을 공급하거나 runtime parameter를 매 tick hot-swap하지 않는다. Client는 사람이 trace/Chrono branch를 눈으로 검토하고 DAgger 교정 label을 만드는 도구다. 학습은 episode batch가 끝난 뒤 오프라인에서 일어나고 artifact는 episode/branch 경계에서만 교체한다.

이번 30% ceiling에서 완료로 주장하는 것은 4-macro 1v1 measured corpus, PyTorch masked BC, deterministic binary export, Server SHA load, C++ shadow, F9 비교까지다. active learned command, 인간 command adapter, DAgger aggregation, PPO trajectory/value head/GAE, recurrent memory, 2v2/5v5 self-play league는 완료로 주장하지 않는다.

후속 gate는 아래 순서다.

```text
G1 Shadow corpus: >= 10,000 Accepted decisions, >= 100 frozen groups,
                  train/holdout all 4 classes, illegal prediction 0
G2 BC review: holdout top1 >= 0.85, top3 >= 0.98,
              two-run artifact SHA equality, C++ max|logit diff| <= 1e-5
G3 DAgger: disagreement/Chrono 어려운 장면만 인간 또는 oracle이 교정,
           aggregate corpus에서 golden scenario 퇴행 0
G4 1v1 active canary: safety mask/Recall/emergency guard는 rule owner 유지,
                      mirrored seeds에서 bootstrap 95% CI 하한 >= baseline
G5 PPO 1v1->2v2: BC 초기화, GAE(lambda), clipped PPO objective,
                  reward exploit/AFK/suicide regression 0
G6 5v5 league: frozen opponent pool, Blue/Red mirror, 과거 champion 회귀,
                promotion gate를 통과한 artifact만 archive/serve
```

PPO 단계의 기본식은 `A_t = delta_t + gamma*lambda*A_(t+1)`, `delta_t = r_t + gamma*V(s_(t+1))-V(s_t)`, `L_clip = E[min(r_t*A_t, clip(r_t,1-epsilon,1+epsilon)*A_t)]`로 문서화하되 S022 코드에는 넣지 않는다. reward는 승패를 최종 목적에 두고 team HP/gold/objective potential을 작은 dense shaping으로 사용하며 kill/last-hit 단일 보상으로 한타를 왜곡하지 않는다.

## 2. 검증

1. Python/fixture contract를 먼저 실행한다.

```text
python -B -m unittest discover -s Tools/AIResearch/tests -p "test_*.py" -v
python -B Tools/AIResearch/TrainImitationRankingBaseline.py --backend pytorch-masked-bc --input Tools/AIResearch/fixtures/imitation_ranking_v1_golden.jsonl --output <tmp>/policy_a.json --runtime-output <tmp>/policy_a.wbc --policy-id s022-contract --policy-revision 2 --minimum-groups 8 --fixture-contract
동일 명령을 policy_b에 반복하고 JSON/binary SHA equality 확인
```

2. Schema를 재생성한다.

```text
Shared\Schemas\run_codegen.bat
```

3. GameSim과 SimLab을 먼저 Debug x64 빌드한다.

```text
msbuild Shared\GameSim\Include\GameSim.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
msbuild Tools\SimLab\SimLab.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
```

4. SimLab native probes에 다음을 추가하고 전부 PASS시킨다.

```text
artifact decoder malformed matrix
Python scalar golden vs C++ candidate logit <= 1e-5
illegal-highest mask, exact tie, legal<2 status
shadow off/on 300 tick command + authoritative hash exact equality
intent/command unchanged while forced disagreement trace exists
30 Hz tick이 아니라 pushed decision count만 inference
checkpoint restore가 transient shadow state 제거 후 재생성
```

기존 1,800 tick determinism과 S021 Recall/executor/Chrono foundation 회귀도 다시 실행한다.

```text
Tools\Bin\Debug\SimLab.exe 1800 42
```

5. measured corpus를 두 번 생성한다.

```text
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/AIResearch/tests/RunLiveAiEpisodeSmokeProbe.ps1 -BuildMeasuredCorpus -MeasuredSeedsPerScenario 8 -OutputRoot <tmp>/corpus_a
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/AIResearch/tests/RunLiveAiEpisodeSmokeProbe.ps1 -BuildMeasuredCorpus -MeasuredSeedsPerScenario 8 -OutputRoot <tmp>/corpus_b
```

각 run은 32 frozen groups/32 Blue-Red mirror pair/64 records, 4 class coverage, raw/metadata/canonical repeat equality, merged promotion validation을 통과해야 한다. corpus A/B JSONL SHA가 같아야 한다. 그 corpus로 PyTorch BC를 두 번 학습하여 report/binary SHA equality를 확인하고 이 binary를 이후 Server/Client 검증 artifact로 사용한다.

6. Server와 Client를 Debug x64 빌드한다.

```text
msbuild Server\Include\Server.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
msbuild Client\Include\Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
```

Server loader matrix는 no-option disabled 성공, correct path+SHA 성공, missing paired option/없는 파일/bad SHA/corrupt binary가 room 생성 전에 실패하는지 확인한다.

7. 전체 solution Debug x64를 한 번 더 빌드한다.

```text
msbuild Winters.sln /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
```

8. 실제 Client 눈검증은 normal-runtime full map path로 한다. Server는 학습 artifact 경로+SHA로 시작하고 Client는 보이는 창으로 실행한다.

```text
Server\Bin\Debug\WintersServer.exe --smoke-seconds=300 --ai-shadow-policy=<policy.wbc> --ai-shadow-policy-sha256=<sha>
Client\Bin\Debug\WintersGame.exe --banpick-smoke --smoke-start --smoke-champion=YONE --smoke-start-min-humans=1 --smoke-full-ingame --smoke-full-map --smoke-no-skill --rhi=dx11 --fps=60 --no-vsync
```

F9에서 artifact status/revision/SHA, active-final vs shadow, 네 logit, top contribution, Decision Trace를 확인한다. 30-tick local sample을 여러 개 쌓고 150-tick rewind 후 epoch/branch가 증가하며 old/new branch row의 policy identity가 값으로 보존되는지 확인한다. rewind 전/후 game-window PNG 두 장과 실행 exe/artifact SHA를 `.md/build/2026-07-13_S022_PYTORCH_BC_SHADOW_POLICY_REPORT.md`에 링크한다. native window 캡처가 D3D black이면 수동 Snipping/Game Bar를 사용하고 성공하지 못한 시각 증거를 통과로 쓰지 않는다.

9. Shared boundary, whitespace, dirty 보존을 확인한다.

```text
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Harness/Check-SharedBoundary.ps1
git diff --check -- Tools/AIResearch Shared/GameSim/Components/ChampionAIComponent.h Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.cpp Shared/GameSim/Systems/ChampionAI/ChampionAISystem.h Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp Shared/Schemas/Snapshot.fbs Shared/Schemas/Generated Server/Private/main.cpp Server/Public/Game/GameRoom.h Server/Private/Game/GameRoom.cpp Server/Public/Game/ServerAICommandProducer.h Server/Private/Game/ServerAICommandProducer.cpp Server/Private/Game/GameRoomChampionAI.cpp Server/Private/Game/SnapshotBuilder.cpp Client/Private/Network/Client/SnapshotApplier.cpp Client/Private/UI/AIDebugPanel.cpp Tools/SimLab/main.cpp .md/plan/2026-07-13_S022_PYTORCH_BC_SHADOW_POLICY_PLAN.md
```

10. 최종 보고서에는 실제 corpus/report/binary/exe SHA, train/holdout loss·top1/top3·class count, Python/C++ max logit diff, shadow non-interference hash, inference count/time, Server fail-closed matrix, 네 빌드와 full solution 결과, 눈검증 PNG를 기록한다. 실패한 gate와 C4251/C4275 기존 경고를 분리하고 packet을 `Active -> Handoff`로 바꾼다.
