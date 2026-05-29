# Winters Gameplay 아키텍처 — 한 눈에 보는 전체 구조

**목적**: "지금 코드가 어디 있고, 왜 그렇게 쪼개졌고, 앞으로 어디로 가는지" 를 한 문서로 이해.
**대상**: 엔진 전체 그림을 처음 잡는 입장.
**짝 문서**: `WINTERS_ENGINE_ARCHITECTURE_FINAL.md` (7-레이어 원론), `WINTERS_ENGINE_CONVENTIONS.md` (네이밍·경계 규칙).

---

## 1. 바이너리 경계 — 무엇이 어디에 사는가

```
   ┌──────────────────────────────┐     ┌────────────────────────────┐
   │   WintersEngine.dll          │     │   WintersLOL.exe (Client)  │
   │   (범용 엔진 · 게임 모름)     │     │   (게임 로직 · 컨텐츠)      │
   ├──────────────────────────────┤     ├────────────────────────────┤
   │  RHI (DX11)                  │     │  main.cpp                  │
   │  ECS (World·Entity·System)   │     │  CGameApp                  │
   │  Resource (Model·Anim·Tex)   │     │  Scene_InGame               │
   │  Renderer (Camera·Model)     │ ◄───│  Champions/                │
   │  Core (Timer·Input·Transform)│     │  GameObject/SkillDef/…     │
   │  Framework (EngineApp)       │     │  DynamicCamera             │
   │  Editor (ImGui)              │     │  (챔프/맵/UI 등 컨텐츠)     │
   │  JobSystem                   │     └────────────────────────────┘
   │  CGameInstance (single API)  │               │
   └──────────────────────────────┘               │ WinHTTP / UDP
             ▲                                    ▼
             │ 빌드 시 .lib 링크           ┌──────────────────┐
             │ 런타임 DLL 로드              │  Services/       │
             │                              │  (Go 백엔드)     │
   ┌─────────┴─────────┐                   │  Auth/Match/Shop │
   │  EngineSDK/       │                   └──────────────────┘
   │  ├── inc/ (*.h)   │ ← Engine public 헤더 복사본
   │  └── lib/         │   (UpdateLib.bat 이 자동 배포)
   └───────────────────┘
```

**핵심 규칙** (`WINTERS_ENGINE_CONVENTIONS.md` §3):
- Engine 은 Client 를 **모른다** (챔프·MOBA 개념 없음. 뼈대만)
- Client 는 Engine 을 **`CGameInstance` 단일 창구**로만 쓴다 (핫패스 제외)
- Server 도 같은 Engine.dll 을 쓸 예정 (물리·ECS·시뮬레이션 공유)

---

## 2. ECS 가 왜 존재하는가 — 5 줄 요약

1. 게임 세계의 모든 "것"(챔프·미니언·투사체·타워·이펙트 더미)을 **EntityID 한 종류**로 통일
2. "것" 이 가진 속성(Transform / Health / Team / Skill 상태)을 **Component(=데이터 구조체)** 로 분리 저장
3. 로직은 "이 컴포넌트를 가진 모든 엔티티에 이걸 해라" 형태의 **System(=함수)** 로 작성
4. 덕분에 **상속 없음, 분기 없음, 레이아웃 캐시 친화적, 네트워크 직렬화가 컴포넌트 단위로 기계적**
5. 150 챔프든 5000 유닛이든 **같은 코드** — 추가 유닛 타입은 "컴포넌트 조합"일 뿐

```
   Entity 42  =  [Transform] + [MeshRef] + [Animator] + [Hitbox] + [Team] + [SkillState] + [YasuoState]
                        ▲                                                              ▲
                        │                                                              │
                     공통                                                           야스오만
```

---

## 3. 현재(2026-04-18) 데이터 흐름 — 한 프레임

```
   ┌─ CGameApp::Update(dt) ──────────────────────────────────────────────────┐
   │                                                                         │
   │   Scene_InGame::OnUpdate(dt)                                             │
   │     │                                                                   │
   │     ├─ SyncECSTransformsFromLegacy()   ← 레거시 CTransform → ECS 복사    │
   │     │                                     (세 겹 동기화 1단)             │
   │     ├─ UpdateTargeting()               ← 레이캐스트 (Sylas 하드코딩)      │
   │     ├─ UpdateCombatInput()             ← 키보드/마우스 에지 검출         │
   │     │    └─ DispatchSkillInput(slot)   ← ChampionDef + SkillDef 룩업     │
   │     │         ├─ BuildCastCommand()    ← eTargetMode 별 payload          │
   │     │         └─ ApplyLocalPrediction  ← 회전 + 애니재생 + 쿨시작        │
   │     │              └─ RotatePlayerToward                                 │
   │     ├─ [타이머 감소] m_fLastActionTimer, YasuoState, SkillState          │
   │     ├─ [이동 적분] 레거시 CTransform 에 SetPosition / SetRotation        │
   │     └─ [애니 복귀] 락 풀리면 idle/run 로 PlayAnimationByName             │
   │                                                                         │
   │   Scene_InGame::OnRender()                                               │
   │     └─ m_Irelia.Render(), m_Yasuo.Render(), ...  ← 수동 나열            │
   └─────────────────────────────────────────────────────────────────────────┘
```

**이 흐름의 문제**: 레이어 경계가 함수 호출 순서에 암묵적으로만 존재. 네트워크 끼워넣을 구멍이 없음.

---

## 4. 타겟 아키텍처 — 레이어가 명시적으로 분리된 모습

```
   World (모든 엔티티·컴포넌트의 단일 원천)
    ▲  ▲  ▲  ▲  ▲  ▲  ▲  ▲
    │  │  │  │  │  │  │  │
    │  │  │  │  │  │  │  └────────── RenderSystem          (ForEach<Transform,MeshRef>)
    │  │  │  │  │  │  └───────────── AnimationSystem        (AnimIntent → Animator tick)
    │  │  │  │  │  └──────────────── MovementSystem         (Velocity → Transform)
    │  │  │  │  └─────────────────── BuffSystem             (BuffInstance 타이머)
    │  │  │  └────────────────────── SkillSimulationSystem  (CastCmd → hookId 디스패치 → 히트·버프)
    │  │  └───────────────────────── SkillDispatchSystem    (InputIntent → CastCommand)
    │  └──────────────────────────── TargetingSystem        (Hitbox + SpatialGrid → hoveredEntity)
    └─────────────────────────────── InputSystem            (로컬 키/마우스 / 봇 / 네트워크)
                                          │
                                          ▼ Phase 4:
                                       NetworkInputSystem (원격)
                                       StateReplicationSystem (서버 스냅샷)
                                       PredictionSystem (rollback)
```

**규칙**:
- 각 시스템은 **읽는 컴포넌트 / 쓰는 컴포넌트** 가 명시된 "계약"을 갖는다
- Scene 은 시스템 등록만 함 (**200 줄**)
- 새 시스템 추가 = 파일 1 개 + 등록 1 줄, Scene 수정 없음

---

## 5. 챔프가 "데이터 + 폴더" 로 존재하는 모습

```
Client/
├─ Scene/Scene_InGame.{h,cpp}                     ← 오케스트레이션만
│
├─ Gameplay/                                        ← 게임 공통 (컨텐츠 무관)
│   ├─ SkillDef.h                                  ← enum·struct 정의
│   ├─ SkillRegistry.{h,cpp}                       ← 동적 Registry (self-register)
│   ├─ ChampionDef.h
│   ├─ ChampionRegistry.{h,cpp}
│   ├─ SkillHookRegistry.{h,cpp}                   ← hookId → 함수 맵
│   ├─ InputIntent.h                               ← Pulse / State 분리
│   ├─ Buff/BuffDef.h + BuffRegistry.{h,cpp}
│   └─ Spatial/SpatialGrid.{h,cpp}                 ← (B-10)
│
├─ Systems/                                         ← 시스템 구현
│   ├─ InputSystem.{h,cpp}
│   ├─ TargetingSystem.{h,cpp}
│   ├─ SkillDispatchSystem.{h,cpp}
│   ├─ SkillSimulationSystem.{h,cpp}
│   ├─ MovementSystem.{h,cpp}
│   ├─ AnimationSystem.{h,cpp}
│   ├─ BuffSystem.{h,cpp}
│   └─ RenderSystem.{h,cpp}
│
├─ Champions/                                       ← 한 챔프 = 한 폴더
│   ├─ Irelia/
│   │   ├─ Irelia_Registration.cpp                 ← 정적 등록자 (Registry 에 1 회)
│   │   ├─ Irelia_Skills.cpp                       ← 훅 함수들 (onCast 구현)
│   │   └─ Irelia_Components.h                     ← IreliaWStageComponent 등
│   └─ Yasuo/
│       ├─ Yasuo_Registration.cpp
│       ├─ Yasuo_Skills.cpp
│       └─ Yasuo_Components.h                      ← YasuoStateComponent 이관
│
└─ UI/
    ├─ CombatDebugPanel.{h,cpp}
    ├─ MapTunerPanel.{h,cpp}
    └─ SkillStatePanel.{h,cpp}
```

**협업 관점**:
- 챔프 담당자 A = `Champions/Irelia/` 만 수정 — 머지 충돌 0
- 시스템 담당자 B = `Systems/MovementSystem.cpp` 만 수정
- UI 담당자 C = `UI/` 만 수정
- 공유 테이블이 없음 (**self-registering** 덕분에 SkillTable.cpp / ChampionTable.cpp 삭제)

---

## 6. "세 겹 동기화" 와 그 해결

### 현재 (Phase B-6.6)
```
   CTransform m_IreliaTransform   [1] 이동 적분이 쓰는 곳
           │
           ▼  SyncECSTransformsFromLegacy()  매 프레임 복사
   TransformComponent (ECS)       [2] 타겟 위치·히트 판정이 읽는 곳
           │
           ▼  ModelRenderer.UpdateTransform(worldMat)
   ModelRenderer GPU cbuffer      [3] 셰이더가 읽는 곳
```
**문제**: 한 사실(위치)이 3 곳에 존재 → 버그·비용·네트워크 장애물.

### B-12 이후
```
   TransformComponent (ECS)       [단일 원천]
           │                           ▲
           ▼                           │  서버 스냅샷 덮어쓰기
   RenderSystem:
     ForEach<Transform,MeshRef>(w, [](e, t, m) {
         GetPipeline(m.shader)
             .UpdateWorld(t.world)
             .Draw(m.meshHandle, e.GetAnimator());
     });
```
- 레거시 `CTransform` 은 맵 같은 정적 오브젝트에만 (선택). 챔프/유닛은 전부 ECS.
- 네트워크 수신은 TransformComponent 에 직접 쓴다.
- 렌더는 ECS 를 읽기만 한다.

---

## 7. ModelRenderer 분해 (B-7a 핵심)

### 현재
```
   ModelRenderer (한 덩어리)
   ├─ Shader + Pipeline                  (쉐이더/상태)
   ├─ CModel (Mesh/Skeleton/Materials)   (공유 에셋)
   ├─ CAnimator (time, currentAnim)       (인스턴스 상태)
   └─ Texture overrides[]                 (인스턴스 꾸미기)
```
**한 덩어리라** ECS 컴포넌트로 넣을 수 없음. 포인터로 넣으면 다시 세 겹 동기화.

### B-7a 후
```
   AssetRegistry (애플리케이션 1 개)          ← 메시/스켈레톤/머티리얼 refcount
     LoadModel(path) → ModelHandle

   Components (엔티티마다):
     MeshRefComponent         { ModelHandle, materialSetId }
     AnimatorComponent        { CAnimator 인스턴스 }        ← 스킨드 때만
     MaterialOverrideComponent{ vector<{slot, texPath}> }   ← 선택

   RenderSystem (애플리케이션 1 개):
     GetPipeline(shaderId)     ← Pipeline·RS·Sampler 보유
     ForEach<Transform, MeshRef>
```
**효과**: 챔프 스폰 = 컴포넌트 조립 한 줄씩. 150 챔프 프리할당 필요 없음.

---

## 8. 네트워크(Phase 4) 가 위 구조에 어떻게 붙는가

```
   Local Client                                          Server (Go or C++)
   ─────────────────                                     ─────────────────
   LocalInputSystem → InputIntent                        
   SkillDispatchSystem → CastCommand ─────[UDP/KCP]────► NetworkInputSystem
   SkillSimulationSystem (예측)                          SkillSimulationSystem (권위)
      ↓                                                    ↓
   TransformComponent (예측값)                            TransformComponent (진실)
                                  ◄───[Snapshot]─────── StateReplicationSystem
   PredictionSystem 
     ├─ 서버 스냅샷 < 예측 tick 이면 덮어쓰기
     └─ CommandQueue 에서 미확인 커맨드 재실행
```

**이 구조의 전제**:
- `SkillSimulationSystem::Execute(world, cmd)` 는 **순수 함수** — 입력만으로 결정적
- 사이드 이펙트(애니 재생, 파티클, 사운드)는 **AnimationRequestComponent** 를 emit 하고 AnimationSystem 이 소비 → 서버에서는 이 시스템이 꺼져 있음
- 위 규약을 Phase 4 직전에 도입하면 늦음 — **B-9~B-10 에서 미리 박아둠** (이래서 InputIntent 의 Pulse/State 분리가 중요)

---

## 9. 지금부터의 로드맵 (B-7a → B-12)

| 단계 | 제목 | 코어 산출물 | 선행 결정 |
|---|---|---|---|
| **B-7a** | ModelRenderer 분해 | `AssetRegistry::LoadModel`, `ModelHandle`, `CAnimator` per-entity, `MaterialOverrideComponent` | ECS 단일 원천 방향 확정 |
| **B-7b** | ChampionSpawnSystem | `ChampionSpawnSystem::Spawn(champ, pos, team)` — 컴포넌트 조립, Scene 의 수동 멤버 제거 | B-7a 완료 |
| **B-8** | UI 분리 | `UI/CombatDebugPanel`, `UI/MapTunerPanel`, `UI/SkillStatePanel` 로 ImGui 코드 이관 (리스크 0, 경험 쌓기) | — |
| **B-9** | InputSystem + SkillDispatchSystem 시스템화 | `InputIntentComponent` (Pulse vs State 규약), `InputSystem`, `SkillDispatchSystem`, `SkillSimulationSystem` 분리 | Pulse/State 규약 확정 |
| **B-10** | SkillHook + 챔프 폴더 | `SkillHookRegistry` (hookId→Fn), self-registering `Registration.cpp`, `Champions/Irelia`·`Champions/Yasuo` 구조 도입 | hookId 방식 확정, self-register 방식 확정 |
| **B-11** | BuffSystem | `BuffComponent`, `BuffInstance`, `BuffSystem` 틱, 첫 적용: 이렐리아 W 방어막 | — |
| **B-12** | 레거시 Transform 제거 | Scene 의 `CTransform m_XxxTransform` 전부 제거, TransformComponent 단일 원천. 카메라가 EntityID 팔로우 | 침습적 — 네트워크 직전 |
| **Phase 4** | 네트워크 통합 | `NetworkInputSystem` / `StateReplicationSystem` / `PredictionSystem` — Scene 수정 0 | 위 6 단계 완료 가정 |

---

## 10. 확정된 6 결정 (리뷰 반영)

| # | 결정 | 이유 |
|---|---|---|
| ① | SkillHook 은 `uint32_t hookId` + 외부 `SkillHookRegistry` (함수 포인터 **금지**) | SkillDef POD 유지 → FlatBuffers 직렬화·DLL 경계·Lua 이관 전부 호환 |
| ② | `ChampionTagComponent` 는 HUD 등 **표시용만**. 챔프별 분기는 `HasComponent<YasuoState>()` 로 | 150-way switch 재발 방지 |
| ③ | ModelRenderer **분해(B-7a)가 ChampionSpawn(B-7b) 선행** | 컴포넌트로 넣을 수 없는 덩어리를 먼저 쪼갬 |
| ④ | `SkillTable.cpp`·`ChampionTable.cpp` 중앙 배열 **폐기**. 챔프별 `_Registration.cpp` 가 Registry 에 자기 등록 | 중앙 파일 머지 충돌 소멸 |
| ⑤ | `InputIntent` = **Pulse(1-tick)** 와 **State(지속)** 분리. Pulse 는 읽은 시스템이 consume (flag 초기화) | 봇·네트워크·예측이 동일 규약으로 동작 |
| ⑥ | `SystemScheduler` 는 B-10 이후 시스템 7-8 개 시점에 도입 (지금은 Scene 오케스트레이션 유지) | 조기 추상화 회피 |

---

## 11. 용어 빠른 참조

| 용어 | 뜻 |
|---|---|
| **World** | ECS 의 "게임 상태 전체" — 엔티티와 컴포넌트의 컨테이너 |
| **Entity** | 정수 ID 하나. 그 자체는 데이터 없음 |
| **Component** | 엔티티에 붙는 **데이터 조각**. 로직 없음 (POD 지향) |
| **System** | 특정 컴포넌트 조합을 가진 엔티티를 대상으로 동작하는 **함수** |
| **Registry** | 런타임에 등록되는 테이블 (`SkillRegistry`, `ChampionRegistry`, `HookRegistry`) |
| **Pulse Input** | 한 틱만 유효한 요청 (예: "Q 지금 눌렀다") — 읽으면 소비 |
| **State Input** | 지속 유지되는 의도 (예: "저 좌표로 이동 중") — 덮어쓸 때까지 유지 |
| **SkillDef** | 스킬의 **정적 데이터** (쿨 · 레인지 · 애니키 · hookId) |
| **SkillHook** | 스킬 발동 시 호출되는 **함수** (Irelia_W_Stage2 등). SkillDef.hookId 로 연결 |
| **CastCommand** | 네트워크 전송 가능한 스킬 시전 요청 POD |
| **예측(Prediction)** | 클라가 서버 응답 전에 미리 결과를 반영하고 나중에 보정 |

---

## 11. 진행 기록 (2026-04 기준)

### B-6.5 ✅ (SkillDispatch 기반)
- `SkillDef` (eTargetMode 5종: Self / UnitTarget / GroundTarget / Direction / Conditional) + `CastSkillCommand` POD + `FindSkillDef`
- `g_SkillTable[]` Irelia 5 + Yasuo 5
- `YasuoStateComponent` (qStackCount 0~3 + 6초 리셋, bEActive / eActiveTimer)
- `DispatchSkillInput` / `BuildCastCommand` / `ApplyLocalPrediction` / `UpdateCombatInput` 재작성
- Direction 폴백 (카메라 forward-XZ) 로 호버 없이 R 동작

### B-6.6 ✅ (디테일 + 확장 기반)
- `ChampionDef` + `FindChampionDef` — 챔프 메타 (animPrefix / idle·run / basicAttackKey) 데이터 이관, `FirePlayerAction` 의 if-else 하드코딩 제거
- `SkillDef` 확장: `eRotateMode` (None/TowardsTarget/TowardsCursor), `lockDurationSec`, `bOneShot`, 2-stage 일체 (`stageCount` / `stage2TargetMode` / `stage2AnimKey` / `stage2LockSec` / `stage2Rotate` / `stageWindowSec`)
- `SkillStateComponent` / `SkillSlotRuntime` — 슬롯 5개 (BA/Q/W/E/R) 각각 `cooldownRemaining` / `currentStage` / `stageWindow`
- `RotatePlayerToward(eRotateMode, CastSkillCommand)` — 이동 로직의 `atan2f(dx,dz)+XM_PI` 컨벤션 재사용
- `DispatchSkillInput` 에 cooldown 체크 + 2-stage 분기 (stage1 진입 → 윈도우 내 재입력 → stage2 발동 + 쿨 시작)
- `ApplyLocalPrediction` 에 rotate + `PlayAnimationByName(full, !bOneShot)` + cooldown 시작
- `ModelRenderer::PlayAnimationByName(const string&, bool bLoop)` 오버로드 구현 (`CAnimator::PlayAnimation(pAnim, bLoop)` 전달 → 진짜 원샷 동작)
- 이렐리아 W **2-stage** (Self → Direction) 데이터 반영 — LoL 본작 Defiant Dance 재현

### 빌드 파이프라인 재편 ✅ (vcpkg 탈피 → ThirdPartyLib 자립)
- `C:\vcpkg` 에서 `Engine/ThirdPartyLib/` 로 Assimp (+ 5 transitive DLL) / DirectXTK / FMOD 이관
- `Engine.vcxproj`: `<VcpkgEnabled>false</VcpkgEnabled>`, ThirdPartyLib 경로·lib 추가, `Public\Sound` include root 추가
- `UpdateLib.bat`: ThirdPartyLib `Bin/{Debug,Release}/*.dll` → Client/Bin 자동 복사. FMOD 는 단일 DLL 양쪽 복사
- `Client.vcxproj`: PreBuild `$(SolutionDir)UpdateLib.bat` (상대경로 버그 수정). PostBuild 가 Shaders + Resource 모두 OutDir 로 xcopy
- 절차 문서화: `.md/build/THIRDPARTY_INTEGRATION_GUIDE.md`

### FMOD + CSound_Manager 통합 ✅
- `Engine/Public/Sound/Sound_Manager.h` (클래스 `CSound_Manager`, 파일명 C 접두사 없음 — Winters 컨벤션)
- `Engine/Public/Sound/SoundChannel.h` — `eSoundChannel` enum 9 슬롯
- `Engine/Private/Sound/Sound_Manager.cpp` — FMOD System init · Resource/Sound 재귀 UTF-8 로드 · 고정/자동/BGM 3가지 재생 패턴 · RAII 정리
- `CGameInstance` Tier1 포워딩: `PlaySoundOn` / `PlayEffect` / `PlayBGM` / `StopChannel` / `StopAllSounds` / `SetChannelVolume` / `SetMasterVolume` / `Tick_Engine()`
- `CEngineApp::Run()` 루프에 `Tick_Engine()` 삽입 → 매 프레임 `FMOD::System::update()`
- **경로 해결 우회**: `WintersResolveContentPath` 가 파일 전용이라 폴더 거부 → `GetModuleFileNameW(nullptr)` + exe dir 기반 직접 구성

### 헤더 위생 ✅
- Entity.h / ComponentStore.h / CCommandBuffer.h — unqualified `vector`/`function` → `std::vector` / `std::function`
- ModelRenderer.h — `bool_t` → `bool` (namespace Engine 밖이라 사용 불가)

### 다음 — B-7a 착수 조건
- ModelRenderer 의 Impl pimpl 이 Shader + Pipeline + CModel + CAnimator + Texture override 를 한 덩어리로 품음 → ECS 컴포넌트 분해 필요
- 목표: AssetRegistry(공유 모델 refcount) + 인스턴스별 CAnimator + MaterialOverride(옵션)
- 기존 ModelRenderer 는 **그대로 유지** 하고 옆에 새 경로 공존, B-7b 에서 이주
- 착수 전 `.md/architecture/WINTERS_GAMEPLAY_ARCHITECTURE.md` 와 `C:\Users\user\.claude\plans\jiggly-leaping-mountain.md` 재확인
