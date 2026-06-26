#include "AssetFormat/Material/WMaterialWriter.h"
#include "AssetFormat/Material/WMaterialFormat.h"
#include "AssetFormat/Common/BinaryWriter.h"
#include "AssetFormat/Common/Hash.h"

#include <assimp/material.h>
#include <assimp/scene.h>
#include <assimp/texture.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cwchar>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace Winters::Asset
{
    static std::wstring WidenAscii(const std::string& s)
    {
        return std::wstring(s.begin(), s.end());
    }

    static std::string NarrowAscii(const std::wstring& s)
    {
        std::string out;
        out.reserve(s.size());
        for (wchar_t ch : s)
            out.push_back(static_cast<char>(ch));
        return out;
    }

    static std::string ToSlashPath(std::string s)
    {
        std::replace(s.begin(), s.end(), '\\', '/');
        return s;
    }

    static std::string ToLowerAscii(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    static std::wstring ToRuntimePath(const std::filesystem::path& path)
    {
        const std::string full = ToSlashPath(NarrowAscii(path.lexically_normal().wstring()));
        const std::string lower = ToLowerAscii(full);
        constexpr const char* marker = "/client/bin/resource/";
        const size_t pos = lower.find(marker);
        if (pos != std::string::npos)
            return WidenAscii(full.substr(pos + 1));

        return WidenAscii(full);
    }

    static bool IsKnownImageExtension(const std::string& ext)
    {
        const std::string lower = ToLowerAscii(ext);
        return lower == ".png" ||
            lower == ".jpg" ||
            lower == ".jpeg" ||
            lower == ".dds" ||
            lower == ".tga" ||
            lower == ".bmp";
    }

    static std::string GetEmbeddedTextureExtension(const aiTexture* pTexture)
    {
        if (!pTexture)
            return ".png";

        std::string hint;
        for (char ch : pTexture->achFormatHint)
        {
            if (ch == '\0')
                break;
            hint.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }

        if (hint == "jpg" || hint == "jpeg")
            return ".jpg";
        if (hint == "dds")
            return ".dds";
        if (hint == "tga")
            return ".tga";
        if (hint == "bmp")
            return ".bmp";
        return ".png";
    }

    static std::string SanitizeFilename(std::string name)
    {
        for (char& ch : name)
        {
            const unsigned char c = static_cast<unsigned char>(ch);
            if (!std::isalnum(c) && ch != '_' && ch != '-' && ch != '.')
                ch = '_';
        }

        while (!name.empty() && (name.front() == '.' || name.front() == '_'))
            name.erase(name.begin());
        if (name.empty())
            name = "embedded";
        if (name.size() > 120)
            name.resize(120);
        return name;
    }

    static std::filesystem::path WriteEmbeddedTexture(
        const aiTexture* pTexture,
        int textureIndex,
        const std::filesystem::path& modelDir)
    {
        if (!pTexture || !pTexture->pcData || pTexture->mHeight != 0 || pTexture->mWidth == 0)
            return {};

        std::string name = pTexture->mFilename.length > 0
            ? pTexture->mFilename.C_Str()
            : ("texture_" + std::to_string(textureIndex));
        name = SanitizeFilename(name);

        const std::string ext = GetEmbeddedTextureExtension(pTexture);
        std::filesystem::path namePath = WidenAscii(name);
        std::string existingExt = NarrowAscii(namePath.extension().wstring());
        if (!IsKnownImageExtension(existingExt))
            name += ext;

        char prefix[32]{};
        sprintf_s(prefix, "embedded_%03d_", textureIndex);

        const std::filesystem::path outDir = modelDir / L"textures" / L"embedded";
        std::error_code ec;
        std::filesystem::create_directories(outDir, ec);
        if (ec)
            return {};

        const std::filesystem::path outPath = outDir / WidenAscii(std::string(prefix) + name);
        if (!std::filesystem::exists(outPath))
        {
            std::ofstream out(outPath, std::ios::binary);
            if (!out)
                return {};
            out.write(reinterpret_cast<const char*>(pTexture->pcData), pTexture->mWidth);
            if (!out)
                return {};
        }

        return outPath;
    }

    static bool TryGetMaterialBaseColorPath(const aiMaterial* pMaterial, aiString& outTexturePath)
    {
        if (!pMaterial)
            return false;

        if (pMaterial->GetTexture(aiTextureType_BASE_COLOR, 0, &outTexturePath) == AI_SUCCESS)
            return true;
        if (pMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &outTexturePath) == AI_SUCCESS)
            return true;
        return false;
    }

    static std::wstring ResolveDiffusePath(
        const aiScene* pScene,
        const std::filesystem::path& modelDir,
        const aiString& texturePath)
    {
        const char* pTexture = texturePath.C_Str();
        if (!pTexture || pTexture[0] == '\0')
            return {};

        if (pTexture[0] == '*')
        {
            if (!pScene)
                return {};

            const auto embedded = pScene->GetEmbeddedTextureAndIndex(pTexture);
            const std::filesystem::path writtenPath =
                WriteEmbeddedTexture(embedded.first, embedded.second, modelDir);
            return writtenPath.empty() ? std::wstring{} : ToRuntimePath(writtenPath);
        }

        std::filesystem::path candidate = WidenAscii(pTexture);
        if (!candidate.is_absolute())
            candidate = modelDir / candidate;

        if (!std::filesystem::exists(candidate))
        {
            std::filesystem::path byFilename = modelDir / std::filesystem::path(WidenAscii(pTexture)).filename();
            if (std::filesystem::exists(byFilename))
                candidate = byFilename;
        }

        return ToRuntimePath(candidate);
    }

    static std::filesystem::path FindTextureInModelDir(
        const std::filesystem::path& modelDir,
        const char* pFilename)
    {
        if (!pFilename || !pFilename[0])
            return {};

        const std::filesystem::path candidate = modelDir / WidenAscii(pFilename);
        if (std::filesystem::exists(candidate))
            return candidate;

        return {};
    }

    static std::string ToAssetToken(std::string s)
    {
        std::string out;
        out.reserve(s.size());
        for (char ch : s)
        {
            const unsigned char c = static_cast<unsigned char>(ch);
            if (std::isalnum(c))
                out.push_back(static_cast<char>(std::tolower(c)));
        }
        return out;
    }

    static int ScoreDiffuseCandidate(
        const std::filesystem::path& path,
        const std::string& materialName,
        const std::string& materialToken)
    {
        const std::string fileName = ToLowerAscii(NarrowAscii(path.filename().wstring()));
        const std::string stemToken = ToAssetToken(NarrowAscii(path.stem().wstring()));
        if (stemToken.empty())
            return 0;

        int score = 0;
        if (!materialToken.empty() && stemToken.find(materialToken) != std::string::npos)
            score += 100;

        const bool defaultLike =
            materialName == "default" ||
            materialName == "defaultmaterial" ||
            materialName == "body" ||
            materialName.find("base") != std::string::npos;
        if (defaultLike &&
            (fileName.find("base") != std::string::npos ||
                fileName.find("body") != std::string::npos))
        {
            score += 60;
        }

        if (fileName.find("basecolor") != std::string::npos ||
            fileName.find("albedo") != std::string::npos ||
            fileName.find("diffuse") != std::string::npos ||
            fileName.find("_cm") != std::string::npos ||
            fileName.find("_tx") != std::string::npos)
        {
            score += 20;
        }

        return score;
    }

    static std::filesystem::path FindBestDiffuseTextureInModelDir(
        const std::filesystem::path& modelDir,
        const char* pMaterialName)
    {
        if (!pMaterialName || !pMaterialName[0])
            return {};

        const std::string materialName = ToLowerAscii(pMaterialName);
        const std::string materialToken = ToAssetToken(materialName);

        std::error_code ec;
        if (!std::filesystem::exists(modelDir, ec))
            return {};

        std::filesystem::path bestPath;
        int bestScore = 0;
        for (const std::filesystem::directory_entry& entry :
            std::filesystem::directory_iterator(modelDir, ec))
        {
            if (ec)
                break;
            if (!entry.is_regular_file(ec) || ec)
                continue;

            const std::string ext = NarrowAscii(entry.path().extension().wstring());
            if (!IsKnownImageExtension(ext))
                continue;

            const int score = ScoreDiffuseCandidate(entry.path(), materialName, materialToken);
            if (score > bestScore)
            {
                bestScore = score;
                bestPath = entry.path();
            }
        }

        return bestScore > 0 ? bestPath : std::filesystem::path{};
    }

    static std::wstring ResolveNearbyDiffusePath(
        const std::filesystem::path& modelDir,
        const char* pMaterialName)
    {
        const std::filesystem::path texturePath =
            FindBestDiffuseTextureInModelDir(modelDir, pMaterialName);
        return texturePath.empty() ? std::wstring{} : ToRuntimePath(texturePath);
    }

    bool CWMaterialWriter::WriteFromAssimp(
        const aiScene* pScene,
        const wchar_t* pSourceModelPath,
        const wchar_t* pOutPath)
    {
        if (!pScene || !pOutPath)
            return false;

        const std::filesystem::path modelPath = pSourceModelPath ? pSourceModelPath : L"";
        const std::filesystem::path modelDir = modelPath.parent_path();

        CBinaryWriter w;
        MaterialMetaHeader hdr{};
        std::memcpy(hdr.magic, WMAT_MAGIC, 4);
        hdr.material_count = pScene->mNumMaterials;
        w.Write(hdr);

        for (uint32_t i = 0; i < pScene->mNumMaterials; ++i)
        {
            const aiMaterial* pMaterial = pScene->mMaterials[i];
            MaterialEntry entry{};
            entry.material_index = i;

            aiString matName;
            if (pMaterial)
                pMaterial->Get(AI_MATKEY_NAME, matName);
            const char* pName = matName.C_Str();
            entry.material_hash = FNV1a(pName);
            strncpy_s(entry.name, sizeof(entry.name), pName ? pName : "", _TRUNCATE);

            aiString diffusePath;
            if (TryGetMaterialBaseColorPath(pMaterial, diffusePath))
            {
                const std::wstring resolved = ResolveDiffusePath(pScene, modelDir, diffusePath);
                if (!resolved.empty())
                    wcsncpy_s(entry.diffuse_path, WMAT_MAX_PATH, resolved.c_str(), _TRUNCATE);
            }
            else
            {
                const std::wstring resolved =
                    ResolveNearbyDiffusePath(modelDir, pName);
                if (!resolved.empty())
                    wcsncpy_s(entry.diffuse_path, WMAT_MAX_PATH, resolved.c_str(), _TRUNCATE);
            }

            w.Write(entry);
        }

        return w.SaveToFile(pOutPath, WF_NONE);
    }
}
