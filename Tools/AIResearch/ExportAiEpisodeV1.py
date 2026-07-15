from __future__ import annotations

import argparse
import json
import math
import os
import re
import tempfile
from pathlib import Path
from typing import Any

from AiDecisionTraceCodec import (
    DecodeTraceFile,
    TraceCodecError,
    TraceToEpisodeRecord,
)
from AiEpisodeSchema import BuildRecordIdentity


MetadataSchemaVersion = 1
Hex64Pattern = re.compile(r"^[0-9a-f]{64}$")
Hex16Pattern = re.compile(r"^[0-9a-f]{16}$")
RequiredMetadataFields = {
    "schema_version",
    "episode_id",
    "scenario_id",
    "timeline_epoch",
    "branch_id",
    "seed",
    "rules_hash",
    "definition_hash",
    "policy_revision",
    "transitions",
}
RequiredTransitionFields = {
    "trace_index",
    "next_state_hash",
    "reward",
    "terminal",
    "truncated",
}


class EpisodeExportError(ValueError):
    pass


def _RejectDuplicateKeys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise EpisodeExportError(f"duplicate metadata key: {key}")
        result[key] = value
    return result


def _RejectNonFiniteConstant(value: str) -> None:
    raise EpisodeExportError(f"non-finite metadata constant: {value}")


def _IsNonNegativeInt(value: Any) -> bool:
    return not isinstance(value, bool) and isinstance(value, int) and value >= 0


def _ValidateExactFields(
    value: dict[str, Any],
    required: set[str],
    path: str,
) -> list[str]:
    missing = sorted(required - value.keys())
    unknown = sorted(value.keys() - required)
    return [
        *(f"{path} missing field: {field}" for field in missing),
        *(f"{path} unknown field: {field}" for field in unknown),
    ]


def LoadMetadata(path: Path, trace_count: int) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    try:
        metadata = json.loads(
            path.read_text(encoding="utf-8"),
            object_pairs_hook=_RejectDuplicateKeys,
            parse_constant=_RejectNonFiniteConstant,
        )
    except (OSError, json.JSONDecodeError, EpisodeExportError) as error:
        raise EpisodeExportError(f"unable to read metadata: {error}") from error

    if not isinstance(metadata, dict):
        raise EpisodeExportError("metadata root must be an object")
    errors = _ValidateExactFields(metadata, RequiredMetadataFields, "metadata")
    if metadata.get("schema_version") != MetadataSchemaVersion:
        errors.append(f"metadata.schema_version must be {MetadataSchemaVersion}")
    for field in ("episode_id", "scenario_id"):
        if not isinstance(metadata.get(field), str) or not metadata[field]:
            errors.append(f"metadata.{field} must be a non-empty string")
    for field in ("timeline_epoch", "branch_id", "seed", "policy_revision"):
        if not _IsNonNegativeInt(metadata.get(field)):
            errors.append(f"metadata.{field} must be a non-negative integer")
    for field in ("rules_hash", "definition_hash"):
        value = metadata.get(field)
        if not isinstance(value, str) or Hex64Pattern.fullmatch(value) is None:
            errors.append(f"metadata.{field} must be 64 lowercase hex characters")

    transitions_value = metadata.get("transitions")
    transitions_by_index: dict[int, dict[str, Any]] = {}
    if not isinstance(transitions_value, list):
        errors.append("metadata.transitions must be an array")
    else:
        for list_index, transition in enumerate(transitions_value):
            path_prefix = f"metadata.transitions[{list_index}]"
            if not isinstance(transition, dict):
                errors.append(f"{path_prefix} must be an object")
                continue
            errors.extend(
                _ValidateExactFields(
                    transition,
                    RequiredTransitionFields,
                    path_prefix,
                )
            )
            trace_index = transition.get("trace_index")
            if not _IsNonNegativeInt(trace_index):
                errors.append(f"{path_prefix}.trace_index must be non-negative")
            elif trace_index >= trace_count:
                errors.append(f"{path_prefix}.trace_index is out of range")
            elif trace_index in transitions_by_index:
                errors.append(f"{path_prefix}.trace_index is duplicated")
            else:
                transitions_by_index[trace_index] = transition
            next_state_hash = transition.get("next_state_hash")
            if (
                not isinstance(next_state_hash, str)
                or Hex16Pattern.fullmatch(next_state_hash) is None
            ):
                errors.append(
                    f"{path_prefix}.next_state_hash must be 16 lowercase hex"
                )
            reward = transition.get("reward")
            if (
                isinstance(reward, bool)
                or not isinstance(reward, (int, float))
                or not math.isfinite(float(reward))
            ):
                errors.append(f"{path_prefix}.reward must be finite")
            if not isinstance(transition.get("terminal"), bool):
                errors.append(f"{path_prefix}.terminal must be a boolean")
            if not isinstance(transition.get("truncated"), bool):
                errors.append(f"{path_prefix}.truncated must be a boolean")
            if (
                transition.get("terminal") is True
                and transition.get("truncated") is True
            ):
                errors.append(
                    f"{path_prefix}.terminal and truncated cannot both be true"
                )

    missing_indices = sorted(set(range(trace_count)) - transitions_by_index.keys())
    if missing_indices:
        errors.append(f"metadata.transitions missing trace indices: {missing_indices}")
    if isinstance(transitions_value, list) and transitions_by_index:
        last_index = trace_count - 1
        for trace_index, transition in transitions_by_index.items():
            boundary = (
                transition.get("terminal") is True
                or transition.get("truncated") is True
            )
            if boundary and trace_index != last_index:
                errors.append(
                    "metadata transition boundary must be the final trace: "
                    f"trace_index={trace_index} final={last_index}"
                )
        last_transition = transitions_by_index.get(last_index)
        if isinstance(last_transition, dict):
            terminal = last_transition.get("terminal") is True
            truncated = last_transition.get("truncated") is True
            if terminal == truncated:
                errors.append(
                    "metadata final transition must be exactly one of "
                    "terminal or truncated"
                )
    if errors:
        raise EpisodeExportError("; ".join(errors))

    transitions = [transitions_by_index[index] for index in range(trace_count)]
    return metadata, transitions


def BuildEpisodeRecords(
    capture_path: Path,
    metadata_path: Path,
) -> list[dict[str, Any]]:
    traces = DecodeTraceFile(capture_path)
    metadata, transitions = LoadMetadata(metadata_path, len(traces))
    records = [
        TraceToEpisodeRecord(trace, metadata, transitions[index])
        for index, trace in enumerate(traces)
    ]

    seen_identities: dict[tuple[Any, ...], int] = {}
    for index, record in enumerate(records):
        identity = BuildRecordIdentity(record)
        if identity is None:
            raise EpisodeExportError(f"record[{index}] has no stable identity")
        first_index = seen_identities.get(identity)
        if first_index is not None:
            raise EpisodeExportError(
                f"record[{index}] duplicates record[{first_index}] identity "
                f"{identity}"
            )
        seen_identities[identity] = index
    return records


def WriteCanonicalJsonl(records: list[dict[str, Any]], output_path: Path) -> None:
    lines = [
        json.dumps(
            record,
            sort_keys=True,
            separators=(",", ":"),
            ensure_ascii=False,
            allow_nan=False,
        )
        for record in records
    ]
    payload = ("\n".join(lines) + "\n").encode("utf-8")
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


def Main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Export raw MSVC little-endian AiDecisionTraceV1 records to "
            "canonical AiEpisodeV1 JSONL."
        )
    )
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--metadata", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    try:
        output_path = args.output.resolve()
        if output_path in {args.input.resolve(), args.metadata.resolve()}:
            raise EpisodeExportError(
                "output must not overwrite the trace capture or metadata"
            )
        records = BuildEpisodeRecords(args.input, args.metadata)
        WriteCanonicalJsonl(records, output_path)
    except (OSError, EpisodeExportError, TraceCodecError) as error:
        print(f"AiEpisodeV1 export FAILED: {error}")
        return 1

    pending_count = sum(
        1
        for record in records
        if record["executor"]["state"] in {0, 1}
    )
    print(
        "AiEpisodeV1 export PASS: "
        f"records={len(records)} pending={pending_count} output={output_path}"
    )
    if pending_count:
        print(
            "AiEpisodeV1 promotion BLOCKED until executor state is "
            "Accepted or Rejected."
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(Main())
