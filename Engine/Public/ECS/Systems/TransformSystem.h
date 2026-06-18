#pragma once
#include "ECS/ISystem.h"
#include "ECS/Entity.h"
#include "WintersMath.h"
#include "WintersAPI.h"    //WINTERS_ENGINE 매크로
#include <memory>
#include <vector>

class CJobSystem;
class CWorld;

#pragma warning(push)
#pragma warning(disable: 4251 4275)

class WINTERS_ENGINE CTransformSystem : public ISystem   // ← 추가
{
public:
    ~CTransformSystem() override = default;

    static std::unique_ptr<CTransformSystem> Create()
    {
        return std::unique_ptr<CTransformSystem>(new CTransformSystem());
    }
    //ISystem
    uint32_t    GetPhase()  const override { return 0; }
    void        Execute(CWorld& world, float fTimeDelta) override;
    const char* GetName()   const override { return "TransformSystem"; }
    void        DescribeAccess(CSystemAccessBuilder& builder) const override;

    //Phase 5-A
    void Set_JobSystem(CJobSystem* pJobSystem) { m_pJobSystem = pJobSystem; }
    void MarkChildrenCacheDirty() { m_bChildrenCacheDirty = true; }

private:
    CTransformSystem() = default;
    static void UpdateEntityRecursive(CWorld& world, EntityID id,
        const Mat4& parentWorld, bool parentWorldDirty);
    //Phase 5-A  모든 TransformComponent 순화하며 m_Parnent 역추적해 각 
    //부모의 m_vecChildren 재빌드
    void RebuildChildrenCache(CWorld& world);

    CJobSystem* m_pJobSystem = nullptr;
    std::vector<EntityID> m_vecRootCache{};
    uint32_t m_uLastEntityCount = 0u;
    bool m_bChildrenCacheDirty = true; // 첫 Execute 때 빌드
};

#pragma warning(pop)
