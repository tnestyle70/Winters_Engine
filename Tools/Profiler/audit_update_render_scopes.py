#!/usr/bin/env python3
"""Heuristic inventory of Winters Update/Render/Tick/Execute entry points."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Iterator


ENTRY_PATTERN = re.compile(
    r"(?P<qualified>[A-Za-z_][\w:<>~]*::"
    r"(?:OnUpdate|OnLateUpdate|LateUpdate|Update|OnRender|Render|Tick|Execute|OnImGui|ImGui))"
    r"\s*\([^;{}]*\)\s*(?:const\s*)?\{",
    re.MULTILINE,
)


def matching_brace(text: str, open_offset: int) -> int | None:
    depth = 0
    index = open_offset
    in_string: str | None = None
    escaped = False
    line_comment = False
    block_comment = False
    while index < len(text):
        char = text[index]
        nxt = text[index + 1] if index + 1 < len(text) else ""
        if line_comment:
            if char == "\n":
                line_comment = False
            index += 1
            continue
        if block_comment:
            if char == "*" and nxt == "/":
                block_comment = False
                index += 2
            else:
                index += 1
            continue
        if in_string:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == in_string:
                in_string = None
            index += 1
            continue
        if char == "/" and nxt == "/":
            line_comment = True
            index += 2
            continue
        if char == "/" and nxt == "*":
            block_comment = True
            index += 2
            continue
        if char in {'"', "'"}:
            in_string = char
            index += 1
            continue
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return index
        index += 1
    return None


def cpp_files(root: Path, source_roots: list[str]) -> Iterator[Path]:
    for source_root in source_roots:
        base = root / source_root
        if not base.exists():
            continue
        yield from sorted(base.rglob("*.cpp"))


def audit(root: Path, source_roots: list[str]) -> dict[str, object]:
    entries: list[dict[str, object]] = []
    for path in cpp_files(root, source_roots):
        text = path.read_text(encoding="utf-8-sig", errors="replace")
        for match in ENTRY_PATTERN.finditer(text):
            open_offset = match.end() - 1
            close_offset = matching_brace(text, open_offset)
            if close_offset is None:
                continue
            body = text[open_offset + 1 : close_offset]
            line = text.count("\n", 0, match.start()) + 1
            entries.append(
                {
                    "file": path.relative_to(root).as_posix(),
                    "line": line,
                    "function": match.group("qualified"),
                    "directScope": "WINTERS_PROFILE_SCOPE" in body,
                    "scopeCount": body.count("WINTERS_PROFILE_SCOPE"),
                    "counterCount": body.count("WINTERS_PROFILE_COUNT"),
                }
            )

    covered = [entry for entry in entries if entry["directScope"]]
    missing = [entry for entry in entries if not entry["directScope"]]
    by_root: dict[str, dict[str, int]] = {}
    for source_root in source_roots:
        root_name = source_root.split("/", 1)[0]
        root_entries = [
            entry for entry in entries if str(entry["file"]).startswith(f"{root_name}/")
        ]
        root_covered = [entry for entry in root_entries if entry["directScope"]]
        by_root[root_name] = {
            "entryPoints": len(root_entries),
            "directlyScoped": len(root_covered),
            "missingDirectScope": len(root_entries) - len(root_covered),
        }
    return {
        "schema": "WintersProfilerCoverageAudit.v1",
        "heuristic": True,
        "directScopeCoverageIsAcceptanceGate": False,
        "instrumentationPolicy": [
            "Engine/Client frame and subsystem boundaries may use direct scopes.",
            "Repeated entity, behavior-tree, animation, and draw leaves use aggregate counters or on-demand traces.",
            "Server and Shared/GameSim must use dependency-neutral metrics instead of Engine ProfilerAPI includes.",
        ],
        "roots": source_roots,
        "summary": {
            "entryPoints": len(entries),
            "directlyScoped": len(covered),
            "missingDirectScope": len(missing),
            "coveragePercent": round(100.0 * len(covered) / len(entries), 2)
            if entries
            else 0.0,
        },
        "byRoot": by_root,
        "missing": missing,
        "covered": covered,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--out", type=Path)
    parser.add_argument(
        "--source-root",
        action="append",
        dest="source_roots",
        default=[],
    )
    args = parser.parse_args()
    source_roots = args.source_roots or [
        "Engine/Private",
        "Client/Private",
        "Server/Private",
        "Shared/GameSim",
    ]
    root = args.root.resolve()
    result = audit(root, source_roots)
    encoded = json.dumps(result, ensure_ascii=False, indent=2) + "\n"
    if args.out:
        output = args.out if args.out.is_absolute() else root / args.out
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(encoded, encoding="utf-8")
    print(encoded, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
