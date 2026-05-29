#pragma once

#include "Engine_Defines.h"
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "ECS/ISystem.h"

#include <memory>

class CWorld;

NS_BEGIN(Engine)

class WINTERS_ENGINE CGameplayCollisionSystem final : public ISystem
{
public:
    ~CGameplayCollisionSystem() override = default;

    static std::unique_ptr<CGameplayCollisionSystem> Create()
    {
        return std::unique_ptr<CGameplayCollisionSystem>(new CGameplayCollisionSystem());
    }

    u32_t GetPhase() const override { return 12; }
    const char* GetName() const override { return "GameplayCollisionSystem"; }
    void DescribeAccess(CSystemAccessBuilder& builder) const override;
    void Execute(CWorld& world, f32_t fTimeDelta) override;

    void Set_Enabled(bool_t) { m_bEnabled = false; }
    bool_t Get_Enabled() const { return false; }

    void Set_Iterations(i32_t v) { m_iIterations = (v < 1) ? 1 : v; }
    i32_t Get_Iterations() const { return m_iIterations; }

    void Set_PushStrength(f32_t) { m_fPushStrength = 0.f; }
    f32_t Get_PushStrength() const { return 0.f; }

private:
    CGameplayCollisionSystem() = default;

    bool_t m_bEnabled = false;
    i32_t m_iIterations = 2;
    f32_t m_fPushStrength = 0.f;
};

NS_END
