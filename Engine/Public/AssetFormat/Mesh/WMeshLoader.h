#pragma once
#include "WintersAPI.h"
#include "AssetFormat/Mesh/WMeshFormat.h"
#include <memory>
#include <string>
#include <vector>

namespace Winters::Asset
{
    // 로드 결과 — CMesh 분해 전의 원시 구조. 렌더러/ResourceCache 가 흡수.
    struct WMeshLoaded
    {
        MeshMetaHeader             header;
        std::vector<SubMeshDesc>   subMeshes;
        std::vector<BoneEntry>     bones;           // bone_count==0 이면 비어있음
        std::vector<SubMeshBounds> bounds;          // has_bounding==0 이면 비어있음

        // zero-copy 버퍼 — 원본 파일 메모리의 포인터. 소유권은 m_vRawFile.
        const uint8_t* pVertexBlob = nullptr;
        size_t         uVertexBlobBytes = 0;
        const uint8_t* pIndexBlob = nullptr;
        size_t         uIndexBlobBytes = 0;

        std::vector<uint8_t> m_vRawFile;   // mmap 수명 잡는 용. 로드 후 유지 필요.
    };

    class WINTERS_ENGINE CWMeshLoader
    {
    public:
        // 디스크 경로 → WMeshLoaded (zero-copy — m_vRawFile 이 blob 수명 소유).
        static bool Load(const wchar_t* pPath, WMeshLoaded& out);

        // 메모리 버퍼 직행 (번들 대비).
        static bool LoadFromMemory(const void* pData, size_t uSize, WMeshLoaded& out);
    };
}
