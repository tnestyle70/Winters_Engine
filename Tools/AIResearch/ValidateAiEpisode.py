from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

from AiEpisodeSchema import BuildRecordIdentity, ValidateRecord


class DuplicateObjectKeyError(ValueError):
    pass


class NonFiniteJsonConstantError(ValueError):
    pass


def RejectDuplicateObjectKeys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise DuplicateObjectKeyError(f"duplicate object key: {key}")
        result[key] = value
    return result


def RejectNonFiniteJsonConstant(value: str) -> None:
    raise NonFiniteJsonConstantError(f"non-finite JSON constant: {value}")


def Main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate Winters AiEpisodeV1 JSONL records."
    )
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument(
        "--promotion",
        action="store_true",
        help="Reject privileged observations and unfinished executor results.",
    )
    args = parser.parse_args()

    errors: list[str] = []
    record_count = 0
    seen_identities: dict[tuple[Any, ...], int] = {}
    episode_rows: dict[
        tuple[Any, ...],
        list[tuple[int, int, int, int, bool, bool]],
    ] = {}
    with args.input.open("r", encoding="utf-8") as stream:
        for line_number, raw_line in enumerate(stream, start=1):
            if not raw_line.strip():
                continue
            record_count += 1
            try:
                record = json.loads(
                    raw_line,
                    object_pairs_hook=RejectDuplicateObjectKeys,
                    parse_constant=RejectNonFiniteJsonConstant,
                )
            except (
                json.JSONDecodeError,
                DuplicateObjectKeyError,
                NonFiniteJsonConstantError,
            ) as error:
                errors.append(f"line {line_number}: {error}")
                continue

            identity = BuildRecordIdentity(record)
            if identity is not None:
                first_line = seen_identities.get(identity)
                if first_line is not None:
                    errors.append(
                        f"line {line_number}: duplicate record identity "
                        f"from line {first_line}: {identity}"
                    )
                else:
                    seen_identities[identity] = line_number

                group_key = (
                    record.get("episode_id"),
                    record.get("scenario_id"),
                    record.get("timeline_epoch"),
                    record.get("branch_id"),
                )
                episode_rows.setdefault(group_key, []).append(
                    (
                        line_number,
                        int(record.get("tick", 0)),
                        int(record.get("observation", {}).get(
                            "self_net_entity_id",
                            0,
                        )),
                        int(record.get("command", {}).get("sequence", 0)),
                        record.get("terminal") is True,
                        record.get("truncated") is True,
                    )
                )

            errors.extend(
                f"line {line_number}: {error}"
                for error in ValidateRecord(record, promotion=args.promotion)
            )

    for group_key, rows in episode_rows.items():
        ordered = sorted(rows, key=lambda row: (row[1], row[2], row[3], row[0]))
        boundary_rows = [row for row in ordered if row[4] or row[5]]
        if len(boundary_rows) > 1:
            errors.append(
                f"episode {group_key} has multiple terminal/truncated records"
            )
        elif boundary_rows and boundary_rows[0] != ordered[-1]:
            errors.append(
                f"line {boundary_rows[0][0]}: terminal/truncated record is "
                f"not the final canonical record for episode {group_key}"
            )

    if record_count == 0:
        errors.append("input contains no JSONL records")

    if errors:
        print("AiEpisodeV1 validation FAILED")
        for error in errors:
            print(f"- {error}")
        return 1

    mode = "promotion" if args.promotion else "raw"
    print(
        f"AiEpisodeV1 validation PASS: records={record_count} mode={mode}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(Main())
