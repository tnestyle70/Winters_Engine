# Stage 8 — 무결성 (Ed25519 서명 + AES-GCM 암호화 + 탬퍼 감지 연계)

> **목표**: Winters 에셋 파이프라인에 **암호학적 무결성** 레이어 추가. 치트가 에셋 바이트를 수정하면 로드 거부 + 서버 신고.
> **선행 필요**: Stage 2 (SHA256) ✅ / Stage 7 (번들) ✅ / CLAUDE.md 보안 §9.
> **후속 연계**: `.md/plan/security/PART5_WINTERS_IMPLEMENTATION.md` Level 1~3.

---

## 1. 3-Level 무결성 체계

| Level | 메커니즘 | 보호 대상 | 비용 |
|---|---|---|---|
| **1. SHA256** | Stage 2 (기본) | 단순 변조 감지 | 0.2 ms / 100 KB |
| **2. Ed25519 서명** | 본 Stage | 에셋/번들 교체 방지 | 1 ms verify / asset |
| **3. AES-GCM 암호화** | 본 Stage (선택) | 민감 에셋 (챔프 스탯/스킬 데이터) | 0.5 ms / 100 KB |

## 2. 위협 모델

### 2.1 CLAUDE.md 보안 §9 반영

| 공격 시나리오 | 대응 Level |
|---|---|
| 치트가 body.wmesh 정점 옮겨 hitbox 확장 | Level 1 SHA256 |
| 치트가 Stage1.wmap 의 벽 제거 (NavGrid bit 클리어) | Level 1 SHA256 |
| 치트가 `.winters` 번들 자체를 자기 것으로 교체 | Level 2 Ed25519 |
| 치트가 Champion 스탯 JSON 을 읽어 예측 | Level 3 AES-GCM (선택) |
| 치트가 사운드 파일을 삭제/교체 (치트 감지 회피) | Level 2 (번들 서명 내 포함) |
| MITM 이 `.winters` 다운로드 중 변조 | Level 2 + HTTPS 핀닝 |

### 2.2 위협 밖 (의도적 제외)

- **암호화로 인한 리버스 엔지니어링 완전 차단**: 불가능. 메모리에 올라간 에셋은 dump 가능. 목표는 "에셋 변조 시 탐지" 지 "리버싱 불가".
- **개발 키 유출 방어**: 서명 키는 빌드 서버만 보유. 노트북 등 개발자 로컬엔 테스트 키.

---

## 3. Ed25519 서명

### 3.1 왜 Ed25519 (vs RSA / ECDSA)

| 항목 | RSA-2048 | ECDSA P-256 | **Ed25519** |
|---|---|---|---|
| 서명 크기 | 256 B | 64 B | **64 B** |
| 검증 속도 | 0.1 ms | 0.5 ms | **0.3 ms** |
| 사이드 채널 내성 | 중 | 하 (nonce 재사용) | **상 (deterministic)** |
| 구현 난이도 | 복잡 | 중 | **단순** |
| 표준 | 광범위 | 광범위 | RFC 8032 (2017+) |

**선택**: Ed25519. 64 byte trailer + 빠른 검증 + 표준 라이브러리 지원.

### 3.2 구현 — mbedtls

```cpp
// Engine/Public/AssetFormat/Integrity/Ed25519.h
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include <array>

namespace Winters::Asset
{
    using Ed25519Seed      = std::array<uint8_t, 32>;   // private key seed
    using Ed25519PublicKey = std::array<uint8_t, 32>;
    using Ed25519Signature = std::array<uint8_t, 64>;

    class WINTERS_API CEd25519
    {
    public:
        // 키 생성 (컨버터 호스트 — 빌드 서버)
        static void Generate(Ed25519Seed& outSeed, Ed25519PublicKey& outPub);

        // 서명 (개발 키 필요 — 빌드 서버)
        static Ed25519Signature Sign(const Ed25519Seed& seed,
                                      const void* pMsg, size_t uLen);

        // 검증 (런타임 — 클라이언트가 공개키로)
        static bool_t Verify(const Ed25519PublicKey& pub,
                              const void* pMsg, size_t uLen,
                              const Ed25519Signature& sig);
    };
}
```

### 3.3 적용 지점

**개별 파일 (선택)**: `WintersFileHeader.flags & WINTERS_FLAG_SIGNED`
- 파일 끝 SHA256(32B) 다음에 Ed25519Signature(64B) trailer
- Signature = Ed25519Sign(seed, payload_bytes)
- `CWintersFile::VerifySignature(pub)` 추가

**번들 (기본)**: `.winters` 파일 끝에 64B trailer
- Signature = Ed25519Sign(seed, fileBytes[0..toc_offset + toc_count * sizeof(TOCEntry)])
- 범위: Header + Data + TOC + StringTable (서명 자체 제외)
- `CWBundleLoader::VerifySignature(pub)` — 게임 시작 시 1회만 호출

### 3.4 `CWintersFile` 확장

```cpp
// Engine/Private/AssetFormat/Core/WintersFile.cpp (Stage 2 확장)
bool_t CWintersFile::VerifyIntegrity() const
{
    if (!VerifySHA256()) return false;

    if (m_header.flags & WINTERS_FLAG_SIGNED) {
        const uint8_t* sigPtr = /* 파일 끝에서 64B */;
        Ed25519Signature sig;
        std::memcpy(sig.data(), sigPtr, 64);

        Ed25519PublicKey pub = LoadEmbeddedPubKey();   // DLL 내부 상수
        return CEd25519::Verify(pub, m_vPayload.data(), m_vPayload.size(), sig);
    }

    return true;
}
```

### 3.5 공개 키 임베드

Release DLL 에 공개 키 **상수로 컴파일 시 삽입** — 치트가 공개 키를 바꾸는 것도 어렵게 (CLAUDE.md 보안 §3 "심볼 가시성" 참조):

```cpp
// Engine/Private/AssetFormat/Integrity/PubKeys.cpp
// BUILD_STEP: 빌드 서버가 이 파일을 빌드 시점에 생성 (private key 없이)
namespace {
    // sha256("WINTERS_STUDIOS_MAIN_KEY_v1") 로 난독화된 이름
    alignas(32) const uint8_t g_Xf8a72c13[32] = {
        0x12, 0x34, /* ... 실제 공개 키 32 B ... */
    };
}

Ed25519PublicKey LoadEmbeddedPubKey()
{
    Ed25519PublicKey k;
    std::memcpy(k.data(), g_Xf8a72c13, 32);
    return k;
}
```

**배포 빌드 파이프라인**:
1. 개발 서명 키 (`dev.seed`) — 노트북 로컬
2. 프로덕션 서명 키 (`prod.seed`) — HSM 또는 안전한 빌드 서버
3. 공개 키 `pub.key` 는 소스에 커밋 (Release 빌드 시)
4. 서명은 CI/CD 의 sign step 에서 수행

---

## 4. AES-GCM 암호화 (선택)

### 4.1 대상 에셋

| 에셋 | 암호화? | 이유 |
|---|---|---|
| `.wmesh` / `.wskel` / `.wanim` / `.wtex` | No | 리버싱 해도 부정 이득 적음 |
| `.wmat` | 조건부 | 셰이더 해시 노출 OK, 파라미터는 상관없음 |
| `.wmap` | 조건부 | 구조물 좌표 — 이미 게임에서 노출됨 |
| **Champions/*/stats.winters** | **Yes** | 데미지 계산식 / 스킬 판정 범위 |
| **AI behavior trees / GOAP plans** | **Yes** | 봇 로직 학습 방어 |
| **anti-cheat signatures** | **Yes** | 탐지 패턴 노출 방지 |

### 4.2 AES-GCM 포맷

```
[ WintersFileHeader ]  flags = ENCRYPTED | SIGNED
    content_size = sizeof(EncryptedPayload)

[ EncryptedPayload ]
    uint8_t  nonce[12]         ← 랜덤 (CSPRNG)
    uint8_t  ciphertext[N-28]
    uint8_t  tag[16]           ← GCM authentication tag

[ SHA256 32B ]                 ← 암호문 해시 (이중 방어)
[ Ed25519 64B ]                ← 파일 전체 서명
```

### 4.3 키 관리

| 키 타입 | 용도 | 저장 |
|---|---|---|
| **마스터 키** | 모든 AES 키를 유도 | 서버 HSM |
| **에셋 키** | per-bundle 유도 (HKDF) | 빌드 서버가 계산 후 배포 번들 에 내장 (패키징 암호) |
| **세션 키** | 매 로그인 시 서버 → 클라이언트 | TLS 경유 |

### 4.4 구현

```cpp
// Engine/Public/AssetFormat/Integrity/AESGCM.h
namespace Winters::Asset
{
    using AESKey   = std::array<uint8_t, 32>;  // AES-256
    using AESNonce = std::array<uint8_t, 12>;
    using AESTag   = std::array<uint8_t, 16>;

    class WINTERS_API CAESGCM
    {
    public:
        // 암호화 (빌드 서버 측)
        static bool_t Encrypt(const AESKey& key,
                               const void* pPlain, size_t uLen,
                               std::vector<uint8_t>& outCipher,
                               AESNonce& outNonce, AESTag& outTag);

        // 복호 (클라이언트)
        static bool_t Decrypt(const AESKey& key,
                               const AESNonce& nonce,
                               const void* pCipher, size_t uLen,
                               const AESTag& tag,
                               std::vector<uint8_t>& outPlain);
    };
}
```

---

## 5. CTamperDetector — 서버 신고

```cpp
// Engine/Public/AssetFormat/Integrity/TamperDetector.h
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"

namespace Winters::Asset
{
    enum eTamperReason : uint32_t
    {
        TAMPER_SHA_MISMATCH        = 1,
        TAMPER_SIGNATURE_FAILED    = 2,
        TAMPER_DECRYPT_FAILED      = 3,
        TAMPER_TOC_CORRUPT         = 4,
        TAMPER_EMBEDDED_PUBKEY_BAD = 5,
    };

    struct TamperEvent
    {
        eTamperReason reason;
        uint64_t      asset_hash;     // FNV-1a of path (내용 노출 안 함)
        uint64_t      expected_sha_prefix;  // 첫 8 B 만
        uint64_t      actual_sha_prefix;
        uint32_t      bundle_hash;
        uint32_t      timestamp_unix;
    };

    class WINTERS_API CTamperDetector
    {
    public:
        // 로컬 이벤트 기록 + 서버에 보고 큐잉
        static void Report(const TamperEvent& ev);

        // 세션별 누적 카운트 (5회 초과 시 디스커넥트 고려)
        static uint32_t GetSessionCount();
    };
}
```

**주의 (CLAUDE.md 보안 §5)**:
- 서버 보고 패킷에 파일 경로 / 해시 전체값 넣지 말 것 → 공격자 정보 노출
- `expected_sha_prefix` 는 8 byte 만 (로그 구분용)
- 로컬 OutputDebugString 금지 (Release)

---

## 6. 빌드 서버 파이프라인

### 6.1 서명 flow

```
[개발자 PC]                [빌드 서버 (CI)]                   [배포]
  ↓ push                     ↓ clone
  .wmesh 등 생성              Winters Engine 빌드
  (개발 키로 서명)             ↓
                              WintersAssetConverter.exe
                                convert-all → Content.winters
                              ↓
                              SignBundle.exe
                                --key prod.seed
                                --input Content.winters
                                → Content.winters (서명 추가)
                              ↓
                              CDN 업로드 → 클라이언트 다운로드
```

### 6.2 키 보안 (CLAUDE.md 보안)

```
prod.seed (32 B)
├─ 보관: Azure Key Vault / AWS KMS / HSM
├─ 접근: CI 서비스 계정 만 (temporary credential)
├─ 회전: 6개월 (공개 키 버전 bump = Minor)
└─ 노출 시 대응: 즉시 회전 + 구 키 서명 번들 거부

pub.key (32 B)
├─ 소스 저장소: 공개 (Engine/Private/AssetFormat/Integrity/PubKeys.cpp)
├─ 버전 관리: pub_v1, pub_v2 ... 병행 (구 번들 호환)
└─ 배포: Release DLL 에 상수 임베드
```

---

## 7. 키 회전

공개 키 교체 시나리오:
- 구 키 유출 발생 시
- 정기 회전 (6~12개월)

### 7.1 호환성

DLL 에 공개 키 N 개 병행 내장:

```cpp
// Engine/Private/AssetFormat/Integrity/PubKeys.cpp
const Ed25519PublicKey g_KeyV1 = { /* 2026 초반 */ };
const Ed25519PublicKey g_KeyV2 = { /* 2026 중반 */ };
const Ed25519PublicKey g_KeyV3 = { /* 2026 말 회전 */ };

bool_t VerifyWithAnyTrustedKey(const void* msg, size_t len, const Ed25519Signature& sig)
{
    if (CEd25519::Verify(g_KeyV1, msg, len, sig)) return true;
    if (CEd25519::Verify(g_KeyV2, msg, len, sig)) return true;
    if (CEd25519::Verify(g_KeyV3, msg, len, sig)) return true;
    return false;
}
```

**만료**: 구 키 DLL 은 클라이언트 업데이트 강제 후 제거.

---

## 8. Ed25519 구현 선택

| 라이브러리 | 라이선스 | 크기 | 편입 경로 |
|---|---|---|---|
| **mbedtls** | Apache-2.0 | ~200 KB | `Engine/ThirdPartyLib/mbedtls/` |
| libsodium | ISC | ~300 KB | (대안) |
| 직접 구현 | — | ~5 KB | RFC 8032 참조 (포트폴리오) |

**선택**: mbedtls. 이미 보안 Stage 에서 사용 예정. SHA256 (bcrypt) 외 Ed25519 / AES-GCM 단일 소스.

`.md/build/THIRDPARTY_INTEGRATION_GUIDE.md` 절차 준수.

---

## 9. Release 강제 플래그

```cpp
// Engine_Macro.h
#ifdef WINTERS_SHIP
    #define WINTERS_REQUIRE_SIGNATURE   1   // 서명 없는 번들 로드 거부
    #define WINTERS_FORBID_LOOSE_FILES  1   // .wmesh 개별 파일 로드 거부
#else
    #define WINTERS_REQUIRE_SIGNATURE   0
    #define WINTERS_FORBID_LOOSE_FILES  0
#endif
```

Release 빌드:
- `Content.winters` 만 로드
- 번들 서명 필수
- 루즈 파일 (`body.wmesh` 단독) → `E_FAIL`

Debug / Editor 빌드:
- 루즈 파일 OK
- 서명 없어도 OK (경고만)

---

## 10. Stage 1 유틸 확장

### 10.1 `WINTERS_FLAG_SIGNED` 처리

Stage 2 초안에선 서명 비트만 있었음. 본 Stage 에서 실제 구현:

```cpp
// Engine/Private/AssetFormat/Core/WintersFile.cpp (재수정)
HRESULT CWintersFile::LoadFromMemory(const void* pData, size_t uSize, CWintersFile& out)
{
    // ... 기존 ...

    const size_t trailerSize = WINTERS_SHA256_LEN
        + ((out.m_header.flags & WINTERS_FLAG_SIGNED) ? WINTERS_ED25519_SIG_LEN : 0);
    const size_t expected = sizeof(WintersFileHeader) + out.m_header.content_size + trailerSize;
    if (expected != uSize) return E_WINTERS_SIZE_OVERFLOW;

    // payload + sha 복사 (기존)

    if (out.m_header.flags & WINTERS_FLAG_SIGNED) {
        const uint8_t* sigPtr = static_cast<const uint8_t*>(pData) + uSize - WINTERS_ED25519_SIG_LEN;
        std::memcpy(out.m_sig.data(), sigPtr, WINTERS_ED25519_SIG_LEN);
    }

#if WINTERS_REQUIRE_SIGNATURE
    if (!out.VerifyIntegrity()) return E_WINTERS_SIGNATURE_FAILED;
#endif
    return S_OK;
}
```

---

## 11. 테스트

- [ ] 공개 키로 유효 서명 검증 성공
- [ ] 잘못된 서명 → `E_WINTERS_SIGNATURE_FAILED`
- [ ] payload 1 byte 조작 → SHA256 mismatch (서명 전 검출)
- [ ] 번들 서명 검증 < 1 ms
- [ ] AES-GCM 100 KB 암호/복호 < 0.5 ms
- [ ] `CTamperDetector::Report` 서버 큐 적재 확인
- [ ] Release 빌드 서명 없는 번들 → 로드 거부
- [ ] 키 회전 — V1 서명 번들이 DLL V2/V3 공개 키로도 검증 성공

---

## 12. 보안 평가

### 12.1 해결된 위협

| 위협 | Level | 상태 |
|---|---|---|
| 에셋 바이트 단순 변조 | 1 | ✅ |
| 에셋 완전 교체 (서명 없는 새 파일) | 2 | ✅ |
| 번들 전체 교체 | 2 | ✅ |
| 민감 데이터 (스탯/AI) 리버스 | 3 | 부분 (메모리 덤프는 불가) |

### 12.2 남은 위협

- **런타임 메모리 훅**: DLL 내 `CEd25519::Verify` 를 항상 `true` 리턴하게 후크 → Phase 6 커널 안티치트 영역
- **공개 키 교체**: DLL 자체를 수정하면 공개 키도 바꿀 수 있음 → Code signing 병행 (별도 Phase 6)
- **타이밍 사이드 채널**: `CSHA256::Equal` 이 timing-safe 이지만 `CEd25519::Verify` 내부 mbedtls 도 constant-time 확인 필요

---

## 13. 완료 기준

- [ ] `Ed25519.h/cpp` mbedtls 래퍼
- [ ] `AESGCM.h/cpp` mbedtls 래퍼 (선택 에셋)
- [ ] `TamperDetector.h/cpp` 서버 신고 큐
- [ ] `PubKeys.cpp` 공개 키 임베드
- [ ] `CWintersFile::VerifyIntegrity` 서명 검증 통합
- [ ] `CWBundleLoader::VerifySignature` 번들 전체 서명
- [ ] CI 빌드 스크립트 — 서명 단계 추가
- [ ] `WINTERS_SHIP` 매크로 서명 강제 테스트
- [ ] 키 회전 설계 문서 (별도 `.md/plan/security/KEY_ROTATION.md` — 선택)

---

## 14. 다음 단계

Stage 9 (Converter CLI) 로 이동 — `WintersAssetConverter.exe` 통합 명령어 체계 + 빌드 자동화.
