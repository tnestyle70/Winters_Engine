#pragma once
#include "WintersTypes.h"
#include "WintersMath.h"
#include "ECS/Entity.h"
#include <memory>
#include <unordered_set>
#include <cstdint>   // int32_t (namespace Engine 밖이라 i32_t 미사용)

class CWorld;
namespace Engine { class CFxStaticMeshRenderer; }

struct UltWaveComponent
{
    Vec3      vWorldPos{ 0.f, 0.f, 0.f };
    Vec3      vDirection{ 1.f, 0.f, 0.f };
    f32_t     fLength = 12.f;
    f32_t     fWidth = 3.f;
    f32_t     fSpeed = 25.f;
    f32_t     fMaxDist = 15.f;
    f32_t     fTravelled = 0.f;
    f32_t     fWallDuration = 2.5f;
    f32_t     fDamage = 250.f;
    EntityID  ownerEntity = NULL_ENTITY;
    bool      bInWallPhase = false;
    std::unordered_set<uint32_t> hitSet;
    std::unordered_set<uint32_t> bladeHitSet;
    EntityID  pulseFxEntity = NULL_ENTITY;

    bool      bFanSpawned = false;
    Engine::CFxStaticMeshRenderer* pRenderer = nullptr;   // 비소유 (Scene 이 소유)
    f32_t     fBladeScale = 0.01f;                         // Phase 1 적정값 (caller 가 지정)
    Vec3      vBladeRotation{ 0.f, 0.f, 0.f };             // pitch/yaw/roll
    int32_t   iFanCount    = 5;
    f32_t     fFanSpread   = 1.5708f;   // π/2 (90도)
    f32_t     fFanDist     = 1.5f;
    f32_t     fFanLifetime = 1.5f;

    bool      bTriangleMode = false;
    f32_t     fTipBoost     = 0.f;   // m, 중앙 forward 추가 거리
    f32_t     fSideShrink   = 0.f;   // 0~0.9, 사이드 scale 감소 비율
};

class CUltWaveSystem final
{
public:
    ~CUltWaveSystem() = default;

    static std::unique_ptr<CUltWaveSystem> Create()
    {
        return std::unique_ptr<CUltWaveSystem>(new CUltWaveSystem());
    }

    void Execute(CWorld& world, f32_t fTimeDelta);

    static EntityID Spawn(CWorld& world,
        const Vec3& vOrigin, const Vec3& vForward, EntityID owner,
        f32_t fLength, f32_t fWidth, f32_t fSpeed, f32_t fMaxDist,
        f32_t fDamage,
        Engine::CFxStaticMeshRenderer* pRenderer,
        f32_t fBladeScale, const Vec3& vBladeRotation,
        int32_t iFanCount = 5, f32_t fFanSpread = 1.5708f,
        f32_t fFanDist = 1.5f, f32_t fFanLifetime = 1.5f,
        bool bTriangle = false, f32_t fTipBoost = 0.f, f32_t fSideShrink = 0.f);

    static void SetPulseFx(CWorld& world, EntityID waveEntity, EntityID pulseFxEntity);

private:
    CUltWaveSystem() = default;
};
