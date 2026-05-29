# Stage 7 — `.winters` 번들 (패키징 + TOC + mmap + 서명)

> **목표**: 개별 `.wmesh` / `.wanim` / `.wtex` / `.wmat` / `.wmap` 을 **단일 `.winters` 번들** 로 묶어서 배포.
> Valve `.vpk` + Unreal `.pak` 혼합 아키텍처. 런타임은 `CreateFileMapping` + `MapViewOfFile` 로 zero-copy 로드.

---

## 1. 왜 번들

| 동기 | 이점 |
|---|---|
| 파일 수 감소 | 배포/업데이트 관리 용이. `.exe` + `Content.winters` + `Shaders.winters` 정도만 |
| I/O 지연 감소 | 수천 개 파일 오픈/close vs mmap 1회 |
| 서명 단위화 | Ed25519 로 번들 전체 1 서명 → 에셋 개별 서명보다 효율 |
| 중복 제거 | 같은 텍스처/메시 해시면 1 저장소만 |
| 압축 효율 | 번들 전체 LZ4 (또는 개별 LZ4 선택) |

---

## 2. 전체 레이아웃

```
[ BundleHeader 128B ]
    magic[8] = "WINTPACK"
    version_major: uint32
    version_minor: uint32
    toc_offset:    uint64       ← TOC 위치 (파일 끝 쪽)
    toc_count:     uint32
    data_offset:   uint64       ← 에셋 바이트 시작
    content_flags: uint32       ← 기본 flags (ALL_LZ4 / ALL_SIGNED)
    created_unix:  uint64       ← 빌드 시각
    publisher_id:  uint32
    reserved[16]: uint32

[ Data Blocks ]
    Asset 0 raw bytes (.wmesh file, 헤더 포함)
    Asset 1 raw bytes
    ...
    Asset N raw bytes
    (각 에셋은 BUNDLE_ALIGN=64B 경계로 padding)

[ TOCEntry × toc_count ]
    name_hash:        uint64      ← FNV-1a of "Characters/Sylas/body.wmesh"
    name_offset:      uint32      ← 문자열 테이블 내 offset (디버그용)
    asset_type:       uint16      ← enum: Mesh/Anim/Skel/Tex/Mat/Map/Scene/FX
    flags:            uint16
    block_offset:     uint64      ← 번들 내 바이트 위치
    size_compressed:  uint64
    size_uncompressed:uint64
    sha256:           uint8[32]   ← 개별 에셋 해시 (중복 — 빠른 검증용)

[ String Table ]  ← 디버그/툴링 전용 (Release 생략 가능)
    null-terminated name strings

[ Trailing Signature (선택) ]
    ed25519_signature[64]         ← Stage 9
```

### 2.1 POD

```cpp
// Engine/Public/AssetFormat/Bundle/WBundleFormat.h
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include <cstdint>

namespace Winters::Asset
{
    constexpr char WBUNDLE_MAGIC[8] = { 'W','I','N','T','P','A','C','K' };

    enum eBundleFlags : uint32_t
    {
        BUNDLE_FLAG_ALL_LZ4      = 1u << 0,   // 모든 에셋 LZ4 강제
        BUNDLE_FLAG_SIGNED       = 1u << 1,   // Ed25519 trailer
        BUNDLE_FLAG_ENCRYPTED    = 1u << 2,   // Stage 9
        BUNDLE_FLAG_NO_STRINGS   = 1u << 3,   // Release — String Table 제거
    };

    enum eAssetType : uint16_t
    {
        ASSET_Mesh      = 1,
        ASSET_Anim      = 2,
        ASSET_Skel      = 3,
        ASSET_Texture   = 4,
        ASSET_Material  = 5,
        ASSET_Map       = 6,
        ASSET_Scene     = 7,
        ASSET_FX        = 8,   // .wfx — Effect Tool
        ASSET_Raw       = 100, // 기타 (hlsl compiled cso, sound wav, json)
    };

    constexpr uint32_t BUNDLE_ALIGN = 64;

    #pragma pack(push, 1)
    struct BundleHeader
    {
        char     magic[8];               // "WINTPACK"
        uint32_t version_major;
        uint32_t version_minor;
        uint64_t toc_offset;
        uint32_t toc_count;
        uint32_t content_flags;          // eBundleFlags
        uint64_t data_offset;
        uint64_t created_unix;
        uint32_t publisher_id;
        uint32_t reserved[16];
    };
    static_assert(sizeof(BundleHeader) == 128);

    struct TOCEntry
    {
        uint64_t name_hash;
        uint32_t name_offset;            // string table
        uint16_t asset_type;             // eAssetType
        uint16_t flags;
        uint64_t block_offset;
        uint64_t size_compressed;
        uint64_t size_uncompressed;
        uint8_t  sha256[32];
    };
    static_assert(sizeof(TOCEntry) == 72);
    #pragma pack(pop)
}
```

---

## 3. 빌더 (컨버터 시점)

```cpp
// Engine/Public/AssetFormat/Bundle/WBundleBuilder.h
namespace Winters::Asset
{
    struct BundleInput
    {
        std::wstring sourceDir;          // "Bin/Resource/Content/"
        std::vector<std::wstring> explicitFiles;
        bool_t       bRecursive   = true;
        uint32_t     flags        = BUNDLE_FLAG_ALL_LZ4;
        uint32_t     publisherId  = 0;
    };

    class WINTERS_API CWBundleBuilder
    {
    public:
        static HRESULT Build(const BundleInput& in, const wchar_t* pOutPath);
    };
}
```

### 3.1 빌드 알고리즘

```cpp
// Engine/Private/AssetFormat/Bundle/WBundleBuilder.cpp
HRESULT CWBundleBuilder::Build(const BundleInput& in, const wchar_t* pOutPath)
{
    // 1. 파일 수집
    std::vector<std::filesystem::path> files;
    if (in.bRecursive) {
        for (auto& p : std::filesystem::recursive_directory_iterator(in.sourceDir))
            if (IsWintersAsset(p.path())) files.push_back(p);
    } else {
        for (auto& f : in.explicitFiles) files.push_back(f);
    }

    // 2. 각 파일 읽기 + 해시 + TOC 엔트리 생성
    std::vector<TOCEntry> toc;
    std::vector<uint8_t> data;    // 전체 에셋 바이트 concat
    std::string           strings;

    for (const auto& path : files) {
        // 에셋 원본 읽기 (이미 .w* 포맷 상태)
        auto bytes = ReadFileAll(path);

        // 정렬 패딩
        Align(data, BUNDLE_ALIGN);

        TOCEntry e{};
        const auto rel = std::filesystem::relative(path, in.sourceDir).generic_string();
        e.name_hash         = FNV1a(rel);
        e.name_offset       = (uint32_t)strings.size();
        e.asset_type        = DetectAssetType(path);
        e.flags             = 0;
        e.block_offset      = data.size();
        e.size_uncompressed = bytes.size();

        // 개별 에셋은 이미 내부 SHA256 가짐 — TOC 에도 중복 저장해 빠른 조회
        auto sha = CSHA256::Hash(bytes.data(), bytes.size());
        std::memcpy(e.sha256, sha.data(), 32);

        // 번들 레벨 압축 (flags.ALL_LZ4). 에셋이 이미 .wtex(BC7) 면 추가 압축 이득 적음 → skip
        std::vector<uint8_t> maybeCompressed;
        if ((in.flags & BUNDLE_FLAG_ALL_LZ4) && ShouldCompress(e.asset_type)) {
            maybeCompressed = LZ4_Compress(bytes);
            e.flags |= 0x01;                      // bit 0 = compressed
            e.size_compressed = maybeCompressed.size();
            data.insert(data.end(), maybeCompressed.begin(), maybeCompressed.end());
        } else {
            e.size_compressed = bytes.size();
            data.insert(data.end(), bytes.begin(), bytes.end());
        }

        strings.append(rel); strings.push_back('\0');
        toc.push_back(e);
    }

    // 3. 헤더 작성
    BundleHeader hdr{};
    std::memcpy(hdr.magic, WBUNDLE_MAGIC, 8);
    hdr.version_major = 1;
    hdr.version_minor = 0;
    hdr.toc_count     = (uint32_t)toc.size();
    hdr.content_flags = in.flags;
    hdr.publisher_id  = in.publisherId;
    hdr.created_unix  = (uint64_t)std::time(nullptr);
    hdr.data_offset   = sizeof(BundleHeader);
    hdr.toc_offset    = hdr.data_offset + data.size();

    // 4. 디스크 기록
    HANDLE hFile = CreateFileW(pOutPath, GENERIC_WRITE, 0, nullptr,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    DWORD written = 0;
    WriteFile(hFile, &hdr, sizeof(hdr), &written, nullptr);
    WriteFile(hFile, data.data(), (DWORD)data.size(), &written, nullptr);
    WriteFile(hFile, toc.data(), (DWORD)(toc.size() * sizeof(TOCEntry)), &written, nullptr);

    if (!(in.flags & BUNDLE_FLAG_NO_STRINGS))
        WriteFile(hFile, strings.data(), (DWORD)strings.size(), &written, nullptr);

    // 5. (Stage 9) Ed25519 서명 trailer
    if (in.flags & BUNDLE_FLAG_SIGNED) {
        // Stage 9 에서 구현
    }

    CloseHandle(hFile);
    return S_OK;
}
```

### 3.2 압축 판단 (`ShouldCompress`)

| 에셋 타입 | LZ4 | 이유 |
|---|---|---|
| `.wmesh` | Yes | 정점 압축률 50~70% |
| `.wanim` | Yes | 키프레임 반복 많음 |
| `.wskel` | Yes | 본 이름/행렬 반복 |
| `.wtex` (BC7) | **No** | 이미 블록 압축 — LZ4 추가 이득 ≈ 0 |
| `.wmat` | Yes | 작지만 JSON-like 반복 |
| `.wmap` | Yes | NavGrid 비트맵 매우 효과적 |
| `.cso` (compiled shader) | Yes | 바이트코드 압축 가능 |

---

## 4. 런타임 로더 (mmap)

### 4.1 API

```cpp
// Engine/Public/AssetFormat/Bundle/WBundleLoader.h
namespace Winters::Asset
{
    class WINTERS_API CWBundleLoader
    {
    public:
        static std::unique_ptr<CWBundleLoader> Open(const std::wstring& path);
        ~CWBundleLoader();

        // 이름 해시로 조회 — 해시 일치 엔트리 반환
        const TOCEntry* Find(uint64_t nameHash) const;

        // 에셋 바이트 추출 — 압축 해제 후 결과 버퍼 반환
        HRESULT Extract(uint64_t nameHash, std::vector<uint8_t>& out) const;

        // mmap 포인터 직접 — 비압축 에셋만 (압축이면 E_FAIL)
        const uint8_t* PeekRaw(uint64_t nameHash, size_t& outSize) const;

        // 번들 전체 서명 검증 (Stage 9)
        bool_t VerifySignature(const uint8_t* pPubKey) const;

        uint32_t TOCCount() const { return m_hdr.toc_count; }
        const TOCEntry& TOC(uint32_t i) const { return m_toc[i]; }

    private:
        CWBundleLoader() = default;

        HANDLE       m_hFile      = INVALID_HANDLE_VALUE;
        HANDLE       m_hMapping   = nullptr;
        const uint8_t* m_pBase    = nullptr;
        size_t       m_uFileSize  = 0;

        BundleHeader             m_hdr{};
        std::vector<TOCEntry>    m_toc;
        std::unordered_map<uint64_t, uint32_t> m_index;   // hash → toc idx
    };
}
```

### 4.2 구현 — mmap

```cpp
std::unique_ptr<CWBundleLoader> CWBundleLoader::Open(const std::wstring& path)
{
    auto loader = std::unique_ptr<CWBundleLoader>(new CWBundleLoader());

    loader->m_hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (loader->m_hFile == INVALID_HANDLE_VALUE) return nullptr;

    LARGE_INTEGER sz{};
    GetFileSizeEx(loader->m_hFile, &sz);
    loader->m_uFileSize = (size_t)sz.QuadPart;

    loader->m_hMapping = CreateFileMappingW(loader->m_hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!loader->m_hMapping) return nullptr;

    loader->m_pBase = (const uint8_t*)MapViewOfFile(loader->m_hMapping,
                                                     FILE_MAP_READ, 0, 0, 0);
    if (!loader->m_pBase) return nullptr;

    // 1. 헤더
    std::memcpy(&loader->m_hdr, loader->m_pBase, sizeof(BundleHeader));
    if (std::memcmp(loader->m_hdr.magic, WBUNDLE_MAGIC, 8) != 0) return nullptr;

    // 2. TOC
    loader->m_toc.resize(loader->m_hdr.toc_count);
    std::memcpy(loader->m_toc.data(),
                loader->m_pBase + loader->m_hdr.toc_offset,
                sizeof(TOCEntry) * loader->m_hdr.toc_count);

    // 3. hash → idx 맵 빌드
    for (uint32_t i = 0; i < loader->m_hdr.toc_count; ++i)
        loader->m_index[loader->m_toc[i].name_hash] = i;

    return loader;
}

HRESULT CWBundleLoader::Extract(uint64_t nameHash, std::vector<uint8_t>& out) const
{
    auto it = m_index.find(nameHash);
    if (it == m_index.end()) return E_WINTERS_ASSET_NOT_FOUND;

    const TOCEntry& e = m_toc[it->second];
    const uint8_t* p = m_pBase + e.block_offset;

    // 범위 검증
    if (e.block_offset + e.size_compressed > m_uFileSize) return E_WINTERS_INVALID_TOC;

    // 압축 해제
    if (e.flags & 0x01) {
        out.resize(e.size_uncompressed);
        if (!LZ4_Decompress(p, e.size_compressed, out.data(), e.size_uncompressed))
            return E_WINTERS_DECOMPRESS_FAILED;
    } else {
        out.assign(p, p + e.size_uncompressed);
    }

    // 개별 해시 검증
    auto sha = CSHA256::Hash(out.data(), out.size());
    if (std::memcmp(sha.data(), e.sha256, 32) != 0)
        return E_WINTERS_HASH_MISMATCH;

    return S_OK;
}

const uint8_t* CWBundleLoader::PeekRaw(uint64_t nameHash, size_t& outSize) const
{
    auto it = m_index.find(nameHash);
    if (it == m_index.end()) return nullptr;
    const TOCEntry& e = m_toc[it->second];
    if (e.flags & 0x01) return nullptr;  // 압축된 건 Extract() 로
    outSize = e.size_uncompressed;
    return m_pBase + e.block_offset;
}
```

**zero-copy 핵심**: `PeekRaw` 는 mmap 포인터 그대로. `.wtex` (BC7 비압축) / `.wmesh` (비압축 옵션) → DX11 `CreateTexture2D` / `CreateBuffer` 에 바로 꽂음.

---

## 5. ResourceCache 통합

```cpp
// Engine/Public/Resource/ResourceCache.h
class CResourceCache
{
public:
    std::shared_ptr<CMesh>    LoadMesh(const std::wstring& path);

private:
    CWBundleLoader* m_pBundle = nullptr;   // GameInstance 보유
};

std::shared_ptr<CMesh> CResourceCache::LoadMesh(const std::wstring& path)
{
    // 1. 캐시 hit
    if (auto sp = m_meshes[path].lock()) return sp;

    // 2. 번들 우선
    if (m_pBundle) {
        uint64_t hash = FNV1aW(path);
        size_t size = 0;
        const uint8_t* p = m_pBundle->PeekRaw(hash, size);
        if (p) {
            auto mesh = CWMeshLoader::LoadFromMemory(m_pDevice, p, size);
            m_meshes[path] = mesh;
            return mesh;
        }
        // 압축된 경우 Extract
        std::vector<uint8_t> buf;
        if (SUCCEEDED(m_pBundle->Extract(hash, buf))) {
            auto mesh = CWMeshLoader::LoadFromMemory(m_pDevice, buf.data(), buf.size());
            m_meshes[path] = mesh;
            return mesh;
        }
    }

    // 3. 디스크 루즈 파일 폴백 (개발)
    auto mesh = CWMeshLoader::Load(m_pDevice, path);
    m_meshes[path] = mesh;
    return mesh;
}
```

---

## 6. 번들 조합 전략

### 6.1 단일 번들 vs 분할

| 전략 | 언제 |
|---|---|
| 단일 `Content.winters` | 총 크기 < 2 GB, 로딩 한 번에 |
| 분할 (챔피언별) | `Champ_Irelia.winters`, `Champ_Yasuo.winters` — 매치 시 필요 챔프만 로드 |
| Base + DLC | `Base.winters` (공통) + `Skins_Latest.winters` (시즌 스킨) |

### 6.2 Winters 초기 권장

```
Bin/
├── Content.winters          ← 맵 + 셰이더 + UI + 공통
├── Champions.winters        ← 5 챔피언 메시 + 애니 + 텍스처
├── SFX.winters              ← FMOD 보조 (wav 대신 미리 디코드)
└── Content_Dev.winters      ← 개발 전용 (디버그 에셋 — Release 제외)
```

Phase 4 (네트워크) 이후 매치 시 서버가 `Champions_Needed` 리스트 → 해당 번들만 streaming.

---

## 7. 번들 덤프 도구

디버깅용 CLI:

```
WintersAssetConverter.exe bundle-info Content.winters
  → Header:
      magic: WINTPACK, version: 1.0
      toc_count: 234
      data_offset: 128
      toc_offset: 23450231
      flags: ALL_LZ4
  → TOC Summary:
      Mesh:     48 entries, total 28 MB
      Anim:    312 entries, total 14 MB
      Skel:      5 entries, total  0.4 MB
      Texture: 180 entries, total 210 MB
      Material: 62 entries, total  0.2 MB
      Map:       2 entries, total  1.1 MB

WintersAssetConverter.exe bundle-ls Content.winters
  → Characters/Irelia/body.wmesh         (mesh, 524 KB → 312 KB LZ4)
  → Characters/Irelia/skeleton.wskel     (skel,  42 KB)
  → ...

WintersAssetConverter.exe bundle-extract Content.winters \
    "Characters/Irelia/body.wmesh" -o body.wmesh

WintersAssetConverter.exe bundle-verify Content.winters
  → All 234 entries verified (SHA256 match)
  → Bundle signature: VALID (Ed25519)
```

---

## 8. 보안 고려사항

| 위협 | 방어 |
|---|---|
| 번들 내부 에셋 1개 조작 | TOC 의 개별 `sha256` 검증 + 전체 번들 서명 |
| 번들 자체 교체 (악의 번들) | Stage 9 Ed25519 서명 — 서버 공개키로 검증 |
| 파일 크기 헤더 조작 (OOM) | `data_offset` / `toc_offset` / `block_offset + size` 모두 `<= file_size` 검증 |
| 순환 참조 (TOC 가 자기 자신 가리킴) | `toc_offset >= data_offset` 검증 |
| 거대 TOC count (수억) | `toc_count * sizeof(TOCEntry) <= file_size - toc_offset` 검증 |
| 해시 충돌 공격 (치트 파일과 같은 FNV-1a) | 의도된 한계. 번들 빌드 시 충돌 감지 + 경고 + 다른 해시 알고리즘 (XXH3) 고려 |

### 8.1 Validator

```cpp
HRESULT ValidateBundleHeader(const BundleHeader& h, size_t fileSize)
{
    if (std::memcmp(h.magic, WBUNDLE_MAGIC, 8) != 0) return E_WINTERS_INVALID_MAGIC;
    if (h.version_major != 1)                       return E_WINTERS_VERSION_MISMATCH;
    if (h.toc_offset > fileSize)                     return E_WINTERS_INVALID_TOC;
    if (h.data_offset < sizeof(BundleHeader))        return E_WINTERS_INVALID_TOC;
    if (h.toc_offset < h.data_offset)                return E_WINTERS_INVALID_TOC;

    size_t tocSize = (size_t)h.toc_count * sizeof(TOCEntry);
    if (h.toc_offset + tocSize > fileSize)          return E_WINTERS_INVALID_TOC;

    return S_OK;
}
```

---

## 9. 성능 측정

| 작업 | 목표 |
|---|---|
| 번들 mmap (1 GB) | < 10 ms (page fault 는 접근 시 지연) |
| TOC 로드 (1000 엔트리) | < 0.5 ms |
| 첫 에셋 조회 + 언압축 (500 KB .wmesh) | < 1 ms |
| 챔피언 1체 풀 로드 (메시+스켈+애니 80 개+텍스처 8 개) | **< 50 ms** |
| 맵 1개 (Stage1.wmap + 모든 구조물 메시) | **< 300 ms** |

---

## 10. 번들 크기 예상

LoL 모작 기준:
- 챔피언 5체 (mesh+anim+tex): ~300 MB
- 맵 + 오브젝트: ~200 MB
- UI / 폰트 / 효과음 사전디코드: ~50 MB
- 셰이더 (compiled cso): ~20 MB
- **Content.winters**: ~570 MB (LZ4 적용)

Phase 6~8 시점 모든 콘텐츠: **~1.2 GB 예상**.

---

## 11. 완료 기준

- [ ] `WBundleFormat.h` POD + static_assert
- [ ] `WBundleBuilder.cpp` 디렉토리 수집 + LZ4 + SHA256 + 기록
- [ ] `WBundleLoader.cpp` mmap + TOC 인덱싱 + Extract/PeekRaw
- [ ] ResourceCache 번들 통합 (`CWBundleLoader*` 멤버)
- [ ] GameInstance `OpenBundle` / `Get_Bundle` API
- [ ] `bundle-info` / `bundle-ls` / `bundle-extract` / `bundle-verify` CLI
- [ ] 챔피언 5체 번들 빌드 + 로드 + 로드 시간 측정
- [ ] Validator 위협 5건 테스트
- [ ] Release 빌드 번들 필수 강제 (Loose file 거부)

---

## 12. 다음 단계

Stage 8 (무결성) 으로 이동 — Ed25519 서명 + 서버 키 발급 + 치트 신고 연계.
