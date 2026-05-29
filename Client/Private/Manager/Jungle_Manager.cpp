#define _CRT_SECURE_NO_WARNINGS
#include "Manager/Jungle_Manager.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/RenderComponent.h"
#include <Windows.h>

namespace //이렇게 const char*로 설정한 것처럼 언리얼, RED Engine의 CName Hashing 전부 써서 String 오버헤드 없애기!
{
    constexpr const char* PATH_BARON  = "Client/Bin/Resource/Texture/Object/Jungle/Baron/baron_textured.wmesh";
    constexpr const char* PATH_DRAGON = "Client/Bin/Resource/Texture/Object/Jungle/Dragon/water/dragon_water_textured.wmesh";
    constexpr const char* PATH_BLUE   = "Client/Bin/Resource/Texture/Object/Jungle/Blue/blue_textured.wmesh";
    constexpr const char* PATH_RED    = "Client/Bin/Resource/Texture/Object/Jungle/Blue/blue_textured.wmesh"; // TODO Red 모델
    constexpr const char* PATH_KRUG   = "Client/Bin/Resource/Texture/Object/Jungle/Krug/krug_textured.wmesh";
    constexpr const char* PATH_GROMP  = "Client/Bin/Resource/Texture/Object/Jungle/Gromp/gromp_textured.wmesh";
    constexpr const char* PATH_WOLF   = "Client/Bin/Resource/Texture/Object/Jungle/Wolf/wolf_textured.wmesh";

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
        case CJungle_Manager::eJungleSub::Wolf:   return 1.0f;
        default:                 return 1.0f;
        }
    }
}

HRESULT CJungle_Manager::Initialize(CWorld* pWorld)
{
    // Phase 5-A 후속: m_pWorld 재바인딩을 guard 앞으로 (Structure_Manager 와 동일 이유).
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
            OutputDebugStringA(msg);
        }
    }
    return S_OK;
}

void CJungle_Manager::Render(const Mat4& matViewProj, const Vec3& vCameraWorld,
    void* pAmbientOcclusionSRV)
{
    if (!m_pWorld) return;
    m_pWorld->ForEach<JungleComponent, RenderComponent, TransformComponent>(
        [&](EntityID, JungleComponent&, RenderComponent& rc, TransformComponent& xform)
        {
            if (!rc.bVisible || !rc.pRenderer) return;
            rc.pRenderer->Update(0.f);
            rc.pRenderer->SetAmbientOcclusionSRV(pAmbientOcclusionSRV);
            rc.pRenderer->UpdateCamera(matViewProj, vCameraWorld);
            rc.pRenderer->UpdateTransform(xform.GetWorldMatrix());
            rc.pRenderer->Render();
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

    //몹별 Hp 결정
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
    ColliderComponent collider{};
    collider.vHalfExtents = { colliderRadius, 2.0f, colliderRadius };
    collider.vOffset = { 0.f, 1.0f, 0.f };
    collider.bIsTrigger = false;
    m_pWorld->AddComponent<ColliderComponent>(id, collider);

    auto& rc = m_pWorld->AddComponent<RenderComponent>(id);
    rc.pRenderer = pRenderer.get();
    rc.bVisible  = (entry.bVisible != 0);
    rc.bAnimated = false;   // ★ 정글 몬스터 정지 상태 — 교전 시작 시 true 로 토글 (향후)

    //Health Bar Data Source
    HealthComponent hp;
    hp.fCurrent = jc.hp;
    hp.fMaximum = jc.maxHp;
    m_pWorld->AddComponent<HealthComponent>(id, hp);

    m_mapRenderers[id] = std::move(pRenderer);
    m_vecEntities.push_back(id);
    m_vecNames.emplace_back(entry.name);
    return id;
}
