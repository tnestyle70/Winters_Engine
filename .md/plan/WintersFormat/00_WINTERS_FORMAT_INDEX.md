# Winters 자체 바이너리 포맷 계획 — `.winters` 에코시스템

> **목표**: Unreal `.uasset` / Source `.mdl` / id Tech `.bsp` 에 대응하는 Winters 고유 바이너리 에셋 파이프라인. FBX/glb/PNG 원본 → `.wmesh` / `.wanim` / `.wtex` / `.wmap` / `.winters 번들` 로 변환 + 런타임 zero-overhead 로드.

> **왜 자체 포맷**:
> 1. **로딩 속도** — 파싱 0, `memcpy`/`mmap` 으로 직접 GPU 업로드 가능
> 2. **무결성** — SHA256 내장, Ed25519 서명으로 치트 메시 교체 방지 (CLAUDE.md 보안 §9)
> 3. **독립성** — Assimp/DirectXTex 런타임 종속성 제거 (런타임 DLL 슬림화)
> 4. **버전 제어** — Major/Minor 체계로 에셋 마이그레이션 명시화
> 5. **패키징** — `.winters` 번들 하나로 게임 배포 (Valve VPK 스타일)

---

## 📂 파일 확장자 체계

| 확장자 | 용도 | 대체 대상 | 우선순위 |
|---|---|---|---|
| `.wmesh` | 정적/스키닝 메시 (정점+인덱스+본바인딩) | FBX/glb 런타임 파싱 | **1** |
| `.wanim` | 본별 키프레임 애니메이션 | FBX 애니 채널 | **2** |
| `.wskel` | 스켈레톤 계층 (본 리스트 + 부모 인덱스) | FBX 본 구조 | 2 |
| `.wtex` | 텍스처 (BC1~7 압축 + 밉맵) | PNG/DDS 런타임 파싱 | **3** |
| `.wmat` | 머티리얼 (셰이더 키 + 파라미터) | JSON/XML | 4 |
| `.wmap` | 맵 (NavGrid + Structure + Jungle + Waypoint) | 현재 `Stage1.dat` 승격 | **5** |
| `.wscene` | ECS 씬 스냅샷 (Entity + Component) | 신규 개념 | 6 |
| `.wfx` | Effect Tool 노드 그래프 | `.md/plan/EffectTool/` 산출물 | 7 (Effect Tool 이후) |
| `.winters` | **번들** — 여러 에셋 묶음 + TOC + 서명 | Valve `.vpk` | **8** (전부 구현 후) |

---

## 🧱 공통 헤더 (모든 `.w*` 파일)

**16 byte 헤더 + 페이로드 + 32 byte SHA256**

```cpp
#pragma pack(push, 1)
struct WintersFileHeader {
    char     magic[4];       // "WINT"
    uint16_t version_major;  // 1 (breaking 변경 시 증가)
    uint16_t version_minor;  // 0 (additive 변경 시 증가)
    uint32_t flags;          // bit0=compressed(LZ4), bit1=encrypted(AES-GCM), bit2=signed(Ed25519)
    uint32_t content_size;   // 헤더 뒤 페이로드 byte 수
};
#pragma pack(pop)
static_assert(sizeof(WintersFileHeader) == 16);

// 파일 레이아웃:
// [Header 16B] [Payload content_size B] [SHA256 32B]
// 만약 flags.signed → [Ed25519 Signature 64B] 추가
```

**엔디언**: Little-endian 고정 (Windows/x64 타겟, 향후 ARM/콘솔은 변환 레이어).

**매직 체크**: 로더 첫 줄에서 `memcmp(header.magic, "WINT", 4)` 불일치 시 `E_WINTERS_INVALID_MAGIC` 반환.

**해시 검증**: 페이로드 SHA256 계산 후 파일 끝 32B 와 비교. 보안 Phase 3 (커널 안티치트) 와 연동.

---

## 🗂 구현 스테이지 (8 단계)

| Stage | 파일 | 내용 | 선행 의존 |
|---|---|---|---|
| **1** | `01_ARCHITECTURE.md` | 전체 아키텍처 / BinaryReader/Writer 공통 유틸 | — |
| **2** | `02_STAGE1_COMMON_HEADER.md` | WintersFileHeader + SHA256 + 매직 검증 | 1 |
| **3** | `03_STAGE2_WMESH.md` | Assimp → `.wmesh` 컨버터 + 런타임 로더 | 2 |
| **4** | `04_STAGE3_WANIM_WSKEL.md` | 애니메이션/스켈레톤 분리 저장 | 2, 3 |
| **5** | `05_STAGE4_WTEX.md` | DirectXTex → BC7 `.wtex` (밉맵 포함) | 2 |
| **6** | `06_STAGE5_WMAT.md` | 머티리얼 직렬화 (셰이더 키 + 파라미터) | 2, 5 |
| **7** | `07_STAGE6_WMAP.md` | Stage1.dat → `.wmap` 마이그레이션 + NavGrid 포함 | 2 |
| **8** | `08_STAGE7_WBUNDLE.md` | `.winters` 번들 패키징 + TOC + 서명 | 3~7 전부 |
| **9** | `09_STAGE8_INTEGRITY.md` | Ed25519 서명 + 서버 키 발급 | 8 + Security Phase 2 |
| **10** | `10_CONVERTER_CLI.md` | `WintersAssetConverter.exe` 통합 CLI | 3~8 |
| **11** | `11_VERSIONING.md` | Major/Minor 마이그레이션 규칙 + 호환성 | — |
| **12** | `12_DEBUG_TOOLS.md` | ImGui 에셋 뷰어 / 해시 검증 패널 | 3~8 |

---

## 🔹 Stage 1 — Architecture (선행)

**핵심 유틸리티**
```cpp
// Engine/Public/AssetFormat/BinaryReader.h
class CBinaryReader {
    const uint8_t* m_pData;
    size_t         m_uCursor;
    size_t         m_uSize;
public:
    CBinaryReader(const void* p, size_t size);
    template<typename T> T Read();              // POD 한 개
    void ReadBytes(void* dst, size_t n);        // 원시 블록
    const uint8_t* Peek() const;                // mmap 직접 참조
    bool AtEnd() const;
};

// Engine/Public/AssetFormat/BinaryWriter.h
class CBinaryWriter {
    std::vector<uint8_t> m_vBuffer;
public:
    template<typename T> void Write(const T& v);
    void WriteBytes(const void* src, size_t n);
    const uint8_t* Data() const;
    size_t         Size() const;
    HRESULT SaveToFile(const wchar_t* path, uint32_t flags = 0);  // 헤더+페이로드+SHA256 자동
};

// Engine/Public/AssetFormat/WintersFile.h
class CWintersFile {
    WintersFileHeader m_header;
    std::vector<uint8_t> m_vPayload;
public:
    static HRESULT LoadFromDisk(const wchar_t* path, CWintersFile& out);
    bool VerifySHA256() const;                 // 파일 끝 32B 와 계산 해시 비교
    const WintersFileHeader& Header() const { return m_header; }
    const uint8_t* Payload() const { return m_vPayload.data(); }
    size_t         PayloadSize() const { return m_vPayload.size(); }
};
```

---

## 🔹 Stage 3 — `.wmesh` 상세 레이아웃 (가장 복잡, 우선 구현)

```
[ WintersFileHeader 16B ]
    magic="WINT", version_major=1, version_minor=0, content_size=...

[ Payload ]
    uint32_t submesh_count

    For each submesh:
        uint32_t vertex_count
        uint32_t index_count
        uint32_t vertex_format_flags   // Position(0x1)|Normal(0x2)|UV(0x4)|Tangent(0x8)|BoneWeight(0x10)
        uint32_t vertex_stride         // 계산 검증용
        uint8_t  vertex_data[vertex_count * vertex_stride]
        uint16_t index_data[index_count]    // or uint32_t if count > 65535
        uint32_t material_index             // .wmat 참조 index
        uint64_t material_hash              // 교차 검증용

    uint32_t bone_count
    For each bone:
        uint64_t name_hash                  // FNV-1a of wide string
        char     name[64]                   // 디버그용 (해시 충돌 시)
        int32_t  parent_index                // -1 = root
        float    offset_matrix[16]           // Assimp offset 이미 전치됨 (CLAUDE.md gotcha 준수)

    uint32_t lod_count                       // 0 = no LOD
    For each LOD:
        uint32_t vertex_count_ratio          // pct
        uint8_t* lod_vertex_data

[ SHA256 32B ]
```

**컨버터 (Python 또는 WintersAssetConverter)**:
```
tools/convert_mesh.py input.fbx -o output.wmesh
  → Assimp 로 scene 읽기
  → aiMatrix4x4 → XMFLOAT4X4 전치 (CLAUDE.md gotcha)
  → Vertex/Index/Bone 직렬화
  → SHA256 append
```

**런타임 로더** (`Engine/Public/Resource/MeshLoader_Winters.h`):
```cpp
class CMeshLoader_Winters {
public:
    static std::unique_ptr<CMesh> Load(const wchar_t* path);
private:
    // 페이로드 파싱 — memcpy 만으로 GPU 업로드 가능한 레이아웃
};
```

**성능 목표**: 500KB `.wmesh` 로드 < 1ms (현재 Assimp FBX 로드는 50~200ms).

---

## 🔹 Stage 7 — `.wmap` (Stage1.dat 승격)

현재 `Client/Private/Map/MapDataIO.cpp` 가 `.dat` POD 구조체 flat dump. 문제:
- 버전 관리 없음 (CLAUDE.md gotcha: POD 변경 시 STAGE_VERSION bump 수동)
- NavGrid 분리 저장 (별도 바이너리 또는 None)
- 해시 검증 없음

**`.wmap` 설계**:
```
Header (Magic=WINT, version, flags)
Payload:
    uint32_t scene_id
    uint32_t structure_count
    StructureEntry[]           // 기존 포맷 유지
    uint32_t jungle_count
    JungleEntry[]
    uint32_t waypoint_chain_count
    For each chain:
        uint8_t team, lane, type
        uint32_t point_count
        Vec3[]
    uint8_t navgrid_bits[11250]  // CNavGrid::kByteSize
    Vec2 navgrid_origin          // x, z 좌하단
SHA256
```

**마이그레이션**: 기존 `Stage1.dat` → `Stage1.wmap` 변환 스크립트 제공 + Scene_InGame 에서 두 포맷 모두 로드 지원 (점진 이관).

---

## 🔹 Stage 8 — `.winters` 번들

**Valve VPK + Unreal PAK 혼합**:
```
[ Bundle Header (64B) ]
    char magic[8] = "WINTPACK"
    uint32_t version
    uint64_t toc_offset
    uint32_t toc_count
    uint32_t content_flags       // LZ4/Encrypted/Signed 전체 옵션
    uint8_t  reserved[...]

[ TOC (toc_count * entry) ]
    uint64_t asset_name_hash     // FNV-1a of "Characters/Sylas/body.wmesh"
    uint32_t asset_type          // enum: Mesh/Anim/Tex/Mat/Map/Scene/FX
    uint64_t offset              // 번들 내 위치
    uint64_t size_compressed
    uint64_t size_uncompressed
    uint8_t  sha256[32]          // 개별 에셋 해시

[ Compressed Asset Blocks ]
    asset 1 data...
    asset 2 data...
    ...

[ Trailing Signature (선택) ]
    uint8_t ed25519_signature[64]   // 전체 번들 (header + TOC + data) 서명
```

**Loader**:
```cpp
class CWintersBundle {
    static std::unique_ptr<CWintersBundle> Open(const wchar_t* path);
    std::unique_ptr<CWintersFile> ExtractAsset(uint64_t nameHash);
    bool VerifySignature(const uint8_t* pPublicKey) const;
};
```

**사용 예**:
```cpp
auto bundle = CWintersBundle::Open(L"Content.winters");
auto mesh   = bundle->ExtractAsset(Hash("Characters/Sylas/body.wmesh"));
```

---

## 🔐 보안 / 무결성 (CLAUDE.md 보안 §9 연동)

### Level 1 — SHA256 무결성 (모든 파일 기본)
- 로드 시 `VerifySHA256()` 자동 호출
- 불일치 시 `E_WINTERS_HASH_MISMATCH` → 서버 신고 (치트 가능성)

### Level 2 — AES-GCM 암호화 (선택, flags.encrypted)
- 민감 에셋 (Champion 스탯 테이블 등) 만 암호화
- 키는 서버 로그인 시 세션별 발급
- 키 저장: `CryptProtectMemory` (Phase 8 C++ Client SDK)

### Level 3 — Ed25519 서명 (번들 전용)
- 번들 전체 해시를 개발 키로 서명
- Release 빌드에 공개 키 임베드 (DLL 내부)
- 서명 불일치 = 에셋 변조 = 강제 종료 + 서버 신고

---

## 🧪 버전 호환성 (`11_VERSIONING.md`)

| 변경 종류 | Major 증가 | Minor 증가 | 로더 동작 |
|---|---|---|---|
| 필드 순서 변경 | ✓ | — | 거부 (E_INCOMPATIBLE) |
| 필드 타입 변경 | ✓ | — | 거부 |
| 필드 뒤에 추가 | — | ✓ | 로드 (신규 필드 기본값) |
| 섹션 추가 (TOC 기반) | — | ✓ | 로드 |
| 섹션 제거 | ✓ | — | 거부 |

**마이그레이션 도구**: `tools/winters_migrate.py v1.0 → v2.0` (개별 파일 일괄 변환).

---

## 🔧 컨버터 CLI (`10_CONVERTER_CLI.md`)

```
WintersAssetConverter.exe
  mesh     input.fbx       -o output.wmesh
  anim     input.fbx        -o output.wanim
  tex      input.png        -o output.wtex --format BC7 --mips 10
  mat      input.json       -o output.wmat
  map      input.dat        -o output.wmap    # Stage1.dat 변환
  bundle   --input dir/     -o output.winters --sign key.pem
  verify   asset.wmesh      # SHA256 + magic 검증
  info     asset.wmesh      # 헤더 덤프 (디버그)
```

**통합 대상**: 현재 `Tools/WintersAssetConverter/` (CLAUDE.md 언급) 를 이 기능으로 확장.

---

## 📊 구현 우선순위 + 예상 소요

| Stage | 작업 | 예상 시간 | Phase 연계 |
|---|---|---|---|
| 1 | Architecture / BinaryReader/Writer | 0.5일 | 선행 |
| 2 | Common Header + SHA256 | 0.5일 | — |
| 3 | **.wmesh** (가장 중요) | 2일 | 챔피언 로딩 가속 |
| 4 | .wanim + .wskel | 1.5일 | — |
| 5 | .wtex (BC7) | 1일 | — |
| 6 | .wmat | 0.5일 | — |
| 7 | **.wmap** (Stage1.dat 승격) | 1일 | NavMesh (Phase 3-B) 와 연동 |
| 8 | .winters 번들 | 1.5일 | 배포 직전 |
| 9 | Ed25519 서명 | 1일 | Security Phase 2 이후 |
| 10 | Converter CLI | 1일 | — |
| 11 | Versioning 시스템 | 0.5일 | — |
| 12 | Debug Tools | 1일 | — |
| **합계** | | **12일** | |

---

## 🎯 전체 로드맵에서의 위치

```
현재: Phase 3-A (HealthBar) 완료 → Phase 3-B (NavMesh) 진행
         ↓
┌────────────────────────────────────────────────────┐
│  Phase 3-B: NavMesh 통합                            │
│  ├─ NavigationSystem World 등록                     │
│  ├─ Structure → NavGrid 차단                        │
│  ├─ 미니언/챔피언 NavAgent                          │
│  └─ (.wmap 으로 NavGrid 저장 — Stage 7 선행 통합)   │
└────────────────────────────────────────────────────┘
         ↓
  Profiler (ImGui 성능 패널) — 모든 후속 Phase 의 기반
         ↓
  Fiber & JobSystem — 병렬화 기반
         ↓
  🟢 **Winters Format Stage 1~3** (.wmesh 핵심)
  — 챔피언 로드 시간 50~200ms → 1ms
  — JobSystem 으로 .wmesh 멀티쓰레드 로드
         ↓
  Effect Tool (.wfx 는 Stage 7 이후 별도 Stage 로 추가)
         ↓
  Network (Backend SDK 완결)
         ↓
  Security (Level 1 SHA256 은 .winters Stage 1 에 이미 포함)
         ↓
  RenderGraph
         ↓
  GPU-Driven Pipeline
         ↓
  Champion AI (.md/plan/ai/ 연동)
```

---

## 📝 참고 문서

| 문서 | 경로 |
|---|---|
| CLAUDE.md 보안 §9 (에셋 치트 방지) | `CLAUDE.md` |
| 기존 Stage1.dat 포맷 | `Client/Private/Map/MapDataIO.cpp` |
| WintersAssetConverter | `Tools/WintersAssetConverter/` (현재 스켈레톤) |
| Assimp gotcha | CLAUDE.md "Assimp aiMatrix4x4 전치" |
| Effect Tool 플랜 | `.md/plan/EffectTool/00_EFFECT_TOOL_PLAN_INDEX.md` |
| Security 플랜 | `.md/plan/security/00_SECURITY_INDEX.md` |

---

## 다음 액션

1. **Stage 1 (Architecture) 파일 생성** — BinaryReader/Writer 유틸 구현
2. **Stage 2 (Common Header) 파일 생성** — WintersFileHeader + SHA256
3. **Stage 3 (`.wmesh`) 파일 생성** — 챔피언 5체 FBX → .wmesh 변환 + 로드 검증
4. 이후 병렬로 Stage 4~12 확장

**현재 이 INDEX 만 작성. 각 Stage 파일은 구현 시점에 작성 (Effect Tool / AI / Physics 플랜 동일 방식).**
