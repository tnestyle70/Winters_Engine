# CLAUDE.md 압축 수정 방안 계획서

**작성일**: 2026-05-01  
**상태**: 사전 검토안 (현재는 historical reference). 작성 당시 기준 `CLAUDE.md` 본문 미수정 상태였으며, 이후 본 검토 결과를 바탕으로 압축 반영이 진행됨.  
**목표**: 루트 코드베이스 실측을 바탕으로 `CLAUDE.md` 를 "작업용 운영 브리프"로 압축하고, 오늘의 그래픽스/RHI 방향까지 정리한 뒤 검토 후 반영한다.

---

## 1. 루트 코드베이스 실측 요약

### 1.1 솔루션 / 실행 단위

- `Winters.sln` 은 현재 **4개 C++ 프로젝트**로 구성된다.
  - `Engine` → `WintersEngine.dll`
  - `Client` → 게임 클라이언트
  - `Server` → IOCP 기반 게임 서버
  - `WintersAssetConverter` → 자산 변환 툴
- 별도로 `Services/` 아래에 **Go 마이크로서비스 6개**가 존재한다.
  - `auth`, `leaderboard`, `matchmaking`, `payment`, `profile`, `shop`

### 1.2 루트 폴더별 현재 역할

| 경로 | 실측 역할 | 현재 판단 |
|---|---|---|
| `Engine/` | DX11 RHI, ECS, Resource, Renderer, UI, Profiler, JobSystem 기반 | 엔진 핵심. 실제 런타임 중심 |
| `Client/` | Scene, 챔프/스킬/FX, 디버그 UI, 맵/정글/미니언 매니저 | 게임 로직 중심. `Scene_InGame` 비대 |
| `Server/` | IOCP, GameRoom, SnapshotBuilder, AntiCheat/LagCompensation 뼈대 | SharedSim 소비 시작됨 |
| `Shared/` | deterministic sim, FlatBuffers schema, 공통 command/snapshot 타입 | Client/Server 공용 핵심 |
| `Shaders/` | 현재 런타임 HLSL 6개 | 아직 PBR/SSAO 없음 |
| `Tools/WintersAssetConverter/` | `.wmesh/.wskel/.wanim` 변환기 | Engine 사설 소스 재컴파일 방식 |
| `Services/` | Go 백엔드 + PostgreSQL/Redis/Kafka 지향 | 콘텐츠/메타게임 영속화 담당 |
| `EngineSDK/` | Engine public header 복사본 + lib 배포 | Engine 수정 후 동기화 필수 |
| `.md/` | architecture / graphics / rhi / sim / backend / security 계획서 | 상세 설계의 실제 소스 오브 트루스 |

### 1.3 코드 규모 체감치

- `Engine/Public` 84개, `Engine/Private` 62개 코드 파일
- `Client/Public` 67개, `Client/Private` 61개 코드 파일
- `Server/Public` 12개, `Server/Private` 14개 코드 파일
- `Shared/` 76개 코드 파일
- `Services/` 41개 Go 코드 파일
- `Shaders/` 6개 HLSL 파일

### 1.4 현재 구현 현실

#### A. 렌더링 / RHI

- 런타임 렌더링은 **사실상 DX11 단일 백엔드**다.
- `Engine/Public/Framework/CEngineApp.h`
  - 공유 `Mesh3D`, `Skinned3D`, `FxSprite`, `FxMesh` 셰이더/파이프라인을 엔진이 소유
- `Engine/Public/RHI/CDX11Device.h`
  - `ID3D11Device*`, `ID3D11DeviceContext*` 를 직접 다루는 DX11 concrete device
- `Engine/Public/Renderer/ModelRenderer.h` + `Engine/Private/Renderer/ModelRenderer.cpp`
  - 공유 `CModel` + 인스턴스별 `CAnimator` 조합의 pImpl 구조
- `Shaders/Mesh3D.hlsl`, `Shaders/Skinned3D.hlsl`
  - 현재 픽셀 셰이더는 사실상 **unlit 텍스처 출력**
  - 광원, BRDF, SSAO, IBL, tone mapping 없음
- 소스 실측상 런타임 코드에는 `PBR`, `GGX`, `Cook-Torrance`, `SSAO`, `Forward+`, `DX12`, `Vulkan` 구현이 **아직 없다**.
  - 이 내용은 현재 대부분 `.md/plan/graphics/*`, `.md/plan/rhi/*` 문서에만 존재

#### B. 게임플레이 / 씬 구조

- `Client/Private/Scene/Scene_InGame.cpp` 는 **2960 lines**
- `Client/Public/Scene/Scene_InGame.h` 는 **549 lines**
- 즉, 실제 인게임 흐름은 아직
  - ECS
  - 레거시 `CTransform`
  - 챔프별 튜닝값
  - FX 시스템
  - 네트워크 snapshot 적용
  - 디버그 UI
  - 직접 렌더 호출
  가 한 클래스에 많이 몰려 있다.
- 반면 방향성은 좋다.
  - `ChampionRegistry`, `SkillRegistry`, `VisualHookRegistry`
  - `Shared/GameSim/Systems/GameplayHookRegistry`
  - `CSystemSchedular`, `CWorld`
  - 챔프별 `FxPresets` / projectile / blade / ult wave 시스템
  로 점진 분해가 시작돼 있다.

#### C. SharedSim / 서버

- `Shared/GameSim` 은 이미 꽤 의미 있는 공용 계층이다.
  - Components: `Health`, `Mana`, `SkillState`, `Stat`, `MoveTarget`, `Buff` 등
  - Systems: `MoveSystem`, `SkillCooldownSystem`, `DamageQueueSystem`, `DeathSystem`, `BuffSystem`, `StatSystem`
  - Registry: `GameplayHookRegistry`
- `Server/Private/Game/GameRoom.cpp`
  - 30Hz tick thread
  - `Phase_DrainCommands`
  - `Phase_ExecuteCommands`
  - `Phase_SimulationSystems`
  - `Phase_BroadcastSnapshot`
  구조가 이미 있다.
- 즉, 서버/공유 시뮬레이션은 "계획만 있는 상태"가 아니라 **작동하는 뼈대가 존재하는 상태**다.

#### D. 자산 파이프라인

- `Engine/Public/AssetFormat/*` + `Engine/Private/AssetFormat/*`
  - `.wmesh`, `.wskel`, `.wanim` 포맷 로더/라이터가 구현돼 있다.
- `Engine/Private/Resource/Model.cpp`
  - `.wmesh` 존재 시 fast-path
  - `.wmesh + .wskel` 존재 시 skinned fast-path
  - 실패 시 Assimp fallback
- `Tools/WintersAssetConverter/WintersAssetConverter.vcxproj`
  - 별도 툴 프로젝트가 `Engine/Private` 의 asset format / animation / skeleton 소스를 직접 컴파일한다.

#### E. Go 백엔드

- `Services/cmd/*/main.go`
  - 서비스 진입점 분리
- `Services/internal/*`
  - `handler / service / repository / model`
  패턴이 정리돼 있다.
- `shop` 서비스는 DB + Kafka writer 까지 실제로 연결되는 구조다.

### 1.5 문서 현실

- `CLAUDE.md` 와 `AGENTS.md` 가 둘 다 매우 크고 일부 내용이 겹친다.
- 그래픽스 문서도 3갈래가 공존한다.
  - `GGX+A/` → **GGX + Forward+**
  - `Graphics/` → **Clustered Deferred / TAA / IBL / FromSoft lesson**
  - `00_GRAPHICS_PLAN_INDEX.md` → 상위 index
- RHI 문서는 이미 별도 master plan 이 존재한다.
  - `00_RHI_MIGRATION_MASTER.md`

---

## 2. 현재 CLAUDE.md 의 문제

1. `CLAUDE.md` 가 **운영 핸드북**, **세션 로그**, **장기 로드맵**, **코딩 컨벤션**, **아카이브** 역할을 한 파일에 동시에 지고 있다.
2. 상단의 "최신 세션" / "다음 세션" / "이전 진행" 이 길게 누적되어, 오늘 실제로 해야 할 일보다 과거 히스토리가 먼저 보인다.
3. 렌더링, AI, Physics, Backend, Security, Champion progress 가 한 파일에 과도하게 함께 있어 집중도가 떨어진다.
4. 이미 `.md/architecture`, `.md/plan`, `.md/guide` 에 존재하는 상세 내용을 `CLAUDE.md` 가 다시 반복한다.
5. `AGENTS.md` 와 중복되는 지점이 있어 장기적으로 드리프트 위험이 높다.
6. 그래픽스 방향이 문서상 여러 갈래라, `CLAUDE.md` 가 그 우선순위를 분명히 정리하지 않으면 작업자가 혼란스럽다.
7. 현재 코드 현실은 DX11/unlit 중심인데, 문서 상단은 네트워크/챔프/장기 렌더링 목표가 한꺼번에 나와 있어 "현재 구현 상태"와 "목표 상태"가 섞여 보인다.

---

## 3. 압축 원칙

1. `CLAUDE.md` 는 **백과사전**이 아니라 **작업자 운영 브리프**여야 한다.
2. 상세 설계는 외부 문서로 보내고, `CLAUDE.md` 에는 **지금 당장 실수 방지에 필요한 정보만 남긴다**.
3. "현재 구현됨" 과 "장기 목표" 를 반드시 분리한다.
4. 날짜는 상대 표현 대신 **절대 날짜**로 적는다.
5. 자주 터지는 gotcha, 빌드/경로/RHI 경계 같은 **고빈도 사고 방지 정보는 남긴다**.
6. 이미 별도 문서가 있는 대형 테이블은 링크만 남기고 본문에서는 1차 요약만 유지한다.
7. 오늘의 우선순위는 `Current Focus` 1섹션으로 맨 위에 올린다.

---

## 4. 제안하는 새 CLAUDE.md 구조

| 새 섹션 | 남길 내용 | 목표 길이 |
|---|---|---|
| 1. Current Focus | 오늘 작업 우선순위, 정확한 날짜, 즉시 진입 명령 | 40~70줄 |
| 2. Read First | 처음 읽을 문서 4~6개 | 15줄 내외 |
| 3. Repo Quick Map | Engine / Client / Server / Shared / Services / Shaders / Tools 요약 | 40줄 내외 |
| 4. Current Implemented State | DX11, ECS-hybrid, SharedSim, Asset pipeline 현황 | 60~100줄 |
| 5. Critical Rules & Gotchas | 경로, EngineSDK 동기화, `Change_Scene`, shader outdir, `.wmesh` layout 등 | 120~180줄 |
| 6. Minimal Conventions | naming / type alias / include / DLL boundary 요약 + 상세 문서 링크 | 60~90줄 |
| 7. Work Style Guardrails | 이번에 추가할 behavioral guidelines | 40~60줄 |
| 8. Active Plans | graphics / rhi / sim / champion / backend 핵심 문서 링크 | 30~50줄 |
| 9. Archive Pointer | 오래된 세션 로그 / 완료 목록 / 확장 로드맵 링크 | 10~20줄 |

**목표**:

- 현재 120KB급 문서를 **대략 35~45% 수준**으로 축소
- 하지만 build/runtime 사고를 막는 정보 밀도는 오히려 높게 유지

---

## 5. 기존 CLAUDE.md 섹션 처리표

| 기존 내용 | 처리 | 비고 |
|---|---|---|
| 최상단 최신 세션 장문 로그 | **압축** | 오늘 focus 5~10 bullet 로 축소 |
| 이전 진행 / 직전 완료 장문 누적 | **분리** | archive 또는 별도 session log 문서로 이동 |
| 기술 스택 / 게임 콘텐츠 목표 | **축약 유지** | 10줄 이내 요약 + 마스터 문서 링크 |
| 대형 Phase 로드맵 표 | **외부 링크화** | `LOL_30DAY_MASTER_PLAN`, `WINTERS_GAMEPLAY_ARCHITECTURE` 로 이동 |
| 병합 철학 / 학원 매핑 / 필터 표 | **압축 유지** | 빠른 구조 표만 남기고 상세는 외부 문서 |
| 코딩 컨벤션 200+줄 | **요약 유지** | 상세는 `WINTERS_ENGINE_CONVENTIONS.md` 로 위임 |
| Physics / Rendering / AI stage 전문 | **제거 후 링크** | CLAUDE 내부에는 현재 focus와 진입 문서만 |
| Gotchas 대형 목록 | **분리 유지** | Core gotchas 만 CLAUDE, 나머지는 확장 gotcha 문서 후보 |
| 보안/안티치트 장문 규칙 | **축약 유지** | Debug/Release 필수 수칙만 본문, 나머지 보안 계획서 링크 |
| Go 백엔드 진행 상황 상세 표 | **외부 링크화** | Services 구조 1블록 + backend index 링크 |
| 완료 챔피언 렌더링 / 오래된 산출물 | **archive 이동** | 현재 작업에 필요할 때만 참조 |

---

## 6. 새로 추가할 섹션 초안

### 6.1 Work Style Guardrails

사용자 요청 내용을 `CLAUDE.md` 에 새 섹션으로 직접 넣는 것을 권장한다.

#### 제안 문안

```md
## Work Style Guardrails

### 1. Think Before Coding
- 가정은 명시한다.
- 해석이 둘 이상이면 조용히 하나를 고르지 말고 차이를 적는다.
- 더 단순한 접근이 있으면 먼저 제안한다.
- 헷갈리면 멈추고 무엇이 모호한지 적는다.

### 2. Simplicity First
- 요청받지 않은 기능은 넣지 않는다.
- 1회용 코드에 과한 추상화는 만들지 않는다.
- 설정/유연성/확장성을 미리 과투자하지 않는다.
- 200줄이 50줄로 끝날 수 있으면 다시 줄인다.

### 3. Surgical Changes
- 필요한 줄만 건드린다.
- 인접 코드의 스타일/포맷/리팩터는 요청 없으면 하지 않는다.
- 내 변경으로 생긴 unused 만 치운다.
- 기존 dead code 는 메모만 남기고 함부로 지우지 않는다.

### 4. Goal-Driven Execution
- 작업을 검증 가능한 목표로 바꾼다.
- 버그 수정은 재현/검증 기준부터 적는다.
- 다단계 작업은 각 단계의 합격 조건을 적는다.
- "작동하게 만들기" 같은 모호한 종료 기준은 피한다.
```

### 6.2 Current Focus 상단 문안 교체

기존 "최신 세션 누적 로그" 대신 다음 스타일을 권장한다.

- `2026-05-01 Current Focus`
- `A. CLAUDE.md 압축 및 정렬`
- `B. DX11 기준 PBR + Forward+ + SSAO 착수 준비`
- `C. DX12/Vulkan 은 RH-0~RH-2 문서 정렬 후 별도 트랙으로 진행`

---

## 7. 2026-05-01 오늘의 계획 정리

### Track A. 문서 정리

1. `CLAUDE.md` 를 운영 브리프 구조로 압축
2. 최신 세션 장문 로그를 위에서 걷어내고 `Current Focus` 로 교체
3. behavioral guidelines 추가
4. graphics / rhi / sim / backend 문서 링크를 "실제 진입 문서" 기준으로 재정렬

### Track B. 렌더링 주력 방향

**권장 1차 주력**: `DX11 + PBR + GGX + Forward+ + SSAO`

이유:

- 현재 런타임은 `Mesh3D.hlsl` / `Skinned3D.hlsl` 기반의 **forward-ish unlit** 구조다.
- 사용자 요청도 `PBR`, `Forward+`, `GGX`, `SSAO` 를 먼저 지목했다.
- 따라서 가장 작은 변화로 가장 큰 시각 향상을 얻는 1차 트랙은
  - Depth pre-pass
  - BRDF math
  - PBR material/light data
  - Forward+ light culling
  - SSAO
  순서가 맞다.

### Track C. Clustered Deferred / TAA / IBL 계열 문서 처리

- 현재 `.md/plan/graphics/Graphics/` 문서군은 더 큰 렌더링 체계다.
- 이 문서군은 버리지 말고 **2차 확장 트랙**으로 둔다.
- 즉, 제안 우선순위는 다음과 같다.

1. `GGX+A/` 를 **즉시 실행용 주력 문서**
2. `Graphics/` 문서를 **확장형 고급 렌더링 로드맵**
3. `00_GRAPHICS_PLAN_INDEX.md` 를 상위 인덱스

### Track D. DX12 / Vulkan 이식

**권장 분리 원칙**:

- 오늘 당장 PBR 코딩과 DX12/Vulkan 이식을 같은 작업 묶음으로 착수하지 않는다.
- 먼저 `RHI Multi-Backend Migration` 문서 기준으로
  - RH-0 inventory
  - RH-1 interface
  - RH-2 command list / public DX11 제거
  전제를 다진다.

이유:

- 현재 `CGameInstance -> CDX11Device*` leak 가 존재한다.
- `Scene_InGame.cpp` 도 `Get_RHIDevice()->GetContext()` 를 직접 사용한다.
- 즉, **렌더링 품질 개선 트랙**과 **백엔드 추상화 트랙**을 분리하지 않으면 변경 범위가 너무 커진다.

### 오늘의 추천 실행 순서

1. `CLAUDE.md` 압축 구조 반영
2. `graphics` 문서군의 우선순위 정리
3. DX11 기준 PBR 0단계 착수
   - Depth pre-pass
   - BRDF common
   - `Mesh3D_PBR`, `Skinned3D_PBR`
4. SSAO 도입 전 normal/depth 입력 안정화
5. 별도 브랜치/문서에서 RH-0~RH-2 준비

---

## 8. 내가 권장하는 실제 반영 포인트

1. `CLAUDE.md` 맨 위를 **Current Focus** 로 교체한다.
2. `CLAUDE.md` 안의 장기 세션 로그는 archive 문서로 뺀다.
3. 코딩 컨벤션은 1-screen summary 만 남기고 상세는 `WINTERS_ENGINE_CONVENTIONS.md` 링크로 보낸다.
4. graphics 방향은
   - **즉시 실행**: `GGX+A`
   - **확장형**: `Graphics/`
   로 명시한다.
5. `DX12/Vulkan` 은 `PBR immediate task` 가 아니라 `RHI migration track` 으로 분리해 적는다.
6. 이번에 받은 behavioral guidelines 는 `CLAUDE.md` 상단부에 새 섹션으로 넣는다.

---

## 9. 검토 후 다음 단계

이 문서 검토 후 다음 순서로 진행하는 것을 제안한다.

1. `CLAUDE.md` 압축 수정 실제 반영
2. 필요 시 세션 로그/archive 문서 분리
3. graphics 문서 링크 정렬
4. DX11 PBR 0단계 구현 계획서 또는 실제 코드 착수

**핵심 제안 한 줄**:

> `CLAUDE.md` 는 앞으로 "지금 작업자가 바로 움직이기 위한 짧은 운영 브리프" 로 쓰고,  
> 렌더링은 **DX11 기준 PBR + GGX + Forward+ + SSAO** 를 1차 주력으로,  
> **DX12/Vulkan** 은 **RHI migration 별도 트랙** 으로 분리하는 편이 가장 안전하고 생산적이다.
