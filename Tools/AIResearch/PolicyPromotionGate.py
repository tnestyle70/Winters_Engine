"""Report-only promotion evidence gate for precomputed Winters league JSON.

The tool validates evidence and emits a review recommendation. It never loads,
builds, replaces, or promotes a policy artifact. External attestation fields
are structural declarations; this gate does not cryptographically verify them.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import statistics
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any


ContractVersion = 1
InputReportType = "WintersPolicyLeagueReportV1"
MeasuredEvidenceKind = "MEASURED_LEAGUE"
FixtureEvidenceKind = "GOLDEN_CONTRACT_FIXTURE"
AllowedEvidenceKinds = {MeasuredEvidenceKind, FixtureEvidenceKind}
AllowedSides = {"BLUE", "RED"}
UnattestedStatus = "UNATTESTED"
ExternalAttestedStatus = "EXTERNAL_ATTESTED"
AllowedAttestationStatuses = {UnattestedStatus, ExternalAttestedStatus}
Hex64Length = 64

RequiredTopLevelFields = {
    "schema_version",
    "report_type",
    "evidence_kind",
    "generated_by",
    "attestation",
    "environment",
    "candidate",
    "frozen_baseline",
    "holdout",
    "runs",
    "summary",
    "evidence_sha256",
}
RequiredAttestationFields = {
    "status",
    "candidate_artifact_sha256",
    "runner_artifact_sha256",
    "attestation_sha256",
    "attested_by",
}
RequiredEnvironmentFields = {
    "observation_schema_version",
    "action_schema_version",
    "rules_sha256",
    "definition_sha256",
}
RequiredPolicyFields = {"policy_id", "policy_revision", "policy_sha256"}
RequiredHoldoutFields = {"suite_id", "suite_sha256", "frozen"}
RequiredRunFields = {
    "run_id",
    "mirror_id",
    "scenario_id",
    "scenario_sha256",
    "seed",
    "side",
    "repeat_index",
    "candidate_result_sha256",
    "baseline_result_sha256",
    "candidate_score",
    "baseline_score",
    "candidate_win",
    "baseline_win",
    "fault_count",
    "invalid_command_count",
    "privileged_observation_count",
}
RequiredSummaryFields = {
    "raw_run_count",
    "sample_count",
    "mirrored_pair_count",
    "candidate_win_rate",
    "baseline_win_rate",
    "score_delta_mean",
    "score_delta_ci_lower",
    "score_delta_ci_upper",
    "confidence_level",
    "fault_count",
    "invalid_command_count",
    "privileged_observation_count",
}


class DuplicateObjectKeyError(ValueError):
    pass


class NonFiniteJsonConstantError(ValueError):
    pass


@dataclass(frozen=True)
class PolicyGateConfig:
    minimum_samples: int = 30
    minimum_mirrored_pairs: int = 15
    minimum_repeat_count: int = 2
    confidence_level: float = 0.95
    maximum_score_regression: float = 0.0
    maximum_win_rate_regression: float = 0.0
    metric_tolerance: float = 1e-9

    def Validate(self) -> list[str]:
        errors: list[str] = []
        if self.minimum_samples < 2:
            errors.append("minimum_samples must be at least 2")
        if self.minimum_mirrored_pairs < 1:
            errors.append("minimum_mirrored_pairs must be at least 1")
        if self.minimum_repeat_count < 2:
            errors.append("minimum_repeat_count must be at least 2")
        if (
            not math.isfinite(self.confidence_level)
            or not 0.0 < self.confidence_level < 1.0
        ):
            errors.append("confidence_level must be finite and between 0 and 1")
        if (
            not math.isfinite(self.maximum_score_regression)
            or self.maximum_score_regression < 0.0
        ):
            errors.append(
                "maximum_score_regression must be finite and non-negative"
            )
        if (
            not math.isfinite(self.maximum_win_rate_regression)
            or not 0.0 <= self.maximum_win_rate_regression <= 1.0
        ):
            errors.append(
                "maximum_win_rate_regression must be finite and between 0 and 1"
            )
        if not math.isfinite(self.metric_tolerance) or self.metric_tolerance <= 0.0:
            errors.append("metric_tolerance must be finite and positive")
        return errors


def RejectDuplicateObjectKeys(
    pairs: list[tuple[str, Any]],
) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise DuplicateObjectKeyError(f"duplicate object key: {key}")
        result[key] = value
    return result


def RejectNonFiniteJsonConstant(value: str) -> None:
    raise NonFiniteJsonConstantError(f"non-finite JSON constant: {value}")


def LoadJsonStrict(path: Path) -> dict[str, Any]:
    value = json.loads(
        path.read_text(encoding="utf-8"),
        object_pairs_hook=RejectDuplicateObjectKeys,
        parse_constant=RejectNonFiniteJsonConstant,
    )
    if not isinstance(value, dict):
        raise ValueError("league report root must be an object")
    return value


def ComputeEvidenceSha256(report: dict[str, Any]) -> str:
    payload = {key: value for key, value in report.items() if key != "evidence_sha256"}
    canonical = json.dumps(
        payload,
        ensure_ascii=False,
        allow_nan=False,
        sort_keys=True,
        separators=(",", ":"),
    ).encode("utf-8")
    return hashlib.sha256(canonical).hexdigest()


def SealReport(report: dict[str, Any]) -> dict[str, Any]:
    report["evidence_sha256"] = ComputeEvidenceSha256(report)
    return report


def _IsNonNegativeInt(value: Any) -> bool:
    return not isinstance(value, bool) and isinstance(value, int) and value >= 0


def _IsFiniteNumber(value: Any) -> bool:
    return (
        not isinstance(value, bool)
        and isinstance(value, (int, float))
        and math.isfinite(float(value))
    )


def _IsSha256(value: Any) -> bool:
    return (
        isinstance(value, str)
        and len(value) == Hex64Length
        and value != "0" * Hex64Length
        and all(character in "0123456789abcdef" for character in value)
    )


def _ValidateExactFields(
    value: Any,
    required: set[str],
    path: str,
) -> list[str]:
    if not isinstance(value, dict):
        return [f"{path} must be an object"]

    missing = sorted(required - value.keys())
    unknown = sorted(value.keys() - required)
    return [
        *(f"{path} missing field: {field}" for field in missing),
        *(f"{path} unknown field: {field}" for field in unknown),
    ]


def _ValidatePolicy(value: Any, path: str) -> list[str]:
    errors = _ValidateExactFields(value, RequiredPolicyFields, path)
    if not isinstance(value, dict):
        return errors
    if not isinstance(value.get("policy_id"), str) or not value.get("policy_id"):
        errors.append(f"{path}.policy_id must be a non-empty string")
    if not _IsNonNegativeInt(value.get("policy_revision")):
        errors.append(f"{path}.policy_revision must be a non-negative integer")
    if not _IsSha256(value.get("policy_sha256")):
        errors.append(f"{path}.policy_sha256 must be a non-placeholder SHA-256")
    return errors


def _ValidateAttestation(
    value: Any,
    candidate: Any,
) -> list[str]:
    path = "report.attestation"
    errors = _ValidateExactFields(value, RequiredAttestationFields, path)
    if not isinstance(value, dict):
        return errors

    status = value.get("status")
    if status not in AllowedAttestationStatuses:
        errors.append(f"{path}.status is invalid")
        return errors

    fields = (
        "candidate_artifact_sha256",
        "runner_artifact_sha256",
        "attestation_sha256",
        "attested_by",
    )
    if status == UnattestedStatus:
        for field in fields:
            if value.get(field) != "":
                errors.append(
                    f"{path}.{field} must be empty when status is UNATTESTED"
                )
        return errors

    for field in (
        "candidate_artifact_sha256",
        "runner_artifact_sha256",
        "attestation_sha256",
    ):
        if not _IsSha256(value.get(field)):
            errors.append(f"{path}.{field} must be a non-placeholder SHA-256")
    if not isinstance(value.get("attested_by"), str) or not value.get(
        "attested_by"
    ):
        errors.append(f"{path}.attested_by must be a non-empty string")
    if isinstance(candidate, dict):
        if value.get("candidate_artifact_sha256") != candidate.get(
            "policy_sha256"
        ):
            errors.append(
                f"{path}.candidate_artifact_sha256 must match candidate policy"
            )
    return errors


def _ValidateSchemaAndHashes(report: Any) -> list[str]:
    errors = _ValidateExactFields(report, RequiredTopLevelFields, "report")
    if not isinstance(report, dict):
        return errors

    if report.get("schema_version") != ContractVersion:
        errors.append(f"report.schema_version must be {ContractVersion}")
    if report.get("report_type") != InputReportType:
        errors.append(f"report.report_type must be {InputReportType}")
    if report.get("evidence_kind") not in AllowedEvidenceKinds:
        errors.append("report.evidence_kind is invalid")
    if not isinstance(report.get("generated_by"), str) or not report.get(
        "generated_by"
    ):
        errors.append("report.generated_by must be a non-empty string")

    environment = report.get("environment")
    errors.extend(
        _ValidateExactFields(
            environment,
            RequiredEnvironmentFields,
            "report.environment",
        )
    )
    if isinstance(environment, dict):
        if environment.get("observation_schema_version") != 1:
            errors.append("report.environment.observation_schema_version must be 1")
        if environment.get("action_schema_version") != 1:
            errors.append("report.environment.action_schema_version must be 1")
        for field in ("rules_sha256", "definition_sha256"):
            if not _IsSha256(environment.get(field)):
                errors.append(
                    f"report.environment.{field} must be a non-placeholder SHA-256"
                )

    candidate = report.get("candidate")
    baseline = report.get("frozen_baseline")
    errors.extend(_ValidateAttestation(report.get("attestation"), candidate))
    errors.extend(_ValidatePolicy(candidate, "report.candidate"))
    errors.extend(_ValidatePolicy(baseline, "report.frozen_baseline"))
    if isinstance(candidate, dict) and isinstance(baseline, dict):
        candidate_revision = candidate.get("policy_revision")
        baseline_revision = baseline.get("policy_revision")
        if _IsNonNegativeInt(candidate_revision) and _IsNonNegativeInt(
            baseline_revision
        ):
            if candidate_revision <= baseline_revision:
                errors.append(
                    "report.candidate.policy_revision must exceed the frozen baseline"
                )
        if candidate.get("policy_sha256") == baseline.get("policy_sha256"):
            errors.append("candidate and frozen baseline policy hashes must differ")

    holdout = report.get("holdout")
    errors.extend(
        _ValidateExactFields(holdout, RequiredHoldoutFields, "report.holdout")
    )
    if isinstance(holdout, dict):
        if not isinstance(holdout.get("suite_id"), str) or not holdout.get(
            "suite_id"
        ):
            errors.append("report.holdout.suite_id must be a non-empty string")
        if not _IsSha256(holdout.get("suite_sha256")):
            errors.append(
                "report.holdout.suite_sha256 must be a non-placeholder SHA-256"
            )
        if not isinstance(holdout.get("frozen"), bool):
            errors.append("report.holdout.frozen must be a boolean")

    runs = report.get("runs")
    if not isinstance(runs, list) or not runs:
        errors.append("report.runs must be a non-empty array")
    else:
        seen_run_ids: set[str] = set()
        for index, run in enumerate(runs):
            path = f"report.runs[{index}]"
            errors.extend(_ValidateExactFields(run, RequiredRunFields, path))
            if not isinstance(run, dict):
                continue
            for field in ("run_id", "mirror_id", "scenario_id"):
                if not isinstance(run.get(field), str) or not run.get(field):
                    errors.append(f"{path}.{field} must be a non-empty string")
            run_id = run.get("run_id")
            if isinstance(run_id, str):
                if run_id in seen_run_ids:
                    errors.append(f"{path}.run_id must be unique")
                seen_run_ids.add(run_id)
            for field in (
                "scenario_sha256",
                "candidate_result_sha256",
                "baseline_result_sha256",
            ):
                if not _IsSha256(run.get(field)):
                    errors.append(f"{path}.{field} must be a non-placeholder SHA-256")
            for field in (
                "seed",
                "repeat_index",
                "fault_count",
                "invalid_command_count",
                "privileged_observation_count",
            ):
                if not _IsNonNegativeInt(run.get(field)):
                    errors.append(f"{path}.{field} must be a non-negative integer")
            for field in ("candidate_score", "baseline_score"):
                if not _IsFiniteNumber(run.get(field)):
                    errors.append(f"{path}.{field} must be a finite number")
            for field in ("candidate_win", "baseline_win"):
                if not isinstance(run.get(field), bool):
                    errors.append(f"{path}.{field} must be a boolean")
            if run.get("candidate_win") is True and run.get(
                "baseline_win"
            ) is True:
                errors.append(
                    f"{path} cannot mark candidate_win and baseline_win true; "
                    "both false is the only draw encoding"
                )
            if run.get("side") not in AllowedSides:
                errors.append(f"{path}.side must be BLUE or RED")

    summary = report.get("summary")
    errors.extend(
        _ValidateExactFields(summary, RequiredSummaryFields, "report.summary")
    )
    if isinstance(summary, dict):
        for field in (
            "raw_run_count",
            "sample_count",
            "mirrored_pair_count",
            "fault_count",
            "invalid_command_count",
            "privileged_observation_count",
        ):
            if not _IsNonNegativeInt(summary.get(field)):
                errors.append(f"report.summary.{field} must be a non-negative integer")
        for field in (
            "candidate_win_rate",
            "baseline_win_rate",
            "score_delta_mean",
            "score_delta_ci_lower",
            "score_delta_ci_upper",
            "confidence_level",
        ):
            if not _IsFiniteNumber(summary.get(field)):
                errors.append(f"report.summary.{field} must be a finite number")
        for field in ("candidate_win_rate", "baseline_win_rate", "confidence_level"):
            value = summary.get(field)
            if _IsFiniteNumber(value) and not 0.0 <= float(value) <= 1.0:
                errors.append(f"report.summary.{field} must be between 0 and 1")

    evidence_hash = report.get("evidence_sha256")
    if not _IsSha256(evidence_hash):
        errors.append("report.evidence_sha256 must be a non-placeholder SHA-256")
    else:
        try:
            actual_hash = ComputeEvidenceSha256(report)
        except (TypeError, ValueError) as error:
            errors.append(f"report evidence cannot be canonicalized: {error}")
        else:
            if evidence_hash != actual_hash:
                errors.append(
                    "report.evidence_sha256 mismatch: "
                    f"expected={evidence_hash} actual={actual_hash}"
                )
    return errors


def _RunGroupKey(run: dict[str, Any]) -> tuple[Any, ...]:
    return (
        run["mirror_id"],
        run["scenario_id"],
        run["scenario_sha256"],
        run["seed"],
        run["side"],
    )


def _RunSignature(run: dict[str, Any]) -> tuple[Any, ...]:
    return (
        run["candidate_result_sha256"],
        run["baseline_result_sha256"],
        float(run["candidate_score"]),
        float(run["baseline_score"]),
        run["candidate_win"],
        run["baseline_win"],
        run["fault_count"],
        run["invalid_command_count"],
        run["privileged_observation_count"],
    )


def _GroupRuns(
    runs: list[dict[str, Any]],
) -> dict[tuple[Any, ...], list[dict[str, Any]]]:
    groups: dict[tuple[Any, ...], list[dict[str, Any]]] = {}
    for run in runs:
        groups.setdefault(_RunGroupKey(run), []).append(run)
    return groups


def _CheckRepeatDeterminism(
    groups: dict[tuple[Any, ...], list[dict[str, Any]]],
    config: PolicyGateConfig,
) -> list[str]:
    reasons: list[str] = []
    for key, group in sorted(groups.items(), key=lambda item: str(item[0])):
        repeat_indices = sorted(run["repeat_index"] for run in group)
        if len(group) < config.minimum_repeat_count:
            reasons.append(
                f"run group {key} has {len(group)} repeats; "
                f"requires {config.minimum_repeat_count}"
            )
        if repeat_indices != list(range(len(repeat_indices))):
            reasons.append(f"run group {key} repeat_index values are not contiguous")
        reference = _RunSignature(group[0])
        if any(_RunSignature(run) != reference for run in group[1:]):
            reasons.append(f"run group {key} diverged under the same seed")
    return reasons


def _CollectRepresentatives(
    groups: dict[tuple[Any, ...], list[dict[str, Any]]],
) -> list[dict[str, Any]]:
    return [
        min(group, key=lambda run: run["repeat_index"])
        for _, group in sorted(groups.items(), key=lambda item: str(item[0]))
    ]


def _CheckMirroredCoverage(
    representatives: list[dict[str, Any]],
    config: PolicyGateConfig,
) -> tuple[
    list[str],
    list[tuple[dict[str, Any], dict[str, Any]]],
]:
    reasons: list[str] = []
    mirror_groups: dict[str, list[dict[str, Any]]] = {}
    for run in representatives:
        mirror_groups.setdefault(run["mirror_id"], []).append(run)

    valid_pairs: list[tuple[dict[str, Any], dict[str, Any]]] = []
    for mirror_id, group in sorted(mirror_groups.items()):
        contexts = {
            (
                run["scenario_id"],
                run["scenario_sha256"],
                run["seed"],
            )
            for run in group
        }
        if len(contexts) != 1:
            reasons.append(
                f"mirror group {mirror_id} mixes scenario identity or seed"
            )
            continue

        by_side: dict[str, list[dict[str, Any]]] = {}
        for run in group:
            by_side.setdefault(run["side"], []).append(run)
        if set(by_side) != AllowedSides:
            reasons.append(
                f"mirror group {mirror_id} does not contain BLUE and RED"
            )
            continue
        if any(len(side_runs) != 1 for side_runs in by_side.values()):
            reasons.append(
                f"mirror group {mirror_id} contains duplicate side samples"
            )
            continue
        valid_pairs.append((by_side["BLUE"][0], by_side["RED"][0]))

    if len(valid_pairs) < config.minimum_mirrored_pairs:
        reasons.append(
            f"mirrored pair count {len(valid_pairs)} is below "
            f"{config.minimum_mirrored_pairs}"
        )
    return reasons, valid_pairs


def _ComputeMetrics(
    runs: list[dict[str, Any]],
    mirrored_pairs: list[tuple[dict[str, Any], dict[str, Any]]],
    confidence_level: float,
) -> dict[str, Any]:
    pair_deltas = [
        statistics.fmean(
            float(run["candidate_score"]) - float(run["baseline_score"])
            for run in pair
        )
        for pair in mirrored_pairs
    ]
    sample_count = len(mirrored_pairs)
    mean_delta = statistics.fmean(pair_deltas) if pair_deltas else 0.0
    if len(pair_deltas) >= 2:
        standard_error = statistics.stdev(pair_deltas) / math.sqrt(
            len(pair_deltas)
        )
    else:
        standard_error = 0.0
    z_score = statistics.NormalDist().inv_cdf(0.5 + confidence_level / 2.0)
    radius = z_score * standard_error

    return {
        "raw_run_count": len(runs),
        "sample_count": sample_count,
        "mirrored_pair_count": len(mirrored_pairs),
        "candidate_win_rate": (
            statistics.fmean(
                statistics.fmean(float(run["candidate_win"]) for run in pair)
                for pair in mirrored_pairs
            )
            if sample_count
            else 0.0
        ),
        "baseline_win_rate": (
            statistics.fmean(
                statistics.fmean(float(run["baseline_win"]) for run in pair)
                for pair in mirrored_pairs
            )
            if sample_count
            else 0.0
        ),
        "score_delta_mean": mean_delta,
        "score_delta_ci_lower": mean_delta - radius,
        "score_delta_ci_upper": mean_delta + radius,
        "confidence_level": confidence_level,
        "fault_count": sum(run["fault_count"] for run in runs),
        "invalid_command_count": sum(run["invalid_command_count"] for run in runs),
        "privileged_observation_count": sum(
            run["privileged_observation_count"] for run in runs
        ),
    }


def _CheckReportedSummary(
    reported: dict[str, Any],
    computed: dict[str, Any],
    config: PolicyGateConfig,
) -> list[str]:
    reasons: list[str] = []
    integer_fields = {
        "raw_run_count",
        "sample_count",
        "mirrored_pair_count",
        "fault_count",
        "invalid_command_count",
        "privileged_observation_count",
    }
    for field in sorted(RequiredSummaryFields):
        if field in integer_fields:
            if reported[field] != computed[field]:
                reasons.append(
                    f"summary.{field} mismatch: "
                    f"reported={reported[field]} computed={computed[field]}"
                )
        elif not math.isclose(
            float(reported[field]),
            float(computed[field]),
            rel_tol=0.0,
            abs_tol=config.metric_tolerance,
        ):
            reasons.append(
                f"summary.{field} mismatch: "
                f"reported={reported[field]} computed={computed[field]}"
            )

    if computed["sample_count"] < config.minimum_samples:
        reasons.append(
            f"sample count {computed['sample_count']} is below "
            f"{config.minimum_samples}"
        )
    return reasons


def _MakeCheck(reasons: list[str]) -> dict[str, Any]:
    return {"passed": not reasons, "reasons": reasons}


def EvaluateReport(
    report: dict[str, Any],
    config: PolicyGateConfig | None = None,
) -> dict[str, Any]:
    resolved_config = config or PolicyGateConfig()
    config_errors = resolved_config.Validate()
    if config_errors:
        raise ValueError("; ".join(config_errors))

    schema_reasons = _ValidateSchemaAndHashes(report)
    checks: dict[str, dict[str, Any]] = {
        "schema_and_hashes": _MakeCheck(schema_reasons)
    }
    metrics: dict[str, Any] = {}

    if schema_reasons:
        blocked = ["blocked by schema_and_hashes"]
        for name in (
            "same_seed_repeat_determinism",
            "mirrored_side_coverage",
            "frozen_holdout_non_regression",
            "fault_and_invalid_command_zero",
            "privileged_observation_zero",
            "confidence_interval_and_sample_count",
        ):
            checks[name] = _MakeCheck(blocked.copy())
    else:
        runs = report["runs"]
        groups = _GroupRuns(runs)
        repeat_reasons = _CheckRepeatDeterminism(groups, resolved_config)
        representatives = _CollectRepresentatives(groups)
        mirror_reasons, mirrored_pairs = _CheckMirroredCoverage(
            representatives,
            resolved_config,
        )
        metrics = _ComputeMetrics(
            runs,
            mirrored_pairs,
            resolved_config.confidence_level,
        )

        non_regression_reasons: list[str] = []
        if report["holdout"]["frozen"] is not True:
            non_regression_reasons.append("holdout suite is not frozen")
        if metrics["score_delta_ci_lower"] < -resolved_config.maximum_score_regression:
            non_regression_reasons.append(
                "paired score confidence lower bound regressed: "
                f"{metrics['score_delta_ci_lower']}"
            )
        win_rate_delta = (
            metrics["candidate_win_rate"] - metrics["baseline_win_rate"]
        )
        if win_rate_delta < -resolved_config.maximum_win_rate_regression:
            non_regression_reasons.append(
                f"candidate win-rate delta regressed: {win_rate_delta}"
            )

        fault_reasons: list[str] = []
        if metrics["fault_count"] != 0:
            fault_reasons.append(
                f"fault_count must be zero; got {metrics['fault_count']}"
            )
        if metrics["invalid_command_count"] != 0:
            fault_reasons.append(
                "invalid_command_count must be zero; "
                f"got {metrics['invalid_command_count']}"
            )

        privileged_reasons: list[str] = []
        if metrics["privileged_observation_count"] != 0:
            privileged_reasons.append(
                "privileged_observation_count must be zero; "
                f"got {metrics['privileged_observation_count']}"
            )

        summary_reasons = _CheckReportedSummary(
            report["summary"],
            metrics,
            resolved_config,
        )

        checks.update(
            {
                "same_seed_repeat_determinism": _MakeCheck(repeat_reasons),
                "mirrored_side_coverage": _MakeCheck(mirror_reasons),
                "frozen_holdout_non_regression": _MakeCheck(
                    non_regression_reasons
                ),
                "fault_and_invalid_command_zero": _MakeCheck(fault_reasons),
                "privileged_observation_zero": _MakeCheck(privileged_reasons),
                "confidence_interval_and_sample_count": _MakeCheck(
                    summary_reasons
                ),
            }
        )

    gate_passed = all(check["passed"] for check in checks.values())
    evidence_kind = report.get("evidence_kind")
    measured_evidence = evidence_kind == MeasuredEvidenceKind
    attestation = report.get("attestation")
    attestation_declared = (
        isinstance(attestation, dict)
        and attestation.get("status") == ExternalAttestedStatus
        and not schema_reasons
    )
    attestation_reasons = (
        []
        if attestation_declared
        else [
            "external candidate-artifact and runner attestation is required "
            "before policy review"
        ]
    )
    eligible_for_policy_review = (
        gate_passed and measured_evidence and attestation_declared
    )
    if not gate_passed:
        recommendation = "BLOCKED"
    elif measured_evidence and not attestation_declared:
        recommendation = "EVIDENCE_PASSED_ATTESTATION_REQUIRED"
    elif measured_evidence:
        recommendation = "EVIDENCE_PASSED_REVIEW_ONLY"
    else:
        recommendation = "CONTRACT_FIXTURE_PASSED"

    reasons = [
        f"{name}: {reason}"
        for name, check in checks.items()
        for reason in check["reasons"]
    ]
    return {
        "contract": "PolicyPromotionGateV1",
        "mode": "report-only",
        "gate_passed": gate_passed,
        "eligible_for_policy_review": eligible_for_policy_review,
        "promotion_performed": False,
        "policy_state_modified": False,
        "recommendation": recommendation,
        "evidence_kind": evidence_kind,
        "input_evidence_sha256": report.get("evidence_sha256"),
        "config": asdict(resolved_config),
        "checks": checks,
        "review_requirements": {
            "artifact_and_runner_attestation": _MakeCheck(
                attestation_reasons
            )
        },
        "provenance": {
            "generated_by_is_self_asserted": True,
            "attestation_status": (
                attestation.get("status")
                if isinstance(attestation, dict)
                else None
            ),
            "attestation_declared": attestation_declared,
            "attestation_verified_by_gate": False,
            "manual_attestation_verification_required": True,
            "assurance": (
                "EXTERNAL_ATTESTATION_DECLARED_NOT_CRYPTOGRAPHICALLY_"
                "VERIFIED_BY_GATE"
                if attestation_declared
                else "UNATTESTED_SELF_ASSERTED_INPUT"
            ),
        },
        "metrics": metrics,
        "review_reasons": attestation_reasons if measured_evidence else [],
        "reasons": reasons,
    }


def WriteJsonAtomic(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(
        json.dumps(value, indent=2, ensure_ascii=False, allow_nan=False) + "\n",
        encoding="utf-8",
    )
    temporary.replace(path)


def Main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Evaluate a Winters policy league report. This command is "
            "report-only and never mutates or promotes a policy."
        )
    )
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--minimum-samples", type=int, default=30)
    parser.add_argument("--minimum-mirrored-pairs", type=int, default=15)
    parser.add_argument("--minimum-repeat-count", type=int, default=2)
    parser.add_argument("--confidence-level", type=float, default=0.95)
    parser.add_argument("--maximum-score-regression", type=float, default=0.0)
    parser.add_argument("--maximum-win-rate-regression", type=float, default=0.0)
    args = parser.parse_args(argv)

    config = PolicyGateConfig(
        minimum_samples=args.minimum_samples,
        minimum_mirrored_pairs=args.minimum_mirrored_pairs,
        minimum_repeat_count=args.minimum_repeat_count,
        confidence_level=args.confidence_level,
        maximum_score_regression=args.maximum_score_regression,
        maximum_win_rate_regression=args.maximum_win_rate_regression,
    )
    config_errors = config.Validate()
    if config_errors:
        for error in config_errors:
            print(f"PolicyPromotionGate configuration error: {error}")
        return 2

    try:
        report = LoadJsonStrict(args.input)
        result = EvaluateReport(report, config)
    except (
        OSError,
        ValueError,
        json.JSONDecodeError,
        DuplicateObjectKeyError,
        NonFiniteJsonConstantError,
    ) as error:
        print(f"PolicyPromotionGate input error: {error}")
        return 2

    if args.output:
        WriteJsonAtomic(args.output, result)
    print(json.dumps(result, indent=2, ensure_ascii=False, allow_nan=False))
    return 0 if result["gate_passed"] else 1


if __name__ == "__main__":
    raise SystemExit(Main())
