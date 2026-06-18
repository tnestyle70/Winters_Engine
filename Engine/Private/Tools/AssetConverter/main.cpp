// WintersAssetConverter.exe
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "AssetFormat/Anim/WAnimFormat.h"
#include "AssetFormat/Anim/WAnimWriter.h"
#include "AssetFormat/Anim/WSkelFormat.h"
#include "AssetFormat/Anim/WSkelLoader.h"
#include "AssetFormat/Anim/WSkelWriter.h"
#include "AssetFormat/Common/BinaryReader.h"
#include "AssetFormat/Common/WintersFileHeader.h"
#include "AssetFormat/Material/WMaterialFormat.h"
#include "AssetFormat/Material/WMaterialLoader.h"
#include "AssetFormat/Material/WMaterialWriter.h"
#include "AssetFormat/Mesh/WMeshFormat.h"
#include "AssetFormat/Mesh/WMeshWriter.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

using namespace Winters::Asset;

static std::wstring Utf8ToWide(const char* s)
{
    const int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    std::wstring r(n > 0 ? n - 1 : 0, L'\0');
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s, -1, r.data(), n);
    return r;
}

static std::string WideToUtf8(const std::wstring& s)
{
    const int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string r(n > 0 ? n - 1 : 0, '\0');
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, r.data(), n, nullptr, nullptr);
    return r;
}

static std::wstring SanitizeFileStem(const char* name, uint32_t fallbackIndex)
{
    std::string stem = (name && *name) ? name : ("anim_" + std::to_string(fallbackIndex));
    for (char& ch : stem)
    {
        switch (ch)
        {
        case '<': case '>': case ':': case '"': case '/': case '\\': case '|': case '?': case '*':
            ch = '_';
            break;
        default:
            break;
        }
    }
    return Utf8ToWide(stem.c_str());
}

static const aiScene* ReadScene(Assimp::Importer& importer, const std::wstring& path)
{
    const unsigned postFlags =
        aiProcess_Triangulate |
        aiProcess_ConvertToLeftHanded |
        aiProcess_GenNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_LimitBoneWeights |
        aiProcess_JoinIdenticalVertices;

    const std::string utf8 = WideToUtf8(path);
    return importer.ReadFile(utf8, postFlags);
}

static bool BuildSkelMap(const WSkelLoaded& ws, std::unordered_map<std::string, uint32_t>& out)
{
    out.clear();
    for (uint32_t i = 0; i < ws.header.bone_count; ++i)
    {
        out.emplace(ws.bones[i].name, i);
    }
    return !out.empty();
}

static int CmdMesh(int argc, char** argv)
{
    if (argc < 4)
    {
        std::fprintf(stderr, "usage: mesh <input> -o <output> [--skel <wskel>] [--mirror-x] [--include-layers] [--scale N]\n");
        return 2;
    }

    const std::wstring inPath = Utf8ToWide(argv[1]);
    std::wstring outPath;
    std::wstring skelPath;

    WMeshWriteOptions opt;
    for (int i = 2; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc)
            outPath = Utf8ToWide(argv[++i]);
        else if (arg == "--skel" && i + 1 < argc)
            skelPath = Utf8ToWide(argv[++i]);
        else if (arg == "--mirror-x")
            opt.bMirrorX = true;
        else if (arg == "--include-layers")
            opt.bIncludeLayerOverlays = true;
        else if (arg == "--scale" && i + 1 < argc)
            opt.fScale = std::strtof(argv[++i], nullptr);
        else
        {
            std::fprintf(stderr, "unknown mesh arg: %s\n", arg.c_str());
            return 2;
        }
    }

    if (outPath.empty())
    {
        std::fprintf(stderr, "missing -o\n");
        return 2;
    }

    WSkelLoaded ws;
    std::unordered_map<std::string, uint32_t> skelMap;
    if (!skelPath.empty())
    {
        if (!CWSkelLoader::Load(skelPath.c_str(), ws) || !BuildSkelMap(ws, skelMap))
        {
            std::fprintf(stderr, "failed to load wskel\n");
            return 1;
        }
        opt.pSkelNameToIdx = &skelMap;
    }

    Assimp::Importer importer;
    const aiScene* pScene = ReadScene(importer, inPath);
    if (!pScene || (pScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !pScene->mRootNode)
    {
        std::fprintf(stderr, "assimp failed: %s\n", importer.GetErrorString());
        return 1;
    }

    if (!CWMeshWriter::WriteFromAssimp(pScene, outPath.c_str(), opt))
    {
        std::fprintf(stderr, "write failed\n");
        return 1;
    }

    std::filesystem::path matPath(outPath);
    matPath.replace_extension(L".wmat");
    if (!CWMaterialWriter::WriteFromAssimp(pScene, inPath.c_str(), matPath.c_str()))
    {
        std::fprintf(stderr, "material write failed\n");
        return 1;
    }

    std::wprintf(L"wrote %ls\n", outPath.c_str());
    std::wprintf(L"wrote %ls\n", matPath.c_str());
    return 0;
}

static int CmdMaterial(int argc, char** argv)
{
    if (argc < 4)
    {
        std::fprintf(stderr, "usage: material <input> -o <output>\n");
        return 2;
    }

    const std::wstring inPath = Utf8ToWide(argv[1]);
    std::wstring outPath;
    for (int i = 2; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc)
            outPath = Utf8ToWide(argv[++i]);
        else
        {
            std::fprintf(stderr, "unknown material arg: %s\n", arg.c_str());
            return 2;
        }
    }
    if (outPath.empty())
    {
        std::fprintf(stderr, "missing -o\n");
        return 2;
    }

    Assimp::Importer importer;
    const aiScene* pScene = ReadScene(importer, inPath);
    if (!pScene || (pScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !pScene->mRootNode)
    {
        std::fprintf(stderr, "assimp failed: %s\n", importer.GetErrorString());
        return 1;
    }

    if (!CWMaterialWriter::WriteFromAssimp(pScene, inPath.c_str(), outPath.c_str()))
    {
        std::fprintf(stderr, "material write failed\n");
        return 1;
    }

    std::wprintf(L"wrote %ls\n", outPath.c_str());
    return 0;
}

static int CmdSkel(int argc, char** argv)
{
    if (argc < 4)
    {
        std::fprintf(stderr, "usage: skel <input> -o <output>\n");
        return 2;
    }

    const std::wstring inPath = Utf8ToWide(argv[1]);
    std::wstring outPath;
    for (int i = 2; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc)
            outPath = Utf8ToWide(argv[++i]);
        else
        {
            std::fprintf(stderr, "unknown skel arg: %s\n", arg.c_str());
            return 2;
        }
    }
    if (outPath.empty())
    {
        std::fprintf(stderr, "missing -o\n");
        return 2;
    }

    Assimp::Importer importer;
    const aiScene* pScene = ReadScene(importer, inPath);
    if (!pScene || (pScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !pScene->mRootNode)
    {
        std::fprintf(stderr, "assimp failed: %s\n", importer.GetErrorString());
        return 1;
    }

    WSkelWriteResult result;
    if (!CWSkelWriter::WriteFromAssimp(pScene, outPath.c_str(), result))
    {
        std::fprintf(stderr, "write failed\n");
        return 1;
    }

    std::wprintf(L"wrote %ls bones=%zu hash=0x%llx\n",
        outPath.c_str(),
        result.bone_order_by_index.size(),
        static_cast<unsigned long long>(result.skel_hash));
    return 0;
}

static int CmdAnim(int argc, char** argv)
{
    if (argc < 6)
    {
        std::fprintf(stderr, "usage: anim <input> --skel <wskel> -o <output_dir>\n");
        return 2;
    }

    const std::wstring inPath = Utf8ToWide(argv[1]);
    std::wstring skelPath;
    std::wstring outDir;
    for (int i = 2; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--skel" && i + 1 < argc)
            skelPath = Utf8ToWide(argv[++i]);
        else if (arg == "-o" && i + 1 < argc)
            outDir = Utf8ToWide(argv[++i]);
        else
        {
            std::fprintf(stderr, "unknown anim arg: %s\n", arg.c_str());
            return 2;
        }
    }
    if (skelPath.empty() || outDir.empty())
    {
        std::fprintf(stderr, "missing --skel or -o\n");
        return 2;
    }

    WSkelLoaded ws;
    if (!CWSkelLoader::Load(skelPath.c_str(), ws))
    {
        std::fprintf(stderr, "failed to load wskel\n");
        return 1;
    }

    Assimp::Importer importer;
    const aiScene* pScene = ReadScene(importer, inPath);
    if (!pScene || (pScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !pScene->mRootNode)
    {
        std::fprintf(stderr, "assimp failed: %s\n", importer.GetErrorString());
        return 1;
    }

    std::filesystem::create_directories(outDir);
    uint32_t ok = 0;
    for (uint32_t i = 0; i < pScene->mNumAnimations; ++i)
    {
        const aiAnimation* anim = pScene->mAnimations[i];
        const std::wstring stem = SanitizeFileStem(anim->mName.C_Str(), i);
        const std::filesystem::path outPath = std::filesystem::path(outDir) / (stem + L".wanim");
        if (CWAnimWriter::WriteFromAssimp(anim, pScene, ws.skelHash, outPath.c_str()))
        {
            ++ok;
            std::wprintf(L"wrote %ls\n", outPath.c_str());
        }
    }

    std::printf("animations=%u/%u\n", ok, pScene->mNumAnimations);
    return ok == pScene->mNumAnimations ? 0 : 1;
}

static int CmdInfo(int argc, char** argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: info <wmesh|wskel|wanim>\n");
        return 2;
    }

    const std::wstring path = Utf8ToWide(argv[1]);
    std::vector<uint8_t> file;
    if (!CBinaryReader::LoadFileToMemory(path.c_str(), file))
    {
        std::fprintf(stderr, "failed to read file\n");
        return 1;
    }

    try
    {
        CBinaryReader r(file.data(), file.size());
        const auto fh = r.Read<WintersFileHeader>();
        if (std::memcmp(fh.magic, WINTERS_MAGIC, 4) != 0 || fh.content_size > r.Remaining())
        {
            std::fprintf(stderr, "not a winters file\n");
            return 1;
        }

        const char* magic = reinterpret_cast<const char*>(r.Peek());
        if (std::memcmp(magic, WMESH_MAGIC, 4) == 0)
        {
            const auto h = r.Read<MeshMetaHeader>();
            std::printf("[Mesh] submeshes=%u bones=%u vertices=%u indices=%u stride=%u\n",
                h.submesh_count, h.bone_count, h.total_vertex_count, h.total_index_count, h.vertex_stride);
        }
        else if (std::memcmp(magic, WSKEL_MAGIC, 4) == 0)
        {
            WSkelLoaded ws;
            if (!CWSkelLoader::Load(path.c_str(), ws)) return 1;
            std::printf("[Skel] bones=%u sockets=%u hash=0x%llx\n",
                ws.header.bone_count, ws.header.socket_count,
                static_cast<unsigned long long>(ws.skelHash));
            if (ws.header.bone_count > 0)
                std::printf("  bone[0]=%s parent=%d\n", ws.bones[0].name, ws.bones[0].parent_index);
        }
        else if (std::memcmp(magic, WANIM_MAGIC, 4) == 0)
        {
            const auto h = r.Read<AnimMetaHeader>();
            const size_t channelBytes = static_cast<size_t>(h.channel_count) * sizeof(AnimChannel);
            uint64_t skelHash = 0;
            if (channelBytes <= r.Remaining())
            {
                r.Skip(channelBytes);
                if (r.Remaining() >= sizeof(WAnimTrailer))
                {
                    const uint8_t* trailerPos = r.Peek() + r.Remaining() - sizeof(WAnimTrailer);
                    WAnimTrailer trailer{};
                    std::memcpy(&trailer, trailerPos, sizeof(trailer));
                    skelHash = trailer.skel_hash;
                }
            }

            std::printf("[Anim] channels=%u duration_ticks=%.3f ticks=%.3f keys=%u events=%u skel_hash=0x%llx\n",
                h.channel_count, h.duration_ticks, h.ticks_per_second, h.total_key_count, h.event_count,
                static_cast<unsigned long long>(skelHash));
        }
        else if (std::memcmp(magic, WMAT_MAGIC, 4) == 0)
        {
            WMaterialLoaded wm;
            if (!CWMaterialLoader::Load(path.c_str(), wm)) return 1;
            std::printf("[Material] materials=%u\n", wm.header.material_count);
            if (!wm.entries.empty())
            {
                const auto& e = wm.entries[0];
                std::wprintf(L"  material[0]=%S diffuse=%ls\n",
                    e.name,
                    e.diffuse_path);
            }
        }
        else
        {
            std::fprintf(stderr, "unknown payload magic\n");
            return 1;
        }
        return 0;
    }
    catch (...)
    {
        return 1;
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr,
            "usage: WintersAssetConverter <command> [args...]\n"
            "  commands: mesh, material, skel, anim, info\n");
        return 2;
    }

    const std::string cmd = argv[1];
    if (cmd == "mesh") return CmdMesh(argc - 1, argv + 1);
    if (cmd == "material") return CmdMaterial(argc - 1, argv + 1);
    if (cmd == "skel") return CmdSkel(argc - 1, argv + 1);
    if (cmd == "anim") return CmdAnim(argc - 1, argv + 1);
    if (cmd == "info") return CmdInfo(argc - 1, argv + 1);

    std::fprintf(stderr, "unknown command: %s\n", cmd.c_str());
    return 2;
}
