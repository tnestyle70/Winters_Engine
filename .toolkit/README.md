# AI-Readiness Toolkit v1

> **Templated infrastructure for making any codebase AI-friendly.**
> Drop this folder into any project root → 30분-1일에 AI 협업 효율 측정 가능.
> 게임 엔진 (UE5/Unity/자체) 우선 + 백엔드 service 보조.

---

## 무엇이 들어 있나

| 폴더 | 용도 | 적용 시간 |
|---|---|---|
| `module-manifest/` | `_MODULE.md` 템플릿 + HOWTO + 도메인별 예시 | 모듈당 15-30분 |
| `compass-validator/` | 깨진 link / 의존성 cycle 자동 검증 (Python) | 30분 (CI 등록) |
| `ai-readiness-scorer/` | 100점 rubric 자동 채점 (Python) | 30분 (실행) |
| `claude-md-refresh-hook/` | Claude Code 세션 종료 시 CLAUDE.md 자동 갱신 | 1시간 (hook 등록) |
| `engine-reference/` | 게임 엔진 도메인 패턴 (UE5 188 모듈 / 703 plugin 분석) | 참조용 |
| `backend-reference/` | 백엔드 microservice 도메인 패턴 | 참조용 |

---

## 5단계 적용 가이드 (어느 코드베이스든)

### Step 1: 코드베이스 평가 (10분)

```bash
# 모듈 후보 폴더 수
find . -maxdepth 3 -type d | wc -l

# 파일 규모
find . -name "*.cpp" -o -name "*.h" -o -name "*.py" -o -name "*.go" | wc -l

# 기존 문서 상태
find . -name "*.md" | wc -l
test -f CLAUDE.md && wc -l CLAUDE.md
```

→ 결과에 따라 진입 우선순위:
- 100 파일 이하: scorer 만 도입 (작은 규모, manifest 부담)
- 100-1000 파일: module-manifest 핵심 5-10 모듈 박제 + scorer
- 1000+ 파일: 전체 toolkit 적용 + Validator CI 등록

### Step 2: Baseline 측정 (5분)

```bash
cp -r .toolkit/ <your-project-root>/
cd <your-project-root>
python .toolkit/ai-readiness-scorer/score.py
# → .ai-readiness/baseline.json + 콘솔 출력
```

### Step 3: 핵심 모듈 manifest 박제 (1주)

`.toolkit/module-manifest/HOWTO.md` 따라:
1. 모듈 정의 (책임 단위 폴더 = 모듈)
2. 우선순위 5-10 모듈 선정 (가장 자주 작업하는 곳)
3. `_MODULE_TEMPLATE.md` 복사 → 각 모듈 폴더에 `_MODULE.md` 박제
4. 의존성 3 분류 (Public / Private / Forward-Decl) 채우기

→ 도메인별 예시 참조:
- 게임 엔진: `.toolkit/module-manifest/EXAMPLES/game-engine-rendering.md`
- 백엔드: `.toolkit/module-manifest/EXAMPLES/backend-service.md`

### Step 4: 자동 검증 + 측정 (30분)

```bash
# 깨진 link / cycle 검증
python .toolkit/compass-validator/validator.py

# 점수 재측정 (Step 2 와 비교)
python .toolkit/ai-readiness-scorer/score.py
```

→ 점수 변화량으로 manifest 박제 효과 확인.

### Step 5: 자동화 + 정기 측정 (1시간)

```bash
# Claude Code 사용 시 — 세션 종료 시 자동 refresh
cp .toolkit/claude-md-refresh-hook/refresh.sh .claude/hooks/
cat .toolkit/claude-md-refresh-hook/settings.json.example
# → .claude/settings.json 에 hook 추가

# 매주 cron 또는 GitHub Actions
crontab -e
# 0 9 * * 1  cd /path/to/project && python .toolkit/ai-readiness-scorer/score.py
```

---

## 시간 예산

| 코드베이스 규모 | 초기 적용 | 주간 유지 |
|---|---|---|
| 작은 (< 100 파일) | 1-2시간 | 5분 |
| 중간 (100-1000) | 1일 | 30분 |
| 큰 (1000+) | 1주 | 1시간 |

---

## 검증 사례

### Winters Engine (300 파일, 13 모듈)

- Baseline: ~34/100 (Pre-Onboarding)
- Module manifest 5개 박제 후: ~50/100
- 전체 13 모듈 + Validator 후: ~75/100 (AI-Ready)
- evals + Refresh Hook 추가: ~92/100 (AI-Optimized)
- 적용 기간: 약 2-3주 (병행 작업 기준)

### (회사 적용 예정)

(첫 적용 후 결과 기록)

---

## 도메인별 가이드

### 게임 엔진 (UE5 / Unity / 자체)

핵심 참조: `engine-reference/`
- UE5 의 188 Runtime 모듈 카테고리 분류 (자동화 가능한 패턴)
- `*.Build.cs` → `_MODULE.md` 매핑
- Plugin 시스템 (uplugin) → wplugin 청사진
- DLL boundary 철학 (FromSoft 엘든링 2022 CVE 사례)

→ Winters Engine 적용 결과: `WINTERS_ARCHITECTURE_BLUEPRINT_V1.md` 참조

### 백엔드 microservice

핵심 참조: `backend-reference/`
- 서비스 단위 = 모듈 (cmd/X/, internal/X/)
- API contract 박제 (OpenAPI / FlatBuffers schema)
- DB schema 박제 (migrations 폴더)
- 메시지 큐 의존성 (Kafka topic / Redis pubsub)

→ Winters Services (Auth / Match / Shop 6개) 가 작은 사례

---

## 커스터마이징

### Rubric 가중치 조정

`ai-readiness-scorer/score.py` 의 `CATEGORY_WEIGHTS` 수정.

기본: A 15 / B 20 / C 20 / D 15 / E 15 / F 10 / G 5

도메인별 권장:
- 게임 엔진: 기본값
- 백엔드: D (의존성) 20 / E (검증) 20 으로 가중 — service 간 의존이 핵심
- 라이브러리: B (컨텍스트) 25 — 외부 사용자가 읽음

### Manifest 템플릿 변형

`module-manifest/_MODULE_TEMPLATE.md` 수정. 도메인별 필수 섹션 추가/제거.

---

## License & Attribution

(Winters Engine 의 자산 — 사용 시 attribution 권장)

---

## 다음 버전 (v2 예정)

- mermaid 자동 생성 (의존성 그래프)
- VSCode extension (manifest 작성 보조)
- GitHub Actions template (PR 시 자동 검증)
- Notion / Confluence export
- 다국어 (한/영) 동시 박제 헬퍼
