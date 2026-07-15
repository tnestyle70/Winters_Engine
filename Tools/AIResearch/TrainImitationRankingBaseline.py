"""Deterministic NumPy pairwise imitation-ranking baseline for AiEpisodeV1.

This is an offline supervised imitation baseline, not reinforcement learning.
It consumes promotion-valid episode records and writes a canonical report/model
artifact. It never executes gameplay transitions or promotes a runtime policy.
Direct AiEpisodeV1 labels require final Accepted executor results. An explicit
ImitationDecisionV1 input may instead carry a legal human/debug correction that
is marked as an unexecuted counterfactual label while its source record remains
unchanged and Accepted.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import platform
import struct
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any

import numpy as np

import AiEpisodeSchema as episode_schema
import AiImitationDatasetSchema as imitation_schema
from ValidateAiEpisode import (
    DuplicateObjectKeyError,
    NonFiniteJsonConstantError,
    RejectDuplicateObjectKeys,
    RejectNonFiniteJsonConstant,
)


ArtifactSchemaVersion = 1
ArtifactType = "ImitationRankingArtifactV1"
PolicyArtifactType = "PolicyArtifactV1"
MeasuredEvidenceKind = "MEASURED_EPISODES"
FixtureEvidenceKind = "GOLDEN_CONTRACT_FIXTURE"
AllowedEvidenceKinds = {MeasuredEvidenceKind, FixtureEvidenceKind}
AiEpisodeInputContract = "ai-episode-v1"
ImitationDecisionInputContract = "imitation-decision-v1"
AllowedInputContracts = {
    AiEpisodeInputContract,
    ImitationDecisionInputContract,
}
SplitGroup = tuple[str, str, str]
RuntimePolicyMagic = b"WBCPOL1\0"
RuntimePolicySchemaVersion = 1
RuntimePolicyScalarFloat32 = 1
RuntimePolicyCandidateOrder = (1, 2, 3, 4)
RuntimePolicyFeatureCount = 67
RuntimePolicyHeaderFormat = "<8sHHIHHHHHHQQQI"
RuntimePolicyHeaderBytes = struct.calcsize(RuntimePolicyHeaderFormat)
RuntimePolicyFileBytes = (
    RuntimePolicyHeaderBytes + RuntimePolicyFeatureCount * 3 * 4
)

ContextFeatureNames = (
    "capability_flags",
    "self_level",
    "enemy_level",
    "self_hp_ratio",
    "enemy_hp_ratio",
    "self_gold",
    "enemy_gold",
    "enemy_distance",
    "attack_range",
    "turret_danger",
    "legal_candidate_mask",
    "illegal_candidate_mask",
    "available_action_mask",
    "available_skill_mask",
)
TargetRelationNames = (
    "none",
    "self",
    "enemy_champion",
    "enemy_minion",
    "enemy_structure",
    "allied_wave",
    "other_observed",
)
ForbiddenFeatureSources = (
    "candidate.flags.selected",
    "candidate.score",
    "candidate.contributions",
    "record.selected_candidate_kind",
    "record.command",
    "record.executor",
    "record.next_state_hash",
    "record.reward",
    "record.terminal",
    "record.truncated",
    "record.observation.provenance_flags",
    "record.observation.fact_tick",
    "record.observation.*_net_entity_id as raw numeric values",
)


def _BuildFeatureOrder() -> tuple[str, ...]:
    order = [f"candidate_kind_{kind}" for kind in sorted(episode_schema.CandidateKinds)]
    order.extend(f"target_relation_{name}" for name in TargetRelationNames)
    for kind in sorted(episode_schema.CandidateKinds):
        order.extend(f"kind_{kind}_x_{name}" for name in ContextFeatureNames)
    return tuple(order)


FeatureOrder = _BuildFeatureOrder()


class TrainingDataError(ValueError):
    pass


@dataclass(frozen=True)
class TrainingConfig:
    seed: int = 1729
    holdout_fraction: float = 0.25
    minimum_groups: int = 10
    minimum_groups_per_split: int = 2
    epochs: int = 400
    learning_rate: float = 0.08
    l2: float = 0.001

    def Validate(self) -> list[str]:
        errors: list[str] = []
        if self.seed < 0:
            errors.append("seed must be non-negative")
        if (
            not math.isfinite(self.holdout_fraction)
            or not 0.0 < self.holdout_fraction < 1.0
        ):
            errors.append("holdout_fraction must be finite and between 0 and 1")
        if self.minimum_groups < 4:
            errors.append("minimum_groups must be at least 4")
        if self.minimum_groups_per_split < 2:
            errors.append("minimum_groups_per_split must be at least 2")
        if self.minimum_groups < self.minimum_groups_per_split * 2:
            errors.append("minimum_groups cannot satisfy both split minima")
        if self.epochs < 1:
            errors.append("epochs must be positive")
        if not math.isfinite(self.learning_rate) or self.learning_rate <= 0.0:
            errors.append("learning_rate must be finite and positive")
        if not math.isfinite(self.l2) or self.l2 < 0.0:
            errors.append("l2 must be finite and non-negative")
        return errors


@dataclass(frozen=True)
class LoadedEpisodes:
    records: list[dict[str, Any]]
    input_sha256: str
    input_contract: str = AiEpisodeInputContract
    expert_candidate_kinds: dict[tuple[Any, ...], int] | None = None
    label_origin_counts: dict[str, int] | None = None
    source_ai_episode_sha256: str | None = None
    correction_sidecar_sha256: str | None = None


@dataclass(frozen=True)
class DecisionExample:
    split_group: SplitGroup
    decision_group: tuple[Any, ...]
    selected_kind: int
    candidate_features: dict[int, np.ndarray]
    executor_state: int


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


def _CanonicalJsonBytes(value: Any) -> bytes:
    return json.dumps(
        value,
        ensure_ascii=False,
        allow_nan=False,
        sort_keys=True,
        separators=(",", ":"),
    ).encode("utf-8")


FeatureOrderSha256 = hashlib.sha256(
    _CanonicalJsonBytes(list(FeatureOrder))
).hexdigest()
FeatureOrderSha256Prefix = int(FeatureOrderSha256[:16], 16)


def ComputeArtifactSha256(artifact: dict[str, Any]) -> str:
    payload = {
        key: value for key, value in artifact.items() if key != "artifact_sha256"
    }
    return hashlib.sha256(_CanonicalJsonBytes(payload)).hexdigest()


def _ParseJsonLine(raw_line: str, line_number: int) -> dict[str, Any]:
    try:
        value = json.loads(
            raw_line,
            object_pairs_hook=RejectDuplicateObjectKeys,
            parse_constant=RejectNonFiniteJsonConstant,
        )
    except (
        json.JSONDecodeError,
        DuplicateObjectKeyError,
        NonFiniteJsonConstantError,
    ) as error:
        raise TrainingDataError(f"line {line_number}: {error}") from error
    if not isinstance(value, dict):
        raise TrainingDataError(f"line {line_number}: record must be an object")
    return value


def LoadPromotionEpisodes(path: Path) -> LoadedEpisodes:
    raw_bytes = path.read_bytes()
    records: list[dict[str, Any]] = []
    seen_record_identities: dict[tuple[Any, ...], int] = {}
    seen_decision_groups: dict[tuple[Any, ...], int] = {}
    episode_rows: dict[
        tuple[Any, ...],
        list[tuple[int, int, int, int, bool, bool]],
    ] = {}

    for line_number, raw_line in enumerate(
        raw_bytes.decode("utf-8").splitlines(),
        start=1,
    ):
        if not raw_line.strip():
            continue
        record = _ParseJsonLine(raw_line, line_number)
        try:
            validation_errors = episode_schema.ValidateRecord(
                record,
                promotion=True,
            )
        except (OverflowError, TypeError, ValueError) as error:
            raise TrainingDataError(
                f"line {line_number}: record validation failed: {error}"
            ) from error
        if validation_errors:
            detail = "; ".join(validation_errors)
            raise TrainingDataError(
                f"line {line_number}: record is not promotion-valid: {detail}"
            )
        if record["executor"]["state"] != episode_schema.ExecutorAccepted:
            raise TrainingDataError(
                f"line {line_number}: imitation expert label requires an "
                "accepted executor result"
            )

        identity = episode_schema.BuildRecordIdentity(record)
        if identity is None:
            raise TrainingDataError(f"line {line_number}: record identity is invalid")
        if identity in seen_record_identities:
            raise TrainingDataError(
                f"line {line_number}: duplicate record identity from line "
                f"{seen_record_identities[identity]}"
            )
        seen_record_identities[identity] = line_number

        episode_group = (
            record["episode_id"],
            record["scenario_id"],
            record["timeline_epoch"],
            record["branch_id"],
        )
        episode_rows.setdefault(episode_group, []).append(
            (
                line_number,
                int(record["tick"]),
                int(record["observation"]["self_net_entity_id"]),
                int(record["command"]["sequence"]),
                record["terminal"] is True,
                record["truncated"] is True,
            )
        )

        observation = record["observation"]
        decision_group = (
            record["episode_id"],
            record["scenario_id"],
            record["timeline_epoch"],
            record["branch_id"],
            record["tick"],
            observation["self_net_entity_id"],
        )
        if decision_group in seen_decision_groups:
            raise TrainingDataError(
                f"line {line_number}: duplicate decision group from line "
                f"{seen_decision_groups[decision_group]}"
            )
        seen_decision_groups[decision_group] = line_number
        records.append(record)

    for episode_group, rows in episode_rows.items():
        ordered = sorted(rows, key=lambda row: (row[1], row[2], row[3], row[0]))
        boundary_rows = [row for row in ordered if row[4] or row[5]]
        if len(boundary_rows) > 1:
            raise TrainingDataError(
                f"episode {episode_group} has multiple terminal/truncated records"
            )
        if boundary_rows and boundary_rows[0] != ordered[-1]:
            raise TrainingDataError(
                f"line {boundary_rows[0][0]}: terminal/truncated record is "
                f"not the final canonical record for episode {episode_group}"
            )

    if not records:
        raise TrainingDataError("input contains no AiEpisodeV1 records")
    records.sort(
        key=lambda record: (
            record["episode_id"],
            record["scenario_id"],
            record["timeline_epoch"],
            record["branch_id"],
            record["tick"],
            record["observation"]["self_net_entity_id"],
        )
    )
    return LoadedEpisodes(
        records=records,
        input_sha256=hashlib.sha256(raw_bytes).hexdigest(),
    )


def LoadImitationDecisionEpisodes(path: Path) -> LoadedEpisodes:
    try:
        loaded = imitation_schema.LoadImitationDataset(path)
    except imitation_schema.ImitationDatasetError as error:
        raise TrainingDataError(str(error)) from error
    return LoadedEpisodes(
        records=loaded.source_records,
        input_sha256=loaded.input_sha256,
        input_contract=ImitationDecisionInputContract,
        expert_candidate_kinds=loaded.expert_candidate_kinds,
        label_origin_counts=loaded.label_origin_counts,
        source_ai_episode_sha256=loaded.source_ai_episode_sha256,
        correction_sidecar_sha256=loaded.correction_sidecar_sha256,
    )


def LoadTrainingEpisodes(path: Path, input_contract: str) -> LoadedEpisodes:
    if input_contract == AiEpisodeInputContract:
        return LoadPromotionEpisodes(path)
    if input_contract == ImitationDecisionInputContract:
        return LoadImitationDecisionEpisodes(path)
    raise TrainingDataError(f"unsupported input contract: {input_contract}")


def _ContextValues(record: dict[str, Any]) -> dict[str, float]:
    observation = record["observation"]
    action_mask = record["action_mask"]
    try:
        values = {
            "capability_flags": float(observation["capability_flags"]),
            "self_level": float(observation["self_level"]),
            "enemy_level": float(observation["enemy_level"]),
            "self_hp_ratio": float(observation["self_hp_ratio"]),
            "enemy_hp_ratio": float(observation["enemy_hp_ratio"]),
            "self_gold": float(observation["self_gold"]),
            "enemy_gold": float(observation["enemy_gold"]),
            "enemy_distance": float(observation["enemy_distance"]),
            "attack_range": float(observation["attack_range"]),
            "turret_danger": float(observation["turret_danger"]),
            "legal_candidate_mask": float(action_mask["legal_candidate_mask"]),
            "illegal_candidate_mask": float(
                action_mask["illegal_candidate_mask"]
            ),
            "available_action_mask": float(
                action_mask["available_action_mask"]
            ),
            "available_skill_mask": float(action_mask["available_skill_mask"]),
        }
    except (OverflowError, TypeError, ValueError) as error:
        raise TrainingDataError(f"context feature conversion failed: {error}") from error
    if not all(math.isfinite(value) for value in values.values()):
        raise TrainingDataError("context features contain a non-finite value")
    return values


def _ResolveTargetRelation(
    observation: dict[str, Any],
    target_net_entity_id: int,
) -> str:
    if target_net_entity_id == 0:
        return "none"
    relations = (
        ("self", "self_net_entity_id"),
        ("enemy_champion", "enemy_champion_net_entity_id"),
        ("enemy_minion", "enemy_minion_net_entity_id"),
        ("enemy_structure", "enemy_structure_net_entity_id"),
        ("allied_wave", "allied_wave_net_entity_id"),
    )
    for relation, field in relations:
        if target_net_entity_id == observation[field]:
            return relation
    return "other_observed"


def BuildFeatureVector(
    record: dict[str, Any],
    candidate: dict[str, Any],
) -> np.ndarray:
    kind = int(candidate["kind"])
    context = _ContextValues(record)
    relation = _ResolveTargetRelation(
        record["observation"],
        int(candidate["target_net_entity_id"]),
    )

    values: list[float] = []
    values.extend(
        1.0 if kind == candidate_kind else 0.0
        for candidate_kind in sorted(episode_schema.CandidateKinds)
    )
    values.extend(1.0 if relation == name else 0.0 for name in TargetRelationNames)
    for candidate_kind in sorted(episode_schema.CandidateKinds):
        active = 1.0 if kind == candidate_kind else 0.0
        values.extend(active * context[name] for name in ContextFeatureNames)

    result = np.asarray(values, dtype=np.float64)
    if result.shape != (len(FeatureOrder),) or not np.all(np.isfinite(result)):
        raise TrainingDataError("feature vector is malformed or non-finite")
    return result


def _LegalCandidates(record: dict[str, Any]) -> list[dict[str, Any]]:
    legal_mask = int(record["action_mask"]["legal_candidate_mask"])
    result = [
        candidate
        for candidate in record["candidates"]
        if legal_mask & episode_schema.CandidateKindToBit[int(candidate["kind"])]
    ]
    result.sort(key=lambda candidate: int(candidate["kind"]))
    return result


def _ResolveExpertCandidateKind(
    record: dict[str, Any],
    expert_candidate_kinds: dict[tuple[Any, ...], int] | None,
) -> int:
    if expert_candidate_kinds is None:
        return int(record["selected_candidate_kind"])
    identity = episode_schema.BuildRecordIdentity(record)
    if identity is None or identity not in expert_candidate_kinds:
        raise TrainingDataError(
            "materialized imitation record is missing an expert label"
        )
    return int(expert_candidate_kinds[identity])


def BuildDecisionExamples(
    records: list[dict[str, Any]],
    expert_candidate_kinds: dict[tuple[Any, ...], int] | None = None,
) -> list[DecisionExample]:
    examples: list[DecisionExample] = []
    for record in records:
        if record["executor"]["state"] != episode_schema.ExecutorAccepted:
            raise TrainingDataError(
                "imitation expert label requires an accepted executor result"
            )
        candidates = _LegalCandidates(record)
        if len(candidates) < 2:
            raise TrainingDataError(
                "every decision group must contain at least two legal candidates"
            )
        selected_kind = _ResolveExpertCandidateKind(
            record,
            expert_candidate_kinds,
        )
        candidate_features = {
            int(candidate["kind"]): BuildFeatureVector(record, candidate)
            for candidate in candidates
        }
        if selected_kind not in candidate_features:
            raise TrainingDataError("selected candidate is absent from legal candidates")
        observation = record["observation"]
        examples.append(
            DecisionExample(
                split_group=(
                    record["scenario_id"],
                    record["rules_hash"],
                    record["definition_hash"],
                ),
                decision_group=(
                    record["episode_id"],
                    record["scenario_id"],
                    record["timeline_epoch"],
                    record["branch_id"],
                    record["tick"],
                    observation["self_net_entity_id"],
                ),
                selected_kind=selected_kind,
                candidate_features=candidate_features,
                executor_state=int(record["executor"]["state"]),
            )
        )
    return examples


def BuildMaskedBCExamples(
    records: list[dict[str, Any]],
    expert_candidate_kinds: dict[tuple[Any, ...], int] | None = None,
) -> list[MaskedBCExample]:
    examples: list[MaskedBCExample] = []
    expected_kinds = set(RuntimePolicyCandidateOrder)
    for record in records:
        if record["executor"]["state"] != episode_schema.ExecutorAccepted:
            raise TrainingDataError(
                "imitation expert label requires an accepted executor result"
            )

        candidate_by_kind = {
            int(candidate["kind"]): candidate
            for candidate in record["candidates"]
        }
        if set(candidate_by_kind) != expected_kinds:
            raise TrainingDataError(
                "masked BC requires exactly one candidate of each runtime kind"
            )

        legal_candidate_mask = int(
            record["action_mask"]["legal_candidate_mask"]
        )
        legal_mask = np.asarray(
            [
                bool(
                    legal_candidate_mask
                    & episode_schema.CandidateKindToBit[kind]
                )
                for kind in RuntimePolicyCandidateOrder
            ],
            dtype=np.bool_,
        )
        if int(np.sum(legal_mask)) < 2:
            raise TrainingDataError(
                "every masked BC decision must contain at least two legal candidates"
            )

        selected_kind = _ResolveExpertCandidateKind(
            record,
            expert_candidate_kinds,
        )
        if selected_kind not in expected_kinds:
            raise TrainingDataError("selected candidate kind is outside runtime order")
        selected_index = RuntimePolicyCandidateOrder.index(selected_kind)
        if not bool(legal_mask[selected_index]):
            raise TrainingDataError("selected candidate is illegal")

        features = np.vstack(
            [
                BuildFeatureVector(record, candidate_by_kind[kind])
                for kind in RuntimePolicyCandidateOrder
            ]
        )
        if features.shape != (
            len(RuntimePolicyCandidateOrder),
            len(FeatureOrder),
        ) or not np.all(np.isfinite(features)):
            raise TrainingDataError("masked BC feature tensor is malformed")

        observation = record["observation"]
        examples.append(
            MaskedBCExample(
                split_group=(
                    record["scenario_id"],
                    record["rules_hash"],
                    record["definition_hash"],
                ),
                decision_group=(
                    record["episode_id"],
                    record["scenario_id"],
                    record["timeline_epoch"],
                    record["branch_id"],
                    record["tick"],
                    observation["self_net_entity_id"],
                ),
                features=features,
                legal_mask=legal_mask,
                selected_index=selected_index,
            )
        )
    return examples


def FitMaskedNormalizer(
    examples: list[MaskedBCExample],
    train_groups: set[SplitGroup],
) -> tuple[np.ndarray, np.ndarray]:
    rows = [
        example.features[index]
        for example in examples
        if example.split_group in train_groups
        for index, legal in enumerate(example.legal_mask)
        if legal
    ]
    if not rows:
        raise TrainingDataError("training split contains no legal candidate rows")
    matrix = np.vstack(rows)
    mean = np.mean(matrix, axis=0, dtype=np.float64)
    scale = np.std(matrix, axis=0, dtype=np.float64)
    scale = np.where(scale < 1e-12, 1.0, scale)
    inverse_scale = 1.0 / scale
    if (
        mean.shape != (len(FeatureOrder),)
        or inverse_scale.shape != (len(FeatureOrder),)
        or not np.all(np.isfinite(mean))
        or not np.all(np.isfinite(inverse_scale))
        or np.any(inverse_scale <= 0.0)
    ):
        raise TrainingDataError("masked BC normalization is invalid")
    return mean, inverse_scale


def BuildMaskedTensors(
    examples: list[MaskedBCExample],
    groups: set[SplitGroup],
    mean: np.ndarray,
    inverse_scale: np.ndarray,
) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    selected = [
        example for example in examples if example.split_group in groups
    ]
    if not selected:
        raise TrainingDataError("masked BC split contains no decisions")
    features = np.stack([example.features for example in selected])
    normalized = (features - mean[None, None, :]) * inverse_scale[
        None, None, :
    ]
    legal_mask = np.stack([example.legal_mask for example in selected])
    labels = np.asarray(
        [example.selected_index for example in selected],
        dtype=np.int64,
    )
    if (
        normalized.shape[1:] != (
            len(RuntimePolicyCandidateOrder),
            len(FeatureOrder),
        )
        or legal_mask.shape != normalized.shape[:2]
        or labels.shape != (normalized.shape[0],)
        or not np.all(np.isfinite(normalized))
        or np.any(np.sum(legal_mask, axis=1) < 2)
        or not np.all(legal_mask[np.arange(len(labels)), labels])
    ):
        raise TrainingDataError("masked BC tensor contract is invalid")
    return normalized, legal_mask, labels


def TrainTorchMaskedBC(
    examples: list[MaskedBCExample],
    train_groups: set[SplitGroup],
    mean: np.ndarray,
    inverse_scale: np.ndarray,
    config: TrainingConfig,
) -> tuple[np.ndarray, float, str]:
    try:
        import torch
    except ImportError as error:
        raise TrainingDataError(
            "pytorch-masked-bc backend requires PyTorch"
        ) from error

    normalized, legal_mask, labels = BuildMaskedTensors(
        examples,
        train_groups,
        mean,
        inverse_scale,
    )
    torch.use_deterministic_algorithms(True, warn_only=False)
    torch.set_num_threads(1)
    torch.manual_seed(config.seed)

    feature_tensor = torch.from_numpy(normalized).to(
        device="cpu",
        dtype=torch.float64,
    )
    legal_tensor = torch.from_numpy(legal_mask).to(
        device="cpu",
        dtype=torch.bool,
    )
    label_tensor = torch.from_numpy(labels).to(
        device="cpu",
        dtype=torch.long,
    )
    weights = torch.zeros(
        len(FeatureOrder),
        dtype=torch.float64,
        device="cpu",
        requires_grad=True,
    )
    final_loss = math.nan
    for _ in range(config.epochs):
        logits = torch.einsum("dcf,f->dc", feature_tensor, weights)
        logits = logits.masked_fill(~legal_tensor, -torch.inf)
        loss = torch.nn.functional.cross_entropy(logits, label_tensor)
        loss = loss + 0.5 * config.l2 * torch.dot(weights, weights)
        (gradient,) = torch.autograd.grad(loss, weights)
        with torch.no_grad():
            weights -= config.learning_rate * gradient
        final_loss = float(loss.detach().cpu().item())
        if not math.isfinite(final_loss) or not bool(
            torch.all(torch.isfinite(weights)).item()
        ):
            raise TrainingDataError("masked BC training became non-finite")

    return (
        weights.detach().cpu().numpy().astype(np.float64, copy=True),
        final_loss,
        str(torch.__version__),
    )


def _RankMaskedExample(
    example: MaskedBCExample,
    mean: np.ndarray,
    inverse_scale: np.ndarray,
    weights: np.ndarray,
) -> tuple[list[int], np.ndarray]:
    logits = ((example.features - mean) * inverse_scale) @ weights
    logits = np.asarray(logits, dtype=np.float64)
    logits[~example.legal_mask] = -math.inf
    if not np.all(np.isfinite(logits[example.legal_mask])):
        raise TrainingDataError("masked BC evaluation produced non-finite logits")
    ranked_indices = sorted(
        np.flatnonzero(example.legal_mask).tolist(),
        key=lambda index: (-float(logits[index]), RuntimePolicyCandidateOrder[index]),
    )
    return ranked_indices, logits


def EvaluateTorchMaskedBC(
    examples: list[MaskedBCExample],
    groups: set[SplitGroup],
    mean: np.ndarray,
    inverse_scale: np.ndarray,
    weights: np.ndarray,
) -> dict[str, Any]:
    top1_hits = 0
    top3_hits = 0
    regret_sum = 0.0
    nll_sum = 0.0
    decision_count = 0
    for example in examples:
        if example.split_group not in groups:
            continue
        ranked_indices, logits = _RankMaskedExample(
            example,
            mean,
            inverse_scale,
            weights,
        )
        selected_rank = ranked_indices.index(example.selected_index)
        top1_hits += int(selected_rank == 0)
        top3_hits += int(selected_rank < min(3, len(ranked_indices)))
        regret_sum += selected_rank / max(1, len(ranked_indices) - 1)
        legal_logits = logits[example.legal_mask]
        maximum = float(np.max(legal_logits))
        log_sum_exp = maximum + math.log(
            float(np.sum(np.exp(legal_logits - maximum)))
        )
        nll_sum += log_sum_exp - float(logits[example.selected_index])
        decision_count += 1
    if decision_count == 0:
        raise TrainingDataError("masked BC evaluation split contains no decisions")
    return {
        "decision_count": decision_count,
        "top1_hits": top1_hits,
        "top1_accuracy": top1_hits / decision_count,
        "top3_hits": top3_hits,
        "top3_accuracy": top3_hits / decision_count,
        "normalized_selected_rank_regret": regret_sum / decision_count,
        "masked_cross_entropy": nll_sum / decision_count,
    }


def SplitGroups(
    groups: set[SplitGroup],
    config: TrainingConfig,
) -> tuple[set[SplitGroup], set[SplitGroup]]:
    if len(groups) < config.minimum_groups:
        raise TrainingDataError(
            f"dataset has {len(groups)} frozen scenario groups; "
            f"requires {config.minimum_groups}"
        )
    ranked = sorted(
        groups,
        key=lambda group: (
            hashlib.sha256(
                (str(config.seed) + "\0" + "\0".join(group)).encode("utf-8")
            ).hexdigest(),
            group,
        ),
    )
    holdout_count = max(
        config.minimum_groups_per_split,
        int(round(len(ranked) * config.holdout_fraction)),
    )
    if len(ranked) - holdout_count < config.minimum_groups_per_split:
        raise TrainingDataError("group split cannot satisfy minimum train groups")
    holdout = set(ranked[:holdout_count])
    train = set(ranked[holdout_count:])
    if train & holdout:
        raise TrainingDataError("frozen scenario group leakage detected")
    return train, holdout


def _ValidateDatasetContracts(
    records: list[dict[str, Any]],
    examples: list[DecisionExample],
    train_groups: set[SplitGroup],
    holdout_groups: set[SplitGroup],
) -> None:
    for field in ("rules_hash", "definition_hash", "policy_revision"):
        values = {record[field] for record in records}
        if len(values) != 1:
            raise TrainingDataError(f"dataset mixes multiple {field} values")

    all_selected = {example.selected_kind for example in examples}
    if len(all_selected) < 2:
        raise TrainingDataError("dataset contains a single selected candidate class")
    for name, groups in (("train", train_groups), ("holdout", holdout_groups)):
        selected = {
            example.selected_kind
            for example in examples
            if example.split_group in groups
        }
        if len(selected) < 2:
            raise TrainingDataError(f"{name} split contains a single selected class")


def _FitNormalizer(
    examples: list[DecisionExample],
    train_groups: set[SplitGroup],
) -> tuple[np.ndarray, np.ndarray]:
    rows = [
        features
        for example in examples
        if example.split_group in train_groups
        for _, features in sorted(example.candidate_features.items())
    ]
    if not rows:
        raise TrainingDataError("training split contains no candidate rows")
    matrix = np.vstack(rows)
    mean = np.mean(matrix, axis=0)
    scale = np.std(matrix, axis=0)
    scale = np.where(scale < 1e-12, 1.0, scale)
    if not np.all(np.isfinite(mean)) or not np.all(np.isfinite(scale)):
        raise TrainingDataError("normalization contains non-finite values")
    return mean, scale


def _BuildPairs(
    examples: list[DecisionExample],
    train_groups: set[SplitGroup],
    mean: np.ndarray,
    scale: np.ndarray,
) -> tuple[np.ndarray, np.ndarray]:
    differences: list[np.ndarray] = []
    labels: list[float] = []
    for example in examples:
        if example.split_group not in train_groups:
            continue
        normalized = {
            kind: (features - mean) / scale
            for kind, features in example.candidate_features.items()
        }
        selected = normalized[example.selected_kind]
        for kind, other in sorted(normalized.items()):
            if kind == example.selected_kind:
                continue
            difference = selected - other
            differences.extend((difference, -difference))
            labels.extend((1.0, 0.0))
    if not differences:
        raise TrainingDataError("training split produced no pairwise rows")
    matrix = np.vstack(differences)
    target = np.asarray(labels, dtype=np.float64)
    if set(target.tolist()) != {0.0, 1.0}:
        raise TrainingDataError("pairwise training labels contain a single class")
    return matrix, target


def TrainPairwiseLogistic(
    pair_features: np.ndarray,
    labels: np.ndarray,
    config: TrainingConfig,
) -> np.ndarray:
    weights = np.zeros(pair_features.shape[1], dtype=np.float64)
    for _ in range(config.epochs):
        logits = np.clip(pair_features @ weights, -40.0, 40.0)
        probabilities = 1.0 / (1.0 + np.exp(-logits))
        gradient = (
            pair_features.T @ (probabilities - labels) / len(labels)
            + config.l2 * weights
        )
        weights -= config.learning_rate * gradient
        if not np.all(np.isfinite(weights)):
            raise TrainingDataError("training produced non-finite weights")
    return weights


def EvaluateRanking(
    examples: list[DecisionExample],
    groups: set[SplitGroup],
    mean: np.ndarray,
    scale: np.ndarray,
    weights: np.ndarray,
) -> dict[str, Any]:
    top1_hits = 0
    top3_hits = 0
    regret_sum = 0.0
    decision_count = 0
    for example in examples:
        if example.split_group not in groups:
            continue
        scored = [
            (
                float(((features - mean) / scale) @ weights),
                kind,
            )
            for kind, features in example.candidate_features.items()
        ]
        scored.sort(key=lambda item: (-item[0], item[1]))
        ranked_kinds = [kind for _, kind in scored]
        selected_rank = ranked_kinds.index(example.selected_kind)
        top1_hits += int(selected_rank == 0)
        top3_hits += int(selected_rank < min(3, len(ranked_kinds)))
        regret_sum += selected_rank / max(1, len(ranked_kinds) - 1)
        decision_count += 1
    if decision_count == 0:
        raise TrainingDataError("evaluation split contains no decision groups")
    return {
        "decision_count": decision_count,
        "top1_hits": top1_hits,
        "top1_accuracy": top1_hits / decision_count,
        "top3_hits": top3_hits,
        "top3_accuracy": top3_hits / decision_count,
        "normalized_selected_rank_regret": regret_sum / decision_count,
    }


def _AddImitationSourceProvenance(
    source: dict[str, Any],
    loaded: LoadedEpisodes,
) -> None:
    if loaded.input_contract == AiEpisodeInputContract:
        return
    if loaded.input_contract != ImitationDecisionInputContract:
        raise TrainingDataError("loaded input contract is invalid")
    if (
        loaded.label_origin_counts is None
        or loaded.source_ai_episode_sha256 is None
        or loaded.correction_sidecar_sha256 is None
    ):
        raise TrainingDataError("materialized imitation provenance is incomplete")
    source.update(
        {
            "input_contract": loaded.input_contract,
            "source_ai_episode_sha256": loaded.source_ai_episode_sha256,
            "correction_sidecar_sha256": loaded.correction_sidecar_sha256,
            "label_origin_counts": dict(sorted(loaded.label_origin_counts.items())),
            "unexecuted_counterfactual_label_count": int(
                loaded.label_origin_counts.get(
                    imitation_schema.LabelOriginHumanDebugCorrection,
                    0,
                )
            ),
        }
    )


def BuildArtifact(
    loaded: LoadedEpisodes,
    config: TrainingConfig,
    evidence_kind: str,
) -> dict[str, Any]:
    config_errors = config.Validate()
    if config_errors:
        raise TrainingDataError("; ".join(config_errors))
    if evidence_kind not in AllowedEvidenceKinds:
        raise TrainingDataError("evidence_kind is invalid")

    examples = BuildDecisionExamples(
        loaded.records,
        loaded.expert_candidate_kinds,
    )
    all_groups = {example.split_group for example in examples}
    train_groups, holdout_groups = SplitGroups(all_groups, config)
    _ValidateDatasetContracts(
        loaded.records,
        examples,
        train_groups,
        holdout_groups,
    )

    mean, scale = _FitNormalizer(examples, train_groups)
    pair_features, labels = _BuildPairs(
        examples,
        train_groups,
        mean,
        scale,
    )
    weights = TrainPairwiseLogistic(pair_features, labels, config)
    train_metrics = EvaluateRanking(
        examples,
        train_groups,
        mean,
        scale,
        weights,
    )
    holdout_metrics = EvaluateRanking(
        examples,
        holdout_groups,
        mean,
        scale,
        weights,
    )

    executor_counts = {
        "accepted": sum(
            example.executor_state == episode_schema.ExecutorAccepted
            for example in examples
        ),
        "rejected": sum(
            example.executor_state == episode_schema.ExecutorRejected
            for example in examples
        ),
    }
    first = loaded.records[0]
    fixture = evidence_kind == FixtureEvidenceKind
    artifact: dict[str, Any] = {
        "schema_version": ArtifactSchemaVersion,
        "artifact_type": ArtifactType,
        "evidence_kind": evidence_kind,
        "model_family": "numpy_pairwise_logistic_ranker",
        "training_paradigm": "supervised_imitation_ranking",
        "reinforcement_learning_used": False,
        "python_transition_model_used": False,
        "pytorch_used": False,
        "performance_claim": (
            "CONTRACT_ONLY_NOT_MEASURED"
            if fixture
            else "OFFLINE_HOLDOUT_ONLY_NOT_PROMOTED"
        ),
        "eligible_for_runtime_promotion": False,
        "policy_state_modified": False,
        "source": {
            "promotion_validation_required": True,
            "input_sha256": loaded.input_sha256,
            "record_count": len(loaded.records),
            "decision_group_count": len(examples),
            "frozen_scenario_group_count": len(all_groups),
            "rules_hash": first["rules_hash"],
            "definition_hash": first["definition_hash"],
            "policy_revision": first["policy_revision"],
            "observation_schema_version": first["observation_schema_version"],
            "action_schema_version": first["action_schema_version"],
            "executor_result_counts": executor_counts,
        },
        "training": {
            **asdict(config),
            "numpy_version": np.__version__,
            "optimizer": "deterministic_full_batch_gradient_descent",
            "initialization": "all_zero_weights",
            "pair_construction": "selected_minus_each_other_legal_candidate",
            "pair_row_count": int(len(labels)),
            "pair_label_zero_count": int(np.sum(labels == 0.0)),
            "pair_label_one_count": int(np.sum(labels == 1.0)),
        },
        "split": {
            "algorithm": "seeded_sha256_episode_scenario_group_holdout",
            "group_key_fields": [
                "scenario_id",
                "rules_hash",
                "definition_hash",
            ],
            "decision_group_key_fields": [
                "episode_id",
                "scenario_id",
                "timeline_epoch",
                "branch_id",
                "tick",
                "self_net_entity_id",
            ],
            "train_groups": [list(group) for group in sorted(train_groups)],
            "holdout_groups": [list(group) for group in sorted(holdout_groups)],
            "group_overlap_count": len(train_groups & holdout_groups),
        },
        "features": {
            "order": list(FeatureOrder),
            "forbidden_sources": list(ForbiddenFeatureSources),
            "normalization_mean": [float(value) for value in mean],
            "normalization_scale": [float(value) for value in scale],
        },
        "weights": [float(value) for value in weights],
        "metrics": {
            "metric_note": (
                "regret is normalized selected-candidate rank; no gameplay "
                "reward or authored candidate score is used"
            ),
            "train": train_metrics,
            "holdout": holdout_metrics,
        },
    }
    _AddImitationSourceProvenance(artifact["source"], loaded)
    artifact["artifact_sha256"] = ComputeArtifactSha256(artifact)
    return artifact


def _CanonicalFloat32Array(
    values: np.ndarray,
    name: str,
    require_positive: bool = False,
) -> np.ndarray:
    source = np.asarray(values, dtype=np.float64)
    if source.shape != (RuntimePolicyFeatureCount,) or not np.all(
        np.isfinite(source)
    ):
        raise TrainingDataError(f"{name} must contain 67 finite values")
    with np.errstate(over="ignore", invalid="ignore"):
        converted = source.astype(np.float32)
    if not np.all(np.isfinite(converted)):
        raise TrainingDataError(f"{name} cannot be represented as float32")
    subnormal = (
        (np.abs(converted) > 0.0)
        & (np.abs(converted) < np.finfo(np.float32).tiny)
    )
    converted[subnormal] = np.float32(0.0)
    converted[converted == 0.0] = np.float32(0.0)
    if require_positive and np.any(converted <= 0.0):
        raise TrainingDataError(f"{name} must remain positive after float32 export")
    return converted.astype("<f4", copy=False)


def BuildRuntimePolicyBinary(
    policy_revision: int,
    source_policy_revision: int,
    normalization_mean: np.ndarray,
    normalization_inverse_scale: np.ndarray,
    weights: np.ndarray,
) -> bytes:
    if not 0 < policy_revision < 1 << 64:
        raise TrainingDataError("policy_revision must fit a positive u64")
    if not 0 <= source_policy_revision < 1 << 64:
        raise TrainingDataError("source policy revision must fit a u64")
    if policy_revision <= source_policy_revision:
        raise TrainingDataError(
            "policy_revision must be greater than source policy revision"
        )
    if len(FeatureOrder) != RuntimePolicyFeatureCount:
        raise TrainingDataError("runtime feature count does not match FeatureOrder")

    mean_f32 = _CanonicalFloat32Array(
        normalization_mean,
        "normalization mean",
    )
    inverse_scale_f32 = _CanonicalFloat32Array(
        normalization_inverse_scale,
        "normalization inverse scale",
        require_positive=True,
    )
    weights_f32 = _CanonicalFloat32Array(weights, "model weight")
    header = struct.pack(
        RuntimePolicyHeaderFormat,
        RuntimePolicyMagic,
        RuntimePolicySchemaVersion,
        RuntimePolicyHeaderBytes,
        RuntimePolicyFileBytes,
        episode_schema.TraceSchemaVersion,
        episode_schema.ObservationSchemaVersion,
        episode_schema.ActionSchemaVersion,
        RuntimePolicyFeatureCount,
        len(RuntimePolicyCandidateOrder),
        RuntimePolicyScalarFloat32,
        policy_revision,
        source_policy_revision,
        FeatureOrderSha256Prefix,
        0,
    )
    result = b"".join(
        (
            header,
            mean_f32.tobytes(order="C"),
            inverse_scale_f32.tobytes(order="C"),
            weights_f32.tobytes(order="C"),
        )
    )
    ValidateRuntimePolicyBinary(result)
    return result


def ValidateRuntimePolicyBinary(binary_bytes: bytes) -> dict[str, Any]:
    if not isinstance(binary_bytes, bytes):
        raise TrainingDataError("runtime policy binary must be bytes")
    if len(binary_bytes) != RuntimePolicyFileBytes:
        raise TrainingDataError(
            f"runtime policy binary size must be {RuntimePolicyFileBytes} bytes"
        )
    try:
        unpacked = struct.unpack_from(
            RuntimePolicyHeaderFormat,
            binary_bytes,
            0,
        )
    except struct.error as error:
        raise TrainingDataError("runtime policy header is truncated") from error
    (
        magic,
        artifact_schema_version,
        header_bytes,
        file_bytes,
        trace_schema_version,
        observation_schema_version,
        action_schema_version,
        feature_count,
        candidate_count,
        scalar_type,
        policy_revision,
        source_policy_revision,
        feature_order_sha256_prefix,
        reserved,
    ) = unpacked
    expected = (
        (magic, RuntimePolicyMagic, "magic"),
        (
            artifact_schema_version,
            RuntimePolicySchemaVersion,
            "artifact schema version",
        ),
        (header_bytes, RuntimePolicyHeaderBytes, "header size"),
        (file_bytes, RuntimePolicyFileBytes, "file size"),
        (
            trace_schema_version,
            episode_schema.TraceSchemaVersion,
            "trace schema version",
        ),
        (
            observation_schema_version,
            episode_schema.ObservationSchemaVersion,
            "observation schema version",
        ),
        (
            action_schema_version,
            episode_schema.ActionSchemaVersion,
            "action schema version",
        ),
        (feature_count, RuntimePolicyFeatureCount, "feature count"),
        (
            candidate_count,
            len(RuntimePolicyCandidateOrder),
            "candidate count",
        ),
        (scalar_type, RuntimePolicyScalarFloat32, "scalar type"),
        (
            feature_order_sha256_prefix,
            FeatureOrderSha256Prefix,
            "feature order hash",
        ),
        (reserved, 0, "reserved field"),
    )
    for actual, required, name in expected:
        if actual != required:
            raise TrainingDataError(f"runtime policy {name} mismatch")
    if policy_revision <= source_policy_revision:
        raise TrainingDataError(
            "runtime policy revision must exceed source policy revision"
        )

    payload = memoryview(binary_bytes)[RuntimePolicyHeaderBytes:]
    arrays: list[np.ndarray] = []
    stride = RuntimePolicyFeatureCount * 4
    for index, name in enumerate(
        ("normalization mean", "normalization inverse scale", "model weight")
    ):
        values = np.frombuffer(
            payload[index * stride : (index + 1) * stride],
            dtype="<f4",
            count=RuntimePolicyFeatureCount,
        ).copy()
        if values.shape != (RuntimePolicyFeatureCount,) or not np.all(
            np.isfinite(values)
        ):
            raise TrainingDataError(f"runtime policy {name} is non-finite")
        if np.any(
            (np.abs(values) > 0.0)
            & (np.abs(values) < np.finfo(np.float32).tiny)
        ):
            raise TrainingDataError(
                f"runtime policy {name} contains non-canonical subnormal values"
            )
        arrays.append(values)
    if np.any(arrays[1] <= 0.0):
        raise TrainingDataError(
            "runtime policy normalization inverse scale must be positive"
        )
    return {
        "policy_revision": policy_revision,
        "source_policy_revision": source_policy_revision,
        "feature_order_sha256_prefix": feature_order_sha256_prefix,
        "normalization_mean": arrays[0],
        "normalization_inverse_scale": arrays[1],
        "weight": arrays[2],
    }


def ComputePolicySha256(artifact: dict[str, Any]) -> str:
    payload = {
        key: value for key, value in artifact.items() if key != "policy_sha256"
    }
    return hashlib.sha256(_CanonicalJsonBytes(payload)).hexdigest()


def ValidateTorchPolicyArtifact(
    artifact: dict[str, Any],
    binary_bytes: bytes | None = None,
) -> None:
    if artifact.get("schema_version") != ArtifactSchemaVersion:
        raise TrainingDataError("policy artifact schema_version mismatch")
    if artifact.get("artifact_type") != PolicyArtifactType:
        raise TrainingDataError("policy artifact type mismatch")
    if not isinstance(artifact.get("policy_id"), str) or not artifact[
        "policy_id"
    ].strip():
        raise TrainingDataError("policy artifact policy_id must be non-empty")
    if artifact.get("evidence_kind") not in AllowedEvidenceKinds:
        raise TrainingDataError("policy artifact evidence_kind is invalid")
    expected_claim = (
        "CONTRACT_ONLY_NOT_MEASURED"
        if artifact["evidence_kind"] == FixtureEvidenceKind
        else "OFFLINE_HOLDOUT_ONLY_NOT_PROMOTED"
    )
    if artifact.get("performance_claim") != expected_claim:
        raise TrainingDataError("policy artifact performance claim is unsafe")
    if artifact.get("policy_sha256") != ComputePolicySha256(artifact):
        raise TrainingDataError("policy artifact SHA-256 mismatch")

    expected_runtime_contract = {
        "trace_schema_version": episode_schema.TraceSchemaVersion,
        "observation_schema_version": episode_schema.ObservationSchemaVersion,
        "action_schema_version": episode_schema.ActionSchemaVersion,
        "candidate_order": list(RuntimePolicyCandidateOrder),
        "feature_count": RuntimePolicyFeatureCount,
        "legal_mask_required": True,
        "tie_break": "LOWEST_CANDIDATE_KIND",
    }
    if artifact.get("runtime_contract") != expected_runtime_contract:
        raise TrainingDataError("policy artifact runtime contract mismatch")
    expected_safety = {
        "pytorch_used": True,
        "reinforcement_learning_used": False,
        "python_transition_model_used": False,
        "runtime_mode": "SHADOW_ONLY",
        "eligible_for_runtime_promotion": False,
        "policy_state_modified": False,
        "policy_promotion_gate_required": True,
    }
    if artifact.get("safety") != expected_safety:
        raise TrainingDataError("policy artifact safety claims are invalid")

    features = artifact.get("features")
    model = artifact.get("model")
    if not isinstance(features, dict) or not isinstance(model, dict):
        raise TrainingDataError("policy artifact model/features are missing")
    if features.get("order") != list(FeatureOrder):
        raise TrainingDataError("policy artifact feature order mismatch")
    if features.get("order_sha256") != FeatureOrderSha256:
        raise TrainingDataError("policy artifact feature order SHA mismatch")
    try:
        mean = np.asarray(features["normalization_mean"], dtype=np.float64)
        inverse_scale = np.asarray(
            features["normalization_inverse_scale"],
            dtype=np.float64,
        )
        weight = np.asarray(model["weight"], dtype=np.float64)
    except (KeyError, TypeError, ValueError) as error:
        raise TrainingDataError("policy artifact arrays are malformed") from error
    for values, name in (
        (mean, "normalization mean"),
        (inverse_scale, "normalization inverse scale"),
        (weight, "model weight"),
    ):
        if values.shape != (RuntimePolicyFeatureCount,) or not np.all(
            np.isfinite(values)
        ):
            raise TrainingDataError(f"policy artifact {name} is invalid")
    if np.any(inverse_scale <= 0.0):
        raise TrainingDataError(
            "policy artifact normalization inverse scale must be positive"
        )

    source = artifact.get("source")
    if not isinstance(source, dict):
        raise TrainingDataError("policy artifact source is missing")
    policy_revision = artifact.get("policy_revision")
    source_policy_revision = source.get("policy_revision")
    if (
        not isinstance(policy_revision, int)
        or isinstance(policy_revision, bool)
        or not isinstance(source_policy_revision, int)
        or isinstance(source_policy_revision, bool)
        or policy_revision <= source_policy_revision
    ):
        raise TrainingDataError("policy artifact revision ordering is invalid")

    runtime_binary = artifact.get("runtime_binary")
    if not isinstance(runtime_binary, dict):
        raise TrainingDataError("policy artifact runtime_binary is missing")
    if runtime_binary.get("magic") != RuntimePolicyMagic.rstrip(b"\0").decode(
        "ascii"
    ):
        raise TrainingDataError("policy artifact runtime binary magic mismatch")
    if runtime_binary.get("file_bytes") != RuntimePolicyFileBytes:
        raise TrainingDataError("policy artifact runtime binary size mismatch")
    if runtime_binary.get("feature_order_sha256_prefix") != (
        f"{FeatureOrderSha256Prefix:016x}"
    ):
        raise TrainingDataError("policy artifact runtime order hash mismatch")
    runtime_sha256 = runtime_binary.get("sha256")
    if (
        not isinstance(runtime_sha256, str)
        or len(runtime_sha256) != 64
        or any(
            character not in "0123456789abcdef"
            for character in runtime_sha256
        )
    ):
        raise TrainingDataError("policy artifact runtime binary SHA is invalid")
    if binary_bytes is not None:
        decoded = ValidateRuntimePolicyBinary(binary_bytes)
        binary_sha256 = hashlib.sha256(binary_bytes).hexdigest()
        if runtime_sha256 != binary_sha256:
            raise TrainingDataError("policy artifact runtime binary SHA mismatch")
        if decoded["policy_revision"] != policy_revision or decoded[
            "source_policy_revision"
        ] != source_policy_revision:
            raise TrainingDataError("policy artifact runtime revision mismatch")
        if not np.array_equal(
            decoded["normalization_mean"],
            np.asarray(mean, dtype=np.float32),
        ) or not np.array_equal(
            decoded["normalization_inverse_scale"],
            np.asarray(inverse_scale, dtype=np.float32),
        ) or not np.array_equal(
            decoded["weight"],
            np.asarray(weight, dtype=np.float32),
        ):
            raise TrainingDataError(
                "policy artifact runtime parameters do not match binary"
            )


def BuildTorchPolicyArtifact(
    loaded: LoadedEpisodes,
    config: TrainingConfig,
    evidence_kind: str,
    policy_id: str,
    policy_revision: int,
) -> MaskedBCRuntimeArtifact:
    config_errors = config.Validate()
    if config_errors:
        raise TrainingDataError("; ".join(config_errors))
    if evidence_kind not in AllowedEvidenceKinds:
        raise TrainingDataError("evidence_kind is invalid")
    if not isinstance(policy_id, str) or not policy_id.strip():
        raise TrainingDataError("policy_id is required for pytorch-masked-bc")
    if not isinstance(policy_revision, int) or isinstance(policy_revision, bool):
        raise TrainingDataError("policy_revision is required for pytorch-masked-bc")

    decision_examples = BuildDecisionExamples(
        loaded.records,
        loaded.expert_candidate_kinds,
    )
    examples = BuildMaskedBCExamples(
        loaded.records,
        loaded.expert_candidate_kinds,
    )
    all_groups = {example.split_group for example in examples}
    train_groups, holdout_groups = SplitGroups(all_groups, config)
    _ValidateDatasetContracts(
        loaded.records,
        decision_examples,
        train_groups,
        holdout_groups,
    )
    for field in (
        "trace_schema_version",
        "observation_schema_version",
        "action_schema_version",
    ):
        if len({record[field] for record in loaded.records}) != 1:
            raise TrainingDataError(f"dataset mixes multiple {field} values")

    source_policy_revision = int(loaded.records[0]["policy_revision"])
    if policy_revision <= source_policy_revision:
        raise TrainingDataError(
            "policy_revision must be greater than source policy revision"
        )
    mean, inverse_scale = FitMaskedNormalizer(examples, train_groups)
    weights, final_loss, torch_version = TrainTorchMaskedBC(
        examples,
        train_groups,
        mean,
        inverse_scale,
        config,
    )
    mean_f32 = _CanonicalFloat32Array(mean, "normalization mean")
    inverse_scale_f32 = _CanonicalFloat32Array(
        inverse_scale,
        "normalization inverse scale",
        require_positive=True,
    )
    weights_f32 = _CanonicalFloat32Array(weights, "model weight")
    binary_bytes = BuildRuntimePolicyBinary(
        policy_revision,
        source_policy_revision,
        mean_f32,
        inverse_scale_f32,
        weights_f32,
    )
    binary_sha256 = hashlib.sha256(binary_bytes).hexdigest()

    runtime_mean = mean_f32.astype(np.float64)
    runtime_inverse_scale = inverse_scale_f32.astype(np.float64)
    runtime_weights = weights_f32.astype(np.float64)
    train_metrics = EvaluateTorchMaskedBC(
        examples,
        train_groups,
        runtime_mean,
        runtime_inverse_scale,
        runtime_weights,
    )
    holdout_metrics = EvaluateTorchMaskedBC(
        examples,
        holdout_groups,
        runtime_mean,
        runtime_inverse_scale,
        runtime_weights,
    )

    first = loaded.records[0]
    fixture = evidence_kind == FixtureEvidenceKind
    executor_counts = {
        "accepted": sum(
            int(record["executor"]["state"])
            == episode_schema.ExecutorAccepted
            for record in loaded.records
        ),
        "rejected": sum(
            int(record["executor"]["state"])
            == episode_schema.ExecutorRejected
            for record in loaded.records
        ),
    }
    report: dict[str, Any] = {
        "schema_version": ArtifactSchemaVersion,
        "artifact_type": PolicyArtifactType,
        "policy_id": policy_id.strip(),
        "policy_revision": policy_revision,
        "evidence_kind": evidence_kind,
        "performance_claim": (
            "CONTRACT_ONLY_NOT_MEASURED"
            if fixture
            else "OFFLINE_HOLDOUT_ONLY_NOT_PROMOTED"
        ),
        "runtime_contract": {
            "trace_schema_version": episode_schema.TraceSchemaVersion,
            "observation_schema_version": episode_schema.ObservationSchemaVersion,
            "action_schema_version": episode_schema.ActionSchemaVersion,
            "candidate_order": list(RuntimePolicyCandidateOrder),
            "feature_count": RuntimePolicyFeatureCount,
            "legal_mask_required": True,
            "tie_break": "LOWEST_CANDIDATE_KIND",
        },
        "source": {
            "promotion_validation_required": True,
            "input_sha256": loaded.input_sha256,
            "record_count": len(loaded.records),
            "decision_group_count": len(examples),
            "frozen_scenario_group_count": len(all_groups),
            "rules_hash": first["rules_hash"],
            "definition_hash": first["definition_hash"],
            "policy_revision": source_policy_revision,
            "executor_result_counts": executor_counts,
        },
        "training": {
            **asdict(config),
            "backend": "pytorch-masked-bc",
            "python_version": platform.python_version(),
            "numpy_version": np.__version__,
            "pytorch_version": torch_version,
            "device": "cpu",
            "dtype": "float64",
            "deterministic_algorithms": True,
            "thread_count": 1,
            "optimizer": "manual_full_batch_gradient_descent",
            "initialization": "all_zero_weights",
            "objective": "legal_masked_cross_entropy_plus_l2",
            "final_regularized_training_loss": final_loss,
            "legal_candidate_row_count": sum(
                int(np.sum(example.legal_mask))
                for example in examples
                if example.split_group in train_groups
            ),
        },
        "split": {
            "algorithm": "seeded_sha256_episode_scenario_group_holdout",
            "group_key_fields": [
                "scenario_id",
                "rules_hash",
                "definition_hash",
            ],
            "decision_group_key_fields": [
                "episode_id",
                "scenario_id",
                "timeline_epoch",
                "branch_id",
                "tick",
                "self_net_entity_id",
            ],
            "train_groups": [list(group) for group in sorted(train_groups)],
            "holdout_groups": [list(group) for group in sorted(holdout_groups)],
            "group_overlap_count": len(train_groups & holdout_groups),
        },
        "features": {
            "order": list(FeatureOrder),
            "order_sha256": FeatureOrderSha256,
            "forbidden_sources": list(ForbiddenFeatureSources),
            "normalization_scope": "TRAIN_LEGAL_CANDIDATE_ROWS_ONLY",
            "normalization_mean": [float(value) for value in mean_f32],
            "normalization_inverse_scale": [
                float(value) for value in inverse_scale_f32
            ],
        },
        "model": {
            "family": "shared_bias_free_linear_candidate_scorer",
            "weight": [float(value) for value in weights_f32],
        },
        "metrics": {
            "metric_note": (
                "offline ranking over legal candidates using float32 runtime "
                "parameters; no gameplay reward or authored score is used"
            ),
            "train": train_metrics,
            "holdout": holdout_metrics,
        },
        "runtime_binary": {
            "artifact_type": "ChampionAIShadowPolicyBinaryV1",
            "magic": RuntimePolicyMagic.rstrip(b"\0").decode("ascii"),
            "header_bytes": RuntimePolicyHeaderBytes,
            "file_bytes": RuntimePolicyFileBytes,
            "scalar_type": "IEEE754_FLOAT32_LE",
            "feature_order_sha256_prefix": (
                f"{FeatureOrderSha256Prefix:016x}"
            ),
            "sha256": binary_sha256,
        },
        "safety": {
            "pytorch_used": True,
            "reinforcement_learning_used": False,
            "python_transition_model_used": False,
            "runtime_mode": "SHADOW_ONLY",
            "eligible_for_runtime_promotion": False,
            "policy_state_modified": False,
            "policy_promotion_gate_required": True,
        },
    }
    _AddImitationSourceProvenance(report["source"], loaded)
    report["policy_sha256"] = ComputePolicySha256(report)
    ValidateTorchPolicyArtifact(report, binary_bytes)
    return MaskedBCRuntimeArtifact(
        report=report,
        binary_bytes=binary_bytes,
        binary_sha256=binary_sha256,
    )


def _AtomicWriteBytes(path: Path, payload: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    try:
        with temporary.open("wb") as stream:
            stream.write(payload)
            stream.flush()
            os.fsync(stream.fileno())
        temporary.replace(path)
    finally:
        if temporary.exists():
            temporary.unlink()


def WriteTorchPolicyArtifact(
    path: Path,
    artifact: dict[str, Any],
) -> None:
    ValidateTorchPolicyArtifact(artifact)
    _AtomicWriteBytes(path, _CanonicalJsonBytes(artifact) + b"\n")


def WriteRuntimePolicyBinary(path: Path, binary_bytes: bytes) -> None:
    ValidateRuntimePolicyBinary(binary_bytes)
    _AtomicWriteBytes(path, binary_bytes)


def WriteArtifact(path: Path, artifact: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_bytes(_CanonicalJsonBytes(artifact) + b"\n")
    temporary.replace(path)


def Main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Train a deterministic supervised imitation ranker from "
            "promotion-valid AiEpisodeV1 or validated ImitationDecisionV1 JSONL."
        )
    )
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument(
        "--input-contract",
        choices=tuple(sorted(AllowedInputContracts)),
        default=AiEpisodeInputContract,
        help=(
            "Read promotion-valid AiEpisodeV1 directly or an explicit "
            "ImitationDecisionV1 materialization."
        ),
    )
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument(
        "--backend",
        choices=("numpy-pairwise", "pytorch-masked-bc"),
        default="numpy-pairwise",
    )
    parser.add_argument("--runtime-output", type=Path)
    parser.add_argument("--policy-id")
    parser.add_argument("--policy-revision", type=int)
    parser.add_argument("--seed", type=int, default=1729)
    parser.add_argument("--holdout-fraction", type=float, default=0.25)
    parser.add_argument("--minimum-groups", type=int, default=10)
    parser.add_argument("--minimum-groups-per-split", type=int, default=2)
    parser.add_argument("--epochs", type=int, default=400)
    parser.add_argument("--learning-rate", type=float, default=0.08)
    parser.add_argument("--l2", type=float, default=0.001)
    parser.add_argument(
        "--fixture-contract",
        action="store_true",
        help="Mark synthetic input as contract-only evidence.",
    )
    args = parser.parse_args(argv)

    config = TrainingConfig(
        seed=args.seed,
        holdout_fraction=args.holdout_fraction,
        minimum_groups=args.minimum_groups,
        minimum_groups_per_split=args.minimum_groups_per_split,
        epochs=args.epochs,
        learning_rate=args.learning_rate,
        l2=args.l2,
    )
    try:
        loaded = LoadTrainingEpisodes(args.input, args.input_contract)
        evidence_kind = (
            FixtureEvidenceKind if args.fixture_contract else MeasuredEvidenceKind
        )
        if args.backend == "numpy-pairwise":
            artifact = BuildArtifact(
                loaded,
                config,
                evidence_kind,
            )
            WriteArtifact(args.output, artifact)
        else:
            if args.runtime_output is None:
                raise TrainingDataError(
                    "--runtime-output is required for pytorch-masked-bc"
                )
            if args.policy_id is None or not args.policy_id.strip():
                raise TrainingDataError(
                    "--policy-id is required for pytorch-masked-bc"
                )
            if args.policy_revision is None:
                raise TrainingDataError(
                    "--policy-revision is required for pytorch-masked-bc"
                )
            runtime_artifact = BuildTorchPolicyArtifact(
                loaded,
                config,
                evidence_kind,
                args.policy_id,
                args.policy_revision,
            )
            WriteRuntimePolicyBinary(
                args.runtime_output,
                runtime_artifact.binary_bytes,
            )
            WriteTorchPolicyArtifact(args.output, runtime_artifact.report)
    except (OSError, UnicodeDecodeError, TrainingDataError) as error:
        print(f"ImitationRankingBaseline FAILED: {error}")
        return 1

    if args.backend == "numpy-pairwise":
        print(
            "ImitationRankingBaseline PASS: "
            f"artifact={artifact['artifact_sha256']} "
            f"holdout_top1={artifact['metrics']['holdout']['top1_accuracy']:.6f} "
            f"holdout_top3={artifact['metrics']['holdout']['top3_accuracy']:.6f} "
            f"contract_only={args.fixture_contract}"
        )
    else:
        print(
            "PyTorchMaskedBC PASS: "
            f"policy={runtime_artifact.report['policy_sha256']} "
            f"binary={runtime_artifact.binary_sha256} "
            "runtime_mode=SHADOW_ONLY "
            f"holdout_top1={runtime_artifact.report['metrics']['holdout']['top1_accuracy']:.6f} "
            f"contract_only={args.fixture_contract}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(Main())
