#!/usr/bin/env python3
"""Controlled turbo runner for the Elden full extraction pipeline.

This runner intentionally runs alongside the normal 24h driver. It takes
future offset ranges from the heavy buckets and splits them across a small
number of extra workers. Use STOP_BOOSTER.txt to stop only this runner, or
STOP_24H.txt to stop both the normal driver and the booster.
"""

from __future__ import annotations

import argparse
import json
import math
import shutil
import subprocess
import sys
import threading
import time
from datetime import datetime, timedelta
from pathlib import Path
from typing import Any

WINTERS = Path(__file__).resolve().parents[2]
PIPELINE = WINTERS / "Tools" / "EldenAssetPipeline" / "elden_pipeline.py"
WORK_ROOT = Path(r"C:/Users/tnest/Desktop/EldenRingExtract/_full_pipeline")
RESOURCE_ROOT = WINTERS / "Client" / "Bin" / "Resource" / "EldenRing"
QUEUE = RESOURCE_ROOT / "Manifests" / "eldenring_full_extraction_queue.json"
GAME_ROOT = Path(r"C:/Program Files (x86)/Steam/steamapps/common/ELDEN RING/Game")
WITCHY = Path(r"C:/Users/tnest/Downloads/WitchyBND-v3.0.0.0-win-x64/WitchyBND.exe")
BLENDER = Path(r"C:/Users/tnest/Downloads/blender-4.2.18-windows-x64/blender-4.2.18-windows-x64/blender.exe")
CONVERTER = WINTERS / "Tools" / "Bin" / "Debug" / "WintersAssetConverter.exe"
TEXCONV = Path(r"C:/Users/tnest/Downloads/texconv.exe")
STOP_ALL_FILE = WORK_ROOT / "STOP_24H.txt"
STOP_BOOSTER_FILE = WORK_ROOT / "STOP_BOOSTER.txt"

DEFAULT_SLICE = 40
DEFAULT_MIN_FREE_GIB = 18.0

COMMON_ARGS = [
    "run-full-pipeline",
    "--queue", str(QUEUE),
    "--game-root", str(GAME_ROOT),
    "--resource-root", str(RESOURCE_ROOT),
    "--work-root", str(WORK_ROOT),
    "--witchy", str(WITCHY),
    "--blender", str(BLENDER),
    "--converter", str(CONVERTER),
    "--texconv", str(TEXCONV),
    "--resume", "--continue-on-error", "--clean-unpack",
]


def now_stamp() -> str:
    return datetime.now().isoformat(timespec="seconds")


def log_line(log_path: Path, message: str) -> None:
    with open(log_path, "a", encoding="utf-8") as handle:
        handle.write(f"[{now_stamp()}] {message}\n")


def disk_free_gib() -> float:
    return shutil.disk_usage(WORK_ROOT).free / 2**30


def read_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def queue_count(top_dir: str, bundle_kind: str) -> int:
    data = read_json(QUEUE)
    count = 0
    for record in data.get("records", []):
        if not isinstance(record, dict):
            continue
        if record.get("action") != "witchy-unpack":
            continue
        if str(record.get("topDir", "")).lower() != top_dir.lower():
            continue
        if str(record.get("bundleKind", "")).lower() != bundle_kind.lower():
            continue
        count += 1
    return count


def slice_offset_from(path: Path, top_dir: str, bundle_kind: str) -> int | None:
    try:
        data = read_json(path)
    except Exception:
        return None
    filters = data.get("filters", {})
    top_dirs = [str(item).lower() for item in filters.get("topDir") or []]
    kinds = [str(item).lower() for item in filters.get("bundleKind") or []]
    if top_dir.lower() not in top_dirs:
        return None
    if bundle_kind.lower() not in kinds:
        return None
    try:
        return int(filters.get("offset", 0))
    except Exception:
        return None


def detect_current_offset(log_dir: Path, top_dir: str, bundle_kind: str) -> int:
    offsets: list[int] = []
    for path in log_dir.glob("*_slice.json"):
        offset = slice_offset_from(path, top_dir, bundle_kind)
        if offset is not None:
            offsets.append(offset)
    return max(offsets) if offsets else 0


def round_up_to_slice(value: int, slice_size: int) -> int:
    return int(math.ceil(value / slice_size) * slice_size)


def should_stop(deadline: datetime, min_free_gib: float) -> str | None:
    if datetime.now() >= deadline:
        return "deadline"
    if STOP_ALL_FILE.exists():
        return "stop-all-file"
    if STOP_BOOSTER_FILE.exists():
        return "stop-booster-file"
    if disk_free_gib() < min_free_gib:
        return "disk-floor"
    return None


def run_slice(
    label: str,
    worker_index: int,
    filters: list[str],
    offset: int,
    slice_size: int,
    min_free_gib: float,
    log_dir: Path,
) -> dict[str, Any]:
    out_json = log_dir / f"{label}_w{worker_index}_slice.json"
    command = [
        sys.executable,
        str(PIPELINE),
        *COMMON_ARGS,
        "--min-free-gib", str(min_free_gib),
        *filters,
        "--offset", str(offset),
        "--limit", str(slice_size),
        "--out", str(out_json),
    ]
    with open(log_dir / f"{label}_w{worker_index}.log", "a", encoding="utf-8") as log:
        subprocess.run(command, stdout=log, stderr=log, cwd=str(WINTERS))
    try:
        data = read_json(out_json)
        return data.get("counts", {})
    except Exception as exc:
        return {"error": repr(exc)}


def worker(
    label: str,
    worker_index: int,
    worker_count: int,
    filters: list[str],
    start_offset: int,
    total_count: int,
    slice_size: int,
    deadline: datetime,
    min_free_gib: float,
    log_dir: Path,
) -> None:
    log_path = log_dir / f"{label}_w{worker_index}.log"
    stride = slice_size * worker_count
    offset = start_offset + worker_index * slice_size
    log_line(
        log_path,
        f"worker start label={label} index={worker_index}/{worker_count} "
        f"start={offset} stride={stride} total={total_count}",
    )

    while offset < total_count:
        reason = should_stop(deadline, min_free_gib)
        if reason:
            log_line(log_path, f"worker stopped: {reason} offset={offset} free={disk_free_gib():.1f}GiB")
            return
        counts = run_slice(label, worker_index, filters, offset, slice_size, min_free_gib, log_dir)
        selected = int(counts.get("selected", 0) or 0)
        ok = int(counts.get("ok", 0) or 0)
        failed = int(counts.get("failed", 0) or 0)
        log_line(
            log_path,
            f"offset={offset} selected={selected} ok={ok} failed={failed} free={disk_free_gib():.1f}GiB",
        )
        if selected == 0:
            break
        offset += stride

    log_line(log_path, f"worker finished offset={offset} free={disk_free_gib():.1f}GiB")


def add_bucket_plan(
    plans: list[dict[str, Any]],
    log_dir: Path,
    label: str,
    top_dir: str,
    bundle_kind: str,
    workers: int,
    manual_start: int | None,
    guard_slices: int,
    slice_size: int,
) -> None:
    if workers <= 0:
        return
    total = queue_count(top_dir, bundle_kind)
    current = detect_current_offset(log_dir, top_dir, bundle_kind)
    start = manual_start
    if start is None:
        start = round_up_to_slice(current + guard_slices * slice_size, slice_size)
    if start >= total:
        return
    plans.append(
        {
            "label": label,
            "top_dir": top_dir,
            "bundle_kind": bundle_kind,
            "workers": workers,
            "current": current,
            "start": start,
            "total": total,
            "filters": ["--top-dir", top_dir, "--bundle-kind", bundle_kind],
        }
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Controlled Elden full-pipeline booster runner.")
    parser.add_argument("--hours", type=float, default=10.0, help="Booster runtime limit.")
    parser.add_argument("--asset-workers", type=int, default=2, help="Extra asset geometry workers.")
    parser.add_argument("--map-workers", type=int, default=1, help="Extra map texture workers.")
    parser.add_argument("--asset-start", type=int, default=None, help="Manual asset geometry start offset.")
    parser.add_argument("--map-start", type=int, default=None, help="Manual map texture start offset.")
    parser.add_argument("--guard-slices", type=int, default=40, help="Safety gap after current normal-driver offset.")
    parser.add_argument("--slice", type=int, default=DEFAULT_SLICE, help="Records per run-full-pipeline call.")
    parser.add_argument("--min-free-gib", type=float, default=DEFAULT_MIN_FREE_GIB, help="Booster disk floor.")
    parser.add_argument("--dry-run", action="store_true", help="Print planned workers without starting them.")
    args = parser.parse_args()

    log_dir = WORK_ROOT / "runs" / "booster"
    log_dir.mkdir(parents=True, exist_ok=True)
    driver_log = log_dir / "booster_driver.log"
    deadline = datetime.now() + timedelta(hours=args.hours)

    if STOP_BOOSTER_FILE.exists():
        STOP_BOOSTER_FILE.unlink()
        log_line(driver_log, "removed stale booster stop file")

    plans: list[dict[str, Any]] = []
    add_bucket_plan(
        plans,
        WORK_ROOT / "runs" / "24h",
        "asset_geom",
        "asset",
        "asset-geometry-binder",
        args.asset_workers,
        args.asset_start,
        args.guard_slices,
        args.slice,
    )
    add_bucket_plan(
        plans,
        WORK_ROOT / "runs" / "24h",
        "map_texture",
        "map",
        "texture-binder",
        args.map_workers,
        args.map_start,
        args.guard_slices,
        args.slice,
    )

    summary = {
        "generatedAt": now_stamp(),
        "deadline": deadline.isoformat(timespec="seconds"),
        "slice": args.slice,
        "minFreeGiB": args.min_free_gib,
        "freeGiB": round(disk_free_gib(), 2),
        "plans": [
            {key: value for key, value in plan.items() if key != "filters"}
            for plan in plans
        ],
    }
    print(json.dumps(summary, indent=2), flush=True)
    log_line(driver_log, f"booster start pid={subprocess.os.getpid()} plan={json.dumps(summary, sort_keys=True)}")

    if args.dry_run:
        log_line(driver_log, "dry-run complete")
        return 0
    if not plans:
        log_line(driver_log, "no booster work planned")
        return 0

    threads: list[threading.Thread] = []
    for plan in plans:
        for index in range(int(plan["workers"])):
            thread = threading.Thread(
                target=worker,
                args=(
                    plan["label"],
                    index,
                    int(plan["workers"]),
                    plan["filters"],
                    int(plan["start"]),
                    int(plan["total"]),
                    args.slice,
                    deadline,
                    args.min_free_gib,
                    log_dir,
                ),
                daemon=False,
            )
            thread.start()
            threads.append(thread)
            time.sleep(3)

    for thread in threads:
        thread.join()

    log_line(driver_log, f"booster end free={disk_free_gib():.1f}GiB")
    return 0


if __name__ == "__main__":
    sys.exit(main())
