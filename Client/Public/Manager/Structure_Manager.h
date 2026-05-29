#pragma once
#include "Defines.h"
#include "Renderer/ModelRenderer.h"
#include "Map/MapDataFormats.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"   // ← 추가
#include "ECS/World.h"
#include "ECS/Entity.h"
#include <cstdio>
#include <unordered_map>

class CStructure_Manager final
{
    DECLARE_SINGLETON(CStructure_Manager)
    CStructure_Manager() = default;

public:
    ~CStructure_Manager() = default;

    HRESULT Initialize(CWorld* pWorld);
    void    Shutdown();

    HRESULT Save_ToFile(FILE* pFile) const;
    HRESULT Load_FromFile(FILE* pFile);

    void    Render(const Mat4& matViewProj, const Vec3& vCameraWorld = Vec3{},
        void* pAmbientOcclusionSRV = nullptr);

    // 반환 타입 변경: EntityID → i32_t (인덱스. 실패 시 -1)
    i32_t    Add_At(Winters::Map::eObjectKind kind,
        eTeam team,
        Winters::Map::eTurretTier tier,
        Winters::Map::eLane lane,
        const Vec3& vPos,
        const char* pCustomName = nullptr);

    bool_t   Remove_At(uint32_t iIndex);
    void     Clear();
    
    uint32_t       Get_Count() const { return static_cast<uint32_t>(m_vecEntities.size()); }
    EntityID       Get_EntityAt(uint32_t iIndex) const;
    const char* Get_Name(uint32_t iIndex) const;

    // ← Scene_Editor Inspector 용 추가 API
    TransformComponent* Get_Transform(uint32_t iIndex);
    bool_t              Get_Visible(uint32_t iIndex) const;
    void                Set_Visible(uint32_t iIndex, bool_t bVisible);

    EntityID            Find_NetworkBindCandidate(
        Winters::Map::eObjectKind kind,
        eTeam team,
        u32_t subtype,
        const Vec3& vPos,
        f32_t maxDistance) const;

    f32_t Get_DefaultScale() const { return m_fDefaultScale; }
    void  Set_DefaultScale(f32_t s) { m_fDefaultScale = s; }

private:
    static const char* ResolveModelPath(Winters::Map::eObjectKind kind, eTeam team);
    void Make_AutoName(Winters::Map::eObjectKind kind, eTeam team,
        Winters::Map::eTurretTier tier, Winters::Map::eLane lane,
        char* pOutBuf, size_t capacity);

    EntityID Spawn_FromEntry(const Winters::Map::StructureEntry& entry);

    CWorld* m_pWorld = nullptr;
    std::vector<EntityID>                                        m_vecEntities;
    std::unordered_map<EntityID, std::unique_ptr<ModelRenderer>> m_mapRenderers;
    std::vector<std::string>                                     m_vecNames;

    f32_t    m_fDefaultScale = 0.01f;
    uint32_t m_uAutoNumber = 0;
    bool_t   m_bInitialized = false;
};
