# Stage 5 — `.wmat` 머티리얼 (셰이더 키 + 파라미터 + 텍스처 참조)

> **목표**: JSON 머티리얼 → 바이너리 `.wmat` 승격. 런타임 `.wmesh::SubMesh` 의 `material_hash` 로 조회 → 즉시 셰이더 + cbuffer + SRV 바인딩.

> **Phase E 연계**: BRDF → PBR → Path Tracing 로 확장될 때 같은 `.wmat` 스키마에 field 추가 (Minor 버전 bump). Disney Principled BSDF (metallic / roughness / specular / anisotropy / clearcoat) 필드 미리 예약.

---

## 1. 포맷 전제

### 1.1 머티리얼 = "어떻게 그릴지" 의 요약

| 구성 요소 | 예 |
|---|---|
| **Shader Key** | `"Mesh3D_Skinned"` / `"PBR_Metal"` / `"Unlit_Billboard"` |
| **Texture Slot** | Diffuse, Normal, MR, Emissive, AO, LUT |
| **Scalar Param** | MetallicScale, RoughnessScale, AlphaCutoff |
| **Vec Param** | BaseColor, EmissiveColor |
| **Render State** | BlendMode, CullMode, DepthTest, DepthWrite |

### 1.2 셰이더 키 레지스트리

런타임에 `CShaderRegistry` 가 이름 → 컴파일된 `ID3D11VertexShader` / `ID3D11PixelShader` 맵. `.wmat` 은 문자열/해시 로만 참조:

```cpp
// Engine/Public/Renderer/ShaderRegistry.h
class CShaderRegistry
{
public:
    struct ShaderPair
    {
        ComPtr<ID3D11VertexShader> vs;
        ComPtr<ID3D11PixelShader>  ps;
        ComPtr<ID3D11InputLayout>  il;
        uint32_t vertex_format;    // VF_STATIC / VF_SKINNED
    };
    void Register(const std::string& name, ShaderPair pair);
    const ShaderPair* Find(uint64_t nameHash) const;
};
```

---

## 2. 파일 레이아웃

```
[ WintersFileHeader 16B ]
[ Payload ]
    MatMetaHeader                 (40 B)
    char shader_name[64]          (nul-terminated)
    uint64_t shader_name_hash
    TextureSlot[texture_count]    (각 80 B : channel + path[64] + flags)
    ScalarParam[scalar_count]     (각 40 B : name[32] + float)
    VectorParam[vector_count]     (각 48 B : name[32] + float4)
    RenderState                   (32 B)
[ SHA256 32B ]
```

### 2.1 POD

```cpp
// Engine/Public/AssetFormat/Material/WMatFormat.h
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include <cstdint>

namespace Winters::Asset
{
    constexpr char WMAT_MAGIC[4] = { 'W','M','A','T' };

    enum eTextureChannel : uint32_t
    {
        TEX_CH_Diffuse        = 0,
        TEX_CH_Normal         = 1,
        TEX_CH_MetallicRough  = 2,
        TEX_CH_Emissive       = 3,
        TEX_CH_AmbientOcc     = 4,
        TEX_CH_Specular       = 5,
        TEX_CH_Opacity        = 6,
        TEX_CH_Detail         = 7,
        TEX_CH_Anisotropy     = 8,    // Phase E Disney
        TEX_CH_ClearCoat      = 9,
        TEX_CH_SubsurfaceTint = 10,
        TEX_CH_Custom0        = 64,
        TEX_CH_Custom1        = 65,
    };

    enum eBlendMode : uint8_t
    {
        BLEND_Opaque              = 0,
        BLEND_AlphaTest           = 1,
        BLEND_AlphaBlend          = 2,
        BLEND_Additive            = 3,
        BLEND_Premultiplied       = 4,
        BLEND_Multiplicative      = 5,
    };

    enum eCullMode : uint8_t
    {
        CULL_None  = 0,
        CULL_Front = 1,
        CULL_Back  = 2,
    };

    #pragma pack(push, 1)
    struct MatMetaHeader
    {
        char     magic[4];           // "WMAT"
        uint64_t shader_name_hash;   // FNV-1a("Mesh3D_Skinned")
        uint32_t texture_count;
        uint32_t scalar_count;
        uint32_t vector_count;
        uint32_t material_name_hash; // .wmesh SubMeshDesc::material_hash 와 매칭
        uint32_t reserved;
    };
    static_assert(sizeof(MatMetaHeader) == 32);

    struct TextureSlot
    {
        uint32_t channel;                 // eTextureChannel
        char     path[64];                // "Characters/Irelia/textures/body_diffuse.wtex"
        uint64_t path_hash;               // FNV-1a (경로 변경 내구성)
        uint8_t  is_srgb;
        uint8_t  has_alpha;
        uint8_t  sampler_preset;          // 0=Linear Wrap, 1=Anisotropic, 2=Point Clamp 등
        uint8_t  reserved;
        uint32_t reserved1;
    };
    static_assert(sizeof(TextureSlot) == 88);

    struct ScalarParam
    {
        char  name[32];
        float value;
        uint32_t reserved;
    };
    static_assert(sizeof(ScalarParam) == 40);

    struct VectorParam
    {
        char  name[32];
        float x, y, z, w;
    };
    static_assert(sizeof(VectorParam) == 48);

    struct RenderState
    {
        uint8_t blend_mode;      // eBlendMode
        uint8_t cull_mode;       // eCullMode
        uint8_t depth_test;      // 0=none, 1=less, 2=lequal, 3=always
        uint8_t depth_write;     // 0/1
        uint8_t stencil_enable;
        uint8_t reserved[3];
        uint32_t reserved1[6];   // 32 B 정렬
    };
    static_assert(sizeof(RenderState) == 32);
    #pragma pack(pop)
}
```

---

## 3. JSON 원본 (컨버터 입력)

```json
// Bin/Resource/Characters/Irelia/body.mat.json
{
  "name": "Irelia_Body",
  "shader": "Mesh3D_Skinned",
  "textures": [
    { "channel": "Diffuse",       "path": "Characters/Irelia/textures/body_diffuse.wtex",        "srgb": true  },
    { "channel": "Normal",        "path": "Characters/Irelia/textures/body_normal.wtex",         "srgb": false },
    { "channel": "MetallicRough", "path": "Characters/Irelia/textures/body_mr.wtex",             "srgb": false }
  ],
  "scalars": [
    { "name": "MetallicScale",  "value": 1.0 },
    { "name": "RoughnessScale", "value": 1.0 },
    { "name": "AlphaCutoff",    "value": 0.5 }
  ],
  "vectors": [
    { "name": "BaseColor",      "value": [1, 1, 1, 1] },
    { "name": "EmissiveColor",  "value": [0, 0, 0, 0] }
  ],
  "renderState": {
    "blendMode": "Opaque",
    "cullMode":  "Back",
    "depthTest": "lequal",
    "depthWrite": true
  }
}
```

---

## 4. 컨버터

```cpp
// Engine/Private/AssetFormat/Material/WMatWriter.cpp
HRESULT CWMatWriter::WriteFromJson(const wchar_t* pJsonPath, const wchar_t* pOutPath)
{
    // 1. JSON 로드 (nlohmann::json)
    std::ifstream f(pJsonPath);
    nlohmann::json j; f >> j;

    MatMetaHeader hdr{};
    std::memcpy(hdr.magic, WMAT_MAGIC, 4);

    const std::string shaderName = j["shader"].get<std::string>();
    hdr.shader_name_hash = FNV1a(shaderName);

    const std::string matName = j["name"].get<std::string>();
    hdr.material_name_hash = (uint32_t)FNV1a(matName);

    std::vector<TextureSlot> slots;
    for (const auto& t : j["textures"]) {
        TextureSlot s{};
        s.channel = ChannelFromString(t["channel"]);
        std::strncpy(s.path, t["path"].get<std::string>().c_str(), sizeof(s.path) - 1);
        s.path_hash = FNV1a(s.path);
        s.is_srgb   = t.value("srgb", false) ? 1 : 0;
        s.has_alpha = t.value("alpha", false) ? 1 : 0;
        s.sampler_preset = SamplerFromString(t.value("sampler", std::string("LinearWrap")));
        slots.push_back(s);
    }
    hdr.texture_count = (uint32_t)slots.size();

    std::vector<ScalarParam> scalars;
    for (const auto& p : j.value("scalars", nlohmann::json::array())) {
        ScalarParam s{};
        std::strncpy(s.name, p["name"].get<std::string>().c_str(), sizeof(s.name) - 1);
        s.value = p["value"].get<float>();
        scalars.push_back(s);
    }
    hdr.scalar_count = (uint32_t)scalars.size();

    std::vector<VectorParam> vectors;
    for (const auto& p : j.value("vectors", nlohmann::json::array())) {
        VectorParam v{};
        std::strncpy(v.name, p["name"].get<std::string>().c_str(), sizeof(v.name) - 1);
        auto arr = p["value"];
        v.x = arr[0]; v.y = arr[1]; v.z = arr[2]; v.w = arr[3];
        vectors.push_back(v);
    }
    hdr.vector_count = (uint32_t)vectors.size();

    RenderState rs = ParseRenderState(j.value("renderState", nlohmann::json::object()));

    // 2. 직렬화
    CBinaryWriter w;
    w.Write(hdr);

    char shaderBuf[64]{};
    std::strncpy(shaderBuf, shaderName.c_str(), sizeof(shaderBuf) - 1);
    w.WriteBytes(shaderBuf, sizeof(shaderBuf));

    for (auto& s : slots)    w.Write(s);
    for (auto& s : scalars)  w.Write(s);
    for (auto& v : vectors)  w.Write(v);
    w.Write(rs);

    return w.SaveToFile(pOutPath, 0);
}
```

---

## 5. 런타임 머티리얼

### 5.1 CMaterial

```cpp
// Engine/Public/Resource/Material.h
class CMaterial
{
public:
    uint64_t ShaderNameHash() const { return m_shaderHash; }
    uint32_t NameHash()       const { return m_nameHash;   }

    ID3D11ShaderResourceView* GetSRV(eTextureChannel ch) const;
    float  GetScalar(uint64_t nameHash, float fDefault = 0) const;
    Vec4   GetVector(uint64_t nameHash, const Vec4& vDefault = {}) const;

    const RenderState& GetRenderState() const { return m_renderState; }

    // Phase C-5 이후 per-material cbuffer 통합
    void BindToPipeline(ID3D11DeviceContext* ctx) const;

private:
    uint64_t m_shaderHash = 0;
    uint32_t m_nameHash   = 0;
    RenderState m_renderState{};

    // 슬롯별 Texture
    std::unordered_map<uint32_t, std::shared_ptr<CTexture>> m_textures;

    // 파라미터 — 런타임 조회용 맵 + cbuffer 백킹
    std::unordered_map<uint64_t, float> m_scalars;
    std::unordered_map<uint64_t, Vec4>  m_vectors;

    // cbuffer 자동 빌드 (Material cbuffer = Scalar 16개 + Vector 8개 고정 slot)
    void BuildCBuffer();
    ComPtr<ID3D11Buffer> m_pMaterialCB;
};
```

### 5.2 Per-material cbuffer (b3 슬롯)

CLAUDE.md HLSL 컨벤션: b0=PerFrame, b1=PerObject, b2=BoneMatrices → **b3=PerMaterial** 신규.

```hlsl
// Shaders/Material_CB.hlsli
cbuffer PerMaterial : register(b3)
{
    float4 u_BaseColor;
    float4 u_EmissiveColor;
    float  u_MetallicScale;
    float  u_RoughnessScale;
    float  u_AlphaCutoff;
    float  _pad0;
    // ... 총 16 B × 16 슬롯 = 256 B
};

Texture2D t_Diffuse  : register(t0);
Texture2D t_Normal   : register(t1);
Texture2D t_MR       : register(t2);
SamplerState s_Aniso : register(s0);
```

---

## 6. 로더

```cpp
// Engine/Public/AssetFormat/Material/WMatLoader.h
namespace Winters::Asset
{
    class WINTERS_API CWMatLoader
    {
    public:
        static std::shared_ptr<CMaterial> Load(CDevice* pDevice, const std::wstring& path);
    };
}

// 구현 — 텍스처 참조는 지연 로드 (ResourceCache 경유)
std::shared_ptr<CMaterial> CWMatLoader::Load(CDevice* pDevice, const std::wstring& path)
{
    CWintersFile file;
    if (FAILED(CWintersFile::LoadFromDisk(path.c_str(), file))) return nullptr;

    CBinaryReader r(file.Decompressed(), file.DecompressedSize());
    auto hdr = r.Read<MatMetaHeader>();
    if (std::memcmp(hdr.magic, WMAT_MAGIC, 4) != 0) return nullptr;

    char shaderBuf[64]{};
    r.ReadBytes(shaderBuf, 64);

    auto mat = std::make_shared<CMaterial>();
    mat->m_shaderHash = hdr.shader_name_hash;
    mat->m_nameHash   = hdr.material_name_hash;

    for (uint32_t i = 0; i < hdr.texture_count; ++i) {
        auto slot = r.Read<TextureSlot>();
        auto tex  = CGameInstance::Get()->Get_ResourceCache()->LoadTexture(
                        WidenFromAscii(slot.path));
        mat->m_textures[slot.channel] = tex;
    }

    for (uint32_t i = 0; i < hdr.scalar_count; ++i) {
        auto s = r.Read<ScalarParam>();
        mat->m_scalars[FNV1a(s.name)] = s.value;
    }
    for (uint32_t i = 0; i < hdr.vector_count; ++i) {
        auto v = r.Read<VectorParam>();
        mat->m_vectors[FNV1a(v.name)] = Vec4{ v.x, v.y, v.z, v.w };
    }
    mat->m_renderState = r.Read<RenderState>();

    mat->BuildCBuffer();
    return mat;
}
```

---

## 7. `.wmesh` ↔ `.wmat` 연결

```cpp
// Client/Private/Scene/Scene_InGame.cpp
void Scene_InGame::RenderChampion(const CMesh* mesh)
{
    for (uint32_t i = 0; i < mesh->GetSubMeshCount(); ++i) {
        const auto& sub = mesh->GetSubMesh(i);
        const auto& mat = LookupMaterialByHash(sub.material_hash);
        mat->BindToPipeline(ctx);
        ctx->DrawIndexed(sub.index_count, sub.index_offset / 4, 0);
    }
}
```

`LookupMaterialByHash` 는 챔피언 로드 시 `body.wmat.json` + `blade.wmat.json` 등을 `material_name_hash` → `CMaterial*` 맵에 채워둔 것. `.wmesh` 의 `SubMeshDesc::material_hash` 는 `material_name_hash` 와 동일.

---

## 8. 현재 LoL 레거시 어댑터

CLAUDE.md Gotcha "FBX 머티리얼 텍스처 경로 없을 수 있음 (LoL 추출)": LoL FBX 는 머티리얼 metadata 비어있음. 컨버터가:
1. FBX → `.wmesh` 변환 시 머티리얼 정보 추출 시도
2. 없으면 **디폴트 `.wmat.json` 템플릿 생성** (`--generate-mat-stub`)
3. 아티스트가 수동으로 채움 (body/blade 구분 등)

```bat
:: Tools/convert_champion_mats.bat
WintersAssetConverter.exe mesh body.fbx -o body.wmesh --generate-mat-stub
:: 결과: body.wmesh + body_Mesh_0.mat.json + body_Mesh_1.mat.json 자동 생성
```

---

## 9. Phase E PBR 확장 대비

### 9.1 Disney Principled BSDF 필드

현재 Stage 5 에는 metallic / roughness 만. Phase E 에서 Minor 버전 1.1 bump:

```cpp
// version_minor = 1 시 추가되는 scalar (기본 로더는 못 찾으면 기본값)
ScalarParam[] {
    { "Anisotropy",    0.0 },
    { "ClearCoat",     0.0 },
    { "ClearCoatGloss",0.0 },
    { "Sheen",         0.0 },
    { "SheenTint",     0.5 },
    { "Subsurface",    0.0 },
    { "Specular",      0.5 },   // IOR override (1.5 → 0.5)
    { "SpecTint",      0.0 },
}
```

`GetScalar(hash, default)` 시 못 찾으면 `default` 반환 → 하위 호환.

### 9.2 IBL (Stage 5 말단)

```cpp
// CMaterial 에 IBL 슬롯 — 씬 전역 머티리얼 아닌 per-object 이면 per-mat 에 세팅 가능
void CMaterial::BindIBLCubes(ID3D11ShaderResourceView* irradiance,
                              ID3D11ShaderResourceView* prefilter,
                              ID3D11ShaderResourceView* brdfLUT);
```

Phase E 에서 셰이더 쪽 register 할당.

---

## 10. 보안 고려사항

| 위협 | 방어 |
|---|---|
| 치트가 `.wmat` BlendMode 를 Opaque → Additive (투명 캐릭터) | SHA256 + Ed25519 |
| 경로 traversal (`path = "../../../windows/system32/..."`) | 로더에서 `path` 에 `..`, `:`, 절대경로 포함 시 거부 |
| scalar/vector_count 거대값 | 상한 `MAX_PARAMS = 256` 검증 |

```cpp
HRESULT ValidateMatMeta(const MatMetaHeader& hdr, const TextureSlot& slot)
{
    if (hdr.texture_count > 32)   return E_WINTERS_SIZE_OVERFLOW;
    if (hdr.scalar_count > 256)   return E_WINTERS_SIZE_OVERFLOW;
    if (hdr.vector_count > 256)   return E_WINTERS_SIZE_OVERFLOW;

    // 경로 검증
    std::string p = slot.path;
    if (p.find("..") != std::string::npos) return E_WINTERS_INVALID_MAGIC;
    if (p.size() >= 2 && p[1] == ':')       return E_WINTERS_INVALID_MAGIC;
    if (!p.empty() && p[0] == '/')          return E_WINTERS_INVALID_MAGIC;
    return S_OK;
}
```

---

## 11. 완료 기준

- [ ] `WMatFormat.h` POD + static_assert
- [ ] `WMatWriter.cpp` JSON → binary
- [ ] `WMatLoader.cpp` → CMaterial + 텍스처 간접 참조
- [ ] CMaterial::BindToPipeline 경로 구현
- [ ] b3 PerMaterial cbuffer 셰이더 통합
- [ ] 챔피언 5체 머티리얼 변환 후 렌더 동일
- [ ] path traversal / 상한 validator 테스트

---

## 12. 다음 단계

Stage 6 (`.wmap`) 로 이동 — 기존 `Stage1.dat` 승격 + NavGrid 통합.
