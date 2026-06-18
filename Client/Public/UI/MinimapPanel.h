#pragma once

#include "ECS/Components/GameplayComponents.h"
#include "WintersMath.h"
#include "WintersTypes.h"

#include <vector>

class CWorld;
class CFogOfWarRenderer;

namespace UI
{
    enum class eMinimapIconKind : u8_t
    {
        Unknown,
        Champion,
        Minion,
        Turret,
        Inhibitor,
        Nexus,
        JungleMob,
        Ward,
    };

    struct MinimapIconView
    {
        EntityID Entity = NULL_ENTITY;
        Vec3 vWorldPos{};
        eMinimapIconKind eKind = eMinimapIconKind::Unknown;
        eTeam eTeamId = eTeam::Neutral;
        eChampion eChampionId = eChampion::END;
        bool_t bAlly = false;
        bool_t bVisible = false;
        bool_t bAlive = true;
    };

    struct MinimapProjection
    {
        Vec2 vWorldAtUv00{ 96.59f, 157.20f };
        Vec2 vWorldAtUv10{ 199.28f, 0.04f };
        Vec2 vWorldAtUv01{ 10.51f, 0.98f };
    };

    struct MinimapFrameState
    {
        void* pFowOverlaySRV = nullptr;
        MinimapProjection Projection{};
        f32_t fScreenWidth = 0.f;
        f32_t fScreenHeight = 0.f;
        f32_t fSize = 252.f;
        f32_t fRightPadding = 12.f;
        f32_t fBottomPadding = 12.f;
        u8_t iLocalTeam = 0;
        bool_t bShow = true;
        bool_t bRevealAll = false;
        std::vector<MinimapIconView> Icons;
    };

    const MinimapProjection& GetDefaultMinimapProjection();
    bool_t ProjectWorldToMinimapUv(
        const MinimapProjection& Projection,
        const Vec3& vWorldPos,
        f32_t& fOutU,
        f32_t& fOutV);

    void BuildMinimapFrameState(
        CWorld& World,
        CFogOfWarRenderer* pFow,
        eTeam eLocalTeam,
        f32_t fScreenWidth,
        f32_t fScreenHeight,
        bool_t bRevealAll,
        MinimapFrameState& OutState);

    class CMinimapPanel
    {
    public:
        static void RenderRuntime(const MinimapFrameState& State);
        static void ShutdownRuntime();
    };
}
