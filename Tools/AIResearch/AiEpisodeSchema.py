from __future__ import annotations

import math
import re
from typing import Any


SchemaVersion = 1
TraceSchemaVersion = 1
ObservationSchemaVersion = 1
ActionSchemaVersion = 1

ObservationPrivilegedSourceFlag = 1 << 0
ObservationTeamFilteredFlag = 1 << 1
AllObservationProvenanceFlags = (
    ObservationPrivilegedSourceFlag | ObservationTeamFilteredFlag
)
AllObservationCapabilityFlags = (1 << 7) - 1

ExecutorUnknown = 0
ExecutorSubmitted = 1
ExecutorAccepted = 2
ExecutorRejected = 3

CandidateRetreatBit = 1 << 0
CandidateFightBit = 1 << 1
CandidateFarmBit = 1 << 2
CandidateSiegeBit = 1 << 3
AllCandidateBits = (
    CandidateRetreatBit
    | CandidateFightBit
    | CandidateFarmBit
    | CandidateSiegeBit
)

CandidateLegalFlag = 1 << 0
CandidateSelectedFlag = 1 << 1
CandidateHasTargetFlag = 1 << 2
AllCandidateFlags = (
    CandidateLegalFlag | CandidateSelectedFlag | CandidateHasTargetFlag
)
AllAvailableActionFlags = (1 << 7) - 1
AllAvailableSkillFlags = (1 << 4) - 1

KnownCommandKinds = {1, 2, 3, 5, 7, 8, 10}
KnownCommandSlots = {0, 1, 2, 3, 4}
KnownExecutorReasons = set(range(0, 23))
KnownFeatureIds = {1}

CandidateKinds = {1, 2, 3, 4}
CandidateKindToBit = {
    1: CandidateRetreatBit,
    2: CandidateFightBit,
    3: CandidateFarmBit,
    4: CandidateSiegeBit,
}

RequiredRecordFields = {
    "schema_version",
    "trace_schema_version",
    "episode_id",
    "scenario_id",
    "tick",
    "timeline_epoch",
    "branch_id",
    "seed",
    "rules_hash",
    "definition_hash",
    "policy_revision",
    "observation_schema_version",
    "action_schema_version",
    "observation",
    "action_mask",
    "candidates",
    "selected_candidate_kind",
    "command",
    "executor",
    "next_state_hash",
    "reward",
    "terminal",
    "truncated",
}

RequiredObservationFields = {
    "capability_flags",
    "provenance_flags",
    "fact_tick",
    "self_net_entity_id",
    "enemy_champion_net_entity_id",
    "enemy_minion_net_entity_id",
    "enemy_structure_net_entity_id",
    "allied_wave_net_entity_id",
    "self_level",
    "enemy_level",
    "self_hp_ratio",
    "enemy_hp_ratio",
    "self_gold",
    "enemy_gold",
    "enemy_distance",
    "attack_range",
    "turret_danger",
}

RequiredActionMaskFields = {
    "legal_candidate_mask",
    "illegal_candidate_mask",
    "available_action_mask",
    "available_skill_mask",
}

RequiredCandidateFields = {
    "kind",
    "flags",
    "target_net_entity_id",
    "score",
    "contributions",
}

RequiredContributionFields = {
    "feature_id",
    "raw_value",
    "weight",
    "contribution",
}

RequiredCommandFields = {
    "kind",
    "slot",
    "target_net_entity_id",
    "sequence",
    "position",
}

RequiredExecutorFields = {"state", "reason"}
Hex64Pattern = re.compile(r"^[0-9a-f]{64}$")
Hex16Pattern = re.compile(r"^[0-9a-f]{16}$")


def _ValidateExactFields(
    value: Any,
    required: set[str],
    path: str,
) -> list[str]:
    if not isinstance(value, dict):
        return [f"{path} must be an object"]

    errors: list[str] = []
    missing = sorted(required - value.keys())
    unknown = sorted(value.keys() - required)
    errors.extend(f"{path} missing field: {field}" for field in missing)
    errors.extend(f"{path} unknown field: {field}" for field in unknown)
    return errors


def _IsNonNegativeInt(value: Any) -> bool:
    return not isinstance(value, bool) and isinstance(value, int) and value >= 0


def _IsFiniteNumber(value: Any) -> bool:
    return (
        not isinstance(value, bool)
        and isinstance(value, (int, float))
        and math.isfinite(float(value))
    )


def _ValidateIntegerFields(
    value: dict[str, Any],
    fields: tuple[str, ...],
    path: str,
) -> list[str]:
    return [
        f"{path}.{field} must be a non-negative integer"
        for field in fields
        if field in value and not _IsNonNegativeInt(value[field])
    ]


def _ValidateNumberFields(
    value: dict[str, Any],
    fields: tuple[str, ...],
    path: str,
) -> list[str]:
    return [
        f"{path}.{field} must be a finite number"
        for field in fields
        if field in value and not _IsFiniteNumber(value[field])
    ]


def ValidateRecord(record: Any, promotion: bool = False) -> list[str]:
    errors = _ValidateExactFields(record, RequiredRecordFields, "record")
    if not isinstance(record, dict):
        return errors

    errors.extend(
        _ValidateIntegerFields(
            record,
            (
                "schema_version",
                "trace_schema_version",
                "tick",
                "timeline_epoch",
                "branch_id",
                "seed",
                "policy_revision",
                "observation_schema_version",
                "action_schema_version",
                "selected_candidate_kind",
            ),
            "record",
        )
    )
    errors.extend(_ValidateNumberFields(record, ("reward",), "record"))

    if record.get("schema_version") != SchemaVersion:
        errors.append(f"record.schema_version must be {SchemaVersion}")
    if record.get("trace_schema_version") != TraceSchemaVersion:
        errors.append(
            f"record.trace_schema_version must match {TraceSchemaVersion}"
        )
    if record.get("observation_schema_version") != ObservationSchemaVersion:
        errors.append(
            "record.observation_schema_version must match "
            f"{ObservationSchemaVersion}"
        )
    if record.get("action_schema_version") != ActionSchemaVersion:
        errors.append(
            f"record.action_schema_version must match {ActionSchemaVersion}"
        )
    for field in ("episode_id", "scenario_id"):
        if not isinstance(record.get(field), str) or not record.get(field):
            errors.append(f"record.{field} must be a non-empty string")
    for field in ("rules_hash", "definition_hash"):
        value = record.get(field)
        if not isinstance(value, str) or Hex64Pattern.fullmatch(value) is None:
            errors.append(f"record.{field} must be 64 lowercase hex characters")
        elif promotion and value == "0" * 64:
            errors.append(f"promotion rejects placeholder record.{field}")
    next_state_hash = record.get("next_state_hash")
    if (
        not isinstance(next_state_hash, str)
        or Hex16Pattern.fullmatch(next_state_hash) is None
    ):
        errors.append("record.next_state_hash must be 16 lowercase hex characters")
    elif promotion and next_state_hash == "0" * 16:
        errors.append("promotion rejects placeholder record.next_state_hash")
    if promotion and record.get("policy_revision") == 0:
        errors.append("promotion requires a non-zero policy_revision")
    if not isinstance(record.get("terminal"), bool):
        errors.append("record.terminal must be a boolean")
    if not isinstance(record.get("truncated"), bool):
        errors.append("record.truncated must be a boolean")
    if record.get("terminal") is True and record.get("truncated") is True:
        errors.append("record.terminal and record.truncated cannot both be true")

    observation = record.get("observation")
    observed_net_entity_ids: set[int] = set()
    errors.extend(
        _ValidateExactFields(
            observation,
            RequiredObservationFields,
            "record.observation",
        )
    )
    if isinstance(observation, dict):
        errors.extend(
            _ValidateIntegerFields(
                observation,
                (
                    "capability_flags",
                    "provenance_flags",
                    "fact_tick",
                    "self_net_entity_id",
                    "enemy_champion_net_entity_id",
                    "enemy_minion_net_entity_id",
                    "enemy_structure_net_entity_id",
                    "allied_wave_net_entity_id",
                    "self_level",
                    "enemy_level",
                ),
                "record.observation",
            )
        )
        errors.extend(
            _ValidateNumberFields(
                observation,
                (
                    "self_hp_ratio",
                    "enemy_hp_ratio",
                    "self_gold",
                    "enemy_gold",
                    "enemy_distance",
                    "attack_range",
                    "turret_danger",
                ),
                "record.observation",
            )
        )
        capability_flags = observation.get("capability_flags")
        if (
            _IsNonNegativeInt(capability_flags)
            and capability_flags & ~AllObservationCapabilityFlags
        ):
            errors.append("record.observation.capability_flags has unknown V1 bits")
        provenance_flags = observation.get("provenance_flags")
        if (
            _IsNonNegativeInt(provenance_flags)
            and provenance_flags & ~AllObservationProvenanceFlags
        ):
            errors.append("record.observation.provenance_flags has unknown V1 bits")

        self_level = observation.get("self_level")
        enemy_level = observation.get("enemy_level")
        if _IsNonNegativeInt(self_level) and not 1 <= self_level <= 255:
            errors.append("record.observation.self_level must be between 1 and 255")
        if _IsNonNegativeInt(enemy_level) and enemy_level > 255:
            errors.append("record.observation.enemy_level must be at most 255")

        for field in ("self_hp_ratio", "enemy_hp_ratio", "turret_danger"):
            value = observation.get(field)
            if _IsFiniteNumber(value) and not 0.0 <= float(value) <= 1.0:
                errors.append(
                    f"record.observation.{field} must be between 0 and 1"
                )
        for field in ("self_gold", "enemy_gold", "enemy_distance", "attack_range"):
            value = observation.get(field)
            if _IsFiniteNumber(value) and float(value) < 0.0:
                errors.append(f"record.observation.{field} must be non-negative")

        if observation.get("self_net_entity_id") == 0:
            errors.append("record.observation.self_net_entity_id must be non-zero")
        for field in (
            "self_net_entity_id",
            "enemy_champion_net_entity_id",
            "enemy_minion_net_entity_id",
            "enemy_structure_net_entity_id",
            "allied_wave_net_entity_id",
        ):
            net_entity_id = observation.get(field)
            if _IsNonNegativeInt(net_entity_id) and net_entity_id != 0:
                observed_net_entity_ids.add(net_entity_id)
        if observation.get("fact_tick") != record.get("tick"):
            errors.append("record.observation.fact_tick must equal record.tick")

        enemy_observed = observation.get("enemy_champion_net_entity_id") != 0
        if enemy_observed and enemy_level == 0:
            errors.append(
                "record.observation.enemy_level must be non-zero when enemy is observed"
            )

        provenance = observation.get("provenance_flags")
        if _IsNonNegativeInt(provenance):
            source_flags = provenance & (
                ObservationPrivilegedSourceFlag
                | ObservationTeamFilteredFlag
            )
            if source_flags == 0:
                errors.append("observation provenance source is missing")
            if promotion:
                if provenance & ObservationPrivilegedSourceFlag:
                    errors.append("promotion rejects privileged observation source")
                if not provenance & ObservationTeamFilteredFlag:
                    errors.append("promotion requires team-filtered observation")

        if promotion and observation.get("enemy_champion_net_entity_id") == 0:
            hidden_fields = {
                "enemy_level": 0,
                "enemy_hp_ratio": 0,
                "enemy_gold": 0,
                "enemy_distance": 0,
            }
            for field, expected in hidden_fields.items():
                if observation.get(field) != expected:
                    errors.append(
                        "promotion rejects hidden enemy current fact: "
                        f"record.observation.{field}"
                    )

    action_mask = record.get("action_mask")
    errors.extend(
        _ValidateExactFields(
            action_mask,
            RequiredActionMaskFields,
            "record.action_mask",
        )
    )
    if isinstance(action_mask, dict):
        errors.extend(
            _ValidateIntegerFields(
                action_mask,
                tuple(sorted(RequiredActionMaskFields)),
                "record.action_mask",
            )
        )
        legal_mask = action_mask.get("legal_candidate_mask")
        illegal_mask = action_mask.get("illegal_candidate_mask")
        if _IsNonNegativeInt(legal_mask) and _IsNonNegativeInt(illegal_mask):
            if legal_mask & illegal_mask:
                errors.append("candidate legal and illegal masks overlap")
            if (legal_mask | illegal_mask) != AllCandidateBits:
                errors.append("candidate masks must cover every V1 candidate")
        available_action_mask = action_mask.get("available_action_mask")
        if (
            _IsNonNegativeInt(available_action_mask)
            and available_action_mask & ~AllAvailableActionFlags
        ):
            errors.append("available_action_mask has unknown V1 bits")
        available_skill_mask = action_mask.get("available_skill_mask")
        if (
            _IsNonNegativeInt(available_skill_mask)
            and available_skill_mask & ~AllAvailableSkillFlags
        ):
            errors.append("available_skill_mask has unknown V1 bits")

    candidates = record.get("candidates")
    selected_kind = record.get("selected_candidate_kind")
    selected_count = 0
    candidate_kinds: set[int] = set()
    if not isinstance(candidates, list):
        errors.append("record.candidates must be an array")
    elif len(candidates) != 4:
        errors.append("record.candidates must contain all four V1 candidates")
    else:
        for index, candidate in enumerate(candidates):
            path = f"record.candidates[{index}]"
            errors.extend(
                _ValidateExactFields(candidate, RequiredCandidateFields, path)
            )
            if not isinstance(candidate, dict):
                continue
            errors.extend(
                _ValidateIntegerFields(
                    candidate,
                    ("kind", "flags", "target_net_entity_id"),
                    path,
                )
            )
            errors.extend(_ValidateNumberFields(candidate, ("score",), path))
            kind = candidate.get("kind")
            flags = candidate.get("flags")
            if not _IsNonNegativeInt(kind) or kind not in CandidateKinds:
                errors.append(f"{path}.kind is not a V1 candidate")
            elif kind in candidate_kinds:
                errors.append(f"{path}.kind is duplicated")
            else:
                candidate_kinds.add(kind)
            if _IsNonNegativeInt(flags):
                if flags & ~AllCandidateFlags:
                    errors.append(f"{path}.flags has unknown V1 bits")
                if flags & CandidateSelectedFlag:
                    selected_count += 1
                    if kind != selected_kind:
                        errors.append(f"{path} selected flag disagrees with record")
                target_net_entity_id = candidate.get("target_net_entity_id")
                if _IsNonNegativeInt(target_net_entity_id):
                    has_target = target_net_entity_id != 0
                    if bool(flags & CandidateHasTargetFlag) != has_target:
                        errors.append(
                            f"{path} has-target flag disagrees with target"
                        )
                if (
                    promotion
                    and _IsNonNegativeInt(target_net_entity_id)
                    and target_net_entity_id != 0
                    and target_net_entity_id not in observed_net_entity_ids
                ):
                    errors.append(f"{path} target is not present in observation")
                legal_candidate_mask = (
                    action_mask.get("legal_candidate_mask")
                    if isinstance(action_mask, dict)
                    else None
                )
                if (
                    _IsNonNegativeInt(kind)
                    and kind in CandidateKindToBit
                    and _IsNonNegativeInt(legal_candidate_mask)
                ):
                    bit = CandidateKindToBit[kind]
                    legal = bool(legal_candidate_mask & bit)
                    if bool(flags & CandidateLegalFlag) != legal:
                        errors.append(f"{path} legal flag disagrees with mask")

            contributions = candidate.get("contributions")
            if not isinstance(contributions, list):
                errors.append(f"{path}.contributions must be an array")
            elif len(contributions) > 4:
                errors.append(f"{path}.contributions exceeds V1 capacity 4")
            else:
                for contribution_index, contribution in enumerate(contributions):
                    contribution_path = (
                        f"{path}.contributions[{contribution_index}]"
                    )
                    errors.extend(
                        _ValidateExactFields(
                            contribution,
                            RequiredContributionFields,
                            contribution_path,
                        )
                    )
                    if isinstance(contribution, dict):
                        errors.extend(
                            _ValidateIntegerFields(
                                contribution,
                                ("feature_id",),
                                contribution_path,
                            )
                        )
                        feature_id = contribution.get("feature_id")
                        if (
                            _IsNonNegativeInt(feature_id)
                            and feature_id not in KnownFeatureIds
                        ):
                            errors.append(
                                f"{contribution_path}.feature_id is not a V1 feature"
                            )
                        errors.extend(
                            _ValidateNumberFields(
                                contribution,
                                ("raw_value", "weight", "contribution"),
                                contribution_path,
                            )
                        )

    if candidate_kinds != CandidateKinds:
        errors.append("record.candidates must cover every V1 candidate kind")
    if (
        not _IsNonNegativeInt(selected_kind)
        or selected_kind not in CandidateKinds
    ):
        errors.append("record.selected_candidate_kind is not a V1 candidate")
    if selected_count != 1:
        errors.append("exactly one candidate must be selected")
    legal_candidate_mask = (
        action_mask.get("legal_candidate_mask")
        if isinstance(action_mask, dict)
        else None
    )
    if (
        _IsNonNegativeInt(selected_kind)
        and selected_kind in CandidateKindToBit
        and _IsNonNegativeInt(legal_candidate_mask)
    ):
        selected_bit = CandidateKindToBit[selected_kind]
        if not legal_candidate_mask & selected_bit:
            errors.append("selected candidate is not legal")

    command = record.get("command")
    errors.extend(
        _ValidateExactFields(command, RequiredCommandFields, "record.command")
    )
    if isinstance(command, dict):
        errors.extend(
            _ValidateIntegerFields(
                command,
                ("kind", "slot", "target_net_entity_id", "sequence"),
                "record.command",
            )
        )
        position = command.get("position")
        if not isinstance(position, list) or len(position) != 3:
            errors.append("record.command.position must contain three numbers")
        elif not all(_IsFiniteNumber(value) for value in position):
            errors.append("record.command.position values must be finite")
        command_kind = command.get("kind")
        if _IsNonNegativeInt(command_kind) and command_kind not in KnownCommandKinds:
            errors.append("record.command.kind is not a finalized V1 command kind")
        command_slot = command.get("slot")
        if _IsNonNegativeInt(command_slot) and command_slot not in KnownCommandSlots:
            errors.append("record.command.slot is not a V1 slot")
        if promotion and command.get("sequence") == 0:
            errors.append("promotion requires a non-zero command sequence")
        command_target = command.get("target_net_entity_id")
        if (
            promotion
            and _IsNonNegativeInt(command_target)
            and command_target != 0
            and command_target not in observed_net_entity_ids
        ):
            errors.append("record.command target is not present in observation")

    executor = record.get("executor")
    errors.extend(
        _ValidateExactFields(executor, RequiredExecutorFields, "record.executor")
    )
    if isinstance(executor, dict):
        errors.extend(
            _ValidateIntegerFields(
                executor,
                ("state", "reason"),
                "record.executor",
            )
        )
        state = executor.get("state")
        reason = executor.get("reason")
        if state not in {
            ExecutorUnknown,
            ExecutorSubmitted,
            ExecutorAccepted,
            ExecutorRejected,
        }:
            errors.append("record.executor.state is invalid")
        if _IsNonNegativeInt(reason) and reason not in KnownExecutorReasons:
            errors.append("record.executor.reason is not a V1 reason")
        if state == ExecutorAccepted and reason != 0:
            errors.append("accepted executor result must use reason None")
        if state == ExecutorRejected and reason == 0:
            errors.append("rejected executor result must include a reject reason")
        if promotion and state not in {ExecutorAccepted, ExecutorRejected}:
            errors.append("promotion requires a final executor result")

    return errors


def BuildRecordIdentity(record: Any) -> tuple[Any, ...] | None:
    if not isinstance(record, dict):
        return None
    observation = record.get("observation")
    command = record.get("command")
    if not isinstance(observation, dict) or not isinstance(command, dict):
        return None

    values = (
        record.get("episode_id"),
        record.get("timeline_epoch"),
        record.get("branch_id"),
        record.get("tick"),
        observation.get("self_net_entity_id"),
        command.get("sequence"),
    )
    if not isinstance(values[0], str) or not values[0]:
        return None
    if not all(_IsNonNegativeInt(value) for value in values[1:]):
        return None
    return values
