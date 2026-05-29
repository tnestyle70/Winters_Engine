#include "GameObject/FX/WfxDocument.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <system_error>
#include <utility>

namespace
{
    std::string NarrowAscii(const wstring_t& value)
    {
        std::string out;
        out.reserve(value.size());
        for (const auto ch : value)
            out.push_back(ch <= 0x7f ? static_cast<char>(ch) : '?');
        return out;
    }

    std::string NormalizePath(std::string value)
    {
        for (char& ch : value)
        {
            if (ch == '\\')
                ch = '/';
        }
        return value;
    }

    std::string EscapeJson(const std::string& value)
    {
        std::string out;
        out.reserve(value.size());
        for (const char ch : value)
        {
            switch (ch)
            {
            case '\\':
                out += "\\\\";
                break;
            case '"':
                out += "\\\"";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                out.push_back(ch);
                break;
            }
        }
        return out;
    }

    const char* RenderTypeToString(eFxRenderType value)
    {
        switch (value)
        {
        case eFxRenderType::Ribbon:
            return "Ribbon";
        case eFxRenderType::Beam:
            return "Beam";
        case eFxRenderType::GroundDecal:
            return "GroundDecal";
        case eFxRenderType::MeshParticle:
            return "MeshParticle";
        case eFxRenderType::ShockwaveRing:
            return "ShockwaveRing";
        case eFxRenderType::Billboard:
        default:
            return "Billboard";
        }
    }

    const char* BlendPresetToString(eBlendPreset value)
    {
        switch (value)
        {
        case eBlendPreset::Opaque:
            return "Opaque";
        case eBlendPreset::PremultipliedAlpha:
            return "PremultipliedAlpha";
        case eBlendPreset::Additive:
            return "Additive";
        case eBlendPreset::AlphaBlend:
        default:
            return "AlphaBlend";
        }
    }

    const char* DepthModeToString(eFxDepthMode value)
    {
        switch (value)
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

    bool_t EnsureParentDirectory(const wstring_t& path, std::string& outError)
    {
        std::error_code ec;
        const std::filesystem::path parent = std::filesystem::path(path).parent_path();
        if (parent.empty() || std::filesystem::exists(parent, ec))
            return true;

        if (std::filesystem::create_directories(parent, ec))
            return true;

        outError = "create_parent_directory_failed";
        return false;
    }

    void WriteVec3(std::ofstream& file, const Vec3& value)
    {
        file << '[' << value.x << ", " << value.y << ", " << value.z << ']';
    }

    void WriteVec4(std::ofstream& file, const Vec4& value)
    {
        file << '[' << value.x << ", " << value.y << ", "
            << value.z << ", " << value.w << ']';
    }

    void WriteBool(std::ofstream& file, bool_t value)
    {
        file << (value ? "true" : "false");
    }

    void WriteStringField(std::ofstream& file,
        const char* name,
        const std::string& value,
        const char* suffix)
    {
        file << "      \"" << name << "\": \""
            << EscapeJson(NormalizePath(value)) << "\"" << suffix << '\n';
    }

    void WriteVec3Field(std::ofstream& file,
        const char* name,
        const Vec3& value,
        const char* suffix)
    {
        file << "      \"" << name << "\": ";
        WriteVec3(file, value);
        file << suffix << '\n';
    }

    void WriteVec4Field(std::ofstream& file,
        const char* name,
        const Vec4& value,
        const char* suffix)
    {
        file << "      \"" << name << "\": ";
        WriteVec4(file, value);
        file << suffix << '\n';
    }

    void WriteEmitter(std::ofstream& file,
        const FxEmitterDesc& emitter,
        bool_t bLast)
    {
        file << "    {\n";
        WriteStringField(file, "name", emitter.strName, ",");
        WriteStringField(file, "render_type", RenderTypeToString(emitter.renderType), ",");
        WriteStringField(file, "blend_mode", BlendPresetToString(emitter.blendMode), ",");
        WriteStringField(file, "depth_mode", DepthModeToString(emitter.depthMode), ",");
        WriteStringField(file, "texture", NarrowAscii(emitter.strTexturePath), ",");
        WriteStringField(file, "erode_texture", NarrowAscii(emitter.strErodeTexturePath), ",");
        WriteStringField(file, "model", emitter.strModelPath, ",");

        file << "      \"max_particles\": " << emitter.maxParticles << ",\n";
        file << "      \"spawn_rate\": " << emitter.spawnRate << ",\n";
        file << "      \"lifetime\": " << emitter.fLifetime << ",\n";
        file << "      \"start_delay\": " << emitter.fStartDelay << ",\n";
        file << "      \"fade_in\": " << emitter.fFadeIn << ",\n";
        file << "      \"fade_out\": " << emitter.fFadeOut << ",\n";
        file << "      \"width\": " << emitter.fWidth << ",\n";
        file << "      \"height\": " << emitter.fHeight << ",\n";
        file << "      \"yaw\": " << emitter.fYaw << ",\n";
        file << "      \"start_radius\": " << emitter.fStartRadius << ",\n";
        file << "      \"end_radius\": " << emitter.fEndRadius << ",\n";
        file << "      \"thickness\": " << emitter.fThickness << ",\n";
        file << "      \"grow_duration\": " << emitter.fGrowDuration << ",\n";
        WriteVec4Field(file, "color", emitter.vColor, ",");
        WriteVec3Field(file, "attach_offset", emitter.vAttachOffset, ",");
        WriteVec3Field(file, "end_offset", emitter.vEndOffset, ",");
        WriteVec3Field(file, "velocity", emitter.vVelocity, ",");
        WriteVec3Field(file, "scale", emitter.vScale, ",");
        WriteVec3Field(file, "rotation", emitter.vRotation, ",");
        file << "      \"world_yaw_spin_speed\": " << emitter.fWorldYawSpinSpeed << ",\n";
        file << "      \"segment_t\": " << emitter.fSegmentT << ",\n";
        file << "      \"scale_z_to_segment\": ";
        WriteBool(file, emitter.bScaleZToSegment);
        file << ",\n";
        file << "      \"uv_scroll\": [" << emitter.fUvScrollU << ", "
            << emitter.fUvScrollV << "],\n";
        file << "      \"alpha_clip\": " << emitter.fAlphaClip << ",\n";
        file << "      \"erode_threshold\": " << emitter.fErodeThreshold << ",\n";
        file << "      \"style_mode\": " << emitter.iStyleMode << ",\n";
        WriteVec4Field(file, "style_color_a", emitter.vStyleColorA, ",");
        WriteVec4Field(file, "style_color_b", emitter.vStyleColorB, ",");
        WriteVec4Field(file, "rim_color", emitter.vRimColor, ",");
        file << "      \"rim_power\": " << emitter.fRimPower << ",\n";
        file << "      \"cell_low\": " << emitter.fCellLow << ",\n";
        file << "      \"cell_high\": " << emitter.fCellHigh << ",\n";
        WriteVec4Field(file, "magic_scroll_a", emitter.vMagicScrollA, ",");
        WriteVec4Field(file, "magic_shape", emitter.vMagicShape, ",");
        WriteVec4Field(file, "magic_core", emitter.vMagicCore, ",");
        file << "      \"material_random\": " << emitter.fMaterialRandom << ",\n";
        file << "      \"atlas_cols\": " << emitter.iAtlasCols << ",\n";
        file << "      \"atlas_rows\": " << emitter.iAtlasRows << ",\n";
        file << "      \"atlas_frame_count\": " << emitter.iAtlasFrameCount << ",\n";
        file << "      \"atlas_fps\": " << emitter.fAtlasFps << ",\n";
        file << "      \"atlas_loop\": ";
        WriteBool(file, emitter.bAtlasLoop);
        file << ",\n";
        file << "      \"ribbon_point_count\": " << emitter.iRibbonPointCount << ",\n";
        file << "      \"billboard\": ";
        WriteBool(file, emitter.bBillboard);
        file << ",\n";
        file << "      \"depth_write\": ";
        WriteBool(file, emitter.bDepthWrite);
        file << ",\n";
        file << "      \"blockable_by_wind_wall\": ";
        WriteBool(file, emitter.bBlockableByWindWall);
        file << '\n';
        file << "    }" << (bLast ? "\n" : ",\n");
    }

    bool_t SaveWfxAsset(const wstring_t& path,
        const FxAsset& asset,
        std::string& outError)
    {
        if (!EnsureParentDirectory(path, outError))
            return false;

        std::ofstream file(std::filesystem::path(path),
            std::ios::binary | std::ios::trunc);
        if (!file)
        {
            outError = "open_wfx_for_write_failed";
            return false;
        }

        file << std::fixed << std::setprecision(4);
        file << "{\n";
        file << "  \"schema\": \"WintersWfx\",\n";
        file << "  \"version\": 1,\n";
        file << "  \"name\": \"" << EscapeJson(asset.strName) << "\",\n";
        file << "  \"emitters\": [\n";
        for (size_t i = 0; i < asset.emitters.size(); ++i)
            WriteEmitter(file, asset.emitters[i], i + 1 == asset.emitters.size());
        file << "  ]\n";
        file << "}\n";
        return true;
    }
}

bool_t WfxTool::CWfxDocument::LoadFromFile(const wstring_t& strPath)
{
    m_strLastError.clear();

    FxAssetLoadResult result = LoadFxAssetFromFile(strPath);
    if (!result.bSucceeded)
    {
        m_bLoaded = false;
        m_strLastError = result.strError;
        return false;
    }

    m_Asset = std::move(result.asset);
    m_strPath = strPath;
    m_bLoaded = true;
    return true;
}

bool_t WfxTool::CWfxDocument::SaveToFile(const wstring_t& strPath) const
{
    m_strLastError.clear();
    if (!m_bLoaded)
    {
        m_strLastError = "document_not_loaded";
        return false;
    }

    if (m_Asset.emitters.empty())
    {
        m_strLastError = "missing_emitters";
        return false;
    }

    return SaveWfxAsset(strPath, m_Asset, m_strLastError);
}

bool_t WfxTool::CWfxDocument::Save() const
{
    if (m_strPath.empty())
    {
        m_strLastError = "missing_wfx_path";
        return false;
    }

    return SaveToFile(m_strPath);
}

void WfxTool::CWfxDocument::SetAsset(FxAsset asset, const wstring_t& strPath)
{
    m_Asset = std::move(asset);
    m_strPath = strPath;
    m_strLastError.clear();
    m_bLoaded = true;
}
