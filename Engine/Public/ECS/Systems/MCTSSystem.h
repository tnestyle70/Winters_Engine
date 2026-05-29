#pragma once

#include "Engine_Defines.h"
#include "WintersAPI.h"
#include "AI/MCTSPlanner.h"
#include "ECS/ISystem.h"

#include <memory>

class CWorld;

NS_BEGIN(Engine)

class WINTERS_ENGINE CMCTSSystem final : public ISystem
{
public:
    ~CMCTSSystem() override = default;

    static std::unique_ptr<CMCTSSystem> Create()
    {
        auto p = std::unique_ptr<CMCTSSystem>(new CMCTSSystem());
        p->m_pPlanner = std::make_unique<CMCTSPlanner>();
        return p;
    }

    // Phase 10 keeps MCTS separate from BT(8) and YoneSoul(9).
    u32_t GetPhase() const override { return 10; }
    const char* GetName() const override { return "MCTSSystem"; }
    void Execute(CWorld& world, f32_t fTimeDelta) override;

private:
    CMCTSSystem() = default;

    std::unique_ptr<CMCTSPlanner> m_pPlanner;
    f32_t m_fAccumDt = 0.f;
    static constexpr f32_t TICK_INTERVAL = 5.f;
    static constexpr u32_t ITERATIONS = 50;
};

NS_END
