#!/usr/bin/env python3
"""
AI-Readiness Scorer — 100점 척도로 코드베이스의 AI 협업 효율 측정.

7 카테고리 × 100점:
  A 탐색·커버리지   15 (A1, A2, A3)
  B 컨텍스트 품질   20 (B1, B2, B3, B4, B5)
  C 암묵지 명시화   20 (C1, C2, C3)
  D 의존성 매핑     15 (D1, D2, D3)
  E 검증 게이트     15 (E1, E2, E3)
  F 최신성·자동화   10 (F1, F2)
  G 성과 지표        5 (G1, G2)

등급:
  90-100  AI-Optimized       grep 0회, manifest 만 진입
  75-89   AI-Ready           grep 1회, manifest 정상
  60-74   Onboarding-Ready   grep 1-2회, manifest 일부 활용
  40-59   Onboarding         grep 3-5회, 핵심 박제는 있음
  0-39    Pre-Onboarding     매번 전수 탐색

사용법:
    python score.py                                # 현재 디렉토리
    python score.py --root /path/to/project        # 명시
    python score.py --json                         # JSON 만
    python score.py --no-actions                   # ROI 액션 출력 X

저장:
    .ai-readiness/<YYYY-MM-DD>.json
    .ai-readiness/latest.json (symlink 또는 copy)
"""

import argparse
import json
import os
import pathlib
import re
import sys
from collections import Counter
from datetime import date

try:
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")
except Exception:
    pass


# ─────────────────────────────────────────────────────────────
# 공통 — find_module_dirs (validator 와 동일 로직, 독립 import 회피)
# ─────────────────────────────────────────────────────────────

MODULE_DIR_PATTERNS = [
    "Engine/Public/*", "Engine/Private/*",
    "Client/Public/*", "Client/Private/*",
    "Server/Public/*", "Server/Private/*",
    "Plugins/*/*",
    "internal/*", "cmd/*", "pkg/*",
    "Services/internal/*", "Services/cmd/*",
    "Tools/*",
    "src/*", "lib/*",
]

EXCLUDED_DIRS = {
    "node_modules", ".git", ".venv", "venv", "__pycache__", "build", "dist",
    "ThirdPartyLib", "third_party", "vendor", "EngineSDK", ".claude",
    "Bin", "Obj", "Intermediate", "out", "target",
}

EXCLUDED_FROM_MD_SCAN = EXCLUDED_DIRS | {".ai-readiness", ".toolkit"}


def find_module_dirs(root):
    dirs, seen = [], set()
    for pattern in MODULE_DIR_PATTERNS:
        for path in root.glob(pattern):
            if not path.is_dir() or path.name in EXCLUDED_DIRS:
                continue
            try:
                rel = path.relative_to(root)
                if any(part in EXCLUDED_DIRS for part in rel.parts):
                    continue
            except ValueError:
                continue
            r = path.resolve()
            if r in seen:
                continue
            seen.add(r)
            dirs.append(path)
    return sorted(dirs)


def find_md_files(root, include_module_md=True):
    """모든 .md 파일 (excluded 제외)"""
    files = []
    for p in root.rglob("*.md"):
        if any(part in EXCLUDED_FROM_MD_SCAN for part in p.relative_to(root).parts):
            continue
        if not include_module_md and p.name == "_MODULE.md":
            continue
        files.append(p)
    return files


def read_text_safe(path):
    try:
        return path.read_text(encoding="utf-8", errors="ignore")
    except Exception:
        return ""


# ─────────────────────────────────────────────────────────────
# Measure 함수 — 18개 sub
# ─────────────────────────────────────────────────────────────

# A 탐색·커버리지 (15pt)

def measure_A1_module_coverage(root):
    """모듈 manifest 커버 (5pt) — _MODULE.md 비율"""
    dirs = find_module_dirs(root)
    if not dirs:
        return 0.0, {"total": 0, "covered": 0, "missing": []}
    covered = sum(1 for d in dirs if (d / "_MODULE.md").exists())
    score = round(covered / len(dirs) * 5, 2)
    missing = [str(d.relative_to(root)) for d in dirs
               if not (d / "_MODULE.md").exists()][:10]
    return score, {"total": len(dirs), "covered": covered, "missing": missing}


def measure_A2_entry_point_validity(root):
    """진입점 정확성 (5pt) — _MODULE.md 의 진입점 파일 실재 비율"""
    pattern = re.compile(r"`([^`]+)`\s+at\s+`([^`]+?)(?::\d+)?`")
    total, valid = 0, 0
    invalid = []
    for d in find_module_dirs(root):
        mp = d / "_MODULE.md"
        if not mp.exists():
            continue
        text = read_text_safe(mp)
        for m in pattern.finditer(text):
            total += 1
            target = m.group(2)
            target_path = (d / target).resolve() if not target.startswith("/") else (root / target.lstrip("/")).resolve()
            if target_path.exists() or (d / target).exists():
                valid += 1
            else:
                invalid.append({"module": d.name, "symbol": m.group(1), "file": target})
    if total == 0:
        return 0.0, {"total": 0, "valid": 0, "note": "no entry points found"}
    score = round(valid / total * 5, 2)
    return score, {"total": total, "valid": valid, "invalid": invalid[:5]}


def measure_A3_folder_readme_depth(root):
    """폴더 README 깊이 (5pt) — depth 3+ 폴더에 README/_MODULE.md 비율"""
    deep_folders = []
    for p in root.rglob("*"):
        if not p.is_dir():
            continue
        try:
            rel = p.relative_to(root)
        except ValueError:
            continue
        if any(part in EXCLUDED_FROM_MD_SCAN for part in rel.parts):
            continue
        if len(rel.parts) >= 3:
            deep_folders.append(p)
    if not deep_folders:
        return 0.0, {"total": 0, "covered": 0}
    covered = sum(1 for p in deep_folders if (p / "README.md").exists() or (p / "_MODULE.md").exists())
    score = round(covered / len(deep_folders) * 5, 2)
    return score, {"total": len(deep_folders), "covered": covered}


# B 컨텍스트 품질 (20pt)

def measure_B1_brevity(root):
    """간결성 (4pt) — CLAUDE.md (또는 README.md) 줄 수"""
    primary = root / "CLAUDE.md"
    if not primary.exists():
        primary = root / "README.md"
    if not primary.exists():
        return 0.0, {"file": None, "lines": 0}
    lines = sum(1 for _ in primary.open(encoding="utf-8", errors="ignore"))
    if lines <= 500:
        score = 4.0
    elif lines <= 1000:
        score = 2.0
    elif lines <= 1500:
        score = 1.0
    else:
        score = 0.0
    return score, {"file": str(primary.relative_to(root)), "lines": lines}


def measure_B2_command_examples(root):
    """명령어 예시 (4pt) — CLAUDE.md + _MODULE.md 의 ```bash/sh/cmd 블록 수"""
    md_files = []
    for f in [root / "CLAUDE.md", root / "README.md"]:
        if f.exists():
            md_files.append(f)
    md_files.extend(d / "_MODULE.md" for d in find_module_dirs(root) if (d / "_MODULE.md").exists())
    pat = re.compile(r"```(?:bash|sh|cmd|powershell|ps1|console)\b")
    count = sum(len(pat.findall(read_text_safe(f))) for f in md_files)
    if count >= 5:
        score = 4.0
    elif count >= 3:
        score = 2.0
    elif count >= 1:
        score = 1.0
    else:
        score = 0.0
    return score, {"count": count, "scanned_files": len(md_files)}


def measure_B3_core_file_refs(root):
    """핵심파일 참조 (4pt) — 파일당 markdown link 평균"""
    md_files = [root / "CLAUDE.md"] + [d / "_MODULE.md" for d in find_module_dirs(root)
                                        if (d / "_MODULE.md").exists()]
    md_files = [f for f in md_files if f.exists()]
    if not md_files:
        return 0.0, {"avg": 0.0}
    pat = re.compile(r"\[([^\]]+)\]\(([^)]+)\)")
    total_links = 0
    for f in md_files:
        text = read_text_safe(f)
        for m in pat.finditer(text):
            target = m.group(2)
            if target.startswith(("http", "mailto:")):
                continue
            total_links += 1
    avg = total_links / len(md_files)
    if avg >= 3:
        score = 4.0
    elif avg >= 2:
        score = 2.0
    elif avg >= 1:
        score = 1.0
    else:
        score = 0.0
    return score, {"avg": round(avg, 2), "total_links": total_links, "files": len(md_files)}


def measure_B4_forbidden_patterns(root):
    """금지 패턴 (4pt) — 금지/X/Don't 키워드 카운트"""
    md_files = [root / "CLAUDE.md"] + [d / "_MODULE.md" for d in find_module_dirs(root)
                                        if (d / "_MODULE.md").exists()]
    md_files = [f for f in md_files if f.exists()]
    pat = re.compile(r"금지|❌|Don't|don't|never|Never|do not|Do not|MUST NOT|FORBIDDEN", re.IGNORECASE)
    count = sum(len(pat.findall(read_text_safe(f))) for f in md_files)
    if count >= 10:
        score = 4.0
    elif count >= 5:
        score = 2.0
    elif count >= 1:
        score = 1.0
    else:
        score = 0.0
    return score, {"count": count}


def measure_B5_cross_refs(root):
    """크로스참조 (4pt) — @-style 또는 [link](*.md) 평균"""
    md_files = [root / "CLAUDE.md"] + [d / "_MODULE.md" for d in find_module_dirs(root)
                                        if (d / "_MODULE.md").exists()]
    md_files = [f for f in md_files if f.exists()]
    if not md_files:
        return 0.0, {"avg": 0.0}
    at_pat = re.compile(r"@\.[\w/.-]+\.md")
    md_link_pat = re.compile(r"\[[^\]]+\]\([^)]+\.md\)")
    total = 0
    for f in md_files:
        text = read_text_safe(f)
        total += len(at_pat.findall(text)) + len(md_link_pat.findall(text))
    avg = total / len(md_files)
    if avg >= 5:
        score = 4.0
    elif avg >= 3:
        score = 2.0
    elif avg >= 1:
        score = 1.0
    else:
        score = 0.0
    return score, {"avg": round(avg, 2), "total": total}


# C 암묵지 명시화 (20pt)

def measure_C1_gotchas(root):
    """Gotchas/함정 박제 (8pt)"""
    md_files = find_md_files(root)
    pat = re.compile(r"gotcha|함정|⚠️|⚠|주의|warning|caveat|pitfall", re.IGNORECASE)
    count = sum(len(pat.findall(read_text_safe(f))) for f in md_files)
    if count >= 50:
        score = 8.0
    elif count >= 30:
        score = 6.0
    elif count >= 10:
        score = 4.0
    elif count >= 1:
        score = 2.0
    else:
        score = 0.0
    return score, {"count": count, "scanned_files": len(md_files)}


def measure_C2_adr(root):
    """ADR (6pt) — decisions/ 폴더의 .md 파일"""
    candidates = [
        root / ".md/architecture/decisions",
        root / ".md/adr",
        root / "docs/decisions",
        root / "docs/adr",
        root / "decisions",
        root / "adr",
    ]
    n = 0
    found_dirs = []
    for c in candidates:
        if c.exists() and c.is_dir():
            md_count = len(list(c.glob("*.md")))
            n += md_count
            if md_count > 0:
                found_dirs.append(str(c.relative_to(root)))
    if n >= 10:
        score = 6.0
    elif n >= 5:
        score = 4.0
    elif n >= 1:
        score = 2.0
    else:
        score = 0.0
    return score, {"adr_count": n, "dirs": found_dirs}


def measure_C3_skill_lesson(root):
    """Skill/Lesson 자료 (6pt)"""
    candidates = [
        root / "winters-skills",
        root / ".claude/skills",
        root / ".md/guide",
        root / ".md/guides",
        root / ".md/lessons",
        root / "skills",
        root / "guides",
        root / "docs/guides",
    ]
    n = 0
    found_dirs = []
    for c in candidates:
        if c.exists() and c.is_dir():
            md_count = sum(1 for _ in c.rglob("*.md"))
            n += md_count
            if md_count > 0:
                found_dirs.append(str(c.relative_to(root)))
    if n >= 20:
        score = 6.0
    elif n >= 10:
        score = 4.0
    elif n >= 5:
        score = 2.0
    else:
        score = 0.0
    return score, {"count": n, "dirs": found_dirs}


# D 의존성 매핑 (15pt)

def measure_D1_module_graph(root):
    """Module Graph (5pt) — MODULE_GRAPH.md 또는 동급 존재"""
    candidates = [
        root / ".md/architecture/MODULE_GRAPH.md",
        root / "docs/MODULE_GRAPH.md",
        root / "MODULE_GRAPH.md",
        root / ".md/MODULE_GRAPH.md",
    ]
    found = next((c for c in candidates if c.exists()), None)
    score = 5.0 if found else 0.0
    return score, {"file": str(found.relative_to(root)) if found else None}


def measure_D2_mermaid(root):
    """Mermaid 시각화 (5pt) — 모든 .md 의 ```mermaid 블록"""
    md_files = find_md_files(root)
    pat = re.compile(r"```mermaid\b")
    count = sum(len(pat.findall(read_text_safe(f))) for f in md_files)
    if count >= 3:
        score = 5.0
    elif count >= 1:
        score = 3.0
    else:
        score = 0.0
    return score, {"count": count}


def measure_D3_code_index(root):
    """Code Index (5pt) — CODE_INDEX.md / ENTRY_POINTS.md / INDEX.md 존재"""
    candidates = [
        root / ".md/architecture/CODE_INDEX.md",
        root / ".md/architecture/ENTRY_POINTS.md",
        root / ".md/CODE_INDEX.md",
        root / "CODE_INDEX.md",
        root / "INDEX.md",
        root / "docs/INDEX.md",
    ]
    found = next((c for c in candidates if c.exists()), None)
    score = 5.0 if found else 0.0
    return score, {"file": str(found.relative_to(root)) if found else None}


# E 검증 게이트 (15pt)

def measure_E1_broken_links(root):
    """깨진 link (5pt) — 모든 .md 의 markdown link 검증"""
    md_files = find_md_files(root)
    pat = re.compile(r"\[([^\]]+)\]\(([^)]+)\)")
    broken = []
    for f in md_files:
        text = read_text_safe(f)
        for m in pat.finditer(text):
            target = m.group(2).split("#")[0].split(":")[0]
            if not target or target.startswith(("http:", "https:", "mailto:", "ftp:")):
                continue
            if target.startswith("/"):
                target_path = (root / target.lstrip("/")).resolve()
            else:
                target_path = (f.parent / target).resolve()
            if not target_path.exists():
                broken.append({"file": str(f.relative_to(root)), "link": target})
    n = len(broken)
    if n == 0:
        score = 5.0
    elif n <= 5:
        score = 3.0
    elif n <= 10:
        score = 1.0
    else:
        score = 0.0
    return score, {"broken_count": n, "samples": broken[:5]}


def measure_E2_doc_code_sync(root):
    """코드↔문서 동기 (5pt) — _MODULE.md 진입점/핵심파일 실재 비율"""
    pat_entry = re.compile(r"`([^`]+)`\s+at\s+`([^`]+?)(?::\d+)?`")
    pat_core = re.compile(r"^\d+\.\s+`([^`]+\.\w+)`", re.MULTILINE)
    total, valid = 0, 0
    for d in find_module_dirs(root):
        mp = d / "_MODULE.md"
        if not mp.exists():
            continue
        text = read_text_safe(mp)
        for m in pat_entry.finditer(text):
            total += 1
            tp = (d / m.group(2)).resolve() if not m.group(2).startswith("/") else (root / m.group(2).lstrip("/")).resolve()
            if tp.exists() or (d / m.group(2)).exists():
                valid += 1
        for m in pat_core.finditer(text):
            total += 1
            tp = d / m.group(1)
            if tp.exists():
                valid += 1
    if total == 0:
        return 0.0, {"total": 0, "valid": 0}
    score = round(valid / total * 5, 2)
    return score, {"total": total, "valid": valid}


def measure_E3_ci_hook(root):
    """CI/Hook 강제 (5pt) — .claude/settings.json hooks + .github/workflows/"""
    has_settings = (root / ".claude" / "settings.json").exists() or \
                   (root / ".claude" / "settings.local.json").exists()
    workflow_dir = root / ".github" / "workflows"
    workflows = list(workflow_dir.glob("*.y*ml")) if workflow_dir.exists() else []
    has_workflow = len(workflows) > 0

    has_hooks_in_settings = False
    if has_settings:
        for sp in [root / ".claude" / "settings.json", root / ".claude" / "settings.local.json"]:
            if sp.exists():
                try:
                    data = json.loads(read_text_safe(sp))
                    if data.get("hooks"):
                        has_hooks_in_settings = True
                        break
                except Exception:
                    pass

    if has_hooks_in_settings and has_workflow:
        score = 5.0
    elif has_hooks_in_settings or has_workflow:
        score = 2.0
    else:
        score = 0.0
    return score, {
        "has_hooks_in_settings": has_hooks_in_settings,
        "workflow_count": len(workflows),
    }


# F 최신성·자동화 (10pt)

def measure_F1_hook_count(root):
    """Hook 등록 수 (5pt) — .claude/settings.json 의 hooks 카운트"""
    n = 0
    for sp in [root / ".claude" / "settings.json", root / ".claude" / "settings.local.json"]:
        if not sp.exists():
            continue
        try:
            data = json.loads(read_text_safe(sp))
            hooks = data.get("hooks", {})
            for event_hooks in hooks.values():
                if isinstance(event_hooks, list):
                    n += len(event_hooks)
        except Exception:
            pass
    if n >= 5:
        score = 5.0
    elif n >= 3:
        score = 3.0
    elif n >= 1:
        score = 1.0
    else:
        score = 0.0
    return score, {"count": n}


def measure_F2_auto_scripts(root):
    """자동 스크립트 (5pt) — .bat / .sh / Tools/*/main.py 카운트"""
    n = 0
    for ext in ["*.bat", "*.sh"]:
        for p in root.rglob(ext):
            if any(part in EXCLUDED_FROM_MD_SCAN for part in p.relative_to(root).parts):
                continue
            n += 1
    tools_dir = root / "Tools"
    if tools_dir.exists():
        for p in tools_dir.rglob("main.py"):
            n += 1
        for p in tools_dir.rglob("__main__.py"):
            n += 1
    if n >= 5:
        score = 5.0
    elif n >= 3:
        score = 3.0
    elif n >= 1:
        score = 1.0
    else:
        score = 0.0
    return score, {"count": n}


# G 성과 지표 (5pt)

def measure_G1_evals(root):
    """evals/ (3pt)"""
    candidates = [root / "evals", root / "tests/evals", root / ".evals"]
    n = 0
    found_dir = None
    for c in candidates:
        if c.exists() and c.is_dir():
            json_count = sum(1 for _ in c.rglob("*.json"))
            n += json_count
            if json_count > 0 and found_dir is None:
                found_dir = c
    if n >= 5:
        score = 3.0
    elif n >= 1:
        score = 1.0
    else:
        score = 0.0
    return score, {"count": n, "dir": str(found_dir.relative_to(root)) if found_dir else None}


def measure_G2_closing_report(root):
    """Closing report / Backlog (2pt)"""
    closing_dirs = [
        root / ".claude/closing-reports",
        root / ".md/closing-report",
        root / "closing-reports",
    ]
    backlog_dirs = [
        root / ".claude/backlog",
        root / ".md/backlog",
        root / "backlog",
    ]
    has_closing = any(d.exists() and any(d.iterdir()) for d in closing_dirs if d.exists())
    has_backlog = any(d.exists() and any(d.iterdir()) for d in backlog_dirs if d.exists())

    # Or "★ 다음" / "TODO" / "Roadmap" 섹션 in CLAUDE.md
    if not has_backlog:
        for f in [root / "CLAUDE.md", root / "README.md"]:
            if f.exists():
                text = read_text_safe(f)
                if re.search(r"##\s+(?:★\s+)?(?:다음|Next|Roadmap|TODO|Backlog)", text, re.IGNORECASE):
                    has_backlog = True
                    break

    if has_closing and has_backlog:
        score = 2.0
    elif has_closing or has_backlog:
        score = 1.0
    else:
        score = 0.0
    return score, {"has_closing": has_closing, "has_backlog": has_backlog}


# ─────────────────────────────────────────────────────────────
# Rubric 정의
# ─────────────────────────────────────────────────────────────

CATEGORIES = {
    "A": {
        "name": "탐색·커버리지",
        "max": 15,
        "subs": [
            ("A1", "모듈 manifest 커버", measure_A1_module_coverage, 5),
            ("A2", "진입점 정확성", measure_A2_entry_point_validity, 5),
            ("A3", "폴더 README 깊이", measure_A3_folder_readme_depth, 5),
        ],
    },
    "B": {
        "name": "컨텍스트 품질",
        "max": 20,
        "subs": [
            ("B1", "간결성", measure_B1_brevity, 4),
            ("B2", "명령어 예시", measure_B2_command_examples, 4),
            ("B3", "핵심파일 참조", measure_B3_core_file_refs, 4),
            ("B4", "금지 패턴", measure_B4_forbidden_patterns, 4),
            ("B5", "크로스참조", measure_B5_cross_refs, 4),
        ],
    },
    "C": {
        "name": "암묵지 명시화",
        "max": 20,
        "subs": [
            ("C1", "Gotchas/함정 박제", measure_C1_gotchas, 8),
            ("C2", "ADR 박제", measure_C2_adr, 6),
            ("C3", "Skill/Lesson 자료", measure_C3_skill_lesson, 6),
        ],
    },
    "D": {
        "name": "의존성 매핑",
        "max": 15,
        "subs": [
            ("D1", "Module Graph", measure_D1_module_graph, 5),
            ("D2", "Mermaid 시각화", measure_D2_mermaid, 5),
            ("D3", "Code Index", measure_D3_code_index, 5),
        ],
    },
    "E": {
        "name": "검증 게이트",
        "max": 15,
        "subs": [
            ("E1", "깨진 link 검증", measure_E1_broken_links, 5),
            ("E2", "코드↔문서 동기", measure_E2_doc_code_sync, 5),
            ("E3", "CI/Hook 강제", measure_E3_ci_hook, 5),
        ],
    },
    "F": {
        "name": "최신성·자동화",
        "max": 10,
        "subs": [
            ("F1", "Hook 등록 수", measure_F1_hook_count, 5),
            ("F2", "자동 스크립트", measure_F2_auto_scripts, 5),
        ],
    },
    "G": {
        "name": "성과 지표",
        "max": 5,
        "subs": [
            ("G1", "evals/ 디렉토리", measure_G1_evals, 3),
            ("G2", "Closing report / Backlog", measure_G2_closing_report, 2),
        ],
    },
}


def grade(score):
    if score >= 90: return "AI-Optimized"
    if score >= 75: return "AI-Ready"
    if score >= 60: return "Onboarding-Ready"
    if score >= 40: return "Onboarding"
    return "Pre-Onboarding"


# ─────────────────────────────────────────────────────────────
# 실행 + ROI 액션
# ─────────────────────────────────────────────────────────────

def load_actions(toolkit_root):
    """actions.json 로드 (없으면 빈 dict)"""
    actions_path = toolkit_root / "ai-readiness-scorer" / "actions.json"
    if actions_path.exists():
        try:
            return json.loads(read_text_safe(actions_path))
        except Exception:
            pass
    return {}


def generate_roi_actions(result, actions_db):
    """미달 sub 별 ROI 정렬 액션 생성"""
    candidates = []
    for cat_id, cat in result["categories"].items():
        for sub in cat["subs"]:
            sub_id = sub["id"]
            delta = sub["max"] - sub["score"]
            if delta < 0.5:
                continue
            action_info = actions_db.get(sub_id, {})
            effort = action_info.get("effort", "M")
            effort_hours = {"S": 4, "M": 16, "L": 40}.get(effort, 16)
            roi = delta / effort_hours
            candidates.append({
                "sub_id": sub_id,
                "delta_pt": round(delta, 2),
                "effort": effort,
                "title": action_info.get("title", f"미달 sub {sub_id} 보강"),
                "reference": action_info.get("reference", ""),
                "roi": roi,
            })
    candidates.sort(key=lambda x: -x["roi"])
    return candidates[:10]


def run_score(root):
    result = {
        "date": date.today().isoformat(),
        "root": str(root),
        "total": 0.0,
        "grade": "",
        "categories": {},
    }
    total = 0.0
    for cat_id, cat in CATEGORIES.items():
        cat_score = 0.0
        cat_subs = []
        for sub_id, name, fn, max_pts in cat["subs"]:
            try:
                score, detail = fn(root)
            except Exception as e:
                score, detail = 0.0, {"error": str(e)}
            cat_score += score
            cat_subs.append({
                "id": sub_id,
                "name": name,
                "score": score,
                "max": max_pts,
                "detail": detail,
            })
        result["categories"][cat_id] = {
            "name": cat["name"],
            "score": round(cat_score, 2),
            "max": cat["max"],
            "subs": cat_subs,
        }
        total += cat_score
    result["total"] = round(total, 2)
    result["grade"] = grade(total)
    return result


# ─────────────────────────────────────────────────────────────
# 출력
# ─────────────────────────────────────────────────────────────

def print_report(result, actions=None):
    print("=" * 68)
    print(f" AI-Ready Rubric — Result")
    print("=" * 68)
    print(f" Score : {result['total']}/100  |  Grade : {result['grade']}")
    print(f" Saved : .ai-readiness/{result['date']}.json")
    print("=" * 68)
    print(" 카테고리별 소견")
    for cat_id, cat in result["categories"].items():
        ratio = cat["score"] / cat["max"] if cat["max"] else 0
        if ratio >= 0.9:
            mark = "[OK]"
        elif ratio >= 0.5:
            mark = "[--]"
        else:
            mark = "[XX]"
        print(f"  {cat_id} {cat['name']:14s}  {cat['score']:5.1f}/{cat['max']:<3d}  {mark}")
        # 미달 sub 만 표시
        for sub in cat["subs"]:
            if sub["score"] < sub["max"] * 0.9:
                print(f"        {sub['id']} {sub['name']:20s}  {sub['score']:.1f}/{sub['max']}")
    print("=" * 68)
    if actions:
        print(" ROI 순 액션 목록 (상위 10)")
        for i, a in enumerate(actions, 1):
            print(f"  [{i}] [{a['effort']}] +{a['delta_pt']:.1f}pt  {a['title']}")
            if a.get("reference"):
                print(f"      참조: {a['reference']}")
        print("=" * 68)


def save_result(root, result):
    out_dir = root / ".ai-readiness"
    out_dir.mkdir(exist_ok=True)
    out_file = out_dir / f"{result['date']}.json"
    out_file.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    latest = out_dir / "latest.json"
    try:
        if latest.exists() or latest.is_symlink():
            latest.unlink()
        latest.symlink_to(out_file.name)
    except (OSError, NotImplementedError):
        # Windows 의 일부 환경에서 symlink 권한 없음 → copy fallback
        latest.write_text(out_file.read_text(encoding="utf-8"), encoding="utf-8")
    return out_file


# ─────────────────────────────────────────────────────────────
# main
# ─────────────────────────────────────────────────────────────

def main():
    p = argparse.ArgumentParser(description="AI-Readiness Scorer")
    p.add_argument("--root", type=str, default=None,
                   help="Project root (default: $CLAUDE_PROJECT_DIR or cwd)")
    p.add_argument("--json", action="store_true", help="JSON output only")
    p.add_argument("--no-actions", action="store_true", help="Skip ROI actions")
    p.add_argument("--no-save", action="store_true", help="Don't save to .ai-readiness/")
    args = p.parse_args()

    root_str = args.root or os.environ.get("CLAUDE_PROJECT_DIR") or os.getcwd()
    root = pathlib.Path(root_str).resolve()
    if not root.exists():
        print(f"ERROR: root not found: {root}", file=sys.stderr)
        sys.exit(2)

    # actions.json 위치 (.toolkit 옆)
    toolkit_root = pathlib.Path(__file__).parent.parent
    actions_db = load_actions(toolkit_root)

    result = run_score(root)
    actions = None if args.no_actions else generate_roi_actions(result, actions_db)
    if actions:
        result["actions"] = actions

    if not args.no_save:
        save_result(root, result)

    if args.json:
        print(json.dumps(result, ensure_ascii=False, indent=2))
    else:
        print_report(result, actions)

    sys.exit(0)


if __name__ == "__main__":
    main()
