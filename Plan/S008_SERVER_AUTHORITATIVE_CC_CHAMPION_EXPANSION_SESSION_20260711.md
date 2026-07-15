Session - Shared/GameSim의 단일 서버 권위 상태이상·강제이동·대상 질의를 완결하고, 요청된 챔피언 CC·사일러스 궁극기 탈취·티버 명령을 150 챔피언 확장 가능한 데이터 경계로 연결한다. 현재 코드는 StatusEffectComponent까지는 존재하지만 stack key, 미니언 AI, Snapshot, 피해자 forced motion, mobile-target 분류가 끊겨 있으므로 이 공통 경로를 먼저 고친 뒤 챔피언별 hook을 연결한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/GameplayComponents.h

`struct GameplayStateComponent` 바로 아래에 추가:

```cpp
enum class eForcedMotionKind : uint8_t
{
    None = 0,
    AirborneArc,
    GatherAirborneArc,
};

struct ForcedMotionComponent
{
    eForcedMotionKind kind = eForcedMotionKind::None;
    EntityID sourceEntity = NULL_ENTITY;
    Vec3 start{};
    Vec3 end{};
    f32_t fElapsedSec = 0.f;
    f32_t fDurationSec = 0.f;
    f32_t fArcHeight = 0.f;
};
```

`IsSameStack`가 효과 종류와 source를 함께 비교할 수 있도록 기존 `StatusEffectApplyDesc`와 `StatusEffectInstance`의 `stackGroup`은 유지한다. 신규 별도 상태 스크립트 컴포넌트는 만들지 않고, 공통 상태와 파생 gameplay state의 단일 소유권을 이 파일에 둔다.

1-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.h

기존 코드:

```cpp
void ApplyStatusEffect(CWorld& world, EntityID target,
    const StatusEffectApplyDesc& desc);
void ApplyStatusEffect(CWorld& world, EntityID target,
    const StatusEffectApplyDesc& desc,
    const TickContext& tc);
void TickStatusEffects(CWorld& world, const TickContext& tc);
void RebuildGameplayState(CWorld& world, EntityID entity);
```

아래로 교체:

```cpp
bool_t TryApplyStatusEffect(CWorld& world, EntityID target,
    const StatusEffectApplyDesc& desc);
bool_t TryApplyStatusEffect(CWorld& world, EntityID target,
    const StatusEffectApplyDesc& desc,
    const TickContext& tc);
void ApplyStatusEffect(CWorld& world, EntityID target,
    const StatusEffectApplyDesc& desc);
void ApplyStatusEffect(CWorld& world, EntityID target,
    const StatusEffectApplyDesc& desc,
    const TickContext& tc);
bool_t StartAirborneMotion(CWorld& world, EntityID target,
    EntityID source, const Vec3& landingPosition,
    f32_t durationSec, f32_t arcHeight,
    bool_t bGatherToLanding);
void TickStatusEffects(CWorld& world, const TickContext& tc);
void TickForcedMotions(CWorld& world, const TickContext& tc);
void ClearStatusEffects(CWorld& world, EntityID entity);
void RebuildGameplayState(CWorld& world, EntityID entity);
```

1-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.cpp

기존 코드:

```cpp
if (desc.stackGroup != 0u)
    return effect.stackGroup == desc.stackGroup;
return effect.effectId == desc.effectId &&
    effect.sourceEntity == desc.sourceEntity;
```

아래로 교체:

```cpp
if (desc.stackGroup != 0u)
{
    return effect.stackGroup == desc.stackGroup &&
        effect.effectId == desc.effectId &&
        effect.sourceEntity == desc.sourceEntity;
}
return effect.effectId == desc.effectId &&
    effect.sourceEntity == desc.sourceEntity;
```

`ApplyStatusEffectInternal`의 기존 유효성 검사 아래에 추가:

```cpp
if (!GameplayStateQuery::CanReceiveCrowdControl(world, desc.sourceEntity, target))
    return false;
```

`TickStatusEffects` 아래에 추가할 `TickForcedMotions`는 `DeterministicEntityIterator<ForcedMotionComponent, TransformComponent>` 순서로 실행하며, `sin(pi * t) * arcHeight`로 Y를 계산하고 gather 종류만 XZ를 start에서 end로 보간한다. 종료 시 정확히 end 위치로 복원하고 component를 제거한다. 자발 이동 잠금과 강제 이동은 분리하며, forced motion은 `CannotMove` 중에도 계속 진행한다.

1-4. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/StatusEffect/StatusEffectRequests.h

기존 `ApplyAirborne` 두 overload를 아래 정책으로 교체:

```cpp
inline bool_t ApplyAirborne(
    CWorld& world,
    const TickContext& tc,
    EntityID target,
    EntityID source,
    eChampion champion,
    eSkillSlot slot,
    f32_t durationSec,
    f32_t arcHeight = 2.1f,
    const Vec3* landingPosition = nullptr)
{
    if (!TryApplyStatusEffect(
        world,
        target,
        MakeAirborneDesc(source, champion, slot, durationSec),
        tc))
    {
        return false;
    }

    Vec3 landing{};
    if (world.HasComponent<TransformComponent>(target))
        landing = world.GetComponent<TransformComponent>(target).position;
    if (landingPosition)
        landing = *landingPosition;
    return StartAirborneMotion(
        world,
        target,
        source,
        landing,
        durationSec,
        arcHeight,
        landingPosition != nullptr);
}
```

TickContext 없는 overload도 동일한 원자적 경로를 사용하되 feedback만 생략한다. 챔피언별 private airborne component는 제거하고 이 API만 사용한다.

1-5. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h

`namespace GameplayStateQuery` 안의 `CanReceiveProjectileHit` 아래에 추가:

```cpp
enum class eGameplayTargetKind : uint8_t
{
    None = 0,
    Champion,
    MinionOrSummon,
    JungleMonster,
    Structure,
};

eGameplayTargetKind ResolveTargetKind(CWorld& world, EntityID entity);
bool_t IsMobileCombatUnit(CWorld& world, EntityID entity);
bool_t CanReceiveCrowdControl(CWorld& world, EntityID source, EntityID target);
std::vector<EntityID> CollectEnemyMobileUnitsInCircle(
    CWorld& world, EntityID source, const Vec3& center, f32_t radius);
std::vector<EntityID> CollectEnemyMobileUnitsInSegment(
    CWorld& world, EntityID source, const Vec3& start,
    const Vec3& end, f32_t halfWidth);
std::vector<EntityID> CollectEnemyMobileUnitsInCone(
    CWorld& world, EntityID source, const Vec3& origin,
    const Vec3& direction, f32_t range, f32_t halfAngleRad);
```

필요 include로 `<vector>`와 Vec3 선언을 추가한다.

1-6. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.cpp

`ResolveEntityTeam` 아래에 추가하는 대상 분류는 다음 순서를 고정한다.

```cpp
if (world.HasComponent<ChampionComponent>(entity))
    return eGameplayTargetKind::Champion;
if (world.HasComponent<JungleComponent>(entity))
    return eGameplayTargetKind::JungleMonster;
if (world.HasComponent<MinionComponent>(entity) ||
    world.HasComponent<MinionStateComponent>(entity))
    return eGameplayTargetKind::MinionOrSummon;
if (world.HasComponent<StructureComponent>(entity))
    return eGameplayTargetKind::Structure;
return eGameplayTargetKind::None;
```

`CanReceiveCrowdControl`은 alive, mobile kind, enemy/neutral-hostile, targetable/untargetable을 한 번만 검사한다. Circle/Segment/Cone 수집은 `TransformComponent`를 deterministic entity id 순서로 한 번 순회하고 구조물·ward·projectile·effect anchor를 제외한다. 챔피언 파일의 중복 Champion/Minion loop는 이 API 호출로 교체한다.

1-7. C:/Users/user/Desktop/Winters/Shared/Schemas/Snapshot.fbs

기존 마지막 필드:

```text
minionAttackRecoverySec:float;
```

아래에 추가:

```text
gameplayStateFlags:uint;
gameplayMoveSpeedMul:float = 1.0;
forcedMotionKind:ubyte;
forcedMotionRemainingSec:float;
```

기존 `stateFlags`는 dead/moving/attack/invisible/ViegoSoul/AI presentation 비트이므로 절대 재사용하지 않는다. FlatBuffers 호환성을 위해 반드시 테이블 끝에 append한다.

1-8. C:/Users/user/Desktop/Winters/Server/Private/Game/SnapshotBuilder.cpp

기존 코드:

```cpp
u32_t stateFlags = 0;
u32_t ownerNet = 0;
```

아래에 추가:

```cpp
u32_t gameplayStateFlags = 0u;
f32_t gameplayMoveSpeedMul = 1.f;
u8_t forcedMotionKind = 0u;
f32_t forcedMotionRemainingSec = 0.f;
```

기존 invisibility mapping 앞에 추가:

```cpp
if (world.HasComponent<GameplayStateComponent>(entity))
{
    const auto& gameplay = world.GetComponent<GameplayStateComponent>(entity);
    gameplayStateFlags = gameplay.stateFlags;
    gameplayMoveSpeedMul = gameplay.fMoveSpeedMul;
}
if (world.HasComponent<ForcedMotionComponent>(entity))
{
    const auto& motion = world.GetComponent<ForcedMotionComponent>(entity);
    forcedMotionKind = static_cast<u8_t>(motion.kind);
    forcedMotionRemainingSec =
        (std::max)(0.f, motion.fDurationSec - motion.fElapsedSec);
}
```

`CreateEntitySnapshot`의 마지막 기존 인자 `minionAttackRecoverySec` 뒤에 네 필드를 같은 순서로 전달한다. `AnnieTibbersComponent`의 owner entity도 `ownerNet`으로 변환한다.

1-9. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/ReplicatedStateComponent.h

`struct ReplicatedStateComponent`의 기존 `stateFlags` 아래에 추가:

```cpp
u32_t gameplayStateFlags = 0u;
f32_t gameplayMoveSpeedMul = 1.f;
eForcedMotionKind forcedMotionKind = eForcedMotionKind::None;
f32_t fForcedMotionRemainingSec = 0.f;
```

1-10. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp

기존 코드:

```cpp
replicatedState.stateFlags = es->stateFlags();
```

아래에 추가:

```cpp
replicatedState.gameplayStateFlags = es->gameplayStateFlags();
replicatedState.gameplayMoveSpeedMul = es->gameplayMoveSpeedMul();
replicatedState.forcedMotionKind =
    static_cast<eForcedMotionKind>(es->forcedMotionKind());
replicatedState.fForcedMotionRemainingSec = es->forcedMotionRemainingSec();

GameplayStateComponent gameplay{};
gameplay.stateFlags = es->gameplayStateFlags();
gameplay.fMoveSpeedMul = es->gameplayMoveSpeedMul();
if (!world.HasComponent<GameplayStateComponent>(e))
    world.AddComponent<GameplayStateComponent>(e, gameplay);
else
    world.GetComponent<GameplayStateComponent>(e) = gameplay;
```

이 복제값은 입력·애니메이션·HUD 표현에만 사용하고 클라이언트 피해/CC truth를 만들지 않는다.

1-11. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameLocalSkills.cpp

`IsPlayerNetworkMoveInputLocked`의 replicated action lock 검사 아래에 추가:

```cpp
if (m_World.HasComponent<GameplayStateComponent>(m_myChampion))
{
    const auto& gameplay = m_World.GetComponent<GameplayStateComponent>(m_myChampion);
    if ((gameplay.stateFlags & kGameplayStateCannotMoveFlag) != 0u)
        return true;
}
```

`DispatchSkillInput`과 기본 공격 intent도 각각 `CannotCast`, `CannotAttack`을 검사한다. 서버 거절이 최종 권위이며, 이 client gate는 잘못된 예측·선회·run animation만 막는다.

1-12. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

기존 legacy stun 판정:

```cpp
State.bStunned = IsPlayerStunned();
```

`IsPlayerStunned` 구현을 `GameplayStateComponent`의 `Stunned | Airborne | CannotMove` 우선, legacy `StunComponent`는 local-only smoke fallback으로 교체한다. 정상 network F5에서 `PendingHitSystem`과 Yasuo client damage가 gameplay truth를 만들지 않도록 network authoritative 분기를 명시한다.

1-13. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomTick.cpp

기존 코드:

```cpp
GameplayStatus::TickStatusEffects(m_World, tc);
```

아래에 추가:

```cpp
GameplayStatus::TickForcedMotions(m_World, tc);
```

forced motion은 미니언 AI와 일반 이동보다 먼저 적용하며, 이후 AI는 해당 entity의 자발 이동을 건너뛴다.

1-14. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomUnitAI.cpp

`CGameRoom::Phase_ServerUnitAI`의 공격 진행·새 공격 시작·이동 블록에 각각 추가:

```cpp
const bool_t bCanMove = GameplayStateQuery::CanMove(m_World, entity);
const bool_t bCanAttack = GameplayStateQuery::CanAttack(m_World, entity);
const bool_t bForcedMotion = m_World.HasComponent<ForcedMotionComponent>(entity);
```

공격 windup 중 `!bCanAttack`이면 impact를 취소하고 Idle로 전환한다. 새 공격은 `bCanAttack`일 때만 시작한다. 이동과 flow-field 이동은 `bCanMove && !bForcedMotion`일 때만 수행하고 step에 `GameplayStateQuery::GetMoveSpeedMultiplier`를 곱한다. `Phase_ServerMinionDepenetration`도 `!bCanMove || bForcedMotion`이면 위치를 직접 보정하지 않는다. 이 규칙은 일반 미니언과 티버에 동일 적용한다.

1-15. C:/Users/user/Desktop/Winters/Server/Private/Game/ServerProjectileAuthority.cpp

`FindSkillProjectileHitTarget`의 기존 server-local `TryResolveCombatTeam` 필터를 아래 공통 필터로 교체:

```cpp
if (!GameplayStateQuery::IsMobileCombatUnit(world, candidate) ||
    !GameplayStateQuery::CanReceiveProjectileHit(world, projectile.owner, candidate) ||
    !GameplayStateQuery::CanReceiveCrowdControl(world, projectile.owner, candidate))
{
    continue;
}
```

투사체별 피해 가능 대상과 CC 가능 대상이 다를 수 있으므로 실제 구현에서는 projectile target mask를 적용하고, 야스오 Q3·애쉬 W/R은 Champion/Minion/Jungle만 허용한다. 구조물은 CC 투사체에 면역이다.

1-16. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Yasuo/YasuoGameSim.cpp

삭제할 범위:
`struct YasuoAirborneComponent`와 전용 airborne tick/cleanup 전체.

`OnQ`의 cast-time stack 증가를 삭제하고 Wind projectile의 실제 hit callback에서 stack을 증가시킨다. Q3 Tornado hit은 다음 공통 호출만 사용한다.

```cpp
GameplayStatus::ApplyAirborne(
    world, tc, target, caster,
    eChampion::YASUO, eSkillSlot::Q,
    airborneDurationSec, airborneArcHeight);
```

챔피언·미니언·정글은 적용되고 구조물은 제외된다. 동일 entity에는 투사체별 1회만 적용하며 Q3는 적중 대상마다 서버 damage와 airborne를 한 번만 발생시킨다.

1-17. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/LeeSin/LeeSinGameSim.cpp

`LeeSinGameSim::OnR`의 damage/cooldown 시작 전 아래 검증을 추가:

```cpp
if (!GameplayStateQuery::CanReceiveCrowdControl(world, caster, target))
    return false;
if (!IsWithinSkillRange(world, caster, target, rRange))
    return false;
```

기존 generic status 호출은 공통 `ApplyAirborne`으로 교체한다. Lee R은 Champion/Minion/Jungle에 서버 airborne arc를 적용하고 ally/structure/out-of-range 대상에는 자원·쿨다운을 소비하지 않는다.

1-18. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Yone/YoneGameSim.cpp

`EnqueueLineDamage`와 `ApplyLineAirborne`의 분리된 champion/minion loop를 `CollectEnemyMobileUnitsInSegment` 한 번으로 교체한다. nav-safe 실제 착지점 뒤쪽 gather endpoint를 계산한 뒤 대상마다 다음을 호출한다.

```cpp
GameplayStatus::ApplyAirborne(
    world, tc, target, caster,
    eChampion::YONE, eSkillSlot::R,
    airborneDurationSec, airborneArcHeight,
    &gatherPoint);
```

피해·gather·airborne은 impact tick에 한 번만 적용하고, cast 시작 시 미리 CC를 걸지 않는다.

1-19. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Annie/AnnieGameSim.cpp

`AddStunStack`과 `ConsumeStunReady`를 교체해 4번째 유효 스킬 적중/시전이 바로 stun-ready를 소비하도록 한다. Q/W/R 적중 대상 수집은 공통 Unit/Cone/Circle query를 사용하며 Champion/Minion/Jungle에 passive stun을 적용한다. 구조물은 제외한다. Q가 대상 없이 거절된 경우 stack을 올리지 않는다.

1-20. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Ashe/AsheGameSim.cpp

W projectile impact는 data의 `slowDurationSec`와 `slowMoveSpeedMul`을 사용해 Champion/Minion/Jungle에 `AsheVolleySlow`를 적용한다. R impact는 같은 mobile target boundary에 `AsheCrystalArrowStun`을 적용한다. 구조물과 아군에는 상태를 적용하지 않는다.

1-21. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Viego/ViegoGameSim.cpp

W stun 및 R 주변 slow의 Champion-only loop를 공통 mobile query로 교체한다. W는 적중 target 하나, R은 landing 주변 Champion/Minion/Jungle 모두에 data-driven slow를 적용한다. private caster dash는 caster가 외부 CC를 받으면 취소/정지되도록 `CanMove` gate를 둔다.

1-22. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Sylas/SylasGameSim.cpp

E2 chain hit에서 같은 slot의 airborne와 slow를 각각 독립 status id로 적용한다.

```cpp
GameplayStatus::ApplyAirborne(
    world, tc, target, caster,
    eChampion::SYLAS, eSkillSlot::E,
    airborneDurationSec, airborneArcHeight);
GameplayStatus::ApplySlow(
    world, tc, target, caster,
    eChampion::SYLAS, eSkillSlot::E,
    slowDurationSec, slowMoveSpeedMul);
```

R은 `SpellbookOverrideComponent`에 target champion의 R definition을 캡처하되, 실제 server R hook이 등록되어 있고 castable한 경우에만 override를 소비한다. handler가 없거나 command가 거절되면 보관 상태를 유지한다.

1-23. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/SpellbookFormOverride/SpellbookFormOverrideSystem.cpp

기존 override consume이 dispatch 전에 일어나는 블록을 `CanDispatchCapturedUltimate` 성공과 champion R hook의 accepted 결과 뒤로 이동한다. 모든 playable roster의 R hook coverage를 startup/SimLab에서 검증하고, 누락된 R은 명시적인 unsupported 결과로 거절한다. client는 `spellbookChampionId/slot/remaining` 복제값으로 아이콘과 target mode를 바꾼다.

1-24. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Jax/JaxGameSim.cpp

`ReleaseJaxCounterStrike`의 Champion-only loop를 `CollectEnemyMobileUnitsInCircle`로 교체하고 radius/damage/stun duration을 definition data에서 읽는다. E1에는 stun이 없고 수동 E2와 자동 종료가 동일한 release 함수 한 곳을 사용한다.

1-25. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Kalista/KalistaGameSim.cpp

E는 공통 circle/marked-target query로 Champion/Minion/Jungle에 data-driven stun을 적용한다. R은 서버 권위 2단계 상태로 구현한다.

```cpp
// Stage 1: owner가 선택한 allied champion을 carry 상태로 전환한다.
// Stage 2: direction으로 ally를 강제 이동시키고 최초 충돌 지점의
// enemy mobile units에 damage + airborne를 적용한 뒤 carry를 해제한다.
```

carry 중 ally는 `KalistaFateCallUntargetable`과 move/attack/cast lock을 가지며 owner, carried entity, stage, timeout, launch start/end를 `KalistaSentinelComponent.h`의 기존 Kalista runtime state에 추가한다. 다른 Kalista가 소유한 ally를 탈취할 수 없고 timeout/death/disconnect 시 안전하게 해제한다.

1-26. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Riven/RivenGameSim.cpp

Q1/Q2/Q3 stage를 server action stage로 고정하고 Q3만 공통 circle query에 airborne를 적용한다. W는 같은 query로 stun을 적용한다. 모든 상태 대상은 Champion/Minion/Jungle이며 구조물은 제외한다. server damage와 status는 동일 impact 함수에서 한 번만 처리한다.

1-27. C:/Users/user/Desktop/Winters/Data/LoL/ClientPublic/Visual/ChampionAssetVisualDefs.json

기존 코드:

```json
"key": "champion.model.riven",
"spawnScale": 0.01
```

아래로 교체:

```json
"key": "champion.model.riven",
"spawnScale": 0.015
```

시각 model scale만 1.5배로 변경한다. 서버 `SpatialAgentComponent.radius`와 collision은 변경하지 않는다.

1-28. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/AnnieSimComponent.h

`AnnieTibbersComponent`에 owner-command 상태를 추가:

```cpp
EntityID commandTarget = NULL_ENTITY;
Vec3 commandPosition{};
bool_t bHasCommandPosition = false;
u32_t commandSeq = 0u;
```

1-29. C:/Users/user/Desktop/Winters/Shared/Schemas/Command.fbs

기존 `CommandKind` enum 마지막 값 뒤에 append:

```text
CompanionCommand
```

기존 command payload의 position/targetNet을 재사용하고 mode는 확장 가능한 `companionMode:ubyte` 필드로 command table 끝에 append한다. enum 숫자는 기존 값을 재배치하지 않는다.

1-30. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h

기존 command dispatch interface에 추가:

```cpp
virtual bool_t HandleCompanionCommand(
    CWorld& world,
    EntityID owner,
    const GameCommand& command,
    const TickContext& tc) = 0;
```

1-31. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp

`ExecuteCommand` switch에 `CompanionCommand`를 추가한다. owner champion의 `AnnieSimComponent.tibbersEntity`와 실제 `AnnieTibbersComponent.ownerEntity`가 모두 일치할 때만 position/target command를 반영한다. 다른 플레이어의 티버, 사망한 티버, 범위 밖 invalid target은 거절한다.

1-32. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomUnitAI.cpp

티버 AI의 owner-follow fallback 앞에 명령 우선순위를 추가한다.

```cpp
// explicit target -> explicit move position -> owner follow -> autonomous attack
```

R 토글은 클라이언트 입력 모드일 뿐 서버 gameplay state가 아니다. 서버는 승인된 companion command만 실행하며, 티버 이동/공격도 공통 CC gate와 navmesh를 사용한다.

1-33. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameLocalSkills.cpp

Annie R 입력 처리에 local companion-command mode를 추가한다. 티버가 없으면 기존 R cast, 티버가 존재하면 R을 누를 때 command mode를 토글한다. 토글 상태의 우클릭은 champion 이동 대신 `SendCompanionCommand`를 전송하고, Escape/티버 사망/owner 사망 시 해제한다. local toggle은 서버 상태를 대체하지 않는다.

1-34. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Minion/Minion_Manager.cpp

generic minion attack reverse 재생에서 티버를 분리한다. 티버는 spawn 1회, idle, run, attack1/attack2 교대, death를 실제 `.wanim` key로 선택하며 attack animation을 역재생하지 않는다. 다음 정적 자산 검증 결과를 기준으로 한다.

```text
tibber.wmesh / tibber.wskel / tibber.wmat: 존재
tibber_attack1.wanim / tibber_attack2.wanim: 존재
tibber_idle1.wanim / tibber_run.wanim / tibber_spawn.wanim / tibber_death.wanim: 존재
skeleton hash: 0xc34e03e5f308b288 일치
```

1-35. C:/Users/user/Desktop/Winters/Data/Gameplay/ChampionGameData/champions.json

요청 스킬의 targetMode를 실제 입력 계약과 맞춘다.

```text
Annie Q UnitTarget, W Direction, R GroundTarget
Ashe W/R Direction
Yasuo Q Direction
LeeSin R UnitTarget
Yone R Direction
Viego W Direction, R GroundTarget
Sylas E Direction
Riven Q Direction, W Self
Kalista E Self, R Direction + stageCount 2
Jax E Self
```

1-36. C:/Users/user/Desktop/Winters/Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json

기존 스킬 정의의 `params`에 다음 designer-owned 값을 추가하고 C++ 하드코딩을 제거한다.

```json
{
  "airborneDurationSec": 0.75,
  "airborneArcHeight": 2.1,
  "stunDurationSec": 0.75,
  "slowDurationSec": 1.5,
  "slowMoveSpeedMul": 0.6,
  "impactRadius": 2.25,
  "gatherBehindDistance": 0.8
}
```

각 key의 실제 값은 기존 gameplay 값과 요청 동작을 보존해 개별 정의에 넣는다. 모든 스킬에 무관한 필드를 일괄 추가하지 않는다.

1-37. C:/Users/user/Desktop/Winters/Tools/LoLData/Build-LoLDefinitionPack.py

기존 typed param allow-list와 source target mapping에 위 신규 param 및 명시 targetMode를 추가한다. `Conditional -> Self` fallback에 의존하지 않게 하고, 생성물은 이 script로만 갱신한다.

1-38. C:/Users/user/Desktop/Winters/Tools/SimLab/main.cpp

`RunActionMovePolicyProbe` 아래에 추가:

```cpp
bool RunCrowdControlCoreProbe();
bool RunChampionCrowdControlProbe();
bool RunSylasUltimateHijackProbe();
bool RunTibbersCommandProbe();
```

probe는 실제 GameSim API를 사용하며 별도 테스트 전용 CC 구현을 만들지 않는다. same seed 상태/모션 per-tick hash도 비교한다.

2. 검증

미검증:
- 공통 status stack이 같은 caster/slot의 Airborne과 Slow를 동시에 유지하는지.
- 서로 다른 caster가 같은 champion/slot을 사용할 때 효과가 독립적인지.
- Champion/Minion/Jungle만 CC를 받고 Structure/Ward/Projectile/EffectAnchor가 면역인지.
- Stun/Airborne 중 Move/BasicAttack/Cast/Recall과 미니언 attack impact가 모두 차단되는지.
- Slow 이동 거리가 base speed × multiplier × dt인지.
- forced motion 중간 Y와 gather XZ, 종료 위치 및 component 정리가 결정적인지.
- Snapshot 중간 접속에서도 gameplay flags, slow multiplier, forced motion metadata가 동일한지.
- Yasuo Q3, Lee R, Annie passive, Ashe W/R, Yone R, Viego W/R, Sylas E2, Jax E2, Kalista E/R, Riven W/Q3의 대상 행렬과 정확한 지속시간.
- Sylas R이 등록된 적 R만 캡처하고 성공한 cast 뒤에만 소비하는지.
- Tibbers owner command, 다른 owner의 command 거절, spawn/idle/run/attack1/attack2/death animation 선택.
- Riven visual scale만 1.5배이고 gameplay collision이 변하지 않는지.

검증 명령:
- `C:\Users\user\Desktop\Winters\Shared\Schemas\run_codegen.bat`
- `python C:\Users\user\Desktop\Winters\Tools\LoLData\Build-LoLDefinitionPack.py --root C:\Users\user\Desktop\Winters`
- `python C:\Users\user\Desktop\Winters\Tools\LoLData\Build-LoLDefinitionPack.py --root C:\Users\user\Desktop\Winters --check`
- `powershell -ExecutionPolicy Bypass -File C:\Users\user\Desktop\Winters\Tools\Harness\Check-SharedBoundary.ps1`
- `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' C:\Users\user\Desktop\Winters\Shared\GameSim\Include\GameSim.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1`
- `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' C:\Users\user\Desktop\Winters\Tools\SimLab\SimLab.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1`
- `C:\Users\user\Desktop\Winters\Tools\Bin\Debug\SimLab.exe 1800 42`
- `C:\Users\user\Desktop\Winters\Tools\Bin\Debug\SimLab.exe 1800 42`
- `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' C:\Users\user\Desktop\Winters\Engine\Include\Engine.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1`
- `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' C:\Users\user\Desktop\Winters\Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1`
- `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' C:\Users\user\Desktop\Winters\Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1`
- `git -C C:\Users\user\Desktop\Winters diff --check`

확인 필요:
- floor 검증은 SimLab 결정론 probe와 GameSim/Server/Client Debug x64 빌드까지 완료한다.
- ceiling 예산 30%는 정상 F5 roster/map/minion/UI/FX를 숨기지 않은 두 PC 동일 Wi-Fi 세션에 사용한다. 서버 1대와 클라이언트 2대에서 각 CC 대상의 `netId`, `gameplayStateFlags`, `forcedMotionKind`, 종료 tick을 debug overlay/OutputDebugString로 캡처하고, 티버 R 토글 우클릭과 사일러스 R 탈취를 직접 검증한다.
- FBX 원본 자체 bake 재생성은 원본 DCC 파일과 exporter가 repository에 없으면 `CONFIRM_NEEDED`이다. 이번 세션은 shipped `.wanim` skeleton hash·clip load·runtime pose 전이를 검증하며 원본 bake를 임의 생성하지 않는다.
