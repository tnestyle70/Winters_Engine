#include "Network/Client/EventApplier.h"
#include "Network/Client/NetworkEventTrace.h"
#include "GameInstance.h"

#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include "Shared/Schemas/Generated/cpp/Event_generated.h"
#include <flatbuffers/flatbuffers.h>
#pragma pop_macro("max")
#pragma pop_macro("min")

#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/RenderComponent.h"
#include "GameObject/ChampionDef.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxCuePlayer.h"
#include "GameObject/FX/FxMeshComponent.h"
#include "GameObject/FX/FxSystem.h"
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
#include "Shared/GameSim/Components/NetAnimationComponent.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
#include "Shared/GameSim/Feedback/GameplayFeedback.h"

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace
{
    constexpr const wchar_t* kTurretTopBeamTexture =
        L"Client/Bin/Resource/Texture/Object/Turret/particles/TurretTopBeam.png";
    constexpr const wchar_t* kEffectTexture = L"Client/Bin/Resource/Texture/FX/Kalista/common_global_indicator_ring_bright.png";
    constexpr const wchar_t* kDamageTexture = L"Client/Bin/Resource/Texture/FX/Kalista/common_color-hit-physical.png";

    // Kill, destroy, and objective event presentation.
    constexpr u8_t kKillFeedObjectChampion = 1;
    constexpr u8_t kKillFeedObjectTurret = 2;
    constexpr u8_t kKillFeedObjectInhibitor = 3;
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
        if (objectKind == kKillFeedObjectChampion)
        {
            if (bSourceAlly)
                return "적을 처치했습니다";
            if (bTargetAlly)
                return "아군이 당했습니다";
            return "적을 처치했습니다";
        }
        if (objectKind == kKillFeedObjectTurret)
        {
            if (bSourceAlly)
                return "포탑을 파괴했습니다";
            return "적이 포탑을 파괴했습니다";
        }
        if (objectKind == kKillFeedObjectInhibitor)
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

    u8_t SlotFromNetAnim(u16_t animId)
    {
        switch (static_cast<eNetAnimId>(animId))
        {
        case eNetAnimId::SkillQ: return static_cast<u8_t>(eSkillSlot::Q);
        case eNetAnimId::SkillW: return static_cast<u8_t>(eSkillSlot::W);
        case eNetAnimId::SkillE: return static_cast<u8_t>(eSkillSlot::E);
        case eNetAnimId::SkillR: return static_cast<u8_t>(eSkillSlot::R);
        default: return static_cast<u8_t>(eSkillSlot::BasicAttack);
        }
    }

    constexpr u16_t kNetAnimFlagLoop = 0x0800u;

    bool_t IsOneShotNetAnim(u16_t animId)
    {
        const auto id = static_cast<eNetAnimId>(animId);
        return id != eNetAnimId::Idle &&
            id != eNetAnimId::Run &&
            id != eNetAnimId::Recall;
    }

    bool_t ShouldLoopNetworkAnimation(u16_t animId, u16_t flags)
    {
        return (flags & kNetAnimFlagLoop) != 0u || !IsOneShotNetAnim(animId);
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

    void SpawnTurretTopBeam(CWorld& world, EntityID ownerEntity, const Vec3& fallbackPos)
    {
        // No .wfx asset exists for this cue yet, so use the runtime billboard path.
        Vec3 pos = fallbackPos;
        EntityID attachTo = NULL_ENTITY;
        if (ownerEntity != NULL_ENTITY && world.HasComponent<TransformComponent>(ownerEntity))
        {
            const Vec3 ownerPos = world.GetComponent<TransformComponent>(ownerEntity).GetPosition();
            pos = { ownerPos.x, ownerPos.y + 2.5f, ownerPos.z };
            attachTo = ownerEntity;
        }

        FxBillboardComponent fx{};
        fx.vWorldPos = pos;
        fx.attachTo = attachTo;
        fx.vAttachOffset = { 0.f, 2.5f, 0.f };
        fx.texturePath = kTurretTopBeamTexture;
        fx.fWidth = 1.8f;
        fx.fHeight = 1.8f;
        fx.fLifetime = 0.22f;
        fx.fFadeOut = 0.14f;
        fx.iAtlasCols = 2;
        fx.iAtlasRows = 2;
        fx.iAtlasFrameCount = 4;
        fx.fAtlasFps = 18.f;
        fx.bAtlasLoop = false;
        fx.vColor = { 1.25f, 1.15f, 0.95f, 1.f };
        fx.blendMode = eBlendPreset::Additive;
        fx.fAlphaClip = 0.02f;
        CFxSystem::Spawn(world, fx);
    }

    u16_t EncodePlaybackRateQ8Local(f32_t playSpeed)
    {
        if (!std::isfinite(playSpeed) || playSpeed <= 0.01f)
            playSpeed = 1.f;

        u32_t encoded = static_cast<u32_t>(playSpeed * 256.f + 0.5f);
        if (encoded < 1u)
            encoded = 1u;
        if (encoded > 4096u)
            encoded = 4096u;
        return static_cast<u16_t>(encoded);
    }
}

std::unique_ptr<CEventApplier> CEventApplier::Create()
{
    return std::unique_ptr<CEventApplier>(new CEventApplier());
}

void CEventApplier::OnEvent(
    CWorld& world,
    EntityIdMap& entityMap,
    const u8_t* payload,
    u32_t len)
{
    if (!payload || len == 0)
        return;

    flatbuffers::Verifier verifier(payload, len);
    if (!Shared::Schema::VerifyEventPacketBuffer(verifier))
    {
        return;
    }

    CNetworkEventTrace::Instance().RecordEventPacket(payload, len);

    const auto* packet = Shared::Schema::GetEventPacket(payload);
    if (!packet)
        return;

    switch (packet->kind())
    {
    case Shared::Schema::EventKind::AnimationStart:
        ApplyAnimationStart(world, entityMap, packet->animation());
        break;
    case Shared::Schema::EventKind::ProjectileSpawn:
        ApplyProjectileSpawn(world, entityMap, packet->projectile(), packet->serverTick());
        break;
    case Shared::Schema::EventKind::ProjectileHit:
        ApplyProjectileHit(world, entityMap, packet->projectileHit());
        break;
    case Shared::Schema::EventKind::EffectTrigger:
        ApplyEffectTrigger(world, entityMap, packet->effect());
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

void CEventApplier::ApplyAnimationStart(
    CWorld& world,
    EntityIdMap& entityMap,
    const Shared::Schema::AnimationStartEvent* ev)
{
    if (!ev || ev->netId() == NULL_NET_ENTITY)
        return;

    const EntityID entity = entityMap.FromNet(ev->netId());
    if (entity == NULL_ENTITY)
        return;

    auto& anim = world.HasComponent<NetAnimationComponent>(entity)
        ? world.GetComponent<NetAnimationComponent>(entity)
        : world.AddComponent<NetAnimationComponent>(entity, NetAnimationComponent{});

    const u32_t previousPlayedSeq = m_lastAnimationSeq[ev->netId()];
    const bool_t bShouldPlay =
        IsNewerActionSeq(ev->actionSeq(), previousPlayedSeq);

    anim.animId = ev->animId();
    anim.animPhaseFrame = 0;
    anim.animStartTick = ev->startTick();
    anim.actionSeq = ev->actionSeq();
    anim.playbackRateQ8 = ev->playbackRateQ8();
    anim.flags = ev->flags();

    if (!bShouldPlay) return;

    m_lastAnimationSeq[ev->netId()] = ev->actionSeq();

    PlayNetworkAnimation(world, entity, ev->animId(), ev->playbackRateQ8(), ev->flags());
}

void CEventApplier::ApplyProjectileSpawn(
    CWorld& world,
    EntityIdMap& entityMap,
    const Shared::Schema::ProjectileSpawnEvent* ev,
    u64_t serverTick)
{
    if (!ev)
        return;

    const u64_t cueKey = BuildCueKey(
        ev->netId(),
        ev->ownerNet(),
        ev->kind(),
        serverTick,
        0u);

    if (m_seenProjectileCueKeys.size() > 4096u)
        m_seenProjectileCueKeys.clear();

    if (!m_seenProjectileCueKeys.insert(cueKey).second)
        return;

    const EntityID ownerEntity = ev->ownerNet() != NULL_NET_ENTITY
        ? entityMap.FromNet(ev->ownerNet())
        : NULL_ENTITY;
    const bool_t bTurretProjectile = ProjectileVisualCatalog::IsTurretProjectileKind(ev->kind());

    const Vec3 pos{ ev->startX(), ev->startY(), ev->startZ() };
    if (bTurretProjectile)
        SpawnTurretTopBeam(world, ownerEntity, pos);
    const Vec3 dir = WintersMath::Normalize3D(Vec3{ ev->dirX(), ev->dirY(), ev->dirZ() });
    const Vec3 velocity{
        dir.x * ev->speed(),
        dir.y * ev->speed(),
        dir.z * ev->speed()
    };

    const f32_t lifetime = (ev->speed() > 0.01f && ev->maxDist() > 0.f)
        ? ev->maxDist() / ev->speed()
        : 1.0f;
    const ProjectileVisualDesc& visual = ProjectileVisualCatalog::Resolve(ev->kind());
    bool_t bPlayedProjectileWfxCue = false;
    if (visual.pszSpawnCue)
    {
        FxCueContext fx{};
        fx.vWorldPos = pos;
        fx.vForward = dir;
        fx.vVelocity = velocity;
        fx.bOverrideVelocity = true;
        fx.fLifetimeOverride = lifetime;
        fx.bOverrideLifetime = true;
        fx.pFxMeshRenderer = m_pFxMeshRenderer;

        std::vector<EntityID> spawnedCueEntities;
        bPlayedProjectileWfxCue =
            CFxCuePlayer::PlayAll(world, visual.pszSpawnCue, fx, &spawnedCueEntities) != NULL_ENTITY;

        if (bTurretProjectile && bPlayedProjectileWfxCue)
        {
            bool_t bSpawnedMesh = false;
            for (EntityID spawned : spawnedCueEntities)
            {
                if (spawned != NULL_ENTITY && world.HasComponent<FxMeshComponent>(spawned))
                {
                    bSpawnedMesh = true;
                    break;
                }
            }

            if (!bSpawnedMesh)
            {
                for (EntityID spawned : spawnedCueEntities)
                {
                    if (spawned != NULL_ENTITY)
                        world.DestroyEntity(spawned);
                }

                bPlayedProjectileWfxCue = false;
            }
        }
    }

    const bool_t bShouldSpawnGenericProjectile =
        !bTurretProjectile &&
        visual.bUseGenericSpawnFallback &&
        visual.pszFallbackSpawnTexture != nullptr &&
        (!visual.pszSpawnCue || !bPlayedProjectileWfxCue);

    if (ev->netId() == NULL_NET_ENTITY)
    {
        if (bShouldSpawnGenericProjectile)
            SpawnBillboard(world, pos, velocity,
                visual.pszFallbackSpawnTexture,
                visual.fFallbackSpawnWidth,
                visual.fFallbackSpawnHeight,
                lifetime);
        return;
    }

    EntityID entity = entityMap.FromNet(ev->netId());
    if (entity == NULL_ENTITY)
    {
        entity = world.CreateEntity();
        entityMap.Bind(ev->netId(), entity);
    }

    if (!world.HasComponent<TransformComponent>(entity))
        world.AddComponent<TransformComponent>(entity, TransformComponent{});
    world.GetComponent<TransformComponent>(entity).SetPosition(pos);
    // The network entity tracks projectile truth; the sprite is a transient visual.
    if (bShouldSpawnGenericProjectile)
        SpawnBillboard(world, pos, velocity,
            visual.pszFallbackSpawnTexture,
            visual.fFallbackSpawnWidth,
            visual.fFallbackSpawnHeight,
            lifetime);
}

void CEventApplier::ApplyProjectileHit(
    CWorld& world,
    EntityIdMap& entityMap,
    const Shared::Schema::ProjectileHitEvent* ev)
{
    if (!ev)
        return;

    const Vec3 pos{ ev->posX(), ev->posY(), ev->posZ() };
    const ProjectileVisualDesc& visual = ProjectileVisualCatalog::Resolve(ev->kind());
    bool_t bPlayedWfxCue = false;
    if (visual.pszHitCue)
    {
        FxCueContext fx{};
        fx.vWorldPos = pos;
        fx.vForward = { 0.f, 0.f, 1.f };
        fx.pFxMeshRenderer = m_pFxMeshRenderer;
        bPlayedWfxCue = CFxCuePlayer::Play(world, visual.pszHitCue, fx) != NULL_ENTITY;
    }

    if (visual.pszAttachedCue)
    {
        EntityID attachTo = NULL_ENTITY;
        if (ev->targetNet() != NULL_NET_ENTITY)
            attachTo = entityMap.FromNet(ev->targetNet());

        if (attachTo != NULL_ENTITY)
        {
            FxCueContext fx{};
            fx.vWorldPos = pos;
            fx.attachTo = attachTo;
            fx.pFxMeshRenderer = m_pFxMeshRenderer;
            bPlayedWfxCue = CFxCuePlayer::Play(world, visual.pszAttachedCue, fx) != NULL_ENTITY || bPlayedWfxCue;
        }
    }

    if (!bPlayedWfxCue &&
        visual.bUseGenericHitFallback &&
        visual.pszFallbackHitTexture)
    {
        SpawnBillboard(world, pos, Vec3{},
            visual.pszFallbackHitTexture,
            visual.fFallbackHitWidth,
            visual.fFallbackHitHeight,
            0.35f);
    }

    if (ev->bDestroyed() && ev->netId() != NULL_NET_ENTITY)
    {
        const EntityID entity = entityMap.FromNet(ev->netId());
        if (entity != NULL_ENTITY)
            world.DestroyEntity(entity);
        entityMap.Unbind(ev->netId());
    }
}

void CEventApplier::ApplyEffectTrigger(
    CWorld& world,
    EntityIdMap& entityMap,
    const Shared::Schema::EffectTriggerEvent* ev)
{
    if (!ev)
        return;

    const u32_t effectId = ev->effectId();
    if (effectId == kGlobalEffectFlashBlink)
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
        attachTo = entityMap.FromNet(ev->targetNet());
    if (attachTo == NULL_ENTITY && ev->sourceNet() != NULL_NET_ENTITY)
        attachTo = entityMap.FromNet(ev->sourceNet());

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
        ? entityMap.FromNet(ev->sourceNet())
        : NULL_ENTITY;

    if (eventSlot == static_cast<u8_t>(eSkillSlot::BasicAttack) &&
        source != NULL_ENTITY)
    {
        const bool_t bAlreadyDrivenByNetAnim =
            world.HasComponent<NetAnimationComponent>(source) &&
            IsOneShotNetAnim(world.GetComponent<NetAnimationComponent>(source).animId);

        if (!bAlreadyDrivenByNetAnim)
        {
            u16_t playbackRate = 256;
            if (world.HasComponent<ChampionComponent>(source))
            {
                const eChampion champion = world.GetComponent<ChampionComponent>(source).id;
                const SkillDef* pDef = CSkillRegistry::Instance().Find(
                    champion,
                    static_cast<u8_t>(eSkillSlot::BasicAttack));
                if (!pDef)
                    pDef = FindSkillDef(
                        champion,
                        static_cast<u8_t>(eSkillSlot::BasicAttack));
                if (pDef)
                    playbackRate = EncodePlaybackRateQ8Local(pDef->animPlaySpeed);
            }

            PlayNetworkAnimation(
                world,
                source,
                static_cast<u16_t>(eNetAnimId::BasicAttack),
                playbackRate,
                ev->flags());
        }
    }

    if (effectId != 0)
    {
        const EntityID target = ev->targetNet() != NULL_NET_ENTITY
            ? entityMap.FromNet(ev->targetNet())
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
        ctx.pFxMeshRenderer = m_pFxMeshRenderer;

        const bool_t bVisualHandled =
            CVisualHookRegistry::Instance().Dispatch(effectId, ctx);

#if defined(_DEBUG)
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
                ++s_ireliaCueTraceCount;
            }
        }
#endif

        if (bVisualHandled)
            return;
    }

    const f32_t lifetime = ev->durationMs() > 0
        ? static_cast<f32_t>(ev->durationMs()) / 1000.f
        : 0.75f;
    if (const char* pszCueName = ResolveEffectTriggerCue(hookChampion, hookSlot, skillStage))
    {
        FxCueContext fx{};
        fx.vWorldPos = pos;
        fx.vForward = WintersMath::Normalize3D(Vec3{ ev->dirX(), ev->dirY(), ev->dirZ() });
        fx.attachTo = bKeepEventPosition ? NULL_ENTITY : attachTo;
        fx.pFxMeshRenderer = m_pFxMeshRenderer;
        // Named champion cues keep per-emitter lifetimes from .wfx assets.
        if (CFxCuePlayer::Play(world, pszCueName, fx) != NULL_ENTITY)
            return;
    }

    SpawnBillboard(
        world,
        pos,
        Vec3{},
        kEffectTexture,
        1.6f,
        1.6f,
        lifetime,
        bKeepEventPosition ? NULL_ENTITY : attachTo);
}

void CEventApplier::ApplyDamage(
    CWorld& world,
    EntityIdMap& entityMap,
    const Shared::Schema::DamageEvent* ev)
{
    if (!ev || ev->targetNet() == NULL_NET_ENTITY)
        return;

    const EntityID target = entityMap.FromNet(ev->targetNet());
    if (target == NULL_ENTITY)
        return;

    const EntityID source = ev->sourceNet() != NULL_NET_ENTITY
        ? entityMap.FromNet(ev->sourceNet())
        : NULL_ENTITY;

    Vec3 pos{};
    if (world.HasComponent<TransformComponent>(target))
        pos = world.GetComponent<TransformComponent>(target).GetPosition();

    Vec3 damageTextPos = pos;
    damageTextPos.y += 2.1f;
    CGameInstance::Get()->UI_Push_DamageNumber(
        damageTextPos,
        ev->amount(),
        ev->type(),
        ev->bWasCrit(),
        ev->bKilled());

    if (ev->bKilled() && IsMinionEntity(world, target))
    {
        const EntityID localEntity = FindLocalPlayerEntity(world);
        if (source != NULL_ENTITY && source == localEntity)
            CGameInstance::Get()->UI_RecordGameContextMinionKill();
    }

    if (!IsMinionEntity(world, source) && !IsMinionEntity(world, target))
        SpawnBillboard(world, pos, Vec3{}, kDamageTexture, 1.0f, 1.0f, 0.25f, target);
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

    if (objectKind == kKillFeedObjectChampion)
    {
        const EntityID localEntity = FindLocalPlayerEntity(world);
        const NetEntityId localNet = localEntity != NULL_ENTITY
            ? entityMap.ToNet(localEntity)
            : NULL_NET_ENTITY;
        const bool_t bLocalSource =
            localNet != NULL_NET_ENTITY && ev->sourceNet() == localNet;
        const bool_t bLocalTarget =
            localNet != NULL_NET_ENTITY && ev->targetNet() == localNet;

        CGameInstance::Get()->UI_RecordGameContextChampionKill(
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
        static_cast<eChampion>(ev->sourceChampion()),
        static_cast<eChampion>(ev->targetChampion()),
        objectKind,
        bSourceAlly,
        pMessage);
}

void CEventApplier::PlayNetworkAnimation(
    CWorld& world,
    EntityID entity,
    u16_t animId,
    u16_t playbackRateQ8,
    u16_t flags)
{
    if (!world.HasComponent<RenderComponent>(entity) ||
        !world.HasComponent<ChampionComponent>(entity))
    {
        return;
    }

    auto& render = world.GetComponent<RenderComponent>(entity);
    if (!render.pRenderer)
        return;

    const auto& champion = world.GetComponent<ChampionComponent>(entity);
    eChampion animationChampion = champion.id;
    const u8_t animSlot = SlotFromNetAnim(animId);
    if (world.HasComponent<FormOverrideComponent>(entity))
    {
        const auto& form = world.GetComponent<FormOverrideComponent>(entity);
        if (form.bActive &&
            form.skillChampion != eChampion::END &&
            form.skillChampion != eChampion::NONE &&
            animSlot < 8u &&
            (form.skillSlotMask & static_cast<u8_t>(1u << animSlot)) != 0u)
        {
            animationChampion = form.skillChampion;
        }
        else if (form.bActive &&
            form.visualChampion != eChampion::END &&
            form.visualChampion != eChampion::NONE &&
            animId != static_cast<u16_t>(eNetAnimId::SkillR))
        {
            animationChampion = form.visualChampion;
        }
    }
    const ChampionDef* cd = FindClientChampionDefForEvent(animationChampion);
    if (!cd)
        return;

    std::string animName;
    const auto id = static_cast<eNetAnimId>(animId);
    switch (id)
    {
    case eNetAnimId::Idle:
        animName = PrefixAnim(*cd, cd->idleAnimKey);
        break;
    case eNetAnimId::Run:
        animName = PrefixAnim(*cd, cd->runAnimKey);
        break;
    case eNetAnimId::Recall:
        animName = ResolveRecallAnimName(*cd, *render.pRenderer);
        break;
    case eNetAnimId::BasicAttack:
        animName = PrefixAnim(*cd, cd->basicAttackKey);
        break;
    case eNetAnimId::Death:
        animName = PrefixAnim(*cd, "death");
        break;
    case eNetAnimId::SkillQ:
    case eNetAnimId::SkillW:
    case eNetAnimId::SkillE:
    case eNetAnimId::SkillR:
    {
        const u8_t slot = SlotFromNetAnim(animId);
        const SkillDef* def = CSkillRegistry::Instance().Find(animationChampion, slot);
        if (!def)
            def = FindSkillDef(animationChampion, slot);
        const u8_t stage = StageFromEffectFlags(flags);
        const bool_t bStage2 = stage >= 2u;
        const char* pAnimKey = nullptr;
        if (animationChampion == eChampion::YASUO && id == eNetAnimId::SkillQ)
        {
            pAnimKey = ResolveYasuoQAnimKey(stage);
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
    // Viego soul animation uses the passive attack clip.
    case eNetAnimId::ViegoConsumeSoul:
        animName = PrefixAnim(*cd, "passive_attack");
        break;
    default:
        break;
    }

    if (!animName.empty())
    {
        const f32_t playSpeed = playbackRateQ8 > 0
            ? static_cast<f32_t>(playbackRateQ8) / 256.f
            : 1.f;
        const bool_t bPlayed = render.pRenderer->PlayAnimationByNameAdvanced(
            animName.c_str(),
            ShouldLoopNetworkAnimation(animId, flags),
            false,
            playSpeed);
#if defined(_DEBUG)
        if (!bPlayed)
        {
            static u32_t s_animMissTraceCount = 0;
            if (s_animMissTraceCount < 64u)
            {
                char msg[192]{};
                sprintf_s(msg,
                    "[NetAnimMiss] champion=%u animId=%u name=%s\n",
                    static_cast<u32_t>(animationChampion),
                    static_cast<u32_t>(animId),
                    animName.c_str());
                OutputDebugStringA(msg);
                ++s_animMissTraceCount;
            }
        }
#endif
    }
}

void CEventApplier::SpawnBillboard(CWorld& world, const Vec3& pos, const Vec3& velocity,
    const wchar_t* texturePath, f32_t width, f32_t height, f32_t lifetime,
    EntityID attachTo)
{
    FxBillboardComponent fx{};
    fx.vWorldPos = pos;
    fx.vVelocity = velocity;
    fx.attachTo = attachTo;
    fx.vAttachOffset = { 0.f, 1.f, 0.f };
    fx.texturePath = texturePath;
    fx.fWidth = width;
    fx.fHeight = height;
    fx.fLifetime = lifetime;
    fx.fFadeOut = lifetime * 0.35f;
    fx.bBillboard = true;
    fx.blendMode = eBlendPreset::AlphaBlend;
    CFxSystem::Spawn(world, fx);
}
