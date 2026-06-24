#include "Network/Client/SnapshotApplier.h"

#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include "Shared/Schemas/Generated/cpp/Hello_generated.h"
#include "Shared/Schemas/Generated/cpp/Snapshot_generated.h"
#include <flatbuffers/flatbuffers.h>
#pragma pop_macro("max")
#pragma pop_macro("min")

#include "Shared/GameSim/Replication/EntityIdMap.h"
#include "Shared/GameSim/Components/ChampionAIComponent.h"
#include "Shared/GameSim/Components/ChampionScore.h"
#include "Shared/GameSim/Components/MatchScore.h"
#include "Shared/GameSim/Components/GoldComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/InventoryComponent.h"
#include "Shared/GameSim/Components/ReplicatedActionComponent.h"
#include "Shared/GameSim/Components/ReplicatedPoseComponent.h"
#include "Shared/GameSim/Components/ReplicatedStateComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Components/RuneComponent.h"
#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/FormOverrideComponent.h"
#include "Shared/GameSim/Components/SpellbookOverrideComponent.h"
#include "Shared/GameSim/Components/KalistaSentinelComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Definitions/EffectAnchorSubtype.h"
#include "Shared/GameSim/Definitions/SnapshotStateFlags.h"
#include "Client/Private/Data/LoLVisualDefinitionPack.h"
//Viego Soul
#include "Shared/GameSim/Components/ViegoSoulComponent.h"
#include "GameObject/Champion/Kalista/KalistaFxPresets.h"
#include "GameObject/Champion/Kalista/KalistaSentinelVisualComponent.h"
#include "GameObject/Champion/Viego/Viego_FxPresets.h"

#include "GameObject/ChampionDef.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxSystem.h"
#include "GamePlay/ChampionCatalog.h"
#include "GamePlay/ChampionRegistry.h"
#include "GamePlay/SkillRegistry.h"
#include "Renderer/ModelRenderer.h"
#include "ECS/World.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/RenderComponent.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/VisionComponents.h"
#include "Manager/Jungle_Manager.h"
#include "Manager/Minion_Manager.h"
#include "Manager/Structure_Manager.h"

#include <algorithm>
#include <Windows.h>
#include <cmath>
#include <cstdio>
#include <cwchar>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>

namespace
{
    constexpr const wchar_t* kMinionMarkerTexture =
        L"Client/Bin/Resource/Texture/FX/Kalista/common_glowring_blue.png";
    constexpr const wchar_t* kStructureMarkerTexture =
        L"Client/Bin/Resource/Texture/FX/Kalista/common_global_indicator_ring_bright.png";
    constexpr const wchar_t* kMonsterMarkerTexture =
        L"Client/Bin/Resource/Texture/FX/Kalista/common_color-hit-physical.png";
    constexpr f32_t kNetworkMinionVisualScale = 0.006f;
    constexpr f32_t kNetworkMinionVisualYOffset = kNetworkMinionVisualScale * 2.f;
    constexpr bool_t kSnapshotMinionDebugOutput = false;
    constexpr bool_t kSnapshotMinionYawDebugOutput = true;
    constexpr u8_t kLocalMoveYawMaxProtectedSnapshots = 12u;
    constexpr f32_t kYawHalfTurnTolerance = 0.35f;

    bool_t ShouldShowSnapshotMarkers()
    {
        static i32_t s_result = -1;
        if (s_result >= 0)
            return s_result == 1;

        const wchar_t* cmdLine = GetCommandLineW();
        s_result = (cmdLine && wcsstr(cmdLine, L"--show-snapshot-markers")) ? 1 : 0;
        return s_result == 1;
    }

    bool_t IsYawClose(f32_t lhs, f32_t rhs, f32_t tolerance)
    {
        return std::fabs(NormalizeChampionVisualYaw(lhs - rhs)) <= tolerance;
    }

    bool_t IsYawHalfTurn(f32_t yawDelta)
    {
        return std::fabs(std::fabs(NormalizeChampionVisualYaw(yawDelta)) - WintersMath::kPi) <=
            kYawHalfTurnTolerance;
    }

    Vec3 GameplayForwardFromVisualYaw(eChampion champion, f32_t yaw)
    {
        const f32_t gameplayYaw = yaw - ClientData::ResolveChampionModelYawOffset(champion);
        return Vec3{ std::sinf(gameplayYaw), 0.f, std::cosf(gameplayYaw) };
    }

    bool_t IsCommandSeqAtLeast(u32_t lhs, u32_t rhs)
    {
        if (rhs == 0)
            return true;
        if (lhs == 0)
            return false;

        return lhs == rhs || static_cast<i32_t>(lhs - rhs) > 0;
    }

    eMinionType ToSnapshotMinionType(u16_t subtype)
    {
        if (subtype >= static_cast<u16_t>(eMinionType::End))
            return eMinionType::Melee;
        return static_cast<eMinionType>(subtype);
    }

    eMinionTeam ToSnapshotMinionTeam(u8_t team)
    {
        return team == static_cast<u8_t>(eTeam::Red)
            ? eMinionTeam::Red
            : eMinionTeam::Blue;
    }

    void SpawnSnapshotMarker(CWorld& world, EntityID entity, Shared::Schema::EntityKind kind, u8_t team)
    {
        if (!ShouldShowSnapshotMarkers())
            return;

        if (kind == Shared::Schema::EntityKind::Champion ||
            kind == Shared::Schema::EntityKind::Projectile ||
            entity == NULL_ENTITY)
        {
            return;
        }

        FxBillboardComponent fx{};
        fx.attachTo = entity;
        fx.vAttachOffset = { 0.f, 1.2f, 0.f };
        fx.fLifetime = 3600.f;
        fx.fFadeOut = 0.f;
        fx.bBillboard = true;
        fx.blendMode = eBlendPreset::AlphaBlend;

        if (kind == Shared::Schema::EntityKind::Minion)
        {
            fx.texturePath = kMinionMarkerTexture;
            fx.fWidth = 0.9f;
            fx.fHeight = 0.9f;
            fx.vColor = (team == static_cast<u8_t>(eTeam::Red))
                ? Vec4{ 1.f, 0.25f, 0.2f, 0.95f }
                : Vec4{ 0.25f, 0.55f, 1.f, 0.95f };
        }
        else if (kind == Shared::Schema::EntityKind::JungleMonster)
        {
            fx.texturePath = kMonsterMarkerTexture;
            fx.fWidth = 1.4f;
            fx.fHeight = 1.4f;
            fx.vColor = { 1.f, 0.8f, 0.25f, 0.95f };
        }
        else
        {
            fx.texturePath = kStructureMarkerTexture;
            fx.fWidth = 2.2f;
            fx.fHeight = 2.2f;
            fx.vColor = (team == static_cast<u8_t>(eTeam::Red))
                ? Vec4{ 1.f, 0.15f, 0.15f, 0.95f }
                : Vec4{ 0.15f, 0.55f, 1.f, 0.95f };
            fx.vAttachOffset = { 0.f, 2.0f, 0.f };
        }

        CFxSystem::Spawn(world, fx);
    }

    bool_t TryGetSnapshotStructureKind(
        Shared::Schema::EntityKind kind,
        Winters::Map::eObjectKind& outKind)
    {
        switch (kind)
        {
        case Shared::Schema::EntityKind::Turret:
            outKind = Winters::Map::eObjectKind::Structure_Turret;
            return true;
        case Shared::Schema::EntityKind::Inhibitor:
            outKind = Winters::Map::eObjectKind::Structure_Inhibitor;
            return true;
        case Shared::Schema::EntityKind::Nexus:
            outKind = Winters::Map::eObjectKind::Structure_Nexus;
            return true;
        default:
            return false;
        }
    }

    void MarkServerId(CWorld& world, EntityID entity, u32_t netId)
    {
        if (entity == NULL_ENTITY)
            return;

        if (!world.HasComponent<ServerIdComponent>(entity))
            world.AddComponent<ServerIdComponent>(entity, ServerIdComponent{});

        world.GetComponent<ServerIdComponent>(entity).serverEntityId = netId;
    }

    MatchScoreComponent& EnsureClientMatchScore(CWorld& world)
    {
        MatchScoreComponent* pScore = nullptr;
        world.ForEach<MatchScoreComponent>(
            [&](EntityID, MatchScoreComponent& score)
            {
                if (!pScore)
                    pScore = &score;
            }
        );
        if (pScore)
            return *pScore;

        return world.AddComponent<MatchScoreComponent>(
            world.CreateEntity(),
            MatchScoreComponent{});
    }

    void EnsureSnapshotStructureRuntimeTags(
        CWorld& world,
        EntityID entity,
        Shared::Schema::EntityKind kind,
        u8_t team,
        u16_t subtype)
    {
        if (entity == NULL_ENTITY)
            return;

        if (!world.HasComponent<StructureComponent>(entity))
            world.AddComponent<StructureComponent>(entity, StructureComponent{});

        auto& structure = world.GetComponent<StructureComponent>(entity);
        structure.team = static_cast<eTeam>(team);

        if (kind == Shared::Schema::EntityKind::Turret)
        {
            structure.kind = static_cast<u32_t>(Winters::Map::eObjectKind::Structure_Turret);
            structure.tier = subtype;
        }
        else if (kind == Shared::Schema::EntityKind::Inhibitor)
        {
            structure.kind = static_cast<u32_t>(Winters::Map::eObjectKind::Structure_Inhibitor);
        }
        else if (kind == Shared::Schema::EntityKind::Nexus)
        {
            structure.kind = static_cast<u32_t>(Winters::Map::eObjectKind::Structure_Nexus);
        }

        if (kind == Shared::Schema::EntityKind::Turret &&
            !world.HasComponent<TurretComponent>(entity))
        {
            TurretComponent turret{};
            turret.team = static_cast<eTeam>(team);
            turret.tier = static_cast<u8_t>(subtype);
            world.AddComponent<TurretComponent>(entity, turret);
        }

        if (kind == Shared::Schema::EntityKind::Inhibitor &&
            !world.HasComponent<InhibitorTag>(entity))
        {
            world.AddComponent<InhibitorTag>(entity);
        }

        if (kind == Shared::Schema::EntityKind::Nexus &&
            !world.HasComponent<NexusTag>(entity))
        {
            world.AddComponent<NexusTag>(entity);
        }

        if (!world.HasComponent<TargetableTag>(entity))
            world.AddComponent<TargetableTag>(entity);
    }

    EntityID TryBindStageStructureVisual(
        CWorld& world,
        EntityIdMap& entityMap,
        u32_t netId,
        Shared::Schema::EntityKind kind,
        u16_t subtype,
        u8_t team,
        const Vec3& vSnapshotPos)
    {
        Winters::Map::eObjectKind structureKind{};
        if (!TryGetSnapshotStructureKind(kind, structureKind))
            return NULL_ENTITY;

        const EntityID candidate = CStructure_Manager::Get()->Find_NetworkBindCandidate(
            structureKind,
            static_cast<eTeam>(team),
            subtype,
            vSnapshotPos,
            2.5f);

        if (candidate == NULL_ENTITY)
            return NULL_ENTITY;

        entityMap.Bind(netId, candidate);
        MarkServerId(world, candidate, netId);
        EnsureSnapshotStructureRuntimeTags(world, candidate, kind, team, subtype);

		return candidate;
	}

    void EnsureSnapshotJungleRuntimeTags(
        CWorld& world,
        EntityID entity,
        u16_t subtype)
    {
        if (entity == NULL_ENTITY)
            return;

        if (!world.HasComponent<JungleComponent>(entity))
            world.AddComponent<JungleComponent>(entity, JungleComponent{});

        auto& jungle = world.GetComponent<JungleComponent>(entity);
        jungle.subKind = subtype;

        if (!world.HasComponent<JungleMonsterTag>(entity))
            world.AddComponent<JungleMonsterTag>(entity);

        if (!world.HasComponent<TargetableTag>(entity))
            world.AddComponent<TargetableTag>(entity);

        if (!world.HasComponent<SpatialAgentComponent>(entity))
            world.AddComponent<SpatialAgentComponent>(entity, SpatialAgentComponent{});

        SpatialAgentComponent& spatial = world.GetComponent<SpatialAgentComponent>(entity);
        spatial.kind = eSpatialKind::JungleMob;
        spatial.team = static_cast<u8_t>(eTeam::Neutral);
        if (spatial.radius <= 0.f)
            spatial.radius = 2.f;

        if (!world.HasComponent<VisibilityComponent>(entity))
            world.AddComponent<VisibilityComponent>(entity);
    }

    void EnsureSnapshotWardRuntimeTags(
        CWorld& world,
        EntityID entity,
        u8_t team,
        u16_t subtype)
    {
        if (entity == NULL_ENTITY)
            return;

        WardComponent& ward = world.HasComponent<WardComponent>(entity)
            ? world.GetComponent<WardComponent>(entity)
            : world.AddComponent<WardComponent>(entity, WardComponent{});
        ward.ownerTeam = static_cast<eTeam>(team);
        ward.bControlWard = subtype != 0u;

        SpatialAgentComponent& spatial = world.HasComponent<SpatialAgentComponent>(entity)
            ? world.GetComponent<SpatialAgentComponent>(entity)
            : world.AddComponent<SpatialAgentComponent>(entity, SpatialAgentComponent{});
        spatial.kind = eSpatialKind::Ward;
        spatial.team = team;
        spatial.radius = 0.35f;

        VisionSourceComponent& vision = world.HasComponent<VisionSourceComponent>(entity)
            ? world.GetComponent<VisionSourceComponent>(entity)
            : world.AddComponent<VisionSourceComponent>(entity, VisionSourceComponent{});
        vision.sightRange = 10.f;

        if (!world.HasComponent<VisibilityComponent>(entity))
            world.AddComponent<VisibilityComponent>(entity);
        if (!world.HasComponent<TargetableTag>(entity))
            world.AddComponent<TargetableTag>(entity);
        if (!world.HasComponent<FxBillboardComponent>(entity))
        {
            FxBillboardComponent fx{};
            fx.attachTo = entity;
            fx.vAttachOffset = { 0.f, 0.45f, 0.f };
            fx.fLifetime = 3600.f;
            fx.fFadeOut = 0.f;
            fx.bBillboard = true;
            fx.blendMode = eBlendPreset::AlphaBlend;
            fx.texturePath = kStructureMarkerTexture;
            fx.fWidth = 0.45f;
            fx.fHeight = 0.45f;
            fx.vColor = (team == static_cast<u8_t>(eTeam::Red))
                ? Vec4{ 1.f, 0.20f, 0.16f, 0.95f }
                : Vec4{ 0.20f, 0.55f, 1.f, 0.95f };
            world.AddComponent<FxBillboardComponent>(entity, fx);
        }
    }

    EntityID TryBindStageJungleVisual(
        CWorld& world,
        EntityIdMap& entityMap,
        u32_t netId,
        Shared::Schema::EntityKind kind,
        u16_t subtype,
        const Vec3& vSnapshotPos)
    {
        if (kind != Shared::Schema::EntityKind::JungleMonster ||
            subtype > static_cast<u16_t>(CJungle_Manager::eJungleSub::KrugMini))
        {
            return NULL_ENTITY;
        }

        const auto sub = static_cast<CJungle_Manager::eJungleSub>(subtype);
        const EntityID candidate = CJungle_Manager::Get()->Find_NetworkBindCandidate(
            sub,
            vSnapshotPos,
            3.0f);

        if (candidate == NULL_ENTITY)
            return NULL_ENTITY;

        entityMap.Bind(netId, candidate);
        MarkServerId(world, candidate, netId);
        EnsureSnapshotJungleRuntimeTags(world, candidate, subtype);

        return candidate;
    }
} // namespace

std::unique_ptr<CSnapshotApplier> CSnapshotApplier::Create()
{
    return std::unique_ptr<CSnapshotApplier>(new CSnapshotApplier());
}

void CSnapshotApplier::ProtectLocalMoveYaw(u32_t netId, u32_t commandSeq, f32_t yaw)
{
    if (netId == 0)
        netId = m_localNetId;
    if (netId == 0 || commandSeq == 0)
        return;

    m_localNetId = netId;
    m_localMoveYawProtection.bActive = true;
    m_localMoveYawProtection.netId = netId;
    m_localMoveYawProtection.commandSeq = commandSeq;
    m_localMoveYawProtection.protectedSnapshotCount = 0;
    m_localMoveYawProtection.ackedProtectedSnapshotCount = 0;
    m_localMoveYawProtection.yaw = yaw;

    if (true)
    {
        char msg[192]{};
        sprintf_s(
            msg,
            "[YawTrace][SnapshotProtect] net=%u seq=%u yaw=%.4f\n",
            netId,
            commandSeq,
            yaw);
    }
}

bool_t CSnapshotApplier::GetLocalMoveYawProtectionDebug(
    u32_t& outNetId,
    u32_t& outCommandSeq,
    f32_t& outYaw) const
{
    if (!m_localMoveYawProtection.bActive)
    {
        outNetId = 0;
        outCommandSeq = 0;
        outYaw = 0.f;
        return false;
    }

    outNetId = m_localMoveYawProtection.netId;
    outCommandSeq = m_localMoveYawProtection.commandSeq;
    outYaw = m_localMoveYawProtection.yaw;
    return true;
}

void CSnapshotApplier::OnHello(
    CWorld& world,
    EntityIdMap& entityMap,
    const u8_t* payload,
    u32_t len,
    u32_t* outMyNetId,
    u32_t* outMySessionId)
{
    if (!payload || len == 0)
        return;

    flatbuffers::Verifier verifier(payload, len);
    if (!Shared::Schema::VerifyHelloBuffer(verifier))
    {
        return;
    }

    const auto* hello = Shared::Schema::GetHello(payload);
    if (!hello)
        return;

    if (outMyNetId)
        *outMyNetId = hello->yourNetId();
    if (outMySessionId)
        *outMySessionId = hello->sessionId();

    if (hello->yourNetId() != 0)
        m_localNetId = hello->yourNetId();

    EnsureEntity(
        world,
        entityMap,
        hello->yourNetId(),
        static_cast<u8_t>(Shared::Schema::EntityKind::Champion),
        hello->championId(),
        0u,
        Vec3{},
        hello->team());

    m_lastServerTick = hello->serverTick();

    if (m_onAuthoritativeSnapshot)
    {
        m_onAuthoritativeSnapshot(
            hello->serverTick(),
            hello->serverTimeMs(),
            0u,
            hello->yourNetId());
    }

    char msg[160]{};
    sprintf_s(msg,
        "[SnapshotApplier] Hello sid=%u netId=%u tick=%llu champion=%u team=%u\n",
        hello->sessionId(),
        hello->yourNetId(),
        static_cast<unsigned long long>(hello->serverTick()),
        hello->championId(),
        hello->team());
}

void CSnapshotApplier::OnSnapshot(
    CWorld& world,
    EntityIdMap& entityMap,
    const u8_t* payload,
    u32_t len)
{
    if (!payload || len == 0)
        return;

    flatbuffers::Verifier verifier(payload, len);
    if (!Shared::Schema::VerifySnapshotBuffer(verifier))
    {
        return;
    }

    const auto* snapshot = Shared::Schema::GetSnapshot(payload);
    if (!snapshot)
        return;

    m_lastServerTick = snapshot->serverTick();
    const u32_t lastAckedCommandSeq = snapshot->lastAckedCommandSeq();
    const u32_t snapshotLocalNetId = snapshot->yourNetId();
    if (m_localNetId == 0 && snapshotLocalNetId != 0)
        m_localNetId = snapshotLocalNetId;
    if (m_localNetId != 0 &&
        snapshotLocalNetId != 0 &&
        snapshotLocalNetId != m_localNetId)
    {
        static u32_t s_localNetIdMismatchLogCount = 0;
        if (s_localNetIdMismatchLogCount < 8)
        {
            char msg[160]{};
            sprintf_s(msg,
                "[SnapshotApplier] snapshot local net mismatch hello=%u snapshot=%u\n",
                m_localNetId,
                snapshotLocalNetId);
            ++s_localNetIdMismatchLogCount;
        }
    }
    const u32_t localNetId = (m_localNetId != 0)
        ? m_localNetId
        : snapshotLocalNetId;

    MatchScoreComponent& matchScore = EnsureClientMatchScore(world);
    matchScore.Blue.iTotalKills = snapshot->blueTotalKills();
    matchScore.Red.iTotalKills = snapshot->redTotalKills();
    matchScore.Blue.iDestroyedTurrets = snapshot->blueDestroyedTurrets();
    matchScore.Red.iDestroyedTurrets = snapshot->redDestroyedTurrets();
    matchScore.Blue.iDragons = snapshot->blueDragons();
    matchScore.Red.iDragons = snapshot->redDragons();
    matchScore.Blue.iBarons = snapshot->blueBarons();
    matchScore.Red.iBarons = snapshot->redBarons();

    if (m_onAuthoritativeSnapshot)
    {
        m_onAuthoritativeSnapshot(
            snapshot->serverTick(),
            snapshot->serverTimeMs(),
            lastAckedCommandSeq,
            localNetId);
    }

    const auto* entities = snapshot->entities();
    if (!entities)
        return;

    std::unordered_set<u32_t> snapshotNetIds;
    snapshotNetIds.reserve(entities->size());

    for (const auto* es : *entities)
    {
        if (!es || es->netId() == NULL_NET_ENTITY)
            continue;

        snapshotNetIds.insert(es->netId());

        const u8_t snapshotChampionId = es->championId();
        const u8_t snapshotVisualChampionId =
            es->visualChampionId() != 0u ? es->visualChampionId() : snapshotChampionId;
        const EntityID e = EnsureEntity(
            world,
            entityMap,
            es->netId(),
            static_cast<u8_t>(es->entityKind()),
            snapshotChampionId,
            es->subtype(),
            Vec3{ es->posX(), es->posY(), es->posZ() },
            es->team());
        if (e == NULL_ENTITY)
            continue;

        if (world.HasComponent<TransformComponent>(e))
        {
            auto& tf = world.GetComponent<TransformComponent>(e);
            Vec3 snapshotPos{ es->posX(), es->posY(), es->posZ() };
            if (es->entityKind() == Shared::Schema::EntityKind::Minion)
                snapshotPos.y += kNetworkMinionVisualYOffset;
            tf.SetPosition(snapshotPos);
            const Vec3 rot = tf.GetRotation();
            const bool_t bLocalChampion =
                es->entityKind() == Shared::Schema::EntityKind::Champion &&
                es->netId() == localNetId;
            const bool_t bServerActionLocked =
                (es->stateFlags() &
                    (kSnapshotStateDeadFlag | kSnapshotStateAttackFlag)) != 0u;
            const f32_t serverYaw = NormalizeChampionVisualYaw(es->yaw());
            const f32_t protectedYaw =
                NormalizeChampionVisualYaw(m_localMoveYawProtection.yaw);
            const bool_t bSnapshotCoversProtectedCommand =
                IsCommandSeqAtLeast(
                    lastAckedCommandSeq,
                    m_localMoveYawProtection.commandSeq);
            const bool_t bServerCaughtProtectedYaw =
                IsYawClose(serverYaw, protectedYaw, 0.20f);
            const f32_t serverVsProtectedDelta =
                NormalizeChampionVisualYaw(serverYaw - protectedYaw);
            const bool_t bServerOpposesProtectedYaw =
                m_localMoveYawProtection.bActive &&
                IsYawHalfTurn(serverVsProtectedDelta);
            const bool_t bProtectedAckGraceExpired =
                bSnapshotCoversProtectedCommand &&
                !bServerOpposesProtectedYaw &&
                m_localMoveYawProtection.ackedProtectedSnapshotCount >=
                    kLocalMoveYawMaxProtectedSnapshots;
            const bool_t bUseProtectedYaw =
                m_localMoveYawProtection.bActive &&
                bLocalChampion &&
                es->netId() == m_localMoveYawProtection.netId &&
                localNetId == m_localMoveYawProtection.netId &&
                !bServerActionLocked &&
                !bServerCaughtProtectedYaw &&
                !bProtectedAckGraceExpired;
            const f32_t sourceYaw = bUseProtectedYaw ? protectedYaw : serverYaw;
            const f32_t resolvedYaw = MakeChampionVisualYawNear(sourceYaw, rot.y);
            tf.SetRotation(Vec3{
                rot.x,
                resolvedYaw,
                rot.z
                });

#if defined(_DEBUG)
            if constexpr (kSnapshotMinionYawDebugOutput)
            {
                static u32_t s_minionYawTraceCount = 0;
                if (es->entityKind() == Shared::Schema::EntityKind::Minion &&
                    s_minionYawTraceCount < 512u)
                {
                    const f32_t yawDelta =
                        NormalizeChampionVisualYaw(resolvedYaw - rot.y);
                    char msg[512]{};
                    sprintf_s(
                        msg,
                        "[MinionYaw][SnapshotApply] tick=%llu net=%u entity=%u prevYaw=%.4f wireYaw=%.4f appliedYaw=%.4f yawDelta=%.4f pos=(%.3f,%.3f,%.3f)\n",
                        static_cast<unsigned long long>(m_lastServerTick),
                        es->netId(),
                        static_cast<u32_t>(e),
                        rot.y,
                        es->yaw(),
                        resolvedYaw,
                        yawDelta,
                        snapshotPos.x,
                        snapshotPos.y,
                        snapshotPos.z);
                    ++s_minionYawTraceCount;
                }
            }
#endif

            if (bLocalChampion)
            {
                static u32_t s_localYawSnapshotTraceCount = 0;
                const f32_t yawDelta = NormalizeChampionVisualYaw(resolvedYaw - rot.y);
                const f32_t serverYawDelta = NormalizeChampionVisualYaw(serverYaw - rot.y);
                const f32_t protectedYawDelta = NormalizeChampionVisualYaw(protectedYaw - rot.y);
                const eChampion championId = static_cast<eChampion>(es->championId());
                const f32_t visualYawOffset = ClientData::ResolveChampionModelYawOffset(championId);
                const Vec3 serverForward =
                    GameplayForwardFromVisualYaw(championId, serverYaw);
                const Vec3 appliedForward =
                    GameplayForwardFromVisualYaw(championId, resolvedYaw);
                const bool_t bAppliedHalfTurn = IsYawHalfTurn(yawDelta);
                if (s_localYawSnapshotTraceCount < 2048u)
                {
                    char msg[1536]{};
                    sprintf_s(
                        msg,
                        "[YawTrace][SnapshotApply] tick=%llu ack=%u net=%u entity=%u champion=%u source=%s protect=%u useProtect=%u prevYaw=%.4f rawWireYaw=%.4f serverYaw=%.4f sourceYaw=%.4f appliedYaw=%.4f yawDelta=%.4f serverDelta=%.4f protectedDelta=%.4f serverVsProtected=%.4f halfTurn=%u serverOpposesProtected=%u offset=%.4f serverF=(%.3f,%.3f) appliedF=(%.3f,%.3f) protectedYaw=%.4f protectedSeq=%u protectedFrames=%u ackedProtectedFrames=%u ackCoversProtected=%u actionLocked=%u caught=%u state=0x%08X pose=%u action=%u actionSeq=%u pos=(%.3f,%.3f,%.3f)\n",
                        static_cast<unsigned long long>(m_lastServerTick),
                        lastAckedCommandSeq,
                        es->netId(),
                        static_cast<u32_t>(e),
                        static_cast<u32_t>(es->championId()),
                        bUseProtectedYaw ? "protected" : "server",
                        m_localMoveYawProtection.bActive ? 1u : 0u,
                        bUseProtectedYaw ? 1u : 0u,
                        rot.y,
                        es->yaw(),
                        serverYaw,
                        sourceYaw,
                        resolvedYaw,
                        yawDelta,
                        serverYawDelta,
                        protectedYawDelta,
                        serverVsProtectedDelta,
                        bAppliedHalfTurn ? 1u : 0u,
                        bServerOpposesProtectedYaw ? 1u : 0u,
                        visualYawOffset,
                        serverForward.x,
                        serverForward.z,
                        appliedForward.x,
                        appliedForward.z,
                        protectedYaw,
                        m_localMoveYawProtection.commandSeq,
                        static_cast<u32_t>(m_localMoveYawProtection.protectedSnapshotCount),
                        static_cast<u32_t>(m_localMoveYawProtection.ackedProtectedSnapshotCount),
                        bSnapshotCoversProtectedCommand ? 1u : 0u,
                        bServerActionLocked ? 1u : 0u,
                        bServerCaughtProtectedYaw ? 1u : 0u,
                        es->stateFlags(),
                        static_cast<u32_t>(es->poseId()),
                        static_cast<u32_t>(es->actionId()),
                        es->actionSeq(),
                        snapshotPos.x,
                        snapshotPos.y,
                        snapshotPos.z);
                    ++s_localYawSnapshotTraceCount;
                }
            }

            if (bLocalChampion &&
                m_localMoveYawProtection.bActive &&
                !bServerActionLocked &&
                bUseProtectedYaw)
            {
                ++m_localMoveYawProtection.protectedSnapshotCount;
                if (bSnapshotCoversProtectedCommand &&
                    m_localMoveYawProtection.ackedProtectedSnapshotCount <
                        kLocalMoveYawMaxProtectedSnapshots)
                {
                    ++m_localMoveYawProtection.ackedProtectedSnapshotCount;
                }
            }

            if (bLocalChampion &&
                m_localMoveYawProtection.bActive &&
                (bServerActionLocked ||
                    bServerCaughtProtectedYaw ||
                    bProtectedAckGraceExpired))
            {
                if (true)
                {
                    char msg[256]{};
                    sprintf_s(
                        msg,
                        "[YawTrace][SnapshotProtectClear] tick=%llu net=%u seq=%u actionLocked=%u caught=%u protectedFrames=%u ackedProtectedFrames=%u\n",
                        static_cast<unsigned long long>(m_lastServerTick),
                        es->netId(),
                        m_localMoveYawProtection.commandSeq,
                        bServerActionLocked ? 1u : 0u,
                        bServerCaughtProtectedYaw ? 1u : 0u,
                        static_cast<u32_t>(m_localMoveYawProtection.protectedSnapshotCount),
                        static_cast<u32_t>(m_localMoveYawProtection.ackedProtectedSnapshotCount));
                }
                m_localMoveYawProtection = {};
            }
        }

        auto& pose = world.HasComponent<ReplicatedPoseComponent>(e)
            ? world.GetComponent<ReplicatedPoseComponent>(e)
            : world.AddComponent<ReplicatedPoseComponent>(e, ReplicatedPoseComponent{});
        pose.poseId = es->poseId();
        pose.startTick = es->poseStartTick();
        auto& action = world.HasComponent<ReplicatedActionComponent>(e)
            ? world.GetComponent<ReplicatedActionComponent>(e)
            : world.AddComponent<ReplicatedActionComponent>(e, ReplicatedActionComponent{});
        action.actionId = es->actionId();
        action.startTick = es->actionStartTick();
        action.sequence = es->actionSeq();
        action.stage = es->actionStage() == 0u ? 1u : es->actionStage();

        if (!world.HasComponent<ReplicatedStateComponent>(e))
            world.AddComponent<ReplicatedStateComponent>(e, ReplicatedStateComponent{});

        auto& replicatedState = world.GetComponent<ReplicatedStateComponent>(e);
        replicatedState.stateFlags = es->stateFlags();
        replicatedState.serverTick = m_lastServerTick;

        if (world.HasComponent<HealthComponent>(e))
        {
            auto& hp = world.GetComponent<HealthComponent>(e);
            hp.fCurrent = es->hp();
            if (es->maxHp() > 0.f)
                hp.fMaximum = es->maxHp();
            hp.bIsDead = (hp.fCurrent <= 0.f);
        }

        const auto kind = es->entityKind();

        if (kind == Shared::Schema::EntityKind::Champion &&
            world.HasComponent<ChampionComponent>(e))
        {
            auto& champ = world.GetComponent<ChampionComponent>(e);
            champ.id = static_cast<eChampion>(snapshotChampionId);
            champ.team = static_cast<eTeam>(es->team());
            champ.hp = es->hp();
            if (es->maxHp() > 0.f)
                champ.maxHp = es->maxHp();
            champ.mana = es->mana();
            if (es->maxMana() > 0.f)
                champ.maxMana = es->maxMana();
            champ.shield = es->shield();
            if (champ.id == eChampion::YASUO &&
                world.HasComponent<YasuoStateComponent>(e))
            {
                auto& yasuoState = world.GetComponent<YasuoStateComponent>(e);
                yasuoState.fPassiveFlow = champ.mana;
                yasuoState.fPassiveFlowMax = champ.maxMana;
                yasuoState.fPassiveShieldRemaining = champ.shield;
                yasuoState.fPassiveShieldMax = (champ.maxMana > 0.f) ? champ.maxMana : 100.f;
            }
            champ.moveSpeed = es->moveSpeed();
            champ.level = es->level();
            if (const auto* pCooldowns = es->skillCooldowns())
            {
                for (u32_t i = 0; i < 4; ++i)
                {
                    const u32_t sourceIndex = (pCooldowns->size() > i + 1) ? i + 1 : i;
                    if (sourceIndex < pCooldowns->size())
                        champ.cooldowns[i] = pCooldowns->Get(sourceIndex);
                }
            }
            if (!world.HasComponent<ExperienceComponent>(e))
                world.AddComponent<ExperienceComponent>(e, ExperienceComponent{});

            auto& xp = world.GetComponent<ExperienceComponent>(e);
            xp.level = es->level();
            xp.current = es->xpCurrent();
            xp.requiredForNextLevel = es->xpRequired();
        }

        if (kind == Shared::Schema::EntityKind::Champion)
        {
            const eChampion baseChampion = static_cast<eChampion>(snapshotChampionId);
            const eChampion visualChampion =
                static_cast<eChampion>(snapshotVisualChampionId);
            const eChampion skillChampion =
                es->skillChampionId() != 0u
                    ? static_cast<eChampion>(es->skillChampionId())
                    : baseChampion;

            eChampion previousVisual = baseChampion;
            if (world.HasComponent<FormOverrideComponent>(e))
            {
                const auto& previousForm = world.GetComponent<FormOverrideComponent>(e);
                if (previousForm.bActive &&
                    previousForm.visualChampion != eChampion::END &&
                    previousForm.visualChampion != eChampion::NONE)
                {
                    previousVisual = previousForm.visualChampion;
                }
            }

            if (snapshotVisualChampionId != snapshotChampionId ||
                es->skillSlotMask() != 0u)
            {
                auto& form = world.HasComponent<FormOverrideComponent>(e)
                    ? world.GetComponent<FormOverrideComponent>(e)
                    : world.AddComponent<FormOverrideComponent>(e, FormOverrideComponent{});
                form.baseChampion = baseChampion;
                form.visualChampion = visualChampion;
                form.skillChampion = skillChampion;
                form.skillSlotMask = es->skillSlotMask();
                form.fRemainingSec = 1.f;
                form.bActive = true;
            }
            else if (world.HasComponent<FormOverrideComponent>(e))
            {
                world.RemoveComponent<FormOverrideComponent>(e);
            }

            if (visualChampion != previousVisual && m_onChampionVisualChanged)
                m_onChampionVisualChanged(e, static_cast<u8_t>(visualChampion), es->team());

            if (es->spellbookChampionId() != 0u)
            {
                auto& spellbook = world.HasComponent<SpellbookOverrideComponent>(e)
                    ? world.GetComponent<SpellbookOverrideComponent>(e)
                    : world.AddComponent<SpellbookOverrideComponent>(e, SpellbookOverrideComponent{});
                spellbook.sourceChampion = static_cast<eChampion>(es->spellbookChampionId());
                spellbook.sourceSlot = es->spellbookSlot();
                spellbook.localSlot = static_cast<u8_t>(eSkillSlot::R);
                spellbook.fRemainingSec = es->spellbookRemaining();
                spellbook.bActive = true;
            }
            else if (world.HasComponent<SpellbookOverrideComponent>(e))
            {
                world.RemoveComponent<SpellbookOverrideComponent>(e);
            }
        }

        if (kind == Shared::Schema::EntityKind::JungleMonster &&
            world.HasComponent<JungleComponent>(e))
        {
            auto& jungle = world.GetComponent<JungleComponent>(e);
            jungle.subKind = es->subtype();
            jungle.hp = es->hp();
            if (es->maxHp() > 0.f)
                jungle.maxHp = es->maxHp();
        }

        const bool_t bViegoSoul =
            (es->stateFlags() & kSnapshotStateViegoSoulFlag) != 0u;
        if (bViegoSoul)
        {
            const bool_t bNewSoul = !world.HasComponent<ViegoSoulComponent>(e);
            if (!world.HasComponent<ViegoSoulComponent>(e))
                world.AddComponent<ViegoSoulComponent>(e, ViegoSoulComponent{});

            auto& soul = world.GetComponent<ViegoSoulComponent>(e);
            soul.champion = static_cast<eChampion>(
                snapshotVisualChampionId != 0u ? snapshotVisualChampionId : es->subtype());
            soul.eligibleTeam = static_cast<eTeam>(
                es->team() == static_cast<u8_t>(eTeam::Blue)
                ? eTeam::Red
                : eTeam::Blue);
            soul.fRemainingSec = 5.f;

            const bool_t bNeedsChampionVisual =
                !world.HasComponent<ChampionComponent>(e) ||
                world.GetComponent<ChampionComponent>(e).id != soul.champion ||
                !world.HasComponent<RenderComponent>(e);

            ChampionComponent& soulChampion = world.HasComponent<ChampionComponent>(e)
                ? world.GetComponent<ChampionComponent>(e)
                : world.AddComponent<ChampionComponent>(e, ChampionComponent{});
            soulChampion.id = soul.champion;
            soulChampion.team = static_cast<eTeam>(es->team());
            soulChampion.hp = 1.f;
            soulChampion.maxHp = 1.f;

            SpatialAgentComponent& soulAgent = world.HasComponent<SpatialAgentComponent>(e)
                ? world.GetComponent<SpatialAgentComponent>(e)
                : world.AddComponent<SpatialAgentComponent>(e, SpatialAgentComponent{});
            soulAgent.kind = eSpatialKind::Champion;
            soulAgent.team = es->team();
            soulAgent.radius = 0.85f;

            if (!world.HasComponent<TargetableTag>(e))
                world.AddComponent<TargetableTag>(e);

            if (bNeedsChampionVisual && m_onChampionVisualChanged)
                m_onChampionVisualChanged(e, static_cast<u8_t>(soul.champion), es->team());

            if (world.HasComponent<RenderComponent>(e))
            {
                auto& soulRender = world.GetComponent<RenderComponent>(e);
                if (soulRender.pRenderer)
                {
                    soulRender.pRenderer->SetMaterialOverrideColor(
                        Vec4{ 0.20f, 1.05f, 0.72f, 0.80f },
                        true);
                }
            }

            if (bNewSoul)
                Viego::Fx::SpawnSoulIdle(world, e, 5.f);
        }

        const bool_t bKalistaSentinel =
            kind == Shared::Schema::EntityKind::EffectAnchor &&
            snapshotChampionId == static_cast<u8_t>(eChampion::KALISTA) &&
            es->subtype() == EffectAnchorSubtype::KalistaWSentinel;
        if (bKalistaSentinel)
        {
            const bool_t bNewSentinel = !world.HasComponent<KalistaSentinelComponent>(e);
            if (!world.HasComponent<KalistaSentinelComponent>(e))
                world.AddComponent<KalistaSentinelComponent>(e, KalistaSentinelComponent{});

            const f32_t yaw = es->yaw();
            const Vec3 forward{ std::sinf(yaw), 0.f, std::cosf(yaw) };
            auto& sentinel = world.GetComponent<KalistaSentinelComponent>(e);
            sentinel.team = static_cast<eTeam>(es->team());
            sentinel.forward = forward;
            sentinel.lifetimeSec = 12.f;
            sentinel.sightRange = 10.f;
            sentinel.halfAngleCos = 0.8660254f;

            SpatialAgentComponent& agent = world.HasComponent<SpatialAgentComponent>(e)
                ? world.GetComponent<SpatialAgentComponent>(e)
                : world.AddComponent<SpatialAgentComponent>(e, SpatialAgentComponent{});
            agent.kind = eSpatialKind::Ward;
            agent.team = es->team();
            agent.radius = 0.45f;

            VisionSourceComponent& vision = world.HasComponent<VisionSourceComponent>(e)
                ? world.GetComponent<VisionSourceComponent>(e)
                : world.AddComponent<VisionSourceComponent>(e, VisionSourceComponent{});
            vision.sightRange = sentinel.sightRange;

            VisionConeComponent& cone = world.HasComponent<VisionConeComponent>(e)
                ? world.GetComponent<VisionConeComponent>(e)
                : world.AddComponent<VisionConeComponent>(e, VisionConeComponent{});
            cone.forward = forward;
            cone.halfAngleCos = sentinel.halfAngleCos;

            if (!world.HasComponent<VisibilityComponent>(e))
                world.AddComponent<VisibilityComponent>(e, VisibilityComponent{});

            if (bNewSentinel || !world.HasComponent<KalistaSentinelVisualComponent>(e))
            {
                EntityID avatarFx = NULL_ENTITY;
                EntityID coneFx = NULL_ENTITY;
                KalistaFx::SpawnWSentinelIdle(world, e, forward, 12.f, &avatarFx, &coneFx);

                KalistaSentinelVisualComponent visual{};
                visual.avatarFx = avatarFx;
                visual.coneFx = coneFx;
                if (world.HasComponent<KalistaSentinelVisualComponent>(e))
                    world.GetComponent<KalistaSentinelVisualComponent>(e) = visual;
                else
                    world.AddComponent<KalistaSentinelVisualComponent>(e, visual);
            }

            if (world.HasComponent<KalistaSentinelVisualComponent>(e))
            {
                const auto& visual = world.GetComponent<KalistaSentinelVisualComponent>(e);
                if (visual.coneFx != NULL_ENTITY &&
                    world.IsAlive(visual.coneFx) &&
                    world.HasComponent<FxBillboardComponent>(visual.coneFx))
                {
                    auto& coneFx = world.GetComponent<FxBillboardComponent>(visual.coneFx);
                    coneFx.vAttachOffset = {
                        forward.x * 4.2f,
                        0.055f,
                        forward.z * 4.2f
                    };
                    coneFx.fYaw = std::atan2f(forward.x, forward.z);
                }
            }
        }

        if (kind == Shared::Schema::EntityKind::Champion)
        {
            const bool_t bHasAIDebug = (es->stateFlags() & kChampionAIDebugPresentFlag) != 0u;
            if (bHasAIDebug)
            {
                if (!world.HasComponent<ChampionAIDebugComponent>(e))
                    world.AddComponent<ChampionAIDebugComponent>(e, ChampionAIDebugComponent{});

                auto& debug = world.GetComponent<ChampionAIDebugComponent>(e);
                debug.bPresent = true;
                debug.netId = es->netId();
                debug.state = static_cast<eChampionAIState>(
                    (es->stateFlags() & kChampionAIStateMask) >> kChampionAIStateShift);
                debug.action = static_cast<eChampionAIAction>(
                    (es->stateFlags() & kChampionAIActionMask) >> kChampionAIActionShift);
                debug.intent = static_cast<eChampionAIIntent>(
                    (es->stateFlags() & kChampionAIIntentMask) >> kChampionAIIntentShift);
                const u32_t legacyActionMask =
                    (es->stateFlags() & kChampionAIAvailableActionMask) >> kChampionAIAvailableActionShift;
                const u32_t legacySkillMask =
                    (es->stateFlags() & kChampionAIAvailableSkillMask) >> kChampionAIAvailableSkillShift;
                debug.targetNetId = es->aiDebugTargetNet() != NULL_NET_ENTITY
                    ? es->aiDebugTargetNet()
                    : es->ownerNet();
                debug.availableActionMask = es->aiDebugAvailableActionMask() != 0u
                    ? es->aiDebugAvailableActionMask()
                    : legacyActionMask;
                debug.availableSkillMask = es->aiDebugAvailableSkillMask() != 0u
                    ? es->aiDebugAvailableSkillMask()
                    : legacySkillMask;
                debug.bOverridePending = (es->stateFlags() & kChampionAIDebugOverrideFlag) != 0u;
                debug.lowHpEnemyNetId = es->aiDebugLowHpEnemyNet();
                debug.diveTargetNetId = es->aiDebugDiveTargetNet();
                debug.lastCommandTargetNetId = es->aiDebugLastCommandTargetNet();
                debug.lastCommandKind = es->aiDebugLastCommandKind();
                debug.lastCommandSlot = es->aiDebugLastCommandSlot();
                debug.divePhase = static_cast<eChampionAIDivePhase>(es->aiDebugDivePhase());
                debug.lastBlockReason =
                    static_cast<eChampionAIDecisionBlockReason>(es->aiDebugLastBlockReason());
                debug.bCanAttackChampion =
                    (es->aiDebugFlags() & kChampionAIDebugCanAttackChampionFlag) != 0u;
                debug.bPostComboBAAllowed =
                    (es->aiDebugFlags() & kChampionAIDebugPostComboBAAllowedFlag) != 0u;
                debug.fChampionDecisionScore = es->aiDebugChampionScore();
                debug.fFarmDecisionScore = es->aiDebugFarmScore();
                debug.fStructureDecisionScore = es->aiDebugStructureScore();
                debug.fSelfHpRatio = es->aiDebugSelfHpRatio();
                debug.fEnemyHpRatio = es->aiDebugEnemyHpRatio();
                debug.fEnemyDistance = es->aiDebugEnemyDistance();
                debug.fAttackRange = es->aiDebugAttackRange();
                debug.fTurretDanger = es->aiDebugTurretDanger();
                debug.fLowHpEnemyRatio = es->aiDebugLowHpEnemyRatio();
                debug.fLowHpEnemyDistance = es->aiDebugLowHpEnemyDistance();
                debug.fChampionScanRange = es->aiDebugChampionScanRange();
                debug.fMinionScanRange = es->aiDebugMinionScanRange();
                debug.fStructureScanRange = es->aiDebugStructureScanRange();
                debug.fLeashRange = es->aiDebugLeashRange();
                debug.fRetreatHpRatio = es->aiDebugRetreatHpRatio();
                debug.fReengageHpRatio = es->aiDebugReengageHpRatio();
                debug.fChampionScoreMargin = es->aiDebugChampionScoreMargin();
                debug.fTurretDangerThreshold = es->aiDebugTurretDangerThreshold();
                debug.fPostComboBASelfHpMinRatio = es->aiDebugPostComboBASelfHpMinRatio();
                debug.fPostComboBAEnemyHpMargin = es->aiDebugPostComboBAEnemyHpMargin();
                debug.fPostComboBAWindow = es->aiDebugPostComboBAWindow();
                debug.fLowHpExecuteThreshold = es->aiDebugLowHpExecuteThreshold();
                debug.fDiveScanRange = es->aiDebugDiveScanRange();
                debug.fDiveExtraBAWindow = es->aiDebugDiveExtraBAWindow();
                debug.fFlashRange = es->aiDebugFlashRange();
                debug.fPostComboBATimer = es->aiDebugPostComboBATimer();
                debug.moveSpeed = es->moveSpeed();
                debug.snapshotPos = Vec3{ es->posX(), es->posY(), es->posZ() };
                debug.lastCommandPos = Vec3{
                    es->aiDebugLastCommandPosX(),
                    es->aiDebugLastCommandPosY(),
                    es->aiDebugLastCommandPosZ()
                };
                debug.debugDecisionTraceCount = 0u;
                if (const auto* pTrace = es->aiDebugTrace())
                {
                    const u32_t count = std::min<u32_t>(
                        pTrace->size(),
                        kChampionAIDebugTraceCapacity);
                    debug.debugDecisionTraceCount = static_cast<u8_t>(count);
                    for (u32_t i = 0u; i < count; ++i)
                    {
                        const auto* pRow = pTrace->Get(i);
                        ChampionAIDecisionTraceEntry& dst = debug.debugDecisionTrace[i];
                        dst.tick = pRow->tick();
                        dst.state = static_cast<eChampionAIState>(pRow->state());
                        dst.intent = static_cast<eChampionAIIntent>(pRow->intent());
                        dst.action = static_cast<eChampionAIAction>(pRow->action());
                        dst.divePhase = static_cast<eChampionAIDivePhase>(pRow->divePhase());
                        dst.blockReason =
                            static_cast<eChampionAIDecisionBlockReason>(pRow->blockReason());
                        dst.commandKind = pRow->commandKind();
                        dst.commandSlot = pRow->commandSlot();
                        dst.target = static_cast<EntityID>(pRow->targetNet());
                        dst.commandPos = Vec3{
                            pRow->commandPosX(),
                            pRow->commandPosY(),
                            pRow->commandPosZ()
                        };
                        dst.championScore = pRow->championScore();
                        dst.farmScore = pRow->farmScore();
                        dst.structureScore = pRow->structureScore();
                        dst.selfHpRatio = pRow->selfHpRatio();
                        dst.enemyHpRatio = pRow->enemyHpRatio();
                        dst.enemyDistance = pRow->enemyDistance();
                        dst.turretDanger = pRow->turretDanger();
                    }
                }
            }
            else if (world.HasComponent<ChampionAIDebugComponent>(e))
            {
                world.GetComponent<ChampionAIDebugComponent>(e).bPresent = false;
            }

            if (const auto* pRanks = es->skillRanks())
            {
                if (!world.HasComponent<SkillRankComponent>(e))
                    world.AddComponent<SkillRankComponent>(e, SkillRankComponent{});

                auto& rank = world.GetComponent<SkillRankComponent>(e);
                const u32_t count = std::min<u32_t>(pRanks->size(), SkillRankComponent::kSlotCount);
                for (u32_t i = 0; i < count; ++i)
                {
                    rank.ranks[i] = pRanks->Get(i);
                }
                rank.pointsAvailable = es->skillPoints();
            }

            if (const auto* pCooldowns = es->skillCooldowns())
            {
                if (!world.HasComponent<SkillStateComponent>(e))
                    world.AddComponent<SkillStateComponent>(e, SkillStateComponent{});

                const auto* pCooldownDurations = es->skillCooldownDurations();
                auto& skillState = world.GetComponent<SkillStateComponent>(e);
                const u32_t count = std::min<u32_t>(pCooldowns->size(), 5u);
                const u32_t durationCount = pCooldownDurations
                    ? std::min<u32_t>(pCooldownDurations->size(), 5u)
                    : 0u;
                for (u32_t i = 0; i < count; ++i)
                {
                    auto& slot = skillState.slots[i];
                    slot.cooldownRemaining = pCooldowns->Get(i);
                    if (i < durationCount)
                        slot.cooldownDuration = pCooldownDurations->Get(i);
                    else if (slot.cooldownDuration < slot.cooldownRemaining)
                        slot.cooldownDuration = slot.cooldownRemaining;
                    if (slot.cooldownRemaining <= 0.f)
                        slot.cooldownDuration = 0.f;
                }
            }
        }

        if (kind == Shared::Schema::EntityKind::Minion)
        {
            CMinion_Manager::Get()->QueueNetworkVisual(
                e,
                ToSnapshotMinionType(es->subtype()),
                ToSnapshotMinionTeam(es->team()));

            if (world.HasComponent<MinionStateComponent>(e))
            {
                auto& ms = world.GetComponent<MinionStateComponent>(e);
                ms.team = static_cast<eTeam>(es->team());
                ms.type = static_cast<u8_t>(es->subtype());
                ms.moveSpeed = es->moveSpeed();
                if ((es->stateFlags() & kSnapshotStateDeadFlag) != 0u)
                {
                    ms.current = MinionStateComponent::Dead;
                    ms.bAttackAnimRequested = false;
                    if (ms.deathTimer <= 0.f)
                        ms.deathTimer = 1.2f;
                }
                else
                {
                    const bool_t bServerAttack =
                        ((es->stateFlags() & kSnapshotStateAttackFlag) != 0u) ||
                        es->actionId() == static_cast<u16_t>(
                            eReplicatedActionId::BasicAttack);
                    if (bServerAttack)
                    {
                        ms.current = MinionStateComponent::Attack;
                    }
                    else if (es->poseId() == static_cast<u16_t>(eReplicatedPoseId::Run))
                    {
                        ms.current = MinionStateComponent::LaneMove;
                    }
                    else if (es->poseId() == static_cast<u16_t>(eReplicatedPoseId::Idle))
                    {
                        ms.current = MinionStateComponent::Idle;
                    }

                    ms.bAttackAnimRequested = false;
                }
            }
            if (world.HasComponent<MinionComponent>(e))
            {
                auto& minion = world.GetComponent<MinionComponent>(e);
                minion.team = static_cast<eTeam>(es->team());
                minion.roleType = static_cast<u8_t>(es->subtype());
                minion.hp = es->hp();
                if (es->maxHp() > 0.f)
                    minion.maxHp = es->maxHp();
            }
        }

        if ((kind == Shared::Schema::EntityKind::Turret ||
             kind == Shared::Schema::EntityKind::Inhibitor ||
             kind == Shared::Schema::EntityKind::Nexus) &&
            world.HasComponent<StructureComponent>(e))
        {
            EnsureSnapshotStructureRuntimeTags(world, e, kind, es->team(), es->subtype());

            auto& structure = world.GetComponent<StructureComponent>(e);
            structure.team = static_cast<eTeam>(es->team());
            structure.hp = es->hp();
            if (es->maxHp() > 0.f)
                structure.maxHp = es->maxHp();

            if (kind == Shared::Schema::EntityKind::Turret &&
                world.HasComponent<TurretComponent>(e))
            {
                auto& turret = world.GetComponent<TurretComponent>(e);
                turret.team = static_cast<eTeam>(es->team());
                turret.hp = es->hp();
                if (es->maxHp() > 0.f)
                    turret.maxHp = es->maxHp();
                turret.tier = static_cast<u8_t>(es->subtype());
                turret.targetId = es->ownerNet() != NULL_NET_ENTITY
                    ? entityMap.FromNet(es->ownerNet())
                    : NULL_ENTITY;
            }
        }

        if (world.HasComponent<StatComponent>(e) &&
            kind == Shared::Schema::EntityKind::Champion)
        {
            auto& stat = world.GetComponent<StatComponent>(e);
            stat.championId = static_cast<eChampion>(es->championId());
            stat.level = es->level();
            stat.moveSpeed = es->moveSpeed();

            stat.ad = es->ad();
            stat.ap = es->ap();
            stat.armor = es->armor();
            stat.mr = es->mr();
            stat.attackSpeed = es->attackSpeed();
            stat.attackRange = es->attackRange();
            stat.critChance = es->critChance();
            stat.abilityHaste = es->abilityHaste();

            stat.manaMax = es->mana();
            if (es->maxMana() > 0.f)
                stat.manaMax = es->maxMana();
            if (es->maxHp() > 0.f)
                stat.hpMax = es->maxHp();
        }

        if (kind == Shared::Schema::EntityKind::Champion)
        {
            if (!world.HasComponent<RuneRuntimeComponent>(e))
                world.AddComponent<RuneRuntimeComponent>(e, RuneRuntimeComponent{});
            world.GetComponent<RuneRuntimeComponent>(e).iLethalTempoStacks =
                es->lethalTempoStacks();

            if (!world.HasComponent<GoldComponent>(e))
                world.AddComponent<GoldComponent>(e, GoldComponent{});
            world.GetComponent<GoldComponent>(e).amount = es->gold();

            InventoryComponent inventory{};
            if (const auto* pItems = es->inventoryItemIds())
            {
                const u32_t count = std::min<u32_t>(
                    pItems->size(),
                    InventoryComponent::kMaxSlots);
                inventory.count = static_cast<u8_t>(count);
                for (u32_t i = 0; i < count; ++i)
                    inventory.itemIds[i] = pItems->Get(i);
            }

            if (world.HasComponent<InventoryComponent>(e))
                world.GetComponent<InventoryComponent>(e) = inventory;
            else
                world.AddComponent<InventoryComponent>(e, inventory);

            ChampionScoreComponent score{};
            score.iKills = es->kills();
            score.iDeaths = es->deaths();
            score.iAssists = es->assists();
            if (const auto* pSummonerSpells = es->summonerSpellIds())
            {
                const u32_t count = std::min<u32_t>(
                    pSummonerSpells->size(),
                    ChampionScoreComponent::kSummonerSpellSlotCount);
                for (u32_t i = 0; i < count; ++i)
                    score.iSummonerSpellIds[i] = pSummonerSpells->Get(i);
            }

            if (world.HasComponent<ChampionScoreComponent>(e))
                world.GetComponent<ChampionScoreComponent>(e) = score;
            else
                world.AddComponent<ChampionScoreComponent>(e, score);

            if (const auto* pSummonerCooldowns = es->summonerSpellCooldowns())
            {
                if (!world.HasComponent<SummonerSpellStateComponent>(e))
                    world.AddComponent<SummonerSpellStateComponent>(e, SummonerSpellStateComponent{});

                const auto* pSummonerDurations = es->summonerSpellCooldownDurations();
                auto& spells = world.GetComponent<SummonerSpellStateComponent>(e);
                const u32_t count = std::min<u32_t>(
                    pSummonerCooldowns->size(),
                    SummonerSpellStateComponent::kSlotCount);
                const u32_t durationCount = pSummonerDurations
                    ? std::min<u32_t>(
                        pSummonerDurations->size(),
                        SummonerSpellStateComponent::kSlotCount)
                    : 0u;
                for (u32_t i = 0; i < count; ++i)
                {
                    spells.cooldownRemaining[i] = pSummonerCooldowns->Get(i);
                    if (i < durationCount)
                        spells.cooldownDuration[i] = pSummonerDurations->Get(i);
                    else if (spells.cooldownDuration[i] < spells.cooldownRemaining[i])
                        spells.cooldownDuration[i] = spells.cooldownRemaining[i];
                    if (spells.cooldownRemaining[i] <= 0.f)
                        spells.cooldownDuration[i] = 0.f;
                }
            }
        }
    }

    std::vector<u32_t> staleNetIds;
    staleNetIds.reserve(16u);

    for (u32_t netId : m_seenNetIds)
    {
        if (snapshotNetIds.find(netId) != snapshotNetIds.end())
            continue;

        const EntityID entity = entityMap.FromNet(netId);
        if (entity == NULL_ENTITY)
        {
            staleNetIds.push_back(netId);
            continue;
        }
        if (!world.IsAlive(entity))
        {
            staleNetIds.push_back(netId);
            continue;
        }

        const bool_t bServerMinion =
            world.HasComponent<MinionComponent>(entity) ||
            world.HasComponent<MinionStateComponent>(entity);
        const bool_t bViegoSoul =
            world.HasComponent<ViegoSoulComponent>(entity);
        const bool_t bKalistaSentinel =
            world.HasComponent<KalistaSentinelComponent>(entity);
        const bool_t bWard =
            world.HasComponent<WardComponent>(entity);
        if (!bServerMinion && !bViegoSoul && !bKalistaSentinel && !bWard)
            continue;

        if (bServerMinion)
            CMinion_Manager::Get()->Release_NetworkVisual(entity);
        if (m_onRemoveEntity)
            m_onRemoveEntity(entity);
        world.DestroyEntity(entity);
        staleNetIds.push_back(netId);

        char msg[160]{};
        sprintf_s(msg,
            "[SnapshotApplier] remove stale minion netId=%u entity=%u\n",
            netId,
            static_cast<u32_t>(entity));
    }

    for (u32_t netId : staleNetIds)
    {
        entityMap.Unbind(netId);
        m_seenNetIds.erase(netId);
    }
}

EntityID CSnapshotApplier::EnsureEntity(
    CWorld& world,
    EntityIdMap& entityMap,
    u32_t netId,
    u8_t entityKind,
    u8_t championId,
    u16_t subtype,
    const Vec3& vPos,
    u8_t team)
{
    if (netId == NULL_NET_ENTITY)
        return NULL_ENTITY;

    const auto kind = static_cast<Shared::Schema::EntityKind>(entityKind);
    EntityID e = entityMap.FromNet(netId);
    if (e != NULL_ENTITY && !world.IsAlive(e))
    {
        entityMap.Unbind(netId);
        m_seenNetIds.erase(netId);
        e = NULL_ENTITY;
    }

    if (e == NULL_ENTITY)
    {
        e = TryBindStageStructureVisual(world, entityMap, netId, kind, subtype, team, vPos);
        if (e == NULL_ENTITY)
            e = TryBindStageJungleVisual(world, entityMap, netId, kind, subtype, vPos);
        if (e != NULL_ENTITY)
            m_seenNetIds.insert(netId);
    }

    if (e != NULL_ENTITY &&
        kind == Shared::Schema::EntityKind::Champion &&
        world.HasComponent<ChampionComponent>(e))
    {
        auto& champ = world.GetComponent<ChampionComponent>(e);
        const u8_t currentChampionID = static_cast<u8_t>(champ.id);
        if (currentChampionID != championId)
        {
            char msg[192]{};
            sprintf_s(msg,
                "[SnapshotApplier] champion mismatch netId=%u entity=%u visual=%u snapshot=%u\n",
                netId,
                static_cast<u32_t>(e),
                currentChampionID,
                championId);

            champ.id = static_cast<eChampion>(championId);
            champ.team = static_cast<eTeam>(team);

            if (m_onChampionVisualChanged)
                m_onChampionVisualChanged(e, championId, team);
        }
    }

    if (e != NULL_ENTITY)
    {
        if (kind == Shared::Schema::EntityKind::Turret ||
            kind == Shared::Schema::EntityKind::Inhibitor ||
            kind == Shared::Schema::EntityKind::Nexus)
        {
            MarkServerId(world, e, netId);
            EnsureSnapshotStructureRuntimeTags(world, e, kind, team, subtype);
        }

        if (kind == Shared::Schema::EntityKind::JungleMonster)
        {
            MarkServerId(world, e, netId);
            EnsureSnapshotJungleRuntimeTags(world, e, subtype);
        }

        if (kind == Shared::Schema::EntityKind::Ward)
        {
            MarkServerId(world, e, netId);
            EnsureSnapshotWardRuntimeTags(world, e, team, subtype);
        }

        return e;
    }

    if (m_onNewEntity && kind == Shared::Schema::EntityKind::Champion)
        e = m_onNewEntity(netId, championId, team);
    else
        e = world.CreateEntity();

    if (e == NULL_ENTITY)
        return NULL_ENTITY;

    entityMap.Bind(netId, e);
    m_seenNetIds.insert(netId);
    MarkServerId(world, e, netId);

    if (!world.HasComponent<TransformComponent>(e))
        world.AddComponent<TransformComponent>(e, TransformComponent{});

    if (kind != Shared::Schema::EntityKind::Projectile &&
        kind != Shared::Schema::EntityKind::Ward &&
        !world.HasComponent<HealthComponent>(e))
    {
        HealthComponent hp{};
        hp.fCurrent = 600.f;
        hp.fMaximum = 600.f;
        hp.bIsDead = false;
        world.AddComponent<HealthComponent>(e, hp);
    }

    if (kind == Shared::Schema::EntityKind::Champion &&
        !world.HasComponent<ChampionComponent>(e))
    {
        ChampionComponent champ{};
        champ.id = static_cast<eChampion>(championId);
        champ.team = static_cast<eTeam>(team);
        world.AddComponent<ChampionComponent>(e, champ);
    }
    if (kind == Shared::Schema::EntityKind::Champion &&
        !world.HasComponent<ExperienceComponent>(e))
    {
        world.AddComponent<ExperienceComponent>(e, ExperienceComponent{});
    }
    if (kind == Shared::Schema::EntityKind::Champion &&
        !world.HasComponent<SkillRankComponent>(e))
    {
        SkillRankComponent skillRank{};
        skillRank.pointsAvailable = 1;
        world.AddComponent<SkillRankComponent>(e, skillRank);
    }

    //kind == Champion 泥섎━ ?댄썑??異붽?
    if (kind == Shared::Schema::EntityKind::Minion)
    {
        if (!world.HasComponent<MinionStateComponent>(e))
        {
            MinionStateComponent ms{};
            ms.team = static_cast<eTeam>(team);
            world.AddComponent<MinionStateComponent>(e, ms);
        }
        if (!world.HasComponent<MinionComponent>(e))
        {
            MinionComponent minion{};
            minion.team = static_cast<eTeam>(team);
            world.AddComponent<MinionComponent>(e, minion);
        }
    }

    if (kind == Shared::Schema::EntityKind::Turret ||
        kind == Shared::Schema::EntityKind::Inhibitor ||
        kind == Shared::Schema::EntityKind::Nexus)
    {
        EnsureSnapshotStructureRuntimeTags(world, e, kind, team, subtype);
    }

    if (kind == Shared::Schema::EntityKind::JungleMonster)
    {
        EnsureSnapshotJungleRuntimeTags(world, e, subtype);
    }

    if (kind == Shared::Schema::EntityKind::Ward)
    {
        EnsureSnapshotWardRuntimeTags(world, e, team, subtype);
    }

    if (kind == Shared::Schema::EntityKind::Champion &&
        !world.HasComponent<StatComponent>(e))
    {
        StatComponent stat{};
        stat.championId = static_cast<eChampion>(championId);
        stat.moveSpeed = 5.f;
        stat.hpMax = 600.f;
        stat.manaMax = 300.f;
        world.AddComponent<StatComponent>(e, stat);
    }

    if (!world.HasComponent<ReplicatedPoseComponent>(e))
        world.AddComponent<ReplicatedPoseComponent>(e, ReplicatedPoseComponent{});
    if (!world.HasComponent<ReplicatedActionComponent>(e))
        world.AddComponent<ReplicatedActionComponent>(e, ReplicatedActionComponent{});

    SpawnSnapshotMarker(world, e, kind, team);

	return e;
}
