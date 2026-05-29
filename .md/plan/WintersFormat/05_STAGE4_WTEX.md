# Stage 4 — `.wtex` 텍스처 (BC7 + 밉맵 + Cube/Array 지원)

> **목표**: PNG/TGA/DDS 런타임 의존 제거. DirectXTex 로 **컨버터 타임에 BC7 압축 + 밉맵** 생성 → 런타임은 `CreateTexture2D` + `pSysMem` 직행.
> 현재 PNG 로딩 100~300 ms → 4 MB BC7 `.wtex` **< 3 ms** (I/O + GPU 업로드).

---

## 1. 왜 BC7

| 포맷 | 압축률 | 알파 | 품질 | HW 비용 |
|---|---|---|---|---|
| BC1 (DXT1) | 6:1 | 1bit | 중 | 모든 GPU |
| BC3 (DXT5) | 4:1 | 8bit | 중 | 모든 GPU |
| BC5 | 4:1 | — | 노말맵용 | 모든 GPU |
| **BC7** | **4:1** | **8bit** | **높음** | DX11+ |
| BC6H | 4:1 | HDR | HDR 고품질 | DX11+ |

**LoL / 엘든링** — BC7 (컬러/알파), BC5 (노말맵), BC6H (HDR 환경맵). BC1~3 레거시 fallback.

---

## 2. 파일 레이아웃

```
[ WintersFileHeader 16B ]  flags=LZ4 비추천(BC7 자체가 압축)
[ Payload ]
    TexMetaHeader              (40 B)
    MipDesc[mip_count]         (각 16 B)
        - level_byte_offset
        - level_byte_size
        - width, height
    Raw Block Data             (BC 블록 연속 — mip 0 부터)
[ SHA256 32B ]
```

### 2.1 POD

```cpp
// Engine/Public/AssetFormat/Texture/WTexFormat.h
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include <cstdint>

namespace Winters::Asset
{
    constexpr char WTEX_MAGIC[4] = { 'W','T','E','X' };

    enum eTexFormat : uint32_t
    {
        TEX_FORMAT_BC1_UNORM      = 71,   // DXGI_FORMAT 값과 일치
        TEX_FORMAT_BC3_UNORM      = 77,
        TEX_FORMAT_BC4_UNORM      = 80,
        TEX_FORMAT_BC5_UNORM      = 83,
        TEX_FORMAT_BC6H_UF16      = 95,
        TEX_FORMAT_BC7_UNORM      = 98,
        TEX_FORMAT_BC7_UNORM_SRGB = 99,
        TEX_FORMAT_R8G8B8A8_UNORM = 28,   // uncompressed fallback
        TEX_FORMAT_R16G16B16A16_FLOAT = 10,
    };

    enum eTexDimension : uint8_t
    {
        TEX_DIM_2D    = 0,
        TEX_DIM_CUBE  = 1,   // array_size = 6
        TEX_DIM_2D_ARRAY = 2,
        TEX_DIM_3D    = 3,   // depth > 1
    };

    #pragma pack(push, 1)
    struct TexMetaHeader
    {
        char     magic[4];           // "WTEX"
        uint32_t width;
        uint32_t height;
        uint32_t depth;              // 3D 용 — 2D 는 1
        uint32_t mip_count;
        uint32_t array_size;         // Cube = 6
        uint32_t format;             // eTexFormat
        uint8_t  dimension;          // eTexDimension
        uint8_t  is_srgb;
        uint8_t  has_alpha;
        uint8_t  reserved0;
        uint32_t reserved[2];
    };
    static_assert(sizeof(TexMetaHeader) == 40);

    struct MipDesc
    {
        uint32_t byte_offset;        // Payload 내 블록 시작
        uint32_t byte_size;          // 이 밉 총 크기
        uint16_t width;
        uint16_t height;
        uint32_t row_pitch;          // D3D11_SUBRESOURCE_DATA.SysMemPitch
    };
    static_assert(sizeof(MipDesc) == 16);
    #pragma pack(pop)
}
```

---

## 3. 컨버터 — DirectXTex → `.wtex`

### 3.1 API

```cpp
// Engine/Public/AssetFormat/Texture/WTexWriter.h
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"

namespace Winters::Asset
{
    struct WTexWriteOptions
    {
        eTexFormat format          = TEX_FORMAT_BC7_UNORM;
        bool_t     bGenMips        = true;
        uint32_t   uMaxMip         = 0;        // 0 = 전체
        bool_t     bSRGB           = true;     // diffuse 는 true, normal 은 false
        bool_t     bFast           = false;    // BC7 fast mode (품질 ↓ 속도 ↑)
        bool_t     bPreserveAlpha  = true;     // BC7 mode 6/7 선호
    };

    class WINTERS_API CWTexWriter
    {
    public:
        // PNG / TGA / DDS / BMP / JPG → .wtex
        static HRESULT WriteFromFile(const wchar_t* pInputPath,
                                      const wchar_t* pOutPath,
                                      const WTexWriteOptions& opt);

        // 메모리 바이트 (glb 임베디드 텍스처 등)
        static HRESULT WriteFromMemory(const void* pData, size_t uSize,
                                         const wchar_t* pOutPath,
                                         const WTexWriteOptions& opt);
    };
}
```

### 3.2 파이프라인

```cpp
// Engine/Private/AssetFormat/Texture/WTexWriter.cpp
#include "WTexWriter.h"
#include <DirectXTex.h>
using namespace DirectX;

HRESULT CWTexWriter::WriteFromFile(const wchar_t* pInputPath,
                                    const wchar_t* pOutPath,
                                    const WTexWriteOptions& opt)
{
    // 1. 디스크에서 이미지 로드 (PNG/TGA/DDS 자동 인식)
    ScratchImage src;
    HRESULT hr = LoadFromWICFile(pInputPath, WIC_FLAGS_NONE, nullptr, src);
    if (FAILED(hr)) hr = LoadFromTGAFile(pInputPath, nullptr, src);
    if (FAILED(hr)) hr = LoadFromDDSFile(pInputPath, DDS_FLAGS_NONE, nullptr, src);
    if (FAILED(hr)) return hr;

    // 2. 밉맵 생성
    ScratchImage mipped;
    if (opt.bGenMips) {
        hr = GenerateMipMaps(src.GetImages(), src.GetImageCount(), src.GetMetadata(),
                              TEX_FILTER_DEFAULT, opt.uMaxMip, mipped);
        if (FAILED(hr)) return hr;
    } else {
        mipped = std::move(src);
    }

    // 3. BC 인코딩
    ScratchImage compressed;
    const DXGI_FORMAT dx = (DXGI_FORMAT)opt.format;
    if (IsCompressed(dx)) {
        TEX_COMPRESS_FLAGS flags = TEX_COMPRESS_DEFAULT;
        if (opt.bFast) flags |= TEX_COMPRESS_BC7_QUICK;
        if (opt.bPreserveAlpha) flags |= TEX_COMPRESS_BC7_USE_3SUBSETS;

        hr = Compress(mipped.GetImages(), mipped.GetImageCount(), mipped.GetMetadata(),
                       dx, flags, TEX_THRESHOLD_DEFAULT, compressed);
        if (FAILED(hr)) return hr;
    } else {
        compressed = std::move(mipped);
    }

    // 4. TexMetaHeader 구성
    const auto& meta = compressed.GetMetadata();
    TexMetaHeader hdr{};
    std::memcpy(hdr.magic, WTEX_MAGIC, 4);
    hdr.width       = (uint32_t)meta.width;
    hdr.height      = (uint32_t)meta.height;
    hdr.depth       = (uint32_t)meta.depth;
    hdr.mip_count   = (uint32_t)meta.mipLevels;
    hdr.array_size  = (uint32_t)meta.arraySize;
    hdr.format      = (uint32_t)meta.format;
    hdr.dimension   = (meta.dimension == TEX_DIMENSION_TEXTURE3D) ? TEX_DIM_3D
                      : (meta.IsCubemap() ? TEX_DIM_CUBE : TEX_DIM_2D);
    hdr.is_srgb     = IsSRGB(meta.format) ? 1 : 0;
    hdr.has_alpha   = compressed.IsAlphaAllOpaque() ? 0 : 1;

    // 5. 밉별 Desc + 블록 데이터 취합
    std::vector<MipDesc> descs;
    std::vector<uint8_t> blocks;
    uint32_t cursor = 0;
    for (uint32_t m = 0; m < meta.mipLevels; ++m) {
        const Image* img = compressed.GetImage(m, 0, 0);
        MipDesc d{};
        d.byte_offset = cursor;
        d.byte_size   = (uint32_t)img->slicePitch;
        d.width       = (uint16_t)img->width;
        d.height      = (uint16_t)img->height;
        d.row_pitch   = (uint32_t)img->rowPitch;
        descs.push_back(d);

        blocks.insert(blocks.end(), img->pixels, img->pixels + img->slicePitch);
        cursor += d.byte_size;
    }

    // 6. 직렬화
    CBinaryWriter w;
    w.Write(hdr);
    for (auto& d : descs) w.Write(d);
    w.WriteBytes(blocks.data(), blocks.size());

    return w.SaveToFile(pOutPath, 0);  // BC7 자체 압축 → LZ4 생략
}
```

---

## 4. 런타임 로더

```cpp
// Engine/Public/AssetFormat/Texture/WTexLoader.h
#pragma once
#include "WintersAPI.h"

namespace Winters { class CTexture; class CDevice; }

namespace Winters::Asset
{
    class WINTERS_API CWTexLoader
    {
    public:
        static std::shared_ptr<CTexture> Load(CDevice* pDevice,
                                                const std::wstring& path);

        // 번들 내부 mmap 바이트 (zero-copy)
        static std::shared_ptr<CTexture> LoadFromMemory(CDevice* pDevice,
                                                         const void* pData, size_t uSize);
    };
}
```

### 4.1 구현 (DX11 직행)

```cpp
std::shared_ptr<CTexture> CWTexLoader::LoadFromMemory(CDevice* pDevice,
                                                       const void* pData, size_t uSize)
{
    CBinaryReader r(pData, uSize);

    auto hdr = r.Read<TexMetaHeader>();
    if (std::memcmp(hdr.magic, WTEX_MAGIC, 4) != 0) return nullptr;

    std::vector<MipDesc> mips(hdr.mip_count);
    r.ReadBytes(mips.data(), sizeof(MipDesc) * hdr.mip_count);

    // mmap 바이트 포인터 그대로 D3D11_SUBRESOURCE_DATA 에
    const uint8_t* pBlocks = r.Peek();

    std::vector<D3D11_SUBRESOURCE_DATA> subs(hdr.mip_count * hdr.array_size);
    for (uint32_t a = 0; a < hdr.array_size; ++a) {
        for (uint32_t m = 0; m < hdr.mip_count; ++m) {
            auto& s = subs[a * hdr.mip_count + m];
            s.pSysMem          = pBlocks + mips[m].byte_offset
                                 + a * (hdr.width * hdr.height * 4);  // array stride 계산
            s.SysMemPitch      = mips[m].row_pitch;
            s.SysMemSlicePitch = mips[m].byte_size;
        }
    }

    // DX11 리소스 생성
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width            = hdr.width;
    desc.Height           = hdr.height;
    desc.MipLevels        = hdr.mip_count;
    desc.ArraySize        = hdr.array_size;
    desc.Format           = (DXGI_FORMAT)hdr.format;
    desc.SampleDesc.Count = 1;
    desc.Usage            = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
    if (hdr.dimension == TEX_DIM_CUBE) desc.MiscFlags |= D3D11_RESOURCE_MISC_TEXTURECUBE;

    ComPtr<ID3D11Texture2D> tex;
    HRESULT hr = pDevice->GetRaw()->CreateTexture2D(&desc, subs.data(), &tex);
    if (FAILED(hr)) return nullptr;

    ComPtr<ID3D11ShaderResourceView> srv;
    D3D11_SHADER_RESOURCE_VIEW_DESC vd{};
    vd.Format = desc.Format;
    if (hdr.dimension == TEX_DIM_CUBE) {
        vd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
        vd.TextureCube.MipLevels = hdr.mip_count;
    } else {
        vd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        vd.Texture2D.MipLevels = hdr.mip_count;
    }
    pDevice->GetRaw()->CreateShaderResourceView(tex.Get(), &vd, &srv);

    auto out = std::make_shared<CTexture>();
    out->Attach(tex, srv, hdr.format, hdr.width, hdr.height);
    return out;
}
```

**Zero-copy 여부 주의**: `D3D11_USAGE_IMMUTABLE` + `pSysMem` 조합에선 DX 가 내부 복사. 하지만 이는 스테이징 버퍼 + GPU 업로드 최적화를 드라이버가 수행. 우리는 PNG 디코딩 0 + BC7 재압축 0 + mmap 한 번만 → **PNG 대비 30~100 배 개선**.

---

## 5. 챔피언 텍스처 변환 매트릭스

| 용도 | 원본 | 포맷 | SRGB |
|---|---|---|---|
| Diffuse / Albedo | PNG RGBA8 | BC7 | Yes |
| Normal Map | PNG RGB8 | BC5 | No |
| Metallic-Roughness | PNG RG8 | BC5 | No |
| Emissive | PNG RGB8 | BC7 | Yes |
| Ambient Occlusion | PNG R8 | BC4 | No |
| HDR Skybox (Phase E IBL) | HDR | BC6H | No |

### 5.1 변환 스크립트

```bat
:: Tools/convert_champion_textures.bat
:: English comments only (CLAUDE.md gotcha)
@echo off
setlocal EnableDelayedExpansion
for %%C in (Irelia Yasuo Sylas Viego Kalista) do (
    for %%F in ("Bin\Resource\Characters\%%C\textures\*_diffuse.png") do (
        WintersAssetConverter.exe tex "%%F" -o "%%~dpnF.wtex" --format BC7 --srgb --mips
    )
    for %%F in ("Bin\Resource\Characters\%%C\textures\*_normal.png") do (
        WintersAssetConverter.exe tex "%%F" -o "%%~dpnF.wtex" --format BC5 --no-srgb --mips
    )
)
```

---

## 6. 성능 측정

| 에셋 | PNG (현재) | `.wtex` BC7 (목표) |
|---|---|---|
| Irelia body_diffuse.png (2048×2048) | 220 ms | **< 3 ms** |
| 맵 디퓨즈 4 K | 800 ms | **< 8 ms** |
| 챔피언 5체 × 8 텍스처 전체 | ~8 s | **< 120 ms** |
| 디스크 크기 | 120 MB (PNG) | ~35 MB (BC7) |

---

## 7. BC7 품질 체크

BC7 은 diffuse 에 거의 무손실이지만 **고주파 패턴 (벽돌, 격자)** 은 아티팩트 발생. 품질 옵션:

| 모드 | 속도 | 품질 | 용도 |
|---|---|---|---|
| `--quick` | 빠름 | 중 | 개발 중 반복 |
| (기본) | 보통 | 상 | 프로덕션 |
| `--slow` | 느림 | 최상 | Release 빌드 |

**개발 편의**: Debug 빌드는 `--quick`, Release 는 `--slow`. 빌드 스크립트 분기.

---

## 8. 밉맵 필터링

DirectXTex `GenerateMipMaps`:
- `TEX_FILTER_DEFAULT` (Fant) — 고품질 (권장)
- `TEX_FILTER_LINEAR` — 빠름
- `TEX_FILTER_SEPARATE_ALPHA` — 알파 블리딩 방지 (foliage 등)

Winters 기본: `TEX_FILTER_DEFAULT | TEX_FILTER_SEPARATE_ALPHA`.

---

## 9. SRGB 정책

CLAUDE.md HLSL 컨벤션 연계:
- Diffuse 샘플링: HLSL 에선 sRGB → linear 자동 변환 (`BC7_UNORM_SRGB`)
- Normal / Roughness / Metallic: linear 공간 (`BC5_UNORM` / `BC7_UNORM`)
- 출력: Backbuffer `R8G8B8A8_UNORM_SRGB` → HLSL `return float4(color, 1)` 그대로

`is_srgb` 필드를 로더가 SRV 생성 시 사용.

---

## 10. CTexture 확장

```cpp
// Engine/Public/Resource/Texture.h
class CTexture
{
public:
    void Attach(ComPtr<ID3D11Texture2D> tex, ComPtr<ID3D11ShaderResourceView> srv,
                 uint32_t uFormat, uint32_t uW, uint32_t uH);

    ID3D11ShaderResourceView* GetSRV() const { return m_pSRV.Get(); }
    uint32_t GetWidth()  const { return m_uWidth;  }
    uint32_t GetHeight() const { return m_uHeight; }
    uint32_t GetFormat() const { return m_uFormat; }
    bool_t   IsSRGB()    const { return m_bSRGB;   }

private:
    ComPtr<ID3D11Texture2D>          m_pTex;
    ComPtr<ID3D11ShaderResourceView> m_pSRV;
    uint32_t m_uFormat = 0, m_uWidth = 0, m_uHeight = 0;
    bool_t   m_bSRGB = false;
};
```

---

## 11. 보안 고려사항

| 위협 | 방어 |
|---|---|
| 치트가 diffuse.wtex 를 완전 투명으로 교체 (상대 캐릭터 안 보이게) | SHA256 검증 + Stage 9 Ed25519 서명 |
| 거대 width/height (OOM) | 상한 `MAX_TEX_DIM = 16384` 검증 |
| mip_count 조작 (밉 오프셋 범위 초과) | 오프셋 합이 payload 크기 초과하면 거부 |
| byte_offset + byte_size 오버플로우 (정수 래핑) | uint64_t 로 계산 후 비교 |

### 11.1 Validator

```cpp
HRESULT ValidateTexMeta(const TexMetaHeader& hdr, size_t payloadSize)
{
    constexpr uint32_t MAX_DIM   = 16384;
    constexpr uint32_t MAX_MIPS  = 16;

    if (hdr.width > MAX_DIM || hdr.height > MAX_DIM)  return E_WINTERS_SIZE_OVERFLOW;
    if (hdr.mip_count > MAX_MIPS)                      return E_WINTERS_SIZE_OVERFLOW;
    if (hdr.array_size > 2048)                         return E_WINTERS_SIZE_OVERFLOW;

    // payload 크기 정합성 — 아래 로직은 로더에서 MipDesc 읽은 후 수행
    return S_OK;
}
```

---

## 12. 테스트

- [ ] 2048×2048 PNG RGBA → BC7 SRGB → 로드 → 시각 diff 육안 확인
- [ ] Normal map PNG RGB → BC5 → 레거시 PNG 와 렌더 일치
- [ ] 큐브맵 Skybox 6면 → BC7_SRGB / BC6H → IBL 전 / 후 sky
- [ ] 밉맵 자동 생성 (16×16 까지) 확인
- [ ] Release 모드 `--slow` 빌드 전체 소요 시간 (목표 < 5분)
- [ ] 변조 테스트 — BC7 블록 1 바이트 변경 → SHA256 거부
- [ ] 3D 볼륨 텍스처 (Phase E VXGI 대비) round-trip

---

## 13. 외부 의존성

| 라이브러리 | 위치 | 용도 |
|---|---|---|
| **DirectXTex** | `Engine/ThirdPartyLib/DirectXTK/` (이미 편입) | BC7 인코딩, 밉맵 생성, 로더 |

DirectXTex 는 DirectXTK 와 별도 라이브러리. `Engine/ThirdPartyLib/DirectXTex/` 신규 폴더 + `.md/build/THIRDPARTY_INTEGRATION_GUIDE.md` 절차 준수.

---

## 14. 완료 기준

- [ ] `WTexFormat.h` POD + static_assert
- [ ] `WTexWriter.cpp` WIC/TGA/DDS 로더 + Compress + 직렬화
- [ ] `WTexLoader.cpp` mmap → DX11 IMMUTABLE 2D
- [ ] 챔피언 5체 텍스처 전체 변환 + 로드 확인
- [ ] Profiler `AssetFormat.Load.Texture` 카테고리 측정 < 3 ms
- [ ] Release `--slow` vs Debug `--quick` 분기 확인
- [ ] 변조/거대/누락 3개 validator 테스트

---

## 15. 다음 단계

Stage 5 (`.wmat`) 로 이동 — 머티리얼 (셰이더 키 + 파라미터 + 텍스처 참조).
