#include "GameObject/Champion/Yasuo/PendingHitSystem.h"
#include "GameObject/Champion/Yasuo/YasuoProjectileSystem.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "ProfilerAPI.h"
#include <Windows.h>
#include <cstdio>
#include <functional>
#include <vector>

std::unique_ptr<CPendingHitSystem> CPendingHitSystem::Create()
{
    return std::unique_ptr<CPendingHitSystem>(new CPendingHitSystem());
}

EntityID CPendingHitSystem::Schedule(CWorld& world,
    EntityID owner, eTeam ownerTeam,
    const Vec3& vDirection, f32_t fDelay,
    eProjectileKind kind,
    f32_t fSpeed, f32_t fMaxDist, f32_t fRadius,
    f32_t fDamage, f32_t fStunSec)
{
    EntityID entity = world.CreateEntity();

    PendingHitComponent pending{};
    pending.ownerEntity = owner;
    pending.ownerTeam = ownerTeam;
    pending.vDirectionAtCast = vDirection;
    pending.fDelay = fDelay;
    pending.kind = kind;
    pending.fSpeed = fSpeed;
    pending.fMaxDist = fMaxDist;
    pending.fRadius = fRadius;
    pending.fDamage = fDamage;
    pending.fStunSec = fStunSec;
    world.AddComponent<PendingHitComponent>(entity, pending);

    return entity;
}

void CPendingHitSystem::Execute(CWorld& world, f32_t dt)
{
    WINTERS_PROFILE_SCOPE("PendingHit::Execute");

    std::vector<EntityID> vecDelete;

    world.ForEach<PendingHitComponent>(
        std::function<void(EntityID, PendingHitComponent&)>(
            [&](EntityID entity, PendingHitComponent& pending)
            {
                pending.fDelay -= dt;
                if (pending.fDelay > 0.f)
                    return;

                if (pending.ownerEntity != NULL_ENTITY
                    && world.HasComponent<TransformComponent>(pending.ownerEntity))
                {
                    const Vec3 ownerPos =
                        world.GetComponent<TransformComponent>(pending.ownerEntity).m_LocalPosition;

                    CYasuoProjectileSystem::Spawn(world,
                        ownerPos, pending.vDirectionAtCast,
                        (pending.kind == eYasuoProjectileKind::EQRing) ? 0.f : pending.fSpeed,
                        (pending.kind == eYasuoProjectileKind::EQRing) ? 0.5f : pending.fMaxDist,
                        pending.fRadius,
                        pending.fDamage, pending.fStunSec,
                        pending.kind,
                        pending.ownerEntity, pending.ownerTeam);

                    char dbg[160]{};
                    sprintf_s(dbg,
                        "[PendingHit] fire kind=%u owner=%u pos=(%.1f,%.1f,%.1f) dmg=%.1f\n",
                        static_cast<u32_t>(pending.kind),
                        static_cast<u32_t>(pending.ownerEntity),
                        ownerPos.x, ownerPos.y, ownerPos.z,
                        pending.fDamage);
                }

                vecDelete.push_back(entity);
            }));

    for (EntityID entity : vecDelete)
        world.DestroyEntity(entity);
}
