from __future__ import annotations

import ctypes
import math
from pathlib import Path
from typing import Any

from AiEpisodeSchema import (
    ActionSchemaVersion,
    AllCandidateBits,
    ExecutorAccepted,
    ExecutorRejected,
    ExecutorSubmitted,
    ExecutorUnknown,
    ObservationPrivilegedSourceFlag,
    ObservationSchemaVersion,
    ObservationTeamFilteredFlag,
    SchemaVersion,
    TraceSchemaVersion,
    ValidateRecord,
)


FeatureCapacity = 4
CandidateCapacity = 4


class TraceCodecError(ValueError):
    pass


class AiObservationV1(ctypes.LittleEndianStructure):
    _fields_ = [
        ("schema_version", ctypes.c_uint16),
        ("byte_size", ctypes.c_uint16),
        ("capability_flags", ctypes.c_uint32),
        ("provenance_flags", ctypes.c_uint32),
        ("reserved_header", ctypes.c_uint32),
        ("fact_tick", ctypes.c_uint64),
        ("self_net_entity_id", ctypes.c_uint32),
        ("enemy_champion_net_entity_id", ctypes.c_uint32),
        ("enemy_minion_net_entity_id", ctypes.c_uint32),
        ("enemy_structure_net_entity_id", ctypes.c_uint32),
        ("allied_wave_net_entity_id", ctypes.c_uint32),
        ("self_level", ctypes.c_uint8),
        ("enemy_level", ctypes.c_uint8),
        ("reserved0", ctypes.c_uint16),
        ("self_hp_ratio", ctypes.c_float),
        ("enemy_hp_ratio", ctypes.c_float),
        ("self_gold", ctypes.c_float),
        ("enemy_gold", ctypes.c_float),
        ("enemy_distance", ctypes.c_float),
        ("attack_range", ctypes.c_float),
        ("turret_danger", ctypes.c_float),
        ("reserved1", ctypes.c_uint32),
    ]


class AiActionMaskV1(ctypes.LittleEndianStructure):
    _fields_ = [
        ("schema_version", ctypes.c_uint16),
        ("byte_size", ctypes.c_uint16),
        ("legal_candidate_mask", ctypes.c_uint32),
        ("illegal_candidate_mask", ctypes.c_uint32),
        ("available_action_mask", ctypes.c_uint32),
        ("available_skill_mask", ctypes.c_uint32),
    ]


class AiFeatureContributionV1(ctypes.LittleEndianStructure):
    _fields_ = [
        ("schema_version", ctypes.c_uint16),
        ("byte_size", ctypes.c_uint16),
        ("feature_id", ctypes.c_uint16),
        ("reserved0", ctypes.c_uint16),
        ("raw_value", ctypes.c_float),
        ("weight", ctypes.c_float),
        ("contribution", ctypes.c_float),
    ]


class AiCandidateEvidenceV1(ctypes.LittleEndianStructure):
    _fields_ = [
        ("schema_version", ctypes.c_uint16),
        ("byte_size", ctypes.c_uint16),
        ("candidate_kind", ctypes.c_uint8),
        ("flags", ctypes.c_uint8),
        ("contribution_count", ctypes.c_uint8),
        ("reserved0", ctypes.c_uint8),
        ("target_net_entity_id", ctypes.c_uint32),
        ("score", ctypes.c_float),
        (
            "contributions",
            AiFeatureContributionV1 * FeatureCapacity,
        ),
    ]


class AiDecisionTraceV1(ctypes.LittleEndianStructure):
    _fields_ = [
        ("schema_version", ctypes.c_uint16),
        ("byte_size", ctypes.c_uint16),
        ("candidate_count", ctypes.c_uint8),
        ("selected_candidate_kind", ctypes.c_uint8),
        ("executor_state", ctypes.c_uint8),
        ("command_kind", ctypes.c_uint8),
        ("command_slot", ctypes.c_uint8),
        ("block_reason", ctypes.c_uint8),
        ("executor_reason", ctypes.c_uint16),
        ("state", ctypes.c_uint8),
        ("intent", ctypes.c_uint8),
        ("action", ctypes.c_uint8),
        ("reserved0", ctypes.c_uint8 * 1),
        ("tick", ctypes.c_uint64),
        ("observation", AiObservationV1),
        ("action_mask", AiActionMaskV1),
        ("candidates", AiCandidateEvidenceV1 * CandidateCapacity),
        ("command_target_net_entity_id", ctypes.c_uint32),
        ("command_sequence", ctypes.c_uint32),
        ("command_position_x", ctypes.c_float),
        ("command_position_y", ctypes.c_float),
        ("command_position_z", ctypes.c_float),
    ]


ExpectedSizes = {
    AiObservationV1: 80,
    AiActionMaskV1: 20,
    AiFeatureContributionV1: 20,
    AiCandidateEvidenceV1: 96,
    AiDecisionTraceV1: 528,
}

for _record_type, _expected_size in ExpectedSizes.items():
    _actual_size = ctypes.sizeof(_record_type)
    if _actual_size != _expected_size:
        raise RuntimeError(
            f"{_record_type.__name__} layout mismatch: "
            f"expected={_expected_size} actual={_actual_size}"
        )


def _CanonicalFloat(value: float, path: str) -> float:
    result = float(value)
    if not math.isfinite(result):
        raise TraceCodecError(f"{path} must be finite")
    return 0.0 if result == 0.0 else result


def _ReservedBytesAreZero(values: Any) -> bool:
    return all(int(value) == 0 for value in values)


def ValidateTrace(trace: AiDecisionTraceV1, index: int) -> list[str]:
    prefix = f"trace[{index}]"
    errors: list[str] = []
    if trace.schema_version != TraceSchemaVersion:
        errors.append(f"{prefix}.schema_version must be {TraceSchemaVersion}")
    if trace.byte_size != ctypes.sizeof(AiDecisionTraceV1):
        errors.append(f"{prefix}.byte_size does not match AiDecisionTraceV1")
    if trace.candidate_count == 0 or trace.candidate_count > CandidateCapacity:
        errors.append(f"{prefix}.candidate_count must be in [1, 4]")
    if trace.executor_state not in {
        ExecutorUnknown,
        ExecutorSubmitted,
        ExecutorAccepted,
        ExecutorRejected,
    }:
        errors.append(f"{prefix}.executor_state is invalid")
    if not _ReservedBytesAreZero(trace.reserved0):
        errors.append(f"{prefix}.reserved0 must be zero")

    observation = trace.observation
    if observation.schema_version != ObservationSchemaVersion:
        errors.append(
            f"{prefix}.observation.schema_version must be "
            f"{ObservationSchemaVersion}"
        )
    if observation.byte_size != ctypes.sizeof(AiObservationV1):
        errors.append(f"{prefix}.observation.byte_size is invalid")
    if observation.fact_tick != trace.tick:
        errors.append(f"{prefix}.observation.fact_tick must equal trace.tick")
    if observation.self_net_entity_id == 0:
        errors.append(f"{prefix}.observation.self_net_entity_id is zero")
    provenance = observation.provenance_flags & (
        ObservationPrivilegedSourceFlag | ObservationTeamFilteredFlag
    )
    if provenance == 0:
        errors.append(
            f"{prefix}.observation provenance must identify its source"
        )
    if (
        observation.reserved_header != 0
        or observation.reserved0 != 0
        or observation.reserved1 != 0
    ):
        errors.append(f"{prefix}.observation reserved fields must be zero")

    action_mask = trace.action_mask
    if action_mask.schema_version != ActionSchemaVersion:
        errors.append(
            f"{prefix}.action_mask.schema_version must be {ActionSchemaVersion}"
        )
    if action_mask.byte_size != ctypes.sizeof(AiActionMaskV1):
        errors.append(f"{prefix}.action_mask.byte_size is invalid")
    if action_mask.legal_candidate_mask & action_mask.illegal_candidate_mask:
        errors.append(f"{prefix} candidate masks overlap")
    if (
        action_mask.legal_candidate_mask | action_mask.illegal_candidate_mask
    ) != AllCandidateBits:
        errors.append(f"{prefix} candidate masks do not cover V1 candidates")

    finite_values = (
        (observation.self_hp_ratio, "observation.self_hp_ratio"),
        (observation.enemy_hp_ratio, "observation.enemy_hp_ratio"),
        (observation.self_gold, "observation.self_gold"),
        (observation.enemy_gold, "observation.enemy_gold"),
        (observation.enemy_distance, "observation.enemy_distance"),
        (observation.attack_range, "observation.attack_range"),
        (observation.turret_danger, "observation.turret_danger"),
        (trace.command_position_x, "command_position_x"),
        (trace.command_position_y, "command_position_y"),
        (trace.command_position_z, "command_position_z"),
    )
    for value, path in finite_values:
        if not math.isfinite(float(value)):
            errors.append(f"{prefix}.{path} must be finite")

    for candidate_index in range(CandidateCapacity):
        candidate = trace.candidates[candidate_index]
        candidate_path = f"{prefix}.candidates[{candidate_index}]"
        if candidate.schema_version != TraceSchemaVersion:
            errors.append(f"{candidate_path}.schema_version must be 1")
        if candidate.byte_size != ctypes.sizeof(AiCandidateEvidenceV1):
            errors.append(f"{candidate_path}.byte_size is invalid")
        if candidate.reserved0 != 0:
            errors.append(f"{candidate_path}.reserved0 must be zero")
        if candidate.contribution_count > FeatureCapacity:
            errors.append(f"{candidate_path}.contribution_count exceeds 4")
        if not math.isfinite(float(candidate.score)):
            errors.append(f"{candidate_path}.score must be finite")
        for contribution_index in range(FeatureCapacity):
            contribution = candidate.contributions[contribution_index]
            contribution_path = (
                f"{candidate_path}.contributions[{contribution_index}]"
            )
            if contribution.schema_version != TraceSchemaVersion:
                errors.append(f"{contribution_path}.schema_version must be 1")
            if contribution.byte_size != ctypes.sizeof(
                AiFeatureContributionV1
            ):
                errors.append(f"{contribution_path}.byte_size is invalid")
            if contribution.reserved0 != 0:
                errors.append(f"{contribution_path}.reserved0 must be zero")
            for value, field in (
                (contribution.raw_value, "raw_value"),
                (contribution.weight, "weight"),
                (contribution.contribution, "contribution"),
            ):
                if not math.isfinite(float(value)):
                    errors.append(f"{contribution_path}.{field} must be finite")
    return errors


def DecodeTraceBytes(data: bytes) -> list[AiDecisionTraceV1]:
    record_size = ctypes.sizeof(AiDecisionTraceV1)
    if not data:
        raise TraceCodecError("capture contains no AiDecisionTraceV1 records")
    if len(data) % record_size != 0:
        raise TraceCodecError(
            f"capture size {len(data)} is not a multiple of {record_size}"
        )

    traces: list[AiDecisionTraceV1] = []
    errors: list[str] = []
    seen_identities: dict[tuple[int, int, int], int] = {}
    for index, offset in enumerate(range(0, len(data), record_size)):
        trace = AiDecisionTraceV1.from_buffer_copy(
            data[offset : offset + record_size]
        )
        errors.extend(ValidateTrace(trace, index))
        identity = (
            int(trace.tick),
            int(trace.observation.self_net_entity_id),
            int(trace.command_sequence),
        )
        first_index = seen_identities.get(identity)
        if first_index is not None:
            errors.append(
                f"trace[{index}] duplicates trace[{first_index}] identity "
                f"{identity}"
            )
        else:
            seen_identities[identity] = index
        traces.append(trace)

    if errors:
        raise TraceCodecError("; ".join(errors))
    return traces


def DecodeTraceFile(path: Path) -> list[AiDecisionTraceV1]:
    return DecodeTraceBytes(path.read_bytes())


def TraceToEpisodeRecord(
    trace: AiDecisionTraceV1,
    metadata: dict[str, Any],
    transition: dict[str, Any],
) -> dict[str, Any]:
    observation = trace.observation
    action_mask = trace.action_mask
    candidates: list[dict[str, Any]] = []
    for candidate_index in range(trace.candidate_count):
        candidate = trace.candidates[candidate_index]
        contributions: list[dict[str, Any]] = []
        for contribution_index in range(candidate.contribution_count):
            contribution = candidate.contributions[contribution_index]
            contributions.append(
                {
                    "feature_id": int(contribution.feature_id),
                    "raw_value": _CanonicalFloat(
                        contribution.raw_value,
                        "contribution.raw_value",
                    ),
                    "weight": _CanonicalFloat(
                        contribution.weight,
                        "contribution.weight",
                    ),
                    "contribution": _CanonicalFloat(
                        contribution.contribution,
                        "contribution.contribution",
                    ),
                }
            )
        candidates.append(
            {
                "kind": int(candidate.candidate_kind),
                "flags": int(candidate.flags),
                "target_net_entity_id": int(candidate.target_net_entity_id),
                "score": _CanonicalFloat(candidate.score, "candidate.score"),
                "contributions": contributions,
            }
        )

    record: dict[str, Any] = {
        "schema_version": SchemaVersion,
        "trace_schema_version": int(trace.schema_version),
        "episode_id": metadata["episode_id"],
        "scenario_id": metadata["scenario_id"],
        "tick": int(trace.tick),
        "timeline_epoch": metadata["timeline_epoch"],
        "branch_id": metadata["branch_id"],
        "seed": metadata["seed"],
        "rules_hash": metadata["rules_hash"],
        "definition_hash": metadata["definition_hash"],
        "policy_revision": metadata["policy_revision"],
        "observation_schema_version": int(observation.schema_version),
        "action_schema_version": int(action_mask.schema_version),
        "observation": {
            "capability_flags": int(observation.capability_flags),
            "provenance_flags": int(observation.provenance_flags),
            "fact_tick": int(observation.fact_tick),
            "self_net_entity_id": int(observation.self_net_entity_id),
            "enemy_champion_net_entity_id": int(
                observation.enemy_champion_net_entity_id
            ),
            "enemy_minion_net_entity_id": int(
                observation.enemy_minion_net_entity_id
            ),
            "enemy_structure_net_entity_id": int(
                observation.enemy_structure_net_entity_id
            ),
            "allied_wave_net_entity_id": int(
                observation.allied_wave_net_entity_id
            ),
            "self_level": int(observation.self_level),
            "enemy_level": int(observation.enemy_level),
            "self_hp_ratio": _CanonicalFloat(
                observation.self_hp_ratio,
                "observation.self_hp_ratio",
            ),
            "enemy_hp_ratio": _CanonicalFloat(
                observation.enemy_hp_ratio,
                "observation.enemy_hp_ratio",
            ),
            "self_gold": _CanonicalFloat(
                observation.self_gold,
                "observation.self_gold",
            ),
            "enemy_gold": _CanonicalFloat(
                observation.enemy_gold,
                "observation.enemy_gold",
            ),
            "enemy_distance": _CanonicalFloat(
                observation.enemy_distance,
                "observation.enemy_distance",
            ),
            "attack_range": _CanonicalFloat(
                observation.attack_range,
                "observation.attack_range",
            ),
            "turret_danger": _CanonicalFloat(
                observation.turret_danger,
                "observation.turret_danger",
            ),
        },
        "action_mask": {
            "legal_candidate_mask": int(action_mask.legal_candidate_mask),
            "illegal_candidate_mask": int(action_mask.illegal_candidate_mask),
            "available_action_mask": int(action_mask.available_action_mask),
            "available_skill_mask": int(action_mask.available_skill_mask),
        },
        "candidates": candidates,
        "selected_candidate_kind": int(trace.selected_candidate_kind),
        "command": {
            "kind": int(trace.command_kind),
            "slot": int(trace.command_slot),
            "target_net_entity_id": int(trace.command_target_net_entity_id),
            "sequence": int(trace.command_sequence),
            "position": [
                _CanonicalFloat(trace.command_position_x, "command.position[0]"),
                _CanonicalFloat(trace.command_position_y, "command.position[1]"),
                _CanonicalFloat(trace.command_position_z, "command.position[2]"),
            ],
        },
        "executor": {
            "state": int(trace.executor_state),
            "reason": int(trace.executor_reason),
        },
        "next_state_hash": transition["next_state_hash"],
        "reward": _CanonicalFloat(transition["reward"], "transition.reward"),
        "terminal": transition["terminal"],
        "truncated": transition["truncated"],
    }
    validation_errors = ValidateRecord(record, promotion=False)
    if validation_errors:
        raise TraceCodecError("; ".join(validation_errors))
    return record
