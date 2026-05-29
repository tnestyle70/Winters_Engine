# Winters AI-Readiness Rubric v1 — 측정 + 채점 + 정기 평가 계획

> **작성일**: 2026-05-03
> **목적**: Winters 코드베이스의 "AI 가 얼마나 잘 작업할 수 있는 상태인가" 를 7 카테고리 × 100점 척도로 정량 측정. 정기 실행 → JSON 기록 → ROI 순 개선 액션 도출.
> **3종 세트 마지막 조각**:
> 1. [`CODEBASE_COMPASS_SYSTEM.md`](.md/architecture/CODEBASE_COMPASS_SYSTEM.md) — 코드베이스 metadata 박제
> 2. [`CLAUDE_MD_REFRESH_HOOK.md`](.md/architecture/CLAUDE_MD_REFRESH_HOOK.md) — 세션 종료 시 자동 갱신
> 3. **본 계획서** — 위 두 인프라가 정량으로 얼마나 작동하는지 측정
> **출력 포맷**: 사용자 제공 샘플 그대로 (Score / Grade / 카테고리별 소견 / ROI 액션)
> **저장 위치**: `.claude/stats/ai-readiness/<YYYY-MM-DD>.json` + 동일 폴더 latest.json 심볼릭

---

## §0. 한 줄 요약

**7 카테고리 (A 탐색·커버리지 15 / B 컨텍스트 품질 20 / C 암묵지 명시화 20 / D 의존성 매핑 15 / E 검증 게이트 15 / F 최신성·자동화 10 / G 성과 지표 5) × 100점 rubric. Python/Bash 채점 스크립트 (`Tools/AIReadiness/score.py`) 가 자동 측정 → JSON 저장 → CLI 출력. Winters baseline 추정 25-30/100 (Onboarding 등급) — Compass System (Phase A) 적용 후 65-75/100 (AI-Ready) 목표. 정기 실행: 매 주 cron (자동) + `/score-ai-readiness` slash command (수동). ROI 순 액션 자동 생성 — 기존 Compass / Refresh Hook 작업과 통합된 점수 향상 청사진.**

---

## §1. Rubric 정의 — 7 카테고리 × Sub-항목

### A. 탐색·커버리지 (15점) — "AI 가 어디든 진입 가능한가"

| Sub | 점수 | 측정 |
|---|---|---|
| **A1 모듈 manifest 커버** | 5pt | `_MODULE.md` 가 있는 모듈 비율 × 5 |
| **A2 진입점 정확성** | 5pt | manifest 의 `## Entry Points` 의 파일 경로가 실제 존재 비율 × 5 |
| **A3 폴더 README 깊이** | 5pt | depth 3+ 폴더에 README 또는 _MODULE.md 존재 비율 × 5 |

**Winters 현 baseline 추정**: A1=0 (Compass 미도입), A2=N/A, A3=2 → **2/15**

### B. 컨텍스트 품질 (20점) — "CLAUDE.md / _MODULE.md 가 AI 친화인가"

사용자 샘플 그대로:

| Sub | 점수 | 측정 |
|---|---|---|
| **B1 간결성** | 4pt | CLAUDE.md ≤ 500줄 = 4 / 500-1000 = 2 / 1000-1500 = 1 / 1500+ = 0 |
| **B2 명령어 예시** | 4pt | bash 코드블록 수 ≥ 5 = 4 / 3-4 = 2 / 1-2 = 1 / 0 = 0 |
| **B3 핵심파일 참조** | 4pt | 파일당 markdown 링크 평균 ≥ 3 = 4 / 2 = 2 / 1 = 1 / 0 = 0 |
| **B4 금지 패턴** | 4pt | "금지/X/❌/Don't" 키워드 ≥ 10 = 4 / 5-9 = 2 / 1-4 = 1 / 0 = 0 |
| **B5 크로스참조** | 4pt | 파일당 `@.md/...` 또는 `[link](path)` 평균 ≥ 5 = 4 / 3 = 2 / 1 = 1 / 0 = 0 |

**Winters baseline 추정**:
- B1=0 (CLAUDE.md 1100+ 줄)
- B2=2 (Services 실행 명령 / `convert_all_assets.bat` 등 있음)
- B3=4 (CLAUDE.md 가 많은 파일 링크)
- B4=4 (Gotchas / 보안 컨벤션 풍부)
- B5=2 (markdown 링크는 있으나 `@`-style 안 씀)

→ **12/20**

### C. 암묵지 명시화 (20점) — "tribal knowledge 가 박제됐나"

| Sub | 점수 | 측정 |
|---|---|---|
| **C1 Gotchas/함정 박제** | 8pt | CLAUDE.md + _MODULE.md 의 "함정/Gotcha" 항목 수 ≥ 50 = 8 / 30-49 = 6 / 10-29 = 4 / 1-9 = 2 / 0 = 0 |
| **C2 결정 ADR 박제** | 6pt | `.md/architecture/decisions/` 또는 동급 ADR 폴더의 .md 파일 수 ≥ 10 = 6 / 5-9 = 4 / 1-4 = 2 / 0 = 0 |
| **C3 Skill / Lesson** | 6pt | `winters-skills/`, `.md/guide/`, `.md/lessons/` 등 학습 자료 수 ≥ 20 = 6 / 10-19 = 4 / 5-9 = 2 / 0-4 = 0 |

**Winters baseline 추정**:
- C1=8 (CLAUDE.md Gotchas 섹션 수십 건)
- C2=2 (별도 ADR 폴더 미존재, 본 계획서들이 사실상 ADR)
- C3=4 (winters-skills/code/, /code-review/, /code-scaffolding/, /debug-pipeline/ + .md/guide/)

→ **14/20**

### D. 의존성 매핑 (15점) — "모듈 그래프가 시각화돼 있나"

| Sub | 점수 | 측정 |
|---|---|---|
| **D1 Module Graph** | 5pt | `MODULE_GRAPH.md` 또는 동급 존재 + cycle 0 = 5 / 존재만 = 3 / 미존재 = 0 |
| **D2 Mermaid/PlantUML 시각화** | 5pt | `mermaid` 코드블록 ≥ 3 = 5 / 1-2 = 3 / 0 = 0 |
| **D3 Code Index** | 5pt | `code-index.md` 또는 동급 (모든 모듈 + 진입점 매핑) = 5 / 부분 = 2 / 미존재 = 0 |

**Winters baseline 추정**:
- D1=0 (MODULE_GRAPH 미존재 — Compass Phase A 산출물)
- D2=0 (mermaid 미사용)
- D3=2 (CLAUDE.md 의 "디렉토리" / "Engine 필터" / "Client 필터" 표가 부분 code-index)

→ **2/15**

### E. 검증 게이트 (15점) — "stale / 깨진 참조 검출되나"

| Sub | 점수 | 측정 |
|---|---|---|
| **E1 깨진 link 검증** | 5pt | 깨진 markdown 참조 = 0 = 5 / 1-5 = 3 / 6-10 = 1 / 11+ = 0 |
| **E2 코드 ↔ 문서 동기 검증** | 5pt | _MODULE.md 의 진입점 파일 경로 실재 비율 × 5 |
| **E3 CI/Hook 강제** | 5pt | PR 시 자동 검증 hook 존재 = 5 / 수동 스크립트 만 = 2 / 미존재 = 0 |

**Winters baseline 추정**:
- E1=N/A (검증 안 해봄 — 첫 측정 시 산출)
- E2=N/A
- E3=0 (link/manifest 검증 hook 미존재)

→ **0~3/15** (실측 후 확정)

### F. 최신성·자동화 (10점) — "갱신 자동화돼 있나"

| Sub | 점수 | 측정 |
|---|---|---|
| **F1 Hook 등록 수** | 5pt | `.claude/settings.json` 의 hook 항목 수 ≥ 5 = 5 / 3-4 = 3 / 1-2 = 1 / 0 = 0 |
| **F2 자동 갱신 스크립트** | 5pt | refresh / sync / convert 류 자동 스크립트 수 ≥ 5 = 5 / 3-4 = 3 / 1-2 = 1 / 0 = 0 |

**Winters baseline 추정**:
- F1=0 (hook 미설정)
- F2=3 (UpdateLib.bat / convert_all_assets.bat / Make 류)

→ **3/10**

### G. 성과 지표 (5점) — "AI 작업 결과를 측정하나"

| Sub | 점수 | 측정 |
|---|---|---|
| **G1 evals/ 디렉토리** | 3pt | task 회귀 테스트 N개 ≥ 5 = 3 / 1-4 = 1 / 0 = 0 |
| **G2 Closing report / Backlog** | 2pt | 세션 종료 보고서 / TODO backlog 박제 = 2 / 부분 = 1 / 0 = 0 |

**Winters baseline 추정**:
- G1=0 (evals/ 미존재)
- G2=1 (CLAUDE.md "★ 다음" 섹션이 사실상 backlog)

→ **1/5**

### Baseline 합계 (예상)

| 카테고리 | 점수 | 비율 |
|---|---|---|
| A 탐색·커버리지 | 2/15 | 13% |
| B 컨텍스트 품질 | 12/20 | 60% |
| C 암묵지 명시화 | 14/20 | 70% |
| D 의존성 매핑 | 2/15 | 13% |
| E 검증 게이트 | 0~3/15 | 0-20% |
| F 최신성·자동화 | 3/10 | 30% |
| G 성과 지표 | 1/5 | 20% |
| **합계 (예상)** | **34~37/100** | |

**예상 등급**: **Pre-Onboarding** (< 40) — Compass + Hook + Validator 인프라 도입 시 65-75 (AI-Ready) 도달 가능.

---

## §2. 측정 방법 — 카테고리별 자동/수동 분리

### 2-1. 자동 측정 가능 (Python 스크립트로 정확)

| Sub | 측정 방법 |
|---|---|
| A1 모듈 커버 | `glob("**/_MODULE.md")` / 모듈 디렉토리 수 |
| A2 진입점 정확성 | _MODULE.md 파싱 → 진입점 파일 경로 → `os.path.exists` |
| A3 폴더 README | depth 3+ 폴더 수 / README 또는 _MODULE.md 있는 폴더 |
| B1 간결성 | `wc -l CLAUDE.md` |
| B2 명령어 예시 | grep ` ```bash ` / ` ```sh ` 코드블록 수 |
| B3 핵심파일 참조 | regex `\[.*?\]\(.*?\)` 카운트 / 파일 수 |
| B4 금지 패턴 | grep `금지\|X\|❌\|Don't\|never\|do not` 카운트 |
| B5 크로스참조 | regex `@\..*?` + `\[.*?\]\(.*?\.md\)` 카운트 / 파일 수 |
| C1 Gotchas | grep `gotcha\|함정\|⚠\|주의` 카운트 |
| C2 ADR | `.md/architecture/decisions/`, `.md/adr/` 등 체크 |
| C3 Skill/Lesson | `winters-skills/`, `.md/guide/`, `.md/lessons/` 파일 수 |
| D1 Module Graph | `MODULE_GRAPH.md` 존재 + cycle 검증 |
| D2 Mermaid | grep ` ```mermaid ` 코드블록 수 |
| D3 Code Index | `code-index.md` 또는 `INDEX.md` 존재 |
| E1 깨진 link | markdown link 추출 → 파일 존재 검증 |
| E2 코드↔문서 | _MODULE.md 의 진입점 파일/줄번호 검증 |
| E3 CI Hook | `.claude/settings.json` + `.github/workflows/` 검증 hook 존재 |
| F1 Hook 수 | `.claude/settings.json` 의 hooks 카운트 |
| F2 자동 스크립트 | `*.bat`, `*.sh`, `Tools/*/main.py` 등 카운트 |
| G1 evals/ | `evals/` 디렉토리 + .json 파일 수 |
| G2 Closing report | `.md/closing-report/`, `.claude/backlog/` 등 |

→ **모든 sub 자동 측정 가능**. 수동 채점 불필요.

### 2-2. 결과 검증 (수동 — 자동 점수 sanity check)

자동 점수가 의도와 맞는지 분기당 1회 사람 sanity check:
- 자동: "B2 명령어 예시 4/4" → 사람: 실제 그 코드블록이 작업 가능한 명령인가? (placeholder X)
- 자동: "C1 Gotchas 8/8" → 사람: 진짜 함정 박제인가, 단순 키워드 사용인가?

미스매치 시 측정 식 보정.

---

## §3. 출력 포맷 (사용자 샘플 그대로)

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 AI-Ready Rubric v1 결과 — Winters Engine
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 Score : 37/100  |  Grade : Pre-Onboarding
 저장  : .claude/stats/ai-readiness/2026-05-03.json
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 카테고리별 소견
  A 탐색·커버리지   2/15  ❌ 모듈 manifest (_MODULE.md) 0개
                          → Compass System Phase A 적용 시 +13pt
  B 컨텍스트 품질  12/20  ⚠️  B1 간결성 0/4 (CLAUDE.md 1100+ 줄)
                          B5 크로스참조 2/4 (@-style 안 씀)
  C 암묵지 명시화  14/20  ⚠️  C2 ADR 폴더 미존재
                          (본 계획서들이 사실상 ADR — 폴더 분리 권장)
  D 의존성 매핑     2/15  ❌ MODULE_GRAPH.md / mermaid / code-index 미존재
                          → Compass Phase A + Mermaid 도입 시 +13pt
  E 검증 게이트     0/15  ❌ Link 검증 / 코드↔문서 동기 / CI Hook 0개
                          → Validator (Compass Phase B) 도입 시 +15pt
  F 최신성·자동화   3/10  ⚠️  Hook 등록 0개 (settings.json 검증 필요)
                          → Refresh Hook 적용 시 +5pt
  G 성과 지표       1/5   △ evals/ 디렉토리 미존재 (CLAUDE.md "★ 다음" 이 backlog)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 ROI 순 액션 목록
  [1] [L] +15pt  Compass System Phase A — 34 모듈 _MODULE.md 박제
      참조: .md/architecture/CODEBASE_COMPASS_SYSTEM.md §5 Phase A
      예상 1주

  [2] [M] +13pt  Compass System 의 MODULE_GRAPH + mermaid + code-index
      참조: 동 §3-1 Components 2~3
      예상 2일

  [3] [M] +15pt  Compass Validator + GitHub Actions
      참조: 동 §5 Phase B
      예상 3일

  [4] [S] +11pt  CLAUDE.md 분할 — 1100줄 → 500줄 + 모듈별 _MODULE.md 분산
      참조: B1 간결성 회복
      예상 0.5일 (Compass Phase A 와 동시 진행)

  [5] [S] +5pt  CLAUDE.md Refresh Hook 등록
      참조: .md/architecture/CLAUDE_MD_REFRESH_HOOK.md §8 Phase A
      예상 0.5일
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

### 등급 표

| 점수 | 등급 | 의미 |
|---|---|---|
| 90-100 | AI-Optimized | AI 가 grep 0회 + Read 5 파일로 task 진입 |
| 75-89 | AI-Ready | AI 가 grep 1-2회 + Read 8 파일로 task 진입 |
| 60-74 | Onboarding-Ready | AI 가 manifest 일부 활용 가능 |
| 40-59 | Onboarding | AI 가 grep 5+ 회 필요, 단 핵심 정보 박제 |
| < 40 | Pre-Onboarding | AI 가 매번 전수 탐색 필요 |

---

## §4. 자동화 — 스크립트 / Hook / 저장

### 4-1. 채점 스크립트 — `Tools/AIReadiness/score.py`

```python
#!/usr/bin/env python3
"""
Winters AI-Readiness Rubric Scorer
"""
import os, sys, re, json, glob, datetime, pathlib

PROJECT_ROOT = pathlib.Path(os.environ.get("CLAUDE_PROJECT_DIR", os.getcwd()))
STATS_DIR = PROJECT_ROOT / ".claude" / "stats" / "ai-readiness"
STATS_DIR.mkdir(parents=True, exist_ok=True)

def measure_A1():
    # _MODULE.md 가 있는 모듈 비율 × 5
    module_dirs = [
        *PROJECT_ROOT.glob("Engine/Public/*"),
        *PROJECT_ROOT.glob("Engine/Private/*"),
        *PROJECT_ROOT.glob("Client/Public/*"),
        *PROJECT_ROOT.glob("Client/Private/*"),
        *PROJECT_ROOT.glob("Server/Public/*"),
        *PROJECT_ROOT.glob("Shared/*"),
        *PROJECT_ROOT.glob("Services/internal/*"),
        *PROJECT_ROOT.glob("Tools/*"),
    ]
    module_dirs = [d for d in module_dirs if d.is_dir()]
    covered = sum(1 for d in module_dirs if (d / "_MODULE.md").exists())
    return round(covered / max(len(module_dirs), 1) * 5, 1), {
        "total": len(module_dirs),
        "covered": covered,
        "missing": [str(d.relative_to(PROJECT_ROOT)) for d in module_dirs
                    if not (d / "_MODULE.md").exists()][:10]
    }

def measure_B1():
    claude_md = PROJECT_ROOT / "CLAUDE.md"
    if not claude_md.exists(): return 0, {"lines": 0}
    lines = sum(1 for _ in claude_md.open(encoding="utf-8"))
    if lines <= 500:   return 4, {"lines": lines}
    if lines <= 1000:  return 2, {"lines": lines}
    if lines <= 1500:  return 1, {"lines": lines}
    return 0, {"lines": lines}

def measure_B2():
    # CLAUDE.md + 모든 _MODULE.md 의 ```bash 코드블록 수
    md_files = [PROJECT_ROOT / "CLAUDE.md"] + list(PROJECT_ROOT.glob("**/_MODULE.md"))
    count = 0
    for f in md_files:
        if not f.exists(): continue
        text = f.read_text(encoding="utf-8", errors="ignore")
        count += len(re.findall(r"```(?:bash|sh|cmd|powershell)\b", text))
    if count >= 5:  return 4, {"count": count}
    if count >= 3:  return 2, {"count": count}
    if count >= 1:  return 1, {"count": count}
    return 0, {"count": count}

def measure_E1():
    # 모든 .md 파일에서 markdown link 추출 → 파일 존재 검증
    md_files = list(PROJECT_ROOT.glob("**/*.md"))
    md_files = [f for f in md_files if ".claude/worktrees" not in str(f)
                                     and "node_modules" not in str(f)
                                     and "ThirdPartyLib" not in str(f)]
    broken = []
    for f in md_files:
        text = f.read_text(encoding="utf-8", errors="ignore")
        # [text](path) 형식, http/https 제외
        for m in re.finditer(r"\[([^\]]+)\]\(([^)]+)\)", text):
            target = m.group(2).split("#")[0].split(":")[0]  # 앵커/줄번호 제거
            if not target or target.startswith(("http:", "https:", "mailto:")):
                continue
            # 상대 경로 해석
            if target.startswith("/"):
                target_path = PROJECT_ROOT / target.lstrip("/")
            else:
                target_path = (f.parent / target).resolve()
            if not target_path.exists():
                broken.append({
                    "file": str(f.relative_to(PROJECT_ROOT)),
                    "link": target
                })
    n = len(broken)
    if n == 0:        return 5, {"broken": 0, "samples": []}
    if n <= 5:        return 3, {"broken": n, "samples": broken[:5]}
    if n <= 10:       return 1, {"broken": n, "samples": broken[:5]}
    return 0, {"broken": n, "samples": broken[:5]}

# ... 다른 measure 함수들 (A2/A3/B3/B4/B5/C1/C2/C3/D1/D2/D3/E2/E3/F1/F2/G1/G2)

CATEGORIES = {
    "A": [("A1", "모듈 manifest 커버", measure_A1, 5)],
    "B": [
        ("B1", "간결성", measure_B1, 4),
        ("B2", "명령어 예시", measure_B2, 4),
        # ...
    ],
    "E": [("E1", "깨진 link 검증", measure_E1, 5), ...],
    # ...
}

def grade(score):
    if score >= 90: return "AI-Optimized"
    if score >= 75: return "AI-Ready"
    if score >= 60: return "Onboarding-Ready"
    if score >= 40: return "Onboarding"
    return "Pre-Onboarding"

def main():
    results = {"date": datetime.date.today().isoformat(),
               "categories": {}, "actions": []}
    total = 0
    for cat, subs in CATEGORIES.items():
        cat_score = 0
        cat_max = 0
        cat_subs = []
        for sub_id, name, fn, max_pts in subs:
            score, detail = fn()
            cat_score += score
            cat_max += max_pts
            cat_subs.append({
                "id": sub_id, "name": name,
                "score": score, "max": max_pts, "detail": detail
            })
        results["categories"][cat] = {
            "score": round(cat_score, 1),
            "max": cat_max,
            "subs": cat_subs
        }
        total += cat_score

    results["total"] = round(total, 1)
    results["grade"] = grade(total)

    # ROI 액션 자동 생성 (sub_id 별 미달 점수 + 기존 계획서 매핑)
    results["actions"] = generate_roi_actions(results)

    # 저장
    out = STATS_DIR / f"{results['date']}.json"
    out.write_text(json.dumps(results, ensure_ascii=False, indent=2))
    latest = STATS_DIR / "latest.json"
    if latest.exists() or latest.is_symlink():
        latest.unlink()
    latest.symlink_to(out.name)

    # CLI 출력
    print_report(results)

if __name__ == "__main__":
    main()
```

### 4-2. ROI 액션 자동 매핑 — `Tools/AIReadiness/actions.yml`

```yaml
# sub_id → 개선 액션 + 참조 + ROI
A1:
  action: "Compass System Phase A — 모듈 _MODULE.md 박제"
  reference: ".md/architecture/CODEBASE_COMPASS_SYSTEM.md §5 Phase A"
  effort: L
  expected_pt_per_module: 0.15  # 1 모듈 박제 당 +0.15pt (5pt / 34 모듈)

D1:
  action: "MODULE_GRAPH.md 작성 + cycle 검증"
  reference: ".md/architecture/CODEBASE_COMPASS_SYSTEM.md §3-1 Component 2"
  effort: S
  expected_pt: 5

E1:
  action: "깨진 markdown link N건 수정"
  reference: "score.json 의 categories.E.subs[0].detail.samples"
  effort: S
  expected_pt: 5

E3:
  action: "Compass Validator + GitHub Actions"
  reference: ".md/architecture/CODEBASE_COMPASS_SYSTEM.md §5 Phase B"
  effort: M
  expected_pt: 5

F1:
  action: "CLAUDE.md Refresh Hook 등록"
  reference: ".md/architecture/CLAUDE_MD_REFRESH_HOOK.md §8 Phase A"
  effort: S
  expected_pt: 5

# ... 다른 sub_id 들
```

### 4-3. CLI 진입점

#### Slash command (수동) — `.claude/skills/score-ai-readiness/SKILL.md`

```markdown
---
name: score-ai-readiness
description: |
  Run AI-Readiness Rubric scorer and print report.
  Use when the user asks "AI ready 점수", "rubric", "score-ai-readiness",
  or "/score-ai-readiness".
---

1. Run `python "$CLAUDE_PROJECT_DIR/Tools/AIReadiness/score.py"`
2. Read latest `.claude/stats/ai-readiness/<today>.json`
3. Print formatted report (사용자 샘플 형식 그대로 — §3 출력 포맷 참조)
4. Compare to previous result if exists, show delta
5. Suggest next ROI action

금지:
- 점수만 보여주고 액션 안 보여주기 (항상 ROI 5개 출력)
- 자동 코드 수정 (액션은 안내만, 실행은 사용자 동의 후)
```

#### Hook (자동) — `.claude/settings.json`:

```json
{
  "hooks": {
    "SessionEnd": [
      {
        "matcher": "clear|logout|other",
        "hooks": [
          { "type": "command",
            "command": "\"$CLAUDE_PROJECT_DIR\"/.claude/hooks/refresh-claude-md.sh" },
          { "type": "command",
            "command": "python \"$CLAUDE_PROJECT_DIR\"/Tools/AIReadiness/score.py >> \"$CLAUDE_PROJECT_DIR\"/.claude/stats/ai-readiness/log.txt 2>&1" }
        ]
      }
    ]
  }
}
```

→ 세션 종료 시 자동 채점 + 로그.

#### Cron (정기) — 매주 월요일 09:00:

```cron
0 9 * * 1  cd /path/to/Winters && python Tools/AIReadiness/score.py
```

또는 GitHub Actions:
```yaml
name: weekly-ai-readiness
on:
  schedule: [ {cron: "0 9 * * 1"} ]
jobs:
  score:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: python Tools/AIReadiness/score.py
      - run: |
          if [ -f .claude/stats/ai-readiness/latest.json ]; then
            cat .claude/stats/ai-readiness/latest.json
          fi
```

---

## §5. 첫 실행 (Baseline) — 현 Winters 의 예상 점수

### 5-1. 카테고리별 예상

(§1 의 "Winters 현 baseline 추정" 합산)

```
A: 2  / 15
B: 12 / 20
C: 14 / 20
D: 2  / 15
E: 0~3 / 15  (실측 후 확정)
F: 3  / 10
G: 1  / 5
─────────────
총 ~34 / 100  →  Pre-Onboarding (< 40)
```

### 5-2. Phase A 적용 후 예상 (Compass Phase A 1주 + Refresh Hook 0.5일)

```
A: 14 / 15  (+12)  ← _MODULE.md 34개 박제
B: 16 / 20  (+4)   ← CLAUDE.md 분할 → 간결성 회복
C: 16 / 20  (+2)   ← _MODULE.md 의 함정 분산 박제
D: 12 / 15  (+10)  ← MODULE_GRAPH + mermaid + code-index
E: 3  / 15  (변동 X — Validator 미적용)
F: 8  / 10  (+5)   ← Refresh Hook 등록
G: 1  / 5   (변동 X)
─────────────
총 ~70 / 100  →  Onboarding-Ready / AI-Ready 경계
```

### 5-3. Phase B 적용 후 예상 (Validator + CI 3일)

```
A: 14 / 15
B: 16 / 20
C: 16 / 20
D: 12 / 15
E: 13 / 15  (+10)  ← Validator + CI + 깨진 link 수정
F: 9  / 10  (+1)   ← CI hook 추가
G: 1  / 5
─────────────
총 ~81 / 100  →  AI-Ready
```

### 5-4. Phase C 적용 후 예상 (evals + ADR + 정착, 1주)

```
A: 15 / 15
B: 18 / 20
C: 18 / 20  ← ADR 폴더 분리 + 추가 박제
D: 13 / 15
E: 14 / 15
F: 10 / 10
G: 4  / 5   (+3)   ← evals/ 디렉토리 + 회귀 테스트
─────────────
총 ~92 / 100  →  AI-Optimized
```

→ **3-4주에 Pre-Onboarding (34) → AI-Optimized (92) 도달 가능**.

---

## §6. ROI 액션 — 기존 계획서 통합

### 6-1. 액션 매트릭스

| 우선 | Effort | 점수 | 액션 | 참조 계획서 |
|---|---|---|---|---|
| **1** | L (1주) | +15pt | Compass System Phase A — 34 모듈 _MODULE.md 박제 | [`CODEBASE_COMPASS_SYSTEM.md §5 Phase A`](.md/architecture/CODEBASE_COMPASS_SYSTEM.md) |
| **2** | M (2일) | +13pt | MODULE_GRAPH + mermaid + code-index | 동 §3-1 Components 2~3 |
| **3** | M (3일) | +15pt | Compass Validator + GitHub Actions | 동 §5 Phase B |
| **4** | S (0.5일) | +5pt | CLAUDE.md Refresh Hook 등록 | [`CLAUDE_MD_REFRESH_HOOK.md §8`](.md/architecture/CLAUDE_MD_REFRESH_HOOK.md) |
| **5** | S (0.5일) | +4pt | CLAUDE.md 1100줄 → 500줄 분할 (간결성) | B1 회복 — Compass Phase A 동시 진행 |
| **6** | S (수시) | +5pt | 깨진 link 11건 수정 (E1) | 첫 실측 후 결정 |
| **7** | M (1주) | +3pt | evals/ 디렉토리 + 회귀 테스트 5개 | 별도 계획서 (G1) |
| **8** | S (0.5일) | +2pt | ADR 폴더 분리 (`.md/architecture/decisions/`) | 본 계획서들이 사실상 ADR — 폴더만 만들고 상징적 이전 |

**총 +62pt** (34 → 96 도달, 모든 액션 적용 시)

### 6-2. 일정 (3-4주)

```
Week 1: Compass Phase A (1주) + Refresh Hook (0.5일) + CLAUDE.md 분할 (병행)
        → 점수 34 → 70 (예상)
Week 2: Compass Phase B (Validator + CI, 3일) + 깨진 link 수정 (수시)
        → 점수 70 → 86 (예상)
Week 3: evals + ADR + 정착 회귀 (1주)
        → 점수 86 → 92 (예상)
Week 4: 측정 정확성 검증 + rubric v1 → v2 갱신
```

### 6-3. 측정 lifecycle

```
Day 1   : Baseline 측정 (점수 ~34) — score.py 첫 실행
Week 1  : Compass Phase A 후 측정 — 모든 sub 의 변화량 확인
Week 2  : Phase B 후 측정 — 깨진 link / Validator 효과 확인
Week 3  : evals 후 측정 — 최종 등급 확인
매주 월 : 자동 cron 측정 (장기 추적)
```

---

## §7. 정기 측정 — JSON 누적 + 트렌드

### 7-1. 저장 구조

```
.claude/stats/ai-readiness/
├── 2026-05-03.json        ← baseline
├── 2026-05-10.json        ← Phase A 후
├── 2026-05-17.json        ← Phase B 후
├── 2026-05-24.json        ← Phase C 후
├── 2026-06-01.json        ← 매주 cron
├── ...
├── latest.json            ← 가장 최근의 symlink
└── log.txt                ← 매 실행 로그
```

### 7-2. JSON 스키마

```json
{
  "date": "2026-05-03",
  "total": 34.5,
  "grade": "Pre-Onboarding",
  "categories": {
    "A": {
      "score": 2.0, "max": 15,
      "subs": [
        {"id": "A1", "name": "모듈 manifest 커버",
         "score": 2.0, "max": 5,
         "detail": {"total": 34, "covered": 0, "missing": ["..."]}}
      ]
    },
    ...
  },
  "actions": [
    {"priority": 1, "effort": "L", "delta_pt": 13,
     "title": "Compass System Phase A",
     "reference": ".md/architecture/CODEBASE_COMPASS_SYSTEM.md §5 Phase A"},
    ...
  ]
}
```

### 7-3. 트렌드 시각화 (선택, Phase C 이후)

`Tools/AIReadiness/trend.py` — 최근 30일 점수 line chart (matplotlib) → `docs/ai-readiness-trend.png`. README.md 에 embed.

---

## §8. 적용 단계

### Phase 0 — Rubric 도구 자체 박제 (1일)

| 항목 | 산출물 |
|---|---|
| `Tools/AIReadiness/score.py` 구현 (모든 measure 함수) | py 1 파일 |
| `Tools/AIReadiness/actions.yml` (sub_id → 액션 매핑) | yml 1 파일 |
| `.claude/skills/score-ai-readiness/SKILL.md` | skill 1 파일 |
| 첫 실행 → baseline JSON 생성 | `.claude/stats/ai-readiness/<today>.json` |

### Phase 1 — 측정 정확성 검증 (반나절)

자동 점수 vs 사람 검증 — 5 sub 골라 sanity check:
- B1 간결성 (line count 확인)
- B2 명령어 (실제 실행 가능한 bash 인지)
- E1 깨진 link (false positive 비율)
- C1 Gotchas (단순 키워드 vs 실제 박제 구분)
- D2 mermaid (코드블록 안 mermaid 인지 검증)

미스매치 시 measure 식 보정.

### Phase 2 — Hook + Cron 등록 (반나절)

- `.claude/settings.json` — SessionEnd hook 에 score.py 추가
- (선택) GitHub Actions — 매주 자동 실행

### Phase 3 — ROI 액션 진행 (3-4주, §6 일정)

매 액션 완료 후 score.py 재실행 → 점수 변화 확인.

### Phase 4 — Rubric v2 (1개월 후)

3-4주 사용 결과 회고:
- 자동 점수가 정확히 "AI 작업 효율" 을 반영하는가?
- 점수 높은데 실제 AI task 어려운 sub 있는가? (위양성)
- 점수 낮은데 AI task 쉬운 sub 있는가? (위음성)

→ 측정 식 / 가중치 / sub 분류 갱신.

---

## §9. 한 줄 요약

**7 카테고리 × 100점 rubric (사용자 제공 패턴 그대로) 을 Python 스크립트로 자동 측정 + JSON 저장 + ROI 액션 자동 생성. Winters baseline 추정 ~34 (Pre-Onboarding) — Compass System Phase A (1주, +36pt) + Refresh Hook (0.5일, +5pt) + Validator (3일, +15pt) + evals (1주, +3pt) = 3-4주에 ~92 (AI-Optimized) 도달 가능. 측정은 SessionEnd hook (자동) + `/score-ai-readiness` slash command (수동) + 매주 cron 3 entry. 기존 계획서 (Compass / Refresh Hook) 와 통합된 ROI 청사진 — 본 rubric 이 그 두 인프라의 효과를 정량화하는 측정자.**
