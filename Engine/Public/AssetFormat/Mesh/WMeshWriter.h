#pragma once
#include "AssetFormat/Mesh/WMeshFormat.h"
#include <string>
#include <unordered_map>
#include <unordered_set>

// Assimp 전방선언 (Engine.dll 은 Assimp 의존 없음 — Tools 프로젝트에서만 링크)
struct aiScene;

namespace Winters::Asset
{
    struct WMeshWriteOptions
    {
        bool  bWriteBounds = true;
        bool  bMirrorX     = false;
        bool  bIncludeLayerOverlays = false;
        float fScale       = 1.f;

        const std::unordered_map<std::string, uint32_t>* pSkelNameToIdx = nullptr;
        const std::unordered_set<std::string>* pExcludedMaterialNames = nullptr;
    };

    class CWMeshWriter
    {
    public:
        // Assimp 로 이미 로드된 aiScene → .wmesh 저장.
        static bool WriteFromAssimp(const aiScene* pScene,
                                     const wchar_t* pOutPath,
                                     const WMeshWriteOptions& opt = {});
    };
}
