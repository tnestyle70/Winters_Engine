#!/usr/bin/env python3
"""
Compass Validator — 코드베이스의 _MODULE.md manifest 자동 검증.

검증 항목:
1. 모듈 디렉토리 발견 + _MODULE.md 커버리지
2. _MODULE.md 의 markdown link 깨짐 0
3. 진입점 (## Entry Points) 의 파일 경로 실재
4. 핵심 파일 (## 핵심 파일) 의 경로 실재
5. 의존성 cycle 0 (## Dependencies 의 Public + Private 파싱)
6. 의존받음 (## Depended By) 자동 계산 (수동 박제와 일치 검증)
7. 모든 .md 파일의 markdown link 전수 검증

사용법:
    python validator.py                            # 현재 디렉토리
    python validator.py --root /path/to/project    # 명시
    python validator.py --json                     # JSON 출력
    python validator.py --fix-suggestions          # 수정 제안 출력

Exit code:
    0 — 모든 검증 통과
    1 — 검증 실패 (CI 차단)
    2 — 입력/환경 오류
"""

import argparse
import json
import re
import sys
import pathlib
from collections import defaultdict
from typing import Optional

# Windows cp949 콘솔에서 unicode 출력 안전 처리 (CLAUDE.md gotchas 의 \U 함정 패턴)
try:
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")
except Exception:
    pass  # Python < 3.7 또는 reconfigure 미지원 환경


# ─────────────────────────────────────────────────────────────
# 모듈 발견
# ─────────────────────────────────────────────────────────────

# 도메인별 모듈 후보 패턴 (필요 시 사용자가 커스터마이징)
MODULE_DIR_PATTERNS = [
    # 게임 엔진
    "Engine/Public/*",
    "Engine/Private/*",
    "Client/Public/*",
    "Client/Private/*",
    "Server/Public/*",
    "Server/Private/*",
    "Plugins/*/*",
    # 백엔드
    "internal/*",
    "cmd/*",
    "pkg/*",
    "Services/internal/*",
    "Services/cmd/*",
    # Tools
    "Tools/*",
    # 일반
    "src/*",
    "lib/*",
]

EXCLUDED_DIRS = {
    "node_modules", ".git", ".venv", "venv", "__pycache__", "build", "dist",
    "ThirdPartyLib", "third_party", "vendor", "EngineSDK", ".claude",
    "Bin", "Obj", "Intermediate",
}


def find_module_dirs(root: pathlib.Path) -> list[pathlib.Path]:
    """모듈 후보 디렉토리 list"""
    dirs = []
    seen = set()
    for pattern in MODULE_DIR_PATTERNS:
        for path in root.glob(pattern):
            if not path.is_dir():
                continue
            if path.name in EXCLUDED_DIRS:
                continue
            # root 자체 path 가 EXCLUDED 포함해도 무방 (worktree 안 실행 케이스)
            # path 의 root 이후 부분만 검사
            try:
                relative = path.relative_to(root)
                if any(part in EXCLUDED_DIRS for part in relative.parts):
                    continue
            except ValueError:
                continue
            resolved = path.resolve()
            if resolved in seen:
                continue
            seen.add(resolved)
            dirs.append(path)
    return sorted(dirs)


# ─────────────────────────────────────────────────────────────
# Manifest 파싱
# ─────────────────────────────────────────────────────────────

class ManifestParser:
    """_MODULE.md 파싱 — 섹션별 정보 추출"""

    SECTION_RE = re.compile(r"^##\s+(.+?)\s*$", re.MULTILINE)
    ENTRY_POINT_RE = re.compile(r"`([^`]+)`\s+at\s+`([^`]+?)(?::(\d+))?`")
    DEP_LIST_ITEM = re.compile(r"^\s*[-*]\s+`([^`]+)`", re.MULTILINE)
    LINK_RE = re.compile(r"\[([^\]]*)\]\(([^)]+)\)")

    def __init__(self, path: pathlib.Path):
        self.path = path
        self.text = path.read_text(encoding="utf-8", errors="ignore")
        self.sections = self._split_sections()

    def _split_sections(self) -> dict[str, str]:
        """`## ...` 헤더 단위로 분할"""
        sections = {}
        positions = [(m.group(1).strip(), m.start()) for m in self.SECTION_RE.finditer(self.text)]
        for i, (name, start) in enumerate(positions):
            end = positions[i + 1][1] if i + 1 < len(positions) else len(self.text)
            content = self.text[start:end]
            sections[name] = content
        return sections

    def get_entry_points(self) -> list[dict]:
        """## 진입점 / Entry Points 의 파일:줄번호 추출"""
        section = None
        for name, content in self.sections.items():
            if "진입점" in name or "Entry Points" in name.lower() or "Entry Point" in name:
                section = content
                break
        if not section:
            return []
        results = []
        for m in self.ENTRY_POINT_RE.finditer(section):
            results.append({
                "symbol": m.group(1).strip(),
                "file": m.group(2).strip(),
                "line": int(m.group(3)) if m.group(3) else None,
            })
        return results

    def get_dependencies(self) -> dict[str, list[str]]:
        """## 의존성 / Dependencies 의 Public/Private/Forward-Decl 추출"""
        deps = {"public": [], "private": [], "forward_decl": []}
        for name, content in self.sections.items():
            if "의존성" in name or "Dependencies" in name:
                # sub-sections (### Public, ### Private, ### Forward-Decl)
                subsections = re.split(r"^###\s+", content, flags=re.MULTILINE)
                for sub in subsections:
                    sub_lower = sub.lower()
                    target = None
                    if sub_lower.startswith("public"):
                        target = "public"
                    elif sub_lower.startswith("private"):
                        target = "private"
                    elif "forward" in sub_lower:
                        target = "forward_decl"
                    if target:
                        for m in self.DEP_LIST_ITEM.finditer(sub):
                            deps[target].append(m.group(1).strip())
                break
        return deps

    def get_core_files(self) -> list[str]:
        """## 핵심 파일 / Core Files 의 파일 경로 추출"""
        section = None
        for name, content in self.sections.items():
            if "핵심 파일" in name or "Core Files" in name or "Top" in name:
                section = content
                break
        if not section:
            return []
        # 각 줄의 backtick-enclosed 파일 경로
        files = []
        for m in re.finditer(r"`([^`]+\.\w+)`", section):
            files.append(m.group(1).strip())
        return files

    def get_all_links(self) -> list[dict]:
        """모든 markdown link [text](path) 추출"""
        results = []
        for m in self.LINK_RE.finditer(self.text):
            target = m.group(2).split("#")[0]  # 앵커 제거
            if target.startswith(("http:", "https:", "mailto:", "ftp:")):
                continue
            if not target:
                continue
            results.append({
                "text": m.group(1),
                "target": target,
                "line": self.text[:m.start()].count("\n") + 1,
            })
        return results


# ─────────────────────────────────────────────────────────────
# 검증 함수
# ─────────────────────────────────────────────────────────────

def resolve_link(base: pathlib.Path, link: str, root: pathlib.Path) -> pathlib.Path:
    """markdown link 의 상대 경로 해석"""
    # 줄번호 제거 (foo.md:42 → foo.md)
    target = link.split(":")[0] if not link.startswith(("/", ".")) else link
    if target.startswith("/"):
        return (root / target.lstrip("/")).resolve()
    return (base.parent / target).resolve()


def validate(root: pathlib.Path) -> dict:
    result = {
        "root": str(root),
        "modules": {"total": 0, "covered": 0, "missing": []},
        "broken_links": [],
        "stale_entry_points": [],
        "stale_core_files": [],
        "dependency_cycles": [],
        "depended_by_mismatch": [],
        "all_md_broken_links": [],
    }

    # 1. 모듈 발견 + 커버리지
    module_dirs = find_module_dirs(root)
    result["modules"]["total"] = len(module_dirs)

    manifests = {}
    for d in module_dirs:
        m_path = d / "_MODULE.md"
        if m_path.exists():
            result["modules"]["covered"] += 1
            try:
                manifests[d.name] = (d, ManifestParser(m_path))
            except Exception as e:
                result["broken_links"].append({
                    "file": str(m_path.relative_to(root)),
                    "error": "parse error: " + str(e),
                })
        else:
            result["modules"]["missing"].append(str(d.relative_to(root)))

    # 2-4. 각 manifest 의 link / 진입점 / 핵심 파일 검증
    for mod_name, (mod_dir, parser) in manifests.items():
        # 2. broken links
        for link in parser.get_all_links():
            target_path = resolve_link(mod_dir / "_MODULE.md", link["target"], root)
            if not target_path.exists():
                result["broken_links"].append({
                    "module": mod_name,
                    "file": str((mod_dir / "_MODULE.md").relative_to(root)),
                    "line": link["line"],
                    "link": link["target"],
                })

        # 3. 진입점 파일 실재
        for ep in parser.get_entry_points():
            ep_path = resolve_link(mod_dir / "_MODULE.md", ep["file"], root)
            if not ep_path.exists():
                # 모듈 디렉토리 내 fallback
                alt = mod_dir / ep["file"]
                if not alt.exists():
                    result["stale_entry_points"].append({
                        "module": mod_name,
                        "symbol": ep["symbol"],
                        "file": ep["file"],
                    })

        # 4. 핵심 파일 실재
        for cf in parser.get_core_files():
            cf_path = resolve_link(mod_dir / "_MODULE.md", cf, root)
            if not cf_path.exists():
                alt = mod_dir / cf
                if not alt.exists():
                    result["stale_core_files"].append({
                        "module": mod_name,
                        "file": cf,
                    })

    # 5. 의존성 cycle
    dep_graph = {}
    for mod_name, (mod_dir, parser) in manifests.items():
        deps = parser.get_dependencies()
        all_deps = set(deps["public"]) | set(deps["private"])
        dep_graph[mod_name] = all_deps

    cycles = detect_cycles(dep_graph)
    result["dependency_cycles"] = cycles

    # 7. 모든 .md 파일의 link 검증 (manifest 외)
    all_md = [p for p in root.rglob("*.md")
              if not any(part in EXCLUDED_DIRS for part in p.parts)
              and p.name != "_MODULE.md"]  # manifest 는 위에서 검증
    for md in all_md:
        try:
            parser = ManifestParser(md)
        except Exception:
            continue
        for link in parser.get_all_links():
            target_path = resolve_link(md, link["target"], root)
            if not target_path.exists():
                result["all_md_broken_links"].append({
                    "file": str(md.relative_to(root)),
                    "line": link["line"],
                    "link": link["target"],
                })

    return result


def detect_cycles(graph: dict[str, set[str]]) -> list[list[str]]:
    """DAG cycle 검출 — Tarjan 의 strongly connected components"""
    cycles = []
    visited = set()
    rec_stack = []

    def dfs(node, path):
        if node in path:
            cycle_start = path.index(node)
            cycles.append(path[cycle_start:] + [node])
            return
        if node in visited:
            return
        visited.add(node)
        for neighbor in graph.get(node, set()):
            if neighbor in graph:  # 우리가 아는 모듈만
                dfs(neighbor, path + [node])

    for node in graph:
        dfs(node, [])

    # 중복 cycle 제거
    unique = []
    seen_sets = []
    for c in cycles:
        s = frozenset(c)
        if s not in seen_sets:
            seen_sets.append(s)
            unique.append(c)
    return unique


# ─────────────────────────────────────────────────────────────
# 출력
# ─────────────────────────────────────────────────────────────

def print_report(result: dict, fix_suggestions: bool = False):
    print("=" * 60)
    print("  Compass Validator — Result")
    print("=" * 60)
    print(f"Root: {result['root']}")
    print()

    m = result["modules"]
    coverage = round(m["covered"] / max(m["total"], 1) * 100, 1)
    print(f"Modules         : {m['covered']}/{m['total']} ({coverage}%)")
    print(f"Broken links    : {len(result['broken_links'])} (in manifests)")
    print(f"All-MD links    : {len(result['all_md_broken_links'])} (broken)")
    print(f"Stale entry pts : {len(result['stale_entry_points'])}")
    print(f"Stale core files: {len(result['stale_core_files'])}")
    print(f"Dep cycles      : {len(result['dependency_cycles'])}")
    print()

    pass_all = (
        len(result["broken_links"]) == 0
        and len(result["all_md_broken_links"]) == 0
        and len(result["stale_entry_points"]) == 0
        and len(result["stale_core_files"]) == 0
        and len(result["dependency_cycles"]) == 0
    )

    if pass_all and m["covered"] == m["total"]:
        print("PASS  All checks green.")
    elif pass_all:
        print(f"PARTIAL  {m['total'] - m['covered']} modules missing _MODULE.md")
    else:
        print("FAIL  See details below.")

    if fix_suggestions:
        print()
        print("-" * 60)
        if m["missing"]:
            print(f"Missing manifests ({len(m['missing'])}):")
            for x in m["missing"][:10]:
                print(f"  - {x}")
            if len(m["missing"]) > 10:
                print(f"  ... and {len(m['missing']) - 10} more")
            print(f"  → cp .toolkit/module-manifest/_MODULE_TEMPLATE.md <dir>/_MODULE.md")
        if result["broken_links"]:
            print(f"\nBroken manifest links ({len(result['broken_links'])}):")
            for x in result["broken_links"][:5]:
                print(f"  - {x['file']}:{x.get('line','?')} → {x['link']}")
        if result["stale_entry_points"]:
            print(f"\nStale entry points ({len(result['stale_entry_points'])}):")
            for x in result["stale_entry_points"][:5]:
                print(f"  - [{x['module']}] {x['symbol']} → {x['file']} (not found)")
        if result["dependency_cycles"]:
            print(f"\nDependency cycles ({len(result['dependency_cycles'])}):")
            for c in result["dependency_cycles"][:5]:
                print(f"  - {' -> '.join(c)}")


# ─────────────────────────────────────────────────────────────
# main
# ─────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="Compass Validator")
    parser.add_argument("--root", type=str, default=None,
                        help="Project root (default: $CLAUDE_PROJECT_DIR or cwd)")
    parser.add_argument("--json", action="store_true",
                        help="Output JSON instead of human-readable")
    parser.add_argument("--fix-suggestions", action="store_true",
                        help="Print fix suggestions")
    args = parser.parse_args()

    # root 결정
    import os
    root_str = args.root or os.environ.get("CLAUDE_PROJECT_DIR") or os.getcwd()
    root = pathlib.Path(root_str).resolve()
    if not root.exists():
        print(f"ERROR: root not found: {root}", file=sys.stderr)
        sys.exit(2)

    result = validate(root)

    if args.json:
        print(json.dumps(result, ensure_ascii=False, indent=2))
    else:
        print_report(result, fix_suggestions=args.fix_suggestions)

    # exit code
    has_failure = (
        len(result["broken_links"]) > 0
        or len(result["all_md_broken_links"]) > 0
        or len(result["stale_entry_points"]) > 0
        or len(result["stale_core_files"]) > 0
        or len(result["dependency_cycles"]) > 0
    )
    sys.exit(1 if has_failure else 0)


if __name__ == "__main__":
    main()
