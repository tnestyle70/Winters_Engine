#pragma once

#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "Shared/GameSim/Core/Ecs/ISystem.h"
#include "WintersTypes.h"

#include <memory>

class CWorld;
class CGameRoomIntegrationProbeAccess;

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

        // Chrono Break: 시스템 객체가 보유한 유일한 sim 상태 — keyframe 왕복 대상.
        f32_t GetActivationAccum() const { return m_fActivationAccum; }
        void SetActivationAccum(f32_t fValue) { m_fActivationAccum = fValue; }

    private:
#if defined(_DEBUG)
        friend class ::CGameRoomIntegrationProbeAccess;
#endif
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
