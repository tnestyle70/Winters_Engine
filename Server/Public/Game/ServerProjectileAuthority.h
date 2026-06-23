#pragma once

#include "ECS/Entity.h"
#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/ProjectileKindComponent.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
#include "WintersMath.h"
#include "WintersTypes.h"

class CWorld;
struct SkillProjectileComponent;

class CServerProjectileAuthority final
{
public:
    inline static constexpr u16_t kTurretProjectileKind = 100u;
    inline static constexpr f32_t kMinionRangedProjectileTargetHeight = 0.85f;

    static bool_t IsMinionRangedProjectileKind(eProjectileKind kind);

    static EntityID FindSkillProjectileHitTarget(
        CWorld& world,
        const SkillProjectileComponent& projectile,
        const Vec3& start,
        const Vec3& end,
        Vec3& outHitPos);

    static ReplicatedEventComponent BuildProjectileSpawnEvent(
        EntityID sourceEntity,
        EntityID targetEntity,
        EntityID projectileEntity,
        u16_t projectileKind,
        const Vec3& position,
        const Vec3& direction,
        f32_t speed,
        f32_t maxDistance,
        u64_t startTick);

    static ReplicatedEventComponent BuildProjectileHitEvent(
        EntityID sourceEntity,
        EntityID targetEntity,
        EntityID projectileEntity,
        u16_t projectileKind,
        const Vec3& position,
        u64_t startTick);

    static DamageRequest BuildTurretDamageRequest(
        EntityID sourceEntity,
        EntityID targetEntity,
        eTeam sourceTeam,
        f32_t damage);

    static DamageRequest BuildSkillProjectileDamageRequest(
        const SkillProjectileComponent& projectile,
        EntityID targetEntity,
        eDamageType damageType);

private:
    CServerProjectileAuthority() = delete;
};
