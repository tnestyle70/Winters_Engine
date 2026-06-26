#pragma once
#include "Defines.h"
#include "Map/MapDataFormats.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "ECS/Entity.h"
#include <cstdio>
#include <string>
#include <vector>

class CBush_Manager final
{
    DECLARE_SINGLETON(CBush_Manager)
    CBush_Manager() = default;

public:
    ~CBush_Manager() = default;

    HRESULT Initialize(CWorld* pWorld);
    void    Shutdown();

    HRESULT Save_ToFile(FILE* pFile) const;
    HRESULT Load_FromFile(FILE* pFile);

    void    RenderEditorOverlay(const Mat4& matViewProj, i32_t selectedIndex) const;

    i32_t   Add_At(
        u32_t bushId,
        const Vec3& vPos,
        f32_t radius,
        const char* pAssetPath = nullptr,
        Winters::Map::eBushRenderKind renderKind = Winters::Map::eBushRenderKind::Billboard,
        const char* pCustomName = nullptr);
    bool_t  Remove_At(u32_t iIndex);
    void    Clear();

    u32_t       Get_Count() const { return static_cast<u32_t>(m_vecEntities.size()); }
    EntityID    Get_EntityAt(u32_t iIndex) const;
    const char* Get_Name(u32_t iIndex) const;

    Vec3    Get_Position(u32_t iIndex) const;
    void    Set_Position(u32_t iIndex, const Vec3& vPos);
    f32_t   Get_Yaw(u32_t iIndex) const;
    void    Set_Yaw(u32_t iIndex, f32_t yaw);
    f32_t   Get_Radius(u32_t iIndex) const;
    void    Set_Radius(u32_t iIndex, f32_t radius);
    u32_t   Get_BushId(u32_t iIndex) const;
    void    Set_BushId(u32_t iIndex, u32_t bushId);
    Winters::Map::eBushRenderKind Get_RenderKind(u32_t iIndex) const;
    void    Set_RenderKind(u32_t iIndex, Winters::Map::eBushRenderKind renderKind);
    void    Get_VisualSize(u32_t iIndex, f32_t& outWidth, f32_t& outHeight, f32_t& outScale) const;
    void    Set_VisualSize(u32_t iIndex, f32_t width, f32_t height, f32_t scale);
    const char* Get_AssetPath(u32_t iIndex) const;
    void    Set_AssetPath(u32_t iIndex, const char* pAssetPath);
    bool_t  Get_Visible(u32_t iIndex) const;
    void    Set_Visible(u32_t iIndex, bool_t bVisible);

    f32_t   Get_DefaultRadius() const { return m_fDefaultRadius; }
    void    Set_DefaultRadius(f32_t radius) { m_fDefaultRadius = radius; }
    f32_t   Get_DefaultWidth() const { return m_fDefaultWidth; }
    void    Set_DefaultWidth(f32_t width) { m_fDefaultWidth = width; }
    f32_t   Get_DefaultHeight() const { return m_fDefaultHeight; }
    void    Set_DefaultHeight(f32_t height) { m_fDefaultHeight = height; }
    const char* Get_DefaultAssetPath() const { return m_strDefaultAssetPath.c_str(); }
    void    Set_DefaultAssetPath(const char* pAssetPath);

private:
    struct BushRuntimeData
    {
        std::string name;
        u32_t bushId = 0;
        Winters::Map::eBushRenderKind renderKind = Winters::Map::eBushRenderKind::Billboard;
        f32_t width = 8.f;
        f32_t height = 4.f;
        f32_t scale = 1.f;
        bool_t bVisible = true;
        std::string assetPath;
    };

    void Make_AutoName(u32_t bushId, char* pOutBuf, size_t capacity);
    EntityID Spawn_FromEntry(const Winters::Map::BushEntry& entry);
    Winters::Map::BushEntry BuildEntry(u32_t iIndex) const;
    void Sync_BushComponents(u32_t iIndex);
    void Sync_FxComponent(u32_t iIndex);

    CWorld* m_pWorld = nullptr;
    std::vector<EntityID> m_vecEntities;
    std::vector<BushRuntimeData> m_vecData;

    f32_t m_fDefaultRadius = 4.6f;
    f32_t m_fDefaultWidth = 8.0f;
    f32_t m_fDefaultHeight = 4.0f;
    std::string m_strDefaultAssetPath =
        "Texture/MAP/output/textures/assets/maps/kitpieces/srx/textures/sru_brush.png";
    u32_t m_uAutoNumber = 0;
    bool_t m_bInitialized = false;
};

