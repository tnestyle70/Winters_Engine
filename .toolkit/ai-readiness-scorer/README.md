# AI-Readiness Scorer

> **100점 척도로 코드베이스의 AI 협업 효율 측정.**
> 7 카테고리 × 18 sub. 자동 채점 + JSON 누적 + ROI 액션 자동 생성.

---

## 사용법

```bash
# 기본
python .toolkit/ai-readiness-scorer/score.py

# 명시적 root
python .toolkit/ai-readiness-scorer/score.py --root /path/to/project

# JSON 만 (CI 통합)
python .toolkit/ai-readiness-scorer/score.py --json

# 액션 출력 X
python .toolkit/ai-readiness-scorer/score.py --no-actions

# 저장 안 함
python .toolkit/ai-readiness-scorer/score.py --no-save
```

---

## 출력 예시

```
====================================================================
 AI-Ready Rubric — Result
====================================================================
 Score : 34.5/100  |  Grade : Pre-Onboarding
 Saved : .ai-readiness/2026-05-03.json
====================================================================
 카테고리별 소견
  A 탐색·커버리지     2.0/15   [XX]
        A1 모듈 manifest 커버      0.0/5
        A2 진입점 정확성            0.0/5
  B 컨텍스트 품질    12.0/20   [--]
        B1 간결성                  0.0/4
        B5 크로스참조              2.0/4
  C 암묵지 명시화    14.0/20   [--]
        C2 ADR 박제                2.0/6
  D 의존성 매핑       2.0/15   [XX]
        D1 Module Graph           0.0/5
        D2 Mermaid 시각화         0.0/5
  E 검증 게이트       0.0/15   [XX]
  F 최신성·자동화     3.0/10   [--]
        F1 Hook 등록 수            0.0/5
  G 성과 지표         1.0/5    [--]
        G1 evals/ 디렉토리         0.0/3
====================================================================
 ROI 순 액션 목록 (상위 10)
  [1] [S] +5.0pt  MODULE_GRAPH.md 작성 — 의존성 DAG (mermaid 포함)
      참조: .toolkit/compass-validator/README.md §의존성 cycle 검출
  [2] [S] +5.0pt  CODE_INDEX.md 또는 ENTRY_POINTS.md 작성
  [3] [S] +5.0pt  Mermaid 그래프 추가
  [4] [L] +5.0pt  Compass System Phase A — 모듈 _MODULE.md 박제
  ...
====================================================================
```

---

## 7 카테고리 / 18 sub 정의

### A 탐색·커버리지 (15pt)
| Sub | 점수 | 측정 |
|---|---|---|
| A1 모듈 manifest 커버 | 5 | _MODULE.md 비율 × 5 |
| A2 진입점 정확성 | 5 | manifest 의 `Class::Method at file:line` 의 파일 실재 비율 × 5 |
| A3 폴더 README 깊이 | 5 | depth 3+ 폴더의 README/_MODULE.md 비율 × 5 |

### B 컨텍스트 품질 (20pt)
| Sub | 점수 | 측정 |
|---|---|---|
| B1 간결성 | 4 | CLAUDE.md 줄 수 (≤500=4 / ≤1000=2 / ≤1500=1 / 1500+=0) |
| B2 명령어 예시 | 4 | ```bash/sh/cmd 블록 수 (≥5=4 / ≥3=2 / ≥1=1 / 0=0) |
| B3 핵심파일 참조 | 4 | 파일당 markdown link 평균 (≥3=4 / ≥2=2 / ≥1=1 / 0=0) |
| B4 금지 패턴 | 4 | 금지/X/Don't 카운트 (≥10=4 / ≥5=2 / ≥1=1 / 0=0) |
| B5 크로스참조 | 4 | @-style + [link](*.md) 평균 (≥5=4 / ≥3=2 / ≥1=1 / 0=0) |

### C 암묵지 명시화 (20pt)
| Sub | 점수 | 측정 |
|---|---|---|
| C1 Gotchas/함정 | 8 | 키워드 카운트 (≥50=8 / ≥30=6 / ≥10=4 / ≥1=2 / 0=0) |
| C2 ADR 박제 | 6 | decisions/ 또는 adr/ 의 .md 수 (≥10=6 / ≥5=4 / ≥1=2 / 0=0) |
| C3 Skill/Lesson | 6 | skills/ guides/ lessons/ 의 .md 수 (≥20=6 / ≥10=4 / ≥5=2 / 0-4=0) |

### D 의존성 매핑 (15pt)
| Sub | 점수 | 측정 |
|---|---|---|
| D1 Module Graph | 5 | MODULE_GRAPH.md 존재 (binary) |
| D2 Mermaid | 5 | ```mermaid 블록 수 (≥3=5 / ≥1=3 / 0=0) |
| D3 Code Index | 5 | CODE_INDEX.md / ENTRY_POINTS.md / INDEX.md 존재 (binary) |

### E 검증 게이트 (15pt)
| Sub | 점수 | 측정 |
|---|---|---|
| E1 깨진 link | 5 | 0=5 / 1-5=3 / 6-10=1 / 11+=0 |
| E2 코드↔문서 동기 | 5 | _MODULE.md 진입점/핵심파일 실재 비율 × 5 |
| E3 CI/Hook 강제 | 5 | settings.json hooks + .github/workflows/ (둘 다=5 / 한쪽=2 / 둘 다 X=0) |

### F 최신성·자동화 (10pt)
| Sub | 점수 | 측정 |
|---|---|---|
| F1 Hook 등록 수 | 5 | settings.json hooks 카운트 (≥5=5 / ≥3=3 / ≥1=1 / 0=0) |
| F2 자동 스크립트 | 5 | .bat/.sh/Tools/main.py 수 (≥5=5 / ≥3=3 / ≥1=1 / 0=0) |

### G 성과 지표 (5pt)
| Sub | 점수 | 측정 |
|---|---|---|
| G1 evals/ | 3 | evals/.json 수 (≥5=3 / ≥1=1 / 0=0) |
| G2 Closing report / Backlog | 2 | closing-reports + backlog 디렉토리 또는 CLAUDE.md "★ 다음" |

---

## 등급

| 점수 | 등급 | 의미 |
|---|---|---|
| 90-100 | AI-Optimized | grep 0회, manifest 만 진입 |
| 75-89 | AI-Ready | grep 1회, manifest 정상 작동 |
| 60-74 | Onboarding-Ready | grep 1-2회, manifest 일부 활용 |
| 40-59 | Onboarding | grep 3-5회, 핵심 박제는 있음 |
| 0-39 | Pre-Onboarding | 매번 전수 탐색 |

---

## ROI 액션 매핑 (`actions.json`)

각 sub_id 의 미달 점수 + effort (S/M/L) → ROI 자동 정렬.

```json
{
  "A1": {
    "title": "Compass System Phase A — 모듈 _MODULE.md 박제",
    "reference": ".toolkit/module-manifest/HOWTO.md",
    "effort": "L"
  },
  ...
}
```

**Effort 의미**:
- S (Small): ≤4시간
- M (Medium): 1-3일
- L (Large): ≥1주

**ROI 공식**: `delta_pt / effort_hours` 내림차순 정렬.

`actions.json` 직접 수정 가능. 회사 도메인에 맞춰 title / reference / effort 변경.

---

## JSON 출력 구조

`.ai-readiness/<YYYY-MM-DD>.json`:

```json
{
  "date": "2026-05-03",
  "root": "/path/to/project",
  "total": 34.5,
  "grade": "Pre-Onboarding",
  "categories": {
    "A": {
      "name": "탐색·커버리지",
      "score": 2.0,
      "max": 15,
      "subs": [
        {"id": "A1", "name": "모듈 manifest 커버",
         "score": 0.0, "max": 5,
         "detail": {"total": 59, "covered": 0, "missing": ["..."]}}
      ]
    },
    ...
  },
  "actions": [
    {"sub_id": "D1", "delta_pt": 5.0, "effort": "S",
     "title": "MODULE_GRAPH.md 작성", "roi": 1.25,
     "reference": "..."}
  ]
}
```

`.ai-readiness/latest.json` = 가장 최근 결과 symlink.

---

## CI 통합

### GitHub Actions (`.github/workflows/ai-readiness.yml`)

```yaml
name: AI-Readiness Score
on:
  schedule: [{cron: "0 9 * * 1"}]   # 매주 월 09:00 UTC
  pull_request:
jobs:
  score:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with: { python-version: '3.12' }
      - run: python .toolkit/ai-readiness-scorer/score.py --json > result.json
      - uses: actions/upload-artifact@v4
        with: { name: ai-readiness-result, path: result.json }
```

### SessionEnd Hook 통합 (Claude Code)

`.claude/settings.json`:

```json
{
  "hooks": {
    "SessionEnd": [
      {"matcher": "clear|logout|other",
       "hooks": [{
         "type": "command",
         "command": "python \"$CLAUDE_PROJECT_DIR\"/.toolkit/ai-readiness-scorer/score.py --json --no-actions >> \"$CLAUDE_PROJECT_DIR\"/.ai-readiness/log.txt 2>&1"
       }]
      }
    ]
  }
}
```

세션 종료 시 자동 측정 + 시계열 로그 누적.

---

## 한계 + v2 예정

### 현재 한계
- 정규식 기반 — markdown 안 코드블록 안의 키워드도 카운트 (위양성)
- 도메인별 가중치 미지원 (CATEGORIES dict 직접 수정)
- 시계열 trend 시각화 X (JSON 만 박제)

### v2 예정
- 도메인 프로파일 (`game-engine.json`, `backend.json` 등)
- Trend 시각화 (matplotlib line chart 자동 생성)
- 카테고리 가중치 사용자 정의 (CLI 옵션)
- LLM-assisted 측정 (Claude API 호출, 의미 기반 평가)

---

## 트러블슈팅

### "0 modules found"
→ `MODULE_DIR_PATTERNS` 가 회사 코드베이스 폴더 구조와 안 맞음. score.py 직접 수정.

### Windows symlink 권한 오류
→ `latest.json` 이 symlink 대신 copy 됨 (자동 fallback). 정상.

### Score 가 들쑥날쑥
→ 박제 후 `git status` 의 변경 사항 확인. 박제 없이 score 변동 시 측정 식 버그 가능 — 알려주세요.

---

## 참고

- Validator: `../compass-validator/`
- Manifest: `../module-manifest/`
- 검증 사례: Winters Engine (300 파일, 13 모듈) — Pre-Onboarding (~34) → AI-Ready (~75) 1주 / AI-Optimized (~92) 2-3주
