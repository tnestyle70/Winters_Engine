# 13. 협업 · 프로세스 · 개발 문화

> 면접 대본 겸 지식 베이스. 이 챕터의 모든 파일 경로는 repo-relative 이며, 인용한 규칙/스크립트는 실제 파일을 열어 검증한 것만 적었다.

---

## ① 도메인 한 줄 정의

"Winters 의 개발 프로세스는 **'규칙은 문서에 적고, 강제는 빌드가 한다'** 는 원칙 위에서, 두 대의 장비와 LLM 에이전트(Claude/Codex)를 하나의 팀처럼 운영한 개발 문화입니다. 반복 실수는 한 줄 규칙으로 박제하고, 문서로만 지켜지던 규칙은 반복해서 깨질 때마다 빌드 게이트(PreBuild lint), 훅(hook), 검증 하네스(harness)로 승격시켰습니다."

---

## ② 구조와 데이터 흐름 — "실수가 규칙이 되고, 규칙이 게이트가 되는" 에스컬레이션

이 프로젝트의 프로세스는 정적인 규정집이 아니라, 사고가 발생할 때마다 아래 사다리를 타고 올라가는 **에스컬레이션 파이프라인**이다.

```text
[사고/실수 발생]
   │  1회성이면 → 날짜별 세션 기록 (.md/plan/YYYY-MM-DD_*.md, .md/build/*_REPORT.md)
   ▼
[.claude/gotchas.md]  ← 반복 실수의 조직 기억
   │  포맷: "YYYY-MM-DD - [Area] mistake -> prevention rule/check" 한 줄
   │  CLAUDE.md 가 @import 로 매 세션 자동 로드
   ▼
[.md/architecture/*]  ← 구조적 결정으로 승격
   │  COMPASS(의도) ↔ DEPENDENCY_MAP(실측, file:line 근거)
   │  DESIGN_PHILOSOPHY(P1~P4) / ERROR_HANDLING_POLICY / ENGINE_CONVENTIONS
   ▼
[기계 강제]  ← 사람/LLM 의 기억에 의존하지 않는 단계
   ├─ Tools/Harness/Check-SharedBoundary.ps1   (GameSim PreBuild lint → 위반 시 빌드 실패)
   ├─ Tools/Harness/Run-S17RhiValidation.ps1   (push 전 하네스: rg audit + 이중 빌드 + 스모크)
   ├─ .claude/hooks/implementation-handoff-pretool-guard.ps1  (plan-first 를 훅으로 강제)
   └─ .claude/hooks/gotchas-refresh-context.ps1               (gotcha 박제 반자동화)
```

문서 트리(`.md/`)는 역할별로 분리되어 있다 (`.md/architecture/WINTERS_CODEBASE_COMPASS.md` "협업 문서 구조" 절에 명시):

| 디렉터리/파일 | 역할 |
|---|---|
| `CLAUDE.md` / `AGENTS.md` | 행동 규칙만. "코드/빌드파일/rg 로 답할 수 있는 사실은 적지 않는다"는 Document Policy, 100줄 넘으면 분할 |
| `.claude/gotchas.md` | 반복 실수 방지 로그 (현재 27개+ 항목) |
| `.md/architecture/` | 설계 철학(P1~P4), 의존성 지도, 에러 정책, 컨벤션, 핸드오프 가이드 |
| `.md/collab/` | OWNERSHIP_MATRIX, GIT_SYNC_RULES, ACTIVE_WORK_PACKETS, HARNESS_RULES |
| `.md/build/` | 날짜별 하네스 리포트 (자동 생성), 서드파티 통합 가이드 |
| `.md/plan/` | 날짜별 일회성 계획서 |
| `.md/계획서작성규칙.md` | 계획서 출력 형식 (아래 ③-5) |

핵심은 **읽기 순서가 명시적**이라는 것이다. `AGENTS.md` 최상단이 "이 파일 → gotchas.md → compass → 도메인 문서" 순서를 못 박고, `WINTERS_HANDOFF_GUIDE.md` §1 이 신규 진입자(사람/에이전트) 기준으로 같은 순서를 다시 명시한다. 사람이든 LLM 이든 새 세션이 같은 순서로 온보딩된다.

그리고 cross-module 작업 전에는 compass 말미의 **"작업 전 체크" 7문항**을 먼저 답하게 되어 있다 (`.md/architecture/WINTERS_CODEBASE_COMPASS.md`):

1. gameplay truth 인가, presentation 인가?
2. 새 의존이 어느 방향으로 생기는가?
3. Engine public header 를 바꾸는가?
4. DX11 concrete type 을 public/API 로 밀어 올리는가?
5. normal F5 runtime 을 debug/lab path 로 우회하고 있지는 않은가?
6. LoL 전용 코드가 Elden/공용 엔진 경계로 새는가?
7. 문서 갱신이 필요한 architectural decision 인가, gotcha 인가, 일회성 plan 인가?

7번이 이 챕터의 핵심 분류기다 — **모든 변경은 기록 위치가 미리 정해져 있다**: 구조 결정 → compass/architecture, 반복 실수 → gotchas, 일회성 → 날짜별 plan. 기록할 곳을 고민하는 비용 자체를 없앴다.

---

## ③ 핵심 설계 결정과 트레이드오프

### 결정 1. 아키텍처 경계를 리뷰 습관이 아니라 PreBuild lint 로 강제

- **왜**: "Shared/GameSim 은 Engine/DX/ImGui 를 include 하지 않는다"는 규칙은 컴파일러가 강제할 수 없다. GameSim 의 include 경로에 `EngineSDK/inc` 가 열려 있어서, 위반해도 그냥 컴파일된다. 문서에 "하지 마라"만 적으면 시간이 지나며 반드시 무너진다 — 실제로 감사 시점에 Shared→Engine 직접 include 가 80개 파일까지 쌓여 있었다.
- **대안**: (a) 코드 리뷰에서 눈으로 잡는다, (b) include 경로 자체를 제거한다, (c) 텍스트 lint 를 빌드에 물린다.
- **선택**: (c). `Tools/Harness/Check-SharedBoundary.ps1` 이 Shared 하위 전 `.h/.cpp/.hpp/.inl` 을 정규식(`#include "(ECS/|Engine_Defines\.h|Client/|Server/|d3d11|dxgi|imgui)`)으로 스캔해서 위반 시 file:line 출력 + `exit 1`. 이 스크립트가 `Shared/GameSim/Include/GameSim.vcxproj` 의 PreBuildEvent(Debug/Release 양쪽)에서 UpdateLib.bat 직후 실행되므로, **위반 = GameSim 빌드 실패**다. Phase 7F 어댑터(`Shared/GameSim/Core/Ecs/*`, `Core/World/World.h`)만 화이트리스트 예외. (b)는 어댑터가 아직 Engine 타입(`using World = ::CWorld`)에 링크 의존이 남아 있어 당장 불가능 — 백엔드 교체 후의 최종 단계로 남겨뒀다.
- **감수한 비용**: 텍스트 정규식 lint 라 컴파일러 수준의 정밀함은 없다(주석 안의 문자열도 잡을 수 있음). 화이트리스트를 손으로 유지해야 하고, 규칙이 바뀌면 스크립트와 문서(`WINTERS_DEPENDENCY_MAP.md` §3)를 함께 갱신해야 한다.

### 결정 2. "의도 문서"와 "실측 문서"의 분리 — code wins over docs

- **왜**: 설계 문서는 반드시 코드와 어긋나게 된다. 실제로 `WINTERS_ENGINE_INTEGRATION_REVIEW.md` 의 "Engine→GameSim UI 위반" 주장은 커밋 f9d4d5c 에서 이미 해소됐는데 문서에 남아 있어, 스테일 주장을 재인용할 뻔한 선례가 있었다.
- **대안**: (a) 문서 하나에 규칙과 현황을 같이 적고 부지런히 갱신, (b) 문서를 아예 최소화, (c) 소유권 분리.
- **선택**: (c). `WINTERS_CODEBASE_COMPASS.md` 가 규칙의 **의도**를 소유하고, `WINTERS_DEPENDENCY_MAP.md` 가 **실측 상태**(grep 전수 감사 결과를 ✅/⚠️ 표 + file:line 근거)를 소유한다. 실측 문서는 스스로 "코드가 바뀌면 스테일해질 수 있다 — 재검증 후 인용할 것"을 헤더에 명시하고, §5 유지 규칙에 "스테일 위반 주장 인용 금지"를 박았다. 최상위 규칙은 code wins over docs.
- **감수한 비용**: 문서 두 벌 유지. 경계 규칙이 바뀌면 compass 를, 위반이 해소되면 map 을 각각 갱신해야 한다. 대신 "문서가 낡아서 틀린 주장을 퍼뜨리는" 최악의 시나리오를 구조적으로 차단했다.

### 결정 3. 빌드 정의 소유권 — vcxproj 가 권위, CMake 는 보조

- **왜**: 레포에 빌드 정의가 두 개다(Winters.sln + vcxproj / 루트 CMakeLists.txt). CMake 쪽 `WintersWorkspaceMap` 은 실제로 아무것도 컴파일하지 않는 IDE 브라우징용 source_group 맵인데, 이걸 빌드 소스로 착각하고 CMake 에만 파일을 추가하면 legacy 타깃에서 조용히 빠진다.
- **대안**: (a) 빌드를 CMake 로 완전 통일, (b) sln 으로 완전 통일(Elden 에디터 포기), (c) 소유권 규칙을 명문화하고 병존.
- **선택**: (c). gotcha 2026-05-28 로 박제: "compiled 파일을 legacy 타깃(Server/Client/Engine)에 추가할 때는 owning `.vcxproj` 와 `.vcxproj.filters` 를 고치고, CMake 는 CMake-owned 타깃(Elden 계열)이나 workspace map 동작에만 손댄다." 통일을 안 한 이유는 실리 — sln 은 게임 3바이너리의 검증된 권위 빌드고, CMake/Ninja 는 EldenRing 에디터와 브라우징 맵을 담당한다.
- **감수한 비용**: 두 정의의 drift 위험. 실제로 `WINTERS_DEPENDENCY_MAP.md` §1 이 확정한 함정들이 있다 — CMake WintersEngine 은 `IMGUI_API=dllexport` 가 누락돼 CMake 빌드 DLL 이 EngineSDK/bin 을 덮으면 vcxproj 클라에서 ImGui 심볼 문제가 날 수 있고, EldenRingEditor 는 CMake 전용이라 sln 빌드로는 절대 컴파일되지 않아 조용히 썩을(rot) 위험이 있다. 후자는 검증 하네스가 CMake 빌드 + 스모크에 강제로 포함시켜 커버한다.

### 결정 4. gotchas.md — 실수를 조직 기억으로 만드는 최소 비용 포맷

- **왜**: 혼자(+LLM) 개발하면 포스트모템을 읽어줄 팀이 없다. 같은 실수가 세션을 건너 반복되는 걸 막을 장치가 필요했다.
- **대안**: (a) 위키식 장문 포스트모템, (b) 코드 주석에 분산, (c) 한 줄 포맷의 append-only 로그 + 매 세션 자동 로드.
- **선택**: (c). `.claude/gotchas.md` 는 `YYYY-MM-DD - [Area] mistake -> prevention rule/check` 한 줄 포맷만 허용하고, "rg/빌드파일로 답할 수 있는 코드 사실 금지, 일회성 사건 금지, ~200줄 넘으면 분할" 규칙을 상단에 명시한다. `CLAUDE.md` 가 `@.claude/gotchas.md` 로 임포트해서 매 세션 LLM 의 컨텍스트에 강제 주입된다. 실제 항목의 스펙트럼을 보면 이 로그의 역할이 보인다:
  - `2026-05-14 - [Plan handoff]` 계획 요청 전 구현 시작 금지 → 나중에 PreToolUse 훅(결정 5)으로 승격
  - `2026-05-20 - [Champion body yaw offset]` 챔피언별 메시 forward 축 차이 → 산발 +PI 패치 금지, 헬퍼 경유 규칙
  - `2026-05-28 - [Build ownership]` CMake WorkspaceMap 은 브라우징 맵 → vcxproj/.filters 가 빌드 소유자 (결정 3)
  - `2026-07-09 - [Async lifetime]` `std::async` future 폐기 = 소멸자 동기 블로킹 → async 래퍼는 future 수명 소유 의무
- 여기에 반자동화까지 붙였다: `.claude/hooks/gotchas-refresh-context.ps1` 은 UserPromptSubmit 훅으로, 프롬프트에 "CLAUDE.md" + (실수/mistake + 반영/추가/기록/박제) 또는 "다시는/never again" 이 동시에 등장할 때만 발동해 "기존 항목 중복 확인 → 재사용 가능한 실패 패턴만 추출 → 한 줄 추가" 지시를 additionalContext 로 주입한다. (한글 트리거를 유니코드 코드포인트 배열로 하드코딩해 스크립트 인코딩 문제를 회피한 디테일도 있다.)
- **감수한 비용**: 한 줄로 압축하는 규율이 필요하다. 사건의 맥락은 날짜별 세션 기록으로 분리해야 하고, 포맷을 어기면 로그가 인벤토리로 변질된다. 대신 27개+ 항목이 쌓인 지금도 전체가 한 화면 분량이라 "매 세션 전부 읽기"가 실제로 가능하다.

### 결정 5. plan-first 워크플로우 — 그리고 그것을 훅으로 물리적으로 강제

- **왜**: 나는 LLM 에게 "계획서/코드 프리뷰를 먼저 보여달라"고 요청한 뒤 직접 검토하고 적용하는(direct-apply) 워크플로우를 쓴다. 그런데 LLM 이 계획 요청에 곧바로 파일을 수정해버리는 사고가 실제로 났다 (gotcha 2026-05-14).
- **대안**: (a) 프롬프트마다 "수정하지 마"라고 반복, (b) 규칙 문서에 적고 기대, (c) 툴 호출 자체를 차단하는 훅.
- **선택**: (c). `.claude/hooks/implementation-handoff-pretool-guard.ps1` 은 PreToolUse 훅으로, 마지막 유저 프롬프트에 plan/handoff/계획서/구현/단계 류 트리거가 있고 apply/수정/반영/적용/검토 류 승인어가 **없으면**, Edit/Write/MultiEdit 및 파일쓰기성 Bash(Set-Content/Out-File/apply_patch/리다이렉트)를 `permissionDecision=deny` 로 거부하고 "handoff 템플릿을 먼저 출력하라"는 사유를 돌려준다. 계획서 자체의 품질도 규격화했다: `.md/계획서작성규칙.md` 가 "계획서는 구현자가 바로 붙잡고 수정할 수 있는 코드 변경 지시서다"를 최상위 목표로, `Session - [목표 한 줄]` 시작 형식을 강제한다. `AGENTS.md` Plan Authoring 절은 "새 파일 섹션은 전체 파일 본문을 코드블록으로 포함, 불확실하면 `CONFIRM_NEEDED` 마킹, 'X를 X로 교체' 같은 no-op 앵커 금지"까지 규정한다 (gotchas 2026-05-18/19 에서 역산된 규칙).
- **감수한 비용**: 트리거/승인어 매칭이 휴리스틱이라 위양성(정당한 수정을 막음)/위음성이 가능하다. 승인어를 명시적으로 쓰는 습관이 나에게도 요구된다. 그래도 "사고 후 되돌리기"보다 "사고 자체를 툴 레벨에서 차단"이 압도적으로 싸다.

### 결정 6. LLM 교차 리뷰 사이클 — 작성자와 리뷰어를 다른 모델로

- **왜**: 한 LLM 이 작성과 리뷰를 다 하면 같은 맹점을 공유한다. 사람 리뷰어가 없는 환경에서 리뷰의 독립성을 확보해야 했다.
- **대안**: (a) 셀프 리뷰만, (b) 테스트로만 검증, (c) 다른 모델(Codex)로 교차 리뷰.
- **선택**: (c). 대표 사례가 2026-04-28 Worker-Safety + Minion Combat 통합이다. Claude 가 작성한 병렬 미니언 AI 설계를 Codex 가 4회에 걸쳐 리뷰해 총 20건을 지적했고 전수 반영했다:
  - 1차: worker-safety 5건 — MinionAI cross-entity write, FindClosest read race, CommandBuffer/Scheduler contract, AStar contention
  - 2차: DamageEvent 만으로는 부족 → Decision/Apply 2-pass 필요, main slot 0 충돌 회피
  - 3차: cooldown hold state, Nav chase override, `ISystem::Initialize` 우회 등
  - 4차: Chase 의 Velocity→Transform 적용 주체 부재, buffer 크기 강제, cooldown tick 정밀화 등
- 네트워크 계획서도 같은 사이클(UDP 마스터플랜 7건 보정, TCP MVP 11건 보정)을 돌았다. 이 과정에서 Codex 의 리뷰 패턴 자체를 프로세스로 흡수했다: **신설 전 기존 인프라 4~5폴더 grep**(eFxBlendMode 신설하려다 기존 eBlendPreset 발견해 폐기), **데이터 형태(컴포넌트→cbuffer→셰이더) 우선**, **원인 확정 전 큰 패치 회피**, **리뷰는 ✅/⚠️/❌ 매트릭스로 차분만**. 이 패턴들은 `winters-skills/code-scaffolding/SKILL.md` 등 스킬 문서로 성문화해 다음 세션에 자동 적용된다.
- **감수한 비용**: 리뷰 왕복 시간. 그리고 두 모델의 지적이 충돌할 때 최종 판정은 결국 내가 코드를 읽고 내려야 한다 — 이건 비용이라기보다 이 워크플로우가 내 실력을 요구하는 지점이다.

### 결정 7. 2-머신 협업을 다인 팀처럼 운영 — Ownership Matrix + Work Packet

- **왜**: 노트북/데스크탑 두 장비가 같은 레포를 번갈아 쓰면 merge conflict 와 반쯤 배포된 SDK 트리 같은 동기화 사고가 난다.
- **대안**: (a) 한 장비 완결 후 push (직렬화), (b) 그때그때 conflict 해결, (c) 파일 소유권과 작업 단위를 명문화.
- **선택**: (c). 세 문서가 한 세트다.
  - `.md/collab/OWNERSHIP_MATRIX.md`: 영역별 기본 owner 지정(RHI backend=Laptop, LoL runtime bridge=Desktop, Resource/model bridge=Laptop-first/Desktop handoff 등). 충돌 위험이 높은 파일은 **Always-Lock** 으로 지정해 한 packet 에서만 수정 — `Scene_InGame.h`, `ModelRenderer.h`, `Model.h`/`Mesh.h`, Client/Engine 의 `.vcxproj`+`.filters`, 그리고 `EngineSDK/inc/**`(hand-edit 자체 금지, UpdateLib.bat 산출물만 커밋).
  - `.md/collab/ACTIVE_WORK_PACKETS.md`: 각 작업을 Reserved/Active/Handoff/Merged/Blocked 상태 + owner device + branch + owned/read-only paths + validation harness + report path 로 기록. 같은 파일을 양쪽에서 만져야 하면 먼저 Handoff 상태로 넘기고 상대가 `git pull --rebase` 를 끝낸 뒤 작업한다.
  - `.md/collab/GIT_SYNC_RULES.md`: `codex/<area>-<task>` 브랜치 prefix, push 전 `fetch → rebase → Run-S17RhiValidation.ps1 → diff --check` 순서, 금지 목록(dirty worktree 무조건 pull 금지, conflict 해결용 `reset --hard`/`checkout --` 금지), 그리고 장비 간 handoff 시 남길 것(branch/commit, touched files, 검증 결과, report path, 다음 장비가 만져도 되는 paths).
- **감수한 비용**: 1인 프로젝트치고 절차가 무겁다. packet 문서 갱신을 빼먹으면 절차가 형해화된다. 대신 이 구조는 그대로 다인 팀에 이식 가능하고, 면접에서 "협업 시스템을 설계해 본 경험"으로 증명할 수 있는 실물이 된다.

### 결정 8. 코딩 컨벤션을 문서 → 스킬 → 리뷰 연계로 계층화

- **왜**: 컨벤션은 문서에만 있으면 새 코드(특히 LLM 이 생성하는 코드)에 일관 적용되지 않는다. "컨벤션 문서를 안 읽고 Manager 를 통째로 재작성"하는 대형 사고를 실제로 겪었다.
- **대안**: (a) 컨벤션 문서 하나로 통일, (b) clang-format/clang-tidy 류 도구 강제, (c) 문서 + 코드 생성 파이프라인(스킬) + 리뷰 연계의 계층화.
- **선택**: (c) 를 먼저. 상세 규칙은 `.md/architecture/WINTERS_ENGINE_CONVENTIONS.md` 가 소유한다 — 파일명은 `C` 접두사 금지 / 클래스명은 `C` 필수(`GameInstance.h` → `class CGameInstance`), 인터페이스 `I` 접두사, `m_` + 타입 문자 접두사, `f32_t`/`u32_t` 별칭, private ctor + static `Create()` 팩토리 + `unique_ptr` 반환, DLL 경계에는 `CGameInstance` 게이트웨이만 신규 export. 그리고 이 규칙을 클래스 생성 시점에 자동 적용하기 위해 `winters-skills/code-scaffolding/SKILL.md` 로 스킬화했다: 실행 순서(engine-api → module-list → gotchas 확인)와 "생각 흐름"(클래스 분류/레이어/소유권/스레드 모델/vcxproj 등록/해당 gotcha 번호) 출력을 강제하고, 생성 후 code-review 로 자동 연계된다. "모든 새 시스템은 ImGui 튜닝 UI 와 함께 작성 — 하드코딩 값 금지" 같은 정책 수준의 규칙도 여기 들어 있다. 컨벤션 §0.6 의 한 줄이 이 문화의 요약이다: **"혼자 작업해도 다음 사람이 읽는다는 기준으로 작성한다."**
- **감수한 비용**: (b) 수준의 기계 강제(정적 분석)는 아직 없어서, 스킬을 우회한 코드에는 리뷰 눈이 필요하다. 또 기존 코드의 일괄 리네임은 강제하지 않아(`CTimer.h` 등 레거시 파일명 공존) 혼재를 감수하고 "신규 코드만 규칙 적용 + 만질 때 점진 이관"으로 갔다 — 일괄 리네임의 diff 폭발보다 낫다고 판단했다.

### 보론: LLM 협업 프로세스를 면접에서 '강점'으로 서술하는 법

이 프로젝트에서 LLM 사용은 숨길 일이 아니라 설계 대상이었다. 서술 전략은 세 문장 구조를 쓴다.

1. **프레이밍**: "AI 가 코드를 짰다"가 아니라 "AI 를 신뢰 가능한 팀원으로 만드는 시스템을 설계했다". 게이트(lint/하네스), 계약(계획서 규격, `CONFIRM_NEEDED`), 기억(gotchas @import), 권한 통제(PreToolUse deny 훅) — 이건 사람 신입에게 주는 온보딩/리뷰/권한 체계와 정확히 같은 문제다.
2. **증거**: 각 장치가 실제 사고에서 역산됐음을 파일로 보여줄 수 있다 — 훅의 기원 gotcha(2026-05-14), lint 의 기원(직접 include 80파일), 교차 리뷰 20건 반영 기록. "프로세스를 만들었다"가 아니라 "사고 → 규칙 → 자동화 사다리를 반복 운용했다"로 말한다.
3. **한계 인정**: 최종 판단(리뷰 충돌 판정, 디버깅 방향 결정, 경계 설계)은 위임 불가능했고 그게 내 역량으로 남았다. LLM 이 같은 stale 분석을 3회 반복했을 때 "계측으로 전환" 규칙을 만든 건 도구가 아니라 나였다.

이 서술이 통하는 이유: 어느 팀이든 곧 겪을 문제(AI 에이전트의 코드 기여를 어떻게 검증·통제하나)에 대해 실물 운영 경험을 가진 지원자는 아직 드물기 때문이다.

---

## ④ 어려웠던 점과 해결

**1) 문서 규칙은 반드시 무너진다는 것을 인정하기까지.**
Shared→Engine 경계는 compass 에 처음부터 적혀 있었지만 직접 include 가 80개 파일까지 쌓였다. 해결은 3단이었다: 오염 include 체인 절단(Engine_Defines 가 dinput.h/using namespace 를 sim 에 전이시키던 것) → `Shared/GameSim/Core/Ecs/` 어댑터 9종 신설 + 79파일 재라우팅 → `Check-SharedBoundary.ps1` 을 PreBuild 에 물려 재발을 빌드 실패로 전환. 이후 "규칙은 있는데 기계 강제가 없다"가 이 코드베이스의 메타 패턴임을 인식하고, 새 규칙을 만들 때 "이걸 기계로 강제할 수 있는가"를 먼저 묻게 됐다.

**2) 배포 스크립트의 파일 잠김 사고.**
`UpdateLib.bat` 이 EngineSDK 트리를 매 빌드 rd /S 로 purge 하던 시절, 병렬 빌드 중 파일이 잠겨 있으면 SDK 트리가 반쯤 비어 Client 컴파일이 깨졌다. 해결은 purge 를 `WINTERS_SDK_PURGE=1` 환경변수 뒤로 옮기고 평시에는 xcopy 덮어쓰기만 하는 것 — 스크립트 주석에 사고 경위가 그대로 남아 있다. 같은 스크립트가 `*_Manager.h` 를 SDK 에서 del /S 로 제거해 "Client 는 매니저를 CGameInstance 게이트웨이로만 접근"하는 경계를 물리적으로 강제한다는 점도 이 배포 허브의 특징이다.

**3) LLM 협업 자체의 실패 모드들.**
계획 요청에 바로 구현(→ PreToolUse deny 훅), 계획서를 프로즈 요약으로 때움(→ 전체 코드블록 + `CONFIRM_NEEDED` 규칙), 같은 stale 분석을 3회 반복(→ "추측 2회 빗나가면 즉시 profiler counter 계측으로 전환" 규칙 — 미니언 stuck 사고에서 counter 5분이 추측 1시간을 이겼다). LLM 의 실패 모드를 하나씩 프로세스 규칙/훅/스킬로 변환한 것이 이 프로젝트 개발 문화의 실체다.

**4) 레거시가 쌓인 코드베이스의 인수인계 — 지뢰밭을 숨기지 않고 목록화.**
코드베이스가 커지면서 "아는 사람만 아는 함정"이 늘었다. 이걸 미화하지 않고 `WINTERS_HANDOFF_GUIDE.md` §5 "알려진 지뢰밭"으로 전부 문서화했다: Scene_InGame 은 9개 TU 로 분할했지만 여전히 8,844줄 단일 클래스라 헤더 수정 시 클라 절반이 리컴파일된다는 것, `Client/Public/Network/Backend/` 의 SnapshotApplier.h·CommandSerializer.h 가 살아있는 `Network/Client/` 버전과 동명인 **죽은 스텁**이라 잘못 include 하면 조용히 ODR 위반이 된다는 것, `Shared/Network/PacketDef.h` 는 includer 0 에 TPS=20 으로 실제(30Hz)와 모순되는 죽은 계약이라는 것, suspicion 시스템은 `IsSuspicious` 호출자가 0 이라 악성 패킷이 드랍만 되고 킥이 없다는 것 등. 핵심 사상은 두 가지다 — (1) 부채를 "해소된 것"처럼 꾸미지 않고 상태 그대로 노출한다, (2) 문서 신뢰 규칙 "code wins over docs" 를 인수인계 문서 자체에 명시해 이 목록조차 재검증 대상임을 못 박는다. 같은 맥락에서 `WINTERS_ERROR_HANDLING_POLICY.md` 의 잔여 침묵 지점 목록에는 "여기 적힌 건 고쳐야 할 것이지 따라 해도 되는 패턴이 아니다"라는 경고를 붙여, 후속 작업자(사람이든 LLM 이든)가 부채를 모범 사례로 오인하고 복제하는 것을 막았다.

**5) 검증을 "빌드 통과"에서 "실행 관측"으로 끌어올리기.**
`Tools/Harness/Run-S17RhiValidation.ps1` 은 push 전 게이트를 한 스크립트로 묶는다. 순서대로:

1. rg 로 Client/Public·Shared 에 금지 그래픽스 심볼(`ID3D11|ID3D12|d3d11.h|d3d12.h|DX11Shader|DX11Pipeline`) audit — rg 의 no-match(exit 1)를 PASS 로 뒤집는 역전 로직으로 "금지 검사"를 게이트화
2. CMake/Ninja 빌드 (sln 이 못 잡는 EldenRingEditor 를 여기서 강제 커버)
3. MSBuild Winters.sln 빌드
4. runtime smoke — 5개 실행파일(WintersElden dx12/dx11 probe, EldenRingEditor, WintersGame, `--rhi-scene-only`)을 N초 띄워 생존 확인, 죽으면 FAIL
5. step 별 PASS/FAIL/exit code/소요 시간/output tail 을 담은 리포트를 `.md/build/<timestamp>_S17_RHI_VALIDATION_HARNESS_REPORT.md` 로 자동 생성. 첫 실패에서 중단.

"서버 로그만으로 클라 비주얼 성공을 판정하지 않는다", "실패를 관측할 수 없는 변경은 완성이 아니다"(`WINTERS_DESIGN_PHILOSOPHY.md` 회사 코드 작업원칙 — 실패 분기에 P1 진단이 없으면 리뷰 반려)가 이 하네스의 사상적 근거다.

---

## ⑤ 향후 개선 방향

- **CI 부재**: 하네스는 아직 로컬 수동 실행이다. 원격 빌드 서버에서 Check-SharedBoundary + Run-S17RhiValidation 을 push 마다 자동 실행하는 것이 다음 단계다 (2026-07-10 UE5.7 대비 감사에서도 HIGH 항목).
- **flatc 코드젠 레이스**: 동일 FlatcCodegen 타깃이 GameSim/Client/Server 3곳에서 Inputs/Outputs 선언 없이 매 빌드 실행돼 `msbuild /m` 병렬 빌드 시 같은 `*_generated.h` 를 동시 재작성할 수 있다 (`WINTERS_DEPENDENCY_MAP.md` §1 감사 확정). Inputs/Outputs 선언으로 증분화 + 단일 소유 프로젝트로 이관이 필요하다.
- **EldenRingEditor 의 sln 편입 또는 CI 강제**: CMake 전용 타깃의 rot 위험을 하네스가 임시로 커버 중인데, 툴 투자 전에 빌드 경계부터 확보해야 한다.
- **경계 검사의 최종 형태**: 텍스트 lint 는 과도기다. Shared 소유 결정론 ECS 백엔드를 만들어 어댑터를 repoint 하면 `EngineSDK/inc` include 경로 자체를 GameSim 에서 제거할 수 있고, 그때 경계는 "컴파일이 안 되는" 수준으로 강제된다 (gotcha 2026-07-09 Dependency boundary).
- **gotchas 분할**: 27개+ 항목이 200줄 한계에 접근하면 영역별 서브페이지로 분할하는 규칙이 이미 예약돼 있다.
- **유닛 테스트 도입**: 현재는 결정론 해시 + 스모크 + 경계 audit 가 회귀 방어의 전부다. CI 도입과 함께 CommandExecutor/Pathfinder 같은 순수 로직부터 유닛 테스트를 붙이는 것이 정직한 다음 단계다.
- **문서 스테일 감지의 자동화**: 지금은 "재검증 후 인용" 규칙(사람의 규율)에 의존한다. DEPENDENCY_MAP 의 file:line 근거를 주기적으로 re-grep 해서 스테일 항목을 표시하는 스크립트로 승격할 수 있다 — 이 챕터의 사다리(문서 규칙 → 기계 강제)를 문서 자체에 적용하는 셈이다.

---

## ⑥ 면접 Q&A

**Q1. 혼자 만든 프로젝트인데 '협업 경험'이라고 할 수 있나요?**
- 답변 골격: 물리적으로는 1인이지만, 운영은 다인 팀 구조였다. (1) 노트북/데스크탑 두 장비를 서로 다른 작업자로 보고 `OWNERSHIP_MATRIX.md` 의 파일 소유권 + Always-Lock 목록 + 상태 있는 Work Packet + `GIT_SYNC_RULES.md` 의 브랜치/rebase/하네스 절차로 운영했다. (2) Claude 가 작성하고 Codex 가 교차 리뷰하는 사이클에서 리뷰 20건 전수 반영 같은 실제 리뷰 왕복이 있었다. 협업의 본질인 "소유권 분할, 충돌 관리, 리뷰, 인수인계 문서"를 전부 실물로 만들었다.
- 꼬리질문 대비: "팀에 오면 뭐가 다를 것 같나" → 이 구조에서 부족했던 건 '내 규칙에 대한 반대'다. 사람 팀에서는 컨벤션 자체가 협상 대상이라는 걸 알고 있고, 그래서 규칙마다 근거 사고(gotcha)를 링크해 두는 습관을 들였다 — 근거 있는 규칙만 남기고 협상하면 된다.

**Q2. 아키텍처 규칙이 시간이 지나도 안 무너지게 어떻게 보장했나요?**
- 답변 골격: 문서는 강제가 아니라는 걸 80개 파일 위반으로 배웠다. 컴파일러가 표현할 수 없는 규칙(Shared 는 Engine/DX/ImGui include 금지 — include 경로가 열려 있어 컴파일은 됨)을 `Check-SharedBoundary.ps1` 텍스트 lint 로 만들어 GameSim PreBuildEvent 에 물렸다. 위반 = file:line 출력 + exit 1 = 빌드 실패. 어댑터 디렉터리만 화이트리스트.
- 꼬리질문 대비: "텍스트 lint 의 한계는?" → 정밀도가 낮고 화이트리스트 유지비가 있다. 최종형은 include 경로 제거(컴파일 불가로 강제)인데, 어댑터가 아직 Engine CWorld 에 링크 의존이라 백엔드 교체 후로 미뤘다 — 과도기 게이트로서의 lint 라고 명확히 위치 지었다.

**Q3. 코드 리뷰는 어떻게 했나요?**
- 답변 골격: 작성 모델과 리뷰 모델을 분리한 교차 리뷰. 병렬 미니언 AI 작업에서 Codex 리뷰 4회, 총 20건(cross-entity write race, Decision/Apply 2-pass, slot 충돌 등)을 전수 반영한 게 대표 사례. 리뷰에서 배운 패턴 — 신설 전 기존 인프라 grep, 데이터 형태 우선, 원인 확정 전 큰 패치 회피, ✅/⚠️/❌ 매트릭스 차분 리뷰 — 을 스킬 문서로 성문화해 다음 작업에 자동 적용했다.
- 꼬리질문 대비: "리뷰 의견이 틀리면?" → 실제로 있었다. 최종 판정은 항상 코드를 열어 내가 내렸고, 그 판단력이 이 워크플로우의 병목이자 내가 기른 근육이다.

**Q4. 설계 문서가 코드와 어긋나는 문제는 어떻게 관리했나요?**
- 답변 골격: 세 가지 규칙. (1) code wins over docs. (2) 규칙의 의도(compass)와 실측 상태(DEPENDENCY_MAP, file:line 근거 + ✅/⚠️ 표)를 서로 다른 문서가 소유. (3) 실측 문서에 "재검증 후 인용, 스테일 위반 주장 재인용 금지"를 명시 — 실제로 이미 해소된 위반 주장이 다른 문서에 남아 있던 선례에서 역산한 규칙이다.
- 꼬리질문 대비: "그럼 문서를 왜 쓰나" → 코드로 답할 수 있는 사실은 안 쓴다는 Document Policy 가 있다. 문서는 행동 규칙, 의사결정 근거, 그리고 rg 로 찾을 수 없는 '왜'만 담는다.

**Q5. 같은 실수를 반복하지 않기 위한 장치가 있나요?**
- 답변 골격: `.claude/gotchas.md` — `날짜 - [영역] 실수 -> 방지 규칙` 한 줄 포맷의 append-only 로그. CLAUDE.md 가 @import 해서 매 세션 자동으로 컨텍스트에 들어간다. "이 실수 다시는 하지 않도록 반영해줘"라는 발화를 감지하는 UserPromptSubmit 훅이 박제 절차(중복 확인 → 패턴 추출 → 한 줄 추가)를 반자동화한다. 27개+ 항목이 쌓였고, 이 중 여러 개가 나중에 빌드 게이트나 훅으로 승격됐다 — gotcha 는 규칙의 저수지다.
- 꼬리질문 대비: "예를 하나 들면" → "plan 요청 전 구현 금지" gotcha(2026-05-14)가 나중에 PreToolUse deny 훅으로 승격된 흐름이 가장 전형적이다.

**Q6. LLM 으로 개발했다면 본인 실력은 어떻게 증명하나요?**
- 답변 골격: LLM 은 증폭기고, 증폭기는 입력 품질과 검증 체계를 요구한다. 내가 설계한 것은 (1) 검증 체계 — 경계 lint, 검증 하네스, SimLab 결정성 해시(리팩터 전후 300틱 해시 동일 증명), 스모크 토큰, (2) 프로세스 — 계획서 규격(`CONFIRM_NEEDED`, no-op 앵커 금지), plan-first 훅, gotcha 루프, (3) 판정 — 교차 리뷰 충돌 시 코드를 읽고 내리는 최종 결정. "AI 가 짠 코드"가 아니라 "AI 를 신뢰 가능하게 만드는 엔지니어링 시스템"이 내 산출물이고, 이건 앞으로 모든 팀이 필요로 하는 역량이라고 생각한다.
- 꼬리질문 대비: "AI 없이도 짤 수 있나" → 이 프로세스의 전제가 그것이다. 리뷰 판정, 디버깅 방법론(계측 우선, CPU/GPU 경계 분기), 경계 설계는 위임이 불가능해서 전부 내 손에 남아 있다. war-story(Chase-Lev race, yaw ±PI 등)는 해당 도메인 챕터에서 코드 수준으로 설명할 수 있다.

**Q7. 빌드 시스템이 두 개(sln/CMake)인데 어떻게 사고를 막았나요?**
- 답변 골격: 소유권 규칙 명문화 — vcxproj 가 legacy 3바이너리의 권위 빌드 소스, CMake 는 Elden 계열 + 브라우징 맵(WintersWorkspaceMap 은 컴파일하지 않는 source_group 타깃) 담당. "컴파일 파일 추가는 owning vcxproj + .filters" 가 gotcha 로 박제돼 있다. drift 는 감사로 추적한다: CMake 쪽 IMGUI_API dllexport 누락, EldenRingEditor 가 sln 에서 안 빌드되는 rot 위험 — 후자는 하네스의 CMake 빌드 + 스모크 단계에 강제 포함시켜 커버.
- 꼬리질문 대비: "왜 통일 안 했나" → 통일 비용(검증된 sln 빌드 재작성 or 에디터 포기) 대비, 소유권 규칙 + 하네스 커버가 더 싸고 안전하다고 판단했다. 통일은 CI 도입 시점의 과제로 남겼다.

**Q8. 변경 검증(테스트) 프로세스를 설명해 주세요.**
- 답변 골격: push 전 단일 하네스 `Run-S17RhiValidation.ps1` — ① rg 로 금지 그래픽스 심볼 audit(no-match=PASS 역전 로직으로 '금지 검사'를 게이트화) ② CMake/Ninja + MSBuild 이중 빌드 ③ 5개 exe 생존 스모크 ④ step 별 PASS/FAIL/exit/seconds 를 담은 리포트 파일 자동 생성. 게임플레이 쪽은 별도로 서버 `--smoke-seconds` 헤드리스 런과 SimLab 결정성 해시 비교(same input → byte-exact)를 쓴다. 실패 리포트가 `.md/build/` 에 날짜별로 쌓여 회귀 추적이 된다.
- 꼬리질문 대비: "유닛 테스트는?" → 정직하게: 전통적 유닛 테스트 커버리지는 낮다. 대신 결정론 시뮬 해시가 사실상 통합 회귀 테스트 역할을 하고, 실패 가시성(P1) 정책이 관측 불가능한 실패를 리뷰에서 반려한다. 유닛 테스트 도입은 CI 와 함께 묶인 향후 과제다.

**Q9. 코딩 컨벤션을 어떻게 일관되게 적용했나요? 특히 AI 가 코드를 생성하는데.**
- 답변 골격: 3계층 — (1) 규칙의 소유 문서 `WINTERS_ENGINE_CONVENTIONS.md`(파일명/클래스명 C 접두사 비대칭, 타입 별칭, Create 팩토리, GameInstance 게이트웨이 경계), (2) 코드 생성 파이프라인에 물린 스킬(`winters-skills/code-scaffolding/SKILL.md` — 생성 전 레이어/소유권/스레드 모델/vcxproj 등록을 "생각 흐름"으로 출력 강제, 생성 후 code-review 자동 연계), (3) 반복 위반은 gotcha 로 박제(헤더 `using std::` 오염 금지 등). 컨벤션을 "읽어야 하는 문서"에서 "생성 경로에 내장된 절차"로 바꾼 것이 요점이다.
- 꼬리질문 대비: "그래도 어기면?" → 실제로 컨벤션 문서를 건너뛰고 Manager 를 재작성한 사고가 있었고, 그 사고가 "컨벤션 문서 필독" 규칙과 스킬 도입의 계기였다. 사고 → 규칙 → 자동화의 같은 사다리다.

**Q10. 이 코드베이스를 다른 사람에게 인수인계한다면 어떻게 하실 건가요?**
- 답변 골격: 이미 문서가 있다 — `WINTERS_HANDOFF_GUIDE.md`. 읽기 순서(§1), 빌드 함정(§2: 개별 vcxproj 단독 빌드 금지, EngineSDK/inc 손편집 금지), 패치 전 체크리스트(§4: 실패 분기 P1 진단, .fbs 변경 시 클라·서버 동시 배포, 새 파일은 owning vcxproj+.filters), 그리고 §5 "알려진 지뢰밭"(동명 죽은 스텁의 ODR 함정, 죽은 계약 헤더, 미집행 suspicion, 8,844줄 갓클래스)까지. 지뢰를 숨기지 않고 목록화한 것 자체가 인수인계의 품질이라고 생각한다.
- 꼬리질문 대비: "문서가 낡으면?" → 그래서 문서 자체에 code wins over docs 와 스테일 사례(INTEGRATION_REVIEW 의 해소된 위반 주장)를 명시했다. 인수인계 문서는 지도이지 진실이 아니고, 진실은 항상 코드와 재검증이다.

---

## 면접 직전 리마인드 카드 (숫자와 실물)

| 주장 | 뒷받침 실물 |
|---|---|
| 경계는 빌드가 강제한다 | `Tools/Harness/Check-SharedBoundary.ps1` + GameSim.vcxproj PreBuildEvent(Debug/Release), 위반 = exit 1 = 빌드 실패 |
| 위반 80파일을 lint 가동까지 소각 | 어댑터 9종 신설 + 79파일 include 재라우팅, 직접 include 잔존 0 (`WINTERS_DEPENDENCY_MAP.md` §3) |
| 교차 리뷰는 실제로 돌았다 | Codex 리뷰 4회, 총 20건 전수 반영 (2026-04-28 Worker-Safety/Minion Combat) |
| 실수는 조직 기억이 된다 | gotchas 27개+ 항목, 한 줄 포맷, CLAUDE.md @import, refresh 훅 반자동화 |
| plan-first 는 물리적으로 강제된다 | PreToolUse 훅이 승인어 없는 편집 툴 호출을 deny (`.claude/hooks/implementation-handoff-pretool-guard.ps1`) |
| 검증은 실행 관측까지 | 하네스 5단계 (rg audit → CMake → MSBuild → 5-exe 생존 스모크 → 리포트 자동 생성) |
| 2-머신을 팀처럼 | Always-Lock 9개 경로, packet 상태 5종, push 전 rebase+하네스 절차 (`.md/collab/*`) |
| 부채는 숨기지 않는다 | 지뢰밭 목록 (`WINTERS_HANDOFF_GUIDE.md` §5), "고쳐야 할 것 vs 따라해도 되는 것" 구분 경고 |

---

## ⑦ 다른 챕터와의 연결

- **의존성/계층 경계 챕터**: 이 챕터의 결정 1(Check-SharedBoundary)은 경계 챕터의 Phase 7F 어댑터 절단(헤더 오염 절단 → 어댑터 재라우팅 → lint 고정 3단계, `WINTERS_DEPENDENCY_MAP.md` §3)의 마지막 단이다. 규칙의 내용은 그쪽, 규칙을 지키게 만드는 장치는 이쪽.
- **빌드/배포 챕터**: UpdateLib.bat 의 EngineSDK 미러링과 `_Manager.h` purge, flatc 레이스, Client 의 Engine ProjectReference 부재(stale lib 함정)는 빌드 챕터가 상세를 소유한다. 이 챕터는 그 함정들이 어떻게 gotcha → 문서 → 하네스로 관리되는지를 다룬다.
- **에러 핸들링/설계 철학 챕터**: "실패를 관측할 수 없는 변경은 완성이 아니다"(P1, `WINTERS_DESIGN_PHILOSOPHY.md`)가 리뷰 반려 기준으로 작동하는 지점이 프로세스와 철학의 접합부다. dead diagnostics 금지, 계층별 로그 채널 분리(Server=std::cerr, Shared=WintersOutputAIDebugStringA)는 그쪽 챕터 참조.
- **디버깅 방법론 챕터**: "추측 2회 빗나가면 계측으로 전환", 셰이더 우선 read, DIAG 로그 — Codex 협업에서 흡수한 디버깅 사이클은 이 챕터의 결정 6 과 같은 뿌리(교차 리뷰에서 배운 패턴의 성문화)다.
- **네트워크/서버 권위 챕터**: SimLab 결정성 해시와 byte-exact snapshot 검증은 이 챕터 Q8 의 "결정론 = 통합 회귀 테스트" 주장의 기술적 근거를 소유한다.
- **데이터 아키텍처 챕터**: 분리율 28% 스코어카드와 "폴백 카운터 0 수렴 = legacy 삭제 게이트"(zero-reader 규칙)는 이 챕터의 "부채를 점수와 상태로 정직하게 관리한다"는 문화가 데이터 도메인에 적용된 사례다 (`.md/architecture/WINTERS_DATA_ARCHITECTURE.md`).
- **클라이언트 구조 챕터**: Scene_InGame 갓클래스의 "동작 불변 기계적 분할"(verbatim 이동 + 설계문서 링크 주석)은 이 챕터 ④-4 의 지뢰밭 관리와 결정 6 의 리뷰 안전성 원칙이 실제 리팩터링에 적용된 형태다.
