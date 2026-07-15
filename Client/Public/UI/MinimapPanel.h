#pragma once

#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Definitions/LoLMatchContext.h"
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
        // Z half-diagonal must equal the X half-diagonal (94.385) so the
        // world->UV basis stays orthogonal and uniform (similarity, not shear).
        // 156.69 inflated world-Z 1.66x and compressed top/bottom lanes toward
        // the minimap center. Never retune one axis alone.
        Vec2 vWorldAtUv00{ 104.50f, 94.385f };
        Vec2 vWorldAtUv10{ 198.885f, 0.00f };
        Vec2 vWorldAtUv01{ 10.115f, 0.00f };
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
        Vec3 vCameraWorldCenter{};
        f32_t fCameraViewHalfWidth = 18.f;
        f32_t fCameraViewHalfDepth = 14.f;
        u8_t iLocalTeam = 0;
        bool_t bShow = true;
        bool_t bRevealAll = false;
        bool_t bShowCameraBounds = false;
        std::vector<MinimapIconView> Icons;
    };

    // Legacy-named accessor for the currently applied runtime projection.
    // Startup/reset uses the S020 canonical uniform basis declared above.
    const MinimapProjection& GetDefaultMinimapProjection();
    Vec3 MinimapUvToWorld(
        const MinimapProjection& Projection,
        f32_t fU,
        f32_t fV,
        f32_t fY = 0.f);
    bool_t ProjectWorldToMinimapUv(
        const MinimapProjection& Projection,
        const Vec3& vWorldPos,
        f32_t& fOutU,
        f32_t& fOutV);
    bool_t TryResolveMinimapClickWorldPos(
        const MinimapFrameState& State,
        f32_t fMouseX,
        f32_t fMouseY,
        Vec3& vOutWorldPos);

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
        static bool_t PrewarmChampionPortrait(eChampion champion);
        static void PrewarmChampionPortraits();
        static bool_t DrawTunerImGui(
            bool_t bProjectionSyncAvailable,
            MinimapProjection& OutAppliedProjection);
        static void RenderRuntime(const MinimapFrameState& State);
        static void ShutdownRuntime();
    };
}
