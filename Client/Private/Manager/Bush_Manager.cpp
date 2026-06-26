#define _CRT_SECURE_NO_WARNINGS
#include "Manager/Bush_Manager.h"
#include "ECS/Components/VisionComponents.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "FX/FxAsset.h"

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

#include <DirectXMath.h>
#include <algorithm>
#include <cmath>

namespace
{
    constexpr f32_t kBushFxLifetimeSeconds = 360000.f;

    std::wstring WidenAsciiPath(const std::string& path)
    {
        std::wstring out;
        out.reserve(path.size());
        for (const char ch : path)
            out.push_back(static_cast<wchar_t>(static_cast<unsigned char>(ch)));
        return out;
    }

    bool_t WorldToScreen(const Mat4& matViewProj, const Vec3& world, ImVec2& out)
    {
        const DirectX::XMMATRIX mVP = matViewProj.ToXMMATRIX();
        DirectX::XMVECTOR v = DirectX::XMVectorSet(world.x, world.y, world.z, 1.f);
        v = DirectX::XMVector4Transform(v, mVP);

        const f32_t wComp = DirectX::XMVectorGetW(v);
        if (wComp <= 0.01f)
            return false;

        const f32_t nx = DirectX::XMVectorGetX(v) / wComp;
        const f32_t ny = DirectX::XMVectorGetY(v) / wComp;
        out.x = (nx * 0.5f + 0.5f) * static_cast<f32_t>(g_iWinSizeX);
        out.y = (1.f - (ny * 0.5f + 0.5f)) * static_cast<f32_t>(g_iWinSizeY);
        return true;
    }
}

HRESULT CBush_Manager::Initialize(CWorld* pWorld)
{
    m_pWorld = pWorld;
    if (m_bInitialized)
        return S_OK;

    m_vecEntities.reserve(64);
    m_vecData.reserve(64);
    m_bInitialized = true;
    return S_OK;
}

void CBush_Manager::Shutdown()
{
    Clear();
    m_pWorld = nullptr;
    m_bInitialized = false;
}

HRESULT CBush_Manager::Save_ToFile(FILE* pFile) const
{
    if (!pFile || !m_pWorld)
        return E_FAIL;

    const u32_t count = static_cast<u32_t>(m_vecEntities.size());
    fwrite(&count, sizeof(u32_t), 1, pFile);

    for (u32_t i = 0; i < count; ++i)
    {
        const Winters::Map::BushEntry entry = BuildEntry(i);
        fwrite(&entry, sizeof(Winters::Map::BushEntry), 1, pFile);
    }

    return S_OK;
}

HRESULT CBush_Manager::Load_FromFile(FILE* pFile)
{
    if (!pFile || !m_pWorld)
        return E_FAIL;

    Clear();

    u32_t count = 0;
    if (fread(&count, sizeof(u32_t), 1, pFile) != 1)
        return E_FAIL;

    for (u32_t i = 0; i < count; ++i)
    {
        Winters::Map::BushEntry entry{};
        if (fread(&entry, sizeof(Winters::Map::BushEntry), 1, pFile) != 1)
            return E_FAIL;

        Spawn_FromEntry(entry);
    }

    return S_OK;
}

void CBush_Manager::RenderEditorOverlay(const Mat4& matViewProj, i32_t selectedIndex) const
{
    if (!m_pWorld)
        return;

    ImDrawList* pDraw = ImGui::GetBackgroundDrawList();
    constexpr i32_t kSegments = 32;

    for (u32_t i = 0; i < static_cast<u32_t>(m_vecEntities.size()); ++i)
    {
        const EntityID entity = m_vecEntities[i];
        if (!m_pWorld->IsAlive(entity) ||
            !m_pWorld->HasComponent<TransformComponent>(entity) ||
            !m_pWorld->HasComponent<ConcealmentVolumeComponent>(entity))
        {
            continue;
        }

        const Vec3 center = m_pWorld->GetComponent<TransformComponent>(entity).GetPosition();
        const f32_t radius = m_pWorld->GetComponent<ConcealmentVolumeComponent>(entity).radius;
        const bool_t bSelected = selectedIndex == static_cast<i32_t>(i);
        const ImU32 color = bSelected
            ? IM_COL32(255, 230, 80, 245)
            : IM_COL32(70, 210, 120, 210);
        const ImU32 fill = bSelected
            ? IM_COL32(255, 230, 80, 55)
            : IM_COL32(70, 210, 120, 35);

        ImVec2 points[kSegments]{};
        i32_t visibleCount = 0;
        for (i32_t s = 0; s < kSegments; ++s)
        {
            const f32_t t = (static_cast<f32_t>(s) / static_cast<f32_t>(kSegments)) *
                DirectX::XM_2PI;
            const Vec3 p{
                center.x + std::cos(t) * radius,
                center.y + 0.08f,
                center.z + std::sin(t) * radius
            };

            if (WorldToScreen(matViewProj, p, points[s]))
                ++visibleCount;
        }

        if (visibleCount == kSegments)
        {
            pDraw->AddConvexPolyFilled(points, kSegments, fill);
            pDraw->AddPolyline(points, kSegments, color, ImDrawFlags_Closed, bSelected ? 3.f : 2.f);
        }

        ImVec2 screen{};
        if (WorldToScreen(matViewProj, { center.x, center.y + 0.15f, center.z }, screen))
        {
            pDraw->AddCircleFilled(screen, bSelected ? 6.f : 4.f, color);
            const char* name = Get_Name(i);
            if (name && bSelected)
                pDraw->AddText(ImVec2(screen.x + 8.f, screen.y - 9.f), IM_COL32_WHITE, name);
        }
    }
}

i32_t CBush_Manager::Add_At(
    u32_t bushId,
    const Vec3& vPos,
    f32_t radius,
    const char* pAssetPath,
    Winters::Map::eBushRenderKind renderKind,
    const char* pCustomName)
{
    Winters::Map::BushEntry entry{};
    entry.bushId = bushId;
    entry.renderKind = static_cast<u32_t>(renderKind);
    entry.px = vPos.x;
    entry.py = vPos.y;
    entry.pz = vPos.z;
    entry.yaw = 0.f;
    entry.radius = (radius > 0.f) ? radius : m_fDefaultRadius;
    entry.width = m_fDefaultWidth;
    entry.height = m_fDefaultHeight;
    entry.scale = 1.f;
    entry.bVisible = 1u;
    strncpy_s(entry.assetPath,
        (pAssetPath && pAssetPath[0]) ? pAssetPath : m_strDefaultAssetPath.c_str(),
        _TRUNCATE);

    if (pCustomName && pCustomName[0])
        strncpy_s(entry.name, pCustomName, _TRUNCATE);
    else
        Make_AutoName(bushId, entry.name, sizeof(entry.name));

    if (Spawn_FromEntry(entry) == NULL_ENTITY)
        return -1;

    return static_cast<i32_t>(m_vecEntities.size() - 1);
}

bool_t CBush_Manager::Remove_At(u32_t iIndex)
{
    if (!m_pWorld || iIndex >= m_vecEntities.size())
        return false;

    const EntityID entity = m_vecEntities[iIndex];
    if (m_pWorld->IsAlive(entity))
        m_pWorld->DestroyEntity(entity);

    m_vecEntities.erase(m_vecEntities.begin() + iIndex);
    m_vecData.erase(m_vecData.begin() + iIndex);
    return true;
}

void CBush_Manager::Clear()
{
    if (m_pWorld)
    {
        for (const EntityID entity : m_vecEntities)
        {
            if (m_pWorld->IsAlive(entity))
                m_pWorld->DestroyEntity(entity);
        }
    }

    m_vecEntities.clear();
    m_vecData.clear();
    m_uAutoNumber = 0;
}

EntityID CBush_Manager::Get_EntityAt(u32_t iIndex) const
{
    return (iIndex < m_vecEntities.size()) ? m_vecEntities[iIndex] : NULL_ENTITY;
}

const char* CBush_Manager::Get_Name(u32_t iIndex) const
{
    return (iIndex < m_vecData.size()) ? m_vecData[iIndex].name.c_str() : nullptr;
}

Vec3 CBush_Manager::Get_Position(u32_t iIndex) const
{
    if (!m_pWorld || iIndex >= m_vecEntities.size())
        return {};

    const EntityID entity = m_vecEntities[iIndex];
    if (!m_pWorld->IsAlive(entity) || !m_pWorld->HasComponent<TransformComponent>(entity))
        return {};

    return m_pWorld->GetComponent<TransformComponent>(entity).GetPosition();
}

void CBush_Manager::Set_Position(u32_t iIndex, const Vec3& vPos)
{
    if (!m_pWorld || iIndex >= m_vecEntities.size())
        return;

    const EntityID entity = m_vecEntities[iIndex];
    if (m_pWorld->HasComponent<TransformComponent>(entity))
        m_pWorld->GetComponent<TransformComponent>(entity).SetPosition(vPos);

    Sync_BushComponents(iIndex);
    Sync_FxComponent(iIndex);
}

f32_t CBush_Manager::Get_Yaw(u32_t iIndex) const
{
    if (!m_pWorld || iIndex >= m_vecEntities.size())
        return 0.f;

    const EntityID entity = m_vecEntities[iIndex];
    if (!m_pWorld->HasComponent<TransformComponent>(entity))
        return 0.f;

    return m_pWorld->GetComponent<TransformComponent>(entity).GetRotation().y;
}

void CBush_Manager::Set_Yaw(u32_t iIndex, f32_t yaw)
{
    if (!m_pWorld || iIndex >= m_vecEntities.size())
        return;

    const EntityID entity = m_vecEntities[iIndex];
    if (m_pWorld->HasComponent<TransformComponent>(entity))
    {
        Vec3 rot = m_pWorld->GetComponent<TransformComponent>(entity).GetRotation();
        rot.y = yaw;
        m_pWorld->GetComponent<TransformComponent>(entity).SetRotation(rot);
    }

    Sync_FxComponent(iIndex);
}

f32_t CBush_Manager::Get_Radius(u32_t iIndex) const
{
    if (!m_pWorld || iIndex >= m_vecEntities.size())
        return 0.f;

    const EntityID entity = m_vecEntities[iIndex];
    if (!m_pWorld->HasComponent<ConcealmentVolumeComponent>(entity))
        return 0.f;

    return m_pWorld->GetComponent<ConcealmentVolumeComponent>(entity).radius;
}

void CBush_Manager::Set_Radius(u32_t iIndex, f32_t radius)
{
    if (!m_pWorld || iIndex >= m_vecEntities.size())
        return;

    const EntityID entity = m_vecEntities[iIndex];
    if (m_pWorld->HasComponent<ConcealmentVolumeComponent>(entity))
        m_pWorld->GetComponent<ConcealmentVolumeComponent>(entity).radius = (std::max)(0.1f, radius);
}

u32_t CBush_Manager::Get_BushId(u32_t iIndex) const
{
    return (iIndex < m_vecData.size()) ? m_vecData[iIndex].bushId : 0u;
}

void CBush_Manager::Set_BushId(u32_t iIndex, u32_t bushId)
{
    if (iIndex >= m_vecData.size())
        return;

    m_vecData[iIndex].bushId = bushId;
    Sync_BushComponents(iIndex);
}

Winters::Map::eBushRenderKind CBush_Manager::Get_RenderKind(u32_t iIndex) const
{
    return (iIndex < m_vecData.size())
        ? m_vecData[iIndex].renderKind
        : Winters::Map::eBushRenderKind::Billboard;
}

void CBush_Manager::Set_RenderKind(u32_t iIndex, Winters::Map::eBushRenderKind renderKind)
{
    if (iIndex >= m_vecData.size())
        return;

    m_vecData[iIndex].renderKind = renderKind;
    Sync_FxComponent(iIndex);
}

void CBush_Manager::Get_VisualSize(u32_t iIndex, f32_t& outWidth, f32_t& outHeight, f32_t& outScale) const
{
    outWidth = 0.f;
    outHeight = 0.f;
    outScale = 1.f;
    if (iIndex >= m_vecData.size())
        return;

    outWidth = m_vecData[iIndex].width;
    outHeight = m_vecData[iIndex].height;
    outScale = m_vecData[iIndex].scale;
}

void CBush_Manager::Set_VisualSize(u32_t iIndex, f32_t width, f32_t height, f32_t scale)
{
    if (iIndex >= m_vecData.size())
        return;

    m_vecData[iIndex].width = (std::max)(0.1f, width);
    m_vecData[iIndex].height = (std::max)(0.1f, height);
    m_vecData[iIndex].scale = (std::max)(0.01f, scale);
    Sync_FxComponent(iIndex);
}

const char* CBush_Manager::Get_AssetPath(u32_t iIndex) const
{
    return (iIndex < m_vecData.size()) ? m_vecData[iIndex].assetPath.c_str() : "";
}

void CBush_Manager::Set_AssetPath(u32_t iIndex, const char* pAssetPath)
{
    if (iIndex >= m_vecData.size())
        return;

    m_vecData[iIndex].assetPath = pAssetPath ? pAssetPath : "";
    Sync_FxComponent(iIndex);
}

bool_t CBush_Manager::Get_Visible(u32_t iIndex) const
{
    return (iIndex < m_vecData.size()) ? m_vecData[iIndex].bVisible : false;
}

void CBush_Manager::Set_Visible(u32_t iIndex, bool_t bVisible)
{
    if (iIndex >= m_vecData.size())
        return;

    m_vecData[iIndex].bVisible = bVisible;
    Sync_FxComponent(iIndex);
}

void CBush_Manager::Set_DefaultAssetPath(const char* pAssetPath)
{
    m_strDefaultAssetPath = pAssetPath ? pAssetPath : "";
}

void CBush_Manager::Make_AutoName(u32_t bushId, char* pOutBuf, size_t capacity)
{
    sprintf_s(pOutBuf, capacity, "Bush_%u_#%u", bushId, m_uAutoNumber++);
}

EntityID CBush_Manager::Spawn_FromEntry(const Winters::Map::BushEntry& entry)
{
    if (!m_pWorld)
        return NULL_ENTITY;

    const EntityID entity = m_pWorld->CreateEntity();

    auto& transform = m_pWorld->AddComponent<TransformComponent>(entity);
    transform.SetPosition({ entry.px, entry.py, entry.pz });
    transform.SetRotation({ 0.f, entry.yaw, 0.f });
    transform.SetScale(entry.scale);

    ConcealmentVolumeComponent volume{};
    volume.center = { entry.px, entry.py, entry.pz };
    volume.radius = (std::max)(0.1f, entry.radius);
    volume.volumeId = entry.bushId;
    m_pWorld->AddComponent<ConcealmentVolumeComponent>(entity, volume);

    BushRuntimeData data{};
    data.name = entry.name[0] ? entry.name : "Bush";
    data.bushId = entry.bushId;
    data.renderKind = static_cast<Winters::Map::eBushRenderKind>(entry.renderKind);
    data.width = (entry.width > 0.f) ? entry.width : m_fDefaultWidth;
    data.height = (entry.height > 0.f) ? entry.height : m_fDefaultHeight;
    data.scale = (entry.scale > 0.f) ? entry.scale : 1.f;
    data.bVisible = entry.bVisible != 0u;
    data.assetPath = entry.assetPath;

    m_vecEntities.push_back(entity);
    m_vecData.push_back(std::move(data));
    Sync_FxComponent(static_cast<u32_t>(m_vecEntities.size() - 1));
    return entity;
}

Winters::Map::BushEntry CBush_Manager::BuildEntry(u32_t iIndex) const
{
    Winters::Map::BushEntry entry{};
    if (!m_pWorld || iIndex >= m_vecEntities.size() || iIndex >= m_vecData.size())
        return entry;

    const EntityID entity = m_vecEntities[iIndex];
    const BushRuntimeData& data = m_vecData[iIndex];

    strncpy_s(entry.name, data.name.c_str(), _TRUNCATE);
    entry.bushId = data.bushId;
    entry.renderKind = static_cast<u32_t>(data.renderKind);
    entry.width = data.width;
    entry.height = data.height;
    entry.scale = data.scale;
    entry.bVisible = data.bVisible ? 1u : 0u;
    strncpy_s(entry.assetPath, data.assetPath.c_str(), _TRUNCATE);

    if (m_pWorld->HasComponent<TransformComponent>(entity))
    {
        const TransformComponent& transform = m_pWorld->GetComponent<TransformComponent>(entity);
        const Vec3 pos = transform.GetPosition();
        const Vec3 rot = transform.GetRotation();
        entry.px = pos.x;
        entry.py = pos.y;
        entry.pz = pos.z;
        entry.yaw = rot.y;
    }

    if (m_pWorld->HasComponent<ConcealmentVolumeComponent>(entity))
        entry.radius = m_pWorld->GetComponent<ConcealmentVolumeComponent>(entity).radius;

    return entry;
}

void CBush_Manager::Sync_BushComponents(u32_t iIndex)
{
    if (!m_pWorld || iIndex >= m_vecEntities.size() || iIndex >= m_vecData.size())
        return;

    const EntityID entity = m_vecEntities[iIndex];
    if (!m_pWorld->HasComponent<ConcealmentVolumeComponent>(entity))
        return;

    ConcealmentVolumeComponent& volume = m_pWorld->GetComponent<ConcealmentVolumeComponent>(entity);
    if (m_pWorld->HasComponent<TransformComponent>(entity))
        volume.center = m_pWorld->GetComponent<TransformComponent>(entity).GetPosition();
    volume.volumeId = m_vecData[iIndex].bushId;
}

void CBush_Manager::Sync_FxComponent(u32_t iIndex)
{
    if (!m_pWorld || iIndex >= m_vecEntities.size() || iIndex >= m_vecData.size())
        return;

    const EntityID entity = m_vecEntities[iIndex];
    if (!m_pWorld->IsAlive(entity))
        return;

    if (!m_pWorld->HasComponent<FxBillboardComponent>(entity))
        m_pWorld->AddComponent<FxBillboardComponent>(entity);

    FxBillboardComponent& fx = m_pWorld->GetComponent<FxBillboardComponent>(entity);
    const BushRuntimeData& data = m_vecData[iIndex];
    const bool_t bBillboard =
        data.renderKind == Winters::Map::eBushRenderKind::Billboard &&
        data.bVisible &&
        !data.assetPath.empty();

    Vec3 pos{};
    f32_t yaw = 0.f;
    if (m_pWorld->HasComponent<TransformComponent>(entity))
    {
        const TransformComponent& transform = m_pWorld->GetComponent<TransformComponent>(entity);
        pos = transform.GetPosition();
        yaw = transform.GetRotation().y;
    }

    fx.vWorldPos = { pos.x, pos.y + data.height * data.scale * 0.5f, pos.z };
    fx.fWidth = data.width * data.scale;
    fx.fHeight = data.height * data.scale;
    fx.fYaw = yaw;
    fx.fLifetime = kBushFxLifetimeSeconds;
    fx.fElapsed = 0.f;
    fx.fFadeIn = 0.f;
    fx.fFadeOut = 0.f;
    fx.vColor = { 1.f, 1.f, 1.f, 1.f };
    fx.renderType = eFxRenderType::Billboard;
    fx.bBillboard = true;
    fx.blendMode = eBlendPreset::AlphaBlend;
    fx.depthMode = eFxDepthMode::DepthTestWriteOn;
    fx.bPendingDelete = false;
    fx.SetTexturePath(bBillboard ? WidenAsciiPath(data.assetPath) : std::wstring{});
    fx.RefreshMaterialFromLegacyFields();
}

