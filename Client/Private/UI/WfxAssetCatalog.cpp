#include "UI/WfxAssetCatalog.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace
{
    // Display-only fallback for wide resource paths in ImGui/status strings.
    std::string NarrowAscii(const wstring_t& value)
    {
        std::string out;
        out.reserve(value.size());
        for (const auto ch : value)
            out.push_back(ch <= 0x7f ? static_cast<char>(ch) : '?');
        return out;
    }

    std::string NormalizeSlashes(std::string value)
    {
        for (char& ch : value)
        {
            if (ch == '\\')
                ch = '/';
        }
        return value;
    }

    std::string PathText(const std::filesystem::path& path)
    {
        return NormalizeSlashes(path.generic_string());
    }

    std::string ExtensionLower(std::filesystem::path path)
    {
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
            [](unsigned char ch)
            {
                return static_cast<char>(std::tolower(ch));
            });
        return ext;
    }

    std::string StringLower(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char ch)
            {
                return static_cast<char>(std::tolower(ch));
            });
        return value;
    }

    bool_t IsTextureExtension(const std::string& ext)
    {
        return ext == ".png";
    }

    bool_t IsParticlesDirectory(const std::filesystem::path& path)
    {
        return StringLower(path.filename().string()) == "particles";
    }

    bool_t IsFrameLikeStem(const std::string& stem)
    {
        if (stem.empty())
            return false;

        for (const unsigned char ch : stem)
        {
            if (!std::isdigit(ch))
                return false;
        }
        return true;
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

    bool_t PathExists(const wstring_t& path)
    {
        if (path.empty())
            return true;

        std::error_code ec;
        return std::filesystem::exists(std::filesystem::path(path), ec);
    }

    bool_t PathExists(const std::string& path)
    {
        if (path.empty())
            return true;

        std::error_code ec;
        return std::filesystem::exists(std::filesystem::path(path), ec);
    }

    void AddMissingResource(UI::WfxAssetEntry& entry,
        const char* pszKind,
        const std::string& path)
    {
        if (path.empty() || PathExists(path))
            return;

        entry.bHasMissingResources = true;
        entry.missingResources.push_back(
            std::string(pszKind) + ": " + NormalizeSlashes(path));
    }

    void AddMissingResource(UI::WfxAssetEntry& entry,
        const char* pszKind,
        const wstring_t& path)
    {
        if (path.empty() || PathExists(path))
            return;

        entry.bHasMissingResources = true;
        entry.missingResources.push_back(
            std::string(pszKind) + ": " + NormalizeSlashes(NarrowAscii(path)));
    }

    std::string BuildRenderTypeSummary(const FxAsset& asset)
    {
        std::string result;
        for (const FxEmitterDesc& emitter : asset.emitters)
        {
            const char* pszType = RenderTypeToString(emitter.renderType);
            if (result.find(pszType) != std::string::npos)
                continue;

            if (!result.empty())
                result += ", ";
            result += pszType;
        }

        return result.empty() ? "(none)" : result;
    }

    UI::WfxAssetEntry BuildEntry(const std::filesystem::path& path)
    {
        UI::WfxAssetEntry entry{};
        entry.strPath = path.wstring();
        entry.strPathText = PathText(path);
        entry.strSkill = path.stem().string();
        entry.strChampion = path.parent_path().filename().string();

        FxAssetLoadResult result = LoadFxAssetFromFile(entry.strPath);
        if (!result.bSucceeded)
        {
            entry.bLoadSucceeded = false;
            entry.strLoadError = result.strError;
            entry.strCueName = path.stem().string();
            return entry;
        }

        entry.bLoadSucceeded = true;
        entry.strCueName = result.asset.strName;
        entry.iEmitterCount = static_cast<u32_t>(result.asset.emitters.size());
        entry.strRenderTypes = BuildRenderTypeSummary(result.asset);

        for (const FxEmitterDesc& emitter : result.asset.emitters)
        {
            AddMissingResource(entry, "texture", emitter.strTexturePath);
            AddMissingResource(entry, "erode", emitter.strErodeTexturePath);
            AddMissingResource(entry, "model", emitter.strModelPath);
        }

        return entry;
    }

    std::string RelativeFolderText(const std::filesystem::path& folder,
        const std::filesystem::path& root)
    {
        std::error_code ec;
        const std::filesystem::path relative = std::filesystem::relative(folder, root, ec);
        if (ec || relative.empty() || relative == ".")
            return ".";

        return PathText(relative);
    }

    UI::WfxTextureEntry BuildTextureEntry(const std::filesystem::path& path,
        const std::filesystem::path& particlesRoot)
    {
        UI::WfxTextureEntry entry{};
        entry.strPath = path.wstring();
        entry.strPathText = PathText(path);
        entry.strChampion = particlesRoot.parent_path().filename().string();
        entry.strFolder = RelativeFolderText(path.parent_path(), particlesRoot);
        entry.strName = path.filename().string();
        entry.strExtension = ExtensionLower(path);
        entry.bLikelyAtlasFrame = IsFrameLikeStem(path.stem().string());
        return entry;
    }
}

void UI::CWfxAssetCatalog::Clear()
{
    m_Entries.clear();
    m_TextureEntries.clear();
    m_strLastError.clear();
}

u32_t UI::CWfxAssetCatalog::ScanDirectory(const wstring_t& strRootPath)
{
    m_Entries.clear();
    m_strLastError.clear();

    std::error_code ec;
    const std::filesystem::path root(strRootPath);
    if (!std::filesystem::exists(root, ec) || !std::filesystem::is_directory(root, ec))
    {
        m_strLastError = "wfx_root_missing";
        return 0u;
    }

    std::filesystem::recursive_directory_iterator it(
        root, std::filesystem::directory_options::skip_permission_denied, ec);
    const std::filesystem::recursive_directory_iterator end{};

    // Find every .wfx asset under the selected authoring root.
    for (; it != end; it.increment(ec))
    {
        if (ec)
        {
            ec.clear();
            continue;
        }

        if (!it->is_regular_file(ec))
            continue;

        if (ExtensionLower(it->path()) != ".wfx")
            continue;

        m_Entries.push_back(BuildEntry(it->path()));
    }

    std::sort(m_Entries.begin(), m_Entries.end(),
        [](const WfxAssetEntry& lhs, const WfxAssetEntry& rhs)
        {
            // Keep the browser order stable: champion, cue, then path.
            if (lhs.strChampion != rhs.strChampion)
                return lhs.strChampion < rhs.strChampion;
            if (lhs.strCueName != rhs.strCueName)
                return lhs.strCueName < rhs.strCueName;
            return lhs.strPathText < rhs.strPathText;
        });

    return static_cast<u32_t>(m_Entries.size());
}

u32_t UI::CWfxAssetCatalog::ScanTextureDirectory(const wstring_t& strRootPath)
{
    m_TextureEntries.clear();
    m_strLastError.clear();

    std::error_code ec;
    const std::filesystem::path root(strRootPath);
    if (!std::filesystem::exists(root, ec) || !std::filesystem::is_directory(root, ec))
    {
        m_strLastError = "texture_root_missing";
        return 0u;
    }

    std::vector<std::filesystem::path> particlesRoots{};
    if (IsParticlesDirectory(root))
        particlesRoots.push_back(root);

    std::filesystem::recursive_directory_iterator dirIt(
        root, std::filesystem::directory_options::skip_permission_denied, ec);
    const std::filesystem::recursive_directory_iterator dirEnd{};

    for (; dirIt != dirEnd; dirIt.increment(ec))
    {
        if (ec)
        {
            ec.clear();
            continue;
        }

        if (!dirIt->is_directory(ec))
            continue;

        if (IsParticlesDirectory(dirIt->path()))
            particlesRoots.push_back(dirIt->path());
    }

    if (particlesRoots.empty())
    {
        m_strLastError = "particles_root_missing";
        return 0u;
    }

    for (const std::filesystem::path& particlesRoot : particlesRoots)
    {
        std::filesystem::recursive_directory_iterator fileIt(
            particlesRoot, std::filesystem::directory_options::skip_permission_denied, ec);
        const std::filesystem::recursive_directory_iterator fileEnd{};

        for (; fileIt != fileEnd; fileIt.increment(ec))
        {
            if (ec)
            {
                ec.clear();
                continue;
            }

            if (!fileIt->is_regular_file(ec))
                continue;

            if (!IsTextureExtension(ExtensionLower(fileIt->path())))
                continue;

            m_TextureEntries.push_back(BuildTextureEntry(fileIt->path(), particlesRoot));
        }
    }

    std::sort(m_TextureEntries.begin(), m_TextureEntries.end(),
        [](const WfxTextureEntry& lhs, const WfxTextureEntry& rhs)
        {
            if (lhs.strChampion != rhs.strChampion)
                return lhs.strChampion < rhs.strChampion;
            if (lhs.strFolder != rhs.strFolder)
                return lhs.strFolder < rhs.strFolder;
            if (lhs.strName != rhs.strName)
                return lhs.strName < rhs.strName;
            return lhs.strPathText < rhs.strPathText;
        });

    return static_cast<u32_t>(m_TextureEntries.size());
}

const UI::WfxAssetEntry* UI::CWfxAssetCatalog::GetEntry(size_t iIndex) const
{
    if (iIndex >= m_Entries.size())
        return nullptr;

    return &m_Entries[iIndex];
}

const UI::WfxTextureEntry* UI::CWfxAssetCatalog::GetTextureEntry(size_t iIndex) const
{
    if (iIndex >= m_TextureEntries.size())
        return nullptr;

    return &m_TextureEntries[iIndex];
}
