#pragma once
#include "Engine_Defines.h"
#include "WintersAPI.h"   // WINTERS_ENGINE (DLL export/import 매크로)
#include "ECS/ISystem.h"
#include "ECS/Entity.h"       // EntityID 정의 — ProcessAgent 파라미터에 필요 (ISystem.h 는 Entity.h 포함 안 함)
#include <memory>

// CJobSystem 은 Engine/Public/Core/JobSystem.h 에서 전역 네임스페이스에 정의됨.
// NS_BEGIN(Engine) 안에서 forward-decl 하면 Engine::CJobSystem 이라는 별도 타입이
// 생성되어 실제 정의(::CJobSystem)와 링크 불일치. NS_BEGIN 밖에 선언한다.
class CJobSystem;
class CWorld;   // Execute / ProcessAgent 파라미터 (참조만, forward-decl 로 충분)

NS_BEGIN(Engine)

class CNavGrid;

class WINTERS_ENGINE CNavigationSystem final : public ISystem
{
public:
    ~CNavigationSystem() override = default;

    static std::unique_ptr<CNavigationSystem> Create()
    {
        return std::unique_ptr<CNavigationSystem>(new CNavigationSystem());
    }

    // ISystem
    uint32_t    GetPhase()  const override { return 3; }
    const char* GetName()   const override { return "NavigationSystem"; }
    void        DescribeAccess(CSystemAccessBuilder& builder) const override;
    void        Execute(CWorld& world, f32_t fTimeDelta) override;

    // 주입
    void Set_Grid(CNavGrid* pGrid) { m_pGrid = pGrid; }
    void Set_JobSystem(CJobSystem* pJS) { m_pJobSystem = pJS; }

private:
    CNavigationSystem() = default;
    void ProcessAgent(CWorld& world, EntityID id);

    CNavGrid* m_pGrid = nullptr;
    CJobSystem* m_pJobSystem = nullptr;
};

NS_END
