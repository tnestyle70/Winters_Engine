from __future__ import annotations

import hashlib
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import sys


AI_RESEARCH_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(AI_RESEARCH_ROOT))

import ValidateBridgeManifest as validator  # noqa: E402


def MakeEntry(source_path: str, source_sha256: str) -> dict[str, object]:
    return {
        "SourcePath": source_path,
        "SourceSha256": source_sha256,
        "ProvenanceAndLicense": "test fixture",
        "Disposition": "Adapt",
        "ReusableSymbol": "Fixture",
        "TargetOwner": "Tools/AIResearch",
        "TargetContract": "FixtureV1",
        "ForbiddenDependencies": ["fixture-only dependency"],
        "ObservationSchemaVersion": 1,
        "ActionSchemaVersion": 1,
        "PolicyRevision": 1,
        "DeterministicFixture": "fixture",
        "GoldenOutputHash": "fixture",
        "PromotionGate": "fixture",
        "RollbackOrDeletePath": "fixture",
    }


class BridgeManifestValidatorTests(unittest.TestCase):
    def test_valid_source_hash_and_versions_pass(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source.py"
            source.write_bytes(b"print('fixture')\n")
            sha256 = hashlib.sha256(source.read_bytes()).hexdigest()

            errors = validator.ValidateEntry(
                MakeEntry("source.py", sha256),
                0,
                root,
            )

            self.assertEqual([], errors)

    def test_path_traversal_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "nypc"
            root.mkdir()
            outside = root.parent / "outside.py"
            outside.write_bytes(b"outside\n")
            sha256 = hashlib.sha256(outside.read_bytes()).hexdigest()

            errors = validator.ValidateEntry(
                MakeEntry("../outside.py", sha256),
                0,
                root,
            )

            self.assertTrue(any("outside NYPC root" in value for value in errors))

    def test_duplicate_source_paths_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            entry = MakeEntry("source.py", "0" * 64)

            errors = validator.ValidateUniqueSourcePaths(
                [entry, dict(entry)],
                root,
            )

            self.assertTrue(any("duplicates SourcePath" in value for value in errors))

    def test_invalid_hash_and_version_types_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            entry = MakeEntry("source.py", "A" * 64)
            entry["ObservationSchemaVersion"] = True
            entry["ActionSchemaVersion"] = -1
            entry["PolicyRevision"] = 1.5

            errors = validator.ValidateEntry(entry, 0, root)

            self.assertTrue(any("SourceSha256" in value for value in errors))
            self.assertEqual(
                3,
                sum("non-negative integer" in value for value in errors),
            )

    def test_do_not_copy_scan_rejects_session_and_scratch_paths(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            winters_root = Path(directory)
            research_root = winters_root / "Tools" / "AIResearch"
            (research_root / "scratch_notes").mkdir(parents=True)
            (research_root / "run_s999.py").write_text("pass\n", encoding="utf-8")
            (research_root / "scratch_notes" / "helper.py").write_text(
                "pass\n",
                encoding="utf-8",
            )
            policy = MakeEntry(
                "mushroom/scripts/run_s*.py",
                "NOT_APPLICABLE_PATTERN",
            )
            policy["Disposition"] = "DoNotCopy"
            policy["TargetOwner"] = "None"

            errors = validator.ValidateDoNotCopyPolicy([policy], winters_root)

            self.assertTrue(any("run_s999.py" in value for value in errors))
            self.assertTrue(any("scratch_notes" in value for value in errors))

    def test_repository_revision_mismatch_is_rejected(self) -> None:
        with patch.object(
            validator,
            "ReadGitRevision",
            return_value=("actual", None),
        ):
            errors = validator.ValidateSourceRepository(
                {"Revision": "expected"},
                Path("unused"),
            )

        self.assertEqual(1, len(errors))
        self.assertIn("Revision mismatch", errors[0])


if __name__ == "__main__":
    unittest.main()
