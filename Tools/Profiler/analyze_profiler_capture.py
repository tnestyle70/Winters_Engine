#!/usr/bin/env python3
"""Analyze Winters Profiler v1 single-frame or v2/v3 timeline JSON captures."""

from __future__ import annotations

import argparse
import json
import math
from collections import defaultdict
from pathlib import Path
from typing import Any, Iterable


def percentile(values: Iterable[float], quantile: float) -> float | None:
    ordered = sorted(float(value) for value in values if math.isfinite(float(value)))
    if not ordered:
        return None
    if len(ordered) == 1:
        return ordered[0]
    position = (len(ordered) - 1) * quantile
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return ordered[lower]
    fraction = position - lower
    return ordered[lower] * (1.0 - fraction) + ordered[upper] * fraction


def stats(values: Iterable[float]) -> dict[str, float | int | None]:
    samples = [float(value) for value in values if math.isfinite(float(value))]
    if not samples:
        return {
            "count": 0,
            "min": None,
            "median": None,
            "p95": None,
            "p99": None,
            "max": None,
            "mean": None,
        }
    return {
        "count": len(samples),
        "min": min(samples),
        "median": percentile(samples, 0.50),
        "p95": percentile(samples, 0.95),
        "p99": percentile(samples, 0.99),
        "max": max(samples),
        "mean": sum(samples) / len(samples),
    }


def counter_map(frame: dict[str, Any]) -> dict[str, int]:
    result: dict[str, int] = {}
    for counter in frame.get("counters", []):
        name = str(counter.get("name", ""))
        if name:
            result[name] = int(counter.get("value", 0))
    return result


def normalize_frames(payload: dict[str, Any]) -> list[dict[str, Any]]:
    frames = payload.get("frames")
    if isinstance(frames, list):
        return [frame for frame in frames if isinstance(frame, dict)]

    return [
        {
            "renderFrame": 0,
            "serverTick": None,
            "frameMs": float(payload.get("frameMs", 0.0)),
            "gpuFrameUs": next(
                (
                    int(counter.get("value", 0))
                    for counter in payload.get("counters", [])
                    if counter.get("name") == "GPU::FrameUs"
                ),
                None,
            ),
            "droppedScopeStats": 0,
            "droppedCounters": 0,
            "droppedRawEvents": int(bool(payload.get("truncatedRawEvents", False))),
            "scopes": payload.get("frameScopes", []),
            "counters": payload.get("counters", []),
            "rawEvents": payload.get("rawEvents", []),
        }
    ]


def build_analysis(
    capture_path: Path,
    payload: dict[str, Any],
    target_fps: float,
    top_count: int,
    expected_width: int,
    expected_height: int,
    minimum_duration_sec: float,
) -> dict[str, Any]:
    frames = normalize_frames(payload)
    performance_frames = [
        frame for frame in frames if bool(frame.get("frameStrideSample", True))
    ]
    if not performance_frames:
        performance_frames = frames
    performance_frame_ids = {id(frame) for frame in performance_frames}

    budget_ms = 1000.0 / target_fps
    median_budget_ms = budget_ms * (5.0 / 6.0)
    p99_budget_ms = budget_ms * 1.25
    hitch_threshold_ms = 1000.0 / 120.0
    profiler_overhead_budget_ms = budget_ms * 0.01

    work_times: list[float] = []
    wall_times: list[float] = []
    cadence_times: list[float] = []
    gpu_times: list[float] = []
    gpu_source_frames: list[int] = []
    gpu_source_pairs: list[tuple[int, int]] = []
    profiler_end_frame_times: list[float] = []
    scope_samples: dict[str, list[float]] = defaultdict(list)
    scope_calls: dict[str, int] = defaultdict(int)
    dropped_scope_stats = 0
    dropped_counters = 0
    dropped_raw_events = 0
    spike_frames: list[dict[str, Any]] = []
    in_game_frame_count = 0
    full_scenario_frame_count = 0
    config_valid_frame_count = 0
    server_tick_frame_count = 0
    server_ticks: list[int] = []
    elapsed_samples: list[int] = []
    required_scope_names = {
        "Scene_InGame::OnUpdate",
        "Scene_InGame::OnRender",
        "Map::Render",
        "Champion::Render",
        "Minion::Render",
        "UIOverlay::Render",
    }

    for frame in frames:
        counters = counter_map(frame)
        elapsed_us = counters.get("Frame::RunElapsedUs")
        if elapsed_us is not None:
            elapsed_samples.append(elapsed_us)
        server_tick = frame.get("serverTick")
        if server_tick is None:
            server_tick = counters.get("Net::LatestServerTick")
        if server_tick is not None:
            server_tick_frame_count += 1
            server_ticks.append(int(server_tick))

        dropped_scope_stats += int(frame.get("droppedScopeStats", 0))
        dropped_counters += int(frame.get("droppedCounters", 0))
        dropped_raw_events += int(frame.get("droppedRawEvents", 0))

        frame_scope_names = {
            str(scope.get("name", "")) for scope in frame.get("scopes", [])
        }
        if {"Scene_InGame::OnUpdate", "Scene_InGame::OnRender"}.issubset(
            frame_scope_names
        ):
            in_game_frame_count += 1
        if required_scope_names.issubset(frame_scope_names):
            full_scenario_frame_count += 1
        if (
            counters.get("Frame::LimiterActive") == 0
            and counters.get("Frame::PresentationVSync") == 0
            and counters.get("Frame::WindowWidth") == expected_width
            and counters.get("Frame::WindowHeight") == expected_height
            and counters.get("RHI::Backend") == 0
        ):
            config_valid_frame_count += 1

        if id(frame) not in performance_frame_ids:
            continue

        previous_end_frame_us = counters.get("Profiler::PreviousEndFrameUs")
        if previous_end_frame_us is not None:
            profiler_end_frame_times.append(float(previous_end_frame_us) / 1000.0)

        for scope in frame.get("scopes", []):
            name = str(scope.get("name", ""))
            if not name:
                continue
            scope_samples[name].append(float(scope.get("totalMs", 0.0)))
            scope_calls[name] += int(scope.get("calls", 0))

        work_ms = float(frame.get("frameMs", 0.0))
        work_times.append(work_ms)
        wall_us = counters.get("Frame::WallUs")
        wall_ms_value = float(wall_us) / 1000.0 if wall_us is not None else None
        if wall_ms_value is not None:
            wall_times.append(wall_ms_value)
        cadence_us = counters.get("Frame::CadenceUs")
        cadence_ms_value = (
            float(cadence_us) / 1000.0 if cadence_us is not None else None
        )
        if cadence_ms_value is not None:
            cadence_times.append(cadence_ms_value)

        gpu_us = frame.get("gpuFrameUs")
        if gpu_us is None:
            gpu_us = counters.get("GPU::FrameUs")
        gpu_source_frame = frame.get("gpuSourceRhiFrame")
        if gpu_source_frame is None:
            gpu_source_frame = counters.get("GPU::SourceRHIFrame")
        if gpu_us is not None:
            gpu_times.append(float(gpu_us) / 1000.0)
            if gpu_source_frame is not None:
                gpu_source_frames.append(int(gpu_source_frame))
                render_frame = frame.get("renderFrame")
                if render_frame is not None:
                    gpu_source_pairs.append((int(render_frame), int(gpu_source_frame)))

        cpu_ms_value = cadence_ms_value if cadence_ms_value is not None else wall_ms_value
        if work_ms > budget_ms or (
            cpu_ms_value is not None and cpu_ms_value > budget_ms
        ):
            spike_frames.append(
                {
                    "renderFrame": frame.get("renderFrame"),
                    "serverTick": server_tick,
                    "workMs": work_ms,
                    "wallMs": wall_ms_value,
                    "cadenceMs": cadence_ms_value,
                    "gpuMs": float(gpu_us) / 1000.0 if gpu_us is not None else None,
                    "gpuSourceRhiFrame": gpu_source_frame,
                }
            )

    top_scopes: list[dict[str, Any]] = []
    for name, samples in scope_samples.items():
        row = {"name": name, **stats(samples), "calls": scope_calls[name]}
        top_scopes.append(row)
    top_scopes.sort(
        key=lambda row: (
            float(row["p95"] or 0.0),
            float(row["max"] or 0.0),
            float(row["mean"] or 0.0),
        ),
        reverse=True,
    )

    work_stats = stats(work_times)
    wall_stats = stats(wall_times)
    cadence_stats = stats(cadence_times)
    gpu_stats = stats(gpu_times)
    profiler_end_frame_stats = stats(profiler_end_frame_times)
    cpu_stats = cadence_stats if cadence_stats["p95"] is not None else wall_stats
    if cpu_stats["p95"] is None:
        cpu_stats = work_stats
    cpu_p95 = cpu_stats["p95"]
    cpu_p99 = cpu_stats["p99"]
    cpu_median = cpu_stats["median"]
    cpu_mean = cpu_stats["mean"]
    gpu_p95 = gpu_stats["p95"]
    gpu_p99 = gpu_stats["p99"]
    gpu_median = gpu_stats["median"]
    last_counters = counter_map(frames[-1]) if frames else {}
    captured_duration_sec = (
        (max(elapsed_samples) - min(elapsed_samples)) / 1_000_000.0
        if len(elapsed_samples) >= 2
        else 0.0
    )
    required_scene_frames = max(1, math.ceil(len(frames) * 0.95))
    required_server_tick_frames = max(2, math.ceil(len(frames) * 0.90))
    minimum_server_tick_span = max(1, math.floor(minimum_duration_sec * 20.0))
    server_tick_monotonic = all(
        later >= earlier for earlier, later in zip(server_ticks, server_ticks[1:])
    )
    server_tick_span = (
        server_ticks[-1] - server_ticks[0] if len(server_ticks) >= 2 else 0
    )
    required_gpu_samples = max(1, math.ceil(len(performance_frames) * 0.90))
    gpu_source_frames_monotonic = all(
        later > earlier
        for earlier, later in zip(gpu_source_frames, gpu_source_frames[1:])
    )
    gpu_source_join_valid = (
        len(gpu_source_pairs) == len(gpu_times)
        and all(source_frame <= readback_frame for readback_frame, source_frame in gpu_source_pairs)
    )
    gpu_join_coverage = (
        len(gpu_source_frames) / len(performance_frames)
        if performance_frames
        else 0.0
    )
    cadence_coverage = (
        len(cadence_times) / len(performance_frames) if performance_frames else 0.0
    )
    cpu_samples_for_hitches = cadence_times or wall_times or work_times
    hitch_count = sum(
        1 for value in cpu_samples_for_hitches if value > hitch_threshold_ms
    )
    effective_fps = 1000.0 / float(cpu_mean) if cpu_mean else 0.0
    scenario_valid = (
        len(frames) > 0
        and in_game_frame_count >= required_scene_frames
        and full_scenario_frame_count >= required_scene_frames
        and config_valid_frame_count >= required_scene_frames
        and server_tick_frame_count >= required_server_tick_frames
        and server_tick_monotonic
        and server_tick_span >= minimum_server_tick_span
        and captured_duration_sec >= minimum_duration_sec
    )
    gate_pass = (
        scenario_valid
        and len(performance_frames) >= 300
        and cadence_coverage >= 0.90
        and cpu_p95 is not None
        and cpu_p95 <= budget_ms
        and cpu_p99 is not None
        and cpu_p99 <= p99_budget_ms
        and cpu_median is not None
        and cpu_median <= median_budget_ms
        and effective_fps >= target_fps
        and gpu_p95 is not None
        and gpu_p95 <= budget_ms
        and gpu_p99 is not None
        and gpu_p99 <= p99_budget_ms
        and gpu_median is not None
        and gpu_median <= median_budget_ms
        and len(gpu_times) >= required_gpu_samples
        and len(gpu_source_frames) == len(gpu_times)
        and gpu_source_frames_monotonic
        and gpu_source_join_valid
        and profiler_end_frame_stats["p95"] is not None
        and float(profiler_end_frame_stats["p95"]) <= profiler_overhead_budget_ms
        and hitch_count == 0
        and dropped_scope_stats == 0
        and dropped_counters == 0
        and dropped_raw_events == 0
    )

    warnings: list[str] = []
    if len(frames) == 1:
        warnings.append("Single-frame capture cannot establish percentile or sustained-FPS claims.")
    if payload.get("schema") == "WintersProfilerCapture.v1":
        warnings.append("v1 capture predates the render-frame/server-tick timeline schema.")
    if dropped_raw_events > 0:
        warnings.append("Raw events were dropped; aggregate scope totals remain the primary evidence.")
    if cadence_coverage < 0.90:
        warnings.append("Frame::CadenceUs coverage is below 90%; profiler overhead is not fully represented.")
    if len(gpu_source_frames) < required_gpu_samples:
        warnings.append("GPU source-frame timing coverage is below the 90% acceptance threshold.")
    if profiler_end_frame_stats["p95"] is None:
        warnings.append("Profiler::PreviousEndFrameUs is missing; profiler overhead cannot be gated.")
    if not scenario_valid:
        warnings.append(
            "Capture is not a valid 1080p normal-F5 network gameplay acceptance scenario."
        )
    warnings.append("DX11 GPU timing is delayed; GPU::SourceRHIFrame preserves its source frame identity.")
    warnings.append(
        "Mesh::DrawCalls is a partial legacy counter until all RHI/direct draw paths share frame stats."
    )

    return {
        "schema": "WintersProfilerAnalysis.v2",
        "capture": str(capture_path.resolve()),
        "captureSchema": payload.get("schema"),
        "targetFps": target_fps,
        "frameBudgetMs": budget_ms,
        "medianBudgetMs": median_budget_ms,
        "p99BudgetMs": p99_budget_ms,
        "hitchThresholdMs": hitch_threshold_ms,
        "profilerOverheadBudgetMs": profiler_overhead_budget_ms,
        "expectedResolution": [expected_width, expected_height],
        "minimumDurationSec": minimum_duration_sec,
        "capturedDurationSec": captured_duration_sec,
        "frameCount": len(frames),
        "performanceSampleCount": len(performance_frames),
        "range": {
            "firstRenderFrame": frames[0].get("renderFrame") if frames else None,
            "lastRenderFrame": frames[-1].get("renderFrame") if frames else None,
            "firstServerTick": frames[0].get("serverTick") if frames else None,
            "lastServerTick": frames[-1].get("serverTick") if frames else None,
        },
        "workFrameMs": work_stats,
        "wallFrameMs": wall_stats,
        "cadenceFrameMs": cadence_stats,
        "gpuFrameMs": gpu_stats,
        "profilerPreviousEndFrameMs": profiler_end_frame_stats,
        "effectiveFps": effective_fps,
        "gpuSourceJoinCoverage": gpu_join_coverage,
        "drops": {
            "scopeStats": dropped_scope_stats,
            "counters": dropped_counters,
            "rawEvents": dropped_raw_events,
        },
        "observedConfig": {
            "targetFps": last_counters.get("Frame::TargetFPS"),
            "limiterActive": last_counters.get("Frame::LimiterActive"),
            "vsyncRequested": last_counters.get("Frame::VSyncRequested"),
            "presentationVsync": last_counters.get("Frame::PresentationVSync"),
            "windowWidth": last_counters.get("Frame::WindowWidth"),
            "windowHeight": last_counters.get("Frame::WindowHeight"),
            "rhiBackend": last_counters.get("RHI::Backend"),
        },
        "gate": {
            "scenarioValid": scenario_valid,
            "inGameFrameCount": in_game_frame_count,
            "fullScenarioFrameCount": full_scenario_frame_count,
            "configValidFrameCount": config_valid_frame_count,
            "serverTickFrameCount": server_tick_frame_count,
            "requiredSceneFrames": required_scene_frames,
            "requiredServerTickFrames": required_server_tick_frames,
            "serverTickMonotonic": server_tick_monotonic,
            "serverTickSpan": server_tick_span,
            "minimumServerTickSpan": minimum_server_tick_span,
            "cadenceCoverage": cadence_coverage,
            "requiredGpuSamples": required_gpu_samples,
            "gpuSourceFramesMonotonic": gpu_source_frames_monotonic,
            "gpuSourceJoinValid": gpu_source_join_valid,
            "requiresCpuP95AtOrBelowBudget": True,
            "requiresGpuP95AtOrBelowBudget": True,
            "requiresMedianHeadroom": True,
            "requiresP99AtOrBelow125PercentBudget": True,
            "requiresNoHitchesAbove120FpsBudget": True,
            "hitchCount": hitch_count,
            "requiresProfilerEndFrameP95AtOrBelowOnePercentBudget": True,
            "requiresNoDroppedRecords": True,
            "pass": gate_pass,
        },
        "topScopesInclusive": top_scopes[:top_count],
        "budgetExceedingFrames": spike_frames[:200],
        "warnings": warnings,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path)
    parser.add_argument("--out", type=Path)
    parser.add_argument("--target-fps", type=float, default=300.0)
    parser.add_argument("--top", type=int, default=30)
    parser.add_argument("--expected-width", type=int, default=1920)
    parser.add_argument("--expected-height", type=int, default=1080)
    parser.add_argument("--minimum-duration-sec", type=float, default=60.0)
    args = parser.parse_args()

    if args.target_fps <= 0:
        parser.error("--target-fps must be positive")
    if args.top <= 0:
        parser.error("--top must be positive")
    if args.expected_width <= 0 or args.expected_height <= 0:
        parser.error("expected resolution must be positive")
    if args.minimum_duration_sec <= 0:
        parser.error("--minimum-duration-sec must be positive")

    with args.capture.open("r", encoding="utf-8-sig") as source:
        payload = json.load(source)

    analysis = build_analysis(
        args.capture,
        payload,
        args.target_fps,
        args.top,
        args.expected_width,
        args.expected_height,
        args.minimum_duration_sec,
    )
    encoded = json.dumps(analysis, ensure_ascii=False, indent=2) + "\n"
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(encoded, encoding="utf-8")
    print(encoded, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
