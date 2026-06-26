#pragma once

#include "ECS/Entity.h"
#include "ECS/ISystem.h"
#include "WintersTypes.h"

#include <memory>

class CWorld;

namespace GameplayTurret
{
    class CStructureProjectileSystem final : public ISystem
    {
    public:
        ~CStructureProjectileSystem() override = default;

        static std::unique_ptr<CStructureProjectileSystem> Create()
        {
            return std::unique_ptr<CStructureProjectileSystem>(new CStructureProjectileSystem());
        }

        u32_t GetPhase() const override { return 7; }
        const char* GetName() const override { return "StructureProjectileSystem"; }
        void Execute(CWorld& world, f32_t fTimeDelta) override;

    private:
        CStructureProjectileSystem() = default;

        void ApplyDamage(CWorld& world, EntityID sourceEntity, EntityID targetEntity,
            f32_t damage) const;
    };
}
