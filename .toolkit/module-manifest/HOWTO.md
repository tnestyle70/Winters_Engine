# HOWTO — 신규 코드베이스에 _MODULE.md 박제하기

> **목표**: 30분 ~ 1일에 코드베이스의 핵심 모듈 5-15개에 manifest 박제.
> **성공 기준**: AI 가 task 받으면 manifest 만으로 작업 진입 (grep 0~1회).

---

## Step 1: 모듈 정의 (10분)

### 게임 엔진 도메인

**모듈 = 책임 단위 폴더**. 다음 패턴 매칭:

```
Engine/Public/{X}/             ← X 가 한 단어 (Core, RHI, Renderer, ...)
Engine/Private/{X}/            ← 동일
Plugins/{Category}/{X}/        ← Plugin 별
```

**모듈이 아닌 것**:
- `Engine/Public/Core/Profiler/` — sub-module (Profiler 가 Core 안)
- `Engine/Public/Resource/internal/` — 내부 구현 폴더
- `ThirdPartyLib/` — 외부 의존성

UE5 참조: `Engine/Source/Runtime/{X}/` 188개. Winters: `Engine/Public/{X}/` 13개.

### 백엔드 microservice 도메인

**모듈 = 1 service**. 다음 패턴:

```
cmd/{service}/main.go          ← 진입점
internal/{service}/            ← 비즈니스 로직 = 모듈
pkg/{shared}/                  ← 공유 라이브러리 = 모듈
```

각 service 의 `internal/{service}/` 가 하나의 _MODULE.md.

### 일반 (라이브러리 / Web app)

**모듈 = 단일 책임 폴더**. cyclomatic 의존성 검사로:
- 폴더 안 import 가 외부보다 내부 위주 → 모듈
- 폴더 안 파일 ≥ 5 + 외부 노출 API ≥ 1 → 모듈

---

## Step 2: 우선순위 모듈 선정 (10분)

전체 모듈 list 만들고, 다음 기준으로 순위:

| 우선순위 | 기준 |
|---|---|
| **1순위** | 가장 자주 수정되는 모듈 (`git log --pretty=format:'%H' -- path/ | wc -l` 상위) |
| **2순위** | 가장 의존받는 모듈 (`grep "include.*X" -r . | wc -l` 상위) |
| **3순위** | 신규 팀원이 가장 헷갈리는 모듈 (사람 판단) |
| **4순위** | 기타 |

→ 1순위 5-10개부터 시작. 효율 ROI 가장 큼.

### Winters 예시

1순위 (최근 1개월 가장 수정 빈번):
- `Client/Private/Scene/` — Scene_InGame 1500줄, 거의 매 작업 수정
- `Client/Private/Champions/` — 챔프 추가 시 매번
- `Engine/Public/ECS/` — Component 추가 시
- `Engine/Public/Renderer/` — FX / 메시 시스템
- `Engine/Public/Core/JobSystem/` — Phase 5-A 진행 중

→ 이 5개부터 박제.

---

## Step 3: `_MODULE.md` 박제 (모듈당 15-30분)

### 워크플로

```bash
# 모듈 폴더로 이동
cd Engine/Public/Core/

# 템플릿 복사
cp ../../../.toolkit/module-manifest/_MODULE_TEMPLATE.md ./_MODULE.md

# 채우기
$EDITOR _MODULE.md
```

### 작성 순서 (15분 budget 기준)

1. **책임** (3분) — 1-2 단락. "이 모듈이 X 를 위해 Y 를 한다"
2. **진입점** (5분) — 가장 자주 호출되는 함수 3-7개. `grep -r "ClassName::" .` 로 후보 찾고 줄번호 박제
3. **의존성** (5분) — 폴더의 모든 .cpp 의 `#include` 또는 `import` 분석. Public (헤더에서 include) vs Private (cpp 만에서 include)
4. **함정** (2분) — CLAUDE.md / 기존 주석에서 발췌

### 단축 팁

- **30분 → 15분 단축**: 진입점은 "가장 큰 .cpp 파일 의 public 함수 5개" 만 우선
- **의존성 3 분류 모호 시**: Public 만 채우고 Private/Forward-Decl 은 추후
- **함정 없음**: 빈 채로 두지 말고 "(현 시점 알려진 함정 없음 — 추후 박제)" 명시

---

## Step 4: 검증 (5분)

```bash
python .toolkit/compass-validator/validator.py

# 출력 예:
# === Module Manifest Validator ===
# Modules found: 13
# Manifests covered: 5/13 (38%)
# Broken links: 0
# Dependency cycles: 0
# Stale entry points: 0
# 
# Missing manifests:
#   - Engine/Public/Audio/
#   - Engine/Public/Network/
#   ...
```

발견된 문제 모두 수정 후 재실행.

---

## Step 5: 정기 갱신 (PR 마다)

### 새 모듈 추가 시

`_MODULE.md` 박제 의무. CI 강제 (compass-validator/ci-check.sh).

### 기존 모듈 변경 시

다음 중 하나라도 해당하면 manifest 갱신:
- 새 진입점 추가
- 의존성 변경
- 새 함정 발견 (디버깅 후)
- 핵심 파일 추가

### 자동 갱신 (선택)

`.toolkit/claude-md-refresh-hook/` 의 패턴으로 _MODULE.md 도 자동 갱신 가능. SessionEnd hook 에서 변경 모듈 감지 → 갱신 proposal 파일 출력.

---

## Anti-patterns (이렇게 박제하면 망함)

### ❌ 1. 진입점 줄번호 누락

```markdown
- `CModelRenderer::Render` — 메시 렌더 시  ❌ 줄번호 없음
```

→ 시간 지나면 함수 위치 변동 → manifest stale → AI 가 grep 으로 다시 헤맴.

### ❌ 2. "이 모듈은 중요합니다" 같은 무의미 문구

```markdown
## 책임
이 모듈은 매우 중요한 렌더링 시스템입니다. ❌
```

→ AI 가 무엇 해야 할지 0 정보.

올바른 박제:
```markdown
## 책임
DX11 mesh / skinned / UI 렌더링. RenderGraph 도입 전까지 직접 IA/PS bind 패턴.
캡슐화: ModelRenderer (인스턴스), FxSystem (particle), PlaneRenderer (지면 quad).
```

### ❌ 3. 함정에 "주의하세요" 만

```markdown
## 함정
- 셰이더 수정 시 주의하세요  ❌
```

올바른 박제:
```markdown
## 함정
- HLSL 수정 후 OutDir 동기화 필수 — `xcopy /D` 가 hlsl 변경 미감지 → OutDir 옛 .hlsl
  / 해결: ① `cp Shaders/*.hlsl Bin/Debug/Shaders/` ② Rebuild Solution ③ vcxproj /Y 옵션
```

### ❌ 4. 모든 모듈이 "Public dep = Core" 만

```markdown
### Public
- Core
```

→ 자동 분류라면 무의미. 진짜 의존성을 직접 분석.

---

## 도메인별 차이 요약

| 항목 | 게임 엔진 | 백엔드 service | 라이브러리 |
|---|---|---|---|
| 모듈 단위 | 폴더 (Engine/Public/X/) | service (cmd/X) | 폴더 또는 namespace |
| 평균 모듈 수 | 50-200 | 5-30 | 5-50 |
| Public/Private 강조 | DLL boundary 핵심 | API contract 핵심 | semver 핵심 |
| 진입점 형식 | Class::Method:line | HTTP endpoint 또는 RPC | exported function |
| 함정 카테고리 | 셰이더 / 빌드 / 동시성 | DB migration / 환경변수 / network | breaking change / deprecation |

---

## 회사 첫날 적용 시나리오

**Day 1 오전 (2시간)**:
1. `.toolkit/` 코드베이스 root 에 복사 (5분)
2. Step 1-2 — 모듈 정의 + 우선순위 (30분)
3. `validator.py` 첫 실행 — baseline (5분)
4. `score.py` 첫 실행 — baseline (5분)
5. 결과 사내 공유 + 1순위 5 모듈 박제 시작 (75분)

**Day 1 오후 (4시간)**:
6. 1순위 5 모듈 _MODULE.md 박제 완료 (모듈당 30분 × 5)
7. validator + score 재실행 — 효과 확인
8. CLAUDE.md (또는 동급) 작성 — 5단계 적용 가이드

**Day 2-5**: 2순위 모듈 박제 + AI 협업 효율 체감 + 동료 onboard.

---

## 참고

- 게임 엔진 예시: `EXAMPLES/game-engine-rendering.md`
- 백엔드 예시: `EXAMPLES/backend-service.md`
- UE5 reference: `../engine-reference/`
- Validator: `../compass-validator/`
- Scorer: `../ai-readiness-scorer/`
