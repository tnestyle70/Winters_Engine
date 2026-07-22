#include "Game/GameRoom.h"

#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "Shared/GameSim/Champions/Ezreal/EzrealGameSim.h"
#include "Shared/GameSim/Champions/LeeSin/LeeSinGameSim.h"
#include "Shared/GameSim/Components/BuffComponent.h"
#include "Shared/GameSim/Components/ChampionDefinitionComponent.h"
#include "Shared/GameSim/Components/EzrealSimComponent.h"
#include "Shared/GameSim/Components/LeeSinSimComponent.h"
#include "Shared/GameSim/Components/NetEntityIdComponent.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/SkillLoadoutComponent.h"
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Systems/Buff/BuffSystem.h"
#include "Shared/GameSim/Systems/Damage/DamageQueueSystem.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/ReplicatedEventSerializer/ReplicatedEventSerializer.h"
#include "Shared/GameSim/Systems/Stat/StatSystem.h"
#include "Shared/GameSim/Systems/Turret/TurretAISystem.h"
#include "Server/Private/Data/RuntimeGameplayDefinitionOverlay.h"

#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include "Shared/Schemas/Generated/cpp/Event_generated.h"
#pragma pop_macro("max")
#pragma pop_macro("min")

#include <cmath>
#include <cstdio>
#include <memory>
#include <utility>
#include <vector>

CGameRoom* g_pRoom = nullptr;

class CGameRoomIntegrationProbeAccess
{
public:
    static std::unique_ptr<CGameRoom> CreateRoom(u32_t roomId)
    {
        return std::unique_ptr<CGameRoom>(new CGameRoom(roomId));
    }

    static CWorld& World(CGameRoom& room)
    {
        return room.m_world;
    }

    static EntityIdMap& EntityMap(CGameRoom& room)
    {
        return room.m_entityMap;
    }

    static void EnterInGame(CGameRoom& room)
    {
        room.m_pLobbyAuthority->m_phase = eRoomPhase::InGame;
    }

    static void RunProjectilePhase(CGameRoom& room, TickContext& tc)
    {
        room.Phase_ServerProjectiles(tc);
    }

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

    static EntityID SpawnStructureProjectile(
        CWorld& world,
        EntityID source,
        EntityID target)
    {
        auto system = GameplayTurret::CTurretAISystem::Create();
        system->SpawnProjectile(world, source, target);
        const auto projectiles =
            DeterministicEntityIterator<StructureProjectileComponent>::CollectSorted(world);
        return projectiles.size() == 1u ? projectiles.front() : NULL_ENTITY;
    }

    static void SetTickIndex(CGameRoom& room, u64_t tickIndex)
    {
        room.m_tickIndex = tickIndex;
    }

    static void SetExecutor(
        CGameRoom& room,
        std::unique_ptr<ICommandExecutor> executor)
    {
        room.m_pExecutor = std::move(executor);
    }

    static void PushCommand(CGameRoom& room, const GameCommand& command)
    {
        room.m_pendingExecCommands.push_back(command);
    }

    static void RunFullTick(CGameRoom& room)
    {
        room.Tick();
    }
};

namespace
{
    static_assert((kProjectileTargetMobileUnits &
        static_cast<u8_t>(ProjectileTarget_Structure)) == 0u);

    struct ObservingCommandExecutor final : ICommandExecutor
    {
        EntityID observedEntity = NULL_ENTITY;
        u32_t callCount = 0u;
        f32_t observedBonusAttackSpeed = -1.f;
        bool_t bObservedPassiveAbsent = false;

        CommandExecutionResult ExecuteCommand(
            CWorld& world,
            const TickContext&,
            const GameCommand&) override
        {
            ++callCount;
            if (world.HasComponent<StatComponent>(observedEntity))
            {
                observedBonusAttackSpeed =
                    world.GetComponent<StatComponent>(observedEntity).bonusAttackSpeed;
            }

            bObservedPassiveAbsent = true;
            if (world.HasComponent<BuffComponent>(observedEntity))
            {
                const BuffComponent& buffs =
                    world.GetComponent<BuffComponent>(observedEntity);
                for (u8_t i = 0u;
                    i < buffs.count && i < BuffComponent::kMaxBuffs;
                    ++i)
                {
                    if (buffs.buffs[i].buffDefId ==
                        kEzrealRisingSpellForceBuffDefId)
                    {
                        bObservedPassiveAbsent = false;
                    }
                }
            }
            return CommandExecutionResult{};
        }
    };

    NetEntityId BindNetworkEntity(
        CWorld& world,
        EntityIdMap& entityMap,
        EntityID entity)
    {
        const NetEntityId netId = entityMap.IssueNew(entity);
        NetEntityIdComponent net{};
        net.netId = netId;
        world.AddComponent<NetEntityIdComponent>(entity, net);
        return netId;
    }

    EntityID SpawnChampion(
        CWorld& world,
        EntityIdMap& entityMap,
        eChampion championId,
        eTeam team,
        const Vec3& position)
    {
        const EntityHandle handle = world.CreateEntityHandle();
        if (!handle.IsValid())
            return NULL_ENTITY;
        const EntityID entity = handle.GetIndex();

        TransformComponent transform{};
        transform.SetPosition(position);
        world.AddComponent<TransformComponent>(entity, transform);

        HealthComponent health{};
        health.fCurrent = 1000.f;
        health.fMaximum = 1000.f;
        world.AddComponent<HealthComponent>(entity, health);

        ChampionComponent champion{};
        champion.id = championId;
        champion.team = team;
        champion.hp = health.fCurrent;
        champion.maxHp = health.fMaximum;
        world.AddComponent<ChampionComponent>(entity, champion);

        const GameplayDefinitionPack& definitions =
            ServerData::GetActiveLoLGameplayDefinitionPack();
        const ChampionGameplayDef* championDef =
            definitions.FindChampion(championId);
        if (!championDef)
            return NULL_ENTITY;

        ChampionDefinitionComponent identity{};
        identity.championDefId = championDef->id;
        world.AddComponent<ChampionDefinitionComponent>(entity, identity);

        SkillLoadoutComponent loadout{};
        for (u8_t slot = 0u; slot < kChampionSkillSlotCount; ++slot)
            loadout.skills[slot] = championDef->skillLoadout[slot];
        world.AddComponent<SkillLoadoutComponent>(entity, loadout);

        StatComponent stat = CStatSystem::BuildBaseStats(
            championDef->stats,
            championDef->legacyChampion,
            1u);
        world.AddComponent<StatComponent>(entity, stat);

        SpatialAgentComponent spatial{};
        spatial.kind = eSpatialKind::Character;
        spatial.team = static_cast<u8_t>(team);
        spatial.radius = 0.65f;
        world.AddComponent<SpatialAgentComponent>(entity, spatial);
        world.AddComponent<TargetableTag>(entity, TargetableTag{});

        if (BindNetworkEntity(world, entityMap, entity) == NULL_NET_ENTITY)
            return NULL_ENTITY;
        return entity;
    }

    EntityID SpawnTurret(
        CWorld& world,
        EntityIdMap& entityMap,
        eTeam team,
        const Vec3& position)
    {
        const EntityHandle handle = world.CreateEntityHandle();
        if (!handle.IsValid())
            return NULL_ENTITY;
        const EntityID entity = handle.GetIndex();

        TransformComponent transform{};
        transform.SetPosition(position);
        world.AddComponent<TransformComponent>(entity, transform);
        world.AddComponent<HealthComponent>(entity, HealthComponent{});

        TurretComponent turret{};
        turret.team = team;
        world.AddComponent<TurretComponent>(entity, turret);

        const TurretAIGameDef& turretDef =
            ServerData::GetActiveLoLSpawnObjectDefinitionPack()
                .structure.turretAI;
        TurretAIComponent ai{};
        ai.attackRange = turretDef.attackRange;
        ai.attackCooldownMax = turretDef.attackCooldownMax;
        ai.attackDamage = turretDef.attackDamage;
        ai.projectileSpeed = turretDef.projectileSpeed;
        world.AddComponent<TurretAIComponent>(entity, ai);

        if (BindNetworkEntity(world, entityMap, entity) == NULL_NET_ENTITY)
            return NULL_ENTITY;
        return entity;
    }

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

    TickContext MakeTickContext(
        CGameRoom& room,
        u64_t tickIndex)
    {
        TickContext tc{};
        tc.tickIndex = tickIndex;
        tc.fDt = DeterministicTime::kFixedDt;
        tc.fSimulatedTimeSec = DeterministicTime::TickToSec(tickIndex);
        tc.pEntityMap = &CGameRoomIntegrationProbeAccess::EntityMap(room);
        tc.pWalkable = &room;
        tc.pDefinitions = &ServerData::GetActiveLoLGameplayDefinitionPack();
        return tc;
    }

    bool_t FindProjectileEvent(
        CWorld& world,
        EntityID projectileEntity,
        eReplicatedEventKind kind,
        ReplicatedEventComponent& outEvent)
    {
        bool_t bFound = false;
        world.ForEach<ReplicatedEventComponent>(
            [&](EntityID, ReplicatedEventComponent& event)
            {
                if (!bFound &&
                    event.kind == kind &&
                    event.projectileEntity == projectileEntity)
                {
                    outEvent = event;
                    bFound = true;
                }
            });
        return bFound;
    }

    bool_t HasDamageRequestForTarget(
        CWorld& world,
        EntityID target)
    {
        bool_t bFound = false;
        world.ForEach<DamageRequestComponent>(
            [&](EntityID, DamageRequestComponent& request)
            {
                if (request.target == target)
                    bFound = true;
            });
        return bFound;
    }

    bool_t ReplaceNetworkedChampionInSameSlot(
        CWorld& world,
        EntityIdMap& entityMap,
        EntityID oldTarget,
        const EntityHandle& oldTargetHandle,
        NetEntityId oldTargetNet,
        const Vec3& position,
        EntityID& outReplacement,
        NetEntityId& outReplacementNet)
    {
        world.DestroyEntity(oldTarget);
        if (world.IsAlive(oldTargetHandle) ||
            world.ResolveEntity(oldTargetHandle) != NULL_ENTITY)
        {
            return false;
        }

        entityMap.Unbind(oldTargetNet);
        outReplacement = SpawnChampion(
            world,
            entityMap,
            eChampion::EZREAL,
            eTeam::Red,
            position);
        if (outReplacement != oldTarget)
            return false;

        const EntityHandle replacementHandle =
            world.GetEntityHandle(outReplacement);
        outReplacementNet = entityMap.ToNet(outReplacement);
        return replacementHandle.IsValid() &&
            replacementHandle != oldTargetHandle &&
            replacementHandle.GetIndex() == oldTargetHandle.GetIndex() &&
            outReplacementNet != NULL_NET_ENTITY &&
            outReplacementNet != oldTargetNet &&
            entityMap.FromNet(oldTargetNet) == NULL_ENTITY &&
            entityMap.FromNet(outReplacementNet) == outReplacement;
    }

    bool_t VerifySerializedTargetInvalid(
        CWorld& world,
        EntityIdMap& entityMap,
        const ReplicatedEventComponent& terminalEvent,
        u64_t tickIndex,
        NetEntityId expectedProjectileNet,
        NetEntityId expectedTargetNet)
    {
        SharedSim::SerializedReplicatedEvent serialized{};
        if (!SharedSim::CReplicatedEventSerializer::Build(
                world,
                entityMap,
                terminalEvent,
                tickIndex,
                1u,
                serialized) ||
            !serialized.bValid ||
            !serialized.bUnbindProjectileAfterSend ||
            serialized.projectileNetToUnbind != expectedProjectileNet)
        {
            return false;
        }

        const auto* packet = Shared::Schema::GetEventPacket(
            serialized.payload.data());
        const auto* hit = packet ? packet->projectileHit() : nullptr;
        return packet && hit &&
            packet->kind() == Shared::Schema::EventKind::ProjectileHit &&
            hit->netId() == expectedProjectileNet &&
            hit->targetNet() == expectedTargetNet &&
            hit->contactReason() ==
                Shared::Schema::ProjectileContactReason::TargetInvalid &&
            hit->bDestroyed();
    }

    bool_t VerifyDelayedUnbindCannotDeleteReplacement(
        CWorld& world,
        EntityIdMap& entityMap,
        EntityID destroyedProjectile,
        NetEntityId oldProjectileNet,
        const ReplicatedEventComponent& terminalEvent,
        u64_t tickIndex)
    {
        const EntityID replacement = world.CreateEntity();
        if (replacement != destroyedProjectile)
            return false;
        const NetEntityId replacementNet =
            BindNetworkEntity(world, entityMap, replacement);
        if (replacementNet == NULL_NET_ENTITY ||
            replacementNet == oldProjectileNet)
        {
            return false;
        }

        SharedSim::SerializedReplicatedEvent serialized{};
        if (!SharedSim::CReplicatedEventSerializer::Build(
                world,
                entityMap,
                terminalEvent,
                tickIndex,
                1u,
                serialized) ||
            !serialized.bUnbindProjectileAfterSend ||
            serialized.projectileNetToUnbind != oldProjectileNet)
        {
            return false;
        }

        entityMap.Unbind(serialized.projectileNetToUnbind);
        return entityMap.ToNet(replacement) == replacementNet;
    }

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

    bool_t CheckSkillProjectileGameRoomLifecycle()
    {
        auto room = CGameRoomIntegrationProbeAccess::CreateRoom(9101u);
        CGameRoomIntegrationProbeAccess::EnterInGame(*room);
        CWorld& world = CGameRoomIntegrationProbeAccess::World(*room);
        EntityIdMap& entityMap =
            CGameRoomIntegrationProbeAccess::EntityMap(*room);

        const EntityID source = SpawnChampion(
            world, entityMap, eChampion::EZREAL, eTeam::Blue, Vec3{});
        const EntityID target = SpawnChampion(
            world,
            entityMap,
            eChampion::EZREAL,
            eTeam::Red,
            Vec3{ 0.1f, 0.f, 0.f });
        if (source == NULL_ENTITY || target == NULL_ENTITY)
            return false;

        DamageRequest request{};
        request.source = source;
        request.target = target;
        request.sourceTeam = eTeam::Blue;
        request.type = eDamageType::Physical;
        request.eSourceKind = eDamageSourceKind::BasicAttack;
        request.rank = 1u;
        request.flatAmount = 25.f;
        request.flags = DamageFlag_OnHit;

        TickContext spawnTick = MakeTickContext(*room, 10u);
        if (!EzrealGameSim::TryLaunchBasicAttackProjectile(
                world,
                spawnTick,
                source,
                target,
                request))
        {
            return false;
        }

        const auto projectiles =
            DeterministicEntityIterator<SkillProjectileComponent>::CollectSorted(world);
        if (projectiles.size() != 1u)
            return false;
        const EntityID projectileEntity = projectiles.front();
        const SkillProjectileComponent& beforeSpawn =
            world.GetComponent<SkillProjectileComponent>(projectileEntity);
        if (beforeSpawn.sourceHandle != world.GetEntityHandle(source) ||
            beforeSpawn.targetHandle != world.GetEntityHandle(target))
        {
            return false;
        }

        CGameRoomIntegrationProbeAccess::RunProjectilePhase(*room, spawnTick);
        const NetEntityId projectileNet = entityMap.ToNet(projectileEntity);
        ReplicatedEventComponent spawnEvent{};
        if (projectileNet == NULL_NET_ENTITY ||
            !world.IsAlive(projectileEntity) ||
            !FindProjectileEvent(
                world,
                projectileEntity,
                eReplicatedEventKind::ProjectileSpawn,
                spawnEvent) ||
            spawnEvent.projectileNetOverride != projectileNet)
        {
            return false;
        }

        TickContext hitTick = MakeTickContext(*room, 11u);
        CGameRoomIntegrationProbeAccess::RunProjectilePhase(*room, hitTick);
        ReplicatedEventComponent terminalEvent{};
        if (world.IsAlive(projectileEntity) ||
            entityMap.ToNet(projectileEntity) != NULL_NET_ENTITY ||
            !FindProjectileEvent(
                world,
                projectileEntity,
                eReplicatedEventKind::ProjectileHit,
                terminalEvent) ||
            terminalEvent.eContactReason != ProjectileContactReason::UnitHit ||
            !terminalEvent.bDestroyed ||
            terminalEvent.projectileNetOverride != projectileNet)
        {
            return false;
        }

        return VerifyDelayedUnbindCannotDeleteReplacement(
            world,
            entityMap,
            projectileEntity,
            projectileNet,
            terminalEvent,
            hitTick.tickIndex);
    }

    bool_t CheckStructureProjectileGameRoomLifecycle()
    {
        auto room = CGameRoomIntegrationProbeAccess::CreateRoom(9102u);
        CGameRoomIntegrationProbeAccess::EnterInGame(*room);
        CWorld& world = CGameRoomIntegrationProbeAccess::World(*room);
        EntityIdMap& entityMap =
            CGameRoomIntegrationProbeAccess::EntityMap(*room);

        const EntityID source = SpawnTurret(
            world, entityMap, eTeam::Blue, Vec3{});
        const EntityID target = SpawnChampion(
            world,
            entityMap,
            eChampion::EZREAL,
            eTeam::Red,
            Vec3{ 0.1f, 2.8f, 0.f });
        if (source == NULL_ENTITY || target == NULL_ENTITY)
            return false;

        const EntityID projectileEntity =
            CGameRoomIntegrationProbeAccess::SpawnStructureProjectile(
                world,
                source,
                target);
        if (projectileEntity == NULL_ENTITY)
            return false;
        const StructureProjectileComponent& projectile =
            world.GetComponent<StructureProjectileComponent>(projectileEntity);
        if (projectile.sourceHandle != world.GetEntityHandle(source) ||
            projectile.targetHandle != world.GetEntityHandle(target))
        {
            return false;
        }

        TickContext hitTick = MakeTickContext(*room, 20u);
        CGameRoomIntegrationProbeAccess::RunProjectilePhase(*room, hitTick);

        ReplicatedEventComponent spawnEvent{};
        ReplicatedEventComponent terminalEvent{};
        if (!FindProjectileEvent(
                world,
                projectileEntity,
                eReplicatedEventKind::ProjectileSpawn,
                spawnEvent) ||
            !FindProjectileEvent(
                world,
                projectileEntity,
                eReplicatedEventKind::ProjectileHit,
                terminalEvent) ||
            spawnEvent.projectileNetOverride == NULL_NET_ENTITY ||
            terminalEvent.projectileNetOverride !=
                spawnEvent.projectileNetOverride ||
            terminalEvent.eContactReason != ProjectileContactReason::UnitHit ||
            !terminalEvent.bDestroyed ||
            world.IsAlive(projectileEntity) ||
            entityMap.ToNet(projectileEntity) != NULL_NET_ENTITY)
        {
            return false;
        }

        return VerifyDelayedUnbindCannotDeleteReplacement(
            world,
            entityMap,
            projectileEntity,
            spawnEvent.projectileNetOverride,
            terminalEvent,
            hitTick.tickIndex);
    }

    bool_t CheckSkillProjectileTargetGenerationSafety()
    {
        auto room = CGameRoomIntegrationProbeAccess::CreateRoom(9104u);
        CGameRoomIntegrationProbeAccess::EnterInGame(*room);
        CWorld& world = CGameRoomIntegrationProbeAccess::World(*room);
        EntityIdMap& entityMap =
            CGameRoomIntegrationProbeAccess::EntityMap(*room);
        const Vec3 targetPosition{ 12.f, 0.f, 0.f };

        const EntityID source = SpawnChampion(
            world, entityMap, eChampion::EZREAL, eTeam::Blue, Vec3{});
        const EntityID target = SpawnChampion(
            world,
            entityMap,
            eChampion::EZREAL,
            eTeam::Red,
            targetPosition);
        if (source == NULL_ENTITY || target == NULL_ENTITY)
            return false;

        const EntityHandle oldTargetHandle = world.GetEntityHandle(target);
        const NetEntityId oldTargetNet = entityMap.ToNet(target);
        if (!oldTargetHandle.IsValid() || oldTargetNet == NULL_NET_ENTITY)
            return false;

        DamageRequest request{};
        request.source = source;
        request.target = target;
        request.sourceTeam = eTeam::Blue;
        request.type = eDamageType::Physical;
        request.eSourceKind = eDamageSourceKind::BasicAttack;
        request.rank = 1u;
        request.flatAmount = 25.f;
        request.flags = DamageFlag_OnHit;

        TickContext spawnTick = MakeTickContext(*room, 30u);
        if (!EzrealGameSim::TryLaunchBasicAttackProjectile(
                world,
                spawnTick,
                source,
                target,
                request))
        {
            return false;
        }

        const auto projectiles =
            DeterministicEntityIterator<SkillProjectileComponent>::CollectSorted(world);
        if (projectiles.size() != 1u)
            return false;
        const EntityID projectileEntity = projectiles.front();
        const SkillProjectileComponent& beforeSpawn =
            world.GetComponent<SkillProjectileComponent>(projectileEntity);
        if (beforeSpawn.targetHandle != oldTargetHandle)
            return false;

        CGameRoomIntegrationProbeAccess::RunProjectilePhase(*room, spawnTick);
        const NetEntityId projectileNet = entityMap.ToNet(projectileEntity);
        ReplicatedEventComponent spawnEvent{};
        if (projectileNet == NULL_NET_ENTITY ||
            !world.IsAlive(projectileEntity) ||
            !FindProjectileEvent(
                world,
                projectileEntity,
                eReplicatedEventKind::ProjectileSpawn,
                spawnEvent) ||
            spawnEvent.projectileNetOverride != projectileNet ||
            spawnEvent.targetNetOverride != oldTargetNet ||
            world.GetComponent<SkillProjectileComponent>(projectileEntity)
                .uTargetNetAtSpawn != oldTargetNet)
        {
            return false;
        }

        EntityID replacement = NULL_ENTITY;
        NetEntityId replacementNet = NULL_NET_ENTITY;
        if (!ReplaceNetworkedChampionInSameSlot(
                world,
                entityMap,
                target,
                oldTargetHandle,
                oldTargetNet,
                targetPosition,
                replacement,
                replacementNet))
        {
            return false;
        }
        const f32_t replacementHealthBefore =
            world.GetComponent<HealthComponent>(replacement).fCurrent;

        TickContext invalidTick = MakeTickContext(*room, 31u);
        CGameRoomIntegrationProbeAccess::RunProjectilePhase(*room, invalidTick);
        ReplicatedEventComponent terminalEvent{};
        const bool_t bFoundTerminal = FindProjectileEvent(
            world,
            projectileEntity,
            eReplicatedEventKind::ProjectileHit,
            terminalEvent);
        const bool_t bNoQueuedDamage =
            !HasDamageRequestForTarget(world, replacement);
        const bool_t bSerialized = bFoundTerminal &&
            VerifySerializedTargetInvalid(
                world,
                entityMap,
                terminalEvent,
                invalidTick.tickIndex,
                projectileNet,
                oldTargetNet);

        CDamageQueueSystem::Execute(world, invalidTick);
        const f32_t replacementHealthAfter =
            world.GetComponent<HealthComponent>(replacement).fCurrent;
        return bFoundTerminal &&
            terminalEvent.eContactReason ==
                ProjectileContactReason::TargetInvalid &&
            terminalEvent.targetEntity == NULL_ENTITY &&
            terminalEvent.targetNetOverride == oldTargetNet &&
            terminalEvent.projectileNetOverride == projectileNet &&
            terminalEvent.bDestroyed &&
            !world.IsAlive(projectileEntity) &&
            entityMap.ToNet(projectileEntity) == NULL_NET_ENTITY &&
            entityMap.ToNet(replacement) == replacementNet &&
            entityMap.FromNet(oldTargetNet) == NULL_ENTITY &&
            bNoQueuedDamage &&
            bSerialized &&
            std::fabs(replacementHealthAfter - replacementHealthBefore) <=
                0.0001f;
    }

    bool_t CheckStructureProjectileTargetGenerationSafety()
    {
        auto room = CGameRoomIntegrationProbeAccess::CreateRoom(9105u);
        CGameRoomIntegrationProbeAccess::EnterInGame(*room);
        CWorld& world = CGameRoomIntegrationProbeAccess::World(*room);
        EntityIdMap& entityMap =
            CGameRoomIntegrationProbeAccess::EntityMap(*room);
        const Vec3 targetPosition{ 12.f, 2.8f, 0.f };

        const EntityID source = SpawnTurret(
            world, entityMap, eTeam::Blue, Vec3{});
        const EntityID target = SpawnChampion(
            world,
            entityMap,
            eChampion::EZREAL,
            eTeam::Red,
            targetPosition);
        if (source == NULL_ENTITY || target == NULL_ENTITY)
            return false;

        const EntityHandle oldTargetHandle = world.GetEntityHandle(target);
        const NetEntityId oldTargetNet = entityMap.ToNet(target);
        if (!oldTargetHandle.IsValid() || oldTargetNet == NULL_NET_ENTITY)
            return false;

        const EntityID projectileEntity =
            CGameRoomIntegrationProbeAccess::SpawnStructureProjectile(
                world,
                source,
                target);
        if (projectileEntity == NULL_ENTITY ||
            world.GetComponent<StructureProjectileComponent>(projectileEntity)
                .targetHandle != oldTargetHandle)
        {
            return false;
        }

        TickContext spawnTick = MakeTickContext(*room, 40u);
        CGameRoomIntegrationProbeAccess::RunProjectilePhase(*room, spawnTick);
        const NetEntityId projectileNet = entityMap.ToNet(projectileEntity);
        ReplicatedEventComponent spawnEvent{};
        if (projectileNet == NULL_NET_ENTITY ||
            !world.IsAlive(projectileEntity) ||
            !FindProjectileEvent(
                world,
                projectileEntity,
                eReplicatedEventKind::ProjectileSpawn,
                spawnEvent) ||
            spawnEvent.projectileNetOverride != projectileNet ||
            spawnEvent.targetNetOverride != oldTargetNet ||
            world.GetComponent<StructureProjectileComponent>(projectileEntity)
                .uTargetNetAtSpawn != oldTargetNet)
        {
            return false;
        }

        EntityID replacement = NULL_ENTITY;
        NetEntityId replacementNet = NULL_NET_ENTITY;
        if (!ReplaceNetworkedChampionInSameSlot(
                world,
                entityMap,
                target,
                oldTargetHandle,
                oldTargetNet,
                targetPosition,
                replacement,
                replacementNet))
        {
            return false;
        }
        const f32_t replacementHealthBefore =
            world.GetComponent<HealthComponent>(replacement).fCurrent;

        TickContext invalidTick = MakeTickContext(*room, 41u);
        CGameRoomIntegrationProbeAccess::RunProjectilePhase(*room, invalidTick);
        ReplicatedEventComponent terminalEvent{};
        const bool_t bFoundTerminal = FindProjectileEvent(
            world,
            projectileEntity,
            eReplicatedEventKind::ProjectileHit,
            terminalEvent);
        const bool_t bNoQueuedDamage =
            !HasDamageRequestForTarget(world, replacement);
        const bool_t bSerialized = bFoundTerminal &&
            VerifySerializedTargetInvalid(
                world,
                entityMap,
                terminalEvent,
                invalidTick.tickIndex,
                projectileNet,
                oldTargetNet);

        CDamageQueueSystem::Execute(world, invalidTick);
        const f32_t replacementHealthAfter =
            world.GetComponent<HealthComponent>(replacement).fCurrent;
        return bFoundTerminal &&
            terminalEvent.eContactReason ==
                ProjectileContactReason::TargetInvalid &&
            terminalEvent.targetEntity == NULL_ENTITY &&
            terminalEvent.targetNetOverride == oldTargetNet &&
            terminalEvent.projectileNetOverride == projectileNet &&
            terminalEvent.bDestroyed &&
            !world.IsAlive(projectileEntity) &&
            entityMap.ToNet(projectileEntity) == NULL_NET_ENTITY &&
            entityMap.ToNet(replacement) == replacementNet &&
            entityMap.FromNet(oldTargetNet) == NULL_ENTITY &&
            bNoQueuedDamage &&
            bSerialized &&
            std::fabs(replacementHealthAfter - replacementHealthBefore) <=
                0.0001f;
    }

    bool_t CheckPassiveExpiryBeforeFirstCommand()
    {
        auto room = CGameRoomIntegrationProbeAccess::CreateRoom(9103u);
        CGameRoomIntegrationProbeAccess::EnterInGame(*room);
        CWorld& world = CGameRoomIntegrationProbeAccess::World(*room);
        EntityIdMap& entityMap =
            CGameRoomIntegrationProbeAccess::EntityMap(*room);
        const EntityID ezreal = SpawnChampion(
            world, entityMap, eChampion::EZREAL, eTeam::Blue, Vec3{});
        if (ezreal == NULL_ENTITY)
            return false;

        BuffInstance passive{};
        passive.buffDefId = kEzrealRisingSpellForceBuffDefId;
        passive.source = ezreal;
        passive.fDurationRemaining = 6.f;
        passive.uExpireTick = 181u;
        passive.stackCount = 5u;
        passive.bonusAttackSpeedPerStack = 0.10f;
        BuffComponent buffs{};
        if (!CBuffSystem::AddOrRefresh(buffs, passive))
            return false;
        world.AddComponent<BuffComponent>(ezreal, buffs);

        StatComponent& stat = world.GetComponent<StatComponent>(ezreal);
        stat.bDirty = true;
        CStatSystem::Execute(world);
        if (std::fabs(stat.bonusAttackSpeed - 0.50f) > 0.0001f)
            return false;

        auto observer = std::make_unique<ObservingCommandExecutor>();
        ObservingCommandExecutor* observerPtr = observer.get();
        observerPtr->observedEntity = ezreal;
        CGameRoomIntegrationProbeAccess::SetExecutor(
            *room,
            std::move(observer));

        GameCommand command{};
        command.kind = eCommandKind::Move;
        command.issuerEntity = ezreal;
        command.sequenceNum = 1u;
        CGameRoomIntegrationProbeAccess::PushCommand(*room, command);
        CGameRoomIntegrationProbeAccess::SetTickIndex(*room, 180u);
        CGameRoomIntegrationProbeAccess::RunFullTick(*room);

        const bool_t bPass = observerPtr->callCount == 1u &&
            observerPtr->bObservedPassiveAbsent &&
            std::fabs(observerPtr->observedBonusAttackSpeed) <= 0.0001f &&
            room->GetCurrentTickIndex() == 181u;
        if (!bPass)
        {
            std::printf(
                "[GameRoomProjectileIntegration][Passive] calls=%u absent=%u bonus=%.3f tick=%llu\n",
                observerPtr->callCount,
                static_cast<u32_t>(observerPtr->bObservedPassiveAbsent),
                observerPtr->observedBonusAttackSpeed,
                static_cast<unsigned long long>(room->GetCurrentTickIndex()));
        }
        return bPass;
    }

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
        {
            std::printf("[GameRoomProjectileIntegration][LeeQ] spawn failed\n");
            return false;
        }
        world.AddComponent<SkillStateComponent>(source, SkillStateComponent{});
        SkillRankComponent ranks{};
        ranks.ranks[kQSlot] = 1u;
        world.AddComponent<SkillRankComponent>(source, ranks);
        world.GetComponent<ChampionComponent>(source).mana = 1000.f;
        if (!RebuildSpatialIndex(world))
        {
            std::printf("[GameRoomProjectileIntegration][LeeQ] spatial rebuild failed\n");
            return false;
        }

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
        {
            const SkillCommandFeedback* q1Feedback =
                CGameRoomIntegrationProbeAccess::FindFeedback(
                    *room, kSessionId, kQSlot);
            std::printf(
                "[GameRoomProjectileIntegration][LeeQ] Q1 projectile count=%zu feedback=%u/%u\n",
                projectiles.size(),
                q1Feedback ? static_cast<u32_t>(q1Feedback->result.state) : 99u,
                q1Feedback ? static_cast<u32_t>(q1Feedback->result.reason) : 99u);
            return false;
        }
        const EntityID projectile = projectiles.front();
        const SkillProjectileComponent before =
            world.GetComponent<SkillProjectileComponent>(projectile);
        if (before.kind != eProjectileKind::LeeSinQ ||
            std::fabs(before.speed - 19.2f) > 0.0001f ||
            before.bCollidesWithTerrain ||
            !before.bBlockedByProjectileBarriers)
        {
            std::printf(
                "[GameRoomProjectileIntegration][LeeQ] projectile contract kind=%u speed=%.3f terrain=%u barrier=%u\n",
                static_cast<u32_t>(before.kind),
                before.speed,
                static_cast<u32_t>(before.bCollidesWithTerrain),
                static_cast<u32_t>(before.bBlockedByProjectileBarriers));
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
            std::printf(
                "[GameRoomProjectileIntegration][LeeQ] hit=%u mark=%u sourceMatch=%u\n",
                static_cast<u32_t>(bUnitHit),
                static_cast<u32_t>(world.HasComponent<LeeSinQMarkComponent>(target)),
                static_cast<u32_t>(world.HasComponent<LeeSinQMarkComponent>(target) &&
                    world.GetComponent<LeeSinQMarkComponent>(target).sourceEntity == source));
            return false;
        }
        auto& qSlot = world.GetComponent<SkillStateComponent>(source).slots[kQSlot];
        if (qSlot.currentStage != 1u || qSlot.stageWindow <= 0.f)
        {
            std::printf(
                "[GameRoomProjectileIntegration][LeeQ] Q1 stage=%u window=%.3f\n",
                static_cast<u32_t>(qSlot.currentStage), qSlot.stageWindow);
            return false;
        }

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
            std::printf(
                "[GameRoomProjectileIntegration][LeeQ] Q2 feedback=%u/%u mark=%u dash=%u ignore=%u\n",
                accepted ? static_cast<u32_t>(accepted->result.state) : 99u,
                accepted ? static_cast<u32_t>(accepted->result.reason) : 99u,
                static_cast<u32_t>(world.HasComponent<LeeSinQMarkComponent>(target)),
                static_cast<u32_t>(world.HasComponent<LeeSinDashComponent>(source)),
                static_cast<u32_t>(world.HasComponent<LeeSinDashComponent>(source) &&
                    world.GetComponent<LeeSinDashComponent>(source)
                        .bIgnoreTerrainDuringTransit));
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
        const bool_t bRejectedPreserved = rejected &&
            rejected->result.state == eCommandExecutionState::Rejected &&
            rejected->result.reason == eCommandExecutionReason::ChampionRuleBlocked &&
            rejected->authoritativeSkillStage == 1u &&
            rejected->stageWindowEndTick > rejectedTick.tickIndex &&
            qSlot.currentStage == 1u && qSlot.stageWindow > 0.f;
        if (!bRejectedPreserved)
        {
            std::printf(
                "[GameRoomProjectileIntegration][LeeQ] invalid Q2 feedback=%u/%u authStage=%u end=%llu tick=%llu localStage=%u window=%.3f\n",
                rejected ? static_cast<u32_t>(rejected->result.state) : 99u,
                rejected ? static_cast<u32_t>(rejected->result.reason) : 99u,
                rejected ? static_cast<u32_t>(rejected->authoritativeSkillStage) : 99u,
                rejected ? static_cast<unsigned long long>(rejected->stageWindowEndTick) : 0ull,
                static_cast<unsigned long long>(rejectedTick.tickIndex),
                static_cast<u32_t>(qSlot.currentStage),
                qSlot.stageWindow);
        }
        return bRejectedPreserved;
    }
}

int main()
{
    const bool_t bSkillPass = CheckSkillProjectileGameRoomLifecycle();
    const bool_t bStructurePass = CheckStructureProjectileGameRoomLifecycle();
    const bool_t bSkillGenerationPass =
        CheckSkillProjectileTargetGenerationSafety();
    const bool_t bStructureGenerationPass =
        CheckStructureProjectileTargetGenerationSafety();
    const bool_t bPassivePass = CheckPassiveExpiryBeforeFirstCommand();
    const bool_t bTurretRangePass = CheckTurretConfiguredRangeBoundary();
    const bool_t bRemoteAggroPass = CheckTurretRemoteAggroRejected();
    const bool_t bAggroExitPass = CheckTurretAggroDropsOnRangeExit();
    const bool_t bLeeSinQPass = CheckLeeSinQAuthorityLifecycle();
    const bool_t bPass = bSkillPass &&
        bStructurePass &&
        bSkillGenerationPass &&
        bStructureGenerationPass &&
        bPassivePass &&
        bTurretRangePass &&
        bRemoteAggroPass &&
        bAggroExitPass &&
        bLeeSinQPass;

    std::printf(
        "[GameRoomProjectileIntegration] %s: skill=%u structure=%u skill_generation=%u structure_generation=%u passive_pre_command=%u turret_range=%u remote_aggro=%u aggro_exit=%u leesin_q=%u\n",
        bPass ? "PASS" : "FAIL",
        static_cast<u32_t>(bSkillPass),
        static_cast<u32_t>(bStructurePass),
        static_cast<u32_t>(bSkillGenerationPass),
        static_cast<u32_t>(bStructureGenerationPass),
        static_cast<u32_t>(bPassivePass),
        static_cast<u32_t>(bTurretRangePass),
        static_cast<u32_t>(bRemoteAggroPass),
        static_cast<u32_t>(bAggroExitPass),
        static_cast<u32_t>(bLeeSinQPass));
    return bPass ? 0 : 1;
}
