# WintersEngine — Claude 스킬 모음

## 스킬 구조
```
winters-skills/
├── code-scaffolding/   ← 클래스/모듈 자동 생성
├── library-reference/  ← Engine API 조회
├── code-review/        ← 코드 리뷰 (GOTCHAS 기반)
└── code-testing/       ← assertion 기반 검증
```

## 대상 프로젝트
- **Winters Engine** — 범용 DX11 C++20 게임 엔진 (하나의 DLL)
- 첫 번째 타겟: **LoL 30일 모작** (풀스택 MOBA — 클라이언트, 서버, 백엔드, 안티치트)
- 두 번째 타겟: **엘든링 모작** (액션RPG — LoL 완료 후 진행)
- C++20 / MSVC v143 / Visual Studio 2022
- 아키텍처: Platform → Core → RHI → Renderer → ECS → Network → AntiCheat → Gameplay → Game
- Engine DLL + Game EXE(s) 분리
