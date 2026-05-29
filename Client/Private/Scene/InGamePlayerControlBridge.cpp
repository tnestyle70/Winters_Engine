#include "Network/Client/ClientNetwork.h"
#include "Scene/InGamePlayerControlBridge.h"

#include "Core/CInput.h"
#include "ECS/Components/GameplayComponents.h"
#include "Dev/SmokeLog.h"
#include "ECS/Components/NavAgentComponent.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "Network/Client/CommandSerializer.h"
#include "Network/Client/SnapshotApplier.h"
#include "GameObject/FX/FxSystem.h"
#include "Resource/Animator.h"
#include "Scene/InGameChampionStateBridge.h"
#include "Scene/InGamePlayerTransformBridge.h"
#include "Scene/Scene_InGame.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Registries/ChampionStats/ChampionStatsRegistry.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>

namespace
{
    constexpr f32_t kPlayerAvoidancePadding = 0.05f;

    bool_t IsPlayerMoveBlockingKind(eSpatialKind kind)
    {
        return kind == eSpatialKind::Champion ||
            kind == eSpatialKind::Minion ||
            kind == eSpatialKind::JungleMob;
    }

    f32_t ResolveAgentRadius(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<SpatialAgentComponent>(entity))
            return (std::max)(0.2f, world.GetComponent<SpatialAgentComponent>(entity).radius);

        return 0.5f;
    }

    ChampionStatsDef ResolvePlayerStatsDef(CWorld& world, EntityID playerEntity)
    {
        eChampion champion = eChampion::NONE;
        if (playerEntity != NULL_ENTITY &&
            world.HasComponent<ChampionComponent>(playerEntity))
        {
            champion = world.GetComponent<ChampionComponent>(playerEntity).id;
        }

        return CChampionStatsRegistry::Instance().Resolve(champion);
    }

    f32_t ResolvePlayerMoveSpeed(CWorld& world, EntityID playerEntity)
    {
        if (playerEntity != NULL_ENTITY &&
            world.HasComponent<StatComponent>(playerEntity))
        {
            const f32_t moveSpeed =
                world.GetComponent<StatComponent>(playerEntity).moveSpeed;
            if (moveSpeed > 0.f)
                return moveSpeed;
        }

        return ResolvePlayerStatsDef(world, playerEntity).baseMoveSpeed;
    }

    f32_t ResolvePlayerArriveRadius(CWorld& world, EntityID playerEntity)
    {
        if (playerEntity != NULL_ENTITY &&
            world.HasComponent<NavAgentComponent>(playerEntity))
        {
            const f32_t arriveRadius =
                world.GetComponent<NavAgentComponent>(playerEntity).fArriveRadius;
            if (arriveRadius > 0.f)
                return arriveRadius;
        }

        return ResolvePlayerStatsDef(world, playerEntity).navArriveRadius;
    }

    bool_t IsSeparatingCandidate(
        const Vec3& vCurrent, const Vec3& vCandidate, const Vec3& vBlockerPos, f32_t minDistSq)
    {
        const f32_t currentDistSq = WintersMath::DistanceSqXZ(vCurrent, vBlockerPos);

        if (currentDistSq >= minDistSq)
            return false;

        const f32_t candidateDistSq = WintersMath::DistanceSqXZ(vCandidate, vBlockerPos);
        return candidateDistSq > currentDistSq + 0.0001f;
    }

    bool_t IsCandidateClear(
        CWorld& world,
        EntityID self,
        const Vec3& current,
        const Vec3& candidate,
        f32_t radius)
    {
        bool_t bClear = true;
        world.ForEach<SpatialAgentComponent, TransformComponent>(
            std::function<void(EntityID, SpatialAgentComponent&, TransformComponent&)>(
                [&](EntityID other, SpatialAgentComponent& agent, TransformComponent& tf)
                {
                    if (!bClear || other == self)
                        return;
                    if (!IsPlayerMoveBlockingKind(agent.kind))
                        return;

                    if (world.HasComponent<HealthComponent>(other))
                    {
                        const auto& health = world.GetComponent<HealthComponent>(other);
                        if (health.bIsDead || health.fCurrent <= 0.f)
                            return;
                    }

                    const f32_t minDist =
                        radius + (std::max)(0.2f, agent.radius) + kPlayerAvoidancePadding;
                    const Vec3 otherPos = tf.GetPosition();
                    const f32_t minDistSq = minDist * minDist;
                    if (WintersMath::DistanceSqXZ(candidate, otherPos) < minDistSq &&
                        !IsSeparatingCandidate(current, candidate, otherPos, minDistSq))
                        bClear = false;
                }));

        return bClear;
    }

    Vec3 ResolveAvoidedMoveDirection(
        CWorld& world,
        EntityID self,
        const Vec3& pos,
        const Vec3& desired,
        f32_t step,
        const std::function<bool_t(const Vec3&)>& isStepWalkable)
    {
        static constexpr f32_t kAngles[] = {
            0.f,
            0.610865f, -0.610865f,
            1.22173f, -1.22173f,
            1.570796f, -1.570796f
        };

        const f32_t radius = ResolveAgentRadius(world, self);
        for (const f32_t angle : kAngles)
        {
            const Vec3 dir = WintersMath::RotateXZ(desired, angle);
            const Vec3 candidate{
                pos.x + dir.x * step,
                pos.y,
                pos.z + dir.z * step
            };

            if (!IsCandidateClear(world, self, pos, candidate, radius))
                continue;

            if (isStepWalkable && !isStepWalkable(candidate))
                continue;

            return dir;
        }

        return Vec3{};
    }
    // Mouse pick indicator arrows converge toward the accepted move target.
    void SpawnMovementIndicator(CScene_InGame& scene, const Vec3& center)
    {
        static constexpr const wchar_t* kTexturePath =
            L"Client/Bin/Resource/Texture/UI/movement_indicator.png";

        static constexpr f32_t kLifetime = 0.32f;
        static constexpr f32_t kStartRadius = 0.95f;
        static constexpr f32_t kEndRadius = 0.18f;
        static constexpr f32_t kInwardSpeed = (kStartRadius - kEndRadius) / kLifetime;
        static constexpr f32_t kYOffset = 0.05f;
        static constexpr f32_t kWidth = 0.55f;
        static constexpr f32_t kHeight = 0.90f;
        static constexpr f32_t kYawOffset = WintersMath::kPi;

        const Vec3 radialDirs[4] = {
            { 1.f, 0.f, 0.f },
            { -1.f, 0.f, 0.f },
            { 0.f, 0.f, 1.f },
            { 0.f, 0.f, -1.f },
        };

        for (const Vec3& radial : radialDirs)
        {
            const Vec3 inward{ -radial.x, 0.f, -radial.z };

            FxBillboardComponent fx{};
            fx.renderType = eFxRenderType::GroundDecal;
            fx.texturePath = kTexturePath;
            fx.vWorldPos = {
                center.x + radial.x * kStartRadius,
                center.y + kYOffset,
                center.z + radial.z * kStartRadius
            };
            fx.vVelocity = { inward.x * kInwardSpeed, 0.f, inward.z * kInwardSpeed };
            fx.fWidth = kWidth;
            fx.fHeight = kHeight;
            fx.fYaw = std::atan2f(inward.x, inward.z) + kYawOffset;
            fx.vColor = { 1.f, 1.f, 1.f, 0.95f };
            fx.fLifetime = kLifetime;
            fx.fFadeIn = 0.02f;
            fx.fFadeOut = 0.22f;
            fx.fAlphaClip = 0.02f;
            fx.blendMode = eBlendPreset::AlphaBlend;
            fx.depthMode = eFxDepthMode::DepthTestWriteOff;
            fx.bBillboard = false;

            CFxSystem::Spawn(scene.GetWorld(), fx);
        }
    }

    bool_t PredictLocalMoveYaw(CScene_InGame& scene, const Vec3& facingTarget, f32_t& outYaw)
    {
        CTransform* playerTransform = scene.GetPlayerTransformPtr();
        if (!playerTransform)
            return false;

        const Vec3 origin = playerTransform->GetPosition();
        const Vec3 direction{
            facingTarget.x - origin.x,
            0.f,
            facingTarget.z - origin.z
        };
        if ((direction.x * direction.x + direction.z * direction.z) <= 0.0001f)
            return false;

        const f32_t yaw =
            ResolveChampionVisualYawNear(
                scene.GetPlayerChampionId(),
                direction,
                playerTransform->GetRotation().y);
        outYaw = yaw;

        static u32_t s_predictMoveYawTraceCount = 0;
        if (s_predictMoveYawTraceCount < 512u)
        {
            const eChampion champion = scene.GetPlayerChampionId();
            const f32_t currentYaw = playerTransform->GetRotation().y;
            const f32_t rawYaw =
                ResolveChampionVisualYawFromDirection(champion, direction);
            char msg[768]{};
            sprintf_s(
                msg,
                "[YawTrace][PredictLocalMoveYaw] entity=%u champion=%u origin=(%.3f,%.3f,%.3f) facing=(%.3f,%.3f,%.3f) dir=(%.3f,%.3f) currentYaw=%.4f rawYaw=%.4f predictedYaw=%.4f delta=%.4f\n",
                static_cast<u32_t>(scene.GetPlayerEntity()),
                static_cast<u32_t>(champion),
                origin.x,
                origin.y,
                origin.z,
                facingTarget.x,
                facingTarget.y,
                facingTarget.z,
                direction.x,
                direction.z,
                currentYaw,
                rawYaw,
                yaw,
                NormalizeChampionVisualYaw(yaw - currentYaw));
            OutputDebugStringA(msg);
            ++s_predictMoveYawTraceCount;
        }

        CInGamePlayerTransformBridge::SetPlayerYaw(scene, yaw);
        return true;
    }

    void OutputClientMoveYawTrace(
        CScene_InGame& scene,
        u32_t seq,
        u32_t netId,
        const Vec3& rawTarget,
        const Vec3& resolvedTarget,
        const Vec3& facingTarget,
        f32_t predictedYaw)
    {
        CTransform* playerTransform = scene.GetPlayerTransformPtr();
        const Vec3 playerPos = playerTransform ? playerTransform->GetPosition() : Vec3{};
        const f32_t currentYaw = playerTransform ? playerTransform->GetRotation().y : 0.f;
        const eChampion champion = scene.GetPlayerChampionId();
        const Vec3 rawDir = playerTransform
            ? WintersMath::DirectionXZ(playerPos, rawTarget, Vec3{})
            : Vec3{};
        const Vec3 resolvedDir = playerTransform
            ? WintersMath::DirectionXZ(playerPos, resolvedTarget, Vec3{})
            : Vec3{};
        const Vec3 facingDir = playerTransform
            ? WintersMath::DirectionXZ(playerPos, facingTarget, Vec3{})
            : Vec3{};
        const f32_t rawYaw = (rawDir.x != 0.f || rawDir.z != 0.f)
            ? ResolveChampionVisualYawFromDirection(champion, rawDir)
            : currentYaw;
        const f32_t resolvedYaw = (resolvedDir.x != 0.f || resolvedDir.z != 0.f)
            ? ResolveChampionVisualYawFromDirection(champion, resolvedDir)
            : currentYaw;
        const f32_t facingYaw = (facingDir.x != 0.f || facingDir.z != 0.f)
            ? ResolveChampionVisualYawFromDirection(champion, facingDir)
            : currentYaw;
        const f32_t rawDotFacing =
            rawDir.x * facingDir.x + rawDir.z * facingDir.z;
        const f32_t rawDotResolved =
            rawDir.x * resolvedDir.x + rawDir.z * resolvedDir.z;
        char msg[1024]{};
        sprintf_s(
            msg,
            "[YawTrace][ClientInput] seq=%u net=%u champion=%u pos=(%.3f,%.3f,%.3f) raw=(%.3f,%.3f,%.3f) resolved=(%.3f,%.3f,%.3f) facing=(%.3f,%.3f,%.3f) rawDir=(%.3f,%.3f) resolvedDir=(%.3f,%.3f) facingDir=(%.3f,%.3f) predictedYaw=%.4f rawYaw=%.4f resolvedYaw=%.4f facingYaw=%.4f currentYaw=%.4f predVsRaw=%.4f predVsResolved=%.4f predVsFacing=%.4f rawDotResolved=%.4f rawDotFacing=%.4f\n",
            seq,
            netId,
            static_cast<u32_t>(champion),
            playerPos.x,
            playerPos.y,
            playerPos.z,
            rawTarget.x,
            rawTarget.y,
            rawTarget.z,
            resolvedTarget.x,
            resolvedTarget.y,
            resolvedTarget.z,
            facingTarget.x,
            facingTarget.y,
            facingTarget.z,
            rawDir.x,
            rawDir.z,
            resolvedDir.x,
            resolvedDir.z,
            facingDir.x,
            facingDir.z,
            predictedYaw,
            rawYaw,
            resolvedYaw,
            facingYaw,
            currentYaw,
            NormalizeChampionVisualYaw(predictedYaw - rawYaw),
            NormalizeChampionVisualYaw(predictedYaw - resolvedYaw),
            NormalizeChampionVisualYaw(predictedYaw - facingYaw),
            rawDotResolved,
            rawDotFacing);
        OutputDebugStringA(msg);
    }

    void OutputClientMoveFrameTrace(
        CScene_InGame& scene,
        f32_t dt,
        bool_t bNetworkActive,
        bool_t bSkipGroundMove,
        bool_t bActionLocked,
        bool_t bMoveIntent,
        bool_t bValidGround,
        bool_t bAcceptedMoveTarget,
        const Vec3& rawTarget,
        const Vec3& resolvedTarget,
        const Vec3& facingTarget)
    {
        static u32_t s_moveFrameTraceCount = 0;
        if (s_moveFrameTraceCount >= 1024u)
            return;

        auto& input = CInput::Get();
        CTransform* playerTransform = scene.GetPlayerTransformPtr();
        const Vec3 playerPos = playerTransform ? playerTransform->GetPosition() : Vec3{};
        const f32_t currentYaw = playerTransform ? playerTransform->GetRotation().y : 0.f;
        const eChampion champion = scene.GetPlayerChampionId();
        const Vec3 rawDir = playerTransform
            ? WintersMath::DirectionXZ(playerPos, rawTarget, Vec3{})
            : Vec3{};
        const Vec3 resolvedDir = playerTransform
            ? WintersMath::DirectionXZ(playerPos, resolvedTarget, Vec3{})
            : Vec3{};
        const Vec3 facingDir = playerTransform
            ? WintersMath::DirectionXZ(playerPos, facingTarget, Vec3{})
            : Vec3{};
        const f32_t rawYaw = (rawDir.x != 0.f || rawDir.z != 0.f)
            ? ResolveChampionVisualYawFromDirection(champion, rawDir)
            : currentYaw;
        const f32_t resolvedYaw = (resolvedDir.x != 0.f || resolvedDir.z != 0.f)
            ? ResolveChampionVisualYawFromDirection(champion, resolvedDir)
            : currentYaw;
        const f32_t facingYaw = (facingDir.x != 0.f || facingDir.z != 0.f)
            ? ResolveChampionVisualYawFromDirection(champion, facingDir)
            : currentYaw;
        const f32_t rawDotResolved =
            rawDir.x * resolvedDir.x + rawDir.z * resolvedDir.z;
        const f32_t rawDotFacing =
            rawDir.x * facingDir.x + rawDir.z * facingDir.z;
        const bool_t bRawOpposesResolved =
            (rawDir.x != 0.f || rawDir.z != 0.f) &&
            (resolvedDir.x != 0.f || resolvedDir.z != 0.f) &&
            rawDotResolved < -0.10f;
        const bool_t bRawOpposesFacing =
            (rawDir.x != 0.f || rawDir.z != 0.f) &&
            (facingDir.x != 0.f || facingDir.z != 0.f) &&
            rawDotFacing < -0.10f;

        char msg[1536]{};
        sprintf_s(
            msg,
            "[YawTrace][ClientMoveFrame] entity=%u champion=%u netAuth=%u dt=%.4f skip=%u actionLocked=%u moveIntent=%u rPressed=%u rDown=%u mouse=(%d,%d) valid=%u accepted=%u pos=(%.3f,%.3f,%.3f) currentYaw=%.4f raw=(%.3f,%.3f,%.3f) resolved=(%.3f,%.3f,%.3f) facing=(%.3f,%.3f,%.3f) rawDir=(%.3f,%.3f) resolvedDir=(%.3f,%.3f) facingDir=(%.3f,%.3f) rawYaw=%.4f resolvedYaw=%.4f facingYaw=%.4f rawVsResolvedDot=%.4f rawVsFacingDot=%.4f rawOppResolved=%u rawOppFacing=%u\n",
            static_cast<u32_t>(scene.GetPlayerEntity()),
            static_cast<u32_t>(champion),
            bNetworkActive ? 1u : 0u,
            dt,
            bSkipGroundMove ? 1u : 0u,
            bActionLocked ? 1u : 0u,
            bMoveIntent ? 1u : 0u,
            input.IsRButtonPressed() ? 1u : 0u,
            input.IsRButtonDown() ? 1u : 0u,
            static_cast<int>(input.GetMouseX()),
            static_cast<int>(input.GetMouseY()),
            bValidGround ? 1u : 0u,
            bAcceptedMoveTarget ? 1u : 0u,
            playerPos.x,
            playerPos.y,
            playerPos.z,
            currentYaw,
            rawTarget.x,
            rawTarget.y,
            rawTarget.z,
            resolvedTarget.x,
            resolvedTarget.y,
            resolvedTarget.z,
            facingTarget.x,
            facingTarget.y,
            facingTarget.z,
            rawDir.x,
            rawDir.z,
            resolvedDir.x,
            resolvedDir.z,
            facingDir.x,
            facingDir.z,
            rawYaw,
            resolvedYaw,
            facingYaw,
            rawDotResolved,
            rawDotFacing,
            bRawOpposesResolved ? 1u : 0u,
            bRawOpposesFacing ? 1u : 0u);
        OutputDebugStringA(msg);
        ++s_moveFrameTraceCount;
    }

}

void CInGamePlayerControlBridge::Update(
    CScene_InGame& scene,
    f32_t dt,
    bool_t bNetworkActive,
    bool_t bSkipGroundMove,
    bool_t bActionLockedBefore)
{
    const bool_t bActionLocked = (scene.m_fLastActionTimer > 0.f);

    if (scene.m_pPlayerRenderer &&
        (!bNetworkActive || scene.m_bKalistaPassiveDashAnimActive))
    {
        auto* pAnim = scene.m_pPlayerRenderer->GetAnimator();
        if (pAnim)
        {
            f32_t s;
            if (scene.m_bKalistaPassiveDashAnimActive)
            {
                s = scene.m_fGlobalAnimSpeed * scene.GetKalistaPassiveDashAnimSpeed();
            }
            else if (bActionLocked)
            {
                const f32_t skillSpeed = scene.m_pActiveSkillDef ? scene.m_pActiveSkillDef->animPlaySpeed : 1.f;
                s = scene.m_fAttackSpeedMul * scene.m_fGlobalAnimSpeed * skillSpeed;
            }
            else
            {
                s = scene.m_fGlobalAnimSpeed;
            }
            pAnim->SetPlaySpeed(s);
        }
    }

    if (scene.m_pPlayerTransform && scene.m_pPlayerRenderer)
    {
        auto& input = CInput::Get();
        const bool bImGuiMouse = ImGui::GetIO().WantCaptureMouse;
        const bool_t bMoveIntent = bNetworkActive
            ? input.IsRButtonPressed()
            : input.IsRButtonDown();
        const bool_t bPassiveDashAnimBlocksMove =
            !bNetworkActive && scene.m_bKalistaPassiveDashAnimActive;

        if (!bImGuiMouse &&
            !bSkipGroundMove &&
            (bNetworkActive || !bActionLocked) &&
            !scene.m_bKalistaPassiveDashActive &&
            !bPassiveDashAnimBlocksMove &&
            bMoveIntent)
        {
            Vec3 ground = scene.ResolveMouseMapSurfacePos();
            Vec3 resolvedGround = ground;
            Vec3 predictedFacingTarget = ground;

            const bool_t bValidGround = fabsf(ground.x) + fabsf(ground.z) > 0.001f;
            bool_t bAcceptedMoveTarget = false;
            if (bValidGround)
            {
                bAcceptedMoveTarget = scene.TryResolveWalkableMoveTarget(
                    ground,
                    resolvedGround,
                    &predictedFacingTarget);
            }

            if (input.IsRButtonPressed() || bAcceptedMoveTarget)
            {
                static u32_t s_moveResolveTraceCount = 0;
                if (s_moveResolveTraceCount < 512u)
                {
                    const Vec3 playerPos = scene.m_pPlayerTransform
                        ? scene.m_pPlayerTransform->GetPosition()
                        : Vec3{};
                    const Vec3 rawDir =
                        WintersMath::DirectionXZ(playerPos, ground, Vec3{});
                    const Vec3 resolvedDir =
                        WintersMath::DirectionXZ(playerPos, resolvedGround, Vec3{});
                    const Vec3 predictedDir =
                        WintersMath::DirectionXZ(playerPos, predictedFacingTarget, Vec3{});
                    char msg[1024]{};
                    sprintf_s(
                        msg,
                        "[YawTrace][ClientMoveResolveResult] accepted=%u valid=%u player=(%.3f,%.3f,%.3f) ground=(%.3f,%.3f,%.3f) resolved=(%.3f,%.3f,%.3f) predictedFacing=(%.3f,%.3f,%.3f) rawDir=(%.3f,%.3f) resolvedDir=(%.3f,%.3f) predictedDir=(%.3f,%.3f) rawVsPredictedDot=%.4f rawVsResolvedDot=%.4f\n",
                        bAcceptedMoveTarget ? 1u : 0u,
                        bValidGround ? 1u : 0u,
                        playerPos.x,
                        playerPos.y,
                        playerPos.z,
                        ground.x,
                        ground.y,
                        ground.z,
                        resolvedGround.x,
                        resolvedGround.y,
                        resolvedGround.z,
                        predictedFacingTarget.x,
                        predictedFacingTarget.y,
                        predictedFacingTarget.z,
                        rawDir.x,
                        rawDir.z,
                        resolvedDir.x,
                        resolvedDir.z,
                        predictedDir.x,
                        predictedDir.z,
                        rawDir.x * predictedDir.x + rawDir.z * predictedDir.z,
                        rawDir.x * resolvedDir.x + rawDir.z * resolvedDir.z);
                    OutputDebugStringA(msg);
                    ++s_moveResolveTraceCount;
                }
            }

            if (input.IsRButtonPressed() || bAcceptedMoveTarget)
            {
                OutputClientMoveFrameTrace(
                    scene,
                    dt,
                    bNetworkActive,
                    bSkipGroundMove,
                    bActionLocked,
                    bMoveIntent,
                    bValidGround,
                    bAcceptedMoveTarget,
                    ground,
                    resolvedGround,
                    predictedFacingTarget);
            }

            if (bAcceptedMoveTarget)
            {
                Vec3 moveIntent = ground;
                //moveIntent.y = resolvedGround.y;
                predictedFacingTarget = moveIntent;

                if (input.IsRButtonPressed())
                    SpawnMovementIndicator(scene, resolvedGround);

                if (bNetworkActive && scene.m_pCommandSerializer && scene.m_pNetworkView)
                {
                    const bool_t bKalistaPassiveDashMove =
                        scene.m_bKalistaPassiveDashMoveCommandPending;
                    const Vec3 moveFacingDirection = scene.m_pPlayerTransform
                        ? WintersMath::DirectionXZ(
                            scene.m_pPlayerTransform->GetPosition(),
                            ground,
                            Vec3{})
                        : Vec3{};
                    static u32_t s_moveSendPrepTraceCount = 0;
                    if (s_moveSendPrepTraceCount < 512u)
                    {
                        const Vec3 playerPos = scene.m_pPlayerTransform
                            ? scene.m_pPlayerTransform->GetPosition()
                            : Vec3{};
                        const Vec3 rawDir =
                            WintersMath::DirectionXZ(playerPos, ground, Vec3{});
                        const f32_t rawYaw = std::atan2f(rawDir.x, rawDir.z);
                        const f32_t sendYaw = std::atan2f(
                            moveFacingDirection.x,
                            moveFacingDirection.z);
                        char msg[1024]{};
                        sprintf_s(
                            msg,
                            "[YawTrace][ClientMoveSendPrep] mouse=(%d,%d) valid=%u accepted=%u player=(%.3f,%.3f,%.3f) ground=(%.3f,%.3f,%.3f) resolved=(%.3f,%.3f,%.3f) moveIntent=(%.3f,%.3f,%.3f) predictedFacing=(%.3f,%.3f,%.3f) rawDir=(%.3f,%.3f) sendDir=(%.3f,%.3f) rawYaw=%.4f sendYaw=%.4f rawVsSendDot=%.4f\n",
                            static_cast<int>(input.GetMouseX()),
                            static_cast<int>(input.GetMouseY()),
                            bValidGround ? 1u : 0u,
                            bAcceptedMoveTarget ? 1u : 0u,
                            playerPos.x,
                            playerPos.y,
                            playerPos.z,
                            ground.x,
                            ground.y,
                            ground.z,
                            resolvedGround.x,
                            resolvedGround.y,
                            resolvedGround.z,
                            moveIntent.x,
                            moveIntent.y,
                            moveIntent.z,
                            predictedFacingTarget.x,
                            predictedFacingTarget.y,
                            predictedFacingTarget.z,
                            rawDir.x,
                            rawDir.z,
                            moveFacingDirection.x,
                            moveFacingDirection.z,
                            rawYaw,
                            sendYaw,
                            rawDir.x * moveFacingDirection.x + rawDir.z * moveFacingDirection.z);
                        Winters::DevSmoke::Log("%s", msg);
                        ++s_moveSendPrepTraceCount;
                    }
                    const u32_t moveSeq =
                        scene.m_pCommandSerializer->SendMove(
                            *scene.m_pNetworkView,
                            moveIntent,
                            moveFacingDirection);
                    if (bKalistaPassiveDashMove)
                    {
                        const bool_t bTriggerAfterMove =
                            scene.m_bKalistaPassiveDashTriggerAfterMove;
                        const u16_t triggerAnimId =
                            scene.m_uKalistaPassiveDashTriggerAnimId;
                        const u32_t triggerActionSeq =
                            scene.m_uKalistaPassiveDashTriggerActionSeq;

                        scene.m_bKalistaPassiveDashMoveCommandPending = false;
                        scene.m_bKalistaPassiveDashTriggerAfterMove = false;
                        scene.m_uKalistaPassiveDashTriggerAnimId = 0;
                        scene.m_uKalistaPassiveDashTriggerActionSeq = 0;

                        if (moveSeq != 0u && bTriggerAfterMove)
                        {
                            const bool_t bDashStarted =
                                CInGameChampionStateBridge::TriggerNetworkPassiveDashFromAction(
                                    scene,
                                    triggerAnimId,
                                    triggerActionSeq,
                                    true);
                            if (bDashStarted &&
                                scene.m_PlayerEntity != NULL_ENTITY)
                            {
                                auto it = scene.m_NetworkActionAnimStates.find(scene.m_PlayerEntity);
                                if (it != scene.m_NetworkActionAnimStates.end())
                                {
                                    it->second.bPassiveDashTriggered = true;
                                    it->second.passiveDashInputGraceSec = 0.f;
                                }
                            }
                        }
                    }
                    else
                    {
                        f32_t predictedYaw = 0.f;
                        if (PredictLocalMoveYaw(scene, predictedFacingTarget, predictedYaw) &&
                            scene.m_pSnapshotApplier)
                        {
                            OutputClientMoveYawTrace(
                                scene,
                                moveSeq,
                                scene.m_pNetworkView->GetMyNetEntityId(),
                                ground,
                                resolvedGround,
                                predictedFacingTarget,
                                predictedYaw);
                            scene.m_pSnapshotApplier->ProtectLocalMoveYaw(
                                scene.m_pNetworkView->GetMyNetEntityId(),
                                moveSeq,
                                predictedYaw);
                        }
                    }
                    if (scene.m_PlayerEntity != NULL_ENTITY)
                    {
                        f32_t& moveGrace =
                            scene.m_NetworkChampionMoveGraceSec[scene.m_PlayerEntity];
                        moveGrace = (std::max)(moveGrace, 0.16f);
                        scene.m_NetworkChampionMoving[scene.m_PlayerEntity] = true;

                        if (!bKalistaPassiveDashMove &&
                            !scene.m_bMoving &&
                            scene.m_pPlayerRenderer &&
                            scene.m_pPlayerRunAnim)
                        {
                            scene.m_pPlayerRenderer->PlayAnimationByName(
                                scene.m_pPlayerRunAnim,
                                true);
                        }
                        scene.m_bMoving = true;
                    }

                    scene.m_vPlayerDest = resolvedGround;
                    if (scene.m_PlayerEntity != NULL_ENTITY &&
                        scene.m_World.HasComponent<NavAgentComponent>(scene.m_PlayerEntity))
                    {
                        auto& agent = scene.m_World.GetComponent<NavAgentComponent>(scene.m_PlayerEntity);
                        agent.vTarget = resolvedGround;
                        agent.bHasGoal = true;
                        agent.bPathDirty = true;
                        agent.pathCellsX.clear();
                        agent.pathCellsY.clear();
                        agent.iPathIndex = 0;
                    }
                }
                else
                {
                    scene.m_vPlayerDest = resolvedGround;

                    if (scene.m_PlayerEntity != NULL_ENTITY &&
                        scene.m_World.HasComponent<NavAgentComponent>(scene.m_PlayerEntity))
                    {
                        auto& agent = scene.m_World.GetComponent<NavAgentComponent>(scene.m_PlayerEntity);
                        agent.vTarget = resolvedGround;
                        agent.bHasGoal = true;
                        agent.bPathDirty = true;
                        agent.pathCellsX.clear();
                        agent.pathCellsY.clear();
                        agent.iPathIndex = 0;
                    }
                }
            }
            else if (input.IsRButtonPressed())
            {
                scene.m_bKalistaPassiveDashMoveCommandPending = false;
                scene.m_bKalistaPassiveDashTriggerAfterMove = false;
                scene.m_uKalistaPassiveDashTriggerAnimId = 0;
                scene.m_uKalistaPassiveDashTriggerActionSeq = 0;
                if (false)
                {
                    char msg[256]{};
                    sprintf_s(
                        msg,
                        "[YawTrace][ClientInputReject] validGround=%u raw=(%.3f,%.3f,%.3f)\n",
                        bValidGround ? 1u : 0u,
                        ground.x,
                        ground.y,
                        ground.z);
                    OutputDebugStringA(msg);
                }
            }
        }

        Vec3 cur = scene.m_pPlayerTransform->GetPosition();
        if (!bNetworkActive)
        {
            Vec3 resolvedCur{};
            if (scene.TryResolveNearestWalkablePosition(cur, resolvedCur, 8))
            {
                if (WintersMath::DistanceSqXZ(resolvedCur, cur) > 0.0001f)
                {
                    cur = resolvedCur;
                    scene.m_pPlayerTransform->SetPosition(cur);
                    if (scene.m_PlayerEntity != NULL_ENTITY &&
                        scene.m_World.HasComponent<TransformComponent>(scene.m_PlayerEntity))
                    {
                        scene.m_World.GetComponent<TransformComponent>(scene.m_PlayerEntity).SetPosition(cur);
                    }
                    scene.m_vPlayerDest.y = cur.y;
                }
            }
        }
        Vec3 localMoveTarget = scene.m_vPlayerDest;
        const f32_t playerArriveRadius =
            ResolvePlayerArriveRadius(scene.m_World, scene.m_PlayerEntity);
        const f32_t playerMoveSpeed =
            ResolvePlayerMoveSpeed(scene.m_World, scene.m_PlayerEntity);
        bool_t bLocalPathControlled = false;
        bool_t bLocalPathReady = false;
        if (!bNetworkActive &&
            scene.m_pNavGrid &&
            scene.m_PlayerEntity != NULL_ENTITY &&
            scene.m_World.HasComponent<NavAgentComponent>(scene.m_PlayerEntity))
        {
            bLocalPathControlled = true;
            auto& agent = scene.m_World.GetComponent<NavAgentComponent>(scene.m_PlayerEntity);
            if (agent.bHasGoal && !agent.bPathDirty)
            {
                const size_t pathCount =
                    (std::min)(agent.pathCellsX.size(), agent.pathCellsY.size());
                while (agent.iPathIndex < pathCount)
                {
                    Vec3 waypoint = scene.m_pNavGrid->CellToWorld(
                        agent.pathCellsX[agent.iPathIndex],
                        agent.pathCellsY[agent.iPathIndex]);
                    if (!scene.TryProjectToMapSurface(waypoint, 0.05f))
                        waypoint.y = cur.y;

                    if (WintersMath::DistanceSqXZ(cur, waypoint) >
                        playerArriveRadius * playerArriveRadius)
                    {
                        localMoveTarget = waypoint;
                        bLocalPathReady = true;
                        break;
                    }

                    ++agent.iPathIndex;
                }

                if (agent.iPathIndex >= pathCount)
                {
                    agent.bHasGoal = false;
                    agent.pathCellsX.clear();
                    agent.pathCellsY.clear();
                    agent.iPathIndex = 0;
                    localMoveTarget = cur;
                }
            }
            else
            {
                localMoveTarget = cur;
            }
        }

        Vec3 delta = { localMoveTarget.x - cur.x, 0.f, localMoveTarget.z - cur.z };
        f32_t dist = sqrtf(delta.x * delta.x + delta.z * delta.z);

        bool wasMoving = scene.m_bMoving;

        if (bNetworkActive)
        {
            const bool_t bNetworkMoving = scene.IsNetworkChampionMoving(scene.m_PlayerEntity);
            scene.SyncPlayerEntityTransformFromECS();
            cur = scene.m_pPlayerTransform->GetPosition();
            scene.m_bMoving = bNetworkMoving;
        }
        else if (!bActionLocked &&
            !scene.m_bKalistaPassiveDashActive &&
            dist > playerArriveRadius)
        {
            if (bLocalPathControlled && !bLocalPathReady)
            {
                scene.m_bMoving = false;
                if (scene.m_PlayerEntity != NULL_ENTITY &&
                    scene.m_World.HasComponent<NavAgentComponent>(scene.m_PlayerEntity))
                {
                    auto& agent = scene.m_World.GetComponent<NavAgentComponent>(scene.m_PlayerEntity);
                    if (!agent.bHasGoal)
                        scene.m_vPlayerDest = cur;
                }
            }
            else
            {
                f32_t inv = 1.f / dist;
                Vec3 dir = { delta.x * inv, 0.f, delta.z * inv };
                f32_t step = playerMoveSpeed * dt;
                if (step > dist) step = dist;

                const f32_t navRadius = ResolveAgentRadius(scene.m_World, scene.m_PlayerEntity);
                const Vec3 moveDir = ResolveAvoidedMoveDirection(
                    scene.m_World,
                    scene.m_PlayerEntity,
                    cur,
                    dir,
                    step,
                    [&](const Vec3& candidate)
                    {
                        return scene.IsWalkableMoveSegment(cur, candidate, navRadius);
                    });

                if (fabsf(moveDir.x) + fabsf(moveDir.z) > 0.0001f)
                {
                    Vec3 next = cur;
                    next.x += moveDir.x * step;
                    next.z += moveDir.z * step;
                    if (!scene.IsWalkableMoveSegment(cur, next, navRadius))
                    {
                        scene.m_bMoving = false;
                        scene.m_vPlayerDest = cur;
                        if (scene.m_PlayerEntity != NULL_ENTITY &&
                            scene.m_World.HasComponent<NavAgentComponent>(scene.m_PlayerEntity))
                        {
                            auto& agent = scene.m_World.GetComponent<NavAgentComponent>(scene.m_PlayerEntity);
                            agent.bHasGoal = false;
                            agent.bPathDirty = false;
                        }
                    }
                    else
                    {
                        cur = next;
                        (void)scene.TryProjectToMapSurface(cur, 0.05f);

                        scene.SetPlayerPosition(cur);

                        f32_t yaw =
                            ResolveChampionVisualYawNear(
                                scene.GetPlayerChampionId(),
                                moveDir,
                                scene.GetPlayerYaw());
                        scene.SetPlayerYaw(yaw);

                        scene.m_bMoving = true;
                    }
                }
                else
                {
                    scene.m_bMoving = false;
                }
            }
        }
        else
        {
            scene.m_bMoving = false;
        }

        const bool bInTransition = (scene.m_fEndTransitionTimer > 0.f);
        if (!scene.m_bKalistaPassiveDashActive
            && !scene.m_bKalistaPassiveDashAnimActive
            && !bNetworkActive
            && !bActionLocked
            && !bInTransition
            && scene.m_bMoving != wasMoving)
        {
            scene.m_pPlayerRenderer->PlayAnimationByName(
                scene.m_bMoving ? scene.m_pPlayerRunAnim : scene.m_pPlayerIdleAnim
            );
        }
        else if (!scene.m_bKalistaPassiveDashActive
            && !scene.m_bKalistaPassiveDashAnimActive
            && !bNetworkActive
            && bActionLockedBefore
            && !bActionLocked
            && scene.m_pPlayerRenderer)
        {
            const char* pTransition = nullptr;
            f32_t fDur = 0.f;
            if (scene.m_pLastDispatchedSkill)
            {
                pTransition = scene.m_bMoving
                    ? scene.m_pLastDispatchedSkill->endTransitionRunAnim
                    : scene.m_pLastDispatchedSkill->endTransitionIdleAnim;
                fDur = scene.m_pLastDispatchedSkill->endTransitionDuration;
            }

            if (pTransition && fDur > 0.01f)
            {
                scene.m_pPlayerRenderer->PlayAnimationByName(pTransition, false);
                scene.m_pPendingEndAnim = pTransition;
                scene.m_fEndTransitionTimer = fDur;

                char dbg[128];
                sprintf_s(dbg, "[EndTransition] %s (dur=%.2fs, moving=%d)\n",
                    pTransition, fDur, scene.m_bMoving ? 1 : 0);
                OutputDebugStringA(dbg);
            }
            else
            {
                scene.m_pPlayerRenderer->PlayAnimationByName(
                    scene.m_bMoving ? scene.m_pPlayerRunAnim : scene.m_pPlayerIdleAnim
                );
            }
        }
    }

    if (!bNetworkActive)
        scene.SyncPlayerEntityTransformToECS();
}
