#include "Scene/InGamePlayerTransformBridge.h"

#include "ECS/Components/TransformComponent.h"
#include "Network/Client/SnapshotApplier.h"
#include "Scene/Scene_InGame.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"

#include <cmath>
#include <cstdio>

namespace
{
    constexpr f32_t kYawHalfTurnTolerance = 0.35f;

    bool_t IsYawHalfTurn(f32_t yawDelta)
    {
        return std::fabs(std::fabs(NormalizeChampionVisualYaw(yawDelta)) - WintersMath::kPi) <=
            kYawHalfTurnTolerance;
    }

    Vec3 GameplayForwardFromVisualYaw(eChampion champion, f32_t yaw)
    {
        const f32_t gameplayYaw = yaw - GetDefaultChampionVisualYawOffset(champion);
        return Vec3{ std::sinf(gameplayYaw), 0.f, std::cosf(gameplayYaw) };
    }
}

bool CInGamePlayerTransformBridge::HasPlayerTransform(const CScene_InGame& scene)
{
    return scene.m_pPlayerTransform != nullptr;
}

Vec3 CInGamePlayerTransformBridge::GetPlayerPosition(const CScene_InGame& scene)
{
    if (scene.m_pPlayerTransform)
        return scene.m_pPlayerTransform->GetPosition();
    return Vec3{};
}

void CInGamePlayerTransformBridge::SetPlayerPosition(CScene_InGame& scene, const Vec3& v)
{
    if (!scene.m_pPlayerTransform)
        return;

    scene.m_pPlayerTransform->SetPosition(v);
    if (scene.m_PlayerEntity == NULL_ENTITY)
        return;
    if (!scene.m_World.HasComponent<TransformComponent>(scene.m_PlayerEntity))
        return;
    if (scene.m_pPlayerTransform != &scene.m_PlayerEntityTransformCache)
        return;

    scene.m_World.GetComponent<TransformComponent>(scene.m_PlayerEntity).SetPosition(v);
}

f32_t CInGamePlayerTransformBridge::GetPlayerYaw(const CScene_InGame& scene)
{
    return scene.m_pPlayerTransform ? scene.m_pPlayerTransform->GetRotation().y : 0.f;
}

void CInGamePlayerTransformBridge::SetPlayerYaw(CScene_InGame& scene, f32_t yaw)
{
    if (!scene.m_pPlayerTransform)
        return;

    Vec3 rot = scene.m_pPlayerTransform->GetRotation();
    const f32_t previousYaw = rot.y;
    const f32_t normalizedYaw = NormalizeChampionVisualYaw(yaw);
    const f32_t resolvedYaw = MakeChampionVisualYawNear(normalizedYaw, previousYaw);
    const f32_t rawDelta = yaw - previousYaw;
    const f32_t appliedDelta = NormalizeChampionVisualYaw(resolvedYaw - previousYaw);
    scene.m_pPlayerTransform->SetRotation({ rot.x, resolvedYaw, rot.z });

    static u32_t s_setYawTraceCount = 0;
    if ((scene.m_bNetworkAuthoritativeGameplay ||
            std::fabs(appliedDelta) > 0.0005f ||
            IsYawHalfTurn(appliedDelta)) &&
        s_setYawTraceCount < 512u)
    {
        const u64_t lastSnapshotTick = scene.m_pSnapshotApplier
            ? scene.m_pSnapshotApplier->GetLastAppliedServerTick()
            : 0ull;
        const eChampion championId = scene.GetPlayerChampionId();
        const f32_t visualYawOffset = GetDefaultChampionVisualYawOffset(championId);
        const Vec3 oldForward = GameplayForwardFromVisualYaw(championId, previousYaw);
        const Vec3 newForward = GameplayForwardFromVisualYaw(championId, resolvedYaw);
        const f32_t oldVsNewDot =
            oldForward.x * newForward.x + oldForward.z * newForward.z;
        char msg[704]{};
        sprintf_s(
            msg,
            "[YawTrace][SetPlayerYaw] entity=%u champion=%u netAuth=%u oldYaw=%.4f rawYaw=%.4f newYaw=%.4f rawDelta=%.4f appliedDelta=%.4f halfTurn=%u offset=%.4f oldF=(%.3f,%.3f) newF=(%.3f,%.3f) oldVsNewDot=%.4f lastSnapTick=%llu\n",
            static_cast<u32_t>(scene.m_PlayerEntity),
            static_cast<u32_t>(championId),
            scene.m_bNetworkAuthoritativeGameplay ? 1u : 0u,
            previousYaw,
            yaw,
            resolvedYaw,
            rawDelta,
            appliedDelta,
            IsYawHalfTurn(appliedDelta) ? 1u : 0u,
            visualYawOffset,
            oldForward.x,
            oldForward.z,
            newForward.x,
            newForward.z,
            oldVsNewDot,
            static_cast<unsigned long long>(lastSnapshotTick));
        OutputDebugStringA(msg);
        ++s_setYawTraceCount;
    }

    if (scene.m_PlayerEntity == NULL_ENTITY)
        return;
    if (!scene.m_World.HasComponent<TransformComponent>(scene.m_PlayerEntity))
        return;
    if (scene.m_pPlayerTransform != &scene.m_PlayerEntityTransformCache)
        return;

    auto& tf = scene.m_World.GetComponent<TransformComponent>(scene.m_PlayerEntity);
    Vec3 ecsRot = tf.GetRotation();
    ecsRot.y = resolvedYaw;
    tf.SetRotation(ecsRot);
}

Vec3 CInGamePlayerTransformBridge::GetPlayerForward(const CScene_InGame& scene)
{
    const f32_t yaw =
        GetPlayerYaw(scene) -
        GetDefaultChampionVisualYawOffset(scene.GetPlayerChampionId());
    return { sinf(yaw), 0.f, cosf(yaw) };
}

void CInGamePlayerTransformBridge::SyncFromECS(CScene_InGame& scene)
{
    if (scene.m_PlayerEntity == NULL_ENTITY)
        return;
    if (!scene.m_World.HasComponent<TransformComponent>(scene.m_PlayerEntity))
        return;

    if (scene.m_bNetworkAuthoritativeGameplay &&
        scene.m_bKalistaPassiveDashActive)
    {
        SyncToECS(scene);
        return;
    }

    auto& tf = scene.m_World.GetComponent<TransformComponent>(scene.m_PlayerEntity);
    scene.m_PlayerEntityTransformCache.SetPosition(tf.GetPosition());
    const f32_t previousYaw = scene.m_PlayerEntityTransformCache.GetRotation().y;
    const f32_t ecsYaw = tf.GetRotation().y;
    const f32_t yawDelta = NormalizeChampionVisualYaw(ecsYaw - previousYaw);
    scene.m_PlayerEntityTransformCache.SetRotation(tf.GetRotation());
    scene.m_PlayerEntityTransformCache.SetScale(tf.GetScale());

    static u32_t s_syncYawTraceCount = 0;
    if (scene.m_bNetworkAuthoritativeGameplay &&
        (std::fabs(yawDelta) > 0.0005f || IsYawHalfTurn(yawDelta)) &&
        s_syncYawTraceCount < 512u)
    {
        const u64_t lastSnapshotTick = scene.m_pSnapshotApplier
            ? scene.m_pSnapshotApplier->GetLastAppliedServerTick()
            : 0ull;
        const eChampion championId = scene.GetPlayerChampionId();
        const f32_t visualYawOffset = GetDefaultChampionVisualYawOffset(championId);
        const Vec3 oldForward = GameplayForwardFromVisualYaw(championId, previousYaw);
        const Vec3 ecsForward = GameplayForwardFromVisualYaw(championId, ecsYaw);
        const f32_t oldVsEcsDot =
            oldForward.x * ecsForward.x + oldForward.z * ecsForward.z;
        char msg[704]{};
        sprintf_s(
            msg,
            "[YawTrace][SyncFromECS] entity=%u champion=%u oldVisualYaw=%.4f ecsYaw=%.4f yawDelta=%.4f halfTurn=%u offset=%.4f oldF=(%.3f,%.3f) ecsF=(%.3f,%.3f) oldVsEcsDot=%.4f lastSnapTick=%llu\n",
            static_cast<u32_t>(scene.m_PlayerEntity),
            static_cast<u32_t>(championId),
            previousYaw,
            ecsYaw,
            yawDelta,
            IsYawHalfTurn(yawDelta) ? 1u : 0u,
            visualYawOffset,
            oldForward.x,
            oldForward.z,
            ecsForward.x,
            ecsForward.z,
            oldVsEcsDot,
            static_cast<unsigned long long>(lastSnapshotTick));
        OutputDebugStringA(msg);
        ++s_syncYawTraceCount;
    }
}

void CInGamePlayerTransformBridge::SyncToECS(CScene_InGame& scene)
{
    if (scene.m_PlayerEntity == NULL_ENTITY)
        return;
    if (!scene.m_World.HasComponent<TransformComponent>(scene.m_PlayerEntity))
        return;
    if (scene.m_pPlayerTransform != &scene.m_PlayerEntityTransformCache)
        return;

    auto& tf = scene.m_World.GetComponent<TransformComponent>(scene.m_PlayerEntity);
    tf.SetPosition(scene.m_PlayerEntityTransformCache.GetPosition());
    tf.SetRotation(scene.m_PlayerEntityTransformCache.GetRotation());
    tf.SetScale(scene.m_PlayerEntityTransformCache.GetScale());
}
