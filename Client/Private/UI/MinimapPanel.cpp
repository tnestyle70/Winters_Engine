#include "UI/MinimapPanel.h"

#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/VisionComponents.h"
#include "ECS/World.h"
#include "GameInstance.h"
#include "ProfilerAPI.h"
#include "Renderer/FogOfWarRenderer.h"
#include "Resource/Texture.h"
#include "Scene/LobbyRosterHelpers.h"
#include "Scene/RenderVisibilityFilter.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

namespace
{
    constexpr const wchar_t* kPathMinimapBase =
        L"Resource/Texture/UI/InGameUI/minimap_base_clean.png";
    const Vec4 kUVFull{ 0.f, 0.f, 1.f, 1.f };

    constexpr f32_t kCanonicalProjectionCenterX = 104.50f;
    constexpr f32_t kCanonicalProjectionCenterZ = 0.f;
    constexpr f32_t kCanonicalProjectionExtent = 94.385f;
    constexpr f32_t kMinProjectionExtent = 70.f;
    constexpr f32_t kMaxProjectionExtent = 160.f;

    struct MinimapRuntimeLayout
    {
        f32_t ViewportHeightRatio = 0.37f;
        f32_t RightPadding = 12.f;
        f32_t BottomPadding = 12.f;
        f32_t IconScale = 1.f;
        f32_t ChampionScale = 2.f;
    };

    std::unique_ptr<Engine::CTexture> s_pMinimapBaseTexture;
    const UI::MinimapProjection kDefaultProjection{};
    UI::MinimapProjection s_RuntimeProjection = kDefaultProjection;
    f32_t s_fRuntimeProjectionExtent = kCanonicalProjectionExtent;
    u32_t s_iRuntimeProjectionRevision = 0u;
    MinimapRuntimeLayout s_MinimapLayout{};
    u64_t s_iPortraitLoadFailuresSinceLastRender = 0u;

    UI::MinimapProjection BuildUniformMinimapProjection(f32_t fExtent)
    {
        const f32_t fClampedExtent = std::clamp(
            fExtent, kMinProjectionExtent, kMaxProjectionExtent);
        UI::MinimapProjection Projection{};
        Projection.vWorldAtUv00 = {
            kCanonicalProjectionCenterX,
            kCanonicalProjectionCenterZ + fClampedExtent };
        Projection.vWorldAtUv10 = {
            kCanonicalProjectionCenterX + fClampedExtent,
            kCanonicalProjectionCenterZ };
        Projection.vWorldAtUv01 = {
            kCanonicalProjectionCenterX - fClampedExtent,
            kCanonicalProjectionCenterZ };
        return Projection;
    }

    void ApplyRuntimeMinimapProjection()
    {
        s_fRuntimeProjectionExtent = std::clamp(
            s_fRuntimeProjectionExtent,
            kMinProjectionExtent,
            kMaxProjectionExtent);
        s_RuntimeProjection = BuildUniformMinimapProjection(
            s_fRuntimeProjectionExtent);
        ++s_iRuntimeProjectionRevision;
    }

    bool_t EnsureMinimapBaseTexture()
    {
        if (s_pMinimapBaseTexture)
            return true;

        CGameInstance* pGameInstance = CGameInstance::Get();
        if (!pGameInstance)
            return false;

        IRHIDevice* pDevice = pGameInstance->Get_RHIDevice();
        if (!pDevice)
            return false;

        s_pMinimapBaseTexture = Engine::CTexture::Create(
            pDevice,
            std::wstring(kPathMinimapBase),
            Engine::eTexSamplerMode::Clamp,
            Engine::eTexColorSpace::IgnoreSRGB);

        return s_pMinimapBaseTexture != nullptr;
    }

    std::unordered_map<eChampion, std::unique_ptr<Engine::CTexture>> s_ChampionPortraits;

    Engine::CTexture* EnsureChampionPortrait(eChampion champion)
    {
        if (champion == eChampion::END || champion == eChampion::NONE)
            return nullptr;

        auto it = s_ChampionPortraits.find(champion);
        if (it != s_ChampionPortraits.end())
            return it->second.get();

        CGameInstance* pGameInstance = CGameInstance::Get();
        if (!pGameInstance)
            return nullptr;
        IRHIDevice* pDevice = pGameInstance->Get_RHIDevice();
        if (!pDevice)
            return nullptr;

        const wchar_t* pPath = GetRosterChampionPortraitPath(champion);
        if (!pPath)
        {
            ++s_iPortraitLoadFailuresSinceLastRender;
            return nullptr;
        }

        auto tex = Engine::CTexture::Create(
            pDevice,
            std::wstring(pPath),
            Engine::eTexSamplerMode::Clamp,
            Engine::eTexColorSpace::IgnoreSRGB);
        if (!tex)
        {
            ++s_iPortraitLoadFailuresSinceLastRender;
            return nullptr;
        }

        Engine::CTexture* pRaw = tex.get();
        s_ChampionPortraits.emplace(champion, std::move(tex));
        return pRaw;
    }

    UI::eMinimapIconKind ResolveMinimapIconKind(eSpatialKind eKind)
    {
        switch (eKind)
        {
        case eSpatialKind::Character: return UI::eMinimapIconKind::Champion;
        case eSpatialKind::Unit: return UI::eMinimapIconKind::Minion;
        case eSpatialKind::Structure: return UI::eMinimapIconKind::Turret;
        case eSpatialKind::NeutralUnit: return UI::eMinimapIconKind::JungleMob;
        case eSpatialKind::Objective: return UI::eMinimapIconKind::Inhibitor;
        case eSpatialKind::Core: return UI::eMinimapIconKind::Nexus;
        case eSpatialKind::Sensor: return UI::eMinimapIconKind::Ward;
        default: return UI::eMinimapIconKind::Unknown;
        }
    }

    bool_t ResolveAlive(CWorld& World, EntityID Entity)
    {
        if (!World.IsAlive(Entity))
            return false;

        if (World.HasComponent<HealthComponent>(Entity))
        {
            const HealthComponent& Health = World.GetComponent<HealthComponent>(Entity);
            return !Health.bIsDead && Health.fCurrent > 0.f;
        }

        if (World.HasComponent<ChampionComponent>(Entity))
            return World.GetComponent<ChampionComponent>(Entity).hp > 0.f;
        if (World.HasComponent<MinionComponent>(Entity))
            return World.GetComponent<MinionComponent>(Entity).hp > 0.f;
        if (World.HasComponent<StructureComponent>(Entity))
            return World.GetComponent<StructureComponent>(Entity).hp > 0.f;
        if (World.HasComponent<JungleComponent>(Entity))
            return World.GetComponent<JungleComponent>(Entity).hp > 0.f;

        return true;
    }

    Vec4 ResolveIconColor(const UI::MinimapIconView& Icon)
    {
        if (!Icon.bAlive)
            return Vec4{ 0.28f, 0.30f, 0.33f, 0.82f };
        if (Icon.eTeamId == eTeam::Neutral)
            return Vec4{ 0.92f, 0.78f, 0.32f, 1.f };
        return Icon.bAlly
            ? Vec4{ 0.16f, 0.50f, 1.f, 1.f }
            : Vec4{ 1.f, 0.18f, 0.16f, 1.f };
    }

    f32_t ResolveIconRadius(
        UI::eMinimapIconKind eKind,
        f32_t fPanelSide)
    {
        f32_t fBaseRadius = 2.8f;
        switch (eKind)
        {
        case UI::eMinimapIconKind::Champion: fBaseRadius = 5.5f; break;
        case UI::eMinimapIconKind::Nexus: fBaseRadius = 5.5f; break;
        case UI::eMinimapIconKind::Inhibitor: fBaseRadius = 4.8f; break;
        case UI::eMinimapIconKind::Turret: fBaseRadius = 4.2f; break;
        case UI::eMinimapIconKind::JungleMob: fBaseRadius = 3.6f; break;
        case UI::eMinimapIconKind::Minion: fBaseRadius = 2.4f; break;
        case UI::eMinimapIconKind::Ward: fBaseRadius = 2.6f; break;
        default: break;
        }

        const f32_t fPanelScale = (std::max)(0.35f, fPanelSide / 252.f);
        const f32_t fChampionScale = eKind == UI::eMinimapIconKind::Champion
            ? s_MinimapLayout.ChampionScale
            : 1.f;
        return fBaseRadius * fPanelScale * s_MinimapLayout.IconScale * fChampionScale;
    }

    bool_t WorldToMinimap(
        const UI::MinimapFrameState& State,
        const Vec3& vWorldPos,
        f32_t fX,
        f32_t fY,
        f32_t fSide,
        f32_t& fOutX,
        f32_t& fOutY)
    {
        f32_t fU = 0.f;
        f32_t fV = 0.f;
        if (!UI::ProjectWorldToMinimapUv(State.Projection, vWorldPos, fU, fV))
            return false;

        if (fU < 0.f || fU > 1.f || fV < 0.f || fV > 1.f)
            return false;

        fOutX = fX + fU * fSide;
        fOutY = fY + fV * fSide;
        return true;
    }

    bool_t ResolveMinimapRect(
        const UI::MinimapFrameState& State,
        f32_t& fOutX,
        f32_t& fOutY,
        f32_t& fOutSide)
    {
        if (!State.bShow || State.fScreenWidth <= 0.f || State.fScreenHeight <= 0.f)
            return false;

        const f32_t fRightPadding = std::clamp(
            s_MinimapLayout.RightPadding, 0.f, (std::max)(0.f, State.fScreenWidth - 96.f));
        const f32_t fBottomPadding = std::clamp(
            s_MinimapLayout.BottomPadding, 0.f, (std::max)(0.f, State.fScreenHeight - 96.f));
        const f32_t fRequestedSide = State.fScreenHeight *
            std::clamp(s_MinimapLayout.ViewportHeightRatio, 0.18f, 0.55f);
        const f32_t fAvailableSide = (std::max)(0.f, (std::min)(
            State.fScreenWidth - fRightPadding,
            State.fScreenHeight - fBottomPadding));
        if (fAvailableSide < 96.f)
            return false;

        fOutSide = (std::max)(96.f, (std::min)(fRequestedSide, fAvailableSide));
        fOutX = State.fScreenWidth - fRightPadding - fOutSide;
        fOutY = State.fScreenHeight - fBottomPadding - fOutSide;
        return fOutX >= 0.f && fOutY >= 0.f;
    }

    void DrawRectOutline(
        CGameInstance& GameInstance,
        f32_t fX,
        f32_t fY,
        f32_t fW,
        f32_t fH,
        f32_t fThickness,
        const Vec4& vColor)
    {
        GameInstance.UI_Draw_RawImage(nullptr, fX, fY, fW, fThickness, kUVFull, vColor);
        GameInstance.UI_Draw_RawImage(nullptr, fX, fY + fH - fThickness, fW, fThickness, kUVFull, vColor);
        GameInstance.UI_Draw_RawImage(nullptr, fX, fY, fThickness, fH, kUVFull, vColor);
        GameInstance.UI_Draw_RawImage(nullptr, fX + fW - fThickness, fY, fThickness, fH, kUVFull, vColor);
    }

    void DrawCameraBounds(
        const UI::MinimapFrameState& State,
        f32_t fX,
        f32_t fY,
        f32_t fSide)
    {
        if (!State.bShowCameraBounds)
            return;

        CGameInstance* pGameInstance = CGameInstance::Get();
        if (!pGameInstance)
            return;

        const Vec3 vCenter = State.vCameraWorldCenter;
        const f32_t fHalfW = (std::max)(1.f, State.fCameraViewHalfWidth);
        const f32_t fHalfD = (std::max)(1.f, State.fCameraViewHalfDepth);
        const Vec3 Corners[4] = {
            Vec3{ vCenter.x - fHalfW, vCenter.y, vCenter.z - fHalfD },
            Vec3{ vCenter.x + fHalfW, vCenter.y, vCenter.z - fHalfD },
            Vec3{ vCenter.x + fHalfW, vCenter.y, vCenter.z + fHalfD },
            Vec3{ vCenter.x - fHalfW, vCenter.y, vCenter.z + fHalfD }
        };

        f32_t fMinX = fX + fSide;
        f32_t fMinY = fY + fSide;
        f32_t fMaxX = fX;
        f32_t fMaxY = fY;
        for (const Vec3& vCorner : Corners)
        {
            f32_t fU = 0.f;
            f32_t fV = 0.f;
            if (!UI::ProjectWorldToMinimapUv(State.Projection, vCorner, fU, fV))
                return;

            const f32_t fPx = fX + fU * fSide;
            const f32_t fPy = fY + fV * fSide;
            fMinX = (std::min)(fMinX, fPx);
            fMinY = (std::min)(fMinY, fPy);
            fMaxX = (std::max)(fMaxX, fPx);
            fMaxY = (std::max)(fMaxY, fPy);
        }

        fMinX = std::clamp(fMinX, fX, fX + fSide);
        fMinY = std::clamp(fMinY, fY, fY + fSide);
        fMaxX = std::clamp(fMaxX, fX, fX + fSide);
        fMaxY = std::clamp(fMaxY, fY, fY + fSide);

        const f32_t fW = fMaxX - fMinX;
        const f32_t fH = fMaxY - fMinY;
        if (fW < 4.f || fH < 4.f)
            return;

        DrawRectOutline(
            *pGameInstance,
            fMinX,
            fMinY,
            fW,
            fH,
            2.f,
            Vec4{ 1.f, 1.f, 1.f, 0.96f });
    }

    bool_t DrawIcon(
        const UI::MinimapIconView& Icon,
        f32_t fCenterX,
        f32_t fCenterY,
        f32_t fPanelSide)
    {
        CGameInstance* pGameInstance = CGameInstance::Get();
        if (!pGameInstance)
            return false;

        const f32_t fRadius = ResolveIconRadius(Icon.eKind, fPanelSide);
        const Vec4 vBorder{ 0.015f, 0.018f, 0.022f, 0.94f };
        const Vec4 vFill = ResolveIconColor(Icon);

        const bool_t bChampion = Icon.eKind == UI::eMinimapIconKind::Champion;

        if (!bChampion)
        {
            pGameInstance->UI_Draw_RawImage(
                nullptr,
                fCenterX - fRadius - 1.f,
                fCenterY - fRadius - 1.f,
                (fRadius + 1.f) * 2.f,
                (fRadius + 1.f) * 2.f,
                kUVFull,
                vBorder);
            pGameInstance->UI_Draw_RawImage(
                nullptr,
                fCenterX - fRadius,
                fCenterY - fRadius,
                fRadius * 2.f,
                fRadius * 2.f,
                kUVFull,
                vFill);
            return false;
        }

        pGameInstance->UI_Draw_RawImageCircle(
            nullptr,
            fCenterX - fRadius - 1.f,
            fCenterY - fRadius - 1.f,
            (fRadius + 1.f) * 2.f,
            (fRadius + 1.f) * 2.f,
            kUVFull,
            vBorder,
            28);

        void* pPortraitSRV = nullptr;
        if (Engine::CTexture* pPortrait = EnsureChampionPortrait(Icon.eChampionId))
            pPortraitSRV = pPortrait->GetNativeSRV();

        if (pPortraitSRV)
        {
            const Vec4 vTint = Icon.bAlive
                ? Vec4{ 1.f, 1.f, 1.f, 1.f }
                : Vec4{ 0.45f, 0.45f, 0.45f, 0.95f };
            pGameInstance->UI_Draw_RawImageCircle(
                pPortraitSRV,
                fCenterX - fRadius,
                fCenterY - fRadius,
                fRadius * 2.f,
                fRadius * 2.f,
                kUVFull,
                vTint,
                40);
            return true;
        }

        pGameInstance->UI_Draw_RawImageCircle(
            nullptr,
            fCenterX - fRadius,
            fCenterY - fRadius,
            fRadius * 2.f,
            fRadius * 2.f,
            kUVFull,
            vFill,
            28);
        return false;
    }
}

namespace UI
{
    const MinimapProjection& GetDefaultMinimapProjection()
    {
        return s_RuntimeProjection;
    }

    Vec3 MinimapUvToWorld(
        const MinimapProjection& Projection,
        f32_t fU,
        f32_t fV,
        f32_t fY)
    {
        const Vec2 vU{
            Projection.vWorldAtUv10.x - Projection.vWorldAtUv00.x,
            Projection.vWorldAtUv10.y - Projection.vWorldAtUv00.y
        };
        const Vec2 vV{
            Projection.vWorldAtUv01.x - Projection.vWorldAtUv00.x,
            Projection.vWorldAtUv01.y - Projection.vWorldAtUv00.y
        };

        return Vec3{
            Projection.vWorldAtUv00.x + vU.x * fU + vV.x * fV,
            fY,
            Projection.vWorldAtUv00.y + vU.y * fU + vV.y * fV
        };
    }

    bool_t ProjectWorldToMinimapUv(
        const MinimapProjection& Projection,
        const Vec3& vWorldPos,
        f32_t& fOutU,
        f32_t& fOutV)
    {
        const f32_t ux = Projection.vWorldAtUv10.x - Projection.vWorldAtUv00.x;
        const f32_t uz = Projection.vWorldAtUv10.y - Projection.vWorldAtUv00.y;
        const f32_t vx = Projection.vWorldAtUv01.x - Projection.vWorldAtUv00.x;
        const f32_t vz = Projection.vWorldAtUv01.y - Projection.vWorldAtUv00.y;
        const f32_t det = ux * vz - uz * vx;
        if (std::fabs(det) <= 0.0001f)
            return false;

        const f32_t wx = vWorldPos.x - Projection.vWorldAtUv00.x;
        const f32_t wz = vWorldPos.z - Projection.vWorldAtUv00.y;

        fOutU = (wx * vz - wz * vx) / det;
        fOutV = (ux * wz - uz * wx) / det;
        return true;
    }

    bool_t TryResolveMinimapClickWorldPos(
        const MinimapFrameState& State,
        f32_t fMouseX,
        f32_t fMouseY,
        Vec3& vOutWorldPos)
    {
        f32_t fX = 0.f;
        f32_t fY = 0.f;
        f32_t fSide = 0.f;
        if (!ResolveMinimapRect(State, fX, fY, fSide))
            return false;

        if (fMouseX < fX || fMouseX > fX + fSide ||
            fMouseY < fY || fMouseY > fY + fSide)
        {
            return false;
        }

        const f32_t fU = (fMouseX - fX) / fSide;
        const f32_t fV = (fMouseY - fY) / fSide;
        vOutWorldPos = MinimapUvToWorld(State.Projection, fU, fV, 0.f);
        return true;
    }

    void BuildMinimapFrameState(
        CWorld& World,
        CFogOfWarRenderer* pFow,
        eTeam eLocalTeam,
        f32_t fScreenWidth,
        f32_t fScreenHeight,
        bool_t bRevealAll,
        MinimapFrameState& OutState)
    {
        OutState = {};
        OutState.pFowOverlaySRV = pFow ? pFow->GetMinimapOverlaySRV() : nullptr;
        OutState.Projection = GetDefaultMinimapProjection();
        OutState.fScreenWidth = fScreenWidth;
        OutState.fScreenHeight = fScreenHeight;
        OutState.iLocalTeam = static_cast<u8_t>(eLocalTeam);
        OutState.bRevealAll = bRevealAll;

        World.ForEach<TransformComponent, SpatialAgentComponent, VisibilityComponent>(
            std::function<void(EntityID, TransformComponent&, SpatialAgentComponent&, VisibilityComponent&)>(
                [&](EntityID Entity, TransformComponent& Transform,
                    SpatialAgentComponent& Agent, VisibilityComponent& Visibility)
                {
                    if (!UI::IsRenderableForLocal(
                        World,
                        Entity,
                        OutState.iLocalTeam,
                        bRevealAll))
                    {
                        return;
                    }

                    const eMinimapIconKind eIconKind = ResolveMinimapIconKind(Agent.kind);
                    if (eIconKind == eMinimapIconKind::Unknown)
                        return;

                    const bool_t bAlly = Agent.team == OutState.iLocalTeam;
                    const bool_t bVisible =
                        bRevealAll ||
                        bAlly ||
                        ((Visibility.teamVisibilityMask & (1u << OutState.iLocalTeam)) != 0);
                    if (!bVisible)
                        return;

                    const bool_t bAlive = ResolveAlive(World, Entity);
                    if (!bAlive && eIconKind != eMinimapIconKind::Champion)
                        return;

                    MinimapIconView Icon{};
                    Icon.Entity = Entity;
                    Icon.vWorldPos = Transform.GetPosition();
                    Icon.eKind = eIconKind;
                    Icon.eTeamId = static_cast<eTeam>(Agent.team);
                    Icon.bAlly = bAlly;
                    Icon.bVisible = bVisible;
                    Icon.bAlive = bAlive;

                    if (World.HasComponent<ChampionComponent>(Entity))
                        Icon.eChampionId = World.GetComponent<ChampionComponent>(Entity).id;

                    OutState.Icons.push_back(Icon);
                }));
    }

    bool_t CMinimapPanel::PrewarmChampionPortrait(eChampion champion)
    {
        return EnsureMinimapBaseTexture() && EnsureChampionPortrait(champion) != nullptr;
    }

    void CMinimapPanel::PrewarmChampionPortraits()
    {
        const u32_t iFirstChampion = static_cast<u32_t>(eChampion::IRELIA);
        const u32_t iLastChampion = static_cast<u32_t>(eChampion::LEESIN);
        for (u32_t iChampion = iFirstChampion; iChampion <= iLastChampion; ++iChampion)
            (void)PrewarmChampionPortrait(static_cast<eChampion>(iChampion));
    }

    bool_t CMinimapPanel::DrawTunerImGui(
        bool_t bProjectionSyncAvailable,
        MinimapProjection& OutAppliedProjection)
    {
        bool_t bProjectionChanged = false;
        OutAppliedProjection = s_RuntimeProjection;

        ImGui::SetNextWindowSize(ImVec2(380.f, 440.f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(340.f, 280.f),
            ImVec2(560.f, 720.f));
        if (!ImGui::Begin("Minimap Layout"))
        {
            ImGui::End();
            return false;
        }

        ImGui::PushID("MinimapLayout");
        ImGui::PushItemWidth(-155.f);
        ImGui::SliderFloat(
            "Viewport Height Ratio",
            &s_MinimapLayout.ViewportHeightRatio,
            0.18f,
            0.55f,
            "%.2f");
        ImGui::SliderFloat(
            "Right Padding",
            &s_MinimapLayout.RightPadding,
            0.f,
            64.f,
            "%.0f px");
        ImGui::SliderFloat(
            "Bottom Padding",
            &s_MinimapLayout.BottomPadding,
            0.f,
            64.f,
            "%.0f px");
        ImGui::SliderFloat(
            "Icon Scale",
            &s_MinimapLayout.IconScale,
            0.65f,
            2.f,
            "%.2f");
        ImGui::SliderFloat(
            "Champion Scale",
            &s_MinimapLayout.ChampionScale,
            0.75f,
            3.f,
            "%.2f");
        ImGui::SeparatorText("World Projection (Top / Bottom)");
        ImGui::TextWrapped(
            "S020 uniform basis: one extent moves render, click, camera bounds, and FoW together.");

        ImGui::BeginDisabled(!bProjectionSyncAvailable);
        f32_t fEditedExtent = s_fRuntimeProjectionExtent;
        if (ImGui::SliderFloat(
                "World Extent",
                &fEditedExtent,
                kMinProjectionExtent,
                kMaxProjectionExtent,
                "%.3f world"))
        {
            s_fRuntimeProjectionExtent = fEditedExtent;
            ApplyRuntimeMinimapProjection();
            OutAppliedProjection = s_RuntimeProjection;
            bProjectionChanged = true;
        }
        ImGui::TextDisabled(
            "smaller = outward, larger = inward | revision %u",
            s_iRuntimeProjectionRevision);
        ImGui::PopItemWidth();

        if (ImGui::Button("Reset S020 Projection"))
        {
            s_fRuntimeProjectionExtent = kCanonicalProjectionExtent;
            ApplyRuntimeMinimapProjection();
            OutAppliedProjection = s_RuntimeProjection;
            bProjectionChanged = true;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Reset Panel / Icons"))
            s_MinimapLayout = {};
        if (!bProjectionSyncAvailable)
        {
            ImGui::TextColored(
                ImVec4(1.f, 0.45f, 0.25f, 1.f),
                "Projection/FoW sync unavailable");
        }
        ImGui::PopID();
        ImGui::End();
        return bProjectionChanged;
    }

    void CMinimapPanel::RenderRuntime(const MinimapFrameState& State)
    {
        CGameInstance* pGameInstance = CGameInstance::Get();
        if (!pGameInstance)
            return;

        f32_t fX = 0.f;
        f32_t fY = 0.f;
        f32_t fSide = 0.f;
        if (!ResolveMinimapRect(State, fX, fY, fSide))
            return;

        const u32_t iScreenWidth = (std::max)(1u, static_cast<u32_t>(State.fScreenWidth + 0.5f));
        const u32_t iScreenHeight = (std::max)(1u, static_cast<u32_t>(State.fScreenHeight + 0.5f));

        if (!pGameInstance->UI_Begin_RawImagePass(iScreenWidth, iScreenHeight, true))
            return;

        WINTERS_PROFILE_COUNT("Minimap::IconCount", static_cast<uint64_t>(State.Icons.size()));

        {
            WINTERS_PROFILE_SCOPE("Minimap::Backdrop");
            pGameInstance->UI_Draw_RawImage(
                nullptr,
                fX - 2.f,
                fY - 2.f,
                fSide + 4.f,
                fSide + 4.f,
                kUVFull,
                Vec4{ 0.015f, 0.018f, 0.022f, 0.96f });
        }

        {
            WINTERS_PROFILE_SCOPE("Minimap::Base");
            if (EnsureMinimapBaseTexture())
            {
                pGameInstance->UI_Draw_RawImage(
                    s_pMinimapBaseTexture->GetNativeSRV(),
                    fX,
                    fY,
                    fSide,
                    fSide,
                    kUVFull,
                    Vec4{ 1.f, 1.f, 1.f, 1.f });
            }
            else
            {
                pGameInstance->UI_Draw_RawImage(
                    nullptr,
                    fX,
                    fY,
                    fSide,
                    fSide,
                    kUVFull,
                    Vec4{ 0.035f, 0.045f, 0.052f, 1.f });
            }
        }

        {
            WINTERS_PROFILE_SCOPE("Minimap::FowOverlay");
            if (State.pFowOverlaySRV)
            {
                pGameInstance->UI_Draw_RawImage(
                    State.pFowOverlaySRV,
                    fX,
                    fY,
                    fSide,
                    fSide,
                    kUVFull,
                    Vec4{ 1.f, 1.f, 1.f, 0.82f });
            }
        }

        {
            WINTERS_PROFILE_SCOPE("Minimap::Icons");
            for (const MinimapIconView& Icon : State.Icons)
            {
                if (Icon.eKind == eMinimapIconKind::Champion)
                    continue;

                f32_t fIconX = 0.f;
                f32_t fIconY = 0.f;
                if (WorldToMinimap(State, Icon.vWorldPos, fX, fY, fSide, fIconX, fIconY))
                    (void)DrawIcon(Icon, fIconX, fIconY, fSide);
            }
        }

        {
            WINTERS_PROFILE_SCOPE("Minimap::CameraBounds");
            DrawCameraBounds(State, fX, fY, fSide);
        }

        u64_t iChampionPortraitCount = 0u;
        {
            WINTERS_PROFILE_SCOPE("Minimap::ChampionPortraits");
            for (const MinimapIconView& Icon : State.Icons)
            {
                if (Icon.eKind != eMinimapIconKind::Champion)
                    continue;

                f32_t fIconX = 0.f;
                f32_t fIconY = 0.f;
                if (WorldToMinimap(State, Icon.vWorldPos, fX, fY, fSide, fIconX, fIconY) &&
                    DrawIcon(Icon, fIconX, fIconY, fSide))
                {
                    ++iChampionPortraitCount;
                }
            }
        }

        WINTERS_PROFILE_COUNT("Minimap::ChampionPortraitCount", iChampionPortraitCount);
        WINTERS_PROFILE_COUNT(
            "Minimap::PortraitLoadFailure",
            s_iPortraitLoadFailuresSinceLastRender);
        s_iPortraitLoadFailuresSinceLastRender = 0u;

        pGameInstance->UI_End_RawImagePass();
    }

    void CMinimapPanel::ShutdownRuntime()
    {
        s_pMinimapBaseTexture.reset();
        s_ChampionPortraits.clear();
        s_iPortraitLoadFailuresSinceLastRender = 0u;
    }
}
