Session - S02 장기 저장 Entity 참조를 EntityHandle로 저장하고 읽는 순간 EntityID로 해석한다.

1. 반영해야 하는 코드

S02 코드 계약:
- `ForEach` row, 함수 인자, 같은 tick 안의 임시 값은 `EntityID`로 남긴다.
- component/request 안에 저장되어 tick을 넘는 entity 참조는 `EntityHandle`로 바꾼다.
- 저장 참조를 읽는 코드는 `CWorld::TryResolveEntity` 또는 `CWorld::TryGetComponent`를 통과한다.
- `GameCommandWire`의 `NetEntityId`와 `GameCommand`의 tick-local `EntityID`는 S02에서 바꾸지 않는다.
- 새 wrapper type을 만들지 않는다. 본질은 `EntityHandle` 하나다.

1-1. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Components/AttackChaseComponent.h

기존 코드:

```cpp
    EntityID target = NULL_ENTITY;
```

아래로 교체:

```cpp
    EntityHandle target{};
```

1-2. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp

`StartAttackChase` 안에서 아래 기존 코드를:

```cpp
        chase.target = cmd.targetEntity;
```

아래로 교체:

```cpp
        chase.target = world.GetEntityHandle(cmd.targetEntity);
```

1-3. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/AttackChase/AttackChaseSystem.cpp

아래 기존 코드 바로 아래에 추가:

```cpp
    bool_t IsAliveForAttackChase(CWorld& world, EntityID entity)
    {
        if (entity == NULL_ENTITY || !world.IsAlive(entity))
            return false;

        if (!world.HasComponent<HealthComponent>(entity))
            return true;

        const auto& health = world.GetComponent<HealthComponent>(entity);
        return !health.bIsDead && health.fCurrent > 0.f;
    }
```

아래에 추가:

```cpp
    bool_t TryResolveAttackChaseTarget(CWorld& world, EntityHandle targetHandle, EntityID& outTarget)
    {
        outTarget = NULL_ENTITY;
        if (!world.TryResolveEntity(targetHandle, outTarget))
            return false;

        return IsAliveForAttackChase(world, outTarget);
    }
```

기존 코드:

```cpp
    GameCommand MakeChasedCastCommand(const TickContext& tc,
        EntityID issuer, const AttackChaseComponent& chase,
        const Vec3& issuerPos, const Vec3& targetPos)
    {
        GameCommand cmd{};
        cmd.kind = eCommandKind::CastSkill;
        cmd.issuerEntity = issuer;
        cmd.issuedAtTick = tc.tickIndex;
        cmd.sequenceNum = chase.sequenceNum;
        cmd.slot = chase.slot;
        cmd.targetEntity = chase.target;
        cmd.groundPos = chase.groundPos;
        cmd.direction = chase.direction;
        cmd.itemId = chase.itemId;

        const f32_t dirLenSq =
            cmd.direction.x * cmd.direction.x +
            cmd.direction.z * cmd.direction.z;
        if (dirLenSq <= 0.0001f)
            cmd.direction = WintersMath::DirectionXZ(issuerPos, targetPos);

        return cmd;
    }
```

아래로 교체:

```cpp
    GameCommand MakeChasedCastCommand(const TickContext& tc,
        EntityID issuer, const AttackChaseComponent& chase, EntityID target,
        const Vec3& issuerPos, const Vec3& targetPos)
    {
        GameCommand cmd{};
        cmd.kind = eCommandKind::CastSkill;
        cmd.issuerEntity = issuer;
        cmd.issuedAtTick = tc.tickIndex;
        cmd.sequenceNum = chase.sequenceNum;
        cmd.slot = chase.slot;
        cmd.targetEntity = target;
        cmd.groundPos = chase.groundPos;
        cmd.direction = chase.direction;
        cmd.itemId = chase.itemId;

        const f32_t dirLenSq =
            cmd.direction.x * cmd.direction.x +
            cmd.direction.z * cmd.direction.z;
        if (dirLenSq <= 0.0001f)
            cmd.direction = WintersMath::DirectionXZ(issuerPos, targetPos);

        return cmd;
    }
```

`CAttackChaseSystem::Execute` 안에서 아래 기존 코드를:

```cpp
        const bool_t bSkillChase =
            chase.commandKind == static_cast<u8_t>(eCommandKind::CastSkill);
        const bool_t bCanExecute = bSkillChase
            ? GameplayStateQuery::CanCast(world, entity)
            : GameplayStateQuery::CanAttack(world, entity);

        if (!chase.bActive ||
            !IsAliveForAttackChase(world, entity) ||
            !IsAliveForAttackChase(world, chase.target) ||
            !bCanExecute ||
            !GameplayStateQuery::CanBeTargetedBy(world, entity, chase.target) ||
            !world.HasComponent<TransformComponent>(entity) ||
            !world.HasComponent<TransformComponent>(chase.target))
        {
            static u32_t s_chaseClearTraceCount = 0;
            if (false && s_chaseClearTraceCount < 256u)
            {
                char msg[384]{};
                sprintf_s(
                    msg,
                    "[YawTrace][AttackChaseClear] tick=%llu entity=%u target=%u seq=%u active=%u canAttack=%u canTarget=%u hasSelfTf=%u hasTargetTf=%u\n",
                    static_cast<unsigned long long>(tc.tickIndex),
                    static_cast<u32_t>(entity),
                    static_cast<u32_t>(chase.target),
                    chase.sequenceNum,
                    chase.bActive ? 1u : 0u,
                    bCanExecute ? 1u : 0u,
                    GameplayStateQuery::CanBeTargetedBy(world, entity, chase.target) ? 1u : 0u,
                    world.HasComponent<TransformComponent>(entity) ? 1u : 0u,
                    world.HasComponent<TransformComponent>(chase.target) ? 1u : 0u);
                WintersOutputAIDebugStringA(msg);
                ++s_chaseClearTraceCount;
            }
            ClearMoveTarget(world, entity);
            world.RemoveComponent<AttackChaseComponent>(entity);
            continue;
        }
```

아래로 교체:

```cpp
        const bool_t bSkillChase =
            chase.commandKind == static_cast<u8_t>(eCommandKind::CastSkill);
        const bool_t bCanExecute = bSkillChase
            ? GameplayStateQuery::CanCast(world, entity)
            : GameplayStateQuery::CanAttack(world, entity);

        EntityID target = NULL_ENTITY;
        const bool_t bHasLiveTarget =
            TryResolveAttackChaseTarget(world, chase.target, target);
        const bool_t bCanTarget = bHasLiveTarget &&
            GameplayStateQuery::CanBeTargetedBy(world, entity, target);
        const bool_t bHasSelfTransform = world.HasComponent<TransformComponent>(entity);
        const bool_t bHasTargetTransform = bHasLiveTarget &&
            world.HasComponent<TransformComponent>(target);

        if (!chase.bActive ||
            !IsAliveForAttackChase(world, entity) ||
            !bHasLiveTarget ||
            !bCanExecute ||
            !bCanTarget ||
            !bHasSelfTransform ||
            !bHasTargetTransform)
        {
            static u32_t s_chaseClearTraceCount = 0;
            if (false && s_chaseClearTraceCount < 256u)
            {
                char msg[384]{};
                sprintf_s(
                    msg,
                    "[YawTrace][AttackChaseClear] tick=%llu entity=%u target=%u seq=%u active=%u canAttack=%u canTarget=%u hasSelfTf=%u hasTargetTf=%u\n",
                    static_cast<unsigned long long>(tc.tickIndex),
                    static_cast<u32_t>(entity),
                    static_cast<u32_t>(target),
                    chase.sequenceNum,
                    chase.bActive ? 1u : 0u,
                    bCanExecute ? 1u : 0u,
                    bCanTarget ? 1u : 0u,
                    bHasSelfTransform ? 1u : 0u,
                    bHasTargetTransform ? 1u : 0u);
                WintersOutputAIDebugStringA(msg);
                ++s_chaseClearTraceCount;
            }
            ClearMoveTarget(world, entity);
            world.RemoveComponent<AttackChaseComponent>(entity);
            continue;
        }
```

같은 함수 안에서 아래 기존 코드를:

```cpp
        const Vec3 targetPos =
            world.GetComponent<TransformComponent>(chase.target).GetLocalPosition();
```

아래로 교체:

```cpp
        const Vec3 targetPos =
            world.GetComponent<TransformComponent>(target).GetLocalPosition();
```

같은 함수 안에서 아래 기존 코드를:

```cpp
                GameplayStateQuery::ResolveGameplayRadius(world, chase.target);
```

아래로 교체:

```cpp
                GameplayStateQuery::ResolveGameplayRadius(world, target);
```

같은 함수 안에서 아래 기존 코드를:

```cpp
                outCommands.push_back(MakeChasedCastCommand(
                    tc, entity, chase, selfPos, targetPos));
```

아래로 교체:

```cpp
                outCommands.push_back(MakeChasedCastCommand(
                    tc, entity, chase, target, selfPos, targetPos));
```

같은 함수 안에서 아래 기존 코드를:

```cpp
                        static_cast<u32_t>(chase.target),
```

아래로 교체:

```cpp
                        static_cast<u32_t>(target),
```

같은 함수 안에서 아래 기존 코드를:

```cpp
                outCommands.push_back(MakeBasicAttackCommand(
                    tc, entity, chase.target, chase.sequenceNum, selfPos, targetPos));
```

아래로 교체:

```cpp
                outCommands.push_back(MakeBasicAttackCommand(
                    tc, entity, target, chase.sequenceNum, selfPos, targetPos));
```

2. 검증

검증 명령:
- git diff --check -- Shared/GameSim/Components/AttackChaseComponent.h Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp Shared/GameSim/Systems/AttackChase/AttackChaseSystem.cpp
- rg -n "chase\\.target|AttackChaseComponent" Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp Shared/GameSim/Systems/AttackChase/AttackChaseSystem.cpp Shared/GameSim/Components/AttackChaseComponent.h
- powershell -ExecutionPolicy Bypass -File Tools\VerifyEntityGeneration.ps1 -SkipBuild
- powershell -ExecutionPolicy Bypass -File Tools\VerifyEntityGeneration.ps1
- & 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' Winters.sln /m /p:Configuration=Debug /p:Platform=x64

수동 확인:
- BasicAttack chase 중 target entity를 destroy한 뒤 같은 index가 재사용되어도 기존 chase가 새 entity를 공격하지 않아야 한다.
- UnitTarget skill chase 중 target이 사망하거나 제거되면 `AttackChaseComponent`가 제거되고 command가 발행되지 않아야 한다.
- 정상 target이 살아 있으면 기존 basic attack chase, cast chase, move path 재계산 동작이 유지되어야 한다.

확인 필요:
- `SkillProjectileComponent`, `DamageRequestComponent`, `StatusEffectApplyDesc`, `StatusEffectInstance`, `BuffInstance`, `AreaAuraComponent`, `PendingHitComponent`의 장기 저장 source/target/owner 필드는 S02 이후 같은 규칙으로 다음 slice에서 `EntityHandle`로 승격한다.
- `GameCommand`는 server tick 안에서 실행되는 command payload라 S02에서 유지한다. command queue가 tick을 넘어 저장되는 owner가 되면 별도 slice에서 `EntityHandle` 또는 `NetEntityId` 경계로 다시 판정한다.
- `EngineSDK/inc`는 이 계획에서 직접 수정하지 않는다.
