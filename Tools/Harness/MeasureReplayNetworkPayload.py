#!/usr/bin/env python3
"""Measure replay payload sizes relevant to the UDP migration budget.

The parser follows Shared/Replay/ReplayFormat.h and intentionally reads only the
fixed replay/record headers. It never deserializes FlatBuffers payloads, so the
measurement remains useful across gameplay schema revisions.
"""

from __future__ import annotations

import argparse
import json
import math
import struct
from dataclasses import asdict, dataclass
from pathlib import Path
from statistics import fmean
from typing import Iterable


REPLAY_HEADER = struct.Struct("<4sHHIIIIQQQ")
RECORD_HEADER = struct.Struct("<BBHIQII")
REPLAY_MAGIC = b"WRPL"
SNAPSHOT_RECORD_TYPE = 1
EVENT_RECORD_TYPE = 2


@dataclass(frozen=True)
class PayloadStats:
    count: int
    minimum_bytes: int
    p50_bytes: int
    p95_bytes: int
    maximum_bytes: int
    average_bytes: float


@dataclass(frozen=True)
class ReplayMeasurement:
    path: str
    replay_version: int
    declared_record_count: int
    parsed_record_count: int
    snapshots: PayloadStats
    events: PayloadStats
    snapshot_payload_kib_per_second: float
    snapshot_wire_kib_per_second: float
    ack_only_uplink_kib_per_second: float
    snapshot_datagrams_average: float
    snapshot_datagrams_p95: int
    snapshot_datagrams_maximum: int


def _nearest_rank(values: list[int], percentile: float) -> int:
    if not values:
        return 0
    ordered = sorted(values)
    rank = max(1, math.ceil(percentile * len(ordered)))
    return ordered[rank - 1]


def _stats(values: list[int]) -> PayloadStats:
    if not values:
        return PayloadStats(0, 0, 0, 0, 0, 0.0)
    return PayloadStats(
        count=len(values),
        minimum_bytes=min(values),
        p50_bytes=_nearest_rank(values, 0.50),
        p95_bytes=_nearest_rank(values, 0.95),
        maximum_bytes=max(values),
        average_bytes=fmean(values),
    )


def _required_datagrams(payload_size: int, fragment_payload_budget: int) -> int:
    if payload_size <= 0:
        return 0
    return math.ceil(payload_size / fragment_payload_budget)


def _wire_bytes(
    payload_size: int,
    datagram_budget: int,
    transport_header_bytes: int,
    fragment_header_bytes: int,
    auth_tag_bytes: int,
) -> tuple[int, int]:
    unfragmented_budget = (
        datagram_budget - transport_header_bytes - auth_tag_bytes
    )
    if payload_size <= unfragmented_budget:
        return 1, payload_size + transport_header_bytes + auth_tag_bytes

    fragment_budget = (
        datagram_budget
        - transport_header_bytes
        - fragment_header_bytes
        - auth_tag_bytes
    )
    datagrams = _required_datagrams(payload_size, fragment_budget)
    wire_bytes = payload_size + datagrams * (
        transport_header_bytes + fragment_header_bytes + auth_tag_bytes
    )
    return datagrams, wire_bytes


def measure_replay(
    path: Path,
    tick_rate: float,
    datagram_budget: int,
    transport_header_bytes: int,
    fragment_header_bytes: int,
    auth_tag_bytes: int,
) -> ReplayMeasurement:
    fragment_payload_budget = (
        datagram_budget
        - transport_header_bytes
        - fragment_header_bytes
        - auth_tag_bytes
    )
    if fragment_payload_budget <= 0:
        raise ValueError("headers consume the complete datagram budget")

    snapshots: list[int] = []
    events: list[int] = []

    with path.open("rb") as replay:
        raw_header = replay.read(REPLAY_HEADER.size)
        if len(raw_header) != REPLAY_HEADER.size:
            raise ValueError(f"{path}: truncated replay header")

        (
            magic,
            version,
            header_size,
            _flags,
            declared_record_count,
            _declared_snapshot_count,
            _declared_event_count,
            _first_tick,
            _last_tick,
            _created_unix_ms,
        ) = REPLAY_HEADER.unpack(raw_header)

        if magic != REPLAY_MAGIC:
            raise ValueError(f"{path}: invalid replay magic {magic!r}")
        if header_size < REPLAY_HEADER.size:
            raise ValueError(f"{path}: invalid replay header size {header_size}")
        replay.seek(header_size)

        parsed_record_count = 0
        while True:
            raw_record = replay.read(RECORD_HEADER.size)
            if not raw_record:
                break
            if len(raw_record) != RECORD_HEADER.size:
                raise ValueError(f"{path}: truncated record header")

            (
                record_type,
                _reserved0,
                record_header_size,
                payload_size,
                _server_tick,
                _sequence,
                _reserved1,
            ) = RECORD_HEADER.unpack(raw_record)

            if record_header_size < RECORD_HEADER.size:
                raise ValueError(
                    f"{path}: invalid record header size {record_header_size}"
                )
            if record_header_size > RECORD_HEADER.size:
                replay.seek(record_header_size - RECORD_HEADER.size, 1)

            if record_type == SNAPSHOT_RECORD_TYPE:
                snapshots.append(payload_size)
            elif record_type == EVENT_RECORD_TYPE:
                events.append(payload_size)

            replay.seek(payload_size, 1)
            parsed_record_count += 1

    snapshot_stats = _stats(snapshots)
    snapshot_wire = [
        _wire_bytes(
            size,
            datagram_budget,
            transport_header_bytes,
            fragment_header_bytes,
            auth_tag_bytes,
        )
        for size in snapshots
    ]
    snapshot_datagrams = [datagrams for datagrams, _wire_size in snapshot_wire]
    snapshot_wire_bytes = [wire_size for _datagrams, wire_size in snapshot_wire]
    snapshot_datagram_stats = _stats(snapshot_datagrams)

    return ReplayMeasurement(
        path=str(path),
        replay_version=version,
        declared_record_count=declared_record_count,
        parsed_record_count=parsed_record_count,
        snapshots=snapshot_stats,
        events=_stats(events),
        snapshot_payload_kib_per_second=(
            snapshot_stats.average_bytes * tick_rate / 1024.0
        ),
        snapshot_wire_kib_per_second=(
            (fmean(snapshot_wire_bytes) if snapshot_wire_bytes else 0.0)
            * tick_rate
            / 1024.0
        ),
        ack_only_uplink_kib_per_second=(
            snapshot_datagram_stats.average_bytes
            * (transport_header_bytes + auth_tag_bytes)
            * tick_rate
            / 1024.0
        ),
        snapshot_datagrams_average=snapshot_datagram_stats.average_bytes,
        snapshot_datagrams_p95=snapshot_datagram_stats.p95_bytes,
        snapshot_datagrams_maximum=snapshot_datagram_stats.maximum_bytes,
    )


def _resolve_inputs(patterns: Iterable[str]) -> list[Path]:
    paths: set[Path] = set()
    for pattern in patterns:
        candidate = Path(pattern)
        if any(character in pattern for character in "*?["):
            paths.update(Path.cwd().glob(pattern))
        elif candidate.is_dir():
            paths.update(candidate.glob("*.wrpl"))
        else:
            paths.add(candidate)
    return sorted(path.resolve() for path in paths if path.is_file())


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "inputs",
        nargs="*",
        default=["Replay/*.wrpl"],
        help="Replay files, directories, or glob patterns",
    )
    parser.add_argument("--tick-rate", type=float, default=30.0)
    parser.add_argument("--datagram-budget", type=int, default=1200)
    parser.add_argument("--transport-header-bytes", type=int, default=40)
    parser.add_argument("--fragment-header-bytes", type=int, default=16)
    parser.add_argument("--auth-tag-bytes", type=int, default=0)
    parser.add_argument("--json-output", type=Path)
    args = parser.parse_args()

    paths = _resolve_inputs(args.inputs)
    if not paths:
        parser.error("no replay files matched")

    measurements = [
        measure_replay(
            path,
            args.tick_rate,
            args.datagram_budget,
            args.transport_header_bytes,
            args.fragment_header_bytes,
            args.auth_tag_bytes,
        )
        for path in paths
    ]

    result = {
        "configuration": {
            "tick_rate": args.tick_rate,
            "datagram_budget": args.datagram_budget,
            "transport_header_bytes": args.transport_header_bytes,
            "fragment_header_bytes": args.fragment_header_bytes,
            "auth_tag_bytes": args.auth_tag_bytes,
            "fragment_payload_budget": (
                args.datagram_budget
                - args.transport_header_bytes
                - args.fragment_header_bytes
                - args.auth_tag_bytes
            ),
        },
        "replays": [
            {
                **asdict(measurement),
                "snapshots": asdict(measurement.snapshots),
                "events": asdict(measurement.events),
            }
            for measurement in measurements
        ],
    }
    rendered = json.dumps(result, indent=2, ensure_ascii=False)
    print(rendered)

    if args.json_output:
        args.json_output.parent.mkdir(parents=True, exist_ok=True)
        args.json_output.write_text(rendered + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
