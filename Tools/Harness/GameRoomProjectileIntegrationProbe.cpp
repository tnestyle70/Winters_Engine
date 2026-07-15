#include "Game/GameRoom.h"

#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "Shared/GameSim/Champions/Ezreal/EzrealGameSim.h"
#include "Shared/GameSim/Components/BuffComponent.h"
#include "Shared/GameSim/Components/EzrealSimComponent.h"
#include "Shared/GameSim/Components/NetEntityIdComponent.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Systems/Buff/BuffSystem.h"
#include "Shared/GameSim/Systems/Damage/DamageQueueSystem.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/ReplicatedEventSerializer/ReplicatedEventSerializer.h"
#include "Shared/GameSim/Systems/Stat/StatSystem.h"
#include "Shared/GameSim/Systems/Turret/TurretAISystem.h"

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

        StatComponent stat{};
        stat.championId = championId;
        stat.level = 1u;
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

        TurretAIComponent ai{};
        ai.attackDamage = 150.f;
        ai.projectileSpeed = 18.f;
        world.AddComponent<TurretAIComponent>(entity, ai);

        if (BindNetworkEntity(world, entityMap, entity) == NULL_NET_ENTITY)
            return NULL_ENTITY;
        return entity;
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

        return observerPtr->callCount == 1u &&
            observerPtr->bObservedPassiveAbsent &&
            std::fabs(observerPtr->observedBonusAttackSpeed) <= 0.0001f &&
            room->GetCurrentTickIndex() == 181u;
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
    return bPass ? 0 : 1;
}
