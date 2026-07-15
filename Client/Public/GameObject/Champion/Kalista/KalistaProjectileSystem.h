#pragma once
#include "WintersTypes.h"
#include "WintersMath.h"
#include "ECS/Entity.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include <memory>

class CWorld;

//Q스킬 시전시 창 날리기
namespace Engine
{
    class CFxStaticMeshRenderer;
}

struct KalistaProjectileComponent
{
    Vec3 vWorldPos{};
    Vec3 vDirection{};
    f32_t fSpeed = 30.f;
    f32_t fMaxDist = 11.f;
    f32_t fTravelled = 0.f;
    f32_t fRadius = 0.6f;
    f32_t fDamage = 70.f;
    EntityID ownerEntity = NULL_ENTITY;
    EntityHandle targetHandle = NULL_ENTITY_HANDLE;
    eTeam ownerTeam = eTeam::Neutral;
    bool_t bHasHit = false;
    //hit시 적에게 박힌 창 spawn / Rend stack -> Renderer도 기존 OOP 구조 상속 받는 거
    //오늘 수업 컨벤션 때 적용한 내용 맞지?
    Engine::CFxStaticMeshRenderer* pRenderer = nullptr;
    f32_t fSpearScale = 0.0005f;
    bool_t bApplyRendStack = true; //BA/Q = true / R = false

    // Visual flying spear entity. The projectile entity is collision-only, so
    // this handle lets hit/max-distance cleanup remove the visible mesh.
    EntityID visualEntity = NULL_ENTITY;
};

class CKalistaProjectileSystem final
{
public:
    ~CKalistaProjectileSystem() = default;

    static std::unique_ptr<CKalistaProjectileSystem> Create();

    void Execute(CWorld& world, f32_t dt);

    static EntityID Spawn(CWorld& world,
        const Vec3& vOrigin, const Vec3& vDirection,
        f32_t fSpeed, f32_t fMaxDist, f32_t fRadius, f32_t fDamage,
        EntityID owner, eTeam ownerTeam,
        Engine::CFxStaticMeshRenderer* pRenderer = nullptr,
        f32_t fSpearScale = 0.005f,
        bool_t bApplyRendStack = true,
        EntityID visualEntity = NULL_ENTITY,
        EntityID targetEntity = NULL_ENTITY);

private:
    CKalistaProjectileSystem() = default;
};
