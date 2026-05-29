# {ModuleName} Module

<!--
이 템플릿을 모듈 폴더 root 에 _MODULE.md 로 복사.
{Placeholder} 부분을 채우고 안내 주석 (HTML comment) 은 삭제.
완성된 manifest 는 compass-validator 가 자동 검증.
-->

## 책임 (Responsibility)

<!-- 1-2 단락. 이 모듈이 해결하는 문제 + 경계. "X 를 위한 Y" 형식 권장. -->

{이 모듈이 무엇을 하는지 1-2 단락}

---

## 진입점 (Entry Points)

<!-- AI/사람이 작업 시작할 위치. 3-7개 권장. 파일:줄번호 형식 필수. -->

- `{ClassName}::{Method}` at `path/to/file.h:42` — {언제 사용}
- `{Function}` at `path/to/file.cpp:123` — {언제 사용}

---

## 의존성 (Dependencies)

<!-- UE5 Build.cs 의 3 분류 패턴. 의존성 cycle 차단의 핵심. -->

### Public (헤더 노출 — 의존자가 dep 헤더 사용 가능)

- `{ModuleName}` — {왜 헤더 노출 필요}

### Private (구현만, .cpp 안에서만 사용)

- `{ModuleName}` — {왜}

### Forward-Decl Only (헤더에 `class X;` 만, 실제 link X)

- `{ModuleName}` — {포인터/참조 멤버용}

---

## 의존받음 (Depended By)

<!-- compass-validator 가 자동 채워줌. 수동 작성 시 한정. -->

- `{ModuleName}` — {용도}

---

## Common Tasks (AI 매핑)

<!-- AI 가 task 받았을 때 어디 보면 되는지. 3-10개. -->

- "{Task 표현, 예: '신규 X 추가'}" → `{진입 위치 / 패턴 설명}`
- "{Task}" → `{...}`

---

## 함정 (Gotchas)

<!-- 재현 가능한 함정 + 회피. CLAUDE.md 의 gotcha 섹션과 연결. -->

- {함정 1} — {증상} / 해결: {회피 방법}
- {함정 2} — {증상} / 해결: {회피}

---

## 외부 노출 API (DLL boundary)

<!-- 보안 / 모듈 격리 핵심. -->

- **노출** (`WINTERS_ENGINE` 또는 동급 마크): {목록 또는 "없음"}
- **비노출** (내부 구현): {목록 또는 "전부"}

---

## Plugin 메타 (Plugin 시스템 도입 후)

<!-- Phase G (Plugin 시스템) 진입 전엔 생략 가능. -->

- **소속 Plugin**: {Plugin 이름 또는 "Engine Core" 또는 "N/A"}
- **LoadingPhase**: `{EarliestPossible / PreDefault / Default / PostDefault / PreLoadingScreen}`
- **EnabledByDefault**: `{true / false}`

---

## 핵심 파일 (Top 5 by importance)

<!-- 사람/AI 가 처음 진입할 때 보는 파일. 5개 권장. -->

1. `path/to/file.h` — {역할}
2. `path/to/file.cpp` — {역할}
3. `...` — {...}

---

## 관련 계획서 / 문서

<!-- 다른 .md 파일 / 계획서 / 가이드 링크. -->

- `.md/path/to/plan.md` — {요약}
- `https://...` — {외부 자료}

---

## 성능 특성 (선택)

<!-- 알려진 병목 / 성능 budget / 측정 결과. -->

- {특성 1: 예 "단일 thread, ~5ms / frame"}
- {특성 2: 예 "JobSystem 병렬화 후 ~1.5ms"}

---

## TODO / 미해결 (선택)

<!-- 박제할 가치 있는 미해결 항목. -->

- [ ] {TODO 1}
- [ ] {TODO 2}

---

<!--
검증 체크리스트 (compass-validator 가 자동 확인):
- [x] 모든 진입점의 파일 경로 실재
- [x] 의존 모듈이 실재
- [x] 의존성 cycle 0
- [x] 핵심 파일 5개 모두 실재
- [x] 관련 문서 link 깨짐 0
-->
