#include "FX/FxAsset.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

namespace
{
    std::string ReadTextFile(const wstring_t& path)
    {
        std::ifstream file(std::filesystem::path(path), std::ios::binary);
        if (!file)
            return {};

        std::ostringstream oss;
        oss << file.rdbuf();
        return oss.str();
    }

    std::string NarrowPathStem(const wstring_t& path)
    {
        return std::filesystem::path(path).stem().string();
    }

    wstring_t WidenAscii(std::string_view value)
    {
        return wstring_t(value.begin(), value.end());
    }

    std::string Trim(std::string value)
    {
        const auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };
        value.erase(value.begin(), std::find_if(value.begin(), value.end(),
            [&](char ch) { return !isSpace(static_cast<unsigned char>(ch)); }));
        value.erase(std::find_if(value.rbegin(), value.rend(),
            [&](char ch) { return !isSpace(static_cast<unsigned char>(ch)); }).base(), value.end());
        return value;
    }

    std::string NormalizeToken(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        value.erase(std::remove(value.begin(), value.end(), '_'), value.end());
        value.erase(std::remove(value.begin(), value.end(), '-'), value.end());
        return value;
    }

    bool_t ExtractString(const std::string& json,
        std::string_view key,
        std::string& outValue)
    {
        const std::string quotedKey = "\"" + std::string(key) + "\"";
        const size_t keyPos = json.find(quotedKey);
        if (keyPos == std::string::npos)
            return false;

        const size_t colonPos = json.find(':', keyPos + quotedKey.size());
        if (colonPos == std::string::npos)
            return false;

        const size_t quoteBegin = json.find('"', colonPos + 1);
        if (quoteBegin == std::string::npos)
            return false;

        const size_t quoteEnd = json.find('"', quoteBegin + 1);
        if (quoteEnd == std::string::npos)
            return false;

        outValue = json.substr(quoteBegin + 1, quoteEnd - quoteBegin - 1);
        return true;
    }

    bool_t ExtractNumber(const std::string& json,
        std::string_view key,
        f32_t& outValue)
    {
        const std::string quotedKey = "\"" + std::string(key) + "\"";
        const size_t keyPos = json.find(quotedKey);
        if (keyPos == std::string::npos)
            return false;

        const size_t colonPos = json.find(':', keyPos + quotedKey.size());
        if (colonPos == std::string::npos)
            return false;

        size_t endPos = json.find_first_of(",}\r\n", colonPos + 1);
        if (endPos == std::string::npos)
            endPos = json.size();

        const std::string token = Trim(json.substr(colonPos + 1, endPos - colonPos - 1));
        try
        {
            outValue = std::stof(token);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool_t ExtractUInt(const std::string& json,
        std::string_view key,
        u32_t& outValue)
    {
        f32_t value = 0.f;
        if (!ExtractNumber(json, key, value))
            return false;
        outValue = static_cast<u32_t>(value);
        return true;
    }

    bool_t ExtractBool(const std::string& json,
        std::string_view key,
        bool_t& outValue)
    {
        const std::string quotedKey = "\"" + std::string(key) + "\"";
        const size_t keyPos = json.find(quotedKey);
        if (keyPos == std::string::npos)
            return false;

        const size_t colonPos = json.find(':', keyPos + quotedKey.size());
        if (colonPos == std::string::npos)
            return false;

        size_t endPos = json.find_first_of(",}\r\n", colonPos + 1);
        if (endPos == std::string::npos)
            endPos = json.size();

        std::string token = Trim(json.substr(colonPos + 1, endPos - colonPos - 1));
        std::transform(token.begin(), token.end(), token.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

        if (token == "true" || token == "1")
        {
            outValue = true;
            return true;
        }

        if (token == "false" || token == "0")
        {
            outValue = false;
            return true;
        }

        return false;
    }

    bool_t ExtractVec2(const std::string& json,
        std::string_view key,
        Vec2& outValue)
    {
        const std::string quotedKey = "\"" + std::string(key) + "\"";
        const size_t keyPos = json.find(quotedKey);
        if (keyPos == std::string::npos)
            return false;

        const size_t openPos = json.find('[', keyPos + quotedKey.size());
        const size_t closePos = json.find(']', openPos + 1);
        if (openPos == std::string::npos || closePos == std::string::npos)
            return false;

        std::string values = json.substr(openPos + 1, closePos - openPos - 1);
        std::replace(values.begin(), values.end(), ',', ' ');

        std::istringstream iss(values);
        iss >> outValue.x >> outValue.y;
        return !iss.fail();
    }

    bool_t ExtractVec3(const std::string& json,
        std::string_view key,
        Vec3& outValue)
    {
        const std::string quotedKey = "\"" + std::string(key) + "\"";
        const size_t keyPos = json.find(quotedKey);
        if (keyPos == std::string::npos)
            return false;

        const size_t openPos = json.find('[', keyPos + quotedKey.size());
        const size_t closePos = json.find(']', openPos + 1);
        if (openPos == std::string::npos || closePos == std::string::npos)
            return false;

        std::string values = json.substr(openPos + 1, closePos - openPos - 1);
        std::replace(values.begin(), values.end(), ',', ' ');

        std::istringstream iss(values);
        iss >> outValue.x >> outValue.y >> outValue.z;
        return !iss.fail();
    }

    bool_t ExtractVec4(const std::string& json,
        std::string_view key,
        Vec4& outValue)
    {
        const std::string quotedKey = "\"" + std::string(key) + "\"";
        const size_t keyPos = json.find(quotedKey);
        if (keyPos == std::string::npos)
            return false;

        const size_t openPos = json.find('[', keyPos + quotedKey.size());
        const size_t closePos = json.find(']', openPos + 1);
        if (openPos == std::string::npos || closePos == std::string::npos)
            return false;

        std::string values = json.substr(openPos + 1, closePos - openPos - 1);
        std::replace(values.begin(), values.end(), ',', ' ');

        std::istringstream iss(values);
        iss >> outValue.x >> outValue.y >> outValue.z >> outValue.w;
        return !iss.fail();
    }

    bool_t ExtractObjectBlock(const std::string& json,
        std::string_view key,
        std::string& outBlock)
    {
        const std::string quotedKey = "\"" + std::string(key) + "\"";
        const size_t keyPos = json.find(quotedKey);
        if (keyPos == std::string::npos)
            return false;

        const size_t openPos = json.find('{', keyPos + quotedKey.size());
        if (openPos == std::string::npos)
            return false;

        i32_t depth = 0;
        bool_t bInString = false;
        bool_t bEscaped = false;
        for (size_t i = openPos; i < json.size(); ++i)
        {
            const char ch = json[i];
            if (bInString)
            {
                if (bEscaped)
                {
                    bEscaped = false;
                    continue;
                }
                if (ch == '\\')
                {
                    bEscaped = true;
                    continue;
                }
                if (ch == '"')
                    bInString = false;
                continue;
            }

            if (ch == '"')
            {
                bInString = true;
                continue;
            }
            if (ch == '{')
            {
                ++depth;
            }
            else if (ch == '}')
            {
                --depth;
                if (depth == 0)
                {
                    outBlock = json.substr(openPos, i - openPos + 1);
                    return true;
                }
            }
        }

        return false;
    }

    std::vector<std::string> ExtractEmitterBlocks(const std::string& json)
    {
        std::vector<std::string> blocks;
        const size_t emittersPos = json.find("\"emitters\"");
        if (emittersPos == std::string::npos)
            return blocks;

        const size_t arrayBegin = json.find('[', emittersPos);
        if (arrayBegin == std::string::npos)
            return blocks;

        i32_t depth = 0;
        size_t blockBegin = std::string::npos;
        for (size_t i = arrayBegin + 1; i < json.size(); ++i)
        {
            if (json[i] == '{')
            {
                if (depth == 0)
                    blockBegin = i;
                ++depth;
            }
            else if (json[i] == '}')
            {
                --depth;
                if (depth == 0 && blockBegin != std::string::npos)
                {
                    blocks.push_back(json.substr(blockBegin, i - blockBegin + 1));
                    blockBegin = std::string::npos;
                }
            }
            else if (json[i] == ']' && depth == 0)
            {
                break;
            }
        }

        return blocks;
    }

    eFxRenderType ParseRenderType(const std::string& value)
    {
        if (value == "Ribbon") return eFxRenderType::Ribbon;
        if (value == "Beam") return eFxRenderType::Beam;
        if (value == "GroundDecal") return eFxRenderType::GroundDecal;
        if (value == "MeshParticle") return eFxRenderType::MeshParticle;
        if (value == "ShockwaveRing") return eFxRenderType::ShockwaveRing;
        return eFxRenderType::Billboard;
    }

    eBlendPreset ParseBlendPreset(const std::string& value)
    {
        if (value == "Opaque") return eBlendPreset::Opaque;
        if (value == "PremultipliedAlpha") return eBlendPreset::PremultipliedAlpha;
        if (value == "Additive") return eBlendPreset::Additive;
        return eBlendPreset::AlphaBlend;
    }

    eFxDepthMode ParseDepthMode(const std::string& value)
    {
        const std::string token = NormalizeToken(value);
        if (token == "depthtestwriteoff" ||
            token == "writeoff")
        {
            return eFxDepthMode::DepthTestWriteOff;
        }

        if (token == "overlaynodepth" ||
            token == "nodepth")
        {
            return eFxDepthMode::OverlayNoDepth;
        }

        if (token == "softparticle" ||
            token == "softparticles")
        {
            return eFxDepthMode::SoftParticle;
        }

        return eFxDepthMode::DepthTestWriteOn;
    }

    eFxAnchorType ParseAnchorType(const std::string& value)
    {
        const std::string token = NormalizeToken(value);
        if (token == "world") return eFxAnchorType::World;
        if (token == "bone") return eFxAnchorType::Bone;
        if (token == "socket") return eFxAnchorType::Socket;
        if (token == "submesh") return eFxAnchorType::Submesh;
        if (token == "targetsegment") return eFxAnchorType::TargetSegment;
        return eFxAnchorType::Entity;
    }

    eFxAnchorFallback ParseAnchorFallback(const std::string& value)
    {
        const std::string token = NormalizeToken(value);
        if (token == "none") return eFxAnchorFallback::None;
        if (token == "world" || token == "worldposition") return eFxAnchorFallback::WorldPosition;
        return eFxAnchorFallback::Entity;
    }

    eFxLifecycleMode ParseLifecycleMode(const std::string& value)
    {
        const std::string token = NormalizeToken(value);
        if (token == "burst") return eFxLifecycleMode::Burst;
        if (token == "whilestate") return eFxLifecycleMode::WhileState;
        if (token == "manualstop") return eFxLifecycleMode::ManualStop;
        if (token == "loopuntilsignal") return eFxLifecycleMode::LoopUntilSignal;
        return eFxLifecycleMode::Timed;
    }

    FxAsset ParseWfxJson(const wstring_t& path,
        const std::string& json,
        bool_t bAddFallbackEmitter)
    {
        FxAsset asset{};
        if (!ExtractString(json, "name", asset.strName) || asset.strName.empty())
            asset.strName = NarrowPathStem(path);

        Vec4 tint{};
        if (ExtractVec4(json, "TintColor", tint))
            asset.initialUserParams.Set(eFxNamespace::User, "TintColor", tint);

        const std::vector<std::string> emitterBlocks = ExtractEmitterBlocks(json);
        for (const std::string& block : emitterBlocks)
        {
            FxEmitterDesc emitter{};

            std::string value;
            if (ExtractString(block, "name", value))
                emitter.strName = value;
            if (ExtractString(block, "render_type", value) ||
                ExtractString(block, "renderType", value))
            {
                emitter.renderType = ParseRenderType(value);
            }
            if (ExtractString(block, "texture", value) ||
                ExtractString(block, "material", value))
            {
                emitter.strTexturePath = WidenAscii(value);
            }
            if (ExtractString(block, "erode_material", value) ||
                ExtractString(block, "erode_texture", value))
            {
                emitter.strErodeTexturePath = WidenAscii(value);
            }
            if (ExtractString(block, "model", value))
                emitter.strModelPath = value;
            if (ExtractString(block, "blend_mode", value) ||
                ExtractString(block, "blend", value))
                emitter.blendMode = ParseBlendPreset(value);
            // Optional depth mode overrides legacy depth_write.
            bool_t bHasExplicitDepthMode = false;
            eFxDepthMode explicitDepthMode = eFxDepthMode::DepthTestWriteOn;
            if (ExtractString(block, "depth_mode", value))
            {
                explicitDepthMode = ParseDepthMode(value);
                bHasExplicitDepthMode = true;
            }

            ExtractUInt(block, "max_particles", emitter.maxParticles);
            ExtractNumber(block, "spawn_rate", emitter.spawnRate);
            ExtractNumber(block, "lifetime", emitter.fLifetime);
            ExtractNumber(block, "width", emitter.fWidth);
            ExtractNumber(block, "height", emitter.fHeight);
            ExtractNumber(block, "yaw", emitter.fYaw);
            ExtractNumber(block, "fade_in", emitter.fFadeIn);
            ExtractNumber(block, "start_delay", emitter.fStartDelay);
            if (emitter.fStartDelay == 0.f)
                ExtractNumber(block, "startDelay", emitter.fStartDelay);
            ExtractNumber(block, "fade_out", emitter.fFadeOut);
            ExtractNumber(block, "start_radius", emitter.fStartRadius);
            ExtractNumber(block, "end_radius", emitter.fEndRadius);
            ExtractNumber(block, "thickness", emitter.fThickness);
            ExtractNumber(block, "grow_duration", emitter.fGrowDuration);
            ExtractNumber(block, "alpha_clip", emitter.fAlphaClip);
            ExtractNumber(block, "erode_threshold", emitter.fErodeThreshold);
            ExtractUInt(block, "style_mode", emitter.iStyleMode);
            ExtractNumber(block, "rim_power", emitter.fRimPower);
            ExtractNumber(block, "cell_low", emitter.fCellLow);
            ExtractNumber(block, "cell_high", emitter.fCellHigh);
            ExtractNumber(block, "material_random", emitter.fMaterialRandom);
            if (!ExtractNumber(block, "segment_t", emitter.fSegmentT))
                ExtractNumber(block, "segmentT", emitter.fSegmentT);
            if (!ExtractBool(block, "scale_z_to_segment", emitter.bScaleZToSegment))
                ExtractBool(block, "scaleZToSegment", emitter.bScaleZToSegment);
            ExtractBool(block, "depth_write", emitter.bDepthWrite);
            ExtractBool(block, "billboard", emitter.bBillboard);
            if (!ExtractBool(block, "blockable_by_wind_wall", emitter.bBlockableByWindWall))
                ExtractBool(block, "blockableByWindWall", emitter.bBlockableByWindWall);
            ExtractUInt(block, "atlas_cols", emitter.iAtlasCols);
            if (!ExtractUInt(block, "atlas_rows", emitter.iAtlasRows))
                ExtractUInt(block, "atlas_row", emitter.iAtlasRows);
            ExtractUInt(block, "atlas_frame_count", emitter.iAtlasFrameCount);
            ExtractNumber(block, "atlas_fps", emitter.fAtlasFps);
            ExtractBool(block, "atlas_loop", emitter.bAtlasLoop);
            ExtractUInt(block, "ribbon_point_count", emitter.iRibbonPointCount);
            if (!ExtractBool(block, "history_trail", emitter.bHistoryTrail))
                ExtractBool(block, "historyTrail", emitter.bHistoryTrail);
            if (!ExtractNumber(block, "trail_sample_interval", emitter.fTrailSampleInterval))
                ExtractNumber(block, "trailSampleInterval", emitter.fTrailSampleInterval);
            if (!ExtractNumber(block, "trail_head_width_scale", emitter.fTrailHeadWidthScale))
                ExtractNumber(block, "trailHeadWidthScale", emitter.fTrailHeadWidthScale);
            if (!ExtractNumber(block, "trail_tail_width_scale", emitter.fTrailTailWidthScale))
                ExtractNumber(block, "trailTailWidthScale", emitter.fTrailTailWidthScale);
            if (!ExtractNumber(block, "trail_head_alpha_scale", emitter.fTrailHeadAlphaScale))
                ExtractNumber(block, "trailHeadAlphaScale", emitter.fTrailHeadAlphaScale);
            if (!ExtractNumber(block, "trail_tail_alpha_scale", emitter.fTrailTailAlphaScale))
                ExtractNumber(block, "trailTailAlphaScale", emitter.fTrailTailAlphaScale);
            if (!ExtractNumber(block, "trail_jitter_amplitude", emitter.fTrailJitterAmplitude))
                ExtractNumber(block, "trailJitterAmplitude", emitter.fTrailJitterAmplitude);
            if (!ExtractNumber(block, "trail_jitter_frequency", emitter.fTrailJitterFrequency))
                ExtractNumber(block, "trailJitterFrequency", emitter.fTrailJitterFrequency);
            if (!ExtractNumber(block, "trail_jitter_seed", emitter.fTrailJitterSeed))
                ExtractNumber(block, "trailJitterSeed", emitter.fTrailJitterSeed);

            Vec2 uvScroll{};
            if (ExtractVec2(block, "uv_scroll", uvScroll))
            {
                emitter.fUvScrollU = uvScroll.x;
                emitter.fUvScrollV = uvScroll.y;
            }

            ExtractVec3(block, "attach_offset", emitter.vAttachOffset);
            emitter.anchor.vAnchorOffset = emitter.vAttachOffset;
            std::string objectBlock;
            if (ExtractObjectBlock(block, "anchor", objectBlock))
            {
                if (ExtractString(objectBlock, "type", value))
                    emitter.anchor.eAnchorType = ParseAnchorType(value);
                if (ExtractString(objectBlock, "name", value))
                    emitter.anchor.strAnchorName = value;
                if (!ExtractVec3(objectBlock, "offset", emitter.anchor.vAnchorOffset))
                    ExtractVec3(objectBlock, "attach_offset", emitter.anchor.vAnchorOffset);
                if (ExtractString(objectBlock, "fallback", value))
                    emitter.anchor.eFallback = ParseAnchorFallback(value);
                ExtractBool(objectBlock, "inherit_rotation", emitter.anchor.bInheritRotation);
            }
            if (ExtractObjectBlock(block, "lifecycle", objectBlock))
            {
                if (ExtractString(objectBlock, "mode", value))
                    emitter.lifecycle.eLifecycleMode = ParseLifecycleMode(value);
                ExtractNumber(objectBlock, "stop_fade_out", emitter.lifecycle.fStopFadeOut);
                ExtractBool(objectBlock, "detach_on_stop", emitter.lifecycle.bDetachOnStop);
                ExtractBool(objectBlock, "kill_when_anchor_invalid", emitter.lifecycle.bKillWhenAnchorInvalid);
            }
            ExtractVec3(block, "end_offset", emitter.vEndOffset);
            ExtractVec3(block, "velocity", emitter.vVelocity);
            ExtractVec3(block, "scale", emitter.vScale);
            ExtractVec3(block, "rotation", emitter.vRotation);
            if (!ExtractNumber(block, "world_yaw_spin_speed", emitter.fWorldYawSpinSpeed))
                ExtractNumber(block, "worldYawSpinSpeed", emitter.fWorldYawSpinSpeed);

            if (ExtractVec4(block, "color", tint))
                emitter.vColor = tint;
            if (ExtractVec4(block, "style_color_a", tint))
                emitter.vStyleColorA = tint;
            if (ExtractVec4(block, "style_color_b", tint))
                emitter.vStyleColorB = tint;
            if (ExtractVec4(block, "rim_color", tint))
                emitter.vRimColor = tint;
            if (ExtractVec4(block, "magic_scroll_a", tint))
                emitter.vMagicScrollA = tint;
            if (ExtractVec4(block, "magic_shape", tint))
                emitter.vMagicShape = tint;
            if (ExtractVec4(block, "magic_core", tint))
                emitter.vMagicCore = tint;

            FxEmitterSetMaterialFromLegacyFields(emitter);
            if (bHasExplicitDepthMode)
            {
                emitter.depthMode = explicitDepthMode;
                emitter.bDepthWrite = FxDepthModeWritesDepth(emitter.depthMode);
            }

            asset.emitters.push_back(std::move(emitter));
        }

        if (asset.emitters.empty() && bAddFallbackEmitter)
        {
            FxEmitterDesc emitter{};
            emitter.strName = asset.strName + "_Emitter";
            asset.emitters.push_back(std::move(emitter));
        }

        return asset;
    }
}

FxAssetLoadResult LoadFxAssetFromFile(const wstring_t& path)
{
    FxAssetLoadResult result{};
    const std::string json = ReadTextFile(path);
    if (json.empty())
    {
        result.strError = "empty_or_missing_wfx";
        return result;
    }

    result.asset = ParseWfxJson(path, json, false);
    if (result.asset.emitters.empty())
    {
        result.strError = "missing_emitters";
        return result;
    }

    result.bSucceeded = true;
    return result;
}

CFxAssetRegistry::CFxAssetRegistry()
{
    m_Slots.push_back(Slot{});
}

FxAssetHandle CFxAssetRegistry::Register(FxAsset asset)
{
    const u32_t index = static_cast<u32_t>(m_Slots.size());
    Slot slot{};
    slot.generation = 1;
    slot.bAlive = true;
    slot.asset = std::move(asset);
    slot.asset.handle = FxAssetHandle::Make(index, slot.generation);

    const FxAssetHandle handle = slot.asset.handle;
    if (!slot.asset.strName.empty())
        m_NameToHandle[slot.asset.strName] = handle;

    m_Slots.push_back(std::move(slot));
    return handle;
}

FxAssetHandle CFxAssetRegistry::RegisterOrReplaceByName(FxAsset asset)
{
    const auto it = m_NameToHandle.find(asset.strName);
    if (it == m_NameToHandle.end())
        return Register(std::move(asset));

    Slot* pSlot = ResolveSlot(it->second);
    if (!pSlot)
        return Register(std::move(asset));

    ++pSlot->generation;
    if (pSlot->generation == 0)
        pSlot->generation = 1;

    pSlot->asset = std::move(asset);
    pSlot->asset.handle = FxAssetHandle::Make(it->second.Index(), pSlot->generation);
    pSlot->bAlive = true;
    m_NameToHandle[pSlot->asset.strName] = pSlot->asset.handle;
    return pSlot->asset.handle;
}

const FxAsset* CFxAssetRegistry::Find(FxAssetHandle handle) const
{
    const Slot* pSlot = ResolveSlot(handle);
    return pSlot ? &pSlot->asset : nullptr;
}

FxAsset* CFxAssetRegistry::FindMutable(FxAssetHandle handle)
{
    Slot* pSlot = ResolveSlot(handle);
    return pSlot ? &pSlot->asset : nullptr;
}

FxAssetHandle CFxAssetRegistry::FindByName(const std::string& name) const
{
    const auto it = m_NameToHandle.find(name);
    if (it == m_NameToHandle.end())
        return {};
    return it->second;
}

void CFxAssetRegistry::UnregisterAll()
{
    m_Slots.clear();
    m_NameToHandle.clear();
    m_Slots.push_back(Slot{});
}

u32_t CFxAssetRegistry::GetAssetCount() const
{
    return static_cast<u32_t>(m_NameToHandle.size());
}

FxAssetHandle CFxAssetRegistry::LoadFromFile(const wstring_t& path)
{
    FxAssetLoadResult result = LoadFxAssetFromFile(path);
    if (!result.bSucceeded)
        return {};

    FxAssetHandle handle = RegisterOrReplaceByName(std::move(result.asset));
    if (Slot* pSlot = ResolveSlot(handle))
        pSlot->path = path;
    return handle;
}

bool_t CFxAssetRegistry::ReloadFromFile(FxAssetHandle handle)
{
    Slot* pSlot = ResolveSlot(handle);
    if (!pSlot || pSlot->path.empty())
        return false;

    FxAssetLoadResult result = LoadFxAssetFromFile(pSlot->path);
    if (!result.bSucceeded)
        return false;

    result.asset.handle = handle;
    pSlot->asset = std::move(result.asset);
    pSlot->asset.handle = handle;
    if (!pSlot->asset.strName.empty())
        m_NameToHandle[pSlot->asset.strName] = handle;
    return true;
}

u32_t CFxAssetRegistry::LoadDirectory(const wstring_t& directoryPath)
{
    namespace fs = std::filesystem;

    u32_t count = 0;
    const fs::path root(directoryPath);
    if (!fs::exists(root))
        return count;

    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(root))
    {
        if (!entry.is_regular_file())
            continue;
        if (entry.path().extension() != L".wfx")
            continue;

        if (LoadFromFile(entry.path().wstring()).IsValid())
            ++count;
    }

    return count;
}

const wstring_t* CFxAssetRegistry::GetAssetPath(FxAssetHandle handle) const
{
    const Slot* pSlot = ResolveSlot(handle);
    return pSlot ? &pSlot->path : nullptr;
}

CFxAssetRegistry::Slot* CFxAssetRegistry::ResolveSlot(FxAssetHandle handle)
{
    if (!handle.IsValid())
        return nullptr;

    const u32_t index = handle.Index();
    if (index >= m_Slots.size())
        return nullptr;

    Slot& slot = m_Slots[index];
    if (!slot.bAlive || slot.generation != handle.Generation())
        return nullptr;

    return &slot;
}

const CFxAssetRegistry::Slot* CFxAssetRegistry::ResolveSlot(FxAssetHandle handle) const
{
    if (!handle.IsValid())
        return nullptr;

    const u32_t index = handle.Index();
    if (index >= m_Slots.size())
        return nullptr;

    const Slot& slot = m_Slots[index];
    if (!slot.bAlive || slot.generation != handle.Generation())
        return nullptr;

    return &slot;
}
