from __future__ import annotations

import hashlib
import json
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import AiEpisodeSchema as episode_schema


CorrectionSidecarSchemaVersion = 1
CorrectionSidecarArtifactType = "AiDecisionCorrectionSidecarV1"
ImitationDecisionSchemaVersion = 1
ImitationDecisionRecordType = "ImitationDecisionV1"

LabelOriginSourceAccepted = "SOURCE_ACCEPTED"
LabelOriginHumanDebugCorrection = "HUMAN_DEBUG_CORRECTION"
ExecutionEvidenceSourceCommandAccepted = "SOURCE_COMMAND_ACCEPTED"
ExecutionEvidenceUnexecutedCounterfactual = "UNEXECUTED_COUNTERFACTUAL"

TeacherKindHuman = "HUMAN"
SourceAnnotationMethod = "SOURCE_EXECUTOR"
SourceReasonCode = "SOURCE_ACCEPTED"
AllowedAnnotationMethods = {
    "F9_TRACE_REVIEW",
    "CHRONO_MANUAL_COUNTERFACTUAL",
}
AllowedReasonCodes = {
    "SAFETY",
    "TACTICAL",
    "MACRO",
    "HUMAN_LIKENESS",
    "OTHER",
}

RequiredSidecarFields = {
    "schema_version",
    "artifact_type",
    "source_ai_episode_sha256",
    "corrections",
}
RequiredCorrectionFields = {
    "annotation_id",
    "record_identity",
    "source_record_sha256",
    "original_candidate_kind",
    "corrected_candidate_kind",
    "teacher_kind",
    "annotation_method",
    "reason_code",
}
RequiredIdentityFields = {
    "episode_id",
    "timeline_epoch",
    "branch_id",
    "tick",
    "self_net_entity_id",
    "command_sequence",
}
RequiredMaterializedFields = {
    "schema_version",
    "record_type",
    "source",
    "source_record",
    "expert_label",
}
RequiredMaterializedSourceFields = {
    "ai_episode_sha256",
    "correction_sidecar_sha256",
    "record_sha256",
}
RequiredExpertLabelFields = {
    "candidate_kind",
    "origin",
    "execution_evidence",
    "annotation_id",
    "annotation_method",
    "reason_code",
}

Hex64Pattern = re.compile(r"^[0-9a-f]{64}$")


class ImitationDatasetError(ValueError):
    pass


class DuplicateObjectKeyError(ValueError):
    pass


class NonFiniteJsonConstantError(ValueError):
    pass


@dataclass(frozen=True)
class LoadedCorrectionSidecar:
    normalized: dict[str, Any]
    canonical_sha256: str


@dataclass(frozen=True)
class LoadedImitationDataset:
    source_records: list[dict[str, Any]]
    expert_candidate_kinds: dict[tuple[Any, ...], int]
    label_origin_counts: dict[str, int]
    input_sha256: str
    source_ai_episode_sha256: str
    correction_sidecar_sha256: str


def RejectDuplicateObjectKeys(
    pairs: list[tuple[str, Any]],
) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise DuplicateObjectKeyError(f"duplicate object key: {key}")
        result[key] = value
    return result


def RejectNonFiniteJsonConstant(value: str) -> None:
    raise NonFiniteJsonConstantError(f"non-finite JSON constant: {value}")


def CanonicalJsonBytes(value: Any) -> bytes:
    return json.dumps(
        value,
        ensure_ascii=False,
        allow_nan=False,
        sort_keys=True,
        separators=(",", ":"),
    ).encode("utf-8")


def ComputeRecordSha256(record: dict[str, Any]) -> str:
    return hashlib.sha256(CanonicalJsonBytes(record)).hexdigest()


def _IsNonNegativeInt(value: Any) -> bool:
    return not isinstance(value, bool) and isinstance(value, int) and value >= 0


def _ValidateExactFields(
    value: Any,
    required: set[str],
    path: str,
) -> list[str]:
    if not isinstance(value, dict):
        return [f"{path} must be an object"]
    missing = sorted(required - value.keys())
    unknown = sorted(value.keys() - required)
    return [
        *(f"{path} missing field: {field}" for field in missing),
        *(f"{path} unknown field: {field}" for field in unknown),
    ]


def _ValidateHex64(value: Any, path: str) -> list[str]:
    if not isinstance(value, str) or Hex64Pattern.fullmatch(value) is None:
        return [f"{path} must be 64 lowercase hex characters"]
    return []


def _IdentityTuple(identity: Any) -> tuple[Any, ...] | None:
    if not isinstance(identity, dict):
        return None
    values = (
        identity.get("episode_id"),
        identity.get("timeline_epoch"),
        identity.get("branch_id"),
        identity.get("tick"),
        identity.get("self_net_entity_id"),
        identity.get("command_sequence"),
    )
    if not isinstance(values[0], str) or not values[0]:
        return None
    if not all(_IsNonNegativeInt(value) for value in values[1:]):
        return None
    if values[4] == 0 or values[5] == 0:
        return None
    return values


def BuildIdentityObject(record: dict[str, Any]) -> dict[str, Any]:
    identity = episode_schema.BuildRecordIdentity(record)
    if identity is None:
        raise ImitationDatasetError("source record identity is invalid")
    return {
        "episode_id": identity[0],
        "timeline_epoch": identity[1],
        "branch_id": identity[2],
        "tick": identity[3],
        "self_net_entity_id": identity[4],
        "command_sequence": identity[5],
    }


def ValidateCorrectionSidecar(sidecar: Any) -> list[str]:
    errors = _ValidateExactFields(sidecar, RequiredSidecarFields, "sidecar")
    if not isinstance(sidecar, dict):
        return errors

    if sidecar.get("schema_version") != CorrectionSidecarSchemaVersion:
        errors.append(
            "sidecar.schema_version must be "
            f"{CorrectionSidecarSchemaVersion}"
        )
    if sidecar.get("artifact_type") != CorrectionSidecarArtifactType:
        errors.append(
            f"sidecar.artifact_type must be {CorrectionSidecarArtifactType}"
        )
    errors.extend(
        _ValidateHex64(
            sidecar.get("source_ai_episode_sha256"),
            "sidecar.source_ai_episode_sha256",
        )
    )

    corrections = sidecar.get("corrections")
    if not isinstance(corrections, list):
        errors.append("sidecar.corrections must be an array")
        return errors

    seen_annotation_ids: set[str] = set()
    seen_identities: set[tuple[Any, ...]] = set()
    for index, correction in enumerate(corrections):
        path = f"sidecar.corrections[{index}]"
        errors.extend(
            _ValidateExactFields(correction, RequiredCorrectionFields, path)
        )
        if not isinstance(correction, dict):
            continue

        annotation_id = correction.get("annotation_id")
        if not isinstance(annotation_id, str) or not annotation_id.strip():
            errors.append(f"{path}.annotation_id must be a non-empty string")
        elif annotation_id in seen_annotation_ids:
            errors.append(f"{path}.annotation_id is duplicated")
        else:
            seen_annotation_ids.add(annotation_id)

        identity = correction.get("record_identity")
        errors.extend(
            _ValidateExactFields(identity, RequiredIdentityFields, f"{path}.record_identity")
        )
        identity_tuple = _IdentityTuple(identity)
        if identity_tuple is None:
            errors.append(f"{path}.record_identity is invalid")
        elif identity_tuple in seen_identities:
            errors.append(f"{path}.record_identity is duplicated")
        else:
            seen_identities.add(identity_tuple)

        errors.extend(
            _ValidateHex64(
                correction.get("source_record_sha256"),
                f"{path}.source_record_sha256",
            )
        )
        original = correction.get("original_candidate_kind")
        corrected = correction.get("corrected_candidate_kind")
        if original not in episode_schema.CandidateKinds:
            errors.append(f"{path}.original_candidate_kind is not a V1 candidate")
        if corrected not in episode_schema.CandidateKinds:
            errors.append(f"{path}.corrected_candidate_kind is not a V1 candidate")
        if original in episode_schema.CandidateKinds and original == corrected:
            errors.append(f"{path} correction must change the candidate kind")
        if correction.get("teacher_kind") != TeacherKindHuman:
            errors.append(f"{path}.teacher_kind must be {TeacherKindHuman}")
        if correction.get("annotation_method") not in AllowedAnnotationMethods:
            errors.append(f"{path}.annotation_method is invalid")
        if correction.get("reason_code") not in AllowedReasonCodes:
            errors.append(f"{path}.reason_code is invalid")
    return errors


def _CorrectionSortKey(correction: dict[str, Any]) -> tuple[Any, ...]:
    identity = _IdentityTuple(correction["record_identity"])
    if identity is None:
        raise ImitationDatasetError("correction identity is invalid")
    return (*identity, correction["annotation_id"])


def LoadCorrectionSidecar(path: Path) -> LoadedCorrectionSidecar:
    try:
        sidecar = json.loads(
            path.read_text(encoding="utf-8"),
            object_pairs_hook=RejectDuplicateObjectKeys,
            parse_constant=RejectNonFiniteJsonConstant,
        )
    except (
        OSError,
        UnicodeDecodeError,
        json.JSONDecodeError,
        DuplicateObjectKeyError,
        NonFiniteJsonConstantError,
    ) as error:
        raise ImitationDatasetError(
            f"unable to read correction sidecar: {error}"
        ) from error

    errors = ValidateCorrectionSidecar(sidecar)
    if errors:
        raise ImitationDatasetError("; ".join(errors))
    normalized = {
        **sidecar,
        "corrections": sorted(sidecar["corrections"], key=_CorrectionSortKey),
    }
    canonical_sha256 = hashlib.sha256(CanonicalJsonBytes(normalized)).hexdigest()
    return LoadedCorrectionSidecar(
        normalized=normalized,
        canonical_sha256=canonical_sha256,
    )


def ValidateMaterializedRecord(row: Any) -> list[str]:
    errors = _ValidateExactFields(row, RequiredMaterializedFields, "row")
    if not isinstance(row, dict):
        return errors
    if row.get("schema_version") != ImitationDecisionSchemaVersion:
        errors.append(
            f"row.schema_version must be {ImitationDecisionSchemaVersion}"
        )
    if row.get("record_type") != ImitationDecisionRecordType:
        errors.append(f"row.record_type must be {ImitationDecisionRecordType}")

    source = row.get("source")
    errors.extend(
        _ValidateExactFields(source, RequiredMaterializedSourceFields, "row.source")
    )
    if isinstance(source, dict):
        for field in RequiredMaterializedSourceFields:
            errors.extend(_ValidateHex64(source.get(field), f"row.source.{field}"))

    source_record = row.get("source_record")
    try:
        source_errors = episode_schema.ValidateRecord(source_record, promotion=True)
    except (OverflowError, TypeError, ValueError) as error:
        errors.append(f"row.source_record validation failed: {error}")
        source_errors = []
    errors.extend(f"row.source_record: {error}" for error in source_errors)
    if isinstance(source_record, dict):
        executor = source_record.get("executor")
        if (
            not isinstance(executor, dict)
            or executor.get("state") != episode_schema.ExecutorAccepted
        ):
            errors.append("row.source_record must have an Accepted executor result")
        if isinstance(source, dict) and source.get("record_sha256") != ComputeRecordSha256(
            source_record
        ):
            errors.append("row.source.record_sha256 does not match source_record")

    label = row.get("expert_label")
    errors.extend(
        _ValidateExactFields(label, RequiredExpertLabelFields, "row.expert_label")
    )
    if not isinstance(label, dict) or not isinstance(source_record, dict):
        return errors

    candidate_kind = label.get("candidate_kind")
    if candidate_kind not in episode_schema.CandidateKinds:
        errors.append("row.expert_label.candidate_kind is not a V1 candidate")
    else:
        action_mask = source_record.get("action_mask")
        legal_mask = (
            action_mask.get("legal_candidate_mask")
            if isinstance(action_mask, dict)
            else None
        )
        if (
            not _IsNonNegativeInt(legal_mask)
            or not legal_mask & episode_schema.CandidateKindToBit[candidate_kind]
        ):
            errors.append("row.expert_label.candidate_kind is illegal")

    origin = label.get("origin")
    if origin == LabelOriginSourceAccepted:
        expected = {
            "candidate_kind": source_record.get("selected_candidate_kind"),
            "origin": LabelOriginSourceAccepted,
            "execution_evidence": ExecutionEvidenceSourceCommandAccepted,
            "annotation_id": "",
            "annotation_method": SourceAnnotationMethod,
            "reason_code": SourceReasonCode,
        }
        if label != expected:
            errors.append("SOURCE_ACCEPTED expert_label is inconsistent")
    elif origin == LabelOriginHumanDebugCorrection:
        if label.get("execution_evidence") != ExecutionEvidenceUnexecutedCounterfactual:
            errors.append(
                "human correction must use UNEXECUTED_COUNTERFACTUAL evidence"
            )
        if (
            not isinstance(label.get("annotation_id"), str)
            or not label["annotation_id"].strip()
        ):
            errors.append("human correction annotation_id must be non-empty")
        if label.get("annotation_method") not in AllowedAnnotationMethods:
            errors.append("human correction annotation_method is invalid")
        if label.get("reason_code") not in AllowedReasonCodes:
            errors.append("human correction reason_code is invalid")
        if candidate_kind == source_record.get("selected_candidate_kind"):
            errors.append("human correction must change the source candidate kind")
    else:
        errors.append("row.expert_label.origin is invalid")
    return errors


def _ParseMaterializedLine(raw_line: str, line_number: int) -> dict[str, Any]:
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
        raise ImitationDatasetError(f"line {line_number}: {error}") from error
    if not isinstance(value, dict):
        raise ImitationDatasetError(f"line {line_number}: row must be an object")
    return value


def LoadImitationDataset(path: Path) -> LoadedImitationDataset:
    try:
        raw_bytes = path.read_bytes()
        raw_lines = raw_bytes.decode("utf-8").splitlines()
    except (OSError, UnicodeDecodeError) as error:
        raise ImitationDatasetError(
            f"unable to read imitation dataset: {error}"
        ) from error

    rows: list[dict[str, Any]] = []
    seen_identities: set[tuple[Any, ...]] = set()
    seen_decision_groups: set[tuple[Any, ...]] = set()
    seen_annotation_ids: set[str] = set()
    source_hashes: set[str] = set()
    sidecar_hashes: set[str] = set()
    episode_rows: dict[
        tuple[Any, ...],
        list[tuple[int, int, int, int, bool, bool]],
    ] = {}

    for line_number, raw_line in enumerate(raw_lines, start=1):
        if not raw_line.strip():
            continue
        row = _ParseMaterializedLine(raw_line, line_number)
        errors = ValidateMaterializedRecord(row)
        if errors:
            raise ImitationDatasetError(
                f"line {line_number}: " + "; ".join(errors)
            )
        source_record = row["source_record"]
        identity = episode_schema.BuildRecordIdentity(source_record)
        if identity is None:
            raise ImitationDatasetError(
                f"line {line_number}: source record identity is invalid"
            )
        if identity in seen_identities:
            raise ImitationDatasetError(
                f"line {line_number}: duplicate source record identity"
            )
        seen_identities.add(identity)

        decision_group = (
            source_record["episode_id"],
            source_record["scenario_id"],
            source_record["timeline_epoch"],
            source_record["branch_id"],
            source_record["tick"],
            source_record["observation"]["self_net_entity_id"],
        )
        if decision_group in seen_decision_groups:
            raise ImitationDatasetError(
                f"line {line_number}: duplicate decision group"
            )
        seen_decision_groups.add(decision_group)

        label = row["expert_label"]
        if label["origin"] == LabelOriginHumanDebugCorrection:
            annotation_id = label["annotation_id"]
            if annotation_id in seen_annotation_ids:
                raise ImitationDatasetError(
                    f"line {line_number}: duplicate correction annotation_id"
                )
            seen_annotation_ids.add(annotation_id)

        source_hashes.add(row["source"]["ai_episode_sha256"])
        sidecar_hashes.add(row["source"]["correction_sidecar_sha256"])
        episode_group = (
            source_record["episode_id"],
            source_record["scenario_id"],
            source_record["timeline_epoch"],
            source_record["branch_id"],
        )
        episode_rows.setdefault(episode_group, []).append(
            (
                line_number,
                int(source_record["tick"]),
                int(source_record["observation"]["self_net_entity_id"]),
                int(source_record["command"]["sequence"]),
                source_record["terminal"] is True,
                source_record["truncated"] is True,
            )
        )
        rows.append(row)

    if not rows:
        raise ImitationDatasetError("input contains no ImitationDecisionV1 rows")
    if len(source_hashes) != 1:
        raise ImitationDatasetError("dataset mixes source AiEpisode SHA-256 values")
    if len(sidecar_hashes) != 1:
        raise ImitationDatasetError("dataset mixes correction sidecar SHA-256 values")

    for episode_group, values in episode_rows.items():
        ordered = sorted(values, key=lambda value: (value[1], value[2], value[3], value[0]))
        boundaries = [value for value in ordered if value[4] or value[5]]
        if len(boundaries) > 1:
            raise ImitationDatasetError(
                f"episode {episode_group} has multiple terminal/truncated records"
            )
        if boundaries and boundaries[0] != ordered[-1]:
            raise ImitationDatasetError(
                f"line {boundaries[0][0]}: terminal/truncated record is not "
                f"the final canonical record for episode {episode_group}"
            )

    rows.sort(
        key=lambda row: (
            row["source_record"]["episode_id"],
            row["source_record"]["scenario_id"],
            row["source_record"]["timeline_epoch"],
            row["source_record"]["branch_id"],
            row["source_record"]["tick"],
            row["source_record"]["observation"]["self_net_entity_id"],
            row["source_record"]["command"]["sequence"],
        )
    )
    expert_candidate_kinds = {
        episode_schema.BuildRecordIdentity(row["source_record"]): int(
            row["expert_label"]["candidate_kind"]
        )
        for row in rows
    }
    if None in expert_candidate_kinds:
        raise ImitationDatasetError("dataset contains an invalid source identity")
    label_origin_counts = {
        LabelOriginSourceAccepted: sum(
            row["expert_label"]["origin"] == LabelOriginSourceAccepted
            for row in rows
        ),
        LabelOriginHumanDebugCorrection: sum(
            row["expert_label"]["origin"] == LabelOriginHumanDebugCorrection
            for row in rows
        ),
    }
    return LoadedImitationDataset(
        source_records=[row["source_record"] for row in rows],
        expert_candidate_kinds=expert_candidate_kinds,
        label_origin_counts=label_origin_counts,
        input_sha256=hashlib.sha256(raw_bytes).hexdigest(),
        source_ai_episode_sha256=next(iter(source_hashes)),
        correction_sidecar_sha256=next(iter(sidecar_hashes)),
    )
