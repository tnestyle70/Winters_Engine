#!/usr/bin/env python3
"""Wait for the Elden full extraction queue, then build runtime-ready outputs.

This is a post-pass orchestrator. It does not claim that missing engine
features such as full HKX/TAE conversion, FXR graph playback, 3D collision,
or complete map streaming are solved. It runs every automatic conversion that
exists today, writes catalogs/audits, and records blockers explicitly.
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
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
CONVERTER = WINTERS / "Tools" / "Bin" / "Debug" / "WintersAssetConverter.exe"


def now_stamp() -> str:
    return datetime.now().isoformat(timespec="seconds")


def now_file_stamp() -> str:
    return datetime.now().strftime("%Y%m%d_%H%M%S")


def write_json(path: Path, data: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def log_line(path: Path, message: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "a", encoding="utf-8") as handle:
        handle.write(f"[{now_stamp()}] {message}\n")


def run_process(command: list[str], cwd: Path, log_path: Path, timeout: int | None = None) -> dict[str, Any]:
    started = now_stamp()
    log_line(log_path, "RUN " + " ".join(command))
    try:
        completed = subprocess.run(command, cwd=str(cwd), capture_output=True, text=True, timeout=timeout)
        if completed.stdout:
            log_line(log_path, "STDOUT\n" + completed.stdout[-12000:])
        if completed.stderr:
            log_line(log_path, "STDERR\n" + completed.stderr[-12000:])
        return {
            "startedAt": started,
            "endedAt": now_stamp(),
            "command": command,
            "exitCode": completed.returncode,
            "ok": completed.returncode == 0,
        }
    except subprocess.TimeoutExpired as exc:
        log_line(log_path, f"TIMEOUT after {timeout}s")
        return {
            "startedAt": started,
            "endedAt": now_stamp(),
            "command": command,
            "exitCode": None,
            "ok": False,
            "timeout": timeout,
            "stdoutTail": (exc.stdout or "")[-4000:] if isinstance(exc.stdout, str) else "",
            "stderrTail": (exc.stderr or "")[-4000:] if isinstance(exc.stderr, str) else "",
        }
    except Exception as exc:
        log_line(log_path, f"EXCEPTION {exc!r}")
        return {
            "startedAt": started,
            "endedAt": now_stamp(),
            "command": command,
            "exitCode": None,
            "ok": False,
            "exception": repr(exc),
        }


def queue_total() -> int:
    data = json.loads(QUEUE.read_text(encoding="utf-8"))
    return sum(1 for item in data.get("records", []) if isinstance(item, dict) and item.get("action") == "witchy-unpack")


def status_count() -> int:
    status_root = WORK_ROOT / "status"
    if not status_root.exists():
        return 0
    return sum(1 for path in status_root.iterdir() if path.is_file() and path.suffix.lower() == ".json")


def disk_free_gib() -> float:
    return shutil.disk_usage(WORK_ROOT).free / 2**30


def pipeline_workers_running() -> int:
    try:
        completed = subprocess.run(
            [
                "powershell.exe",
                "-NoProfile",
                "-NonInteractive",
                "-Command",
                "(Get-CimInstance Win32_Process -Filter \"Name='python.exe'\" | "
                "Where-Object { $_.CommandLine -match 'run-full-pipeline|run_booster_driver.py|run_24h_driver.py' }).Count",
            ],
            capture_output=True,
            text=True,
            timeout=60,
        )
        return int((completed.stdout or "0").strip() or 0)
    except Exception:
        return 0


def wait_for_completion(deadline: datetime, poll_seconds: int, log_path: Path) -> dict[str, Any]:
    total = queue_total()
    last_done = -1
    while True:
        done = status_count()
        workers = pipeline_workers_running()
        pct = round(done / total * 100.0, 2) if total else 100.0
        if done != last_done:
            log_line(log_path, f"progress done={done}/{total} pct={pct} workers={workers} free={disk_free_gib():.2f}GiB")
            last_done = done
        if done >= total and workers == 0:
            return {"ok": True, "total": total, "done": done, "pct": pct, "workers": workers}
        if datetime.now() >= deadline:
            return {"ok": False, "reason": "deadline", "total": total, "done": done, "pct": pct, "workers": workers}
        time.sleep(poll_seconds)


def discover_characters() -> list[str]:
    root = RESOURCE_ROOT / "FullGame" / "chr" / "character-binder"
    if not root.exists():
        return []
    characters: list[str] = []
    for path in sorted(root.glob("chr_c*.chrbnd")):
        name = path.name
        if name.startswith("chr_") and name.endswith(".chrbnd"):
            characters.append(name[len("chr_") : -len(".chrbnd")])
    return characters


def discover_map_tiles() -> list[str]:
    root = RESOURCE_ROOT / "FullGame" / "map" / "map-msb"
    if not root.exists():
        return []
    tiles: list[str] = []
    for path in sorted(root.glob("map_mapstudio_*.msb")):
        name = path.name
        if name.startswith("map_mapstudio_") and name.endswith(".msb"):
            tiles.append(name[len("map_mapstudio_") : -len(".msb")])
    return tiles


def chunked(items: list[str], size: int) -> list[list[str]]:
    return [items[index : index + size] for index in range(0, len(items), size)]


def summarize_runtime_character_cooks(paths: list[Path]) -> dict[str, Any]:
    records: list[dict[str, Any]] = []
    for path in paths:
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except Exception:
            continue
        for record in data.get("records", []):
            if isinstance(record, dict):
                records.append(record)

    mesh_ready = []
    bone_blocked = []
    failed = []
    for record in records:
        character = record.get("character")
        runtime_dir = Path(str(record.get("runtimeDir", "")))
        wmesh_exists = bool(character and (runtime_dir / f"{character}.wmesh").exists())
        if wmesh_exists:
            mesh_ready.append(character)
        elif record.get("blocked") == "bone-limit":
            bone_blocked.append(character)
        else:
            failed.append({"character": character, "detail": record.get("detail")})

    return {
        "characters": len(records),
        "runtimeMeshReady": len(mesh_ready),
        "boneLimitBlocked": len(bone_blocked),
        "failed": len(failed),
        "runtimeMeshReadySamples": mesh_ready[:40],
        "boneLimitBlockedSamples": bone_blocked[:40],
        "failedSamples": failed[:40],
    }


def summarize_map_placements(paths: list[Path]) -> dict[str, Any]:
    placed = 0
    unresolved = 0
    ok = 0
    failed = 0
    samples: list[dict[str, Any]] = []
    for path in paths:
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except Exception:
            failed += 1
            continue
        counts = data.get("counts", {}) if isinstance(data.get("counts"), dict) else {}
        tile = data.get("mapTile")
        tile_placed = int(counts.get("placed", 0) or 0)
        tile_unresolved = int(counts.get("unresolved", 0) or 0)
        placed += tile_placed
        unresolved += tile_unresolved
        ok += 1
        if len(samples) < 40:
            samples.append({"tile": tile, "placed": tile_placed, "unresolved": tile_unresolved})
    return {
        "tilesOk": ok,
        "tilesFailedOrMissing": failed,
        "placed": placed,
        "unresolved": unresolved,
        "samples": samples,
    }


def run_post_cook(args: argparse.Namespace, run_dir: Path, log_path: Path) -> dict[str, Any]:
    manifests = RESOURCE_ROOT / "Manifests"
    stamp = now_file_stamp()
    report: dict[str, Any] = {
        "schema": "winters.elden.post_full_cook.v1",
        "generatedAt": now_stamp(),
        "resourceRoot": str(RESOURCE_ROOT),
        "workRoot": str(WORK_ROOT),
        "steps": {},
        "notes": [
            "자동 후처리는 현재 구현된 변환기만 실행한다.",
            "HKX/TAE 전량 .wanim 변환, FXR graph 재생, 3D collision/navmesh runtime 변환, 전체 map streaming은 별도 구현/검증 대상이다.",
        ],
    }

    catalog_out = manifests / "eldenring_asset_catalog.json"
    texture_out = manifests / "eldenring_texture_index.json"
    report["steps"]["buildResourceCatalog"] = run_process(
        [
            sys.executable,
            str(PIPELINE),
            "build-resource-catalog",
            "--resource-root",
            str(RESOURCE_ROOT),
            "--game-root",
            str(GAME_ROOT),
            "--witchy-root",
            str(WORK_ROOT / "unpacked"),
            "--work-root",
            str(WORK_ROOT),
            "--converter",
            str(CONVERTER),
            "--texture-out",
            str(texture_out),
            "--out",
            str(catalog_out),
            "--pretty",
        ],
        WINTERS,
        log_path,
        args.catalog_timeout,
    )

    characters = discover_characters()
    report["charactersDiscovered"] = len(characters)
    cook_manifests: list[Path] = []
    for batch_index, batch in enumerate(chunked(characters, args.character_batch_size), start=1):
        out = run_dir / f"runtime_character_cook_batch_{batch_index:03d}_{stamp}.json"
        command = [
            sys.executable,
            str(PIPELINE),
            "cook-runtime-character",
            "--resource-root",
            str(RESOURCE_ROOT),
            "--runtime-root",
            str(RESOURCE_ROOT / "Runtime"),
            "--converter",
            str(CONVERTER),
            "--repo-root",
            str(WINTERS),
            "--out",
            str(out),
            "--pretty",
            "--max-bones",
            str(args.max_bones),
        ]
        for character in batch:
            command.extend(["--character", character])
        result = run_process(command, WINTERS, log_path, args.character_timeout)
        report["steps"][f"cookRuntimeCharacterBatch{batch_index:03d}"] = result
        cook_manifests.append(out)

    report["runtimeCharacterSummary"] = summarize_runtime_character_cooks(cook_manifests)

    map_tiles = discover_map_tiles()
    if args.map_tile_limit > 0:
        map_tiles = map_tiles[: args.map_tile_limit]
    report["mapTilesDiscovered"] = len(map_tiles)
    placement_manifests: list[Path] = []
    for index, tile in enumerate(map_tiles, start=1):
        out_dir = RESOURCE_ROOT / "Runtime" / "MapPlacement" / tile
        result = run_process(
            [
                sys.executable,
                str(PIPELINE),
                "build-map-placement",
                "--resource-root",
                str(RESOURCE_ROOT),
                "--repo-root",
                str(WINTERS),
                "--map-tile",
                tile,
                "--out-dir",
                str(out_dir),
            ],
            WINTERS,
            log_path,
            args.map_placement_timeout,
        )
        report["steps"][f"buildMapPlacement{index:04d}_{tile}"] = result
        manifest_path = out_dir / "map_placement.json"
        if manifest_path.exists():
            placement_manifests.append(manifest_path)

    report["mapPlacementSummary"] = summarize_map_placements(placement_manifests)

    audit_out = manifests / "eldenring_full_pipeline_audit.json"
    report["steps"]["auditFullPipeline"] = run_process(
        [
            sys.executable,
            str(PIPELINE),
            "audit-full-pipeline",
            "--queue",
            str(QUEUE),
            "--resource-root",
            str(RESOURCE_ROOT),
            "--work-root",
            str(WORK_ROOT),
            "--catalog",
            str(catalog_out),
            "--converter",
            str(CONVERTER),
            "--out",
            str(audit_out),
            "--pretty",
        ],
        WINTERS,
        log_path,
        args.audit_timeout,
    )

    report["finalStatus"] = {
        "queueTotal": queue_total(),
        "statusFiles": status_count(),
        "attemptedPct": round(status_count() / queue_total() * 100.0, 2) if queue_total() else 100.0,
        "freeGiB": round(disk_free_gib(), 2),
        "catalog": str(catalog_out),
        "textureIndex": str(texture_out),
        "audit": str(audit_out),
    }
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description="Wait for full extraction completion, then post-cook runtime assets.")
    parser.add_argument("--hours", type=float, default=36.0, help="Maximum time to wait before giving up.")
    parser.add_argument("--poll-seconds", type=int, default=300, help="Completion polling interval.")
    parser.add_argument("--max-bones", type=int, default=512, help="Runtime skinned mesh bone limit.")
    parser.add_argument("--character-batch-size", type=int, default=24, help="Characters per cook-runtime-character call.")
    parser.add_argument("--map-tile-limit", type=int, default=0, help="Limit map placement builds. 0 means all discovered tiles.")
    parser.add_argument("--catalog-timeout", type=int, default=7200)
    parser.add_argument("--character-timeout", type=int, default=7200)
    parser.add_argument("--map-placement-timeout", type=int, default=600)
    parser.add_argument("--audit-timeout", type=int, default=7200)
    parser.add_argument("--dry-run", action="store_true", help="Print discovered plan without waiting or running.")
    args = parser.parse_args()

    run_dir = WORK_ROOT / "runs" / "post_full_cook"
    run_dir.mkdir(parents=True, exist_ok=True)
    log_path = run_dir / "post_full_cook.log"
    summary_path = run_dir / "post_full_cook_summary.json"

    plan = {
        "generatedAt": now_stamp(),
        "queueTotal": queue_total(),
        "statusFiles": status_count(),
        "charactersDiscovered": len(discover_characters()),
        "mapTilesDiscovered": len(discover_map_tiles()),
        "deadline": (datetime.now() + timedelta(hours=args.hours)).isoformat(timespec="seconds"),
        "resourceRoot": str(RESOURCE_ROOT),
        "workRoot": str(WORK_ROOT),
    }
    print(json.dumps(plan, indent=2), flush=True)
    log_line(log_path, "post full cook watcher start " + json.dumps(plan, sort_keys=True))

    if args.dry_run:
        write_json(summary_path, {"dryRun": True, "plan": plan})
        return 0

    completion = wait_for_completion(datetime.now() + timedelta(hours=args.hours), args.poll_seconds, log_path)
    if not completion.get("ok"):
        write_json(summary_path, {"schema": "winters.elden.post_full_cook.v1", "generatedAt": now_stamp(), "completion": completion})
        log_line(log_path, "completion wait failed " + json.dumps(completion, sort_keys=True))
        return 2

    log_line(log_path, "completion reached " + json.dumps(completion, sort_keys=True))
    report = run_post_cook(args, run_dir, log_path)
    report["completion"] = completion
    write_json(summary_path, report)
    log_line(log_path, f"post full cook finished summary={summary_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
