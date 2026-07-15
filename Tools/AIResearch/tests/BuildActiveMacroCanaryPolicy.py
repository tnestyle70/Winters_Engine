"""Build the deterministic WBC policy used by the active SimLab canary probe."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import sys

import numpy as np


AI_RESEARCH_ROOT = Path(__file__).resolve().parents[1]
if str(AI_RESEARCH_ROOT) not in sys.path:
    sys.path.insert(0, str(AI_RESEARCH_ROOT))

import TrainImitationRankingBaseline as baseline


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    mean = np.zeros(baseline.RuntimePolicyFeatureCount, dtype=np.float64)
    inverse_scale = np.ones(
        baseline.RuntimePolicyFeatureCount,
        dtype=np.float64,
    )
    weights = np.zeros(baseline.RuntimePolicyFeatureCount, dtype=np.float64)
    retreat_feature_index = baseline.FeatureOrder.index("candidate_kind_1")
    weights[: len(baseline.RuntimePolicyCandidateOrder)] = -32.0
    weights[retreat_feature_index] = 32.0

    policy = baseline.BuildRuntimePolicyBinary(
        policy_revision=9001,
        source_policy_revision=1,
        normalization_mean=mean,
        normalization_inverse_scale=inverse_scale,
        weights=weights,
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary = args.output.with_suffix(args.output.suffix + ".tmp")
    temporary.write_bytes(policy)
    temporary.replace(args.output)
    print(hashlib.sha256(policy).hexdigest())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
