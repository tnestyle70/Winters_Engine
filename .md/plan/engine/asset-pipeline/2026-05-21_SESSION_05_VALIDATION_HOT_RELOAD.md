Session - cooked asset validation과 안전한 hot reload invalidation을 추가한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Engine/Public/Asset/AssetValidator.h

새 파일:

```cpp
#pragma once

#include "WintersAPI.h"

#include <string>
#include <vector>

namespace Engine
{
    struct AssetValidationMessage
    {
        bool bError = false;
        std::string strText;
    };

    struct AssetValidationResult
    {
        bool bOk = true;
        std::vector<AssetValidationMessage> vecMessages;

        void AddError(const std::string& strText)
        {
            bOk = false;
            vecMessages.push_back({ true, strText });
        }

        void AddWarning(const std::string& strText)
        {
            vecMessages.push_back({ false, strText });
        }
    };

    class WINTERS_ENGINE CAssetValidator
    {
    public:
        static AssetValidationResult ValidateCookedFile(const std::wstring& strPath);

    private:
        CAssetValidator() = delete;
    };
}
```

1-2. C:/Users/user/Desktop/Winters/Engine/Private/Asset/AssetValidator.cpp

새 파일:

```cpp
#include "Asset/AssetValidator.h"
#include "AssetFormat/Anim/WAnimFormat.h"
#include "AssetFormat/Anim/WSkelLoader.h"
#include "AssetFormat/Common/BinaryReader.h"
#include "AssetFormat/Common/WintersFileHeader.h"
#include "AssetFormat/Material/WMaterialLoader.h"
#include "AssetFormat/Mesh/WMeshLoader.h"

#include <cwctype>
#include <cstring>
#include <filesystem>

namespace Engine
{
    namespace
    {
        std::wstring LowerExtension(const std::wstring& strPath)
        {
            std::wstring ext = std::filesystem::path(strPath).extension().wstring();
            for (wchar_t& ch : ext)
                ch = static_cast<wchar_t>(std::towlower(ch));
            return ext;
        }

        void ValidateWAnim(const std::wstring& strPath, AssetValidationResult& result)
        {
            std::vector<uint8_t> raw;
            if (!Winters::Asset::CBinaryReader::LoadFileToMemory(strPath.c_str(), raw))
            {
                result.AddError("failed to read .wanim");
                return;
            }

            try
            {
                Winters::Asset::CBinaryReader reader(raw.data(), raw.size());
                const auto fileHeader = reader.Read<Winters::Asset::WintersFileHeader>();
                if (std::memcmp(fileHeader.magic, Winters::Asset::WINTERS_MAGIC, 4) != 0)
                {
                    result.AddError("invalid Winters file magic");
                    return;
                }

                const auto animHeader = reader.Read<Winters::Asset::AnimMetaHeader>();
                if (std::memcmp(animHeader.magic, Winters::Asset::WANIM_MAGIC, 4) != 0)
                    result.AddError("invalid WANIM magic");
                if (animHeader.ticks_per_second <= 0.f)
                    result.AddError("invalid animation ticks_per_second");
                if (animHeader.duration_ticks < 0.f)
                    result.AddError("invalid animation duration");
            }
            catch (...)
            {
                result.AddError("exception while validating .wanim");
            }
        }
    }

    AssetValidationResult CAssetValidator::ValidateCookedFile(const std::wstring& strPath)
    {
        AssetValidationResult result{};
        if (!std::filesystem::exists(std::filesystem::path(strPath)))
        {
            result.AddError("file does not exist");
            return result;
        }

        const std::wstring ext = LowerExtension(strPath);
        if (ext == L".wmesh")
        {
            Winters::Asset::WMeshLoaded mesh{};
            if (!Winters::Asset::CWMeshLoader::Load(strPath.c_str(), mesh))
                result.AddError("CWMeshLoader failed");
            if (mesh.header.bone_count > 256)
                result.AddWarning("bone_count exceeds current shader-friendly limit");
            return result;
        }

        if (ext == L".wmat")
        {
            Winters::Asset::WMaterialLoaded material{};
            if (!Winters::Asset::CWMaterialLoader::Load(strPath.c_str(), material))
                result.AddError("CWMaterialLoader failed");
            return result;
        }

        if (ext == L".wskel")
        {
            Winters::Asset::WSkelLoaded skeleton{};
            if (!Winters::Asset::CWSkelLoader::Load(strPath.c_str(), skeleton))
                result.AddError("CWSkelLoader failed");
            return result;
        }

        if (ext == L".wanim")
        {
            ValidateWAnim(strPath, result);
            return result;
        }

        result.AddWarning("unknown cooked extension");
        return result;
    }
}
```

1-3. C:/Users/user/Desktop/Winters/Engine/Public/Asset/AssetWatcher.h

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
    class CAssetDatabase;

    class WINTERS_ENGINE CAssetWatcher
    {
    public:
        void SetDatabase(const CAssetDatabase* pDatabase);
        void RebuildSnapshot();
        std::vector<std::string> PollChangedVirtualPaths();

    private:
        struct FileSnapshot
        {
            uint64_t uSourceWriteTime = 0;
            uint64_t uCookedWriteTime = 0;
        };

        static uint64_t ReadWriteTime(const std::wstring& strPath);

        const CAssetDatabase* m_pDatabase = nullptr;
        std::unordered_map<std::string, FileSnapshot> m_mapSnapshots;
    };
}
```

1-4. C:/Users/user/Desktop/Winters/Engine/Private/Asset/AssetWatcher.cpp

새 파일:

```cpp
#include "Asset/AssetWatcher.h"
#include "Asset/AssetDatabase.h"

#include <filesystem>

namespace Engine
{
    void CAssetWatcher::SetDatabase(const CAssetDatabase* pDatabase)
    {
        m_pDatabase = pDatabase;
        RebuildSnapshot();
    }

    void CAssetWatcher::RebuildSnapshot()
    {
        m_mapSnapshots.clear();
        if (!m_pDatabase)
            return;

        for (const AssetRecord& record : m_pDatabase->GetRecords())
        {
            FileSnapshot snapshot{};
            snapshot.uSourceWriteTime = ReadWriteTime(record.strSourcePath);
            snapshot.uCookedWriteTime = ReadWriteTime(record.strCookedPath);
            m_mapSnapshots[record.strVirtualPath] = snapshot;
        }
    }

    std::vector<std::string> CAssetWatcher::PollChangedVirtualPaths()
    {
        std::vector<std::string> changed;
        if (!m_pDatabase)
            return changed;

        for (const AssetRecord& record : m_pDatabase->GetRecords())
        {
            FileSnapshot current{};
            current.uSourceWriteTime = ReadWriteTime(record.strSourcePath);
            current.uCookedWriteTime = ReadWriteTime(record.strCookedPath);

            auto it = m_mapSnapshots.find(record.strVirtualPath);
            if (it == m_mapSnapshots.end())
            {
                m_mapSnapshots[record.strVirtualPath] = current;
                continue;
            }

            if (it->second.uSourceWriteTime != current.uSourceWriteTime ||
                it->second.uCookedWriteTime != current.uCookedWriteTime)
            {
                changed.push_back(record.strVirtualPath);
                it->second = current;
            }
        }

        return changed;
    }

    uint64_t CAssetWatcher::ReadWriteTime(const std::wstring& strPath)
    {
        if (strPath.empty())
            return 0;

        try
        {
            return static_cast<uint64_t>(
                std::filesystem::last_write_time(std::filesystem::path(strPath))
                    .time_since_epoch()
                    .count());
        }
        catch (...)
        {
            return 0;
        }
    }
}
```

1-5. C:/Users/user/Desktop/Winters/Engine/Public/Framework/CEngineApp.h

기존 코드:

```cpp
#include "Asset/AssetDatabase.h"
#include "Resource/ResourceCache.h"
```

아래로 교체:

```cpp
#include "Asset/AssetDatabase.h"
#include "Asset/AssetWatcher.h"
#include "Resource/ResourceCache.h"
```

기존 코드:

```cpp
    CAssetDatabase m_AssetDatabase{};
    CResourceCache m_ResourceCache{};
```

아래로 교체:

```cpp
    CAssetDatabase m_AssetDatabase{};
    CAssetWatcher m_AssetWatcher{};
    CResourceCache m_ResourceCache{};
```

1-6. C:/Users/user/Desktop/Winters/Engine/Private/Framework/CEngineApp.cpp

기존 코드:

```cpp
    m_AssetDatabase.ScanManifestDirectory(L"Data");
    m_AssetDatabase.ScanManifestDirectory(L"Client/Bin/Resource");
    m_ResourceCache.Initialize(m_pDevice.get());
    m_ResourceCache.SetAssetDatabase(&m_AssetDatabase);
```

아래로 교체:

```cpp
    m_AssetDatabase.ScanManifestDirectory(L"Data");
    m_AssetDatabase.ScanManifestDirectory(L"Client/Bin/Resource");
    m_AssetWatcher.SetDatabase(&m_AssetDatabase);
    m_ResourceCache.Initialize(m_pDevice.get());
    m_ResourceCache.SetAssetDatabase(&m_AssetDatabase);
```

기존 코드:

```cpp
    auto* pSceneManager = CGameInstance::Get()->Get_SceneManager();
    pSceneManager->Update(deltaTime);
    pSceneManager->LateUpdate(deltaTime);
```

아래로 교체:

```cpp
    for (const std::string& strVirtualPath : m_AssetWatcher.PollChangedVirtualPaths())
    {
        const AssetRecord* pRecord = m_AssetDatabase.FindByVirtualPath(strVirtualPath);
        if (!pRecord)
            continue;

        if (pRecord->eKind == eAssetKind::StaticMesh ||
            pRecord->eKind == eAssetKind::SkeletalMesh)
        {
            const std::string cookedPath(pRecord->strCookedPath.begin(), pRecord->strCookedPath.end());
            m_ResourceCache.InvalidateModel(cookedPath);
        }
        else if (pRecord->eKind == eAssetKind::Texture)
        {
            m_ResourceCache.InvalidateTexture(pRecord->strCookedPath);
        }
    }

    auto* pSceneManager = CGameInstance::Get()->Get_SceneManager();
    pSceneManager->Update(deltaTime);
    pSceneManager->LateUpdate(deltaTime);
```

1-7. C:/Users/user/Desktop/Winters/Engine/Private/Tools/AssetConverter/main.cpp

기존 코드:

```cpp
#include "AssetFormat/Anim/WAnimFormat.h"
```

아래로 교체:

```cpp
#include "Asset/AssetValidator.h"
#include "AssetFormat/Anim/WAnimFormat.h"
```

기존 코드:

```cpp
static int CmdImport(int argc, char** argv)
{
```

아래로 교체:

```cpp
static int CmdValidate(int argc, char** argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: validate <file-or-directory>\n");
        return 2;
    }

    const std::wstring path = Utf8ToWide(argv[1]);
    uint32_t checked = 0;
    uint32_t failed = 0;

    const auto validateOne = [&](const std::filesystem::path& filePath)
    {
        const Engine::AssetValidationResult result =
            Engine::CAssetValidator::ValidateCookedFile(filePath.wstring());
        ++checked;
        if (!result.bOk)
            ++failed;

        for (const Engine::AssetValidationMessage& msg : result.vecMessages)
        {
            std::fprintf(msg.bError ? stderr : stdout,
                "%s: %s: %ls\n",
                msg.bError ? "error" : "warning",
                msg.strText.c_str(),
                filePath.c_str());
        }
    };

    if (std::filesystem::is_directory(std::filesystem::path(path)))
    {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(std::filesystem::path(path)))
        {
            if (!entry.is_regular_file())
                continue;

            const std::wstring ext = entry.path().extension().wstring();
            if (ext == L".wmesh" || ext == L".wmat" || ext == L".wskel" || ext == L".wanim")
                validateOne(entry.path());
        }
    }
    else
    {
        validateOne(std::filesystem::path(path));
    }

    std::printf("validated=%u failed=%u\n", checked, failed);
    return failed == 0 ? 0 : 1;
}

static int CmdImport(int argc, char** argv)
{
```

기존 코드:

```cpp
            "usage: WintersAssetConverter <command> [args...]\n"
            "  commands: mesh, material, skel, anim, import, info\n");
```

아래로 교체:

```cpp
            "usage: WintersAssetConverter <command> [args...]\n"
            "  commands: mesh, material, skel, anim, import, validate, info\n");
```

기존 코드:

```cpp
    if (cmd == "anim") return CmdAnim(argc - 1, argv + 1);
    if (cmd == "import") return CmdImport(argc - 1, argv + 1);
    if (cmd == "info") return CmdInfo(argc - 1, argv + 1);
```

아래로 교체:

```cpp
    if (cmd == "anim") return CmdAnim(argc - 1, argv + 1);
    if (cmd == "import") return CmdImport(argc - 1, argv + 1);
    if (cmd == "validate") return CmdValidate(argc - 1, argv + 1);
    if (cmd == "info") return CmdInfo(argc - 1, argv + 1);
```

2. 검증

미검증:
- `CAssetValidator` loader roundtrip 미검증
- `CAssetWatcher`가 source/cooked 변경을 감지하고 cache invalidation까지 도달하는지 미검증
- converter `validate` 명령 빌드 미검증

검증 명령:
- git diff --check
- msbuild Engine/Include/Engine.vcxproj /p:Configuration=Debug /p:Platform=x64
- msbuild Tools/WintersAssetConverter/WintersAssetConverter.vcxproj /p:Configuration=Debug /p:Platform=x64
- Tools/Bin/Debug/WintersAssetConverter.exe validate Client/Bin/Resource

수동 확인:
- `.wasset`가 등록된 모델의 cooked `.wmesh` timestamp를 바꾸면 다음 프레임 이후 `CResourceCache` 모델 캐시가 무효화되는지 확인.
- 잘못된 `.wmesh` 파일을 `validate`에 넣으면 nonzero exit code가 나오는지 확인.

확인 필요:
- `CmdValidate`가 `Engine/Private/Asset/AssetValidator.cpp`를 링크하려면 `Tools/WintersAssetConverter.vcxproj`에 해당 cpp 포함이 필요한지 확인.
- 이 세션의 watcher는 polling 기반 invalidation만 한다. source 변경 후 자동 reimport 실행은 별도 job queue 세션에서 확정한다.

후속 동기화:
- Engine public header 변경 후 `UpdateLib.bat` 실행 필요.
