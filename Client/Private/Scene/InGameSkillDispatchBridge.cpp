#include "Network/Client/ClientNetwork.h"

#include "Scene/InGameSkillDispatchBridge.h"

#include "Core/CInput.h"
#include "ECS/Components/GameplayComponents.h"
#include "GameObject/ChampionDef.h"
#include "GamePlay/ChampionCatalog.h"
#include "GamePlay/ChampionRegistry.h"
#include "GamePlay/SkillHookRegistry.h"
#include "GamePlay/SkillRegistry.h"
#include "GamePlay/VisualHookRegistry.h"
#include "Dev/SmokeLog.h"
#include "Network/Client/CommandSerializer.h"
#include "Network/Client/SnapshotApplier.h"
#include "Scene/InGameChampionStateBridge.h"
#include "Scene/Scene_InGame.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"

#include <Windows.h>
#include <cmath>
#include <cstdio>
#include <string>

namespace
{
    const ChampionDef* FindClientChampionDefForBridge(eChampion champion)
    {
        const ChampionCatalogEntry* pEntry = CChampionCatalog::Instance().Find(champion);
        if (pEntry && pEntry->pDef)
            return pEntry->pDef;

        const ChampionDef* pDef = CChampionRegistry::Instance().Find(champion);
        if (pDef)
            return pDef;

        return FindChampionDef(champion);
    }

    void ProtectNetworkBasicAttackYaw(CScene_InGame& scene, u32_t commandSeq)
    {
        CSnapshotApplier* pSnapshotApplier = scene.GetSnapshotApplier();
        CClientNetwork* pNetworkView = scene.GetNetworkView();
        CTransform* pPlayerTransform = scene.GetPlayerTransformPtr();
        if (commandSeq == 0 ||
            !pSnapshotApplier ||
            !pNetworkView ||
            !pPlayerTransform)
        {
            return;
        }

        pSnapshotApplier->ProtectLocalMoveYaw(
            pNetworkView->GetMyNetEntityId(),
            commandSeq,
            pPlayerTransform->GetRotation().y);
    }

    bool_t IsLocalSkillLearned(CScene_InGame& scene, uint8_t slot)
    {
        if (slot == static_cast<uint8_t>(eSkillSlot::BasicAttack))
            return true;

        CWorld& world = scene.GetWorld();
        const EntityID player = scene.GetPlayerEntity();
        if (!world.HasComponent<SkillRankComponent>(player) ||
            slot >= SkillRankComponent::kSlotCount)
        {
            return false;
        }

        return world.GetComponent<SkillRankComponent>(player).ranks[slot] > 0;
    }
}

void CInGameSkillDispatchBridge::SendNetworkSkillCommand(CScene_InGame& scene, u8_t slot,
    const CastSkillCommand& cmd, u8_t skillStage)
{
    if (!scene.m_pNetworkView || !scene.m_pCommandSerializer)
        return;
    if (!scene.m_pNetworkView->IsConnected())
        return;

    NetEntityId targetNet = NULL_NET_ENTITY;
    if (cmd.targetEntityId != NULL_ENTITY && scene.m_pEntityIdMap)
        targetNet = scene.m_pEntityIdMap->ToNet(cmd.targetEntityId);

    if (slot == static_cast<u8_t>(eSkillSlot::BasicAttack))
    {
        const u32_t attackSeq = scene.m_pCommandSerializer->SendBasicAttack(
            *scene.m_pNetworkView,
            targetNet,
            cmd.groundPos,
            cmd.direction);
        ProtectNetworkBasicAttackYaw(scene, attackSeq);
    }
    else
    {
        scene.m_pCommandSerializer->SendCastSkill(
            *scene.m_pNetworkView,
            slot,
            targetNet,
            cmd.groundPos,
            cmd.direction,
            skillStage);
    }
}

bool CInGameSkillDispatchBridge::DispatchSkillInput(
    CScene_InGame& scene,
    uint8_t slot,
    u8_t requestedStage)
{
    if (!scene.m_pPlayerRenderer || scene.m_PlayerEntity == NULL_ENTITY)
    {
        Winters::DevSmoke::Log(
            "[SkillDispatch] rejected slot=%u reason=no-player renderer=%u entity=%u\n",
            static_cast<u32_t>(slot),
            scene.m_pPlayerRenderer ? 1u : 0u,
            static_cast<u32_t>(scene.m_PlayerEntity));
        return false;
    }

    if (slot == static_cast<uint8_t>(eSkillSlot::BasicAttack)
        && scene.m_World.HasComponent<DisarmComponent>(scene.m_PlayerEntity))
        return false;

    using namespace Engine;
    const eChampion champ = scene.GetPlayerChampionId();
    const SkillDef* def = CSkillRegistry::Instance().Find(champ, slot);
    if (!def)
        def = FindSkillDef(champ, slot);
    if (!def)
    {
        Winters::DevSmoke::Log(
            "[SkillDispatch] rejected slot=%u champ=%u reason=no-def\n",
            static_cast<u32_t>(slot),
            static_cast<u32_t>(champ));
        return false;
    }

    if (!scene.m_World.HasComponent<SkillStateComponent>(scene.m_PlayerEntity))
    {
        Winters::DevSmoke::Log(
            "[SkillDispatch] rejected slot=%u champ=%u reason=no-skill-state entity=%u\n",
            static_cast<u32_t>(slot),
            static_cast<u32_t>(champ),
            static_cast<u32_t>(scene.m_PlayerEntity));
        return false;
    }

    if (!IsLocalSkillLearned(scene, slot))
    {
        Winters::DevSmoke::Log(
            "[SkillDispatch] rejected slot=%u champ=%u reason=unlearned\n",
            static_cast<u32_t>(slot),
            static_cast<u32_t>(champ));
        return false;
    }

    if (!CInGameChampionStateBridge::ValidateLocalSkillStart(scene, *def))
        return false;

    auto& slotState = scene.m_World.GetComponent<SkillStateComponent>(scene.m_PlayerEntity).slots[slot];

    const bool_t bRequestedStage2 = requestedStage >= 2u;
    const bool_t bLocalStage2Ready =
        slotState.currentStage == 1 && slotState.stageWindow > 0.f;

    if (def->stageCount == 2 && (bLocalStage2Ready || bRequestedStage2))
    {
        SkillDef s2 = *def;
        s2.targetMode = def->stage2TargetMode;
        s2.animKey = def->stage2AnimKey ? def->stage2AnimKey : def->animKey;
        s2.lockDurationSec = def->stage2LockSec > 0.f ? def->stage2LockSec : def->lockDurationSec;
        s2.rotate = def->stage2Rotate;
        s2.animPlaySpeed = def->stage2PlaySpeed;
        s2.castFrame = def->stage2CastFrame;
        s2.recoveryFrame = def->stage2RecoveryFrame;
        s2.stageCount = 1;

        CastSkillCommand cmd{};
        cmd.slot = slot;
        if (!BuildCastCommand(scene, s2, cmd))
            return false;

        if (scene.m_bNetworkAuthoritativeGameplay)
            RotatePlayerToward(scene, s2.rotate, cmd);

        SendNetworkSkillCommand(scene, slot, cmd, 2);
        if (bRequestedStage2 && !bLocalStage2Ready)
        {
            Winters::DevSmoke::Log(
                "[SkillDispatch] forced stage2 slot=%u champ=%u localWindow=%.2f\n",
                static_cast<u32_t>(slot),
                static_cast<u32_t>(champ),
                slotState.stageWindow);
        }

        if (scene.m_bNetworkAuthoritativeGameplay)
        {
            slotState.currentStage = 0;
            slotState.stageWindow = 0.f;
            return true;
        }

        ApplyLocalPrediction(scene, cmd, s2, 2);

        slotState.currentStage = 0;
        slotState.stageWindow = 0.f;
        slotState.cooldownRemaining = def->cooldownSec;
        slotState.cooldownDuration = def->cooldownSec;
        return true;
    }

    if (slot != static_cast<uint8_t>(eSkillSlot::BasicAttack)
        && slotState.cooldownRemaining > 0.f)
    {
        return false;
    }

    CastSkillCommand cmd{};
    cmd.slot = slot;
    if (!BuildCastCommand(scene, *def, cmd))
    {
        Winters::DevSmoke::Log(
            "[SkillDispatch] rejected slot=%u champ=%u mode=%u reason=build-command\n",
            static_cast<u32_t>(slot),
            static_cast<u32_t>(champ),
            static_cast<u32_t>(def->targetMode));
        return false;
    }

    if (scene.m_bNetworkAuthoritativeGameplay)
    {
        RotatePlayerToward(scene, def->rotate, cmd);
        SendNetworkSkillCommand(scene, slot, cmd, 1);
        if (def->stageCount == 2)
        {
            slotState.currentStage = 1;
            slotState.stageWindow = def->stageWindowSec;
        }
        return true;
    }

    if (def->stageCount == 2)
    {
        SendNetworkSkillCommand(scene, slot, cmd, 1);
        ApplyLocalPrediction(scene, cmd, *def, 1);

        slotState.currentStage = 1;
        slotState.stageWindow = def->stageWindowSec;
        return true;
    }

    SendNetworkSkillCommand(scene, slot, cmd, 1);
    ApplyLocalPrediction(scene, cmd, *def);
    Winters::DevSmoke::Log(
        "[SkillDispatch] accepted slot=%u champ=%u hook=0x%08X anim=%s\n",
        static_cast<u32_t>(slot),
        static_cast<u32_t>(champ),
        def->castFrameHookId,
        def->animKey ? def->animKey : "(null)");
    if (slot != static_cast<uint8_t>(eSkillSlot::BasicAttack))
    {
        slotState.cooldownRemaining = def->cooldownSec;
        slotState.cooldownDuration = def->cooldownSec;
    }

    return true;
}

bool CInGameSkillDispatchBridge::BuildCastCommand(
    CScene_InGame& scene,
    const SkillDef& def,
    CastSkillCommand& outCmd)
{
    eTargetMode mode = def.targetMode;

    if (mode == eTargetMode::Conditional)
    {
        mode = eTargetMode::Direction;
    }

    outCmd.resolvedTargetMode = static_cast<uint8_t>(mode);

    switch (mode)
    {
    case eTargetMode::Self:
    {
        outCmd.targetEntityId = scene.m_PlayerEntity;
        return true;
    }
    case eTargetMode::UnitTarget:
    {
        if (!scene.IsEnemyOfPlayer(scene.m_HoveredEntity))
            return false;
        outCmd.targetEntityId = scene.m_HoveredEntity;
        return true;
    }
    case eTargetMode::GroundTarget:
    {
        if (!scene.m_pCamera) return false;
        Vec3 ground = scene.ResolveMouseMapSurfacePos();
        outCmd.groundPos = ground;
        return true;
    }
    case eTargetMode::Direction:
    {
        if (!scene.m_pCamera) return false;
        Vec3 cursor = scene.ResolveMouseMapSurfacePos();
        const Vec3 origin = scene.m_pPlayerTransform ? scene.m_pPlayerTransform->GetPosition() : Vec3{};
        f32_t dx = cursor.x - origin.x;
        f32_t dz = cursor.z - origin.z;
        f32_t len2 = dx * dx + dz * dz;

        if (len2 < 1e-3f)
        {
            Vec3 fwd = scene.m_pCamera->GetForward();
            dx = fwd.x;
            dz = fwd.z;
            len2 = dx * dx + dz * dz;
            if (len2 < 1e-4f) return false;
        }

        const f32_t len = sqrtf(len2);
        outCmd.direction = { dx / len, 0.f, dz / len };
        return true;
    }
    default:
        return false;
    }
}

void CInGameSkillDispatchBridge::ApplyLocalPrediction(
    CScene_InGame& scene,
    const CastSkillCommand& cmd,
    const SkillDef& def,
    u8_t skillStage)
{
    if (scene.m_bNetworkAuthoritativeGameplay)
    {
        Winters::DevSmoke::Log(
            "[SkillDispatch] local prediction blocked in network authority slot=%u\n",
            static_cast<u32_t>(def.slot));
        return;
    }

    RotatePlayerToward(scene, def.rotate, cmd);

    if (def.animKey && scene.m_pPlayerRenderer)
    {
        using namespace Engine;
        const eChampion champ = scene.GetPlayerChampionId();
        const ChampionDef* cd = FindClientChampionDefForBridge(champ);
        if (cd)
        {
            std::string key = def.animKey;
            if (key == "attack") key = cd->basicAttackKey;

            if (def.keySwapHookId != 0)
            {
                VisualHookContext visualCtx{};
                visualCtx.pWorld = &scene.m_World;
                visualCtx.casterEntity = scene.m_PlayerEntity;
                visualCtx.pDef = &def;
                visualCtx.pCommand = &cmd;
                visualCtx.pKeyOut = &key;
                visualCtx.pFxMeshRenderer = scene.m_pFxMeshRenderer.get();
                const bool visualKeyHandled =
                    CVisualHookRegistry::Instance().Dispatch(def.keySwapHookId, visualCtx);

                if (!visualKeyHandled)
                {
                    SkillHookContext ctx{};
                    ctx.pWorld = &scene.m_World;
                    ctx.casterEntity = scene.m_PlayerEntity;
                    ctx.casterTeam = scene.m_PlayerTeam;
                    ctx.pDef = &def;
                    ctx.pCommand = &cmd;
                    ctx.skillStage = skillStage;
                    ctx.pKeyOut = &key;
                    ctx.pFxMeshRenderer = scene.m_pFxMeshRenderer.get();
                    CSkillHookRegistry::Instance().Dispatch(def.keySwapHookId, ctx);
                }
            }

            const std::string full = std::string(cd->animPrefix) + key;
            scene.m_pPlayerRenderer->PlayAnimationByName(full, !def.bOneShot);

            scene.m_pLastActionLabel = def.animKey;
            scene.m_fLastActionTimer = def.lockDurationSec > 0.f ? def.lockDurationSec : 1.2f;

            scene.m_ActiveSkillDefStorage = def;
            scene.m_ActiveSkillCommandStorage = cmd;
            scene.m_pActiveSkillDef = &scene.m_ActiveSkillDefStorage;
            scene.m_fActivePrevFrame = 0.f;
            scene.m_bCastFrameFired = false;
            scene.m_bRecoveryFrameFired = false;
            CInGameChampionStateBridge::ResetLocalSkillRuntimeState(scene);
            scene.m_pLastDispatchedSkill = &scene.m_ActiveSkillDefStorage;
        }
    }

    bool acceptedHandled = false;
    if (def.onCastAcceptedHookId != 0)
    {
        GameCommand gameCommand{};
        gameCommand.kind = (def.slot == static_cast<uint8_t>(eSkillSlot::BasicAttack))
            ? eCommandKind::BasicAttack
            : eCommandKind::CastSkill;
        gameCommand.issuerEntity = scene.m_PlayerEntity;
        gameCommand.slot = def.slot;
        gameCommand.targetEntity = cmd.targetEntityId;
        gameCommand.groundPos = cmd.groundPos;
        gameCommand.direction = cmd.direction;

        TickContext tickCtx{};
        tickCtx.fDt = 0.f;
        tickCtx.localPlayer = scene.m_PlayerEntity;

        GameplayHookContext gameCtx{};
        gameCtx.pWorld = &scene.m_World;
        gameCtx.casterEntity = scene.m_PlayerEntity;
        gameCtx.casterTeam = scene.m_PlayerTeam;
        gameCtx.casterChampion = def.champ;
        gameCtx.skillRank = 1;
        gameCtx.pDef = &def;
        gameCtx.pCommand = &gameCommand;
        gameCtx.pTickCtx = &tickCtx;
        const bool gameplayAcceptedHandled =
            CGameplayHookRegistry::Instance().Dispatch(def.onCastAcceptedHookId, gameCtx);

        VisualHookContext visualCtx{};
        visualCtx.pWorld = &scene.m_World;
        visualCtx.casterEntity = scene.m_PlayerEntity;
        visualCtx.pDef = &def;
        visualCtx.pCommand = &cmd;
        visualCtx.skillStage = skillStage;
        visualCtx.pFxMeshRenderer = scene.m_pFxMeshRenderer.get();
        const bool hasLegacyAcceptedHook =
            CSkillHookRegistry::Instance().Has(def.onCastAcceptedHookId);
        const bool suppressVisualAcceptedForLegacy =
            hasLegacyAcceptedHook && def.champ == eChampion::IRELIA;
        bool visualAcceptedHandled = false;
        if (!suppressVisualAcceptedForLegacy)
        {
            visualAcceptedHandled =
                CVisualHookRegistry::Instance().Dispatch(def.onCastAcceptedHookId, visualCtx);
        }

        SkillHookContext ctx{};
        ctx.pWorld = &scene.m_World;
        ctx.casterEntity = scene.m_PlayerEntity;
        ctx.casterTeam = scene.m_PlayerTeam;
        ctx.pDef = &def;
        ctx.pCommand = &cmd;
        ctx.skillStage = skillStage;
        ctx.pFxMeshRenderer = scene.m_pFxMeshRenderer.get();
        ctx.startPointDash = [&scene](const Vec3& start, const Vec3& end, f32_t duration, EntityID target)
            {
                scene.m_bDashActive = true;
                scene.m_fDashElapsed = 0.f;
                scene.m_fDashDuration = duration;
                scene.m_vDashStart = start;
                scene.m_vDashEnd = end;
                scene.m_DashTargetEntity = target;
            };
        ctx.startTargetDash = [&scene](EntityID target)
            {
                CInGameChampionStateBridge::StartLocalTargetDash(scene, target);
            };
        ctx.startUltimateDash = [&scene](EntityID target)
            {
                CInGameChampionStateBridge::StartLocalUltimateDash(scene, target);
            };
        ctx.findAirborneTarget = [&scene](const Vec3& origin, f32_t radius) -> EntityID
            {
                return CInGameChampionStateBridge::FindAirborneEnemyNear(scene, origin, radius);
            };
        ctx.applyTargetDamage = [&scene](EntityID target, f32_t damage)
            {
                CInGameChampionStateBridge::ApplyLocalChampionDamage(
                    scene,
                    target,
                    damage,
                    "SkillHookDamage");
            };
        ctx.setLocalLoopAnimations = [&scene](const char* idle, const char* run, bool_t playNow)
            {
                scene.m_pPlayerIdleAnim = idle;
                scene.m_pPlayerRunAnim = run;
                if (playNow && scene.m_pPlayerRenderer)
                    scene.m_pPlayerRenderer->PlayAnimationByName(idle, true);
            };
        ctx.getLocalDashDuration = [&scene]() -> f32_t
            {
                return scene.m_fDashDuration;
            };
        const bool legacyAcceptedHandled =
            CSkillHookRegistry::Instance().Dispatch(def.onCastAcceptedHookId, ctx);

        acceptedHandled = gameplayAcceptedHandled || visualAcceptedHandled || legacyAcceptedHandled;
        Winters::DevSmoke::Log(
            "[SkillDispatch] acceptedHook slot=%u champ=%u hook=0x%08X stage=%u gameplay=%u visual=%u legacy=%u\n",
            static_cast<u32_t>(def.slot),
            static_cast<u32_t>(def.champ),
            def.onCastAcceptedHookId,
            static_cast<u32_t>(skillStage),
            gameplayAcceptedHandled ? 1u : 0u,
            visualAcceptedHandled ? 1u : 0u,
            legacyAcceptedHandled ? 1u : 0u);
    }
    (void)acceptedHandled;

    using namespace Engine;

    scene.m_bCastFrameFired = false;
    scene.m_bRecoveryFrameFired = false;
    CInGameChampionStateBridge::ResetLocalSkillRuntimeState(scene);

    scene.m_pActiveSkillDef = &scene.m_ActiveSkillDefStorage;
    scene.m_fActivePrevFrame = 0.f;

    char buf[192];
    const char* modeName = "?";
    switch (static_cast<eTargetMode>(cmd.resolvedTargetMode))
    {
    case eTargetMode::Self:         modeName = "Self";         break;
    case eTargetMode::UnitTarget:   modeName = "UnitTarget";   break;
    case eTargetMode::GroundTarget: modeName = "GroundTarget"; break;
    case eTargetMode::Direction:    modeName = "Direction";    break;
    default: break;
    }
    sprintf_s(buf, "[Cast] slot=%u mode=%s anim=%s target=%u ground=(%.1f,%.1f,%.1f) dir=(%.2f,%.2f)\n",
        cmd.slot, modeName, def.animKey ? def.animKey : "(null)",
        cmd.targetEntityId,
        cmd.groundPos.x, cmd.groundPos.y, cmd.groundPos.z,
        cmd.direction.x, cmd.direction.z);
    Winters::DevSmoke::Log("%s", buf);
}

void CInGameSkillDispatchBridge::RotatePlayerToward(
    CScene_InGame& scene,
    eRotateMode mode,
    const CastSkillCommand& cmd)
{
    if (mode == eRotateMode::None || !scene.m_pPlayerTransform) return;

    const Vec3 origin = scene.m_pPlayerTransform->GetPosition();
    Vec3 target = origin;

    if (mode == eRotateMode::TowardsTarget
        && cmd.targetEntityId != NULL_ENTITY
        && scene.m_World.HasComponent<TransformComponent>(cmd.targetEntityId))
    {
        target = scene.m_World.GetComponent<TransformComponent>(cmd.targetEntityId).m_LocalPosition;
    }
    else if (mode == eRotateMode::TowardsCursor)
    {
        const bool bHasDir = (fabsf(cmd.direction.x) + fabsf(cmd.direction.z)) > 1e-4f;
        target = bHasDir
            ? Vec3{ origin.x + cmd.direction.x, origin.y, origin.z + cmd.direction.z }
        : cmd.groundPos;
    }

    const f32_t dx = target.x - origin.x;
    const f32_t dz = target.z - origin.z;
    if (dx * dx + dz * dz < 1e-4f) return;

    const f32_t yaw = ResolveChampionVisualYawNear(
        scene.GetPlayerChampionId(),
        Vec3{ dx, 0.f, dz },
        scene.GetPlayerYaw());
    scene.SetPlayerYaw(yaw);

    if (scene.GetPlayerChampionId() == eChampion::KALISTA &&
        (cmd.slot == static_cast<u8_t>(eSkillSlot::BasicAttack) ||
            cmd.slot == static_cast<u8_t>(eSkillSlot::Q)))
    {
        scene.m_vKalistaPassiveDashFaceDir =
            WintersMath::NormalizeXZ(Vec3{ dx, 0.f, dz }, Vec3{}, 0.0001f);
        scene.m_bKalistaPassiveDashHasFaceDir =
            scene.m_vKalistaPassiveDashFaceDir.x != 0.f ||
            scene.m_vKalistaPassiveDashFaceDir.z != 0.f;
    }
}
