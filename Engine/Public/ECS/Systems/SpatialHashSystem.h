#pragma once

#include "Engine_Defines.h"
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "ECS/ISystem.h"

#include <memory>

class CWorld;

NS_BEGIN(Engine)

class WINTERS_ENGINE CSpatialHashSystem final : public ISystem
{
public:
    ~CSpatialHashSystem() override = default;

    static std::unique_ptr<CSpatialHashSystem> Create()
    {
        return std::unique_ptr<CSpatialHashSystem>(new CSpatialHashSystem());
    }

    u32_t GetPhase() const override { return 1u; }
    const char* GetName() const override { return "SpatialHashSystem"; }
    void Execute(CWorld& world, f32_t fTimeDelta) override;

private:
    CSpatialHashSystem() = default;
};

NS_END
