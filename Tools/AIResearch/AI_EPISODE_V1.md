# AiEpisodeV1 export contract

`ExportAiEpisodeV1.py` converts a sequence of raw, little-endian MSVC
`AiDecisionTraceV1` POD records into canonical UTF-8 JSONL. Gameplay code does
not perform file IO. The bounded Tools-owned native capture boundary is
`Native/AiDecisionTraceCaptureWriter.h`; a production owner must pass records
in deterministic chronological/entity order.

The exporter requires a metadata sidecar. It deliberately does not invent the
episode seed, rules/data hashes, policy revision, next-state hash, reward, or
terminal/truncation result.

```powershell
python -B Tools/AIResearch/ExportAiEpisodeV1.py `
  --input <decision_trace_v1.bin> `
  --metadata <episode_metadata.json> `
  --output <episode_v1.jsonl>
```

The metadata root contains the episode identity and one transition entry for
every raw trace index. Missing, duplicated, or out-of-range transition entries
fail the export. `terminal` means a real authoritative episode end such as
death/game completion. A time-limit capture ends with `truncated=true` instead.
The two flags are mutually exclusive, and only the final canonical record may
carry either boundary flag. If a future trajectory adapter maps this boundary
into PPO/GAE, it must bootstrap a truncated transition and must not bootstrap a
terminal transition; V1 by itself is not that trajectory adapter.

Executor state values preserve the C++ V1 contract:

- `0`: Unknown, pending
- `1`: Submitted, pending
- `2`: Accepted, final
- `3`: Rejected, final

The GameSim executor finalizes `Move`, `BasicAttack`, `CastSkill`, `Recall`,
`RecallCancel`, and `Flash` with stable Accepted/Rejected reasons by matching
the exact command sequence back to the submitted AI trace. Unsupported command
kinds remain Unknown/Submitted. The exporter preserves those pending values and
promotion rejects them; it never invents a terminal executor result.

An observation may legitimately carry both `TeamFiltered` and
`PrivilegedSource` when position is team-visible but another feature remains
privileged. That combination is raw-valid but cannot be promoted. The current
champion-bot producer uses team vision for current targets and observable item
purchase value for combat economy, so its research record is `TeamFiltered`
without the privileged bit. Promotion always requires that stricter form.

Promotion validation:

```powershell
python -B Tools/AIResearch/ValidateAiEpisode.py `
  --input <episode_v1.jsonl> `
  --promotion
```

The validator rejects duplicate JSON keys, duplicate record identities,
non-finite numbers, schema drift, placeholder hashes, privileged observations,
illegal selected candidates, unobserved targets, and unfinished executor
results. It also rejects `terminal && truncated`, multiple boundaries, or a
boundary before the final canonical record. Canonical output uses sorted keys,
compact separators, UTF-8 without a BOM, and a trailing newline.

## Live GameSim contract smoke

The dedicated SimLab CLI crosses the actual runtime path rather than the
synthetic POD fixture:

```powershell
Tools/Bin/Debug/SimLab.exe `
  --export-ai-research-smoke <out-dir> `
  <rules_sha256> <definition_sha256> [seed]
```

Both hashes must be lowercase, non-placeholder 64-hex values supplied by the
caller. The harness runs a deterministic 240-tick observable 1v1 lane through
`CChampionAISystem -> GameCommand -> CDefaultCommandExecutor -> GameSim`. It
keeps only pure `TeamFiltered`, final Accepted/Rejected traces whose selected
candidate is legal, sorts them by tick/NetEntityId/command sequence, and writes
the raw POD capture with the Tools-owned native writer. It never creates a
transition in Python.

For every retained decision, the metadata sidecar records the same tick's
post-simulation canonical authoritative state projection hash. That projection
hashes tick/RNG/NetEntityId order plus explicit transform, health, champion,
skill, action, combat-action, move/chase, and AI decision fields; it never hashes
padding, pointers, or allocator state. Reward is explicitly
`delta((self HP ratio) - (visible enemy HP ratio))` across that authoritative
tick. The fixed 240-tick limit marks only the final record `truncated=true`.

The smoke also asserts the observation boundary used by the influence map:
visible enemy facts produce `ThreatNow` dominated by the enemy NetEntityId;
after hiding the enemy on a forced real decision tick, `ThreatNow` is zero and
`ThreatBelief` is positive; the all-walkable SimLab query keeps `EscapeCost`
zero.

The live probe derives `rules_hash` from the exact built `SimLab.exe` bytes,
not from one selected C++ source file. `definition_hash` is the SHA-256 of the
loaded shared definition manifest. The harness deliberately resets the actor at
tick 240 so that the bounded capture ends on a final Accepted decision; that
last decision is the time-limit boundary and is marked `truncated=true`, never
`terminal=true`.

Run the two-export determinism and validation probe with:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File Tools/AIResearch/tests/RunLiveAiEpisodeSmokeProbe.ps1
```

It compares the SHA-256 of both native captures, metadata files, and canonical
JSONL exports, then requires raw and `--promotion` validation to pass. This is a
live pipeline contract smoke only. `AiEpisodeV1` is currently a decision-event
contract suitable for supervised behavior-cloning/ranking work. It is not a PPO
trajectory contract: it does not provide a dense per-simulation-tick sequence,
an explicit next observation, elapsed-step discount metadata, or the complete
reward/value/hidden-state information required by recurrent PPO. It therefore
makes no claim about bot skill, learning quality, win rate, RL readiness, or
runtime policy promotion.

2026-07-13의 full validation에서 두 run의 canonical live episode SHA-256은
`BECC9A151ABD8C24E45D79C787EA37FDB7BAFFA513896723128F912C98B0130E`로
일치했다. 이 값은 당시의 exact `SimLab.exe`, definition manifest, seed와
schema에 결합된 검증 기록이며 이후 binary/data 변경 때 고정 golden으로
재사용하지 않는다.

## Human/debug correction boundary

`AiEpisodeV1` is immutable factual evidence. A human/debug correction must not
rewrite `selected_candidate_kind`, candidate selected flags, the emitted
command, executor result, reward, or next-state hash. Those fields describe the
action that actually ran, not the action a reviewer later preferred.

`AiDecisionCorrectionSidecarV1` binds a correction to the exact source JSONL
SHA-256, canonical source-record SHA-256, and
`(episode_id, timeline_epoch, branch_id, tick, self_net_entity_id,
command_sequence)` identity. `MaterializeImitationDataset.py` validates that
binding and that the corrected candidate was legal, then emits canonical
`ImitationDecisionV1` JSONL. Each row embeds the unchanged source record and a
separate expert label.

- Source actor label: `SOURCE_ACCEPTED` / `SOURCE_COMMAND_ACCEPTED`
- Human label: `HUMAN_DEBUG_CORRECTION` / `UNEXECUTED_COUNTERFACTUAL`

The second form is supervised imitation evidence only. The embedded command,
executor result, reward, terminal flag, and next-state hash still belong to the
source actor. They do not prove that the corrected action executed or produced
that outcome, and the materialized dataset is not a PPO/RL trajectory.

An active learned-policy rollout is a DAgger state-query source, not an expert
source. It must be materialized with `--corrected-only`; unreviewed source
actions are excluded, and an empty correction set fails closed. Passing an
active rollout directly to the `ai-episode-v1` trainer path is forbidden because
an executor-accepted command proves execution, not expertise.

## Debug active-policy canary

The Debug-only SimLab active canary uses a checkpoint two-pass decision:

1. save the authoritative world/RNG/NetId keyframe at a real decision boundary;
2. evaluate the immutable `.wbc` artifact without executing its commands;
3. restore the keyframe and reacquire actors by NetEntityId;
4. inject the proposed macro as a one-shot AI Debug control;
5. execute and retain only the second pass's final command/trace.

`RunActiveAiPolicyEpisodeProbe.ps1` requires a real learned-policy disagreement
and applied intervention from a deterministic canary artifact. It compares two
identical-seed runs at the native capture, metadata, canonical JSONL, and report
levels, then verifies that a truncated artifact with its own correct SHA-256 is
rejected before output. The report marks the episode
`EVALUATION_AND_DAGGER_STATE_DISCOVERY_ONLY` and
`eligible_as_imitation_expert_input=false`. Raw schema validation is allowed;
promotion or direct BC ingestion is not evidence that the active action was an
expert label.

## Current V1 scope and privacy boundary

The research trace owns exactly four candidate kinds:

- `Retreat`
- `Fight`
- `Farm`
- `Siege`

Legacy `Recall`, `DefendMid`, and other intents may still appear in the legacy
AI debug trace, but they are intentionally not encoded as `AiEpisodeV1`
candidates. Expanding that set requires a schema revision and matching native,
Python, fixture, validator, and trainer updates.

When the enemy champion is not currently team-observed, all *current* enemy
fields in the exported observation are canonical zero: NetEntityId, level, HP
ratio, observable inventory value, and distance. A remembered enemy can still
contribute to transient `ThreatBelief`, but remembered or privileged facts are
not smuggled into these current-observation fields.

The 9x9 `ThreatNow`, `ThreatBelief`, `SupportEta`, and `EscapeCost` influence
map is currently transient GameSim research instrumentation. It is exercised by
native and live smoke probes, but it is neither consumed by the active
RuleBased/PlayerLike decision score nor serialized in `AiEpisodeV1`, and there
is no Client influence-map UI yet.

`observation.self_gold` and `observation.enemy_gold` retain their V1 wire names,
but the authoritative producer stores observable inventory purchase value in
them. It does not expose either champion's current wallet gold. A future schema
may rename these fields; changing their V1 meaning back to `GoldComponent`
wallet state is forbidden because that would turn an apparently team-filtered
record into privileged training data.
