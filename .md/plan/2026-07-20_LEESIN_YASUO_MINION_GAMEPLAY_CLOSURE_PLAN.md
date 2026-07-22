Session - 리 신 Q/E/R 권위 폐쇄, ward·칼리스타·objective 표현 정리, siege 1.5배, 포탑 인접 미니언 교착을 한 gameplay slice로 구현·검증
좌표: 신규 좌표 후보 · 축: C7 권위와 정합성, C8 검증이 병목
관련: 2026-07-20_LEESIN_YASUO_MINION_GAMEPLAY_CLOSURE_HANDOFF.md, 2026-07-19_CHAMPION_DAMAGE_DATA_AUTHORITY_CLOSURE_PLAN.md, 2026-07-19_CHAMPION_DAMAGE_DATA_AUTHORITY_CLOSURE_RESULT.md

## 1. 결정 기록

① 문제·제약: Q1 24→19.2, terrain 무시/barrier 유지, Q2 mark-hit 기준 3초·0.18초 wall-transit, E1 반경 3.5 피해/E2 hit-only slow, R 10m walkable landing을 server truth로 닫는다.
② 순진한 해법의 실패: client hover를 Q2 truth로 두거나 hook 내부에서만 reject하면 generic executor가 stage를 먼저 소비한다. Bot AI가 mark·이동·피해를 직접 고치면 `Input→GameCommand→GameSim` 경계를 깨뜨린다.
③ 메커니즘: executor가 commit 전에 유일한 source-owned Q mark를 resolve하고 effective command를 만든다. projectile contact가 server/client stage mirror를 mark 시점으로 재무장하고 GameSim만 dash/damage/status를 생성한다.
④ 대조: Lee Q2만 transit terrain clamp를 생략하되 도착 walkable snap은 유지한다. W dash와 R knockback은 terrain을 관통하지 않는다. projectile barrier는 계속 Q1을 파괴한다.
⑤ 대가: Lee E hit mark 1종이 checkpoint 대상에 추가되고 Q hit client mirror가 1곳 늘어난다. 시각 smoke는 build/probe만으로 완료 판정하지 않는다.
① 문제·제약: siege blue/red mesh만 1.5배이고 collision/spatial은 1.0이다. ward는 Q mark ground texture를 작게, Kalista W는 cone만 남긴다.
③ 메커니즘: ClientPublic minion visual definition에 기본 1.0 배율을 cook하고 local/network transform에만 곱한다. 기존 Baron render multiplier는 최종 world matrix에서 추가 곱으로 유지한다.
⑤ 대가: global Minion Tuner scale 변경 시에도 role multiplier를 다시 적용해야 한다.
① 문제·제약: red-mid-outer 포탑 인접 2 melee 교착은 실제 위치 실측이 없지만 current mixed static+soft depenetration에서 상쇄 가능한 코드 근거가 있다.
③ 메커니즘: soft push를 항상 forward-safe로 정규화한 뒤 static/dynamic push와 결합하고, 1차 nav clamp가 실패·무이동이면 static surface의 결정적 tangent 후보를 1회 시도한다.
④ 대조: allied minion을 hard blocker로 승격하지 않고 기존 soft separation 반경/가중치를 보존한다.
⑤ 대가: exact live 위치 캡처 전에는 원인 확정이 아니라 강한 후보로만 판결한다.

## 예산

- 바닥 70%: A Lee Q/Q2+bot probe, B Lee E/R, C ward/Kalista/objective, D siege scale, E minion resolver, generator/probe/Debug build.
- 천장 30%: source-owned stage/mark 재사용, deterministic policy helper, authoring→generated→runtime 계약을 재사용 가능한 검증으로 환전한다.

## 인터뷰 확정값

1. Q2는 cursor와 무관하게 server가 정확히 하나인 유효 Q1 mark를 선택한다. 없거나 모호하면 stage를 소비하지 않는다.
2. Q1은 terrain/nav wall을 통과하고 champion projectile barrier에는 막힌다.
3. Q1 speed만 19.2, Q2 duration은 0.18이다.
4. E1은 반경 3.5 적 mobile unit physical AoE, E2는 E1-hit 생존 대상만 slow, mark는 3초 stage window와 함께 만료한다.
5. R은 Lee→target 방향 10 world units, terrain 관통 없이 마지막/nearest walkable 착지다.
6. blue/red siege visual만 1.5배다.
7. ward는 작은 Lee Q mark ground 표현, Kalista는 moving cone만 유지한다.
8. Baron/Elder ground decal은 유지하며 follow/death prune 계약을 검증한다.
9. replay 세션과 같은 checkout에서 병렬 진행한다. 각 묶음 직전 target hash/process를 재확인하고 build는 `/m:1` 순차 실행한다.

## 현재 코드 증거와 소유 경계

- 시작 branch는 `codex/2026-07-16-replay-backend-worktree`; `MSBuild/cl/link/Server/Client`는 계획 작성 직전 0개다.
- 2026-07-20 추가 사용자 결정에 따라 전 챔피언의 variant flat damage를 `ByRank[5]`로 확장하는 광범위 F4 폐쇄는 병렬 세션 소유다. 이 slice는 Yasuo E 0.1과 현재 Yasuo/Lee Q 편집 경로의 계약 확인까지만 포함하며, 광범위 F4 데이터·UI 구현을 덮어쓰지 않는다.
- replay 직접 소유는 Replay/Backend/UI timeline이며, 이번 target과 겹치는 `EventApplier.cpp`, `SnapshotApplier.cpp`에는 replay 및 gameplay hunk가 함께 있다. 해당 파일은 최신 anchor를 다시 읽고 함수 내부 최소 hunk만 적용한다.
- `CommandExecutor.cpp`, `GameRoomUnitAI.cpp`, `Minion_Manager.cpp`, `Tools/SimLab/main.cpp`, projectile probe, generator/data/generated pack도 기존 gameplay 변경이 크다. reset/checkout/정렬/인접 리팩터링을 하지 않는다.
- `git diff --check`의 기존 `Scene_InGameInput.cpp:244` trailing whitespace는 이번 slice 밖이며 건드리지 않는다.
- Bot AI는 `GameCommand`만 생산한다. mark 선택, stage 소비, damage, dash, status, knockback은 executor/GameSim만 소유한다.

## 2. 반영해야 하는 코드

### 2-1. `Shared/GameSim/Components/LeeSinSimComponent.h`

기존 `LeeSinDashComponent` 블록을 아래로 교체하고 E mark를 바로 아래에 추가한다.

```cpp
struct LeeSinDashComponent
{
	Vec3 vStart{};
	Vec3 vEnd{};
	f32_t fElapsedSec = 0.f;
	f32_t fDurationSec = 0.18f;
	bool_t bIgnoreTerrainDuringTransit = false;
};

struct LeeSinTempestMarkComponent
{
	EntityID sourceEntity = NULL_ENTITY;
	f32_t fRemainingSec = 0.f;
};
```

### 2-2. `Shared/GameSim/Core/Checkpoint/WorldKeyframe.cpp`

기존 Lee 등록 블록에서 `LeeSinQMarkComponent` 아래에 추가한다.

```cpp
		reg.Register<LeeSinTempestMarkComponent>("LeeSinTempestMarkComponent");
```

### 2-3. `Shared/GameSim/Champions/LeeSin/LeeSinGameSim.h`

`CanCastDragonRage` 선언 아래에 추가한다.

```cpp
    EntityID ResolveSonicWaveMarkTarget(
        CWorld& world,
        EntityID caster);
```

### 2-4. `Shared/GameSim/Champions/LeeSin/LeeSinGameSim.cpp`

`StartTargetDash` signature와 dash 작성부를 아래 계약으로 교체한다. W는 기본 false, Q2만 true를 넘긴다.

```cpp
    void StartTargetDash(
        CWorld& world,
        EntityID caster,
        EntityID target,
        f32_t gap,
        f32_t durationSec,
        bool_t bIgnoreTerrainDuringTransit = false)
    {
        if (!world.HasComponent<TransformComponent>(caster) ||
            target == NULL_ENTITY ||
            !world.HasComponent<TransformComponent>(target))
        {
            return;
        }

        const Vec3 start = world.GetComponent<TransformComponent>(caster).GetPosition();
        const Vec3 targetPos = world.GetComponent<TransformComponent>(target).GetPosition();
        const Vec3 dir = WintersMath::NormalizeXZ(Vec3{
            targetPos.x - start.x, 0.f, targetPos.z - start.z });
        const f32_t dist = std::sqrt(WintersMath::DistanceSqXZ(start, targetPos));
        const f32_t moveDist = (std::max)(0.f, dist - gap);

        LeeSinDashComponent dash{};
        dash.vStart = start;
        dash.vEnd = Vec3{
            start.x + dir.x * moveDist,
            start.y,
            start.z + dir.z * moveDist };
        dash.fDurationSec = durationSec;
        dash.bIgnoreTerrainDuringTransit = bIgnoreTerrainDuringTransit;

        if (world.HasComponent<LeeSinDashComponent>(caster))
            world.GetComponent<LeeSinDashComponent>(caster) = dash;
        else
            world.AddComponent<LeeSinDashComponent>(caster, dash);

        RotateToward(world, caster, dir);
        ClearMove(world, caster);
    }
```

`OnQ`는 command target을 이미 executor가 resolve한 것으로 검증하고, 호출을 아래처럼 바꾼다.

```cpp
        StartTargetDash(
            *ctx.pWorld,
            ctx.casterEntity,
            target,
            qDashGap,
            qDashDurationSec,
            true);
```

기존 `ApplyTempestCrippleSlow`와 `OnE`를 아래 동작으로 교체한다: E1은 `CollectEnemyMobileUnitsInCircle` 결과 각각에 `EnqueuePhysicalDamage(..., 0.f, E, rank)`를 넣어 canonical 200+1.0 total AD formula를 queue가 적용하게 하고 `LeeSinTempestMarkComponent{caster, stageWindow}`를 upsert한다. E2는 deterministic component iteration에서 source가 caster인 살아 있는 mark만 slow하고 mark를 제거한다. `EnqueuePhysicalDamage`에는 아래 metadata를 추가한다.

```cpp
        request.iSourceSlot = slot;
        request.eSourceKind = eDamageSourceKind::Skill;
```

R landing helper를 `OnR` 앞에 추가한다.

```cpp
    Vec3 ResolveDragonRageLanding(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        EntityID target)
    {
        const Vec3 casterPos =
            world.GetComponent<TransformComponent>(caster).GetPosition();
        const Vec3 targetPos =
            world.GetComponent<TransformComponent>(target).GetPosition();
        const Vec3 direction = WintersMath::DirectionXZ(
            casterPos, targetPos, Vec3{ 0.f, 0.f, 1.f }, 0.0001f);
        const Vec3 desired{
            targetPos.x + direction.x * 10.f,
            targetPos.y,
            targetPos.z + direction.z * 10.f };
        if (!tc.pWalkable)
            return desired;

        Vec3 landing = targetPos;
        const f32_t radius = GameplayStateQuery::ResolveGameplayRadius(world, target);
        if (!tc.pWalkable->TryClampMoveSegmentXZ(
                targetPos, desired, radius, landing))
        {
            landing = targetPos;
        }
        if (!tc.pWalkable->IsWalkableXZ(landing))
        {
            Vec3 resolved = landing;
            if (tc.pWalkable->TryResolveMoveTarget(targetPos, landing, resolved))
                landing = resolved;
        }
        f32_t surfaceY = landing.y;
        if (tc.pWalkable->TrySampleHeight(landing.x, landing.z, surfaceY))
            landing.y = surfaceY;
        return landing;
    }
```

`OnR`의 `ApplyAirborne` 호출은 `landing` pointer를 전달한다. `Tick`은 `bIgnoreTerrainDuringTransit`일 때 보간 위치를 직접 적용하고 끝에서 기존 `SnapDashArrivalToWalkable`을 실행한다. W는 기존 per-tick clamp를 유지한다. Q/E mark 만료 loop를 둘 다 처리한다.

`namespace LeeSinGameSim`에 아래 resolver를 추가한다.

```cpp
    EntityID ResolveSonicWaveMarkTarget(CWorld& world, EntityID caster)
    {
        EntityID resolved = NULL_ENTITY;
        const auto targets =
            DeterministicEntityIterator<LeeSinQMarkComponent>::CollectSorted(world);
        for (EntityID target : targets)
        {
            const LeeSinQMarkComponent& mark =
                world.GetComponent<LeeSinQMarkComponent>(target);
            if (mark.sourceEntity != caster ||
                mark.fRemainingSec <= 0.f ||
                !world.IsAlive(target) ||
                !GameplayStateQuery::CanBeTargetedBy(world, caster, target))
            {
                continue;
            }
            if (resolved != NULL_ENTITY)
                return NULL_ENTITY;
            resolved = target;
        }
        return resolved;
    }
```

`ApplySonicWaveMark`는 mark upsert 뒤 source Q slot을 mark duration으로 재무장한다.

```cpp
        if (world.HasComponent<SkillStateComponent>(source))
        {
            auto& qSlot = world.GetComponent<SkillStateComponent>(source)
                .slots[static_cast<u8_t>(eSkillSlot::Q)];
            qSlot.currentStage = 1u;
            qSlot.stageWindow = markDurationSec;
        }
```

`ApplyTempestCrippleSlow` 전체와 `OnE` 전체는 아래로 교체한다. E1 damage amount 0은 canonical flat+AD formula를 `DamageQueueSystem`이 적용하게 하는 값이며, `EnqueuePhysicalDamage`의 source metadata는 이 계획 앞부분의 두 줄을 반드시 포함한다.

```cpp
    void OnE(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pTickCtx || !ctx.pCommand)
            return;

        CWorld& world = *ctx.pWorld;
        const TickContext& tc = *ctx.pTickCtx;
        if (ctx.pCommand->itemId != 2u)
        {
            if (!world.HasComponent<TransformComponent>(ctx.casterEntity))
                return;
            const Vec3 origin =
                world.GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
            const f32_t radius = ResolveLeeSinSkillEffectParam(
                ctx, eSkillSlot::E, eSkillEffectParamId::Radius);
            const f32_t stageWindowSec =
                GameplayDefinitionQuery::ResolveSkillStageWindowSec(
                    world, ctx.casterEntity, tc, eChampion::LEESIN,
                    static_cast<u8_t>(eSkillSlot::E));
            const std::vector<EntityID> targets =
                GameplayStateQuery::CollectEnemyMobileUnitsInCircle(
                    world, ctx.casterEntity, origin, radius);
            for (EntityID target : targets)
            {
                EnqueuePhysicalDamage(
                    world, ctx.casterEntity, target, ctx.casterTeam, 0.f,
                    static_cast<u8_t>(eSkillSlot::E), ctx.skillRank);
                LeeSinTempestMarkComponent mark{};
                mark.sourceEntity = ctx.casterEntity;
                mark.fRemainingSec = stageWindowSec;
                if (world.HasComponent<LeeSinTempestMarkComponent>(target))
                    world.GetComponent<LeeSinTempestMarkComponent>(target) = mark;
                else
                    world.AddComponent<LeeSinTempestMarkComponent>(target, mark);
            }
            return;
        }

        const f32_t slowDurationSec = ResolveLeeSinSkillEffectParam(
            ctx, eSkillSlot::E, eSkillEffectParamId::SlowDurationSec);
        const f32_t slowMoveSpeedMul = ResolveLeeSinSkillEffectParam(
            ctx, eSkillSlot::E, eSkillEffectParamId::MoveSpeedMul);
        const std::vector<EntityID> marked =
            DeterministicEntityIterator<LeeSinTempestMarkComponent>::CollectSorted(world);
        for (EntityID target : marked)
        {
            const LeeSinTempestMarkComponent& mark =
                world.GetComponent<LeeSinTempestMarkComponent>(target);
            if (mark.sourceEntity != ctx.casterEntity ||
                mark.fRemainingSec <= 0.f ||
                !GameplayStateQuery::CanReceiveCrowdControl(
                    world, ctx.casterEntity, target))
            {
                continue;
            }
            GameplayStatus::ApplySlow(
                world, tc, target, ctx.casterEntity, eChampion::LEESIN,
                eSkillSlot::E, slowDurationSec, slowMoveSpeedMul);
            world.RemoveComponent<LeeSinTempestMarkComponent>(target);
        }
    }
```

`OnR`의 기존 `GameplayStatus::ApplyAirborne(...)` 호출 바로 위에 `const Vec3 landing = ResolveDragonRageLanding(...)`을 두고 호출을 아래로 교체한다.

```cpp
        const Vec3 landing = ResolveDragonRageLanding(
            world, *ctx.pTickCtx, ctx.casterEntity, target);
        GameplayStatus::ApplyAirborne(
            world,
            *ctx.pTickCtx,
            target,
            ctx.casterEntity,
            eChampion::LEESIN,
            eSkillSlot::R,
            rAirborneDurationSec,
            2.1f,
            &landing);
```

`Tick`의 `Vec3 guardedPosition = position;`부터 `finishedDashes.push_back` 조건까지를 아래로 교체한다. 따라서 W는 현행 per-tick clamp, Q2만 transit clamp bypass이며 공통 completion path의 `SnapDashArrivalToWalkable`은 유지된다.

```cpp
                    Vec3 guardedPosition = position;
                    bool_t bDashBlocked = false;
                    if (tc.pWalkable && !dash.bIgnoreTerrainDuringTransit)
                    {
                        const Vec3 currentPos = transform.GetLocalPosition();
                        if (!tc.pWalkable->TryClampMoveSegmentXZ(
                                currentPos, position, 0.5f, guardedPosition))
                        {
                            guardedPosition = currentPos;
                            bDashBlocked = true;
                        }
                        else if (WintersMath::DistanceSqXZ(
                                     guardedPosition, position) > 0.0001f)
                        {
                            bDashBlocked = true;
                        }
                    }

                    transform.SetPosition(guardedPosition);
                    if (bDashBlocked && t < 1.f)
                        finishedDashes.push_back(entity);
```

Q mark removal loop 바로 뒤에 아래 exact block을 추가한다. Q1 hit의 `ApplySonicWaveMark`가 source Q slot을 stage=1/window=markDuration으로 재무장하며, Q2 accepted path만 mark를 소비한다.

```cpp
        std::vector<EntityID> expiredTempestMarks;
        world.ForEach<LeeSinTempestMarkComponent>(
            std::function<void(EntityID, LeeSinTempestMarkComponent&)>(
                [&](EntityID entity, LeeSinTempestMarkComponent& mark)
                {
                    mark.fRemainingSec =
                        std::max(0.f, mark.fRemainingSec - tc.fDt);
                    if (mark.fRemainingSec <= 0.f ||
                        !world.IsAlive(mark.sourceEntity))
                    {
                        expiredTempestMarks.push_back(entity);
                    }
                }));

        for (EntityID entity : expiredTempestMarks)
            world.RemoveComponent<LeeSinTempestMarkComponent>(entity);
```

### 2-5. `Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`

`CanCast` check 다음의 두 generic target check와 뒤쪽 `champion` 선언까지를 아래 블록으로 교체한다. native Lee Q2만 caller-supplied target을 신뢰하지 않고, 나머지 skill의 기존 검사는 그대로다.

```cpp
    const eChampion champion = ResolveChampion(world, cmd.issuerEntity);
    const bool_t bNativeLeeSinQ2Request =
        champion == eChampion::LEESIN &&
        cmd.slot == static_cast<u8_t>(eSkillSlot::Q) &&
        cmd.itemId == 2u;
    if (!bNativeLeeSinQ2Request &&
        cmd.targetEntity != NULL_ENTITY &&
        !IsAliveForCommand(world, cmd.targetEntity))
    {
        LogCastSkill("reject", "dead-target", cmd, champion, 0.f);
        return CommandExecutionResult::Rejected(
            cmd.sequenceNum,
            eCommandExecutionReason::DeadTarget);
    }
    if (!bNativeLeeSinQ2Request &&
        cmd.targetEntity != NULL_ENTITY &&
        !GameplayStateQuery::CanBeTargetedBy(
            world, cmd.issuerEntity, cmd.targetEntity))
    {
        LogCastSkill("reject", "untargetable", cmd, champion, 0.f);
        return CommandExecutionResult::Rejected(
            cmd.sequenceNum,
            eCommandExecutionReason::UntargetableTarget);
    }

    auto& skillState = world.GetComponent<SkillStateComponent>(cmd.issuerEntity);
```

기존 `auto& skillState = ...`와 `const eChampion champion = ...` 두 줄은 삭제하고 `ResolveSkill`부터 이어간다.

`GameCommand effectiveCmd = cmd;` 바로 아래, commit 전 아래를 추가한다.

```cpp
    if (bStage2 &&
        hookChampion == eChampion::LEESIN &&
        hookSlot == static_cast<u8_t>(eSkillSlot::Q))
    {
        const EntityID markedTarget =
            LeeSinGameSim::ResolveSonicWaveMarkTarget(
                world, cmd.issuerEntity);
        if (markedTarget == NULL_ENTITY)
        {
            LogCastSkill("reject", "sonic-wave-mark", cmd, hookChampion, slot.stageWindow);
            return CommandExecutionResult::Rejected(
                cmd.sequenceNum,
                eCommandExecutionReason::ChampionRuleBlocked);
        }
        effectiveCmd.targetEntity = markedTarget;
    }
```

`SpawnServerSkillProjectile`의 Lee Q component는 아래 필드를 갖는다.

```cpp
        projectile.speed = 19.2f;
        projectile.bCollidesWithTerrain = false;
        projectile.bBlockedByProjectileBarriers = true;
```

### 2-6. `Client/Private/Scene/Scene_InGameLocalSkills.cpp`

`BuildCastCommand`의 `UnitTarget` 첫 줄에 아래를 추가한다. Q2 target null은 server resolve 요청이며 local gameplay truth가 아니다.

```cpp
        if (GetPlayerChampionId() == eChampion::LEESIN &&
            outCmd.slot == static_cast<u8_t>(eSkillSlot::Q) &&
            skillStage >= 2u)
        {
            outCmd.targetEntityId = NULL_ENTITY;
            return true;
        }
```

### 2-7. `Client/Private/Network/Client/EventApplier.cpp`

`ApplyProjectileHit`에서 Lee Q authoritative UnitHit를 받은 직후 owner가 `LocalPlayerTag` Lee이고 Q SkillState가 있으면 ClientPublic stage window로 local mirror를 갱신한다.

```cpp
    if (ev->kind() == static_cast<u16_t>(eProjectileKind::LeeSinQ) &&
        ev->contactReason() == Shared::Schema::ProjectileContactReason::UnitHit)
    {
        const EntityID owner = ResolveLiveEntity(world, entityMap, ev->ownerNet());
        const EntityID local = FindLocalPlayerEntity(world);
        if (owner != NULL_ENTITY && owner == local &&
            world.HasComponent<SkillStateComponent>(owner))
        {
            SkillGameAtomBundle gameData{};
            const u8_t qSlot = static_cast<u8_t>(eSkillSlot::Q);
            if (CSkillRegistry::Instance().ResolveGameAtoms(
                    eChampion::LEESIN, qSlot, gameData))
            {
                auto& slot = world.GetComponent<SkillStateComponent>(owner).slots[qSlot];
                slot.currentStage = 1u;
                slot.stageWindow = gameData.stage.stageWindowSec;
            }
        }
    }
```

이 mirror는 projectile hit event의 unique-key dedupe 이후에만 실행한다. live/replay 모두 `ApplyProjectileHit` 순서를 따르되, 이후 도착한 authoritative command result/snapshot이 더 최신 truth다. replay rewind는 `RebaseTimeline` 뒤 snapshot truth에서 다시 시작한다. SimLab은 Client를 링크하지 않으므로 client method 직접 호출을 주장하지 않는다. GameRoom probe가 rejected Q2의 server `SkillCommandFeedback` stage/window 보존을 실행 검증하고, `Test-F4BalanceContracts.py`가 `Scene_InGameNetwork.cpp`의 `OnAuthoritativeCommandResult` stage/window assignment와 `RebaseNetworkTimeline` sequence reset을 정적 계약으로 확인하며, Client `/m:1` build를 gate로 둔다.

### 2-8. `Tools/Harness/GameRoomProjectileIntegrationProbe.cpp`

include에 `LeeSinGameSim.h`, `LeeSinSimComponent.h`, `SkillRankComponent.h`를 추가한다. `CGameRoomIntegrationProbeAccess`에 아래 두 method를 추가한다.

```cpp
    static void RunCommandPhase(CGameRoom& room, TickContext& tc)
    {
        room.Phase_ExecuteCommands(tc);
    }

    static const SkillCommandFeedback* FindFeedback(
        CGameRoom& room, u32_t sessionId, u8_t slot)
    {
        const auto found = room.m_lastCommandFeedbackBySession.find(sessionId);
        if (found == room.m_lastCommandFeedbackBySession.end() || slot >= 5u)
            return nullptr;
        return &found->second[slot];
    }
```

기존 lifecycle probe들 아래에 다음 function을 추가한다. Q1 projectile은 최대 8 fixed tick 동안 real GameRoom projectile phase를 통과시키고 UnitHit/mark를 요구한다.

```cpp
    bool_t CheckLeeSinQAuthorityLifecycle()
    {
        constexpr u32_t kSessionId = 77u;
        constexpr u8_t kQSlot = static_cast<u8_t>(eSkillSlot::Q);
        auto room = CGameRoomIntegrationProbeAccess::CreateRoom(9110u);
        CGameRoomIntegrationProbeAccess::EnterInGame(*room);
        CGameRoomIntegrationProbeAccess::SetExecutor(
            *room, CDefaultCommandExecutor::Create());
        CWorld& world = CGameRoomIntegrationProbeAccess::World(*room);
        EntityIdMap& entityMap = CGameRoomIntegrationProbeAccess::EntityMap(*room);
        LeeSinGameSim::RegisterHooks();

        const EntityID source = SpawnChampion(
            world, entityMap, eChampion::LEESIN, eTeam::Blue, Vec3{});
        const EntityID target = SpawnChampion(
            world, entityMap, eChampion::EZREAL, eTeam::Red,
            Vec3{ 0.2f, 0.f, 0.f });
        if (source == NULL_ENTITY || target == NULL_ENTITY)
            return false;
        world.AddComponent<SkillStateComponent>(source, SkillStateComponent{});
        SkillRankComponent ranks{};
        ranks.ranks[kQSlot] = 1u;
        world.AddComponent<SkillRankComponent>(source, ranks);
        world.GetComponent<ChampionComponent>(source).mana = 1000.f;
        if (!RebuildSpatialIndex(world))
            return false;

        GameCommand q1{};
        q1.kind = eCommandKind::CastSkill;
        q1.issuerEntity = source;
        q1.sourceSessionId = kSessionId;
        q1.sequenceNum = 1u;
        q1.slot = kQSlot;
        q1.itemId = 1u;
        q1.groundPos = Vec3{ 5.f, 0.f, 0.f };
        q1.direction = Vec3{ 1.f, 0.f, 0.f };
        TickContext castTick = MakeTickContext(*room, 100u);
        CGameRoomIntegrationProbeAccess::PushCommand(*room, q1);
        CGameRoomIntegrationProbeAccess::RunCommandPhase(*room, castTick);

        auto projectiles =
            DeterministicEntityIterator<SkillProjectileComponent>::CollectSorted(world);
        if (projectiles.size() != 1u)
            return false;
        const EntityID projectile = projectiles.front();
        const SkillProjectileComponent before =
            world.GetComponent<SkillProjectileComponent>(projectile);
        if (before.kind != eProjectileKind::LeeSinQ ||
            std::fabs(before.speed - 19.2f) > 0.0001f ||
            before.bCollidesWithTerrain ||
            !before.bBlockedByProjectileBarriers)
        {
            return false;
        }

        bool_t bUnitHit = false;
        for (u64_t tick = 101u; tick < 109u && !bUnitHit; ++tick)
        {
            TickContext projectileTick = MakeTickContext(*room, tick);
            CGameRoomIntegrationProbeAccess::RunProjectilePhase(
                *room, projectileTick);
            ReplicatedEventComponent hit{};
            bUnitHit = FindProjectileEvent(
                world, projectile, eReplicatedEventKind::ProjectileHit, hit) &&
                hit.eContactReason == ProjectileContactReason::UnitHit;
        }
        if (!bUnitHit ||
            !world.HasComponent<LeeSinQMarkComponent>(target) ||
            world.GetComponent<LeeSinQMarkComponent>(target).sourceEntity != source)
        {
            return false;
        }
        auto& qSlot = world.GetComponent<SkillStateComponent>(source).slots[kQSlot];
        if (qSlot.currentStage != 1u || qSlot.stageWindow <= 0.f)
            return false;

        GameCommand q2 = q1;
        q2.sequenceNum = 2u;
        q2.itemId = 2u;
        q2.targetEntity = NULL_ENTITY;
        TickContext q2Tick = MakeTickContext(*room, 109u);
        CGameRoomIntegrationProbeAccess::PushCommand(*room, q2);
        CGameRoomIntegrationProbeAccess::RunCommandPhase(*room, q2Tick);
        const SkillCommandFeedback* accepted =
            CGameRoomIntegrationProbeAccess::FindFeedback(
                *room, kSessionId, kQSlot);
        if (!accepted ||
            accepted->result.state != eCommandExecutionState::Accepted ||
            world.HasComponent<LeeSinQMarkComponent>(target) ||
            !world.HasComponent<LeeSinDashComponent>(source) ||
            !world.GetComponent<LeeSinDashComponent>(source)
                .bIgnoreTerrainDuringTransit)
        {
            return false;
        }

        qSlot.currentStage = 1u;
        qSlot.stageWindow = 2.f;
        q2.sequenceNum = 3u;
        TickContext rejectedTick = MakeTickContext(*room, 146u);
        CGameRoomIntegrationProbeAccess::PushCommand(*room, q2);
        CGameRoomIntegrationProbeAccess::RunCommandPhase(*room, rejectedTick);
        const SkillCommandFeedback* rejected =
            CGameRoomIntegrationProbeAccess::FindFeedback(
                *room, kSessionId, kQSlot);
        return rejected &&
            rejected->result.state == eCommandExecutionState::Rejected &&
            rejected->result.reason == eCommandExecutionReason::ChampionRuleBlocked &&
            rejected->authoritativeSkillStage == 1u &&
            rejected->stageWindowEndTick > rejectedTick.tickIndex &&
            qSlot.currentStage == 1u && qSlot.stageWindow > 0.f;
    }
```

`main`에 `bLeeSinQPass`를 추가하고 전체 `bPass`, printf label/argument에 포함한다.

### 2-9. `Tools/SimLab/main.cpp`

`RunLeeSinGameplayClosureProbe`를 아래 완전한 body로 추가한다. AI phase에서는 생성 command만 검사하며 Q mark/dash/projectile/damage truth가 새로 생기지 않았음을 확인한다. gameplay 변화는 executor 또는 Lee GameSim Tick 뒤에만 검사한다.

```cpp
    bool_t RunLeeSinGameplayClosureProbe()
    {
        constexpr u8_t kQ = static_cast<u8_t>(eSkillSlot::Q);
        constexpr u8_t kE = static_cast<u8_t>(eSkillSlot::E);
        constexpr u8_t kR = static_cast<u8_t>(eSkillSlot::R);
        const ChampionAIComboPlan& combo =
            GetChampionAIComboPlan(eChampion::LEESIN);
        if (combo.stepCount < 3u ||
            combo.steps[0].slot != kQ || combo.steps[0].itemId == 2u ||
            combo.steps[1].slot != kQ || combo.steps[1].itemId != 2u ||
            combo.steps[2].slot != static_cast<u8_t>(eSkillSlot::BasicAttack))
        {
            std::printf("[SimLab][LeeClosure] FAIL: Q1/Q2/BA combo contract\n");
            return false;
        }

        const auto CheckAIComboCommand = [&](u8_t comboStep)
        {
            CWorld world;
            DeterministicRng rng(2026072000ull + comboStep);
            EntityIdMap entityMap;
            FlatWalkable walkable;
            const EntityID lee = SpawnChampion(
                world, entityMap, eChampion::LEESIN,
                static_cast<u8_t>(eTeam::Blue), 0u);
            const EntityID enemy = SpawnChampion(
                world, entityMap, eChampion::EZREAL,
                static_cast<u8_t>(eTeam::Red), 5u);
            world.GetComponent<TransformComponent>(lee).SetPosition(Vec3{});
            world.GetComponent<TransformComponent>(enemy).SetPosition(
                Vec3{ 1.f, 0.f, 0.f });
            world.GetComponent<SkillRankComponent>(lee).ranks[kQ] = 1u;
            world.GetComponent<GoldComponent>(lee).amount = 0u;

            ChampionAIComponent ai{};
            ai.champion = eChampion::LEESIN;
            ai.team = eTeam::Blue;
            ai.state = eChampionAIState::LaneCombat;
            ai.intent = eChampionAIIntent::AttackChampion;
            ai.lockedChampion = enemy;
            ai.comboTarget = enemy;
            ai.comboStep = comboStep;
            ai.decisionTimer = 0.f;
            ai.championScanRange = 20.f;
            ai.fSkillCastCooldownTimer = 0.f;
            world.AddComponent<ChampionAIComponent>(lee, ai);

            if (comboStep == 1u)
            {
                auto& slot = world.GetComponent<SkillStateComponent>(lee).slots[kQ];
                slot.currentStage = 1u;
                slot.stageWindow = 3.f;
                LeeSinQMarkComponent mark{};
                mark.sourceEntity = lee;
                mark.fRemainingSec = 3.f;
                world.AddComponent<LeeSinQMarkComponent>(enemy, mark);
            }
            const f32_t hpBefore =
                world.GetComponent<HealthComponent>(enemy).fCurrent;
            const f32_t markBefore = comboStep == 1u
                ? world.GetComponent<LeeSinQMarkComponent>(enemy).fRemainingSec
                : 0.f;
            TickContext tc = MakeProbeTickContext(
                1ull, rng, entityMap, walkable);
            std::vector<GameCommand> commands;
            CChampionAISystem::Execute(world, tc, commands);
            if (commands.size() != 1u)
                return false;

            const GameCommand& command = commands.front();
            const bool_t bExpectedCommand = comboStep < 2u
                ? command.kind == eCommandKind::CastSkill &&
                    command.slot == kQ &&
                    command.itemId == combo.steps[comboStep].itemId
                : command.kind == eCommandKind::BasicAttack &&
                    command.targetEntity == enemy;
            const bool_t bAIKeptGameplayTruth =
                std::fabs(world.GetComponent<HealthComponent>(enemy).fCurrent -
                    hpBefore) <= 0.0001f &&
                DeterministicEntityIterator<SkillProjectileComponent>::CollectSorted(
                    world).empty() &&
                DeterministicEntityIterator<DamageRequestComponent>::CollectSorted(
                    world).empty() &&
                !world.HasComponent<LeeSinDashComponent>(lee) &&
                (comboStep != 1u ||
                    (world.HasComponent<LeeSinQMarkComponent>(enemy) &&
                     std::fabs(world.GetComponent<LeeSinQMarkComponent>(enemy)
                         .fRemainingSec - markBefore) <= 0.0001f));
            return bExpectedCommand && bAIKeptGameplayTruth;
        };

        if (!CheckAIComboCommand(0u) ||
            !CheckAIComboCommand(1u) ||
            !CheckAIComboCommand(2u))
        {
            std::printf("[SimLab][LeeClosure] FAIL: AI producer contract\n");
            return false;
        }

        {
            CWorld world;
            DeterministicRng rng(2026072010ull);
            EntityIdMap entityMap;
            FlatWalkable walkable;
            auto executor = CDefaultCommandExecutor::Create();
            const EntityID lee = SpawnChampion(
                world, entityMap, eChampion::LEESIN,
                static_cast<u8_t>(eTeam::Blue), 0u);
            const EntityID survivor = SpawnChampion(
                world, entityMap, eChampion::EZREAL,
                static_cast<u8_t>(eTeam::Red), 5u);
            const EntityID deadBeforeE2 = SpawnChampion(
                world, entityMap, eChampion::JAX,
                static_cast<u8_t>(eTeam::Red), 6u);
            world.GetComponent<TransformComponent>(lee).SetPosition(Vec3{});
            world.GetComponent<TransformComponent>(survivor).SetPosition(
                Vec3{ 1.f, 0.f, 0.f });
            world.GetComponent<TransformComponent>(deadBeforeE2).SetPosition(
                Vec3{ 2.f, 0.f, 0.f });
            world.GetComponent<SkillRankComponent>(lee).ranks[kE] = 1u;

            GameCommand e1{};
            e1.kind = eCommandKind::CastSkill;
            e1.issuerEntity = lee;
            e1.sequenceNum = 1u;
            e1.slot = kE;
            e1.itemId = 1u;
            TickContext e1Tick = MakeProbeTickContext(
                1ull, rng, entityMap, walkable);
            if (executor->ExecuteCommand(world, e1Tick, e1).state !=
                    eCommandExecutionState::Accepted ||
                !world.HasComponent<LeeSinTempestMarkComponent>(survivor) ||
                !world.HasComponent<LeeSinTempestMarkComponent>(deadBeforeE2) ||
                DeterministicEntityIterator<DamageRequestComponent>::CollectSorted(
                    world).size() != 2u)
            {
                std::printf("[SimLab][LeeClosure] FAIL: E1 queue/mark\n");
                return false;
            }
            const f32_t survivorHpBefore =
                world.GetComponent<HealthComponent>(survivor).fCurrent;
            CDamageQueueSystem::Execute(world, e1Tick);
            if (world.GetComponent<HealthComponent>(survivor).fCurrent >=
                survivorHpBefore)
            {
                return false;
            }
            auto& deadHealth = world.GetComponent<HealthComponent>(deadBeforeE2);
            deadHealth.fCurrent = 0.f;
            deadHealth.bIsDead = true;

            GameCommand e2 = e1;
            e2.sequenceNum = 2u;
            e2.itemId = 2u;
            TickContext e2Tick = MakeProbeTickContext(
                40ull, rng, entityMap, walkable);
            if (executor->ExecuteCommand(world, e2Tick, e2).state !=
                    eCommandExecutionState::Accepted ||
                CountStatusEffects(
                    world, survivor, eStatusEffectId::GenericSlow, lee) != 1u ||
                CountStatusEffects(
                    world, deadBeforeE2, eStatusEffectId::GenericSlow, lee) != 0u ||
                world.HasComponent<LeeSinTempestMarkComponent>(survivor))
            {
                std::printf("[SimLab][LeeClosure] FAIL: E2 surviving-mark slow\n");
                return false;
            }
        }

        struct LandingWalkable final : IWalkableQuery
        {
            bool_t bClamp = false;
            Vec3 vClamped{};
            bool_t IsWalkableXZ(const Vec3&) const override { return true; }
            bool_t SegmentWalkableXZ(const Vec3&, const Vec3&, f32_t) const override
            {
                return true;
            }
            bool_t TryClampMoveSegmentXZ(
                const Vec3&, const Vec3& desired, f32_t, Vec3& out) const override
            {
                out = bClamp ? vClamped : desired;
                return true;
            }
            bool_t TryResolveMoveTarget(
                const Vec3&, const Vec3& raw, Vec3& out) const override
            {
                out = raw;
                return true;
            }
            bool_t TryBuildMovePath(
                const Vec3&, const Vec3& raw, Vec3*, u16_t, u16_t& count,
                Vec3& out) const override
            {
                count = 0u;
                out = raw;
                return true;
            }
            bool_t TrySampleHeight(f32_t, f32_t, f32_t& y) const override
            {
                y = 0.f;
                return true;
            }
        };

        const auto CheckRLanding = [&](LandingWalkable& walkable, f32_t expectedX)
        {
            CWorld world;
            DeterministicRng rng(2026072020ull);
            EntityIdMap entityMap;
            auto executor = CDefaultCommandExecutor::Create();
            const EntityID lee = SpawnChampion(
                world, entityMap, eChampion::LEESIN,
                static_cast<u8_t>(eTeam::Blue), 0u);
            const EntityID target = SpawnChampion(
                world, entityMap, eChampion::EZREAL,
                static_cast<u8_t>(eTeam::Red), 5u);
            world.GetComponent<TransformComponent>(lee).SetPosition(Vec3{});
            world.GetComponent<TransformComponent>(target).SetPosition(
                Vec3{ 1.f, 0.f, 0.f });
            world.GetComponent<SkillRankComponent>(lee).ranks[kR] = 1u;
            GameCommand r{};
            r.kind = eCommandKind::CastSkill;
            r.issuerEntity = lee;
            r.sequenceNum = 1u;
            r.slot = kR;
            r.targetEntity = target;
            TickContext tc = MakeProbeTickContext(
                1ull, rng, entityMap, walkable);
            if (executor->ExecuteCommand(world, tc, r).state !=
                    eCommandExecutionState::Accepted ||
                !world.HasComponent<ForcedMotionComponent>(target))
            {
                return false;
            }
            const Vec3 landing =
                world.GetComponent<ForcedMotionComponent>(target).end;
            return std::fabs(landing.x - expectedX) <= 0.0001f &&
                std::fabs(landing.z) <= 0.0001f;
        };

        LandingWalkable openLanding{};
        LandingWalkable clampedLanding{};
        clampedLanding.bClamp = true;
        clampedLanding.vClamped = Vec3{ 3.f, 0.f, 0.f };
        if (!CheckRLanding(openLanding, 11.f) ||
            !CheckRLanding(clampedLanding, 3.f))
        {
            std::printf("[SimLab][LeeClosure] FAIL: R landing clamp\n");
            return false;
        }

        struct TransitBlockingWalkable final : IWalkableQuery
        {
            bool_t IsWalkableXZ(const Vec3&) const override { return true; }
            bool_t SegmentWalkableXZ(const Vec3&, const Vec3&, f32_t) const override
            {
                return false;
            }
            bool_t TryClampMoveSegmentXZ(
                const Vec3& from, const Vec3&, f32_t, Vec3& out) const override
            {
                out = from;
                return false;
            }
            bool_t TryResolveMoveTarget(
                const Vec3&, const Vec3& raw, Vec3& out) const override
            {
                out = raw;
                return true;
            }
            bool_t TryBuildMovePath(
                const Vec3&, const Vec3&, Vec3*, u16_t, u16_t& count,
                Vec3&) const override
            {
                count = 0u;
                return false;
            }
            bool_t TrySampleHeight(f32_t, f32_t, f32_t& y) const override
            {
                y = 0.f;
                return true;
            }
        } transitWalkable;

        CWorld dashWorld;
        const EntityID wDash = dashWorld.CreateEntity();
        const EntityID q2Dash = dashWorld.CreateEntity();
        dashWorld.AddComponent<TransformComponent>(wDash, TransformComponent{});
        dashWorld.AddComponent<TransformComponent>(q2Dash, TransformComponent{});
        LeeSinDashComponent wState{};
        wState.vStart = Vec3{};
        wState.vEnd = Vec3{ 4.f, 0.f, 0.f };
        wState.fDurationSec = 0.18f;
        wState.bIgnoreTerrainDuringTransit = false;
        LeeSinDashComponent q2State = wState;
        q2State.bIgnoreTerrainDuringTransit = true;
        dashWorld.AddComponent<LeeSinDashComponent>(wDash, wState);
        dashWorld.AddComponent<LeeSinDashComponent>(q2Dash, q2State);
        TickContext dashTick{};
        dashTick.fDt = 0.09f;
        dashTick.pWalkable = &transitWalkable;
        LeeSinGameSim::Tick(dashWorld, dashTick);
        const Vec3 wAfter =
            dashWorld.GetComponent<TransformComponent>(wDash).GetPosition();
        const Vec3 qAfter =
            dashWorld.GetComponent<TransformComponent>(q2Dash).GetPosition();
        if (WintersMath::DistanceSqXZ(Vec3{}, wAfter) > 0.0001f ||
            dashWorld.HasComponent<LeeSinDashComponent>(wDash) ||
            WintersMath::DistanceSqXZ(Vec3{}, qAfter) <= 0.0001f ||
            !dashWorld.HasComponent<LeeSinDashComponent>(q2Dash))
        {
            std::printf("[SimLab][LeeClosure] FAIL: W/Q2 first transit tick\n");
            return false;
        }
        LeeSinGameSim::Tick(dashWorld, dashTick);
        const Vec3 qArrival =
            dashWorld.GetComponent<TransformComponent>(q2Dash).GetPosition();
        if (dashWorld.HasComponent<LeeSinDashComponent>(q2Dash) ||
            std::fabs(qArrival.x - 4.f) > 0.0001f)
        {
            std::printf("[SimLab][LeeClosure] FAIL: Q2 arrival snap\n");
            return false;
        }

        std::printf(
            "[SimLab][LeeClosure] PASS: AI producer Q1/Q2/BA, E1/E2, R, W/Q2 terrain\n");
        return true;
    }
```

`main`의 option branches에 아래를 추가한다.

```cpp
    if (argc > 1 && std::strcmp(argv[1], "--leesin-closure-only") == 0)
    {
        RegisterAllChampionHooks();
        const bool_t bPass = RunLeeSinGameplayClosureProbe();
        std::printf("[SimLab] %s\n", bPass ? "PASS" : "FAIL");
        return bPass ? 0 : 1;
    }
```

전체-run local 변수에 `const bool_t bLeeSinGameplayClosureProbePass = RunLeeSinGameplayClosureProbe();`를 추가하고 최종 `bPass` conjunction에서 `bMinionSoftSeparationPolicyProbePass` 바로 다음에 `bLeeSinGameplayClosureProbePass &&`를 추가한다.

### 2-10. `Client/Private/Network/Client/SnapshotApplier.cpp`

marker constants 아래에 Q mark ground texture를 추가한다.

```cpp
    constexpr const wchar_t* kLeeSinWardMarkerTexture =
        L"Texture/Character/LeeSin/particles/base_leesin_q_mark_floor_01.png";
```

`EnsureSnapshotWardRuntimeTags`의 FX 작성부를 아래 값으로 교체한다.

```cpp
            fx.attachTo = entity;
            fx.vAttachOffset = { 0.f, 0.055f, 0.f };
            fx.fLifetime = 3600.f;
            fx.fFadeOut = 0.f;
            fx.bBillboard = false;
            fx.renderType = eFxRenderType::GroundDecal;
            fx.depthMode = eFxDepthMode::DepthTestWriteOff;
            fx.blendMode = eBlendPreset::Additive;
            fx.texturePath = kLeeSinWardMarkerTexture;
            fx.fWidth = 0.8f;
            fx.fHeight = 0.8f;
            fx.vColor = (team == static_cast<u8_t>(eTeam::Red))
                ? Vec4{ 1.f, 0.30f, 0.22f, 0.58f }
                : Vec4{ 0.34f, 0.96f, 0.92f, 0.58f };
```

### 2-11. `Client/Private/GameObject/Champion/Kalista/KalistaFxPresets.cpp`

`kPathWSentinelAvatarTex` 상수를 삭제한다. `PreloadWSentinelTextures` 전체를 아래로 교체한다.

```cpp
bool_t KalistaFx::PreloadWSentinelTextures(CFxSystem& fxSystem)
{
    return fxSystem.PreloadTextureResource(kPathWSentinelViewConeTex);
}
```

`SpawnWSentinelIdle`에서 `const f32_t sightCenterOffset...` 바로 아래의 `FxBillboardComponent avatar{}`부터 `*pOutAvatarFx = avatarFx;`까지를 아래 두 줄로 교체한다.

```cpp
    if (pOutAvatarFx)
        *pOutAvatarFx = NULL_ENTITY;
```

그 아래의 기존 `FxBillboardComponent cone{}`부터 함수 끝까지는 그대로 둔다. cone 작성·visibility team mask·snapshot follow update는 유지한다.

### 2-12. objective FX 기존 계약

`EventApplier::ReconcileObjectiveSnapshot`과 `SnapshotApplier`의 objective flags 전달은 이미 champion attach + full-snapshot prune을 구현했다. 새 gameplay/visual owner를 추가하지 않는다. 아래 F5 절차를 이 slice의 필수 수동 검증으로 고정한다.

1. Debug Server와 Debug Client를 순차 기동하고 연습 명령으로 같은 local champion에 Baron flag를 부여한다. 위치 A에서 champion 발밑 decal과 월드 원점 잔상 부재를 캡처한다.
2. champion을 5m 이상 이동해 위치 B에서 같은 decal이 champion 발밑을 따라왔고 위치 A에 잔상이 없음을 캡처한다.
3. champion death/full snapshot 이후 첫 reconcile frame에 flag가 사라지고 runtime key가 prune되어 decal이 없음을 캡처한다.
4. Elder flag에도 1~3을 반복한다.
5. 캡처와 debugger trace를 `.md/build/2026-07-20_gameplay-closure/objective-fx/`에 `baron_attach.png`, `baron_follow.png`, `baron_death_prune.png`, `elder_attach.png`, `elder_follow.png`, `elder_death_prune.png`로 남긴다.

이 6개 증거 중 하나라도 없거나 attach/follow/death-prune 기준을 만족하지 못하면 RESULT의 objective visual 항목은 `FAIL` 또는 `미검증`이다. Client build만으로 완료 처리하지 않는다.

### 2-13. `Client/Private/Data/LoLVisualDefinitionPack.h`

`MinionVisualDefinition`의 shader 아래에 추가한다.

```cpp
        f32_t visualScaleMultiplier = 1.f;
```

### 2-14. `Data/LoL/ClientPublic/Visual/ObjectVisualDefs.json`

`minion.blue.siege`, `minion.red.siege`에만 아래를 추가한다.

```json
      "visualScaleMultiplier": 1.5,
```

### 2-15. `Tools/LoLData/Build-LoLDefinitionPack.py`

`minions.append(...)` 블록을 아래로 교체한다. 입력에 없는 항목은 normalized JSON에 쓰지 않고 C++ 기본값 1.0을 사용한다.

```python
        visual_scale_multiplier = legacy.as_float(
            item.get("visualScaleMultiplier", 1.0),
            f"minions[{index}].visualScaleMultiplier")
        if visual_scale_multiplier <= 0.0:
            fail(f"minions[{index}].visualScaleMultiplier must be > 0")
        record = {
            "key": key,
            "type": minion_type,
            "team": team,
            "mesh": mesh.replace("\\", "/"),
            "shader": shader.replace("\\", "/"),
            "textureAllMeshes": texture_all.replace("\\", "/"),
        }
        if visual_scale_multiplier != 1.0:
            record["visualScaleMultiplier"] = visual_scale_multiplier
        minions.append(record)
```

minion C++ emitter의 `def.shader = ...` 아래에 추가한다.

```python
        if record.get("visualScaleMultiplier", 1.0) != 1.0:
            lines.append(
                f"        def.visualScaleMultiplier = {cpp_float(record['visualScaleMultiplier'])};"
            )
```

### 2-16. `Client/Private/Data/Generated/LoLVisualDefinitions.generated.cpp`

generator 산출물로만 갱신한다. blue/red siege factory만 `def.visualScaleMultiplier = 1.5f;`를 갖고 다른 minion은 기본 1.0을 유지해야 한다.

### 2-17. `Client/Private/Manager/Minion_Manager.cpp`

anonymous namespace에 visual base scale helper를 추가한다.

```cpp
    f32_t ResolveMinionVisualScale(
        f32_t baseScale,
        eMinionType type,
        eMinionTeam team)
    {
        if (type == eMinionType::Tibbers)
            return kTibbersVisualScale;
        const ClientData::MinionVisualDefinition* visual =
            ClientData::FindMinionVisualDefinition(
                static_cast<u32_t>(type),
                static_cast<u32_t>(team));
        return baseScale * (visual ? visual->visualScaleMultiplier : 1.f);
    }
```

enum은 `Client/Public/Manager/Minion_Manager.h`의 global `eMinionType`, `eMinionTeam`을 그대로 사용한다. network/local spawn의 실제 변수명 `eType`, `eTeamParam`을 사용해 기존 `fVisualScale` 선언을 각각 아래 한 줄로 교체한다.

```cpp
    const f32_t fVisualScale =
        ResolveMinionVisualScale(m_fVisualScale, eType, eTeamParam);
```

`OnImGui_Tuner`의 `ImGui::Begin` 성공 직후에 아래 lambda를 추가한다.

```cpp
    const auto ApplyCurrentVisualScale = [&]()
    {
        if (!m_pWorld)
            return;
        for (EntityID entity : m_vecEntities)
        {
            if (!m_pWorld->IsAlive(entity) ||
                !m_pWorld->HasComponent<TransformComponent>(entity) ||
                !m_pWorld->HasComponent<MinionStateComponent>(entity))
            {
                continue;
            }
            const auto& state = m_pWorld->GetComponent<MinionStateComponent>(entity);
            const eMinionType eType = static_cast<eMinionType>(state.type);
            const eMinionTeam eTeamParam = state.team == eTeam::Red
                ? eMinionTeam::Red
                : eMinionTeam::Blue;
            m_pWorld->GetComponent<TransformComponent>(entity).SetScale(
                ResolveMinionVisualScale(m_fVisualScale, eType, eTeamParam));
        }
    };
```

`ImGui::DragFloat("Visual Scale"...)`의 현재 `if (m_pWorld) { for ... }` 전체를 `ApplyCurrentVisualScale();` 한 줄로 교체한다. `Reset Visual Scale`의 `m_fVisualScale = kDefaultMinionScale;`은 유지하고 그 아래 현재 guarded loop 전체를 `ApplyCurrentVisualScale();`로 교체한다. lambda 내부 null return이 기존 안전 경계를 보존한다.

Tibbers는 helper의 첫 branch로 정확히 0.01을 반환한다. 일반 melee/ranged/super는 base×1.0, blue/red siege만 base×1.5다. `ResolveMinionRenderTransform`의 Baron multiplier는 이 world transform scale 위에 기존처럼 추가 곱하며 삭제·이동하지 않는다.

### 2-18. `Shared/GameSim/Systems/Move/MinionSoftSeparationPolicy.h`

기존 `ResolveForwardSafeDirection` 아래에 deterministic static tangent helper를 추가한다.

```cpp
    inline Vec3 ResolveStaticTangentDirection(
        const Vec3& staticPush,
        const Vec3& preferredForward,
        std::uint32_t entityTieBreaker)
    {
        const Vec3 forward = WintersMath::NormalizeXZ(
            preferredForward, Vec3{ 0.f, 0.f, 1.f });
        const Vec3 normal = WintersMath::NormalizeXZ(staticPush, forward);
        Vec3 tangent{ -normal.z, 0.f, normal.x };
        const float oppositeDot =
            (-tangent.x) * forward.x + (-tangent.z) * forward.z;
        const float tangentDot = tangent.x * forward.x + tangent.z * forward.z;
        if (oppositeDot > tangentDot + 0.0001f ||
            (std::fabs(oppositeDot - tangentDot) <= 0.0001f &&
                (entityTieBreaker & 1u) != 0u))
        {
            tangent.x = -tangent.x;
            tangent.z = -tangent.z;
        }
        return WintersMath::NormalizeXZ(
            Vec3{
                tangent.x + normal.x * 0.25f,
                0.f,
                tangent.z + normal.z * 0.25f },
            tangent);
    }
```

`<cmath>`를 include한다. 동일 header에 아래 composite helper를 추가하고 GameRoom과 probe가 함께 사용한다. 두 soft-minion push가 raw 합으로 후진을 만들더라도 총 magnitude는 forward-safe 방향에 투영되고 static/other push와 결정적으로 결합된다.

```cpp
    inline Vec3 ResolveCompositeDepenetrationDirection(
        const Vec3& softMinionPush,
        const Vec3& otherPush,
        const Vec3& preferredForward,
        std::uint32_t entityTieBreaker)
    {
        Vec3 resolved = otherPush;
        const float softLength = std::sqrt(
            softMinionPush.x * softMinionPush.x +
            softMinionPush.z * softMinionPush.z);
        if (softLength > 0.0001f)
        {
            const Vec3 safe = ResolveForwardSafeDirection(
                softMinionPush, preferredForward, entityTieBreaker);
            resolved.x += safe.x * softLength;
            resolved.z += safe.z * softLength;
        }
        return WintersMath::NormalizeXZ(resolved, preferredForward);
    }
```

같은 header에 production과 SimLab이 함께 실행할 complete primary→tangent selection helper를 추가한다.

```cpp
    struct DepenetrationCandidateSelection
    {
        Vec3 vPosition{};
        bool_t bUsedStaticTangent = false;
    };

    template <typename TTryClamp>
    inline bool_t TrySelectDepenetrationCandidate(
        const Vec3& start,
        f32_t clearanceRadius,
        f32_t step,
        const Vec3& primaryDirection,
        const Vec3& staticPush,
        const Vec3& preferredForward,
        std::uint32_t entityTieBreaker,
        bool_t bHasStaticBlocker,
        const TTryClamp& tryClamp,
        DepenetrationCandidateSelection& outSelection)
    {
        const auto TryCandidate = [&](const Vec3& direction, Vec3& out)
        {
            const Vec3 desired{
                start.x + direction.x * step,
                start.y,
                start.z + direction.z * step };
            if (!tryClamp(start, desired, clearanceRadius, out))
                return false;
            return WintersMath::DistanceSqXZ(start, out) > 0.0001f;
        };

        outSelection = {};
        if (TryCandidate(primaryDirection, outSelection.vPosition))
            return true;
        if (!bHasStaticBlocker)
            return false;

        const Vec3 tangentDirection = ResolveStaticTangentDirection(
            staticPush, preferredForward, entityTieBreaker);
        if (!TryCandidate(tangentDirection, outSelection.vPosition))
            return false;
        outSelection.bUsedStaticTangent = true;
        return true;
    }
```

### 2-19. `Server/Private/Game/GameRoomUnitAI.cpp`

`TryResolveMinionDepenetrationStep`에서 `vSoftMinionPush`, `vOtherPush`, `vStaticPush`를 별도로 누적한다. soft count가 있으면 magnitude를 보존한 forward-safe 방향으로 매번 정규화한 뒤 other push와 합친다. 1차 `TryClampMoveSegmentXZ`가 false이거나 이동량 0이면 static count가 있을 때 `ResolveStaticTangentDirection` 후보를 동일 step으로 한 번 clamp한다. 두 후보 모두 실패할 때만 false다. trace에는 `candidate=primary|static-tangent`를 추가한다.

`TryResolveMinionDepenetrationStep`의 기존 `Vec3 vPush{}`를 `vSoftMinionPush`, `vOtherPush`, `vStaticPush`로 교체한다. 실제 코드의 두 `vPush +=` 줄을 아래 전체 블록으로 교체한다.

```cpp
        const Vec3 vDelta{
            (vAway.x / dist) * penetration * weight,
            0.f,
            (vAway.z / dist) * penetration * weight };
        if (bSoftMinion)
        {
            vSoftMinionPush.x += vDelta.x;
            vSoftMinionPush.z += vDelta.z;
        }
        else
        {
            vOtherPush.x += vDelta.x;
            vOtherPush.z += vDelta.z;
            if (bStatic)
            {
                vStaticPush.x += vDelta.x;
                vStaticPush.z += vDelta.z;
            }
        }
```

기존 `Vec3 vResolvedPush = vPush;`부터 `TryClampMoveSegmentXZ` 1회 반환까지를 아래로 교체한다. shared selection helper가 이동량 0도 실패로 보므로 turret collider에 막힌 1차 후보가 성공으로 오판되지 않는다.

```cpp
    if (blockerCount == 0u)
        return false;
    const Vec3 vRawPush{
        vSoftMinionPush.x + vOtherPush.x,
        0.f,
        vSoftMinionPush.z + vOtherPush.z };
    const Vec3 primaryDirection =
        MinionSoftSeparationPolicy::ResolveCompositeDepenetrationDirection(
            vSoftMinionPush, vOtherPush, vPreferredForward,
            static_cast<u32_t>(entity));
    const bool_t bSoftMinionOnly =
        softMinionCount > 0u && staticCount == 0u && dynamicCount == 0u;
    const f32_t maxPushStep = bSoftMinionOnly
        ? ServerData::GetActiveLoLSpawnObjectDefinitionPack()
            .minionBehavior.softSeparationMaxStep
        : 0.35f;
    const f32_t pushStep =
        (std::min)((std::max)(0.08f, fStep), maxPushStep);
    const f32_t clearanceRadius =
        ServerData::GetActiveLoLSpawnObjectDefinitionPack()
            .minionBehavior.laneClearanceRadius;
    MinionSoftSeparationPolicy::DepenetrationCandidateSelection selection{};
    const auto clamp = [&](const Vec3& from, const Vec3& desired,
                           f32_t radius, Vec3& out)
    {
        return TryClampMoveSegmentXZ(from, desired, radius, out);
    };
    if (!MinionSoftSeparationPolicy::TrySelectDepenetrationCandidate(
            vPos,
            clearanceRadius,
            pushStep,
            primaryDirection,
            vStaticPush,
            vPreferredForward,
            static_cast<u32_t>(entity),
            staticCount > 0u,
            clamp,
            selection))
    {
        return false;
    }
    Vec3 vGuarded = selection.vPosition;
    const char* pCandidateKind =
        selection.bUsedStaticTangent ? "static-tangent" : "primary";
```

이후 기존 height sample/`vOutNext` 대입을 유지한다. bounded trace의 raw `vPush` 두 인자는 `vRawPush`, resolved 두 인자는 `primaryDirection`으로 교체하고 format/argument에 `pCandidateKind`를 넣는다.

### 2-20. `Tools/LoLData/Test-F4BalanceContracts.py`

현재 authoring/generated parity에 더해 Yasuo E cooldown 전 rank 0.1, Yasuo Q runtime labels 3종, Lee Q canonical flat row enabled + Q2 runtime label을 정적 계약으로 추가한다. 숫자 자체를 영구 고정하려는 테스트가 아니라 사용자가 확정한 이번 closure 값과 편집 surface가 생성 pack까지 연결되는지 확인한다.

`main()`의 기존 `champions` load 및 `skills_surface` read 뒤에 아래 assertion을 추가한다.

```python
    yasuo = champion_by_name(champions, "YASUO")
    yasuo_e = skill_by_slot(yasuo, 3)
    require_list(
        yasuo_e["cooldownSecByRank"],
        [0.1] * 5,
        "Yasuo E cooldown is 0.1 sec at every rank")

    runtime_only_block = skills_surface.split(
        "kRuntimeFlatOnlySkills", 1)[1].split("};", 1)[0]
    require('"skill.yasuo.q"' in runtime_only_block,
            "Yasuo Q flat damage is editable through runtime variant rows")
    require('"skill.leesin.q"' not in runtime_only_block,
            "Lee Sin Q canonical flat damage row remains enabled")
    for label in ("Q1/Q2 Base Damage", "Q3 Tornado Base Damage", "EQ Base Damage"):
        require(label in skills_surface, f"Yasuo Q F4 label {label}")
    require('pLabel = "Q2 Recast Base Damage"' in skills_surface,
            "Lee Sin Q2 runtime base damage row")

    scene_network = (root / "Client/Private/Scene/Scene_InGameNetwork.cpp").read_text(
        encoding="utf-8")
    command_result = scene_network.split(
        "void CScene_InGame::OnAuthoritativeCommandResult(", 1)[1].split(
        "void CScene_InGame::RebaseNetworkTimeline(", 1)[0]
    require("slot.currentStage = authoritativeSkillStage == 1u ? 1u : 0u;" in
            command_result and
            "stageWindowEndTick > serverTick" in command_result,
            "client command result restores authoritative skill stage/window")
    rebase = scene_network.split(
        "void CScene_InGame::RebaseNetworkTimeline(", 1)[1]
    require("std::fill_n(m_uLastSkillCommandResultSeq, 5u, 0u);" in rebase,
            "replay rebase clears client command result sequence history")
```

### 2-21. 결정적 mixed blocker 회귀 픽스처

`Tools/SimLab/main.cpp`의 기존 soft-separation probe에 아래 exact fixture를 추가한다. 이는 터렛 static blocker 1개와 같은 전방 셀의 melee soft-minion 2개를 수치화한다. synthetic clamp callback은 첫 호출(primary)을 static collision로 0 이동 처리하고 둘째 호출(tangent)을 walkable corridor로 허용한다. 중요한 gate는 callback을 따로 호출하는 것이 아니라 production이 쓰는 `TrySelectDepenetrationCandidate` 전체를 실행한다는 점이다.

```cpp
    const Vec3 preferredForward{ 1.f, 0.f, 0.f };
    const Vec3 softPushA{ -0.8f, 0.f, 0.2f };
    const Vec3 softPushB{ -0.8f, 0.f, -0.2f };
    const Vec3 softPush{
        softPushA.x + softPushB.x, 0.f,
        softPushA.z + softPushB.z };
    const Vec3 staticPush{ 0.f, 0.f, 1.f };
    const Vec3 primary =
        MinionSoftSeparationPolicy::ResolveCompositeDepenetrationDirection(
            softPush, staticPush, preferredForward, 42u);
    u32_t clampCallCount = 0u;
    const auto SyntheticClamp = [&clampCallCount](
        const Vec3& from, const Vec3& candidate, f32_t, Vec3& out)
    {
        ++clampCallCount;
        if (clampCallCount == 1u)
        {
            out = from;
            return false;
        }
        out = candidate;
        return candidate.x > 0.f;
    };

    MinionSoftSeparationPolicy::DepenetrationCandidateSelection selection{};
    const bool_t bMixedStaticTwoSoftRecovered =
        MinionSoftSeparationPolicy::TrySelectDepenetrationCandidate(
            Vec3{}, 0.5f, 0.35f, primary, staticPush,
            preferredForward, 42u, true, SyntheticClamp, selection) &&
        clampCallCount == 2u &&
        selection.bUsedStaticTangent &&
        selection.vPosition.x > 0.f &&
        WintersMath::DistanceSqXZ(Vec3{}, selection.vPosition) > 0.0001f;
```

같은 probe에 soft-only regression을 유지한다: `otherPush={0,0,0}`인 두 soft push fixture에서 composite 결과의 preferred-forward dot가 0 이상이고 두 번 호출 결과가 bit-identical이어야 한다. `bMixedStaticTwoSoftRecovered`와 soft-only 결과 모두 SimLab PASS 집계에 포함한다. production `GameRoomUnitAI.cpp`도 같은 complete selection helper를 호출하므로 branch wiring은 같은 코드로 검증된다.

## 3. 검증 — 예측을 먼저 쓴다

예측:

- generator `--check`는 blue/red siege 1.5를 포함한 새 visual pack hash로 PASS하며 나머지 minion은 1.0이다.
- F4 contract는 Yasuo E 0.1, Yasuo Q variant params, Lee Q flat/Q2 param 편집 경로를 PASS한다.
- SimLab Lee probe는 invalid Q2 stage 보존, bot Q1→Q2→BA command 생산, E1 damage/E2 hit-only slow, R 10m clamp, Q2 transit/arrival 정책을 PASS한다.
- GameRoom projectile probe는 Q1 19.2/terrain false/barrier true와 real projectile contact→mark→Q2를 PASS한다.
- existing SimLab 전체와 GameRoom probe 기존 turret/projectile cases는 회귀 없이 PASS한다.
- Server/Client Debug `/m:1` 순차 build가 PASS한다. objective/ward/sentinel/siege 및 exact red-mid-outer 체감은 F5 캡처 없이는 미검증으로 남긴다.

검증 명령:

```powershell
python Tools/LoLData/Build-LoLDefinitionPack.py
python Tools/LoLData/Build-LoLDefinitionPack.py --check
python Tools/ChampionData/build_champion_game_data.py --check
python Tools/LoLData/Test-F4BalanceContracts.py --root .
msbuild Tools/SimLab/SimLab.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
Tools/Bin/Debug/SimLab.exe --leesin-closure-only
Tools/Bin/Debug/SimLab.exe
powershell -ExecutionPolicy Bypass -File Tools/Harness/RunGameRoomProjectileIntegrationProbe.ps1
msbuild Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
msbuild Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
git diff --check
```

추가 정적 확인:

```powershell
rg -n 'visualScaleMultiplier = 1.5f|projectile.speed = 19.2f|bCollidesWithTerrain = false|LeeSinTempestMarkComponent|ResolveStaticTangentDirection' Client Shared Server Tools
Get-Process MSBuild,cl,link,Server,Client -ErrorAction SilentlyContinue
```

미검증/판정 경계:

- 실제 F5 visual capture가 없으면 Yasuo Q3 sink/E target-death, ward/Q-mark 표현, Kalista cone, objective follow/death, siege 체감 크기를 완료로 과장하지 않는다.
- exact red-mid-outer 좌표 trace가 없으면 resolver의 deterministic synthetic/probe PASS만 기록하고 live closure는 미검증으로 남긴다.
- replay 세션이 target 파일을 동시에 바꾸면 최신 anchor를 재검토해 자동 overwrite하지 않는다.

## 서브 에이전트 비평

- 1차 독립 read-only 비평: `FAIL`, 잔존 P0 0 / P1 5.
- P1-1 exact edit contract 부족: **수용**. Lee E/Tick/R, generator, minion manager, depenetration, F4에 exact anchor/교체 블록을 추가했다. probe는 assertion 순서와 fixture contract를 구체화했다.
- P1-2 존재하지 않는 project 경로: **수용**. GameRoom은 `RunGameRoomProjectileIntegrationProbe.ps1`, Server/Client는 실제 `*/Include/*.vcxproj`, SimLab은 실제 `Tools/SimLab/SimLab.vcxproj`로 교정했다.
- P1-3 mixed static+two-soft exact fixture 부재: **수용**. `ResolveCompositeDepenetrationDirection`을 production/probe 공용 policy로 두고 primary clamp stall→static tangent nonzero forward-safe progress, soft-only 결정성 회귀를 mandatory SimLab gate로 추가했다.
- P1-4 siege enum/배율 계약 오류: **수용**. global enum으로 교정하고 Tibbers 0.01, only siege 1.5, others 1.0, Baron 추가 곱을 exact contract로 고정했다.
- P1-5 objective follow/death가 선택 검증: **수용**. 6개 mandatory F5 capture 경로와 attach/follow/death-prune 판정 기준을 고정했다.
- P2 client mirror ordering/W regression: **수용**. unique-key 이후 mirror, command-result/snapshot newer truth, rejected Q2 restore, W clamp 보존 assertion을 추가했다.
- 2차 독립 delta 비평: `FAIL`, 잔존 P0 0 / P1 5.
- 2차 P1-1 exact contract/undefined `vDelta`: **수용**. generic target check 전체 교체, Tempest expiry exact block, Kalista exact deletion anchors, GameRoom Q lifecycle full fixture, 실제 weighted `vDelta` 선언과 production block을 추가했다.
- 2차 P1-2 F4 fixture factual mismatch: **수용**. live helper/변수/schema에 맞춰 `champion_by_name(champions, "YASUO")`, `skill_by_slot`, `require_list`로 교정했다.
- 2차 P1-3 mixed fixture가 resolver branch 미실행: **수용**. complete primary→tangent candidate selection을 shared templated helper로 추출하고 production과 SimLab이 동일 helper를 실행하게 했다.
- 2차 P1-4 Client method를 SimLab에서 호출 불가: **수용**. 직접 호출 주장을 삭제하고 GameRoom server feedback 실행 검증 + Client source 정적 계약 + Client build 경계로 교정했다. W/Q2는 Shared-only direct Tick fixture로 고정했다.
- 2차 P1-5 tuner null safety/실제 변수명: **수용**. lambda에 early null return을 추가하고 `eType`/`eTeamParam` exact spawn 교체 및 Drag/Reset exact call site를 기록했다. stale conditional 문단을 삭제했다.
- 3차 독립 delta 비평: `FAIL`, 잔존 P0 0 / P1 2.
- 3차 P1-1 invalid Q2가 action lock에 선차단: **수용**. Q2 0.6초 lock 종료 뒤인 tick 146으로 fixture를 이동해 mark resolver reject reason과 stage/window 보존을 검사한다.
- 3차 P1-2 SimLab Lee probe가 산문: **수용**. AI Q1/Q2/BA production-only fixture, E1 queue/damage+mark/E2 surviving-only slow, R 10m+clamp, W clamp/Q2 transit+arrival의 entity setup·실행·assertion·return을 포함한 완전한 function body와 option/full main 집계를 추가했다.
- 4차 독립 delta 비평: `PASS`, 잔존 P0 0 / P1 0. 구현 착수 gate 충족.
- 비차단 P2 1건: full-run 변수/conjunction anchor가 fenced block보다 약하나 단일 의미라 구현 시 exact anchor fresh-read로 적용한다.
- 현재 상태: 비평 gate 통과, 소스 구현 진행 가능.
