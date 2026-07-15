from __future__ import annotations

import ctypes
import math
import unittest
from pathlib import Path

import sys


AI_RESEARCH_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(AI_RESEARCH_ROOT))

import AiDecisionTraceCodec as codec  # noqa: E402
import AiEpisodeSchema as schema  # noqa: E402


def MakeValidTrace() -> codec.AiDecisionTraceV1:
    trace = codec.AiDecisionTraceV1()
    trace.schema_version = schema.TraceSchemaVersion
    trace.byte_size = ctypes.sizeof(codec.AiDecisionTraceV1)
    trace.candidate_count = 4
    trace.selected_candidate_kind = 2
    trace.executor_state = schema.ExecutorSubmitted
    trace.command_kind = 3
    trace.command_slot = 1
    trace.tick = 120
    trace.command_target_net_entity_id = 2001
    trace.command_sequence = 17

    observation = trace.observation
    observation.schema_version = schema.ObservationSchemaVersion
    observation.byte_size = ctypes.sizeof(codec.AiObservationV1)
    observation.provenance_flags = schema.ObservationPrivilegedSourceFlag
    observation.fact_tick = trace.tick
    observation.self_net_entity_id = 1001
    observation.enemy_champion_net_entity_id = 2001
    observation.self_level = 3
    observation.enemy_level = 3
    observation.self_hp_ratio = 0.8
    observation.enemy_hp_ratio = 0.5
    observation.enemy_distance = 4.5
    observation.attack_range = 5.5

    action_mask = trace.action_mask
    action_mask.schema_version = schema.ActionSchemaVersion
    action_mask.byte_size = ctypes.sizeof(codec.AiActionMaskV1)
    action_mask.legal_candidate_mask = schema.CandidateFightBit
    action_mask.illegal_candidate_mask = (
        schema.AllCandidateBits & ~schema.CandidateFightBit
    )

    for index, kind in enumerate((1, 2, 3, 4)):
        candidate = trace.candidates[index]
        candidate.schema_version = schema.TraceSchemaVersion
        candidate.byte_size = ctypes.sizeof(codec.AiCandidateEvidenceV1)
        candidate.candidate_kind = kind
        candidate.score = 0.1 * kind
        candidate.contribution_count = 1
        if kind == 2:
            candidate.flags = (
                schema.CandidateLegalFlag
                | schema.CandidateSelectedFlag
                | schema.CandidateHasTargetFlag
            )
            candidate.target_net_entity_id = 2001
        for contribution_index in range(codec.FeatureCapacity):
            contribution = candidate.contributions[contribution_index]
            contribution.schema_version = schema.TraceSchemaVersion
            contribution.byte_size = ctypes.sizeof(
                codec.AiFeatureContributionV1
            )
        candidate.contributions[0].feature_id = 1
        candidate.contributions[0].raw_value = candidate.score
        candidate.contributions[0].weight = 1.0
        candidate.contributions[0].contribution = candidate.score
    return trace


def MakeMetadata() -> dict[str, object]:
    return {
        "episode_id": "codec-unit-test",
        "scenario_id": "one-decision",
        "timeline_epoch": 1,
        "branch_id": 0,
        "seed": 42,
        "rules_hash": "1" * 64,
        "definition_hash": "2" * 64,
        "policy_revision": 1,
    }


class AiDecisionTraceCodecTests(unittest.TestCase):
    def test_pending_privileged_trace_exports_but_is_not_promotable(self) -> None:
        trace = MakeValidTrace()
        decoded = codec.DecodeTraceBytes(bytes(trace))
        record = codec.TraceToEpisodeRecord(
            decoded[0],
            MakeMetadata(),
            {
                "next_state_hash": "1234567890abcdef",
                "reward": 0.0,
                "terminal": False,
                "truncated": False,
            },
        )

        self.assertEqual([], schema.ValidateRecord(record))
        promotion_errors = schema.ValidateRecord(record, promotion=True)
        self.assertTrue(any("privileged" in value for value in promotion_errors))
        self.assertTrue(
            any("final executor result" in value for value in promotion_errors)
        )

    def test_synthetic_final_team_filtered_trace_is_promotable(self) -> None:
        trace = MakeValidTrace()
        trace.observation.provenance_flags = schema.ObservationTeamFilteredFlag
        trace.executor_state = schema.ExecutorAccepted
        record = codec.TraceToEpisodeRecord(
            trace,
            MakeMetadata(),
            {
                "next_state_hash": "1234567890abcdef",
                "reward": 0.25,
                "terminal": False,
                "truncated": False,
            },
        )

        self.assertEqual([], schema.ValidateRecord(record, promotion=True))

    def test_team_filtered_privileged_trace_is_raw_only(self) -> None:
        trace = MakeValidTrace()
        trace.observation.provenance_flags = (
            schema.ObservationTeamFilteredFlag
            | schema.ObservationPrivilegedSourceFlag
        )
        decoded = codec.DecodeTraceBytes(bytes(trace))
        record = codec.TraceToEpisodeRecord(
            decoded[0],
            MakeMetadata(),
            {
                "next_state_hash": "1234567890abcdef",
                "reward": 0.0,
                "terminal": False,
                "truncated": False,
            },
        )

        self.assertEqual([], schema.ValidateRecord(record))
        promotion_errors = schema.ValidateRecord(record, promotion=True)
        self.assertTrue(any("privileged" in value for value in promotion_errors))

    def test_duplicate_trace_identity_is_rejected(self) -> None:
        payload = bytes(MakeValidTrace())

        with self.assertRaisesRegex(codec.TraceCodecError, "duplicates"):
            codec.DecodeTraceBytes(payload + payload)

    def test_non_finite_trace_value_is_rejected(self) -> None:
        trace = MakeValidTrace()
        trace.observation.self_hp_ratio = math.nan

        with self.assertRaisesRegex(codec.TraceCodecError, "must be finite"):
            codec.DecodeTraceBytes(bytes(trace))

    def test_trace_schema_drift_is_rejected(self) -> None:
        trace = MakeValidTrace()
        trace.schema_version = 2

        with self.assertRaisesRegex(codec.TraceCodecError, "schema_version"):
            codec.DecodeTraceBytes(bytes(trace))


if __name__ == "__main__":
    unittest.main()
