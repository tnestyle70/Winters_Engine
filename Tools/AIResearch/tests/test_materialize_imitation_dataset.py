from __future__ import annotations

import copy
import hashlib
import json
import tempfile
import unittest
from pathlib import Path
import sys


AI_RESEARCH_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(AI_RESEARCH_ROOT))

import AiEpisodeSchema as episode_schema  # noqa: E402
import AiImitationDatasetSchema as imitation_schema  # noqa: E402
import MaterializeImitationDataset as materializer  # noqa: E402


EpisodeFixture = (
    AI_RESEARCH_ROOT / "fixtures" / "imitation_ranking_v1_golden.jsonl"
)
SidecarFixture = (
    AI_RESEARCH_ROOT
    / "fixtures"
    / "ai_decision_correction_sidecar_v1_golden.json"
)
ExpectedMaterializedSha256 = (
    "c6d3c4f5aebc7257dc6263fac1d6ffb50a0859bf8077b478caa0622d3009f1ee"
)


def LoadEpisodeRecords() -> list[dict[str, object]]:
    return [
        json.loads(line)
        for line in EpisodeFixture.read_text(encoding="utf-8").splitlines()
        if line.strip()
    ]


def LoadSidecar() -> dict[str, object]:
    return json.loads(SidecarFixture.read_text(encoding="utf-8"))


def WriteJson(path: Path, value: object) -> None:
    path.write_text(
        json.dumps(
            value,
            ensure_ascii=False,
            allow_nan=False,
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )


def WriteEpisode(path: Path, records: list[dict[str, object]]) -> None:
    path.write_bytes(
        b"".join(
            imitation_schema.CanonicalJsonBytes(record) + b"\n"
            for record in records
        )
    )


class ImitationDatasetMaterializationTests(unittest.TestCase):
    def test_golden_materialization_preserves_source_and_marks_correction(self) -> None:
        source_records = LoadEpisodeRecords()
        rows = materializer.BuildMaterializedRows(EpisodeFixture, SidecarFixture)

        self.assertEqual(8, len(rows))
        self.assertEqual(source_records, [row["source_record"] for row in rows])
        corrected = next(
            row
            for row in rows
            if row["source_record"]["episode_id"] == "fixture-episode-02"
        )
        self.assertEqual(3, corrected["source_record"]["selected_candidate_kind"])
        self.assertEqual(2, corrected["expert_label"]["candidate_kind"])
        self.assertEqual(
            imitation_schema.LabelOriginHumanDebugCorrection,
            corrected["expert_label"]["origin"],
        )
        self.assertEqual(
            imitation_schema.ExecutionEvidenceUnexecutedCounterfactual,
            corrected["expert_label"]["execution_evidence"],
        )
        self.assertEqual(
            source_records[2]["command"],
            corrected["source_record"]["command"],
        )
        self.assertEqual(
            source_records[2]["executor"],
            corrected["source_record"]["executor"],
        )
        self.assertEqual(
            source_records[2]["reward"],
            corrected["source_record"]["reward"],
        )

    def test_repeated_materialization_is_byte_deterministic(self) -> None:
        rows = materializer.BuildMaterializedRows(EpisodeFixture, SidecarFixture)
        with tempfile.TemporaryDirectory() as directory:
            first = Path(directory) / "first.jsonl"
            second = Path(directory) / "second.jsonl"
            materializer.WriteCanonicalJsonl(rows, first)
            materializer.WriteCanonicalJsonl(rows, second)
            first_bytes = first.read_bytes()
            second_bytes = second.read_bytes()

        self.assertEqual(first_bytes, second_bytes)
        self.assertEqual(
            ExpectedMaterializedSha256,
            hashlib.sha256(first_bytes).hexdigest(),
        )

    def test_corrected_only_excludes_unreviewed_source_actions(self) -> None:
        rows = materializer.BuildMaterializedRows(
            EpisodeFixture,
            SidecarFixture,
            corrected_only=True,
        )

        self.assertEqual(1, len(rows))
        self.assertEqual(
            imitation_schema.LabelOriginHumanDebugCorrection,
            rows[0]["expert_label"]["origin"],
        )

        with tempfile.TemporaryDirectory() as directory:
            sidecar = LoadSidecar()
            sidecar["corrections"] = []
            sidecar_path = Path(directory) / "empty-sidecar.json"
            WriteJson(sidecar_path, sidecar)
            with self.assertRaisesRegex(
                imitation_schema.ImitationDatasetError,
                "contains no human corrections",
            ):
                materializer.BuildMaterializedRows(
                    EpisodeFixture,
                    sidecar_path,
                    corrected_only=True,
                )

    def test_correction_array_order_does_not_change_output(self) -> None:
        source_records = LoadEpisodeRecords()
        sidecar = LoadSidecar()
        second_record = source_records[3]
        sidecar["corrections"].append(
            {
                "annotation_id": "fixture-human-debug-correction-03",
                "record_identity": imitation_schema.BuildIdentityObject(second_record),
                "source_record_sha256": imitation_schema.ComputeRecordSha256(
                    second_record
                ),
                "original_candidate_kind": 4,
                "corrected_candidate_kind": 1,
                "teacher_kind": imitation_schema.TeacherKindHuman,
                "annotation_method": "CHRONO_MANUAL_COUNTERFACTUAL",
                "reason_code": "SAFETY",
            }
        )
        reversed_sidecar = copy.deepcopy(sidecar)
        reversed_sidecar["corrections"].reverse()

        with tempfile.TemporaryDirectory() as directory:
            first_sidecar = Path(directory) / "first.json"
            second_sidecar = Path(directory) / "second.json"
            first_output = Path(directory) / "first.jsonl"
            second_output = Path(directory) / "second.jsonl"
            WriteJson(first_sidecar, sidecar)
            WriteJson(second_sidecar, reversed_sidecar)
            materializer.WriteCanonicalJsonl(
                materializer.BuildMaterializedRows(EpisodeFixture, first_sidecar),
                first_output,
            )
            materializer.WriteCanonicalJsonl(
                materializer.BuildMaterializedRows(EpisodeFixture, second_sidecar),
                second_output,
            )
            first_bytes = first_output.read_bytes()
            second_bytes = second_output.read_bytes()

        self.assertEqual(first_bytes, second_bytes)

    def test_stale_source_record_and_unknown_identity_fail_closed(self) -> None:
        cases = (
            ("source", "source_ai_episode_sha256", "0" * 64, "source AiEpisode"),
            (
                "record",
                "source_record_sha256",
                "0" * 64,
                "source record SHA-256",
            ),
        )
        for name, field, value, message in cases:
            with self.subTest(name=name), tempfile.TemporaryDirectory() as directory:
                sidecar = LoadSidecar()
                if name == "source":
                    sidecar[field] = value
                else:
                    sidecar["corrections"][0][field] = value
                path = Path(directory) / "sidecar.json"
                WriteJson(path, sidecar)
                with self.assertRaisesRegex(
                    imitation_schema.ImitationDatasetError,
                    message,
                ):
                    materializer.BuildMaterializedRows(EpisodeFixture, path)

        with tempfile.TemporaryDirectory() as directory:
            sidecar = LoadSidecar()
            sidecar["corrections"][0]["record_identity"]["tick"] += 999
            path = Path(directory) / "unknown.json"
            WriteJson(path, sidecar)
            with self.assertRaisesRegex(
                imitation_schema.ImitationDatasetError,
                "unknown source record",
            ):
                materializer.BuildMaterializedRows(EpisodeFixture, path)

    def test_illegal_correction_fails_closed(self) -> None:
        records = LoadEpisodeRecords()
        corrected_record = records[2]
        corrected_bit = episode_schema.CandidateKindToBit[2]
        corrected_record["action_mask"]["legal_candidate_mask"] &= ~corrected_bit
        corrected_record["action_mask"]["illegal_candidate_mask"] |= corrected_bit
        candidate = next(
            value for value in corrected_record["candidates"] if value["kind"] == 2
        )
        candidate["flags"] &= ~episode_schema.CandidateLegalFlag

        with tempfile.TemporaryDirectory() as directory:
            episode_path = Path(directory) / "episode.jsonl"
            sidecar_path = Path(directory) / "sidecar.json"
            WriteEpisode(episode_path, records)
            sidecar = LoadSidecar()
            sidecar["source_ai_episode_sha256"] = hashlib.sha256(
                episode_path.read_bytes()
            ).hexdigest()
            sidecar["corrections"][0][
                "source_record_sha256"
            ] = imitation_schema.ComputeRecordSha256(corrected_record)
            WriteJson(sidecar_path, sidecar)
            with self.assertRaisesRegex(
                imitation_schema.ImitationDatasetError,
                "illegal candidate",
            ):
                materializer.BuildMaterializedRows(episode_path, sidecar_path)

    def test_duplicate_unknown_and_non_finite_sidecar_fields_fail_closed(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            duplicate_path = Path(directory) / "duplicate.json"
            duplicate_path.write_text(
                '{"schema_version":1,"schema_version":1}\n',
                encoding="utf-8",
            )
            with self.assertRaisesRegex(
                imitation_schema.ImitationDatasetError,
                "duplicate object key",
            ):
                imitation_schema.LoadCorrectionSidecar(duplicate_path)

            nan_path = Path(directory) / "nan.json"
            nan_path.write_text('{"value":NaN}\n', encoding="utf-8")
            with self.assertRaisesRegex(
                imitation_schema.ImitationDatasetError,
                "non-finite JSON constant",
            ):
                imitation_schema.LoadCorrectionSidecar(nan_path)

            sidecar = LoadSidecar()
            sidecar["unexpected"] = True
            unknown_path = Path(directory) / "unknown.json"
            WriteJson(unknown_path, sidecar)
            with self.assertRaisesRegex(
                imitation_schema.ImitationDatasetError,
                "unknown field",
            ):
                imitation_schema.LoadCorrectionSidecar(unknown_path)

            duplicated = LoadSidecar()
            duplicated["corrections"].append(
                copy.deepcopy(duplicated["corrections"][0])
            )
            duplicate_correction_path = Path(directory) / "duplicate-correction.json"
            WriteJson(duplicate_correction_path, duplicated)
            with self.assertRaisesRegex(
                imitation_schema.ImitationDatasetError,
                "duplicated",
            ):
                imitation_schema.LoadCorrectionSidecar(duplicate_correction_path)

    def test_materialized_loader_rejects_forged_execution_claim_and_record(self) -> None:
        rows = materializer.BuildMaterializedRows(EpisodeFixture, SidecarFixture)
        corrected_index = next(
            index
            for index, row in enumerate(rows)
            if row["expert_label"]["origin"]
            == imitation_schema.LabelOriginHumanDebugCorrection
        )
        cases = (
            ("execution", "human correction must use"),
            ("record", "record_sha256 does not match"),
        )
        for name, message in cases:
            with self.subTest(name=name), tempfile.TemporaryDirectory() as directory:
                changed = copy.deepcopy(rows)
                if name == "execution":
                    changed[corrected_index]["expert_label"]["execution_evidence"] = (
                        imitation_schema.ExecutionEvidenceSourceCommandAccepted
                    )
                else:
                    changed[corrected_index]["source_record"]["reward"] += 1.0
                path = Path(directory) / "changed.jsonl"
                materializer.WriteCanonicalJsonl(changed, path)
                with self.assertRaisesRegex(
                    imitation_schema.ImitationDatasetError,
                    message,
                ):
                    imitation_schema.LoadImitationDataset(path)


if __name__ == "__main__":
    unittest.main()
