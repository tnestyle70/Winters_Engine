SUPERSEDED: 이 계획의 `-PI/2` projectile yaw 및 E 3배 제안은 사용자 시각 검증과 최종 snapshot 소유 경로 분석으로 폐기됐다. BA/W/E/R의 authored yaw offset은 모두 `0.f`이며, E는 BA 2배, R은 BA 3배가 현재 정본이다. 실제 반영은 `2026-07-15_ASHE_PROJECTILE_ZERO_YAW_GRASS_TINT_CORRECTIVE_REPORT.md`를 따른다.

Session - Ashe W의 서버 권위 8발·45도 cone과 BA FBX MeshParticle(BA/W 0.021)은 유지하고, snapshot yaw 보정으로 FBX의 local X축을 진행 방향에 맞춘다. E는 서버 EffectTrigger가 전달한 방향으로만 3x BA FBX를 0.70초간 이동시키며, R의 현재 3x BA FBX(0.063)는 보존·검증한다. Gameplay projectile, hit, damage, vision/reveal 권한은 추가하지 않는다. 같은 authority 원칙으로 Irelia E의 미니언 피해, E/R 표식, 표식 Q 소비·Q 쿨다운 초기화와 그 snapshot/FX 복구 경로를 이 문서에 통합한다.

## 1. 반영해야 하는 코드

### 1-1. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Projectile/ProjectileVisualCatalog.cpp

기존 코드:

```cpp
    constexpr u16_t kStructureProjectileKind = 100;

    constexpr ProjectileVisualDesc kNoProjectileVisual{};
```

아래에 추가:

```cpp
    constexpr f32_t kAsheArrowMeshYawOffset = -1.57079632679f;
```

기존 코드:

```cpp
    constexpr ProjectileVisualDesc kAsheBasicAttackVisual{
        "Ashe.BA.Arrow", nullptr, "Ashe.BA.Hit" 
    };

    constexpr ProjectileVisualDesc kAsheVolleyArrowVisual{
        "Ashe.W.Arrow", "Ashe.W.Hit", nullptr
    };

    constexpr ProjectileVisualDesc kAsheCrystalArrowVisual{
        "Ashe.R.Arrow", "Ashe.R.Hit", nullptr
    };
```

아래로 교체:

```cpp
    constexpr ProjectileVisualDesc kAsheBasicAttackVisual{
        "Ashe.BA.Arrow", nullptr, "Ashe.BA.Hit", nullptr, nullptr, nullptr,
        kAsheArrowMeshYawOffset
    };

    constexpr ProjectileVisualDesc kAsheVolleyArrowVisual{
        "Ashe.W.Arrow", "Ashe.W.Hit", nullptr, nullptr, nullptr, nullptr,
        kAsheArrowMeshYawOffset
    };

    constexpr ProjectileVisualDesc kAsheCrystalArrowVisual{
        "Ashe.R.Arrow", "Ashe.R.Hit", nullptr, nullptr, nullptr, nullptr,
        kAsheArrowMeshYawOffset
    };
```

`EnsureProjectilePresentation`은 매 snapshot에서 `mesh.vRotation.y = yaw`를 설정한다. 이 catalog offset은 WFX의 `rotation.y = -1.57079632679`와 같은 값을 그 `yaw`에 포함하므로 BA/W/R mesh의 authored 축 보정이 매 snapshot 뒤에도 유지된다. W/R trail은 camera-facing Billboard이므로 이 값으로 회전하지 않는다.

### 1-2. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/EventApplier.cpp

`EntityID CEventApplier::EnsureProjectilePresentation`의 `if (visual.pszSpawnCue)` 블록에서 아래 기존 코드 바로 아래에 bounded Debug trace를 추가한다. 이 trace는 W 한 번에 8개 projectile마다 mesh와 billboard가 각각 실제로 생성됐는지 확인하는 검증 전용이며 Release에는 포함되지 않는다.

기존 코드:

```cpp
            CFxCuePlayer::PlayAll(
                world,
                visual.pszSpawnCue,
                fx,
                &visualIt->second);
```

아래에 추가:

```cpp
#if defined(_DEBUG)
            const eProjectileKind projectileKind =
                static_cast<eProjectileKind>(uProjectileKind);
            if (projectileKind == eProjectileKind::AsheBasicAttack ||
                projectileKind == eProjectileKind::AsheVolleyArrow ||
                projectileKind == eProjectileKind::AsheCrystalArrow)
            {
                static u32_t s_asheProjectileVisualTraceCount = 0u;
                if (s_asheProjectileVisualTraceCount < 64u)
                {
                    u32_t meshCount = 0u;
                    u32_t billboardCount = 0u;
                    for (const EntityID visualEntity : visualIt->second)
                    {
                        if (world.HasComponent<FxMeshComponent>(visualEntity))
                            ++meshCount;
                        if (world.HasComponent<FxBillboardComponent>(visualEntity))
                            ++billboardCount;
                    }

                    char msg[256]{};
                    sprintf_s(
                        msg,
                        "[AsheProjectileVisual] net=%u kind=%u mesh=%u billboard=%u yaw=%.3f\\n",
                        static_cast<u32_t>(uProjectileNet),
                        static_cast<u32_t>(uProjectileKind),
                        meshCount,
                        billboardCount,
                        yaw);
                    OutputDebugStringA(msg);
                    ++s_asheProjectileVisualTraceCount;
                }
            }
#endif
```

### 1-3. C:/Users/user/Desktop/Winters/Client/Public/GameObject/Champion/Ashe/AsheVisualCueCatalog.h

기존 코드:

```cpp
namespace Ashe::VisualCue
{
    constexpr const char* kBAArrow = "Ashe.BA.Arrow";
    constexpr const char* kBAHit = "Ashe.BA.Hit";
    constexpr const char* kWCast = "Ashe.W.Cast";
    constexpr const char* kRCharge = "Ashe.R.Cast";

    constexpr f32_t kBAArrowSpeed = 18.f;
    constexpr f32_t kBAArrowLifetime = 0.4f;
    constexpr f32_t kWCastLifetime = 0.45f;
    constexpr f32_t kRChargeLifetime = 0.45f;
}
```

아래로 교체:

```cpp
namespace Ashe::VisualCue
{
    constexpr const char* kBAArrow = "Ashe.BA.Arrow";
    constexpr const char* kBAHit = "Ashe.BA.Hit";
    constexpr const char* kWCast = "Ashe.W.Cast";
    constexpr const char* kEHawkshot = "Ashe.E.Hawkshot";
    constexpr const char* kRCharge = "Ashe.R.Cast";

    constexpr f32_t kBAArrowSpeed = 18.f;
    constexpr f32_t kBAArrowLifetime = 0.4f;
    constexpr f32_t kWCastLifetime = 0.45f;
    constexpr f32_t kEHawkshotTravelSec = 0.70f;
    constexpr f32_t kRChargeLifetime = 0.45f;
}
```

### 1-4. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Ashe/Ashe_Skills.cpp

기존 코드:

```cpp
        void OnCastFrame_E_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand) return;
            if (!ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity)) return;

            const Vec3 origin =
                ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
            const Vec3 dir = ctx.pCommand->direction;
            const Vec3 dest = { origin.x + dir.x * 25.f, origin.y, origin.z + dir.z * 25.f };
            Fx::SpawnEHawkshot(*ctx.pWorld, origin, dest, 5.0f);
        }
```

아래로 교체:

```cpp
        void OnCastFrame_E_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand || !ctx.bAuthoritativeEvent) return;
            if (!ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity)) return;

            const Vec3 forward =
                WintersMath::NormalizeXZOrZero(ctx.pCommand->direction);
            if (forward.x == 0.f && forward.z == 0.f) return;

            const f32_t range = (ctx.pDef && ctx.pDef->rangeMax > 0.f)
                ? ctx.pDef->rangeMax
                : 25.f;
            const f32_t speed = range / VisualCue::kEHawkshotTravelSec;
            const Vec3 casterPosition =
                ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();

            FxCueContext fx{};
            fx.vWorldPos = {
                casterPosition.x + forward.x * 0.8f,
                casterPosition.y + 1.f,
                casterPosition.z + forward.z * 0.8f };
            fx.vForward = forward;
            fx.vVelocity = { forward.x * speed, 0.f, forward.z * speed };
            fx.attachTo = NULL_ENTITY;
            fx.pFxMeshRenderer = ctx.pFxMeshRenderer;
            fx.bOverrideVelocity = true;
            fx.bOverrideLifetime = true;
            fx.fLifetimeOverride = VisualCue::kEHawkshotTravelSec;
            CFxCuePlayer::PlayAll(
                *ctx.pWorld,
                VisualCue::kEHawkshot,
                fx,
                nullptr);
        }
```

이 함수는 local cast-frame/레거시 경로를 재생하지 않고 server EffectTrigger의 `ctx.bAuthoritativeEvent`에서 한 번만 실행한다. 새 cue에는 `anchor`를 쓰지 않고 `attachTo = NULL_ENTITY`로 두어 `CFxMeshSystem`이 velocity를 적분하게 한다.

### 1-5. C:/Users/user/Desktop/Winters/Client/Public/GameObject/Champion/Ashe/Ashe_FxPresets.h

삭제할 코드:

```cpp
    void SpawnEHawkshot(CWorld& world, const Vec3& start, const Vec3& dest, f32_t fLifetime);
```

### 1-6. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Ashe/Ashe_FxPresets.cpp

삭제할 코드:

```cpp
    constexpr const wchar_t* kPathEHawkTex =
        L"Texture/Character/Ashe/particles/ashe_base_e_textureowl.png";
```

삭제할 코드:

```cpp
    void SpawnEHawkshot(CWorld& world, const Vec3& start, const Vec3&, f32_t fLifetime)
    {
        FxBillboardComponent fx{};
        fx.attachTo = NULL_ENTITY;
        fx.vWorldPos = { start.x, start.y + 3.0f, start.z };
        fx.vAttachOffset = { 0.f, 0.f, 0.f };
        fx.texturePath = kPathEHawkTex;
        fx.fWidth = 1.4f;
        fx.fHeight = 1.0f;
        fx.bBillboard = true;
        fx.fLifetime = fLifetime;
        fx.vColor = { 0.8f, 1.1f, 1.3f, 1.f };
        fx.blendMode = eBlendPreset::Additive;
        fx.fFadeOut = fLifetime * 0.4f;
        CFxSystem::Spawn(world, fx);
    }
```

### 1-7. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Ashe/e_hawkshot.wfx

새 파일:

```json
{
  "name": "Ashe.E.Hawkshot",
  "emitters": [
    {
      "name": "e_hawkshot_arrow_mesh",
      "render_type": "MeshParticle",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "model": "Client/Bin/Resource/Texture/Character/Ashe/particles/fbx/ashe_base_aa_arrow.fbx",
      "texture": "Client/Bin/Resource/Texture/Character/Ashe/particles/ashe_base_aa_arrowtext.png",
      "lifetime": 0.70,
      "scale": [0.063, 0.063, 0.063],
      "rotation": [0.0, -1.57079632679, 0.0],
      "color": [0.78, 1.18, 1.52, 0.98],
      "attach_offset": [0.0, 0.0, 0.0],
      "fade_in": 0.01,
      "fade_out": 0.18
    }
  ]
}
```

E는 첫 검증 단계에서 Billboard/Ribbon emitter를 넣지 않는다. 따라서 mesh renderer 미주입 또는 mesh preload 실패 시 “trail만 남아서 FBX처럼 보이지 않는” 상태를 숨기지 않는다.

### 1-8. C:/Users/user/Desktop/Winters/Data/LoL/ClientPublic/Visual/ObjectVisualDefs.json

기존 코드:

```json
  "fxMeshPreloads": [
    {
      "key": "fx.irelia.e_beam",
      "mesh": "Texture/FX/Irelia/fbx/irelia_base_e_beam.fbx",
      "texture": "Texture/FX/Irelia/irelia_base_e_beam_mult.png"
    },
```

아래로 교체:

```json
  "fxMeshPreloads": [
    {
      "key": "fx.ashe.base_arrow",
      "mesh": "Texture/Character/Ashe/particles/fbx/ashe_base_aa_arrow.fbx",
      "texture": "Texture/Character/Ashe/particles/ashe_base_aa_arrowtext.png"
    },
    {
      "key": "fx.irelia.e_beam",
      "mesh": "Texture/FX/Irelia/fbx/irelia_base_e_beam.fbx",
      "texture": "Texture/FX/Irelia/irelia_base_e_beam_mult.png"
    },
```

이 single preload는 BA/W/E/R이 공유하는 cooked `ashe_base_aa_arrow.wmesh`를 게임 시작 시 한 번 준비한다. `Client/Private/Data/Generated/LoLVisualDefinitions.generated.cpp`는 직접 수정하지 않고 아래 검증 명령으로 재생성한다.

### 1-9. Irelia 표식 요구사항과 권위 경계

이번 범위의 게임 규칙은 다음으로 고정한다.

- E 2타의 선분에 닿은 적 챔피언과 적 미니언은 같은 E 물리 피해를 받는다. 챔피언만 기존 E stun을 받으며, 미니언 stun은 이번 요구에 포함하지 않는다.
- E 또는 R에 실제로 맞은 대상에는 `시전자 Irelia + 대상` 쌍의 서버 표식이 생긴다. R은 현재와 동일하게 챔피언 전용이며, R을 미니언 피해/표식으로 확장하지 않는다.
- 해당 시전자의 Q가 아직 살아 있는 같은 표식 대상을 대상으로 정상 승인되면 표식을 즉시 소비하고 Q의 `cooldownRemaining`과 `cooldownDuration`을 모두 0으로 만든다. 표식이 없거나 다른 Irelia의 표식이면 일반 Q 쿨다운을 유지한다.
- Q의 kill reset, 미니언 CC, 클라이언트 예측 쿨다운은 추가하지 않는다. 사용자가 요청한 것은 표식 소비 reset이며, damage queue의 사망 판정보다 앞선 서버 command/hook 처리에서 결정한다.

현재 문제의 직접 원인은 세 갈래다.

1. `IreliaGameSim::OnE`는 `ChampionComponent, TransformComponent`만 순회한다. 같은 파일의 W는 바로 아래에서 `MinionComponent, TransformComponent`도 순회하므로 damage pipeline이 아니라 E의 후보 수집 누락이다.
2. 현재 R의 `kIreliaREffectStageWallMark`는 이름만 mark인 R visual stage이며, 서버에 표식 상태를 저장하지 않는다. E/R 모두 표식 relation, 만료 tick, Q 소비가 없다.
3. 클라이언트 `PlayTargetMarkCue`/`ApplyEConnectHitTargets`는 로컬 cue만 재생한다. normal network 경로는 `Scene_InGameLocalSkills.cpp`에서 `bApplyLocalGameplay = false`로 호출되므로 이 cue는 서버 damage·표식·Q reset을 만들 수 없다. R도 stage 2/4에서 1.5초짜리 cue만 직접 재생한다.

권위 흐름은 아래 하나로 제한한다.

```text
Client input -> GameCommand -> CommandExecutor (Q cooldown write) -> IreliaGameSim
    E/R hit -> IreliaMark relation + EffectTrigger -> Snapshot gameplay state
    Q on same source/target -> consume relation + Q cooldown = 0 + EffectTrigger
-> SnapshotBuilder -> Client SnapshotApplier/EventApplier -> Irelia.Target.Mark FX
```

### 1-10. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/IreliaSimComponent.h

기존 코드:

```cpp
};

static_assert(std::is_trivially_copyable_v<IreliaSimComponent>);
```

아래로 교체:

```cpp
};

struct IreliaMarkComponent
{
    EntityHandle hSource = NULL_ENTITY_HANDLE;
    EntityHandle hTarget = NULL_ENTITY_HANDLE;
    u32_t uSourceNet = 0u;
    u32_t uTargetNet = 0u;
    u64_t uExpireTick = 0u;
    u8_t uRank = 1u;
};

static_assert(std::is_trivially_copyable_v<IreliaSimComponent>);
static_assert(std::is_trivially_copyable_v<IreliaMarkComponent>);
```

표식을 대상 챔피언의 단일 bool/component로 두지 않고 별도 relation entity로 둔다. 따라서 같은 적 미니언/챔피언을 두 Irelia가 각각 표식해도 `(source, target)` 두 관계가 공존하고, 한 Irelia의 Q가 다른 Irelia의 표식을 소비하지 않는다. `EntityHandle`은 재사용된 ECS index를 막고, 저장한 net ID는 source/target이 이미 사라진 만료 clear event와 snapshot 복구에 사용한다. 이 구조는 `EzrealEssenceFluxMarkComponent`의 검증된 모델을 그대로 따른다.

### 1-11. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Irelia/IreliaGameSim.cpp 및 C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Irelia/IreliaGameSim.h

기존 include anchor:

```cpp
#include "Shared/GameSim/Components/IreliaSimComponent.h"
```

아래에 추가:

```cpp
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Core/Checkpoint/KeyframeComponentRegistry.h"
#include "Shared/GameSim/Core/Determinism/DeterministicTime.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
```

`constexpr u8_t kIreliaREffectStageWallMark = 4u;` 바로 아래에 Irelia mark의 keyframe 자기등록을 추가한다. `EzrealGameSim.cpp`의 `s_bEzrealKeyframeComponentsRegistered`와 같은 한 번만 실행되는 등록으로 `IreliaMarkComponent`를 등록하며, 중복된 중앙 등록은 만들지 않는다.

```cpp
    const bool_t s_bIreliaKeyframeComponentsRegistered = []()
    {
        SimCheckpoint::KeyframeComponentRegistry::Get()
            .Register<IreliaMarkComponent>("IreliaMarkComponent");
        return true;
    }();
```

`void TickRWave(...)` 바로 위에 아래 helper 군을 추가한다. 구현은 `EzrealGameSim.cpp`의 `FindEssenceFluxMarkRelation`, `AttachOrRefreshEssenceFluxMark`, `TryConsumeEssenceFluxMark`, `TickEssenceFluxMarks`를 Irelia 이름으로 옮기되, damage/detonate는 넣지 않는다.

```cpp
    EntityID FindIreliaMarkRelation(CWorld& world, EntityID source, EntityID target);
    void EmitIreliaMarkEffect(CWorld& world, const TickContext& tc,
        u32_t effectId, EntityID source, EntityID target, u8_t rank,
        f32_t lifetimeSec, u32_t sourceNetOverride, u32_t targetNetOverride);
    void AttachOrRefreshIreliaMark(CWorld& world, const TickContext& tc,
        EntityID source, EntityID target, u8_t rank, f32_t durationSec);
    bool_t TryConsumeIreliaMark(CWorld& world, const TickContext& tc,
        EntityID source, EntityID target);
    void TickIreliaMarks(CWorld& world, const TickContext& tc);
    void ResetIreliaQCooldown(CWorld& world, EntityID caster);
```

helper의 정확한 책임은 다음과 같다.

- 모든 relation 탐색/만료 순회는 `DeterministicEntityIterator<IreliaMarkComponent>::CollectSorted(world)`를 사용한다.
- apply/refresh는 `MarkDurationSec`을 `SecondsToTicksCeil`로 올림한 최소 1 tick으로 바꾸고, `(source,target)` relation이 있으면 expire/rank/net ID만 갱신하며, 없으면 새 entity를 만들어 component를 붙인다.
- `kIreliaEffectMarkApply` event에는 서버가 정한 duration과 source/target net override를 실어 보낸다. consume에는 `kIreliaEffectMarkConsume`, 만료 또는 stale handle에는 `kIreliaEffectMarkClear`를 보낸 뒤 relation entity를 `DestroyEntity`한다. stale endpoint도 저장된 net override로 client의 남은 FX를 지울 수 있어야 한다.
- consume은 같은 source와 같은 Q target의 미만료 relation만 허용한다. 만료된 relation을 Q가 소비하지 않으며, 다음 `TickIreliaMarks`가 clear한다.
- `ResetIreliaQCooldown`은 `SkillStateComponent::slots[Q]`의 `cooldownRemaining`과 `cooldownDuration`을 함께 0으로 만든다. snapshot UI는 이미 두 값을 받아 표시하므로 client UI를 별도로 고치지 않는다.
- Debug에서는 64회 이하의 `[IreliaMark] apply/consume/clear/reset`를 `OutputDebugStringA`로 남긴다. 기존 `std::cout`만으로는 debugger에서 action/target/expire tick을 검증하기 어렵다.

`void OnQ(GameplayHookContext& ctx)`의 아래 기존 코드 바로 뒤에 표식 소비와 reset을 넣는다.

```cpp
        EnqueuePhysicalDamage(
            world,
            ctx.casterEntity,
            cmd.targetEntity,
            ctx.casterTeam,
            qBaseDamage + qDamagePerRank * static_cast<f32_t>(ctx.skillRank),
            static_cast<u16_t>((static_cast<u32_t>(eChampion::IRELIA) << 8) | 1u),
            ctx.skillRank);
```

아래에 추가:

```cpp
        if (TryConsumeIreliaMark(
                world,
                *ctx.pTickCtx,
                ctx.casterEntity,
                cmd.targetEntity))
        {
            ResetIreliaQCooldown(world, ctx.casterEntity);
        }
```

`CommandExecutor.cpp`는 hook를 부르기 전에 일반 Q cooldown을 기록한다. 따라서 이 위치가 정확히 그 값을 같은 tick에 덮어쓰며, `GameRoomTick.cpp`의 damage queue 실행보다 앞선다. Q kill 여부나 client prediction으로 reset하지 않는다.

`void OnE(GameplayHookContext& ctx)`에서 `eDamagePerRank`를 구한 다음의 기존 `world.ForEach<ChampionComponent, TransformComponent>(...)` 전체를 아래 구조로 교체한다. `a/b`, projection, closest point, `beamRadius`의 기존 기하 계산은 그대로 유지한다.

```cpp
        const f32_t eMarkDurationSec = ResolveIreliaSkillEffectParam(
            ctx, eSkillSlot::E, eSkillEffectParamId::MarkDurationSec);
        const f32_t eDamage =
            eBaseDamage + eDamagePerRank * static_cast<f32_t>(ctx.skillRank);

        auto tryHit = [&](EntityID target, eTeam team, const Vec3& pos,
            bool_t bChampionTarget)
        {
            if (team == ctx.casterTeam ||
                !GameplayStateQuery::CanReceiveEnemyAbilityHit(
                    world, ctx.casterEntity, target))
            {
                return;
            }

            f32_t u = ((pos.x - a.x) * dx + (pos.z - a.z) * dz) / segLenSq;
            if (u < 0.f) u = 0.f;
            if (u > 1.f) u = 1.f;
            const Vec3 closest{ a.x + dx * u, pos.y, a.z + dz * u };
            if (WintersMath::DistanceSqXZ(pos, closest) > beamRadius * beamRadius)
                return;

            if (bChampionTarget)
            {
                GameplayStatus::ApplyStun(world, *ctx.pTickCtx, target,
                    ctx.casterEntity, eChampion::IRELIA, eSkillSlot::E, stunSec);
            }
            EnqueuePhysicalDamage(world, ctx.casterEntity, target, ctx.casterTeam,
                eDamage,
                static_cast<u16_t>((static_cast<u32_t>(eChampion::IRELIA) << 8) | 3u),
                ctx.skillRank);
            AttachOrRefreshIreliaMark(world, *ctx.pTickCtx,
                ctx.casterEntity, target, ctx.skillRank, eMarkDurationSec);
        };

        world.ForEach<ChampionComponent, TransformComponent>(
            std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                [&](EntityID target, ChampionComponent& champion, TransformComponent& tf)
                {
                    tryHit(target, champion.team, tf.GetPosition(), true);
                }));
        world.ForEach<MinionComponent, TransformComponent>(
            std::function<void(EntityID, MinionComponent&, TransformComponent&)>(
                [&](EntityID target, MinionComponent& minion, TransformComponent& tf)
                {
                    tryHit(target, minion.team, tf.GetPosition(), false);
                }));
```

이 변경은 W의 이미 동작하는 champion/minion 이중 loop를 E에도 적용한다. `CanReceiveEnemyAbilityHit`가 사망·아군·untargetable·비전투 대상을 거르고, minion에는 `GameplayStatus::ApplyStun`을 호출하지 않아 요구한 피해/표식만 부여한다.

`TickRWave(...)`의 wave-hit block에서 기존 `GameplayStatus::ApplySlow(...)` 바로 뒤, 그리고 wall-contact block에서 기존 `ApplyDisarm(...)` 바로 뒤에 각각 아래 호출을 넣는다. `rMarkDurationSec`은 `rWallDurationSec`과 별도로 R의 `MarkDurationSec`에서 한 번만 resolve한다.

```cpp
                AttachOrRefreshIreliaMark(world, tc, caster, hitTarget,
                    state.rRank, rMarkDurationSec);
```

```cpp
                    AttachOrRefreshIreliaMark(world, tc, caster, target,
                        state.rRank, rMarkDurationSec);
```

이 위치는 이미 R의 최초 hit 및 wall cross를 판정한 챔피언 전용 흐름 안이다. 기존 `kIreliaREffectStageHit`/`kIreliaREffectStageWallMark` FX event는 유지하되, 새 mark event가 실제 표식 수명과 clear를 담당한다.

`IreliaGameSim::Tick(CWorld&, const TickContext&)`의 아래 기존 코드 바로 위에 만료 처리를 추가한다.

```cpp
        world.ForEach<IreliaSimComponent, TransformComponent>(
```

아래에 추가:

```cpp
        TickIreliaMarks(world, tc);
```

마크 만료는 caster의 `IreliaSimComponent`가 제거돼도 계속 돌아야 하므로, 기존 caster-state loop 안이 아니라 그 앞에서 relation 전체를 순회한다.

`IreliaGameSim.h`의 아래 기존 선언 아래에는 lifecycle cleanup 선언만 추가한다.

```cpp
    void Tick(CWorld& world, const TickContext& tc);
```

아래에 추가:

```cpp
    void CancelRuntime(CWorld& world, EntityID caster);
```

`CancelRuntime`은 Viego가 Irelia possession을 끝낼 때 caster의 `IreliaSimComponent`와 source-owned `IreliaMarkComponent` relation을 제거한다. 이 함수에는 `TickContext`가 없으므로 별도 clear effect를 위조하지 않으며, 뒤의 full snapshot reconciliation이 stale visual을 제거한다.

### 1-12. C:/Users/user/Desktop/Winters/Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json

`eSkillEffectParamId::MarkDurationSec`와 `Tools/LoLData/Build-LoLDefinitionPack.py`의 `markDurationSec` mapping은 이미 존재한다. 따라서 source JSON의 Irelia E/R 두 항목에만 값을 넣고, generated `Data/LoL/ServerPrivate/Gameplay/SkillGameplayDefs.json` 및 `Server/Private/Data/Generated/LoLGameplayDefinitions.generated.cpp`는 직접 수정하지 않는다.

`skill.irelia.e`의 아래 기존 코드:

```json
        "baseDamage": 70.0,
        "damagePerRank": 30.0,
        "radius": 1.5,
```

아래로 교체:

```json
        "baseDamage": 70.0,
        "damagePerRank": 30.0,
        "markDurationSec": 5.0,
        "radius": 1.5,
```

`skill.irelia.r`의 아래 기존 코드:

```json
        "baseDamage": 250.0,
        "disarmDurationSec": 1.5,
```

아래로 교체:

```json
        "baseDamage": 250.0,
        "markDurationSec": 5.0,
        "disarmDurationSec": 1.5,
```

`5.0초`는 이번 계획의 기본안이다. 현재 client의 E/R `1.5f` cue lifetime은 stun/disarm visual의 하드코드일 뿐 gameplay mark duration 근거가 아니므로 재사용하지 않는다. 밸런스 owner가 다른 duration을 원하면 이 두 source 값만 함께 바꾸면 E/R/Q/FX/snapshot lifetime이 같은 서버 tick 기준으로 정렬된다.

### 1-13. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/ReplicatedEventComponent.h 및 C:/Users/user/Desktop/Winters/Shared/Schemas/Snapshot.fbs

`ReplicatedEventComponent.h`의 아래 기존 상수 바로 아래에 Irelia 전용 EffectTrigger ID를 추가한다.

```cpp
inline constexpr u32_t kEzrealEffectEssenceFluxClear = 0x455A5703u;
```

아래에 추가:

```cpp
inline constexpr u32_t kIreliaEffectMarkApply = 0x49524D01u;
inline constexpr u32_t kIreliaEffectMarkConsume = 0x49524D02u;
inline constexpr u32_t kIreliaEffectMarkClear = 0x49524D03u;
```

`EffectTriggerEvent` 자체는 source/target net override와 `durationMs`를 이미 가지므로 이 event를 위해 Event.fbs를 변경하지 않는다.

`Snapshot.fbs`의 아래 enum을 append-only로 바꾼다.

기존 코드:

```fbs
    EzrealEssenceFlux = 2,
    YasuoWindWall = 3
```

아래로 교체:

```fbs
    EzrealEssenceFlux = 2,
    YasuoWindWall = 3,
    IreliaMark = 4
```

기존 enum value는 절대 재번호 매기지 않는다. schema 변경 뒤에는 `Shared/Schemas/run_codegen.bat`로 C++/Go binding을 생성하며 generated binding은 직접 편집하지 않는다.

### 1-14. C:/Users/user/Desktop/Winters/Server/Private/Game/SnapshotBuilder.cpp

`EzrealEssenceFluxMarkComponent` row 생성 loop가 끝나는 바로 아래에 `IreliaMarkComponent` loop를 추가한다. anchor는 다음 다음 블록 직전이다.

```cpp
    const auto yasuoWindWalls =
```

추가 loop는 `DeterministicEntityIterator<IreliaMarkComponent>::CollectSorted(world)`를 사용하고, `uSourceNet/uTargetNet`이 0이거나 `uExpireTick <= serverTick`인 relation은 건너뛴다. 유효 relation은 아래 행만 만든다.

```cpp
        GameplayStateRow row{};
        row.kind = Shared::Schema::GameplayStateKind::IreliaMark;
        row.sourceNet = mark.uSourceNet;
        row.targetNet = mark.uTargetNet;
        row.expireTick = mark.uExpireTick;
        row.rank = mark.uRank;
        gameplayStateRows.push_back(row);
```

이는 event가 하나 유실되어도 다음 full snapshot이 살아 있는 표식과 남은 lifetime을 다시 만들어 주고, Q consume/만료가 event보다 먼저 도착해도 full snapshot에서 visual을 없애는 복구 경로다.

### 1-15. C:/Users/user/Desktop/Winters/Client/Public/Network/Client/EventApplier.h, C:/Users/user/Desktop/Winters/Client/Private/Network/Client/EventApplier.cpp 및 C:/Users/user/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp

`EventApplier.h`에서 Ezreal Flux declaration/field와 나란히 아래 항목을 추가한다.

```cpp
    void UpsertIreliaMarkSnapshot(CWorld& world, EntityIdMap& entityMap,
        NetEntityId uSourceNet, NetEntityId uTargetNet, u64_t uExpireTick);
    void DestroyIreliaMarkVisuals(CWorld& world, u64_t relationKey);

    std::unordered_map<u64_t, std::vector<EntityID>> m_ireliaMarkVisualEntities;
    std::unordered_map<u64_t, u64_t> m_ireliaMarkExpireTicks;
    std::unordered_set<u64_t> m_snapshotIreliaMarkKeys;
    std::unordered_map<u64_t, PresentationMutationStamp>
        m_ireliaMarkMutationStamps;
```

관계 key는 Ezreal과 동일하게 `(static_cast<u64_t>(sourceNet) << 32u) | targetNet`이다. source가 다른 Irelia의 동일 target은 별도 key가 된다.

`EventApplier.cpp`에는 아래 Ezreal Flux anchor들을 그대로 복제해 Irelia map을 추가한다.

- `CEventApplier::RebaseTimeline`: Irelia visual entity, expire map, mutation stamp, snapshot key를 함께 destroy/clear한다.
- `CEventApplier::BeginSnapshotReconciliation`: `m_snapshotIreliaMarkKeys.clear()`를 추가한다.
- `CEventApplier::UpsertEzrealFluxSnapshot(...)` 바로 아래: `UpsertIreliaMarkSnapshot(...)`을 추가한다. generic `TransformComponent` target을 `ResolveLiveEntity`로 찾고, cue는 기존 asset `"Irelia.Target.Mark"`을 `attachTo = target`, 서버 snapshot에서 계산한 남은 lifetime으로 재생한다. 챔피언 type cast를 하지 않으므로 표식된 미니언도 같은 path로 표시된다.
- `CEventApplier::EndSnapshotReconciliation`: full snapshot에 없는 Irelia relation key를 mutation ordering 검사를 통과한 뒤 `DestroyIreliaMarkVisuals`로 제거한다.
- `CEventApplier::ApplyEffectTrigger(...)`: Ezreal Flux special branch 바로 뒤, generic Irelia visual hook dispatch보다 앞에 세 effect ID branch를 둔다. apply는 `TryAdvanceMutation(... SpawnOrMark)` 성공 시 현재 FX를 교체/갱신하고, consume/clear는 `ContactOrClear`로 현재 FX만 제거한다. `m_seenEffectCueKeys`도 사용해 중복 event로 cue를 두 번 만들지 않는다.

apply/consume/clear branch는 Debug에서 64회 이하의 `[IreliaMark][Client]` trace를 `OutputDebugStringA`로 남긴다. 기존 `IreliaReplayCue` buffer는 만든 뒤 출력하지 않는 구간이 있으므로, 이 검증에 의존하지 않는다.

`SnapshotApplier.cpp`의 아래 existing switch case 뒤에 Irelia case를 추가한다.

```cpp
            case Shared::Schema::GameplayStateKind::EzrealEssenceFlux:
                if (m_pEventApplier)
                {
                    m_pEventApplier->UpsertEzrealFluxSnapshot(
                        world,
                        entityMap,
                        state->sourceNet(),
                        state->targetNet(),
                        state->expireTick());
                }
                break;
```

아래에 추가:

```cpp
            case Shared::Schema::GameplayStateKind::IreliaMark:
                if (m_pEventApplier)
                {
                    m_pEventApplier->UpsertIreliaMarkSnapshot(
                        world,
                        entityMap,
                        state->sourceNet(),
                        state->targetNet(),
                        state->expireTick());
                }
                break;
```

Q cooldown UI는 이 파일의 기존 `skillCooldowns`/`skillCooldownDurations` snapshot 적용 코드가 이미 담당한다. 이 계획에는 UI state를 직접 0으로 만드는 client patch를 넣지 않는다.

### 1-16. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Irelia/Irelia_Skills.cpp, C:/Users/user/Desktop/Winters/Client/Public/GameObject/Champion/Irelia/Irelia_Skills.h 및 C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameLocalSkills.cpp

삭제 범위:

```cpp
    bool_t PlayTargetMarkCue(CWorld& world, EntityID target, f32_t lifetime)
```

부터 `ApplyEConnectHitTargets(...)` 함수 끝까지를 삭제한다. 이 두 함수는 champion만 직접 스캔해 local stun/FX를 만들므로 서버 E minion 판정과 무관하고, normal network 권위를 흐린다.

`OnCastAccepted_E`의 아래 기존 call을 삭제한다. `PlayEConnectCue`와 blade visual은 남긴다.

```cpp
                ApplyEConnectHitTargets(*ctx.pWorld,
                    ctx.casterEntity,
                    ctx.casterTeam,
                    p1,
                    p2,
                    1.5f,
                    t.bladeStunSec,
                    ctx.applyTargetDamage ? true : false);
```

`UpdateLocalBladeState`의 동일 `ApplyEConnectHitTargets(...)` call도 삭제한다. 따라서 `Irelia_Skills.h`와 cpp definition의 `bool_t bApplyLocalGameplay` parameter, 그리고 `Scene_InGameLocalSkills.cpp`의 마지막 `!m_bNetworkAuthoritativeGameplay` argument도 함께 삭제한다.

`Visual::OnCastAccepted_R_Visual`에서 stage 2 및 stage 4의 아래 call만 삭제한다. R pulse/hit/wall visual은 유지한다.

```cpp
                PlayTargetMarkCue(*ctx.pWorld,
                    ctx.pCommand ? ctx.pCommand->targetEntityId : NULL_ENTITY,
                    1.5f);
```

표식 표시/제거는 이후 `EventApplier`의 서버 EffectTrigger와 snapshot reconciliation만 수행한다. 따라서 E/R mark lifetime이 stun/disarm의 1.5초에 묶이지 않고, Q consume event가 도착했을 때만 사라진다. 기존 `Data/LoL/FX/Champions/Irelia/target_mark.wfx`는 target attach emitter이므로 asset 수정 없이 재사용한다.

### 1-17. C:/Users/user/Desktop/Winters/Shared/GameSim/Core/Checkpoint/WorldKeyframe.cpp, C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomCommands.cpp 및 C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Viego/ViegoGameSim.cpp

`WorldKeyframe.cpp`의 아래 기존 버전을 한 단계 올린다.

```cpp
    constexpr u64_t kKeyframeVersion = 5ull;
```

아래로 교체:

```cpp
    constexpr u64_t kKeyframeVersion = 6ull;
```

Irelia mark store가 keyframe registry에 새로 들어가므로 구 버전의 store count/payload를 새 format으로 잘못 읽지 않게 한다. component 등록 자체는 1-11의 owner TU self-registration으로 한 번만 한다.

`GameRoomCommands.cpp`의 `AppendReplacementCleanupEntities<EzrealEssenceFluxMarkComponent>(...)` block 바로 아래에 같은 predicate의 `AppendReplacementCleanupEntities<IreliaMarkComponent>` block을 추가한다.

```cpp
        AppendReplacementCleanupEntities<IreliaMarkComponent>(
            world, entities,
            [&](const IreliaMarkComponent& value)
            {
                return value.hSource == sourceHandle ||
                    value.hTarget == sourceHandle;
            });
```

그리고 아래 기존 `fluxMarks` deterministic cleanup loop 바로 뒤에 `IreliaMarkComponent`의 동일 source/target-handle cleanup loop를 추가한다. replacement/death 뒤에 dangling relation이 남지 않으며, client는 이후 full snapshot reconciliation으로 stale mark FX를 정리한다.

`ViegoGameSim.cpp`의 아래 existing case는 source-owned mark가 possession 종료 뒤에도 남을 수 있으므로 직접 component removal 대신 owner cleanup으로 교체한다.

기존 코드:

```cpp
        case eChampion::IRELIA: RemoveBorrowedComponent<IreliaSimComponent>(world, caster); break;
```

아래로 교체:

```cpp
        case eChampion::IRELIA: IreliaGameSim::CancelRuntime(world, caster); break;
```

이 범위는 Viego possession 경계만 정리하며, Viego의 spellbook/skill rank restore를 바꾸지 않는다. `CancelRuntime`의 removal 뒤 marker가 즉시 event로 clear되어야 한다는 별도 UX 요구가 생기면, 현재 `ClearBorrowedChampionRuntime`에 없는 `TickContext`를 전체 call chain에 전달하는 변경은 이 patch와 분리해 검토한다.

### 1-18. C:/Users/user/Desktop/Winters/Tools/SimLab/main.cpp

`RunEzrealProjectileAuthorityProbe`와 command-line option parsing을 anchor로 `RunIreliaMarkAuthorityProbe` 및 `--irelia-mark-only`를 추가한다. 새 파일은 만들지 않는다.

probe는 한 deterministic server world에서 아래를 assert한다.

- E 2타 선분 안의 적 champion과 적 minion의 HP가 모두 감소하고, 각각 `(Irelia, target)` mark relation이 생긴다. 아군·사망·untargetable 대상에는 damage/mark가 없다.
- E mark와 R direct hit/wall contact mark는 expire tick을 refresh한다. R은 champion-only라는 현행 domain을 유지한다.
- 다른 Irelia의 Q는 source가 다른 mark를 소비하지 못한다. 같은 Irelia의 Q는 mark entity를 파괴하고 Q `cooldownRemaining`/`cooldownDuration`을 모두 0으로 만들며, unmarked Q는 일반 cooldown을 유지한다.
- expire tick, stale source/target, entity replacement/death가 relation을 남기지 않는다. keyframe save/load 뒤에도 active mark가 보존된다.
- snapshot row와 effect event를 client path에 적용했을 때 apply/consume/clear, event drop 뒤 full snapshot 복구, timeline rebase가 `Irelia.Target.Mark`을 중복 생성하거나 고아 FX로 남기지 않는다.

## 2. 검증

미검증:

- 이번 세션은 조사·계획서 작성만 수행한다. 사용자 클라이언트/서버가 실행 중이므로 Ashe/Irelia 코드, WFX, schema, generated definition, 빌드는 수정·실행하지 않는다.

검증 명령:

```powershell
cmd /c .\Shared\Schemas\run_codegen.bat
python Tools/LoLData/Build-LoLDefinitionPack.py
python Tools/LoLData/Build-LoLDefinitionPack.py --check
PowerShell -NoProfile -ExecutionPolicy Bypass -File Tools\Harness\Check-SharedBoundary.ps1
git diff --check
MSBuild.exe Shared\GameSim\Include\GameSim.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
MSBuild.exe Server\Include\Server.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
MSBuild.exe Client\Include\Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
MSBuild.exe Tools\SimLab\SimLab.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
Tools\SimLab\x64\Debug\SimLab.exe --irelia-mark-only
```

수동 확인:

- normal F5 서버 권위 세션에서 BA/W/R을 각각 전방·후방·좌·우·대각선으로 발사한다. Debug Output의 `[AsheProjectileVisual]`에서 BA/W/R마다 `mesh=1`을 확인하고, W 한 번은 서로 다른 net ID 8개가 `mesh=1`, `billboard=1`로 기록되는지 확인한다.
- W의 8개 snapshot direction이 중앙을 기준으로 `-22.5°`부터 `+22.5°`까지 분포하고, 각 BA FBX long axis가 자신의 direction과 일치하는지 영상 캡처로 확인한다. 이때 W cast muzzle은 별도 cue이며 cone projectile을 다시 만들지 않는다.
- E를 좌/우 서로 다른 cursor 방향으로 두 번 시전한다. replicated EffectTrigger의 direction이 두 방향과 일치하고, `Ashe.E.Hawkshot`이 25 units를 `0.70s` 동안 이동하는지 확인한다. 새 E는 `MeshParticle` 하나만 생성하며, 서버에 projectile, damage, status, vision/reveal entity가 새로 생기지 않아야 한다.
- R은 현재 `r_arrow.wfx`의 BA FBX scale `0.063`을 유지한다. BA/W `0.021` 대비 E/R 모두 정확히 3x이고, R의 ice/smoke billboard trail은 보조 효과로만 남으며 3D arrow mesh의 방향을 가리지 않는지 확인한다.
- `FxCuePlayer`의 skipped-mesh 경고 또는 model preload 실패가 나오면 catalog yaw를 재조정하지 말고 먼저 renderer 주입(`Scene_InGameLifecycle`)과 `fx.ashe.base_arrow` preload 결과를 고친다. `FxMesh::Drawn` profiler counter도 동일 구간에서 증가해야 한다.
- Irelia E 2타가 적 champion과 적 minion을 동시에 가를 때 server Debug `[IreliaMark] apply`가 target net별로 한 번씩 나오고, 두 HP가 떨어지는지 확인한다. champion만 stun되고 minion에는 stun component가 생기지 않아야 한다.
- E/R 이후 mark는 server definition의 `markDurationSec` 동안 유지되고, client `[IreliaMark][Client]` apply와 `Irelia.Target.Mark` FX가 target transform을 따라가는지 확인한다. event를 끊은 뒤 full snapshot을 주입해도 남은 lifetime으로 재생성되어야 한다.
- 같은 caster가 mark된 대상에 Q를 쓰면 한 tick에 server consume/reset, client clear, 다음 snapshot의 Q cooldown 두 값 0이 모두 보이는지 확인한다. unmarked target과 다른 Irelia가 남긴 mark target에는 Q cooldown reset이 일어나면 안 된다.
- R wave 직접 hit와 R wall cross 모두 actual mark relation을 만들되, R line에 있는 minion에는 새 R damage/mark가 추가되지 않는지 확인한다.
- entity replacement/death, keyframe restore, Viego possession 종료 후 stale Irelia relation/FX가 남지 않는지 SimLab과 normal F5 capture 양쪽에서 확인한다.

확인 필요:

- `Data/Gameplay/ChampionGameData/champions.json`의 Ashe E source targetMode는 현재 `Conditional`이지만, manual client registration은 `Direction`이다. normal F5 capture에서 E EffectTrigger direction이 nonzero이면 이번 visual 변경만 적용한다.
- E direction이 0으로 재현되는 bot/remote 경로가 있으면, generated `Data/LoL/ServerPrivate/Gameplay/SkillGameplayDefs.json`을 직접 고치지 않는다. 사용자 승인 후 source `champions.json`의 slot 3 `"targetMode": "Conditional"`을 `"targetMode": "Direction"`으로 바꾸고 `Build-LoLDefinitionPack.py`를 다시 실행해 서버/client definition contract를 함께 정렬한다. 이 변경은 target semantics를 바꾸므로 별도 authority 검증이 필요하다.
