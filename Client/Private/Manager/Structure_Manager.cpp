#define _CRT_SECURE_NO_WARNINGS
#include "Manager/Structure_Manager.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/RenderComponent.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/VisionComponents.h"
#include "Scene/RenderVisibilityFilter.h"
#include "ProfilerAPI.h"
#include <Windows.h>
#include "ECS/Components/CoreComponents.h"

namespace
{
    constexpr const char* PATH_NEXUS_BLUE  = "Client/Bin/Resource/Texture/Object/Nexus/nexus_textured.wmesh";
    constexpr const char* PATH_NEXUS_RED   = "Client/Bin/Resource/Texture/Object/Nexus/nexus_red_textured.wmesh";
    constexpr const char* PATH_INHIB_BLUE  = "Client/Bin/Resource/Texture/Object/Inhibitor/inhibitor_textured.wmesh";
    constexpr const char* PATH_INHIB_RED   = "Client/Bin/Resource/Texture/Object/Inhibitor/inhibitor_red_textured.wmesh";
    constexpr const char* PATH_TURRET_BLUE = "Client/Bin/Resource/Texture/Object/Turret/turret_textured.wmesh";
    constexpr const char* PATH_TURRET_RED  = "Client/Bin/Resource/Texture/Object/Turret/turret_red_textured.wmesh";
}

HRESULT CStructure_Manager::Initialize(CWorld* pWorld)
{
    // Phase 5-A ?꾩냽 (2026-04-23): m_pWorld ?щ컮?몃뵫??guard ?욎쑝濡??대룞.
    //  ?댁쑀: CGameApp::OnInit ??Initialize(nullptr) 濡?1李?珥덇린????m_bInitialized=true.
    //       ?댄썑 Scene_InGame::OnEnter ??Initialize(&m_World) 媛 guard ??嫄몃젮
    //       m_pWorld=nullptr ?좎? ??Load_FromFile ??E_FAIL 諛섑솚 ??Stage ?곗씠??濡쒕뱶 ?ㅽ뙣.
    //       Scene ?꾪솚留덈떎 Manager 媛 ?꾩옱 Scene ??World 瑜?諛붾씪蹂대룄濡?留??몄텧 ?щ컮?몃뵫.
    m_pWorld = pWorld;
    if (m_bInitialized) return S_OK;
    m_vecEntities.reserve(32);
    m_vecNames.reserve(32);
    m_bInitialized = true;
    return S_OK;
}

void CStructure_Manager::Shutdown()
{
    Clear();
    m_pWorld = nullptr;
    m_bInitialized = false;
}

HRESULT CStructure_Manager::Save_ToFile(FILE* pFile) const
{
    if (!pFile || !m_pWorld) return E_FAIL;
    uint32_t count = static_cast<uint32_t>(m_vecEntities.size());
    fwrite(&count, sizeof(uint32_t), 1, pFile);
    for (size_t i = 0; i < m_vecEntities.size(); ++i)
    {
        EntityID id = m_vecEntities[i];
        if (!m_pWorld->HasComponent<TransformComponent>(id) ||
            !m_pWorld->HasComponent<StructureComponent>(id))
            continue;

        auto& xform = m_pWorld->GetComponent<TransformComponent>(id);
        auto& sc    = m_pWorld->GetComponent<StructureComponent>(id);
        const bool_t bHasRc   = m_pWorld->HasComponent<RenderComponent>(id);
        const bool_t bVisible = bHasRc && m_pWorld->GetComponent<RenderComponent>(id).bVisible;

        Winters::Map::StructureEntry e{};
        strncpy_s(e.name, m_vecNames[i].c_str(), _TRUNCATE);
        e.subKind = static_cast<u32_t>(sc.kind);
        e.team    = static_cast<u32_t>(sc.team);
        e.tier    = static_cast<u32_t>(sc.tier);
        e.lane    = static_cast<u32_t>(sc.lane);
        const Vec3 p = xform.GetPosition();
        const Vec3 r = xform.GetRotation();
        const Vec3 s = xform.GetScale();
        e.px = p.x; e.py = p.y; e.pz = p.z;
        e.rx = r.x; e.ry = r.y; e.rz = r.z;
        e.scale    = s.x;
        e.bVisible = bVisible ? 1u : 0u;
        fwrite(&e, sizeof(Winters::Map::StructureEntry), 1, pFile);
    }
    return S_OK;
}

HRESULT CStructure_Manager::Load_FromFile(FILE* pFile)
{
    if (!pFile || !m_pWorld) return E_FAIL;
    Clear();

    uint32_t count = 0;
    if (fread(&count, sizeof(uint32_t), 1, pFile) != 1) return E_FAIL;
    for (uint32_t i = 0; i < count; ++i)
    {
        Winters::Map::StructureEntry e{};
        if (fread(&e, sizeof(Winters::Map::StructureEntry), 1, pFile) != 1) return E_FAIL;
        if (Spawn_FromEntry(e) == NULL_ENTITY)
        {
            char msg[256];
            sprintf_s(msg, "[Structure] spawn failed: %s\n", e.name);
        }
    }
    return S_OK;
}

void CStructure_Manager::Render(const Mat4& matViewProj, const Vec3& vCameraWorld,
    void* pAmbientOcclusionSRV,
    bool_t bIgnoreFogOfWar)
{
    if (!m_pWorld) return;
    const u8_t localTeam = UI::QueryLocalTeam(*m_pWorld);
    uint64_t candidateCount = 0;
    uint64_t visibleCount = 0;
    uint64_t fowSkippedCount = 0;

    m_pWorld->ForEach<StructureComponent, RenderComponent, TransformComponent>(
        [&](EntityID id, StructureComponent& structure, RenderComponent& rc, TransformComponent& xform)
        {
            if (!rc.bVisible || !rc.pRenderer)
                return;

            ++candidateCount;
            if (!UI::IsRenderableForLocal(*m_pWorld, id, localTeam, bIgnoreFogOfWar))
            {
                ++fowSkippedCount;
                return;
            }

            ++visibleCount;
            rc.pRenderer->SetAmbientOcclusionSRV(pAmbientOcclusionSRV);
            rc.pRenderer->UpdateCamera(matViewProj, vCameraWorld);
            rc.pRenderer->UpdateTransform(xform.GetWorldMatrix());
            const bool_t bBlueNexus =
                structure.team == eTeam::Blue &&
                structure.kind == static_cast<u32_t>(Winters::Map::eObjectKind::Structure_Nexus);
            if (bBlueNexus)
                rc.pRenderer->Render();
            else
                rc.pRenderer->RenderFrustumCulled(matViewProj);
        });

    WINTERS_PROFILE_COUNT("Structure::Candidates", candidateCount);
    WINTERS_PROFILE_COUNT("Structure::Visible", visibleCount);
    WINTERS_PROFILE_COUNT("Structure::FowSkipped", fowSkippedCount);
}

i32_t CStructure_Manager::Add_At(Winters::Map::eObjectKind kind, eTeam team,
    Winters::Map::eTurretTier tier, Winters::Map::eLane lane,
    const Vec3& vPos, const char* pCustomName)
{
    Winters::Map::StructureEntry e{};
    e.subKind = static_cast<u32_t>(kind);
    e.team = static_cast<u32_t>(team);
    e.tier = static_cast<u32_t>(tier);
    e.lane = static_cast<u32_t>(lane);
    e.px = vPos.x; e.py = vPos.y; e.pz = vPos.z;
    e.rx = 0.f;    e.ry = 0.f;    e.rz = 0.f;
    e.scale = m_fDefaultScale;
    e.bVisible = 1;

    if (pCustomName && pCustomName[0])
        strncpy_s(e.name, pCustomName, _TRUNCATE);
    else
        Make_AutoName(kind, team, tier, lane, e.name, sizeof(e.name));

    if (Spawn_FromEntry(e) == NULL_ENTITY) return -1;
    return static_cast<i32_t>(m_vecEntities.size() - 1);
}

bool_t CStructure_Manager::Remove_At(uint32_t iIndex)
{
    if (!m_pWorld || iIndex >= m_vecEntities.size()) return false;
    EntityID id = m_vecEntities[iIndex];
    m_mapRenderers.erase(id);
    m_pWorld->DestroyEntity(id);
    m_vecEntities.erase(m_vecEntities.begin() + iIndex);
    m_vecNames.erase(m_vecNames.begin() + iIndex);
    return true;
}

void CStructure_Manager::Clear()
{
    if (m_pWorld)
    {
        for (auto id : m_vecEntities) m_pWorld->DestroyEntity(id);
    }
    m_vecEntities.clear();
    m_vecNames.clear();
    m_mapRenderers.clear();
    m_uAutoNumber = 0;
}

EntityID CStructure_Manager::Get_EntityAt(uint32_t iIndex) const
{
    return (iIndex < m_vecEntities.size()) ? m_vecEntities[iIndex] : NULL_ENTITY;
}

const char* CStructure_Manager::Get_Name(uint32_t iIndex) const
{
    return (iIndex < m_vecNames.size()) ? m_vecNames[iIndex].c_str() : nullptr;
}

TransformComponent* CStructure_Manager::Get_Transform(uint32_t iIndex)
{
    if (!m_pWorld || iIndex >= m_vecEntities.size()) return nullptr;
    const EntityID id = m_vecEntities[iIndex];
    if (!m_pWorld->HasComponent<TransformComponent>(id)) return nullptr;
    return &m_pWorld->GetComponent<TransformComponent>(id);
}

bool_t CStructure_Manager::Get_Visible(uint32_t iIndex) const
{
    if (!m_pWorld || iIndex >= m_vecEntities.size()) return false;
    const EntityID id = m_vecEntities[iIndex];
    if (!m_pWorld->HasComponent<RenderComponent>(id)) return false;
    return m_pWorld->GetComponent<RenderComponent>(id).bVisible;
}

void CStructure_Manager::Set_Visible(uint32_t iIndex, bool_t bVisible)
{
    if (!m_pWorld || iIndex >= m_vecEntities.size()) return;
    const EntityID id = m_vecEntities[iIndex];
    if (!m_pWorld->HasComponent<RenderComponent>(id)) return;
    m_pWorld->GetComponent<RenderComponent>(id).bVisible = bVisible;
}

EntityID CStructure_Manager::Find_NetworkBindCandidate(
    Winters::Map::eObjectKind kind,
    eTeam team,
    u32_t subtype,
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
            !m_pWorld->HasComponent<StructureComponent>(entity) ||
            !m_pWorld->HasComponent<TransformComponent>(entity))
        {
            continue;
        }

        if (m_pWorld->HasComponent<ServerIdComponent>(entity) &&
            m_pWorld->GetComponent<ServerIdComponent>(entity).serverEntityId != 0u)
        {
            continue;
        }

        const auto& structure = m_pWorld->GetComponent<StructureComponent>(entity);
        if (structure.kind != static_cast<u32_t>(kind) ||
            structure.team != team)
        {
            continue;
        }

        if (kind == Winters::Map::eObjectKind::Structure_Turret &&
            structure.tier != subtype)
        {
            continue;
        }

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

const char* CStructure_Manager::ResolveModelPath(Winters::Map::eObjectKind kind, eTeam team)
{
    const bool_t bBlue = (team == eTeam::Blue);
    switch (kind)
    {
    case Winters::Map::eObjectKind::Structure_Nexus:     return bBlue ? PATH_NEXUS_BLUE  : PATH_NEXUS_RED;
    case Winters::Map::eObjectKind::Structure_Inhibitor: return bBlue ? PATH_INHIB_BLUE  : PATH_INHIB_RED;
    case Winters::Map::eObjectKind::Structure_Turret:    return bBlue ? PATH_TURRET_BLUE : PATH_TURRET_RED;
    default: return nullptr;
    }
}

void CStructure_Manager::Make_AutoName(Winters::Map::eObjectKind kind, eTeam team,
    Winters::Map::eTurretTier tier, Winters::Map::eLane lane,
    char* pOutBuf, size_t capacity)
{
    const char* k = "Struct";
    switch (kind)
    {
    case Winters::Map::eObjectKind::Structure_Nexus:     k = "Nexus";  break;
    case Winters::Map::eObjectKind::Structure_Inhibitor: k = "Inhib";  break;
    case Winters::Map::eObjectKind::Structure_Turret:    k = "Turret"; break;
    default: break;
    }
    const char* t = (team == eTeam::Blue) ? "Blue" : "Red";

    if (kind == Winters::Map::eObjectKind::Structure_Turret)
    {
        const char* pLane = "None";
        switch (lane)
        {
        case Winters::Map::eLane::Top:  pLane = "Top";  break;
        case Winters::Map::eLane::Mid:  pLane = "Mid";  break;
        case Winters::Map::eLane::Bot:  pLane = "Bot";  break;
        case Winters::Map::eLane::Base: pLane = "Base"; break;
        default: break;
        }
        const char* pTier = "None";
        switch (tier)
        {
        case Winters::Map::eTurretTier::Outer:     pTier = "Outer"; break;
        case Winters::Map::eTurretTier::Inner:     pTier = "Inner"; break;
        case Winters::Map::eTurretTier::Inhibitor: pTier = "Inhib"; break;
        case Winters::Map::eTurretTier::Nexus:     pTier = "Nexus"; break;
        default: break;
        }
        sprintf_s(pOutBuf, capacity, "%s_%s_%s_%s_#%u",
            k, t, pLane, pTier, m_uAutoNumber++);
    }
    else
    {
        sprintf_s(pOutBuf, capacity, "%s_%s_#%u", k, t, m_uAutoNumber++);
    }
}

EntityID CStructure_Manager::Spawn_FromEntry(const Winters::Map::StructureEntry& entry)
{
    if (!m_pWorld) return NULL_ENTITY;
    const char* pPath = ResolveModelPath(static_cast<Winters::Map::eObjectKind>(entry.subKind),
        static_cast<eTeam>(entry.team));
    if (!pPath) return NULL_ENTITY;

    auto pRenderer = std::unique_ptr<ModelRenderer>(new ModelRenderer());
    if (!pRenderer->Initialize(pPath, L"Shaders/Mesh3D.hlsl"))
        return NULL_ENTITY;


    EntityID id = m_pWorld->CreateEntity();

    auto& xform = m_pWorld->AddComponent<TransformComponent>(id);
    xform.SetPosition({ entry.px, entry.py, entry.pz });
    xform.SetRotation({ entry.rx, entry.ry, entry.rz });
    xform.SetScale(entry.scale);

    auto& sc = m_pWorld->AddComponent<StructureComponent>(id);
    sc.team = static_cast<eTeam>(entry.team);
    sc.kind = entry.subKind;
    sc.tier = entry.tier;
    sc.lane = entry.lane;

    f32_t maxHp = 3000.f;
    if (sc.kind == static_cast<uint32_t>(Winters::Map::eObjectKind::Structure_Nexus))
        maxHp = 5500.f;
    else if (sc.kind == static_cast<uint32_t>(Winters::Map::eObjectKind::Structure_Inhibitor))
        maxHp = 4000.f;

    sc.hp = maxHp;
    sc.maxHp = maxHp;


    if (sc.kind == static_cast<uint32_t>(Winters::Map::eObjectKind::Structure_Turret))
    {
        TurretComponent tc{};
        tc.team = sc.team;
        tc.hp = 3000.f;
        tc.maxHp = 3000.f;
        tc.laneType = static_cast<uint8_t>(sc.lane);
        tc.tier = static_cast<uint8_t>(sc.tier);
        m_pWorld->AddComponent<TurretComponent>(id, tc);

        TurretAIComponent ai{};
        ai.attackRange = 7.75f;
        ai.attackCooldownMax = 1.0f;
        ai.attackDamage = (tc.tier == static_cast<uint8_t>(Winters::Map::eTurretTier::Nexus))
            ? 180.f : 150.f;
        ai.projectileSpeed = 18.f;
        m_pWorld->AddComponent<TurretAIComponent>(id, ai);

        VisionSourceComponent vision{};
        vision.sightRange = 12.f;
        vision.bTrueSight = true;
        m_pWorld->AddComponent<VisionSourceComponent>(id, vision);

        m_pWorld->AddComponent<VisibilityComponent>(id);
        m_pWorld->AddComponent<TargetableTag>(id);
    }
    else if (sc.kind == static_cast<uint32_t>(Winters::Map::eObjectKind::Structure_Nexus))
    {
        m_pWorld->AddComponent<NexusTag>(id);
        m_pWorld->AddComponent<VisibilityComponent>(id);
    }
    else if (sc.kind == static_cast<uint32_t>(Winters::Map::eObjectKind::Structure_Inhibitor))
    {
        m_pWorld->AddComponent<InhibitorTag>(id);
        m_pWorld->AddComponent<VisibilityComponent>(id);
    }

    SpatialAgentComponent spatial{};
    spatial.team = static_cast<u8_t>(sc.team);
    spatial.radius = 1.5f;
    if (sc.kind == static_cast<uint32_t>(Winters::Map::eObjectKind::Structure_Turret))
        spatial.kind = eSpatialKind::Turret;
    else if (sc.kind == static_cast<uint32_t>(Winters::Map::eObjectKind::Structure_Nexus))
        spatial.kind = eSpatialKind::Nexus;
    else if (sc.kind == static_cast<uint32_t>(Winters::Map::eObjectKind::Structure_Inhibitor))
        spatial.kind = eSpatialKind::Inhibitor;
    m_pWorld->AddComponent<SpatialAgentComponent>(id, spatial);

    ColliderComponent collider{};
    collider.vHalfExtents = { spatial.radius, 3.0f, spatial.radius };
    collider.vOffset = { 0.f, 1.5f, 0.f };
    collider.bIsTrigger = false;
    m_pWorld->AddComponent<ColliderComponent>(id, collider);

    auto& rc = m_pWorld->AddComponent<RenderComponent>(id);
    rc.pRenderer = pRenderer.get();
    rc.bVisible  = (entry.bVisible != 0);
    rc.bAnimated = false;   // ??援ъ“臾?(Turret/Inhib/Nexus) ? ?뺤쟻 ??AnimUpdate ?ㅽ궢

    //HealthBar Data Source 
    HealthComponent hp;
    hp.fCurrent = sc.hp;
    hp.fMaximum = sc.maxHp;
    m_pWorld->AddComponent<HealthComponent>(id, hp);

    m_mapRenderers[id] = std::move(pRenderer);
    m_vecEntities.push_back(id);
    m_vecNames.emplace_back(entry.name);
    return id;
}
