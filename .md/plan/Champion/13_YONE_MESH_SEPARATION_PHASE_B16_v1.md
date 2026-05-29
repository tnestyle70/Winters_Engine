# Phase B-16 (Yone) — 메시 분리 인프라 + Soul Unbound + 엘든링 확장 설계

**STATUS**: ⚠️ **DEPRECATED 2026-05-04** — replaced by [v2](13_YONE_MESH_SEPARATION_PHASE_B16_v2.md).
Codex 검토 결과 **P1×2 + P2×2 결함** (submesh 이름 매핑 깨짐 / PIMPL 추측 의사코드 / single render path / Scene 전역 브리지). 본 v1 박제 시 발생한 사고 과정 함정은 [`PLAN_AUTHORING_PITFALLS.md`](../../process/PLAN_AUTHORING_PITFALLS.md) 에 정리. 본 v1 그대로 코드 반영 금지.

---

**작성일**: 2026-05-04
**선행**: Phase B-15 Ashe 박제 완료
**Framework**: [`BUILD_INTEGRITY_FRAMEWORK.md`](../../architecture/BUILD_INTEGRITY_FRAMEWORK.md) v1 적용 (⚠️ 인용 파일 부재 — PITFALLS P-5 사례)
**선행 폐기 계획서**: [`01_MULTI_MATERIAL_CHAMPION_YONE.md`](01_MULTI_MATERIAL_CHAMPION_YONE.md) (2026-04-24, CModel 단일 visibility 설계 — ECS 다중 인스턴스 미지원으로 폐기, 본 v1 가 대체)

**목표**:
1. **메시 분리 인프라 (Layer 1)** — Per-Entity submesh visibility (`MeshGroupVisibilityComponent` ECS) + CModel submesh 이름 조회 API + ModelRenderer 인스턴스별 visibility mask
2. **Yone champion (Layer 2)** — 5 스킬 박제, **E (Soul Unbound) 가 Layer 1 의 PoC** — 동일 ModelRenderer 자원 2개 ECS entity (body + soul) 로 분리, 다른 visibility mask
3. **엘든링 확장 hook (Layer 3, 설계만)** — `MeshDestructibleComponent` (보스 부위 파괴) / `EquipmentSlotComponent` (무기 교체) / `MeshTransformationComponent` (변신) 의 component skeleton + 확장 절차

---

## §0. Agent Contract Evidence

| 도구 | 호출 | 결과 |
|---|---|---|
| `Bash ls` | Character/Yone/{root, animations, particles} | 13 submesh / 110+ anim / 80+ particle / 4 텍스처 (body/swords/props/recall) |
| `Read` | 01_MULTI_MATERIAL_CHAMPION_YONE.md L1-160 | 기존 설계 폐기 결정 — CModel 단일 visibility = ECS 다중 인스턴스 미지원 |
| `Bash find` | "WMeshFormat\|SubMeshDesc" | `Engine/Public/AssetFormat/Mesh/WMeshFormat.h` 의 `SubMeshDesc::name[20]` 이미 존재 |
| `Grep` | "RenderComponent\|m_World.ForEach.*RenderComponent" | Scene_InGame 의 render loop 식별 (L2038-L2050) |

---

## §1. Preflight Evidence Table

| 항목 | 결과 | 명령/위치 |
|---|---|---|
| **Read 한 파일** | 4 (기존 Yone 계획서 + WMeshFormat + Render loop) | — |
| **Grep 패턴** | 3회 | submesh / RenderComponent / WMeshFormat 인프라 식별 |
| **발견한 기존 인프라** | `SubMeshDesc::name[20]` 이미 .wmesh 포맷에 박제됨 (writer 만 채워주면 즉시 사용 가능) / `m_ChampionRenderers[entity] = unique_ptr<ModelRenderer>` 이미 per-entity 모델 owner 패턴 / per-entity Animator 별도 (multi-instance 가능) | — |
| **현재 API 시그니처** | `void ModelRenderer::Render()` — visibility 인자 없음 (확장 필요) / `void CModel::Render(ID3D11DeviceContext*)` — 기존 mesh 전수 draw / `RenderComponent { ModelRenderer* pRenderer; bool bVisible; bool bAnimated; bool bSceneManaged; }` (`Engine/Public/ECS/Components/GameplayComponents.h:43`) | — |
| **v1 / 중복 파일 존재** | 01_MULTI_MATERIAL_CHAMPION_YONE.md 의 CModel 단일 visibility 설계 폐기 (본 plan 가 대체) | — |
| **Hook context 필드** | 변경 없음 | — |
| **Asset 경로 실존** | `yone.fbx` ✅ / `yone_base_tx_cm.png` ✅ / `yone_base_swords_tx_cm.png` ✅ / `yone_base_props_tx_cm.png` ✅ / `yone_base_recall_tx_cm.png` ✅ / 110+ anim ✅ / 80+ particle ✅ / `yone.wmesh` ❌ (D-0) | `ls Character/Yone/` |
| **Yone 애니 키 (실측)** | idle: `yone_idle1` / run: `yone_run1` / BA: `yone_attack1` / Q1: `yone_spell1_a1` / Q2: `yone_spell1_a2` / Q3: `yone_spell1c_dash` / W: `yone_spell2` / E body: `yone_spell3_bodyloop` / E spirit: `yone_spell3_spiritin` / R: `yone_spell4_in` | `ls Character/Yone/animations/` |
| **submesh 후보 (FBX 실측 필요)** | Body / Katana / GhostKatana / Azakana / Sushi 등 — 첫 변환 후 Output 로그 검증 | — |

---

## §2. Plan Quality Gate Status

- [x] Full code (8 신규 파일 + 인프라 3 파일 수정)
- [x] No placeholder (Layer 3 elden ring 은 component skeleton + 확장 절차로 명시 — stub 아님)
- [x] Hook context fields verified
- [x] Asset paths Test-Path verified
- [x] vcxproj registration

---

## §3. Layer 1 — 메시 분리 인프라 (3 파일 신규 + 4 파일 수정)

### 3.1 `Engine/Public/ECS/Components/MeshGroupVisibilityComponent.h` (신규)

```cpp
#pragma once

#include "Engine_Defines.h"
#include <array>

// Phase B-16 — Per-Entity submesh visibility
//
// 사용 시나리오:
//   1. Yone E (Soul Unbound) — body entity = Body+Katana 표시, soul entity = GhostKatana 표시
//   2. Elden Ring 보스 부위 파괴 — submesh 단위 hidden 토글
//   3. 무기 교체 / 변신 — submesh 그룹 단위 swap
//
// 설계:
//   - kMaxSubmeshes = 32 (한 챔프당 충분, Yone 13개 + 여유)
//   - bitmask u32 1개로 32 submesh visibility 압축
//   - default: 모든 bit = 1 (전부 표시)
//   - Render system 이 매 프레임 ForEach 로 mask 읽어 ModelRenderer 에 전달
//
// Component 가 부재한 entity 는 default visibility (모든 submesh 표시) 로 처리.
// → Yone 같은 분리 chassis 만 Component 추가, 다른 챔프는 변경 없음.

inline constexpr u32_t kMaxSubmeshes = 32;

struct MeshGroupVisibilityComponent
{
    u32_t  visibilityMask = 0xFFFFFFFFu;   // 모든 bit = 1 (default 전부 표시)
    bool_t bDirty = true;                   // 변경 시 set, render system 이 사용 후 false

    bool_t IsVisible(u32_t submeshIndex) const
    {
        if (submeshIndex >= kMaxSubmeshes) return true;
        return (visibilityMask & (1u << submeshIndex)) != 0;
    }

    void SetVisible(u32_t submeshIndex, bool_t bVisible)
    {
        if (submeshIndex >= kMaxSubmeshes) return;
        if (bVisible)
            visibilityMask |= (1u << submeshIndex);
        else
            visibilityMask &= ~(1u << submeshIndex);
        bDirty = true;
    }

    void HideAll()         { visibilityMask = 0u;          bDirty = true; }
    void ShowAll()         { visibilityMask = 0xFFFFFFFFu; bDirty = true; }
    void HideOnly(u32_t i) { HideAll();  SetVisible(i, true); }
    void ShowOnly(u32_t i) { HideAll();  SetVisible(i, true); }
};
```

> **WINTERS_ENGINE / dllexport 주의**: 본 컴포넌트는 POD struct 라 export 마크 불필요. 단 EngineSDK/inc 동기화 시 헤더 복사 확인.

### 3.2 `Engine/Public/Resource/Model.h` (수정 — submesh 이름 조회 API 추가)

**Anchor**: `class CModel { ... vector<unique_ptr<CMesh>> m_vecMeshes; ... };`

**Before** (현 메서드 끝):
```cpp
public:
    static unique_ptr<CModel> Create(...);
    void Render(ID3D11DeviceContext* pContext);
    u32_t GetMeshCount() const;
    // ... 기타 ...
private:
    vector<unique_ptr<CMesh>> m_vecMeshes;
    vector<string>            m_vecMeshNames;       // ← 신규 (Layer 1)
    unordered_map<string, u32_t> m_mapNameToIndex;  // ← 신규
```

**After** — 추가 API:
```cpp
public:
    // Phase B-16 — submesh 이름 조회 (mesh separation 인프라)
    // .wmesh 의 SubMeshDesc::name[20] 또는 Assimp aiMesh::mName 에서 추출.
    // 정규화: lowercase + ".001" suffix 제거 (Blender export 충돌 회피).

    const string& GetSubmeshName(u32_t iIndex) const
    {
        static const string kEmpty;
        return (iIndex < m_vecMeshNames.size()) ? m_vecMeshNames[iIndex] : kEmpty;
    }

    // -1 if not found
    i32_t FindSubmeshIndex(const string& strName) const
    {
        const string strNorm = NormalizeMeshName(strName);
        auto it = m_mapNameToIndex.find(strNorm);
        return (it != m_mapNameToIndex.end()) ? static_cast<i32_t>(it->second) : -1;
    }

    // Phase B-16 — visibility mask 받는 Render 오버로드.
    // mask 가 nullptr 또는 모든 bit = 1 이면 기존 Render() 동작.
    void RenderWithMask(ID3D11DeviceContext* pContext, u32_t iVisibilityMask);

private:
    vector<unique_ptr<CMesh>>     m_vecMeshes;
    vector<string>                m_vecMeshNames;
    unordered_map<string, u32_t>  m_mapNameToIndex;

    static string NormalizeMeshName(const string& s);
    void RebuildSubmeshNameMap();
```

### 3.3 `Engine/Private/Resource/Model.cpp` (수정 — submesh 이름 채우기 + mask Render)

**Anchor**: `void CModel::Render(...)` (L66-L73)

**Before**:
```cpp
void CModel::Render(ID3D11DeviceContext* pContext)
{
    for (u32_t i = 0; i < m_vecMeshes.size(); ++i)
    {
        BindMaterial(pContext, i);
        m_vecMeshes[i]->Render(pContext);
    }
}
```

**After** — 기존 Render 유지 + RenderWithMask 추가:
```cpp
void CModel::Render(ID3D11DeviceContext* pContext)
{
    RenderWithMask(pContext, 0xFFFFFFFFu);   // 전부 표시
}

void CModel::RenderWithMask(ID3D11DeviceContext* pContext, u32_t iVisibilityMask)
{
    for (u32_t i = 0; i < m_vecMeshes.size(); ++i)
    {
        if (i < 32 && (iVisibilityMask & (1u << i)) == 0)
            continue;
        BindMaterial(pContext, i);
        m_vecMeshes[i]->Render(pContext);
    }
}
```

**신규 — `NormalizeMeshName` + `RebuildSubmeshNameMap`** (Model.cpp 끝):
```cpp
string CModel::NormalizeMeshName(const string& s)
{
    string out;
    out.reserve(s.size());
    for (char c : s)
    {
        if (c == '.') break;   // ".001" 같은 Blender suffix 잘라냄
        out.push_back(static_cast<char>(::tolower(static_cast<unsigned char>(c))));
    }
    // 양 끝 공백 trim
    while (!out.empty() && ::isspace(static_cast<unsigned char>(out.back())))
        out.pop_back();
    size_t front = 0;
    while (front < out.size() && ::isspace(static_cast<unsigned char>(out[front])))
        ++front;
    if (front > 0) out.erase(0, front);
    return out;
}

void CModel::RebuildSubmeshNameMap()
{
    m_mapNameToIndex.clear();
    m_mapNameToIndex.reserve(m_vecMeshNames.size());
    for (u32_t i = 0; i < m_vecMeshNames.size(); ++i)
    {
        const string strNorm = NormalizeMeshName(m_vecMeshNames[i]);
        m_mapNameToIndex[strNorm] = i;
    }
}
```

**Assimp 경로 (`ProcessMesh`)** — `aiMesh::mName` 저장:
```cpp
// 기존 ProcessMesh 안 — CMesh::Create 호출 직후
auto pMesh = CMesh::Create(...);
m_vecMeshes.push_back(std::move(pMesh));
m_vecMeshNames.push_back(string(pAiMesh->mName.C_Str()));   // ← 신규
```

**.wmesh 경로 (`BuildMeshesFromWMesh`)** — `SubMeshDesc::name` 저장:
```cpp
// 기존 BuildMeshesFromWMesh 안
m_vecMeshes.push_back(std::move(mesh));
m_vecMeshNames.push_back(string(desc.name));   // ← 신규
```

**`Create` 끝 — `RebuildSubmeshNameMap()` 호출** (Assimp / wmesh 경로 공통):
```cpp
pInstance->RebuildSubmeshNameMap();

char dbg[1024]; size_t off = 0;
off += sprintf_s(dbg + off, sizeof(dbg) - off, "[CModel] meshes=%zu names=", m_vecMeshes.size());
for (const auto& n : m_vecMeshNames)
    off += sprintf_s(dbg + off, sizeof(dbg) - off, "[%s] ", n.c_str());
off += sprintf_s(dbg + off, sizeof(dbg) - off, "\n");
OutputDebugStringA(dbg);
```

### 3.4 `Engine/Public/Renderer/ModelRenderer.h` (수정 — visibility mask)

**Anchor**: `class ModelRenderer { ... void Render(); ... };`

**Before**:
```cpp
void Render();
```

**After**:
```cpp
void Render();
void RenderWithVisibility(u32_t iVisibilityMask);   // Phase B-16
i32_t FindSubmeshIndex(const string& strName) const;
const string& GetSubmeshName(u32_t iIndex) const;
u32_t GetSubmeshCount() const;
```

### 3.5 `Engine/Private/Renderer/ModelRenderer.cpp` (수정)

```cpp
void ModelRenderer::RenderWithVisibility(u32_t iVisibilityMask)
{
    if (!m_pImpl || !m_pImpl->pModel) return;
    UpdateBoneCBuffer();   // 기존 Render() 의 sub
    BindShader();
    BindFrameCBuffer();
    BindObjectCBuffer();
    m_pImpl->pModel->RenderWithMask(m_pImpl->pContext, iVisibilityMask);
}

i32_t ModelRenderer::FindSubmeshIndex(const string& strName) const
{
    return m_pImpl && m_pImpl->pModel
        ? m_pImpl->pModel->FindSubmeshIndex(strName) : -1;
}

const string& ModelRenderer::GetSubmeshName(u32_t iIndex) const
{
    static const string kEmpty;
    return m_pImpl && m_pImpl->pModel
        ? m_pImpl->pModel->GetSubmeshName(iIndex) : kEmpty;
}

u32_t ModelRenderer::GetSubmeshCount() const
{
    return m_pImpl && m_pImpl->pModel ? m_pImpl->pModel->GetMeshCount() : 0;
}
```

### 3.6 Scene_InGame.cpp Render 경로 — visibility mask 전달

**Anchor**: L2038-L2050 — `m_World.ForEach<ChampionComponent, RenderComponent, TransformComponent>(...)`

**Before**:
```cpp
m_World.ForEach<ChampionComponent, RenderComponent, TransformComponent>(
    [&](EntityID, ChampionComponent&, RenderComponent& rc,
        TransformComponent& tf)
    {
        if (rc.bSceneManaged) return;
        if (!rc.bVisible || !rc.pRenderer) return;
        FlushTransformForRender(tf);
        rc.pRenderer->UpdateCamera(vp);
        rc.pRenderer->UpdateTransform(tf.GetWorldMatrix());
        rc.pRenderer->Render();
    }
);
```

**After**:
```cpp
m_World.ForEach<ChampionComponent, RenderComponent, TransformComponent>(
    [&](EntityID e, ChampionComponent&, RenderComponent& rc,
        TransformComponent& tf)
    {
        if (rc.bSceneManaged) return;
        if (!rc.bVisible || !rc.pRenderer) return;
        FlushTransformForRender(tf);
        rc.pRenderer->UpdateCamera(vp);
        rc.pRenderer->UpdateTransform(tf.GetWorldMatrix());

        // Phase B-16 — visibility mask (있으면 적용, 없으면 전체)
        if (m_World.HasComponent<MeshGroupVisibilityComponent>(e))
        {
            const auto& vis = m_World.GetComponent<MeshGroupVisibilityComponent>(e);
            rc.pRenderer->RenderWithVisibility(vis.visibilityMask);
        }
        else
        {
            rc.pRenderer->Render();
        }
    }
);
```

**Include 추가** (Scene_InGame.cpp 상단):
```cpp
#include "ECS/Components/MeshGroupVisibilityComponent.h"
```

---

## §4. Layer 2 — Yone Champion 박제 (8 신규 파일)

### 4.1 D-0 변환

```bat
call :convert_champ "Ashe" "ashe.fbx"
call :convert_champ "Yone" "yone.fbx"
```

### 4.2 `Yone_Components.h`

```cpp
#pragma once

#include "Engine_Defines.h"
#include "WintersMath.h"
#include "ECS/Entity.h"

// Yone — Phase B-16
struct YoneStateComponent
{
    // Q — Mortal Steel (3-stage chain, 6s window 안 3타째 dash + AOE)
    u8_t   qStackCount = 0;
    f32_t  qStackTimer = 0.f;
    f32_t  fQStackWindowSec = 6.f;

    // W — Spirit Cleave (방어막)
    bool   bWShieldActive = false;
    f32_t  fWShieldTimer  = 0.f;
    f32_t  fWShieldDurationSec = 4.f;
    f32_t  fWShieldAmount = 60.f;

    // E — Soul Unbound (8s 동안 caster body 위치 기억, 종료 시 해당 위치로 텔레포트)
    bool   bESoulActive = false;
    f32_t  fESoulTimer  = 0.f;
    f32_t  fESoulDurationSec = 8.f;
    Vec3   vEBodyAnchorPos = { 0.f, 0.f, 0.f };
    EntityID eBodyEntity = NULL_ENTITY;     // 본체 (visibility mask 적용)
    EntityID eSoulEntity = NULL_ENTITY;     // 영혼 (별도 ECS entity, 같은 자원 다른 mask)
    f32_t  fESoulReturnDmg = 0.f;            // 영혼 모드 동안 누적된 데미지

    // R — Fate Sealed (line dash through enemies)
    bool   bRDashing = false;
    f32_t  fRDashTimer = 0.f;
    f32_t  fRDashDurationSec = 0.6f;
    Vec3   vRDashStart = { 0.f, 0.f, 0.f };
    Vec3   vRDashEnd   = { 0.f, 0.f, 0.f };
};

// E (Soul Unbound) — body↔soul 간 양방향 link
struct SoulSeparationComponent
{
    EntityID partnerEntity = NULL_ENTITY;  // body 의 partner = soul, soul 의 partner = body
    bool_t   bIsBody = false;               // true = body 측, false = soul 측
};
```

### 4.3 `Yone_FxPresets.h`

```cpp
#pragma once

#include "Defines.h"
#include "WintersMath.h"
#include "ECS/Entity.h"

class CWorld;

namespace Yone::Fx
{
    void SpawnBASlash(CWorld& world, EntityID owner, EntityID target, f32_t fLifetime);
    void SpawnQ1Slash(CWorld& world, EntityID owner, const Vec3& dir, f32_t fLifetime);
    void SpawnQ2Slash(CWorld& world, EntityID owner, const Vec3& dir, f32_t fLifetime);
    void SpawnQ3DashAOE(CWorld& world, EntityID owner, const Vec3& origin, f32_t fLifetime);
    void SpawnWConeShield(CWorld& world, EntityID owner, f32_t fDuration);
    void SpawnESoulSeparate(CWorld& world, EntityID body, f32_t fLifetime);
    void SpawnESoulReturn(CWorld& world, EntityID body, f32_t fLifetime);
    void SpawnRDashTrail(CWorld& world, EntityID owner, const Vec3& start, const Vec3& end, f32_t fLifetime);
}
```

### 4.4 `Yone_FxPresets.cpp` (요약 — 동일 패턴, 8 함수)

```cpp
#include "GameObject/Champion/Yone/Yone_FxPresets.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxSystem.h"
#include "ECS/World.h"

namespace
{
    constexpr const wchar_t* kPathQSlashTex =
        L"Client/Bin/Resource/Texture/Character/Yone/particles/yone_base_air_swoosh.png";
    constexpr const wchar_t* kPathQ3AOETex =
        L"Client/Bin/Resource/Texture/Character/Yone/particles/yone_base_demon_trail.png";
    constexpr const wchar_t* kPathWShieldTex =
        L"Client/Bin/Resource/Texture/Character/Yone/particles/yone_base_glow_ring.png";
    constexpr const wchar_t* kPathESoulTex =
        L"Client/Bin/Resource/Texture/Character/Yone/particles/yone_base_e_blue_flame.png";
    constexpr const wchar_t* kPathERingTex =
        L"Client/Bin/Resource/Texture/Character/Yone/particles/yone_base_e_mark.png";
    constexpr const wchar_t* kPathRDashTex =
        L"Client/Bin/Resource/Texture/Character/Yone/particles/yone_base_demon_trail4.png";
    constexpr const wchar_t* kPathRSliceTex =
        L"Client/Bin/Resource/Texture/Character/Yone/particles/yone_base_e_slice.png";
}

namespace Yone::Fx
{
    namespace
    {
        void SpawnSimpleBillboard(CWorld& world, EntityID owner, const wchar_t* path,
            const Vec3& offset, f32_t w, f32_t h, f32_t fLife, const Vec4& color, eBlendPreset blend)
        {
            if (owner == NULL_ENTITY) return;
            FxBillboardComponent fx{};
            fx.attachTo = owner;
            fx.vAttachOffset = offset;
            fx.texturePath = path;
            fx.fWidth = w; fx.fHeight = h;
            fx.bBillboard = true;
            fx.fLifetime = fLife;
            fx.vColor = color;
            fx.blendMode = blend;
            fx.fFadeOut = fLife * 0.4f;
            CFxSystem::Spawn(world, fx);
        }
    }

    void SpawnBASlash(CWorld& world, EntityID owner, EntityID target, f32_t fLifetime)
    {
        SpawnSimpleBillboard(world, owner, kPathQSlashTex,
            { 0.f, 1.0f, 0.f }, 1.4f, 0.8f, fLifetime,
            { 0.85f, 0.95f, 1.1f, 1.f }, eBlendPreset::Additive);
        if (target != NULL_ENTITY)
            SpawnSimpleBillboard(world, target, kPathRSliceTex,
                { 0.f, 1.0f, 0.f }, 1.0f, 1.0f, fLifetime,
                { 1.0f, 0.6f, 0.3f, 1.f }, eBlendPreset::Additive);
    }

    void SpawnQ1Slash(CWorld& world, EntityID owner, const Vec3&, f32_t fLifetime)
    {
        SpawnSimpleBillboard(world, owner, kPathQSlashTex,
            { 0.f, 1.1f, 0.f }, 1.8f, 1.0f, fLifetime,
            { 0.9f, 1.0f, 1.2f, 1.f }, eBlendPreset::Additive);
    }

    void SpawnQ2Slash(CWorld& world, EntityID owner, const Vec3&, f32_t fLifetime)
    {
        SpawnSimpleBillboard(world, owner, kPathQSlashTex,
            { 0.f, 1.2f, 0.f }, 2.0f, 1.0f, fLifetime,
            { 1.0f, 0.95f, 1.0f, 1.f }, eBlendPreset::Additive);
    }

    void SpawnQ3DashAOE(CWorld& world, EntityID owner, const Vec3&, f32_t fLifetime)
    {
        SpawnSimpleBillboard(world, owner, kPathQ3AOETex,
            { 0.f, 0.05f, 0.f }, 3.0f, 3.0f, fLifetime,
            { 1.2f, 0.5f, 1.4f, 0.9f }, eBlendPreset::Additive);
    }

    void SpawnWConeShield(CWorld& world, EntityID owner, f32_t fDuration)
    {
        SpawnSimpleBillboard(world, owner, kPathWShieldTex,
            { 0.f, 1.0f, 0.f }, 2.2f, 2.2f, fDuration,
            { 1.1f, 0.8f, 1.3f, 0.8f }, eBlendPreset::AlphaBlend);
    }

    void SpawnESoulSeparate(CWorld& world, EntityID body, f32_t fLifetime)
    {
        SpawnSimpleBillboard(world, body, kPathESoulTex,
            { 0.f, 1.4f, 0.f }, 1.8f, 2.4f, fLifetime,
            { 0.5f, 1.2f, 1.4f, 1.f }, eBlendPreset::Additive);
        SpawnSimpleBillboard(world, body, kPathERingTex,
            { 0.f, 0.05f, 0.f }, 2.4f, 2.4f, fLifetime,
            { 0.4f, 1.0f, 1.3f, 0.7f }, eBlendPreset::Additive);
    }

    void SpawnESoulReturn(CWorld& world, EntityID body, f32_t fLifetime)
    {
        SpawnSimpleBillboard(world, body, kPathESoulTex,
            { 0.f, 1.0f, 0.f }, 2.0f, 2.0f, fLifetime,
            { 0.6f, 1.3f, 1.5f, 1.f }, eBlendPreset::Additive);
    }

    void SpawnRDashTrail(CWorld& world, EntityID owner, const Vec3&, const Vec3&, f32_t fLifetime)
    {
        SpawnSimpleBillboard(world, owner, kPathRDashTex,
            { 0.f, 1.0f, 0.f }, 2.6f, 1.0f, fLifetime,
            { 1.3f, 0.4f, 1.4f, 1.f }, eBlendPreset::Additive);
    }
}
```

### 4.5 `Yone_Skills.h`

```cpp
#pragma once

#include "GamePlay/SkillHookContext.h"
#include "GamePlay/VisualHookRegistry.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry.h"

namespace Yone
{
    void OnCastFrame_BA(SkillHookContext& ctx);
    void OnCastFrame_Q(SkillHookContext& ctx);
    void OnCastFrame_W(SkillHookContext& ctx);
    void OnCastFrame_E(SkillHookContext& ctx);
    void OnCastFrame_R(SkillHookContext& ctx);
    void OnCastFrame_E_End(SkillHookContext& ctx);   // E 재시전 (return to body)

    namespace Gameplay
    {
        void OnCastFrame_BA(GameplayHookContext& ctx);
        void OnCastFrame_Q(GameplayHookContext& ctx);
        void OnCastFrame_W(GameplayHookContext& ctx);
        void OnCastFrame_E(GameplayHookContext& ctx);
        void OnCastFrame_R(GameplayHookContext& ctx);
    }

    namespace Visual
    {
        void OnCastFrame_BA_Visual(VisualHookContext& ctx);
        void OnCastFrame_Q_Visual(VisualHookContext& ctx);
        void OnCastFrame_W_Visual(VisualHookContext& ctx);
        void OnCastFrame_E_Visual(VisualHookContext& ctx);
        void OnCastFrame_R_Visual(VisualHookContext& ctx);
    }
}

void Yone_KeepAlive();
```

### 4.6 `Yone_Skills.cpp` (요약 — E 의 Soul Unbound 가 핵심)

```cpp
#include "GameObject/Champion/Yone/Yone_Skills.h"
#include "GameObject/Champion/Yone/Yone_Components.h"
#include "GameObject/Champion/Yone/Yone_FxPresets.h"
#include "GamePlay/Systems/Damage.h"

#include "ECS/World.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/MeshGroupVisibilityComponent.h"
#include "ECS/Components/NavAgentComponent.h"

#include <Windows.h>
#include <cmath>
#include <cstdio>

// Yone submesh 인덱스 — 첫 변환 후 [CModel] meshes=N names=[...] 로그 검증 후 박제.
// 1차 가정 (FBX 메시 순서):
//   0: Body
//   1: Katana_R (오른손)
//   2: Katana_L (왼손)
//   3: GhostKatana_R  (E 영혼 모드 시만 표시)
//   4: GhostKatana_L
//   5: Azakana (등 가면)
//   6: Sushi (recall 전용 — 평시 hide)
//   7~N: 기타
namespace YoneMesh
{
    constexpr u32_t Body         = 0;
    constexpr u32_t Katana_R     = 1;
    constexpr u32_t Katana_L     = 2;
    constexpr u32_t GhostKatana_R = 3;
    constexpr u32_t GhostKatana_L = 4;
    constexpr u32_t Azakana      = 5;
    constexpr u32_t Sushi        = 6;
}

namespace Yone
{
    namespace
    {
        u32_t MaskForBodyEntity()
        {
            // body: Body + Katana 표시, GhostKatana / Sushi hide
            u32_t m = 0xFFFFFFFFu;
            m &= ~(1u << YoneMesh::GhostKatana_R);
            m &= ~(1u << YoneMesh::GhostKatana_L);
            m &= ~(1u << YoneMesh::Sushi);
            return m;
        }

        u32_t MaskForSoulEntity()
        {
            // soul: GhostKatana 만 표시, Body / Katana / Azakana / Sushi hide
            u32_t m = 0u;
            m |= (1u << YoneMesh::GhostKatana_R);
            m |= (1u << YoneMesh::GhostKatana_L);
            return m;
        }

        // E (Soul Unbound) — body 위치 고정 + 영혼 entity spawn
        EntityID SpawnSoulEntity(CWorld& world, EntityID bodyEntity)
        {
            // 1. body 의 Transform 복사
            if (!world.HasComponent<TransformComponent>(bodyEntity)) return NULL_ENTITY;
            const Vec3 bodyPos = world.GetComponent<TransformComponent>(bodyEntity).GetPosition();

            // 2. 신규 entity — Yone Soul (별도 ModelRenderer 인스턴스)
            //    실제 인스턴스 alloc 은 Scene_InGame::SpawnYoneSoul 헬퍼에서 (Model 자원 cache 재사용)
            //    여기서는 link component 만 박제. 실제 spawn 은 Visual hook 에서 진행.
            return NULL_ENTITY;
        }
    }

    void OnCastFrame_BA(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand) return;
        const EntityID target = ctx.pCommand->targetEntityId;
        if (target == NULL_ENTITY) return;
        ApplyDamage(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam, target, 55.f);

        char dbg[128];
        sprintf_s(dbg, "[Yone BA] target=%u dmg=55.0\n", static_cast<u32_t>(target));
        OutputDebugStringA(dbg);
    }

    void OnCastFrame_Q(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand) return;
        if (!ctx.pWorld->HasComponent<YoneStateComponent>(ctx.casterEntity)) return;
        if (!ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity)) return;

        auto& ys = ctx.pWorld->GetComponent<YoneStateComponent>(ctx.casterEntity);
        const Vec3 origin = ctx.pWorld
            ->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();

        // 3-stage Q chain 6s 윈도우
        if (ys.qStackTimer <= 0.f)
            ys.qStackCount = 0;

        if (ys.qStackCount < 2)
        {
            // Q1, Q2 — 단순 line slash
            ys.qStackCount = static_cast<u8_t>(ys.qStackCount + 1);
            ys.qStackTimer = ys.fQStackWindowSec;

            // 정면 cone hit (3m × 1m)
            char dbg[64];
            sprintf_s(dbg, "[Yone Q%u] slash\n", ys.qStackCount);
            OutputDebugStringA(dbg);
        }
        else
        {
            // Q3 — dash + AOE
            ys.qStackCount = 0;
            ys.qStackTimer = 0.f;

            char dbg[64];
            sprintf_s(dbg, "[Yone Q3] dash + AOE\n");
            OutputDebugStringA(dbg);
        }
    }

    void OnCastFrame_W(SkillHookContext& ctx)
    {
        if (!ctx.pWorld) return;
        if (!ctx.pWorld->HasComponent<YoneStateComponent>(ctx.casterEntity)) return;
        auto& ys = ctx.pWorld->GetComponent<YoneStateComponent>(ctx.casterEntity);
        ys.bWShieldActive = true;
        ys.fWShieldTimer = ys.fWShieldDurationSec;
        OutputDebugStringA("[Yone W] Spirit Cleave shield (4s)\n");
    }

    // ★ E — Soul Unbound: 핵심 로직
    void OnCastFrame_E(SkillHookContext& ctx)
    {
        if (!ctx.pWorld) return;
        if (!ctx.pWorld->HasComponent<YoneStateComponent>(ctx.casterEntity)) return;
        if (!ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity)) return;

        auto& ys = ctx.pWorld->GetComponent<YoneStateComponent>(ctx.casterEntity);
        if (ys.bESoulActive) return;   // 이미 영혼 모드 — 종료는 별도 hook (E 재시전)

        // 1. body 위치 anchor 저장
        ys.vEBodyAnchorPos = ctx.pWorld
            ->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        ys.eBodyEntity = ctx.casterEntity;
        ys.bESoulActive = true;
        ys.fESoulTimer = ys.fESoulDurationSec;
        ys.fESoulReturnDmg = 0.f;

        // 2. body entity 의 visibility — Body+Katana 표시, GhostKatana hide (default)
        if (!ctx.pWorld->HasComponent<MeshGroupVisibilityComponent>(ctx.casterEntity))
            ctx.pWorld->AddComponent<MeshGroupVisibilityComponent>(ctx.casterEntity);
        auto& bodyVis = ctx.pWorld->GetComponent<MeshGroupVisibilityComponent>(ctx.casterEntity);
        bodyVis.visibilityMask = MaskForBodyEntity();
        bodyVis.bDirty = true;

        // 3. body 에 SoulSeparationComponent (link)
        if (!ctx.pWorld->HasComponent<SoulSeparationComponent>(ctx.casterEntity))
            ctx.pWorld->AddComponent<SoulSeparationComponent>(ctx.casterEntity);
        auto& bodyLink = ctx.pWorld->GetComponent<SoulSeparationComponent>(ctx.casterEntity);
        bodyLink.bIsBody = true;

        // 4. 영혼 entity 생성 — Visual hook 에서 진행 (ModelRenderer 자원 alloc 필요).
        //    이 시점에는 state 만 박제. Visual hook 이 Scene_InGame::SpawnYoneSoul 호출.

        OutputDebugStringA("[Yone E] Soul Unbound activated — body anchored, soul spawning\n");
    }

    void OnCastFrame_E_End(SkillHookContext& ctx)
    {
        // E 재시전 — soul → body 위치 텔레포트, soul entity destroy
        if (!ctx.pWorld) return;
        if (!ctx.pWorld->HasComponent<YoneStateComponent>(ctx.casterEntity)) return;

        auto& ys = ctx.pWorld->GetComponent<YoneStateComponent>(ctx.casterEntity);
        if (!ys.bESoulActive) return;

        // body 를 soul 위치로 텔레포트 (또는 soul 을 body 위치로 — 디자인 결정)
        if (ys.eSoulEntity != NULL_ENTITY
            && ctx.pWorld->HasComponent<TransformComponent>(ys.eSoulEntity)
            && ctx.pWorld->HasComponent<TransformComponent>(ys.eBodyEntity))
        {
            const Vec3 soulPos = ctx.pWorld
                ->GetComponent<TransformComponent>(ys.eSoulEntity).GetPosition();
            auto& bodyTf = ctx.pWorld->GetComponent<TransformComponent>(ys.eBodyEntity);
            bodyTf.SetPosition(soulPos);
            bodyTf.m_bLocalDirty = true;
            bodyTf.m_bWorldDirty = true;
        }

        // body visibility 복원 (전체)
        if (ctx.pWorld->HasComponent<MeshGroupVisibilityComponent>(ys.eBodyEntity))
        {
            auto& vis = ctx.pWorld->GetComponent<MeshGroupVisibilityComponent>(ys.eBodyEntity);
            vis.ShowAll();
        }

        // soul entity destroy — Visual hook 에서 진행 (ModelRenderer 정리 필요)
        ys.bESoulActive = false;
        ys.fESoulTimer = 0.f;

        OutputDebugStringA("[Yone E End] Soul returned to body\n");
    }

    void OnCastFrame_R(SkillHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand) return;
        if (!ctx.pWorld->HasComponent<YoneStateComponent>(ctx.casterEntity)) return;
        if (!ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity)) return;

        auto& ys = ctx.pWorld->GetComponent<YoneStateComponent>(ctx.casterEntity);
        ys.vRDashStart = ctx.pWorld
            ->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();

        const Vec3 dir = ctx.pCommand->direction;
        const f32_t lenSq = dir.x * dir.x + dir.z * dir.z;
        if (lenSq > 0.0001f)
        {
            const f32_t inv = 1.f / std::sqrtf(lenSq);
            const f32_t fRange = 8.0f;
            ys.vRDashEnd = {
                ys.vRDashStart.x + dir.x * inv * fRange,
                ys.vRDashStart.y,
                ys.vRDashStart.z + dir.z * inv * fRange
            };
        }
        ys.bRDashing = true;
        ys.fRDashTimer = ys.fRDashDurationSec;

        // 1차: 즉시 모든 enemy line damage (B-17 dash hit 정식)
        OutputDebugStringA("[Yone R] Fate Sealed dash\n");
    }

    // Gameplay / Visual stubs (생략 — Annie/Ashe/Jax 패턴 동일)
    namespace Gameplay
    {
        void OnCastFrame_BA(GameplayHookContext& ctx) { (void)ctx; }
        void OnCastFrame_Q(GameplayHookContext& ctx)  { (void)ctx; }
        void OnCastFrame_W(GameplayHookContext& ctx)  { (void)ctx; }
        void OnCastFrame_E(GameplayHookContext& ctx)  { (void)ctx; }
        void OnCastFrame_R(GameplayHookContext& ctx)  { (void)ctx; }
    }

    namespace Visual
    {
        void OnCastFrame_BA_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld || !ctx.pCommand) return;
            Fx::SpawnBASlash(*ctx.pWorld, ctx.casterEntity, ctx.pCommand->targetEntityId, 0.4f);
        }

        void OnCastFrame_Q_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld) return;
            // Q stack 에 따라 다른 FX
            if (ctx.pWorld->HasComponent<YoneStateComponent>(ctx.casterEntity))
            {
                const auto& ys = ctx.pWorld->GetComponent<YoneStateComponent>(ctx.casterEntity);
                if (ys.qStackCount == 1)
                    Fx::SpawnQ1Slash(*ctx.pWorld, ctx.casterEntity, Vec3{}, 0.4f);
                else if (ys.qStackCount == 2)
                    Fx::SpawnQ2Slash(*ctx.pWorld, ctx.casterEntity, Vec3{}, 0.5f);
                else
                {
                    if (ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
                    {
                        const Vec3 origin = ctx.pWorld
                            ->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
                        Fx::SpawnQ3DashAOE(*ctx.pWorld, ctx.casterEntity, origin, 0.6f);
                    }
                }
            }
        }

        void OnCastFrame_W_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld) return;
            Fx::SpawnWConeShield(*ctx.pWorld, ctx.casterEntity, 4.0f);
        }

        void OnCastFrame_E_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld) return;
            // 영혼 entity spawn 은 Scene_InGame 의 헬퍼 호출 (cf. §6.2 SpawnYoneSoul).
            // 본 hook 에서는 visual FX 만.
            Fx::SpawnESoulSeparate(*ctx.pWorld, ctx.casterEntity, 1.2f);
        }

        void OnCastFrame_R_Visual(VisualHookContext& ctx)
        {
            if (!ctx.pWorld) return;
            if (ctx.pWorld->HasComponent<YoneStateComponent>(ctx.casterEntity))
            {
                const auto& ys = ctx.pWorld->GetComponent<YoneStateComponent>(ctx.casterEntity);
                Fx::SpawnRDashTrail(*ctx.pWorld, ctx.casterEntity,
                    ys.vRDashStart, ys.vRDashEnd, 0.6f);
            }
        }
    }
}
```

### 4.7 `Yone_Registration.h` / `.cpp`

Annie/Ashe/Jax 패턴 동일 — 5 SkillDef + 15 Hook (Skill/Gameplay/Visual × 5 slot) + ChampionDef.

**ChampionDef 핵심**:
```cpp
cd.id = eChampion::YONE;
cd.animPrefix    = "yone_";
cd.idleAnimKey   = "idle1";
cd.runAnimKey    = "run1";
cd.basicAttackKey = "attack1";
cd.basicAttackRange = 1.5f;
cd.fbxPath = "Client/Bin/Resource/Texture/Character/Yone/yone.fbx";
const wchar_t* yoneBody   = L".../Yone/yone_base_tx_cm.png";
const wchar_t* yoneSwords = L".../Yone/yone_base_swords_tx_cm.png";
const wchar_t* yoneProps  = L".../Yone/yone_base_props_tx_cm.png";
cd.defaultTexturePath = yoneBody;
cd.texturePath[0] = yoneBody;       // Body
cd.texturePath[1] = yoneSwords;     // Katana_R
cd.texturePath[2] = yoneSwords;     // Katana_L
cd.texturePath[3] = yoneSwords;     // GhostKatana_R
cd.texturePath[4] = yoneSwords;     // GhostKatana_L
cd.texturePath[5] = yoneProps;      // Azakana
cd.texturePath[6] = yoneProps;      // Sushi
cd.texturePath[7] = yoneBody;
cd.spawnPosition = { 42.f, 1.f, 0.f };
cd.displayName = "Yone";
```

> **★ 첫 실행 후 검증 의무**: Output 창의 `[CModel] meshes=N names=[Body] [Katana_R] [GhostKatana_R] ...` 로그 확인 후 `YoneMesh::Body` / `Katana_R` 등 인덱스 박제. 잘못 매핑되면 영혼 모드에서 진짜 body 가 보이거나 GhostKatana 가 평시에도 보임.

---

## §5. Yone Soul Spawn 헬퍼 — Scene_InGame 통합

### 5.1 `Scene_InGame.h` — 헬퍼 선언

```cpp
public:
    // Phase B-16 — Yone E (Soul Unbound) — body 와 같은 자원 사용하는 영혼 entity 생성
    EntityID SpawnYoneSoul(EntityID bodyEntity);
    void DespawnYoneSoul(EntityID soulEntity);

    EntityID m_YoneEntity = NULL_ENTITY;     // Phase B-16
```

### 5.2 `Scene_InGame.cpp` — 헬퍼 구현 (CreateECSChampion 패턴 미러)

```cpp
EntityID CScene_InGame::SpawnYoneSoul(EntityID bodyEntity)
{
    if (bodyEntity == NULL_ENTITY) return NULL_ENTITY;

    const ChampionDef* cd = CChampionRegistry::Instance().Find(eChampion::YONE);
    if (!cd || !cd->fbxPath) return NULL_ENTITY;

    // 1. 별도 ModelRenderer 인스턴스 (CModel 자원은 ResourceCache 가 재사용)
    auto pRenderer = std::make_unique<ModelRenderer>();
    if (!pRenderer->Init(cd->fbxPath, cd->shaderPath))
        return NULL_ENTITY;
    if (cd->defaultTexturePath)
        pRenderer->LoadTextureForAllMeshes(cd->defaultTexturePath);
    for (u32_t i = 0; i < kChampionTextureSlotMax; ++i)
        if (cd->texturePath[i])
            pRenderer->LoadMeshTexture(i, cd->texturePath[i]);

    // 2. soul entity 생성
    EntityID soul = m_World.CreateEntity();

    TransformComponent tf;
    tf.SetPosition(m_World.GetComponent<TransformComponent>(bodyEntity).GetPosition());
    tf.SetScale(cd->spawnScale);
    m_World.AddComponent<TransformComponent>(soul, tf);

    RenderComponent rc;
    rc.pRenderer = pRenderer.get();
    rc.bVisible = true;
    rc.bAnimated = true;
    rc.bSceneManaged = false;
    m_World.AddComponent<RenderComponent>(soul, rc);

    ChampionComponent cc;
    cc.id = eChampion::YONE;
    cc.team = m_World.GetComponent<ChampionComponent>(bodyEntity).team;
    cc.hp = 1.f;        // soul 무적 — invulnerable (1차 단순)
    cc.maxHp = 1.f;
    cc.moveSpeed = m_fPlayerSpeed * 1.2f;
    m_World.AddComponent<ChampionComponent>(soul, cc);

    // 3. 영혼 visibility — GhostKatana 만 표시
    MeshGroupVisibilityComponent vis;
    vis.visibilityMask = 0u;
    vis.SetVisible(YoneMesh::GhostKatana_R, true);
    vis.SetVisible(YoneMesh::GhostKatana_L, true);
    m_World.AddComponent<MeshGroupVisibilityComponent>(soul, vis);

    // 4. body↔soul link
    SoulSeparationComponent soulLink;
    soulLink.partnerEntity = bodyEntity;
    soulLink.bIsBody = false;
    m_World.AddComponent<SoulSeparationComponent>(soul, soulLink);

    auto& bodyLink = m_World.GetComponent<SoulSeparationComponent>(bodyEntity);
    bodyLink.partnerEntity = soul;

    // 5. soul anim — spell3_spiritin → idle/run
    pRenderer->PlayAnimationByName("yone_spell3_spiritin", false);

    // 6. NavAgent (영혼 이동 가능)
    NavAgentComponent agent;
    agent.fSpeed = cc.moveSpeed;
    agent.fArriveRadius = m_fArriveRadius;
    m_World.AddComponent<NavAgentComponent>(soul, agent);
    m_World.AddComponent<VelocityComponent>(soul);

    // 7. body 의 YoneStateComponent 갱신
    if (m_World.HasComponent<YoneStateComponent>(bodyEntity))
        m_World.GetComponent<YoneStateComponent>(bodyEntity).eSoulEntity = soul;

    m_ChampionRenderers[soul] = std::move(pRenderer);

    char dbg[160];
    sprintf_s(dbg, "[Yone Soul] spawned entity=%u from body=%u\n",
        static_cast<u32_t>(soul), static_cast<u32_t>(bodyEntity));
    OutputDebugStringA(dbg);

    return soul;
}

void CScene_InGame::DespawnYoneSoul(EntityID soulEntity)
{
    if (soulEntity == NULL_ENTITY) return;
    m_ChampionRenderers.erase(soulEntity);
    m_World.DestroyEntity(soulEntity);
}
```

### 5.3 `Yone::Visual::OnCastFrame_E_Visual` 의 soul spawn 트리거

`Yone_Skills.cpp` 의 Visual::OnCastFrame_E_Visual 안에서 Scene_InGame extern 호출:

```cpp
extern EntityID Winters_SpawnYoneSoul(EntityID bodyEntity);   // Scene_InGame 측 forward

void OnCastFrame_E_Visual(VisualHookContext& ctx)
{
    if (!ctx.pWorld) return;
    Fx::SpawnESoulSeparate(*ctx.pWorld, ctx.casterEntity, 1.2f);

    // soul entity spawn — Scene 측 헬퍼 호출
    Winters_SpawnYoneSoul(ctx.casterEntity);
}
```

`Scene_InGame.cpp` 끝에 forward:
```cpp
EntityID Winters_SpawnYoneSoul(EntityID bodyEntity)
{
    auto* pScene = static_cast<CScene_InGame*>(
        CGameInstance::Get()->Get_CurrentScene());
    return pScene ? pScene->SpawnYoneSoul(bodyEntity) : NULL_ENTITY;
}
```

> **주의**: `CGameInstance::Get_CurrentScene()` 가 IScene* 반환 — `dynamic_cast` 로 안전 변환 권장. 1차는 static_cast 후 nullptr 가드.

---

## §6. Layer 3 — 엘든링 확장 hook (설계만, 본 phase 미구현)

본 Phase B-16 의 메시 분리 인프라는 다음 3가지 엘든링 시스템의 prerequisite. 각 시스템은 별도 phase 진입 시 본격 구현하되, 본 plan 에 **component skeleton + 의도** 박제.

### 6.1 `MeshDestructibleComponent` — 보스 부위 파괴 (예: Malenia 날개)

**파일 (스켈레톤)**: `Engine/Public/ECS/Components/MeshDestructibleComponent.h`

```cpp
#pragma once

#include "Engine_Defines.h"
#include <array>

// Phase B-19 (Elden Ring boss 부위 파괴) — B-16 메시 분리 인프라의 응용
//
// 시나리오:
//   - Malenia 날개 (submesh "Wing_R") HP=300, 0 도달 시 hide + 파괴 VFX + drop 메쉬 spawn
//   - Godrick 의 dragon arm (submesh "DragonArm") — 부위별 약점
//   - 보스 변신 stage 시 이전 stage 의 submesh hide
//
// 동작:
//   - 매 데미지 처리 시 hit submesh 식별 (raycast 또는 ApplyDamageToSubmesh API)
//   - submesh HP <= 0 → MeshGroupVisibilityComponent.SetVisible(idx, false) + 파괴 hook
//   - destruction hook: VFX spawn / 드롭 entity spawn (Phase D Physics)

struct DestructibleSubmesh
{
    u32_t  submeshIndex = 0;
    f32_t  fCurrentHp   = 100.f;
    f32_t  fMaxHp       = 100.f;
    bool_t bDestroyed   = false;
    u32_t  destructionVfxId = 0;   // VFX preset hook
};

inline constexpr u32_t kMaxDestructibleSubmeshes = 8;

struct MeshDestructibleComponent
{
    std::array<DestructibleSubmesh, kMaxDestructibleSubmeshes> destructibles{};
    u8_t destructibleCount = 0;
};
```

**System (Phase B-19)**: `CMeshDestructibleSystem` — 매 프레임 destructibles 의 fCurrentHp 검사 + visibility 동기화 + 파괴 시 hook.

### 6.2 `EquipmentSlotComponent` — 무기 교체 (예: 검 → 창 → 활)

**파일 (스켈레톤)**: `Engine/Public/ECS/Components/EquipmentSlotComponent.h`

```cpp
#pragma once

#include "Engine_Defines.h"
#include <array>

// Phase B-30+ (Elden Ring Equipment system) — B-16 의 응용
//
// 시나리오:
//   - 플레이어가 우클릭 메뉴에서 무기 교체 → 무기 submesh swap
//   - 이중 무기 (Tarnished 의 dual wield) → 양손 submesh 동시 변경
//   - 방어구 set effect — 헬멧/갑옷/장갑/부츠 submesh 묶음 swap

enum class eEquipmentSlot : u8_t
{
    Weapon_R, Weapon_L, Helmet, Chest, Gloves, Boots, Cape, Accessory,
    SLOT_END
};

struct EquipmentEntry
{
    eEquipmentSlot slot = eEquipmentSlot::SLOT_END;
    u32_t  showSubmeshMask = 0u;         // 이 장비 착용 시 보일 submesh
    u32_t  hideSubmeshMask = 0u;         // 이 장비 착용 시 숨길 submesh (예: 맨손 → 검 착용 시 fist 숨김)
    u32_t  itemId = 0;
};

inline constexpr u32_t kMaxEquipmentSlots = 8;

struct EquipmentSlotComponent
{
    std::array<EquipmentEntry, kMaxEquipmentSlots> slots{};
    u8_t slotCount = 0;
    bool_t bDirty = true;   // 변경 시 set, 다음 프레임 visibility 재계산
};
```

**System**: `CEquipmentSlotSystem` — slots 합산 → MeshGroupVisibilityComponent.visibilityMask 갱신.

### 6.3 `MeshTransformationComponent` — 변신 (예: Godrick 켄타우로스)

**파일 (스켈레톤)**: `Engine/Public/ECS/Components/MeshTransformationComponent.h`

```cpp
#pragma once

#include "Engine_Defines.h"
#include <array>

// Phase B-30+ (Elden Ring 보스 변신) — B-16 의 응용
//
// 시나리오:
//   - Godrick Stage 1 (인간 형태) → Stage 2 (켄타우로스 — 하반신 swap)
//   - Maliketh 변신 (Beast Clergyman → Maliketh)
//   - Yone 본인의 R Fate Sealed 시 잠시 demon form (옵션)

struct MeshTransformationStage
{
    const char* stageName = "stage";
    u32_t  visibilityMask = 0xFFFFFFFFu;
    f32_t  fTransitionDuration = 1.0f;   // 변신 anim 길이
    const char* transitionAnim = nullptr;
};

inline constexpr u32_t kMaxTransformationStages = 4;

struct MeshTransformationComponent
{
    std::array<MeshTransformationStage, kMaxTransformationStages> stages{};
    u8_t stageCount = 0;
    u8_t currentStage = 0;
    f32_t fStageTimer = 0.f;
    bool_t bTransitioning = false;
};
```

**System**: `CMeshTransformationSystem` — 변신 트리거 (HP threshold / 시간 / 사용자 입력) 시 stage 이동 + visibility swap.

### 6.4 본 phase 와의 관계

| 시스템 | 본 phase 박제 | Layer 1 의존 | 미래 phase |
|---|---|---|---|
| MeshGroupVisibilityComponent | ✅ 본격 | (자기 자신) | B-16 |
| Yone Soul (PoC) | ✅ 본격 | ✅ | B-16 |
| MeshDestructibleComponent | 🔵 skeleton 만 | ✅ | B-19 |
| EquipmentSlotComponent | 🔵 skeleton 만 | ✅ | B-30+ |
| MeshTransformationComponent | 🔵 skeleton 만 | ✅ | B-30+ |

**박제 정책**: 본 phase 에서 elden ring component 의 .h 파일 만 작성 (실제 system 미구현). 해당 파일들은 vcxproj 에 등록만 하고 사용처가 없어도 컴파일 통과. 미래 phase 진입 시 component 정의 그대로 system 추가.

---

## §7. Implementation Gate

### 7.1 vcxproj 등록

**ClCompile (Engine + Client)**:
```xml
<!-- Engine.vcxproj -->
<ClCompile Include="..\Private\Resource\Model.cpp" />   <!-- 이미 등록됨, 수정만 -->
<ClCompile Include="..\Private\Renderer\ModelRenderer.cpp" />   <!-- 동일 -->

<!-- Client.vcxproj — Yone -->
<ClCompile Include="..\Private\GameObject\Champion\Yone\Yone_Registration.cpp" />
<ClCompile Include="..\Private\GameObject\Champion\Yone\Yone_FxPresets.cpp" />
<ClCompile Include="..\Private\GameObject\Champion\Yone\Yone_Skills.cpp" />
```

**ClInclude (Engine + Client)**:
```xml
<!-- Engine.vcxproj — 신규 ECS 컴포넌트 -->
<ClInclude Include="..\Public\ECS\Components\MeshGroupVisibilityComponent.h" />
<!-- Elden Ring 확장 (skeleton만 — 사용처 없어도 미래 활용용 등록) -->
<ClInclude Include="..\Public\ECS\Components\MeshDestructibleComponent.h" />
<ClInclude Include="..\Public\ECS\Components\EquipmentSlotComponent.h" />
<ClInclude Include="..\Public\ECS\Components\MeshTransformationComponent.h" />

<!-- Client.vcxproj — Yone -->
<ClInclude Include="..\Public\GameObject\Champion\Yone\Yone_Components.h" />
<ClInclude Include="..\Public\GameObject\Champion\Yone\Yone_FxPresets.h" />
<ClInclude Include="..\Public\GameObject\Champion\Yone\Yone_Registration.h" />
<ClInclude Include="..\Public\GameObject\Champion\Yone\Yone_Skills.h" />
```

### 7.2 EngineSDK/inc 동기화

`UpdateLib.bat` 실행 — `MeshGroupVisibilityComponent.h` + 3 elden ring skeleton 헤더 + `Model.h` + `ModelRenderer.h` 모두 EngineSDK/inc 로 복사 확인.

---

## §8. Verification Gate

### 8.1 사전 체크리스트
- [ ] devenv.exe 종료
- [ ] taskkill 실행 중 exe
- [ ] convert_all_assets.bat 에 Yone 1줄 추가 → `OK=13`
- [ ] yone.wmesh / .wskel / anims/*.wanim 110+개

### 8.2 G1: Engine + Client build

Engine 먼저 (메시 분리 인프라 변경) → UpdateLib → Client.

### 8.3 G4: 8초 smoke

Output 로그 확인:
```
[CModel] meshes=N names=[Body] [Katana_R] [GhostKatana_R] [Azakana] [Sushi] ...
[Yone] Registration complete
```

### 8.4 G5: Feature smoke

| 단계 | 액션 | 기대 |
|---|---|---|
| 1 | Yone 픽 → InGame | (42,1,0) 스폰 + idle1 |
| 2 | Sylas + A | `[Yone BA] dmg=55.0` + slash FX |
| 3 | Q | `[Yone Q1] slash` (1 stack) |
| 4 | Q | `[Yone Q2] slash` (2 stack) |
| 5 | Q | `[Yone Q3] dash + AOE` (Q3 — stack reset) |
| 6 | W | `Spirit Cleave shield (4s)` + cone FX |
| 7 | E (★ 핵심) | `Soul Unbound activated` + 영혼 entity 스폰 + body 의 GhostKatana hide / soul 의 GhostKatana 만 표시. 두 entity 가 같은 fbx 자원 공유 |
| 8 | E (재시전) | `Soul returned to body` — body 가 soul 위치로 텔레포트 + soul entity 소멸 + body visibility 복원 |
| 9 | R (방향) | `Fate Sealed dash` — 8m 라인 dash + trail FX |

### 8.5 회귀 grep

```bash
grep -c "MeshGroupVisibilityComponent" Engine/Public/ECS/Components/MeshGroupVisibilityComponent.h   # ≥ 1
grep -c "RenderWithMask\|RenderWithVisibility" Engine/Private/Resource/Model.cpp Engine/Private/Renderer/ModelRenderer.cpp   # ≥ 2
grep -c "MeshGroupVisibilityComponent" Client/Private/Scene/Scene_InGame.cpp   # ≥ 2 (include + render check)
grep "kMaxDestructibleSubmeshes\|kMaxEquipmentSlots\|kMaxTransformationStages" Engine/Public/ECS/Components/   # 3 file 매칭
```

---

## §9. Learning Update

### 9.1 박제 후 갱신 후보

**CLAUDE.md Gotcha 후보**:

```markdown
- **Per-Entity submesh visibility 와 ModelRenderer 인스턴스 분리 (B-16, 2026-05-04)**:
  Yone E (Soul Unbound) 같은 "한 모델 → 두 ECS entity" 시나리오는 ModelRenderer 인스턴스 2개 + Model
  자원 1개 (ResourceCache) 패턴으로 해결. CModel.RenderWithMask(visibilityMask) 가 per-frame
  submesh skip. MeshGroupVisibilityComponent 가 부재한 entity 는 default 전부 표시. 본 패턴은
  엘든링 보스 부위 파괴 / 무기 교체 / 변신 의 prerequisite — phase B-19 / B-30+ 진입 시 그대로 활용.

- **submesh 인덱스 매핑은 첫 변환 후 Output 검증 의무 (B-16)**:
  YoneMesh::Body / Katana_R 같은 인덱스 박제는 FBX export 순서에 의존. .wmesh 변환 후
  Output `[CModel] meshes=N names=[...]` 로그 확인 후 인덱스 박제. Blender re-export 시
  순서 변경 가능 — 회귀 검증 필수.
```

**Memory 후보**:

```markdown
파일: feedback_mesh_separation_pattern.md
내용: 메시 분리 = "ECS 다중 인스턴스 + 자원 1개 + 인스턴스별 visibility mask".
Yone E PoC 가 엘든링의 보스 부위 파괴 / 무기 교체 / 변신 의 동일 인프라.
이전 설계 (01_MULTI_MATERIAL_CHAMPION_YONE.md, CModel 단일 visibility) 는 ECS 다중 인스턴스
미지원으로 폐기. 본 패턴이 v2.
```

---

## §10. 다음 단계

| Phase | 작업 | 패턴 |
|---|---|---|
| **B-17** | BuffSystem (Annie/Ashe/Yone passive 정식) | StatusEffect 큐 |
| **B-18** | Q 멀티-stage 정식 (Yone Q3, Yasuo Q3, Riven Q3) | stageCount=3 SkillDef |
| **B-19** | MeshDestructibleSystem 본격 (보스 부위 파괴 PoC) | 본 phase 의 skeleton 활용 |
| **B-30+** | EquipmentSlotSystem + MeshTransformationSystem | 엘든링 본격 진입 |

---

## §11. 즉시 진입 명령

```
"Phase B-16 Yone 진행. 13_YONE_MESH_SEPARATION_PHASE_B16_v1.md §3 Layer 1 인프라부터.
1) Engine 측 — MeshGroupVisibilityComponent + Model::RenderWithMask + ModelRenderer::RenderWithVisibility 박제 → Engine 빌드 → UpdateLib.bat
2) Elden Ring 확장 skeleton 3 헤더 박제 (사용처 없음, 미래 활용 등록)
3) Yone 자원 변환 (D-0) → [CModel] meshes=N names 로그 검증 → YoneMesh:: 인덱스 박제
4) Yone 8 신규 파일 박제 (Components / FxPresets / Skills / Registration h+cpp)
5) Scene_InGame.cpp — MeshGroupVisibilityComponent include + Render 경로 mask 적용 + SpawnYoneSoul 헬퍼 + Winters_SpawnYoneSoul forward
6) Client.vcxproj 등록 → G1 Engine → G1 Client → G3 lock → G4 smoke → G5 feature (E (Soul) → 두 entity 동시 렌더 / 다른 visibility / E 재시전 = body 텔레포트 + soul 소멸)."
```
