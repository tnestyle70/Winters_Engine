Session - WintersAssetConverter에 import 통합 명령과 sidecar 기록 흐름을 추가한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Engine/Private/Tools/AssetConverter/main.cpp

기존 코드:

```cpp
#include "AssetFormat/Common/BinaryReader.h"
#include "AssetFormat/Common/WintersFileHeader.h"
```

아래로 교체:

```cpp
#include "AssetFormat/Common/BinaryReader.h"
#include "AssetFormat/Common/Hash.h"
#include "AssetFormat/Common/WintersFileHeader.h"
```

기존 코드:

```cpp
#include <filesystem>
#include <string>
```

아래로 교체:

```cpp
#include <filesystem>
#include <fstream>
#include <string>
```

기존 코드:

```cpp
    return ok == pScene->mNumAnimations ? 0 : 1;
}

static int CmdInfo(int argc, char** argv)
```

아래로 교체:

```cpp
    return ok == pScene->mNumAnimations ? 0 : 1;
}

static std::wstring ReplaceExtensionW(const std::wstring& path, const wchar_t* ext)
{
    std::filesystem::path p(path);
    p.replace_extension(ext);
    return p.wstring();
}

static uint64_t ReadFileTimeCount(const std::wstring& path)
{
    try
    {
        return static_cast<uint64_t>(
            std::filesystem::last_write_time(std::filesystem::path(path))
                .time_since_epoch()
                .count());
    }
    catch (...)
    {
        return 0;
    }
}

static uint64_t ReadFileSizeOrZero(const std::wstring& path)
{
    try
    {
        return static_cast<uint64_t>(std::filesystem::file_size(std::filesystem::path(path)));
    }
    catch (...)
    {
        return 0;
    }
}

static void BuildArgv(const std::vector<std::string>& args, std::vector<char*>& out)
{
    out.clear();
    out.reserve(args.size());
    for (const std::string& arg : args)
        out.push_back(const_cast<char*>(arg.c_str()));
}

static bool WriteImportSidecar(const std::wstring& sourcePath,
    const std::wstring& cookedMeshPath,
    const std::wstring& skelPath,
    const std::wstring& animDir)
{
    std::filesystem::path sidecarPath(cookedMeshPath);
    sidecarPath.replace_extension(L".wasset");

    std::ofstream file(sidecarPath, std::ios::trunc);
    if (!file.is_open())
        return false;

    const std::string virtualPath = std::filesystem::path(cookedMeshPath).generic_string();
    const uint64_t sourceWriteTime = ReadFileTimeCount(sourcePath);
    const uint64_t sourceSize = ReadFileSizeOrZero(sourcePath);

    file << "# Winters asset sidecar\n";
    file << "guid=asset-" << std::hex << Winters::Asset::FNV1a(virtualPath.c_str()) << std::dec << "\n";
    file << "virtualPath=" << virtualPath << "\n";
    file << "sourcePath=" << WideToUtf8(sourcePath) << "\n";
    file << "cookedPath=" << WideToUtf8(cookedMeshPath) << "\n";
    file << "kind=StaticMesh\n";
    file << "importerVersion=1\n";
    file << "sourceWriteTime=" << sourceWriteTime << "\n";
    file << "sourceSize=" << sourceSize << "\n";
    file << "importStatus=Imported\n";

    file << "dependencies=";
    std::filesystem::path matPath(cookedMeshPath);
    matPath.replace_extension(L".wmat");
    file << matPath.generic_string() << "|Material";
    if (!skelPath.empty())
        file << ";" << std::filesystem::path(skelPath).generic_string() << "|SkeletalMesh";
    if (!animDir.empty())
        file << ";" << std::filesystem::path(animDir).generic_string() << "|Animation";
    file << "\n";
    file << "lastError=\n";
    return true;
}

static int CmdImport(int argc, char** argv)
{
    if (argc < 4)
    {
        std::fprintf(stderr,
            "usage: import <input> -o <output.wmesh> [--skel] [--anim-dir <dir>] [--mirror-x] [--scale N]\n");
        return 2;
    }

    const std::wstring inPath = Utf8ToWide(argv[1]);
    std::wstring outMeshPath;
    bool buildSkel = false;
    std::wstring animDir;
    bool mirrorX = false;
    std::string scaleValue;

    for (int i = 2; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc)
            outMeshPath = Utf8ToWide(argv[++i]);
        else if (arg == "--skel")
            buildSkel = true;
        else if (arg == "--anim-dir" && i + 1 < argc)
            animDir = Utf8ToWide(argv[++i]);
        else if (arg == "--mirror-x")
            mirrorX = true;
        else if (arg == "--scale" && i + 1 < argc)
            scaleValue = argv[++i];
        else
        {
            std::fprintf(stderr, "unknown import arg: %s\n", arg.c_str());
            return 2;
        }
    }

    if (outMeshPath.empty())
    {
        std::fprintf(stderr, "missing -o\n");
        return 2;
    }

    const std::wstring skelPath = buildSkel ? ReplaceExtensionW(outMeshPath, L".wskel") : L"";
    std::vector<std::string> args;
    std::vector<char*> nestedArgv;

    if (buildSkel)
    {
        args = { "skel", WideToUtf8(inPath), "-o", WideToUtf8(skelPath) };
        BuildArgv(args, nestedArgv);
        const int rc = CmdSkel(static_cast<int>(nestedArgv.size()), nestedArgv.data());
        if (rc != 0)
            return rc;
    }

    args = { "mesh", WideToUtf8(inPath), "-o", WideToUtf8(outMeshPath) };
    if (buildSkel)
    {
        args.push_back("--skel");
        args.push_back(WideToUtf8(skelPath));
    }
    if (mirrorX)
        args.push_back("--mirror-x");
    if (!scaleValue.empty())
    {
        args.push_back("--scale");
        args.push_back(scaleValue);
    }

    BuildArgv(args, nestedArgv);
    const int meshRc = CmdMesh(static_cast<int>(nestedArgv.size()), nestedArgv.data());
    if (meshRc != 0)
        return meshRc;

    if (!animDir.empty())
    {
        if (!buildSkel)
        {
            std::fprintf(stderr, "--anim-dir requires --skel for skeleton hash validation\n");
            return 2;
        }

        args = { "anim", WideToUtf8(inPath), "--skel", WideToUtf8(skelPath), "-o", WideToUtf8(animDir) };
        BuildArgv(args, nestedArgv);
        const int animRc = CmdAnim(static_cast<int>(nestedArgv.size()), nestedArgv.data());
        if (animRc != 0)
            return animRc;
    }

    if (!WriteImportSidecar(inPath, outMeshPath, skelPath, animDir))
    {
        std::fprintf(stderr, "warning: failed to write .wasset sidecar\n");
    }

    return 0;
}

static int CmdInfo(int argc, char** argv)
```

기존 코드:

```cpp
            "usage: WintersAssetConverter <command> [args...]\n"
            "  commands: mesh, material, skel, anim, info\n");
```

아래로 교체:

```cpp
            "usage: WintersAssetConverter <command> [args...]\n"
            "  commands: mesh, material, skel, anim, import, info\n");
```

기존 코드:

```cpp
    if (cmd == "skel") return CmdSkel(argc - 1, argv + 1);
    if (cmd == "anim") return CmdAnim(argc - 1, argv + 1);
    if (cmd == "info") return CmdInfo(argc - 1, argv + 1);
```

아래로 교체:

```cpp
    if (cmd == "skel") return CmdSkel(argc - 1, argv + 1);
    if (cmd == "anim") return CmdAnim(argc - 1, argv + 1);
    if (cmd == "import") return CmdImport(argc - 1, argv + 1);
    if (cmd == "info") return CmdInfo(argc - 1, argv + 1);
```

2. 검증

미검증:
- `import` 통합 명령 빌드 미검증
- `.wasset` sidecar 생성 후 Session 01 `CAssetManifest::Load`로 읽히는지 미검증

검증 명령:
- git diff --check
- msbuild Tools/WintersAssetConverter/WintersAssetConverter.vcxproj /p:Configuration=Debug /p:Platform=x64
- Tools/Bin/Debug/WintersAssetConverter.exe import Data/LoL/Champions/Test/Test.fbx -o Client/Bin/Resource/Model/Test/Test.wmesh --skel --anim-dir Client/Bin/Resource/Model/Test/anims --scale 0.01
- Tools/Bin/Debug/WintersAssetConverter.exe info Client/Bin/Resource/Model/Test/Test.wmesh

확인 필요:
- `WriteImportSidecar`의 dependency 문자열 포맷이 Session 01 `CAssetManifest`와 같은 구분자를 쓰는지 확인.
- 새 `Engine/Private/Asset/*.cpp`를 converter에서 직접 참조하도록 바꾸면 `Tools/WintersAssetConverter.vcxproj` 포함 여부를 확인.
