#pragma once

#include "Defines.h"
#include "ECS/Entity.h"
#include "ECS/Components/GameplayComponents.h"
#include "GameObject/ChampionDef.h"
#include "WintersMath.h"

class CWorld;

class GameplayQuery final
{
public:
    static bool_t RayVsCylinder(
        const Vec3& vRayOrigin,
        const Vec3& vRayDir,
        const Vec3& vBase,
        f32_t fRadius,
        f32_t fHeight,
        f32_t& outT);

    static bool_t TryFindHoverTarget(
        CWorld& world,
        EntityID player,
        eTeam playerTeam,
        const Vec3& vRayOrigin,
        const Vec3& vRayDir,
        f32_t fChampionHitRadius,
        f32_t fChampionHitHeight,
        EntityID& outEntity,
        eTeam& outTeam);

    static bool_t IsAttackTargetLocallySelectable(
        CWorld& world,
        EntityID player,
        EntityID target);

    static bool_t IsValidAttackTarget(
        CWorld& world,
        EntityID player,
        EntityID target,
        eTeam playerTeam);

    static f32_t ResolvePlayerAttackRange(
        CWorld& world,
        EntityID player,
        eChampion playerChampion,
        f32_t fFallbackRange);

    static f32_t ResolveEffectiveAttackRange(
        CWorld& world,
        EntityID player,
        EntityID target,
        eChampion playerChampion,
        f32_t fFallbackRange);

    static f32_t ResolveAttackRangePreviewRadius(
        CWorld& world,
        EntityID player,
        eChampion playerChampion,
        f32_t fFallbackRange,
        bool_t bNetworkAuthoritativeGameplay);

    static EntityID FindAttackTargetNearCursor(
        CWorld& world,
        EntityID player,
        EntityID hoveredEntity,
        eTeam playerTeam,
        const Vec3& vCursorGround,
        bool_t bAttackMoveClick,
        eChampion playerChampion,
        f32_t fFallbackRange);
};
