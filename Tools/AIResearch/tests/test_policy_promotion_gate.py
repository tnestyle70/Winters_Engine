from __future__ import annotations

import contextlib
import io
import json
import tempfile
import unittest
from pathlib import Path
import sys


AI_RESEARCH_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(AI_RESEARCH_ROOT))

import PolicyPromotionGate as gate  # noqa: E402


PassFixture = (
    AI_RESEARCH_ROOT / "fixtures" / "policy_promotion_gate_v1_pass.json"
)
FailFixture = (
    AI_RESEARCH_ROOT / "fixtures" / "policy_promotion_gate_v1_fail.json"
)
FixtureConfig = gate.PolicyGateConfig(
    minimum_samples=2,
    minimum_mirrored_pairs=2,
    minimum_repeat_count=2,
    confidence_level=0.95,
)


def LoadFixture(path: Path) -> dict[str, object]:
    return json.loads(path.read_text(encoding="utf-8"))


def Reseal(report: dict[str, object]) -> dict[str, object]:
    return gate.SealReport(report)


def DeclareExternalAttestation(report: dict[str, object]) -> None:
    report["attestation"] = {
        "status": gate.ExternalAttestedStatus,
        "candidate_artifact_sha256": report["candidate"]["policy_sha256"],
        "runner_artifact_sha256": "a" * 64,
        "attestation_sha256": "b" * 64,
        "attested_by": "fixture-ci-attestor",
    }


def RecomputeSummary(
    report: dict[str, object],
    config: gate.PolicyGateConfig = FixtureConfig,
) -> None:
    groups = gate._GroupRuns(report["runs"])
    representatives = gate._CollectRepresentatives(groups)
    _, mirrored_pairs = gate._CheckMirroredCoverage(
        representatives,
        config,
    )
    report["summary"] = gate._ComputeMetrics(
        report["runs"],
        mirrored_pairs,
        config.confidence_level,
    )


class PolicyPromotionGateTests(unittest.TestCase):
    def test_golden_pass_proves_contract_without_promotion_claim(self) -> None:
        result = gate.EvaluateReport(LoadFixture(PassFixture), FixtureConfig)

        self.assertTrue(result["gate_passed"])
        self.assertFalse(result["eligible_for_policy_review"])
        self.assertFalse(result["promotion_performed"])
        self.assertFalse(result["policy_state_modified"])
        self.assertEqual("CONTRACT_FIXTURE_PASSED", result["recommendation"])
        self.assertTrue(all(row["passed"] for row in result["checks"].values()))

    def test_default_thresholds_reject_the_small_contract_fixture(self) -> None:
        result = gate.EvaluateReport(LoadFixture(PassFixture))

        self.assertFalse(result["gate_passed"])
        self.assertFalse(
            result["checks"]["mirrored_side_coverage"]["passed"]
        )
        self.assertFalse(
            result["checks"]["confidence_interval_and_sample_count"]["passed"]
        )

    def test_measured_evidence_can_only_become_review_eligible(self) -> None:
        report = LoadFixture(PassFixture)
        report["evidence_kind"] = gate.MeasuredEvidenceKind
        DeclareExternalAttestation(report)
        Reseal(report)

        result = gate.EvaluateReport(report, FixtureConfig)

        self.assertTrue(result["gate_passed"])
        self.assertTrue(result["eligible_for_policy_review"])
        self.assertEqual("EVIDENCE_PASSED_REVIEW_ONLY", result["recommendation"])
        self.assertFalse(result["promotion_performed"])
        self.assertFalse(result["policy_state_modified"])
        self.assertTrue(result["provenance"]["generated_by_is_self_asserted"])
        self.assertFalse(result["provenance"]["attestation_verified_by_gate"])

    def test_measured_evidence_without_attestation_is_not_review_eligible(self) -> None:
        report = LoadFixture(PassFixture)
        report["evidence_kind"] = gate.MeasuredEvidenceKind
        Reseal(report)

        result = gate.EvaluateReport(report, FixtureConfig)

        self.assertTrue(result["gate_passed"])
        self.assertFalse(result["eligible_for_policy_review"])
        self.assertEqual(
            "EVIDENCE_PASSED_ATTESTATION_REQUIRED",
            result["recommendation"],
        )
        self.assertFalse(
            result["review_requirements"][
                "artifact_and_runner_attestation"
            ]["passed"]
        )

    def test_cross_mirror_runs_cannot_masquerade_as_repeats(self) -> None:
        report = LoadFixture(PassFixture)
        report["runs"][1]["mirror_id"] = "mirror-cross-contamination"
        Reseal(report)

        result = gate.EvaluateReport(report, FixtureConfig)

        self.assertFalse(
            result["checks"]["same_seed_repeat_determinism"]["passed"]
        )

    def test_fake_mirror_pair_with_different_scenario_identity_is_rejected(
        self,
    ) -> None:
        report = LoadFixture(PassFixture)
        for run in report["runs"]:
            if run["mirror_id"] == "mirror-a" and run["side"] == "RED":
                run["scenario_id"] = "different-scenario"
                run["scenario_sha256"] = "7" * 64
        Reseal(report)

        result = gate.EvaluateReport(report, FixtureConfig)

        self.assertTrue(
            result["checks"]["same_seed_repeat_determinism"]["passed"]
        )
        self.assertFalse(result["checks"]["mirrored_side_coverage"]["passed"])
        self.assertTrue(
            any("scenario identity" in reason for reason in result["reasons"])
        )

    def test_correlated_blue_red_pair_is_one_statistical_sample(self) -> None:
        report = LoadFixture(PassFixture)
        groups = gate._GroupRuns(report["runs"])
        representatives = gate._CollectRepresentatives(groups)
        reasons, mirrored_pairs = gate._CheckMirroredCoverage(
            representatives,
            FixtureConfig,
        )
        metrics = gate._ComputeMetrics(
            report["runs"],
            mirrored_pairs,
            FixtureConfig.confidence_level,
        )

        self.assertEqual([], reasons)
        self.assertEqual(4, len(representatives))
        self.assertEqual(2, len(mirrored_pairs))
        self.assertEqual(2, metrics["sample_count"])
        self.assertEqual(2, metrics["mirrored_pair_count"])

    def test_outcome_contract_rejects_double_win_and_allows_draw(self) -> None:
        double_win = LoadFixture(PassFixture)
        double_win["runs"][0]["candidate_win"] = True
        double_win["runs"][0]["baseline_win"] = True
        Reseal(double_win)

        rejected = gate.EvaluateReport(double_win, FixtureConfig)

        self.assertFalse(rejected["checks"]["schema_and_hashes"]["passed"])
        self.assertTrue(any("only draw encoding" in row for row in rejected["reasons"]))

        draw = LoadFixture(PassFixture)
        for run in draw["runs"][:2]:
            run["candidate_win"] = False
            run["baseline_win"] = False
        RecomputeSummary(draw)
        Reseal(draw)

        accepted = gate.EvaluateReport(draw, FixtureConfig)

        self.assertTrue(accepted["checks"]["schema_and_hashes"]["passed"])
        self.assertTrue(accepted["gate_passed"])

    def test_metric_tolerance_has_a_strict_accept_reject_boundary(self) -> None:
        within = LoadFixture(PassFixture)
        within["summary"]["score_delta_mean"] += (
            FixtureConfig.metric_tolerance * 0.5
        )
        Reseal(within)

        within_result = gate.EvaluateReport(within, FixtureConfig)

        self.assertTrue(
            within_result["checks"]
            ["confidence_interval_and_sample_count"]["passed"]
        )

        outside = LoadFixture(PassFixture)
        outside["summary"]["score_delta_mean"] += (
            FixtureConfig.metric_tolerance * 2.0
        )
        Reseal(outside)

        outside_result = gate.EvaluateReport(outside, FixtureConfig)

        self.assertFalse(
            outside_result["checks"]
            ["confidence_interval_and_sample_count"]["passed"]
        )

    def test_declared_attestation_must_bind_the_candidate_artifact(self) -> None:
        report = LoadFixture(PassFixture)
        report["evidence_kind"] = gate.MeasuredEvidenceKind
        DeclareExternalAttestation(report)
        report["attestation"]["candidate_artifact_sha256"] = "c" * 64
        Reseal(report)

        result = gate.EvaluateReport(report, FixtureConfig)

        self.assertFalse(result["checks"]["schema_and_hashes"]["passed"])
        self.assertFalse(result["eligible_for_policy_review"])

    def test_golden_fail_blocks_every_substantive_gate(self) -> None:
        config = gate.PolicyGateConfig(
            minimum_samples=2,
            minimum_mirrored_pairs=1,
            minimum_repeat_count=2,
        )
        result = gate.EvaluateReport(LoadFixture(FailFixture), config)

        self.assertFalse(result["gate_passed"])
        self.assertTrue(result["checks"]["schema_and_hashes"]["passed"])
        for name in (
            "same_seed_repeat_determinism",
            "mirrored_side_coverage",
            "frozen_holdout_non_regression",
            "fault_and_invalid_command_zero",
            "privileged_observation_zero",
            "confidence_interval_and_sample_count",
        ):
            self.assertFalse(result["checks"][name]["passed"], name)

    def test_evidence_hash_detects_tampering(self) -> None:
        report = LoadFixture(PassFixture)
        report["runs"][0]["candidate_score"] = 999.0

        result = gate.EvaluateReport(report, FixtureConfig)

        self.assertFalse(result["checks"]["schema_and_hashes"]["passed"])
        self.assertTrue(any("mismatch" in reason for reason in result["reasons"]))

    def test_same_seed_repeat_divergence_is_rejected(self) -> None:
        report = LoadFixture(PassFixture)
        report["runs"][1]["candidate_result_sha256"] = "f" * 64
        Reseal(report)

        result = gate.EvaluateReport(report, FixtureConfig)

        self.assertFalse(
            result["checks"]["same_seed_repeat_determinism"]["passed"]
        )

    def test_privileged_observation_and_invalid_command_are_rejected(self) -> None:
        report = LoadFixture(PassFixture)
        report["runs"][0]["invalid_command_count"] = 1
        report["runs"][0]["privileged_observation_count"] = 1
        Reseal(report)

        result = gate.EvaluateReport(report, FixtureConfig)

        self.assertFalse(
            result["checks"]["fault_and_invalid_command_zero"]["passed"]
        )
        self.assertFalse(result["checks"]["privileged_observation_zero"]["passed"])

    def test_reported_confidence_interval_is_recomputed(self) -> None:
        report = LoadFixture(PassFixture)
        report["summary"]["score_delta_ci_lower"] = 100.0
        Reseal(report)

        result = gate.EvaluateReport(report, FixtureConfig)

        self.assertFalse(
            result["checks"]["confidence_interval_and_sample_count"]["passed"]
        )

    def test_frozen_holdout_score_regression_is_rejected(self) -> None:
        report = LoadFixture(PassFixture)
        for run in report["runs"]:
            run["candidate_score"] -= 0.5
            run["candidate_win"] = False

        groups = gate._GroupRuns(report["runs"])
        representatives = gate._CollectRepresentatives(groups)
        _, mirrored_pairs = gate._CheckMirroredCoverage(
            representatives,
            FixtureConfig,
        )
        report["summary"] = gate._ComputeMetrics(
            report["runs"],
            mirrored_pairs,
            FixtureConfig.confidence_level,
        )
        Reseal(report)

        result = gate.EvaluateReport(report, FixtureConfig)

        self.assertFalse(
            result["checks"]["frozen_holdout_non_regression"]["passed"]
        )

    def test_cli_writes_only_a_gate_report(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "gate_report.json"
            with contextlib.redirect_stdout(io.StringIO()):
                exit_code = gate.Main(
                    [
                        "--input",
                        str(PassFixture),
                        "--output",
                        str(output),
                        "--minimum-samples",
                        "2",
                        "--minimum-mirrored-pairs",
                        "2",
                    ]
                )
            written = json.loads(output.read_text(encoding="utf-8"))

        self.assertEqual(0, exit_code)
        self.assertEqual("report-only", written["mode"])
        self.assertFalse(written["promotion_performed"])
        self.assertFalse(written["policy_state_modified"])

    def test_no_forbidden_runtime_or_compiler_dependency(self) -> None:
        source = (AI_RESEARCH_ROOT / "PolicyPromotionGate.py").read_text(
            encoding="utf-8"
        )
        for forbidden in ("head2head", "subprocess", "g++"):
            self.assertNotIn(forbidden, source)

if __name__ == "__main__":
    unittest.main()
