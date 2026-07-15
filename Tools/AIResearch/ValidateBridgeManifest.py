from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
from fnmatch import fnmatch
from pathlib import Path
from typing import Any


RequiredEntryFields = (
    "SourcePath",
    "SourceSha256",
    "ProvenanceAndLicense",
    "Disposition",
    "ReusableSymbol",
    "TargetOwner",
    "TargetContract",
    "ForbiddenDependencies",
    "ObservationSchemaVersion",
    "ActionSchemaVersion",
    "PolicyRevision",
    "DeterministicFixture",
    "GoldenOutputHash",
    "PromotionGate",
    "RollbackOrDeletePath",
)

AllowedDispositions = {"Extract", "Adapt", "Reference", "DoNotCopy"}
VersionFields = (
    "ObservationSchemaVersion",
    "ActionSchemaVersion",
    "PolicyRevision",
)
Sha256Pattern = re.compile(r"^[0-9a-f]{64}$")


def ComputeSha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def IsPatternEntry(entry: dict[str, Any]) -> bool:
    return entry.get("SourceSha256") == "NOT_APPLICABLE_PATTERN"


def ReadGitRevision(nypc_root: Path) -> tuple[str | None, str | None]:
    try:
        result = subprocess.run(
            ["git", "-C", str(nypc_root), "rev-parse", "HEAD"],
            capture_output=True,
            check=False,
            text=True,
        )
    except OSError as error:
        return None, f"unable to execute git: {error}"

    if result.returncode != 0:
        detail = result.stderr.strip() or f"exit code {result.returncode}"
        return None, f"unable to read source repository revision: {detail}"

    revision = result.stdout.strip()
    if not revision:
        return None, "source repository revision is empty"
    return revision, None


def ValidateSourceRepository(
    source_repository: Any,
    nypc_root: Path,
) -> list[str]:
    if not isinstance(source_repository, dict):
        return ["SourceRepository must be an object"]

    expected_revision = source_repository.get("Revision")
    if not isinstance(expected_revision, str) or not expected_revision:
        return ["SourceRepository.Revision must be a non-empty string"]

    actual_revision, read_error = ReadGitRevision(nypc_root)
    if read_error is not None:
        return [read_error]
    if actual_revision != expected_revision:
        return [
            "SourceRepository.Revision mismatch: "
            f"expected={expected_revision} actual={actual_revision}"
        ]
    return []


def ValidateEntry(
    entry: dict[str, Any],
    index: int,
    nypc_root: Path,
) -> list[str]:
    errors: list[str] = []
    for field in RequiredEntryFields:
        if field not in entry:
            errors.append(f"Entries[{index}] missing field: {field}")

    disposition = entry.get("Disposition")
    if disposition not in AllowedDispositions:
        errors.append(
            f"Entries[{index}] invalid Disposition: {disposition!r}"
        )

    forbidden = entry.get("ForbiddenDependencies")
    if not isinstance(forbidden, list) or not all(
        isinstance(value, str) and value for value in forbidden
    ):
        errors.append(
            f"Entries[{index}] ForbiddenDependencies must be non-empty strings"
        )

    for field in VersionFields:
        value = entry.get(field)
        if isinstance(value, bool) or not isinstance(value, int) or value < 0:
            errors.append(
                f"Entries[{index}] {field} must be a non-negative integer"
            )

    source_path = entry.get("SourcePath")
    if not isinstance(source_path, str) or not source_path:
        errors.append(f"Entries[{index}] SourcePath must be a non-empty string")
        return errors

    pattern_entry = IsPatternEntry(entry)
    expected_hash = entry.get("SourceSha256")
    if not pattern_entry and (
        not isinstance(expected_hash, str)
        or Sha256Pattern.fullmatch(expected_hash) is None
    ):
        errors.append(
            f"Entries[{index}] SourceSha256 must be 64 lowercase hex characters"
        )

    if disposition == "DoNotCopy":
        if entry.get("TargetOwner") != "None":
            errors.append(
                f"Entries[{index}] DoNotCopy TargetOwner must be 'None'"
            )
        return errors

    if pattern_entry:
        errors.append(
            f"Entries[{index}] pattern hash is only valid for DoNotCopy"
        )
        return errors

    source_root = nypc_root.resolve()
    source = (source_root / Path(source_path)).resolve()
    try:
        source.relative_to(source_root)
    except ValueError:
        errors.append(
            f"Entries[{index}] SourcePath resolves outside NYPC root: "
            f"{source_path}"
        )
        return errors

    if not source.is_file():
        errors.append(f"Entries[{index}] source missing: {source}")
        return errors

    if not isinstance(expected_hash, str) or Sha256Pattern.fullmatch(
        expected_hash
    ) is None:
        return errors

    actual_hash = ComputeSha256(source)
    if actual_hash != expected_hash:
        errors.append(
            f"Entries[{index}] SHA-256 mismatch: {source_path} "
            f"expected={expected_hash} actual={actual_hash}"
        )

    return errors


def ValidateUniqueSourcePaths(
    entries: list[Any],
    nypc_root: Path,
) -> list[str]:
    errors: list[str] = []
    seen: dict[str, int] = {}
    source_root = nypc_root.resolve()

    for index, entry in enumerate(entries):
        if not isinstance(entry, dict):
            continue
        source_path = entry.get("SourcePath")
        if not isinstance(source_path, str) or not source_path:
            continue

        if IsPatternEntry(entry):
            key = source_path.replace("\\", "/").casefold()
        else:
            key = str((source_root / Path(source_path)).resolve()).casefold()

        first_index = seen.get(key)
        if first_index is not None:
            errors.append(
                f"Entries[{index}] duplicates SourcePath from "
                f"Entries[{first_index}]: {source_path}"
            )
        else:
            seen[key] = index

    return errors


def IsForbiddenAIResearchPath(relative_path: Path) -> bool:
    lowered_parts = tuple(part.casefold() for part in relative_path.parts)
    if fnmatch(relative_path.name.casefold(), "run_s*.py"):
        return True
    return any(
        part.startswith("tmp") or part.startswith("scratch")
        for part in lowered_parts
    )


def ValidateDoNotCopyPolicy(
    entries: list[Any],
    winters_root: Path,
) -> list[str]:
    has_pattern_policy = any(
        isinstance(entry, dict)
        and entry.get("Disposition") == "DoNotCopy"
        and IsPatternEntry(entry)
        for entry in entries
    )
    if not has_pattern_policy:
        return []

    resolved_winters_root = winters_root.resolve()
    ai_research_root = (
        resolved_winters_root / "Tools" / "AIResearch"
    ).resolve()
    try:
        ai_research_root.relative_to(resolved_winters_root)
    except ValueError:
        return [
            "Tools/AIResearch resolves outside the configured Winters root"
        ]

    if not ai_research_root.is_dir():
        return [f"Tools/AIResearch directory missing: {ai_research_root}"]

    errors: list[str] = []
    for path in sorted(ai_research_root.rglob("*")):
        relative_path = path.relative_to(ai_research_root)
        if IsForbiddenAIResearchPath(relative_path):
            errors.append(
                "DoNotCopy policy violation in Tools/AIResearch: "
                f"{relative_path.as_posix()}"
            )
    return errors


def Main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate the selective NYPC to Winters AI bridge manifest."
    )
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--nypc-root", required=True, type=Path)
    parser.add_argument("--winters-root", type=Path)
    args = parser.parse_args()

    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    errors: list[str] = []
    winters_root = (
        args.winters_root.resolve()
        if args.winters_root is not None
        else args.manifest.resolve().parents[2]
    )

    if manifest.get("SchemaVersion") != 1:
        errors.append("SchemaVersion must be 1")

    errors.extend(
        ValidateSourceRepository(manifest.get("SourceRepository"), args.nypc_root)
    )

    entries = manifest.get("Entries")
    if not isinstance(entries, list) or not entries:
        errors.append("Entries must be a non-empty list")
    else:
        errors.extend(ValidateUniqueSourcePaths(entries, args.nypc_root))
        for index, value in enumerate(entries):
            if not isinstance(value, dict):
                errors.append(f"Entries[{index}] must be an object")
                continue
            errors.extend(ValidateEntry(value, index, args.nypc_root))
        errors.extend(ValidateDoNotCopyPolicy(entries, winters_root))

    if errors:
        print("NYPC bridge manifest validation FAILED")
        for error in errors:
            print(f"- {error}")
        return 1

    print(
        "NYPC bridge manifest validation PASS: "
        f"entries={len(entries)} sourceRoot={args.nypc_root} "
        f"wintersRoot={winters_root}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(Main())
