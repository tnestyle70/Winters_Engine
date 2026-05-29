#include "GameObject/FX/UltWaveSystem.h"
#include "GameObject/FX/FxSystem.h"
#include "GameObject/FX/FxCuePlayer.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/Champion/Irelia/IreliaFxPresets.h"   // ★ SpawnRBladeFan
#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/GameplayComponents.h"
#include "ProfilerAPI.h"
#include <cmath>
#include <vector>

namespace
{
    constexpr const wchar_t* kPathWave =
        L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_r_disarm_sword_icon.png";
    constexpr const wchar_t* kPathDisarmRing =
        L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_disarm_ring.png";
    constexpr const wchar_t* kPathEnemyMark =
        L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_q_mark_pulse_erode.png";

    bool_t SpawnTargetMarkCueOrLegacy(CWorld& world, EntityID target,
        const Vec3& targetPos, f32_t lifetime)
    {
        FxCueContext mark{};
        mark.attachTo = target;
        mark.vWorldPos = targetPos;
        mark.bOverrideLifetime = true;
        mark.fLifetimeOverride = lifetime;

        if (CFxCuePlayer::PlayAll(world, "Irelia.Target.Mark", mark, nullptr) != NULL_ENTITY)
            return true;

        FxBillboardComponent fxMark{};
        fxMark.attachTo = target;
        fxMark.vAttachOffset = { 0.f, 1.f, 0.f };
        fxMark.texturePath = kPathEnemyMark;
        fxMark.fWidth = 1.3f;
        fxMark.fHeight = 1.3f;
        fxMark.fLifetime = lifetime;
        fxMark.bBillboard = true;
        CFxSystem::Spawn(world, fxMark);
        return false;
    }

    bool_t SpawnRWallCueOrLegacy(CWorld& world, const UltWaveComponent& w)
    {
        FxCueContext wall{};
        wall.vWorldPos = w.vWorldPos;
        wall.vWorldPos.y += 0.02f;
        wall.vForward = w.vDirection;

        if (CFxCuePlayer::Play(world, "Irelia.R.Wall", wall) != NULL_ENTITY)
            return true;

        FxBillboardComponent fxFan{};
        fxFan.attachTo = NULL_ENTITY;
        fxFan.vWorldPos = wall.vWorldPos;
        fxFan.texturePath = kPathWave;
        fxFan.fWidth = w.fWidth * 2.0f;
        fxFan.fHeight = w.fLength * 1.2f;
        fxFan.fLifetime = w.fWallDuration;
        fxFan.bBillboard = false;
        CFxSystem::Spawn(world, fxFan);
        return false;
    }

    bool_t SpawnRHitCueOrLegacy(CWorld& world, const Vec3& hitPos,
        const Vec3& forward, f32_t lifetime)
    {
        FxCueContext hit{};
        hit.vWorldPos = hitPos;
        hit.vForward = forward;

        if (CFxCuePlayer::Play(world, "Irelia.R.Hit", hit) != NULL_ENTITY)
            return true;

        FxBillboardComponent fxRing{};
        fxRing.attachTo = NULL_ENTITY;
        fxRing.vWorldPos = { hitPos.x, hitPos.y + 1.0f, hitPos.z };
        fxRing.texturePath = kPathDisarmRing;
        fxRing.fWidth = 2.0f;
        fxRing.fHeight = 2.0f;
        fxRing.fLifetime = lifetime;
        fxRing.bBillboard = true;
        CFxSystem::Spawn(world, fxRing);
        return false;
    }

    void DestroyFxEntityIfAlive(CWorld& world, EntityID fxEntity)
    {
        if (fxEntity != NULL_ENTITY && world.IsAlive(fxEntity))
            world.DestroyEntity(fxEntity);
    }
}

EntityID CUltWaveSystem::Spawn(CWorld& world,
    const Vec3& vOrigin, const Vec3& vForward, EntityID owner,
    f32_t fLength, f32_t fWidth, f32_t fSpeed, f32_t fMaxDist, f32_t fDamage,
    Engine::CFxStaticMeshRenderer* pRenderer,
    f32_t fBladeScale, const Vec3& vBladeRotation,
    i32_t iFanCount, f32_t fFanSpread, f32_t fFanDist, f32_t fFanLifetime,
    bool bTriangle, f32_t fTipBoost, f32_t fSideShrink)
{
    EntityID e = world.CreateEntity();

    TransformComponent tf{};
    tf.m_LocalPosition = vOrigin;
    world.AddComponent<TransformComponent>(e, tf);

    UltWaveComponent w{};
    w.vWorldPos = vOrigin;
    w.vDirection = vForward;
    w.ownerEntity = owner;
    w.fLength = fLength;
    w.fWidth = fWidth;
    w.fSpeed = fSpeed;
    w.fMaxDist = fMaxDist;
    w.fDamage = fDamage;
    w.fWallDuration = 2.5f;
    w.bInWallPhase = false;

    // ★ 부채꼴 칼날 파라미터 (R 명중 시 1회 spawn)
    w.bFanSpawned    = false;
    w.pRenderer      = pRenderer;
    w.fBladeScale    = fBladeScale;
    w.vBladeRotation = vBladeRotation;
    w.iFanCount      = iFanCount;
    w.fFanSpread     = fFanSpread;
    w.fFanDist       = fFanDist;
    w.fFanLifetime   = fFanLifetime;
    w.bTriangleMode  = bTriangle;
    w.fTipBoost      = fTipBoost;
    w.fSideShrink    = fSideShrink;

    world.AddComponent<UltWaveComponent>(e, w);

    return e;
}

void CUltWaveSystem::SetPulseFx(CWorld& world, EntityID waveEntity, EntityID pulseFxEntity)
{
    if (waveEntity == NULL_ENTITY || !world.IsAlive(waveEntity))
        return;
    if (!world.HasComponent<UltWaveComponent>(waveEntity))
        return;

    world.GetComponent<UltWaveComponent>(waveEntity).pulseFxEntity = pulseFxEntity;
}

void CUltWaveSystem::Execute(CWorld& world, f32_t fTimeDelta)
{
    WINTERS_PROFILE_SCOPE("UltWave::Execute");

    std::vector<EntityID> vecDelete;

    world.ForEach<UltWaveComponent, TransformComponent>(
        std::function<void(EntityID, UltWaveComponent&, TransformComponent&)>(
            [&](EntityID e, UltWaveComponent& w, TransformComponent& tf)
            {
                eTeam ownerTeam = eTeam::Neutral;
                if (world.HasComponent<ChampionComponent>(w.ownerEntity))
                    ownerTeam = world.GetComponent<ChampionComponent>(w.ownerEntity).team;

                if (!w.bInWallPhase)
                {
                    const f32_t step = w.fSpeed * fTimeDelta;
                    w.vWorldPos.x += w.vDirection.x * step;
                    w.vWorldPos.z += w.vDirection.z * step;
                    w.fTravelled += step;
                    tf.m_LocalPosition = w.vWorldPos;

                    const f32_t halfL = w.fLength * 0.5f;
                    const f32_t halfW = w.fWidth * 0.5f;

                    world.ForEach<ChampionComponent, TransformComponent>(
                        std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                            [&](EntityID target, ChampionComponent& cc, TransformComponent& ttf)
                            {
                                if (w.bInWallPhase) return;
                                if (w.hitSet.count(target)) return;
                                if (cc.team == ownerTeam) return;
                                if (w.bladeHitSet.count(target)) return;

                                const f32_t dx = ttf.m_LocalPosition.x - w.vWorldPos.x;
                                const f32_t dz = ttf.m_LocalPosition.z - w.vWorldPos.z;
                                const f32_t along = dx * w.vDirection.x + dz * w.vDirection.z;
                                const f32_t perp = dx * (-w.vDirection.z) + dz * w.vDirection.x;
                                if (std::fabs(along) <= halfL && std::fabs(perp) <= halfW)
                                {
                                    {
                                        const Vec3 vHitPos{
                                            ttf.m_LocalPosition.x,
                                            ttf.m_LocalPosition.y,
                                            ttf.m_LocalPosition.z };

                                        cc.hp -= w.fDamage;
                                        if (cc.hp < 0.f) cc.hp = 0.f;

                                        DisarmComponent d{};
                                        d.fRemaining = 1.5f;
                                        d.sourceEntity = w.ownerEntity;
                                        if (world.HasComponent<DisarmComponent>(target))
                                            world.GetComponent<DisarmComponent>(target) = d;
                                        else
                                            world.AddComponent<DisarmComponent>(target, d);

                                        StunComponent s{};
                                        s.fRemaining = 0.3f;
                                        s.sourceEntity = w.ownerEntity;
                                        if (world.HasComponent<StunComponent>(target))
                                            world.GetComponent<StunComponent>(target) = s;
                                        else
                                            world.AddComponent<StunComponent>(target, s);

                                        SpawnTargetMarkCueOrLegacy(world, target, vHitPos, 1.5f);
                                        SpawnRHitCueOrLegacy(world, vHitPos, w.vDirection, 0.45f);

                                        if (!w.bFanSpawned && w.pRenderer)
                                        {
                                            IreliaFx::SpawnRBladeFan(
                                                world, w.pRenderer,
                                                vHitPos, w.vDirection,
                                                w.iFanCount, w.fFanSpread, w.fFanDist,
                                                w.fFanLifetime, w.fBladeScale, w.vBladeRotation,
                                                w.bTriangleMode, w.fTipBoost, w.fSideShrink);
                                            w.bFanSpawned = true;
                                        }

                                        w.hitSet.insert(target);
                                        w.bladeHitSet.insert(target);
                                        w.vWorldPos = vHitPos;
                                        tf.m_LocalPosition = vHitPos;

                                        DestroyFxEntityIfAlive(world, w.pulseFxEntity);
                                        w.pulseFxEntity = NULL_ENTITY;

                                        w.bInWallPhase = true;
                                        SpawnRWallCueOrLegacy(world, w);
                                        return;
                                    }
                                }
                            }));

                    if (!w.bInWallPhase && w.fTravelled >= w.fMaxDist)
                    {
                        w.bInWallPhase = true;

                        // [Phase T-8t] 사거리 끝 — disarm_sword_icon 지면 부채꼴 1회 스폰
                        SpawnRWallCueOrLegacy(world, w);
                    }
                }
                else
                {
                    w.fWallDuration -= fTimeDelta;
                    if (w.fWallDuration <= 0.f)
                    {
                        vecDelete.push_back(e);
                        return;
                    }

                    const f32_t halfL = w.fLength * 0.5f;
                    const f32_t halfW = w.fWidth * 0.5f;

                    world.ForEach<ChampionComponent, TransformComponent>(
                        std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                            [&](EntityID target, ChampionComponent& cc, TransformComponent& ttf)
                            {
                                if (cc.team == ownerTeam) return;
                                if (w.bladeHitSet.count(target)) return;
                                const f32_t dx = ttf.m_LocalPosition.x - w.vWorldPos.x;
                                const f32_t dz = ttf.m_LocalPosition.z - w.vWorldPos.z;
                                const f32_t along = dx * w.vDirection.x + dz * w.vDirection.z;
                                const f32_t perp = dx * (-w.vDirection.z) + dz * w.vDirection.x;
                                if (std::fabs(along) <= halfL && std::fabs(perp) <= halfW)
                                {
                                    {
                                        SlowComponent sl{};
                                        sl.fRemaining = 0.5f;
                                        sl.fMoveSpeedMul = 0.5f;
                                        sl.sourceEntity = w.ownerEntity;
                                        if (world.HasComponent<SlowComponent>(target))
                                            world.GetComponent<SlowComponent>(target) = sl;
                                        else
                                            world.AddComponent<SlowComponent>(target, sl);

                                        DisarmComponent d{};
                                        d.fRemaining = 1.5f;
                                        d.sourceEntity = w.ownerEntity;
                                        if (world.HasComponent<DisarmComponent>(target))
                                            world.GetComponent<DisarmComponent>(target) = d;
                                        else
                                            world.AddComponent<DisarmComponent>(target, d);

                                        SpawnTargetMarkCueOrLegacy(world, target, ttf.m_LocalPosition, 1.5f);
                                        w.bladeHitSet.insert(target);
                                        return;
                                    }
                                }
                            }));
                }
            }));

    for (EntityID e : vecDelete) world.DestroyEntity(e);
}
