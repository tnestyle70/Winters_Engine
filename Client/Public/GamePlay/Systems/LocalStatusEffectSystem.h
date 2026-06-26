#pragma once

#include "ECS/ISystem.h"
#include "WintersTypes.h"

#include <memory>

class CWorld;

class CLocalStatusEffectSystem final : public ISystem
{
public:
    ~CLocalStatusEffectSystem() override = default;

    static std::unique_ptr<CLocalStatusEffectSystem> Create();
    u32_t GetPhase() const override { return 4; }
    const char* GetName() const override { return "LocalStatusEffectSystem"; }
    void Execute(CWorld& world, f32_t fTimeDelta) override;

private:
    CLocalStatusEffectSystem() = default;
};
