#pragma once

#include "Engine_Defines.h"
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "ECS/Entity.h"
#include "ECS/ISystem.h"

#include <memory>

class CWorld;

NS_BEGIN(Engine)

class WINTERS_ENGINE CTurretProjectileSystem final : public ISystem
{
public:
    ~CTurretProjectileSystem() override = default;

    static std::unique_ptr<CTurretProjectileSystem> Create()
    {
        return std::unique_ptr<CTurretProjectileSystem>(new CTurretProjectileSystem());
    }

    // Phase 7 keeps projectile create/destroy out of TurretAI phase 6.
    u32_t GetPhase() const override { return 7; }
    const char* GetName() const override { return "TurretProjectileSystem"; }
    void Execute(CWorld& world, f32_t fTimeDelta) override;

private:
    CTurretProjectileSystem() = default;

    void ApplyDamage(CWorld& world, EntityID sourceEntity, EntityID targetEntity,
        f32_t damage) const;
};

NS_END
