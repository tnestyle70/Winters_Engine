#include "Scene/InGameChampionStateBridge.h"

#include "Core/CInput.h"
#include "ECS/Components/NavAgentComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/GameplayComponents.h"
#include "GameObject/Champion/Annie/Annie_Components.h"
#include "GameObject/Champion/Ashe/Ashe_Components.h"
#include "GameObject/Champion/Irelia/Irelia_Skills.h"
#include "GameObject/Champion/Jax/Jax_Components.h"
#include "GameObject/Champion/Kalista/Kalista_Skills.h"
#include "GameObject/Champion/Kalista/Kalista_Tuning.h"
#include "GamePlay/SkillRegistry.h"
#include "Resource/Animator.h"
#include "Scene/Scene_InGame.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/NetAnimationComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"

#include <Windows.h>
#include <cmath>
#include <cstdio>

namespace
{
    void NotifyTowerAggroOnChampionHit(CWorld& world, EntityID attacker, EntityID victim)
    {
        if (attacker == NULL_ENTITY || victim == NULL_ENTITY)
            return;
        if (!world.IsAlive(attacker) || !world.IsAlive(victim))
            return;
        if (!world.HasComponent<ChampionComponent>(attacker) ||
            !world.HasComponent<ChampionComponent>(victim))
        {
            return;
        }

        TowerAggroNotifyComponent notify{};
        notify.attackerEntity = attacker;
        notify.victimEntity = victim;
        notify.priorityDuration = 2.0f;

        if (world.HasComponent<TowerAggroNotifyComponent>(attacker))
            world.GetComponent<TowerAggroNotifyComponent>(attacker) = notify;
        else
            world.AddComponent<TowerAggroNotifyComponent>(attacker, notify);
    }
}

void CInGameChampionStateBridge::Update(CScene_InGame& scene, f32_t dt)
{
    scene.m_World.ForEach<YasuoStateComponent>(
        [dt](EntityID, YasuoStateComponent& ys)
        {
            if (ys.qStackTimer > 0.f)
            {
                ys.qStackTimer -= dt;
                if (ys.qStackTimer <= 0.f) ys.qStackCount = 0;
            }
            if (ys.eActiveTimer > 0.f)
            {
                ys.eActiveTimer -= dt;
                if (ys.eActiveTimer <= 0.f) ys.bEActive = false;
            }
        });

    if (scene.m_PlayerEntity != NULL_ENTITY
        && scene.m_World.HasComponent<RivenStateComponent>(scene.m_PlayerEntity))
    {
        auto& rs = scene.m_World.GetComponent<RivenStateComponent>(scene.m_PlayerEntity);
        if (rs.qStackTimer > 0.f)
        {
            rs.qStackTimer -= dt;
            if (rs.qStackTimer <= 0.f)
            {
                rs.qStackTimer = 0.f;
                rs.qStackCount = 0;
            }
        }

        if (rs.bUlted)
        {
            rs.fUltTimer -= dt;
            if (rs.fUltTimer <= 0.f)
            {
                rs.bUlted = false;
                rs.fUltTimer = 0.f;
                scene.m_pPlayerIdleAnim = "riven_idle1";
                scene.m_pPlayerRunAnim = "riven_run";
                if (scene.m_pPlayerRenderer)
                {
                    scene.m_pPlayerRenderer->PlayAnimationByName(
                        scene.m_bMoving ? scene.m_pPlayerRunAnim : scene.m_pPlayerIdleAnim,
                        true);
                }
            }
        }

        if (rs.fShieldTimer > 0.f)
        {
            rs.fShieldTimer -= dt;
            if (rs.fShieldTimer <= 0.f)
            {
                rs.fShieldTimer = 0.f;
                rs.fShieldRemaining = 0.f;
            }
        }
    }

    if (scene.m_PlayerEntity != NULL_ENTITY
        && scene.m_World.HasComponent<JaxStateComponent>(scene.m_PlayerEntity))
    {
        auto& js = scene.m_World.GetComponent<JaxStateComponent>(scene.m_PlayerEntity);
        if (js.bEmpowerActive && js.fEmpowerTimer > 0.f)
        {
            js.fEmpowerTimer -= dt;
            if (js.fEmpowerTimer <= 0.f)
            {
                js.bEmpowerActive = false;
                js.fEmpowerTimer = 0.f;
            }
        }

        if (js.bCounterActive && js.fCounterTimer > 0.f)
        {
            js.fCounterTimer -= dt;
            if (js.fCounterTimer <= 0.f)
            {
                js.bCounterActive = false;
                js.fCounterTimer = 0.f;
            }
        }

        if (js.bUltActive && js.fUltTimer > 0.f)
        {
            js.fUltTimer -= dt;
            if (js.fUltTimer <= 0.f)
            {
                js.bUltActive = false;
                js.fUltTimer = 0.f;
                js.ultAttackCounter = 0;
            }
        }
    }

    if (scene.m_PlayerEntity != NULL_ENTITY
        && scene.m_World.HasComponent<AnnieStateComponent>(scene.m_PlayerEntity))
    {
        auto& as = scene.m_World.GetComponent<AnnieStateComponent>(scene.m_PlayerEntity);
        if (as.bEShieldActive && as.fEShieldTimer > 0.f)
        {
            as.fEShieldTimer -= dt;
            if (as.fEShieldTimer <= 0.f)
            {
                as.bEShieldActive = false;
                as.fEShieldTimer = 0.f;
            }
        }

        if (as.bTibbersActive && as.fTibbersTimer > 0.f)
        {
            as.fTibbersTimer -= dt;
            if (as.fTibbersTimer <= 0.f)
            {
                as.bTibbersActive = false;
                as.fTibbersTimer = 0.f;
                as.vTibbersPos = {};
            }
        }
    }

    if (scene.m_PlayerEntity != NULL_ENTITY
        && scene.m_World.HasComponent<AsheStateComponent>(scene.m_PlayerEntity))
    {
        auto& as = scene.m_World.GetComponent<AsheStateComponent>(scene.m_PlayerEntity);
        if (as.bQActive && as.fQTimer > 0.f)
        {
            as.fQTimer -= dt;
            if (as.fQTimer <= 0.f)
            {
                as.bQActive = false;
                as.fQTimer = 0.f;
            }
        }
    }
}

void CInGameChampionStateBridge::UpdateLocalRuntime(CScene_InGame& scene, f32_t dt)
{
    if (scene.m_pIreliaBladeSystem)
        scene.m_pIreliaBladeSystem->Execute(scene.m_World, dt);
    if (scene.m_pWindWallSystem)
        scene.m_pWindWallSystem->Execute(scene.m_World, dt);

    if (scene.m_bKalistaPassiveDashActive)
        UpdateLocalPassiveDash(scene, dt);

    if (scene.m_bYasuoDashActive)
        UpdateLocalTargetDash(scene, dt);
    if (scene.m_bYasuoRActive)
        UpdateLocalUltimateSequence(scene, dt);

    if (scene.m_pPendingHitSystem)
        scene.m_pPendingHitSystem->Execute(scene.m_World, dt);
    if (scene.m_pYasuoProjectileSystem)
        scene.m_pYasuoProjectileSystem->Execute(scene.m_World, dt);
    if (scene.m_pKalistaProjectileSystem)
        scene.m_pKalistaProjectileSystem->Execute(scene.m_World, dt);
    if (scene.m_pKalistaRendSystem)
        scene.m_pKalistaRendSystem->Execute(scene.m_World, dt);

    Irelia::UpdateLocalBladeState(
        scene.m_World,
        scene.m_pFxMeshRenderer.get(),
        scene.m_PlayerEntity,
        scene.m_PlayerTeam,
        dt,
        scene.ResolveMouseMapSurfacePos(),
        !scene.m_bNetworkAuthoritativeGameplay);
}

bool_t CInGameChampionStateBridge::CanResumeBaseAnimation(const CScene_InGame& scene)
{
    return !scene.m_bKalistaPassiveDashActive && !scene.m_bKalistaPassiveDashAnimActive;
}

bool_t CInGameChampionStateBridge::IsLocalActionProtected(const CScene_InGame& scene)
{
    return scene.m_fLastActionTimer > 0.f ||
        scene.m_fEndTransitionTimer > 0.f ||
        scene.m_bDashActive ||
        scene.m_bKalistaPassiveDashActive;
}

void CInGameChampionStateBridge::UpdateLocalPostAnimation(CScene_InGame& scene)
{
    if (scene.m_bKalistaPassiveDashAnimActive && !scene.m_bKalistaPassiveDashActive)
    {
        const Engine::CAnimator* pAnim = scene.m_pPlayerRenderer
            ? scene.m_pPlayerRenderer->GetAnimator()
            : nullptr;
        if (!pAnim || !pAnim->IsPlaying())
        {
            if (scene.m_pPlayerRenderer && !scene.m_bNetworkAuthoritativeGameplay)
            {
                scene.m_pPlayerRenderer->PlayAnimationByName(
                    scene.m_bMoving ? scene.m_pPlayerRunAnim : scene.m_pPlayerIdleAnim);
            }
            scene.m_bKalistaPassiveDashAnimActive = false;
        }
    }
}

void CInGameChampionStateBridge::ResetLocalSkillRuntimeState(CScene_InGame& scene)
{
    Kalista::ClearPassiveDashRequest();
    scene.m_bKalistaPassiveDashAnimActive = false;
    scene.m_bKalistaPassiveDashMoveCommandPending = false;
    scene.m_bKalistaPassiveDashTriggerAfterMove = false;
    scene.m_uKalistaPassiveDashTriggerAnimId = 0;
    scene.m_uKalistaPassiveDashTriggerActionSeq = 0;
    scene.m_vKalistaPassiveDashFaceDir = {};
    scene.m_bKalistaPassiveDashHasFaceDir = false;
}

bool_t CInGameChampionStateBridge::TryQueueLocalPassiveDashFromCursor(CScene_InGame& scene)
{
    const eChampion champ = scene.GetPlayerChampionId();
    if (champ != eChampion::KALISTA)
        return false;

    u8_t passiveSlot = static_cast<u8_t>(eSkillSlot::BasicAttack);
    bool_t bNetworkGraceWindow = false;
    u16_t networkAnimId = 0;
    u32_t networkActionSeq = 0;
    bool_t bPassiveDashWindow =
        scene.m_pActiveSkillDef &&
        (scene.m_pActiveSkillDef->slot == 0 || scene.m_pActiveSkillDef->slot == 1) &&
        !scene.m_bRecoveryFrameFired;

    if (bPassiveDashWindow)
    {
        passiveSlot = scene.m_pActiveSkillDef->slot;
    }
    else if (scene.m_bNetworkAuthoritativeGameplay && scene.m_PlayerEntity != NULL_ENTITY)
    {
        const auto it = scene.m_NetworkActionAnimStates.find(scene.m_PlayerEntity);
        if (it != scene.m_NetworkActionAnimStates.end())
        {
            const auto animId = static_cast<eNetAnimId>(it->second.animId);
            const bool_t bPassiveAnim =
                animId == eNetAnimId::BasicAttack || animId == eNetAnimId::SkillQ;
            const bool_t bGraceOpen =
                !it->second.bActionActive && it->second.passiveDashInputGraceSec > 0.f;
            if (bPassiveAnim &&
                !it->second.bPassiveDashTriggered &&
                (it->second.bActionActive || bGraceOpen))
            {
                passiveSlot = (animId == eNetAnimId::SkillQ)
                    ? static_cast<u8_t>(eSkillSlot::Q)
                    : static_cast<u8_t>(eSkillSlot::BasicAttack);
                bPassiveDashWindow = true;
                bNetworkGraceWindow = bGraceOpen;
                networkAnimId = it->second.animId;
                networkActionSeq = it->second.actionSeq;
            }
        }
    }

    if (!bPassiveDashWindow || !scene.m_pPlayerTransform || !scene.m_pCamera)
        return false;

    const Vec3 origin = scene.m_pPlayerTransform->GetPosition();
    Vec3 dashTarget = scene.ResolveMouseMapSurfacePos();

    if (passiveSlot == static_cast<u8_t>(eSkillSlot::BasicAttack) &&
        std::fabsf(dashTarget.x) + std::fabsf(dashTarget.z) <= 0.001f)
    {
        const Vec3 forward = scene.GetPlayerForward();
        dashTarget = {
            origin.x + forward.x,
            origin.y,
            origin.z + forward.z
        };
    }

    const f32_t dx = dashTarget.x - origin.x;
    const f32_t dz = dashTarget.z - origin.z;
    const f32_t len = std::sqrtf(dx * dx + dz * dz);
    if (len > 0.1f)
    {
        const Vec3 dashDir{ dx / len, 0.f, dz / len };
        Vec3 faceDir = scene.m_bKalistaPassiveDashHasFaceDir
            ? scene.m_vKalistaPassiveDashFaceDir
            : scene.GetPlayerForward();

        if (!scene.m_bKalistaPassiveDashHasFaceDir && scene.m_pActiveSkillDef)
        {
            const auto& activeCmd = scene.m_ActiveSkillCommandStorage;
            if (passiveSlot == static_cast<u8_t>(eSkillSlot::BasicAttack) &&
                activeCmd.targetEntityId != NULL_ENTITY &&
                scene.m_World.HasComponent<TransformComponent>(activeCmd.targetEntityId))
            {
                const Vec3 targetPos =
                    scene.m_World.GetComponent<TransformComponent>(activeCmd.targetEntityId).GetPosition();
                faceDir = WintersMath::DirectionXZ(origin, targetPos, faceDir);
            }
            else if (activeCmd.direction.x != 0.f || activeCmd.direction.z != 0.f)
            {
                faceDir = activeCmd.direction;
            }
        }

        scene.SetKalistaPassiveDashFaceDir(
            WintersMath::NormalizeXZ(faceDir, scene.GetPlayerForward(), 0.0001f));
        Kalista::QueuePassiveDash(dashDir);
        if (scene.m_bNetworkAuthoritativeGameplay)
        {
            scene.m_bKalistaPassiveDashMoveCommandPending = true;
            scene.m_bKalistaPassiveDashTriggerAfterMove = bNetworkGraceWindow;
            scene.m_uKalistaPassiveDashTriggerAnimId = networkAnimId;
            scene.m_uKalistaPassiveDashTriggerActionSeq = networkActionSeq;
        }

        char dbg[160]{};
        sprintf_s(dbg, "[KalistaPassive] pending move-dir=(%.2f,0,%.2f) slot=%d\n",
            dashDir.x,
            dashDir.z,
            passiveSlot);
        ::OutputDebugStringA(dbg);
    }

    return true;
}

bool_t CInGameChampionStateBridge::TriggerNetworkPassiveDashFromAction(
    CScene_InGame& scene,
    u16_t animId,
    u32_t actionSeq,
    bool_t bServerDashLikely)
{
    if (scene.GetPlayerChampionId() != eChampion::KALISTA)
        return false;
    if (scene.m_PlayerEntity == NULL_ENTITY)
        return false;

    const auto netAnim = static_cast<eNetAnimId>(animId);
    const u8_t slot = (netAnim == eNetAnimId::SkillQ)
        ? static_cast<u8_t>(eSkillSlot::Q)
        : static_cast<u8_t>(eSkillSlot::BasicAttack);

    if (netAnim != eNetAnimId::BasicAttack && netAnim != eNetAnimId::SkillQ)
        return false;

    if (!Kalista::HasPassiveDashRequest())
        return false;

    if (actionSeq != 0u &&
        scene.m_uKalistaLastPassiveDashActionSeq == actionSeq)
    {
        Kalista::ClearPassiveDashRequest();
        char dbg[160]{};
        sprintf_s(dbg,
            "[KalistaPassive] skip duplicate dash actionSeq=%u animId=%u\n",
            actionSeq,
            static_cast<u32_t>(animId));
        ::OutputDebugStringA(dbg);
        return false;
    }

    if (scene.m_bKalistaPassiveDashActive ||
        scene.m_bKalistaPassiveDashAnimActive)
    {
        Kalista::ClearPassiveDashRequest();
        char dbg[160]{};
        sprintf_s(dbg,
            "[KalistaPassive] skip dash while active actionSeq=%u animId=%u\n",
            actionSeq,
            static_cast<u32_t>(animId));
        ::OutputDebugStringA(dbg);
        return false;
    }

    const SkillDef* pDef = CSkillRegistry::Instance().Find(eChampion::KALISTA, slot);
    if (!pDef)
        pDef = FindSkillDef(eChampion::KALISTA, slot);
    if (!pDef)
        return false;

    SkillHookContext ctx{};
    ctx.pWorld = &scene.m_World;
    ctx.casterEntity = scene.m_PlayerEntity;
    ctx.casterTeam = scene.m_PlayerTeam;
    ctx.pDef = pDef;
    ctx.pCasterRenderer = scene.m_pPlayerRenderer;
    ctx.fGlobalAnimSpeed = scene.m_fGlobalAnimSpeed;
    ctx.actionSeq = actionSeq;
    ctx.startLocalDash = [&scene](const Vec3& dir)
        {
            CInGameChampionStateBridge::StartLocalPassiveDash(scene, dir);
        };
    ctx.setLocalDashDuration = [](f32_t duration)
        {
            CInGameChampionStateBridge::SetLocalPassiveDashDuration(duration);
        };
    ctx.getLocalDashDuration = []() -> f32_t
        {
            return CInGameChampionStateBridge::GetLocalPassiveDashDuration();
        };
    ctx.setLocalActionAnimActive = [&scene](bool_t active)
        {
            CInGameChampionStateBridge::SetLocalActionAnimActive(scene, active);
        };
    ctx.bPlayPassiveDashAnimation = true;

    const bool_t bWasDashActive =
        scene.m_bKalistaPassiveDashActive ||
        scene.m_bKalistaPassiveDashAnimActive;
    Kalista::OnRecoveryFrame_PassiveDash(ctx);
    const bool_t bDashStarted =
        !bWasDashActive &&
        (scene.m_bKalistaPassiveDashActive ||
            scene.m_bKalistaPassiveDashAnimActive);
    if (bDashStarted)
        scene.m_uKalistaLastPassiveDashActionSeq = actionSeq;
    return bDashStarted;
}

bool_t CInGameChampionStateBridge::ValidateLocalSkillStart(CScene_InGame& scene, const SkillDef& def)
{
    if (def.champ == eChampion::YASUO
        && def.slot == static_cast<uint8_t>(eSkillSlot::R))
    {
        if (scene.m_bNetworkAuthoritativeGameplay)
            return true;

        if (!scene.m_pPlayerTransform)
            return false;

        const EntityID airborne = FindAirborneEnemyNear(
            scene,
            scene.m_pPlayerTransform->GetPosition(),
            scene.GetYasuoRSearchRadius());
        if (airborne == NULL_ENTITY)
        {
            OutputDebugStringA("[Yasuo R] No airborne (Stun) target - input rejected\n");
            return false;
        }
    }

    return true;
}

void CInGameChampionStateBridge::StartLocalTargetDash(CScene_InGame& scene, EntityID target)
{
    if (!scene.m_pPlayerTransform) return;
    if (!scene.m_World.HasComponent<TransformComponent>(target)) return;

    const Vec3 targetPos = scene.m_World.GetComponent<TransformComponent>(target).m_LocalPosition;
    scene.m_vYasuoDashStart = scene.m_pPlayerTransform->GetPosition();
    scene.m_vYasuoDashEnd = { targetPos.x, scene.m_vYasuoDashStart.y, targetPos.z };
    scene.m_fYasuoDashElapsed = 0.f;
    scene.m_bYasuoDashActive = true;
    scene.m_YasuoDashTargetEntity = target;
}

void CInGameChampionStateBridge::StartLocalUltimateDash(CScene_InGame& scene, EntityID airborne)
{
    if (!scene.m_pPlayerTransform) return;
    if (!scene.m_World.HasComponent<TransformComponent>(airborne)) return;

    const Vec3 targetPos = scene.m_World.GetComponent<TransformComponent>(airborne).m_LocalPosition;
    scene.m_vYasuoDashStart = scene.m_pPlayerTransform->GetPosition();
    scene.m_vYasuoDashEnd = { targetPos.x, targetPos.y + 0.5f, targetPos.z };
    scene.m_fYasuoDashElapsed = 0.f;
    scene.m_bYasuoDashActive = true;
    scene.m_YasuoDashTargetEntity = airborne;
    scene.m_bYasuoRActive = true;
    scene.m_fYasuoRElapsed = 0.f;
    scene.m_fYasuoRPrevHitTime = 0.f;
    scene.m_iYasuoRHitsFired = 0;
    scene.m_YasuoRTarget = airborne;
}

void CInGameChampionStateBridge::StartLocalPassiveDash(CScene_InGame& scene, const Vec3& vForward)
{
    if (!scene.m_pPlayerTransform)
        return;

    const Vec3 vOrigin = scene.m_pPlayerTransform->GetPosition();

    scene.m_vKalistaPassiveDashStart = vOrigin;
    const f32_t dashDist = Kalista::GetTuning().passiveDashDist;
    scene.m_vKalistaPassiveDashEnd = {
        vOrigin.x + vForward.x * dashDist,
        vOrigin.y,
        vOrigin.z + vForward.z * dashDist
    };
    scene.m_fKalistaPassiveDashElapsed = 0.f;
    scene.m_bKalistaPassiveDashActive = true;
    if (scene.m_bKalistaPassiveDashHasFaceDir)
    {
        const f32_t yaw = ResolveChampionVisualYawNear(
            scene.GetPlayerChampionId(),
            scene.m_vKalistaPassiveDashFaceDir,
            scene.GetPlayerYaw());
        scene.SetPlayerYaw(yaw);
    }

    scene.m_vPlayerDest = scene.m_vKalistaPassiveDashEnd;

    if (scene.m_PlayerEntity != NULL_ENTITY
        && scene.m_World.HasComponent<NavAgentComponent>(scene.m_PlayerEntity))
    {
        auto& agent = scene.m_World.GetComponent<NavAgentComponent>(scene.m_PlayerEntity);
        agent.vTarget = scene.m_vKalistaPassiveDashEnd;
        agent.bHasGoal = false;
        agent.bPathDirty = false;
    }
}

void CInGameChampionStateBridge::SetLocalPassiveDashDuration(f32_t duration)
{
    Kalista::GetTuning().passiveDashDuration = (duration < 0.03f) ? 0.03f : duration;
}

f32_t CInGameChampionStateBridge::GetLocalPassiveDashDuration()
{
    return Kalista::GetTuning().passiveDashDuration;
}

void CInGameChampionStateBridge::SetLocalActionAnimActive(CScene_InGame& scene, bool_t active)
{
    scene.m_bKalistaPassiveDashAnimActive = active;
}

EntityID CInGameChampionStateBridge::FindAirborneEnemyNear(
    CScene_InGame& scene,
    const Vec3& origin,
    f32_t radius)
{
    EntityID closest = NULL_ENTITY;
    f32_t bestDist2 = radius * radius;

    scene.m_World.ForEach<ChampionComponent, TransformComponent>(
        [&](EntityID entity, ChampionComponent& champion, TransformComponent& transform)
        {
            if (champion.team == scene.m_PlayerTeam) return;
            if (!scene.m_World.HasComponent<StunComponent>(entity)) return;

            const f32_t dx = transform.m_LocalPosition.x - origin.x;
            const f32_t dz = transform.m_LocalPosition.z - origin.z;
            const f32_t dist2 = dx * dx + dz * dz;
            if (dist2 < bestDist2)
            {
                bestDist2 = dist2;
                closest = entity;
            }
        });

    return closest;
}

void CInGameChampionStateBridge::ApplyLocalChampionDamage(
    CScene_InGame& scene,
    EntityID target,
    f32_t fDamage,
    const char* pDebugLabel)
{
    if (target == NULL_ENTITY || target == scene.m_PlayerEntity) return;
    if (!scene.m_World.HasComponent<ChampionComponent>(target)) return;

    auto& champion = scene.m_World.GetComponent<ChampionComponent>(target);
    if (champion.team == scene.m_PlayerTeam) return;

    champion.hp = (champion.hp > fDamage) ? (champion.hp - fDamage) : 0.f;

    f32_t hpCur = champion.hp;
    f32_t hpMax = champion.maxHp;
    if (scene.m_World.HasComponent<HealthComponent>(target))
    {
        auto& hp = scene.m_World.GetComponent<HealthComponent>(target);
        hp.fCurrent = champion.hp;
        hp.fMaximum = champion.maxHp;
        hp.bIsDead = (hp.fCurrent <= 0.f);
        hpCur = hp.fCurrent;
        hpMax = hp.fMaximum;
    }

    NotifyTowerAggroOnChampionHit(scene.m_World, scene.m_PlayerEntity, target);

    char buf[160];
    sprintf_s(buf, "[%s] target=%u dmg=%.1f hp=%.1f/%.1f\n",
        pDebugLabel ? pDebugLabel : "LocalChampionDamage",
        static_cast<u32_t>(target), fDamage, hpCur, hpMax);
    OutputDebugStringA(buf);
}

void CInGameChampionStateBridge::UpdateLocalTargetDash(CScene_InGame& scene, f32_t dt)
{
    if (!scene.m_pPlayerTransform)
    {
        scene.m_bYasuoDashActive = false;
        scene.m_YasuoDashTargetEntity = NULL_ENTITY;
        return;
    }

    scene.m_fYasuoDashElapsed += dt;
    const f32_t dashDuration = scene.GetYasuoEDashDuration();
    f32_t t = (dashDuration > 0.01f)
        ? (scene.m_fYasuoDashElapsed / dashDuration) : 1.f;
    if (t >= 1.f)
    {
        t = 1.f;
        if (!scene.m_bYasuoRActive
            && scene.m_YasuoDashTargetEntity != NULL_ENTITY
            && scene.m_World.HasComponent<ChampionComponent>(scene.m_YasuoDashTargetEntity))
        {
            ApplyLocalChampionDamage(
                scene,
                scene.m_YasuoDashTargetEntity,
                scene.GetYasuoEDamage(),
                "Yasuo Hit");
        }
        scene.m_bYasuoDashActive = false;
        scene.m_YasuoDashTargetEntity = NULL_ENTITY;
    }

    const Vec3 pos{
        scene.m_vYasuoDashStart.x + (scene.m_vYasuoDashEnd.x - scene.m_vYasuoDashStart.x) * t,
        scene.m_vYasuoDashStart.y + (scene.m_vYasuoDashEnd.y - scene.m_vYasuoDashStart.y) * t,
        scene.m_vYasuoDashStart.z + (scene.m_vYasuoDashEnd.z - scene.m_vYasuoDashStart.z) * t
    };
    scene.m_pPlayerTransform->SetPosition(pos);

    if (scene.m_PlayerEntity != NULL_ENTITY
        && scene.m_World.HasComponent<TransformComponent>(scene.m_PlayerEntity))
    {
        scene.m_World.GetComponent<TransformComponent>(scene.m_PlayerEntity).m_LocalPosition = pos;
    }
}

void CInGameChampionStateBridge::UpdateLocalUltimateSequence(CScene_InGame& scene, f32_t dt)
{
    if (scene.m_bYasuoDashActive)
        return;

    scene.m_fYasuoRElapsed += dt;
    constexpr i32_t kMaxHits = 5;
    while (scene.m_iYasuoRHitsFired < kMaxHits
        && scene.m_YasuoRTarget != NULL_ENTITY
        && scene.m_World.HasComponent<ChampionComponent>(scene.m_YasuoRTarget)
        && scene.m_fYasuoRElapsed >= (static_cast<f32_t>(scene.m_iYasuoRHitsFired + 1) * scene.GetYasuoRHitInterval()))
    {
        ApplyLocalChampionDamage(
            scene,
            scene.m_YasuoRTarget,
            scene.GetYasuoRPerHitDamage(),
            "Yasuo Hit");
        scene.m_iYasuoRHitsFired += 1;
        scene.m_fYasuoRPrevHitTime = scene.m_fYasuoRElapsed;
    }

    if (scene.m_fYasuoRElapsed >= scene.GetYasuoRSequenceDuration())
    {
        scene.m_bYasuoRActive = false;
        scene.m_YasuoRTarget = NULL_ENTITY;
        scene.m_iYasuoRHitsFired = 0;
        scene.m_fYasuoRPrevHitTime = 0.f;
    }
}

void CInGameChampionStateBridge::UpdateLocalPassiveDash(CScene_InGame& scene, f32_t dt)
{
    if (!scene.m_pPlayerTransform)
    {
        scene.m_bKalistaPassiveDashActive = false;
        scene.m_vKalistaPassiveDashFaceDir = {};
        scene.m_bKalistaPassiveDashHasFaceDir = false;
        return;
    }

    scene.m_fKalistaPassiveDashElapsed += dt;
    const f32_t dashDuration = GetLocalPassiveDashDuration();
    f32_t t = (dashDuration > 0.01f)
        ? (scene.m_fKalistaPassiveDashElapsed / dashDuration) : 1.f;
    if (t >= 1.f)
    {
        t = 1.f;
        scene.m_bKalistaPassiveDashActive = false;
    }

    const Vec3 pos{
        scene.m_vKalistaPassiveDashStart.x + (scene.m_vKalistaPassiveDashEnd.x - scene.m_vKalistaPassiveDashStart.x) * t,
        scene.m_vKalistaPassiveDashStart.y,
        scene.m_vKalistaPassiveDashStart.z + (scene.m_vKalistaPassiveDashEnd.z - scene.m_vKalistaPassiveDashStart.z) * t
    };

    scene.m_pPlayerTransform->SetPosition(pos);

    if (scene.m_PlayerEntity != NULL_ENTITY
        && scene.m_World.HasComponent<TransformComponent>(scene.m_PlayerEntity))
    {
        scene.m_World.GetComponent<TransformComponent>(scene.m_PlayerEntity).m_LocalPosition = pos;
    }

    if (scene.m_bKalistaPassiveDashHasFaceDir)
    {
        const f32_t yaw = ResolveChampionVisualYawNear(
            scene.GetPlayerChampionId(),
            scene.m_vKalistaPassiveDashFaceDir,
            scene.GetPlayerYaw());
        scene.SetPlayerYaw(yaw);
    }

    if (!scene.m_bKalistaPassiveDashActive)
    {
        scene.m_vKalistaPassiveDashFaceDir = {};
        scene.m_bKalistaPassiveDashHasFaceDir = false;
    }
}
