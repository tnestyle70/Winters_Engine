from __future__ import annotations

import ast
import contextlib
import copy
import hashlib
import io
import json
import math
import struct
import tempfile
import unittest
from pathlib import Path
import sys

import numpy as np


AI_RESEARCH_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(AI_RESEARCH_ROOT))

import AiEpisodeSchema as schema  # noqa: E402
import TrainImitationRankingBaseline as baseline  # noqa: E402
import MaterializeImitationDataset as materializer  # noqa: E402


Fixture = (
    AI_RESEARCH_ROOT
    / "fixtures"
    / "imitation_ranking_v1_golden.jsonl"
)
CorrectionFixture = (
    AI_RESEARCH_ROOT
    / "fixtures"
    / "ai_decision_correction_sidecar_v1_golden.json"
)
FixtureConfig = baseline.TrainingConfig(minimum_groups=8)


def WriteJsonl(
    directory: str,
    name: str,
    records: list[dict[str, object]],
) -> Path:
    path = Path(directory) / name
    path.write_text(
        "".join(
            json.dumps(
                record,
                ensure_ascii=False,
                allow_nan=False,
                separators=(",", ":"),
            )
            + "\n"
            for record in records
        ),
        encoding="utf-8",
    )
    return path


def SelectOnly(record: dict[str, object], selected_kind: int) -> None:
    record["selected_candidate_kind"] = selected_kind
    for candidate in record["candidates"]:
        candidate["flags"] &= ~schema.CandidateSelectedFlag
        if candidate["kind"] == selected_kind:
            candidate["flags"] |= schema.CandidateSelectedFlag
            record["command"]["slot"] = selected_kind
            record["command"]["target_net_entity_id"] = candidate[
                "target_net_entity_id"
            ]


class ImitationRankingBaselineTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.loaded = baseline.LoadPromotionEpisodes(Fixture)

    def test_contract_fixture_builds_without_runtime_promotion_claim(self) -> None:
        artifact = baseline.BuildArtifact(
            self.loaded,
            FixtureConfig,
            baseline.FixtureEvidenceKind,
        )

        self.assertEqual(baseline.ArtifactType, artifact["artifact_type"])
        self.assertEqual(
            "CONTRACT_ONLY_NOT_MEASURED",
            artifact["performance_claim"],
        )
        self.assertFalse(artifact["eligible_for_runtime_promotion"])
        self.assertFalse(artifact["policy_state_modified"])
        self.assertFalse(artifact["reinforcement_learning_used"])
        self.assertFalse(artifact["python_transition_model_used"])
        self.assertFalse(artifact["pytorch_used"])
        self.assertTrue(artifact["source"]["promotion_validation_required"])

    def test_repeated_build_has_identical_artifact_hash(self) -> None:
        first = baseline.BuildArtifact(
            self.loaded,
            FixtureConfig,
            baseline.FixtureEvidenceKind,
        )
        second = baseline.BuildArtifact(
            self.loaded,
            FixtureConfig,
            baseline.FixtureEvidenceKind,
        )

        self.assertEqual(first, second)
        self.assertEqual(first["artifact_sha256"], second["artifact_sha256"])
        self.assertEqual(
            first["artifact_sha256"],
            baseline.ComputeArtifactSha256(first),
        )

    def test_frozen_scenario_split_has_zero_leakage(self) -> None:
        artifact = baseline.BuildArtifact(
            self.loaded,
            FixtureConfig,
            baseline.FixtureEvidenceKind,
        )
        train = {tuple(group) for group in artifact["split"]["train_groups"]}
        holdout = {
            tuple(group) for group in artifact["split"]["holdout_groups"]
        }

        self.assertFalse(train & holdout)
        self.assertEqual(0, artifact["split"]["group_overlap_count"])
        self.assertEqual(8, len(train | holdout))

        repeated_episode = copy.deepcopy(self.loaded.records[0])
        repeated_episode["episode_id"] = "repeat-of-same-frozen-scenario"
        examples = baseline.BuildDecisionExamples(
            [*self.loaded.records, repeated_episode]
        )
        scenario_groups = {example.split_group for example in examples}
        self.assertEqual(8, len(scenario_groups))

    def test_forbidden_authored_fields_do_not_change_feature_vector(self) -> None:
        original = copy.deepcopy(self.loaded.records[1])
        changed = copy.deepcopy(original)

        changed["selected_candidate_kind"] = 4
        changed["command"] = {
            "kind": 99,
            "slot": 99,
            "target_net_entity_id": 0,
            "sequence": 999999,
            "position": [99.0, 98.0, 97.0],
        }
        changed["executor"] = {"state": schema.ExecutorRejected, "reason": 999}
        changed["next_state_hash"] = "ffffffffffffffff"
        changed["reward"] = -999.0
        changed["terminal"] = not changed["terminal"]
        changed["truncated"] = not changed["terminal"]
        changed["observation"]["provenance_flags"] = 999
        changed["observation"]["fact_tick"] = 999999

        remapped_ids: dict[int, int] = {}
        for field in (
            "self_net_entity_id",
            "enemy_champion_net_entity_id",
            "enemy_minion_net_entity_id",
            "enemy_structure_net_entity_id",
            "allied_wave_net_entity_id",
        ):
            old_value = changed["observation"][field]
            remapped_ids[old_value] = old_value + 100000
            changed["observation"][field] = remapped_ids[old_value]
        for candidate in changed["candidates"]:
            target = candidate["target_net_entity_id"]
            if target in remapped_ids:
                candidate["target_net_entity_id"] = remapped_ids[target]

        original_candidate = next(
            candidate
            for candidate in original["candidates"]
            if candidate["kind"] == 2
        )
        changed_candidate = next(
            candidate
            for candidate in changed["candidates"]
            if candidate["kind"] == 2
        )
        changed_candidate["flags"] ^= schema.CandidateSelectedFlag
        changed_candidate["score"] = 123456.0
        changed_candidate["contributions"] = [
            {
                "feature_id": 999,
                "raw_value": -500.0,
                "weight": 400.0,
                "contribution": -200000.0,
            }
        ]

        np.testing.assert_array_equal(
            baseline.BuildFeatureVector(original, original_candidate),
            baseline.BuildFeatureVector(changed, changed_candidate),
        )

    def test_authored_scores_and_rewards_do_not_train_the_model(self) -> None:
        changed_records = copy.deepcopy(self.loaded.records)
        for index, record in enumerate(changed_records):
            record["reward"] = 1000.0 + index
            record["terminal"] = not record["terminal"]
            record["truncated"] = not record["terminal"]
            record["next_state_hash"] = f"{1000 + index:016x}"
            for candidate in record["candidates"]:
                candidate["score"] = 10000.0 + candidate["kind"] * index
                candidate["contributions"] = [
                    {
                        "feature_id": 1,
                        "raw_value": 50.0 + index,
                        "weight": -20.0,
                        "contribution": -1000.0 - index,
                    }
                ]

        with tempfile.TemporaryDirectory() as directory:
            changed_path = WriteJsonl(
                directory,
                "changed.jsonl",
                changed_records,
            )
            changed_loaded = baseline.LoadPromotionEpisodes(changed_path)
            changed_artifact = baseline.BuildArtifact(
                changed_loaded,
                FixtureConfig,
                baseline.FixtureEvidenceKind,
            )
        original_artifact = baseline.BuildArtifact(
            self.loaded,
            FixtureConfig,
            baseline.FixtureEvidenceKind,
        )

        self.assertEqual(original_artifact["features"], changed_artifact["features"])
        self.assertEqual(original_artifact["weights"], changed_artifact["weights"])
        self.assertEqual(original_artifact["metrics"], changed_artifact["metrics"])

    def test_small_dataset_fails_closed(self) -> None:
        small = baseline.LoadedEpisodes(
            records=self.loaded.records[:3],
            input_sha256=self.loaded.input_sha256,
        )

        with self.assertRaisesRegex(baseline.TrainingDataError, "requires 8"):
            baseline.BuildArtifact(
                small,
                FixtureConfig,
                baseline.FixtureEvidenceKind,
            )

    def test_episode_boundary_before_final_record_fails_closed(self) -> None:
        records = copy.deepcopy(self.loaded.records[:2])
        for index, record in enumerate(records):
            record["episode_id"] = "shared-episode"
            record["scenario_id"] = "shared-scenario"
            record["timeline_epoch"] = 1
            record["branch_id"] = 0
            record["tick"] = index + 1
            record["observation"]["fact_tick"] = index + 1
            record["command"]["sequence"] = index + 1
            record["terminal"] = False
            record["truncated"] = index == 0

        with tempfile.TemporaryDirectory() as directory:
            path = WriteJsonl(directory, "early-boundary.jsonl", records)
            with self.assertRaisesRegex(
                baseline.TrainingDataError,
                "not the final canonical record",
            ):
                baseline.LoadPromotionEpisodes(path)

    def test_single_selected_class_fails_closed(self) -> None:
        records = copy.deepcopy(self.loaded.records)
        for record in records:
            SelectOnly(record, 1)
            self.assertEqual([], schema.ValidateRecord(record, promotion=True))

        with tempfile.TemporaryDirectory() as directory:
            path = WriteJsonl(directory, "single-class.jsonl", records)
            loaded = baseline.LoadPromotionEpisodes(path)
            with self.assertRaisesRegex(
                baseline.TrainingDataError,
                "single selected candidate class",
            ):
                baseline.BuildArtifact(
                    loaded,
                    FixtureConfig,
                    baseline.FixtureEvidenceKind,
                )

    def test_non_finite_input_and_configuration_fail_closed(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "nan.jsonl"
            path.write_text('{"reward":NaN}\n', encoding="utf-8")
            with self.assertRaises(baseline.TrainingDataError):
                baseline.LoadPromotionEpisodes(path)

        records = copy.deepcopy(self.loaded.records)
        records[0]["observation"]["self_gold"] = 10**1000
        with tempfile.TemporaryDirectory() as directory:
            path = WriteJsonl(directory, "overflow.jsonl", records)
            with self.assertRaises(baseline.TrainingDataError):
                baseline.LoadPromotionEpisodes(path)

        errors = baseline.TrainingConfig(holdout_fraction=math.nan).Validate()
        self.assertTrue(any("finite" in error for error in errors))

    def test_privileged_and_pending_records_fail_closed(self) -> None:
        cases = (
            ("privileged", "observation", "provenance_flags", 1),
            ("pending", "executor", "state", schema.ExecutorSubmitted),
        )
        for name, parent, field, value in cases:
            with self.subTest(name=name), tempfile.TemporaryDirectory() as directory:
                records = copy.deepcopy(self.loaded.records)
                records[0][parent][field] = value
                path = WriteJsonl(directory, f"{name}.jsonl", records)
                with self.assertRaisesRegex(
                    baseline.TrainingDataError,
                    "not promotion-valid",
                ):
                    baseline.LoadPromotionEpisodes(path)

    def test_rejected_executor_result_is_not_an_expert_label(self) -> None:
        records = copy.deepcopy(self.loaded.records)
        records[0]["executor"] = {
            "state": schema.ExecutorRejected,
            "reason": 17,
        }
        self.assertEqual([], schema.ValidateRecord(records[0], promotion=True))

        with tempfile.TemporaryDirectory() as directory:
            path = WriteJsonl(directory, "rejected.jsonl", records)
            with self.assertRaisesRegex(
                baseline.TrainingDataError,
                "expert label requires an accepted executor result",
            ):
                baseline.LoadPromotionEpisodes(path)

        with self.assertRaisesRegex(
            baseline.TrainingDataError,
            "expert label requires an accepted executor result",
        ):
            baseline.BuildDecisionExamples(records)

    def test_duplicate_decision_group_fails_closed(self) -> None:
        first = copy.deepcopy(self.loaded.records[0])
        duplicate = copy.deepcopy(first)
        duplicate["command"]["sequence"] += 1000
        duplicate["next_state_hash"] = "ffffffffffffffff"

        with tempfile.TemporaryDirectory() as directory:
            path = WriteJsonl(directory, "duplicate.jsonl", [first, duplicate])
            with self.assertRaisesRegex(
                baseline.TrainingDataError,
                "duplicate decision group",
            ):
                baseline.LoadPromotionEpisodes(path)

    def test_artifact_dimensions_metrics_and_canonical_file_match(self) -> None:
        artifact = baseline.BuildArtifact(
            self.loaded,
            FixtureConfig,
            baseline.FixtureEvidenceKind,
        )
        feature_count = len(artifact["features"]["order"])

        self.assertEqual(feature_count, len(artifact["features"]["normalization_mean"]))
        self.assertEqual(feature_count, len(artifact["features"]["normalization_scale"]))
        self.assertEqual(feature_count, len(artifact["weights"]))
        for split in ("train", "holdout"):
            self.assertIn("top1_accuracy", artifact["metrics"][split])
            self.assertIn("top3_accuracy", artifact["metrics"][split])
            self.assertIn(
                "normalized_selected_rank_regret",
                artifact["metrics"][split],
            )

        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "artifact.json"
            baseline.WriteArtifact(path, artifact)
            raw = path.read_bytes()
        self.assertEqual(baseline._CanonicalJsonBytes(artifact) + b"\n", raw)

    def test_cli_writes_contract_only_artifact(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "artifact.json"
            with contextlib.redirect_stdout(io.StringIO()):
                exit_code = baseline.Main(
                    [
                        "--input",
                        str(Fixture),
                        "--output",
                        str(output),
                        "--minimum-groups",
                        "8",
                        "--fixture-contract",
                    ]
                )
            artifact = json.loads(output.read_text(encoding="utf-8"))

        self.assertEqual(0, exit_code)
        self.assertEqual(
            baseline.FixtureEvidenceKind,
            artifact["evidence_kind"],
        )
        self.assertFalse(artifact["eligible_for_runtime_promotion"])

    def test_imports_have_no_torch_or_python_transition_runtime(self) -> None:
        source = (AI_RESEARCH_ROOT / "TrainImitationRankingBaseline.py").read_text(
            encoding="utf-8"
        )
        tree = ast.parse(source)
        module_imported_roots: set[str] = set()
        for node in tree.body:
            if isinstance(node, ast.Import):
                module_imported_roots.update(
                    alias.name.split(".")[0] for alias in node.names
                )
            elif isinstance(node, ast.ImportFrom) and node.module:
                module_imported_roots.add(node.module.split(".")[0])

        self.assertNotIn("torch", module_imported_roots)

        imported_roots: set[str] = set()
        for node in ast.walk(tree):
            if isinstance(node, ast.Import):
                imported_roots.update(
                    alias.name.split(".")[0] for alias in node.names
                )
            elif isinstance(node, ast.ImportFrom) and node.module:
                imported_roots.add(node.module.split(".")[0])
        self.assertFalse(
            imported_roots
            & {"Client", "Engine", "GameSim", "Server", "Shared"}
        )


class PyTorchMaskedBehaviorCloningTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.loaded = baseline.LoadPromotionEpisodes(Fixture)
        cls.runtime_artifact = baseline.BuildTorchPolicyArtifact(
            cls.loaded,
            FixtureConfig,
            baseline.FixtureEvidenceKind,
            "s022-contract",
            8,
        )

    def test_contract_shape_split_and_safety_claims(self) -> None:
        report = self.runtime_artifact.report
        examples = baseline.BuildMaskedBCExamples(self.loaded.records)
        self.assertEqual(8, len(examples))
        self.assertTrue(
            all(example.features.shape == (4, 67) for example in examples)
        )
        self.assertEqual(baseline.PolicyArtifactType, report["artifact_type"])
        self.assertEqual("CONTRACT_ONLY_NOT_MEASURED", report["performance_claim"])
        self.assertEqual(8, report["source"]["record_count"])
        self.assertEqual(8, report["source"]["decision_group_count"])
        self.assertEqual(6, len(report["split"]["train_groups"]))
        self.assertEqual(2, len(report["split"]["holdout_groups"]))
        self.assertEqual(0, report["split"]["group_overlap_count"])
        self.assertEqual(67, len(report["features"]["order"]))
        self.assertEqual(67, len(report["features"]["normalization_mean"]))
        self.assertEqual(
            67,
            len(report["features"]["normalization_inverse_scale"]),
        )
        self.assertEqual(67, len(report["model"]["weight"]))
        self.assertEqual("SHADOW_ONLY", report["safety"]["runtime_mode"])
        self.assertTrue(report["safety"]["pytorch_used"])
        self.assertFalse(report["safety"]["reinforcement_learning_used"])
        self.assertFalse(report["safety"]["eligible_for_runtime_promotion"])
        self.assertFalse(report["safety"]["policy_state_modified"])
        self.assertEqual(
            report["policy_sha256"],
            baseline.ComputePolicySha256(report),
        )
        self.assertEqual(860, len(self.runtime_artifact.binary_bytes))
        self.assertEqual(
            self.runtime_artifact.binary_sha256,
            hashlib.sha256(self.runtime_artifact.binary_bytes).hexdigest(),
        )
        baseline.ValidateTorchPolicyArtifact(
            report,
            self.runtime_artifact.binary_bytes,
        )

    def test_repeated_training_has_identical_report_and_binary_bytes(self) -> None:
        repeated = baseline.BuildTorchPolicyArtifact(
            self.loaded,
            FixtureConfig,
            baseline.FixtureEvidenceKind,
            "s022-contract",
            8,
        )

        self.assertEqual(self.runtime_artifact.report, repeated.report)
        self.assertEqual(
            self.runtime_artifact.binary_bytes,
            repeated.binary_bytes,
        )
        self.assertEqual(
            self.runtime_artifact.binary_sha256,
            repeated.binary_sha256,
        )

    def test_materialized_human_correction_is_explicit_and_cli_trainable(
        self,
    ) -> None:
        rows = materializer.BuildMaterializedRows(Fixture, CorrectionFixture)
        with tempfile.TemporaryDirectory() as directory:
            dataset_path = Path(directory) / "imitation-decision-v1.jsonl"
            report_path = Path(directory) / "policy.json"
            binary_path = Path(directory) / "policy.wbc"
            materializer.WriteCanonicalJsonl(rows, dataset_path)
            loaded = baseline.LoadTrainingEpisodes(
                dataset_path,
                baseline.ImitationDecisionInputContract,
            )
            examples = baseline.BuildMaskedBCExamples(
                loaded.records,
                loaded.expert_candidate_kinds,
            )
            with contextlib.redirect_stdout(io.StringIO()):
                return_code = baseline.Main(
                    [
                        "--backend",
                        "pytorch-masked-bc",
                        "--input",
                        str(dataset_path),
                        "--input-contract",
                        baseline.ImitationDecisionInputContract,
                        "--output",
                        str(report_path),
                        "--runtime-output",
                        str(binary_path),
                        "--policy-id",
                        "s022-correction-contract",
                        "--policy-revision",
                        "8",
                        "--minimum-groups",
                        "8",
                        "--fixture-contract",
                    ]
                )
            report = json.loads(report_path.read_text(encoding="utf-8"))
            binary = binary_path.read_bytes()

        corrected_record = next(
            record
            for record in loaded.records
            if record["episode_id"] == "fixture-episode-02"
        )
        corrected_example = next(
            example
            for example in examples
            if example.decision_group[0] == "fixture-episode-02"
        )
        original_example = baseline.BuildMaskedBCExamples([corrected_record])[0]
        self.assertEqual(0, return_code)
        self.assertEqual(3, corrected_record["selected_candidate_kind"])
        self.assertEqual(1, corrected_example.selected_index)
        self.assertEqual(2, original_example.selected_index)
        np.testing.assert_array_equal(
            corrected_example.features,
            original_example.features,
        )
        self.assertEqual(
            baseline.ImitationDecisionInputContract,
            report["source"]["input_contract"],
        )
        self.assertEqual(
            {
                "HUMAN_DEBUG_CORRECTION": 1,
                "SOURCE_ACCEPTED": 7,
            },
            report["source"]["label_origin_counts"],
        )
        self.assertEqual(
            1,
            report["source"]["unexecuted_counterfactual_label_count"],
        )
        self.assertEqual("SHADOW_ONLY", report["safety"]["runtime_mode"])
        self.assertFalse(report["safety"]["eligible_for_runtime_promotion"])
        baseline.ValidateTorchPolicyArtifact(report, binary)

    def test_illegal_candidate_is_excluded_and_low_kind_breaks_ties(self) -> None:
        records = [copy.deepcopy(self.loaded.records[0])]
        illegal_kind = 4
        bit = schema.CandidateKindToBit[illegal_kind]
        records[0]["action_mask"]["legal_candidate_mask"] &= ~bit
        records[0]["action_mask"]["illegal_candidate_mask"] |= bit
        candidate = next(
            item
            for item in records[0]["candidates"]
            if item["kind"] == illegal_kind
        )
        candidate["flags"] &= ~schema.CandidateLegalFlag

        example = baseline.BuildMaskedBCExamples(records)[0]
        weights = np.zeros(len(baseline.FeatureOrder), dtype=np.float64)
        weights[baseline.FeatureOrder.index("candidate_kind_4")] = 100.0
        ranked, logits = baseline._RankMaskedExample(
            example,
            np.zeros(len(baseline.FeatureOrder), dtype=np.float64),
            np.ones(len(baseline.FeatureOrder), dtype=np.float64),
            weights,
        )

        self.assertEqual([0, 1, 2], ranked)
        self.assertTrue(math.isinf(logits[3]) and logits[3] < 0.0)
        self.assertEqual(1, baseline.RuntimePolicyCandidateOrder[ranked[0]])

    def test_malformed_expert_and_mask_inputs_fail_closed(self) -> None:
        selected_illegal = copy.deepcopy(self.loaded.records[0])
        selected_bit = schema.CandidateKindToBit[
            selected_illegal["selected_candidate_kind"]
        ]
        selected_illegal["action_mask"]["legal_candidate_mask"] &= ~selected_bit
        selected_illegal["action_mask"]["illegal_candidate_mask"] |= selected_bit
        selected_candidate = next(
            candidate
            for candidate in selected_illegal["candidates"]
            if candidate["kind"] == selected_illegal["selected_candidate_kind"]
        )
        selected_candidate["flags"] &= ~schema.CandidateLegalFlag
        with self.assertRaisesRegex(baseline.TrainingDataError, "selected.*illegal"):
            baseline.BuildMaskedBCExamples([selected_illegal])

        one_legal = copy.deepcopy(self.loaded.records[0])
        one_legal["action_mask"]["legal_candidate_mask"] = 1
        one_legal["action_mask"]["illegal_candidate_mask"] = 14
        for candidate in one_legal["candidates"]:
            if candidate["kind"] != 1:
                candidate["flags"] &= ~schema.CandidateLegalFlag
        with self.assertRaisesRegex(baseline.TrainingDataError, "at least two"):
            baseline.BuildMaskedBCExamples([one_legal])

        for executor_state in (
            schema.ExecutorSubmitted,
            schema.ExecutorRejected,
        ):
            with self.subTest(executor_state=executor_state):
                unfinished = copy.deepcopy(self.loaded.records[0])
                unfinished["executor"]["state"] = executor_state
                with self.assertRaisesRegex(
                    baseline.TrainingDataError,
                    "accepted executor result",
                ):
                    baseline.BuildMaskedBCExamples([unfinished])

    def test_normalizer_and_weights_use_train_legal_rows_only(self) -> None:
        original_examples = baseline.BuildMaskedBCExamples(self.loaded.records)
        groups = {example.split_group for example in original_examples}
        train_groups, holdout_groups = baseline.SplitGroups(groups, FixtureConfig)
        original_mean, original_inverse_scale = baseline.FitMaskedNormalizer(
            original_examples,
            train_groups,
        )

        changed_records = copy.deepcopy(self.loaded.records)
        for index, record in enumerate(changed_records):
            group = (
                record["scenario_id"],
                record["rules_hash"],
                record["definition_hash"],
            )
            if group in holdout_groups:
                record["observation"]["self_gold"] += 100000 + index
                record["observation"]["enemy_distance"] += 1000.0 + index
        changed_examples = baseline.BuildMaskedBCExamples(changed_records)
        changed_mean, changed_inverse_scale = baseline.FitMaskedNormalizer(
            changed_examples,
            train_groups,
        )

        np.testing.assert_array_equal(original_mean, changed_mean)
        np.testing.assert_array_equal(
            original_inverse_scale,
            changed_inverse_scale,
        )
        original_weight, _, _ = baseline.TrainTorchMaskedBC(
            original_examples,
            train_groups,
            original_mean,
            original_inverse_scale,
            FixtureConfig,
        )
        changed_weight, _, _ = baseline.TrainTorchMaskedBC(
            changed_examples,
            train_groups,
            changed_mean,
            changed_inverse_scale,
            FixtureConfig,
        )
        np.testing.assert_array_equal(original_weight, changed_weight)

    def test_runtime_binary_rejects_corruption(self) -> None:
        original = self.runtime_artifact.binary_bytes

        def Mutate(offset: int, format_string: str, value: object) -> bytes:
            changed = bytearray(original)
            struct.pack_into(format_string, changed, offset, value)
            return bytes(changed)

        corruptions = {
            "magic": b"X" + original[1:],
            "version": Mutate(8, "<H", 2),
            "size": Mutate(12, "<I", len(original) - 1),
            "trace_schema": Mutate(16, "<H", 2),
            "order_hash": Mutate(44, "<Q", 0),
            "nan_mean": Mutate(56, "<f", math.nan),
            "zero_inverse_scale": Mutate(56 + 67 * 4, "<f", 0.0),
            "infinite_weight": Mutate(56 + 67 * 8, "<f", math.inf),
            "truncated": original[:-1],
            "trailing": original + b"\0",
        }
        for name, binary in corruptions.items():
            with self.subTest(name=name), self.assertRaises(
                baseline.TrainingDataError
            ):
                baseline.ValidateRuntimePolicyBinary(binary)

    def test_unsafe_policy_claims_are_rejected_even_with_resealed_sha(self) -> None:
        changes = (
            ("runtime_mode", "ACTIVE"),
            ("reinforcement_learning_used", True),
            ("eligible_for_runtime_promotion", True),
            ("policy_state_modified", True),
        )
        for field, value in changes:
            with self.subTest(field=field):
                changed = copy.deepcopy(self.runtime_artifact.report)
                changed["safety"][field] = value
                changed["policy_sha256"] = baseline.ComputePolicySha256(changed)
                with self.assertRaisesRegex(
                    baseline.TrainingDataError,
                    "safety claims",
                ):
                    baseline.ValidateTorchPolicyArtifact(
                        changed,
                        self.runtime_artifact.binary_bytes,
                    )

    def test_cli_writes_canonical_report_and_fixed_binary(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "policy.json"
            runtime_output = Path(directory) / "policy.wbc"
            with contextlib.redirect_stdout(io.StringIO()):
                exit_code = baseline.Main(
                    [
                        "--backend",
                        "pytorch-masked-bc",
                        "--input",
                        str(Fixture),
                        "--output",
                        str(output),
                        "--runtime-output",
                        str(runtime_output),
                        "--policy-id",
                        "s022-contract-cli",
                        "--policy-revision",
                        "8",
                        "--minimum-groups",
                        "8",
                        "--fixture-contract",
                    ]
                )
            report = json.loads(output.read_text(encoding="utf-8"))
            report_bytes = output.read_bytes()
            binary = runtime_output.read_bytes()

        self.assertEqual(0, exit_code)
        self.assertEqual(
            baseline._CanonicalJsonBytes(report) + b"\n",
            report_bytes,
        )
        self.assertEqual(860, len(binary))
        baseline.ValidateTorchPolicyArtifact(report, binary)

    def test_policy_revision_must_advance_source(self) -> None:
        with self.assertRaisesRegex(
            baseline.TrainingDataError,
            "greater than source",
        ):
            baseline.BuildTorchPolicyArtifact(
                self.loaded,
                FixtureConfig,
                baseline.FixtureEvidenceKind,
                "s022-contract",
                7,
            )


if __name__ == "__main__":
    unittest.main()
