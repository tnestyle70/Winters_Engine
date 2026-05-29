# Winters Format — 아키텍처 (선행 Stage 1)

> **목적**: 이후 모든 Stage (2~12) 가 공유하는 **공통 유틸** + **디렉토리 구조** + **ECS/GameInstance 경계** 확정.
> 디스크 포맷을 짜기 전에 BinaryReader/Writer, `CWintersFile`, ResourceCache 연계 규칙을 먼저 확정해야 각 Stage 파일이 일관된다.

---

## 1. 왜 먼저인가

| 이유 | 영향 Stage |
|---|---|
| BinaryReader/Writer 가 모든 포맷 입출력의 기반 | 2~8 전부 |
| `CWintersFile` 가 Header + Payload + SHA256 통합 로더 | 2~8 전부 |
| 디렉토리 (`Engine/Public/AssetFormat/`) 확정 후 각 Stage 헤더 배치 | 3~8 전부 |
| ResourceCache 통합 규약 미리 정해야 메시/텍스처/사운드 일관 | 3, 5, 6 |
| GameInstance Tier 경계 (Tier1 로드 요청 / Tier2 바이트 직접 접근) | 3, 8 |

---

## 2. 디렉토리 구조

```
Engine/Public/AssetFormat/
├── Core/
│   ├── WintersFileHeader.h       ← Stage 2 (공통 헤더 POD)
│   ├── WintersFile.h             ← 헤더+페이로드+SHA256 통합 로더
│   ├── BinaryReader.h            ← 읽기 유틸 (mmap zero-copy 지원)
│   ├── BinaryWriter.h            ← 쓰기 유틸 (SaveToFile 자동 SHA256)
│   ├── FNVHash.h                 ← 이름 → uint64 (TOC/본 이름 해시)
│   └── FormatErrors.h            ← E_WINTERS_* HRESULT enum
├── Mesh/
│   ├── WMeshFormat.h             ← Stage 3 (.wmesh 구조체 POD)
│   ├── WMeshWriter.h             ← Assimp scene → .wmesh (컨버터 측)
│   └── WMeshLoader.h             ← .wmesh → CMesh (런타임 측)
├── Anim/
│   ├── WAnimFormat.h             ← Stage 4 .wanim POD
│   ├── WSkelFormat.h             ← Stage 4 .wskel POD
│   ├── WAnimWriter.h / Loader.h
│   └── WSkelWriter.h / Loader.h
├── Texture/
│   ├── WTexFormat.h              ← Stage 5 (.wtex + BC 포맷 enum)
│   ├── WTexWriter.h              ← DirectXTex → BC7 인코딩
│   └── WTexLoader.h              ← GPU 업로드 직전까지
├── Material/
│   ├── WMatFormat.h              ← Stage 6 .wmat
│   ├── WMatWriter.h / Loader.h
├── Map/
│   ├── WMapFormat.h              ← Stage 7 .wmap (Stage1.dat 승격)
│   ├── WMapWriter.h / Loader.h
├── Scene/
│   ├── WSceneFormat.h            ← (선택) ECS 씬 스냅샷
│   ├── WSceneWriter.h / Loader.h
├── Bundle/
│   ├── WBundleFormat.h           ← Stage 8 .winters TOC
│   ├── WBundleBuilder.h          ← 빌드 시 에셋 묶기
│   └── WBundleLoader.h           ← 런타임 mmap 로더
├── Integrity/
│   ├── Ed25519.h                 ← Stage 9 서명 래퍼
│   ├── AESGCM.h                  ← (선택) 암호화 래퍼
│   └── TamperDetector.h          ← 해시 불일치 시 서버 신고
├── Versioning/
│   ├── VersionMigrator.h         ← Major/Minor 마이그레이션 런타임
│   └── LegacyReaders.h           ← 구버전 호환 어댑터
└── Editor/
    ├── WintersAssetViewer.h      ← Stage 12 ImGui 에셋 뷰어
    └── HashMismatchPanel.h       ← Stage 12 무결성 패널
```

### Engine.vcxproj.filters 섹션 추가

```
15. AssetFormat    Core, Mesh, Anim, Texture, Material, Map, Scene, Bundle,
                   Integrity, Versioning, Editor
```

`14. FX` 다음 슬롯. Phase G (Effect Tool) 와 독립.

### 공개 헤더 경로 (Engine/Include/ flat)

CLAUDE.md Gotcha "EngineSDK/inc 는 flat 구조" 준수. 공개 헤더는 파일명만으로 해결:

| Engine/Public 위치 | EngineSDK flat 배포 | Client 에서 include |
|---|---|---|
| `AssetFormat/Core/WintersFile.h` | `WintersFile.h` | `#include "WintersFile.h"` |
| `AssetFormat/Mesh/WMeshFormat.h` | `WMeshFormat.h` | `#include "WMeshFormat.h"` |
| `AssetFormat/Bundle/WBundleLoader.h` | `WBundleLoader.h` | `#include "WBundleLoader.h"` |

Engine.vcxproj `AdditionalIncludeDirectories` 에 서브폴더 추가:
```
$(SolutionDir)Engine\Public\AssetFormat\Core;
$(SolutionDir)Engine\Public\AssetFormat\Mesh;
$(SolutionDir)Engine\Public\AssetFormat\Anim;
$(SolutionDir)Engine\Public\AssetFormat\Texture;
$(SolutionDir)Engine\Public\AssetFormat\Material;
$(SolutionDir)Engine\Public\AssetFormat\Map;
$(SolutionDir)Engine\Public\AssetFormat\Bundle;
$(SolutionDir)Engine\Public\AssetFormat\Integrity;
$(SolutionDir)Engine\Public\AssetFormat\Versioning;
```

---

## 3. 핵심 유틸리티

### 3.1 BinaryReader

```cpp
// Engine/Public/AssetFormat/Core/BinaryReader.h
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include <cstring>
#include <type_traits>

namespace Winters::Asset
{
    // 원시 바이트 + 커서 기반. 소유권 없음 (mmap 포인터 수용).
    class WINTERS_API CBinaryReader
    {
    public:
        CBinaryReader(const void* pData, size_t uSize)
            : m_pData(static_cast<const uint8_t*>(pData))
            , m_uCursor(0)
            , m_uSize(uSize)
        {}

        // POD 한 개 복사 읽기
        template<typename T>
        T Read()
        {
            static_assert(std::is_trivially_copyable_v<T>, "BinaryReader: T must be POD");
            T v{};
            std::memcpy(&v, m_pData + m_uCursor, sizeof(T));
            m_uCursor += sizeof(T);
            return v;
        }

        // 원시 블록 복사 (vertex/index data)
        void ReadBytes(void* pDst, size_t n)
        {
            std::memcpy(pDst, m_pData + m_uCursor, n);
            m_uCursor += n;
        }

        // mmap zero-copy — GPU 업로드 시 포인터만 반환
        const uint8_t* Peek() const { return m_pData + m_uCursor; }

        // 커서 이동
        void Skip(size_t n)     { m_uCursor += n; }
        void Seek(size_t pos)   { m_uCursor = pos; }
        size_t Tell() const     { return m_uCursor; }

        bool_t AtEnd() const    { return m_uCursor >= m_uSize; }
        size_t Remaining() const{ return (m_uCursor < m_uSize) ? (m_uSize - m_uCursor) : 0; }

        // 범위 체크 (보안 — 악의적 파일 방어)
        bool_t CanRead(size_t n) const { return m_uCursor + n <= m_uSize; }

    private:
        const uint8_t* m_pData;
        size_t         m_uCursor;
        size_t         m_uSize;
    };
}
```

**설계 의도**:
- `const uint8_t*` + size 만 보관 → mmap 바이트 직접 참조 가능 (zero-copy)
- `Peek()` 로 GPU 업로드 시 `D3D11_SUBRESOURCE_DATA.pSysMem` 에 바로 꽂음
- `CanRead()` 는 보안용 — Stage 8 번들 로더가 TOC offset/size 를 신뢰하지 못할 때 사용

### 3.2 BinaryWriter

```cpp
// Engine/Public/AssetFormat/Core/BinaryWriter.h
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include <vector>
#include <cstring>
#include <type_traits>

namespace Winters::Asset
{
    class WINTERS_API CBinaryWriter
    {
    public:
        CBinaryWriter() = default;
        explicit CBinaryWriter(size_t uReserve) { m_vBuffer.reserve(uReserve); }

        template<typename T>
        void Write(const T& v)
        {
            static_assert(std::is_trivially_copyable_v<T>, "BinaryWriter: T must be POD");
            const auto oldSize = m_vBuffer.size();
            m_vBuffer.resize(oldSize + sizeof(T));
            std::memcpy(m_vBuffer.data() + oldSize, &v, sizeof(T));
        }

        void WriteBytes(const void* pSrc, size_t n)
        {
            const auto oldSize = m_vBuffer.size();
            m_vBuffer.resize(oldSize + n);
            std::memcpy(m_vBuffer.data() + oldSize, pSrc, n);
        }

        // 정렬 맞추기 (cbuffer 등 16B 정렬 필요 시)
        void AlignTo(size_t uAlign)
        {
            const size_t pad = (uAlign - (m_vBuffer.size() % uAlign)) % uAlign;
            m_vBuffer.resize(m_vBuffer.size() + pad, 0);
        }

        const uint8_t* Data() const { return m_vBuffer.data(); }
        size_t         Size() const { return m_vBuffer.size(); }

        // Winters 공통 헤더 (WINT magic) + 페이로드 + SHA256 을 통째로 저장.
        // flags: WINTERS_FLAG_LZ4 | WINTERS_FLAG_ENCRYPTED | WINTERS_FLAG_SIGNED
        HRESULT SaveToFile(const wchar_t* pPath, uint32_t flags = 0,
                           uint16_t verMajor = 1, uint16_t verMinor = 0) const;

    private:
        std::vector<uint8_t> m_vBuffer;
    };
}
```

### 3.3 WintersFile

```cpp
// Engine/Public/AssetFormat/Core/WintersFile.h
#pragma once
#include "WintersAPI.h"
#include "WintersFileHeader.h"
#include <vector>
#include <memory>

namespace Winters::Asset
{
    class WINTERS_API CWintersFile
    {
    public:
        static HRESULT LoadFromDisk(const wchar_t* pPath, CWintersFile& out);

        // 메모리 버퍼에서 직접 로드 (번들 내부 파일)
        static HRESULT LoadFromMemory(const void* pData, size_t uSize, CWintersFile& out);

        // SHA256 검증 — flags.signed 면 Ed25519 까지
        bool_t VerifyIntegrity() const;
        bool_t VerifySHA256() const;

        const WintersFileHeader& Header() const   { return m_header; }
        const uint8_t*           Payload() const  { return m_vPayload.data(); }
        size_t                   PayloadSize() const { return m_vPayload.size(); }

        // LZ4 압축 해제 결과까지 포함한 "실제 사용 가능한 바이트"
        const uint8_t*           Decompressed() const { return m_vDecompressed.data(); }
        size_t                   DecompressedSize() const { return m_vDecompressed.size(); }

    private:
        CWintersFile() = default;

        WintersFileHeader     m_header{};
        std::vector<uint8_t>  m_vPayload;       // 디스크 원본 (압축/암호 상태)
        std::vector<uint8_t>  m_vDecompressed;  // 디코드 후 (사용자용)
        uint8_t               m_sha256[32]{};
    };
}
```

---

## 4. ResourceCache 통합

### 4.1 현재 ResourceCache

```cpp
// Engine/Public/Resource/ResourceCache.h
class CResourceCache
{
    std::unordered_map<std::wstring, std::weak_ptr<CTexture>>  m_textures;
    std::unordered_map<std::wstring, std::weak_ptr<CMesh>>     m_meshes;
    // ...
public:
    std::shared_ptr<CTexture> LoadTexture(const std::wstring& path);
    std::shared_ptr<CMesh>    LoadMesh(const std::wstring& path);
};
```

### 4.2 확장 — 포맷 자동 디스패치

```cpp
std::shared_ptr<CMesh> CResourceCache::LoadMesh(const std::wstring& path)
{
    // 1. 캐시 조회
    if (auto it = m_meshes.find(path); it != m_meshes.end()) {
        if (auto sp = it->second.lock()) return sp;
    }

    // 2. 확장자 / 매직 기반 디스패치
    const auto ext = GetExt(path);  // L".wmesh" / L".fbx" / L".glb"

    std::shared_ptr<CMesh> mesh;
    if (ext == L".wmesh") {
        mesh = Winters::Asset::CMeshLoader_Winters::Load(path);    // 새 파이프
    } else {
        mesh = CMeshLoader_Assimp::Load(path);                     // 레거시 fallback
    }

    m_meshes[path] = mesh;
    return mesh;
}
```

**정책**: `.wmesh` 가 있으면 우선. 없으면 원본 FBX/glb. Phase 초반은 이중 운영 → 최종적으로 모든 에셋 `.wmesh` 로 전환 후 Assimp 런타임 경로 제거.

### 4.3 경로 해석

`WintersResolveContentPath` (CLAUDE.md Gotcha 참조) 는 파일 전용. `.wmesh` / `.wanim` 도 동일 해석:

```cpp
const auto full = Winters::Path::ResolveContent(L"Characters/Sylas/body.wmesh");
// 탐색 순서: exe/Resource → Client/Bin/Resource → Engine/Assets (fallback)
```

---

## 5. GameInstance Tier 경계

CLAUDE.md §6 GameInstance 경계 규칙 적용:

| 용도 | Tier | API |
|---|---|---|
| 에셋 로드 요청 (경로) | Tier 1 | `CGameInstance::Get()->LoadMesh(L"Sylas/body.wmesh")` |
| 번들 열기 (경로) | Tier 1 | `CGameInstance::Get()->OpenBundle(L"Content.winters")` |
| 번들 내 에셋 해시 조회 (hot path) | Tier 2 | `CWBundleLoader* pBundle = gi->Get_Bundle(); pBundle->Extract(hash);` |
| mmap 바이트 포인터 (zero-copy GPU 업로드) | Tier 2 | `const uint8_t* p = bundle->PeekRaw(hash);` |
| SHA256 검증 (로드 시 1회) | Tier 1 | `CWintersFile::VerifySHA256()` |

```cpp
// Engine/Include/GameInstance.h 확장
class ENGINE_DLL CGameInstance
{
public: // Asset — Tier 1
    std::shared_ptr<CMesh>    LoadMesh(const std::wstring& path);
    std::shared_ptr<CTexture> LoadTexture(const std::wstring& path);
    HRESULT                   OpenBundle(const std::wstring& path);

    // Tier 2 — hot path 는 포인터 캐시
    class CWBundleLoader*      Get_Bundle()       { return m_pBundle.get(); }
    class CResourceCache*      Get_ResourceCache(){ return m_pResourceCache.get(); }

private:
    std::unique_ptr<class CResourceCache>  m_pResourceCache;
    std::unique_ptr<class CWBundleLoader>  m_pBundle;
};
```

**export 금지** (보안): `CWBundleLoader`, `CResourceCache` 는 `WINTERS_API` 마크 X. GameInstance 만 export.

---

## 6. 에러 코드

```cpp
// Engine/Public/AssetFormat/Core/FormatErrors.h
#pragma once
#include "WintersAPI.h"

namespace Winters::Asset
{
    // HRESULT facility=FACILITY_ITF (0x4), custom-bit=1
    //   S_OK                   = 0
    //   E_WINTERS_*            = 0xA041xxxx
    constexpr HRESULT E_WINTERS_INVALID_MAGIC    = 0xA0410001;
    constexpr HRESULT E_WINTERS_VERSION_MISMATCH = 0xA0410002;
    constexpr HRESULT E_WINTERS_HASH_MISMATCH    = 0xA0410003;
    constexpr HRESULT E_WINTERS_SIZE_OVERFLOW    = 0xA0410004;  // payload > 파일 크기
    constexpr HRESULT E_WINTERS_TRUNCATED        = 0xA0410005;  // header/trailer 부족
    constexpr HRESULT E_WINTERS_COMPRESS_FAILED  = 0xA0410006;
    constexpr HRESULT E_WINTERS_DECOMPRESS_FAILED= 0xA0410007;
    constexpr HRESULT E_WINTERS_SIGNATURE_FAILED = 0xA0410008;
    constexpr HRESULT E_WINTERS_INVALID_TOC      = 0xA0410009;  // 번들 TOC 파손
    constexpr HRESULT E_WINTERS_ASSET_NOT_FOUND  = 0xA041000A;  // hash 조회 실패
    constexpr HRESULT E_WINTERS_FORMAT_UNKNOWN   = 0xA041000B;  // 확장자 미지원
}
```

**보안 연동**: `E_WINTERS_HASH_MISMATCH` / `E_WINTERS_SIGNATURE_FAILED` 발생 시 Phase 6 안티치트의 `CTamperDetector::Report(err, path)` 호출 → 서버 신고.

---

## 7. 스레딩 모델

| 단계 | 스레드 | 비고 |
|---|---|---|
| `LoadMesh("Sylas/body.wmesh")` | Game thread | 캐시 hit 이면 즉시 리턴 |
| 파일 오픈 + SHA256 검증 | Worker (JobSystem) | I/O + 해시는 백그라운드 |
| GPU 업로드 (CreateBuffer) | Render thread | Device 는 free-threaded 지만 immediate context 는 단일 |
| 번들 TOC 파싱 | Game thread 1회 | 게임 시작 시 1번, 이후 read-only |
| mmap 바이트 zero-copy | Render thread 직접 | `D3D11_SUBRESOURCE_DATA.pSysMem` 바인딩 |

**JobSystem 의존**: Phase 5 (Fiber + JobSystem) 완료 후 챔피언 5체 병렬 로드 전환.

---

## 8. 빌드 플래그

```cpp
// Engine_Macro.h 에 추가 (AssetFormat 전용)
#ifdef _DEBUG
    #define WINTERS_ASSET_VERIFY_HASH    1   // 매 로드마다 SHA256
    #define WINTERS_ASSET_LOG_LOAD       1
#else
    #define WINTERS_ASSET_VERIFY_HASH    1   // Release 에서도 켬 (치트 방어)
    #define WINTERS_ASSET_LOG_LOAD       0   // 릴리스 로그 금지 (CLAUDE.md 보안 §5)
#endif

#ifdef WINTERS_SHIP
    #define WINTERS_ASSET_VERIFY_SIGN    1   // 출시 빌드는 Ed25519 서명 강제
#else
    #define WINTERS_ASSET_VERIFY_SIGN    0   // 개발 빌드는 서명 없어도 로드
#endif
```

---

## 9. 성능 목표

| 지표 | 목표 | 현재 (Assimp FBX) |
|---|---|---|
| 500 KB `.wmesh` 로드 | **< 1 ms** | 50~200 ms |
| 2 KB `.wmat` 로드 | < 0.1 ms | N/A (아직 없음) |
| 4 MB `.wtex` (BC7) 로드 + GPU 업로드 | **< 3 ms** | PNG 100~300 ms |
| 번들 TOC 파싱 (1000 엔트리) | < 0.5 ms | N/A |
| SHA256 검증 (100 KB 에셋) | < 0.2 ms (HW SHA) | N/A |

**측정 기준**: Phase 3-C (Profiler) 패널에서 `AssetFormat.Load` 카테고리로 자동 수집.

---

## 10. 외부 의존성

| 라이브러리 | 용도 | 편입 위치 |
|---|---|---|
| **LZ4** (BSD) | Payload 압축 (flags.compressed) | `Engine/ThirdPartyLib/LZ4/` |
| **DirectXTex** (기존) | Stage 5 BC7 인코딩 | 이미 편입됨 (`Engine/ThirdPartyLib/DirectXTK/`) |
| **Assimp** (기존) | 컨버터 측에서 FBX → .wmesh 생성 | 이미 편입됨 |
| **mbedtls** (Apache-2.0) | SHA256 + Ed25519 + AES-GCM | `Engine/ThirdPartyLib/mbedtls/` (Stage 9 에서 편입) |

**주의**: `mbedtls` 는 Stage 9 에 가서야 필요. Stage 2 SHA256 은 Windows `bcrypt.lib` (`BCryptHash`) 로 시작 — 외부 의존 최소화.

`.md/build/THIRDPARTY_INTEGRATION_GUIDE.md` 절차 준수 (CLAUDE.md Gotcha).

---

## 11. 컨벤션 준수 체크리스트

CLAUDE.md 코딩 컨벤션 매핑:

- [x] 클래스 `C` 접두사: `CBinaryReader`, `CWintersFile`
- [x] struct (POD) `C` 없음: `WintersFileHeader`, `MeshHeader`, `TOCEntry`
- [x] 파일명 `C` 없음: `BinaryReader.h`, `WintersFile.h`
- [x] 타입 alias: `f32_t`, `u32_t`, `bool_t` (공개 API 는 `uint32_t` 그대로)
- [x] Create 팩토리 + private ctor: `CWintersFile::LoadFromDisk` 정적 팩토리
- [x] 멤버 `m_` 접두사: `m_pData`, `m_uCursor`
- [x] DLL 경계: `WINTERS_API` 는 **유틸 클래스만** (ResourceCache 등 내부 매니저는 마크 X)
- [x] 공개 헤더 flat include: `#include "WintersFile.h"` (서브경로 X)
- [x] 공개 헤더 `std::` 명시: `std::vector<uint8_t>` (CLAUDE.md Gotcha B-6.6)

---

## 12. 다음 Stage 진입 조건

| 조건 | 확인 |
|---|---|
| `Engine/Public/AssetFormat/Core/` 디렉토리 생성 | Stage 2 전 |
| `Engine.vcxproj.filters` 에 섹션 15. AssetFormat 추가 | Stage 2 전 |
| `BinaryReader.h/Writer.h` 스켈레톤 컴파일 통과 | Stage 2 전 |
| EngineSDK flat 복사 확인 (`UpdateLib.bat` 실행 후 `EngineSDK/inc/BinaryReader.h`) | Stage 2 전 |
| GameInstance `LoadMesh` / `OpenBundle` API 인터페이스만 선언 | Stage 2 후 |

---

## 13. 관련 문서

| 문서 | 역할 |
|---|---|
| `CLAUDE.md` §GameInstance 경계 규칙 | Tier 1/2 구분 |
| `CLAUDE.md` §Include 컨벤션 | flat include 요구 |
| `CLAUDE.md` §보안/안티치트 | SHA256 / 서명 연동 |
| `.md/architecture/WINTERS_ENGINE_CONVENTIONS.md` | 네이밍 / 팩토리 패턴 |
| `.md/build/THIRDPARTY_INTEGRATION_GUIDE.md` | mbedtls / LZ4 편입 절차 |
| `.md/plan/security/PART5_WINTERS_IMPLEMENTATION.md` | 무결성 검증 Level 1~3 |

---

## 14. 다음 단계

Stage 2 로 이동 — `WintersFileHeader.h` POD 확정 + SHA256 구현 + 매직 검증 테스트.
