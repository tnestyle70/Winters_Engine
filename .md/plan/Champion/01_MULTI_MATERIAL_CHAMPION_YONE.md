# 다중 머티리얼 챔피언 통합 계획 (Yone + Kalista 기준)

> **작성**: 2026-04-24
> **대상**: Winters Engine (DX11) — `CModel` / `ModelRenderer` / `ChampionDef`
> **트리거 챔프**:
>   - **Yone**: 13 submesh, 4 texture, 혼/평타 잔상 등 **조건부 가시성** 요구
>   - **Kalista**: 3 submesh, 창(Spear) 을 **프로젝타일·박힘 상태** 로 분리 렌더해야 함 (E Rend, Q Pierce)
> **스코프**: 데이터-드리븐 챔피언 에셋 정의 + submesh 이름 기반 바인딩/토글/분리 API. **실제 스킬 로직 미포함** (B-10 SkillHook 때 합류).

---

## 1. Context — 왜 지금 이 작업이 필요한가

현재 `Client/Private/Scene/Scene_InGame.cpp:129-181` 는 챔피언마다 `LoadMeshTexture(iIndex, path)` 를 **서브메시 인덱스로** 하드코딩한다.

```cpp
m_Irelia.LoadMeshTexture(0, L".../irelia_base_blades_tx_cm.png");
m_Irelia.LoadMeshTexture(1, L".../irelia_base_tx_cm.png");
m_Irelia.LoadMeshTexture(2, L".../irelia_base_tx_cm.png");
m_Irelia.LoadMeshTexture(3, L".../irelia_base_blades_tx_cm.png");
```

이렐리아(4개)·야스오(4개)·비에고(6개) 까진 버틴다. 그러나 **Yone 에서 폭발** 한다:

1. **서브메시 13개 × 텍스처 4종**: 인덱스 하드코딩은 실수 확률 기하급수. Assimp 의 노드 트래버스 순서가 FBX 리익스포트마다 달라질 위험이 있어 인덱스 기반 자체가 fragile.
2. **조건부 가시성 필요**:
   - `GhostKatana` / `GhostKatana_Smear` — 혼 폼 진입 / 궁극기 Q3 시에만 표시
   - `Azakana` — 평시 등에 붙음, 특정 스킬 시 분리/강조
   - `*_Smear` (Katana_Smear / GhostKatana_Smear / Bow_Smear) — 평타 스윙 window 동안만 잔상 표시
   - `Sushi` / `Fish` / `Instrument` — recall 애니메이션 전용 프롭
   - 현재 `CModel` 에는 **서브메시별 visibility API 자체가 없음**.

### 목표

- **선언형 ChampionAssetDef** 로 Yone 을 3~4줄에 셋업 가능하게 한다.
- **submesh 이름 기반** 바인딩/토글 API 도입 (인덱스 배제).
- 데이터-드리븐 설계로 `B-7a` (ModelRenderer 분해 → AssetRegistry + RenderComponent) 와 충돌하지 않게 한다.
- Irelia/Yasuo 등 기존 챔프는 **그대로 둔다** (이 PR 범위 밖). 기존 `LoadMeshTexture(index)` API 는 유지 — B-7a 에서 한꺼번에 전환.

---

## 2. 현재 아키텍처 요약 (읽기 완료한 부분)

| 클래스 | 파일 | 역할 |
|---|---|---|
| `CModel` | `Engine/Public/Resource/Model.h:17-67` | 공용 리소스. `vector<CMesh>` + `vector<CTexture>` + 스켈레톤/애니메이션 |
| `CMesh` | `Engine/Public/Resource/Mesh.h:7-31` | 단일 submesh. `m_iMaterialIndex` 보유, **이름 필드 없음** |
| `ModelRenderer` | `Engine/Public/Renderer/ModelRenderer.h:12-54` | 인스턴스 wrapper. pimpl, per-instance animator + cbuffer |
| `ChampionDef` | `Client/Public/GameObject/ChampionDef.h:4-14` | 게임플레이 메타 (animPrefix, attack key, range). **에셋 경로 없음** |
| `ChampionTable` | `Client/Private/GameObject/ChampionTable.cpp:3-16` | Irelia/Yasuo 2개 하드코딩 테이블 |
| `WMeshFormat::SubMeshDesc` | `Engine/Public/AssetFormat/Mesh/WMeshFormat.h` | 이미 `char name[20]` 필드 존재 (wmesh 경로에서 이름 복구 가능) |
| `Mesh3D.hlsl / Skinned3D.hlsl` | `Shaders/` | 단일 t0 DiffuseMap. per-submesh 스왑은 **CPU 측 BindMaterial** 로만 처리 |
| `CModel::BindMaterial` | `Engine/Private/Resource/Model.cpp:75-100` | 우선순위 체인: mesh override > global override > material index texture |
| `CModel::Render` | `Engine/Private/Resource/Model.cpp:66-73` | `for each mesh: BindMaterial(i) + mesh->Render()` — 서브메시 draw-call 분할 **이미 됨** |

**핵심 발견**: per-submesh draw call 분할과 material index → texture 매핑은 **이미 구현되어 있다**. 빠진 건 ① 이름 기반 조회와 ② 가시성 토글뿐.

---

## 3. 구현 직전 선(先)확인 2건

**0순위, 구현 진입 전 반드시 확인**:

### 3-1. `SubMeshDesc::name[20]` 이 실제로 채워지는지

`Tools/WintersAssetConverter/Private/main.cpp` 의 writer 가 Assimp `aiMesh->mName` 을 복사하는지 전수 확인.

확인 방법:
```cpp
// Tools main.cpp 에서
std::cout << "submesh[" << i << "] name=" << desc.name << "\n";
```

**예상**: 27개 기존 변환 결과 중 하나라도 `name[0] == '\0'` 이면 writer 수정 선행 (1줄):
```cpp
strncpy_s(desc.name, aiMesh->mName.C_Str(), sizeof(desc.name) - 1);
```

### 3-2. Assimp FBX 임포트 시 `aiMesh->mName` 이 submesh 이름과 일치하는지

우리 파이프라인(`glb_to_fbx_multi.py`) 은 **material name 기준** 으로 텍스처를 바인딩한다. Blender FBX exporter 는 object name 을 mesh name 으로 내보내므로, **object name = submesh name 이 되도록** Blender 에서 export 되는지 확인 필요.

확인 방법: `Engine/Private/Resource/Model.cpp:ProcessMesh` 에 `std::cout << aiMesh->mName.C_Str() << "\n";` 로그 한 줄 추가 → Yone FBX 로드해서 `Body / Katana / GhostKatana / Azakana / ...` 13개 이름이 찍히는지 확인.

**만약 이름이 `FbxMesh_0` 같은 자동 이름이면**: `glb_to_fbx_multi.py` 에서 `bpy.data.objects[...].name = material.name` 으로 export 직전에 object rename 훅 추가 (한 줄).

---

## 4. 구현 설계

### 4-1. `CMesh` 이름 필드 (저수준)

**파일**: `Engine/Public/Resource/Mesh.h:7-31`, `Engine/Private/Resource/Mesh.cpp`

```cpp
class CMesh {
    ...
    const string& GetName() const { return m_strName; }
    void SetName(const string& s) { m_strName = s; }
private:
    string m_strName;          // NEW: "Body", "GhostKatana", "Azakana"
    u32_t m_iMaterialIndex = 0;
    ...
};
```

`CMesh::Create()` 서명 변경은 하지 않는다 (기존 호출부 보존). Create 후에 호출부에서 `SetName` 으로 세팅. 비용: submesh 당 1× `std::string` — Yone 13개 × 대략 15바이트 = 200B 수준, 무시 가능.

### 4-2. `CModel` — 이름 조회 + 가시성 벡터

**파일**: `Engine/Public/Resource/Model.h`, `Engine/Private/Resource/Model.cpp`

**신규 멤버**:
```cpp
vector<bool> m_vecMeshVisible;                    // default: true, size = mesh count
unordered_map<string, u32_t> m_mapNameToMeshIndex; // normalized name → index
```

**신규 public API**:
```cpp
i32_t     FindMeshIndex(string_view name) const;          // -1 if not found
bool_t    SetMeshTextureByName(string_view name, CTexture* pTex);
bool_t    SetMeshVisible(string_view name, bool_t bVisible);
bool_t    IsMeshVisible(u32_t iMeshIndex) const;
u32_t     GetMeshCount() const;                           // 기존
const string& GetMeshName(u32_t iMeshIndex) const;        // 디버그/ImGui 용
```

**이름 정규화 규칙** — `glb_to_fbx_multi.py:23-26` 과 **비트 단위로 동일하게** C++ 포팅 (규칙 두 곳에 갈라지면 추적 지옥):
1. Lowercase (`"Body"` == `"body"`)
2. Blender 충돌 suffix `.001` 제거 (`"Body.001"` → `"body"`)
3. 기타 trim (양 끝 공백 — safety)

내부 함수 `normalize_mesh_name(string_view)` 하나 두고, 맵 삽입 시와 조회 시 **둘 다** 통과시킨다.

**`Render()` 수정** (`Model.cpp:66-73`):
```cpp
void CModel::Render(ID3D11DeviceContext* pContext) {
    for (u32_t i = 0; i < m_vecMeshes.size(); ++i) {
        if (!m_vecMeshVisible[i]) continue;      // ← 신규
        BindMaterial(pContext, i);
        m_vecMeshes[i]->Render(pContext);
    }
}
```

**로딩 훅**:
- Assimp 경로 (`ProcessMesh`, `Model.cpp:207-288`): 생성된 `CMesh` 에 `pMesh->SetName(aiMesh->mName.C_Str())`
- wmesh 경로 (`BuildMeshesFromWMesh`, `Model.cpp:28-50`): `mesh->SetName(string(desc.name))` — 이미 `SubMeshDesc::name[20]` 존재
- 로드 끝난 뒤 `RebuildNameMap()` 호출 — submesh 전체를 한 번 walk 하여 `m_mapNameToMeshIndex` 구축 + `m_vecMeshVisible` 를 `vector<bool>(count, true)` 로 초기화

### 4-3. `ModelRenderer` wrapper 확장

**파일**: `Engine/Public/Renderer/ModelRenderer.h:12-54`, `.cpp`

```cpp
bool LoadMeshTextureByName(string_view name, const wstring& path);
void SetMeshVisibleByName(string_view name, bool visible);
bool InitChampion(const ChampionAssetDef& def);   // 4-4 의 declarative setup one-shot
```

`InitChampion` 본체:
```cpp
bool ModelRenderer::InitChampion(const ChampionAssetDef& def) {
    if (!Init(def.fbxPath, L"Shaders/Skinned3D.hlsl")) return false;

    for (u32_t i = 0; i < def.textureMapCount; ++i) {
        auto& e = def.textureMap[i];
        if (!LoadMeshTextureByName(e.submeshName, e.texturePath)) {
            DEBUG_LOG("[ChampionAsset] missing submesh: %s", e.submeshName);
        }
    }
    for (u32_t i = 0; i < def.defaultHiddenCount; ++i) {
        SetMeshVisibleByName(def.defaultHiddenSubmeshes[i], false);
    }
    return true;
}
```

### 4-4. `ChampionAssetDef` — 선언형 에셋 테이블

**파일**: `Client/Public/GameObject/ChampionDef.h` — 기존 `ChampionDef` **추가**, 삭제 없음. 게임플레이 메타(`animPrefix` 등) 와 렌더 에셋 메타는 별도 struct 로 분리 (SRP).

```cpp
struct SubmeshTextureEntry {
    const char*    submeshName;    // "Body", "GhostKatana"
    const wchar_t* texturePath;    // L".../yone_base_tx_cm.png"
};

struct ChampionAssetDef {
    eChampion               id;
    const char*             fbxPath;
    const SubmeshTextureEntry* textureMap;
    u32_t                   textureMapCount;
    const char* const*      defaultHiddenSubmeshes;  // nullptr-terminated OR count-paired
    u32_t                   defaultHiddenCount;
};

const ChampionAssetDef* FindChampionAssetDef(eChampion champ);
```

**`ChampionTable.cpp` 에 Yone 추가**:

```cpp
// 기존 ChampionDef 테이블에 1줄
{ eChampion::YONE, "yone_", "idle1", "run", "attack1" },

// 신규 ChampionAssetDef 테이블
static const SubmeshTextureEntry s_YoneTex[] = {
    {"Body",              L".../Yone/yone_base_tx_cm.png"},
    {"Skirt",             L".../Yone/yone_base_tx_cm.png"},
    {"Skirt_Inner",       L".../Yone/yone_base_tx_cm.png"},
    {"Sheath",            L".../Yone/yone_base_tx_cm.png"},
    {"Azakana",           L".../Yone/yone_base_tx_cm.png"},
    {"Katana",            L".../Yone/yone_base_swords_tx_cm.png"},
    {"GhostKatana",       L".../Yone/yone_base_swords_tx_cm.png"},
    {"Katana_Smear",      L".../Yone/streak_white_tx_cm.png"},
    {"GhostKatana_Smear", L".../Yone/streak_white_tx_cm.png"},
    {"Bow_Smear",         L".../Yone/streak_white_tx_cm.png"},
    {"Sushi",             L".../Yone/yone_base_props_tx_cm.png"},
    {"Fish",              L".../Yone/yone_base_props_tx_cm.png"},
    {"Instrument",        L".../Yone/yone_base_props_tx_cm.png"},
};

static const char* s_YoneHidden[] = {
    "GhostKatana", "GhostKatana_Smear",    // 혼 폼 진입 전 숨김
    "Katana_Smear", "Bow_Smear",           // 평타 window 에서만 표시 (C-3 이후)
    "Sushi", "Fish", "Instrument",         // recall-only 프롭
};

static const ChampionAssetDef s_YoneAsset = {
    eChampion::YONE,
    "C:/Users/user/Desktop/LOL_Resource/Character/Yone/yone.fbx",
    s_YoneTex, _countof(s_YoneTex),
    s_YoneHidden, _countof(s_YoneHidden),
};
```

**주의** — `eChampion::YONE` 값이 `GameContext.h` 의 eChampion enum 에 **이미 있는지** 확인. 없으면 추가 필요 (단순 `YONE` 한 줄).

### 4-5. `Scene_InGame.cpp` 호출부 — 축소

**파일**: `Client/Private/Scene/Scene_InGame.cpp:129-181`

Yone 블록 **추가** (기존 Irelia/Yasuo/Sylas/Viego 블록은 건드리지 않음):

```cpp
// ── Yone 모델 ── (declarative)
if (const auto* def = FindChampionAssetDef(eChampion::YONE)) {
    m_Yone.InitChampion(*def);
    m_YoneTransform.SetPosition(9.f, 3.f, 0.f);
    m_YoneTransform.SetScale(0.01f);
}
```

`m_Yone` / `m_YoneTransform` 멤버는 `Scene_InGame` 헤더에 2줄 추가.

`GameContext.SelectedChampion == eChampion::YONE` 분기도 기존 Irelia/Yasuo 처럼 1 블록 추가 (`m_pPlayerRenderer = &m_Yone;` 등, `ChampionTable` 의 `animPrefix` 는 자동으로 풀림).

### 4-6. 게임플레이 훅 — 인터페이스만 준비

**이 PR 범위 밖** — 스킬 구현(`B-10 SkillHook`) 시 아래 API 를 호출하게 된다:

```cpp
// 혼 폼 진입 (궁극기 Q3 혹은 별도 ult 트리거)
m_Yone.SetMeshVisibleByName("GhostKatana", true);
m_Yone.SetMeshVisibleByName("GhostKatana_Smear", true);
m_Yone.SetMeshVisibleByName("Azakana", false);     // 등에서 분리됨

// 평타 스윙 window (C-3 AnimationEvent 기반)
m_Yone.SetMeshVisibleByName("Katana_Smear", true);
// window 만료 후
m_Yone.SetMeshVisibleByName("Katana_Smear", false);
```

`ImGui 디버그 패널` — `B-8 UI 패널 분리` 에서 ChampionDebugPanel 추가 (submesh 체크박스 목록 + 애니메이션 스크러버). 이 PR 에서는 **패널 없음**, 대신 `Scene_InGame::OnUpdate` 의 기존 ImGui 블록에 임시 디버그 UI 를 한 묶음 추가해서 육안 검증만 가능하게 한다:

```cpp
ImGui::Begin("Yone Debug (temporary)");
for (u32_t i = 0; i < m_Yone.GetMeshCount(); ++i) {
    bool v = m_Yone.IsMeshVisible(i);
    if (ImGui::Checkbox(m_Yone.GetMeshName(i).c_str(), &v))
        m_Yone.SetMeshVisible(i, v);
}
ImGui::End();
```

---

## 5. 수정 파일 전수 목록

| 파일 | 라인 기준 | 변경 |
|---|---|---|
| `Engine/Public/Resource/Mesh.h` | 7-31 | name 필드 + getter/setter |
| `Engine/Private/Resource/Mesh.cpp` | 전체 | 생성자/Create 에서 name 초기화 허용 |
| `Engine/Public/Resource/Model.h` | 17-67 | 가시성 벡터, 이름 맵, 신규 API 5개 |
| `Engine/Private/Resource/Model.cpp` | 28-50, 66-73, 207-288 | Render 필터, ProcessMesh 이름 복사, wmesh 이름 복사, 정규화/조회 구현, RebuildNameMap |
| `Engine/Public/Renderer/ModelRenderer.h` | 12-54 | ByName API 3개 + InitChampion |
| `Engine/Private/Renderer/ModelRenderer.cpp` | 전체 | Impl 위임 구현 |
| `Client/Public/GameObject/ChampionDef.h` | 4-14 | SubmeshTextureEntry / ChampionAssetDef / FindChampionAssetDef |
| `Client/Private/GameObject/ChampionTable.cpp` | 3-16 | Yone 엔트리 (ChampionDef + ChampionAssetDef) |
| `Client/Public/GameObject/GameContext.h` (또는 eChampion enum 정의 파일) | - | `YONE` enum 추가 (미존재 시) |
| `Client/Private/Scene/Scene_InGame.cpp` | 129-181 | Yone 블록 + `m_Yone`/`m_YoneTransform` 멤버 + Player 분기 1개 + ImGui 디버그 패널 |
| `Client/Public/Scene/Scene_InGame.h` | - | `ModelRenderer m_Yone;` / `CTransform m_YoneTransform;` 멤버 2줄 |
| `Tools/WintersAssetConverter/Private/main.cpp` | writer 부분 | `aiMesh->mName` → `SubMeshDesc::name` 복사 확인/수정 (§3-1) |

**총 파일 ~11개, 대부분 1~20줄 변경**. 가장 큰 변경은 `Model.h/.cpp` (이름 맵 + visibility 배열 + 정규화 함수).

---

## 6. 검증 체크리스트

### 컴파일
- [ ] `Winters.sln` 빌드 — Engine / Client / Tools / EngineSDK 전부 통과
- [ ] `WINTERS_STATIC_BUILD` / export 매크로 일관성 유지 (ModelRenderer 신규 API 에 `WINTERS_ENGINE` 붙이기)

### 런타임 (WintersLOL.exe)
- [ ] Scene_InGame 진입 — Yone FBX 로드 성공 로그 (`CModel::LoadModel` 정상 리턴)
- [ ] 13개 submesh name 로그 찍히는지 (`Body`, `Katana`, `GhostKatana`, `Azakana`, `Skirt`, `Skirt_Inner`, `Sheath`, `Katana_Smear`, `GhostKatana_Smear`, `Bow_Smear`, `Sushi`, `Fish`, `Instrument`)
- [ ] 기본 표시: Body/Skirt/Skirt_Inner/Sheath/Azakana/Katana 만 보임 (6개)
- [ ] 기본 숨김: GhostKatana/GhostKatana_Smear/Katana_Smear/Bow_Smear/Sushi/Fish/Instrument (7개)

### 시각 검증
- [ ] Yone 이 `(9, 3, 0)` 위치에 정상 스케일(0.01) 로 렌더됨
- [ ] Katana (물리 검) — swords 텍스처 (은/검푸른 칼날)
- [ ] Body — body 텍스처 (붉은색 기반 의복)
- [ ] Azakana (등) — body 텍스처 (검은 가면)
- [ ] 아무 submesh 도 분홍색 (기본 텍스처 fallback) 로 안 뜸

### 토글 검증
- [ ] ImGui 디버그 패널에서 `GhostKatana` 체크 on → 혼 검이 생겨나고, swords 텍스처로 보임 (채도 낮은 변형이 기대치)
- [ ] `Katana_Smear` on → 칼 주변에 흰색 잔상 평면이 보임 (streak_white 텍스처)
- [ ] `Sushi` / `Fish` / `Instrument` on → recall 프롭 3개 표시 (용도 확인)

### 회귀
- [ ] Irelia/Yasuo/Sylas/Viego/Kalista 는 기존 그대로 렌더됨 (어떤 API 도 제거 안 함)
- [ ] 다른 씬(Scene_Editor / Scene_Logo) 영향 없음
- [ ] `CModel::Render` 의 visibility 필터 추가로 frame time 변화 측정 — Irelia 4 submesh 기준 <1% 오차 예상

### 데이터
- [ ] `Yone/yone.fbx` 기존 파일 그대로 사용 (이미 2026-04-24 작업으로 생성됨, 13 submesh + 4 텍스처 바인딩 완료)
- [ ] PNG 경로 5종 실존 확인: `yone_base_tx_cm.png`, `yone_base_swords_tx_cm.png`, `yone_base_props_tx_cm.png`, `streak_white_tx_cm.png` (+ 추후 red 버전)

---

## 6.5. Kalista — 분리 가능한 무기 (Detachable Weapon) 케이스

### 6.5-1. 에셋 현황

```
Character/Kalista/
├── kalista.fbx                       ← 본체 (armature + 3 머티리얼 슬롯)
│     material slots: Kalista / Spear / Altar_Spear
│     bbox: x[-111,117] y[-0.2,226] z[-147,124]
├── kalista_spear.fbx                 ← NEW: 창만 (armature 유지, 106 verts)
├── kalista_spear.glb                 ← NEW: 창만 (GLB 버전, 8MB)
├── kalista_base_tx_cm.png            ← 본체
├── kalista_spearsubmesh_tx_cm.png    ← 메인 창
└── kalistaaltar_base_tx_cm.png       ← 제단(동맹 공유) 창
```

### 6.5-2. 왜 "T-pose 밑에 창이 생긴다" 는 현상이 뜨는가

Blender Edit 모드는 **armature 변형을 무시하고 raw 정점 위치** 를 보여준다. Kalista 의 Spear 정점은 바인드 포즈(T-pose) 에서 손 본의 로컬 위치 기준으로 **몸체 좌측·아래·뒤쪽 `(-85, 128, -122)`** 에 저장돼있다 (로컬 좌표, 1.0 = 0.01 world unit). 런타임 armature 스키닝이 켜지면 손 본을 따라 자연스럽게 손에 붙는다. → **버그 아님, 정상 리깅**.

엔진 사이드에서 `Skinned3D.hlsl` 은 이미 bone matrix cbuffer(b2) 로 정점을 본 변환하므로 **그냥 로드만 하면 손에 붙어 렌더된다**. Scene_InGame 에서 Kalista 가 정지 상태일 때도 `idle` 애니메이션이 돌아가야 창이 제자리에 있는다. 최초 프레임(애니메이션 인덱스 0, time 0) 에서 창이 공중에 떠 있으면 = idle 애니메이션 미기동.

### 6.5-3. 왜 별도 `kalista_spear.fbx` 가 필요한가

Kalista 의 **E 스킬 (Rend)** 은 적에게 박힌 창들을 한꺼번에 회수하는 메카니즘. **Q 스킬 (Pierce)** 은 창을 직선 프로젝타일로 발사. 이 둘은 **본체 armature 와 무관한 개별 창 인스턴스** 가 필요하다:

| 시나리오 | 필요 에셋 | 왜 |
|---|---|---|
| E Rend — 적에 박힌 창들 | **정적 단일 창 mesh** (armature 없음, 106 verts, 한 프레임 freeze 포즈) | 적 월드 좌표에 고정. 본체 armature 따라다니면 안 됨. |
| Q Pierce — 공중 프로젝타일 | **정적 단일 창 mesh** + 궤적 파티클 | 매 프레임 월드 위치 업데이트. 회전만 방향 따라감. |
| W — 제단(Oathsworn) 창 | 본체 Altar_Spear submesh 그대로 | 공유 창은 본체에 붙어있음 (변수 디자인, 이 계획 밖) |
| 기본 평타/이동 | 본체 `kalista.fbx` 의 Spear submesh (armature 스키닝) | 손에 자동 부착 |

**이 계획의 kalista_spear.fbx** 는 armature 를 **유지** 하고 있다. 이유:
- Blender 에서 "creating detached mesh" 표준 절차는 faces 삭제 후 armature 유지 (bones 은 weight 의존성)
- 런타임에서 엔진이 armature 를 무시하고 static 으로 로드할 수 있음 (bone_count > 0 이지만 skinning 무력화)
- **또는** 엔진 측에서 armature 벗긴 별도 asset 을 생성: `kalista_spear_static.fbx` (weight 정보 폐기, 정점을 world-relative 로 베이킹)

**권장 구조** — 런타임 2종 에셋:

```
Resources/Models/Kalista/
├── kalista.fbx              ← 본체 (armature + 3 머티리얼)
├── kalista_spear.fbx        ← 프로젝타일 / 박힘용 (armature 유지 또는 정적)
└── kalista_altar_spear.fbx  ← (선택) Altar_Spear submesh 만 별도 추출 필요 시
```

### 6.5-4. `ChampionAssetDef` 확장 — DetachedMeshEntry

현재 설계(§4-4) 에 **분리 가능 submesh 정의** 를 추가. 기본 `textureMap` 과 별개:

```cpp
struct DetachedMeshEntry {
    const char* submeshName;       // "Spear" - 본체에서 이 submesh 는 숨길 수도 있음
    const char* detachedAssetPath; // "Resources/Models/Kalista/kalista_spear.fbx"
    const wchar_t* texturePath;    // L".../kalista_spearsubmesh_tx_cm.png"
    bool        hideOnMain;        // true: 분리 인스턴스 스폰 시 본체의 해당 submesh 숨김
};

struct ChampionAssetDef {
    // ... 기존 필드 (fbxPath, textureMap, defaultHidden) ...
    const DetachedMeshEntry* detachedMeshes;   // NEW
    u32_t                    detachedMeshCount;
};
```

Kalista 엔트리 예:
```cpp
static const DetachedMeshEntry s_KalistaDetached[] = {
    { "Spear",
      "C:/.../Kalista/kalista_spear.fbx",
      L"C:/.../Kalista/kalista_spearsubmesh_tx_cm.png",
      false },   // 평타 시엔 본체 창도 보이고, 투사체도 보이는 게 정상 (LoL 에선 한 번에 1개만 존재하지만 설계상 유연하게)
};
```

**런타임 사용 플로우** (B-10 SkillHook 때 구현):
```cpp
// Q Pierce 시전
auto pProjectile = g_SpawnSystem.SpawnProjectile(
    kalistaDef.detachedMeshes[0].detachedAssetPath,  // kalista_spear.fbx
    kalistaDef.detachedMeshes[0].texturePath,
    casterPos, targetDir, kalistaQ_speed, kalistaQ_range);

// E Rend — 적에 박힌 창은 피격 시 RendAccumulator 에 박히고, 적 위치에 프로젝타일 mesh 고정
g_SpawnSystem.SpawnStaticAt(kalistaDef.detachedMeshes[0].detachedAssetPath, enemyPos);
```

### 6.5-5. 구현 영향 (§5 표에 추가)

| 파일 | 변경 |
|---|---|
| `Client/Public/GameObject/ChampionDef.h` | `DetachedMeshEntry` struct + `detachedMeshes`/`count` 필드 추가 |
| `Client/Private/GameObject/ChampionTable.cpp` | Kalista 엔트리 등록 (본체 + Spear detached) |
| `Engine/Public/Renderer/ModelRenderer.h/.cpp` | `InitChampion(def)` 에서 `detachedMeshes` 는 **사전 로드만** (메시를 AssetRegistry 에 등록, 런타임 스폰은 별도 시스템 담당) |
| `Client/Private/Scene/Scene_InGame.cpp` | Kalista 기존 `LoadTextureForAllMeshes` 단일 호출을 `InitChampion(def)` 로 치환 (기존 1장 → 3장 정상화) |

### 6.5-6. 에셋 파이프라인 팁 (도구 사이드)

`LOL_CHAMPION_EXTRACT_GUIDE.md` 에 **Phase 6 신규 섹션** "분리 무기 에셋 추출" 으로 승격 예정:

1. **`extract_submesh.py`** (`LOL_Resource/Character/extract_submesh.py`) — glb 임포트 → 지정 머티리얼 외 face 삭제 → 선택적 loose vert 청소 → FBX+GLB 이중 출력
   ```cmd
   blender --background --python extract_submesh.py -- <src_glb> <out_base> <keep_mat> [<mat2> ...]
   ```
2. **Icosphere 자동 제거**: lol2gltf 가 모든 glb 에 머티리얼 없는 42 vert Icosphere 를 placeholder 로 삽입. `glb_to_fbx_multi.py` 가 material_slots 비어있는 mesh 를 import 직후 purge.
3. **Armature 유지 vs 정적 mesh**: E Rend / Q Pierce 처럼 본과 무관하게 월드 공간에 박히는 용도면 **armature 벗긴 정적 FBX** 가 이상적. `extract_submesh.py` 에 `--strip-armature` 옵션 추가 예정 (현재 미구현).

### 6.5-7. 검증 체크리스트 (Kalista 전용)

- [ ] 본체 Kalista 가 3 텍스처 정상 바인딩 (Body=본체 PNG, Spear=창 PNG, Altar_Spear=제단 PNG)
- [ ] idle 애니메이션 기동 시 창이 손에 자연스럽게 붙음 (바인드 포즈가 아닌 animated pose)
- [ ] `kalista_spear.fbx` 단독 로드 — armature 94본 유지 + 106 verts 만 렌더 (추후 정적 변형 시 armature 벗김)
- [ ] DetachedMeshEntry 런타임 preload 로 `kalista_spear.fbx` 가 AssetRegistry 에 등록되어 있음 (실제 스폰은 B-10)

---

## 7. 스코프 밖 (다음 세션/단계)

| 항목 | 언제 | 의존 |
|---|---|---|
| Yone 스킬 실제 구현 (혼 폼, Q3 콤보, R) | B-10 SkillHook | 이 계획의 `SetMeshVisibleByName` 사용 |
| Kalista 스킬 실제 구현 (Q Pierce 발사, E Rend 박힘/회수) | B-10 SkillHook | 이 계획의 `DetachedMeshEntry` + SpawnSystem |
| AnimationEvent 기반 `*_Smear` 자동 토글 | C-3 (Socket + Hitbox + AnimEvent) | - |
| `extract_submesh.py --strip-armature` 옵션 | 도구 팀 | 정적 프로젝타일 메시 전용 |
| 스킨 시스템 `LoadWithSkin(champId, skinId)` | B-7b ChampionSpawnSystem | ChampionAssetDef 에 `skins[]` 필드 확장 |
| Irelia/Yasuo/Sylas/Viego/Kalista 의 InitChampion 이전 | B-7a ModelRenderer 분해 | 기존 `LoadMeshTexture(index)` 제거 동시 |
| ChampionDebugPanel (정식 ImGui 패널) | B-8 UI 패널 분리 | 이 계획의 임시 ImGui 블록 삭제 |
| `.wmesh` 경로로 Yone 전환 (스키닝 지원) | .wmesh Stage 3 (wanim/wskel) | `SubMeshDesc::name` 이미 준비됨 |

---

## 8. 핵심 의사결정 로그

1. **이름 기반 vs 인덱스 기반**: 이름. Assimp 트래버스 순서 불안정 + 리익스포트 위험.
2. **ChampionDef 확장 vs 신규 ChampionAssetDef**: 신규. 게임플레이 메타(ChampionDef) 와 렌더 에셋 메타 분리 — SRP. 향후 skin 변형도 AssetDef 쪽에 확장 예정.
3. **기존 `LoadMeshTexture(index)` 제거 vs 보존**: 보존. 이 PR 은 **추가만**, Irelia/Yasuo 는 B-7a 에서 한꺼번에 리팩터.
4. **visibility 구현 방식**: CPU 측 `Render()` 루프 skip. 샤더 측 alpha threshold 는 submesh 가 항상 파이프라인에 들어가서 낭비 — skip 이 우월.
5. **정규화 규칙**: Python 파이프라인(`glb_to_fbx_multi.py:23-26`) 과 비트 단위 동일. 규칙 두 곳에 갈라지면 디버그 지옥.
6. **AnimationEvent 로 Smear 자동 토글**: 이 PR 범위 밖 (C-3). 지금은 수동 API + ImGui 검증까지만.
