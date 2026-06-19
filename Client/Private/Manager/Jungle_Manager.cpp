#define _CRT_SECURE_NO_WARNINGS
#include "Manager/Jungle_Manager.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/RenderComponent.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "Shared/GameSim/Components/ActionStateComponent.h"
#include "Shared/GameSim/Components/PoseStateComponent.h"
#include "Shared/GameSim/Components/ReplicatedStateComponent.h"
#include "Shared/GameSim/Definitions/SnapshotStateFlags.h"
#include "ECS/Components/VisionComponents.h"
#include "ProfilerAPI.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>

namespace //?대젃寃?const char*濡??ㅼ젙??寃껋쿂???몃━?? RED Engine??CName Hashing ?꾨? ?⑥꽌 String ?ㅻ쾭?ㅻ뱶 ?놁븷湲?
{
    constexpr const char* PATH_BARON  = "Client/Bin/Resource/Texture/Object/Jungle/Baron/baron_textured.wmesh";
    constexpr const char* PATH_DRAGON = "Client/Bin/Resource/Texture/Object/Jungle/Dragon/air/dragon_air_textured.wmesh";
    constexpr const char* PATH_BLUE   = "Client/Bin/Resource/Texture/Object/Jungle/Blue/blue_textured.wmesh";
    constexpr const char* PATH_RED    = "Client/Bin/Resource/Texture/Object/Jungle/Red/red_textured.wmesh";
    constexpr const char* PATH_KRUG   = "Client/Bin/Resource/Texture/Object/Jungle/Krug/krug_textured.wmesh";
    constexpr const char* PATH_GROMP  = "Client/Bin/Resource/Texture/Object/Jungle/Gromp/gromp_textured.wmesh";
    constexpr const char* PATH_WOLF   = "Client/Bin/Resource/Texture/Object/Jungle/Wolf/wolf_textured.wmesh";
    constexpr const char* PATH_RAZORBEAK = "Client/Bin/Resource/Texture/Object/Jungle/Razorbeak/razorbeak_textured.wmesh";
    constexpr const char* PATH_RAZORBEAK_MINI = "Client/Bin/Resource/Texture/Object/Jungle/RazorbeakMini/razorbeakmini_textured.wmesh";
    constexpr const char* PATH_WOLF_MINI = "Client/Bin/Resource/Texture/Object/Jungle/WolfMini/wolfmini_textured.wmesh";
    constexpr const char* PATH_KRUG_MINI = "Client/Bin/Resource/Texture/Object/Jungle/KrugMini/krugmini_textured.wmesh";

    constexpr f32_t kJungleBaseAnimUpdateInterval = 1.f / 8.f;
    constexpr f32_t kJungleHighPriorityAnimUpdateInterval = 1.f / 20.f;
    constexpr uint64_t kJungleAnimUpdateBudget = 6u;

    f32_t Resolve_ColliderRadius(CJungle_Manager::eJungleSub sub)
    {
        switch (sub)
        {
        case CJungle_Manager::eJungleSub::Baron:  return 2.5f;
        case CJungle_Manager::eJungleSub::Dragon: return 2.2f;
        case CJungle_Manager::eJungleSub::BlueBuff:
        case CJungle_Manager::eJungleSub::RedBuff:
        case CJungle_Manager::eJungleSub::Gromp:  return 1.2f;
        case CJungle_Manager::eJungleSub::Krug:
        case CJungle_Manager::eJungleSub::Razorbeak:
        case CJungle_Manager::eJungleSub::Wolf:   return 1.0f;
        case CJungle_Manager::eJungleSub::KrugMini:
        case CJungle_Manager::eJungleSub::RazorbeakMini:
        case CJungle_Manager::eJungleSub::WolfMini: return 0.7f;
        default:                 return 1.0f;
        }
    }

    struct JungleAnimationSet
    {
        const char* idle = "";
        const char* run = "";
        const char* attack = "";
        const char* death = "";
    };

    JungleAnimationSet Resolve_JungleAnimations(CJungle_Manager::eJungleSub sub)
    {
        switch (sub)
        {
        case CJungle_Manager::eJungleSub::Baron:
            return { "sru_baron_idle1", "sru_baron_idle1_aggro", "sru_baron_attack1", "sru_baron_death" };
        case CJungle_Manager::eJungleSub::Dragon:
            return { "sru_dragon_flying_run", "sru_dragon_flying_run", "sru_dragon_flying_attack1", "" };
        case CJungle_Manager::eJungleSub::BlueBuff:
            return { "sru_blue_idle_normal", "sru_blue_run", "sru_blue_attack1", "sru_blue_death" };
        case CJungle_Manager::eJungleSub::RedBuff:
            return { "sru_red_idle1", "sru_red_run", "sru_red_attack1", "sru_red_death" };
        case CJungle_Manager::eJungleSub::Krug:
            return { "krug_idle_normal", "krug_run2", "krug_attack1", "krug_death" };
        case CJungle_Manager::eJungleSub::Gromp:
            return { "sru_gromp_idle1", "sru_gromp_run", "sru_gromp_attack1", "sru_gromp_death" };
        case CJungle_Manager::eJungleSub::Wolf:
            return { "sru_murkwolf_idle1", "sru_murkwolf_run", "sru_murkwolf_attack1", "sru_murkwolf_death" };
        case CJungle_Manager::eJungleSub::Razorbeak:
            return { "sru_razorbeak_idle_normal1", "sru_razorbeak_run", "sru_razorbeak_ranged_attack1", "sru_razorbeak_death" };
        case CJungle_Manager::eJungleSub::RazorbeakMini:
            return { "sru_razorbeakmini_idle_normal1", "sru_razorbeakmini_run", "sru_razorbeakmini_attack1", "sru_razorbeakmini_death" };
        case CJungle_Manager::eJungleSub::WolfMini:
            return { "sru_murkwolfmini_idle1", "", "sru_murkwolfmini_howl", "sru_murkwolfmini_death3" };
        case CJungle_Manager::eJungleSub::KrugMini:
            return { "krug_mini_idle_n2ag", "", "", "" };
        default:
            return {};
        }
    }

    const char* Resolve_PlayableAnimation(
        ModelRenderer& renderer,
        const char* primary,
        const char* fallback)
    {
        if (primary && primary[0] && renderer.HasAnimationByName(primary))
            return primary;
        if (fallback && fallback[0] && renderer.HasAnimationByName(fallback))
            return fallback;
        return nullptr;
    }

    const char* Resolve_DefaultAnimationName(CJungle_Manager::eJungleSub sub)
    {
        return Resolve_JungleAnimations(sub).idle;
    }
}

HRESULT CJungle_Manager::Initialize(CWorld* pWorld)
{
    // Phase 5-A ?꾩냽: m_pWorld ?щ컮?몃뵫??guard ?욎쑝濡?(Structure_Manager ? ?숈씪 ?댁쑀).
    m_pWorld = pWorld;
    if (m_bInitialized) return S_OK;
    m_vecEntities.reserve(16);
    m_vecNames.reserve(16);
    m_bInitialized = true;
    return S_OK;
}

void CJungle_Manager::Shutdown()
{
    Clear();
    m_pWorld = nullptr;
    m_bInitialized = false;
}

HRESULT CJungle_Manager::Save_ToFile(FILE* pFile) const
{
    if (!pFile || !m_pWorld) return E_FAIL;
    uint32_t count = static_cast<uint32_t>(m_vecEntities.size());
    fwrite(&count, sizeof(uint32_t), 1, pFile);
    for (size_t i = 0; i < m_vecEntities.size(); ++i)
    {
        EntityID id = m_vecEntities[i];
        if (!m_pWorld->HasComponent<TransformComponent>(id) ||
            !m_pWorld->HasComponent<JungleComponent>(id))
            continue;

        auto& xform = m_pWorld->GetComponent<TransformComponent>(id);
        auto& jc    = m_pWorld->GetComponent<JungleComponent>(id);
        const bool_t bHasRc   = m_pWorld->HasComponent<RenderComponent>(id);
        const bool_t bVisible = bHasRc && m_pWorld->GetComponent<RenderComponent>(id).bVisible;

        Winters::Map::JungleEntry e{};
        strncpy_s(e.name, m_vecNames[i].c_str(), _TRUNCATE);
        e.subKind = jc.subKind;
        e.campId  = jc.campId;
        const Vec3 p = xform.GetPosition();
        const Vec3 r = xform.GetRotation();
        const Vec3 s = xform.GetScale();
        e.px = p.x; e.py = p.y; e.pz = p.z;
        e.rx = r.x; e.ry = r.y; e.rz = r.z;
        e.scale    = s.x;
        e.bVisible = bVisible ? 1u : 0u;
        fwrite(&e, sizeof(Winters::Map::JungleEntry), 1, pFile);
    }
    return S_OK;
}

HRESULT CJungle_Manager::Load_FromFile(FILE* pFile)
{
    if (!pFile || !m_pWorld) return E_FAIL;
    Clear();

    uint32_t count = 0;
    if (fread(&count, sizeof(uint32_t), 1, pFile) != 1) return E_FAIL;
    for (uint32_t i = 0; i < count; ++i)
    {
        Winters::Map::JungleEntry e{};
        if (fread(&e, sizeof(Winters::Map::JungleEntry), 1, pFile) != 1) return E_FAIL;
        if (Spawn_FromEntry(e) == NULL_ENTITY)
        {
            char msg[256];
            sprintf_s(msg, "[Jungle] spawn failed: %s\n", e.name);
        }
    }
    return S_OK;
}

void CJungle_Manager::Update(f32_t dt)
{
    WINTERS_PROFILE_SCOPE("Jungle::Update");

    if (dt <= 0.f) return;

    uint64_t animCount = 0;
    uint64_t skippedCount = 0;
    uint64_t budgetSkippedCount = 0;

    for (auto& it : m_mapRenderers)
    {
        if (!it.second)
            continue;

        Apply_NetworkAnimation(it.first, *it.second, dt);

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
    }

    WINTERS_PROFILE_COUNT("JungleAnim::UpdateCalls", animCount);
    WINTERS_PROFILE_COUNT("JungleAnim::Skipped", skippedCount);
    WINTERS_PROFILE_COUNT("JungleAnim::BudgetSkipped", budgetSkippedCount);
}

void CJungle_Manager::Render(const Mat4& matViewProj, const Vec3& vCameraWorld,
    void* pAmbientOcclusionSRV)
{
    if (!m_pWorld) return;
    m_pWorld->ForEach<JungleComponent, RenderComponent, TransformComponent>(
        [&](EntityID, JungleComponent&, RenderComponent& rc, TransformComponent& xform)
        {
            if (!rc.bVisible || !rc.pRenderer) return;
            rc.pRenderer->SetAmbientOcclusionSRV(pAmbientOcclusionSRV);
            rc.pRenderer->UpdateCamera(matViewProj, vCameraWorld);
            rc.pRenderer->UpdateTransform(xform.GetWorldMatrix());
            rc.pRenderer->RenderFrustumCulled(matViewProj);
        });
}

i32_t CJungle_Manager::Add_At(eJungleSub sub, u32_t campId, const Vec3& vPos,
    const char* pCustomName)
{
    Winters::Map::JungleEntry e{};
    e.subKind = static_cast<u32_t>(sub);
    e.campId = campId;
    e.px = vPos.x; e.py = vPos.y; e.pz = vPos.z;
    e.scale = m_fDefaultScale;
    e.bVisible = 1;

    if (pCustomName && pCustomName[0])
        strncpy_s(e.name, pCustomName, _TRUNCATE);
    else
        Make_AutoName(sub, e.name, sizeof(e.name));

    if (Spawn_FromEntry(e) == NULL_ENTITY) return -1;
    return static_cast<i32_t>(m_vecEntities.size() - 1);
}

bool_t CJungle_Manager::Remove_At(uint32_t iIndex)
{
    if (!m_pWorld || iIndex >= m_vecEntities.size()) return false;
    EntityID id = m_vecEntities[iIndex];
    m_mapRenderers.erase(id);
    m_mapVisualStates.erase(id);
    m_pWorld->DestroyEntity(id);
    m_vecEntities.erase(m_vecEntities.begin() + iIndex);
    m_vecNames.erase(m_vecNames.begin() + iIndex);
    return true;
}

void CJungle_Manager::Clear()
{
    if (m_pWorld)
    {
        for (auto id : m_vecEntities) m_pWorld->DestroyEntity(id);
    }
    m_vecEntities.clear();
    m_vecNames.clear();
    m_mapRenderers.clear();
    m_mapVisualStates.clear();
    m_uAutoNumber = 0;
}

EntityID CJungle_Manager::Get_EntityAt(uint32_t iIndex) const
{
    return (iIndex < m_vecEntities.size()) ? m_vecEntities[iIndex] : NULL_ENTITY;
}

const char* CJungle_Manager::Get_Name(uint32_t iIndex) const
{
    return (iIndex < m_vecNames.size()) ? m_vecNames[iIndex].c_str() : nullptr;
}

TransformComponent* CJungle_Manager::Get_Transform(uint32_t iIndex)
{
    if (!m_pWorld || iIndex >= m_vecEntities.size()) return nullptr;
    const EntityID id = m_vecEntities[iIndex];
    if (!m_pWorld->HasComponent<TransformComponent>(id)) return nullptr;
    return &m_pWorld->GetComponent<TransformComponent>(id);
}

bool_t CJungle_Manager::Get_Visible(uint32_t iIndex) const
{
    if (!m_pWorld || iIndex >= m_vecEntities.size()) return false;
    const EntityID id = m_vecEntities[iIndex];
    if (!m_pWorld->HasComponent<RenderComponent>(id)) return false;
    return m_pWorld->GetComponent<RenderComponent>(id).bVisible;
}

void CJungle_Manager::Set_Visible(uint32_t iIndex, bool_t bVisible)
{
    if (!m_pWorld || iIndex >= m_vecEntities.size()) return;
    const EntityID id = m_vecEntities[iIndex];
    if (!m_pWorld->HasComponent<RenderComponent>(id)) return;
    m_pWorld->GetComponent<RenderComponent>(id).bVisible = bVisible;
}

EntityID CJungle_Manager::Find_NetworkBindCandidate(
    eJungleSub sub,
    const Vec3& vPos,
    f32_t maxDistance) const
{
    if (!m_pWorld)
        return NULL_ENTITY;

    const f32_t maxDistSq = maxDistance * maxDistance;
    f32_t bestDistSq = maxDistSq;
    EntityID best = NULL_ENTITY;

    for (EntityID entity : m_vecEntities)
    {
        if (entity == NULL_ENTITY ||
            !m_pWorld->IsAlive(entity) ||
            !m_pWorld->HasComponent<JungleComponent>(entity) ||
            !m_pWorld->HasComponent<TransformComponent>(entity))
        {
            continue;
        }

        if (m_pWorld->HasComponent<ServerIdComponent>(entity) &&
            m_pWorld->GetComponent<ServerIdComponent>(entity).serverEntityId != 0u)
        {
            continue;
        }

        const auto& jungle = m_pWorld->GetComponent<JungleComponent>(entity);
        if (jungle.subKind != static_cast<u32_t>(sub))
            continue;

        const Vec3 pos = m_pWorld->GetComponent<TransformComponent>(entity).GetPosition();
        const f32_t dx = pos.x - vPos.x;
        const f32_t dz = pos.z - vPos.z;
        const f32_t distSq = dx * dx + dz * dz;
        if (distSq <= bestDistSq)
        {
            bestDistSq = distSq;
            best = entity;
        }
    }

    return best;
}

void CJungle_Manager::Apply_NetworkAnimation(
    EntityID entity,
    ModelRenderer& renderer,
    f32_t dt)
{
    if (!m_pWorld || !m_pWorld->HasComponent<JungleComponent>(entity))
        return;

    auto& visual = m_mapVisualStates[entity];
    const auto sub = static_cast<eJungleSub>(
        m_pWorld->GetComponent<JungleComponent>(entity).subKind);
    const JungleAnimationSet anims = Resolve_JungleAnimations(sub);

    const bool_t bDeadByHealth =
        m_pWorld->HasComponent<HealthComponent>(entity) &&
        m_pWorld->GetComponent<HealthComponent>(entity).bIsDead;
    const bool_t bDeadBySnapshot =
        m_pWorld->HasComponent<ReplicatedStateComponent>(entity) &&
        (m_pWorld->GetComponent<ReplicatedStateComponent>(entity).stateFlags &
            kSnapshotStateDeadFlag) != 0u;

    if (bDeadByHealth || bDeadBySnapshot)
    {
        if (!visual.bDead)
        {
            if (const char* pDeath = Resolve_PlayableAnimation(renderer, anims.death, nullptr))
                renderer.PlayAnimationByNameAdvanced(pDeath, false, false, 1.f);
            visual.bDead = true;
            visual.bAction = false;
        }
        return;
    }

    visual.bDead = false;

    const PoseStateComponent* pPose = nullptr;
    if (m_pWorld->HasComponent<PoseStateComponent>(entity))
        pPose = &m_pWorld->GetComponent<PoseStateComponent>(entity);

    const ActionStateComponent* pAction = nullptr;
    if (m_pWorld->HasComponent<ActionStateComponent>(entity))
        pAction = &m_pWorld->GetComponent<ActionStateComponent>(entity);

    const bool_t bMovingBySnapshot =
        m_pWorld->HasComponent<ReplicatedStateComponent>(entity) &&
        (m_pWorld->GetComponent<ReplicatedStateComponent>(entity).stateFlags &
            kSnapshotStateMovingFlag) != 0u;
    const bool_t bMovingByAnim =
        pPose &&
        static_cast<ePoseStateId>(pPose->poseId) == ePoseStateId::Run;

    if (pAction &&
        static_cast<eActionStateId>(pAction->actionId) == eActionStateId::BasicAttack &&
        pAction->sequence != 0u &&
        (visual.lastActionSeq != pAction->sequence ||
            visual.lastActionAnimId != pAction->actionId))
    {
        visual.lastActionSeq = pAction->sequence;
        visual.lastActionAnimId = pAction->actionId;

        if (const char* pAttack = Resolve_PlayableAnimation(renderer, anims.attack, nullptr))
        {
            renderer.PlayAnimationByNameAdvanced(pAttack, false, false, 1.f);
            visual.actionTimer = renderer.GetAnimationDurationSecondsByName(pAttack);
            if (visual.actionTimer <= 0.f)
                visual.actionTimer = 0.75f;
            visual.bAction = true;
            visual.baseAnimId = 0;
            return;
        }
    }

    if (visual.bAction)
    {
        visual.actionTimer -= dt;
        if (visual.actionTimer > 0.f)
            return;

        visual.bAction = false;
        visual.baseAnimId = 0;
    }

    const ePoseStateId basePose = (bMovingBySnapshot || bMovingByAnim)
        ? ePoseStateId::Run
        : ePoseStateId::Idle;
    const u16_t baseAnimId = static_cast<u16_t>(basePose);

    if (visual.baseAnimId != baseAnimId)
    {
        const char* pBase = (basePose == ePoseStateId::Run)
            ? Resolve_PlayableAnimation(renderer, anims.run, anims.idle)
            : Resolve_PlayableAnimation(renderer, anims.idle, nullptr);

        if (pBase)
            renderer.PlayAnimationByNameAdvanced(pBase, true, false, 1.f);

        visual.baseAnimId = baseAnimId;
    }
}

const char* CJungle_Manager::ResolveModelPath(eJungleSub sub)
{
    switch (sub)
    {
    case eJungleSub::Baron:    return PATH_BARON;
    case eJungleSub::Dragon:   return PATH_DRAGON;
    case eJungleSub::BlueBuff: return PATH_BLUE;
    case eJungleSub::RedBuff:  return PATH_RED;
    case eJungleSub::Krug:     return PATH_KRUG;
    case eJungleSub::Gromp:    return PATH_GROMP;
    case eJungleSub::Wolf:     return PATH_WOLF;
    case eJungleSub::Razorbeak:     return PATH_RAZORBEAK;
    case eJungleSub::RazorbeakMini: return PATH_RAZORBEAK_MINI;
    case eJungleSub::WolfMini:      return PATH_WOLF_MINI;
    case eJungleSub::KrugMini:      return PATH_KRUG_MINI;
    default: return nullptr;
    }
}

void CJungle_Manager::Make_AutoName(eJungleSub sub, char* pOutBuf, size_t capacity)
{
    const char* s = "Jungle";
    switch (sub)
    {
    case eJungleSub::Baron:    s = "Baron";  break;
    case eJungleSub::Dragon:   s = "Dragon"; break;
    case eJungleSub::BlueBuff: s = "Blue";   break;
    case eJungleSub::RedBuff:  s = "Red";    break;
    case eJungleSub::Krug:     s = "Krug";   break;
    case eJungleSub::Gromp:    s = "Gromp";  break;
    case eJungleSub::Wolf:     s = "Wolf";   break;
    case eJungleSub::Razorbeak:     s = "Razorbeak";     break;
    case eJungleSub::RazorbeakMini: s = "RazorbeakMini"; break;
    case eJungleSub::WolfMini:      s = "WolfMini";      break;
    case eJungleSub::KrugMini:      s = "KrugMini";      break;
    default: break;
    }
    sprintf_s(pOutBuf, capacity, "%s_#%u", s, m_uAutoNumber++);
}

EntityID CJungle_Manager::Spawn_FromEntry(const Winters::Map::JungleEntry& entry)
{
    if (!m_pWorld) return NULL_ENTITY;
    const char* pPath = ResolveModelPath(static_cast<eJungleSub>(entry.subKind));
    if (!pPath) return NULL_ENTITY;

    auto pRenderer = std::unique_ptr<ModelRenderer>(new ModelRenderer());
    if (!pRenderer->Initialize(pPath, L"Shaders/Mesh3D.hlsl"))
        return NULL_ENTITY;

    const char* pDefaultAnim = Resolve_DefaultAnimationName(static_cast<eJungleSub>(entry.subKind));
    if (pDefaultAnim && pDefaultAnim[0] && pRenderer->HasAnimationByName(pDefaultAnim))
        pRenderer->PlayAnimationByName(pDefaultAnim, true);
    else if (pRenderer->GetAnimationCount() > 0)
        pRenderer->PlayAnimation(0);

    if (static_cast<eJungleSub>(entry.subKind) == eJungleSub::Baron)
    {
        pRenderer->LoadMeshTexture(0,
            L"Client/Bin/Resource/Texture/Object/Jungle/Baron/sru_baron_tx_cm.png");
        pRenderer->LoadMeshTexture(1,
            L"Client/Bin/Resource/Texture/Object/Jungle/Baron/sru_baron_tx_cm.png");
    }

    EntityID id = m_pWorld->CreateEntity();

    auto& xform = m_pWorld->AddComponent<TransformComponent>(id);
    xform.SetPosition({ entry.px, entry.py, entry.pz });
    xform.SetRotation({ entry.rx, entry.ry, entry.rz });
    xform.SetScale(entry.scale);

    auto& jc = m_pWorld->AddComponent<JungleComponent>(id);
    jc.subKind = entry.subKind;
    jc.campId  = entry.campId;

    //紐밸퀎 Hp 寃곗젙
    f32_t maxHp = 1500.f;
    if (static_cast<eJungleSub>(entry.subKind) == eJungleSub::Baron)
        maxHp = 8000.f;
    else if (static_cast<eJungleSub>(entry.subKind) == eJungleSub::Dragon)
        maxHp = 5000.f;

    jc.hp = maxHp;
    jc.maxHp = maxHp;

    m_pWorld->AddComponent<JungleMonsterTag>(id);
    m_pWorld->AddComponent<TargetableTag>(id);

    const f32_t colliderRadius = Resolve_ColliderRadius(static_cast<eJungleSub>(entry.subKind));

    SpatialAgentComponent spatial{};
    spatial.kind = eSpatialKind::JungleMob;
    spatial.team = static_cast<u8_t>(eTeam::Neutral);
    spatial.radius = colliderRadius;
    m_pWorld->AddComponent<SpatialAgentComponent>(id, spatial);
    m_pWorld->AddComponent<VisibilityComponent>(id);

    ColliderComponent collider{};
    collider.vHalfExtents = { colliderRadius, 2.0f, colliderRadius };
    collider.vOffset = { 0.f, 1.0f, 0.f };
    collider.bIsTrigger = false;
    m_pWorld->AddComponent<ColliderComponent>(id, collider);

    auto& rc = m_pWorld->AddComponent<RenderComponent>(id);
    rc.pRenderer = pRenderer.get();
    rc.bVisible  = (entry.bVisible != 0);

    rc.bAnimated = pRenderer->HasSkeleton() && pRenderer->GetAnimationCount() > 0;

    //Health Bar Data Source
    HealthComponent hp;
    hp.fCurrent = jc.hp;
    hp.fMaximum = jc.maxHp;
    m_pWorld->AddComponent<HealthComponent>(id, hp);

    m_mapRenderers[id] = std::move(pRenderer);
    m_mapVisualStates[id] = JungleVisualState{};
    m_vecEntities.push_back(id);
    m_vecNames.emplace_back(entry.name);
    return id;
}
