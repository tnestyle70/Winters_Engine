#include "Scene/InGameCombatInputBridge.h"

#include "Core/CInput.h"
#include "ECS/Components/GameplayComponents.h"

#include "Scene/InGameChampionStateBridge.h"
#include "Scene/GameplayQuery.h"
#include "Scene/InGamePlayerTransformBridge.h"
#include "Scene/InGameSkillDispatchBridge.h"
#include "Scene/Scene_InGame.h"
#include "GameInstance.h"

#include "Network/Client/ClientNetwork.h"
#include "Network/Client/CommandSerializer.h"
#include "Network/Client/SnapshotApplier.h"
#include "Dev/SmokeLog.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
    bool_t s_bWReleasePending = false;
    EntityID s_NetworkAttackTarget = NULL_ENTITY;
    u32_t s_uNetworkAttackCommandFrame = 0u;
    u32_t s_uNetworkAttackMissLogCount = 0u;
    constexpr u32_t kNetworkAttackCommandIntervalFrames = 6u;

    void ClearNetworkAttackIntent()
    {
        s_NetworkAttackTarget = NULL_ENTITY;
        s_uNetworkAttackCommandFrame = 0u;
    }

    bool_t HasPendingSkillStage(CScene_InGame& scene, u8_t slot)
    {
        CWorld& world = scene.GetWorld();
        const EntityID player = scene.GetPlayerEntity();
        if (player == NULL_ENTITY ||
            !world.HasComponent<SkillStateComponent>(player) ||
            slot >= 5u)
        {
            return false;
        }

        const auto& skillSlot =
            world.GetComponent<SkillStateComponent>(player).slots[slot];
        return skillSlot.currentStage == 1 && skillSlot.stageWindow > 0.f;
    }

    void ProtectNetworkAttackYaw(
        CScene_InGame& scene,
        CClientNetwork* pNetworkView,
        u32_t commandSeq,
        const Vec3& facingTarget)
    {
        CSnapshotApplier* pSnapshotApplier = scene.GetSnapshotApplier();
        CTransform* pPlayerTransform = scene.GetPlayerTransformPtr();
        if (commandSeq == 0 ||
            !pNetworkView ||
            !pSnapshotApplier ||
            !pPlayerTransform)
        {
            return;
        }

        const Vec3 origin = pPlayerTransform->GetPosition();
        const Vec3 facingDirection =
            WintersMath::DirectionXZ(origin, facingTarget, Vec3{});
        if (facingDirection.x == 0.f && facingDirection.z == 0.f)
            return;

        const f32_t predictedYaw = ResolveChampionVisualYawNear(
            scene.GetPlayerChampionId(),
            facingDirection,
            pPlayerTransform->GetRotation().y);

        CInGamePlayerTransformBridge::SetPlayerYaw(scene, predictedYaw);
        pSnapshotApplier->ProtectLocalMoveYaw(
            pNetworkView->GetMyNetEntityId(),
            commandSeq,
            predictedYaw);
        if (scene.GetPlayerChampionId() == eChampion::KALISTA)
        {
            scene.SetKalistaPassiveDashFaceDir(facingDirection);
        }

        static u32_t s_attackYawProtectLogCount = 0;
        if (false && s_attackYawProtectLogCount < 64u)
        {
            char msg[384]{};
            sprintf_s(
                msg,
                "[YawTrace][ClientAttackIntent] seq=%u net=%u champion=%u target=(%.3f,%.3f,%.3f) dir=(%.3f,%.3f) predictedYaw=%.4f\n",
                commandSeq,
                pNetworkView->GetMyNetEntityId(),
                static_cast<u32_t>(scene.GetPlayerChampionId()),
                facingTarget.x,
                facingTarget.y,
                facingTarget.z,
                facingDirection.x,
                facingDirection.z,
                predictedYaw);
            OutputDebugStringA(msg);
            ++s_attackYawProtectLogCount;
        }
    }

    void DriveNetworkAttackIntent(
        CScene_InGame& scene,
        bool_t bNetworkAuthoritativeGameplay,
        eTeam playerTeam,
        CCommandSerializer* pCommandSerializer,
        CClientNetwork* pNetworkView,
        EntityIdMap* pEntityIdMap,
        bool& outSkipGroundMove)
    {
        if (s_NetworkAttackTarget == NULL_ENTITY)
            return;

        outSkipGroundMove = true;

        if (!bNetworkAuthoritativeGameplay ||
            !GameplayQuery::IsValidAttackTarget(
                scene.GetWorld(),
                scene.GetPlayerEntity(),
                s_NetworkAttackTarget,
                playerTeam))
        {
            ClearNetworkAttackIntent();
            outSkipGroundMove = false;
            return;
        }

        if (!pCommandSerializer || !pNetworkView || !pEntityIdMap)
        {
            OutputDebugStringA("[BA] network basic-attack intent skipped: network objects missing\n");
            Winters::DevSmoke::Log("[BA] network basic-attack intent skipped: network objects missing\n");
            return;
        }

        if (s_uNetworkAttackCommandFrame < kNetworkAttackCommandIntervalFrames)
        {
            ++s_uNetworkAttackCommandFrame;
            return;
        }
        s_uNetworkAttackCommandFrame = 0u;

        const NetEntityId targetNet = pEntityIdMap->ToNet(s_NetworkAttackTarget);
        if (targetNet == NULL_NET_ENTITY)
        {
            char dbg[192]{};
            sprintf_s(dbg,
                "[BA] network basic-attack intent cleared: target has no netId entity=%u\n",
                static_cast<u32_t>(s_NetworkAttackTarget));
            OutputDebugStringA(dbg);
            Winters::DevSmoke::Log("%s", dbg);
            ClearNetworkAttackIntent();
            outSkipGroundMove = false;
            return;
        }

        Vec3 cursorGround{};
        Vec3 direction{};
        CWorld& world = scene.GetWorld();
        const EntityID player = scene.GetPlayerEntity();
        if (player != NULL_ENTITY &&
            world.HasComponent<TransformComponent>(player) &&
            world.HasComponent<TransformComponent>(s_NetworkAttackTarget))
        {
            const Vec3 origin = world.GetComponent<TransformComponent>(player).GetPosition();
            cursorGround = world.GetComponent<TransformComponent>(s_NetworkAttackTarget).GetPosition();
            const f32_t dx = cursorGround.x - origin.x;
            const f32_t dz = cursorGround.z - origin.z;
            const f32_t lenSq = dx * dx + dz * dz;
            if (lenSq > 0.0001f)
            {
                const f32_t invLen = 1.f / std::sqrtf(lenSq);
                direction = Vec3{ dx * invLen, 0.f, dz * invLen };
            }
        }
        else if (const CDynamicCamera* pCamera = scene.GetCameraPtr())
        {
            (void)pCamera;
            cursorGround = scene.ResolveMouseMapSurfacePos();

            if (player != NULL_ENTITY && world.HasComponent<TransformComponent>(player))
            {
                const Vec3 origin = world.GetComponent<TransformComponent>(player).GetPosition();
                const f32_t dx = cursorGround.x - origin.x;
                const f32_t dz = cursorGround.z - origin.z;
                const f32_t lenSq = dx * dx + dz * dz;
                if (lenSq > 0.0001f)
                {
                    const f32_t invLen = 1.f / std::sqrtf(lenSq);
                    direction = Vec3{ dx * invLen, 0.f, dz * invLen };
                }
            }
        }

        const u32_t attackSeq =
            pCommandSerializer->SendBasicAttack(
                *pNetworkView,
                targetNet,
                cursorGround,
                direction);
        ProtectNetworkAttackYaw(scene, pNetworkView, attackSeq, cursorGround);
        ClearNetworkAttackIntent();
        outSkipGroundMove = true;
    }
}

void CInGameCombatInputBridge::UpdateTargeting(CScene_InGame& scene)
{
    scene.SetHoveredTarget(NULL_ENTITY, eTeam::TEAM_END);

    const CDynamicCamera* pCamera = scene.GetCameraPtr();
    if (!pCamera)
        return;
    if (ImGui::GetIO().WantCaptureMouse)
        return;

    const auto ray = CInput::Get().GetMouseWorldRay(
        *pCamera, static_cast<i32_t>(g_iWinSizeX),
        static_cast<i32_t>(g_iWinSizeY));

    EntityID hoveredEntity = NULL_ENTITY;
    eTeam hoveredTeam = eTeam::TEAM_END;

    GameplayQuery::TryFindHoverTarget(
        scene.GetWorld(),
        scene.GetPlayerEntity(),
        scene.GetPlayerTeam(),
        ray.Origin,
        ray.Dir,
        scene.GetChampionHitRadius(),
        scene.GetChampionHitHeight(),
        hoveredEntity,
        hoveredTeam);
    scene.SetHoveredTarget(hoveredEntity, hoveredTeam);
}

void CInGameCombatInputBridge::UpdateCombatInput(
    CScene_InGame& scene,
    bool& outSkipGroundMove)
{
    outSkipGroundMove = false;

    if (!scene.HasPlayerRenderer())
        return;

    if (scene.IsPlayerStunned())
        return;

    auto& in = CInput::Get();
    const bool bImGuiMouse = ImGui::GetIO().WantCaptureMouse;
    const bool bImGuiKbd = ImGui::GetIO().WantCaptureKeyboard;
    const bool_t bAttackMoveClick =
        !bImGuiMouse && in.IsKeyDown('A') && in.IsLButtonPressed();
    const bool_t bBasicAttackClick =
        !bImGuiMouse && (in.IsRButtonPressed() || bAttackMoveClick);

    if (!bImGuiKbd)
    {
        if (in.IsKeyPressed('P'))
            CGameInstance::Get()->UI_Toggle_InGameShop();

        if (in.IsKeyPressed('B') && scene.IsNetworkAuthoritativeGameplay())
        {
            CCommandSerializer* pCommandSerializer = scene.GetCommandSerializer();
            CClientNetwork* pNetworkView = scene.GetNetworkView();
            if (pCommandSerializer && pNetworkView && pNetworkView->IsConnected())
            {
                ClearNetworkAttackIntent();
                pCommandSerializer->SendRecall(*pNetworkView);
            }
        }

        if (in.IsKeyPressed('Q'))
        {
            ClearNetworkAttackIntent();
            scene.DispatchSkillInput(static_cast<uint8_t>(eSkillSlot::Q));
        }
        if (in.IsKeyPressed('D'))
            scene.TriggerFlash();

        const u8_t wSlot = static_cast<u8_t>(eSkillSlot::W);
        if (in.IsKeyPressed('W'))
        {
            if (!HasPendingSkillStage(scene, wSlot))
            {
                ClearNetworkAttackIntent();
                const bool_t bDispatched = scene.DispatchSkillInput(wSlot);
                s_bWReleasePending = bDispatched && HasPendingSkillStage(scene, wSlot);
            }
        }
        else if (in.IsKeyReleased('W') &&
            (s_bWReleasePending || HasPendingSkillStage(scene, wSlot)))
        {
            ClearNetworkAttackIntent();
            CInGameSkillDispatchBridge::DispatchSkillInput(scene, wSlot, 2u);
            s_bWReleasePending = false;
        }

        if (in.IsKeyPressed('E'))
        {
            ClearNetworkAttackIntent();
            scene.DispatchSkillInput(static_cast<uint8_t>(eSkillSlot::E));
        }

        if (in.IsKeyPressed('R'))
        {
            ClearNetworkAttackIntent();
            scene.DispatchSkillInput(static_cast<uint8_t>(eSkillSlot::R));
        }
    }

    if (scene.IsNetworkAuthoritativeGameplay())
    {
        if (bBasicAttackClick)
        {
            CInGameChampionStateBridge::TryQueueLocalPassiveDashFromCursor(scene);

            const Vec3 vCursorGround = scene.ResolveMouseMapSurfacePos();
            const EntityID target = GameplayQuery::FindAttackTargetNearCursor(
                scene.GetWorld(),
                scene.GetPlayerEntity(),
                scene.GetHoveredEntity(),
                scene.GetPlayerTeam(),
                vCursorGround,
                bAttackMoveClick,
                scene.GetPlayerChampionId(),
                scene.GetBasicAttackRange());

            if (target == NULL_ENTITY)
            {
                if (s_uNetworkAttackMissLogCount < 32u)
                {
                    Winters::DevSmoke::Log(
                        "[BA] network attack intent miss hover=%u hoverTeam=%d playerTeam=%d\n",
                        static_cast<u32_t>(scene.GetHoveredEntity()),
                        static_cast<i32_t>(scene.GetHoveredTeam()),
                        static_cast<i32_t>(scene.GetPlayerTeam()));
                    ++s_uNetworkAttackMissLogCount;
                }
                ClearNetworkAttackIntent();
            }
            else
            {
                s_NetworkAttackTarget = target;
                s_uNetworkAttackCommandFrame = kNetworkAttackCommandIntervalFrames;
                char dbg[128]{};
                sprintf_s(dbg,
                    "[BA] network attack intent target=%u hover=%u\n",
                    static_cast<u32_t>(s_NetworkAttackTarget),
                    static_cast<u32_t>(scene.GetHoveredEntity()));
                Winters::DevSmoke::Log("%s", dbg);
            }
        }

        DriveNetworkAttackIntent(
            scene,
            scene.IsNetworkAuthoritativeGameplay(),
            scene.GetPlayerTeam(),
            scene.GetCommandSerializer(),
            scene.GetNetworkView(),
            scene.GetEntityIdMap(),
            outSkipGroundMove);
        if (outSkipGroundMove)
            return;
    }

    if (!scene.IsNetworkAuthoritativeGameplay() && bBasicAttackClick)
    {
        if (CInGameChampionStateBridge::TryQueueLocalPassiveDashFromCursor(scene))
        {
            outSkipGroundMove = true;
        }
        else
        {
            if (scene.GetLastActionTimer() > 0.f
                && scene.GetLastActionLabel()
                && std::strncmp(scene.GetLastActionLabel(), "attack", 6) == 0)
            {
                scene.PreemptAction("Move");
            }

            const bool fired = scene.DispatchSkillInput(static_cast<uint8_t>(eSkillSlot::BasicAttack));
            if (fired)
                outSkipGroundMove = true;
            else
            {
                scene.PreemptAction("Move");
                char dbg[192];
                sprintf_s(dbg, "[BA Skip] hover=%u hoverTeam=%d playerTeam=%d isEnemy=%d\n",
                    scene.GetHoveredEntity(),
                    static_cast<int>(scene.GetHoveredTeam()),
                    static_cast<int>(scene.GetPlayerTeam()),
                    scene.IsEnemyOfPlayer(scene.GetHoveredEntity()) ? 1 : 0);
                OutputDebugStringA(dbg);
            }
        }
    }
}
