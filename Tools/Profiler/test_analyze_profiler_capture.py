#!/usr/bin/env python3
"""Regression tests for Winters profiler acceptance-gate semantics."""

from __future__ import annotations

import unittest
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from analyze_profiler_capture import build_analysis


REQUIRED_SCOPES = (
    "Frame",
    "Scene_InGame::OnUpdate",
    "Scene_InGame::OnRender",
    "Map::Render",
    "Champion::Render",
    "Minion::Render",
    "UIOverlay::Render",
)


def make_capture(*, sparse_gpu: bool = False, omit_full_scene: bool = False) -> dict:
    frames = []
    frame_count = 301
    for index in range(frame_count):
        elapsed_us = int(index * 60_000_000 / (frame_count - 1))
        scopes = [
            {"name": name, "totalMs": 2.0, "calls": 1}
            for name in REQUIRED_SCOPES
            if not (omit_full_scene and name == "Minion::Render")
        ]
        gpu_present = not sparse_gpu or index == 0
        frames.append(
            {
                "renderFrame": index + 1,
                "frameStrideSample": True,
                "serverTickSample": True,
                "serverTick": 1000 + index * 4,
                "frameMs": 2.0,
                "gpuFrameUs": 2400 if gpu_present else None,
                "gpuSourceRhiFrame": index + 1 if gpu_present else None,
                "droppedScopeStats": 0,
                "droppedCounters": 0,
                "droppedRawEvents": 0,
                "scopes": scopes,
                "counters": [
                    {"name": "Frame::CadenceUs", "value": 2500},
                    {"name": "Frame::WallUs", "value": 2000},
                    {"name": "Frame::RunElapsedUs", "value": elapsed_us},
                    {"name": "Profiler::PreviousEndFrameUs", "value": 20},
                    {"name": "Frame::LimiterActive", "value": 0},
                    {"name": "Frame::PresentationVSync", "value": 0},
                    {"name": "Frame::WindowWidth", "value": 1920},
                    {"name": "Frame::WindowHeight", "value": 1080},
                    {"name": "RHI::Backend", "value": 0},
                    {"name": "Net::LatestServerTick", "value": 1000 + index * 4},
                ],
                "rawEvents": [],
            }
        )
    return {"schema": "WintersProfilerTimeline.v3", "frames": frames}


class ProfilerGateTests(unittest.TestCase):
    def analyze(self, payload: dict) -> dict:
        return build_analysis(
            Path("synthetic_profiler.json"),
            payload,
            target_fps=300.0,
            top_count=10,
            expected_width=1920,
            expected_height=1080,
            minimum_duration_sec=60.0,
        )

    def test_valid_synthetic_capture_passes(self) -> None:
        analysis = self.analyze(make_capture())
        self.assertTrue(analysis["gate"]["scenarioValid"])
        self.assertTrue(analysis["gate"]["pass"])

    def test_v2_timeline_remains_accepted(self) -> None:
        payload = make_capture()
        payload["schema"] = "WintersProfilerTimeline.v2"
        analysis = self.analyze(payload)
        self.assertEqual(analysis["captureSchema"], "WintersProfilerTimeline.v2")
        self.assertTrue(analysis["gate"]["scenarioValid"])
        self.assertTrue(analysis["gate"]["pass"])

    def test_sparse_gpu_samples_cannot_pass(self) -> None:
        analysis = self.analyze(make_capture(sparse_gpu=True))
        self.assertFalse(analysis["gate"]["pass"])
        self.assertLess(analysis["gpuSourceJoinCoverage"], 0.90)

    def test_missing_full_system_scope_invalidates_scenario(self) -> None:
        analysis = self.analyze(make_capture(omit_full_scene=True))
        self.assertFalse(analysis["gate"]["scenarioValid"])
        self.assertFalse(analysis["gate"]["pass"])


if __name__ == "__main__":
    unittest.main()
