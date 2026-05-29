# Stage 10 — Versioning (Major/Minor 호환성 + 런타임 VersionMigrator)

> **목표**: 포맷이 진화할 때 **기존 에셋을 재빌드하지 않고** 로드 가능하도록 Major/Minor 버전 규칙 + 런타임 마이그레이션 레이어 구축.
> CLAUDE.md Gotcha "바이너리 POD 포맷 변경 시 VERSION bump + 기존 .dat 삭제" 문제를 체계화.

---

## 1. 버전 규칙

### 1.1 Semantic Versioning 적용

| 변경 | Major | Minor | 로더 |
|---|---|---|---|
| 필드 순서 변경 | ✓ | — | **거부** |
| 필드 타입 변경 (u32→u64) | ✓ | — | **거부** |
| 필드 의미 변경 (unit m → cm) | ✓ | — | **거부** |
| 필드 삭제 (POD 축소) | ✓ | — | **거부** |
| 섹션 삭제 | ✓ | — | **거부** |
| 엔디언 변경 (Little → Big) | ✓ | — | **거부** |
| 필드 추가 (POD 확장 뒤) | — | ✓ | 호환 로드 (기본값) |
| 섹션 추가 (TOC 기반) | — | ✓ | 호환 로드 |
| 의미 변경 없는 상수 업데이트 | — | ✓ | 호환 로드 |
| 매직 변경 | — | — | **다른 포맷으로 취급** |

### 1.2 Winters 포맷별 현재 버전

| 포맷 | 매직 | Ver | 최소 호환 |
|---|---|---|---|
| `.wmesh` | `WINT/WMSH` | 1.0 | 1.0 |
| `.wanim` | `WINT/WANM` | 1.0 | 1.0 |
| `.wskel` | `WINT/WSKL` | 1.0 | 1.0 |
| `.wtex` | `WINT/WTEX` | 1.0 | 1.0 |
| `.wmat` | `WINT/WMAT` | 1.0 | 1.0 |
| `.wmap` | `WINT/WMAP` | 1.0 | 1.0 |
| `.winters` | `WINTPACK` | 1.0 | 1.0 |
| **(기존 Stage1.dat)** | `WSTG` | 4 (v3 호환) | 3 |

---

## 2. 런타임 VersionMigrator

### 2.1 전략

**Backward Compatible (권장)**: 최신 로더가 구버전 파일 읽기.
**Forward Compatible (제한적)**: 구 로더가 Minor 버전 업 파일 읽기 — 추가 필드 무시.

```cpp
// Engine/Public/AssetFormat/Versioning/VersionMigrator.h
namespace Winters::Asset
{
    struct VersionRange
    {
        uint16_t major_min, major_max;
        uint16_t minor_min, minor_max;
    };

    class WINTERS_API CVersionMigrator
    {
    public:
        // 로더가 정의한 지원 범위 확인
        static bool_t IsSupported(const WintersFileHeader& h, VersionRange supported);

        // Minor 차이 -> 추가 필드 0 채움, 제거 필드 무시 (런타임 어댑터)
        // Major 차이 -> false 반환 (Major 마이그레이션은 컨버터 타임 강제)
    };
}
```

### 2.2 Legacy Reader 패턴

각 포맷이 `LegacyReaders` 에 구버전 Reader 등록:

```cpp
// Engine/Public/AssetFormat/Versioning/LegacyReaders.h
namespace Winters::Asset::Legacy
{
    // .wmesh v1.0 → v1.1 (LoD 섹션 추가 가정)
    struct MeshMetaHeader_v1_0
    {
        char     magic[4];
        uint32_t submesh_count;
        uint32_t bone_count;
        // ... v1.0 필드만 (LoD 없음) ...
    };

    HRESULT UpgradeMeshMeta_v1_0_to_v1_1(const MeshMetaHeader_v1_0& old,
                                           MeshMetaHeader& out);
}
```

### 2.3 실제 분기 예

```cpp
// Engine/Private/AssetFormat/Mesh/WMeshLoader.cpp
std::shared_ptr<CMesh> CWMeshLoader::LoadFromMemory(CDevice* d, const void* pData, size_t n)
{
    CBinaryReader r(pData, n);
    auto raw = r.Read<MeshMetaHeader>();

    // 현재 로더는 v1.x 지원
    constexpr VersionRange SUPPORTED = { 1, 1,    0, 999 };
    WintersFileHeader outer{};  // 호출자에서 받음
    if (!CVersionMigrator::IsSupported(outer, SUPPORTED))
        return nullptr;

    // Minor < 현재 → 추가 필드 0 채움
    // 이 포맷은 POD 단일 헤더라 별도 업그레이드 불필요
    MeshMetaHeader hdr = raw;

    // ...나머지 로직
}
```

---

## 3. Major 버전 업 플로우

### 3.1 공식 절차

1. **새 Major 버전 설계 문서 작성** (`.md/plan/WintersFormat/migration_v2.md`)
2. 컨버터에 `migrate-v1-to-v2` subcommand 추가
3. 기존 자산 일괄 변환 스크립트 배포
4. 로더는 Major 2 만 지원 → v1 로드 시 `E_WINTERS_VERSION_MISMATCH`
5. 패치 노트: "이번 업데이트 후 v1 번들 로드 불가. 자동 재다운로드."

### 3.2 migrate-v1-to-v2 subcommand 예

```
WintersAssetConverter.exe migrate \
    --from 1 \
    --to 2 \
    --input "Bin\Resource\" \
    --output "Bin\Resource_v2\" \
    --backup \
    --continue-on-error
```

동작:
- 재귀적으로 모든 `.w*` 파일 스캔
- 매직/버전 읽고 v1 → v2 업그레이드 로직 실행
- 새 SHA256 재계산
- Ed25519 재서명 (키 있으면)

---

## 4. Minor 버전 업 예 — `.wmat` v1.1 (Phase E PBR)

Stage 5 문서에 예고한 Disney Principled BSDF 확장. 기존 `.wmat` v1.0 은 `MetallicScale`, `RoughnessScale` 만. v1.1 에서 `Anisotropy`, `ClearCoat`, `Sheen`, `Subsurface` 추가.

### 4.1 변경 포인트

```cpp
// v1.0 vs v1.1 POD 는 동일 (ScalarParam 배열이 유연하므로)
// version_minor 만 bump, 로더는 기존 그대로
```

### 4.2 호환성 모드

v1.1 로더가 v1.0 파일 읽기:
- `GetScalar(FNV1a("ClearCoat"), 0.0f)` → 기본값 0.0 반환
- 셰이더는 파라미터 없을 때 경로도 커버

v1.0 로더가 v1.1 파일 읽기 (구버전 DLL):
- `version_minor=1` 인데 기대 0 → 경고 로그, scalar 배열 size 가 크면 **추가 엔트리 무시**
- 셰이더는 확장 파라미터 없으면 기본 BRDF 사용

---

## 5. 번들 버전

### 5.1 번들 Major/Minor

번들 자체 (`.winters`) 도 Major/Minor 체계. 내부 에셋 버전과 독립:

```
Content.winters
  Bundle version: 1.0
  Contains:
    body.wmesh   (Mesh format 1.0)
    body.wmat    (Material format 1.1)
    stage1.wmap  (Map format 1.0)
```

번들 Major 가 오르면 TOC 레이아웃 변경 → 구 로더 거부.

### 5.2 혼합 버전 번들 (권장 X, 가능)

Minor 범위 내에선 한 번들에 여러 Minor 버전 에셋 가능. Major 는 번들 레벨에서 통일.

---

## 6. DLL 공개 키 버전 연계

Stage 8 키 회전에서 언급한 공개 키 버전도 포맷 버전과 **독립** 관리:

| 포맷 Ver | 공개 키 Ver | 호환성 |
|---|---|---|
| 1.0 | v1 | OK |
| 1.0 | v2 | OK (포맷은 그대로, 서명만 새 키) |
| 1.1 | v1 | OK (포맷 확장, 서명 구 키) |
| 2.0 | v2 | OK (Major 만 확인) |

---

## 7. 호환 테스트 매트릭스

CI 에서 자동 수행:

| Loader | Asset Ver | 기대 |
|---|---|---|
| v1.0 | v1.0 | ✅ Load |
| v1.1 | v1.0 | ✅ Load (missing field default) |
| v1.0 | v1.1 | ⚠️ Load + warn (ignore extra fields) |
| v1.x | v2.0 | ❌ `E_WINTERS_VERSION_MISMATCH` |
| v2.0 | v1.0 | ❌ `E_WINTERS_VERSION_MISMATCH` |

---

## 8. Stage1.dat → .wmap 마이그레이션 (실제 케이스)

현재 `STAGE_VERSION = 4`, `STAGE_VERSION_MIN_COMPAT = 3`. `.wmap` 으로 이관 시:

### 8.1 상황별 처리

| 기존 파일 | 조치 |
|---|---|
| `Stage1.dat` v3 | migrate subcommand 로 v1.0 `.wmap` 생성 |
| `Stage1.dat` v4 | 동일 — MinionWaypoint 섹션 포함 |
| `Stage1.dat` v < 3 | **재작성** (너무 오래됨, 컨버터 거부) |

### 8.2 명령

```
WintersAssetConverter.exe map "Data\Stage1.dat" -o "Data\Stage1.wmap" --scene-id 1
```

### 8.3 런타임 하이브리드 모드

현재 Client 는 `.dat` 만 로드. 과도기 동안:

```cpp
// Client/Private/Scene/Scene_InGame.cpp
if (FileExists(L"Data/Stage1.wmap")) {
    // 새 포맷 로드
    LoadWMap();
} else if (FileExists(L"Data/Stage1.dat")) {
    // 구 포맷 로드 + 변환 안내
    LoadLegacyDat();
#ifdef _DEBUG
    OutputDebugStringA("[WARN] Legacy .dat. Run WintersAssetConverter.exe map.\n");
#endif
}
```

---

## 9. 버전 정보 출력

`WintersAssetConverter.exe info` 가 버전 정보 노출:

```
$ info Content.winters
  Bundle version: 1.0
  TOC entries: 234
    body.wmesh           (Mesh 1.0)
    body.wmat            (Material 1.1)      ← 혼합 Minor 버전
    stage1.wmap          (Map 1.0)
    ...
  Signature: Ed25519 (key v2)
  Build time: 2026-05-01 14:23:12 UTC
```

---

## 10. 로더 지원 범위 상수

```cpp
// Engine/Public/AssetFormat/Versioning/SupportedVersions.h
namespace Winters::Asset::SupportedVersion
{
    // 각 포맷별 로더가 지원하는 범위
    constexpr VersionRange Mesh     = { 1, 1,  0, 2 };  // v1.0~1.2
    constexpr VersionRange Anim     = { 1, 1,  0, 1 };
    constexpr VersionRange Skel     = { 1, 1,  0, 1 };
    constexpr VersionRange Tex      = { 1, 1,  0, 3 };
    constexpr VersionRange Mat      = { 1, 1,  0, 2 };
    constexpr VersionRange Map      = { 1, 1,  0, 1 };
    constexpr VersionRange Bundle   = { 1, 1,  0, 2 };

    // 현재 빌드된 쓰기 버전 (컨버터가 내는 기본 Minor)
    constexpr uint16_t Mesh_Writer_Minor    = 0;
    constexpr uint16_t Anim_Writer_Minor    = 0;
    constexpr uint16_t Mat_Writer_Minor     = 1;   // Phase E 예정
    constexpr uint16_t Tex_Writer_Minor     = 0;
    constexpr uint16_t Map_Writer_Minor     = 0;
    constexpr uint16_t Bundle_Writer_Minor  = 0;
}
```

이 파일을 Major 버전 업마다 갱신 → 한 곳에서 버전 관리.

---

## 11. 마이그레이션 스크립트 운영

### 11.1 위치

```
Tools/Migration/
├── v1_to_v2/
│   ├── migrate_mesh.cpp
│   ├── migrate_map.cpp
│   └── ...
├── migrate_common.h
└── README.md
```

### 11.2 정기 정리

Major 3 로 가면 v1→v2, v2→v3 둘 다 유지해 **체인 마이그레이션** 가능:

```
v1 → v2 → v3
```

5+ Major 버전 후엔 오래된 변환 스크립트 제거 (프로덕션은 매번 최신으로 변환해 저장).

---

## 12. 오류 메시지 가이드

CLAUDE.md 보안 §5 (로그 누출 방지) 준수:

```cpp
// 🟢 OK (Release): 해시 에러 코드
"E_WINTERS_VERSION_MISMATCH (0xA0410002)"

// 🔴 금지 (Release): 상세 내용
"Bundle was built with version 1.3 but loader only supports up to 1.2. \
 Please update your client."

// 🟡 Debug 만
#ifdef _DEBUG
  OutputDebugStringA("[VER] Mesh file 1.3 vs loader 1.2 — ignoring extra fields\n");
#endif
```

---

## 13. 사용자 체감 UX

Major 버전 차이로 로드 실패 시:
- Error 코드 수신 → 자동 CDN 재다운로드 트리거
- 재다운로드 실패 → "클라이언트 업데이트 필요" 다이얼로그 → 업데이터 실행
- 업데이터는 새 DLL + 새 번들 세트 수신

```
┌─ Winters ─────────────────────────┐
│                                   │
│   클라이언트 업데이트 필요         │
│                                   │
│   에셋 포맷이 업데이트되었습니다. │
│   [업데이트 시작]                 │
│                                   │
└───────────────────────────────────┘
```

---

## 14. 완료 기준

- [ ] `VersionMigrator.h/cpp`
- [ ] `SupportedVersions.h` 포맷별 범위
- [ ] 모든 로더 `IsSupported()` 호출 통합
- [ ] 번들 버전 체크
- [ ] Minor 하위 호환 테스트 (기본값 보충)
- [ ] Major mismatch 에러 반환 테스트
- [ ] `migrate` subcommand (`v1_to_v2` 예시)
- [ ] 호환 테스트 매트릭스 CI 자동화
- [ ] 업데이터 핸드오프 설계 (구체 구현은 Phase 8 SDK)

---

## 15. 다음 단계

Stage 11 (Debug Tools) 로 이동 — ImGui 에셋 뷰어 + 해시 검증 UI + 런타임 대시보드.
