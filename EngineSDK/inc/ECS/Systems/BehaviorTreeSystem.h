#pragma once

#include "Engine_Defines.h"
#include "WintersAPI.h"
#include "AI/BehaviorTree.h"
#include "ECS/ISystem.h"

#include <memory>
#include <shared_mutex>

class CWorld;

NS_BEGIN(Engine)

struct BotComponent
{
    std::shared_ptr<CBehaviorTree> pBT;
    bool_t bUseRL = false;
    f32_t tickAccumulator = 0.f;
    u8_t difficulty = 1;
};

class WINTERS_ENGINE CBehaviorTreeSystem final : public ISystem
{
public:
    ~CBehaviorTreeSystem() override = default;

    static std::unique_ptr<CBehaviorTreeSystem> Create()
    {
        return std::unique_ptr<CBehaviorTreeSystem>(new CBehaviorTreeSystem());
    }

    // Phase 8 follows TurretProjectile(7). Same phase means parallel in Scheduler.
    u32_t GetPhase() const override { return 8; }
    const char* GetName() const override { return "BehaviorTreeSystem"; }
    void Execute(CWorld& world, f32_t fTimeDelta) override;

private:
    CBehaviorTreeSystem() = default;
    static constexpr f32_t TICK_INTERVAL = 0.2f;
};

NS_END
