#!/usr/bin/env python3
"""Unattended multi-lane driver for the Elden full extraction pipeline.

Runs `elden_pipeline.py run-full-pipeline` slices across parallel category
lanes until the deadline, a stop file, or the disk floor is reached. Designed
to be launched as a detached process so it survives the editor/agent session.

Stop it at any time by creating the stop file:
    C:/Users/tnest/Desktop/EldenRingExtract/_full_pipeline/STOP_24H.txt
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
import threading
import time
from datetime import datetime, timedelta
from pathlib import Path

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
STOP_FILE = WORK_ROOT / "STOP_24H.txt"
SLICE = 40
DISK_FLOOR_GIB = 13.0

# Lanes run concurrently; stages inside a lane run sequentially. Priority is
# roughly: finish chr/parts, then asset geometry, then asset misc, then map,
# then animation raw + cutscene. --resume makes every stage idempotent.
LANES: dict[str, list[list[str]]] = {
    "lane1_chr_parts": [
        ["--top-dir", "chr"],
        ["--top-dir", "parts"],
    ],
    "lane2_asset_geom": [
        ["--top-dir", "asset", "--bundle-kind", "asset-geometry-binder"],
    ],
    "lane3_asset_dcx": [
        ["--top-dir", "asset", "--bundle-kind", "dcx"],
    ],
    "lane4_map": [
        ["--top-dir", "map", "--bundle-kind", "map-msb"],
        ["--top-dir", "map", "--bundle-kind", "map-binder"],
        ["--top-dir", "map", "--bundle-kind", "navmesh-binder"],
        ["--top-dir", "map", "--bundle-kind", "dcx"],
        ["--top-dir", "map", "--bundle-kind", "ivinfo-binder"],
        ["--top-dir", "map", "--bundle-kind", "battle-binder"],
        ["--top-dir", "map", "--bundle-kind", "texture-binder"],
    ],
    "lane5_anim_cutscene": [
        ["--top-dir", "chr", "--bundle-kind", "character-animation-binder"],
        ["--top-dir", "cutscene"],
        ["--top-dir", "menu"],
        ["--top-dir", "msg"],
        ["--top-dir", "param"],
        ["--top-dir", "material"],
        ["--top-dir", "script"],
        ["--top-dir", "event"],
        ["--top-dir", "shader"],
    ],
}

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
    "--min-free-gib", "12",
]


def log_line(log_path: Path, message: str) -> None:
    stamp = datetime.now().isoformat(timespec="seconds")
    with open(log_path, "a", encoding="utf-8") as handle:
        handle.write(f"[{stamp}] {message}\n")


def disk_free_gib() -> float:
    return shutil.disk_usage(WORK_ROOT).free / 2**30


def should_stop(deadline: datetime, log_path: Path) -> str | None:
    if datetime.now() >= deadline:
        return "deadline"
    if STOP_FILE.exists():
        return "stop-file"
    if disk_free_gib() < DISK_FLOOR_GIB:
        return "disk-floor"
    return None


def other_pipeline_running() -> bool:
    """True when another run-full-pipeline python process is active."""
    try:
        completed = subprocess.run(
            [
                "powershell.exe", "-NoProfile", "-NonInteractive", "-Command",
                "(Get-CimInstance Win32_Process -Filter \"Name='python.exe'\" | "
                "Where-Object { $_.CommandLine -match 'run-full-pipeline' }).Count",
            ],
            capture_output=True,
            text=True,
            timeout=60,
        )
        return int((completed.stdout or "0").strip() or 0) > 0
    except Exception:
        return False


def run_slice(lane: str, filters: list[str], offset: int, log_dir: Path) -> dict:
    out_json = log_dir / f"{lane}_slice.json"
    command = [
        sys.executable, str(PIPELINE), *COMMON_ARGS, *filters,
        "--offset", str(offset), "--limit", str(SLICE), "--out", str(out_json),
    ]
    with open(log_dir / f"{lane}.log", "a", encoding="utf-8") as log:
        subprocess.run(command, stdout=log, stderr=log, cwd=str(WINTERS))
    try:
        data = json.loads(out_json.read_text(encoding="utf-8"))
        return data.get("counts", {})
    except Exception:
        return {}


def lane_worker(lane: str, stages: list[list[str]], deadline: datetime, log_dir: Path) -> None:
    lane_log = log_dir / f"{lane}.log"
    for filters in stages:
        offset = 0
        consecutive_all_fail = 0
        while True:
            reason = should_stop(deadline, lane_log)
            if reason:
                log_line(lane_log, f"lane stopped: {reason}")
                return
            counts = run_slice(lane, filters, offset, log_dir)
            selected = int(counts.get("selected", 0))
            ok = int(counts.get("ok", 0))
            failed = int(counts.get("failed", 0))
            log_line(
                lane_log,
                f"stage={' '.join(filters)} offset={offset} selected={selected} "
                f"ok={ok} failed={failed} free={disk_free_gib():.1f}GiB",
            )
            if selected == 0:
                break
            if selected > 0 and ok == 0 and failed >= selected:
                consecutive_all_fail += 1
                if consecutive_all_fail >= 5:
                    log_line(lane_log, "lane aborted: 5 consecutive all-fail slices, check toolchain")
                    return
            else:
                consecutive_all_fail = 0
            offset += SLICE
    log_line(lane_log, "lane finished all stages")


def main() -> int:
    parser = argparse.ArgumentParser(description="24h unattended Elden pipeline driver.")
    parser.add_argument("--hours", type=float, default=24.0, help="Run duration in hours.")
    args = parser.parse_args()

    log_dir = WORK_ROOT / "runs" / "24h"
    log_dir.mkdir(parents=True, exist_ok=True)
    driver_log = log_dir / "driver.log"
    deadline = datetime.now() + timedelta(hours=args.hours)
    log_line(driver_log, f"driver start, deadline={deadline.isoformat(timespec='seconds')}, pid={subprocess.os.getpid()}")

    if STOP_FILE.exists():
        STOP_FILE.unlink()
        log_line(driver_log, "removed stale stop file")

    while other_pipeline_running():
        if datetime.now() >= deadline or STOP_FILE.exists():
            log_line(driver_log, "stopped while waiting for existing workers")
            return 0
        log_line(driver_log, "waiting for existing run-full-pipeline workers to drain")
        time.sleep(60)
    log_line(driver_log, "no other pipeline workers, starting lanes")

    threads = []
    for lane, stages in LANES.items():
        thread = threading.Thread(target=lane_worker, args=(lane, stages, deadline, log_dir), daemon=False)
        thread.start()
        threads.append(thread)
        time.sleep(5)
    for thread in threads:
        thread.join()

    log_line(driver_log, f"driver end, free={disk_free_gib():.1f}GiB")
    return 0


if __name__ == "__main__":
    sys.exit(main())
