# Winters AI-Readiness 100점 도달 마스터 실행 계획서 v1

> **작성일**: 2026-05-03
> **목적**: AI_READINESS_RUBRIC.md 의 baseline ~34/100 → 목표 100/100 도달까지의 단계별 실행 청사진. 각 phase 의 sub-task / 정확한 명령 / 검증 방법 / 롤백 시나리오 / 체크리스트 박제.
> **참조**:
> - [`AI_READINESS_RUBRIC.md`](.md/architecture/AI_READINESS_RUBRIC.md) — rubric 정의 + 측정 시스템
> - [`CODEBASE_COMPASS_SYSTEM.md`](.md/architecture/CODEBASE_COMPASS_SYSTEM.md) — Compass System (Phase A 의 핵심)
> - [`CLAUDE_MD_REFRESH_HOOK.md`](.md/architecture/CLAUDE_MD_REFRESH_HOOK.md) — Refresh Hook (Phase F 일부)
> **추정 일정**: **3-4주** (집중 작업 시) — Phase 0 (1일) → A (1주) → B (3일) → C (1주) → D (3일) → E (반나절)
> **최종 목표**: 100/100 (AI-Optimized) — grep 0회, _MODULE.md 만으로 진입 가능, 정기 측정 자동화

---

## §0. 한 줄 요약

**Phase 0 도구 박제 (1일, baseline 측정) → Phase 1 Compass A (1주, +36pt → 70) → Phase 2 Compass B Validator (3일, +13pt → 83) → Phase 3 evals + ADR (1주, +5pt → 88) → Phase 4 정밀 튜닝 (3일, +8pt → 96) → Phase 5 100점 sweep (반나절, +4pt → 100). 총 3-4주. 매 phase 끝 score.py 실행 + JSON 누적 + git commit + 롤백 가능 branch 분리. 사용자 confirm 시 Phase 0 부터 즉시 시작.**

---

## §1. 도달 경로 — Baseline 34 → 100

### 1-1. Phase 별 점수 변화 (예상)

| Phase | 작업 | 일수 | 누적 점수 | 등급 |
|---|---|---|---|---|
| (baseline) | 첫 측정 | — | ~34 | Pre-Onboarding |
| **0** | Rubric 도구 박제 + 정확 baseline | 1일 | ~34 (변동 X — 측정만) | Pre-Onboarding |
| **1** | Compass Phase A (34 모듈 _MODULE.md + MODULE_GRAPH + mermaid + code-index + CLAUDE.md 분할) | 1주 | ~70 | Onboarding-Ready |
| **2** | Compass Phase B (Validator + GitHub Actions + 깨진 link 수정) + Refresh Hook | 3일 | ~83 | AI-Ready |
| **3** | evals/ + ADR 폴더 + 회귀 테스트 5개 | 1주 | ~88 | AI-Ready |
| **4** | 정밀 튜닝 (B5 크로스참조 / C2 ADR 보강 / D2 mermaid 추가) | 3일 | ~96 | AI-Optimized |
| **5** | 100점 sweep (E1 0 / G2 closing-report 자동화) | 반나절 | **100** | AI-Optimized |

### 1-2. 각 phase 의 가시성 (sub_id 단위 변화 추적)

```
Baseline (예상):
  A1=0  A2=N/A A3=2     | B1=0  B2=2  B3=4  B4=4  B5=2  | C1=8  C2=2  C3=4
  D1=0  D2=0  D3=2      | E1=?  E2=N/A E3=0    | F1=0  F2=3 | G1=0  G2=1

Phase 1 후:
  A1=5  A2=5  A3=4 (+12) | B1=4 (+4)        B5=2  | C1=8  C2=2  C3=4
  D1=5  D2=5  D3=2 (+10) | E1=?  E2=4 (+4)  E3=0  | F1=0  F2=3 | G1=0  G2=1
  → 70

Phase 2 후:
  E1=5  E3=5  +기존 (+10) | F1=5 (+5) | 나머지 동일
  → 83 (혹은 86 — 깨진 link 11건 모두 수정 시)

Phase 3 후:
  G1=3 (+3) | C2=4 (+2) | 나머지 동일
  → 88

Phase 4 후:
  B5=4 (+2) | C2=6 (+2) | D2=5 → 5 (이미)| C3=6 (+2) | B3=4 (이미)
  → 94-96

Phase 5 후:
  E1=5 (이미) | G2=2 (+1) | 잔여 잡티 +2-3
  → 100
```

---

## §2. Phase 0 — Rubric 도구 박제 (1일)

### 2-1. 목표

`Tools/AIReadiness/score.py` + `actions.yml` + `/score-ai-readiness` skill 작성. 첫 baseline JSON 생성. 정확한 시작점 확정.

### 2-2. Sub-task 체크리스트

- [ ] **0-1** `Tools/AIReadiness/` 디렉토리 생성
- [ ] **0-2** `Tools/AIReadiness/score.py` — 모든 measure 함수 (A1~G2 약 18개) 구현
- [ ] **0-3** `Tools/AIReadiness/actions.yml` — sub_id → 액션 매핑 18개
- [ ] **0-4** `Tools/AIReadiness/requirements.txt` (pyyaml 정도, 표준 라이브러리 위주 권장)
- [ ] **0-5** `.claude/skills/score-ai-readiness/SKILL.md` 작성
- [ ] **0-6** 첫 실행 — `python Tools/AIReadiness/score.py` → `.claude/stats/ai-readiness/<today>.json` 생성
- [ ] **0-7** baseline 점수 확인 + JSON 구조 검증
- [ ] **0-8** sanity check — 5 sub 의 자동 점수가 실제와 맞는지 사람 검증

### 2-3. 정확한 명령

```bash
mkdir -p Tools/AIReadiness
mkdir -p .claude/stats/ai-readiness
mkdir -p .claude/skills/score-ai-readiness

# score.py 작성 (다음 §2-4 참조)
# actions.yml 작성
# SKILL.md 작성

python Tools/AIReadiness/score.py
ls -la .claude/stats/ai-readiness/
cat .claude/stats/ai-readiness/latest.json | head -50
```

### 2-4. score.py 핵심 측정 함수 시그니처

(전체 코드는 별도 sub-plan 또는 첫 작업 turn 에서 박제)

```python
# A 카테고리
def measure_A1_module_coverage() -> tuple[float, dict]: ...
def measure_A2_entry_point_validity() -> tuple[float, dict]: ...
def measure_A3_folder_readme_depth() -> tuple[float, dict]: ...

# B 카테고리
def measure_B1_brevity() -> tuple[float, dict]: ...
def measure_B2_command_examples() -> tuple[float, dict]: ...
def measure_B3_core_file_refs() -> tuple[float, dict]: ...
def measure_B4_forbidden_patterns() -> tuple[float, dict]: ...
def measure_B5_cross_refs() -> tuple[float, dict]: ...

# C 카테고리
def measure_C1_gotchas() -> tuple[float, dict]: ...
def measure_C2_adr() -> tuple[float, dict]: ...
def measure_C3_skill_lesson() -> tuple[float, dict]: ...

# D 카테고리
def measure_D1_module_graph() -> tuple[float, dict]: ...
def measure_D2_mermaid() -> tuple[float, dict]: ...
def measure_D3_code_index() -> tuple[float, dict]: ...

# E 카테고리
def measure_E1_broken_links() -> tuple[float, dict]: ...
def measure_E2_doc_code_sync() -> tuple[float, dict]: ...
def measure_E3_ci_hook() -> tuple[float, dict]: ...

# F 카테고리
def measure_F1_hooks() -> tuple[float, dict]: ...
def measure_F2_auto_scripts() -> tuple[float, dict]: ...

# G 카테고리
def measure_G1_evals() -> tuple[float, dict]: ...
def measure_G2_closing_report() -> tuple[float, dict]: ...
```

### 2-5. 검증

- [ ] JSON 파일 정상 생성
- [ ] 7 카테고리 모두 점수 산출
- [ ] sub detail 의 sample (예: 깨진 link, 모듈 missing 등) 가 실제와 일치
- [ ] 등급 (Pre-Onboarding / Onboarding / ...) 정확

### 2-6. 합격 기준

- baseline JSON 저장 완료
- 점수가 30-40 범위 (예상치 검증)
- ROI 액션 5개 자동 생성

### 2-7. 롤백 시나리오

- score.py 가 잘못된 측정 — sub 단위로 measure 함수 분리돼 있어 한 함수만 수정 → 재실행
- JSON 파일 잘못 — 삭제 후 재실행 (idempotent)

---

## §3. Phase 1 — Compass Phase A (1주, +36pt 목표)

### 3-1. 목표

A1/A2/A3 만점 (15/15) + B1 회복 (4/4) + D1/D2/D3 만점 (15/15) + E2 회복 (4-5/5).

핵심: **34 모듈 _MODULE.md 박제 + MODULE_GRAPH + mermaid + code-index + CLAUDE.md 분할**.

### 3-2. Sub-task 체크리스트

#### Day 1: Engine 13 모듈

- [ ] **1-1** Engine 모듈 list 확정 (CLAUDE.md "Engine 필터" 표 13개)
- [ ] **1-2** `Engine/Public/Core/_MODULE.md` 박제
- [ ] **1-3** `Engine/Public/RHI/_MODULE.md` 박제
- [ ] **1-4** `Engine/Public/Resource/_MODULE.md` 박제
- [ ] **1-5** `Engine/Public/ECS/_MODULE.md` 박제
- [ ] **1-6** `Engine/Public/Renderer/_MODULE.md` 박제
- [ ] **1-7** `Engine/Public/Scene/_MODULE.md` 박제
- [ ] **1-8** `Engine/Public/Framework/_MODULE.md` 박제
- [ ] **1-9** `Engine/Public/Manager/{Sound,UI,Navigation,Profiler}/_MODULE.md` 4개 박제
- [ ] **1-10** `Engine/Public/JobSystem/_MODULE.md` 박제 (Phase 5-A + 향후 5-B)
- [ ] **1-11** `Engine/Public/Editor/_MODULE.md` 박제 (ImGui)

#### Day 2: Client 8 모듈

- [ ] **1-12** `Client/Public/Scene/_MODULE.md`
- [ ] **1-13** `Client/Public/GameObject/_MODULE.md`
- [ ] **1-14** `Client/Public/Manager/_MODULE.md`
- [ ] **1-15** `Client/Public/Network/_MODULE.md`
- [ ] **1-16** `Client/Public/GameMode/_MODULE.md`
- [ ] **1-17** `Client/Public/AI/_MODULE.md` (Phase F 대비)
- [ ] **1-18** `Client/Public/Champions/_MODULE.md` (5+ 챔프 통합)
- [ ] **1-19** Client root `_MODULE.md` (MainApp / 진입점)

#### Day 3: Server / Shared / Tools / Services

- [ ] **1-20** `Server/Public/Network/_MODULE.md`
- [ ] **1-21** `Server/Public/Game/_MODULE.md`
- [ ] **1-22** `Server/Public/Security/_MODULE.md`
- [ ] **1-23** `Shared/_MODULE.md`
- [ ] **1-24** `Tools/WintersAssetConverter/_MODULE.md`
- [ ] **1-25** `Tools/AIReadiness/_MODULE.md` (Phase 0 산출물)
- [ ] **1-26** `Services/internal/{auth,leaderboard,matchmaking,profile,payment,shop}/_MODULE.md` 6개

#### Day 4: MODULE_GRAPH + mermaid + code-index

- [ ] **1-27** `.md/architecture/MODULE_GRAPH.md` — 텍스트 DAG (의존성 cycle 검증)
- [ ] **1-28** 위 파일에 mermaid graph 코드블록 3+ 추가
- [ ] **1-29** `.md/architecture/CODE_INDEX.md` — 모든 모듈 + 핵심 진입점 매핑
- [ ] **1-30** `.md/architecture/ENTRY_POINTS.md` — task → 모듈 매핑 50+ 개

#### Day 5: CLAUDE.md 분할 (간결성 회복)

- [ ] **1-31** CLAUDE.md 1100+ 줄 → 500줄 이하로 분할
- [ ] **1-32** 분리된 내용은 `_MODULE.md` 또는 `.md/architecture/` 로 이전
- [ ] **1-33** CLAUDE.md 에는 "프로젝트 개요 + 진입 명령 + 핵심 컨벤션 링크" 만 유지

#### Day 6-7: 검증 + 보강

- [ ] **1-34** `python Tools/AIReadiness/score.py` 재실행 — 점수 변화 확인
- [ ] **1-35** A1/A2/A3 만점 검증
- [ ] **1-36** B1 = 4 검증 (CLAUDE.md ≤ 500줄)
- [ ] **1-37** D1/D2/D3 만점 검증
- [ ] **1-38** AI 가 실제 task ("신규 챔프 추가") 받았을 때 grep 0회 + Read ≤ 6 파일로 진입 가능한지 사람 검증

### 3-3. _MODULE.md 작성 표준 (CODEBASE_COMPASS_SYSTEM.md §3-2 템플릿 사용)

각 _MODULE.md 작성 시 필수 9 섹션:
1. 책임
2. 진입점 (3-7개)
3. 의존
4. 의존받음
5. Common Tasks (AI 매핑)
6. 함정 (Gotchas)
7. 외부 노출 API
8. 핵심 파일 Top 5
9. 관련 계획서

### 3-4. 합격 기준

- 34 _MODULE.md 모두 박제
- score.py 점수 ≥ 65 (Onboarding-Ready 진입)
- AI 가 새 task 받을 때 manifest 만으로 진입 (grep < 2 회)
- CLAUDE.md ≤ 500줄

### 3-5. 롤백 시나리오

- _MODULE.md 박제 중 잘못된 정보 발견 — 해당 1 파일만 수정 (다른 모듈 영향 X)
- CLAUDE.md 분할 후 핵심 정보 누락 — git revert + 재분할
- branch: `feature/compass-phase-a` (1주 분량)

---

## §4. Phase 2 — Compass Validator + Refresh Hook (3일, +13pt 목표)

### 4-1. 목표

E1/E3 만점 (10/10) + F1 만점 (5/5).

핵심: **Validator 도구 + GitHub Actions + Refresh Hook + 깨진 link 수정**.

### 4-2. Sub-task 체크리스트

#### Day 1: Validator 도구

- [ ] **2-1** `Tools/CompassValidator/main.py` (또는 .cpp) — 7 검증 항목 구현 (CODEBASE_COMPASS_SYSTEM.md §3-1 Component 5)
- [ ] **2-2** 첫 실행 — 깨진 link / cycle / stale 진입점 list 출력
- [ ] **2-3** 발견된 문제 모두 수정 (특히 깨진 link)

#### Day 2: GitHub Actions + Refresh Hook

- [ ] **2-4** `.github/workflows/compass-validator.yml` — PR 시 자동 실행
- [ ] **2-5** `.github/workflows/ai-readiness.yml` — 매주 score.py 자동 실행
- [ ] **2-6** `.claude/skills/refresh-claude-md/SKILL.md` 작성 (CLAUDE_MD_REFRESH_HOOK.md §7-1)
- [ ] **2-7** `.claude/hooks/refresh-claude-md.sh` 작성 (동 §7-2)
- [ ] **2-8** `.claude/settings.json` hook 등록 (동 §7-3)

#### Day 3: 검증

- [ ] **2-9** PR 만들어 Validator CI 동작 검증
- [ ] **2-10** SessionEnd 발동시켜 refresh-claude-md.sh 동작 검증
- [ ] **2-11** `/refresh-claude-md` slash command 동작 검증
- [ ] **2-12** score.py 재실행 — 점수 변화 확인

### 4-3. 합격 기준

- E1 = 5 (깨진 link 0개)
- E3 = 5 (CI hook 작동)
- F1 ≥ 3 (hook 등록 3개+)
- score.py 점수 ≥ 80 (AI-Ready 진입)

### 4-4. 롤백 시나리오

- Validator false positive — 측정 식 보정 후 재실행
- GitHub Actions 실패 — workflow YAML 수정
- Refresh Hook proposal 누락 박제 — SKILL.md 의 7 원칙 보강

---

## §5. Phase 3 — evals + ADR (1주, +5pt 목표)

### 5-1. 목표

G1 = 3 (evals 5+ 회귀 테스트) + C2 = 4-6 (ADR 폴더 분리 + 추가 박제).

### 5-2. Sub-task 체크리스트

#### Day 1-2: evals/ 디렉토리

- [ ] **3-1** `evals/` 디렉토리 + README.md
- [ ] **3-2** `evals/champion-add.json` — "Yasuo 신규 스킬 추가" task 회귀
- [ ] **3-3** `evals/skill-cooldown-fix.json`
- [ ] **3-4** `evals/network-packet-add.json`
- [ ] **3-5** `evals/render-shader-add.json`
- [ ] **3-6** `evals/jobsystem-stress.json` (Phase 5-B 검증)

각 eval 의 형식:
```json
{
  "task": "Yasuo Q 4타 conditional 추가",
  "expected_steps": [
    "Read .md/architecture/ENTRY_POINTS.md",
    "Read Champions/Yasuo/_MODULE.md",
    "Read SkillTable.cpp + Yasuo_Skills.cpp",
    "Edit 3 files"
  ],
  "max_grep_calls": 1,
  "max_files_read": 6,
  "success_criteria": "stage 필드 + Q4 분기 + FX 박제 모두 정확"
}
```

#### Day 3-4: ADR 폴더 + 기존 박제 이전

- [ ] **3-7** `.md/architecture/decisions/` 디렉토리
- [ ] **3-8** 기존 결정들 ADR 형식으로 이전:
  - `001-ecs-vs-oop.md` (CLAUDE.md "병합 철학" 의 표)
  - `002-fiber-jobcounter-no-waitlist.md` (Codex 검토 #4)
  - `003-get-workerslot-option-a.md` (Codex 검토 #3)
  - `004-compass-system.md`
  - `005-ai-readiness-rubric.md`
- [ ] **3-9** ADR 템플릿 박제 (`_TEMPLATE.md`)

#### Day 5: 검증

- [ ] **3-10** score.py 재실행 — 점수 변화 확인
- [ ] **3-11** AI 가 evals/ 의 task 받았을 때 expected_steps 그대로 따라가는지 검증

### 5-3. 합격 기준

- G1 ≥ 3 (evals 5+)
- C2 ≥ 4 (ADR 5+)
- score.py 점수 ≥ 88

### 5-4. 롤백 시나리오

- ADR 폴더가 기존 계획서와 중복 — 계획서 → ADR 링크 추가, 통합
- evals false positive (AI 가 못 푸는 게 정상) — task 난이도 조정

---

## §6. Phase 4 — 정밀 튜닝 (3일, +8pt 목표)

### 6-1. 목표

남은 sub 의 미달 점수 회복. B5 / C2 / D2 / C3 / B3 등.

### 6-2. Sub-task 체크리스트

- [ ] **4-1** B5 크로스참조 강화 — `@.md/path/to/file.md` 형식 도입 (또는 markdown link 평균 5+ 보장)
- [ ] **4-2** C2 ADR 추가 박제 — 5+ → 10+
- [ ] **4-3** D2 mermaid 추가 — 모듈 그래프 / 시퀀스 / 클래스 다이어그램 3+ 추가
- [ ] **4-4** C3 Skill/Lesson 추가 — `winters-skills/` 에 신규 skill 박제 (예: `compass-bootstrap`, `ai-readiness-tuning`)
- [ ] **4-5** B3 핵심파일 참조 강화 — _MODULE.md 별 markdown 링크 평균 3+ 보장
- [ ] **4-6** A2 진입점 정확성 검증 — 모든 _MODULE.md 의 진입점 파일 경로 실재
- [ ] **4-7** F2 자동 스크립트 추가 — backup-cleanup.sh / lint.sh 등
- [ ] **4-8** score.py 재실행 + 미달 sub 재확인 + 추가 보강

### 6-3. 합격 기준

- score.py 점수 ≥ 96

---

## §7. Phase 5 — 100점 Sweep (반나절, +4pt 목표)

### 7-1. 목표

마지막 잡티 처리. E1 = 5 보장 + G2 = 2 + 작은 미달.

### 7-2. Sub-task 체크리스트

- [ ] **5-1** Validator 재실행 — 깨진 link 0 보장
- [ ] **5-2** G2 Closing report 자동화 — `.claude/closing-reports/` + 세션 종료 시 자동 생성 hook
- [ ] **5-3** 모든 sub 만점 또는 만점 90% 이상 보장
- [ ] **5-4** score.py 재실행 — 100/100 확인
- [ ] **5-5** AI-Optimized 등급 확정 + git tag `ai-readiness-100`

### 7-3. 합격 기준

- **score.py 점수 = 100/100**
- 모든 카테고리 만점 또는 만점에 ≤ 1pt 차이
- AI 가 새 task 받을 때 grep 0회 + Read ≤ 5 파일로 진입

---

## §8. 진행 추적 표

| Phase | Sub-task 수 | 상태 | 시작일 | 완료일 | 점수 |
|---|---|---|---|---|---|
| 0 | 8 | ⏭️ 대기 | — | — | — |
| 1 | 38 | ⏭️ 대기 | — | — | — |
| 2 | 12 | ⏭️ 대기 | — | — | — |
| 3 | 11 | ⏭️ 대기 | — | — | — |
| 4 | 8 | ⏭️ 대기 | — | — | — |
| 5 | 5 | ⏭️ 대기 | — | — | — |
| **합계** | **82** | | | | |

★ Phase 진입 시 본 표의 상태/날짜/점수 업데이트.

---

## §9. 롤백 전략 종합

| 상황 | 대응 |
|---|---|
| score.py 가 잘못된 측정 | sub 단위 measure 함수만 수정 (전체 영향 X) |
| _MODULE.md 박제 잘못 | 해당 1 파일만 수정 |
| CLAUDE.md 분할 후 정보 누락 | git revert + 재분할 (Phase 1 의 1-31~1-33 만 영향) |
| Validator false positive 폭주 | 측정 식 보정 + sub_id 비활성 옵션 |
| Refresh Hook 무한 갱신 | settings.json 의 hook 비활성 + 수동 trigger 만 |
| 100점 도달했는데 실제 AI 작업 효율 낮음 | Rubric v1 → v2 갱신 (가중치 / 측정식 보정) |

각 Phase 별 branch:
- `feature/ai-readiness-phase0-tools`
- `feature/ai-readiness-phase1-compass-a`
- `feature/ai-readiness-phase2-validator-hook`
- `feature/ai-readiness-phase3-evals-adr`
- `feature/ai-readiness-phase4-tuning`
- `feature/ai-readiness-phase5-sweep`

각 phase 합격 후 main 으로 merge + tag.

---

## §10. 최종 합격 (100점) 정의

다음 5 조건 모두 만족:

1. **`python Tools/AIReadiness/score.py` 결과 ≥ 100**
2. **모든 카테고리 만점 또는 만점에 ≤ 1pt 차이**
3. **AI 가 새 task 받을 때 grep 0회 + Read ≤ 5 파일로 진입** (eval 5개 모두 통과)
4. **`.github/workflows/ai-readiness.yml` 매주 자동 실행 + 점수 회귀 시 알림**
5. **CLAUDE.md ≤ 500줄 + _MODULE.md 34+ + ADR 10+ + evals 5+ + Validator CI 작동**

5 조건 모두 만족 시 → git tag `ai-readiness-100` + README 에 "AI-Optimized" 배지 추가.

---

## §11. 한 줄 요약

**Baseline ~34 → Phase 0 도구 (1일) + Phase 1 Compass A (1주, 38 sub-task) + Phase 2 Validator/Hook (3일, 12 sub-task) + Phase 3 evals/ADR (1주, 11 sub-task) + Phase 4 정밀 튜닝 (3일, 8 sub-task) + Phase 5 sweep (반나절, 5 sub-task) = 총 82 sub-task / 3-4주 / 6 branch / 6 tag → 100/100 (AI-Optimized). 매 Phase 끝 score.py 실행 + JSON 누적 + 롤백 가능 branch 분리. 사용자 confirm 시 Phase 0 부터 즉시 시작 — 첫 작업 = `Tools/AIReadiness/score.py` + `actions.yml` + `.claude/skills/score-ai-readiness/SKILL.md` 박제 + 첫 baseline 측정.**
