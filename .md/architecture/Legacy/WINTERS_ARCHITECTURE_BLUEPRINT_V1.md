# Winters Engine Architecture Blueprint v1 — 100 모듈 청사진

> **작성일**: 2026-05-03
> **목적**: 현재 13 모듈 → 미래 **100 모듈** Winters Engine 청사진. UE5 (Runtime 188 + Plugin 777) 의 패턴 흡수, Unity / O3DE / Source 2 의 5 공통 패턴 채택. AI-Readiness Rubric 100점 + UE5 급 규모 수용 + LoL 모작 + 엘든링 모작 + 자체 엔진 디자인 동시 만족.
> **참조 input**:
> - [`UE5_REFERENCE/MODULE_INVENTORY.md`](.md/architecture/UE5_REFERENCE/MODULE_INVENTORY.md) — UE5 188 모듈 카테고리
> - [`UE5_REFERENCE/CORE_DEPENDENCY_GRAPH.md`](.md/architecture/UE5_REFERENCE/CORE_DEPENDENCY_GRAPH.md) — 핵심 12 모듈 의존성
> - [`UE5_REFERENCE/PLUGIN_INVENTORY.md`](.md/architecture/UE5_REFERENCE/PLUGIN_INVENTORY.md) — 777 Plugin 분석
> - [`ENGINE_COMPARISON.md`](.md/architecture/ENGINE_COMPARISON.md) — 6 엔진 패턴 비교
> - [`CODEBASE_COMPASS_SYSTEM.md`](.md/architecture/CODEBASE_COMPASS_SYSTEM.md) — Compass 시스템
> - [`AI_READINESS_RUBRIC.md`](.md/architecture/AI_READINESS_RUBRIC.md) — Rubric

---

## §0. 한 줄 요약

**현 13 모듈 (Engine + Client + Server + Shared + Tools + Services) → 미래 100 모듈 (Engine Core 30 + Engine Plugin 35 + Game Plugin 25 + Tool Plugin 10) 청사진. UE5 의 의존성 3 분류 (Public/Private/Forward-Decl) + Unity asmdef 강제 + O3DE Gem 격리 + Source 2 Hot Reload + Godot build flag 5 패턴 채택. Phase D (Physics) → E (Rendering) → F (AI) → G (Plugin 시스템) 점진 진화. 본 청사진이 Compass System 의 _MODULE.md 박제 + AI-Readiness Rubric 100 도달의 base.**

---

## §1. 비전 — 무엇을 만들고 있는가

### 1-1. 3개 동시 목표

| # | 목표 | 의미 |
|---|---|---|
| 1 | **LoL 모작 (WintersLOL)** | 실시간 5v5 MOBA — 30일 풀스택 (Phase 7 까지 박제됨) |
| 2 | **엘든링 모작 (WintersElden)** | 액션 RPG — 오픈월드 + 보스 + 스태미나 (Phase TBD) |
| 3 | **자체 엔진 (Winters Engine)** | UE5 / Unity 와 비견 가능한 일반 엔진 — 이게 진짜 product |

3개 모두 **하나의 WintersEngine.dll** 로 구동. UE5 의 GameplayFramework 가 Lyra Sample / Fortnite / Valorant 모두 구동하는 것과 동일.

### 1-2. 1000 파일 / 100 모듈 도달 시점

| 시점 | 파일 수 (예상) | 모듈 수 (예상) | 상태 |
|---|---|---|---|
| 현재 (2026-05-03) | ~300 | 13 | LoL Phase B-10 챔프 7체 |
| Phase D 완료 (2026-08?) | ~600 | 30 | 자체 Physics 도입 |
| Phase E 완료 (2026-12?) | ~1200 | 50 | 자체 Renderer (PBR/GI/PathTracing) |
| Phase F 완료 (2027-04?) | ~1800 | 70 | 자체 AI 봇 (HFSM~RL) |
| Phase G 완료 (2027-08?) | ~2500 | 100 | Plugin 시스템 + 100 챔프 + 엘든링 진입 |

**1000 파일 도달 = Phase E 시점**. 그 전에 Compass System / AI Rubric 100 / Architecture Blueprint v1 박제 필수.

### 1-3. UE5 와의 의도적 차이

| 항목 | UE5 | Winters |
|---|---|---|
| 오브젝트 모델 | Actor / Component (OOP) | **ECS** (SoA, 데이터 지향) |
| 리플렉션 | UObject (커스텀 RTTI + GC) | **C++20 concepts + 일부 RTTI** (가벼움) |
| 메모리 | UE 스마트 포인터 (TSharedPtr) | **std::unique_ptr / shared_ptr** |
| 셰이더 | HLSL → USF (자체 전처리) | **raw HLSL** |
| Lua/Python | 외부 Plugin | **Lua 5.4 native** |
| Editor | UE Editor (UMG) | **ImGui** (게임+에디터 통합) |
| 빌드 | UBT + UAT (자체) | **MSBuild + vcxproj** (현재) → 향후 CMake 검토 |
| Plugin | uplugin | **wplugin (Phase G)** |

→ Winters 는 UE5 의 모듈 시스템 패턴은 차용, 단 구현 방식은 자체 (가벼움 / 모던 C++).

---

## §2. 현 13 모듈 진단

### 2-1. Engine 13 (CLAUDE.md "Engine 필터" 표)

| # | 모듈 | 현 위치 | 상태 | UE5 매칭 |
|---|---|---|---|---|
| 00 | Manager (DX11/RHI) | `Engine/Public/RHI/` | ✓ 작동 | RHI |
| 01 | Core | `Engine/Public/Core/` | ✓ 작동 | Core |
| 02 | Structure (Framework) | `Engine/Public/Framework/` | ✓ 작동 | Engine (부분) |
| 03 | Renderer | `Engine/Public/Renderer/` | ✓ 작동 | Renderer (부분) |
| 04 | Editor (ImGui) | `Engine/Public/Editor/` | ✓ 작동 | (UE5 는 Editor/ 별도) |
| 05 | ECS | `Engine/Public/ECS/` | ✓ 작동 | (UE5 미사용) |
| 06 | Resource | `Engine/Public/Resource/` | ✓ 작동 | Asset |
| 07 | Physics | `Engine/Public/Physics/` | 🔄 Phase D | Physics |
| 08 | Audio | `Engine/Public/Sound/` | ✓ 작동 (FMOD) | Audio |
| 09 | Network | `Engine/Public/Network/` | 🔄 Phase 4 | Network |
| 10 | JobSystem | `Engine/Public/Core/JobSystem/` | ✓ Phase 5-A | (UE5 는 Core 안) |
| 11 | Scene | `Engine/Public/Scene/` | ✓ 작동 | (UE5 는 World/Level) |
| 12 | Collision | (Physics 와 통합 예정) | 🔄 | Physics 일부 |
| 13 | AI | `Engine/Public/AI/` | 🔄 Phase F | AI |

### 2-2. 진단 — 5 미흡 영역

| 영역 | 현 상태 | 미흡 사유 |
|---|---|---|
| **CoreUObject 같은 Reflection layer** | 미존재 | Lua 바인딩 / 에디터 / 직렬화 base 없음 — Phase 4 추가 |
| **RenderCore (RHI 와 Renderer 사이)** | 미존재 | 셰이더 매니저 / 버퍼 캐시 분산 — 통합 필요 |
| **Streaming / WorldPartition** | 미존재 | 엘든링 오픈월드 진입 시 필수 |
| **Movie/Sequencer** | 미존재 | 컷씬 / Replay 미지원 |
| **Plugin 시스템** | 미존재 | 모든 챔프가 ChampionTable hardcoded — Phase G 도입 |

---

## §3. 100 모듈 청사진 (카테고리별)

### 3-1. Engine Core — 30 모듈 (Engine.dll 의 base)

```
Engine/
├── Core/                       (8 모듈)
│   ├── Foundation              ← Allocator / Span / Containers / Atomics
│   ├── Time                    ← Timer / Clock
│   ├── Input                   ← Keyboard / Mouse / Gamepad
│   ├── Threading               ← Thread / Mutex / SpinLock
│   ├── JobSystem               ← Phase 5-A + 5-B (Fiber)
│   ├── Profiler                ← CPU / GPU / Memory / Network
│   ├── Logging                 ← Sink / Format / Filter
│   └── Reflection              ← (★ 신규) RTTI + serialization base
│
├── RHI/                        (5 모듈)
│   ├── DX11                    ← 현재
│   ├── DX12                    ← 향후
│   ├── Vulkan                  ← 향후 (cross-platform)
│   ├── RHICore                 ← 추상 인터페이스
│   └── ShaderCompiler          ← HLSL → bytecode + reflection
│
├── RenderCore/                 (★ 신규 layer, 5 모듈)
│   ├── BufferCache             ← VB/IB/CB 풀
│   ├── ShaderManager           ← 셰이더 변종 + hot reload
│   ├── TextureStreaming        ← 비동기 텍스처 로드
│   ├── MaterialSystem          ← Material → Shader 매핑
│   └── DebugDraw               ← 콜라이더/본/Frustum 시각화
│
├── ECS/                        (4 모듈)
│   ├── EntityCore              ← Entity ID / Generation
│   ├── ComponentStore          ← SoA + archetype
│   ├── SystemScheduler         ← Phase / SystemAccess / 병렬
│   └── EventBus                ← inter-system 메시지
│
├── Resource/                   (5 모듈)
│   ├── ResourceCache           ← 경로 기반 캐싱
│   ├── ModelLoader             ← .wmesh + Assimp fallback
│   ├── TextureLoader           ← .wtex + WIC fallback
│   ├── AnimLoader              ← .wanim + .wskel
│   └── SoundLoader             ← FMOD wrapper
│
└── Framework/                  (3 모듈)
    ├── Application             ← CEngineApp + Window / Loop
    ├── Scene                   ← IScene + Scene_Manager
    └── GameInstance            ← Tier-1 forwarding hub
```

### 3-2. Engine Plugin — 35 모듈 (선택적 활성)

```
Plugins/Engine/
├── Renderer-Forward+/          (Phase E, 8 모듈)
│   ├── BRDF                    ← Cook-Torrance / GGX
│   ├── PBR                     ← Metallic/Roughness + IBL
│   ├── ShadowMap               ← CSM / VSM
│   ├── GI-VXGI                 ← Voxel Cone Tracing
│   ├── GI-DDGI                 ← Probe 기반
│   ├── PostFX                  ← TAA / Bloom / DoF
│   ├── PathTracer-Reference    ← Compute Shader 기반
│   └── FFT-Ocean               ← Tessendorf
│
├── Physics/                    (Phase D, 7 모듈)
│   ├── BoundingVolume          ← AABB / OBB / Sphere / Capsule
│   ├── Collision-NarrowPhase   ← SAT + GJK + EPA
│   ├── Collision-BroadPhase    ← BVH + SAP
│   ├── RigidBody               ← Semi-implicit Euler
│   ├── ConstraintSolver        ← Sequential Impulse (Catto)
│   ├── PBD-Cloth-Rope-Soft     ← Müller XPBD
│   └── CCD                     ← Swept Capsule
│
├── AI/                         (Phase F, 10 모듈)
│   ├── HFSM                    ← Hierarchical FSM
│   ├── BehaviorTree            ← Selector/Sequence/Decorator
│   ├── GOAP                    ← A* planner
│   ├── Utility                 ← Score + Response Curves
│   ├── Blackboard              ← 팀 공유 메모리
│   ├── InfluenceMap            ← Team/Threat/Opportunity layers
│   ├── Pathfinding             ← A* + JPS (Navigation 공유)
│   ├── MCTS                    ← 교전 시뮬
│   ├── Imitation               ← Behavior Cloning (ONNX)
│   └── RL                      ← PPO 추론 (선택, ONNX)
│
├── Animation/                  (3 모듈)
│   ├── Animator                ← 현재 + state machine
│   ├── IK                      ← FABRIK + 2-bone IK
│   └── BlendSpace              ← 2D/3D 블렌딩
│
├── VFX/                        (3 모듈)
│   ├── Particle                ← FxBillboard + FxMesh (현재)
│   ├── Niagara-like            ← (★ 신규) GPU compute particle
│   └── PostProcess             ← Bloom 등 (Renderer 와 협력)
│
├── Networking/                 (4 모듈)
│   ├── Transport-TCP           ← Phase 4a
│   ├── Transport-UDP-KCP       ← Phase 4b
│   ├── Reliability             ← Ack / Resend
│   └── Replication             ← Snapshot / Delta / Prediction
│
└── Sequencer/                  (★ 신규 영역, 4 모듈)
    ├── MovieScene              ← Track / Section
    ├── Cinematic               ← 컷씬 재생
    ├── Replay                  ← 게임 리플레이
    └── Recording               ← 키프레임 캡처
```

### 3-3. Game Plugin — 25 모듈

```
Plugins/Game/
├── WintersLOL/                 (10 모듈)
│   ├── ChampionRegistry        ← 100 챔프 metadata
│   ├── SkillSystem             ← SkillDef + dispatch
│   ├── BuffSystem              ← BuffComponent
│   ├── ItemSystem              ← LoL 아이템
│   ├── MapManager              ← 소환사 협곡 + 데이터
│   ├── MinionAI                ← lane 미니언
│   ├── TurretAI                ← 포탑
│   ├── JungleAI                ← 정글몹
│   ├── FOW                     ← Fog of War
│   └── MatchFlow               ← BanPick → Loading → InGame → PostGame
│
├── WintersElden/               (8 모듈, Phase TBD)
│   ├── BossRegistry            ← 보스 metadata
│   ├── StaminaSystem           ← 회피/패링/공격
│   ├── OpenWorld               ← Streaming + WorldPartition
│   ├── LootSystem              ← 드랍 / 인벤토리
│   ├── CombatSystem            ← 액션 RPG 전투
│   ├── ParrySystem             ← 패링 윈도우
│   ├── WeaponMoveset           ← 무기별 모션
│   └── MagicSystem             ← 마법 / 영창
│
└── Champions/                  (5 모듈, 챔프 그룹)
    ├── Champions-Core          ← Champion 인터페이스 + factory
    ├── Champions-Bruiser       ← 가렌/제드/사일러스
    ├── Champions-Marksman      ← 칼리스타/이즈리얼
    ├── Champions-Assassin      ← 야스오/요네
    └── Champions-Support       ← (향후)
```

### 3-4. Tool Plugin — 10 모듈

```
Plugins/Tools/
├── AssetConverter              ← 현재 + .wmesh/.wanim/.wskel
├── CompassValidator            ← _MODULE.md / link / cycle
├── AIReadinessScorer           ← Rubric 측정자
├── ShaderHotReload             ← .hlsl watch + 재컴파일
├── LuaReload                   ← .lua hot reload (Phase 7)
├── ProfileViewer               ← Profiler 결과 시각화
├── ECSInspector                ← ImGui 패널
├── NetworkDebugger             ← packet sniffer / latency 측정
├── BuildSystem                 ← UBT-like 자체 빌드 도구 (장기)
└── EditorMode                  ← 연습모드 통합 에디터
```

### 3-5. 합계 — 100 모듈

| 카테고리 | 모듈 수 |
|---|---|
| Engine Core | 30 |
| Engine Plugin | 35 |
| Game Plugin | 25 |
| Tool Plugin | 10 |
| **합계** | **100** |

UE5 (Runtime 188 + Plugin 777 = 매우 거대) 보다 작지만, **상품 시장 경쟁 가능** (Unity 가 비슷한 규모 — 핵심 Engine 50 + Package 50+).

---

## §4. 의존성 계약 — UE5 Build.cs 의 3 분류

### 4-1. 3 분류 정의 (★ Winters 채택)

각 `_MODULE.md` 의 `## Dependencies` 섹션:

```markdown
## Dependencies

### Public (헤더 노출)
이 모듈을 의존하는 모든 모듈도 아래 dep 의 헤더 직접 사용 가능.
- `Core` — Foundation / Time
- `RHI` — IBuffer 인터페이스 (헤더로 노출)

### Private (구현만)
.cpp 안에서만 사용. 의존자는 dep 의 헤더 못 봄.
- `Profiler` — WINTERS_PROFILE_SCOPE 매크로
- `Resource` — TextureLoader 호출

### Forward-Decl (헤더만, link X)
헤더에서 forward declare 만. 실제 link 안 함 (포인터/참조만).
- `Editor` — class CImGuiLayer; (포인터 멤버)
```

### 4-2. 강제 검증 (Compass Validator Phase 2)

```python
# Validator 가 검증
def check_module_dependencies(module_dir):
    manifest = parse_module_md(module_dir / "_MODULE.md")
    actual_includes = scan_cpp_includes(module_dir)

    # Public dep 의 헤더가 자기 모듈 헤더에 include 됐어야
    # Private dep 은 자기 모듈 .cpp 에만
    # Forward-decl 은 헤더에 'class X;' 만, .cpp include X
    ...
```

위반 시 PR 차단.

### 4-3. Cycle 차단

`MODULE_GRAPH.md` 의 의존성 그래프를 DAG 검증. cycle 발견 시 CI fail.

UE5 도 cycle 0 강제. Winters 도 동일.

---

## §5. DLL Boundary 철학

### 5-1. 단일 WintersEngine.dll

현재 + 미래 모두 **WintersEngine.dll 단일** (UE5 가 모듈 별 DLL 인 것과 다름).

**이유**:
- 빌드 시간 ↓ (단일 link)
- export 표면적 ↓ (보안)
- Cross-module inline 가능 (성능)

**단점**: 부분 hot reload 어려움 → Plugin 시스템 (§6) 으로 회피.

### 5-2. Export 정책 (CLAUDE.md §보안 §3 박제)

- **`WINTERS_ENGINE` (= dllexport) 마크**: 최소 세트만
- **CGameInstance 단독 export**: 내부 매니저는 forwarding 게터로만 접근
- **FromSoft 엘든링 2022 CVE 사례**: PlayerManager 가 Client EXE 노출 → RCE. Winters 는 모든 매니저 Engine.dll 안에 격리

### 5-3. Plugin 시스템 (Phase G) 의 DLL 분리

Plugin 은 **별도 .dll** (lazy load):
```
WintersEngine.dll              ← Engine Core 30 모듈 (필수)
Plugins/Engine/Renderer-Forward+.dll
Plugins/Engine/Physics.dll
Plugins/Game/WintersLOL.dll
...
```

Game 별로 필요한 Plugin 만 로드. Cyberpunk 2077 처럼 modder 가 자체 .dll 추가 가능.

---

## §6. Plugin 시스템 청사진 (Phase G — Beyond 100)

### 6-1. `*.wplugin` JSON 형식 (UE5 .uplugin 차용)

```json
{
  "FileVersion": 1,
  "Version": "1.0",
  "FriendlyName": "Renderer Forward+",
  "Description": "Forward+ rendering with PBR/IBL/GI",
  "Category": "Rendering",
  "CreatedBy": "Winters Team",
  "EnabledByDefault": true,
  "Modules": [
    {"Name": "BRDF",       "Type": "Runtime", "LoadingPhase": "PreDefault"},
    {"Name": "PBR",        "Type": "Runtime", "LoadingPhase": "Default"},
    {"Name": "PostFX",     "Type": "Runtime", "LoadingPhase": "PostDefault"}
  ],
  "Plugins": [
    {"Name": "RHI",         "Enabled": true},
    {"Name": "RenderCore",  "Enabled": true}
  ]
}
```

### 6-2. LoadingPhase

UE5 패턴 그대로:
- `EarliestPossible` — 매우 빠름
- `PreDefault` — Engine init 전
- `Default` — 일반 Plugin
- `PostDefault` — Engine init 후
- `PreLoadingScreen` — 로딩 직전

### 6-3. EnabledByDefault

게임별 .winters 프로젝트 파일에서 override:
```json
{
  "Plugins": [
    {"Name": "Renderer-Forward+", "Enabled": true},
    {"Name": "Physics",            "Enabled": true},
    {"Name": "AI",                 "Enabled": false},  // 봇전 안 만들 게임
    {"Name": "WintersLOL",         "Enabled": true},
    {"Name": "WintersElden",       "Enabled": false}
  ]
}
```

### 6-4. Mod 친화 — 외부 Plugin 추가

`Plugins/Mods/<ModName>/<ModName>.wplugin` 자동 스캔. 사용자가 Plugin 추가 만으로 게임 확장.

---

## §7. 점진 진화 경로 — Phase 별 모듈 추가 timeline

### 7-1. 현 → Phase D (Physics, ~3개월)

| 추가 모듈 | 위치 |
|---|---|
| BoundingVolume / Collision-Narrow / Broad / RigidBody / ConstraintSolver / PBD / CCD | `Engine/Public/Physics/` (현 빈 폴더 → 7 모듈) |
| 합계 13 → 20 |

### 7-2. → Phase E (Rendering, ~4개월)

| 추가 모듈 | 위치 |
|---|---|
| RenderCore-5 (BufferCache / ShaderManager / Streaming / Material / DebugDraw) | `Engine/Public/RenderCore/` (신규 layer) |
| BRDF / PBR / ShadowMap / GI-VXGI / GI-DDGI / PostFX / PathTracer / FFT-Ocean | `Engine/Public/Renderer/Advanced/` (8 모듈) |
| 합계 20 → 33 |

### 7-3. → Phase F (AI, ~4개월)

| 추가 모듈 | 위치 |
|---|---|
| HFSM / BT / GOAP / Utility / Blackboard / InfluenceMap / Pathfinding / MCTS / Imitation / RL | `Engine/Public/AI/` (10 모듈) |
| Reflection (★ Phase 4 신규) | `Engine/Public/Core/Reflection/` |
| 합계 33 → 44 |

### 7-4. → Phase G (Plugin 시스템, ~3개월)

| 추가 모듈 | 위치 |
|---|---|
| `*.wplugin` 로더 + LoadingPhase + EnabledByDefault 처리 | `Engine/Public/Core/PluginCore/` |
| 기존 모듈 일부를 Plugin 으로 분리 (Audio / Network / VFX 등) | `Plugins/Engine/` |
| Game Plugin 분리 (WintersLOL / Champions/*) | `Plugins/Game/` |
| Sequencer / Movie / Streaming (★ 신규 영역) | `Plugins/Engine/Sequencer/` 등 |
| 합계 44 → 70 |

### 7-5. → 100 모듈 (~6개월 추가)

| 추가 모듈 | 위치 |
|---|---|
| WintersElden 8 모듈 | `Plugins/Game/WintersElden/` |
| Tool Plugin 10 모듈 | `Plugins/Tools/` |
| 100 챔프 → Champions 5 그룹 모듈 | `Plugins/Game/Champions-*/` |
| 합계 70 → 100 |

**총 timeline**: 현 → 100 모듈 = **약 20개월** (집중 작업 시).

---

## §8. `_MODULE.md` 템플릿 갱신 (UE5 3 분류 반영)

CODEBASE_COMPASS_SYSTEM.md §3-2 의 템플릿을 다음으로 갱신:

```markdown
# {ModuleName} Module

## 책임 (Responsibility)
{1-2 단락}

## 진입점 (Entry Points)
- `{ClassName}::{Method}` at `path/to/file.h:line` — {언제 사용}
- ... (3-7개)

## 의존성 (Dependencies) — ★ UE5 Build.cs 3 분류

### Public (헤더 노출)
- `{Module}` — {왜 헤더 노출 필요}

### Private (구현만, .cpp)
- `{Module}` — {왜}

### Forward-Decl Only (헤더에 class X; 만)
- `{Module}` — {포인터/참조 멤버}

## 의존받음 (Depended By)
- `{Module}` — {용도}

## Common Tasks (AI 매핑)
- "{Task}" → {진입 위치}

## 함정 (Gotchas)
- {함정 1} — {증상 / 회피}

## 외부 노출 API (DLL boundary)
- `WINTERS_ENGINE` 마크: {목록}
- 비노출: {목록}

## Plugin 메타 (Phase G 이후)
- 소속 Plugin: {Plugin name 또는 "Engine Core"}
- LoadingPhase: {EarliestPossible / PreDefault / Default / PostDefault}
- EnabledByDefault: {true / false}

## 핵심 파일 (Top 5)
1. `{File}` — {역할}

## 관련 계획서 / 문서
- `{path}` — {요약}
```

★ 변경점:
- `## 의존성` 을 3 sub 로 분리 (UE5 패턴)
- `## Plugin 메타` 신설 (Phase G 이후)
- 진입점에 줄번호 포함

---

## §9. AI-Readiness Rubric 100점과의 매핑

본 청사진을 적용하면 Rubric 의 다음 sub 가 자동 향상:

| Sub | 현재 | 청사진 적용 후 |
|---|---|---|
| A1 모듈 manifest 커버 | 0/5 | 5/5 (100 모듈 모두 _MODULE.md) |
| A2 진입점 정확성 | N/A | 5/5 (Validator 강제) |
| B1 간결성 | 0/4 | 4/4 (CLAUDE.md 분할 — 모듈별 _MODULE.md 로 분산) |
| B5 크로스참조 | 2/4 | 4/4 (Public/Private 명시 + Plugin 메타) |
| C1 Gotchas | 8/8 | 8/8 유지 |
| C2 ADR | 2/6 | 6/6 (본 청사진 자체가 ADR + decisions/ 분리) |
| D1 Module Graph | 0/5 | 5/5 (의존성 DAG) |
| D2 Mermaid | 0/5 | 5/5 (그래프 시각화) |
| D3 Code Index | 2/5 | 5/5 (100 모듈 매핑) |
| E1 깨진 link | ? | 5/5 (Validator 자동 검증) |
| E3 CI Hook | 0/5 | 5/5 (GitHub Actions) |
| F1 Hook 등록 | 0/5 | 5/5 (Refresh + Score + Validator hook) |
| G1 evals | 0/3 | 3/3 (5+ 회귀 테스트) |

→ **본 청사진 적용 = Rubric ~95-100 자동 도달** (점수 chasing 없이).

---

## §10. 한 줄 요약

**현 13 → 미래 100 모듈 (Engine Core 30 + Engine Plugin 35 + Game Plugin 25 + Tool Plugin 10) 청사진. UE5 의 의존성 3 분류 (Public/Private/Forward-Decl) + Unity asmdef 강제 + O3DE Gem + Source 2 Hot Reload + Godot build flag 5 패턴 채택. Phase D Physics (3개월, 13→20) → E Rendering (4개월, 20→33) → F AI (4개월, 33→44) → G Plugin 시스템 (3개월, 44→70) → 100 (6개월 추가) = 총 20개월. 본 청사진 적용 = AI-Readiness Rubric 95-100 자동 도달 + UE5 급 확장성 + 단일 WintersEngine.dll 단순함 유지 + LoL/엘든링/자체 엔진 3 목표 동시 만족. _MODULE.md 템플릿 갱신 (UE5 3 분류 + Plugin 메타) → Compass System Phase A 진입 시 본 청사진 따라 100 모듈 박제.**
