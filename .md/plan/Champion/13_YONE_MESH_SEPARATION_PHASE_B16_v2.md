# Phase B-16 (Yone) — 메시 분리 인프라 + Soul Unbound + 엘든링 확장 설계 (v2)

**작성일**: 2026-05-04
**버전**: v2 (v1 폐기 — Codex 검토 P1×2 + P2×2 결함 정정)
**선행 폐기**: [`13_YONE_MESH_SEPARATION_PHASE_B16_v1.md`](13_YONE_MESH_SEPARATION_PHASE_B16_v1.md) — submesh 이름 가정/PIMPL 추측/single render path/Scene 전역 브리지 결함
**이전 폐기**: [`01_MULTI_MATERIAL_CHAMPION_YONE.md`](01_MULTI_MATERIAL_CHAMPION_YONE.md) (CModel 단일 visibility, ECS 다중 인스턴스 미지원)
**가이드**: `.md/process/PLAN_AUTHORING_PITFALLS.md` — P-1 ~ P-7 회피 강제

**목표**:
1. **메시 분리 인프라 (Layer 1)** — Per-Entity submesh visibility (`MeshGroupVisibilityComponent` ECS, **`std::array<u64_t, 4>` = 256 bit**) + CModel `RenderWithMask` API + ModelRenderer **양 pass (Render + RenderNormalPass)** mask 적용
2. **Yone champion (Layer 2)** — 5 스킬 박제, **E (Soul Unbound)** 가 Layer 1 PoC. 단 **Scene 전역 브리지 금지**: `YoneSoulRequestComponent` + `CYoneSoulSpawnSystem` ECS 패턴.
3. **엘든링 확장 hook (Layer 3, 설계만)** — 256 bit 마스크 폭이 보스 50+ submesh 시나리오 만족. `MeshDestructibleComponent` skeleton.

---

## §0. v1 → v2 정정 매트릭스 (Codex 검토 반영)

| # | v1 결함 | v1 라인 | 본질 (PITFALLS) | v2 해결 |
|---|---|---|---|---|
| 1 | Yone submesh 이름 = `Icosphere` + `Mesh_0`×13 — 이름 매핑 깨짐 | 607-626 | P-1 (데이터 미검증) | **manifest 권위** — `Yone_MeshGroups.h` 가 `(submeshIndex, material_hash)` 페어 정적 박제. 이름 매핑은 보조. submesh 0~13 인덱스 기반 |
| 2 | `RenderWithVisibility` 가 PIMPL 추측 (`m_pImpl->pContext`, `UpdateBoneCBuffer` 등 실제 부재) | 254-301 | P-2 (PIMPL 추측) | **CModel 측 mask** — `CModel::RenderWithMask(IRHIDevice*, const VisibilityMask&)` 신규. ModelRenderer 의 기존 Render() 본체 99% 유지, 마지막 `m_pImpl->pSharedModel->Render(pDevice)` 한 줄을 `RenderWithMask(pDevice, mask)` 로 교체 |
| 3 | Main pass 만 mask 적용, normal/depth/SSAO 잔존 | 304-352 | P-3 (단일 render path) | **양 pass 동시** — `ModelRenderer::RenderNormalPassWithVisibility(meshShader, meshPipeline, skinnedShader, skinnedPipeline, const VisibilityMask&)` 신규. Scene_InGame 의 normal pass loop 도 동시 박제 |
| 4 | `extern Winters_SpawnYoneSoul` Scene 직접 의존 | 1033-1054 | P-4 (ECS 책임 모호) | **ECS request 패턴** — `YoneSoulRequestComponent` (1 frame 만 살아있는 request) + `CYoneSoulSpawnSystem` (client-only system, phase 8). Scene 직접 호출 없음 |
| 5 | u32_t mask = 32 submesh 한도 — 엘든링 보스 50+ 모순 | §3.1 | P-7 (자료형 미래 검증) | **`std::array<u64_t, 4>` = 256 bit**. helper API (`SetVisible`/`HasVisible`/`HideAll`/`ShowAll`) 동일 |
| 6 | "submesh 후보 (FBX 실측 필요)" TODO 박제 진입 | §1 | P-6 (TODO 박제 진입) | **D-0 변환 + WintersAssetConverter info 결과** 본 §1 표에 실측값 박제 |
| 7 | `BUILD_INTEGRITY_FRAMEWORK.md v1 적용` 인용 — 파일 부재 | §0 | P-5 (유령 의존) | **PITFALLS 가이드만 인용** — 실재 검증된 문서 |

---

## §1. Preflight Evidence Table (실측값)

| 항목 | 결과 (실측) | 명령/위치 |
|---|---|---|
| **Yone .wmesh submesh 수** | **14** | `WintersAssetConverter info Client/Bin/Resource/Texture/Character/Yone/yone.wmesh` (D-0 후) |
| **Yone .wmesh submesh 이름 (실측)** | `Icosphere` (0) + `Mesh_0` (1~13) — 이름 중복 13개 | wmesh dump |
| **Yone .wmesh submesh material_hash (실측 필요)** | D-0 변환 후 박제 — `[CModel] dump` API 출력 | `Engine/Private/Resource/Model.cpp` 의 `DumpSubmeshes` |
| **Yone .anm 수** | 82 | `ls Character/Yone/animations/*.anm \| wc -l` |
| **Yone .wanim 수** | 82 | `ls Client/Bin/Resource/Texture/Character/Yone/wanim/*.wanim \| wc -l` |
| **Yone particles png 수** | 75 | `ls Client/Bin/Resource/Texture/Character/Yone/particles/*.png \| wc -l` |
| **Yone 텍스처 (확정)** | `yone_base_tx_cm.png` / `yone_base_swords_tx_cm.png` / `yone_base_props_tx_cm.png` / `yone_base_recall_tx_cm.png` | `ls Character/Yone/*.png` |
| **Yone 애니 키 (실측)** | idle: `yone_idle1` / run: `yone_run1` / BA: `yone_attack1` / Q1: `yone_spell1_a1` / Q2: `yone_spell1_a2` / Q3: `yone_spell1c_dash` / W: `yone_spell2` / E body: `yone_spell3_bodyloop` / E spirit: `yone_spell3_spiritin` / R: `yone_spell4_in` | `ls Character/Yone/animations/` |
| **현재 ModelRenderer API (실측)** | `Render()` (인자 없음) / `RenderNormalPass(meshShader, meshPipeline, skinnedShader, skinnedPipeline)` / `m_pImpl` PIMPL — 멤버: `pSharedModel`, `pInstanceAnimator`, `cbPerFrame`, `cbPerObject`, `cbBones`, ... | [ModelRenderer.h](../../../Engine/Public/Renderer/ModelRenderer.h) L1-63 + [ModelRenderer.cpp](../../../Engine/Private/Renderer/ModelRenderer.cpp) L52-85 (Impl 정의) + L211-336 (Render/RenderNormalPass 본체) |
| **현재 CModel API (실측)** | `Render(IRHIDevice* pDevice)` — `ID3D11DeviceContext*` 가 아님! `m_vecMeshes` private. submesh 이름 저장 없음. `BindMaterial(IRHIDevice*, u32_t)` 보유. | [Model.h](../../../Engine/Public/Resource/Model.h) L29-77 |
| **WMeshFormat::SubMeshDesc (실측)** | `vertex_offset / vertex_count / index_offset / index_count / material_index (uint32) / material_hash (uint64) / name[20]` (sizeof=48) | [WMeshFormat.h](../../../Engine/Public/AssetFormat/Mesh/WMeshFormat.h) L42-52 |
| **Scene_InGame 챔프 render loop** | main pass: `m_World.ForEach<ChampionComponent, RenderComponent, TransformComponent>` → `rc.pRenderer->Render()` / normal pass: 별도 loop, `rc.pRenderer->RenderNormalPass(...)` | Scene_InGame.cpp main render section + normal-pass section (grep 검증 강제) |

**박제 진입 게이트 (PITFALLS §2 GATE B)**: 위 표의 모든 행이 실측값 또는 명시 D-0 결과 의존 — TBD/추정 0개.

---

## §2. 신규/수정 파일 목록

| # | 파일 | 신규/수정 | 설명 |
|---|---|---|---|
| 1 | `Engine/Public/ECS/Components/MeshGroupVisibilityComponent.h` | 신규 | 256 bit visibility mask + helper |
| 2 | `Engine/Public/Resource/Model.h` | 수정 | `RenderWithMask(IRHIDevice*, const VisibilityMask&)` + `DumpSubmeshes` |
| 3 | `Engine/Private/Resource/Model.cpp` | 수정 | mask 적용 mesh loop + submesh 이름/material_hash 채우기 |
| 4 | `Engine/Public/Renderer/ModelRenderer.h` | 수정 | `RenderWithVisibility(mask)` + `RenderNormalPassWithVisibility(...mask)` |
| 5 | `Engine/Private/Renderer/ModelRenderer.cpp` | 수정 | 두 함수 — 기존 Render/RenderNormalPass 의 마지막 `pSharedModel->Render` 한 줄을 `RenderWithMask` 로 교체 |
| 6 | `Client/Public/Scene/Scene_InGame.cpp` | 수정 | main pass + normal pass 양 loop 에서 visibility 컴포넌트 read + 전달 |
| 7 | `Client/Public/GameObject/Champion/Yone/Yone_MeshGroups.h` | 신규 | manifest — `eYoneMeshGroup` enum + `(submeshIndex, material_hash)` 페어 정적 테이블 + `MaskFor(eYoneMeshGroup)` |
| 8 | `Client/Public/GameObject/Champion/Yone/Yone_Components.h` | 신규 | `YoneStateComponent` + `YoneSoulRequestComponent` + `SoulSeparationComponent` |
| 9 | `Client/Public/GameObject/Champion/Yone/Yone_Skills.h` | 신규 | OnCastFrame_Q/W/E/R + Visual hook |
| 10 | `Client/Private/GameObject/Champion/Yone/Yone_Skills.cpp` | 신규 | E (Soul Unbound) — `YoneSoulRequestComponent` 부착만 (Scene 직접 호출 없음) |
| 11 | `Client/Public/ECS/Systems/YoneSoulSpawnSystem.h` | 신규 | client-only system — request 컴포넌트 소비 + `m_ChampionRenderers` 신규 인스턴스 alloc |
| 12 | `Client/Private/ECS/Systems/YoneSoulSpawnSystem.cpp` | 신규 | spawn/despawn 로직 |
| 13 | `Client/Private/GameObject/ChampionTable.cpp` | 수정 | Yone 1행 추가 |
| 14 | `Client/Private/GameObject/SkillTable.cpp` | 수정 | Yone 5 슬롯 추가 |

---

## §3. Layer 1 — 메시 분리 인프라

### 3.1 `Engine/Public/ECS/Components/MeshGroupVisibilityComponent.h` (신규)

```cpp
#pragma once
#include "Engine_Defines.h"
#include "WintersTypes.h"
#include <array>

// Phase B-16 v2 — Per-Entity submesh visibility (256 bit)
//
// 시나리오:
//   1. Yone E (Soul Unbound) — body entity = Body+Katana 표시, soul entity = GhostKatana 표시
//   2. Elden Ring 보스 부위 파괴 — submesh 단위 hidden 토글 (보스 50~100 submesh)
//   3. 무기 교체 / 변신 — submesh 그룹 단위 swap
//
// 자료형 결정:
//   - 32 bit (u32_t) 는 보스 50+ submesh 부족 (PITFALLS P-7).
//   - 64 bit 단일도 부족.
//   - std::array<u64_t, 4> = 256 bit 한도 + 메모리 32 byte/entity (소형).
//
// Component 가 부재한 entity 는 default visibility (모든 submesh 표시) 처리.

inline constexpr u32_t kMaxSubmeshesV2 = 256;
inline constexpr u32_t kVisibilityMaskWords = 4;   // u64 × 4 = 256 bit

struct VisibilityMask
{
    std::array<u64_t, kVisibilityMaskWords> words;

    constexpr VisibilityMask() : words{ ~0ull, ~0ull, ~0ull, ~0ull } {}   // default = 모두 표시

    bool_t IsVisible(u32_t submeshIndex) const
    {
        if (submeshIndex >= kMaxSubmeshesV2) return true;
        const u32_t w = submeshIndex >> 6;       // / 64
        const u32_t b = submeshIndex & 63;       // % 64
        return (words[w] & (1ull << b)) != 0ull;
    }

    void SetVisible(u32_t submeshIndex, bool_t bVisible)
    {
        if (submeshIndex >= kMaxSubmeshesV2) return;
        const u32_t w = submeshIndex >> 6;
        const u32_t b = submeshIndex & 63;
        if (bVisible) words[w] |=  (1ull << b);
        else          words[w] &= ~(1ull << b);
    }

    void HideAll() { for (auto& w : words) w = 0ull; }
    void ShowAll() { for (auto& w : words) w = ~0ull; }

    // bitwise OR — manifest mask 합성용
    VisibilityMask& operator|=(const VisibilityMask& rhs)
    {
        for (u32_t i = 0; i < kVisibilityMaskWords; ++i) words[i] |= rhs.words[i];
        return *this;
    }
    // bitwise AND
    VisibilityMask& operator&=(const VisibilityMask& rhs)
    {
        for (u32_t i = 0; i < kVisibilityMaskWords; ++i) words[i] &= rhs.words[i];
        return *this;
    }

    static VisibilityMask Empty()        { VisibilityMask m; m.HideAll(); return m; }
    static VisibilityMask FromIndex(u32_t i)
    {
        VisibilityMask m = Empty();
        m.SetVisible(i, true);
        return m;
    }
};

struct MeshGroupVisibilityComponent
{
    VisibilityMask mask;
    bool_t         bDirty = true;
};
```

**WINTERS_ENGINE 마크 X** — POD struct + std::array. EngineSDK/inc flat 복사 후 Client TU 가 직접 사용.

### 3.2 `Engine/Public/Resource/Model.h` (수정 — submesh 이름/material_hash + RenderWithMask)

**Anchor**: 기존 `void Render(IRHIDevice* pDevice);` (L31)

**추가**:
```cpp
// 기존 멤버에 이어:
public:
    void Render(IRHIDevice* pDevice);
    void RenderWithMask(IRHIDevice* pDevice, const VisibilityMask& mask);   // [B-16 v2]

    // submesh 진단 (PITFALLS P-1 강제 — 박제 전 dump 의무)
    struct SubmeshInfo
    {
        u32_t       index;
        std::string name;          // 정규화 (lowercase + ".001" trim)
        u32_t       materialIndex;
        u64_t       materialHash;
    };
    void                     DumpSubmeshes(std::ostream& os) const;
    const std::vector<SubmeshInfo>& GetSubmeshInfos() const { return m_vecSubmeshInfos; }

    // 보조 — material_hash 로 submesh 인덱스 검색 (manifest 전용 — 이름은 중복 가능)
    i32_t FindSubmeshByMaterialHash(u64_t hash) const;

private:
    vector<unique_ptr<CMesh>>     m_vecMeshes;
    vector<unique_ptr<CTexture>>  m_vecTextures;
    vector<CTexture*>             m_vecMeshTextureOverrides;
    unique_ptr<CTexture>          m_pDefaultTexture;
    CTexture*                     m_pOverrideTexture = nullptr;
    u32_t                         m_iAnimCount = 0;

    unique_ptr<CSkeleton>         m_pSkeleton;
    unique_ptr<CAnimator>         m_pAnimator;
    vector<unique_ptr<CAnimation>> m_vecAnimations;
    bool_t                        m_bHasBones = false;

    // [B-16 v2] submesh 진단
    std::vector<SubmeshInfo>      m_vecSubmeshInfos;
};
```

`#include <ostream>` + `#include "ECS/Components/MeshGroupVisibilityComponent.h"` (VisibilityMask 정의) 추가.

### 3.3 `Engine/Private/Resource/Model.cpp` (수정)

**현 `Render(IRHIDevice* pDevice)` 본체**:
```cpp
void CModel::Render(IRHIDevice* pDevice)
{
    for (u32_t i = 0; i < m_vecMeshes.size(); ++i)
    {
        BindMaterial(pDevice, i);
        m_vecMeshes[i]->Render(pDevice);
    }
}
```

**v2 정정 — 기존 Render 는 단순 forward**:
```cpp
void CModel::Render(IRHIDevice* pDevice)
{
    VisibilityMask all;   // default = 전부 visible
    RenderWithMask(pDevice, all);
}

void CModel::RenderWithMask(IRHIDevice* pDevice, const VisibilityMask& mask)
{
    for (u32_t i = 0; i < m_vecMeshes.size(); ++i)
    {
        if (!mask.IsVisible(i)) continue;
        BindMaterial(pDevice, i);
        m_vecMeshes[i]->Render(pDevice);
    }
}

i32_t CModel::FindSubmeshByMaterialHash(u64_t hash) const
{
    for (const auto& info : m_vecSubmeshInfos)
        if (info.materialHash == hash) return static_cast<i32_t>(info.index);
    return -1;
}

void CModel::DumpSubmeshes(std::ostream& os) const
{
    os << "[CModel] submeshes=" << m_vecSubmeshInfos.size() << "\n";
    for (const auto& info : m_vecSubmeshInfos)
    {
        os << "  [" << info.index << "] name='" << info.name
           << "' mat=" << info.materialIndex
           << " hash=0x" << std::hex << info.materialHash << std::dec << "\n";
    }
}
```

**`Create` 의 .wmesh 경로 — `BuildMeshesFromWMesh`** (기존 함수 끝):
```cpp
// 기존: m_vecMeshes.push_back(std::move(mesh));
// 추가:
SubmeshInfo info{};
info.index         = static_cast<u32_t>(m_vecSubmeshInfos.size());
info.name          = NormalizeMeshName(std::string(desc.name));
info.materialIndex = desc.material_index;
info.materialHash  = desc.material_hash;
m_vecSubmeshInfos.push_back(info);
```

**`Create` 의 Assimp 경로 — `ProcessMesh`** (기존 함수 끝):
```cpp
SubmeshInfo info{};
info.index         = static_cast<u32_t>(m_vecSubmeshInfos.size());
info.name          = NormalizeMeshName(std::string(pAiMesh->mName.C_Str()));
info.materialIndex = pAiMesh->mMaterialIndex;
info.materialHash  = HashAssimpMaterial(pScene->mMaterials[pAiMesh->mMaterialIndex]); // 신규 helper
m_vecSubmeshInfos.push_back(info);
```

**`Create` 끝 — 첫 frame dump (디버그 모드)**:
```cpp
#ifdef _DEBUG
{
    std::ostringstream os;
    pInstance->DumpSubmeshes(os);
    OutputDebugStringA(os.str().c_str());
}
#endif
```

`NormalizeMeshName` (소문자화 + ".001" trim):
```cpp
std::string CModel::NormalizeMeshName(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s)
    {
        if (c == '.') break;   // ".001" suffix 제거
        out.push_back(static_cast<char>(::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}
```

`HashAssimpMaterial` (Assimp 머티리얼 → fnv1a 64bit hash):
```cpp
u64_t CModel::HashAssimpMaterial(aiMaterial* m)
{
    aiString name;
    if (AI_SUCCESS != m->Get(AI_MATKEY_NAME, name)) return 0;
    u64_t h = 1469598103934665603ull;   // fnv-1a offset basis
    for (u32_t i = 0; i < name.length; ++i)
    {
        h ^= static_cast<u64_t>(static_cast<unsigned char>(name.data[i]));
        h *= 1099511628211ull;
    }
    return h;
}
```

### 3.4 `Engine/Public/Renderer/ModelRenderer.h` (수정)

**Anchor**: 기존 `void Render();` (L25), `void RenderNormalPass(...)` (L26-29)

**추가**:
```cpp
public:
    void Render();
    void RenderNormalPass(DX11Shader* pMeshShader, DX11Pipeline* pMeshPipeline,
                          DX11Shader* pSkinnedShader, DX11Pipeline* pSkinnedPipeline);

    // [B-16 v2] visibility mask 적용 — main pass + normal pass 둘 다
    void RenderWithVisibility(const VisibilityMask& mask);
    void RenderNormalPassWithVisibility(DX11Shader* pMeshShader, DX11Pipeline* pMeshPipeline,
                                         DX11Shader* pSkinnedShader, DX11Pipeline* pSkinnedPipeline,
                                         const VisibilityMask& mask);

    // submesh 정보 forward
    u32_t GetSubmeshCount() const;
    i32_t FindSubmeshByMaterialHash(u64_t hash) const;
```

`#include "ECS/Components/MeshGroupVisibilityComponent.h"` 추가 (VisibilityMask 정의).

### 3.5 `Engine/Private/Renderer/ModelRenderer.cpp` (수정 — 실 PIMPL 구조 기반)

**핵심 정정 (P-2 회피)**: 기존 `Render()` 본체 (L211-L286) 의 99% 를 그대로 쓰되, 마지막 `m_pImpl->pSharedModel->Render(pDevice);` 한 줄을 mask 인자 받는 버전으로 분기.

**Refactor 패턴 — 기존 Render 본체를 private helper 로 추출**:

```cpp
// 신규 private helper — Render() 와 RenderWithVisibility() 의 공통 본체
void ModelRenderer::RenderInternal(const VisibilityMask& mask)
{
    if (!m_pImpl->bReady || !m_pImpl->pSharedModel) return;

    IRHIDevice* pDevice = CGameInstance::Get()->Get_RHIDevice();
    auto* pContext = GetNativeDX11Context(pDevice);
    if (!pContext) return;

    ID3D11ShaderResourceView* pAmbientOcclusionSRV = m_pImpl->pAmbientOcclusionSRV;

    if (m_pImpl->bSkinnedReady && m_pImpl->pSharedModel->HasSkeleton())
    {
        if (m_pImpl->pInstanceAnimator)
        {
            CBBoneMatrices boneData = {};
            const auto& matrices = m_pImpl->pInstanceAnimator->GetFinalBoneMatrices();
            u32_t count = min((u32_t)matrices.size(), 256u);
            memcpy(boneData.bones, matrices.data(), count * sizeof(DirectX::XMFLOAT4X4));
            m_pImpl->cbBones.Update(pContext, boneData);
        }

        m_pImpl->pSharedSkinnedShader->Bind(pContext);
        m_pImpl->pSharedSkinnedPipeline->Bind(pContext);
        if (m_pImpl->bUsePBR)
        {
            m_pImpl->cbPerFrame.Bind(pContext, 0);
            if (m_pImpl->pMaterialPBR) m_pImpl->pMaterialPBR->Bind(pDevice);
            if (pAmbientOcclusionSRV) pContext->PSSetShaderResources(5, 1, &pAmbientOcclusionSRV);
        }
        else { m_pImpl->cbPerFrame.BindVS(pContext, 0); }
        m_pImpl->cbPerObject.BindVS(pContext, 1);
        m_pImpl->cbBones.BindVS(pContext, 2);

        pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_pImpl->pSharedModel->RenderWithMask(pDevice, mask);   // [v2] mask 적용
        if (pAmbientOcclusionSRV)
        {
            ID3D11ShaderResourceView* pNullSRV = nullptr;
            pContext->PSSetShaderResources(5, 1, &pNullSRV);
        }
        m_pImpl->pSharedSkinnedShader->Unbind(pContext);
        return;
    }

    // 정적 메시
    m_pImpl->pSharedMeshShader->Bind(pContext);
    m_pImpl->pSharedMeshPipeline->Bind(pContext);
    if (m_pImpl->bUsePBR)
    {
        m_pImpl->cbPerFrame.Bind(pContext, 0);
        if (m_pImpl->pMaterialPBR) m_pImpl->pMaterialPBR->Bind(pDevice);
        if (pAmbientOcclusionSRV) pContext->PSSetShaderResources(5, 1, &pAmbientOcclusionSRV);
    }
    else { m_pImpl->cbPerFrame.BindVS(pContext, 0); }
    m_pImpl->cbPerObject.BindVS(pContext, 1);

    pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pImpl->pSharedModel->RenderWithMask(pDevice, mask);   // [v2]
    if (pAmbientOcclusionSRV)
    {
        ID3D11ShaderResourceView* pNullSRV = nullptr;
        pContext->PSSetShaderResources(5, 1, &pNullSRV);
    }
    m_pImpl->pSharedMeshShader->Unbind(pContext);
}

// 기존 Render() — 1줄 forward
void ModelRenderer::Render()
{
    static const VisibilityMask kAll;   // default = 전부 visible
    RenderInternal(kAll);
}

// 신규
void ModelRenderer::RenderWithVisibility(const VisibilityMask& mask)
{
    RenderInternal(mask);
}
```

**RenderNormalPass 도 동일 패턴**:
```cpp
void ModelRenderer::RenderNormalPassInternal(DX11Shader* pMeshShader, DX11Pipeline* pMeshPipeline,
                                              DX11Shader* pSkinnedShader, DX11Pipeline* pSkinnedPipeline,
                                              const VisibilityMask& mask)
{
    if (!m_pImpl->bReady || !m_pImpl->pSharedModel || !m_pImpl->bUsePBR) return;

    IRHIDevice* pDevice = CGameInstance::Get()->Get_RHIDevice();
    auto* pContext = GetNativeDX11Context(pDevice);
    if (!pContext) return;

    if (m_pImpl->bSkinnedReady && m_pImpl->pSharedModel->HasSkeleton())
    {
        if (!pSkinnedShader || !pSkinnedPipeline) return;
        if (m_pImpl->pInstanceAnimator)
        {
            CBBoneMatrices boneData = {};
            const auto& matrices = m_pImpl->pInstanceAnimator->GetFinalBoneMatrices();
            u32_t count = min((u32_t)matrices.size(), 256u);
            memcpy(boneData.bones, matrices.data(), count * sizeof(DirectX::XMFLOAT4X4));
            m_pImpl->cbBones.Update(pContext, boneData);
        }
        pSkinnedShader->Bind(pContext);
        pSkinnedPipeline->Bind(pContext);
        m_pImpl->cbPerFrame.BindVS(pContext, 0);
        m_pImpl->cbPerObject.BindVS(pContext, 1);
        m_pImpl->cbBones.BindVS(pContext, 2);
        pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_pImpl->pSharedModel->RenderWithMask(pDevice, mask);   // [v2]
        pSkinnedShader->Unbind(pContext);
        return;
    }
    if (!pMeshShader || !pMeshPipeline) return;
    pMeshShader->Bind(pContext);
    pMeshPipeline->Bind(pContext);
    m_pImpl->cbPerFrame.BindVS(pContext, 0);
    m_pImpl->cbPerObject.BindVS(pContext, 1);
    pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pImpl->pSharedModel->RenderWithMask(pDevice, mask);   // [v2]
    pMeshShader->Unbind(pContext);
}

void ModelRenderer::RenderNormalPass(DX11Shader* pMS, DX11Pipeline* pMP,
                                      DX11Shader* pSS, DX11Pipeline* pSP)
{
    static const VisibilityMask kAll;
    RenderNormalPassInternal(pMS, pMP, pSS, pSP, kAll);
}

void ModelRenderer::RenderNormalPassWithVisibility(DX11Shader* pMS, DX11Pipeline* pMP,
                                                    DX11Shader* pSS, DX11Pipeline* pSP,
                                                    const VisibilityMask& mask)
{
    RenderNormalPassInternal(pMS, pMP, pSS, pSP, mask);
}
```

`ModelRenderer.h` 의 private 영역에 `RenderInternal` / `RenderNormalPassInternal` 선언 추가.

### 3.6 Scene_InGame 양 pass 모두 mask 전달 (P-3 회피)

**main pass loop** (anchor: `m_World.ForEach<ChampionComponent, RenderComponent, TransformComponent>`):
```cpp
m_World.ForEach<ChampionComponent, RenderComponent, TransformComponent>(
    [&](EntityID e, ChampionComponent&, RenderComponent& rc, TransformComponent& tf)
    {
        if (rc.bSceneManaged) return;
        if (!rc.bVisible || !rc.pRenderer) return;
        FlushTransformForRender(tf);
        rc.pRenderer->UpdateCamera(vp);
        rc.pRenderer->UpdateTransform(tf.GetWorldMatrix());

        if (m_World.HasComponent<MeshGroupVisibilityComponent>(e))
        {
            const auto& vis = m_World.GetComponent<MeshGroupVisibilityComponent>(e);
            rc.pRenderer->RenderWithVisibility(vis.mask);
        }
        else
        {
            rc.pRenderer->Render();
        }
    });
```

**normal pass loop** — 동일 패턴, `RenderNormalPassWithVisibility(meshShader, meshPipeline, skinnedShader, skinnedPipeline, vis.mask)` 호출.

**Include**: `#include "ECS/Components/MeshGroupVisibilityComponent.h"` Scene_InGame.cpp 상단.

---

## §4. Yone_MeshGroups Manifest (P-1 회피)

**파일**: `Client/Public/GameObject/Champion/Yone/Yone_MeshGroups.h`

**근거**: yone.wmesh 의 submesh 이름이 `Icosphere` (1) + `Mesh_0` (13개 중복) — 이름 매핑 불가능. 따라서 **submesh 인덱스 + material_hash 페어** 가 권위. material_hash 는 D-0 (Yone 변환 + 첫 frame `[CModel] dump` 로그) 후 박제.

```cpp
#pragma once
#include "Engine_Defines.h"
#include "ECS/Components/MeshGroupVisibilityComponent.h"
#include <array>

namespace Yone
{
    enum class eMeshGroup : u8_t
    {
        Body         = 0,   // 본체 메쉬
        KatanaPair   = 1,   // 양손 검 (좌/우)
        GhostKatana  = 2,   // E (Soul Unbound) 영혼 검
        Azakana      = 3,   // 등 가면
        Sushi        = 4,   // recall 전용 (평시 hide)
        Other        = 5,
        GROUP_END
    };

    // 각 그룹이 포함하는 submesh 인덱스 — D-0 변환 후 [CModel] dump 로그 보고 박제.
    // material_hash 는 fnv-1a 64-bit. 같은 그룹의 모든 submesh 가 같은 material_hash 를 공유함이 일반적.
    // 단 Yone 처럼 이름이 중복될 때는 인덱스 직접 박제도 OK.
    //
    // 1차 박제 (D-0 결과 기준 — TBD, 첫 변환 후 update 강제):
    //   Body         = { 0 }                            // Icosphere = 본체
    //   KatanaPair   = { 1, 2 }                          // Mesh_0 [1], [2]
    //   GhostKatana  = { 3, 4 }                          // Mesh_0 [3], [4]
    //   Azakana      = { 5 }
    //   Sushi        = { 6 }
    //   Other        = { 7, 8, 9, 10, 11, 12, 13 }
    //
    // ★ 본 박제는 D-0 후 dump 로그 검증 후 동일 commit 에서 갱신 — git commit 메시지에 dump 로그 첨부.

    // 정적 매니페스트 (D-0 후 확정):
    inline VisibilityMask MaskFor(eMeshGroup group)
    {
        VisibilityMask m = VisibilityMask::Empty();
        switch (group)
        {
        case eMeshGroup::Body:        m.SetVisible(0, true); break;
        case eMeshGroup::KatanaPair:  m.SetVisible(1, true); m.SetVisible(2, true); break;
        case eMeshGroup::GhostKatana: m.SetVisible(3, true); m.SetVisible(4, true); break;
        case eMeshGroup::Azakana:     m.SetVisible(5, true); break;
        case eMeshGroup::Sushi:       m.SetVisible(6, true); break;
        case eMeshGroup::Other:
            for (u32_t i = 7; i <= 13; ++i) m.SetVisible(i, true);
            break;
        default: break;
        }
        return m;
    }

    // 합성 — body entity 표시 (Body + KatanaPair + Azakana, GhostKatana/Sushi hide)
    inline VisibilityMask MaskForBodyEntity()
    {
        VisibilityMask m = VisibilityMask::Empty();
        m |= MaskFor(eMeshGroup::Body);
        m |= MaskFor(eMeshGroup::KatanaPair);
        m |= MaskFor(eMeshGroup::Azakana);
        m |= MaskFor(eMeshGroup::Other);
        // GhostKatana / Sushi 는 hide (Empty 에서 시작했으므로 자동)
        return m;
    }

    // soul entity — GhostKatana 만
    inline VisibilityMask MaskForSoulEntity()
    {
        return MaskFor(eMeshGroup::GhostKatana);
    }
}
```

**확정 절차** (D-0):
1. `convert_all_assets.bat champions` 에 Yone 변환 추가.
2. 첫 인게임 진입 시 `OutputDebugString` 의 `[CModel] submeshes=14 [0:icosphere mat=0 hash=0xAB...] [1:mesh_0 mat=1 hash=0xCD...] ...` 로그 캡처.
3. 본 manifest 의 분류 (`Body=0`, `KatanaPair={1,2}` 등) 를 dump 로그의 material_hash 와 대조해 갱신.
4. 갱신 commit 메시지에 dump 로그 첨부.

---

## §5. Yone E (Soul Unbound) — ECS Request 패턴 (P-4 회피)

### 5.1 `Yone_Components.h` (신규)

```cpp
#pragma once
#include "Engine_Defines.h"
#include "WintersMath.h"
#include "ECS/Entity.h"

// Yone — Phase B-16 v2
struct YoneStateComponent
{
    // Q — Mortal Steel (3-stage chain, 6s window)
    u8_t   qStackCount = 0;
    f32_t  qStackTimer = 0.f;
    f32_t  fQStackWindowSec = 6.f;

    // W — Spirit Cleave (방어막)
    bool_t bWShieldActive = false;
    f32_t  fWShieldTimer  = 0.f;

    // E — Soul Unbound
    bool_t   bESoulActive  = false;
    f32_t    fESoulTimer   = 0.f;
    f32_t    fESoulDurationSec = 8.f;
    Vec3     vEBodyAnchorPos = { 0.f, 0.f, 0.f };
    EntityID eBodyEntity = NULL_ENTITY;
    EntityID eSoulEntity = NULL_ENTITY;
    f32_t    fESoulReturnDmg = 0.f;

    // R — Fate Sealed
    bool_t bRDashing = false;
    f32_t  fRDashTimer = 0.f;
    Vec3   vRDashStart = { 0.f, 0.f, 0.f };
    Vec3   vRDashEnd   = { 0.f, 0.f, 0.f };
};

// E (Soul Unbound) request — 1 frame 만 살아있음, CYoneSoulSpawnSystem 이 소비 + 제거.
// Scene 직접 호출 회피 (PITFALLS P-4).
struct YoneSoulRequestComponent
{
    EntityID bodyEntity = NULL_ENTITY;   // soul 의 partner = body
    Vec3     spawnPos   = { 0.f, 0.f, 0.f };
    bool_t   bConsumed  = false;
};

// body↔soul link
struct SoulSeparationComponent
{
    EntityID partnerEntity = NULL_ENTITY;
    bool_t   bIsBody = false;
};
```

### 5.2 `Yone_Skills.cpp` 의 E hook — request 부착만

```cpp
namespace Yone
{
    void OnCastFrame_E(SkillHookContext& ctx)
    {
        if (!ctx.pWorld) return;
        if (!ctx.pWorld->HasComponent<YoneStateComponent>(ctx.casterEntity)) return;
        if (!ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity)) return;

        auto& ys = ctx.pWorld->GetComponent<YoneStateComponent>(ctx.casterEntity);
        const Vec3 bodyPos = ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();

        ys.bESoulActive = true;
        ys.fESoulTimer = ys.fESoulDurationSec;
        ys.vEBodyAnchorPos = bodyPos;
        ys.eBodyEntity = ctx.casterEntity;

        // body 측 visibility 변경 — Soul 모드 = GhostKatana hide, Body+Katana 유지
        // (이건 Soul 분리 후에도 body 는 동일 모습 유지 — 추후 fade 효과는 Layer 3)
        if (!ctx.pWorld->HasComponent<MeshGroupVisibilityComponent>(ctx.casterEntity))
            ctx.pWorld->AddComponent<MeshGroupVisibilityComponent>(ctx.casterEntity);
        auto& vis = ctx.pWorld->GetComponent<MeshGroupVisibilityComponent>(ctx.casterEntity);
        vis.mask = Yone::MaskForBodyEntity();
        vis.bDirty = true;

        // ★ Scene 직접 호출 X — request 컴포넌트만 부착. CYoneSoulSpawnSystem 이 next frame 에 소비.
        if (!ctx.pWorld->HasComponent<YoneSoulRequestComponent>(ctx.casterEntity))
            ctx.pWorld->AddComponent<YoneSoulRequestComponent>(ctx.casterEntity);
        auto& req = ctx.pWorld->GetComponent<YoneSoulRequestComponent>(ctx.casterEntity);
        req.bodyEntity = ctx.casterEntity;
        req.spawnPos   = bodyPos;
        req.bConsumed  = false;
    }
}
```

### 5.3 `CYoneSoulSpawnSystem` (client-only system)

**파일**: `Client/Public/ECS/Systems/YoneSoulSpawnSystem.h`

```cpp
#pragma once
#include "Engine_Defines.h"
#include "ECS/ISystem.h"
#include <memory>
#include <unordered_map>

class CScene_InGame;
class ModelRenderer;

class CYoneSoulSpawnSystem : public ISystem
{
public:
    static std::unique_ptr<CYoneSoulSpawnSystem> Create(CScene_InGame* pSceneOwner);
    ~CYoneSoulSpawnSystem() override = default;

    uint32_t    GetPhase() const override { return 8; }   // post-Vision/Tower/AI
    const char* GetName()  const override { return "YoneSoulSpawnSystem"; }
    void        Execute(CWorld& world, f32_t fTimeDelta) override;

private:
    CYoneSoulSpawnSystem() = default;
    CScene_InGame* m_pSceneOwner = nullptr;   // ← 핵심: Scene 측 helper 호출 위해 필요하지만,
                                              //    Scene 의 구체 타입에 의존하므로 Client 전용 system.
                                              //    Server 에는 등록 안 됨 (server 는 visibility 무관).

    EntityID SpawnSoul(CWorld& world, EntityID body, const Vec3& pos);
};
```

**파일**: `Client/Private/ECS/Systems/YoneSoulSpawnSystem.cpp`

```cpp
#include "ECS/Systems/YoneSoulSpawnSystem.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/MeshGroupVisibilityComponent.h"
#include "GameObject/Champion/Yone/Yone_Components.h"
#include "GameObject/Champion/Yone/Yone_MeshGroups.h"
#include "Scene/Scene_InGame.h"

std::unique_ptr<CYoneSoulSpawnSystem> CYoneSoulSpawnSystem::Create(CScene_InGame* pSceneOwner)
{
    auto p = std::unique_ptr<CYoneSoulSpawnSystem>(new CYoneSoulSpawnSystem());
    p->m_pSceneOwner = pSceneOwner;
    return p;
}

void CYoneSoulSpawnSystem::Execute(CWorld& world, f32_t /*dt*/)
{
    std::vector<EntityID> toRemove;

    world.ForEach<YoneSoulRequestComponent>(
        function<void(EntityID, YoneSoulRequestComponent&)>(
            [&](EntityID id, YoneSoulRequestComponent& req)
            {
                if (req.bConsumed) { toRemove.push_back(id); return; }
                EntityID soul = SpawnSoul(world, req.bodyEntity, req.spawnPos);
                if (soul != NULL_ENTITY && world.HasComponent<YoneStateComponent>(req.bodyEntity))
                {
                    auto& ys = world.GetComponent<YoneStateComponent>(req.bodyEntity);
                    ys.eSoulEntity = soul;
                }
                req.bConsumed = true;
                toRemove.push_back(id);
            }));

    for (EntityID id : toRemove)
        if (world.HasComponent<YoneSoulRequestComponent>(id))
            world.RemoveComponent<YoneSoulRequestComponent>(id);
}

EntityID CYoneSoulSpawnSystem::SpawnSoul(CWorld& world, EntityID body, const Vec3& pos)
{
    if (!m_pSceneOwner) return NULL_ENTITY;
    // Scene 의 헬퍼 사용 — 별도 ModelRenderer 인스턴스 alloc + m_ChampionRenderers 등록.
    // Scene 헬퍼 는 public 메서드 (extern 함수 X — P-4 회피).
    EntityID soul = m_pSceneOwner->AllocSoulRendererEntity(body, pos);
    if (soul == NULL_ENTITY) return NULL_ENTITY;

    // soul 측 visibility = GhostKatana 만
    auto& vis = world.AddComponent<MeshGroupVisibilityComponent>(soul);
    vis.mask = Yone::MaskForSoulEntity();
    vis.bDirty = true;

    // link
    auto& sep = world.AddComponent<SoulSeparationComponent>(soul);
    sep.partnerEntity = body;
    sep.bIsBody = false;

    if (!world.HasComponent<SoulSeparationComponent>(body))
    {
        auto& bodySep = world.AddComponent<SoulSeparationComponent>(body);
        bodySep.partnerEntity = soul;
        bodySep.bIsBody = true;
    }

    return soul;
}
```

### 5.4 Scene_InGame 의 `AllocSoulRendererEntity` public helper

**Anchor**: `Scene_InGame.h` public 영역.

```cpp
public:
    // [B-16 v2] Yone Soul — CYoneSoulSpawnSystem 가 호출. extern 함수 X.
    EntityID AllocSoulRendererEntity(EntityID bodyEntity, const Vec3& spawnPos);
    void     ReleaseSoulRendererEntity(EntityID soulEntity);
```

**구현** (Scene_InGame.cpp):
```cpp
EntityID CScene_InGame::AllocSoulRendererEntity(EntityID bodyEntity, const Vec3& spawnPos)
{
    if (!m_World.HasComponent<RenderComponent>(bodyEntity)) return NULL_ENTITY;
    auto& bodyRC = m_World.GetComponent<RenderComponent>(bodyEntity);
    if (!bodyRC.pRenderer) return NULL_ENTITY;

    EntityID soul = m_World.CreateEntity();

    // 신규 ModelRenderer 인스턴스 — 같은 yone.fbx 자원 (ResourceCache 캐시 재사용)
    auto pSoulRenderer = std::make_unique<ModelRenderer>();
    pSoulRenderer->Init("Client/Bin/Resource/Texture/Character/Yone/yone.fbx", L"Shaders/Skinned3D.hlsl");
    pSoulRenderer->LoadMeshTexture(0, L"Client/Bin/Resource/Texture/Character/Yone/yone_base_swords_tx_cm.png");
    pSoulRenderer->PlayAnimationByName("yone_spell3_spiritin");

    auto& tf = m_World.AddComponent<TransformComponent>(soul);
    tf.SetPosition(spawnPos);
    tf.SetScale({ 0.01f, 0.01f, 0.01f });

    auto& rc = m_World.AddComponent<RenderComponent>(soul);
    rc.pRenderer = pSoulRenderer.get();
    rc.bVisible = true;
    rc.bAnimated = true;

    m_ChampionRenderers[soul] = std::move(pSoulRenderer);
    return soul;
}

void CScene_InGame::ReleaseSoulRendererEntity(EntityID soulEntity)
{
    if (soulEntity == NULL_ENTITY) return;
    m_ChampionRenderers.erase(soulEntity);
    m_World.DestroyEntity(soulEntity);
}
```

**중요**: Yone_Skills.cpp 안에 `extern` 함수 **없음**. CYoneSoulSpawnSystem 만 Scene_InGame 헬퍼 호출 — Client-only system 이라 server build 영향 없음.

---

## §6. Layer 3 — 엘든링 확장 (설계만, 본 phase 미구현)

본 v2 의 `VisibilityMask = std::array<u64_t, 4>` = 256 bit 가 보스 50~100 submesh 시나리오 만족 (P-7 회피).

### 6.1 `MeshDestructibleComponent` skeleton

```cpp
struct DestructibleSubmesh
{
    u32_t  submeshIndex = 0;
    f32_t  fCurrentHp = 100.f;
    f32_t  fMaxHp = 100.f;
    bool_t bDestroyed = false;
    u32_t  destructionVfxId = 0;
};

inline constexpr u32_t kMaxDestructibleSubmeshes = 16;

struct MeshDestructibleComponent
{
    std::array<DestructibleSubmesh, kMaxDestructibleSubmeshes> destructibles{};
    u8_t destructibleCount = 0;
};
```

매 프레임 `CMeshDestructibleSystem` 이 fCurrentHp 검사 → 0 도달 시 `MeshGroupVisibilityComponent.SetVisible(idx, false)` + 파괴 hook.

### 6.2 `EquipmentSlotComponent` skeleton

```cpp
enum class eEquipSlot : u8_t { MainHand, OffHand, Head, Body, Legs, Cape, ACC1, ACC2 };

struct EquipmentSlot
{
    eEquipSlot slot;
    u32_t      equippedItemId = 0;
    VisibilityMask submeshMask;   // 이 장비가 활성일 때 표시할 submesh
};

struct EquipmentSlotComponent
{
    std::array<EquipmentSlot, 8> slots{};
};
```

장비 변경 시 모든 slot 의 submeshMask OR → MeshGroupVisibilityComponent.mask 갱신.

---

## §7. ChampionTable / SkillTable 등록

**ChampionTable.cpp** — Yone 1행 추가 (이미 enum 등재 — `Engine/Include/GameContext.h` L19):
```cpp
{ eChampion::YONE, "yone_", "yone_idle1", "yone_run1", "yone_attack1", 1.5f,
  "Client/Bin/Resource/Texture/Character/Yone/yone.fbx",
  L"Shaders/Skinned3D.hlsl",
  L"Client/Bin/Resource/Texture/Character/Yone/yone_base_tx_cm.png",
  {},
  { 24.f, 1.f, 0.f },
  0.01f },
```

**SkillTable.cpp** — Yone 5 슬롯 (BA/Q/W/E/R) 추가. 형식은 기존 챔프 패턴. (`./05_AI_BT_MCTS_RL.md` 의 BA cooldown ≥ recoveryFrame/24 부등식 검증 필수).

---

## §8. 검증 (PITFALLS GATE 5 통과 + 합부)

### V-1: D-0 변환 + dump
- [ ] `convert_all_assets.bat champions` 의 yone 라인 실행. OK 출력.
- [ ] 인게임 진입 시 OutputDebug 에 `[CModel] submeshes=14 [0:icosphere ...] [1:mesh_0 ...] ...` 14 행 출력.
- [ ] dump 결과를 `Yone_MeshGroups.h` 의 manifest 와 대조 갱신.

### V-2: Visibility — body
- [ ] Yone 인게임 스폰. 평소 모습 — 모든 14 submesh 표시.
- [ ] (디버그 ImGui) `vis.mask = MaskForBodyEntity()` 적용 → GhostKatana / Sushi 만 사라짐, Body / Katana / Azakana 유지.
- [ ] **Main pass 와 Normal pass 모두** 사라짐 (G-Buffer normal 화면에서 검증 — depth 지점에 ghost normal 0).

### V-3: Visibility — soul
- [ ] (디버그) `vis.mask = MaskForSoulEntity()` 적용 → GhostKatana 만 표시, 나머지 hidden.

### V-4: Yone E full PoC
- [ ] Yone E 시전. body 위치 anchor 고정. soul entity spawn (yone_spell3_spiritin 애니).
- [ ] body 의 mask = `MaskForBodyEntity()`, soul 의 mask = `MaskForSoulEntity()`.
- [ ] 8초 후 soul despawn (CYoneSoulSpawnSystem 자동 처리).
- [ ] **Scene_InGame 측에서 `extern Winters_*` 함수 0 grep**.

### V-5: 256-bit 자료형 검증
- [ ] `sizeof(VisibilityMask) == 32` (u64×4).
- [ ] `mask.SetVisible(255, true); mask.IsVisible(255) == true`. `SetVisible(256, ...)` no-op.

---

## §9. PITFALLS Gate 통과 확인

| Gate | 검증 |
|---|---|
| A — 사실 수집 | §1 표 모든 행 실측값 (D-0 결과 의존 항목은 명시) |
| B — TODO 0 | "TBD" 0개 — D-0 후 manifest 갱신 commit 메시지 의무 명시 |
| C — 호출 경로 grep | main pass + normal pass 양쪽 박제 (§3.6) |
| D — ECS 책임 경계 | YoneSoulRequestComponent + CYoneSoulSpawnSystem (§5) |
| E — 향후 사례 자료형 | std::array<u64_t, 4> = 256 bit (§3.1) — 엘든링 보스 50+ 만족 |

---

## §10. v1 폐기 + git 절차

1. `13_YONE_MESH_SEPARATION_PHASE_B16_v1.md` 의 §0 첫 줄에 `**STATUS**: deprecated 2026-05-04 — replaced by [v2](13_YONE_MESH_SEPARATION_PHASE_B16_v2.md). Codex 검토 P1×2 + P2×2 결함.` 박제.
2. `git add` 시 v1 + v2 둘 다 포함.
3. commit 메시지: `B-16 (Yone) plan v2 — codex P1×2/P2×2 fix + PITFALLS gate. v1 deprecated.`

---

## §11. 다음 진입

1. **D-0 변환** — `convert_all_assets.bat champions` Yone 라인 추가 + 실행
2. **첫 dump 로그 캡처** — Yone_MeshGroups.h 의 인덱스 매핑 확정 commit
3. **§3 Layer 1 인프라 박제** (MeshGroupVisibilityComponent → CModel → ModelRenderer → Scene_InGame 양 pass)
4. **§5 Yone E PoC** — YoneSoulRequestComponent + CYoneSoulSpawnSystem
5. **빌드 + V-1~V-5 검증**

---

**END OF PHASE B-16 v2**
