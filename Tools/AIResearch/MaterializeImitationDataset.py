from __future__ import annotations

import argparse
import hashlib
import os
import tempfile
from pathlib import Path
from typing import Any

import AiEpisodeSchema as episode_schema
import AiImitationDatasetSchema as imitation_schema
from TrainImitationRankingBaseline import LoadPromotionEpisodes, TrainingDataError


def _BuildSourceAcceptedLabel(record: dict[str, Any]) -> dict[str, Any]:
    return {
        "candidate_kind": int(record["selected_candidate_kind"]),
        "origin": imitation_schema.LabelOriginSourceAccepted,
        "execution_evidence": (
            imitation_schema.ExecutionEvidenceSourceCommandAccepted
        ),
        "annotation_id": "",
        "annotation_method": imitation_schema.SourceAnnotationMethod,
        "reason_code": imitation_schema.SourceReasonCode,
    }


def _BuildHumanCorrectionLabel(correction: dict[str, Any]) -> dict[str, Any]:
    return {
        "candidate_kind": int(correction["corrected_candidate_kind"]),
        "origin": imitation_schema.LabelOriginHumanDebugCorrection,
        "execution_evidence": (
            imitation_schema.ExecutionEvidenceUnexecutedCounterfactual
        ),
        "annotation_id": correction["annotation_id"],
        "annotation_method": correction["annotation_method"],
        "reason_code": correction["reason_code"],
    }


def BuildMaterializedRows(
    episode_path: Path,
    correction_path: Path,
    corrected_only: bool = False,
) -> list[dict[str, Any]]:
    episode_bytes = episode_path.read_bytes()
    episode_sha256 = hashlib.sha256(episode_bytes).hexdigest()
    loaded_episodes = LoadPromotionEpisodes(episode_path)
    loaded_sidecar = imitation_schema.LoadCorrectionSidecar(correction_path)
    sidecar = loaded_sidecar.normalized
    if sidecar["source_ai_episode_sha256"] != episode_sha256:
        raise imitation_schema.ImitationDatasetError(
            "correction sidecar source AiEpisode SHA-256 mismatch"
        )

    records_by_identity: dict[tuple[Any, ...], dict[str, Any]] = {}
    for record in loaded_episodes.records:
        identity = episode_schema.BuildRecordIdentity(record)
        if identity is None:
            raise imitation_schema.ImitationDatasetError(
                "source episode contains an invalid record identity"
            )
        records_by_identity[identity] = record

    corrections_by_identity: dict[tuple[Any, ...], dict[str, Any]] = {}
    for correction in sidecar["corrections"]:
        identity_object = correction["record_identity"]
        identity = (
            identity_object["episode_id"],
            identity_object["timeline_epoch"],
            identity_object["branch_id"],
            identity_object["tick"],
            identity_object["self_net_entity_id"],
            identity_object["command_sequence"],
        )
        record = records_by_identity.get(identity)
        if record is None:
            raise imitation_schema.ImitationDatasetError(
                f"correction references an unknown source record: {identity}"
            )
        actual_record_sha256 = imitation_schema.ComputeRecordSha256(record)
        if correction["source_record_sha256"] != actual_record_sha256:
            raise imitation_schema.ImitationDatasetError(
                f"correction source record SHA-256 mismatch: {identity}"
            )
        if correction["original_candidate_kind"] != record[
            "selected_candidate_kind"
        ]:
            raise imitation_schema.ImitationDatasetError(
                f"correction original candidate mismatch: {identity}"
            )
        corrected_kind = int(correction["corrected_candidate_kind"])
        legal_mask = int(record["action_mask"]["legal_candidate_mask"])
        if not legal_mask & episode_schema.CandidateKindToBit[corrected_kind]:
            raise imitation_schema.ImitationDatasetError(
                f"correction selects an illegal candidate: {identity}"
            )
        corrections_by_identity[identity] = correction

    rows: list[dict[str, Any]] = []
    consumed_corrections: set[tuple[Any, ...]] = set()
    for record in loaded_episodes.records:
        identity = episode_schema.BuildRecordIdentity(record)
        if identity is None:
            raise imitation_schema.ImitationDatasetError(
                "source episode contains an invalid record identity"
            )
        correction = corrections_by_identity.get(identity)
        if correction is None:
            if corrected_only:
                continue
            expert_label = _BuildSourceAcceptedLabel(record)
        else:
            expert_label = _BuildHumanCorrectionLabel(correction)
            consumed_corrections.add(identity)
        row = {
            "schema_version": imitation_schema.ImitationDecisionSchemaVersion,
            "record_type": imitation_schema.ImitationDecisionRecordType,
            "source": {
                "ai_episode_sha256": episode_sha256,
                "correction_sidecar_sha256": loaded_sidecar.canonical_sha256,
                "record_sha256": imitation_schema.ComputeRecordSha256(record),
            },
            "source_record": record,
            "expert_label": expert_label,
        }
        errors = imitation_schema.ValidateMaterializedRecord(row)
        if errors:
            raise imitation_schema.ImitationDatasetError("; ".join(errors))
        rows.append(row)

    unconsumed = sorted(set(corrections_by_identity) - consumed_corrections)
    if unconsumed:
        raise imitation_schema.ImitationDatasetError(
            f"corrections were not consumed exactly once: {unconsumed}"
        )
    if corrected_only and not rows:
        raise imitation_schema.ImitationDatasetError(
            "corrected-only materialization contains no human corrections"
        )
    return rows


def WriteCanonicalJsonl(rows: list[dict[str, Any]], output_path: Path) -> None:
    payload = b"".join(
        imitation_schema.CanonicalJsonBytes(row) + b"\n" for row in rows
    )
    output_path.parent.mkdir(parents=True, exist_ok=True)
    temporary_path: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="wb",
            prefix=f".{output_path.name}.",
            suffix=".tmp",
            dir=output_path.parent,
            delete=False,
        ) as stream:
            temporary_path = Path(stream.name)
            stream.write(payload)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary_path, output_path)
    finally:
        if temporary_path is not None and temporary_path.exists():
            temporary_path.unlink()


def Main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Materialize immutable AiEpisodeV1 source records and explicit "
            "human/debug expert labels into canonical ImitationDecisionV1 JSONL."
        )
    )
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--corrections", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument(
        "--corrected-only",
        action="store_true",
        help=(
            "Emit only HUMAN_DEBUG_CORRECTION rows. This is mandatory for "
            "active-policy/DAgger query episodes because their accepted source "
            "actions are not expert labels."
        ),
    )
    args = parser.parse_args(argv)

    try:
        output_path = args.output.resolve()
        if output_path in {args.input.resolve(), args.corrections.resolve()}:
            raise imitation_schema.ImitationDatasetError(
                "output must not overwrite the source episode or correction sidecar"
            )
        rows = BuildMaterializedRows(
            args.input,
            args.corrections,
            corrected_only=args.corrected_only,
        )
        WriteCanonicalJsonl(rows, output_path)
        output_sha256 = hashlib.sha256(output_path.read_bytes()).hexdigest()
    except (
        OSError,
        UnicodeDecodeError,
        TrainingDataError,
        imitation_schema.ImitationDatasetError,
    ) as error:
        print(f"ImitationDecisionV1 materialization FAILED: {error}")
        return 1

    correction_count = sum(
        row["expert_label"]["origin"]
        == imitation_schema.LabelOriginHumanDebugCorrection
        for row in rows
    )
    print(
        "ImitationDecisionV1 materialization PASS: "
        f"records={len(rows)} corrections={correction_count} "
        f"sha256={output_sha256} output={output_path}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(Main())
