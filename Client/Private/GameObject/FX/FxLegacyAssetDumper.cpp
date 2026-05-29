#include "GameObject/FX/FxLegacyAssetDumper.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace
{
    std::string NarrowAscii(const wstring_t& text)
    {
        std::string result;
        result.reserve(text.size());
        for (const tchar_t ch : text)
            result.push_back(static_cast<char>(ch < 128 ? ch : '?'));
        return result;
    }

    std::string NormalizePath(std::string value)
    {
        std::replace(value.begin(), value.end(), '\\', '/');
        return value;
    }

    std::string NormalizePath(const tchar_t* path)
    {
        if (!path)
            return {};
        return NormalizePath(NarrowAscii(wstring_t(path)));
    }

    std::string EscapeJson(std::string value)
    {
        std::string result;
        result.reserve(value.size());
        for (const char ch : value)
        {
            switch (ch)
            {
            case '\\': result += "\\\\"; break;
            case '"':  result += "\\\""; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:   result.push_back(ch); break;
            }
        }
        return result;
    }

    const char* RenderTypeToString(eFxRenderType eType)
    {
        switch (eType)
        {
        case eFxRenderType::Ribbon: return "Ribbon";
        case eFxRenderType::Beam: return "Beam";
        case eFxRenderType::GroundDecal: return "GroundDecal";
        case eFxRenderType::MeshParticle: return "MeshParticle";
        case eFxRenderType::ShockwaveRing: return "ShockwaveRing";
        case eFxRenderType::Billboard:
        default:
            return "Billboard";
        }
    }

    const char* BlendPresetToString(eBlendPreset ePreset)
    {
        switch (ePreset)
        {
        case eBlendPreset::Opaque: return "Opaque";
        case eBlendPreset::PremultipliedAlpha: return "PremultipliedAlpha";
        case eBlendPreset::Additive: return "Additive";
        case eBlendPreset::AlphaBlend:
        default:
            return "AlphaBlend";
        }
    }

    const char* DepthModeToString(eFxDepthMode eMode)
    {
        switch (eMode)
        {
        case eFxDepthMode::DepthTestWriteOff:
            return "DepthTestWriteOff";
        case eFxDepthMode::OverlayNoDepth:
            return "OverlayNoDepth";
        case eFxDepthMode::SoftParticle:
            return "SoftParticle";
        case eFxDepthMode::DepthTestWriteOn:
        default:
            return "DepthTestWriteOn";
        }
    }

    bool EnsureParentDirectory(const wstring_t& strPath)
    {
        const std::filesystem::path path(strPath);
        const std::filesystem::path parent = path.parent_path();
        if (parent.empty())
            return true;

        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        return !ec;
    }

    void WriteVec3(std::ostream& os, const Vec3& v)
    {
        os << "[" << v.x << ", " << v.y << ", " << v.z << "]";
    }

    void WriteVec4(std::ostream& os, const Vec4& v)
    {
        os << "[" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << "]";
    }
}

bool LegacyFx::SaveManifest(const wstring_t& strPath,
    const std::vector<FxLegacyManifestEntry>& entries)
{
    if (!EnsureParentDirectory(strPath))
        return false;

    std::ofstream file(std::filesystem::path(strPath), std::ios::binary);
    if (!file)
        return false;

    file << "{\n";
    file << "  \"version\": 1,\n";
    file << "  \"generatedBy\": \"Winters EFX-0 Legacy Bridge\",\n";
    file << "  \"entries\": [\n";

    for (u32_t i = 0; i < static_cast<u32_t>(entries.size()); ++i)
    {
        const FxLegacyManifestEntry& e = entries[i];
        file << "    {\n";
        file << "      \"id\": \"" << EscapeJson(e.pszId ? e.pszId : "") << "\",\n";
        file << "      \"champion\": \"" << EscapeJson(e.pszChampion ? e.pszChampion : "") << "\",\n";
        file << "      \"sourceFunction\": \"" << EscapeJson(e.pszSourceFunction ? e.pszSourceFunction : "") << "\",\n";
        file << "      \"sourceFile\": \"" << EscapeJson(e.pszSourceFile ? e.pszSourceFile : "") << "\",\n";
        file << "      \"renderTypes\": \"" << EscapeJson(e.pszRenderTypes ? e.pszRenderTypes : "") << "\",\n";
        file << "      \"assetPath\": \"" << EscapeJson(NormalizePath(e.pszAssetPath)) << "\",\n";
        file << "      \"materialInstancePath\": \"" << EscapeJson(NormalizePath(e.pszMaterialInstancePath)) << "\",\n";
        file << "      \"migrationState\": \"" << EscapeJson(e.pszMigrationState ? e.pszMigrationState : "") << "\"\n";
        file << "    }" << (i + 1 < entries.size() ? "," : "") << "\n";
    }

    file << "  ]\n";
    file << "}\n";
    return true;
}

bool LegacyFx::SaveSeedManifest(const wstring_t& strPath)
{
    return SaveManifest(strPath, GetSeedManifestEntries());
}

bool LegacyFx::SaveAssetAsWfx(const wstring_t& strPath, const FxAsset& asset)
{
    if (!EnsureParentDirectory(strPath))
        return false;

    std::ofstream file(std::filesystem::path(strPath), std::ios::binary);
    if (!file)
        return false;

    file << std::setprecision(6);
    file << "{\n";
    file << "  \"schema\": \"WintersFxLegacyAsset\",\n";
    file << "  \"version\": 1,\n";
    file << "  \"name\": \"" << EscapeJson(asset.strName) << "\",\n";
    file << "  \"emitters\": [\n";

    for (u32_t i = 0; i < static_cast<u32_t>(asset.emitters.size()); ++i)
    {
        const FxEmitterDesc& e = asset.emitters[i];
        file << "    {\n";
        file << "      \"name\": \"" << EscapeJson(e.strName) << "\",\n";
        file << "      \"render_type\": \"" << RenderTypeToString(e.renderType) << "\",\n";
        file << "      \"material\": \"" << EscapeJson(NormalizePath(NarrowAscii(e.strTexturePath))) << "\",\n";
        file << "      \"erode_material\": \"" << EscapeJson(NormalizePath(NarrowAscii(e.strErodeTexturePath))) << "\",\n";
        file << "      \"model\": \"" << EscapeJson(NormalizePath(e.strModelPath)) << "\",\n";
        file << "      \"blend_mode\": \"" << BlendPresetToString(e.blendMode) << "\",\n";
        file << "      \"max_particles\": " << e.maxParticles << ",\n";
        file << "      \"spawn_rate\": " << e.spawnRate << ",\n";
        file << "      \"lifetime\": " << e.fLifetime << ",\n";
        file << "      \"width\": " << e.fWidth << ",\n";
        file << "      \"height\": " << e.fHeight << ",\n";
        file << "      \"yaw\": " << e.fYaw << ",\n";
        file << "      \"color\": ";
        WriteVec4(file, e.vColor);
        file << ",\n";
        file << "      \"attach_offset\": ";
        WriteVec3(file, e.vAttachOffset);
        file << ",\n";
        file << "      \"end_offset\": ";
        WriteVec3(file, e.vEndOffset);
        file << ",\n";
        file << "      \"velocity\": ";
        WriteVec3(file, e.vVelocity);
        file << ",\n";
        file << "      \"scale\": ";
        WriteVec3(file, e.vScale);
        file << ",\n";
        file << "      \"rotation\": ";
        WriteVec3(file, e.vRotation);
        file << ",\n";
        file << "      \"fade_in\": " << e.fFadeIn << ",\n";
        file << "      \"fade_out\": " << e.fFadeOut << ",\n";
        file << "      \"uv_scroll\": [" << e.fUvScrollU << ", " << e.fUvScrollV << "],\n";
        file << "      \"alpha_clip\": " << e.fAlphaClip << ",\n";
        file << "      \"erode_threshold\": " << e.fErodeThreshold << ",\n";
        file << "      \"style_mode\": " << e.iStyleMode << ",\n";
        file << "      \"style_color_a\": ";
        WriteVec4(file, e.vStyleColorA);
        file << ",\n";
        file << "      \"style_color_b\": ";
        WriteVec4(file, e.vStyleColorB);
        file << ",\n";
        file << "      \"rim_color\": ";
        WriteVec4(file, e.vRimColor);
        file << ",\n";
        file << "      \"rim_power\": " << e.fRimPower << ",\n";
        file << "      \"cell_low\": " << e.fCellLow << ",\n";
        file << "      \"cell_high\": " << e.fCellHigh << ",\n";
        file << "      \"magic_scroll_a\": ";
        WriteVec4(file, e.vMagicScrollA);
        file << ",\n";
        file << "      \"magic_shape\": ";
        WriteVec4(file, e.vMagicShape);
        file << ",\n";
        file << "      \"magic_core\": ";
        WriteVec4(file, e.vMagicCore);
        file << ",\n";
        file << "      \"material_random\": " << e.fMaterialRandom << ",\n";
        file << "      \"depth_mode\": \"" << DepthModeToString(e.depthMode) << "\",\n";
        file << "      \"depth_write\": " << (e.bDepthWrite ? "true" : "false") << ",\n";
        file << "      \"blockable_by_wind_wall\": " << (e.bBlockableByWindWall ? "true" : "false") << "\n";
        file << "    }" << (i + 1 < asset.emitters.size() ? "," : "") << "\n";
    }

    file << "  ]\n";
    file << "}\n";
    return true;
}
