#include "Network/Client/EventApplier.h"
#include "Network/Client/NetworkEventTrace.h"
#include "GameInstance.h"
#include "Client/Private/Data/LoLVisualDefinitionPack.h"

#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include "Shared/Schemas/Generated/cpp/Event_generated.h"
#include "Shared/Schemas/Generated/cpp/Snapshot_generated.h"
#include <flatbuffers/flatbuffers.h>
#pragma pop_macro("max")
#pragma pop_macro("min")

#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "Map/MapDataFormats.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "ECS/Components/RenderComponent.h"
#include "GameObject/ChampionDef.h"
#include "GameObject/FX/FxCuePlayer.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxMeshComponent.h"
#include "GameObject/Champion/Yasuo/YasuoFxPresets.h"
#include "GameObject/Projectile/ProjectileKind.h"
#include "GameObject/Projectile/ProjectileVisualCatalog.h"
#include "GamePlay/ChampionCatalog.h"
#include "GamePlay/ChampionRegistry.h"
#include "GamePlay/SkillRegistry.h"
#include "GamePlay/VisualHookRegistry.h"
#include "GameObject/Champion/Ezreal/Ezreal_FxPresets.h"
#include "Resource/Animator.h"
#include "Renderer/ModelRenderer.h"
#include "Shared/GameSim/Components/FormOverrideComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/ReplicatedActionComponent.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
#include "Shared/GameSim/Feedback/GameplayFeedback.h"
#include "UI/AttackSpeedLab.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace
{
    // Kill, destroy, and objective event presentation.
    constexpr u8_t kKillFeedObjectActor = 1;
    constexpr u8_t kKillFeedObjectStructure = 2;
    constexpr u8_t kKillFeedObjectObjective = 3;
    constexpr u8_t kKillFeedObjectDragon = 4;
    constexpr u8_t kKillFeedObjectBaron = 5;

    eTeam TeamFromWire(u8_t value)
    {
        if (value == static_cast<u8_t>(eTeam::Blue))
            return eTeam::Blue;
        if (value == static_cast<u8_t>(eTeam::Red))
            return eTeam::Red;
        return eTeam::Neutral;
    }

    eTeam ResolveLocalTeamForKillFeed(CWorld& world)
    {
        eTeam localTeam = eTeam::Blue;
        bool_t bFound = false;
        world.ForEach<ChampionComponent, LocalPlayerTag>(
            [&](EntityID, ChampionComponent& champion, LocalPlayerTag&)
            {
                if (bFound)
                    return;
                localTeam = champion.team;
                bFound = true;
            });
        return localTeam;
    }

    const char* ResolveKillFeedMessage(u8_t objectKind, bool_t bSourceAlly, bool_t bTargetAlly)
    {
        if (objectKind == kKillFeedObjectActor)
        {
            if (bSourceAlly)
                return "적을 처치했습니다";
            if (bTargetAlly)
                return "아군이 당했습니다";
            return "적을 처치했습니다";
        }
        if (objectKind == kKillFeedObjectStructure)
        {
            if (bSourceAlly)
                return "포탑을 파괴했습니다";
            return "적이 포탑을 파괴했습니다";
        }
        if (objectKind == kKillFeedObjectObjective)
        {
            if (bSourceAlly)
                return "억제기를 파괴했습니다";
            return "적이 억제기를 파괴했습니다";
        }
        if (objectKind == kKillFeedObjectDragon)
        {
            if (bSourceAlly)
                return "드래곤을 처치했습니다";
            return "적이 드래곤을 처치했습니다";
        }
        if (objectKind == kKillFeedObjectBaron)
        {
            if (bSourceAlly)
                return "내셔 남작을 처치했습니다";
            return "적이 내셔 남작을 처치했습니다";
        }
        return "";
    }

    const ChampionDef* FindClientChampionDefForEvent(eChampion champion)
    {
        const ChampionCatalogEntry* entry = CChampionCatalog::Instance().Find(champion);
        if (entry && entry->pDef)
            return entry->pDef;

        const ChampionDef* registered = CChampionRegistry::Instance().Find(champion);
        if (registered)
            return registered;

        return FindChampionDef(champion);
    }

    u8_t SlotFromReplicatedAction(u16_t actionId)
    {
        switch (static_cast<eReplicatedActionId>(actionId))
        {
        case eReplicatedActionId::SkillQ: return static_cast<u8_t>(eSkillSlot::Q);
        case eReplicatedActionId::SkillW: return static_cast<u8_t>(eSkillSlot::W);
        case eReplicatedActionId::SkillE: return static_cast<u8_t>(eSkillSlot::E);
        case eReplicatedActionId::SkillR: return static_cast<u8_t>(eSkillSlot::R);
        default: return static_cast<u8_t>(eSkillSlot::BasicAttack);
        }
    }
    bool_t IsOneShotReplicatedAction(u16_t actionId)
    {
        const auto id = static_cast<eReplicatedActionId>(actionId);
        return id != eReplicatedActionId::None &&
            id != eReplicatedActionId::Recall;
    }
    bool_t ShouldLoopReplicatedAction(
        u16_t actionId,
        u8_t actionStage,
        const SkillDef* pSkillDef)
    {
        const auto id = static_cast<eReplicatedActionId>(actionId);
        if (id == eReplicatedActionId::None)
            return false;
        if (id == eReplicatedActionId::Recall)
            return true;
        if (!pSkillDef)
            return false;

        return actionStage >= 2u
            ? pSkillDef->bStage2PresentationLoopWhileActive
            : pSkillDef->bPresentationLoopWhileActive;
    }

    bool_t IsNewerActionSeq(u32_t seq, u32_t previousSeq)
    {
        if (seq == 0u)
            return false;
        if (previousSeq == 0u)
            return true;
        return static_cast<i32_t>(seq - previousSeq) > 0;
    }

    bool_t IsMinionEntity(CWorld& world, EntityID entity)
    {
        return entity != NULL_ENTITY &&
            (world.HasComponent<MinionComponent>(entity) ||
             world.HasComponent<MinionStateComponent>(entity));
    }

    EntityID FindLocalPlayerEntity(CWorld& world)
    {
        EntityID localEntity = NULL_ENTITY;
        world.ForEach<LocalPlayerTag>(
            [&](EntityID entity, LocalPlayerTag&)
            {
                if (localEntity == NULL_ENTITY)
                    localEntity = entity;
            });
        return localEntity;
    }

    std::string PrefixAnim(const ChampionDef& cd, const char* key)
    {
        if (!key || key[0] == 0)
            return {};

        if (cd.animPrefix && cd.animPrefix[0] != 0)
        {
            const size_t prefixLen = std::strlen(cd.animPrefix);
            if (std::strncmp(key, cd.animPrefix, prefixLen) == 0)
                return std::string(key);
            return std::string(cd.animPrefix) + key;
        }

        return std::string(key);
    }

    std::string ResolveRecallAnimName(const ChampionDef& cd, const ModelRenderer& renderer)
    {
        const std::string recall = PrefixAnim(cd, "recall");
        if (!recall.empty() && renderer.HasAnimationByName(recall))
            return recall;

        const std::string channel = PrefixAnim(cd, "channel");
        if (!channel.empty() && renderer.HasAnimationByName(channel))
            return channel;

        return recall;
    }

    u8_t SlotFromEffectFlags(u16_t flags)
    {
        const u8_t slot = static_cast<u8_t>(flags & 0x00ffu);
        return (slot < static_cast<u8_t>(eSkillSlot::SLOT_END))
            ? slot
            : static_cast<u8_t>(eSkillSlot::BasicAttack);
    }

    u8_t StageFromEffectFlags(u16_t flags)
    {
        const u8_t stage = static_cast<u8_t>((flags >> 12) & 0x0fu);
        return stage == 0u ? 1u : stage;
    }

    const char* ResolveYasuoQAnimKey(u8_t stage)
    {
        switch (stage)
        {
        case 4: return "spell1c";
        case 3: return "spell1_wind";
        case 2: return "spell1b";
        default: return "spell1a";
        }
    }

    const char* ResolveRivenQAnimKey(u8_t stage)
    {
        switch (stage)
        {
        case 3: return "spell1c";
        case 2: return "spell1b";
        default: return "spell1a";
        }
    }

    f32_t ResolveReplicatedActionPlaySpeed(
        eChampion champion,
        u8_t slot,
        u8_t stage,
        const SkillDef* pDef)
    {
        const u8_t stageIndex = stage > 0u ? static_cast<u8_t>(stage - 1u) : 0u;
        f32_t playSpeed = 1.f;
        if (const ClientData::ChampionVisualDefinition* pVisual =
            ClientData::FindChampionVisualDefinition(champion))
        {
            if (slot < ClientData::kVisualSkillSlotCount &&
                stageIndex < pVisual->skills[slot].stageCount &&
                stageIndex < ClientData::kVisualSkillStageCount)
            {
                playSpeed = pVisual->skills[slot].stages[stageIndex].animationPlaybackSpeed;
            }
        }
        else if (pDef)
        {
            playSpeed = (stage >= 2u && pDef->stage2VisualPlaySpeed > 0.01f)
                ? pDef->stage2VisualPlaySpeed
                : pDef->visualPlaySpeed;
        }

        return (std::isfinite(playSpeed) && playSpeed > 0.01f) ? playSpeed : 1.f;
    }

    u8_t SlotFromHookId(u32_t hookId)
    {
        switch (hookId & 0x00ffu)
        {
        case 0x12u:
        case 0x22u:
        case 0x32u:
        case 0x42u:
            return static_cast<u8_t>(eSkillSlot::Q);
        case 0x13u:
        case 0x23u:
        case 0x33u:
        case 0x43u:
            return static_cast<u8_t>(eSkillSlot::W);
        case 0x14u:
        case 0x24u:
        case 0x34u:
        case 0x44u:
            return static_cast<u8_t>(eSkillSlot::E);
        case 0x15u:
        case 0x25u:
        case 0x35u:
        case 0x45u:
            return static_cast<u8_t>(eSkillSlot::R);
        default:
            return static_cast<u8_t>(eSkillSlot::BasicAttack);
        }
    }

    eChampion ResolveChampionForVisualHook(CWorld& world, EntityID caster)
    {
        if (caster != NULL_ENTITY && world.HasComponent<ChampionComponent>(caster))
            return world.GetComponent<ChampionComponent>(caster).id;
        return eChampion::NONE;
    }

    eChampion ChampionFromHookId(u32_t effectId)
    {
        const u32_t champion = (effectId >> 16) & 0xffu;
        return champion != 0u ? static_cast<eChampion>(champion) : eChampion::NONE;
    }

    u8_t SlotFromHookIdOrFallback(u32_t effectId, u8_t fallbackSlot)
    {
        if (ChampionFromHookId(effectId) == eChampion::NONE)
            return fallbackSlot;
        return SlotFromHookId(effectId);
    }

    const SkillDef* FindSkillDefForVisualHook(
        CWorld& world,
        EntityID caster,
        eChampion hookChampion,
        u8_t slot)
    {
        const eChampion champion = hookChampion != eChampion::NONE
            ? hookChampion
            : ResolveChampionForVisualHook(world, caster);
        if (champion == eChampion::NONE || champion == eChampion::END)
            return nullptr;

        const SkillDef* def = CSkillRegistry::Instance().Find(champion, slot);
        if (!def)
            def = FindSkillDef(champion, slot);
        return def;
    }

    bool_t ShouldKeepEffectEventPosition(eChampion hookChampion, u8_t slot)
    {
        if (hookChampion != eChampion::KINDRED)
            return false;

        switch (static_cast<eSkillSlot>(slot))
        {
        case eSkillSlot::W:
        case eSkillSlot::R:
            return true;
        default:
            return false;
        }
    }

    const char* ResolveEffectTriggerCue(eChampion hookChampion, u8_t slot, u8_t skillStage)
    {
        if (hookChampion != eChampion::KINDRED)
            return nullptr;

        switch (static_cast<eSkillSlot>(slot))
        {
        case eSkillSlot::BasicAttack:
            return "Kindred.BA.Hit";
        case eSkillSlot::Q:
            return "Kindred.Q.Arrow";
        case eSkillSlot::W:
            return "Kindred.W.Zone";
        case eSkillSlot::E:
            switch (skillStage)
            {
            case 2:
                return "Kindred.E.Stack1";
            case 3:
                return "Kindred.E.Stack2";
            case 4:
                return "Kindred.E.Stack3";
            default:
                break;
            }
            return "Kindred.E.Mark";
        case eSkillSlot::R:
            return "Kindred.R.Zone";
        default:
            return nullptr;
        }
    }

    struct FeedbackLabel
    {
        GameplayFeedback::WorldTextFeedbackKind eKind =
            GameplayFeedback::WorldTextFeedbackKind::None;
        const char* pText = nullptr;
        Vec4 vColor{ 0.96f, 0.92f, 0.78f, 1.f };
    };

    const FeedbackLabel* FindFeedbackLabel(GameplayFeedback::WorldTextFeedbackKind kind)
    {
        static const FeedbackLabel kFeedbackLabels[] =
        {
            { GameplayFeedback::WorldTextFeedbackKind::Dodge, "회피!", Vec4{ 0.96f, 0.92f, 0.78f, 1.f } },
            { GameplayFeedback::WorldTextFeedbackKind::Slow, "느려짐", Vec4{ 0.58f, 0.78f, 1.0f, 1.f } },
            { GameplayFeedback::WorldTextFeedbackKind::Stun, "기절!", Vec4{ 1.0f, 0.82f, 0.32f, 1.f } },
            { GameplayFeedback::WorldTextFeedbackKind::Airborne, "공중에 뜸!", Vec4{ 0.72f, 0.88f, 1.0f, 1.f } },
            { GameplayFeedback::WorldTextFeedbackKind::Disarm, "무장 해제!", Vec4{ 0.96f, 0.92f, 0.78f, 1.f } },
            { GameplayFeedback::WorldTextFeedbackKind::Untargetable, "대상 지정 불가!", Vec4{ 0.96f, 0.92f, 0.78f, 1.f } },
            { GameplayFeedback::WorldTextFeedbackKind::Invisible, "은신", Vec4{ 0.96f, 0.92f, 0.78f, 1.f } },
            { GameplayFeedback::WorldTextFeedbackKind::Shield, "보호막", Vec4{ 0.96f, 0.92f, 0.78f, 1.f } },
            { GameplayFeedback::WorldTextFeedbackKind::Heal, "회복!", Vec4{ 0.96f, 0.92f, 0.78f, 1.f } },
            { GameplayFeedback::WorldTextFeedbackKind::Crit, "치명타!", Vec4{ 0.96f, 0.92f, 0.78f, 1.f } },
        };

        for (const FeedbackLabel& label : kFeedbackLabels)
        {
            if (label.eKind == kind)
                return &label;
        }

        return nullptr;
    }

    u64_t BuildCueKey(u32_t a, u32_t b, u32_t c, u64_t d, u32_t e)
    {
        u64_t h = 1469598103934665603ull;
        auto Mix = [&h](u64_t v)
            {
                h ^= v;
                h *= 1099511628211ull;
            };

        Mix(a);
        Mix(b);
        Mix(c);
        Mix(d);
        Mix(e);
        return h;
    }

    bool_t TryAdvanceMutation(
        PresentationMutationStamp& current,
        const PresentationMutationStamp& candidate)
    {
        if (!IsNewerPresentationMutation(candidate, current))
            return false;
        current = candidate;
        return true;
    }

    PresentationMutationStamp MakeEventMutation(
        u64_t uServerTick,
        u32_t uEventOrdinal,
        ePresentationMutationPhase ePhase)
    {
        return PresentationMutationStamp{
            uServerTick,
            uEventOrdinal,
            ePhase,
            true };
    }

    PresentationMutationStamp MakeSnapshotMutation(u64_t uServerTick)
    {
        return PresentationMutationStamp{
            uServerTick,
            (std::numeric_limits<u32_t>::max)(),
            ePresentationMutationPhase::SnapshotTruth,
            true };
    }

    f32_t RemainingTickLifetimeSec(u64_t uNowTick, u64_t uExpireTick)
    {
        constexpr f32_t kTicksPerSecond = 30.f;
        const u64_t uRemainingTicks = uExpireTick > uNowTick
            ? uExpireTick - uNowTick
            : 0u;
        return static_cast<f32_t>(uRemainingTicks) / kTicksPerSecond;
    }

    EntityID ResolveLiveEntity(CWorld& world, EntityIdMap& entityMap, NetEntityId netId)
    {
        if (netId == NULL_NET_ENTITY)
            return NULL_ENTITY;

        const EntityID entity = entityMap.FromNet(netId);
        if (entity == NULL_ENTITY || !world.IsAlive(entity))
            return NULL_ENTITY;

        return entity;
    }

    void DestroyEntityIfAlive(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.IsAlive(entity))
            world.DestroyEntity(entity);
    }

    bool_t HasLiveVisualEntity(
        CWorld& world,
        const std::vector<EntityID>& entities)
    {
        for (EntityID entity : entities)
        {
            if (entity != NULL_ENTITY && world.IsAlive(entity))
                return true;
        }
        return false;
    }

}

std::unique_ptr<CEventApplier> CEventApplier::Create()
{
    return std::unique_ptr<CEventApplier>(new CEventApplier());
}

void CEventApplier::RebaseTimeline(
    CWorld& world,
    EntityIdMap& entityMap)
{
    for (EntityHandle entity : m_timelineVisualEntities)
    {
        if (world.IsAlive(entity))
            world.DestroyEntity(entity);
    }
    for (const auto& [projectileNet, visuals] : m_projectileVisualEntities)
    {
        for (EntityID entity : visuals)
            DestroyEntityIfAlive(world, entity);
        const EntityID projectileEntity = entityMap.FromNet(projectileNet);
        if (projectileEntity != NULL_ENTITY &&
            world.IsAlive(projectileEntity) &&
            world.HasComponent<ReplicatedProjectilePresentationTag>(projectileEntity))
        {
            world.DestroyEntity(projectileEntity);
        }
        entityMap.Unbind(projectileNet);
    }
    for (const auto& entry : m_ezrealFluxVisualEntities)
    {
        for (EntityID entity : entry.second)
            DestroyEntityIfAlive(world, entity);
    }
    for (const auto& [_, entity] : m_yasuoWindWallAnchors)
        DestroyEntityIfAlive(world, entity);
    for (const auto& [_, visuals] : m_yasuoWindWallVisualEntities)
    {
        for (EntityID entity : visuals)
            DestroyEntityIfAlive(world, entity);
    }

    m_timelineVisualEntities.clear();
    m_projectileVisualEntities.clear();
    m_ezrealFluxVisualEntities.clear();
    m_ezrealFluxExpireTicks.clear();
    m_yasuoWindWallAnchors.clear();
    m_yasuoWindWallVisualEntities.clear();
    m_yasuoWindWallExpireTicks.clear();
    m_lastActionSeq.clear();
    m_seenEffectCueKeys.clear();
    m_seenKillFeedKeys.clear();
    m_snapshotProjectileNetIds.clear();
    m_snapshotEzrealFluxKeys.clear();
    m_snapshotYasuoWindWallKeys.clear();
    m_seenProjectileHitCueKeys.clear();
    m_projectileMutationStamps.clear();
    m_ezrealFluxMutationStamps.clear();
    m_reconcileServerTick = 0u;
    m_bReconcileFullSnapshot = false;
}

void CEventApplier::OnEvent(
    CWorld& world,
    EntityIdMap& entityMap,
    const u8_t* payload,
    u32_t len)
{
    if (!payload || len == 0)
        return;

    if (m_timelineVisualEntities.size() > 4096u)
    {
        m_timelineVisualEntities.erase(
            std::remove_if(
                m_timelineVisualEntities.begin(),
                m_timelineVisualEntities.end(),
                [&](EntityHandle entity) { return !world.IsAlive(entity); }),
            m_timelineVisualEntities.end());
    }

    flatbuffers::Verifier verifier(payload, len);
    if (!Shared::Schema::VerifyEventPacketBuffer(verifier))
    {
        static u32_t s_eventVerifyFailLogCount = 0;
        if (s_eventVerifyFailLogCount < 8)
        {
            char msg[128]{};
            sprintf_s(msg, "[EventApplier] invalid EventPacket buffer len=%u\n", len);
            OutputDebugStringA(msg);
            ++s_eventVerifyFailLogCount;
        }
        return;
    }

    CNetworkEventTrace::Instance().RecordEventPacket(payload, len);

    const auto* packet = Shared::Schema::GetEventPacket(payload);
    if (!packet)
        return;

    switch (packet->kind())
    {
    case Shared::Schema::EventKind::ActionStart:
        ApplyActionStart(world, entityMap, packet->actionStart());
        break;
    case Shared::Schema::EventKind::ProjectileSpawn:
        ApplyProjectileSpawn(
            world,
            entityMap,
            packet->projectile(),
            packet->serverTick(),
            packet->eventOrdinal());
        break;
    case Shared::Schema::EventKind::ProjectileHit:
        ApplyProjectileHit(
            world,
            entityMap,
            packet->projectileHit(),
            packet->serverTick(),
            packet->eventOrdinal());
        break;
    case Shared::Schema::EventKind::EffectTrigger:
        ApplyEffectTrigger(
            world,
            entityMap,
            packet->effect(),
            packet->serverTick(),
            packet->eventOrdinal());
        break;
    case Shared::Schema::EventKind::Damage:
        ApplyDamage(world, entityMap, packet->damage());
        break;
    case Shared::Schema::EventKind::KillFeed:
        ApplyKillFeed(world, entityMap, packet->killFeed(), packet->serverTick());
        break;
    default:
        break;
    }
}

void CEventApplier::ApplyActionStart(
    CWorld& world,
    EntityIdMap& entityMap,
    const Shared::Schema::ActionStartEvent* ev)
{
    if (!ev || ev->netId() == NULL_NET_ENTITY)
        return;
    const EntityID entity = ResolveLiveEntity(world, entityMap, ev->netId());
    if (entity == NULL_ENTITY)
        return;
    auto& action = world.HasComponent<ReplicatedActionComponent>(entity)
        ? world.GetComponent<ReplicatedActionComponent>(entity)
        : world.AddComponent<ReplicatedActionComponent>(entity, ReplicatedActionComponent{});
    const u8_t actionStage = ev->actionStage() == 0u ? 1u : ev->actionStage();
    const u32_t previousPlayedSeq = m_lastActionSeq[ev->netId()];
    const bool_t bShouldPlay =
        IsNewerActionSeq(ev->actionSeq(), previousPlayedSeq);
    action.actionId = ev->actionId();
    action.startTick = ev->startTick();
    action.lockEndTick = ev->lockEndTick();
    action.sequence = ev->actionSeq();
    action.commandSequence = ev->commandSeq();
    action.sourceChampion = static_cast<eChampion>(ev->sourceChampionId());
    action.sourceSlot = ev->sourceSlot();
    action.stage = actionStage;
    action.movePolicy = static_cast<eSkillActionMovePolicy>(ev->movePolicy());
    if (!bShouldPlay)
        return;
    m_lastActionSeq[ev->netId()] = ev->actionSeq();
    PlayReplicatedActionVisual(world, entity, ev->actionId(), actionStage);
}

bool_t CEventApplier::RetryCurrentActionVisual(
    CWorld& world,
    EntityID entity,
    u16_t expectedActionId)
{
    if (entity == NULL_ENTITY ||
        !world.HasComponent<ReplicatedActionComponent>(entity))
    {
        return false;
    }

    const auto& action = world.GetComponent<ReplicatedActionComponent>(entity);
    if (action.sequence == 0u || action.actionId != expectedActionId)
        return false;

    return PlayReplicatedActionVisual(
        world,
        entity,
        action.actionId,
        action.stage == 0u ? 1u : action.stage);
}

void CEventApplier::ApplyProjectileSpawn(
    CWorld& world,
    EntityIdMap& entityMap,
    const Shared::Schema::ProjectileSpawnEvent* ev,
    u64_t serverTick,
    u32_t uEventOrdinal)
{
    if (!ev)
        return;

    const Vec3 pos{ ev->startX(), ev->startY(), ev->startZ() };
    if (ev->netId() == NULL_NET_ENTITY)
    {
        const ProjectileVisualDesc& visual =
            ProjectileVisualCatalog::Resolve(ev->kind());
        if (visual.pszSpawnCue)
        {
            Vec3 dir = WintersMath::Normalize3D(
                Vec3{ ev->dirX(), ev->dirY(), ev->dirZ() });
            if (dir.x == 0.f && dir.y == 0.f && dir.z == 0.f)
                dir = { 0.f, 0.f, 1.f };
            FxCueContext fx{};
            fx.vWorldPos = pos;
            fx.vForward = dir;
            fx.vVelocity = {
                dir.x * ev->speed(),
                dir.y * ev->speed(),
                dir.z * ev->speed() };
            fx.bOverrideVelocity = true;
            fx.bOverrideLifetime = true;
            fx.fLifetimeOverride =
                ev->speed() > 0.01f && ev->maxDist() > 0.f
                    ? ev->maxDist() / ev->speed()
                    : 1.f;
            fx.pFxMeshRenderer = m_pFxMeshRenderer;
            std::vector<EntityID> spawned;
            CFxCuePlayer::PlayAll(world, visual.pszSpawnCue, fx, &spawned);
            for (EntityID entity : spawned)
                m_timelineVisualEntities.push_back(world.GetEntityHandle(entity));
        }
        return;
    }

    if (!TryAdvanceMutation(
            m_projectileMutationStamps[ev->netId()],
            MakeEventMutation(
                serverTick,
                uEventOrdinal,
                ePresentationMutationPhase::SpawnOrMark)))
    {
        return;
    }

    EnsureProjectilePresentation(
        world,
        entityMap,
        ev->netId(),
        ev->kind(),
        pos,
        Vec3{ ev->dirX(), ev->dirY(), ev->dirZ() },
        ev->speed(),
        ev->maxDist());
}

void CEventApplier::ApplyProjectileHit(
    CWorld& world,
    EntityIdMap& entityMap,
    const Shared::Schema::ProjectileHitEvent* ev,
    u64_t serverTick,
    u32_t uEventOrdinal)
{
    if (!ev)
        return;

    const u64_t cueKey = BuildCueKey(
        ev->netId(),
        ev->targetNet(),
        ev->kind(),
        serverTick,
        ev->contactOrdinal());
    if (m_seenProjectileHitCueKeys.size() > 4096u)
        m_seenProjectileHitCueKeys.clear();
    if (!m_seenProjectileHitCueKeys.insert(cueKey).second)
        return;

    const Vec3 pos{ ev->posX(), ev->posY(), ev->posZ() };
    const ProjectileVisualDesc& visual = ProjectileVisualCatalog::Resolve(ev->kind());
    const char* pszContactCue = nullptr;
    switch (ev->contactReason())
    {
    case Shared::Schema::ProjectileContactReason::None:
        pszContactCue = ev->targetNet() != NULL_NET_ENTITY
            ? visual.pszHitCue
            : visual.pszExpireCue;
        break;
    case Shared::Schema::ProjectileContactReason::UnitHit:
        pszContactCue = visual.pszHitCue;
        break;
    case Shared::Schema::ProjectileContactReason::Barrier:
        pszContactCue = visual.pszBarrierCue;
        break;
    case Shared::Schema::ProjectileContactReason::Terrain:
        pszContactCue = visual.pszTerrainCue;
        break;
    case Shared::Schema::ProjectileContactReason::RangeExpired:
    case Shared::Schema::ProjectileContactReason::SourceInvalid:
    case Shared::Schema::ProjectileContactReason::TargetInvalid:
    case Shared::Schema::ProjectileContactReason::InvalidTrajectory:
    case Shared::Schema::ProjectileContactReason::HitLimit:
        pszContactCue = visual.pszExpireCue;
        break;
    default:
        break;
    }

    if (pszContactCue)
    {
        FxCueContext fx{};
        fx.vWorldPos = pos;
        fx.vForward = { 0.f, 0.f, 1.f };
        fx.pFxMeshRenderer = m_pFxMeshRenderer;
        std::vector<EntityID> spawned;
        CFxCuePlayer::PlayAll(world, pszContactCue, fx, &spawned);
        for (EntityID entity : spawned)
            m_timelineVisualEntities.push_back(world.GetEntityHandle(entity));
    }

    if (visual.pszAttachedCue &&
        (ev->contactReason() ==
                Shared::Schema::ProjectileContactReason::UnitHit ||
            (ev->contactReason() ==
                Shared::Schema::ProjectileContactReason::None &&
                ev->targetNet() != NULL_NET_ENTITY)))
    {
        EntityID attachTo = NULL_ENTITY;
        if (ev->targetNet() != NULL_NET_ENTITY)
            attachTo = ResolveLiveEntity(world, entityMap, ev->targetNet());

        if (attachTo != NULL_ENTITY)
        {
            FxCueContext fx{};
            fx.vWorldPos = pos;
            fx.attachTo = attachTo;
            fx.pFxMeshRenderer = m_pFxMeshRenderer;
            if (visual.bAttachedCueRandomJitter)
            {
                // 히트마다 다른 각도/위치 (박힌 창 누적 분산 — 레거시
                // KalistaFx::SpawnESpearStuck 의 밴드와 동일: yaw 0~2π,
                // 틸트 ±0.3rad, 오프셋 XZ ±0.3 / Y +0~1).
                fx.bApplyAttachJitter = true;
                fx.vRotationJitter = {
                    (static_cast<f32_t>(rand()) / RAND_MAX - 0.5f) * 0.6f,
                    (static_cast<f32_t>(rand()) / RAND_MAX) * 6.2832f,
                    (static_cast<f32_t>(rand()) / RAND_MAX - 0.5f) * 0.6f };
                fx.vAttachOffsetJitter = {
                    (static_cast<f32_t>(rand()) / RAND_MAX - 0.5f) * 0.6f,
                    (static_cast<f32_t>(rand()) / RAND_MAX),
                    (static_cast<f32_t>(rand()) / RAND_MAX - 0.5f) * 0.6f };
            }
            std::vector<EntityID> spawned;
            CFxCuePlayer::PlayAll(
                world, visual.pszAttachedCue, fx, &spawned);
            for (EntityID entity : spawned)
                m_timelineVisualEntities.push_back(world.GetEntityHandle(entity));
        }
    }

    bool_t bApplyTerminalMutation = true;
    if (ev->bDestroyed() && ev->netId() != NULL_NET_ENTITY)
    {
        bApplyTerminalMutation = TryAdvanceMutation(
            m_projectileMutationStamps[ev->netId()],
            MakeEventMutation(
                serverTick,
                uEventOrdinal,
                ePresentationMutationPhase::ContactOrClear));
    }

    if (ev->bDestroyed() &&
        ev->netId() != NULL_NET_ENTITY &&
        bApplyTerminalMutation)
    {
        DestroyProjectileVisuals(world, ev->netId());
        const EntityID entity = ResolveLiveEntity(world, entityMap, ev->netId());
        DestroyEntityIfAlive(world, entity);
        entityMap.Unbind(ev->netId());
    }
}

EntityID CEventApplier::EnsureProjectilePresentation(
    CWorld& world,
    EntityIdMap& entityMap,
    NetEntityId uProjectileNet,
    u16_t uProjectileKind,
    const Vec3& vPosition,
    const Vec3& vDirection,
    f32_t fSpeed,
    f32_t fRemainingDistance)
{
    if (uProjectileNet == NULL_NET_ENTITY)
        return NULL_ENTITY;

    EntityID entity = ResolveLiveEntity(world, entityMap, uProjectileNet);
    if (entity == NULL_ENTITY)
    {
        entity = world.CreateEntity();
        entityMap.Bind(uProjectileNet, entity);
    }

    if (!world.HasComponent<TransformComponent>(entity))
        world.AddComponent<TransformComponent>(entity, TransformComponent{});
    if (!world.HasComponent<ReplicatedProjectilePresentationTag>(entity))
    {
        world.AddComponent<ReplicatedProjectilePresentationTag>(
            entity,
            ReplicatedProjectilePresentationTag{});
    }

    Vec3 direction = WintersMath::Normalize3D(vDirection);
    if (direction.x == 0.f && direction.y == 0.f && direction.z == 0.f)
        direction = { 0.f, 0.f, 1.f };
    const ProjectileVisualDesc& visual =
        ProjectileVisualCatalog::Resolve(uProjectileKind);
    const f32_t yaw =
        WintersMath::YawFromDirectionXZ(direction) + visual.fYawOffset;

    TransformComponent& transform = world.GetComponent<TransformComponent>(entity);
    transform.SetPosition(vPosition);
    const Vec3 rotation = transform.GetRotation();
    transform.SetRotation({ rotation.x, yaw, rotation.z });

    auto visualIt = m_projectileVisualEntities.find(uProjectileNet);
    if (visualIt != m_projectileVisualEntities.end() &&
        !visualIt->second.empty() &&
        !HasLiveVisualEntity(world, visualIt->second))
    {
        m_projectileVisualEntities.erase(visualIt);
        visualIt = m_projectileVisualEntities.end();
    }
    if (visualIt == m_projectileVisualEntities.end())
    {
        auto [insertedIt, _] = m_projectileVisualEntities.try_emplace(
            uProjectileNet,
            std::vector<EntityID>{});
        visualIt = insertedIt;

        if (visual.pszSpawnCue)
        {
            const f32_t lifetime =
                fSpeed > 0.01f && fRemainingDistance > 0.f
                    ? (std::max)(0.05f, fRemainingDistance / fSpeed)
                    : 1.f;
            FxCueContext fx{};
            fx.vWorldPos = vPosition;
            fx.vForward = direction;
            fx.vVelocity = {};
            fx.attachTo = entity;
            fx.bOverrideVelocity = true;
            fx.bOverrideLifetime = true;
            fx.fLifetimeOverride = lifetime;
            fx.pFxMeshRenderer = m_pFxMeshRenderer;
            CFxCuePlayer::PlayAll(
                world,
                visual.pszSpawnCue,
                fx,
                &visualIt->second);
        }
    }

    for (EntityID visualEntity : visualIt->second)
    {
        if (!world.IsAlive(visualEntity))
            continue;
        if (world.HasComponent<FxMeshComponent>(visualEntity))
        {
            FxMeshComponent& mesh =
                world.GetComponent<FxMeshComponent>(visualEntity);
            mesh.vWorldPos = vPosition;
            mesh.vVelocity = {};
            mesh.attachTo = entity;
            mesh.vRotation.y = visual.bSuppressMeshDirectionalYaw ? 0.f : yaw;
        }
        if (world.HasComponent<FxBillboardComponent>(visualEntity))
        {
            FxBillboardComponent& billboard =
                world.GetComponent<FxBillboardComponent>(visualEntity);
            billboard.vWorldPos = vPosition;
            billboard.vVelocity = {};
            billboard.attachTo = entity;
            billboard.fYaw = yaw;
        }
    }

    return entity;
}

void CEventApplier::DestroyProjectileVisuals(CWorld& world, NetEntityId projectileNet)
{
    auto it = m_projectileVisualEntities.find(projectileNet);
    if (it == m_projectileVisualEntities.end())
        return;

    for (EntityID visualEntity : it->second)
    {
        DestroyEntityIfAlive(world, visualEntity);
    }
    m_projectileVisualEntities.erase(it);
}

void CEventApplier::DestroyEzrealFluxVisuals(CWorld& world, u64_t relationKey)
{
    auto it = m_ezrealFluxVisualEntities.find(relationKey);
    if (it != m_ezrealFluxVisualEntities.end())
    {
        for (EntityID visualEntity : it->second)
            DestroyEntityIfAlive(world, visualEntity);
        m_ezrealFluxVisualEntities.erase(it);
    }
    m_ezrealFluxExpireTicks.erase(relationKey);
}

void CEventApplier::DestroyYasuoWindWallVisuals(
    CWorld& world,
    u64_t wallKey)
{
    const auto visualIt = m_yasuoWindWallVisualEntities.find(wallKey);
    if (visualIt != m_yasuoWindWallVisualEntities.end())
    {
        for (EntityID visualEntity : visualIt->second)
            DestroyEntityIfAlive(world, visualEntity);
        m_yasuoWindWallVisualEntities.erase(visualIt);
    }

    const auto anchorIt = m_yasuoWindWallAnchors.find(wallKey);
    if (anchorIt != m_yasuoWindWallAnchors.end())
    {
        DestroyEntityIfAlive(world, anchorIt->second);
        m_yasuoWindWallAnchors.erase(anchorIt);
    }
    m_yasuoWindWallExpireTicks.erase(wallKey);
}

void CEventApplier::BeginSnapshotReconciliation(
    u64_t uServerTick,
    bool_t bFullSnapshot)
{
    m_reconcileServerTick = uServerTick;
    m_bReconcileFullSnapshot = bFullSnapshot;
    m_snapshotProjectileNetIds.clear();
    m_snapshotEzrealFluxKeys.clear();
    m_snapshotYasuoWindWallKeys.clear();
}

void CEventApplier::UpsertProjectileSnapshot(
    CWorld& world,
    EntityIdMap& entityMap,
    NetEntityId uProjectileNet,
    u16_t uProjectileKind,
    const Vec3& vPosition,
    const Vec3& vDirection,
    f32_t fSpeed,
    f32_t fMaxDistance,
    f32_t fTraveledDistance)
{
    if (uProjectileNet == NULL_NET_ENTITY)
        return;

    m_snapshotProjectileNetIds.insert(uProjectileNet);
    if (!TryAdvanceMutation(
            m_projectileMutationStamps[uProjectileNet],
            MakeSnapshotMutation(m_reconcileServerTick)))
    {
        return;
    }

    EnsureProjectilePresentation(
        world,
        entityMap,
        uProjectileNet,
        uProjectileKind,
        vPosition,
        vDirection,
        fSpeed,
        (std::max)(0.f, fMaxDistance - fTraveledDistance));
}

void CEventApplier::UpsertEzrealFluxSnapshot(
    CWorld& world,
    EntityIdMap& entityMap,
    NetEntityId uSourceNet,
    NetEntityId uTargetNet,
    u64_t uExpireTick)
{
    if (uSourceNet == NULL_NET_ENTITY || uTargetNet == NULL_NET_ENTITY)
        return;

    const u64_t relationKey =
        (static_cast<u64_t>(uSourceNet) << 32u) |
        static_cast<u64_t>(uTargetNet);
    m_snapshotEzrealFluxKeys.insert(relationKey);
    if (!TryAdvanceMutation(
            m_ezrealFluxMutationStamps[relationKey],
            MakeSnapshotMutation(m_reconcileServerTick)))
    {
        return;
    }

    if (uExpireTick <= m_reconcileServerTick)
    {
        DestroyEzrealFluxVisuals(world, relationKey);
        return;
    }

    const EntityID target = ResolveLiveEntity(world, entityMap, uTargetNet);
    if (target == NULL_ENTITY)
        return;

    Vec3 position{};
    if (world.HasComponent<TransformComponent>(target))
        position = world.GetComponent<TransformComponent>(target).GetPosition();

    const auto expireIt = m_ezrealFluxExpireTicks.find(relationKey);
    const auto visualIt = m_ezrealFluxVisualEntities.find(relationKey);
    const bool_t bNeedsRefresh =
        expireIt == m_ezrealFluxExpireTicks.end() ||
        expireIt->second != uExpireTick ||
        visualIt == m_ezrealFluxVisualEntities.end() ||
        (visualIt != m_ezrealFluxVisualEntities.end() &&
            !visualIt->second.empty() &&
            !HasLiveVisualEntity(world, visualIt->second));
    if (bNeedsRefresh)
    {
        DestroyEzrealFluxVisuals(world, relationKey);
        FxCueContext fx{};
        fx.vWorldPos = position;
        fx.vForward = { 0.f, 0.f, 1.f };
        fx.attachTo = target;
        fx.bOverrideLifetime = true;
        fx.fLifetimeOverride = RemainingTickLifetimeSec(
            m_reconcileServerTick,
            uExpireTick);
        fx.pFxMeshRenderer = m_pFxMeshRenderer;
        std::vector<EntityID> spawned;
        CFxCuePlayer::PlayAll(world, "Ezreal.W.Mark", fx, &spawned);
        m_ezrealFluxVisualEntities[relationKey] = std::move(spawned);
        m_ezrealFluxExpireTicks[relationKey] = uExpireTick;
        return;
    }

    for (EntityID visualEntity : visualIt->second)
    {
        if (!world.IsAlive(visualEntity))
            continue;
        if (world.HasComponent<FxMeshComponent>(visualEntity))
        {
            FxMeshComponent& mesh =
                world.GetComponent<FxMeshComponent>(visualEntity);
            mesh.attachTo = target;
            mesh.vWorldPos = position;
        }
        if (world.HasComponent<FxBillboardComponent>(visualEntity))
        {
            FxBillboardComponent& billboard =
                world.GetComponent<FxBillboardComponent>(visualEntity);
            billboard.attachTo = target;
            billboard.vWorldPos = position;
        }
    }
}

void CEventApplier::UpsertYasuoWindWallSnapshot(
    CWorld& world,
    NetEntityId uSourceNet,
    u64_t uSpawnTick,
    const Vec3& vCenter,
    const Vec3& vDirection,
    f32_t fHalfLength,
    f32_t fHalfThickness,
    u64_t uExpireTick)
{
    const u64_t wallKey = BuildCueKey(
        uSourceNet,
        0u,
        static_cast<u32_t>(
            Shared::Schema::GameplayStateKind::YasuoWindWall),
        uSpawnTick,
        0u);
    m_snapshotYasuoWindWallKeys.insert(wallKey);

    if (uExpireTick <= m_reconcileServerTick)
    {
        DestroyYasuoWindWallVisuals(world, wallKey);
        return;
    }

    Vec3 direction = WintersMath::Normalize3D(vDirection);
    if (direction.x == 0.f && direction.y == 0.f && direction.z == 0.f)
        direction = { 0.f, 0.f, 1.f };
    const f32_t yaw = WintersMath::YawFromDirectionXZ(direction);

    const auto expireIt = m_yasuoWindWallExpireTicks.find(wallKey);
    const auto anchorIt = m_yasuoWindWallAnchors.find(wallKey);
    const auto visualIt = m_yasuoWindWallVisualEntities.find(wallKey);
    const bool_t bNeedsRefresh =
        expireIt == m_yasuoWindWallExpireTicks.end() ||
        expireIt->second != uExpireTick ||
        anchorIt == m_yasuoWindWallAnchors.end() ||
        !world.IsAlive(anchorIt->second) ||
        (visualIt != m_yasuoWindWallVisualEntities.end() &&
            !visualIt->second.empty() &&
            !HasLiveVisualEntity(world, visualIt->second));

    EntityID anchor = NULL_ENTITY;
    if (bNeedsRefresh)
    {
        DestroyYasuoWindWallVisuals(world, wallKey);
        anchor = world.CreateEntity();
        world.AddComponent<TransformComponent>(anchor, TransformComponent{});
        m_yasuoWindWallAnchors[wallKey] = anchor;
        m_yasuoWindWallExpireTicks[wallKey] = uExpireTick;

        std::vector<EntityID> visuals;
        YasuoFx::SpawnWWindWall(
            world,
            m_pFxMeshRenderer,
            vCenter,
            direction,
            RemainingTickLifetimeSec(m_reconcileServerTick, uExpireTick),
            (std::max)(0.f, 2.f * fHalfLength),
            (std::max)(0.f, 2.f * fHalfThickness),
            0.01f,
            anchor,
            &visuals);
        m_yasuoWindWallVisualEntities[wallKey] = std::move(visuals);
    }
    else
    {
        anchor = anchorIt->second;
    }

    TransformComponent& transform =
        world.GetComponent<TransformComponent>(anchor);
    transform.SetPosition(vCenter);
    const Vec3 rotation = transform.GetRotation();
    transform.SetRotation({ rotation.x, yaw, rotation.z });

    for (EntityID visualEntity : m_yasuoWindWallVisualEntities[wallKey])
    {
        if (!world.IsAlive(visualEntity))
            continue;
        if (world.HasComponent<FxMeshComponent>(visualEntity))
        {
            FxMeshComponent& mesh =
                world.GetComponent<FxMeshComponent>(visualEntity);
            mesh.attachTo = anchor;
            mesh.vWorldPos = vCenter;
            mesh.vRotation.y = yaw;
        }
        if (world.HasComponent<FxBillboardComponent>(visualEntity))
        {
            FxBillboardComponent& billboard =
                world.GetComponent<FxBillboardComponent>(visualEntity);
            billboard.attachTo = anchor;
            billboard.vWorldPos = vCenter;
            billboard.fYaw = yaw;
        }
    }
}

void CEventApplier::EndSnapshotReconciliation(
    CWorld& world,
    EntityIdMap& entityMap)
{
    if (!m_bReconcileFullSnapshot)
        return;

    const PresentationMutationStamp snapshotMutation =
        MakeSnapshotMutation(m_reconcileServerTick);

    std::vector<NetEntityId> staleProjectiles;
    staleProjectiles.reserve(m_projectileVisualEntities.size());
    for (const auto& [projectileNet, _] : m_projectileVisualEntities)
    {
        if (m_snapshotProjectileNetIds.find(projectileNet) !=
            m_snapshotProjectileNetIds.end())
        {
            continue;
        }
        if (TryAdvanceMutation(
                m_projectileMutationStamps[projectileNet],
                snapshotMutation))
        {
            staleProjectiles.push_back(projectileNet);
        }
    }
    for (NetEntityId projectileNet : staleProjectiles)
    {
        DestroyProjectileVisuals(world, projectileNet);
        const EntityID entity = entityMap.FromNet(projectileNet);
        if (entity != NULL_ENTITY &&
            world.IsAlive(entity) &&
            world.HasComponent<ReplicatedProjectilePresentationTag>(entity))
        {
            world.DestroyEntity(entity);
        }
        entityMap.Unbind(projectileNet);
    }

    std::vector<u64_t> staleFluxKeys;
    staleFluxKeys.reserve(m_ezrealFluxVisualEntities.size());
    for (const auto& [relationKey, _] : m_ezrealFluxVisualEntities)
    {
        if (m_snapshotEzrealFluxKeys.find(relationKey) !=
            m_snapshotEzrealFluxKeys.end())
        {
            continue;
        }
        if (TryAdvanceMutation(
                m_ezrealFluxMutationStamps[relationKey],
                snapshotMutation))
        {
            staleFluxKeys.push_back(relationKey);
        }
    }
    for (u64_t relationKey : staleFluxKeys)
        DestroyEzrealFluxVisuals(world, relationKey);

    std::vector<u64_t> staleWallKeys;
    staleWallKeys.reserve(m_yasuoWindWallAnchors.size());
    for (const auto& [wallKey, _] : m_yasuoWindWallAnchors)
    {
        if (m_snapshotYasuoWindWallKeys.find(wallKey) ==
            m_snapshotYasuoWindWallKeys.end())
        {
            staleWallKeys.push_back(wallKey);
        }
    }
    for (u64_t wallKey : staleWallKeys)
        DestroyYasuoWindWallVisuals(world, wallKey);
}

void CEventApplier::ApplyEffectTrigger(
    CWorld& world,
    EntityIdMap& entityMap,
    const Shared::Schema::EffectTriggerEvent* ev,
    u64_t uServerTick,
    u32_t uEventOrdinal)
{
    if (!ev)
        return;

    const u32_t effectId = ev->effectId();
    if (effectId == kGlobalGameEndEffect)
    {
        // 넥서스 파괴 게임 종료 — 엔티티 해석 없이 latch만 세운다 (S030).
        m_bGameEndPending = true;
        m_uGameEndWinningTeam = static_cast<u8_t>(ev->flags());

        // 패배 팀 넥서스 파괴 버스트 — 파괴 서브메시 스왑과 같은 순간에 재생.
        // 스테이지 비주얼과 스냅샷 생성 넥서스가 공존할 수 있으므로
        // 서버 바운드(ServerIdComponent) 엔티티를 우선해 한 번만 재생한다.
        const eTeam losingTeam =
            m_uGameEndWinningTeam == 0u ? eTeam::Red : eTeam::Blue;
        EntityID burstNexus = NULL_ENTITY;
        bool_t bBurstNexusBound = false;
        world.ForEach<StructureComponent, TransformComponent>(
            [&](EntityID e, StructureComponent& structure, TransformComponent&)
            {
                if (structure.kind !=
                        static_cast<u32_t>(Winters::Map::eObjectKind::Structure_Nexus) ||
                    structure.team != losingTeam)
                {
                    return;
                }
                const bool_t bBound =
                    world.HasComponent<ServerIdComponent>(e) &&
                    world.GetComponent<ServerIdComponent>(e).serverEntityId != 0u;
                if (burstNexus == NULL_ENTITY || (bBound && !bBurstNexusBound))
                {
                    burstNexus = e;
                    bBurstNexusBound = bBound;
                }
            });
        if (burstNexus != NULL_ENTITY)
        {
            FxCueContext fx{};
            fx.vWorldPos =
                world.GetComponent<TransformComponent>(burstNexus).GetPosition();
            fx.pFxMeshRenderer = m_pFxMeshRenderer;
            std::vector<EntityID> spawned;
            CFxCuePlayer::PlayAll(
                world,
                losingTeam == eTeam::Blue
                    ? "Structure.Destruction.Blue"
                    : "Structure.Destruction.Red",
                fx,
                &spawned);
            for (EntityID fxEntity : spawned)
                m_timelineVisualEntities.push_back(world.GetEntityHandle(fxEntity));
        }
        return;
    }

    if (effectId == kEzrealEffectArcaneShiftBlink)
    {
        const Vec3 origin{ ev->posX(), ev->posY(), ev->posZ() };
        const Vec3 delta{ ev->dirX(), ev->dirY(), ev->dirZ() };
        const Vec3 dest{ origin.x + delta.x, origin.y + delta.y, origin.z + delta.z };
        const f32_t lifetime = ev->durationMs() > 0
            ? static_cast<f32_t>(ev->durationMs()) / 1000.f
            : 0.4f;
        Ezreal::Fx::SpawnEFlash(world, origin, dest, lifetime);
        return;
    }

    if (effectId == kEzrealEffectEssenceFluxMark ||
        effectId == kEzrealEffectEssenceFluxDetonate ||
        effectId == kEzrealEffectEssenceFluxClear)
    {
        const u64_t cueKey = BuildCueKey(
            ev->effectId(),
            ev->sourceNet(),
            ev->targetNet(),
            ev->startTick(),
            ev->flags());
        if (m_seenEffectCueKeys.size() > 4096u)
            m_seenEffectCueKeys.clear();
        const bool_t bFirstCue = m_seenEffectCueKeys.insert(cueKey).second;

        const u64_t relationKey =
            (static_cast<u64_t>(ev->sourceNet()) << 32u) |
            static_cast<u64_t>(ev->targetNet());
        const ePresentationMutationPhase ePhase =
            effectId == kEzrealEffectEssenceFluxMark
                ? ePresentationMutationPhase::SpawnOrMark
                : ePresentationMutationPhase::ContactOrClear;
        const bool_t bApplyRelationMutation = TryAdvanceMutation(
            m_ezrealFluxMutationStamps[relationKey],
            MakeEventMutation(
                uServerTick,
                uEventOrdinal,
                ePhase));

        EntityID target = NULL_ENTITY;
        if (ev->targetNet() != NULL_NET_ENTITY)
            target = ResolveLiveEntity(world, entityMap, ev->targetNet());
        Vec3 pos{ ev->posX(), ev->posY(), ev->posZ() };
        if (target != NULL_ENTITY && world.HasComponent<TransformComponent>(target))
            pos = world.GetComponent<TransformComponent>(target).GetPosition();

        if (effectId == kEzrealEffectEssenceFluxMark)
        {
            if (!bApplyRelationMutation || target == NULL_ENTITY)
                return;

            const u64_t uStartTick = ev->startTick() != 0u
                ? ev->startTick()
                : uServerTick;
            const u64_t uDurationTicks = ev->durationMs() > 0u
                ? (static_cast<u64_t>(ev->durationMs()) * 30u + 999u) / 1000u
                : 120u;
            const u64_t uExpireTick = uStartTick + uDurationTicks;
            const auto expireIt = m_ezrealFluxExpireTicks.find(relationKey);
            if (expireIt != m_ezrealFluxExpireTicks.end() &&
                expireIt->second == uExpireTick &&
                m_ezrealFluxVisualEntities.find(relationKey) !=
                    m_ezrealFluxVisualEntities.end())
            {
                return;
            }

            DestroyEzrealFluxVisuals(world, relationKey);
            FxCueContext fx{};
            fx.vWorldPos = pos;
            fx.vForward = { 0.f, 0.f, 1.f };
            fx.attachTo = target;
            fx.fLifetimeOverride = ev->durationMs() > 0u
                ? static_cast<f32_t>(ev->durationMs()) / 1000.f
                : 4.f;
            fx.bOverrideLifetime = true;
            fx.pFxMeshRenderer = m_pFxMeshRenderer;

            std::vector<EntityID> spawned;
            CFxCuePlayer::PlayAll(world, "Ezreal.W.Mark", fx, &spawned);
            m_ezrealFluxVisualEntities[relationKey] = std::move(spawned);
            m_ezrealFluxExpireTicks[relationKey] = uExpireTick;
        }
        else if (effectId == kEzrealEffectEssenceFluxDetonate)
        {
            if (bApplyRelationMutation)
            {
                DestroyEzrealFluxVisuals(world, relationKey);
                m_ezrealFluxExpireTicks.erase(relationKey);
            }
            if (bFirstCue)
            {
                FxCueContext fx{};
                fx.vWorldPos = pos;
                fx.vForward = { 0.f, 0.f, 1.f };
                fx.attachTo = target;
                fx.pFxMeshRenderer = m_pFxMeshRenderer;
                std::vector<EntityID> spawned;
                CFxCuePlayer::PlayAll(
                    world, "Ezreal.W.Detonate", fx, &spawned);
                for (EntityID entity : spawned)
                    m_timelineVisualEntities.push_back(
                        world.GetEntityHandle(entity));
            }
        }
        else if (bApplyRelationMutation)
        {
            DestroyEzrealFluxVisuals(world, relationKey);
            m_ezrealFluxExpireTicks.erase(relationKey);
        }
        return;
    }

    GameplayFeedback::WorldTextFeedbackKind worldTextKind =
        GameplayFeedback::WorldTextFeedbackKind::None;
    if (GameplayFeedback::TryResolveWorldTextEffectId(effectId, worldTextKind))
    {
        EntityID attachTo = NULL_ENTITY;
        if (ev->targetNet() != NULL_NET_ENTITY)
            attachTo = entityMap.FromNet(ev->targetNet());
        if (attachTo == NULL_ENTITY && ev->sourceNet() != NULL_NET_ENTITY)
            attachTo = entityMap.FromNet(ev->sourceNet());

        Vec3 pos{ ev->posX(), ev->posY(), ev->posZ() };
        if (attachTo != NULL_ENTITY && world.HasComponent<TransformComponent>(attachTo))
            pos = world.GetComponent<TransformComponent>(attachTo).GetPosition();
        pos.y += 2.75f;

        const f32_t lifetime = ev->durationMs() > 0
            ? static_cast<f32_t>(ev->durationMs()) / 1000.f
            : 0.7f;

        if (worldTextKind == GameplayFeedback::WorldTextFeedbackKind::Gold)
        {
            const u32_t goldAmount =
                GameplayFeedback::UnpackWorldTextGoldAmount(ev->flags());
            CGameInstance::Get()->UI_Push_GoldText(pos, goldAmount, lifetime);
            return;
        }

        const FeedbackLabel* pLabel = FindFeedbackLabel(worldTextKind);
        if (!pLabel || !pLabel->pText)
            return;

        CGameInstance::Get()->UI_Push_WorldText(
            pos,
            pLabel->pText,
            pLabel->vColor,
            lifetime);
        return;
    }

    const u8_t eventSlot = (ev->flags() & 0x00ffu) != 0u
        ? SlotFromEffectFlags(ev->flags())
        : SlotFromHookId(effectId);
    const eChampion hookChampion = ChampionFromHookId(effectId);
    const u8_t hookSlot = SlotFromHookIdOrFallback(effectId, eventSlot);
    const u8_t skillStage = StageFromEffectFlags(ev->flags());
    const bool_t bKeepEventPosition =
        ShouldKeepEffectEventPosition(hookChampion, hookSlot);

    EntityID attachTo = NULL_ENTITY;
    if (ev->targetNet() != NULL_NET_ENTITY)
        attachTo = ResolveLiveEntity(world, entityMap, ev->targetNet());
    if (attachTo == NULL_ENTITY && ev->sourceNet() != NULL_NET_ENTITY)
        attachTo = ResolveLiveEntity(world, entityMap, ev->sourceNet());

    Vec3 pos{ ev->posX(), ev->posY(), ev->posZ() };
    if (!bKeepEventPosition &&
        attachTo != NULL_ENTITY &&
        world.HasComponent<TransformComponent>(attachTo))
    {
        pos = world.GetComponent<TransformComponent>(attachTo).GetPosition();
    }

    const u64_t cueKey = BuildCueKey(
        ev->effectId(),
        ev->sourceNet(),
        ev->targetNet(),
        ev->startTick(),
        ev->flags());

    if (m_seenEffectCueKeys.size() > 4096u)
        m_seenEffectCueKeys.clear();

    if (!m_seenEffectCueKeys.insert(cueKey).second)
        return;

    const EntityID source = ev->sourceNet() != NULL_NET_ENTITY
        ? ResolveLiveEntity(world, entityMap, ev->sourceNet())
        : NULL_ENTITY;

    if (eventSlot == static_cast<u8_t>(eSkillSlot::BasicAttack) &&
        source != NULL_ENTITY)
    {
        const bool_t bAlreadyDrivenByAction =
            world.HasComponent<ReplicatedActionComponent>(source) &&
            IsOneShotReplicatedAction(
                world.GetComponent<ReplicatedActionComponent>(source).actionId);
        if (!bAlreadyDrivenByAction)
        {
            PlayReplicatedActionVisual(
                world,
                source,
                static_cast<u16_t>(eReplicatedActionId::BasicAttack),
                1u);
        }
    }

    if (effectId != 0)
    {
        const EntityID target = ev->targetNet() != NULL_NET_ENTITY
            ? ResolveLiveEntity(world, entityMap, ev->targetNet())
            : NULL_ENTITY;

        const u8_t slot = hookSlot;

        CastSkillCommand command{};
        command.slot = slot;
        command.targetEntityId = target;
        command.groundPos = Vec3{ ev->posX(), ev->posY(), ev->posZ() };
        command.direction = WintersMath::Normalize3D(
            Vec3{ ev->dirX(), ev->dirY(), ev->dirZ() });

        const SkillDef* pDef = FindSkillDefForVisualHook(
            world,
            source,
            hookChampion,
            slot);
        if (pDef)
        {
            command.resolvedTargetMode = static_cast<u8_t>(
                (skillStage >= 2u && pDef->stageCount >= 2)
                    ? pDef->stage2TargetMode
                    : pDef->targetMode);
        }

        VisualHookContext ctx{};
        ctx.pWorld = &world;
        ctx.casterEntity = source;
        ctx.pDef = pDef;
        ctx.pCommand = &command;
        ctx.skillStage = skillStage;
        ctx.fEffectLifetimeSec = ev->durationMs() > 0u
            ? static_cast<f32_t>(ev->durationMs()) / 1000.f
            : 0.f;
        ctx.pFxMeshRenderer = m_pFxMeshRenderer;
        ctx.bAuthoritativeEvent = true;

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
        {
            static u32_t s_ireliaCueTraceCount = 0;
            if (s_ireliaCueTraceCount < 64u)
            {
                char msg[320]{};
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
            }
        }
#endif

        if (bVisualHandled)
            return;
    }

    if (const char* pszCueName = ResolveEffectTriggerCue(hookChampion, hookSlot, skillStage))
    {
        FxCueContext fx{};
        fx.vWorldPos = pos;
        fx.vForward = WintersMath::Normalize3D(Vec3{ ev->dirX(), ev->dirY(), ev->dirZ() });
        fx.attachTo = bKeepEventPosition ? NULL_ENTITY : attachTo;
        fx.pFxMeshRenderer = m_pFxMeshRenderer;
        std::vector<EntityID> spawned;
        CFxCuePlayer::PlayAll(world, pszCueName, fx, &spawned);
        for (EntityID entity : spawned)
            m_timelineVisualEntities.push_back(world.GetEntityHandle(entity));
    }
}

void CEventApplier::ApplyDamage(
    CWorld& world,
    EntityIdMap& entityMap,
    const Shared::Schema::DamageEvent* ev)
{
    if (!ev || ev->targetNet() == NULL_NET_ENTITY)
        return;

    const EntityID target = ResolveLiveEntity(world, entityMap, ev->targetNet());
    if (target == NULL_ENTITY)
        return;

    const EntityID source = ev->sourceNet() != NULL_NET_ENTITY
        ? ResolveLiveEntity(world, entityMap, ev->sourceNet())
        : NULL_ENTITY;

    Vec3 pos{};
    if (world.HasComponent<TransformComponent>(target))
        pos = world.GetComponent<TransformComponent>(target).GetPosition();

    Vec3 damageTextPos = pos;
    damageTextPos.y += 2.1f;
    const bool_t bShowCriticalIndicator =
        ev->bWasCrit() ||
        (ev->flags() & DamageFlag_ShowCriticalIndicator) != 0u;
    CGameInstance::Get()->UI_Push_DamageNumber(
        damageTextPos,
        ev->amount(),
        ev->type(),
        ev->bWasCrit(),
        ev->bKilled(),
        bShowCriticalIndicator);

    if (ev->bKilled() && IsMinionEntity(world, target))
    {
        const EntityID localEntity = FindLocalPlayerEntity(world);
        if (source != NULL_ENTITY && source == localEntity)
            CGameInstance::Get()->UI_RecordMatchContextUnitKill();
    }

}

void CEventApplier::ApplyKillFeed(
    CWorld& world,
    EntityIdMap& entityMap,
    const Shared::Schema::KillFeedEvent* ev,
    u64_t serverTick)
{
    if (!ev)
        return;

    const u8_t objectKind = static_cast<u8_t>(ev->objectKind());
    const u32_t packedPresentation =
        (static_cast<u32_t>(ev->sourceChampion()) << 24) |
        (static_cast<u32_t>(ev->targetChampion()) << 16) |
        (static_cast<u32_t>(ev->sourceTeam()) << 8) |
        static_cast<u32_t>(ev->targetTeam());
    const u64_t killFeedKey = BuildCueKey(
        ev->sourceNet(),
        ev->targetNet(),
        objectKind,
        serverTick,
        packedPresentation);

    if (m_seenKillFeedKeys.size() > 4096u)
        m_seenKillFeedKeys.clear();

    if (!m_seenKillFeedKeys.insert(killFeedKey).second)
        return;

    const eTeam localTeam = ResolveLocalTeamForKillFeed(world);
    const eTeam sourceTeam = TeamFromWire(ev->sourceTeam());
    const eTeam targetTeam = TeamFromWire(ev->targetTeam());
    const bool_t bSourceAlly = sourceTeam == localTeam;
    const bool_t bTargetAlly = targetTeam == localTeam;
    const EntityID sourceEntity = ev->sourceNet() != NULL_NET_ENTITY
        ? ResolveLiveEntity(world, entityMap, ev->sourceNet())
        : NULL_ENTITY;
    const bool_t bSourceMinion = IsMinionEntity(world, sourceEntity);

    // 포탑/억제기 파괴 순간 연출 — 구조물 엔티티는 사망 후에도 ECS 에 남으므로 위치 해석 가능.
    if (objectKind == kKillFeedObjectStructure || objectKind == kKillFeedObjectObjective)
    {
        const EntityID structureEntity =
            ResolveLiveEntity(world, entityMap, ev->targetNet());
        if (structureEntity != NULL_ENTITY &&
            world.HasComponent<TransformComponent>(structureEntity))
        {
            FxCueContext fx{};
            fx.vWorldPos =
                world.GetComponent<TransformComponent>(structureEntity).GetPosition();
            fx.pFxMeshRenderer = m_pFxMeshRenderer;
            std::vector<EntityID> spawned;
            CFxCuePlayer::PlayAll(
                world,
                targetTeam == eTeam::Blue
                    ? "Structure.Destruction.Blue"
                    : "Structure.Destruction.Red",
                fx,
                &spawned);
            for (EntityID fxEntity : spawned)
                m_timelineVisualEntities.push_back(world.GetEntityHandle(fxEntity));
        }
    }

    if (objectKind == kKillFeedObjectActor)
    {
        const EntityID localEntity = FindLocalPlayerEntity(world);
        const NetEntityId localNet = localEntity != NULL_ENTITY
            ? entityMap.ToNet(localEntity)
            : NULL_NET_ENTITY;
        const bool_t bLocalSource =
            localNet != NULL_NET_ENTITY && ev->sourceNet() == localNet;
        const bool_t bLocalTarget =
            localNet != NULL_NET_ENTITY && ev->targetNet() == localNet;

        CGameInstance::Get()->UI_RecordMatchContextActorKill(
            ev->sourceTeam(),
            ev->targetTeam(),
            bLocalSource,
            bLocalTarget);
    }

    const char* pMessage =
        ResolveKillFeedMessage(objectKind, bSourceAlly, bTargetAlly);
    if (!pMessage || pMessage[0] == '\0')
        return;

    CGameInstance::Get()->UI_Push_KillFeedBanner(
        ev->sourceChampion(),
        ev->targetChampion(),
        objectKind,
        ev->targetTeam(),
        bSourceAlly,
        bSourceMinion,
        pMessage);
}

bool_t CEventApplier::PlayReplicatedActionVisual(
    CWorld& world,
    EntityID entity,
    u16_t actionId,
    u8_t actionStage)
{
    if (!world.HasComponent<RenderComponent>(entity) ||
        !world.HasComponent<ChampionComponent>(entity))
    {
        return false;
    }
    auto& render = world.GetComponent<RenderComponent>(entity);
    if (!render.pRenderer)
        return false;
    const auto& champion = world.GetComponent<ChampionComponent>(entity);
    eChampion animationChampion = champion.id;
    u8_t replicatedSourceSlot = SlotFromReplicatedAction(actionId);
    if (world.HasComponent<ReplicatedActionComponent>(entity))
    {
        const auto& action = world.GetComponent<ReplicatedActionComponent>(entity);
        replicatedSourceSlot = action.sourceSlot;
        if (action.sourceChampion != eChampion::NONE &&
            action.sourceChampion != eChampion::END)
        {
            animationChampion = action.sourceChampion;
        }
    }
    const u8_t actionSlot = SlotFromReplicatedAction(actionId);
    if (animationChampion == champion.id &&
        world.HasComponent<FormOverrideComponent>(entity))
    {
        const auto& form = world.GetComponent<FormOverrideComponent>(entity);
        if (form.bActive &&
            form.skillChampion != eChampion::END &&
            form.skillChampion != eChampion::NONE &&
            actionSlot < 8u &&
            (form.skillSlotMask & static_cast<u8_t>(1u << actionSlot)) != 0u)
        {
            animationChampion = form.skillChampion;
        }
        else if (form.bActive &&
            form.visualChampion != eChampion::END &&
            form.visualChampion != eChampion::NONE &&
            actionId != static_cast<u16_t>(eReplicatedActionId::SkillR))
        {
            animationChampion = form.visualChampion;
        }
    }
    if (actionId == static_cast<u16_t>(eReplicatedActionId::ViegoConsumeSoul))
        animationChampion = eChampion::VIEGO;

    const ChampionDef* cd = FindClientChampionDefForEvent(animationChampion);
    if (!cd)
        return false;
    std::string animName;
    const auto id = static_cast<eReplicatedActionId>(actionId);
    switch (id)
    {
    case eReplicatedActionId::Recall:
        animName = ResolveRecallAnimName(*cd, *render.pRenderer);
        break;
    case eReplicatedActionId::BasicAttack:
    {
        const SkillDef* def = CSkillRegistry::Instance().Find(
            animationChampion,
            static_cast<u8_t>(eSkillSlot::BasicAttack));
        if (!def)
        {
            def = FindSkillDef(
                animationChampion,
                static_cast<u8_t>(eSkillSlot::BasicAttack));
        }
        const char* pAnimKey = actionStage >= 2u && def && def->stage2AnimKey
            ? def->stage2AnimKey
            : cd->basicAttackKey;
        animName = PrefixAnim(*cd, pAnimKey);
        break;
    }
    case eReplicatedActionId::DeathStart:
        animName = PrefixAnim(*cd, "death");
        break;
    case eReplicatedActionId::SkillQ:
    case eReplicatedActionId::SkillW:
    case eReplicatedActionId::SkillE:
    case eReplicatedActionId::SkillR:
    {
        const u8_t slot = SlotFromReplicatedAction(actionId);
        const SkillDef* def = CSkillRegistry::Instance().Find(animationChampion, slot);
        if (!def)
            def = FindSkillDef(animationChampion, slot);
        const bool_t bStage2 = actionStage >= 2u;
        const char* pAnimKey = nullptr;
        if (animationChampion == eChampion::YASUO && id == eReplicatedActionId::SkillQ)
        {
            pAnimKey = ResolveYasuoQAnimKey(actionStage);
        }
        else if (animationChampion == eChampion::RIVEN &&
            id == eReplicatedActionId::SkillQ)
        {
            pAnimKey = ResolveRivenQAnimKey(actionStage);
        }
        else if (def)
        {
            pAnimKey = (bStage2 && def->stage2AnimKey)
                ? def->stage2AnimKey
                : def->animKey;
        }
        if (pAnimKey)
            animName = PrefixAnim(*cd, pAnimKey);
        break;
    }
    case eReplicatedActionId::ViegoConsumeSoul:
        animName = PrefixAnim(*cd, "passive_attack");
        break;
    default:
        break;
    }
    if (!animName.empty())
    {
        const bool_t bBasicAttackPresentation =
            UI::IsAttackSpeedPlaybackAction(actionId, replicatedSourceSlot);
        f32_t playSpeed = 1.f;
        const SkillDef* pPresentationDef = nullptr;
        if (id == eReplicatedActionId::BasicAttack ||
            id == eReplicatedActionId::SkillQ ||
            id == eReplicatedActionId::SkillW ||
            id == eReplicatedActionId::SkillE ||
            id == eReplicatedActionId::SkillR)
        {
            pPresentationDef =
                CSkillRegistry::Instance().Find(animationChampion, actionSlot);
            if (!pPresentationDef)
                pPresentationDef = FindSkillDef(animationChampion, actionSlot);
            playSpeed = ResolveReplicatedActionPlaySpeed(
                animationChampion,
                actionSlot,
                actionStage,
                pPresentationDef);

            // BA 모션은 복제된 최종 공속 / canonical base 비율로 재생하고,
            // AttackSpeedLab('8' 키)의 per-entity 시각 보정값을 그 위에 곱한다.
            if (bBasicAttackPresentation)
            {
                const UI::AttackSpeedPlaybackScales scales =
                    UI::ResolveAttackSpeedPlaybackScales(world, entity);
                playSpeed *= scales.fAttackSpeedScale *
                    scales.fAnimCorrectionScale;
            }
        }
        const bool_t bPlayed = render.pRenderer->PlayAnimationByNameAdvanced(
            animName.c_str(),
            ShouldLoopReplicatedAction(
                actionId,
                actionStage,
                pPresentationDef),
            false,
            playSpeed);
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
        return bPlayed;
    }

    return false;
}
