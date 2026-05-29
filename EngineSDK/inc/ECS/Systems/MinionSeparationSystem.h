#pragma once

#include "Engine_Defines.h"
#include "WintersAPI.h"
#include "WintersMath.h"
#include "WintersTypes.h"
#include "ECS/Entity.h"
#include "ECS/ISystem.h"

#include <memory>

class CJobSystem;
class CWorld;

NS_BEGIN(Engine)

class WINTERS_ENGINE CMinionSeparationSystem final : public ISystem
{
public:
    ~CMinionSeparationSystem() override = default;

    static std::unique_ptr<CMinionSeparationSystem> Create()
    {
        return std::unique_ptr<CMinionSeparationSystem>(new CMinionSeparationSystem());
    }

    u32_t GetPhase() const override { return 11; }
    const char* GetName() const override { return "MinionSeparationSystem"; }
    void DescribeAccess(CSystemAccessBuilder& builder) const override;
    void Execute(CWorld& world, f32_t fTimeDelta) override;

    void Set_JobSystem(CJobSystem* pJS) { m_pJobSystem = pJS; }

    void Set_Enabled(bool_t b) { m_bEnabled = b; }
    bool_t Get_Enabled() const { return m_bEnabled; }

    void Set_SeparationRadius(f32_t v) { m_fSeparationRadius = (v < 0.f) ? 0.f : v; }
    f32_t Get_SeparationRadius() const { return m_fSeparationRadius; }

    void Set_SeparationWeight(f32_t v) { m_fSeparationWeight = (v < 0.f) ? 0.f : v; }
    f32_t Get_SeparationWeight() const { return m_fSeparationWeight; }

    void Set_MaxNeighbors(i32_t v) { m_iMaxNeighbors = (v < 1) ? 1 : v; }
    i32_t Get_MaxNeighbors() const { return m_iMaxNeighbors; }

private:
    CMinionSeparationSystem() = default;

    f32_t m_fSeparationRadius = 1.0f;
    f32_t m_fSeparationWeight = 0.5f;
    i32_t m_iMaxNeighbors = 8;
    bool_t m_bEnabled = false;

    CJobSystem* m_pJobSystem = nullptr;
};

NS_END
