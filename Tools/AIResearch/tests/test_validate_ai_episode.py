from __future__ import annotations

import copy
import json
import math
import subprocess
import tempfile
import unittest
from pathlib import Path

import sys


AI_RESEARCH_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(AI_RESEARCH_ROOT))

import AiEpisodeSchema as schema  # noqa: E402
from ValidateAiEpisode import (  # noqa: E402
    DuplicateObjectKeyError,
    NonFiniteJsonConstantError,
    RejectDuplicateObjectKeys,
    RejectNonFiniteJsonConstant,
)


def LoadGolden() -> dict[str, object]:
    fixture = (
        AI_RESEARCH_ROOT
        / "fixtures"
        / "ai_episode_v1_golden.jsonl"
    )
    return json.loads(fixture.read_text(encoding="utf-8"))


class AiEpisodeValidatorTests(unittest.TestCase):
    def test_golden_record_passes_promotion(self) -> None:
        self.assertEqual([], schema.ValidateRecord(LoadGolden(), promotion=True))

    def test_privileged_observation_is_not_promotable(self) -> None:
        record = LoadGolden()
        record["observation"]["provenance_flags"] = (
            schema.ObservationPrivilegedSourceFlag
        )

        errors = schema.ValidateRecord(record, promotion=True)

        self.assertTrue(any("privileged" in value for value in errors))
        self.assertTrue(any("team-filtered" in value for value in errors))

    def test_submitted_command_is_not_promotable(self) -> None:
        record = LoadGolden()
        record["executor"]["state"] = schema.ExecutorSubmitted

        errors = schema.ValidateRecord(record, promotion=True)

        self.assertTrue(any("final executor result" in value for value in errors))

    def test_hidden_enemy_current_facts_are_rejected(self) -> None:
        record = LoadGolden()
        record["observation"]["enemy_champion_net_entity_id"] = 0

        errors = schema.ValidateRecord(record, promotion=True)

        self.assertTrue(any("hidden enemy current fact" in value for value in errors))
        self.assertTrue(any("target is not present" in value for value in errors))

    def test_unobserved_command_target_is_rejected(self) -> None:
        record = LoadGolden()
        record["command"]["target_net_entity_id"] = 9999

        errors = schema.ValidateRecord(record, promotion=True)

        self.assertTrue(any("command target" in value for value in errors))

    def test_selected_candidate_must_be_legal(self) -> None:
        record = LoadGolden()
        record["action_mask"]["legal_candidate_mask"] = 12
        record["action_mask"]["illegal_candidate_mask"] = 3
        record["candidates"][1]["flags"] = schema.CandidateSelectedFlag

        errors = schema.ValidateRecord(record, promotion=True)

        self.assertTrue(any("selected candidate is not legal" in value for value in errors))

    def test_non_finite_values_are_rejected(self) -> None:
        record = LoadGolden()
        record["reward"] = math.nan

        errors = schema.ValidateRecord(record)

        self.assertTrue(any("reward must be a finite number" in value for value in errors))

    def test_semantic_ranges_and_unknown_bits_are_rejected(self) -> None:
        record = LoadGolden()
        record["observation"]["self_hp_ratio"] = 1.25
        record["observation"]["enemy_distance"] = -1.0
        record["observation"]["capability_flags"] = 1 << 20
        record["action_mask"]["available_skill_mask"] = 1 << 12
        record["candidates"][0]["flags"] |= 1 << 7

        errors = schema.ValidateRecord(record, promotion=True)

        self.assertTrue(any("self_hp_ratio" in value for value in errors))
        self.assertTrue(any("enemy_distance" in value for value in errors))
        self.assertTrue(any("capability_flags" in value for value in errors))
        self.assertTrue(any("available_skill_mask" in value for value in errors))
        self.assertTrue(any("unknown V1 bits" in value for value in errors))

    def test_final_executor_reason_and_command_enum_are_consistent(self) -> None:
        accepted = LoadGolden()
        accepted["executor"]["reason"] = 9
        accepted["command"]["kind"] = 99
        accepted_errors = schema.ValidateRecord(accepted, promotion=True)
        self.assertTrue(any("accepted executor" in value for value in accepted_errors))
        self.assertTrue(any("command.kind" in value for value in accepted_errors))

        rejected = LoadGolden()
        rejected["executor"]["state"] = schema.ExecutorRejected
        rejected["executor"]["reason"] = 0
        rejected_errors = schema.ValidateRecord(rejected, promotion=True)
        self.assertTrue(any("rejected executor" in value for value in rejected_errors))

    def test_terminal_and_truncated_are_mutually_exclusive(self) -> None:
        record = LoadGolden()
        record["terminal"] = True
        record["truncated"] = True

        errors = schema.ValidateRecord(record, promotion=True)

        self.assertTrue(any("cannot both be true" in value for value in errors))

    def test_time_limit_truncation_is_promotion_valid(self) -> None:
        record = LoadGolden()
        record["terminal"] = False
        record["truncated"] = True

        self.assertEqual([], schema.ValidateRecord(record, promotion=True))

    def test_duplicate_json_keys_are_rejected(self) -> None:
        with self.assertRaises(DuplicateObjectKeyError):
            json.loads(
                '{"tick":1,"tick":2}',
                object_pairs_hook=RejectDuplicateObjectKeys,
            )

    def test_non_finite_json_constants_are_rejected(self) -> None:
        with self.assertRaises(NonFiniteJsonConstantError):
            json.loads(
                '{"reward":NaN}',
                parse_constant=RejectNonFiniteJsonConstant,
            )

    def test_duplicate_record_identity_is_rejected(self) -> None:
        golden_line = json.dumps(LoadGolden(), separators=(",", ":"))
        with tempfile.TemporaryDirectory() as directory:
            duplicate_path = Path(directory) / "duplicate.jsonl"
            duplicate_path.write_text(
                f"{golden_line}\n{golden_line}\n",
                encoding="utf-8",
            )
            result = subprocess.run(
                [
                    sys.executable,
                    "-B",
                    str(AI_RESEARCH_ROOT / "ValidateAiEpisode.py"),
                    "--input",
                    str(duplicate_path),
                ],
                capture_output=True,
                check=False,
                text=True,
            )

        self.assertNotEqual(0, result.returncode)
        self.assertIn("duplicate record identity", result.stdout)

    def test_boundary_before_final_canonical_record_is_rejected(self) -> None:
        first = LoadGolden()
        first["truncated"] = True
        second = copy.deepcopy(first)
        second["tick"] += 1
        second["observation"]["fact_tick"] = second["tick"]
        second["command"]["sequence"] += 1
        second["next_state_hash"] = "fedcba9876543210"
        second["truncated"] = False

        with tempfile.TemporaryDirectory() as directory:
            episode_path = Path(directory) / "early-boundary.jsonl"
            episode_path.write_text(
                "\n".join(
                    json.dumps(record, separators=(",", ":"))
                    for record in (first, second)
                )
                + "\n",
                encoding="utf-8",
            )
            result = subprocess.run(
                [
                    sys.executable,
                    "-B",
                    str(AI_RESEARCH_ROOT / "ValidateAiEpisode.py"),
                    "--input",
                    str(episode_path),
                ],
                capture_output=True,
                check=False,
                text=True,
            )

        self.assertNotEqual(0, result.returncode)
        self.assertIn("not the final canonical record", result.stdout)

    def test_trace_schema_is_required(self) -> None:
        record = LoadGolden()
        record["trace_schema_version"] = 2

        errors = schema.ValidateRecord(record)

        self.assertTrue(any("trace_schema_version" in value for value in errors))

    def test_unknown_fields_are_rejected(self) -> None:
        record = copy.deepcopy(LoadGolden())
        record["observation"]["hidden_enemy_position"] = [1, 2, 3]

        errors = schema.ValidateRecord(record)

        self.assertTrue(any("unknown field" in value for value in errors))


if __name__ == "__main__":
    unittest.main()
