#include "UI/EffectTuner.h"
#include "Scene/Scene_InGame.h"
#include "GameObject/Champion/Irelia/IreliaFxPresets.h"
#include "GameObject/Champion/Irelia/Irelia_Tuning.h"
#include "GameObject/FX/FxLegacyAssetDumper.h"
#include "GameObject/FX/LegacyFxAdapter.h"
#include "GameObject/FX/FxMeshSystem.h"
#include "GameObject/FX/FxSystem.h"
#include "Renderer/FxStaticMeshRenderer.h"
#include "ECS/World.h"
#include "FX/FxDepthMode.h"

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

namespace
{
    // ── 활성 프리셋 enum (ImGui Combo) ──
    enum class ePreset : int
    {
        QTrail = 0,
        QMark,
        WSpin,
        WStage2Slash,
        EBeam,
        RPulse,
        Count
    };

    const char* kPresetNames[] = {
        "Q Trail", "Q Mark", "W Spin (atlas)", "W Stage2 Slash",
        "E Beam (Mesh)", "R Pulse"
    };

    // ── 튜너 상태 (정적 보관) ──
    struct TunerState
    {
        int   ePreset = 0;
        // 공통
        float vColor[4] = { 1.f, 1.f, 1.f, 1.f };
        float fWidth = 1.f;
        float fHeight = 1.f;
        float fLifetime = 1.f;
        // Atlas
        int   iAtlasCols = 1;
        int   iAtlasRows = 1;
        int   iAtlasFps = 0;
        // UV scroll
        float fUVScrollU = 0.f;
        float fUVScrollV = 0.f;
        // Blend
        int   iBlendMode = 0;   // AlphaBlend
        // Mesh 전용
        float vScale[3] = { 0.01f, 0.01f, 0.01f };
        float vRotation[3] = { 0.f, 0.f, 0.f };
        float fAlphaClip = 0.05f;
        float fErodeThreshold = 0.f;
        bool  bDepthWrite = true;
        int   iDepthMode = 0;
        int   iStyleMode = 0;
        float vStyleColorA[4] = { 3.2f, 2.4f, 0.75f, 5.5f };
        float vStyleColorB[4] = { 0.18f, 0.48f, 1.25f, 2.2f };
        float vRimColor[4] = { 0.75f, 0.95f, 1.6f, 2.8f };
        float fRimPower = 2.7f;
        float fCellLow = 0.f;
        float fCellHigh = 0.5f;
        float vMagicScrollA[4] = { 0.f, 0.5f, 0.1f, 0.05f };
        float vMagicShape[4] = { 2.5f, 0.06f, 1.0f, 0.035f };
        float vMagicCore[4] = { 2.0f, 1.0f, 2.0f, 0.f };
        float fMaterialRandom = 0.37f;
    };

    static TunerState s_State;
    static std::string s_LastWfxLoadError;

    const char* kBlendModeNames[] = { "Opaque", "AlphaBlend", "Premultiplied", "Additive" };
    const char* kStyleModeNames[] = {
        "Legacy", "LoL Brush Rim", "Toon Cell", "Gradient", "Magic Surface"
    };
    const char* kDepthModeNames[] = {
        "Depth Test + Write", "Depth Test Only", "Overlay No Depth", "Soft Particle"
    };

    constexpr const wchar_t* kPathQTrail =
        L"Texture/FX/Irelia/irelia_base_q_dark_trail.png";
    constexpr const wchar_t* kPathQMark =
        L"Texture/FX/Irelia/irelia_base_q_mark_pulse_erode.png";
    constexpr const wchar_t* kPathWSpin =
        L"Texture/FX/Irelia/irelia_base_w_bladeimages_spin_02.png";
    constexpr const wchar_t* kPathWBlade =
        L"Texture/FX/Irelia/irelia_base_w_blade_erode.png";
    constexpr const wchar_t* kPathRPulse = L"Texture/FX/Irelia/irelia_base_r_pulse_mesh_tex.png";
    constexpr const char* kPathEBeamFbx = "Texture/FX/Irelia/fbx/irelia_base_e_beam.fbx";
    constexpr const wchar_t* kPathEBeamTex = L"Texture/FX/Irelia/irelia_base_e_beam_mult.png";

    Vec4 CurrentColor()
    {
        return { s_State.vColor[0], s_State.vColor[1],
                 s_State.vColor[2], s_State.vColor[3] };
    }

    eBlendPreset CurrentBlendMode()
    {
        return static_cast<eBlendPreset>(s_State.iBlendMode);
    }

    eFxDepthMode CurrentDepthMode()
    {
        switch (s_State.iDepthMode)
        {
        case 1:
            return eFxDepthMode::DepthTestWriteOff;
        case 2:
            return eFxDepthMode::OverlayNoDepth;
        case 3:
            return eFxDepthMode::SoftParticle;
        case 0:
        default:
            return eFxDepthMode::DepthTestWriteOn;
        }
    }

    const char* CurrentAssetName()
    {
        switch ((ePreset)s_State.ePreset)
        {
        case ePreset::QTrail: return "Irelia_Q_Trail";
        case ePreset::QMark: return "Irelia_Q_Mark";
        case ePreset::WSpin: return "Irelia_W_Spin";
        case ePreset::WStage2Slash: return "Irelia_W_Stage2Slash";
        case ePreset::EBeam: return "Irelia_E_Beam";
        case ePreset::RPulse: return "Irelia_R_Pulse";
        default: return "Irelia_Unknown";
        }
    }
    const tchar_t* CurrentWfxPath()
    {
        switch ((ePreset)s_State.ePreset)
        {
        case ePreset::QTrail: return L"Data/LoL/FX/Champions/Irelia/q_leadingedge.wfx";
        case ePreset::QMark: return L"Data/LoL/FX/Champions/Irelia/target_mark.wfx";
        case ePreset::WSpin: return L"Data/LoL/FX/Champions/Irelia/w_hold.wfx";
        case ePreset::WStage2Slash: return L"Data/LoL/FX/Champions/Irelia/w_release.wfx";
        case ePreset::EBeam: return L"Data/LoL/FX/Champions/Irelia/e_connect.wfx";
        case ePreset::RPulse: return L"Data/LoL/FX/Champions/Irelia/r_pulse.wfx";
        default: return L"Data/LoL/FX/Champions/Irelia/unknown.wfx";
        }
    }

    FxAsset BuildCurrentWfxAsset()
    {
        if (s_State.ePreset == (int)ePreset::EBeam)
        {
            FxMeshComponent mesh{};
            mesh.SetModelPath(kPathEBeamFbx);
            mesh.SetTexturePath(kPathEBeamTex);
            mesh.vScale = { s_State.vScale[0], s_State.vScale[1], s_State.vScale[2] };
            mesh.vRotation = { s_State.vRotation[0], s_State.vRotation[1], s_State.vRotation[2] };
            mesh.vColor = CurrentColor();
            mesh.blendMode = CurrentBlendMode();
            mesh.fLifetime = s_State.fLifetime;
            mesh.fAlphaClip = s_State.fAlphaClip;
            mesh.fErodeThreshold = s_State.fErodeThreshold;
            mesh.fUvScrollU = s_State.fUVScrollU;
            mesh.fUvScrollV = s_State.fUVScrollV;
            mesh.iStyleMode = static_cast<u32_t>(s_State.iStyleMode);
            mesh.vStyleColorA = { s_State.vStyleColorA[0], s_State.vStyleColorA[1],
                                  s_State.vStyleColorA[2], s_State.vStyleColorA[3] };
            mesh.vStyleColorB = { s_State.vStyleColorB[0], s_State.vStyleColorB[1],
                                  s_State.vStyleColorB[2], s_State.vStyleColorB[3] };
            mesh.vRimColor = { s_State.vRimColor[0], s_State.vRimColor[1],
                               s_State.vRimColor[2], s_State.vRimColor[3] };
            mesh.fRimPower = s_State.fRimPower;
            mesh.fCellLow = s_State.fCellLow;
            mesh.fCellHigh = s_State.fCellHigh;
            mesh.vMagicScrollA = { s_State.vMagicScrollA[0], s_State.vMagicScrollA[1],
                                   s_State.vMagicScrollA[2], s_State.vMagicScrollA[3] };
            mesh.vMagicShape = { s_State.vMagicShape[0], s_State.vMagicShape[1],
                                 s_State.vMagicShape[2], s_State.vMagicShape[3] };
            mesh.vMagicCore = { s_State.vMagicCore[0], s_State.vMagicCore[1],
                                s_State.vMagicCore[2], s_State.vMagicCore[3] };
            mesh.fMaterialRandom = s_State.fMaterialRandom;
            mesh.RefreshMaterialFromLegacyFields();
            mesh.depthMode = CurrentDepthMode();
            mesh.bDepthWrite = FxDepthModeWritesDepth(mesh.depthMode);
            return LegacyFx::MakeAssetFromMesh(mesh, CurrentAssetName());
        }

        FxBillboardComponent fx{};
        fx.texturePath = kPathQTrail;
        fx.bBillboard = true;
        fx.fYaw = 0.f;

        switch ((ePreset)s_State.ePreset)
        {
        case ePreset::QMark:
            fx.texturePath = kPathQMark;
            break;
        case ePreset::WSpin:
            fx.texturePath = kPathWSpin;
            break;
        case ePreset::WStage2Slash:
            fx.texturePath = kPathWBlade;
            break;
        case ePreset::RPulse:
            fx.texturePath = kPathRPulse;
            fx.bBillboard = false;
            fx.fYaw = 1.5708f;
            break;
        case ePreset::QTrail:
        default:
            break;
        }

        fx.vColor = CurrentColor();
        fx.fWidth = s_State.fWidth;
        fx.fHeight = s_State.fHeight;
        fx.fLifetime = s_State.fLifetime;
        fx.iAtlasCols = static_cast<u32_t>(s_State.iAtlasCols);
        fx.iAtlasRows = static_cast<u32_t>(s_State.iAtlasRows);
        fx.iAtlasFrameCount = fx.iAtlasCols * fx.iAtlasRows;
        fx.fAtlasFps = static_cast<f32_t>(s_State.iAtlasFps);
        fx.fUvScrollU = s_State.fUVScrollU;
        fx.fUvScrollV = s_State.fUVScrollV;
        fx.blendMode = CurrentBlendMode();

        FxMaterialDesc material{};
        material.vTint = CurrentColor();
        material.vUVRect = { 0.f, 0.f, 1.f, 1.f };
        material.vUVScroll = { s_State.fUVScrollU, s_State.fUVScrollV };
        material.fAlphaClip = s_State.fAlphaClip;
        material.fErodeThreshold = s_State.fErodeThreshold;
        material.iStyleMode = static_cast<u32_t>(s_State.iStyleMode);
        material.vStyleColorA = { s_State.vStyleColorA[0], s_State.vStyleColorA[1],
                                  s_State.vStyleColorA[2], s_State.vStyleColorA[3] };
        material.vStyleColorB = { s_State.vStyleColorB[0], s_State.vStyleColorB[1],
                                  s_State.vStyleColorB[2], s_State.vStyleColorB[3] };
        material.vRimColor = { s_State.vRimColor[0], s_State.vRimColor[1],
                               s_State.vRimColor[2], s_State.vRimColor[3] };
        material.fRimPower = s_State.fRimPower;
        material.fCellLow = s_State.fCellLow;
        material.fCellHigh = s_State.fCellHigh;
        material.vMagicScrollA = { s_State.vMagicScrollA[0], s_State.vMagicScrollA[1],
                                   s_State.vMagicScrollA[2], s_State.vMagicScrollA[3] };
        material.vMagicShape = { s_State.vMagicShape[0], s_State.vMagicShape[1],
                                 s_State.vMagicShape[2], s_State.vMagicShape[3] };
        material.vMagicCore = { s_State.vMagicCore[0], s_State.vMagicCore[1],
                                s_State.vMagicCore[2], s_State.vMagicCore[3] };
        material.fMaterialRandom = s_State.fMaterialRandom;

        fx.SetMaterialFromDesc(material, CurrentDepthMode());

        return LegacyFx::MakeAssetFromBillboard(fx, CurrentAssetName());
    }

    EntityID SpawnCurrentWfxAsset(CScene_InGame* pScene)
    {
        if (!pScene)
            return NULL_ENTITY;

        CWorld& world = pScene->GetWorld();
        Engine::CFxStaticMeshRenderer* pRenderer = pScene->GetFxMeshRenderer();

        s_LastWfxLoadError.clear();

        FxAssetLoadResult loadResult = LoadFxAssetFromFile(CurrentWfxPath());
        if (!loadResult.bSucceeded)
        {
            s_LastWfxLoadError = loadResult.strError;
            return NULL_ENTITY;
        }

        CFxAssetRegistry previewRegistry{};
        const FxAssetHandle handle =
            previewRegistry.RegisterOrReplaceByName(std::move(loadResult.asset));
        if (!handle.IsValid())
        {
            s_LastWfxLoadError = "preview_register_failed";
            return NULL_ENTITY;
        }

        const FxAsset* pAsset = previewRegistry.Find(handle);
        if (!pAsset)
        {
            s_LastWfxLoadError = "preview_asset_missing";
            return NULL_ENTITY;
        }

        const EntityID player = pScene->GetPlayerEntity();
        const Vec3 vSpawnPos{ 0.f, 0.f, 5.f };

        if (s_State.ePreset == static_cast<int>(ePreset::EBeam))
        {
            return CFxMeshSystem::SpawnFromAsset(
                world, pRenderer, previewRegistry, handle, vSpawnPos, NULL_ENTITY);
        }

        return CFxSystem::SpawnFromAsset(world, *pAsset, vSpawnPos, player);
    }
}

void UI::CEffectTuner::Render(CScene_InGame* pScene)
{
    if (!pScene) return;

    if (!ImGui::Begin("Effect Tuner — Irelia"))
    {
        ImGui::End();
        return;
    }

    // ── 프리셋 선택 ──
    ImGui::Combo("Preset", &s_State.ePreset, kPresetNames, IM_ARRAYSIZE(kPresetNames));
    ImGui::Separator();

    // ── 공통 슬라이더 ──
    ImGui::ColorEdit4("Color (RGBA)", s_State.vColor);
    ImGui::SliderFloat("Width", &s_State.fWidth, 0.1f, 10.f, "%.2f");
    ImGui::SliderFloat("Height", &s_State.fHeight, 0.1f, 10.f, "%.2f");
    ImGui::SliderFloat("Lifetime", &s_State.fLifetime, 0.1f, 5.f, "%.2f");
    ImGui::Combo("Blend", &s_State.iBlendMode, kBlendModeNames, IM_ARRAYSIZE(kBlendModeNames));

    // ── Atlas (sprite 만) ──
    if (s_State.ePreset == (int)ePreset::WSpin
        || s_State.ePreset == (int)ePreset::QTrail
        || s_State.ePreset == (int)ePreset::QMark)
    {
        ImGui::Spacing();
        ImGui::Text("Atlas:");
        ImGui::SliderInt("Cols", &s_State.iAtlasCols, 1, 8);
        ImGui::SliderInt("Rows", &s_State.iAtlasRows, 1, 8);
        ImGui::SliderInt("FPS", &s_State.iAtlasFps, 0, 60);
    }

    // ── UV scroll ──
    ImGui::Spacing();
    ImGui::SliderFloat("UV Scroll U", &s_State.fUVScrollU, -2.f, 2.f, "%.2f");
    ImGui::SliderFloat("UV Scroll V", &s_State.fUVScrollV, -2.f, 2.f, "%.2f");

    // ── Mesh 전용 (E beam) ──
    if (s_State.ePreset == (int)ePreset::EBeam)
    {
        ImGui::Spacing();
        ImGui::Text("Mesh (E Beam):");
        ImGui::SliderFloat3("Scale", s_State.vScale, 0.001f, 1.f, "%.4f");
        ImGui::SliderFloat3("Rotation", s_State.vRotation, -3.14f, 3.14f, "%.3f");
        ImGui::SliderFloat("Alpha Clip", &s_State.fAlphaClip, 0.f, 1.f, "%.3f");
        ImGui::SliderFloat("Erode Threshold", &s_State.fErodeThreshold, 0.f, 1.f, "%.3f");
        ImGui::Combo("Depth Mode", &s_State.iDepthMode,
            kDepthModeNames, IM_ARRAYSIZE(kDepthModeNames));
        s_State.bDepthWrite = FxDepthModeWritesDepth(CurrentDepthMode());
        ImGui::Spacing();
        ImGui::Combo("Style Mode", &s_State.iStyleMode,
            kStyleModeNames, IM_ARRAYSIZE(kStyleModeNames));
        ImGui::ColorEdit4("Hot/Top + Emission", s_State.vStyleColorA);
        ImGui::ColorEdit4("Outline/Bottom + Contrast", s_State.vStyleColorB);
        ImGui::ColorEdit4("Rim + Intensity", s_State.vRimColor);
        ImGui::SliderFloat("Rim Power", &s_State.fRimPower, 0.5f, 8.f, "%.2f");
        ImGui::SliderFloat("Cell Low", &s_State.fCellLow, -1.f, 1.f, "%.2f");
        ImGui::SliderFloat("Cell High", &s_State.fCellHigh, -1.f, 1.f, "%.2f");
        if (s_State.iStyleMode == 4)
        {
            ImGui::Spacing();
            ImGui::Text("Magic Surface:");
            ImGui::SliderFloat4("Magic Scroll A", s_State.vMagicScrollA,
                -2.f, 2.f, "%.3f");
            ImGui::SliderFloat4("Magic Shape", s_State.vMagicShape,
                0.f, 5.f, "%.3f");
            ImGui::SliderFloat4("Magic Core", s_State.vMagicCore,
                0.f, 5.f, "%.3f");
            ImGui::SliderFloat("Material Random", &s_State.fMaterialRandom,
                0.f, 1.f, "%.3f");
        }
    }

    ImGui::Separator();

    if (ImGui::CollapsingHeader("Irelia Live Tuning", ImGuiTreeNodeFlags_DefaultOpen))
    {
        Irelia::IreliaTuning& t = Irelia::GetTuning();

        ImGui::SliderFloat("E Main Blade Scale Mul", &t.fPlacedBladeScaleMul, 0.05f, 1.0f, "%.3f");
        ImGui::SliderFloat("E Orbit Blade Scale Mul", &t.fOrbitBladeScaleMul, 0.05f, 2.0f, "%.3f");
        ImGui::SliderFloat("E Orbit Radius", &t.fOrbitRadius, 0.1f, 2.5f, "%.2f");
        ImGui::SliderFloat("E Orbit Angular Speed", &t.fOrbitAngularSpeed, 0.f, 12.f, "%.2f");
        ImGui::SliderFloat("E Orbit Spin Mul", &t.fOrbitSpinSpeedMul, 0.f, 5.f, "%.2f");

        ImGui::ColorEdit4("E Blade Color", &t.eBladeColor.x);
        ImGui::ColorEdit4("E Ground Glow", &t.eGroundGlowColor.x);
        ImGui::ColorEdit4("E Ground Core", &t.eGroundCoreColor.x);
        ImGui::SliderFloat("E Ground Y Offset", &t.eGroundYOffset, -0.2f, 0.2f, "%.3f");
        ImGui::SliderFloat("E Ground Glow Size", &t.eGroundGlowSize, 0.5f, 5.0f, "%.2f");
        ImGui::SliderFloat("E Ground Core Size", &t.eGroundCoreSize, 0.2f, 3.0f, "%.2f");
        ImGui::SliderFloat("E Ground Spin", &t.eGroundSpinSpeed, -5.f, 5.f, "%.2f");
        ImGui::ColorEdit4("E Close Spark", &t.eCloseSparkColor.x);
        ImGui::ColorEdit4("E Close Beam", &t.eCloseBeamColor.x);
        ImGui::SliderFloat("E Close Spark Size", &t.eCloseSparkSize, 0.2f, 5.0f, "%.2f");
        ImGui::SliderFloat("E Close Beam Width", &t.eCloseBeamWidth, 0.1f, 4.0f, "%.2f");
        ImGui::SliderFloat("E Close Core Width Mul", &t.fECloseCoreWidthMul, 0.1f, 2.0f, "%.2f");
        ImGui::SliderFloat("E Close Dark Width Mul", &t.fECloseDarkWidthMul, 0.5f, 4.0f, "%.2f");
        ImGui::SliderFloat("E Close Afterglow Width Mul", &t.fECloseAfterglowWidthMul, 0.5f, 5.0f, "%.2f");
        ImGui::SliderFloat("E Close Streak Width", &t.fECloseStreakWidth, 0.05f, 2.0f, "%.2f");
        ImGui::SliderFloat("E Close Streak Lifetime", &t.fECloseStreakLifetime, 0.05f, 0.5f, "%.2f");
        ImGui::ColorEdit4("E Close Afterglow", &t.vECloseAfterglowColor.x);
        ImGui::ColorEdit4("E Close Flash", &t.vECloseFlashColor.x);

        ImGui::ColorEdit4("Q Trail Color", &t.qTrailColor.x);
        ImGui::SliderFloat("Q Trail Y Offset", &t.qTrailYOffset, -0.2f, 2.0f, "%.2f");
        ImGui::SliderFloat("Q Trail Width", &t.qTrailWidth, 0.1f, 5.0f, "%.2f");
        ImGui::SliderFloat("Q Trail Height", &t.qTrailHeight, 0.05f, 3.0f, "%.2f");
        ImGui::SliderFloat("Q Trail Lifetime Max", &t.qTrailLifetimeMax, 0.05f, 1.0f, "%.2f");
        ImGui::SliderFloat("Q Trail FPS", &t.qTrailAtlasFps, 1.f, 60.f, "%.1f");

        ImGui::ColorEdit4("W Release Blades", &t.wLayerBladesColor.x);
        ImGui::ColorEdit4("W Release Glow", &t.wLayerGlowColor.x);
        ImGui::SliderFloat("W Release Size", &t.wLayerSize, 0.2f, 6.f, "%.2f");
        ImGui::SliderFloat("W Release Lifetime", &t.wLayerLifetime, 0.05f, 2.f, "%.2f");
        ImGui::SliderFloat("W Hold Shield Size", &t.fWHoldShieldSize, 0.5f, 6.0f, "%.2f");
        ImGui::SliderFloat("W Hold Glow Size", &t.fWHoldGlowSize, 0.5f, 7.0f, "%.2f");
        ImGui::ColorEdit4("W Hold Shield", &t.vWHoldShieldColor.x);
        ImGui::ColorEdit4("W Hold Glow", &t.vWHoldGlowColor.x);

        ImGui::SliderFloat("R Wave Speed", &t.waveSpeed, 1.f, 40.f, "%.2f");
        ImGui::SliderFloat("R Pulse Width", &t.rFxWidth, 0.5f, 8.f, "%.2f");
        ImGui::SliderFloat("R Pulse Height", &t.rFxHeight, 0.5f, 8.f, "%.2f");
        ImGui::SliderFloat("R Y Offset", &t.rFxYOffset, -0.2f, 2.5f, "%.2f");
        ImGui::SliderFloat("R Fwd Offset", &t.rFxFwdOffset, -2.f, 6.f, "%.2f");
        ImGui::SliderFloat("R Yaw Offset", &t.rFxYawOffset, -3.141f, 3.141f, "%.3f");
        ImGui::SliderFloat("R Trail Width Mul", &t.fRTrailWidthMul, 0.2f, 4.0f, "%.2f");
        ImGui::SliderFloat("R Trail Height Mul", &t.fRTrailHeightMul, 0.2f, 4.0f, "%.2f");
        ImGui::ColorEdit4("R Trail Color", &t.vRTrailColor.x);
        ImGui::ColorEdit4("R Lead Color", &t.vRLeadColor.x);
        if (ImGui::Button("Reset Irelia Tuning"))
            Irelia::ResetTuning();

        ImGui::Separator();
    }

    // ── Spawn Test 버튼 ──
    if (ImGui::Button("Spawn Test (Player Pos)", ImVec2(180, 30)))
    {
        EntityID player = pScene->GetPlayerEntity();
        CWorld&  world  = pScene->GetWorld();
        Engine::CFxStaticMeshRenderer* pRenderer = pScene->GetFxMeshRenderer();

        // 캐릭터 위치 / forward 추정 — Q/E/R 의 일부는 캐릭터 transform 필요
        // 단순화: forward = +Z 가정 (테스트용). 실제 spawn 은 게임 로직에서 정확한 fwd 사용.
        const Vec3 vForward{ 0.f, 0.f, 1.f };
        // hit 위치는 캐릭터 앞 5m 가정
        const Vec3 vHitPos{ 0.f, 0.f, 5.f };

        switch ((ePreset)s_State.ePreset)
        {
        case ePreset::QTrail:
            IreliaFx::SpawnQTrail(world, player, s_State.fLifetime);
            break;
        case ePreset::QMark:
            IreliaFx::SpawnQMark(world, player, s_State.fLifetime);
            break;
        case ePreset::WSpin:
            IreliaFx::SpawnWSpin(world, player, s_State.fLifetime);
            break;
        case ePreset::WStage2Slash:
            IreliaFx::SpawnWStage2Slash(world, player, vForward);
            break;
        case ePreset::EBeam:
            IreliaFx::SpawnEBeam(world, pRenderer, vHitPos,
                /*fYaw*/ 0.f, /*fLength*/ 5.f,
                /*fGirth*/ 1.f, /*fBaseScale*/ 0.01f, /*fAxisScale*/ 1.f);
            break;
        case ePreset::RPulse:
            IreliaFx::SpawnRPulse(world, vHitPos, vForward,
                /*fSpeed*/ 25.f, s_State.fLifetime,
                s_State.fWidth, s_State.fHeight,
                /*fYOffset*/ 1.5f, /*fFwdOffset*/ 1.2f, /*fYawOffset*/ 1.5708f);
            break;
        default: break;
        }
    }

    ImGui::SameLine();

    // ── Save Preset 버튼 — 클립보드에 코드 복사 ──
    if (ImGui::Button("Save Preset (Clipboard)", ImVec2(180, 30)))
    {
        char buf[1024];
        snprintf(buf, sizeof(buf),
            "// Generated by EffectTuner — paste into IreliaFxPresets.cpp\n"
            "fx.vColor    = { %.3ff, %.3ff, %.3ff, %.3ff };\n"
            "fx.fWidth    = %.3ff;\n"
            "fx.fHeight   = %.3ff;\n"
            "fx.fLifetime = %.3ff;\n"
            "fx.blendMode = static_cast<eBlendPreset>(%d);\n"
            "fx.iAtlasCols = %d; fx.iAtlasRows = %d; fx.fAtlasFps = %d.f;\n",
            s_State.vColor[0], s_State.vColor[1], s_State.vColor[2], s_State.vColor[3],
            s_State.fWidth, s_State.fHeight, s_State.fLifetime,
            s_State.iBlendMode,
            s_State.iAtlasCols, s_State.iAtlasRows, s_State.iAtlasFps);
        ImGui::SetClipboardText(buf);
        ImGui::OpenPopup("preset_saved");
    }

    ImGui::SameLine();

    if (ImGui::Button("Dump EFX-0 Manifest", ImVec2(180, 30)))
    {
        const bool bSaved = LegacyFx::SaveSeedManifest(
            L"Data/LoL/FX/Manifest/LegacyFxSeedManifest.json");
        if (bSaved)
            ImGui::OpenPopup("manifest_saved");
        else
            ImGui::OpenPopup("manifest_save_failed");
    }

    ImGui::SameLine();

    if (ImGui::Button("Dump Current .wfx", ImVec2(180, 30)))
    {
        const FxAsset asset = BuildCurrentWfxAsset();
        const bool bSaved = LegacyFx::SaveAssetAsWfx(CurrentWfxPath(), asset);
        if (bSaved)
            ImGui::OpenPopup("wfx_saved");
        else
            ImGui::OpenPopup("wfx_save_failed");
    }

    ImGui::SameLine();

    if (ImGui::Button("Load .wfx + Spawn", ImVec2(180, 30)))
    {
        const EntityID spawned = SpawnCurrentWfxAsset(pScene);
        if (spawned != NULL_ENTITY)
            ImGui::OpenPopup("wfx_loaded_spawned");
        else
            ImGui::OpenPopup("wfx_load_spawn_failed");
    }

    if (ImGui::BeginPopup("preset_saved"))
    {
        ImGui::Text("Preset 코드 클립보드 복사 완료.\nIreliaFxPresets.cpp 의 해당 함수에 붙여넣기.");
        if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("manifest_saved"))
    {
        ImGui::Text("EFX-0 legacy manifest written.");
        if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("manifest_save_failed"))
    {
        ImGui::Text("Failed to write EFX-0 legacy manifest.");
        if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("wfx_saved"))
    {
        ImGui::Text("Current EFX-0 .wfx written.");
        if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("wfx_save_failed"))
    {
        ImGui::Text("Failed to write current EFX-0 .wfx.");
        if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("wfx_loaded_spawned"))
    {
        ImGui::Text("Loaded current .wfx and spawned asset.");
        if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("wfx_load_spawn_failed"))
    {
        ImGui::Text("Failed to load or spawn current .wfx.");
        if (!s_LastWfxLoadError.empty())
            ImGui::Text("Reason: %s", s_LastWfxLoadError.c_str());
        if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::End();
}
