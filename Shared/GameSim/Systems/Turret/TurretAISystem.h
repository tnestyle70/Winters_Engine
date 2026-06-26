#pragma once

#include "ECS/Entity.h"
#include "ECS/ISystem.h"
#include "WintersTypes.h"

#include <memory>

class CWorld;

namespace GameplayTurret
{
    class CTurretAISystem final : public ISystem
    {
    public:
        ~CTurretAISystem() override = default;

        static std::unique_ptr<CTurretAISystem> Create()
        {
            return std::unique_ptr<CTurretAISystem>(new CTurretAISystem());
        }

        u32_t GetPhase() const override { return 6; }
        const char* GetName() const override { return "TurretAISystem"; }
        void Execute(CWorld& world, f32_t fTimeDelta) override;

    private:
        CTurretAISystem() = default;

        void ConsumeAggroNotifications(CWorld& world);
        void UpdateActivation(CWorld& world);
        void TickTurrets(CWorld& world, f32_t fTimeDelta);
        void ApplyAggro(CWorld& world, EntityID attacker, EntityID victim,
            f32_t priorityDuration);
        EntityID SelectTarget(CWorld& world, EntityID turretEntity) const;
        void SpawnProjectile(CWorld& world, EntityID turretEntity, EntityID targetEntity) const;
        bool_t IsValidTarget(CWorld& world, EntityID entity, u8_t turretTeam) const;
        i32_t TargetPriority(CWorld& world, EntityID entity) const;

        f32_t m_fActivationAccum = 0.f;
        static constexpr f32_t ACTIVATION_INTERVAL = 0.25f;
    };
}
