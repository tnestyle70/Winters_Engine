# Compass Validator

> **코드베이스의 `_MODULE.md` manifest 자동 검증.**
> 깨진 link / stale 진입점 / 의존성 cycle 검출 → CI 차단.

---

## 사용법

### 즉시 실행

```bash
python .toolkit/compass-validator/validator.py
```

CWD 기준 자동 실행. 또는:

```bash
python .toolkit/compass-validator/validator.py --root /path/to/project
python .toolkit/compass-validator/validator.py --fix-suggestions
python .toolkit/compass-validator/validator.py --json > validation.json
```

### Exit code

- `0` — 모든 검증 통과
- `1` — 검증 실패 (CI 차단 신호)
- `2` — 입력/환경 오류

---

## 출력 예시

### Human-readable (기본)

```
============================================================
  Compass Validator — Result
============================================================
Root: /home/user/project

Modules         : 8/13 (61.5%)
Broken links    : 2 (in manifests)
All-MD links    : 11 (broken)
Stale entry pts : 1
Stale core files: 0
Dep cycles      : 0

FAIL  See details below.
```

### `--fix-suggestions` 추가 시

```
------------------------------------------------------------
Missing manifests (5):
  - Engine/Public/Audio
  - Engine/Public/Network
  - Engine/Public/Physics
  - Client/Public/AI
  - Server/Public/Security
  → cp .toolkit/module-manifest/_MODULE_TEMPLATE.md <dir>/_MODULE.md

Broken manifest links (2):
  - Engine/Public/Renderer/_MODULE.md:42 → ../../../foo/bar.h
  - Client/Public/Scene/_MODULE.md:88 → ../OldScene_InGame.cpp

Stale entry points (1):
  - [Renderer] CModelRenderer::OldRender → ModelRenderer.h:54 (not found)
```

---

## 검증 항목 7가지

| # | 검증 | 의미 |
|---|---|---|
| 1 | 모듈 발견 + _MODULE.md 커버리지 | 모듈 디렉토리 N 개 중 manifest 박제 비율 |
| 2 | Manifest 안 markdown link 깨짐 | `[text](foo.md)` 같은 링크의 파일 실재 |
| 3 | 진입점 파일 실재 | `## Entry Points` 의 `path/file.h:42` 검증 |
| 4 | 핵심 파일 실재 | `## 핵심 파일` 의 경로 검증 |
| 5 | 의존성 cycle 0 | `## Dependencies` Public + Private 파싱 → DAG 검증 |
| 6 | Depended By 자동 계산 vs 박제 일치 | (선택) 박제값 검증 |
| 7 | 모든 .md 파일 link 전수 | manifest 외 모든 .md 의 link 검증 |

---

## 모듈 디렉토리 패턴

`validator.py` 의 `MODULE_DIR_PATTERNS` 가 default:

```python
# 게임 엔진
"Engine/Public/*", "Engine/Private/*",
"Client/Public/*", "Client/Private/*",
"Server/Public/*", "Server/Private/*",
"Plugins/*/*",

# 백엔드
"internal/*", "cmd/*", "pkg/*",
"Services/internal/*", "Services/cmd/*",

# Tools
"Tools/*",

# 일반
"src/*", "lib/*",
```

자기 코드베이스의 패턴이 다르면 직접 수정.

### 제외 디렉토리

```python
EXCLUDED_DIRS = {
    "node_modules", ".git", ".venv", "venv", "__pycache__",
    "build", "dist", "ThirdPartyLib", "third_party", "vendor",
    "EngineSDK", ".claude", "Bin", "Obj", "Intermediate",
}
```

---

## CI 통합

### GitHub Actions

`.github/workflows/compass.yml`:

```yaml
name: Compass Validator
on: [pull_request]
jobs:
  validate:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with: { python-version: '3.12' }
      - name: Run Compass Validator
        run: python .toolkit/compass-validator/validator.py --fix-suggestions
```

PR 의 manifest 깨진 link / stale 진입점 자동 차단.

### GitLab CI

`.gitlab-ci.yml`:

```yaml
compass-validator:
  stage: test
  image: python:3.12
  script:
    - python .toolkit/compass-validator/validator.py --fix-suggestions
  only: [merge_requests]
```

### Pre-commit hook

`.pre-commit-config.yaml`:

```yaml
- repo: local
  hooks:
    - id: compass-validator
      name: Compass Validator
      entry: python .toolkit/compass-validator/validator.py
      language: system
      pass_filenames: false
```

---

## 의존성 cycle 검출 (Tarjan SCC)

`## Dependencies` 의 `Public` + `Private` 모듈 list → DAG → SCC 검출.

cycle 발견 시:
```
Dependency cycles (1):
  - Renderer -> Resource -> ECS -> Renderer
```

해결: 의존성 재설계. 일반적으로 cycle 의 weakest link 를 forward-decl 로 강등.

---

## 한계 + 개선 예정

### 현재 한계

- markdown 안의 줄번호 (`file.cpp:42`) 의 줄번호 자체는 검증 X (파일 실재만)
- include 문 자동 분석 없음 (manifest 의 박제값만 신뢰)
- mermaid 그래프 자동 생성 없음

### v2 예정

- C++ `#include` / Go `import` / Python `import` 자동 분석 → manifest 와 비교
- mermaid graph 자동 생성 (DAG 시각화)
- 줄번호까지 검증 (regex `^[\s]*XXX::YYY` 매칭)
- VSCode extension (저장 시 자동 검증)

---

## 의존성

Python 3.10+ 표준 라이브러리만. 외부 패키지 0.

---

## 트러블슈팅

### "No modules found"

→ `MODULE_DIR_PATTERNS` 가 자기 코드베이스의 폴더 구조와 안 맞음. validator.py 의 패턴 직접 수정.

### "Permission denied" (Windows)

→ `python` 대신 `python3` 또는 `py -3`.

### CI 에서 `--fix-suggestions` 가 너무 verbose

→ 첫 실행은 `--fix-suggestions` 로 baseline, 이후엔 옵션 제거.

---

## 참고

- 사용 예시: Winters Engine — baseline ~34, manifest 13 박제 후 ~75
- 관련: `../ai-readiness-scorer/` (점수 측정), `../module-manifest/` (manifest 작성)
