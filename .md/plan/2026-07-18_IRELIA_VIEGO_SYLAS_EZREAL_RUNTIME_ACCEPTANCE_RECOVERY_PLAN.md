Session - 인게임 실측으로 남은 Irelia E·Viego W/R·Sylas Passive BA·Ezreal R 표현/판정 회귀를 최소 수정으로 닫는다
좌표: 신규 좌표 후보 · 축: C7 권위와 정합성 · C8 검증이 병목
관련: 2026-07-18_IRELIA_VIEGO_EZREAL_STAGE_INPUT_REGRESSION_RECOVERY_RESULT.md, 2026-07-17_SYLAS_PASSIVE_STACK_BA_ANIMATION_FX_RESULT.md

# 1. 결정 기록

① 목표: 사용자가 통과시킨 Irelia W, Irelia E1/E2 입력, Viego W charge, Ezreal E는 보존하고 남은 네 묶음만 수정한다.
② 권위: W 사거리/스턴 같은 기획 수치는 canonical JSON, 착지점 계산·이벤트 소비·애니 선택 같은 실행 방식은 C++가 소유한다.
③ 최소 수정: Irelia E는 authoritative visual event를 legacy target-mode로 재검증하는 한 분기만 제거하고, Viego R은 클릭 착지점을 하나의 중심으로 재사용한다.
④ 미확정: Sylas는 서버 stack/stage/cue와 asset이 이미 있어 수치를 다시 넣지 않는다. 한 번의 trace/BP로 끊긴 구간을 확정한 뒤 해당 구간만 고친다.
⑤ 검증: schema/codegen → focused SimLab → Debug Client/Server → Debug 인게임 acceptance → Release 전체 pipeline 순서로 간다.
범위 상한: 이번 slice는 아래 4개 런타임 묶음만 다룬다. MainMenu, 타 챔피언 튜닝, 신규 FX 제작, 구조 개편은 30% 여유 범위 밖이다.

# 2. 현재 증거와 파일별 변경 계획

## 2-1. 인게임 실측을 기준선으로 고정

| 항목 | 현재 판정 | 이번 작업 |
|---|---|---|
| Irelia W hold/release/즉시 이동·E | 인게임 PASS | 코드 변경 금지, 회귀 acceptance만 반복 |
| Irelia E1/E2 명령·기절 | 서버 gameplay PASS | 변경 금지 |
| Irelia E blade FBX·E2 표식 | Client visual FAIL | authoritative E cue가 legacy `resolvedTargetMode`에 막히는 분기 제거 |
| Viego W charge/release | 인게임 PASS | 구조 변경 금지 |
| Viego W 거리·스턴 | 체감 부족 | JSON에서 최대 거리와 최대 charge stun만 조정 |
| Viego R | 착지점 중심 피해/slow 불일치 | raw 방향 최대거리 대신 실제 클릭/범위/보행 clamp 착지점을 단일 중심으로 사용 |
| Sylas passive BA | animation 미적용, FX도 재확인 필요 | stage가 끊기는 정확한 구간부터 trace/BP로 판정 |
| Ezreal E 및 나머지 | PASS | 변경 금지 |
| Ezreal R | 최종 mesh가 진행 방향 대비 +90도 | 최종 writer인 projectile catalog yaw만 `PI → PI/2`로 교정 |

## 2-2. 소유권: “JSON은 무엇, C++은 어떻게”

```text
Data/Gameplay/ChampionGameData/champions.json
  Viego W 최대 사용 거리, charge별 stun 범위
            |
            v
ChampionGameData.generated.* + LoL Definition Pack
            |
            v
Shared/GameSim C++
  charge ratio 적용, R 착지점 clamp, 원형 피해/slow 실행
            |
            v
ActionStart / EffectTrigger / Snapshot
            |
            v
Client C++
  Irelia blade/mark 생성, Sylas stage2 animation/FX 재생,
  Ezreal projectile 최종 yaw 적용
```

금지 사항은 다음과 같다.

- Viego W 수치를 `ViegoGameSim.cpp` 상수로 다시 넣지 않는다.
- Irelia E FBX를 Server/GameSim에서 생성하지 않는다.
- Sylas passive stack을 Client가 추측하지 않는다.
- Ezreal R 방향을 WFX rotation만 바꿔서 고쳤다고 판정하지 않는다.
- generated C++/JSON을 손으로 편집하지 않는다.

## 2-3. V0 — 먼저 남길 진단 신호

### `Client/Private/Network/Client/EventApplier.cpp`: 죽어 있던 Irelia trace 활성화

기존 코드:

```cpp
                sprintf_s(msg,
                    "[IreliaReplayCue][Client] slot=%u stage=%u effect=0x%08X source=%u target=%u def=%u visual=%u pos=(%.2f,%.2f,%.2f)\n",
                    static_cast<u32_t>(slot),
                    static_cast<u32_t>(skillStage),
                    effectId,
                    static_cast<u32_t>(source),
                    static_cast<u32_t>(target),
                    pDef ? 1u : 0u,
                    bVisualHandled ? 1u : 0u,
                    command.groundPos.x,
                    command.groundPos.y,
                    command.groundPos.z);
                ++s_ireliaCueTraceCount;
```

아래로 교체:

```cpp
                sprintf_s(msg,
                    "[IreliaReplayCue][Client] slot=%u stage=%u effect=0x%08X source=%u target=%u def=%u visual=%u pos=(%.2f,%.2f,%.2f)\n",
                    static_cast<u32_t>(slot),
                    static_cast<u32_t>(skillStage),
                    effectId,
                    static_cast<u32_t>(source),
                    static_cast<u32_t>(target),
                    pDef ? 1u : 0u,
                    bVisualHandled ? 1u : 0u,
                    command.groundPos.x,
                    command.groundPos.y,
                    command.groundPos.z);
                OutputDebugStringA(msg);
                ++s_ireliaCueTraceCount;
```

이 trace는 Debug에서만 최대 64회이며 gameplay를 바꾸지 않는다. `def=0`이어도 authoritative E cue가 blade를 만들 수 있어야 한다는 것이 이번 수정의 핵심이다.

### `Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`: Sylas 서버 stage trace

기존 코드:

```cpp
    action.uFlags = bJaxEmpowerAttack ? CombatActionFlags::JaxEmpower : 0u;
    if (bSylasPassiveAttack)
        action.uFlags |= CombatActionFlags::SylasPassive;
    if (bZedPassiveAttack)
        action.uFlags |= CombatActionFlags::ZedPassive;
    action.entityTarget = cmd.targetEntity;
```

아래로 교체:

```cpp
    action.uFlags = bJaxEmpowerAttack ? CombatActionFlags::JaxEmpower : 0u;
    if (bSylasPassiveAttack)
        action.uFlags |= CombatActionFlags::SylasPassive;
    if (bZedPassiveAttack)
        action.uFlags |= CombatActionFlags::ZedPassive;
#if defined(_DEBUG)
    if (champion == eChampion::SYLAS)
    {
        const u8_t remainingStacks =
            world.HasComponent<SylasSimComponent>(cmd.issuerEntity)
                ? world.GetComponent<SylasSimComponent>(cmd.issuerEntity).passiveStacks
                : 0u;
        char message[192]{};
        sprintf_s(
            message,
            "[SylasPassive][ServerBA] consumed=%u stage=%u flags=0x%04X remaining=%u seq=%u\n",
            bSylasPassiveAttack ? 1u : 0u,
            static_cast<u32_t>(attackActionStage),
            static_cast<u32_t>(action.uFlags),
            static_cast<u32_t>(remainingStacks),
            cmd.sequenceNum);
        OutputCommandDebug(message);
    }
#endif
    action.entityTarget = cmd.targetEntity;
```

예상 정상값은 `consumed=1 stage=2 flags`에 `SylasPassive` bit가 포함된 상태다.

### `Client/Private/Network/Client/EventApplier.cpp`: Sylas 최종 animation 선택/재생 trace

기존 코드:

```cpp
#if defined(_DEBUG)
        if (!bPlayed)
        {
            static u32_t s_actionMissTraceCount = 0;
            if (s_actionMissTraceCount < 64u)
            {
                char msg[192]{};
                sprintf_s(msg,
                    "[ActionVisualMiss] champion=%u actionId=%u stage=%u name=%s\n",
                    static_cast<u32_t>(animationChampion),
                    static_cast<u32_t>(actionId),
                    static_cast<u32_t>(actionStage),
                    animName.c_str());
                OutputDebugStringA(msg);
                ++s_actionMissTraceCount;
            }
        }
#endif
```

아래로 교체:

```cpp
#if defined(_DEBUG)
        if (animationChampion == eChampion::SYLAS &&
            id == eReplicatedActionId::BasicAttack &&
            actionStage >= 2u)
        {
            static u32_t s_sylasPassiveActionTraceCount = 0u;
            if (s_sylasPassiveActionTraceCount < 32u)
            {
                char msg[224]{};
                sprintf_s(
                    msg,
                    "[SylasPassive][ClientAction] stage=%u def=%u stage2Key=%s selected=%s played=%u\n",
                    static_cast<u32_t>(actionStage),
                    pPresentationDef ? 1u : 0u,
                    pPresentationDef && pPresentationDef->stage2AnimKey
                        ? pPresentationDef->stage2AnimKey
                        : "-",
                    animName.c_str(),
                    bPlayed ? 1u : 0u);
                OutputDebugStringA(msg);
                ++s_sylasPassiveActionTraceCount;
            }
        }
        else if (!bPlayed)
        {
            static u32_t s_actionMissTraceCount = 0;
            if (s_actionMissTraceCount < 64u)
            {
                char msg[192]{};
                sprintf_s(msg,
                    "[ActionVisualMiss] champion=%u actionId=%u stage=%u name=%s\n",
                    static_cast<u32_t>(animationChampion),
                    static_cast<u32_t>(actionId),
                    static_cast<u32_t>(actionStage),
                    animName.c_str());
                OutputDebugStringA(msg);
                ++s_actionMissTraceCount;
            }
        }
#endif
```

### `Client/Private/Network/Client/EventApplier.cpp`: Sylas EffectTrigger trace

기존 코드:

```cpp
        const bool_t bVisualHandled =
            CVisualHookRegistry::Instance().Dispatch(effectId, ctx);

#if defined(_DEBUG)
        if (hookChampion == eChampion::IRELIA)
```

아래로 교체:

```cpp
        const bool_t bVisualHandled =
            CVisualHookRegistry::Instance().Dispatch(effectId, ctx);

#if defined(_DEBUG)
        if (hookChampion == eChampion::SYLAS &&
            hookSlot == static_cast<u8_t>(eSkillSlot::BasicAttack))
        {
            static u32_t s_sylasPassiveCueTraceCount = 0u;
            if (s_sylasPassiveCueTraceCount < 32u)
            {
                char msg[192]{};
                sprintf_s(
                    msg,
                    "[SylasPassive][ClientCue] stage=%u effect=0x%08X visual=%u authoritative=%u\n",
                    static_cast<u32_t>(skillStage),
                    effectId,
                    bVisualHandled ? 1u : 0u,
                    ctx.bAuthoritativeEvent ? 1u : 0u);
                OutputDebugStringA(msg);
                ++s_sylasPassiveCueTraceCount;
            }
        }
        if (hookChampion == eChampion::IRELIA)
```

## 2-4. V1 — Irelia E client visual 복구

### `Client/Private/GameObject/Champion/Irelia/Irelia_Skills.cpp`

기존 코드:

```cpp
    void OnCastAccepted_E(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand || !ctx.pFxMeshRenderer)
            return;
        if (ctx.pCommand->resolvedTargetMode != static_cast<u8_t>(eTargetMode::GroundTarget))
            return;

        const IreliaTuning& t = GetTuning();
```

아래로 교체:

```cpp
    void OnCastAccepted_E(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand || !ctx.pFxMeshRenderer)
            return;

        const IreliaTuning& t = GetTuning();
```

근거:

- 이 함수는 local raw input handler가 아니라 서버가 승인해 보낸 E `EffectTrigger`의 visual hook이다.
- E1/E2의 slot, stage, ground position은 이미 authoritative event가 소유한다.
- `resolvedTargetMode`는 Client `SkillRegistry` lookup이 성공할 때만 다시 채워지는 legacy adapter 값이다.
- 이 재검증이 실패하면 `CIreliaBladeSystem::SpawnPlaced`뿐 아니라 E2 connect/target mark까지 함수 전체가 조기 종료된다.
- R에서 같은 E blade FBX가 정상 생성되므로 resource/renderer/FBX 자체는 무죄다.

변경하지 않는 것:

- `CIreliaBladeSystem`, FBX 경로, shader, texture
- Server `IreliaGameSim::OnE`의 line hit/stun/damage
- Q mark와 R visual 경로
- E target mode canonical JSON (`GroundTarget` 유지)

## 2-5. V2 — Viego W 기획 수치 조정

### `Data/Gameplay/ChampionGameData/champions.json`

Viego W의 기존 코드:

```json
                    "rangeMax": 4.0,
```

아래로 교체:

```json
                    "rangeMax": 5.0,
```

Viego W charge의 기존 코드:

```json
                        "stunSeconds": [
                            0.25,
                            0.75
                        ]
```

아래로 교체:

```json
                        "stunSeconds": [
                            0.25,
                            2.0
                        ]
```

해석을 명시한다.

- “2초”는 **풀차지 최대 스턴 2.0초**로 적용한다.
- 짧은 탭도 무조건 2초가 되는 `[2.0, 2.0]`은 charge 의미를 없애므로 적용하지 않는다.
- `rangeScale=[0.5,1.0]`은 유지한다. 따라서 탭/풀차지 중심 거리는 `2.5 → 5.0`이다.
- `radius=0.75`는 hit 두께이고 “앞으로 조금”과 다른 축이므로 이번에는 유지한다.

### `Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json`

기존 코드:

```json
      "params": {
        "baseDamage": 55.0,
        "dashDurationSec": 0.26,
        "radius": 0.75,
        "stunDurationSec": 0.75
      },
```

아래로 교체:

```json
      "params": {
        "baseDamage": 55.0,
        "dashDurationSec": 0.26,
        "radius": 0.75,
        "stunDurationSec": 2.0
      },
```

이 값은 charge component가 없는 비정상/fallback 경로에서도 canonical 최대값과 모순되지 않게 맞추는 parity 수정이다. 정상 W2는 `stunSeconds` curve가 최종값을 소유한다.

### `Tools/SimLab/main.cpp`: charge contract 고정

기존 코드:

```cpp
            !NearlyEqual(pViegoW->charge.minRangeScale, 0.5f) ||
            !NearlyEqual(pViegoW->charge.maxRangeScale, 1.f) ||
            !NearlyEqual(pViegoW->charge.minStunSec, 0.25f) ||
            !NearlyEqual(pViegoW->charge.maxStunSec, 0.75f) ||
            !NearlyEqual(ResolveSkillChargeRatio(10u, 110u, 10u), 0.f) ||
            !NearlyEqual(ResolveSkillChargeRatio(10u, 110u, 60u), 0.5f) ||
            !NearlyEqual(ResolveSkillChargeRatio(10u, 110u, 110u), 1.f) ||
            !NearlyEqual(ResolveSkillChargeValue(0.5f, 1.f, 0.5f), 0.75f) ||
            !NearlyEqual(ResolveSkillChargeValue(0.25f, 0.75f, 0.5f), 0.5f))
```

아래로 교체:

```cpp
            !NearlyEqual(pViegoW->rangeMax, 5.f) ||
            !NearlyEqual(pViegoW->charge.minRangeScale, 0.5f) ||
            !NearlyEqual(pViegoW->charge.maxRangeScale, 1.f) ||
            !NearlyEqual(pViegoW->charge.minStunSec, 0.25f) ||
            !NearlyEqual(pViegoW->charge.maxStunSec, 2.f) ||
            !NearlyEqual(ResolveSkillChargeRatio(10u, 110u, 10u), 0.f) ||
            !NearlyEqual(ResolveSkillChargeRatio(10u, 110u, 60u), 0.5f) ||
            !NearlyEqual(ResolveSkillChargeRatio(10u, 110u, 110u), 1.f) ||
            !NearlyEqual(ResolveSkillChargeValue(0.5f, 1.f, 0.5f), 0.75f) ||
            !NearlyEqual(ResolveSkillChargeValue(0.25f, 2.f, 0.5f), 1.125f))
```

## 2-6. V2 — Viego R 착지점 중심 통일

현재 버그는 `targetMode=GroundTarget`인데도 `OnR`가 클릭 거리와 무관하게 `origin + direction * rangeMax`를 사용한다는 것이다. 따라서 3m 앞을 클릭해도 피해/slow 중심은 항상 6m 앞이다.

이번 범위는 **중심 좌표 복구**다. 피해 발생 시점을 cast 시점에서 dash arrival 시점으로 미루는 별도 시스템은 만들지 않는다. 그 변경은 전투 타이밍과 checkpoint component를 넓히므로 이번 요구보다 크다.

### `Shared/GameSim/Champions/Viego/ViegoGameSim.cpp`: 착지점 resolver 추가

기존 코드:

```cpp
    Vec3 ResolveDirection(const GameplayHookContext& ctx)
    {
        if (ctx.pCommand)
        {
            Vec3 dir = WintersMath::NormalizeXZ(ctx.pCommand->direction);
            if (dir.x != 0.f || dir.z != 0.f)
                return dir;

            if (ctx.pWorld && ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
            {
                const Vec3 origin =
                    ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
                dir = WintersMath::NormalizeXZ(Vec3{
                    ctx.pCommand->groundPos.x - origin.x,
                    0.f,
                    ctx.pCommand->groundPos.z - origin.z
                });
                if (dir.x != 0.f || dir.z != 0.f)
                    return dir;
            }
        }

        return Vec3{ 0.f, 0.f, 1.f };
    }
```

아래에 추가:

```cpp
    Vec3 ResolveGroundDashEnd(
        const GameplayHookContext& ctx,
        const Vec3& origin,
        const Vec3& fallbackDirection,
        f32_t maxRange)
    {
        Vec3 end = ctx.pCommand
            ? ctx.pCommand->groundPos
            : Vec3{
                origin.x + fallbackDirection.x * maxRange,
                origin.y,
                origin.z + fallbackDirection.z * maxRange };
        end.y = origin.y;

        Vec3 delta{ end.x - origin.x, 0.f, end.z - origin.z };
        const f32_t distanceSq = delta.x * delta.x + delta.z * delta.z;
        const f32_t maxRangeSq = maxRange * maxRange;
        if (maxRange > 0.f && distanceSq > maxRangeSq)
        {
            const f32_t inverseDistance = 1.f / std::sqrt(distanceSq);
            end.x = origin.x + delta.x * inverseDistance * maxRange;
            end.z = origin.z + delta.z * inverseDistance * maxRange;
        }

        if (ctx.pTickCtx && ctx.pTickCtx->pWalkable && ctx.pWorld)
        {
            Vec3 clampedEnd = end;
            const f32_t casterRadius = GameplayStateQuery::ResolveGameplayRadius(
                *ctx.pWorld,
                ctx.casterEntity);
            if (ctx.pTickCtx->pWalkable->TryClampMoveSegmentXZ(
                origin,
                end,
                casterRadius,
                clampedEnd))
            {
                end = clampedEnd;
            }
            else
            {
                end = origin;
            }
        }

        return end;
    }
```

### `Shared/GameSim/Champions/Viego/ViegoGameSim.cpp`: `OnR` 중심 교체

기존 코드:

```cpp
        const f32_t slowMoveSpeedMul = ResolveViegoSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::MoveSpeedMul);
        const Vec3 end{ origin.x + dir.x * range, origin.y, origin.z + dir.z * range };
        RotateToward(*ctx.pWorld, ctx.casterEntity, dir);
        StartDash(*ctx.pWorld, ctx.casterEntity, end, dashDurationSec);
```

아래로 교체:

```cpp
        const f32_t slowMoveSpeedMul = ResolveViegoSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::MoveSpeedMul);
        const Vec3 end = ResolveGroundDashEnd(ctx, origin, dir, range);
        const Vec3 landingDirection = WintersMath::NormalizeXZ(Vec3{
            end.x - origin.x,
            0.f,
            end.z - origin.z });
        RotateToward(
            *ctx.pWorld,
            ctx.casterEntity,
            (landingDirection.x != 0.f || landingDirection.z != 0.f)
                ? landingDirection
                : dir);
        StartDash(*ctx.pWorld, ctx.casterEntity, end, dashDurationSec);
```

이후 기존 `EnqueueCircleDamage(... end ...)`와 `ApplyCircleSlow(... end ...)`는 그대로 둔다. dash, 피해, slow가 같은 `end` 하나를 사용하게 만드는 것이 목적이다.

### `Tools/SimLab/main.cpp`: Viego R 회귀 probe 추가

`RunViegoPossessionProbe()` 바로 아래에 추가:

```cpp
    bool_t RunViegoRLandingCenterProbe()
    {
        CWorld world;
        DeterministicRng rng(2026071803ull);
        EntityIdMap entityMap;
        FlatWalkable walkable;
        auto executor = CDefaultCommandExecutor::Create();

        const EntityID viego = SpawnChampion(
            world, entityMap, eChampion::VIEGO,
            static_cast<u8_t>(eTeam::Blue), 0u);
        const EntityID clickedCenterTarget = SpawnChampion(
            world, entityMap, eChampion::JAX,
            static_cast<u8_t>(eTeam::Red), 5u);
        const EntityID legacyMaxRangeTarget = SpawnChampion(
            world, entityMap, eChampion::ANNIE,
            static_cast<u8_t>(eTeam::Red), 6u);

        world.GetComponent<TransformComponent>(viego).SetPosition(Vec3{});
        world.GetComponent<TransformComponent>(clickedCenterTarget).SetPosition(
            Vec3{ 3.f, 0.f, 0.f });
        world.GetComponent<TransformComponent>(legacyMaxRangeTarget).SetPosition(
            Vec3{ 6.f, 0.f, 0.f });
        world.GetComponent<SkillRankComponent>(viego)
            .ranks[static_cast<u8_t>(eSkillSlot::R)] = 1u;

        TickContext tc = MakeProbeTickContext(
            1ull, rng, entityMap, walkable);
        GameCommand cast{};
        cast.kind = eCommandKind::CastSkill;
        cast.issuerEntity = viego;
        cast.issuedAtTick = tc.tickIndex;
        cast.sequenceNum = 1u;
        cast.slot = static_cast<u8_t>(eSkillSlot::R);
        cast.groundPos = Vec3{ 3.f, 0.f, 0.f };
        cast.direction = Vec3{ 1.f, 0.f, 0.f };

        const CommandExecutionResult result =
            executor->ExecuteCommand(world, tc, cast);
        bool_t bClickedTargetDamaged = false;
        bool_t bLegacyTargetDamaged = false;
        world.ForEach<DamageRequestComponent>(
            [&](EntityID, DamageRequestComponent& request)
            {
                const u16_t viegoRSkillId = static_cast<u16_t>(
                    (static_cast<u32_t>(eChampion::VIEGO) << 8) |
                    static_cast<u32_t>(eSkillSlot::R));
                if (request.source != viego ||
                    request.skillId != viegoRSkillId)
                {
                    return;
                }
                bClickedTargetDamaged =
                    bClickedTargetDamaged ||
                    request.target == clickedCenterTarget;
                bLegacyTargetDamaged =
                    bLegacyTargetDamaged ||
                    request.target == legacyMaxRangeTarget;
            });

        const bool_t bClickedTargetSlowed =
            CountStatusEffects(
                world,
                clickedCenterTarget,
                eStatusEffectId::GenericSlow,
                viego) == 1u;
        const bool_t bLegacyTargetSlowed =
            CountStatusEffects(
                world,
                legacyMaxRangeTarget,
                eStatusEffectId::GenericSlow,
                viego) != 0u;

        if (result.state != eCommandExecutionState::Accepted ||
            !bClickedTargetDamaged ||
            bLegacyTargetDamaged ||
            !bClickedTargetSlowed ||
            bLegacyTargetSlowed)
        {
            std::printf(
                "[SimLab][ViegoRLanding] FAIL: state=%u clickedDamage=%u legacyDamage=%u clickedSlow=%u legacySlow=%u\n",
                static_cast<u32_t>(result.state),
                bClickedTargetDamaged ? 1u : 0u,
                bLegacyTargetDamaged ? 1u : 0u,
                bClickedTargetSlowed ? 1u : 0u,
                bLegacyTargetSlowed ? 1u : 0u);
            return false;
        }

        std::printf(
            "[SimLab][ViegoRLanding] PASS: damage/slow centered on requested landing point\n");
        return true;
    }
```

`main()`의 `--stage-input-only` 분기에 기존 코드:

```cpp
        const bool_t bChargePass = RunSkillChargeContractProbe();
        const bool_t bCommandResultWirePass =
            RunCommandResultSnapshotCompatibilityProbe();
        const bool_t bActionPolicyPass = RunActionMovePolicyProbe();
        const bool_t bPass = bContractPass && bIreliaPass && bChargePass &&
            bCommandResultWirePass && bActionPolicyPass;
```

아래로 교체:

```cpp
        const bool_t bChargePass = RunSkillChargeContractProbe();
        const bool_t bViegoRLandingPass = RunViegoRLandingCenterProbe();
        const bool_t bCommandResultWirePass =
            RunCommandResultSnapshotCompatibilityProbe();
        const bool_t bActionPolicyPass = RunActionMovePolicyProbe();
        const bool_t bPass = bContractPass && bIreliaPass && bChargePass &&
            bViegoRLandingPass && bCommandResultWirePass && bActionPolicyPass;
```

전체 기본 probe 목록에도 `RunViegoRLandingCenterProbe()`를 연결해 focused 실행에서만 통과하는 테스트가 되지 않게 한다.

기존 코드:

```cpp
    const bool_t bYoneEReturnProbePass = RunYoneEReturnProbe();
    const bool_t bViegoPossessionProbePass = RunViegoPossessionProbe();
    const bool_t bDataDrivenSkillContractProbePass =
        RunDataDrivenSkillContractProbe();
```

아래로 교체:

```cpp
    const bool_t bYoneEReturnProbePass = RunYoneEReturnProbe();
    const bool_t bViegoPossessionProbePass = RunViegoPossessionProbe();
    const bool_t bViegoRLandingCenterProbePass =
        RunViegoRLandingCenterProbe();
    const bool_t bDataDrivenSkillContractProbePass =
        RunDataDrivenSkillContractProbe();
```

기존 코드:

```cpp
        bYoneEReturnProbePass &&
        bViegoPossessionProbePass &&
        bDataDrivenSkillContractProbePass &&
```

아래로 교체:

```cpp
        bYoneEReturnProbePass &&
        bViegoPossessionProbePass &&
        bViegoRLandingCenterProbePass &&
        bDataDrivenSkillContractProbePass &&
```

## 2-7. V2 — Ezreal R 최종 yaw 교정

### `Client/Private/GameObject/Projectile/ProjectileVisualCatalog.cpp`

기존 코드:

```cpp
    constexpr ProjectileVisualDesc kEzrealGlobalBeamVisual{
        "Ezreal.R.Missile", "Ezreal.R.Hit", nullptr, nullptr, nullptr, nullptr,
        WintersMath::kPi
    };
```

아래로 교체:

```cpp
    constexpr ProjectileVisualDesc kEzrealGlobalBeamVisual{
        "Ezreal.R.Missile", "Ezreal.R.Hit", nullptr, nullptr, nullptr, nullptr,
        WintersMath::kPi * 0.5f
    };
```

근거:

- catalog offset 0일 때 사용자가 관찰한 방향은 -90도였다.
- catalog에 `PI`를 더한 현재 방향은 +90도다.
- 두 관찰 사이의 중간 보정인 `PI/2`가 진행 방향 0도다.
- 최종 replicated transform writer는 `EventApplier::EnsureProjectilePresentation`이고 여기서 `ProjectileVisualDesc::fYawOffset`을 적용한다.
- `r_missile.wfx`의 emitter rotation은 local/non-replicated 초기 표현이므로 이번 변경에서 건드리지 않는다.

정적 빌드는 방향을 증명하지 못한다. 인게임에서 동/서/남/북 및 대각 2방향 총 6회 발사 acceptance가 최종 판정이다.

## 2-8. V3 — Sylas passive BA 가설별 수정 분기

코드를 먼저 더 바꾸지 않고 V0 trace와 BP로 아래 한 행을 확정한다.

| 관찰 | 판정 | 그때의 최소 수정 |
|---|---|---|
| `[ServerBA] consumed=0 stage=1` | stack arm/expire/consume 문제 | `ArmPassiveOnSkillCast`와 `TryConsumePassiveBasicAttack`만 수정; JSON maxStacks/window는 유지 |
| 서버 `stage=2`, Client ActionStart가 `stage=1` | wire/snapshot stage 전달 문제 | serializer/snapshot assignment만 수정 |
| Client `stage=2`, `stage2Key=-` | registration/SkillRegistry presentation merge 문제 | Sylas BA `stage2AnimKey` 보존 지점만 수정 |
| `selected=skinned_mesh_sylas_attack_passive played=0` | animator asset lookup 실패 | cooked asset 이름/loader lookup만 수정; gameplay 불변 |
| `played=1` 직후 일반 BA/idle로 바뀜 | 두 번째 animation writer가 덮어씀 | 덮어쓴 호출에 action sequence/stage guard 추가 |
| Client Action PASS, Client Cue가 없거나 `stage=1` | EffectTrigger stage 문제 | CombatAction EffectTrigger flags/dispatch만 수정 |
| Client Cue `stage=2 visual=1`, 화면 FX 없음 | WFX spawn/render 문제 | `CFxCuePlayer::PlayAll` 결과와 spawned entity/render component만 수정 |

현재 정적 증거는 다음을 이미 통과한다.

- canonical Sylas passive: `maxStacks=3`, `stackWindowSec=5`
- 서버 accepted skill 후 `ArmPassiveOnSkillCast`
- BA에서 `TryConsumePassiveBasicAttack` 후 `attackActionStage=2`
- `CombatActionFlags::SylasPassive` 광역 피해
- stage 2 EffectTrigger
- Client registration의 `stage2AnimKey="skinned_mesh_sylas_attack_passive"`
- `Client/Bin/Resource/Texture/Character/Sylas/anims/skinned_mesh_sylas_attack_passive.wanim` 실재
- Client visual JSON의 Sylas BA stage 2 timing
- SimLab `--sylas-passive-only`의 stack/stage/cue 계약

따라서 이 항목을 “Data 수치가 없어서”라고 단정하고 중복 수치를 넣는 것은 금지한다.

## 2-9. Sylas BP 좌표

1. `Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`
   - `SylasGameSim::ArmPassiveOnSkillCast`
   - `SylasGameSim::TryConsumePassiveBasicAttack`
   - `action.uStage = attackActionStage`
   - `StartCommandActionState(... attackActionStage)`
2. `Shared/GameSim/Systems/Combat/CombatActionSystem.cpp`
   - `effectEvent.flags = ... stage ...`
3. `Shared/GameSim/Systems/ReplicatedEventSerializer/ReplicatedEventSerializer.cpp`
   - `CreateActionStartEvent(... action.stage ...)`
4. `Client/Private/Network/Client/EventApplier.cpp`
   - `ApplyActionStart`: `actionStage = ev->actionStage()`
   - `PlayReplicatedActionVisual`: `pAnimKey`, `animName`, `bPlayed`
   - EffectTrigger의 `skillStage`와 `CVisualHookRegistry::Dispatch`
5. `Engine/Private/Renderer/ModelRenderer.cpp`
   - `ModelRenderer::PlayAnimationByNameAdvanced`
6. `Client/Private/GameObject/FX/FxCuePlayer.cpp`
   - `CFxCuePlayer::PlayAll("Sylas.PassiveBA.Hit", ...)`

## 2-10. generator 산출물

canonical 수정 후 아래 파일은 generator가 갱신한다. 손편집하지 않는다.

- `Shared/GameSim/Generated/ChampionGameData.generated.cpp`
- `Server/Private/Data/Generated/LoLGameplayDefinitions.generated.cpp`
- definition manifest/parity 산출물

실행 순서:

```powershell
python Tools/ChampionData/build_champion_game_data.py
python Tools/LoLData/Build-LoLDefinitionPack.py
python Tools/ChampionData/build_champion_game_data.py --check
python Tools/LoLData/Build-LoLDefinitionPack.py --check
python Tools/LoLData/Test-ChampionGameDataSchema.py
```

# 3. 검증 및 완료 조건

## 3-1. 명령 실행 전에 예상 관찰을 고정

### Irelia E

- E1: `[IreliaReplayCue] slot=3 stage=1 visual=1`, 첫 blade FBX 1개.
- E2: `slot=3 stage=2 visual=1`, 두 번째 blade FBX, 두 blade 연결 FX, 실제 server hit 대상 위 mark.
- E2 miss: blade/connect는 보이되 적 mark는 없어야 한다.
- R: 기존 blade FBX와 Q mark는 변화가 없어야 한다.

### Viego W

- tap range는 2.5, full charge range는 5.0으로 단조 증가한다.
- tap stun은 0.25초, full charge stun은 2.0초다.
- W1 hold loop, release, 이동 복구는 현재 PASS 상태를 유지한다.

### Viego R

- 3m 앞 클릭 시 dash endpoint, damage circle, slow circle 중심이 모두 3m 앞이다.
- 6m 밖 클릭 시 endpoint는 최대 6m로 clamp된다.
- 클릭 중심 반경 2.0 안의 적만 피해/slow를 받고, 과거의 무조건 max-range 중심 적은 받지 않는다.

### Sylas passive BA

- 스킬 1회 accepted 후 passive stack 1.
- BA 시작 시 server `stage=2`, client `selected=skinned_mesh_sylas_attack_passive played=1`.
- impact 시 `ClientCue stage=2 visual=1`과 `Sylas.PassiveBA.Hit` FX 1회.
- stack이 0이면 일반 BA stage 1/일반 anim/일반 cue.

### Ezreal R

- projectile 이동 벡터와 crescent/mesh 전방이 같은 방향.
- +X, -X, +Z, -Z, 대각선 2방향에서 90도 오차가 없어야 한다.

## 3-2. 정적·focused 자동 검증

```powershell
python Tools/LoLData/Test-ChampionGameDataSchema.py
python Tools/ChampionData/build_champion_game_data.py --check
python Tools/LoLData/Build-LoLDefinitionPack.py --check

$msbuild = "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
& $msbuild Tools/SimLab/SimLab.vcxproj /m:1 /p:Configuration=Debug /p:Platform=x64
& Tools/Bin/Debug/SimLab.exe --stage-input-only
& Tools/Bin/Debug/SimLab.exe --sylas-passive-only
```

예상 PASS:

```text
[SimLab][ChargeContract] PASS
[SimLab][ViegoRLanding] PASS
[SimLab][IreliaWRelease] PASS
[SimLab][SylasPassive] PASS
[SimLab] PASS
```

## 3-3. Debug 빌드

```powershell
$msbuild = "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
& $msbuild Shared/GameSim/Include/GameSim.vcxproj /m:1 /p:Configuration=Debug /p:Platform=x64
& $msbuild Server/Include/Server.vcxproj /m:1 /p:Configuration=Debug /p:Platform=x64
& $msbuild Client/Include/Client.vcxproj /m:1 /p:Configuration=Debug /p:Platform=x64
```

완료 조건은 error 0이며, 이전 경고 수 증가는 별도로 기록한다. `Client/Bin/Resource`만 runtime resource source로 사용한다.

## 3-4. Debug 인게임 수동 acceptance

한 경기에서 섞어 보지 말고 아래 순서로 짧게 분리한다.

1. Irelia: E1 설치 → E2 적중 → E2 빗나감 → R → Q mark.
2. Irelia: W tap/hold/release 직후 이동/E를 한 번 더 확인해 기존 PASS 보존.
3. Viego: W tap/중간/full charge의 거리와 stun 시간 계측.
4. Viego: R을 2m, 4m, 최대거리 밖에 클릭하고 landing/damage/slow 중심 확인.
5. Sylas: stack 0 BA → Q 후 BA → 3 stack 후 BA 3회. trace와 화면을 같은 순서로 저장.
6. Ezreal: R을 6방향 발사해 projectile 진행 방향 캡처.

수동 acceptance에서 실패하면 해당 항목만 되돌아간다. 다른 PASS 항목의 수치나 구조를 같이 바꾸지 않는다.

## 3-5. Release 전체 gate

Debug 인게임 acceptance 뒤에만 실행한다.

```powershell
powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1 -Configuration Debug -RequireComplete
powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1 -Configuration Release -RequireComplete
```

필수 기대값:

- Data-Driven goal `12/12`
- champion/skill/stage ordered contract 수 유지: `17 / 85 / 98`
- Irelia W2 move delta 유지
- Viego W authored range/stun 변경만 expected contract delta로 기록
- Debug/Release GameSim, SimLab, Server, Client compile/link error 0
- deterministic same-seed hash는 새 gameplay tuning 때문에 바뀔 수 있으나 동일 seed 두 실행은 같아야 한다.

## 3-6. diff 검증

dirty worktree 전체에는 사용자 변경이 많으므로 이번 대상 파일만 먼저 검사한다.

```powershell
git diff --check -- `
  Client/Private/GameObject/Champion/Irelia/Irelia_Skills.cpp `
  Client/Private/Network/Client/EventApplier.cpp `
  Client/Private/GameObject/Projectile/ProjectileVisualCatalog.cpp `
  Data/Gameplay/ChampionGameData/champions.json `
  Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json `
  Shared/GameSim/Champions/Viego/ViegoGameSim.cpp `
  Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp `
  Tools/SimLab/main.cpp
```

## 3-7. 결과 문서와 완료 선언

구현 후 아래 문서를 새로 작성한다.

`C:/Users/user/Desktop/Winters/.md/plan/2026-07-18_IRELIA_VIEGO_SYLAS_EZREAL_RUNTIME_ACCEPTANCE_RECOVERY_RESULT.md`

반드시 기록할 것:

- 실제 수정한 파일과 최종 수치
- generator hash와 ordered contract hash
- focused SimLab/Debug/Release gate의 실제 출력
- Irelia E1/E2 blade/mark 인게임 결과
- Viego W tap/full range·stun 실측
- Viego R landing/damage/slow 중심 실측
- Sylas trace 네 단계와 선택된 가설, 실제 수정 위치
- Ezreal R 6방향 캡처 결과
- 남은 미검증 항목

자동 테스트와 빌드만 통과한 상태에서는 완료가 아니다. 이번 세션의 완료 조건은 **네 묶음의 Debug 인게임 acceptance + Release pipeline PASS**다.
