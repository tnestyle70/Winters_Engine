# 2026-07-19 드래곤 애니메이션·포탑 사거리/어그로 수정 계획서

```text
Session - 드래곤 날개 애니메이션의 8 Hz 계단 현상 제거 + 포탑 7.75 중심 사거리와 챔피언 피격 어그로 범위 강제
좌표: 신규 좌표 후보 · 축: C5 이산화와 오차, C7 권위와 정합성
관련: WintersFormat/04_STAGE3_WANIM_WSKEL.md, B13/03_TOWER_ATTACK_SYSTEM.md
```

운영 배분: 바닥 70%는 바이너리 무결성·GameSim 회귀 probe·Debug 빌드, 천장 30%는 실제 인게임 드래곤 날개짓과 inner/nexus turret 경계 체감 확인에 배정한다.

## 1. 결정 기록

```text
① 문제·제약: 드래곤 WMesh는 6,347정점/107본이며 날개 20본이 2,416정점(38.1%)에 정상 가중되고, flying_run WAnim은 2.300s/30 Hz/본당 70키/루프 이음새 0.0255°인데 Client가 평상시 정글 포즈를 8 Hz(125ms)만 갱신해 33.333ms 원본 키를 포즈당 약 3.75개 건너뛴다. 포탑 전역 authored attackRange=7.75를 모든 tier가 소비하지만 SpatialIndex가 챔피언 반경 0.65를 더한 8.40까지 후보를 반환한다.
② 순진한 해법의 실패: WAnim을 재쿠킹하거나 보간기를 바꾸면 해시 107/107 일치·키 누락 0·비정상 쿼터니언 0인 정상 자산/엔진 보간을 훼손한다. 포탑 수치를 7.10으로 줄이면 공간 후보 확장과 원거리 어그로 우회를 숨길 뿐 7.75 계약을 복구하지 못한다.
③ 메커니즘: 드래곤 1종만 ModelRenderer 시간 전진을 매 프레임 수행하고 다른 정글 예산은 유지한다. TurretAI는 후보 채택·어그로 부여·어그로 유지 세 지점 모두 포탑 중심↔대상 중심 XZ 거리 <= attackRange를 동일하게 강제한다.
④ 대조: 이즈리얼 R은 kProjectileTargetMobileUnits만 맞혀 구조물 피격이 아니라 넥서스 포탑 근처 챔피언 피격→TowerAggroNotify가 원거리 시전자를 잠근 경로다. 이미 발사된 포탑 투사체의 추적은 유지하되 새 발사 타깃 획득만 사거리로 제한한다.
⑤ 대가: 드래곤이 화면에 보이는 프레임마다 최대 107본 포즈를 평가해 기존 8 Hz보다 비용이 늘어난다(드래곤 1개로 한정, 컬링 시 Animator 지연 평가로 포즈 비용 0). 중심 사거리 계약은 기존의 대상 반경 포함 체감을 최대 0.65 줄이므로, 향후 edge-to-edge 사거리가 의도라면 데이터 계약을 별도 필드로 명시해야 한다.
```

### 현재 코드·데이터 증거와 소유권 경계

- `Client/Bin/Resource/Texture/Object/Jungle/Dragon/air/dragon_air_textured.wmesh`: skinned stride 76, 2 submesh, 107/107 mesh↔skeleton ordered hash 일치, invalid bone index/weight sum 오류 0.
- `.../anims/sru_dragon_flying_run.wanim`: 106 channel, 22,260 keys, 1,000 TPS, 30 Hz key spacing, 20개 날개 본 전부 P/R/S 70키, channel hash 불일치/non-finite/non-monotonic key 0. WSkel 107본 중 채널이 없는 루트 1본은 rest pose를 사용한다.
- `Client/Private/Manager/Jungle_Manager.cpp`: 평상시 `kJungleBaseAnimUpdateInterval = 1/8`, action/death도 1/20; presentation만 변경한다.
- `Data/LoL/ServerPrivate/Gameplay/SpawnObjectGameplayDefs.json`: 전역 `structure.turretAI.attackRange: 7.75`를 모든 turret tier가 소비하고 nexus는 공격력만 다르다. 다른 세션의 dirty 파일이므로 수정하지 않는다.
- `Engine/Private/ECS/SpatialIndex.cpp`: `QueryRadius`가 `radius + entry.radius`까지 반환한다. generic broad-phase 계약은 유지하고 TurretAI narrow-phase에서 authored 중심 사거리를 강제한다.
- `Shared/GameSim/Systems/Damage/DamageQueueSystem.cpp`: champion→champion 실피해에 `TowerAggroNotifyComponent`를 생성한다. 다른 세션의 dirty 파일이므로 수정하지 않는다.
- gameplay truth는 `Shared/GameSim/Systems/Turret/TurretAISystem.cpp`, visual sampling은 Client manager가 소유한다. Bot AI는 GameCommand 생산자이며 게임플레이 진실을 직접 변경하지 않는다.

## 2. 반영해야 하는 코드

### 2-1. C:/Users/user/Desktop/Winters/Client/Private/Manager/Jungle_Manager.cpp

(a) `CJungle_Manager::Update`의 profiler counter 선언, 기존 코드:

```cpp
    uint64_t animCount = 0;
    uint64_t skippedCount = 0;
    uint64_t budgetSkippedCount = 0;
```

아래로 교체:

```cpp
    uint64_t animCount = 0;
    uint64_t budgetedAnimCount = 0;
    uint64_t skippedCount = 0;
    uint64_t budgetSkippedCount = 0;
```

(b) `CJungle_Manager::Update`의 애니메이션 우선순위/누산 블록, 기존 코드:

```cpp
        auto& visual = m_mapVisualStates[it.first];
        const bool_t bHighPriorityAnim = visual.bAction || visual.bDead;
        const f32_t updateInterval = bHighPriorityAnim
            ? kJungleHighPriorityAnimUpdateInterval
            : kJungleBaseAnimUpdateInterval;

        visual.animUpdateAccumulator += dt;

        if (visual.animUpdateAccumulator < updateInterval)
        {
            ++skippedCount;
            continue;
        }

        if (!bHighPriorityAnim && animCount >= kJungleAnimUpdateBudget)
        {
            ++skippedCount;
            ++budgetSkippedCount;
            continue;
        }

        it.second->Update(visual.animUpdateAccumulator);
        visual.animUpdateAccumulator = std::fmod(visual.animUpdateAccumulator, updateInterval);
        ++animCount;
```

아래로 교체:

```cpp
        auto& visual = m_mapVisualStates[it.first];
        const bool_t bFrameRateAnim =
            m_pWorld->HasComponent<JungleComponent>(it.first) &&
            static_cast<eJungleSub>(
                m_pWorld->GetComponent<JungleComponent>(it.first).subKind) ==
                eJungleSub::Dragon;
        if (bFrameRateAnim)
        {
            it.second->Update(dt);
            visual.animUpdateAccumulator = 0.f;
            ++animCount;
            continue;
        }

        const bool_t bHighPriorityAnim = visual.bAction || visual.bDead;
        const f32_t updateInterval = bHighPriorityAnim
            ? kJungleHighPriorityAnimUpdateInterval
            : kJungleBaseAnimUpdateInterval;

        visual.animUpdateAccumulator += dt;

        if (visual.animUpdateAccumulator < updateInterval)
        {
            ++skippedCount;
            continue;
        }

        if (!bHighPriorityAnim && budgetedAnimCount >= kJungleAnimUpdateBudget)
        {
            ++skippedCount;
            ++budgetSkippedCount;
            continue;
        }

        it.second->Update(visual.animUpdateAccumulator);
        visual.animUpdateAccumulator = std::fmod(visual.animUpdateAccumulator, updateInterval);
        ++animCount;
        ++budgetedAnimCount;
```

### 2-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/Turret/TurretAISystem.cpp

(a) 익명 네임스페이스의 `IsHealthDead` 기존 코드:

```cpp
    bool_t IsHealthDead(CWorld& world, EntityID entity)
    {
        if (!world.HasComponent<HealthComponent>(entity))
            return false;

        const HealthComponent& hp = world.GetComponent<HealthComponent>(entity);
        return hp.bIsDead || hp.fCurrent <= 0.f;
    }
```

아래에 추가:

```cpp
    bool_t IsWithinAttackRange(
        CWorld& world,
        const Vec3& turretPos,
        EntityID target,
        f32_t attackRange)
    {
        if (!std::isfinite(attackRange) ||
            attackRange < 0.f ||
            !world.HasComponent<TransformComponent>(target))
        {
            return false;
        }

        const Vec3 targetPos =
            world.GetComponent<TransformComponent>(target).GetPosition();
        return WintersMath::DistanceSqXZ(turretPos, targetPos) <=
            attackRange * attackRange;
    }
```

(b-1) `TickTurrets`의 turret 순회 람다 선언, 기존 코드:

```cpp
        world.ForEach<TurretAIComponent, TurretComponent, TransformComponent>(
            std::function<void(EntityID, TurretAIComponent&, TurretComponent&, TransformComponent&)>(
                [&](EntityID id, TurretAIComponent& ai, TurretComponent& turret, TransformComponent&)
```

아래로 교체:

```cpp
        world.ForEach<TurretAIComponent, TurretComponent, TransformComponent>(
            std::function<void(EntityID, TurretAIComponent&, TurretComponent&, TransformComponent&)>(
                [&](EntityID id, TurretAIComponent& ai, TurretComponent& turret, TransformComponent& xf)
```

(b-2) 같은 람다의 어그로 유효성 블록, 기존 코드:

```cpp
                    const u8_t turretTeam = TeamOf(turret.team);
                    if (ai.aggroTargetId != NULL_ENTITY &&
                        !IsValidTarget(world, ai.aggroTargetId, turretTeam))
                    {
                        ai.aggroTargetId = NULL_ENTITY;
                        ai.aggroLockTimer = 0.f;
                    }
```

아래로 교체:

```cpp
                    const u8_t turretTeam = TeamOf(turret.team);
                    if (ai.aggroTargetId != NULL_ENTITY &&
                        (!IsValidTarget(world, ai.aggroTargetId, turretTeam) ||
                            !IsWithinAttackRange(
                                world,
                                xf.GetPosition(),
                                ai.aggroTargetId,
                                ai.attackRange)))
                    {
                        ai.aggroTargetId = NULL_ENTITY;
                        ai.aggroLockTimer = 0.f;
                    }
```

(c) `ApplyAggro`의 입력/거리 검증, 기존 코드:

```cpp
        if (!world.HasComponent<ChampionComponent>(attacker) ||
            !world.HasComponent<ChampionComponent>(victim) ||
            !world.HasComponent<TransformComponent>(victim))
        {
            return;
        }

        const u8_t attackerTeam = TeamOf(world.GetComponent<ChampionComponent>(attacker).team);
        const u8_t victimTeam = TeamOf(world.GetComponent<ChampionComponent>(victim).team);
        if (attackerTeam == victimTeam)
            return;

        const Vec3 victimPos = world.GetComponent<TransformComponent>(victim).GetPosition();

        world.ForEach<TurretAIComponent, TurretComponent, TransformComponent>(
            std::function<void(EntityID, TurretAIComponent&, TurretComponent&, TransformComponent&)>(
                [&](EntityID, TurretAIComponent& ai, TurretComponent& turret, TransformComponent& xf)
                {
                    if (!ai.bActive || TeamOf(turret.team) != victimTeam)
                        return;
                    if (WintersMath::DistanceSqXZ(xf.GetPosition(), victimPos) > ai.attackRange * ai.attackRange)
                        return;

                    ai.aggroTargetId = attacker;
                    ai.aggroLockTimer = priorityDuration;
                }));
```

아래로 교체:

```cpp
        if (!world.HasComponent<ChampionComponent>(attacker) ||
            !world.HasComponent<ChampionComponent>(victim) ||
            !world.HasComponent<TransformComponent>(attacker) ||
            !world.HasComponent<TransformComponent>(victim))
        {
            return;
        }

        const u8_t attackerTeam = TeamOf(world.GetComponent<ChampionComponent>(attacker).team);
        const u8_t victimTeam = TeamOf(world.GetComponent<ChampionComponent>(victim).team);
        if (attackerTeam == victimTeam)
            return;

        world.ForEach<TurretAIComponent, TurretComponent, TransformComponent>(
            std::function<void(EntityID, TurretAIComponent&, TurretComponent&, TransformComponent&)>(
                [&](EntityID, TurretAIComponent& ai, TurretComponent& turret, TransformComponent& xf)
                {
                    if (!ai.bActive || TeamOf(turret.team) != victimTeam)
                        return;
                    if (!IsWithinAttackRange(
                            world,
                            xf.GetPosition(),
                            victim,
                            ai.attackRange) ||
                        !IsWithinAttackRange(
                            world,
                            xf.GetPosition(),
                            attacker,
                            ai.attackRange))
                    {
                        return;
                    }

                    ai.aggroTargetId = attacker;
                    ai.aggroLockTimer = priorityDuration;
                }));
```

(d) `SelectTarget` 후보 루프의 거리 계산, 기존 코드:

```cpp
            const Vec3 targetPos = world.GetComponent<TransformComponent>(candidate).GetPosition();
            const f32_t distSq = WintersMath::DistanceSqXZ(pos, targetPos);
            if (priority < bestPriority || (priority == bestPriority && distSq < bestDistSq))
```

아래로 교체:

```cpp
            if (!IsWithinAttackRange(world, pos, candidate, ai.attackRange))
                continue;

            const Vec3 targetPos = world.GetComponent<TransformComponent>(candidate).GetPosition();
            const f32_t distSq = WintersMath::DistanceSqXZ(pos, targetPos);
            if (priority < bestPriority || (priority == bestPriority && distSq < bestDistSq))
```

### 2-3. C:/Users/user/Desktop/Winters/Tools/Harness/GameRoomProjectileIntegrationProbe.cpp

(a) 기존 include:

```cpp
#include "Shared/GameSim/Systems/Turret/TurretAISystem.h"
```

아래에 추가:

```cpp
#include "Server/Private/Data/RuntimeGameplayDefinitionOverlay.h"
```

익명 네임스페이스 시작 기존 코드:

```cpp
namespace
{
```

아래에 추가:

```cpp
    static_assert((kProjectileTargetMobileUnits &
        static_cast<u8_t>(ProjectileTarget_Structure)) == 0u);
```

(b) `SpawnTurret`의 AI 초기화, 기존 코드:

```cpp
        TurretAIComponent ai{};
        ai.attackDamage = 150.f;
        ai.projectileSpeed = 18.f;
        world.AddComponent<TurretAIComponent>(entity, ai);
```

아래로 교체:

```cpp
        const TurretAIGameDef& turretDef =
            ServerData::GetActiveLoLSpawnObjectDefinitionPack()
                .structure.turretAI;
        TurretAIComponent ai{};
        ai.attackRange = turretDef.attackRange;
        ai.attackCooldownMax = turretDef.attackCooldownMax;
        ai.attackDamage = turretDef.attackDamage;
        ai.projectileSpeed = turretDef.projectileSpeed;
        world.AddComponent<TurretAIComponent>(entity, ai);
```

(c) `SpawnTurret` 끝의 기존 코드:

```cpp
        if (BindNetworkEntity(world, entityMap, entity) == NULL_NET_ENTITY)
            return NULL_ENTITY;
        return entity;
    }
```

아래에 추가:

```cpp
    bool_t RebuildSpatialIndex(CWorld& world)
    {
        if (!world.Get_SpatialIndex())
            world.Initialize_Spatial(DefaultSpatialGridDesc());

        CSpatialIndex* pSpatial = world.Get_SpatialIndex();
        if (!pSpatial)
            return false;
        pSpatial->Rebuild(world);
        return true;
    }

    std::vector<EntityID> CollectStructureProjectiles(CWorld& world)
    {
        return DeterministicEntityIterator<StructureProjectileComponent>::CollectSorted(world);
    }

    void PushTowerAggroNotification(
        CWorld& world,
        EntityID attacker,
        EntityID victim)
    {
        TowerAggroNotifyComponent notify{};
        notify.attackerEntity = attacker;
        notify.victimEntity = victim;
        notify.priorityDuration = 2.f;
        world.AddComponent<TowerAggroNotifyComponent>(world.CreateEntity(), notify);
    }
```

(d) `VerifyDelayedUnbindCannotDeleteReplacement` 끝의 기존 코드:

```cpp
        entityMap.Unbind(serialized.projectileNetToUnbind);
        return entityMap.ToNet(replacement) == replacementNet;
    }
```

아래에 추가:

```cpp
    bool_t CheckTurretConfiguredRangeBoundary()
    {
        auto room = CGameRoomIntegrationProbeAccess::CreateRoom(9106u);
        CWorld& world = CGameRoomIntegrationProbeAccess::World(*room);
        EntityIdMap& entityMap =
            CGameRoomIntegrationProbeAccess::EntityMap(*room);

        const EntityID turret = SpawnTurret(
            world, entityMap, eTeam::Red, Vec3{});
        const f32_t configuredRange =
            ServerData::GetActiveLoLSpawnObjectDefinitionPack()
                .structure.turretAI.attackRange;
        const EntityID target = SpawnChampion(
            world,
            entityMap,
            eChampion::EZREAL,
            eTeam::Blue,
            Vec3{ configuredRange, 0.f, 0.f });
        if (turret == NULL_ENTITY ||
            target == NULL_ENTITY ||
            configuredRange <= 0.f ||
            !RebuildSpatialIndex(world))
        {
            return false;
        }

        auto system = GameplayTurret::CTurretAISystem::Create();
        system->Execute(world, DeterministicTime::kFixedDt);
        std::vector<EntityID> projectiles = CollectStructureProjectiles(world);
        if (projectiles.size() != 1u ||
            world.GetComponent<TurretAIComponent>(turret).attackTargetId !=
                target)
        {
            return false;
        }

        for (EntityID projectile : projectiles)
            world.DestroyEntity(projectile);
        world.GetComponent<TransformComponent>(target).SetPosition(
            Vec3{ configuredRange + 0.25f, 0.f, 0.f });
        world.GetComponent<TurretAIComponent>(turret).attackCooldown = 0.f;
        if (!RebuildSpatialIndex(world))
            return false;

        system->Execute(world, DeterministicTime::kFixedDt);
        return world.GetComponent<TurretAIComponent>(turret).attackTargetId ==
                NULL_ENTITY &&
            world.GetComponent<TurretComponent>(turret).targetId ==
                NULL_ENTITY &&
            CollectStructureProjectiles(world).empty();
    }

    bool_t CheckTurretRemoteAggroRejected()
    {
        auto room = CGameRoomIntegrationProbeAccess::CreateRoom(9107u);
        CWorld& world = CGameRoomIntegrationProbeAccess::World(*room);
        EntityIdMap& entityMap =
            CGameRoomIntegrationProbeAccess::EntityMap(*room);

        const EntityID turret = SpawnTurret(
            world, entityMap, eTeam::Red, Vec3{});
        const EntityID victim = SpawnChampion(
            world,
            entityMap,
            eChampion::EZREAL,
            eTeam::Red,
            Vec3{ 2.f, 0.f, 0.f });
        const EntityID attacker = SpawnChampion(
            world,
            entityMap,
            eChampion::EZREAL,
            eTeam::Blue,
            Vec3{ 40.f, 0.f, 0.f });
        if (turret == NULL_ENTITY ||
            victim == NULL_ENTITY ||
            attacker == NULL_ENTITY ||
            !RebuildSpatialIndex(world))
        {
            return false;
        }

        DamageRequest request{};
        request.source = attacker;
        request.target = victim;
        request.sourceTeam = eTeam::Blue;
        request.type = eDamageType::Magic;
        request.eSourceKind = eDamageSourceKind::Skill;
        request.rank = 1u;
        request.flatAmount = 50.f;
        request.skillId = static_cast<u16_t>(
            (static_cast<u32_t>(eChampion::EZREAL) << 8u) |
            static_cast<u32_t>(eSkillSlot::R));
        request.iSourceSlot = static_cast<u8_t>(eSkillSlot::R);
        world.AddComponent<DamageRequestComponent>(
            world.CreateEntity(),
            request);

        const f32_t victimHealthBefore =
            world.GetComponent<HealthComponent>(victim).fCurrent;
        TickContext damageTick = MakeTickContext(*room, 50u);
        CDamageQueueSystem::Execute(world, damageTick);
        const std::vector<EntityID> notifications =
            DeterministicEntityIterator<TowerAggroNotifyComponent>::CollectSorted(
                world);
        if (notifications.size() != 1u ||
            world.GetComponent<HealthComponent>(victim).fCurrent >=
                victimHealthBefore)
        {
            return false;
        }

        auto system = GameplayTurret::CTurretAISystem::Create();
        system->Execute(world, DeterministicTime::kFixedDt);
        const TurretAIComponent& ai =
            world.GetComponent<TurretAIComponent>(turret);
        return ai.aggroTargetId == NULL_ENTITY &&
            ai.attackTargetId == NULL_ENTITY &&
            CollectStructureProjectiles(world).empty();
    }

    bool_t CheckTurretAggroDropsOnRangeExit()
    {
        auto room = CGameRoomIntegrationProbeAccess::CreateRoom(9108u);
        CWorld& world = CGameRoomIntegrationProbeAccess::World(*room);
        EntityIdMap& entityMap =
            CGameRoomIntegrationProbeAccess::EntityMap(*room);

        const EntityID turret = SpawnTurret(
            world, entityMap, eTeam::Red, Vec3{});
        const EntityID victim = SpawnChampion(
            world,
            entityMap,
            eChampion::EZREAL,
            eTeam::Red,
            Vec3{ 2.f, 0.f, 0.f });
        const EntityID attacker = SpawnChampion(
            world,
            entityMap,
            eChampion::EZREAL,
            eTeam::Blue,
            Vec3{ 7.f, 0.f, 0.f });
        if (turret == NULL_ENTITY ||
            victim == NULL_ENTITY ||
            attacker == NULL_ENTITY ||
            !RebuildSpatialIndex(world))
        {
            return false;
        }

        PushTowerAggroNotification(world, attacker, victim);
        auto system = GameplayTurret::CTurretAISystem::Create();
        system->Execute(world, DeterministicTime::kFixedDt);
        std::vector<EntityID> projectiles = CollectStructureProjectiles(world);
        if (projectiles.size() != 1u ||
            world.GetComponent<TurretAIComponent>(turret).aggroTargetId !=
                attacker)
        {
            return false;
        }

        const EntityID firstProjectile = projectiles.front();
        world.GetComponent<TransformComponent>(attacker).SetPosition(
            Vec3{ 20.f, 0.f, 0.f });
        world.GetComponent<TurretAIComponent>(turret).attackCooldown = 0.f;
        if (!RebuildSpatialIndex(world))
            return false;

        system->Execute(world, DeterministicTime::kFixedDt);
        const TurretAIComponent& ai =
            world.GetComponent<TurretAIComponent>(turret);
        const std::vector<EntityID> remainingProjectiles =
            CollectStructureProjectiles(world);
        return ai.aggroTargetId == NULL_ENTITY &&
            ai.attackTargetId == NULL_ENTITY &&
            remainingProjectiles.size() == 1u &&
            remainingProjectiles.front() == firstProjectile;
    }
```

(e) `main` 기존 코드:

```cpp
    const bool_t bPassivePass = CheckPassiveExpiryBeforeFirstCommand();
    const bool_t bPass = bSkillPass &&
        bStructurePass &&
        bSkillGenerationPass &&
        bStructureGenerationPass &&
        bPassivePass;

    std::printf(
        "[GameRoomProjectileIntegration] %s: skill=%u structure=%u skill_generation=%u structure_generation=%u passive_pre_command=%u\n",
        bPass ? "PASS" : "FAIL",
        static_cast<u32_t>(bSkillPass),
        static_cast<u32_t>(bStructurePass),
        static_cast<u32_t>(bSkillGenerationPass),
        static_cast<u32_t>(bStructureGenerationPass),
        static_cast<u32_t>(bPassivePass));
```

아래로 교체:

```cpp
    const bool_t bPassivePass = CheckPassiveExpiryBeforeFirstCommand();
    const bool_t bTurretRangePass = CheckTurretConfiguredRangeBoundary();
    const bool_t bRemoteAggroPass = CheckTurretRemoteAggroRejected();
    const bool_t bAggroExitPass = CheckTurretAggroDropsOnRangeExit();
    const bool_t bPass = bSkillPass &&
        bStructurePass &&
        bSkillGenerationPass &&
        bStructureGenerationPass &&
        bPassivePass &&
        bTurretRangePass &&
        bRemoteAggroPass &&
        bAggroExitPass;

    std::printf(
        "[GameRoomProjectileIntegration] %s: skill=%u structure=%u skill_generation=%u structure_generation=%u passive_pre_command=%u turret_range=%u remote_aggro=%u aggro_exit=%u\n",
        bPass ? "PASS" : "FAIL",
        static_cast<u32_t>(bSkillPass),
        static_cast<u32_t>(bStructurePass),
        static_cast<u32_t>(bSkillGenerationPass),
        static_cast<u32_t>(bStructureGenerationPass),
        static_cast<u32_t>(bPassivePass),
        static_cast<u32_t>(bTurretRangePass),
        static_cast<u32_t>(bRemoteAggroPass),
        static_cast<u32_t>(bAggroExitPass));
```

## 3. 검증

예측:

- 일회성 상세 바이너리 audit 증거는 WMesh ordered hash 107/107, skin 오류 0과 flying_run 30 Hz/70키/channel hash 불일치 0/루프 seam 0.0255°다. 자동 gate는 converter metadata와 zeroed-root dry-run을 재실행하며 자산 파일은 수정하지 않는다.
- 드래곤은 60 FPS 기준 Animator 시간이 16.7ms마다 전진하고 slerp/lerp가 매 렌더 프레임 평가되어, 기존 125ms 계단이 사라진다. 다른 정글 몬스터는 기존 8/20 Hz 및 6개 예산을 유지한다.
- active definition의 정확한 중심 경계 7.75는 투사체 1, 7.75+0.25(8.40 broad-phase 안)는 타깃/투사체 0이다. 중심 7.00의 어그로 공격자가 20.00으로 이탈하면 기존 투사체 1개는 유지되고 추가 발사는 0이다.
- `kProjectileTargetMobileUnits`는 Structure 비트를 포함하지 않는다. 포탑 근처 피해자(2.00)에게 이즈리얼 R DamageRequest를 적용하면 DamageQueue notify 1개/실피해가 발생하지만, 원거리 공격자(40.00)는 어그로/투사체 0이다. 기존 StructureProjectile 추적/피해 lifecycle probe는 그대로 통과한다.
- Bot AI는 GameCommand 생산자이며 게임플레이 진실을 직접 변경하지 않는다.
- 다른 세션 dirty 파일(`SpawnObjectGameplayDefs.json`, `DamageQueueSystem.cpp`, `GameRoom*.cpp`)을 수정하지 않는다.

검증 명령:

```powershell
Tools/Bin/Release/WintersAssetConverter.exe info Client/Bin/Resource/Texture/Object/Jungle/Dragon/air/dragon_air_textured.wmesh
Tools/Bin/Release/WintersAssetConverter.exe info Client/Bin/Resource/Texture/Object/Jungle/Dragon/air/dragon_air_textured.wskel
Get-ChildItem Client/Bin/Resource/Texture/Object/Jungle/Dragon/air/anims/*.wanim | ForEach-Object { Tools/Bin/Release/WintersAssetConverter.exe info $_.FullName }
python Tools/Anim/patch_wanim_root_track.py Client/Bin/Resource/Texture/Object/Jungle/Dragon/air/anims/sru_dragon_flying_run.wanim Client/Bin/Resource/Texture/Object/Jungle/Dragon/air/dragon_air_textured.wskel --dry-run
powershell -ExecutionPolicy Bypass -File Tools/Harness/RunGameRoomProjectileIntegrationProbe.ps1
& "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find MSBuild/**/Bin/MSBuild.exe | Select-Object -First 1 | ForEach-Object { & $_ Client/Include/Client.vcxproj /m:1 /t:Build /p:Configuration=Debug /p:Platform=x64 /verbosity:minimal }
git diff --check -- Client/Private/Manager/Jungle_Manager.cpp Shared/GameSim/Systems/Turret/TurretAISystem.cpp Tools/Harness/GameRoomProjectileIntegrationProbe.cpp .md/plan/2026-07-19_DRAGON_ANIMATION_TURRET_RANGE_AGGRO_FIX_PLAN.md .md/plan/2026-07-19_DRAGON_ANIMATION_TURRET_RANGE_AGGRO_FIX_RESULT.md
```

미검증:

- 자동 probe/빌드는 실제 카메라 거리·FPS에서 날개 실루엣 체감을 판정하지 못한다. F5 전후 영상/육안 확인은 천장 30% 수동 항목이다.

확인 필요:

- 없음. 사거리 계약은 현재 authored 값 7.75의 중심↔중심으로 고정한다.

## 서브 에이전트 비평

```text
비평 주체: /root/dragon_turret_plan_critique (read-only, 2026-07-19)
- P0 수용: TickTurrets의 무명 TransformComponent&로는 xf 사용이 컴파일되지 않는다. 람다 선언 자체의 정확한 교체 블록을 2-2(b-1)에 추가했다.
- P0 수용: 경량 CreateRoom은 spatial index를 초기화하지 않는다. RebuildSpatialIndex가 null이면 DefaultSpatialGridDesc로 초기화하도록 2-3(c)를 수정했다.
- P1 수용: 드래곤 UpdateCalls가 비드래곤 6개 예산을 차감하지 않도록 총 animCount와 budgetedAnimCount를 분리했다.
- P1 수용: harness turret 값을 active SpawnObject definition에서 가져오고 정확한 7.75 포함 경계 성공과 +0.25 실패를 한 probe에서 검증한다.
- P1 수용: R의 Structure 비트 제외를 static_assert로 고정하고, synthetic notify 대신 Ezreal R DamageRequest를 DamageQueue에 넣어 실피해→notify→TurretAI 경로를 검증한다.
- P1 수용: 사거리 이탈 시 첫 투사체를 파괴하지 않고 동일 entity 1개 유지 + 추가 발사 0을 확인한다. 기존 lifecycle probe가 추적/피해 종료를 별도로 보장한다.
- P1 수용: 날개 정점/해시/키/seam 상세치는 이번 세션의 read-only 일회성 분석 증거로 분리하고, 자동 gate는 converter info + root-track dry-run으로 재현 가능한 범위를 명시했다.
- P1 수용: 소스 수정 전 baseline은 Server/Client Debug 빌드 성공. 기존 projectile 4항목은 1, 기존 dirty 세션의 passive_pre_command만 이미 0이라 probe 전체 exit 1이다. 이후 신규 항목을 개별 판독한다.
- P2 수용: harness 삽입 위치를 실제 기존 코드 끝 블록 + 아래에 추가 형식으로 바꿨다.
- P2 수용: tier별 데이터가 아니라 전역 turretAI.attackRange를 모든 tier가 소비하며, 107본/106채널은 루트 1본 rest pose라는 표현으로 정밀화했다.
- 기각/보류: 없음. 위 수정 후 구현 진행 승인.
```
