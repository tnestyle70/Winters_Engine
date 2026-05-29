# WintersElden Client Runtime Architecture

## 목표

`WintersElden.exe`는 LoL 클라이언트의 씬/카메라/입력/전투를 재사용하지 않는다.

공통 엔진만 사용한다.

```
WintersElden.exe
  -> WintersEngine.dll
  -> CEldenGameApp
  -> Scene_EldenFieldTest
  -> ThirdPersonCamera / ActionCombat / WorldPartition / RaidClient
```

## 모듈 구조

```
WintersElden/Public/
├── CEldenGameApp.h
├── Defines.h
├── Scene/
│   ├── Scene_EldenBoot.h
│   ├── Scene_EldenLoading.h
│   ├── Scene_EldenMainMenu.h
│   ├── Scene_EldenFieldTest.h
│   ├── Scene_EldenRaidTest.h
│   └── Scene_EldenEditor.h
├── Camera/
│   ├── ThirdPersonCamera.h
│   ├── SpringArm.h
│   ├── LockOnSystem.h
│   └── CameraCollisionSolver.h
├── Character/
│   ├── EldenCharacterController.h
│   ├── ActionStateMachine.h
│   └── PlayerActionDefs.h
├── Combat/
│   ├── StaminaSystem.h
│   ├── HitboxTimeline.h
│   ├── HurtboxComponent.h
│   ├── DodgeRollSystem.h
│   └── ParryGuardSystem.h
├── World/
│   ├── WorldPartitionSystem.h
│   ├── WorldCell.h
│   ├── StreamingSourceComponent.h
│   └── DataLayerSystem.h
├── Raid/
│   ├── RaidClient.h
│   ├── BossControllerClient.h
│   ├── BossPhaseGraph.h
│   └── RaidReplicationView.h
├── FX/
├── Editor/
└── UI/
```

## Scene 흐름

```
Boot
  -> Loading
  -> MainMenu
      -> FieldTest
      -> RaidTest
      -> Editor
```

초기 구현은 바로 `FieldTest`로 진입해도 된다.

```cpp
bool CEldenGameApp::OnInit()
{
    RegisterEldenSystems();

    auto pScene = CScene_EldenFieldTest::Create();
    CGameInstance::Get()->Change_Scene(
        static_cast<uint32_t>(eEldenSceneID::FieldTest),
        std::move(pScene));

    return true;
}
```

## Scene_EldenFieldTest 책임

`Scene_EldenFieldTest`는 첫 세로 슬라이스의 중심이다.

책임:

1. player character 1체 spawn
2. test enemy 1체 spawn
3. ground/map chunk 1개 로드
4. third-person camera 연결
5. action input 처리
6. hitbox/hurtbox debug draw
7. asset streaming tick 호출
8. ImGui debug panels 표시

금지:

1. 월드 파티셔닝 전체 구현을 Scene 내부에 직접 하드코딩
2. 보스 패턴을 Scene `if` 분기로 작성
3. 에셋 경로를 여러 시스템에 흩뿌리기

Scene은 orchestration만 한다.

## Player Controller

Elden 스타일 입력은 LoL QWER과 다르다.

| 입력 | 의미 |
|---|---|
| WASD / Left Stick | 이동 |
| Mouse / Right Stick | 카메라 |
| Lock-on key | 타겟 고정 |
| Light attack | 약공격 |
| Heavy attack | 강공격 |
| Dodge | 회피/구르기 |
| Guard | 방어 |
| Parry | 패링 |
| Use item | 아이템 |

Action state 예시:

```cpp
enum class eActionState : uint8_t
{
    Idle,
    Move,
    Sprint,
    Dodge,
    LightAttack,
    HeavyAttack,
    Guard,
    Parry,
    HitReact,
    Knockdown,
    Death
};
```

## Action State Machine

액션 RPG는 "입력 즉시 애니 재생"이 아니라 상태 전환 규칙이 중요하다.

```
Idle/Move
  -> LightAttack
  -> Dodge
  -> Guard
  -> HitReact

LightAttack
  -> ComboWindow -> LightAttack2
  -> Recovery -> Idle/Move
  -> DodgeCancel if allowed

Dodge
  -> IFrameWindow
  -> Recovery
  -> Idle/Move
```

핵심 데이터:

```cpp
struct ActionWindow
{
    f32_t startSec = 0.f;
    f32_t endSec = 0.f;
};

struct PlayerActionDef
{
    const char* actionName = nullptr;
    const char* animKey = nullptr;
    f32_t durationSec = 0.f;
    f32_t staminaCost = 0.f;
    ActionWindow inputBuffer;
    ActionWindow cancelWindow;
    ActionWindow iFrameWindow;
    ActionWindow hitboxWindow;
};
```

## Third Person Camera

LoL `DynamicCamera`와 별도로 둔다.

```
CThirdPersonCamera
├── CSpringArm
├── CLockOnSystem
├── CCameraCollisionSolver
├── CCameraShakeSystem
└── CCameraPreset
```

필수 기능:

| 기능 | 기준 |
|---|---|
| Spring arm | player pivot에서 일정 거리 유지 |
| Shoulder offset | 약간 오른쪽/위쪽 offset |
| Pitch clamp | 바닥/하늘 과회전 방지 |
| Lock-on | 타겟 중심으로 orbit |
| Collision | 벽 뒤 카메라 clipping 방지 |
| Smooth | yaw/pitch/distance 보간 |
| Shake | 피격/보스 stomp 이벤트 |

## Combat Runtime

Elden 전투는 hitbox timeline 중심으로 간다.

```
Animation time
  -> HitboxTimeline evaluates active hit volumes
  -> Server or local test validates overlap with hurtboxes
  -> DamageEvent / HitReactEvent
  -> FX / Sound / CameraShake
```

컴포넌트 후보:

```cpp
struct StaminaComponent
{
    f32_t current = 100.f;
    f32_t max = 100.f;
    f32_t regenPerSec = 20.f;
    f32_t regenDelay = 0.8f;
};

struct HurtboxComponent
{
    EntityID owner = NULL_ENTITY;
    Vec3 centerOffset;
    Vec3 halfExtents;
    u32_t bodyPart = 0;
};

struct HitboxInstance
{
    EntityID attacker = NULL_ENTITY;
    Vec3 center;
    Vec3 halfExtents;
    f32_t damage = 0.f;
    u32_t hitFlags = 0;
};
```

## Boss/Raid Client

클라는 보스 판정의 권위자가 아니다. 클라 역할:

1. 서버 snapshot 수신
2. 보스 phase/animation state 표시
3. telegraph/FX/camera를 예쁘게 재생
4. local prediction 가능한 것은 player movement와 일부 client-side effect뿐

```text
Server BossSim
  -> BossSnapshot
  -> Client RaidReplicationView
  -> Animator / FX / UI
```

## UI

Elden UI는 LoL HUD와 분리한다.

```
UI/
├── EldenHUD.h
├── LockOnReticle.h
├── StaminaBar.h
├── BossHealthBar.h
├── RaidPartyPanel.h
├── InteractionPrompt.h
└── Debug/
    ├── ActionStatePanel.h
    ├── HitboxTimelinePanel.h
    ├── WorldPartitionPanel.h
    └── AssetStreamingPanel.h
```

## 초기 Scene 로드 순서

```
Scene_EldenFieldTest::OnEnter
  1. Initialize world
  2. Initialize asset streaming system
  3. Load minimal field cell
  4. Load player asset manifest
  5. Spawn player entity
  6. Attach ThirdPersonCamera
  7. Spawn dummy enemy
  8. Enter play mode
```

## Runtime 업데이트 순서

```
Input
  -> PlayerActionController
  -> Movement / Dodge / Combat state
  -> Camera update
  -> WorldPartition streaming source update
  -> Asset streaming pump
  -> Animation update
  -> Hitbox timeline
  -> FX
  -> UI
  -> Render
```

네트워크 모드에서는:

```
Input
  -> local prediction
  -> send InputCommand
  -> receive authoritative Snapshot
  -> reconciliation/interpolation
  -> render
```

## 완료 기준

| Stage | 기준 |
|---|---|
| E0 | `WintersElden.exe` 부팅 |
| E1 | Scene_EldenFieldTest 진입 |
| E2 | chr3010 표시 |
| E3 | idle/run/attack/dodge 애니 재생 |
| E4 | 3인칭 카메라 + lock-on |
| E5 | hitbox/hurtbox debug draw |
| E6 | dummy enemy 피격 |
| E7 | streaming source가 world cell load/unload 요청 |
