Session - 에셋 identity, path, manifest, database 기반을 추가한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Engine/Public/Asset/AssetTypes.h

새 파일:

```cpp
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"

#include <string>
#include <vector>

namespace Engine
{
    enum class eAssetKind : uint8_t
    {
        Unknown = 0,
        Texture,
        StaticMesh,
        SkeletalMesh,
        Material,
        Animation,
        Fx,
        Shader,
        Sound,
        MapData
    };

    enum class eAssetImportStatus : uint8_t
    {
        Unknown = 0,
        NotImported,
        Imported,
        Failed,
        SourceMissing,
        CookedMissing,
        OutOfDate
    };

    struct AssetDependency
    {
        std::string strPath;
        eAssetKind eKind = eAssetKind::Unknown;
    };

    struct AssetRecord
    {
        std::string strGuid;
        std::string strVirtualPath;
        std::wstring strSourcePath;
        std::wstring strCookedPath;
        eAssetKind eKind = eAssetKind::Unknown;
        uint64_t uSourceWriteTime = 0;
        uint64_t uSourceSize = 0;
        uint32_t uImporterVersion = 1;
        eAssetImportStatus eImportStatus = eAssetImportStatus::Unknown;
        std::vector<AssetDependency> vecDependencies;
        std::string strLastError;
    };
}
```

1-2. C:/Users/user/Desktop/Winters/Engine/Public/Asset/AssetPath.h

새 파일:

```cpp
#pragma once

#include "WintersAPI.h"
#include "Asset/AssetTypes.h"

#include <string>

namespace Engine
{
    class WINTERS_ENGINE CAssetPath
    {
    public:
        static std::string NormalizeVirtualPath(const std::string& strPath);
        static std::wstring NormalizeDiskPath(const std::wstring& strPath);
        static std::string ToLowerAscii(const std::string& strText);
        static std::wstring ReplaceExtension(const std::wstring& strPath, const wchar_t* pExtension);
        static eAssetKind InferKindFromExtension(const std::wstring& strPath);
        static eAssetKind InferKindFromExtension(const std::string& strPath);
        static bool IsCookedExtension(const std::wstring& strPath);
        static std::string BuildDefaultGuid(const std::string& strVirtualPath);

    private:
        CAssetPath() = delete;
    };
}
```

1-3. C:/Users/user/Desktop/Winters/Engine/Private/Asset/AssetPath.cpp

새 파일:

```cpp
#include "Asset/AssetPath.h"
#include "AssetFormat/Common/Hash.h"

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <filesystem>
#include <sstream>

namespace Engine
{
    std::string CAssetPath::NormalizeVirtualPath(const std::string& strPath)
    {
        std::string out = strPath;
        std::replace(out.begin(), out.end(), '\\', '/');
        while (!out.empty() && out.front() == '/')
            out.erase(out.begin());
        out = ToLowerAscii(out);
        return out;
    }

    std::wstring CAssetPath::NormalizeDiskPath(const std::wstring& strPath)
    {
        std::wstring out = strPath;
        std::replace(out.begin(), out.end(), L'/', L'\\');
        return out;
    }

    std::string CAssetPath::ToLowerAscii(const std::string& strText)
    {
        std::string out = strText;
        std::transform(out.begin(), out.end(), out.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return out;
    }

    std::wstring CAssetPath::ReplaceExtension(const std::wstring& strPath, const wchar_t* pExtension)
    {
        std::filesystem::path path(strPath);
        path.replace_extension(pExtension ? pExtension : L"");
        return path.wstring();
    }

    eAssetKind CAssetPath::InferKindFromExtension(const std::wstring& strPath)
    {
        std::filesystem::path path(strPath);
        std::wstring ext = path.extension().wstring();
        std::transform(ext.begin(), ext.end(), ext.begin(),
            [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });

        if (ext == L".png" || ext == L".jpg" || ext == L".jpeg" || ext == L".dds" || ext == L".tga" || ext == L".bmp")
            return eAssetKind::Texture;
        if (ext == L".wmesh" || ext == L".fbx" || ext == L".gltf" || ext == L".glb" || ext == L".obj")
            return eAssetKind::StaticMesh;
        if (ext == L".wskel")
            return eAssetKind::SkeletalMesh;
        if (ext == L".wmat")
            return eAssetKind::Material;
        if (ext == L".wanim")
            return eAssetKind::Animation;
        if (ext == L".wfx")
            return eAssetKind::Fx;
        if (ext == L".hlsl" || ext == L".cso")
            return eAssetKind::Shader;
        if (ext == L".wav" || ext == L".ogg" || ext == L".mp3")
            return eAssetKind::Sound;
        if (ext == L".dat" || ext == L".wmap")
            return eAssetKind::MapData;

        return eAssetKind::Unknown;
    }

    eAssetKind CAssetPath::InferKindFromExtension(const std::string& strPath)
    {
        return InferKindFromExtension(std::wstring(strPath.begin(), strPath.end()));
    }

    bool CAssetPath::IsCookedExtension(const std::wstring& strPath)
    {
        std::filesystem::path path(strPath);
        std::wstring ext = path.extension().wstring();
        std::transform(ext.begin(), ext.end(), ext.begin(),
            [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });

        return ext == L".wmesh" ||
            ext == L".wmat" ||
            ext == L".wskel" ||
            ext == L".wanim" ||
            ext == L".wfx" ||
            ext == L".wmap";
    }

    std::string CAssetPath::BuildDefaultGuid(const std::string& strVirtualPath)
    {
        const std::string normalized = NormalizeVirtualPath(strVirtualPath);
        const uint64_t hash = Winters::Asset::FNV1a(normalized.c_str());

        std::ostringstream oss;
        oss << "asset-" << std::hex << hash;
        return oss.str();
    }
}
```

1-4. C:/Users/user/Desktop/Winters/Engine/Public/Asset/AssetManifest.h

새 파일:

```cpp
#pragma once

#include "WintersAPI.h"
#include "Asset/AssetTypes.h"

#include <string>

namespace Engine
{
    class WINTERS_ENGINE CAssetManifest
    {
    public:
        static bool Load(const std::wstring& strPath, AssetRecord& outRecord);
        static bool Save(const std::wstring& strPath, const AssetRecord& record);

        static const char* ToString(eAssetKind eKind);
        static const char* ToString(eAssetImportStatus eStatus);
        static eAssetKind ParseKind(const std::string& strText);
        static eAssetImportStatus ParseStatus(const std::string& strText);

    private:
        CAssetManifest() = delete;
    };
}
```

1-5. C:/Users/user/Desktop/Winters/Engine/Private/Asset/AssetManifest.cpp

새 파일:

```cpp
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "Asset/AssetManifest.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <unordered_map>

namespace Engine
{
    namespace
    {
        std::string WideToUtf8(const std::wstring& strText)
        {
            if (strText.empty())
                return {};

            const int count = WideCharToMultiByte(CP_UTF8, 0, strText.c_str(), -1, nullptr, 0, nullptr, nullptr);
            if (count <= 1)
                return {};

            std::string out(static_cast<size_t>(count - 1), '\0');
            WideCharToMultiByte(CP_UTF8, 0, strText.c_str(), -1, out.data(), count, nullptr, nullptr);
            return out;
        }

        std::wstring Utf8ToWide(const std::string& strText)
        {
            if (strText.empty())
                return {};

            const int count = MultiByteToWideChar(CP_UTF8, 0, strText.c_str(), -1, nullptr, 0);
            if (count <= 1)
                return {};

            std::wstring out(static_cast<size_t>(count - 1), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, strText.c_str(), -1, out.data(), count);
            return out;
        }

        std::string Trim(const std::string& strText)
        {
            const size_t begin = strText.find_first_not_of(" \t\r\n");
            if (begin == std::string::npos)
                return {};

            const size_t end = strText.find_last_not_of(" \t\r\n");
            return strText.substr(begin, end - begin + 1);
        }

        std::vector<AssetDependency> ParseDependencies(const std::string& strText)
        {
            std::vector<AssetDependency> out;
            std::stringstream ss(strText);
            std::string item;

            while (std::getline(ss, item, ';'))
            {
                item = Trim(item);
                if (item.empty())
                    continue;

                AssetDependency dep{};
                const size_t sep = item.find('|');
                if (sep == std::string::npos)
                {
                    dep.strPath = item;
                }
                else
                {
                    dep.strPath = item.substr(0, sep);
                    dep.eKind = CAssetManifest::ParseKind(item.substr(sep + 1));
                }
                out.push_back(std::move(dep));
            }

            return out;
        }

        std::string FormatDependencies(const std::vector<AssetDependency>& deps)
        {
            std::string out;
            for (size_t i = 0; i < deps.size(); ++i)
            {
                if (i > 0)
                    out += ';';
                out += deps[i].strPath;
                out += '|';
                out += CAssetManifest::ToString(deps[i].eKind);
            }
            return out;
        }
    }

    bool CAssetManifest::Load(const std::wstring& strPath, AssetRecord& outRecord)
    {
        std::ifstream file(std::filesystem::path(strPath));
        if (!file.is_open())
            return false;

        std::unordered_map<std::string, std::string> values;
        std::string line;
        while (std::getline(file, line))
        {
            line = Trim(line);
            if (line.empty() || line[0] == '#')
                continue;

            const size_t sep = line.find('=');
            if (sep == std::string::npos)
                continue;

            values[Trim(line.substr(0, sep))] = Trim(line.substr(sep + 1));
        }

        outRecord = {};
        outRecord.strGuid = values["guid"];
        outRecord.strVirtualPath = values["virtualPath"];
        outRecord.strSourcePath = Utf8ToWide(values["sourcePath"]);
        outRecord.strCookedPath = Utf8ToWide(values["cookedPath"]);
        outRecord.eKind = ParseKind(values["kind"]);
        outRecord.uImporterVersion = static_cast<uint32_t>(std::strtoul(values["importerVersion"].c_str(), nullptr, 10));
        outRecord.uSourceWriteTime = static_cast<uint64_t>(std::strtoull(values["sourceWriteTime"].c_str(), nullptr, 10));
        outRecord.uSourceSize = static_cast<uint64_t>(std::strtoull(values["sourceSize"].c_str(), nullptr, 10));
        outRecord.eImportStatus = ParseStatus(values["importStatus"]);
        outRecord.vecDependencies = ParseDependencies(values["dependencies"]);
        outRecord.strLastError = values["lastError"];
        return !outRecord.strVirtualPath.empty();
    }

    bool CAssetManifest::Save(const std::wstring& strPath, const AssetRecord& record)
    {
        const std::filesystem::path path(strPath);
        std::filesystem::create_directories(path.parent_path());

        std::ofstream file(path, std::ios::trunc);
        if (!file.is_open())
            return false;

        file << "# Winters asset sidecar\n";
        file << "guid=" << record.strGuid << "\n";
        file << "virtualPath=" << record.strVirtualPath << "\n";
        file << "sourcePath=" << WideToUtf8(record.strSourcePath) << "\n";
        file << "cookedPath=" << WideToUtf8(record.strCookedPath) << "\n";
        file << "kind=" << ToString(record.eKind) << "\n";
        file << "importerVersion=" << record.uImporterVersion << "\n";
        file << "sourceWriteTime=" << record.uSourceWriteTime << "\n";
        file << "sourceSize=" << record.uSourceSize << "\n";
        file << "importStatus=" << ToString(record.eImportStatus) << "\n";
        file << "dependencies=" << FormatDependencies(record.vecDependencies) << "\n";
        file << "lastError=" << record.strLastError << "\n";
        return true;
    }

    const char* CAssetManifest::ToString(eAssetKind eKind)
    {
        switch (eKind)
        {
        case eAssetKind::Texture: return "Texture";
        case eAssetKind::StaticMesh: return "StaticMesh";
        case eAssetKind::SkeletalMesh: return "SkeletalMesh";
        case eAssetKind::Material: return "Material";
        case eAssetKind::Animation: return "Animation";
        case eAssetKind::Fx: return "Fx";
        case eAssetKind::Shader: return "Shader";
        case eAssetKind::Sound: return "Sound";
        case eAssetKind::MapData: return "MapData";
        default: return "Unknown";
        }
    }

    const char* CAssetManifest::ToString(eAssetImportStatus eStatus)
    {
        switch (eStatus)
        {
        case eAssetImportStatus::NotImported: return "NotImported";
        case eAssetImportStatus::Imported: return "Imported";
        case eAssetImportStatus::Failed: return "Failed";
        case eAssetImportStatus::SourceMissing: return "SourceMissing";
        case eAssetImportStatus::CookedMissing: return "CookedMissing";
        case eAssetImportStatus::OutOfDate: return "OutOfDate";
        default: return "Unknown";
        }
    }

    eAssetKind CAssetManifest::ParseKind(const std::string& strText)
    {
        if (strText == "Texture") return eAssetKind::Texture;
        if (strText == "StaticMesh") return eAssetKind::StaticMesh;
        if (strText == "SkeletalMesh") return eAssetKind::SkeletalMesh;
        if (strText == "Material") return eAssetKind::Material;
        if (strText == "Animation") return eAssetKind::Animation;
        if (strText == "Fx") return eAssetKind::Fx;
        if (strText == "Shader") return eAssetKind::Shader;
        if (strText == "Sound") return eAssetKind::Sound;
        if (strText == "MapData") return eAssetKind::MapData;
        return eAssetKind::Unknown;
    }

    eAssetImportStatus CAssetManifest::ParseStatus(const std::string& strText)
    {
        if (strText == "NotImported") return eAssetImportStatus::NotImported;
        if (strText == "Imported") return eAssetImportStatus::Imported;
        if (strText == "Failed") return eAssetImportStatus::Failed;
        if (strText == "SourceMissing") return eAssetImportStatus::SourceMissing;
        if (strText == "CookedMissing") return eAssetImportStatus::CookedMissing;
        if (strText == "OutOfDate") return eAssetImportStatus::OutOfDate;
        return eAssetImportStatus::Unknown;
    }
}
```

1-6. C:/Users/user/Desktop/Winters/Engine/Public/Asset/AssetDatabase.h

새 파일:

```cpp
#pragma once

#include "WintersAPI.h"
#include "Asset/AssetTypes.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace Engine
{
    class WINTERS_ENGINE CAssetDatabase
    {
    public:
        bool ScanManifestDirectory(const std::wstring& strRootDirectory);
        void Clear();
        void Upsert(const AssetRecord& record);

        const AssetRecord* FindByVirtualPath(const std::string& strVirtualPath) const;
        const AssetRecord* FindByGuid(const std::string& strGuid) const;
        const std::vector<AssetRecord>& GetRecords() const { return m_vecRecords; }

        uint32_t GetRecordCount() const { return static_cast<uint32_t>(m_vecRecords.size()); }

    private:
        void RebuildIndexes();

        std::vector<AssetRecord> m_vecRecords;
        std::unordered_map<std::string, size_t> m_mapVirtualPathToIndex;
        std::unordered_map<std::string, size_t> m_mapGuidToIndex;
    };
}
```

1-7. C:/Users/user/Desktop/Winters/Engine/Private/Asset/AssetDatabase.cpp

새 파일:

```cpp
#include "Asset/AssetDatabase.h"
#include "Asset/AssetManifest.h"
#include "Asset/AssetPath.h"

#include <filesystem>

namespace Engine
{
    bool CAssetDatabase::ScanManifestDirectory(const std::wstring& strRootDirectory)
    {
        Clear();

        const std::filesystem::path root(strRootDirectory);
        if (!std::filesystem::exists(root))
            return false;

        for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
        {
            if (!entry.is_regular_file())
                continue;

            if (entry.path().extension() != L".wasset")
                continue;

            AssetRecord record{};
            if (CAssetManifest::Load(entry.path().wstring(), record))
                m_vecRecords.push_back(std::move(record));
        }

        RebuildIndexes();
        return true;
    }

    void CAssetDatabase::Clear()
    {
        m_vecRecords.clear();
        m_mapVirtualPathToIndex.clear();
        m_mapGuidToIndex.clear();
    }

    void CAssetDatabase::Upsert(const AssetRecord& record)
    {
        const std::string normalizedPath = CAssetPath::NormalizeVirtualPath(record.strVirtualPath);
        const auto it = m_mapVirtualPathToIndex.find(normalizedPath);
        if (it != m_mapVirtualPathToIndex.end())
        {
            m_vecRecords[it->second] = record;
            RebuildIndexes();
            return;
        }

        m_vecRecords.push_back(record);
        RebuildIndexes();
    }

    const AssetRecord* CAssetDatabase::FindByVirtualPath(const std::string& strVirtualPath) const
    {
        const std::string normalizedPath = CAssetPath::NormalizeVirtualPath(strVirtualPath);
        const auto it = m_mapVirtualPathToIndex.find(normalizedPath);
        if (it == m_mapVirtualPathToIndex.end())
            return nullptr;

        return &m_vecRecords[it->second];
    }

    const AssetRecord* CAssetDatabase::FindByGuid(const std::string& strGuid) const
    {
        const auto it = m_mapGuidToIndex.find(strGuid);
        if (it == m_mapGuidToIndex.end())
            return nullptr;

        return &m_vecRecords[it->second];
    }

    void CAssetDatabase::RebuildIndexes()
    {
        m_mapVirtualPathToIndex.clear();
        m_mapGuidToIndex.clear();

        for (size_t i = 0; i < m_vecRecords.size(); ++i)
        {
            const AssetRecord& record = m_vecRecords[i];
            if (!record.strVirtualPath.empty())
                m_mapVirtualPathToIndex[CAssetPath::NormalizeVirtualPath(record.strVirtualPath)] = i;
            if (!record.strGuid.empty())
                m_mapGuidToIndex[record.strGuid] = i;
        }
    }
}
```

2. 검증

미검증:
- 신규 Asset 파일 빌드 미검증
- `.wasset` sidecar scan roundtrip 미검증

검증 명령:
- git diff --check
- msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64
- Tools/Bin/Debug/WintersAssetConverter.exe info Client/Bin/Resource/Texture/MAP/output/sr_base_flip.wmesh

확인 필요:
- 새로 추가한 `Engine/Public/Asset/*.h`, `Engine/Private/Asset/*.cpp` 파일이 Visual Studio Engine 프로젝트에 포함되는지 확인.
- CMake 경로는 `cmake/WintersEngine.cmake`의 `GLOB_RECURSE`로 자동 포함되는지 확인.

후속 동기화:
- Engine public header 변경 후 `UpdateLib.bat` 실행 필요.
