# Stage 1 — 공통 헤더 `WintersFileHeader` + SHA256 + 매직 검증

> **목표**: 모든 `.w*` 파일이 공유하는 16 byte 헤더를 POD 로 확정 + 파일 끝 32 byte SHA256 자동 기록/검증 + 매직 / 버전 오판별 로직.
> 이 Stage 까지는 **포맷 종류 무관한 기반 인프라**. Stage 3 (`.wmesh`) 이후부터 각 포맷이 이 헤더 위에 페이로드를 얹는다.

---

## 1. 목표 / 비목표

**목표**
- 16 byte 고정 헤더 (cache line 친화적, 정렬 불필요)
- 매직 `"WINT"` + Major/Minor 2단 버전 (breaking vs additive)
- flags 비트 플래그 (압축 / 암호화 / 서명)
- 페이로드 끝 32 byte SHA256 자동 부착
- 검증 API 하나 — `CWintersFile::VerifySHA256()`
- Windows `bcrypt.lib` (BCryptHashDataEx) 사용 — 외부 의존 0

**비목표 (Stage 9 로 이월)**
- Ed25519 서명 (별도 64 byte trailer)
- AES-GCM 암호화 (키 발급 체계 필요)
- 서버 신고 로직 (Phase 6 안티치트 연동)

---

## 2. 포맷 레이아웃

```
Offset   Size   Name              설명
───────────────────────────────────────────────────
0        4      magic             "WINT" (4 바이트, NUL 없음)
4        2      version_major     breaking 변경 시 +1
6        2      version_minor     additive 변경 시 +1
8        4      flags             비트 플래그 (아래 표)
12       4      content_size      헤더 뒤 payload 실제 byte
───────────────────────────────────────────────────
16       N      payload           content_size 만큼
16+N     32     sha256            payload 의 SHA256 (little-endian 바이트 그대로)
───────────────────────────────────────────────────
[선택] 48+N     64     ed25519_sig  flags.signed 시 — Stage 9 에서 추가
```

**total size**: `16 + content_size + 32` (+64 서명 시).

### 비트 플래그

| 비트 | 이름 | 설명 | Stage |
|---|---|---|---|
| 0 | `WINTERS_FLAG_LZ4` | payload 는 LZ4 프레임 포맷 압축 | 3 |
| 1 | `WINTERS_FLAG_ENCRYPTED` | payload 는 AES-GCM 암호화 (12B nonce + payload + 16B tag) | 9 |
| 2 | `WINTERS_FLAG_SIGNED` | payload 뒤 SHA256 뒤에 Ed25519 64B 추가 | 9 |
| 3 | `WINTERS_FLAG_PAYLOAD_RAW_SHA` | SHA256 은 원본 (압축 전) 기준. 기본값 0 = 디스크 payload (압축 후) 기준 | 3 |
| 4~31 | 예약 | 0 고정 | — |

**파생 결정**:
- SHA256 은 **디스크에 기록된 payload 바이트 그대로** 계산. 압축된 payload 라면 그 상태의 해시 → `memcpy` 후 즉시 검증 가능.
- 원본 해시 가 필요하면 (치트 방어 — 압축 알고리즘 교체에 대한 내성) `FLAG_PAYLOAD_RAW_SHA` 설정.

---

## 3. POD 정의

```cpp
// Engine/Public/AssetFormat/Core/WintersFileHeader.h
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include <cstdint>

namespace Winters::Asset
{
    // 매직 — 리틀 엔디언 "WINT" (0x54 0x4E 0x49 0x57)
    constexpr char WINTERS_MAGIC[4] = { 'W', 'I', 'N', 'T' };

    // 현재 포맷 버전 — Stage 9 에서 2.0 으로 bump 예정
    constexpr uint16_t WINTERS_VERSION_MAJOR = 1;
    constexpr uint16_t WINTERS_VERSION_MINOR = 0;

    // Flags
    constexpr uint32_t WINTERS_FLAG_LZ4              = 1u << 0;
    constexpr uint32_t WINTERS_FLAG_ENCRYPTED        = 1u << 1;
    constexpr uint32_t WINTERS_FLAG_SIGNED           = 1u << 2;
    constexpr uint32_t WINTERS_FLAG_PAYLOAD_RAW_SHA  = 1u << 3;

    #pragma pack(push, 1)
    struct WintersFileHeader
    {
        char     magic[4];        // "WINT"
        uint16_t version_major;
        uint16_t version_minor;
        uint32_t flags;
        uint32_t content_size;    // 헤더 뒤 payload byte (압축 상태 기준)
    };
    #pragma pack(pop)

    static_assert(sizeof(WintersFileHeader) == 16, "WintersFileHeader must be 16 bytes");
    static_assert(alignof(WintersFileHeader) == 1, "WintersFileHeader pack(1) required");

    // 페이로드 끝 trailer
    constexpr size_t WINTERS_SHA256_LEN = 32;
    constexpr size_t WINTERS_ED25519_SIG_LEN = 64;

    // 최소 파일 크기 (서명 없음)
    constexpr size_t WINTERS_MIN_FILE_SIZE = sizeof(WintersFileHeader) + WINTERS_SHA256_LEN;
}
```

**왜 `#pragma pack(1)`**: 16 byte 가 자연 정렬이지만 다른 플랫폼(ARM) 이식 시 패딩 들어갈 수 있음 → 고정.

---

## 4. SHA256 — bcrypt.lib

### 4.1 래퍼 클래스

```cpp
// Engine/Public/AssetFormat/Core/FNVHash.h 와 분리된 별도 파일
// Engine/Public/AssetFormat/Core/SHA256.h
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include <array>
#include <cstdint>

namespace Winters::Asset
{
    using SHA256Digest = std::array<uint8_t, 32>;

    class WINTERS_API CSHA256
    {
    public:
        CSHA256();
        ~CSHA256();

        void Reset();
        void Update(const void* pData, size_t n);
        SHA256Digest Finalize();

        // 원샷 헬퍼
        static SHA256Digest Hash(const void* pData, size_t n);

        // 32 byte 비교 (timing-safe)
        static bool_t Equal(const SHA256Digest& a, const SHA256Digest& b);

    private:
        void* m_pAlgHandle  = nullptr;   // BCRYPT_ALG_HANDLE
        void* m_pHashHandle = nullptr;   // BCRYPT_HASH_HANDLE
        void* m_pHashObject = nullptr;   // opaque buffer
        uint32_t m_uObjectSize = 0;
    };
}
```

### 4.2 구현 개요

```cpp
// Engine/Private/AssetFormat/Core/SHA256.cpp
#include "SHA256.h"
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")

namespace Winters::Asset
{
    CSHA256::CSHA256()
    {
        BCryptOpenAlgorithmProvider((BCRYPT_ALG_HANDLE*)&m_pAlgHandle,
                                     BCRYPT_SHA256_ALGORITHM, nullptr, 0);
        uint32_t cbResult = 0;
        BCryptGetProperty((BCRYPT_ALG_HANDLE)m_pAlgHandle, BCRYPT_OBJECT_LENGTH,
                          (PUCHAR)&m_uObjectSize, sizeof(uint32_t), (ULONG*)&cbResult, 0);
        m_pHashObject = ::operator new(m_uObjectSize);
        BCryptCreateHash((BCRYPT_ALG_HANDLE)m_pAlgHandle,
                         (BCRYPT_HASH_HANDLE*)&m_pHashHandle,
                         (PUCHAR)m_pHashObject, m_uObjectSize,
                         nullptr, 0, 0);
    }

    void CSHA256::Update(const void* pData, size_t n)
    {
        BCryptHashData((BCRYPT_HASH_HANDLE)m_pHashHandle,
                       (PUCHAR)pData, (ULONG)n, 0);
    }

    SHA256Digest CSHA256::Finalize()
    {
        SHA256Digest out{};
        BCryptFinishHash((BCRYPT_HASH_HANDLE)m_pHashHandle,
                         out.data(), (ULONG)out.size(), 0);
        return out;
    }

    bool_t CSHA256::Equal(const SHA256Digest& a, const SHA256Digest& b)
    {
        // timing-safe 비교 — 치트 개발자가 타이밍 공격으로 brute-force 못 하게
        uint8_t diff = 0;
        for (size_t i = 0; i < a.size(); ++i) diff |= a[i] ^ b[i];
        return diff == 0;
    }
}
```

**성능**: 1 MB SHA256 ≈ 2 ms (bcrypt SHA-NI 하드웨어 가속 자동 사용). 100 KB `.wmesh` 기준 < 0.2 ms — 목표 달성.

---

## 5. CWintersFile — 로드 구현

```cpp
// Engine/Private/AssetFormat/Core/WintersFile.cpp
HRESULT CWintersFile::LoadFromDisk(const wchar_t* pPath, CWintersFile& out)
{
    // 1. 파일 오픈 + 전체 읽기 (Stage 3 에서 mmap 최적화 추가)
    HANDLE hFile = ::CreateFileW(pPath, GENERIC_READ, FILE_SHARE_READ, nullptr,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return HRESULT_FROM_WIN32(GetLastError());

    LARGE_INTEGER size{};
    ::GetFileSizeEx(hFile, &size);
    if (size.QuadPart < (LONGLONG)WINTERS_MIN_FILE_SIZE) {
        ::CloseHandle(hFile);
        return E_WINTERS_TRUNCATED;
    }

    std::vector<uint8_t> raw(size.QuadPart);
    DWORD read = 0;
    ::ReadFile(hFile, raw.data(), (DWORD)size.QuadPart, &read, nullptr);
    ::CloseHandle(hFile);

    return LoadFromMemory(raw.data(), raw.size(), out);
}

HRESULT CWintersFile::LoadFromMemory(const void* pData, size_t uSize, CWintersFile& out)
{
    if (uSize < WINTERS_MIN_FILE_SIZE)
        return E_WINTERS_TRUNCATED;

    // 2. 헤더 복사
    std::memcpy(&out.m_header, pData, sizeof(WintersFileHeader));

    // 3. 매직 검증
    if (std::memcmp(out.m_header.magic, WINTERS_MAGIC, 4) != 0)
        return E_WINTERS_INVALID_MAGIC;

    // 4. 버전 검증 (Major 만. Minor 는 호환)
    if (out.m_header.version_major != WINTERS_VERSION_MAJOR) {
        // Stage 11 VersionMigrator 가 처리 — 여기서는 hard fail
        return E_WINTERS_VERSION_MISMATCH;
    }

    // 5. 크기 검증 (오버플로우 방어)
    const size_t trailerSize = WINTERS_SHA256_LEN
        + ((out.m_header.flags & WINTERS_FLAG_SIGNED) ? WINTERS_ED25519_SIG_LEN : 0);
    const size_t expected = sizeof(WintersFileHeader) + out.m_header.content_size + trailerSize;
    if (expected != uSize) return E_WINTERS_SIZE_OVERFLOW;

    // 6. payload 복사
    const uint8_t* p = static_cast<const uint8_t*>(pData);
    out.m_vPayload.assign(p + sizeof(WintersFileHeader),
                          p + sizeof(WintersFileHeader) + out.m_header.content_size);

    // 7. SHA256 추출
    const uint8_t* shaPtr = p + sizeof(WintersFileHeader) + out.m_header.content_size;
    std::memcpy(out.m_sha256, shaPtr, WINTERS_SHA256_LEN);

    // 8. 해시 검증
    if (!out.VerifySHA256()) return E_WINTERS_HASH_MISMATCH;

    // 9. 압축 해제 (Stage 3)
    if (out.m_header.flags & WINTERS_FLAG_LZ4) {
        if (FAILED(out.DecompressLZ4())) return E_WINTERS_DECOMPRESS_FAILED;
    } else {
        out.m_vDecompressed = out.m_vPayload;  // shallow copy (Stage 3 에서 move)
    }

    return S_OK;
}

bool_t CWintersFile::VerifySHA256() const
{
    const auto actual = CSHA256::Hash(m_vPayload.data(), m_vPayload.size());
    SHA256Digest expected;
    std::memcpy(expected.data(), m_sha256, 32);
    return CSHA256::Equal(actual, expected);
}
```

---

## 6. BinaryWriter::SaveToFile 구현

```cpp
// Engine/Private/AssetFormat/Core/BinaryWriter.cpp
HRESULT CBinaryWriter::SaveToFile(const wchar_t* pPath, uint32_t flags,
                                   uint16_t verMajor, uint16_t verMinor) const
{
    // 1. Header 생성
    WintersFileHeader hdr{};
    std::memcpy(hdr.magic, WINTERS_MAGIC, 4);
    hdr.version_major = verMajor;
    hdr.version_minor = verMinor;
    hdr.flags         = flags;

    std::vector<uint8_t> finalPayload = m_vBuffer;

    // 2. (선택) LZ4 압축
    if (flags & WINTERS_FLAG_LZ4) {
        // Stage 3 에서 구현 — 지금은 no-op
        // finalPayload = LZ4_Compress(m_vBuffer);
    }

    hdr.content_size = (uint32_t)finalPayload.size();

    // 3. SHA256
    const auto digest = CSHA256::Hash(finalPayload.data(), finalPayload.size());

    // 4. 디스크 기록
    HANDLE hFile = ::CreateFileW(pPath, GENERIC_WRITE, 0, nullptr,
                                  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return HRESULT_FROM_WIN32(GetLastError());

    DWORD written = 0;
    ::WriteFile(hFile, &hdr, sizeof(hdr), &written, nullptr);
    ::WriteFile(hFile, finalPayload.data(), (DWORD)finalPayload.size(), &written, nullptr);
    ::WriteFile(hFile, digest.data(), (DWORD)digest.size(), &written, nullptr);

    // 5. (Stage 9) Ed25519 서명
    if (flags & WINTERS_FLAG_SIGNED) {
        // 미래 Stage 9 에서 구현
    }

    ::CloseHandle(hFile);
    return S_OK;
}
```

---

## 7. 검증 테스트 (GoogleTest)

```cpp
// Test/AssetFormat/WintersFileHeader_Test.cpp
#include <gtest/gtest.h>
#include "WintersFile.h"
using namespace Winters::Asset;

TEST(WintersFile, RoundTrip_EmptyPayload)
{
    CBinaryWriter w;
    w.Write<uint32_t>(0x12345678);

    ASSERT_HRESULT_SUCCEEDED(w.SaveToFile(L"test_roundtrip.wtest"));

    CWintersFile loaded;
    ASSERT_HRESULT_SUCCEEDED(CWintersFile::LoadFromDisk(L"test_roundtrip.wtest", loaded));
    EXPECT_EQ(loaded.Header().version_major, 1);
    EXPECT_EQ(loaded.PayloadSize(), 4);
    EXPECT_TRUE(loaded.VerifySHA256());
}

TEST(WintersFile, RejectBadMagic)
{
    uint8_t fake[48] = { 'F','A','K','E',  1,0, 0,0,  0,0,0,0,  0,0,0,0 /* + 32 zeros */ };
    CWintersFile loaded;
    EXPECT_EQ(CWintersFile::LoadFromMemory(fake, sizeof(fake), loaded),
              E_WINTERS_INVALID_MAGIC);
}

TEST(WintersFile, RejectTampered)
{
    CBinaryWriter w;
    w.Write<uint32_t>(0x12345678);
    w.SaveToFile(L"test_tamper.wtest");

    // payload byte 조작
    HANDLE h = CreateFileW(L"test_tamper.wtest", GENERIC_READ | GENERIC_WRITE,
                          0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    SetFilePointer(h, 16, nullptr, FILE_BEGIN);
    uint8_t tamper = 0xFF;
    DWORD w2 = 0;
    WriteFile(h, &tamper, 1, &w2, nullptr);
    CloseHandle(h);

    CWintersFile loaded;
    EXPECT_EQ(CWintersFile::LoadFromDisk(L"test_tamper.wtest", loaded),
              E_WINTERS_HASH_MISMATCH);
}

TEST(WintersFile, RejectTruncated)
{
    uint8_t tiny[8] = {};
    CWintersFile loaded;
    EXPECT_EQ(CWintersFile::LoadFromMemory(tiny, sizeof(tiny), loaded),
              E_WINTERS_TRUNCATED);
}

TEST(WintersFile, RejectSizeMismatch)
{
    // content_size 가 실제 파일보다 큰 파일 합성
    std::vector<uint8_t> buf(48, 0);
    std::memcpy(buf.data(), WINTERS_MAGIC, 4);
    *(uint16_t*)(buf.data()+4) = 1;
    *(uint16_t*)(buf.data()+6) = 0;
    *(uint32_t*)(buf.data()+8) = 0;
    *(uint32_t*)(buf.data()+12) = 999999;  // 거짓 크기

    CWintersFile loaded;
    EXPECT_EQ(CWintersFile::LoadFromMemory(buf.data(), buf.size(), loaded),
              E_WINTERS_SIZE_OVERFLOW);
}
```

---

## 8. 보안 고려사항

CLAUDE.md 보안 §9 (파일 무결성) 연계:

| 위협 | 방어 |
|---|---|
| 치트가 `.wmesh` 내부 vertex 조작 → 벽뚫기 | SHA256 자동 검증, 불일치 시 `E_WINTERS_HASH_MISMATCH` |
| 파일 헤더 조작 (매직/버전 변경) | 매직 체크 + Major 버전 mismatch 거부 |
| 거대 `content_size` 로 메모리 고갈 공격 | `expected != uSize` 체크 (실제 파일 크기 대조) |
| Timing attack 으로 SHA256 brute-force | `CSHA256::Equal` XOR 누적 (constant-time) |
| 해시는 OK 지만 내용 악의적 조작 (매직/버전 유지) | Stage 9 Ed25519 서명 — 개발 키 없으면 위조 불가 |

**Release 빌드 기본 켬**: `WINTERS_ASSET_VERIFY_HASH = 1` (Architecture Stage 1 §8).

---

## 9. 로그 정책

CLAUDE.md 보안 §5 (로그/문자열 누출 방지) 준수:

```cpp
// 🔴 금지 (Release)
OutputDebugStringA("HASH MISMATCH on Sylas/body.wmesh\n");

// 🟢 허용 (Release)
// 내부 오류 코드만 반환, 서버에 hash 기댓값/실제값 전송 금지 — 해시 길이/위치 노출 방지
CTamperDetector::Report(E_WINTERS_HASH_MISMATCH, pathHash);
```

---

## 10. LoadFromDisk 성능 최적화 (Stage 3 이후)

현재 `LoadFromDisk` 는 `ReadFile` 로 전체 읽기. Stage 3 에서:

1. `CreateFileMappingW` + `MapViewOfFile` 로 mmap 전환
2. Payload 포인터 직접 GPU 로 업로드 (memcpy 1회 제거)
3. SHA256 계산도 mmap 바이트 그대로 → 파일 크기 > 워킹 세트 시에도 페이지 fault 만 발생

현재 Stage 1 에선 **단순 구현 우선**. mmap 은 `.wbundle` (Stage 8) 에서 필수.

---

## 11. 완료 기준 (Definition of Done)

- [ ] `WintersFileHeader.h` POD + static_assert
- [ ] `SHA256.h/cpp` bcrypt 래퍼 (timing-safe Equal 포함)
- [ ] `BinaryReader.h` + `BinaryWriter.h` + `.cpp`
- [ ] `WintersFile.h/cpp` LoadFromDisk/LoadFromMemory/VerifySHA256
- [ ] `FormatErrors.h` enum
- [ ] GoogleTest 5개 케이스 (RoundTrip/BadMagic/Tamper/Truncated/SizeMismatch)
- [ ] Release 빌드 SHA256 활성 확인 (매크로 `WINTERS_ASSET_VERIFY_HASH=1`)
- [ ] EngineSDK flat 복사 확인

---

## 12. 다음 단계

Stage 2 로 이동 — `.wmesh` 포맷 설계 + Assimp 컨버터 + 런타임 로더 + 챔피언 5체 로드 시간 측정.
