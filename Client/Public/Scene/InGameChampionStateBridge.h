#pragma once

#include "Defines.h"
#include "ECS/Entity.h"
#include "GameObject/SkillDef.h"

class CScene_InGame;

class CInGameChampionStateBridge final
{
public:
    static void Update(CScene_InGame& scene, f32_t dt);
    static void UpdateLocalRuntime(CScene_InGame& scene, f32_t dt);
    static void UpdateLocalPostAnimation(CScene_InGame& scene);
    static bool_t CanResumeBaseAnimation(const CScene_InGame& scene);
    static bool_t IsLocalActionProtected(const CScene_InGame& scene);

    static void ResetLocalSkillRuntimeState(CScene_InGame& scene);
    static bool_t TryQueueLocalPassiveDashFromCursor(CScene_InGame& scene);
    static bool_t TriggerNetworkPassiveDashFromAction(
        CScene_InGame& scene,
        u16_t animId,
        u32_t actionSeq,
        bool_t bServerDashLikely = false);
    static bool_t ValidateLocalSkillStart(CScene_InGame& scene, const SkillDef& def);

    static void StartLocalTargetDash(CScene_InGame& scene, EntityID target);
    static void StartLocalUltimateDash(CScene_InGame& scene, EntityID target);
    static void StartLocalPassiveDash(CScene_InGame& scene, const Vec3& vForward);
    static void SetLocalPassiveDashDuration(f32_t duration);
    static f32_t GetLocalPassiveDashDuration();
    static void SetLocalActionAnimActive(CScene_InGame& scene, bool_t active);

    static EntityID FindAirborneEnemyNear(CScene_InGame& scene, const Vec3& origin, f32_t radius);
    static void ApplyLocalChampionDamage(
        CScene_InGame& scene,
        EntityID target,
        f32_t fDamage,
        const char* pDebugLabel);

private:
    static void UpdateLocalTargetDash(CScene_InGame& scene, f32_t dt);
    static void UpdateLocalUltimateSequence(CScene_InGame& scene, f32_t dt);
    static void UpdateLocalPassiveDash(CScene_InGame& scene, f32_t dt);
};
