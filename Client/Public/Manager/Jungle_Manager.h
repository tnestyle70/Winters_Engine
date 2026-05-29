#pragma once
#include "Defines.h"
#include "Renderer/ModelRenderer.h"
#include "Map/MapDataFormats.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "ECS/Entity.h"
#include <cstdio>
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>

// CJungle_Manager — Client 싱글턴, ECS 기반 (Phase B-6.7)
//  - ECS 엔티티 생성/소멸 담당. ModelRenderer 소유.
//  - CGameInstance 가 아닌 Client 전용 (Tier 2 외 / ENGINE_DLL 미표기).
//  - Scene_InGame::OnEnter 에서 Initialize(CWorld*) 호출.
class CJungle_Manager final
{
    DECLARE_SINGLETON(CJungle_Manager)
    CJungle_Manager() = default;

public:
    ~CJungle_Manager() = default;

    // 정글 서브 종류. Scene_Editor 가 CJungle_Manager::eJungleSub::Baron 형태로 참조.
    enum class eJungleSub : u32_t
    {
        Baron = 0,
        Dragon = 1,
        BlueBuff = 2,
        RedBuff = 3,
        Krug = 4,
        Gromp = 5,
        Wolf = 6,
    };

    HRESULT Initialize(CWorld* pWorld);
    void    Shutdown();

    HRESULT Save_ToFile(FILE* pFile) const;
    HRESULT Load_FromFile(FILE* pFile);

    void    Render(const Mat4& matViewProj, const Vec3& vCameraWorld = Vec3{},
        void* pAmbientOcclusionSRV = nullptr);

    i32_t    Add_At(eJungleSub sub, u32_t campId, const Vec3& vPos,
        const char* pCustomName = nullptr);  
    bool_t   Remove_At(uint32_t iIndex);
    void     Clear();

    uint32_t    Get_Count()      const { return static_cast<uint32_t>(m_vecEntities.size()); }
    EntityID    Get_EntityAt(uint32_t iIndex) const;
    const char* Get_Name(uint32_t iIndex) const;

    TransformComponent* Get_Transform(uint32_t iIndex);
    bool_t              Get_Visible(uint32_t iIndex) const;
    void                Set_Visible(uint32_t iIndex, bool_t bVisible);

    f32_t Get_DefaultScale() const { return m_fDefaultScale; }
    void  Set_DefaultScale(f32_t s) { m_fDefaultScale = s; }

private:
    static const char* ResolveModelPath(eJungleSub sub);
    void  Make_AutoName(eJungleSub sub, char* pOutBuf, size_t capacity);
    EntityID Spawn_FromEntry(const Winters::Map::JungleEntry& entry);

    CWorld* m_pWorld = nullptr;
    std::vector<EntityID>                                         m_vecEntities;
    std::unordered_map<EntityID, std::unique_ptr<ModelRenderer>>  m_mapRenderers;
    std::vector<std::string>                                      m_vecNames;

    f32_t    m_fDefaultScale = 0.01f;
    uint32_t m_uAutoNumber = 0;
    bool_t   m_bInitialized = false;
};
