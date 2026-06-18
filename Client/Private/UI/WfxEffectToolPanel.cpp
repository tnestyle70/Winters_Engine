#include "UI/WfxEffectToolPanel.h"

#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "GameObject/Champion/Irelia/Irelia_Tuning.h"
#include "GameObject/FX/FxCuePlayer.h"
#include "GameObject/FX/FxRibbonComponent.h"
#include "GameObject/FX/FxSystem.h"
#include "GameObject/FX/WfxDocument.h"
#include "Scene/Scene_InGame.h"
#include "UI/WfxAssetCatalog.h"
#include "WintersMath.h"

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

#include <cstddef>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace
{
    constexpr const char* kRenderTypeNames[] = {
        "Billboard", "Ribbon", "Beam", "GroundDecal", "MeshParticle", "ShockwaveRing"
    };

    constexpr const char* kBlendModeNames[] = {
        "Opaque", "AlphaBlend", "PremultipliedAlpha", "Additive"
    };

    constexpr const char* kDepthModeNames[] = {
        "DepthTestWriteOn", "DepthTestWriteOff", "OverlayNoDepth", "SoftParticle"
    };

    constexpr const char* kAnchorTypeNames[] = {
        "Entity", "World", "Bone", "Socket", "Submesh", "TargetSegment"
    };

    constexpr const char* kAnchorFallbackNames[] = {
        "None", "Entity", "WorldPosition"
    };

    constexpr const char* kLifecycleModeNames[] = {
        "Burst", "Timed", "WhileState", "ManualStop", "LoopUntilSignal"
    };

    struct ToolState
    {
        UI::CWfxAssetCatalog catalog;
        WfxTool::CWfxDocument document;
        int iSelectedAsset = -1;
        int iSelectedTexture = -1;
        int iSelectedEmitter = -1;
        char szRootPath[512] = "Data/LoL/FX/Champions";
        char szTextureRoot[512] = "Client/Bin/Resource/Texture/Character";
        char szPath[512] = "Data/LoL/FX/Champions/Annie/q_fireball.wfx";
        char szNewCueName[128] = "Wfx.New";
        char szNewWfxPath[512] = "Data/LoL/FX/Champions/New/new_effect.wfx";
        //Q Quick
        char szQuickCueName[128] = "Irelia.Q.LeadingEdge";
        char szQuickWfxPath[512] = "Data/LoL/FX/Champions/Irelia/q_leadingedge.wfx";
        int iQuickAtlasCols = 2;
        int iQuickAtlasRows = 2;
        int iQuickAtlasFrames = 4;
        f32_t fQuickDuration = 0.25f;
        f32_t fQuickSize = 2.45f;
        Vec3 vQuickAttachOffset = { 0.f, 1.18f, 0.f };
        Vec4 vQuickColor = { 0.68f, 0.92f, 1.90f, 0.85f };

        std::string strStatus;
        bool_t bAttachPreviewToPlayer = false;
        bool_t bPreviewUseMouseDirection = false;
        f32_t fPreviewDistance = 3.0f;
        f32_t fPreviewSegmentLength = 6.f;
        f32_t fPreviewAnchorYOffset = 0.f;
        bool_t bInitialized = false;
    };

    ToolState s_State;

    wstring_t WidenAscii(const char* pszValue)
    {
        if (!pszValue)
            return {};
        std::string value = pszValue;
        return wstring_t(value.begin(), value.end());
    }

    std::string NarrowAscii(const wstring_t& value)
    {
        std::string out;
        out.reserve(value.size());
        for (const auto ch : value)
            out.push_back(ch <= 0x7f ? static_cast<char>(ch) : '?');
        return out;
    }

    std::string NormalizePathText(std::string value)
    {
        for (char& ch : value)
        {
            if (ch == '\\')
                ch = '/';
        }
        return value;
    }

    template <size_t Size>
    void CopyStringToBuffer(char(&buffer)[Size], const std::string& value)
    {
        std::snprintf(buffer, Size, "%s", value.c_str());
    }

    void SetStatus(const char* pszPrefix, const std::string& detail)
    {
        s_State.strStatus = pszPrefix ? pszPrefix : "";
        if (!detail.empty())
        {
            s_State.strStatus += ": ";
            s_State.strStatus += detail;
        }
    }

    bool_t LoadCurrentPath()
    {
        const wstring_t path = WidenAscii(s_State.szPath);
        if (!s_State.document.LoadFromFile(path))
        {
            s_State.iSelectedEmitter = -1;
            SetStatus("Load failed", s_State.document.GetLastError());
            return false;
        }

        s_State.iSelectedEmitter =
            s_State.document.GetAsset().emitters.empty() ? -1 : 0;
        CFxSystem::GetAssetRegistry().RegisterOrReplaceByName(s_State.document.GetAsset());
        SetStatus("Loaded", s_State.szPath);
        return true;
    }

    bool_t SaveCurrentPath()
    {
        const wstring_t path = WidenAscii(s_State.szPath);
        if (!s_State.document.SaveToFile(path))
        {
            SetStatus("Save failed", s_State.document.GetLastError());
            return false;
        }

        CFxSystem::GetAssetRegistry().RegisterOrReplaceByName(s_State.document.GetAsset());
        SetStatus("Saved", s_State.szPath);
        return true;
    }

    int FindCatalogEntryByPath(const char* pszPath)
    {
        const std::string target = NormalizePathText(pszPath ? pszPath : "");
        const std::vector<UI::WfxAssetEntry>& entries = s_State.catalog.GetEntries();
        for (size_t i = 0; i < entries.size(); ++i)
        {
            if (entries[i].strPathText == target)
                return static_cast<int>(i);
        }

        return -1;
    }

    bool_t SelectCatalogEntry(size_t iIndex, bool_t bLoad)
    {
        const UI::WfxAssetEntry* pEntry = s_State.catalog.GetEntry(iIndex);
        if (!pEntry)
            return false;

        s_State.iSelectedAsset = static_cast<int>(iIndex);
        CopyStringToBuffer(s_State.szPath, pEntry->strPathText);
        return bLoad ? LoadCurrentPath() : true;
    }

    bool_t ScanCatalog(bool_t bLoadSelection)
    {
        const u32_t iCount = s_State.catalog.ScanDirectory(WidenAscii(s_State.szRootPath));
        if (iCount == 0u)
        {
            s_State.iSelectedAsset = -1;
            SetStatus("Catalog scan failed", s_State.catalog.GetLastError());
            return false;
        }

        int iSelection = FindCatalogEntryByPath(s_State.szPath);
        if (iSelection < 0)
            iSelection = 0;

        char detail[64]{};
        std::snprintf(detail, sizeof(detail), "%u assets", iCount);
        SetStatus("Catalog scanned", detail);
        SelectCatalogEntry(static_cast<size_t>(iSelection), bLoadSelection);
        return true;
    }

    bool_t ScanTextureCatalog()
    {
        const u32_t iCount =
            s_State.catalog.ScanTextureDirectory(WidenAscii(s_State.szTextureRoot));
        if (iCount == 0u)
        {
            s_State.iSelectedTexture = -1;
            SetStatus("Texture scan failed", s_State.catalog.GetLastError());
            return false;
        }

        s_State.iSelectedTexture = 0;

        char detail[64]{};
        std::snprintf(detail, sizeof(detail), "%u textures", iCount);
        SetStatus("Textures scanned", detail);
        return true;
    }

    eFxDepthMode DepthModeFromIndex(int index)
    {
        switch (index)
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

    int DepthModeToIndex(eFxDepthMode mode)
    {
        switch (mode)
        {
        case eFxDepthMode::DepthTestWriteOff:
            return 1;
        case eFxDepthMode::OverlayNoDepth:
            return 2;
        case eFxDepthMode::SoftParticle:
            return 3;
        case eFxDepthMode::DepthTestWriteOn:
        default:
            return 0;
        }
    }

    void SyncEditableMaterial(FxEmitterDesc& emitter)
    {
        emitter.material.vTint = emitter.vColor;
        emitter.material.vUVScroll = { emitter.fUvScrollU, emitter.fUvScrollV };
        emitter.material.fAlphaClip = emitter.fAlphaClip;
        emitter.material.fErodeThreshold = emitter.fErodeThreshold;
        emitter.material.iStyleMode = emitter.iStyleMode;
        emitter.material.vStyleColorA = emitter.vStyleColorA;
        emitter.material.vStyleColorB = emitter.vStyleColorB;
        emitter.material.vRimColor = emitter.vRimColor;
        emitter.material.fRimPower = emitter.fRimPower;
        emitter.material.fCellLow = emitter.fCellLow;
        emitter.material.fCellHigh = emitter.fCellHigh;
        emitter.material.vMagicScrollA = emitter.vMagicScrollA;
        emitter.material.vMagicShape = emitter.vMagicShape;
        emitter.material.vMagicCore = emitter.vMagicCore;
        emitter.material.fMaterialRandom = emitter.fMaterialRandom;
        emitter.bDepthWrite = FxDepthModeWritesDepth(emitter.depthMode);
    }

    FxEmitterDesc BuildDefaultBillboardEmitterFromTexture(const UI::WfxTextureEntry& entry)
    {
        FxEmitterDesc emitter{};
        emitter.strName = entry.strName;
        emitter.renderType = eFxRenderType::Billboard;
        emitter.maxParticles = 1;
        emitter.spawnRate = 0.f;
        emitter.strTexturePath = entry.strPath;
        emitter.blendMode = eBlendPreset::Additive;
        emitter.depthMode = eFxDepthMode::OverlayNoDepth;
        emitter.fLifetime = 1.f;
        emitter.fFadeOut = 0.15f;
        emitter.fWidth = 1.5f;
        emitter.fHeight = 1.5f;
        emitter.vColor = { 1.f, 1.f, 1.f, 1.f };
        emitter.bBillboard = true;
        SyncEditableMaterial(emitter);
        return emitter;
    }

    void NormalizeQuickAtlasSettings()
    {
        if (s_State.iQuickAtlasCols < 1)
            s_State.iQuickAtlasCols = 1;
        if (s_State.iQuickAtlasRows < 1)
            s_State.iQuickAtlasRows = 1;

        const int maxFrames = s_State.iQuickAtlasCols * s_State.iQuickAtlasRows;
        if (s_State.iQuickAtlasFrames < 1)
            s_State.iQuickAtlasFrames = 1;
        if (s_State.iQuickAtlasFrames > maxFrames)
            s_State.iQuickAtlasFrames = maxFrames;

        if (s_State.fQuickDuration < 0.01f)
            s_State.fQuickDuration = 0.01f;
        if (s_State.fQuickSize < 0.01f)
            s_State.fQuickSize = 0.01f;
    }

    FxEmitterDesc BuildQuickAtlasEmitterFromTexture(const UI::WfxTextureEntry& entry)
    {
        NormalizeQuickAtlasSettings();

        FxEmitterDesc emitter{};
        emitter.strName = "q_leadingedge_body_atlas";
        emitter.renderType = eFxRenderType::Billboard;
        emitter.maxParticles = 1;
        emitter.spawnRate = 0.f;
        emitter.strTexturePath = entry.strPath;
        emitter.blendMode = eBlendPreset::Additive;
        emitter.depthMode = eFxDepthMode::OverlayNoDepth;
        emitter.fLifetime = s_State.fQuickDuration;
        emitter.fFadeIn = 0.01f;
        emitter.fFadeOut = s_State.fQuickDuration * 0.4f;
        if (emitter.fFadeOut > 0.12f)
            emitter.fFadeOut = 0.12f;
        emitter.fWidth = s_State.fQuickSize;
        emitter.fHeight = s_State.fQuickSize;
        emitter.vAttachOffset = s_State.vQuickAttachOffset;
        emitter.vColor = s_State.vQuickColor;
        emitter.iAtlasCols = static_cast<u32_t>(s_State.iQuickAtlasCols);
        emitter.iAtlasRows = static_cast<u32_t>(s_State.iQuickAtlasRows);
        emitter.iAtlasFrameCount = static_cast<u32_t>(s_State.iQuickAtlasFrames);
        emitter.fAtlasFps =
            static_cast<f32_t>(s_State.iQuickAtlasFrames) / s_State.fQuickDuration;
        emitter.bAtlasLoop = false;

        // Q leading edge must rotate with the skill direction; camera-facing billboard ignores yaw.
        emitter.bBillboard = false;

        SyncEditableMaterial(emitter);
        return emitter;
    }

    bool_t SelectIreliaQLeadingEdgeTexture()
    {
        if (s_State.catalog.GetTextureEntryCount() == 0u)
            ScanTextureCatalog();

        constexpr const char* pszQTexture =
            "Client/Bin/Resource/Texture/Character/Irelia/particles/irelia_base_q_leadingedge.png";

        const std::vector<UI::WfxTextureEntry>& entries =
            s_State.catalog.GetTextureEntries();
        for (size_t i = 0; i < entries.size(); ++i)
        {
            if (NormalizePathText(entries[i].strPathText) == pszQTexture)
            {
                s_State.iSelectedTexture = static_cast<int>(i);
                SetStatus("Q texture selected", entries[i].strPathText);
                return true;
            }
        }

        SetStatus("Q texture missing", pszQTexture);
        return false;
    }

    bool_t CreateQuickAtlasCueFromSelectedTexture()
    {
        const UI::WfxTextureEntry* pEntry =
            s_State.catalog.GetTextureEntry(static_cast<size_t>(s_State.iSelectedTexture));
        if (!pEntry)
        {
            SetStatus("Quick Q failed", "texture_not_selected");
            return false;
        }

        FxAsset asset{};
        asset.strName = s_State.szQuickCueName[0]
            ? s_State.szQuickCueName
            : "Irelia.Q.LeadingEdge";
        asset.emitters.push_back(BuildQuickAtlasEmitterFromTexture(*pEntry));

        CopyStringToBuffer(s_State.szPath, std::string(s_State.szQuickWfxPath));
        s_State.document.SetAsset(std::move(asset), WidenAscii(s_State.szQuickWfxPath));
        s_State.iSelectedEmitter = 0;
        SetStatus("Quick Q cue built", pEntry->strPathText);
        return true;
    }

    FxEmitterDesc* GetSelectedEmitter()
    {
        if (!s_State.document.IsLoaded())
            return nullptr;

        std::vector<FxEmitterDesc>& emitters = s_State.document.GetAsset().emitters;
        if (emitters.empty())
        {
            s_State.iSelectedEmitter = -1;
            return nullptr;
        }

        if (s_State.iSelectedEmitter < 0 ||
            s_State.iSelectedEmitter >= static_cast<int>(emitters.size()))
        {
            s_State.iSelectedEmitter = 0;
        }

        return &emitters[static_cast<size_t>(s_State.iSelectedEmitter)];
    }

    bool_t AppendEmitterFromSelectedTexture()
    {
        if (!s_State.document.IsLoaded())
        {
            SetStatus("Add emitter failed", "document_not_loaded");
            return false;
        }

        const UI::WfxTextureEntry* pEntry =
            s_State.catalog.GetTextureEntry(static_cast<size_t>(s_State.iSelectedTexture));
        if (!pEntry)
        {
            SetStatus("Add emitter failed", "texture_not_selected");
            return false;
        }

        std::vector<FxEmitterDesc>& emitters = s_State.document.GetAsset().emitters;
        emitters.push_back(BuildDefaultBillboardEmitterFromTexture(*pEntry));
        s_State.iSelectedEmitter = static_cast<int>(emitters.size()) - 1;
        SetStatus("Emitter added", pEntry->strPathText);
        return true;
    }

    bool_t ApplySelectedTextureToSelectedEmitter()
    {
        FxEmitterDesc* pEmitter = GetSelectedEmitter();
        const UI::WfxTextureEntry* pEntry =
            s_State.catalog.GetTextureEntry(static_cast<size_t>(s_State.iSelectedTexture));
        if (!pEmitter || !pEntry)
        {
            SetStatus("Apply texture failed", "emitter_or_texture_missing");
            return false;
        }

        pEmitter->strTexturePath = pEntry->strPath;
        SyncEditableMaterial(*pEmitter);
        SetStatus("Texture applied", pEntry->strPathText);
        return true;
    }

    bool_t DuplicateSelectedEmitter()
    {
        if (!s_State.document.IsLoaded())
            return false;

        std::vector<FxEmitterDesc>& emitters = s_State.document.GetAsset().emitters;
        FxEmitterDesc* pEmitter = GetSelectedEmitter();
        if (!pEmitter)
            return false;

        emitters.push_back(*pEmitter);
        s_State.iSelectedEmitter = static_cast<int>(emitters.size()) - 1;
        SetStatus("Emitter duplicated", emitters.back().strName);
        return true;
    }

    bool_t DeleteSelectedEmitter()
    {
        if (!s_State.document.IsLoaded())
            return false;

        std::vector<FxEmitterDesc>& emitters = s_State.document.GetAsset().emitters;
        if (s_State.iSelectedEmitter < 0 ||
            s_State.iSelectedEmitter >= static_cast<int>(emitters.size()))
        {
            return false;
        }

        emitters.erase(emitters.begin() + s_State.iSelectedEmitter);
        if (emitters.empty())
            s_State.iSelectedEmitter = -1;
        else if (s_State.iSelectedEmitter >= static_cast<int>(emitters.size()))
            s_State.iSelectedEmitter = static_cast<int>(emitters.size()) - 1;

        SetStatus("Emitter deleted", "");
        return true;
    }

    bool_t CreateDocumentFromSelectedTexture()
    {
        const UI::WfxTextureEntry* pEntry =
            s_State.catalog.GetTextureEntry(static_cast<size_t>(s_State.iSelectedTexture));
        if (!pEntry)
        {
            SetStatus("New WFX failed", "texture_not_selected");
            return false;
        }

        FxAsset asset{};
        asset.strName = s_State.szNewCueName[0] ? s_State.szNewCueName : "Wfx.New";
        asset.emitters.push_back(BuildDefaultBillboardEmitterFromTexture(*pEntry));

        CopyStringToBuffer(s_State.szPath, std::string(s_State.szNewWfxPath));
        s_State.document.SetAsset(std::move(asset), WidenAscii(s_State.szNewWfxPath));
        s_State.iSelectedEmitter = 0;
        SetStatus("New WFX from texture", pEntry->strPathText);
        return true;
    }

    void InputTextString(const char* pszLabel, std::string& value)
    {
        char buffer[512]{};
        std::snprintf(buffer, sizeof(buffer), "%s", value.c_str());
        if (ImGui::InputText(pszLabel, buffer, sizeof(buffer)))
            value = buffer;
    }

    void InputTextWString(const char* pszLabel, wstring_t& value)
    {
        char buffer[512]{};
        const std::string ascii = NarrowAscii(value);
        std::snprintf(buffer, sizeof(buffer), "%s", ascii.c_str());
        if (ImGui::InputText(pszLabel, buffer, sizeof(buffer)))
            value = WidenAscii(buffer);
    }

    void DragUInt(const char* pszLabel, u32_t& value, int minValue, int maxValue)
    {
        int editable = static_cast<int>(value);
        if (ImGui::DragInt(pszLabel, &editable, 1.f, minValue, maxValue))
            value = static_cast<u32_t>(editable < minValue ? minValue : editable);
    }

    void RenderTransformEditor(FxEmitterDesc& emitter)
    {
        if (ImGui::TreeNode("Transform"))
        {
            ImGui::DragFloat3("Attach Offset", &emitter.vAttachOffset.x, 0.02f, -20.f, 20.f, "%.3f");
            ImGui::DragFloat3("End Offset", &emitter.vEndOffset.x, 0.02f, -50.f, 50.f, "%.3f");
            ImGui::DragFloat3("Velocity", &emitter.vVelocity.x, 0.02f, -80.f, 80.f, "%.3f");
            ImGui::DragFloat3("Scale", &emitter.vScale.x, 0.001f, 0.f, 10.f, "%.4f");
            ImGui::DragFloat3("Rotation", &emitter.vRotation.x, 0.01f, -6.28318f, 6.28318f, "%.4f");
            ImGui::DragFloat("World Yaw Spin Speed", &emitter.fWorldYawSpinSpeed, 0.01f, -80.f, 80.f, "%.4f");
            ImGui::DragFloat("Yaw", &emitter.fYaw, 0.01f, -6.28318f, 6.28318f, "%.4f");
            ImGui::DragFloat("Segment T", &emitter.fSegmentT, 0.01f, -1.f, 1.f, "%.3f");
            bool bScaleZToSegment = emitter.bScaleZToSegment;
            if (ImGui::Checkbox("Scale Z To Segment", &bScaleZToSegment))
                emitter.bScaleZToSegment = bScaleZToSegment;

            bool bBillboard = emitter.bBillboard;
            if (ImGui::Checkbox("Camera Facing Billboard", &bBillboard))
                emitter.bBillboard = bBillboard;

            int anchorType = static_cast<int>(emitter.anchor.eAnchorType);
            if (ImGui::Combo("Anchor Type", &anchorType,
                kAnchorTypeNames,
                IM_ARRAYSIZE(kAnchorTypeNames)))
            {
                emitter.anchor.eAnchorType = static_cast<eFxAnchorType>(anchorType);
            }
            InputTextString("Anchor Name", emitter.anchor.strAnchorName);
            ImGui::DragFloat3("Anchor Offset", &emitter.anchor.vAnchorOffset.x, 0.02f, -20.f, 20.f, "%.3f");
            if (ImGui::Button("Copy Attach Offset To Anchor"))
                emitter.anchor.vAnchorOffset = emitter.vAttachOffset;

            bool bInheritRotation = emitter.anchor.bInheritRotation;
            if (ImGui::Checkbox("Anchor Inherit Rotation", &bInheritRotation))
                emitter.anchor.bInheritRotation = bInheritRotation;

            int anchorFallback = static_cast<int>(emitter.anchor.eFallback);
            if (ImGui::Combo("Anchor Fallback", &anchorFallback,
                kAnchorFallbackNames,
                IM_ARRAYSIZE(kAnchorFallbackNames)))
            {
                emitter.anchor.eFallback = static_cast<eFxAnchorFallback>(anchorFallback);
            }

            int lifecycleMode = static_cast<int>(emitter.lifecycle.eLifecycleMode);
            if (ImGui::Combo("Lifecycle Mode", &lifecycleMode,
                kLifecycleModeNames,
                IM_ARRAYSIZE(kLifecycleModeNames)))
            {
                emitter.lifecycle.eLifecycleMode = static_cast<eFxLifecycleMode>(lifecycleMode);
            }
            ImGui::DragFloat("Stop Fade Out", &emitter.lifecycle.fStopFadeOut, 0.005f, 0.f, 10.f, "%.3f");
            bool bDetachOnStop = emitter.lifecycle.bDetachOnStop;
            if (ImGui::Checkbox("Detach On Stop", &bDetachOnStop))
                emitter.lifecycle.bDetachOnStop = bDetachOnStop;
            bool bKillWhenAnchorInvalid = emitter.lifecycle.bKillWhenAnchorInvalid;
            if (ImGui::Checkbox("Kill When Anchor Invalid", &bKillWhenAnchorInvalid))
                emitter.lifecycle.bKillWhenAnchorInvalid = bKillWhenAnchorInvalid;

            ImGui::TreePop();
        }
    }

    void RenderPathEditor(FxEmitterDesc& emitter)
    {
        if (ImGui::TreeNode("Paths"))
        {
            InputTextWString("Texture", emitter.strTexturePath);
            InputTextWString("Erode Texture", emitter.strErodeTexturePath);
            InputTextString("Model", emitter.strModelPath);
            ImGui::TreePop();
        }
    }

    void RenderMaterialEditor(FxEmitterDesc& emitter)
    {
        if (ImGui::TreeNode("Material"))
        {
            ImGui::DragFloat2("UV Scroll", &emitter.fUvScrollU, 0.01f, -8.f, 8.f, "%.3f");
            ImGui::DragFloat("Alpha Clip", &emitter.fAlphaClip, 0.005f, 0.f, 1.f, "%.3f");
            ImGui::DragFloat("Erode Threshold", &emitter.fErodeThreshold, 0.005f, 0.f, 1.f, "%.3f");
            DragUInt("Style Mode", emitter.iStyleMode, 0, 8);
            ImGui::ColorEdit4("Style Color A", &emitter.vStyleColorA.x);
            ImGui::ColorEdit4("Style Color B", &emitter.vStyleColorB.x);
            ImGui::ColorEdit4("Rim Color", &emitter.vRimColor.x);
            ImGui::DragFloat("Rim Power", &emitter.fRimPower, 0.05f, 0.f, 16.f, "%.3f");
            ImGui::DragFloat("Cell Low", &emitter.fCellLow, 0.01f, 0.f, 1.f, "%.3f");
            ImGui::DragFloat("Cell High", &emitter.fCellHigh, 0.01f, 0.f, 1.f, "%.3f");
            ImGui::DragFloat4("Magic Scroll A", &emitter.vMagicScrollA.x, 0.01f, -8.f, 8.f, "%.3f");
            ImGui::DragFloat4("Magic Shape", &emitter.vMagicShape.x, 0.01f, -8.f, 8.f, "%.3f");
            ImGui::DragFloat4("Magic Core", &emitter.vMagicCore.x, 0.01f, -8.f, 8.f, "%.3f");
            ImGui::DragFloat("Material Random", &emitter.fMaterialRandom, 0.01f, 0.f, 1.f, "%.3f");
            ImGui::TreePop();
        }
    }

    void RenderAtlasEditor(FxEmitterDesc& emitter)
    {
        if (ImGui::TreeNode("Atlas / Ribbon"))
        {
            DragUInt("Atlas Cols", emitter.iAtlasCols, 1, 64);
            DragUInt("Atlas Rows", emitter.iAtlasRows, 1, 64);
            DragUInt("Atlas Frame Count", emitter.iAtlasFrameCount, 1, 4096);
            ImGui::DragFloat("Atlas FPS", &emitter.fAtlasFps, 0.25f, 0.f, 120.f, "%.2f");
            bool bAtlasLoop = emitter.bAtlasLoop;
            if (ImGui::Checkbox("Atlas Loop", &bAtlasLoop))
                emitter.bAtlasLoop = bAtlasLoop;
            DragUInt("Ribbon Point Count", emitter.iRibbonPointCount, 2, FX_RIBBON_MAX_POINTS);
            bool bHistoryTrail = emitter.bHistoryTrail;
            if (ImGui::Checkbox("History Trail", &bHistoryTrail))
                emitter.bHistoryTrail = bHistoryTrail;
            ImGui::DragFloat("Trail Sample Interval", &emitter.fTrailSampleInterval, 0.001f, 0.001f, 0.25f, "%.4f");
            ImGui::DragFloat("Trail Head Width Scale", &emitter.fTrailHeadWidthScale, 0.01f, 0.f, 8.f, "%.3f");
            ImGui::DragFloat("Trail Tail Width Scale", &emitter.fTrailTailWidthScale, 0.01f, 0.f, 8.f, "%.3f");
            ImGui::DragFloat("Trail Head Alpha Scale", &emitter.fTrailHeadAlphaScale, 0.01f, 0.f, 8.f, "%.3f");
            ImGui::DragFloat("Trail Tail Alpha Scale", &emitter.fTrailTailAlphaScale, 0.01f, 0.f, 8.f, "%.3f");
            ImGui::DragFloat("Trail Jitter Amplitude", &emitter.fTrailJitterAmplitude, 0.005f, 0.f, 5.f, "%.3f");
            ImGui::DragFloat("Trail Jitter Frequency", &emitter.fTrailJitterFrequency, 0.05f, 0.f, 80.f, "%.3f");
            ImGui::DragFloat("Trail Jitter Seed", &emitter.fTrailJitterSeed, 0.05f, -100.f, 100.f, "%.3f");
            ImGui::TreePop();
        }
    }

    void RenderEmitterEditor()
    {
        if (!s_State.document.IsLoaded())
        {
            ImGui::TextDisabled("No WFX loaded.");
            return;
        }

        FxAsset& asset = s_State.document.GetAsset();
        InputTextString("Cue Name", asset.strName);

        if (ImGui::Button("Add From Texture"))
            AppendEmitterFromSelectedTexture();
        ImGui::SameLine();
        if (ImGui::Button("Duplicate"))
            DuplicateSelectedEmitter();
        ImGui::SameLine();
        if (ImGui::Button("Delete"))
            DeleteSelectedEmitter();

        if (ImGui::BeginTable("WfxEmitterList",
            4,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable,
            ImVec2(0.f, 150.f)))
        {
            ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 36.f);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 110.f);
            ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 80.f);
            ImGui::TableHeadersRow();

            for (size_t i = 0; i < asset.emitters.size(); ++i)
            {
                FxEmitterDesc& emitter = asset.emitters[i];
                const bool_t bSelected = static_cast<int>(i) == s_State.iSelectedEmitter;

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%02zu", i);
                ImGui::TableSetColumnIndex(1);
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::Selectable(
                    emitter.strName.empty() ? "(unnamed)" : emitter.strName.c_str(),
                    bSelected,
                    ImGuiSelectableFlags_SpanAllColumns))
                {
                    s_State.iSelectedEmitter = static_cast<int>(i);
                }
                ImGui::PopID();
                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(kRenderTypeNames[static_cast<int>(emitter.renderType)]);
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%.2f / %.2f", emitter.fStartDelay, emitter.fLifetime);
            }

            ImGui::EndTable();
        }

        FxEmitterDesc* pEmitter = GetSelectedEmitter();
        if (!pEmitter)
        {
            ImGui::TextDisabled("No emitter selected.");
            return;
        }

        FxEmitterDesc& emitter = *pEmitter;
        ImGui::Separator();
        InputTextString("Name", emitter.strName);

        int renderType = static_cast<int>(emitter.renderType);
        if (ImGui::Combo("Render Type", &renderType,
            kRenderTypeNames,
            IM_ARRAYSIZE(kRenderTypeNames)))
        {
            emitter.renderType = static_cast<eFxRenderType>(renderType);
        }

        int blendMode = static_cast<int>(emitter.blendMode);
        if (ImGui::Combo("Blend Mode", &blendMode,
            kBlendModeNames,
            IM_ARRAYSIZE(kBlendModeNames)))
        {
            emitter.blendMode = static_cast<eBlendPreset>(blendMode);
        }

        int depthMode = DepthModeToIndex(emitter.depthMode);
        if (ImGui::Combo("Depth Mode", &depthMode,
            kDepthModeNames,
            IM_ARRAYSIZE(kDepthModeNames)))
        {
            emitter.depthMode = DepthModeFromIndex(depthMode);
        }

        DragUInt("Max Particles", emitter.maxParticles, 1, 4096);
        ImGui::DragFloat("Spawn Rate", &emitter.spawnRate, 0.1f, 0.f, 4096.f, "%.2f");
        ImGui::DragFloat("Lifetime", &emitter.fLifetime, 0.01f, 0.01f, 30.f, "%.3f");
        ImGui::DragFloat("Start Delay", &emitter.fStartDelay, 0.01f, 0.f, 30.f, "%.3f");
        ImGui::DragFloat("Fade In", &emitter.fFadeIn, 0.005f, 0.f, 10.f, "%.3f");
        ImGui::DragFloat("Fade Out", &emitter.fFadeOut, 0.005f, 0.f, 10.f, "%.3f");
        ImGui::DragFloat("Width", &emitter.fWidth, 0.02f, 0.f, 50.f, "%.3f");
        ImGui::DragFloat("Height", &emitter.fHeight, 0.02f, 0.f, 50.f, "%.3f");
        ImGui::ColorEdit4("Color", &emitter.vColor.x);
        RenderTransformEditor(emitter);
        RenderPathEditor(emitter);
        RenderMaterialEditor(emitter);
        RenderAtlasEditor(emitter);
        SyncEditableMaterial(emitter);
    }

    EntityID PreviewEditedAsset(CScene_InGame* pScene)
    {
        if (!pScene || !s_State.document.IsLoaded())
        {
            SetStatus("Preview failed", "document_not_loaded");
            return NULL_ENTITY;
        }

        FxAsset previewAsset = s_State.document.GetAsset();
        std::string cueName = previewAsset.strName;
        if (cueName.empty())
        {
            cueName = "Wfx.Preview";
            previewAsset.strName = cueName;
        }

        CFxSystem::GetAssetRegistry().RegisterOrReplaceByName(std::move(previewAsset));

        CWorld& world = pScene->GetWorld();
        const EntityID player = pScene->GetPlayerEntity();
        Vec3 spawnPos{ 0.f, 0.f, 5.f };
        Vec3 forward{ 0.f, 0.f, 1.f };

        if (player != NULL_ENTITY && world.HasComponent<TransformComponent>(player))
        {
            const TransformComponent& transform = world.GetComponent<TransformComponent>(player);
            spawnPos = transform.GetPosition();
            forward = WintersMath::DirectionFromYawXZ(transform.GetRotation().y);

            if (s_State.bPreviewUseMouseDirection)
            {
                const Vec3 cursorGround = pScene->ResolveMouseMapSurfacePos();
                forward = WintersMath::DirectionXZ(spawnPos, cursorGround, forward);
            }
        }

        spawnPos += forward * s_State.fPreviewDistance;
        spawnPos.y += s_State.fPreviewAnchorYOffset;

        if (cueName == "Irelia.E.Connect")
            spawnPos.y += 3.f;

        const f32_t segmentLength =
            (s_State.fPreviewSegmentLength > 0.1f) ? s_State.fPreviewSegmentLength : 0.1f;

        FxCueContext fx{};
        fx.vWorldPos = spawnPos;
        fx.vForward = forward;
        fx.vEndWorldPos = spawnPos + forward * segmentLength;
        fx.attachTo = s_State.bAttachPreviewToPlayer ? player : NULL_ENTITY;
        fx.pFxMeshRenderer = pScene->GetFxMeshRenderer();
        fx.bOverrideEndWorldPos = true;

        if (cueName == "Irelia.W.Stage2Slash" &&
            player != NULL_ENTITY &&
            world.HasComponent<TransformComponent>(player))
        {
            const Irelia::IreliaTuning& tuning = Irelia::GetTuning();
            const Vec3 playerPos = world.GetComponent<TransformComponent>(player).GetPosition();
            const Vec3 cursorGround = pScene->ResolveMouseMapSurfacePos();
            const Vec3 castForward = WintersMath::DirectionXZ(playerPos, cursorGround, forward);

            Vec3 start = playerPos;
            start.y += tuning.fWReleaseYOffset;

            fx.vWorldPos = start;
            fx.vForward = castForward;
            fx.vEndWorldPos = {
                start.x + castForward.x * tuning.fWReleaseRange,
                start.y,
                start.z + castForward.z * tuning.fWReleaseRange
            };
            fx.attachTo = NULL_ENTITY;
            fx.bOverrideLifetime = true;
            fx.fLifetimeOverride = tuning.wLayerLifetime;
        }

        const EntityID spawned = CFxCuePlayer::PlayAll(world, cueName.c_str(), fx, nullptr);
        if (spawned == NULL_ENTITY)
            SetStatus("Preview failed", cueName);
        else
            SetStatus("Preview spawned", cueName);

        return spawned;
    }

    void RenderAssetTooltip(const UI::WfxAssetEntry& entry)
    {
        if (!ImGui::IsItemHovered())
            return;

        ImGui::BeginTooltip();
        ImGui::TextUnformatted(entry.strPathText.c_str());

        if (!entry.bLoadSucceeded)
        {
            ImGui::Separator();
            ImGui::TextUnformatted(entry.strLoadError.c_str());
        }
        else if (entry.bHasMissingResources)
        {
            ImGui::Separator();
            for (const std::string& missing : entry.missingResources)
                ImGui::BulletText("%s", missing.c_str());
        }

        ImGui::EndTooltip();
    }

    void RenderAssetCatalog()
    {
        const std::vector<UI::WfxAssetEntry>& entries = s_State.catalog.GetEntries();
        ImGui::TextDisabled("%u assets", static_cast<unsigned>(entries.size()));

        constexpr ImGuiTableFlags kTableFlags =
            ImGuiTableFlags_Borders |
            ImGuiTableFlags_RowBg |
            ImGuiTableFlags_Resizable |
            ImGuiTableFlags_ScrollY;

        if (!ImGui::BeginTable("WfxAssetCatalog",
            5,
            kTableFlags,
            ImVec2(0.f, 180.f)))
        {
            return;
        }

        ImGui::TableSetupColumn("Champion", ImGuiTableColumnFlags_WidthFixed, 92.f);
        ImGui::TableSetupColumn("Cue", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Emit", ImGuiTableColumnFlags_WidthFixed, 44.f);
        ImGui::TableSetupColumn("Render", ImGuiTableColumnFlags_WidthFixed, 112.f);
        ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 76.f);
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < entries.size(); ++i)
        {
            const UI::WfxAssetEntry& entry = entries[i];
            const bool_t bSelected = static_cast<int>(i) == s_State.iSelectedAsset;
            const char* pszChampion =
                entry.strChampion.empty() ? "(unknown)" : entry.strChampion.c_str();
            const char* pszCue =
                entry.strCueName.empty() ? entry.strSkill.c_str() : entry.strCueName.c_str();

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::Selectable(pszChampion,
                bSelected,
                ImGuiSelectableFlags_SpanAllColumns))
            {
                SelectCatalogEntry(i, true);
            }
            ImGui::PopID();

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(pszCue);
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%u", entry.iEmitterCount);
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(entry.strRenderTypes.c_str());
            ImGui::TableSetColumnIndex(4);
            if (!entry.bLoadSucceeded)
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "Load");
            else if (entry.bHasMissingResources)
                ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.25f, 1.0f), "Missing");
            else
                ImGui::TextColored(ImVec4(0.45f, 0.95f, 0.55f, 1.0f), "Ready");
            RenderAssetTooltip(entry);
        }

        ImGui::EndTable();
    }

    void RenderTextureCatalog()
    {
        const std::vector<UI::WfxTextureEntry>& entries =
            s_State.catalog.GetTextureEntries();
        ImGui::TextDisabled("%u textures", static_cast<unsigned>(entries.size()));

        constexpr ImGuiTableFlags kTableFlags =
            ImGuiTableFlags_Borders |
            ImGuiTableFlags_RowBg |
            ImGuiTableFlags_Resizable |
            ImGuiTableFlags_ScrollY;

        if (!ImGui::BeginTable("WfxTextureCatalog", 5, kTableFlags, ImVec2(0.f, 150.f)))
            return;

        ImGui::TableSetupColumn("Champion", ImGuiTableColumnFlags_WidthFixed, 92.f);
        ImGui::TableSetupColumn("Folder", ImGuiTableColumnFlags_WidthFixed, 92.f);
        ImGui::TableSetupColumn("Texture", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 52.f);
        ImGui::TableSetupColumn("Frame", ImGuiTableColumnFlags_WidthFixed, 54.f);
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < entries.size(); ++i)
        {
            const UI::WfxTextureEntry& entry = entries[i];
            const bool_t bSelected = static_cast<int>(i) == s_State.iSelectedTexture;

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::Selectable(entry.strChampion.c_str(),
                bSelected,
                ImGuiSelectableFlags_SpanAllColumns))
            {
                s_State.iSelectedTexture = static_cast<int>(i);
            }
            ImGui::PopID();

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(entry.strFolder.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(entry.strName.c_str());
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted(entry.strPathText.c_str());
                ImGui::EndTooltip();
            }
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(entry.strExtension.c_str());
            ImGui::TableSetColumnIndex(4);
            ImGui::TextUnformatted(entry.bLikelyAtlasFrame ? "Yes" : "-");
        }

        ImGui::EndTable();
    }

    void RenderReferenceHints()
    {
        if (!ImGui::TreeNode("Reference"))
            return;

        ImGui::BulletText("Q frames: Client/Bin/Resource/Texture/UI/Annie/Q/00.png - 02.png");
        ImGui::BulletText("W frames: Client/Bin/Resource/Texture/UI/Annie/W/00.png - 02.png");
        ImGui::BulletText("Tune in .wfx first; Niagara-style graph nodes can be layered on this document model later.");
        ImGui::TreePop();
    }

    void RenderDocumentBar(CScene_InGame* pScene)
    {
        ImGui::InputText("WFX Path", s_State.szPath, sizeof(s_State.szPath));

        if (ImGui::Button("Load"))
            LoadCurrentPath();
        ImGui::SameLine();
        if (ImGui::Button("Save"))
            SaveCurrentPath();
        ImGui::SameLine();
        if (ImGui::Button("Preview Edited"))
            PreviewEditedAsset(pScene);

        if (!s_State.strStatus.empty())
        {
            ImGui::TextColored(ImVec4(0.55f, 0.85f, 1.0f, 1.0f),
                "%s",
                s_State.strStatus.c_str());
        }
    }

    void RenderWfxAssetsTab()
    {
        ImGui::InputText("WFX Root", s_State.szRootPath, sizeof(s_State.szRootPath));
        ImGui::SameLine();
        if (ImGui::Button("Scan"))
            ScanCatalog(false);

        RenderAssetCatalog();
    }

    void RenderQuickQTab(CScene_InGame* pScene)
    {
        if (ImGui::Button("Select Irelia Q Texture"))
            SelectIreliaQLeadingEdgeTexture();

        const UI::WfxTextureEntry* pEntry =
            s_State.catalog.GetTextureEntry(static_cast<size_t>(s_State.iSelectedTexture));
        if (pEntry)
            ImGui::TextWrapped("%s", pEntry->strPathText.c_str());
        else
            ImGui::TextDisabled("No texture selected.");

        ImGui::Separator();

        ImGui::InputText("Cue Name", s_State.szQuickCueName, sizeof(s_State.szQuickCueName));
        ImGui::InputText("WFX Path", s_State.szQuickWfxPath, sizeof(s_State.szQuickWfxPath));

        if (ImGui::DragInt("Atlas Cols", &s_State.iQuickAtlasCols, 1.f, 1, 16))
            NormalizeQuickAtlasSettings();
        if (ImGui::DragInt("Atlas Rows", &s_State.iQuickAtlasRows, 1.f, 1, 16))
            NormalizeQuickAtlasSettings();
        if (ImGui::DragInt("Frame Count", &s_State.iQuickAtlasFrames, 1.f, 1, 256))
            NormalizeQuickAtlasSettings();

        if (ImGui::DragFloat("Duration", &s_State.fQuickDuration, 0.01f, 0.01f, 5.f, "%.3f"))
            NormalizeQuickAtlasSettings();

        NormalizeQuickAtlasSettings();
        const f32_t fAtlasFps =
            static_cast<f32_t>(s_State.iQuickAtlasFrames) / s_State.fQuickDuration;
        ImGui::TextDisabled("Atlas FPS %.2f", fAtlasFps);

        ImGui::DragFloat("Size", &s_State.fQuickSize, 0.02f, 0.01f, 20.f, "%.3f");
        ImGui::DragFloat3("Attach Offset", &s_State.vQuickAttachOffset.x, 0.02f, -10.f, 10.f, "%.3f");
        ImGui::ColorEdit4("Color", &s_State.vQuickColor.x);

        ImGui::Separator();

        if (ImGui::Button("Build Q Cue"))
            CreateQuickAtlasCueFromSelectedTexture();
        ImGui::SameLine();
        if (ImGui::Button("Build + Save Q Cue"))
        {
            if (CreateQuickAtlasCueFromSelectedTexture())
                SaveCurrentPath();
        }
        ImGui::SameLine();
        if (ImGui::Button("Preview Q Cue"))
        {
            if (!s_State.document.IsLoaded())
                CreateQuickAtlasCueFromSelectedTexture();
            PreviewEditedAsset(pScene);
        }
    }

    void RenderTexturesTab()
    {
        ImGui::InputText("Texture Root", s_State.szTextureRoot, sizeof(s_State.szTextureRoot));
        ImGui::SameLine();
        if (ImGui::Button("Scan Textures"))
            ScanTextureCatalog();

        RenderTextureCatalog();

        ImGui::InputText("New Cue Name", s_State.szNewCueName, sizeof(s_State.szNewCueName));
        ImGui::InputText("New WFX Path", s_State.szNewWfxPath, sizeof(s_State.szNewWfxPath));

        if (ImGui::Button("New WFX From Texture"))
            CreateDocumentFromSelectedTexture();
        ImGui::SameLine();
        if (ImGui::Button("Add Emitter"))
            AppendEmitterFromSelectedTexture();
        ImGui::SameLine();
        if (ImGui::Button("Apply Texture"))
            ApplySelectedTextureToSelectedEmitter();
    }

    void RenderPreviewTab(CScene_InGame* pScene)
    {
        bool bAttachPreviewToPlayer = s_State.bAttachPreviewToPlayer;
        if (ImGui::Checkbox("Attach Preview To Player", &bAttachPreviewToPlayer))
            s_State.bAttachPreviewToPlayer = bAttachPreviewToPlayer;

        bool bPreviewUseMouseDirection = s_State.bPreviewUseMouseDirection;
        if (ImGui::Checkbox("Use Mouse Direction", &bPreviewUseMouseDirection))
            s_State.bPreviewUseMouseDirection = bPreviewUseMouseDirection;

        ImGui::DragFloat("Preview Distance",
            &s_State.fPreviewDistance,
            0.05f,
            0.f,
            12.f,
            "%.2f");

        ImGui::DragFloat("Segment Length",
            &s_State.fPreviewSegmentLength,
            0.05f,
            0.1f,
            20.f,
            "%.2f");

        ImGui::DragFloat("Preview Anchor Y",
            &s_State.fPreviewAnchorYOffset,
            0.05f,
            -5.f,
            8.f,
            "%.2f");

        if (ImGui::Button("Preview Edited"))
            PreviewEditedAsset(pScene);

        RenderReferenceHints();
    }
}

void UI::CWfxEffectToolPanel::Render(CScene_InGame* pScene)
{
    if (!pScene)
        return;

    if (!s_State.bInitialized)
    {
        ScanTextureCatalog();
        if (!ScanCatalog(true))
            LoadCurrentPath();
        s_State.bInitialized = true;
    }

    ImGui::SetNextWindowPos(ImVec2(24.f, 260.f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(720.f, 720.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
    if (!ImGui::Begin("WFX Effect Tool"))
    {
        ImGui::End();
        return;
    }

    RenderDocumentBar(pScene);
    ImGui::Separator();

    if (ImGui::BeginTabBar("WfxEffectToolTabs"))
    {
        if (ImGui::BeginTabItem("WFX Assets"))
        {
            RenderWfxAssetsTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Textures"))
        {
            RenderTexturesTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Inspector"))
        {
            RenderEmitterEditor();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Preview"))
        {
            RenderPreviewTab(pScene);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}
