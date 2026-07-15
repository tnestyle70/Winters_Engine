Session - 야스오 E 중 Q 입력을 서버 권위 stage-4 EQ로 이어 붙이고, 독립 WFX 3종과 네트워크 아군 미니언 시야를 충돌 없이 반영한다.

## 1. 반영해야 하는 코드

### 1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Yasuo/YasuoGameSim.h

기존 전방 선언 아래에 추가:

```cpp
struct GameCommand;
class ICommandExecutor;
```

`namespace YasuoGameSim` 안의 기존 선언을 아래로 교체:

```cpp
    bool_t TryBufferQDuringE(
        CWorld& world,
        const TickContext& tc,
        const GameCommand& cmd);
    u8_t ResolveQVariantStage(CWorld& world, EntityID caster);
    void RegisterQHit(CWorld& world, const TickContext& tc, EntityID caster, eProjectileKind kind);
    EntityID FindAirborneTarget(CWorld& world, EntityID caster, eTeam casterTeam, f32_t radius);
    void ApplyTornadoAirborne(CWorld& world, const TickContext& tc, EntityID source, EntityID target);
    void RegisterHooks();
    void Tick(CWorld& world, const TickContext& tc, ICommandExecutor* pExecutor = nullptr);
    void CancelRuntime(CWorld& world, EntityID caster);
```

### 1-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Yasuo/YasuoGameSim.cpp

컴포넌트 include 영역에 추가:

```cpp
#include "Shared/GameSim/Components/ActionStateComponent.h"
```

표준 라이브러리 include 영역에 추가:

```cpp
#include <cstdio>
#include <type_traits>
```

anonymous namespace의 `YasuoDashComponent` 아래에 추가:

```cpp
    struct YasuoEqInputBufferComponent
    {
        EntityHandle hCaster = NULL_ENTITY_HANDLE;
        Vec3 vDirection{};
        Vec3 vGroundPos{};
        u64_t uIssuedAtTick = 0u;
        u64_t uRewindTicks = 0u;
        u32_t uCommandSequence = 0u;
        u32_t uSourceSessionId = 0u;
        u32_t uEActionSequence = 0u;
        EntityID uTargetEntity = NULL_ENTITY;
        u16_t uItemId = 0u;
        bool_t bPending = false;
        bool_t bExecuting = false;
        u8_t reservedTail[4]{};
    };

    static_assert(std::is_trivially_copyable_v<YasuoEqInputBufferComponent>);
    static_assert(sizeof(YasuoEqInputBufferComponent) == 72u);
```

기존 `s_bYasuoDashKeyframeRegistered` 등록 블록에서 `ProjectileBarrierComponent` 등록 아래에 추가:

```cpp
        SimCheckpoint::KeyframeComponentRegistry::Get()
            .Register<YasuoEqInputBufferComponent>("YasuoEqInputBufferComponent");
```

anonymous namespace의 helper 영역에 추가:

```cpp
    void ClearYasuoEqInputBuffer(CWorld& world, EntityID caster)
    {
        if (world.HasComponent<YasuoEqInputBufferComponent>(caster))
            world.RemoveComponent<YasuoEqInputBufferComponent>(caster);
    }

    bool_t IsYasuoEAction(const ActionStateComponent& action)
    {
        return action.sourceChampion == eChampion::YASUO &&
            action.sourceSlot == static_cast<u8_t>(eSkillSlot::E) &&
            action.movePolicy == eSkillActionMovePolicy::ForcedMotion;
    }
```

`namespace YasuoGameSim`의 `CancelRuntime` 위에 추가:

```cpp
    bool_t TryBufferQDuringE(
        CWorld& world,
        const TickContext& tc,
        const GameCommand& cmd)
    {
        if (cmd.kind != eCommandKind::CastSkill ||
            cmd.slot != static_cast<u8_t>(eSkillSlot::Q) ||
            cmd.itemId > 1u ||
            cmd.issuerEntity == NULL_ENTITY ||
            !world.IsAlive(cmd.issuerEntity) ||
            !world.HasComponent<ChampionComponent>(cmd.issuerEntity) ||
            world.GetComponent<ChampionComponent>(cmd.issuerEntity).id != eChampion::YASUO ||
            !world.HasComponent<ActionStateComponent>(cmd.issuerEntity) ||
            !world.HasComponent<YasuoStateComponent>(cmd.issuerEntity))
        {
            return false;
        }

        const ActionStateComponent& action =
            world.GetComponent<ActionStateComponent>(cmd.issuerEntity);
        if (!IsYasuoEAction(action) ||
            tc.tickIndex >= action.lockEndTick ||
            !world.GetComponent<YasuoStateComponent>(cmd.issuerEntity).bEActive)
        {
            return false;
        }

        YasuoEqInputBufferComponent& buffer =
            world.HasComponent<YasuoEqInputBufferComponent>(cmd.issuerEntity)
                ? world.GetComponent<YasuoEqInputBufferComponent>(cmd.issuerEntity)
                : world.AddComponent<YasuoEqInputBufferComponent>(
                    cmd.issuerEntity,
                    YasuoEqInputBufferComponent{});
        if (buffer.bPending || buffer.bExecuting)
            return true;

        buffer = {};
        buffer.hCaster = world.GetEntityHandle(cmd.issuerEntity);
        buffer.vDirection = cmd.direction;
        buffer.vGroundPos = cmd.groundPos;
        buffer.uIssuedAtTick = cmd.issuedAtTick;
        buffer.uRewindTicks = cmd.rewindTicks;
        buffer.uCommandSequence = cmd.sequenceNum;
        buffer.uSourceSessionId = cmd.sourceSessionId;
        buffer.uEActionSequence = action.sequence;
        buffer.uTargetEntity = cmd.targetEntity;
        buffer.uItemId = cmd.itemId;
        buffer.bPending = true;
        return true;
    }
```

`ResolveQVariantStage` 함수 첫 부분에 추가:

```cpp
        if (world.HasComponent<YasuoEqInputBufferComponent>(caster) &&
            world.GetComponent<YasuoEqInputBufferComponent>(caster).bExecuting)
        {
            return 4;
        }
```

`Tick` 시그니처를 아래로 교체:

```cpp
    void Tick(CWorld& world, const TickContext& tc, ICommandExecutor* pExecutor)
```

`finishedDashes` 제거 루프 아래, `YasuoStateComponent` 타이머 루프 위에 추가:

```cpp
        std::vector<EntityID> invalidEqBuffers;
        std::vector<EntityID> readyEqBuffers;
        world.ForEach<YasuoEqInputBufferComponent>(
            std::function<void(EntityID, YasuoEqInputBufferComponent&)>(
                [&](EntityID entity, YasuoEqInputBufferComponent& buffer)
                {
                    if (!buffer.bPending ||
                        !buffer.hCaster.IsValid() ||
                        !world.IsAlive(buffer.hCaster) ||
                        buffer.hCaster.GetIndex() != entity ||
                        !world.HasComponent<ChampionComponent>(entity) ||
                        world.GetComponent<ChampionComponent>(entity).id != eChampion::YASUO ||
                        !world.HasComponent<ActionStateComponent>(entity) ||
                        !GameplayStateQuery::CanCast(world, entity))
                    {
                        invalidEqBuffers.push_back(entity);
                        return;
                    }

                    const ActionStateComponent& action =
                        world.GetComponent<ActionStateComponent>(entity);
                    if (!IsYasuoEAction(action) ||
                        action.sequence != buffer.uEActionSequence)
                    {
                        invalidEqBuffers.push_back(entity);
                        return;
                    }
                    if (tc.tickIndex < action.lockEndTick ||
                        world.HasComponent<YasuoDashComponent>(entity) ||
                        !pExecutor)
                    {
                        return;
                    }
                    readyEqBuffers.push_back(entity);
                }));

        for (EntityID entity : invalidEqBuffers)
            ClearYasuoEqInputBuffer(world, entity);

        std::sort(readyEqBuffers.begin(), readyEqBuffers.end());
        for (EntityID entity : readyEqBuffers)
        {
            if (!world.HasComponent<YasuoEqInputBufferComponent>(entity))
                continue;

            YasuoEqInputBufferComponent& buffer =
                world.GetComponent<YasuoEqInputBufferComponent>(entity);
            buffer.bPending = false;
            buffer.bExecuting = true;

            GameCommand q{};
            q.kind = eCommandKind::CastSkill;
            q.issuerEntity = entity;
            q.issuedAtTick = buffer.uIssuedAtTick;
            q.sequenceNum = buffer.uCommandSequence;
            q.rewindTicks = buffer.uRewindTicks;
            q.slot = static_cast<u8_t>(eSkillSlot::Q);
            q.targetEntity = buffer.uTargetEntity;
            q.groundPos = buffer.vGroundPos;
            q.direction = buffer.vDirection;
            q.itemId = buffer.uItemId;
            q.sourceSessionId = buffer.uSourceSessionId;
            pExecutor->ExecuteCommand(world, tc, q);
            ClearYasuoEqInputBuffer(world, entity);
        }
```

### 1-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp

기존 코드:

```cpp
    if (IsCommandBlockedByAuthoritativeAction(world, tc, cmd))
    {
        return FinalizeChampionAICommandTrace(
            world,
            cmd,
            CommandExecutionResult::Rejected(
                cmd.sequenceNum,
                eCommandExecutionReason::ActionBlocked));
    }
```

아래로 교체:

```cpp
    if (IsCommandBlockedByAuthoritativeAction(world, tc, cmd))
    {
        if (YasuoGameSim::TryBufferQDuringE(world, tc, cmd))
            return CommandExecutionResult::Accepted(cmd.sequenceNum);

        return FinalizeChampionAICommandTrace(
            world,
            cmd,
            CommandExecutionResult::Rejected(
                cmd.sequenceNum,
                eCommandExecutionReason::ActionBlocked));
    }
```

첫 입력에서는 AI cadence/쿨다운/마나/ActionState를 확정하지 않는다. 실제 release 때 정상 executor가 같은 sequence를 한 번 finalize한다.

### 1-4. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomTick.cpp

기존 코드:

```cpp
    YasuoGameSim::Tick(m_world, tc);
```

아래로 교체:

```cpp
    YasuoGameSim::Tick(m_world, tc, m_pExecutor.get());
```

### 1-5. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Yasuo/YasuoFxPresets.cpp

cue 상수 영역에 추가:

```cpp
    constexpr const char* kCueEDashRing = "Yasuo.E.DashRing";
    constexpr const char* kCueEQWindRing = "Yasuo.EQ.WindRing";
```

`SpawnEDashTrail`의 마지막 줄을 아래로 교체:

```cpp
    const EntityID trail = CFxCuePlayer::Play(world, kCueEDashTrail, cue);
    const EntityID ring = CFxCuePlayer::Play(world, kCueEDashRing, cue);
    return trail != NULL_ENTITY ? trail : ring;
```

`SpawnEQRing`의 기존 `kCueEQInnerWind` 재생 아래에 추가:

```cpp
    CFxCuePlayer::Play(world, kCueEQWindRing, glow);
```

### 1-6. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Projectile/ProjectileVisualCatalog.cpp

Yasuo projectile visual 상수로 추가:

```cpp
    constexpr ProjectileVisualDesc kYasuoTornadoVisual{
        nullptr, nullptr, "Yasuo.Q.TornadoHit"
    };
```

기존 코드:

```cpp
        case eProjectileKind::Wind:
        case eProjectileKind::Tornado:
        case eProjectileKind::EQRing:
            return kNoProjectileVisual;
```

아래로 교체:

```cpp
        case eProjectileKind::Wind:
        case eProjectileKind::EQRing:
            return kNoProjectileVisual;
        case eProjectileKind::Tornado:
            return kYasuoTornadoVisual;
```

### 1-7. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp

`ToSnapshotMinionTeam` 아래에 추가:

```cpp
    void EnsureSnapshotMinionVisionRuntime(
        CWorld& world,
        EntityID entity,
        u8_t team,
        u16_t subtype)
    {
        if (entity == NULL_ENTITY || !world.IsAlive(entity))
            return;

        const u8_t role = subtype <= kGameSimMinionRoleTibbers
            ? static_cast<u8_t>(subtype)
            : kGameSimMinionRoleMelee;

        SpatialAgentComponent& spatial =
            world.HasComponent<SpatialAgentComponent>(entity)
                ? world.GetComponent<SpatialAgentComponent>(entity)
                : world.AddComponent<SpatialAgentComponent>(entity, SpatialAgentComponent{});
        spatial.kind = eSpatialKind::Unit;
        spatial.team = team;
        spatial.radius = 0.5f;

        VisionSourceComponent& vision =
            world.HasComponent<VisionSourceComponent>(entity)
                ? world.GetComponent<VisionSourceComponent>(entity)
                : world.AddComponent<VisionSourceComponent>(entity, VisionSourceComponent{});
        vision.sightRange = ResolveMinionCombatDef(role).sightRange;

        if (!world.HasComponent<VisibilityComponent>(entity))
            world.AddComponent<VisibilityComponent>(entity, VisibilityComponent{});
    }
```

`if (kind == Shared::Schema::EntityKind::Minion)` 블록 첫 줄에 추가:

```cpp
            EnsureSnapshotMinionVisionRuntime(
                world,
                e,
                es->team(),
                es->subtype());
```

### 1-8. 새 파일: C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Yasuo/e_dash_ring.wfx

```json
{
  "schema": "WintersWfx",
  "version": 1,
  "name": "Yasuo.E.DashRing",
  "emitters": [
    {
      "name": "e_dash_ring_shrink",
      "render_type": "ShockwaveRing",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Yasuo/particles/yasuo_base_e_dash_ring.png",
      "lifetime": 0.25,
      "fade_in": 0.01,
      "fade_out": 0.10,
      "width": 2.30,
      "height": 2.30,
      "start_radius": 1.15,
      "end_radius": 0.18,
      "color": [0.52, 0.94, 0.88, 0.78],
      "attach_offset": [0.0, 0.10, 0.0],
      "atlas_cols": 2,
      "atlas_rows": 2,
      "atlas_frame_count": 4,
      "atlas_fps": 16.0,
      "atlas_loop": false,
      "alpha_clip": 0.0,
      "billboard": false,
      "depth_write": false
    }
  ]
}
```

### 1-9. 새 파일: C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Yasuo/eq_wind_ring.wfx

```json
{
  "schema": "WintersWfx",
  "version": 1,
  "name": "Yasuo.EQ.WindRing",
  "emitters": [
    {
      "name": "eq_wind_ring_rgba",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Yasuo/particles/yasuo_basic_attack_wind_ring_02.png",
      "lifetime": 0.60,
      "fade_in": 0.02,
      "fade_out": 0.30,
      "width": 4.70,
      "height": 4.70,
      "color": [0.48, 0.88, 0.96, 0.66],
      "attach_offset": [0.0, 1.00, 0.0],
      "alpha_clip": 0.0,
      "billboard": true,
      "depth_write": false
    }
  ]
}
```

### 1-10. 새 파일: C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Yasuo/q_tornado_hit.wfx

```json
{
  "schema": "WintersWfx",
  "version": 1,
  "name": "Yasuo.Q.TornadoHit",
  "emitters": [
    {
      "name": "q_tornado_hit_blast",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Yasuo/particles/yasuo_q_tornado_blast.png",
      "lifetime": 0.34,
      "fade_in": 0.01,
      "fade_out": 0.20,
      "width": 2.45,
      "height": 2.45,
      "color": [0.72, 0.96, 1.00, 0.82],
      "attach_offset": [0.0, 1.10, 0.0],
      "alpha_clip": 0.0,
      "billboard": true,
      "depth_write": false
    }
  ]
}
```

## 2. 검증

1. `e_dash_trail.wfx`의 SHA-256과 수정 시각이 본 packet 시작 시점과 달라도 덮어쓰지 않았는지 확인한다.
2. 새 WFX 3개를 JSON parser로 읽고 cue 이름이 유일하며 세 texture 절대 소스가 실제 존재하는지 확인한다.
3. Shared/GameSim, Server, Client 변경 TU를 현재 Debug x64 include/define 환경에서 `/Zs` 또는 isolated `ClCompile`로 통과시킨다.
4. 정상 출력물을 덮어쓰지 않는 isolated OutDir/IntDir로 관련 프로젝트를 빌드한다. 현재 실행 중인 Client/Server는 종료하지 않는다.
5. E 시작 T, Q 입력 T+1에서 Q 쿨다운/피해/FX가 즉시 생기지 않고 입력만 Accepted되는지 Debug trace로 확인한다. 반복 Q도 한 개만 유지한다.
6. T+12에서 E dash와 action lock이 모두 끝난 뒤 Q ActionState stage 4, `spell1c`, EQ AoE, EffectTrigger가 정확히 한 번 발생하는지 확인한다. 사망, CC, 다른 action sequence로 교체되면 버퍼가 폐기되어야 한다.
7. E ring이 Yasuo를 따라가며 2x2 atlas를 한 번 재생하고 radius `1.15 -> 0.18`로 축소되는지 확인한다. EQ wind ring은 WFX `color/width/height/lifetime` 변경만으로 즉시 튜닝 가능해야 한다.
8. 토네이도가 여러 적을 관통할 때 각 `contactOrdinal`/대상당 `Yasuo.Q.TornadoHit`가 한 번 붙고, 이동 중 토네이도 spawn visual은 중복되지 않는지 확인한다.
9. Blue 플레이어가 떨어져 있고 Blue 미니언만 전진하는 장면에서 다음 Vision 100 ms tick 내 role별 12/14/16 m FoW가 열리는지 확인한다. Red 미니언은 Blue FoW를 열면 안 되며, 제어 팀 전환 후 반대도 동일해야 한다.
10. 같은 5v5 장면 전후 profiler에서 `Vision::TickVisibility` EMA 증가 `<= 0.10 ms`, `Vision::UpdateFow` EMA 증가 `<= 0.15 ms`, FOW max `<= 1 ms`를 게이트로 삼는다.
11. scoped `git diff --check`를 통과하고 결과를 `C:/Users/user/Desktop/Winters/.md/build/2026-07-14_YASUO_EQ_FX_AND_ALLIED_MINION_VISION_REPORT.md`에 갱신한다.
