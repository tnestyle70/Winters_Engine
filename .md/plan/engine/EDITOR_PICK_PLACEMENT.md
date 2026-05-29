# Editor 마우스 피킹 / 배치 시스템 — 구현 계획서 (H/CPP 전문)

**상태**: 설계 확정, 사용자 수정 후 검토 요청 예정
**의존**: `IWintersApp::OnImGui` 훅 (Phase B-5 완료), `ModelRenderer` (Pimpl), `CInput`, `CCamera`, Scene_InGame
**레퍼런스**: `SR_Minecraft_Dungeons/Engine/Code/CCalculator.cpp`, `Client/Code/CBlockMgr.cpp`,
`Client/Code/CBlockPlacer.cpp`

## Context

Scene_InGame 의 44개 맵 오브젝트를 ImGui 슬라이더로만 조작하는 현재 UX 는 비효율. 사용자가 익숙한
DX9 마인크래프트 방식 (마우스 피킹 → 좌클릭 배치 / 우클릭 삭제 / 드래그 이동 + ImGui Inspector 세밀
조정) 으로 개편. Collider 시스템 (Phase C-4) 기다리지 않고 수동 Ray-AABB slab 검사로 즉시 구현.

## 참조 파일 현재 상태 (Read 완료)

| 파일 | 필요 변경 |
|---|---|
| `Engine/Public/Renderer/ModelRenderer.h` | **Getter 추가**: `GetLocalAABBMin/Max`, `HasValidAABB` |
| `Engine/Private/Renderer/ModelRenderer.cpp` | Assimp 로드 시 정점 min/max 계산 → `Impl` 에 저장 |
| `Engine/Public/Renderer/CCamera.h` | **변경 없음** — `GetViewMatrix/ProjectionMatrix/ViewProjection/GetEye` 이미 존재 |
| `Engine/Public/Core/CInput.h` | **마우스 LButton 상태 추가** (`IsLButtonDown`, `OnLButtonDown/Up`) |
| `Engine/Private/Platform/CWin32Window.cpp` | WndProc 에 `WM_LBUTTONDOWN/UP` 분기 추가 |
| `Client/Public/Scene/Scene_InGame.h` | `MapObject` 에 `ObjectType` enum, API `SpawnObject/RemoveObject`, 피커 보유 |
| `Client/Private/Scene/Scene_InGame.cpp` | `OnUpdate` 에서 피커 위임, `OnImGui` 에 팔레트 추가 |
| `Client/Include/Client.vcxproj(.filters)` | 신규 Editor 파일 등록 |

## 신규 파일

- `Client/Public/Editor/EditorPicker.h`
- `Client/Private/Editor/EditorPicker.cpp`
- `Client/Public/Editor/EditorPlacer.h`
- `Client/Private/Editor/EditorPlacer.cpp`

---

# 1. ModelRenderer — AABB Getter 추가

## 1.1 `Engine/Public/Renderer/ModelRenderer.h` 수정

**Before** (L10-L36):
```cpp
public:
	ModelRenderer();
	~ModelRenderer();

	bool	Init(const std::string& strFbxPath,
		const wchar_t* pHlslPath = L"Shaders/Mesh3D.hlsl");
	void	UpdateTransform(const Mat4& matWorld);
	void	UpdateCamera(const Mat4& matViewProj);
	void	Render();
	void	Shutdown();

	//텍스쳐 로드
	bool LoadMeshTexture(uint32_t iMeshIndex, const wstring& strPath);
	void LoadTextureForAllMeshes(const std::wstring& strPath);
	uint32 GetMeshCount() const;

	// 텍스처 수동 로드 (FBX에 텍스처 경로 없을 때)
	bool	LoadTexture(const std::wstring& strPath);

	//애니메이션
	void Update(f32_t fDeltaTime);
	void PlayAnimation(uint32 iIndex);
	void PlayAnimationByName(const std::string& strKeyword);
	bool HasSkeleton() const;

	uint32	GetAnimationCount() const;
```

**After**: `GetAnimationCount` 아래에 AABB getter 3개 추가.
```cpp
public:
	ModelRenderer();
	~ModelRenderer();

	bool	Init(const std::string& strFbxPath,
		const wchar_t* pHlslPath = L"Shaders/Mesh3D.hlsl");
	void	UpdateTransform(const Mat4& matWorld);
	void	UpdateCamera(const Mat4& matViewProj);
	void	Render();
	void	Shutdown();

	//텍스쳐 로드
	bool LoadMeshTexture(uint32_t iMeshIndex, const wstring& strPath);
	void LoadTextureForAllMeshes(const std::wstring& strPath);
	uint32 GetMeshCount() const;

	// 텍스처 수동 로드 (FBX에 텍스처 경로 없을 때)
	bool	LoadTexture(const std::wstring& strPath);

	//애니메이션
	void Update(f32_t fDeltaTime);
	void PlayAnimation(uint32 iIndex);
	void PlayAnimationByName(const std::string& strKeyword);
	bool HasSkeleton() const;

	uint32	GetAnimationCount() const;

	// ── 로컬 AABB (피킹 / 에디터용) ─────────────────────────
	// Init() 성공 후에만 유효. 모든 서브메시 정점의 min/max.
	bool	HasValidAABB() const;
	Vec3	GetLocalAABBMin() const;
	Vec3	GetLocalAABBMax() const;
```

## 1.2 `Engine/Private/Renderer/ModelRenderer.cpp` 수정

**위치**: `Impl` 구조체 내부에 멤버 2개 + flag 추가.

**수정 사항 (Impl 구조체)**:
```cpp
// ModelRenderer.cpp 내부의 struct ModelRenderer::Impl 에
// 기존 멤버들 아래에 추가:

Vec3    m_vLocalAABBMin = { FLT_MAX,  FLT_MAX,  FLT_MAX};
Vec3    m_vLocalAABBMax = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
bool    m_bAABBValid    = false;
```

**위치**: `Init()` 내에서 Assimp `aiScene` 순회 시 모든 `aiMesh` 의 정점 순회 후 min/max 갱신.

**추가 코드 예시** (이미 존재하는 메시 로딩 루프 내부에 삽입):
```cpp
// Impl::Init() — Assimp aiMesh 순회 loop 내부
for (uint32_t v = 0; v < pMesh->mNumVertices; ++v)
{
    const aiVector3D& p = pMesh->mVertices[v];

    if (p.x < m_vLocalAABBMin.x) m_vLocalAABBMin.x = p.x;
    if (p.y < m_vLocalAABBMin.y) m_vLocalAABBMin.y = p.y;
    if (p.z < m_vLocalAABBMin.z) m_vLocalAABBMin.z = p.z;

    if (p.x > m_vLocalAABBMax.x) m_vLocalAABBMax.x = p.x;
    if (p.y > m_vLocalAABBMax.y) m_vLocalAABBMax.y = p.y;
    if (p.z > m_vLocalAABBMax.z) m_vLocalAABBMax.z = p.z;
}
m_bAABBValid = true;   // 메시 루프 전체 종료 후 1회 세팅
```

**위치**: `.cpp` 파일 맨 아래 getter 구현:
```cpp
// ─── AABB Getters ────────────────────────────────────────

bool ModelRenderer::HasValidAABB() const
{
    return m_pImpl ? m_pImpl->m_bAABBValid : false;
}

Vec3 ModelRenderer::GetLocalAABBMin() const
{
    return (m_pImpl && m_pImpl->m_bAABBValid)
        ? m_pImpl->m_vLocalAABBMin
        : Vec3{ 0.f, 0.f, 0.f };
}

Vec3 ModelRenderer::GetLocalAABBMax() const
{
    return (m_pImpl && m_pImpl->m_bAABBValid)
        ? m_pImpl->m_vLocalAABBMax
        : Vec3{ 0.f, 0.f, 0.f };
}
```

**주의**: `ModelRenderer` 는 Pimpl 이므로 공개 헤더에서 `Impl` 구조체 변경이 외부에 영향 없음. ABI
안정. EngineSDK 동기화 시 `ModelRenderer.h` 만 복사하면 됨.

---

# 2. CInput — 좌클릭 상태 추가

## 2.1 `Engine/Public/Core/CInput.h` 수정

**Before** (L46-L54):
```cpp
    bool  IsRButtonDown() const { return m_bRButton; }

    // ── WndProc에서 호출 (엔진 내부) ──────────────────────────
    void OnKeyDown(uint8 vKey) { m_Keys[vKey] = true; }
    void OnKeyUp(uint8 vKey) { m_Keys[vKey] = false; }
    void OnMouseMove(int32 x, int32 y);
    void OnRawMouseDelta(int32 dx, int32 dy);
    void OnRButtonDown() { m_bRButton = true; }
    void OnRButtonUp() { m_bRButton = false; }
```

**After**: 좌버튼 상태 + edge 감지용 prev 플래그 추가.
```cpp
    bool  IsRButtonDown() const { return m_bRButton; }
    bool  IsLButtonDown() const { return m_bLButton; }

    // Edge 감지 — 이번 프레임에 눌림 / 떼짐 (클릭 판정)
    bool  IsLButtonPressed()  const { return  m_bLButton && !m_bLButtonPrev; }
    bool  IsLButtonReleased() const { return !m_bLButton &&  m_bLButtonPrev; }
    bool  IsRButtonPressed()  const { return  m_bRButton && !m_bRButtonPrev; }
    bool  IsRButtonReleased() const { return !m_bRButton &&  m_bRButtonPrev; }

    // ── WndProc에서 호출 (엔진 내부) ──────────────────────────
    void OnKeyDown(uint8 vKey) { m_Keys[vKey] = true; }
    void OnKeyUp(uint8 vKey) { m_Keys[vKey] = false; }
    void OnMouseMove(int32 x, int32 y);
    void OnRawMouseDelta(int32 dx, int32 dy);
    void OnRButtonDown() { m_bRButton = true; }
    void OnRButtonUp() { m_bRButton = false; }
    void OnLButtonDown() { m_bLButton = true; }
    void OnLButtonUp() { m_bLButton = false; }
```

**EndFrame 수정** (L57):
```cpp
    // ── 프레임 끝에 호출 — 델타 초기화 + 버튼 prev 갱신 ───────
    void EndFrame()
    {
        m_MouseDeltaX = 0.f;
        m_MouseDeltaY = 0.f;
        m_bLButtonPrev = m_bLButton;
        m_bRButtonPrev = m_bRButton;
    }
```

**멤버 추가** (L68 주변):
```cpp
private:
    CInput() = default;

    HWND m_hWnd = nullptr;

    bool  m_Keys[256] = {};
    int32 m_MouseX = 0;
    int32 m_MouseY = 0;
    f32_t m_MouseDeltaX = 0.f;
    f32_t m_MouseDeltaY = 0.f;
    bool  m_bRButton = false;
    bool  m_bLButton = false;
    bool  m_bRButtonPrev = false;
    bool  m_bLButtonPrev = false;
};
```

## 2.2 `Engine/Private/Platform/CWin32Window.cpp` 수정

**WndProc 에 LBUTTON 처리 추가**. 기존 `WM_RBUTTONDOWN/UP` 분기 옆에:

```cpp
case WM_LBUTTONDOWN:
    CInput::Get().OnLButtonDown();
    return 0;

case WM_LBUTTONUP:
    CInput::Get().OnLButtonUp();
    return 0;
```

**대안**: 마크 프로젝트처럼 `GetAsyncKeyState(VK_LBUTTON) & 0x8000` 직접 사용하면 CInput 수정
**전혀 불필요**. Edge 감지도 피커 내부 상태로 관리 가능. 더 간단한 이 경로 채택 권장 → **본 계획서는
2.1 ~ 2.2 대신 피커 내부에서 `GetAsyncKeyState` 사용하는 방식으로 진행**. 2.1/2.2 는 장기적
깔끔함 위한 옵션으로 문서화.

---

# 3. 신규 파일 — EditorPicker

## 3.1 `Client/Public/Editor/EditorPicker.h` (전문)

```cpp
#pragma once
#include "Defines.h"

//──────────────────────────────────────────────────────────────
// CEditorPicker
//   마우스 스크린 좌표 → 월드 공간 Ray 변환
//   Ray vs AABB (slab) 정밀 교차
//   Ray vs 수평면 (Y=const) 교차 — 바닥 피킹
//
//   CCollider 시스템 독립. ModelRenderer::GetLocalAABBMin/Max 와
//   Transform 만 있으면 동작.
//
//   레퍼런스: SR_Minecraft_Dungeons CCalculator::ComputePickRay,
//            CBlockMgr::RayAABBIntersectWithNormal.
//──────────────────────────────────────────────────────────────

class CCamera;
class ModelRenderer;
class CTransform;

struct PickRay
{
    Vec3  origin    = {};
    Vec3  direction = { 0.f, 0.f, 1.f };   // 단위벡터 보장
};

struct PickAABBResult
{
    bool  bHit   = false;
    f32_t t      = FLT_MAX;
    Vec3  point  = {};
    Vec3  normal = {};                     // 히트 면 법선 (±X, ±Y, ±Z)
};

class CEditorPicker
{
public:
    CEditorPicker() = default;
    ~CEditorPicker() = default;

    // 매 프레임 카메라/윈도우 변경 시 갱신
    void UpdateContext(CCamera* pCamera, HWND hWnd, u32_t viewportW, u32_t viewportH);

    // 현재 마우스 위치 기반 Ray (또는 명시적 스크린 좌표)
    PickRay BuildRayFromMouse() const;
    PickRay BuildRayFromScreen(i32_t screenX, i32_t screenY) const;

    // Ray ↔ AABB 슬랩 검사 (법선 포함)
    // 레이 최소 거리 hit 만 의미 있음.
    static bool RayAABB_Slab(const PickRay& ray,
                             const Vec3& aabbMin, const Vec3& aabbMax,
                             f32_t& outT, Vec3& outNormal);

    // Ray ↔ Y=planeY 수평면 교차 (지형 대체)
    // 리턴 false = 평행 또는 뒤쪽
    static bool RayGroundPlane(const PickRay& ray, f32_t planeY, Vec3& outHit);

    // ModelRenderer + Transform → 월드 AABB 계산
    // ModelRenderer 가 HasValidAABB() false 시 단위 박스 fallback.
    static void ComputeWorldAABB(const ModelRenderer& mr,
                                 CTransform& transform,
                                 Vec3& outMin, Vec3& outMax);

    // 마우스 현재 위치가 클라이언트 영역 내부인지
    bool IsMouseInClient() const;

private:
    CCamera*  m_pCamera      = nullptr;
    HWND      m_hWnd         = nullptr;
    u32_t     m_viewportW    = 1;
    u32_t     m_viewportH    = 1;
};
```

## 3.2 `Client/Private/Editor/EditorPicker.cpp` (전문)

```cpp
#include "Editor/EditorPicker.h"
#include "CCamera.h"
#include "CTransform.h"
#include "ModelRenderer.h"
#include <algorithm>

using namespace DirectX;

//──────────────────────────────────────────────────────────────
// UpdateContext
//──────────────────────────────────────────────────────────────
void CEditorPicker::UpdateContext(CCamera* pCamera, HWND hWnd,
                                   u32_t viewportW, u32_t viewportH)
{
    m_pCamera   = pCamera;
    m_hWnd      = hWnd;
    m_viewportW = (viewportW > 0) ? viewportW : 1;
    m_viewportH = (viewportH > 0) ? viewportH : 1;
}

//──────────────────────────────────────────────────────────────
// IsMouseInClient
//──────────────────────────────────────────────────────────────
bool CEditorPicker::IsMouseInClient() const
{
    if (!m_hWnd) return false;

    POINT pt;
    if (!GetCursorPos(&pt)) return false;
    ScreenToClient(m_hWnd, &pt);

    RECT rc;
    GetClientRect(m_hWnd, &rc);

    return (pt.x >= 0 && pt.x < rc.right &&
            pt.y >= 0 && pt.y < rc.bottom);
}

//──────────────────────────────────────────────────────────────
// BuildRayFromMouse
//──────────────────────────────────────────────────────────────
PickRay CEditorPicker::BuildRayFromMouse() const
{
    if (!m_hWnd) return {};

    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(m_hWnd, &pt);

    return BuildRayFromScreen((i32_t)pt.x, (i32_t)pt.y);
}

//──────────────────────────────────────────────────────────────
// BuildRayFromScreen
//
//   마크 CCalculator::ComputePickRay 의 DX11 포팅.
//   NDC → ViewSpace → WorldSpace 역행렬 연쇄.
//──────────────────────────────────────────────────────────────
PickRay CEditorPicker::BuildRayFromScreen(i32_t screenX, i32_t screenY) const
{
    PickRay r;
    if (!m_pCamera) return r;

    // 1. NDC (-1 ~ 1)
    const f32_t ndcX = (2.f * (f32_t)screenX / (f32_t)m_viewportW) - 1.f;
    const f32_t ndcY = 1.f - (2.f * (f32_t)screenY / (f32_t)m_viewportH);

    // 2. Clip → View
    XMMATRIX proj     = XMLoadFloat4x4(&m_pCamera->GetProjectionMatrix());
    XMMATRIX projInv  = XMMatrixInverse(nullptr, proj);
    XMVECTOR clipNear = XMVectorSet(ndcX, ndcY, 0.f, 1.f);
    XMVECTOR viewPos  = XMVector3TransformCoord(clipNear, projInv);

    // 3. View → World
    XMMATRIX view     = XMLoadFloat4x4(&m_pCamera->GetViewMatrix());
    XMMATRIX viewInv  = XMMatrixInverse(nullptr, view);

    // 카메라 월드 좌표 = inverse(view) * origin
    XMVECTOR worldOrigin = XMVector3TransformCoord(XMVectorZero(), viewInv);
    XMVECTOR worldDir    = XMVectorSubtract(
        XMVector3TransformCoord(viewPos, viewInv),
        worldOrigin);
    worldDir = XMVector3Normalize(worldDir);

    XMStoreFloat3(&r.origin,    worldOrigin);
    XMStoreFloat3(&r.direction, worldDir);
    return r;
}

//──────────────────────────────────────────────────────────────
// RayAABB_Slab  (Static)
//
//   마크 RayAABBIntersectWithNormal 이식.
//   각 축별 slab 검사 + 히트 축 기록 → 법선 복원.
//──────────────────────────────────────────────────────────────
bool CEditorPicker::RayAABB_Slab(const PickRay& ray,
                                  const Vec3& aabbMin, const Vec3& aabbMax,
                                  f32_t& outT, Vec3& outNormal)
{
    f32_t tMin = 0.f;
    f32_t tMax = FLT_MAX;

    i32_t hitAxis = -1;      // 0=X, 1=Y, 2=Z
    bool  negDir  = false;   // ray.direction > 0 이면 음(-)면 진입

    const f32_t origin[3] = { ray.origin.x,    ray.origin.y,    ray.origin.z };
    const f32_t dir[3]    = { ray.direction.x, ray.direction.y, ray.direction.z };
    const f32_t bMin[3]   = { aabbMin.x, aabbMin.y, aabbMin.z };
    const f32_t bMax[3]   = { aabbMax.x, aabbMax.y, aabbMax.z };

    for (i32_t i = 0; i < 3; ++i)
    {
        if (std::fabs(dir[i]) < 1e-6f)
        {
            // 평행 — origin 이 슬랩 밖이면 miss
            if (origin[i] < bMin[i] || origin[i] > bMax[i]) return false;
            continue;
        }

        f32_t t1 = (bMin[i] - origin[i]) / dir[i];
        f32_t t2 = (bMax[i] - origin[i]) / dir[i];

        if (t1 > t2) std::swap(t1, t2);

        if (t1 > tMin)
        {
            tMin    = t1;
            hitAxis = i;
            negDir  = (dir[i] > 0.f);   // 진입 면은 -면 (법선 반대)
        }
        tMax = std::min(tMax, t2);
        if (tMin > tMax) return false;
    }

    if (hitAxis < 0) return false;
    if (tMin < 0.f)  return false;       // 카메라 뒤쪽

    outT = tMin;
    outNormal = { 0.f, 0.f, 0.f };
    (&outNormal.x)[hitAxis] = negDir ? -1.f : 1.f;
    return true;
}

//──────────────────────────────────────────────────────────────
// RayGroundPlane
//──────────────────────────────────────────────────────────────
bool CEditorPicker::RayGroundPlane(const PickRay& ray, f32_t planeY, Vec3& outHit)
{
    if (std::fabs(ray.direction.y) < 1e-4f) return false;

    const f32_t t = (planeY - ray.origin.y) / ray.direction.y;
    if (t < 0.f) return false;

    outHit.x = ray.origin.x + ray.direction.x * t;
    outHit.y = planeY;
    outHit.z = ray.origin.z + ray.direction.z * t;
    return true;
}

//──────────────────────────────────────────────────────────────
// ComputeWorldAABB
//
//   8 corner 변환 후 각 축 min/max 재계산.
//   회전이 있으면 AABB 가 팽창된다 (Ericson 4.2.6).
//──────────────────────────────────────────────────────────────
void CEditorPicker::ComputeWorldAABB(const ModelRenderer& mr,
                                      CTransform& transform,
                                      Vec3& outMin, Vec3& outMax)
{
    Vec3 lMin = mr.HasValidAABB()
        ? mr.GetLocalAABBMin()
        : Vec3{ -50.f, -50.f, -50.f };  // Fallback (FBX 가 비정상인 경우)
    Vec3 lMax = mr.HasValidAABB()
        ? mr.GetLocalAABBMax()
        : Vec3{  50.f,  50.f,  50.f };

    const Mat4& mat = transform.GetWorldMatrix();
    XMMATRIX world  = XMLoadFloat4x4(&mat);

    // 8 corner 생성
    Vec3 corners[8] = {
        { lMin.x, lMin.y, lMin.z },
        { lMax.x, lMin.y, lMin.z },
        { lMin.x, lMax.y, lMin.z },
        { lMax.x, lMax.y, lMin.z },
        { lMin.x, lMin.y, lMax.z },
        { lMax.x, lMin.y, lMax.z },
        { lMin.x, lMax.y, lMax.z },
        { lMax.x, lMax.y, lMax.z },
    };

    XMVECTOR vMin = XMVectorReplicate( FLT_MAX);
    XMVECTOR vMax = XMVectorReplicate(-FLT_MAX);

    for (i32_t i = 0; i < 8; ++i)
    {
        XMVECTOR c = XMLoadFloat3(&corners[i]);
        c = XMVector3TransformCoord(c, world);
        vMin = XMVectorMin(vMin, c);
        vMax = XMVectorMax(vMax, c);
    }

    XMStoreFloat3(&outMin, vMin);
    XMStoreFloat3(&outMax, vMax);
}
```

---

# 4. 신규 파일 — EditorPlacer

## 4.1 `Client/Public/Editor/EditorPlacer.h` (전문)

```cpp
#pragma once
#include "Defines.h"
#include "Editor/EditorPicker.h"
#include <stack>

//──────────────────────────────────────────────────────────────
// CEditorPlacer
//   팔레트 기반 오브젝트 배치 / 선택 / 드래그 / 삭제 / Undo
//
//   Scene_InGame 에 포함되어 동작. 씬이 오브젝트 배열 소유, 피커가
//   피킹만 담당. Placer 는 상태 머신 + 입력 → 씬 API 호출.
//
//   레퍼런스: CBlockPlacer::Update_Placer.
//──────────────────────────────────────────────────────────────

class CScene_InGame;
class CCamera;

enum class EPaletteType : u32_t
{
    None = 0,
    Turret_Blue, Turret_Red,
    Inhibitor_Blue, Inhibitor_Red,
    Nexus_Blue, Nexus_Red,
    Jungle_Baron, Jungle_Dragon,
    Jungle_Blue, Jungle_Krug, Jungle_Gromp, Jungle_Wolf,
    Minion_OrderMelee, Minion_OrderRanged, Minion_OrderSiege, Minion_OrderSuper,
    Minion_ChaosMelee, Minion_ChaosRanged, Minion_ChaosSiege, Minion_ChaosSuper,
    Count
};

struct UndoRecord
{
    enum class Kind : u8_t { Spawn, Remove, Move };
    Kind        kind;
    i32_t       objectIdx;
    EPaletteType type;
    Vec3        before;
    Vec3        after;
};

class CEditorPlacer
{
public:
    CEditorPlacer() = default;
    ~CEditorPlacer() = default;

    // 매 프레임 씬에서 호출
    void Update(CScene_InGame* pScene, CCamera* pCamera, HWND hWnd,
                u32_t viewportW, u32_t viewportH, f32_t dt);

    // ImGui 팔레트 패널 (Scene_InGame::OnImGui 에서 호출)
    void DrawPaletteWindow();

    // 상태 조회 (씬이 Hierarchy 강조 등에 사용)
    bool         IsEditMode() const       { return m_bEditMode; }
    bool         IsDragging() const       { return m_bDragging; }
    EPaletteType GetPalette() const       { return m_palette; }
    i32_t        GetHoveredIndex() const  { return m_hoveredIdx; }

    void SetEditMode(bool b) { m_bEditMode = b; }
    void SetGroundPlaneY(f32_t y) { m_groundPlaneY = y; }
    void SetSnapEnabled(bool b)   { m_bSnap = b; }
    void SetSnapSize(f32_t s)     { m_snapSize = (s > 0.01f) ? s : 0.01f; }

private:
    // 단계별 처리
    void HandleHover(CScene_InGame* pScene);
    void HandleLeftClick(CScene_InGame* pScene);
    void HandleRightClick(CScene_InGame* pScene);
    void HandleDrag(CScene_InGame* pScene);
    void HandleUndoRedo(CScene_InGame* pScene);

    Vec3 SnapIfNeeded(const Vec3& v) const;

private:
    CEditorPicker m_picker;

    bool    m_bEditMode      = false;
    bool    m_bDragging      = false;
    bool    m_bSnap          = false;
    f32_t   m_snapSize       = 1.f;
    f32_t   m_groundPlaneY   = 3.f;       // Scene_InGame 의 y=3 평면

    EPaletteType m_palette   = EPaletteType::Turret_Blue;
    i32_t   m_hoveredIdx     = -1;         // 이번 프레임 마우스 위 오브젝트
    Vec3    m_hoverPoint     = {};
    Vec3    m_hoverNormal    = {};
    bool    m_bHoverGround   = false;
    Vec3    m_groundHitPoint = {};

    // 드래그 상태
    i32_t   m_dragObjectIdx  = -1;
    Vec3    m_dragStartWorld = {};
    Vec3    m_dragStartObjPos = {};

    // Undo / Redo
    std::stack<UndoRecord> m_undoStack;
    std::stack<UndoRecord> m_redoStack;

    // 이전 프레임 버튼 (edge 감지 — GetAsyncKeyState 사용)
    bool    m_bLPrev = false;
    bool    m_bRPrev = false;
    bool    m_bZPrev = false;   // Ctrl+Z
    bool    m_bYPrev = false;   // Ctrl+Y
    bool    m_bEPrev = false;   // Edit 모드 토글
    bool    m_bGPrev = false;   // Snap 토글
    bool    m_bDelPrev = false; // Delete 키
};
```

## 4.2 `Client/Private/Editor/EditorPlacer.cpp` (전문)

```cpp
#include "Editor/EditorPlacer.h"
#include "Scene/Scene_InGame.h"
#include "CCamera.h"
#include <algorithm>

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

static bool IsKeyHeld(i32_t vk)   { return (GetAsyncKeyState(vk) & 0x8000) != 0; }
static bool IsCtrlHeld()          { return (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0; }

//──────────────────────────────────────────────────────────────
// Update
//──────────────────────────────────────────────────────────────
void CEditorPlacer::Update(CScene_InGame* pScene, CCamera* pCamera, HWND hWnd,
                            u32_t viewportW, u32_t viewportH, f32_t /*dt*/)
{
    if (!pScene || !pCamera) return;

    m_picker.UpdateContext(pCamera, hWnd, viewportW, viewportH);

    // Edit 모드 토글 (E 키, edge)
    const bool eHeld = IsKeyHeld('E');
    if (eHeld && !m_bEPrev) m_bEditMode = !m_bEditMode;
    m_bEPrev = eHeld;

    // Snap 토글 (G 키, edge)
    const bool gHeld = IsKeyHeld('G');
    if (gHeld && !m_bGPrev) m_bSnap = !m_bSnap;
    m_bGPrev = gHeld;

    // Undo/Redo (Ctrl+Z / Ctrl+Y)
    if (m_bEditMode && IsCtrlHeld())
    {
        const bool zHeld = IsKeyHeld('Z');
        if (zHeld && !m_bZPrev)
        {
            if (!m_undoStack.empty())
            {
                UndoRecord rec = m_undoStack.top();
                m_undoStack.pop();
                // Undo 적용
                switch (rec.kind)
                {
                case UndoRecord::Kind::Spawn:
                    pScene->RemoveObjectAt(rec.objectIdx);
                    break;
                case UndoRecord::Kind::Remove:
                    pScene->RespawnObjectAt(rec.objectIdx, rec.type, rec.before);
                    break;
                case UndoRecord::Kind::Move:
                    pScene->SetObjectPosition(rec.objectIdx, rec.before);
                    break;
                }
                m_redoStack.push(rec);
            }
        }
        m_bZPrev = zHeld;

        const bool yHeld = IsKeyHeld('Y');
        if (yHeld && !m_bYPrev)
        {
            if (!m_redoStack.empty())
            {
                UndoRecord rec = m_redoStack.top();
                m_redoStack.pop();
                switch (rec.kind)
                {
                case UndoRecord::Kind::Spawn:
                    pScene->RespawnObjectAt(rec.objectIdx, rec.type, rec.after);
                    break;
                case UndoRecord::Kind::Remove:
                    pScene->RemoveObjectAt(rec.objectIdx);
                    break;
                case UndoRecord::Kind::Move:
                    pScene->SetObjectPosition(rec.objectIdx, rec.after);
                    break;
                }
                m_undoStack.push(rec);
            }
        }
        m_bYPrev = yHeld;
    }
    else
    {
        m_bZPrev = IsKeyHeld('Z');
        m_bYPrev = IsKeyHeld('Y');
    }

    if (!m_bEditMode) { m_hoveredIdx = -1; m_bDragging = false; return; }

    // ImGui 가 마우스 캡처 중이면 피킹 생략 (패널 위 클릭)
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) {
        m_bLPrev = false;
        m_bRPrev = false;
        return;
    }

    if (!m_picker.IsMouseInClient()) {
        m_hoveredIdx = -1;
        return;
    }

    HandleHover(pScene);

    const bool lHeld = IsKeyHeld(VK_LBUTTON);
    const bool rHeld = IsKeyHeld(VK_RBUTTON);

    // Left button edge → 클릭
    if (lHeld && !m_bLPrev)
    {
        HandleLeftClick(pScene);
    }
    // Left button hold → 드래그 (선택된 오브젝트 이동)
    else if (lHeld && m_bLPrev && m_bDragging)
    {
        HandleDrag(pScene);
    }
    // Left button release → 드래그 종료
    else if (!lHeld && m_bLPrev && m_bDragging)
    {
        // 이동 확정 — Undo 기록
        if (m_dragObjectIdx >= 0)
        {
            Vec3 finalPos = pScene->GetObjectPosition(m_dragObjectIdx);
            if (finalPos.x != m_dragStartObjPos.x ||
                finalPos.y != m_dragStartObjPos.y ||
                finalPos.z != m_dragStartObjPos.z)
            {
                UndoRecord rec;
                rec.kind      = UndoRecord::Kind::Move;
                rec.objectIdx = m_dragObjectIdx;
                rec.before    = m_dragStartObjPos;
                rec.after     = finalPos;
                m_undoStack.push(rec);
                while (!m_redoStack.empty()) m_redoStack.pop();
            }
        }
        m_bDragging     = false;
        m_dragObjectIdx = -1;
    }

    // Right button edge → 삭제
    if (rHeld && !m_bRPrev) HandleRightClick(pScene);

    // Delete 키
    const bool delHeld = IsKeyHeld(VK_DELETE);
    if (delHeld && !m_bDelPrev)
    {
        const i32_t sel = pScene->GetSelectedIndex();
        if (sel >= 0)
        {
            UndoRecord rec;
            rec.kind      = UndoRecord::Kind::Remove;
            rec.objectIdx = sel;
            rec.type      = pScene->GetObjectType(sel);
            rec.before    = pScene->GetObjectPosition(sel);
            m_undoStack.push(rec);
            while (!m_redoStack.empty()) m_redoStack.pop();

            pScene->RemoveObjectAt(sel);
            pScene->SetSelectedIndex(-1);
        }
    }
    m_bDelPrev = delHeld;

    m_bLPrev = lHeld;
    m_bRPrev = rHeld;
}

//──────────────────────────────────────────────────────────────
// HandleHover
//──────────────────────────────────────────────────────────────
void CEditorPlacer::HandleHover(CScene_InGame* pScene)
{
    PickRay ray = m_picker.BuildRayFromMouse();

    // 오브젝트 Pick
    m_hoveredIdx = -1;
    f32_t bestT = FLT_MAX;
    Vec3  bestNormal = {};
    Vec3  bestPoint  = {};

    const u32_t count = pScene->GetObjectCount();
    for (u32_t i = 0; i < count; ++i)
    {
        if (!pScene->IsObjectVisible(i)) continue;

        Vec3 minW, maxW;
        if (!pScene->TryGetObjectWorldAABB(i, minW, maxW)) continue;

        f32_t t; Vec3 n;
        if (CEditorPicker::RayAABB_Slab(ray, minW, maxW, t, n))
        {
            if (t < bestT)
            {
                bestT      = t;
                bestNormal = n;
                bestPoint  = { ray.origin.x + ray.direction.x * t,
                               ray.origin.y + ray.direction.y * t,
                               ray.origin.z + ray.direction.z * t };
                m_hoveredIdx = (i32_t)i;
            }
        }
    }

    if (m_hoveredIdx >= 0)
    {
        m_hoverPoint   = bestPoint;
        m_hoverNormal  = bestNormal;
        m_bHoverGround = false;
    }
    else
    {
        // Ground 평면 시도
        m_bHoverGround = CEditorPicker::RayGroundPlane(ray, m_groundPlaneY, m_groundHitPoint);
    }
}

//──────────────────────────────────────────────────────────────
// HandleLeftClick
//──────────────────────────────────────────────────────────────
void CEditorPlacer::HandleLeftClick(CScene_InGame* pScene)
{
    if (m_hoveredIdx >= 0)
    {
        // 기존 오브젝트 → 선택 + 드래그 시작
        pScene->SetSelectedIndex(m_hoveredIdx);
        m_bDragging        = true;
        m_dragObjectIdx    = m_hoveredIdx;
        m_dragStartWorld   = m_hoverPoint;
        m_dragStartObjPos  = pScene->GetObjectPosition(m_hoveredIdx);
        return;
    }

    // Ground 클릭 → 팔레트 타입 스폰
    if (!m_bHoverGround || m_palette == EPaletteType::None) return;

    Vec3 spawnPos = SnapIfNeeded(m_groundHitPoint);
    spawnPos.y    = m_groundPlaneY;   // 강제 고정

    i32_t newIdx = pScene->SpawnObject(m_palette, spawnPos);
    if (newIdx >= 0)
    {
        pScene->SetSelectedIndex(newIdx);

        UndoRecord rec;
        rec.kind      = UndoRecord::Kind::Spawn;
        rec.objectIdx = newIdx;
        rec.type      = m_palette;
        rec.before    = {};
        rec.after     = spawnPos;
        m_undoStack.push(rec);
        while (!m_redoStack.empty()) m_redoStack.pop();
    }
}

//──────────────────────────────────────────────────────────────
// HandleRightClick
//──────────────────────────────────────────────────────────────
void CEditorPlacer::HandleRightClick(CScene_InGame* pScene)
{
    if (m_hoveredIdx < 0) return;

    UndoRecord rec;
    rec.kind      = UndoRecord::Kind::Remove;
    rec.objectIdx = m_hoveredIdx;
    rec.type      = pScene->GetObjectType(m_hoveredIdx);
    rec.before    = pScene->GetObjectPosition(m_hoveredIdx);
    m_undoStack.push(rec);
    while (!m_redoStack.empty()) m_redoStack.pop();

    pScene->RemoveObjectAt(m_hoveredIdx);
    if (pScene->GetSelectedIndex() == m_hoveredIdx)
        pScene->SetSelectedIndex(-1);

    m_hoveredIdx = -1;
}

//──────────────────────────────────────────────────────────────
// HandleDrag
//──────────────────────────────────────────────────────────────
void CEditorPlacer::HandleDrag(CScene_InGame* pScene)
{
    if (m_dragObjectIdx < 0) return;

    PickRay ray = m_picker.BuildRayFromMouse();

    Vec3 currentHit;
    if (!CEditorPicker::RayGroundPlane(ray, m_groundPlaneY, currentHit)) return;

    Vec3 delta = {
        currentHit.x - m_dragStartWorld.x,
        0.f,
        currentHit.z - m_dragStartWorld.z
    };

    Vec3 newPos = {
        m_dragStartObjPos.x + delta.x,
        m_dragStartObjPos.y,
        m_dragStartObjPos.z + delta.z
    };

    newPos = SnapIfNeeded(newPos);
    pScene->SetObjectPosition(m_dragObjectIdx, newPos);
}

//──────────────────────────────────────────────────────────────
// HandleUndoRedo / SnapIfNeeded
//──────────────────────────────────────────────────────────────
void CEditorPlacer::HandleUndoRedo(CScene_InGame* /*pScene*/)
{
    // Update() 내에서 처리됨 (편의용 placeholder)
}

Vec3 CEditorPlacer::SnapIfNeeded(const Vec3& v) const
{
    if (!m_bSnap) return v;
    return Vec3{
        std::round(v.x / m_snapSize) * m_snapSize,
        v.y,
        std::round(v.z / m_snapSize) * m_snapSize
    };
}

//──────────────────────────────────────────────────────────────
// DrawPaletteWindow  (Scene_InGame::OnImGui 에서 호출)
//──────────────────────────────────────────────────────────────
void CEditorPlacer::DrawPaletteWindow()
{
    ImGui::SetNextWindowPos(ImVec2(260, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(260, 360), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Editor Palette"))
    {
        ImGui::Checkbox("Edit Mode (E)", &m_bEditMode);
        ImGui::SameLine();
        ImGui::TextDisabled("LMB:Place/Drag  RMB:Delete");

        ImGui::Checkbox("Snap (G)", &m_bSnap);
        if (m_bSnap)
        {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.f);
            ImGui::DragFloat("##SnapSize", &m_snapSize, 0.1f, 0.1f, 10.f, "%.2f");
        }

        ImGui::DragFloat("Ground Y", &m_groundPlaneY, 0.1f, -50.f, 50.f);
        ImGui::Separator();

        // 팔레트 트리
        struct Entry { EPaletteType type; const char* label; };
        static const Entry s_structures[] = {
            { EPaletteType::Turret_Blue,    "Turret (Blue)"    },
            { EPaletteType::Turret_Red,     "Turret (Red)"     },
            { EPaletteType::Inhibitor_Blue, "Inhibitor (Blue)" },
            { EPaletteType::Inhibitor_Red,  "Inhibitor (Red)"  },
            { EPaletteType::Nexus_Blue,     "Nexus (Blue)"     },
            { EPaletteType::Nexus_Red,      "Nexus (Red)"      },
        };
        static const Entry s_jungle[] = {
            { EPaletteType::Jungle_Baron,   "Baron"  },
            { EPaletteType::Jungle_Dragon,  "Dragon" },
            { EPaletteType::Jungle_Blue,    "Blue"   },
            { EPaletteType::Jungle_Krug,    "Krug"   },
            { EPaletteType::Jungle_Gromp,   "Gromp"  },
            { EPaletteType::Jungle_Wolf,    "Wolf"   },
        };
        static const Entry s_minions[] = {
            { EPaletteType::Minion_OrderMelee,  "Order Melee"  },
            { EPaletteType::Minion_OrderRanged, "Order Ranged" },
            { EPaletteType::Minion_OrderSiege,  "Order Siege"  },
            { EPaletteType::Minion_OrderSuper,  "Order Super"  },
            { EPaletteType::Minion_ChaosMelee,  "Chaos Melee"  },
            { EPaletteType::Minion_ChaosRanged, "Chaos Ranged" },
            { EPaletteType::Minion_ChaosSiege,  "Chaos Siege"  },
            { EPaletteType::Minion_ChaosSuper,  "Chaos Super"  },
        };

        auto drawGroup = [this](const char* title, const Entry* arr, size_t n) {
            if (ImGui::TreeNodeEx(title, ImGuiTreeNodeFlags_DefaultOpen))
            {
                for (size_t i = 0; i < n; ++i)
                {
                    bool selected = (m_palette == arr[i].type);
                    if (ImGui::Selectable(arr[i].label, selected))
                        m_palette = arr[i].type;
                }
                ImGui::TreePop();
            }
        };

        drawGroup("Structures", s_structures, std::size(s_structures));
        drawGroup("Jungle",     s_jungle,     std::size(s_jungle));
        drawGroup("Minions",    s_minions,    std::size(s_minions));

        ImGui::Separator();
        ImGui::Text("Hover Idx : %d", m_hoveredIdx);
        ImGui::Text("Dragging  : %s", m_bDragging ? "yes" : "no");
        ImGui::Text("Undo stack: %zu", m_undoStack.size());
    }
    ImGui::End();
}
```

---

# 5. Scene_InGame 수정

## 5.1 `Client/Public/Scene/Scene_InGame.h` 수정

**Before** (L41-L65):
```cpp
    // Map Objects
    struct MapObject
    {
        ModelRenderer renderer;
        CTransform transform;
        const char* name = nullptr;
        const char* category = nullptr;
        bool bVisible = true;
    };
    static constexpr u32_t MAP_OBJECT_COUNT = 44;
    MapObject m_MapObjects[MAP_OBJECT_COUNT];

    // Editor State
    i32_t m_iSelectedObject = -1;
    bool  m_bShowEditor = true;

    void SaveObjectLayout(const char* path);
    void LoadObjectLayout(const char* path);
};
```

**After** (타입 enum 도입 + Placer 통합 + API 공개):
```cpp
#include "Editor/EditorPlacer.h"   // 상단 include 에 추가

class CScene_InGame : public IScene
{
public:
    CScene_InGame() = default;
    ~CScene_InGame() override = default;

    bool OnEnter()              override;
    void OnExit()               override;
    void OnUpdate(f32_t dt)     override;
    void OnLateUpdate(f32_t dt) override;
    void OnRender()             override;
    void OnImGui()              override;

    // ── Editor API (Placer 가 호출) ─────────────────────────
    u32_t        GetObjectCount() const { return MAP_OBJECT_COUNT; }
    bool         IsObjectVisible(u32_t idx) const;
    bool         TryGetObjectWorldAABB(u32_t idx, Vec3& outMin, Vec3& outMax);
    Vec3         GetObjectPosition(u32_t idx) const;
    EPaletteType GetObjectType(u32_t idx) const;
    i32_t        GetSelectedIndex() const { return m_iSelectedObject; }
    void         SetSelectedIndex(i32_t idx) { m_iSelectedObject = idx; }
    void         SetObjectPosition(i32_t idx, const Vec3& pos);
    i32_t        SpawnObject(EPaletteType type, const Vec3& pos);
    void         RemoveObjectAt(i32_t idx);
    void         RespawnObjectAt(i32_t idx, EPaletteType type, const Vec3& pos);

private:
    // Cube
    CubeRenderer    m_Cube;
    CTransform      m_CubeTransform;
    f32_t           m_fElapsed = 0.f;

    // Camera
    unique_ptr<CDynamicCamera> m_pCamera;

    // Champions (5)
    ModelRenderer   m_Irelia;   CTransform m_IreliaTransform;
    ModelRenderer   m_Yasuo;    CTransform m_YasuoTransform;
    ModelRenderer   m_Sylas;    CTransform m_SylasTransform;
    ModelRenderer   m_Viego;    CTransform m_ViegoTransform;
    ModelRenderer   m_Kalista;  CTransform m_KalistaTransform;

    // Map
    ModelRenderer   m_Map;
    CTransform      m_MapTransform;

    // Map Objects
    struct MapObject
    {
        ModelRenderer renderer;
        CTransform    transform;
        const char*   name     = nullptr;
        const char*   category = nullptr;
        EPaletteType  type     = EPaletteType::None;
        bool          bVisible = true;
        bool          bActive  = true;     // 삭제되면 false (재활용 대기)
    };

    // LoL 맵 완전 배치 — 고정 크기 배열, 확장 필요 시 slot 재활용
    static constexpr u32_t MAP_OBJECT_COUNT = 64;   // 44 → 64 (여유 20슬롯)
    MapObject m_MapObjects[MAP_OBJECT_COUNT];
    u32_t     m_ActiveObjectCount = 0;

    // Editor State
    i32_t m_iSelectedObject = -1;
    bool  m_bShowEditor     = true;

    CEditorPlacer m_Placer;

    // 내부 헬퍼
    bool InitObjectSlot(u32_t idx, EPaletteType type, const Vec3& pos,
                        const char* name, const char* category,
                        const char* glbPath, f32_t scale);
    const char* GetGlbPathForType(EPaletteType type) const;
    const char* GetCategoryForType(EPaletteType type) const;
    f32_t       GetDefaultScaleForType(EPaletteType type) const;

    void SaveObjectLayout(const char* path);
    void LoadObjectLayout(const char* path);
};
```

## 5.2 `Client/Private/Scene/Scene_InGame.cpp` 수정 항목

현재 477줄 전문 재작성은 과도. **변경 지점만 명시**:

### 5.2.1 OnEnter 내부 defs[] 이름 → EPaletteType 매핑

defs[] 배열 정의는 유지하되 각 entry 에 `EPaletteType` 추가. 배열 크기는 44 유지, 나머지 20 슬롯은
초기 `bActive=false`.

**Before** (L77-L122, 기존 `ObjectDef` 배열):
```cpp
    struct ObjectDef {
        const char* glbPath;
        const char* name;
        const char* category;
        f32_t x, y, z;
        f32_t scale;
    };
```

**After**:
```cpp
    struct ObjectDef {
        const char*  glbPath;
        const char*  name;
        const char*  category;
        EPaletteType type;
        f32_t        x, y, z;
        f32_t        scale;
    };
```

각 defs[] 엔트리에 `EPaletteType` 필드 채우기 (예):
```cpp
{PATH_NEXUS_BLUE,  "Nexus_Blue",  "Structure", EPaletteType::Nexus_Blue,  -60.f, 3.f, -60.f, 0.01f},
{PATH_INHIB_BLUE,  "Inhib_Blue_Top", "Structure", EPaletteType::Inhibitor_Blue, -60.f, 3.f, -35.f, 0.01f},
// ... (나머지 42개 entry 동일 패턴)
```

그리고 초기화 루프:
```cpp
for (u32_t i = 0; i < 44; ++i)
{
    m_MapObjects[i].renderer.Init(defs[i].glbPath, L"Shaders/Mesh3D.hlsl");
    m_MapObjects[i].transform.SetPosition(defs[i].x, defs[i].y, defs[i].z);
    m_MapObjects[i].transform.SetScale(defs[i].scale);
    m_MapObjects[i].name     = defs[i].name;
    m_MapObjects[i].category = defs[i].category;
    m_MapObjects[i].type     = defs[i].type;
    m_MapObjects[i].bActive  = true;
    m_MapObjects[i].bVisible = true;
}
for (u32_t i = 44; i < MAP_OBJECT_COUNT; ++i)
{
    m_MapObjects[i].bActive  = false;    // 예비 슬롯
    m_MapObjects[i].bVisible = false;
}
m_ActiveObjectCount = 44;
```

### 5.2.2 OnUpdate — 피커 Update + 애니 루프 bActive 체크

```cpp
void CScene_InGame::OnUpdate(f32_t dt)
{
    m_fElapsed += dt;
    m_CubeTransform.SetRotationX(m_fElapsed * 0.8f);

    // 맵 오브젝트 애니메이션 (bActive 만)
    for (u32_t i = 0; i < MAP_OBJECT_COUNT; ++i)
    {
        if (!m_MapObjects[i].bActive) continue;
        m_MapObjects[i].renderer.Update(dt);
    }

    // 챔피언 애니메이션
    m_Irelia.Update(dt);
    m_Yasuo.Update(dt);
    m_Sylas.Update(dt);
    m_Viego.Update(dt);
    m_Kalista.Update(dt);

    // (기존 키보드 애니메이션 전환 블록 그대로)
    // ...

    // ── 에디터 피커 업데이트 ────────────────────────────────
    // 카메라가 Edit 모드에서 잠기도록 — 선택사항
    u32_t viewportW = 1280;   // TODO: 엔진 viewport 가져오기
    u32_t viewportH = 720;
    HWND hWnd = CInput::Get().GetWindowHandle();

    m_Placer.Update(this, m_pCamera.get(), hWnd, viewportW, viewportH, dt);
}
```

### 5.2.3 OnRender — bActive 체크 추가

```cpp
for (u32_t i = 0; i < MAP_OBJECT_COUNT; ++i)
{
    if (!m_MapObjects[i].bActive || !m_MapObjects[i].bVisible) continue;

    m_MapObjects[i].renderer.UpdateCamera(vp);
    m_MapObjects[i].renderer.UpdateTransform(m_MapObjects[i].transform.GetWorldMatrix());
    m_MapObjects[i].renderer.Render();
}
```

### 5.2.4 OnImGui — 팔레트 창 + Hierarchy bActive 필터

```cpp
void CScene_InGame::OnImGui()
{
    auto& input = CInput::Get();
    if (input.IsKeyDown(VK_F1))
        m_bShowEditor = !m_bShowEditor;
    if (!m_bShowEditor) return;

    // ── 에디터 팔레트 창 ─────────────────────────────
    m_Placer.DrawPaletteWindow();

    // MenuBar / Hierarchy / Inspector / Camera
    // ... (기존 동일, Hierarchy 루프만 bActive 체크 추가)

    // Hierarchy Panel 내부:
    for (u32_t i = 0; i < MAP_OBJECT_COUNT; ++i)
    {
        if (!m_MapObjects[i].bActive) continue;
        if (strcmp(m_MapObjects[i].category, cat) != 0) continue;
        // ... 기존 TreeNodeEx 렌더
    }
}
```

### 5.2.5 API 구현 (신규 함수)

`Scene_InGame.cpp` 파일 맨 아래에 추가:

```cpp
//──────────────────────────────────────────────────────────
// Editor API 구현
//──────────────────────────────────────────────────────────

bool CScene_InGame::IsObjectVisible(u32_t idx) const
{
    if (idx >= MAP_OBJECT_COUNT) return false;
    return m_MapObjects[idx].bActive && m_MapObjects[idx].bVisible;
}

bool CScene_InGame::TryGetObjectWorldAABB(u32_t idx, Vec3& outMin, Vec3& outMax)
{
    if (idx >= MAP_OBJECT_COUNT) return false;
    if (!m_MapObjects[idx].bActive) return false;

    MapObject& obj = m_MapObjects[idx];
    if (!obj.renderer.HasValidAABB()) {
        // Fallback — transform 기준 유닛 박스
        Vec3 pos = obj.transform.GetPosition();
        Vec3 s   = obj.transform.GetScale();
        f32_t r  = 1.f * std::max({ s.x, s.y, s.z });
        outMin = { pos.x - r, pos.y - r, pos.z - r };
        outMax = { pos.x + r, pos.y + r, pos.z + r };
        return true;
    }

    CEditorPicker::ComputeWorldAABB(obj.renderer, obj.transform, outMin, outMax);
    return true;
}

Vec3 CScene_InGame::GetObjectPosition(u32_t idx) const
{
    if (idx >= MAP_OBJECT_COUNT) return {};
    return m_MapObjects[idx].transform.GetPosition();
}

EPaletteType CScene_InGame::GetObjectType(u32_t idx) const
{
    if (idx >= MAP_OBJECT_COUNT) return EPaletteType::None;
    return m_MapObjects[idx].type;
}

void CScene_InGame::SetObjectPosition(i32_t idx, const Vec3& pos)
{
    if (idx < 0 || (u32_t)idx >= MAP_OBJECT_COUNT) return;
    m_MapObjects[idx].transform.SetPosition(pos);
}

i32_t CScene_InGame::SpawnObject(EPaletteType type, const Vec3& pos)
{
    // 비활성 슬롯 찾기
    for (u32_t i = 0; i < MAP_OBJECT_COUNT; ++i)
    {
        if (!m_MapObjects[i].bActive)
        {
            static char nameBuf[MAP_OBJECT_COUNT][64];   // 슬롯별 고유 이름 버퍼
            std::snprintf(nameBuf[i], sizeof(nameBuf[i]), "Spawned_%u", i);

            if (InitObjectSlot(i, type, pos,
                               nameBuf[i],
                               GetCategoryForType(type),
                               GetGlbPathForType(type),
                               GetDefaultScaleForType(type)))
            {
                ++m_ActiveObjectCount;
                return (i32_t)i;
            }
            return -1;
        }
    }
    return -1;   // 슬롯 가득
}

void CScene_InGame::RemoveObjectAt(i32_t idx)
{
    if (idx < 0 || (u32_t)idx >= MAP_OBJECT_COUNT) return;
    if (!m_MapObjects[idx].bActive) return;

    m_MapObjects[idx].renderer.Shutdown();
    m_MapObjects[idx].bActive  = false;
    m_MapObjects[idx].bVisible = false;
    m_MapObjects[idx].type     = EPaletteType::None;
    --m_ActiveObjectCount;
}

void CScene_InGame::RespawnObjectAt(i32_t idx, EPaletteType type, const Vec3& pos)
{
    if (idx < 0 || (u32_t)idx >= MAP_OBJECT_COUNT) return;
    if (m_MapObjects[idx].bActive) return;

    static char nameBuf[64];
    std::snprintf(nameBuf, sizeof(nameBuf), "Respawned_%d", idx);
    InitObjectSlot((u32_t)idx, type, pos,
                   nameBuf,
                   GetCategoryForType(type),
                   GetGlbPathForType(type),
                   GetDefaultScaleForType(type));
    ++m_ActiveObjectCount;
}

bool CScene_InGame::InitObjectSlot(u32_t idx, EPaletteType type, const Vec3& pos,
                                    const char* name, const char* category,
                                    const char* glbPath, f32_t scale)
{
    MapObject& o = m_MapObjects[idx];
    o.renderer.Shutdown();
    if (!o.renderer.Init(glbPath, L"Shaders/Mesh3D.hlsl")) return false;
    o.transform.SetPosition(pos);
    o.transform.SetScale(scale);
    o.transform.SetRotation({ 0.f, 0.f, 0.f });
    o.name     = name;
    o.category = category;
    o.type     = type;
    o.bActive  = true;
    o.bVisible = true;
    return true;
}

const char* CScene_InGame::GetGlbPathForType(EPaletteType type) const
{
    switch (type) {
    case EPaletteType::Turret_Blue:       return "C:/Users/user/Desktop/LOL_Resource/Object/Turret/turret_textured.glb";
    case EPaletteType::Turret_Red:        return "C:/Users/user/Desktop/LOL_Resource/Object/Turret/turret_red_textured.glb";
    case EPaletteType::Inhibitor_Blue:    return "C:/Users/user/Desktop/LOL_Resource/Object/Inhibitor/inhibitor_textured.glb";
    case EPaletteType::Inhibitor_Red:     return "C:/Users/user/Desktop/LOL_Resource/Object/Inhibitor/inhibitor_red_textured.glb";
    case EPaletteType::Nexus_Blue:        return "C:/Users/user/Desktop/LOL_Resource/Object/Nexus/nexus_textured.glb";
    case EPaletteType::Nexus_Red:         return "C:/Users/user/Desktop/LOL_Resource/Object/Nexus/nexus_red_textured.glb";
    case EPaletteType::Jungle_Baron:      return "C:/Users/user/Desktop/LOL_Resource/Object/Jungle/Baron/baron_textured.glb";
    case EPaletteType::Jungle_Dragon:     return "C:/Users/user/Desktop/LOL_Resource/Object/Jungle/Dragon/water/dragon_water_textured.glb";
    case EPaletteType::Jungle_Blue:       return "C:/Users/user/Desktop/LOL_Resource/Object/Jungle/Blue/blue_textured.glb";
    case EPaletteType::Jungle_Krug:       return "C:/Users/user/Desktop/LOL_Resource/Object/Jungle/Krug/krug_textured.glb";
    case EPaletteType::Jungle_Gromp:      return "C:/Users/user/Desktop/LOL_Resource/Object/Jungle/Gromp/gromp_textured.glb";
    case EPaletteType::Jungle_Wolf:       return "C:/Users/user/Desktop/LOL_Resource/Object/Jungle/Wolf/wolf_textured.glb";
    case EPaletteType::Minion_OrderMelee:  return "C:/Users/user/Desktop/LOL_Resource/Object/Minion_Order/Melee/order_melee_textured.glb";
    case EPaletteType::Minion_OrderRanged: return "C:/Users/user/Desktop/LOL_Resource/Object/Minion_Order/Ranged/order_ranged_textured.glb";
    case EPaletteType::Minion_OrderSiege:  return "C:/Users/user/Desktop/LOL_Resource/Object/Minion_Order/Siege/order_siege_textured.glb";
    case EPaletteType::Minion_OrderSuper:  return "C:/Users/user/Desktop/LOL_Resource/Object/Minion_Order/Super/order_super_textured.glb";
    case EPaletteType::Minion_ChaosMelee:  return "C:/Users/user/Desktop/LOL_Resource/Object/Minion_Chaos/Melee/chaos_melee_textured.glb";
    case EPaletteType::Minion_ChaosRanged: return "C:/Users/user/Desktop/LOL_Resource/Object/Minion_Chaos/Ranged/chaos_ranged_textured.glb";
    case EPaletteType::Minion_ChaosSiege:  return "C:/Users/user/Desktop/LOL_Resource/Object/Minion_Chaos/Siege/chaos_siege_textured.glb";
    case EPaletteType::Minion_ChaosSuper:  return "C:/Users/user/Desktop/LOL_Resource/Object/Minion_Chaos/Super/chaos_super_textured.glb";
    default: return nullptr;
    }
}

const char* CScene_InGame::GetCategoryForType(EPaletteType type) const
{
    u32_t t = (u32_t)type;
    if (t >= (u32_t)EPaletteType::Turret_Blue     && t <= (u32_t)EPaletteType::Nexus_Red)      return "Structure";
    if (t >= (u32_t)EPaletteType::Jungle_Baron    && t <= (u32_t)EPaletteType::Jungle_Wolf)    return "Jungle";
    if (t >= (u32_t)EPaletteType::Minion_OrderMelee && t <= (u32_t)EPaletteType::Minion_ChaosSuper) return "Minion";
    return "Unknown";
}

f32_t CScene_InGame::GetDefaultScaleForType(EPaletteType /*type*/) const
{
    return 0.01f;   // 전 오브젝트 동일 (LOL 맵 오브젝트 스케일)
}
```

---

# 6. vcxproj / filters 업데이트

## 6.1 `Client/Include/Client.vcxproj.filters` 추가

기존 filter 정의에 Editor 용 카테고리 추가:

```xml
<Filter Include="06. Editor">
  <UniqueIdentifier>{b3d4f5a8-9c2e-4f1b-a76d-8e3f2d1c5b4a}</UniqueIdentifier>
</Filter>
```

ClInclude / ClCompile 항목:

```xml
<ClInclude Include="..\Public\Editor\EditorPicker.h">
  <Filter>06. Editor</Filter>
</ClInclude>
<ClInclude Include="..\Public\Editor\EditorPlacer.h">
  <Filter>06. Editor</Filter>
</ClInclude>

<ClCompile Include="..\Private\Editor\EditorPicker.cpp">
  <Filter>06. Editor</Filter>
</ClCompile>
<ClCompile Include="..\Private\Editor\EditorPlacer.cpp">
  <Filter>06. Editor</Filter>
</ClCompile>
```

## 6.2 `Client/Include/Client.vcxproj` 추가

```xml
<ClCompile Include="..\Private\Editor\EditorPicker.cpp" />
<ClCompile Include="..\Private\Editor\EditorPlacer.cpp" />

<ClInclude Include="..\Public\Editor\EditorPicker.h" />
<ClInclude Include="..\Public\Editor\EditorPlacer.h" />
```

---

# 7. EngineSDK 동기화

`Engine/Public/Renderer/ModelRenderer.h` + `Engine/Public/Core/CInput.h` 변경 시:
1. `EngineSDK/inc/ModelRenderer.h` 로 복사
2. `EngineSDK/inc/CInput.h` 로 복사
3. `UpdateLib.bat` 실행 (자동화된 경우) 또는 수동 복사

---

# 8. Phase 분할 (권장 진행 순서)

| Phase | 범위 | 결과 |
|---|---|---|
| **1** | `ModelRenderer` AABB getter + `EditorPicker` + Scene_InGame 에 `TryGetObjectWorldAABB` + Hover 표시만 | 마우스 hover 시 ImGui 에 "Hover: #idx" 표시 |
| **2** | `EditorPlacer` 의 좌클릭 선택 + 우클릭 삭제 | 기존 44 오브젝트 피킹으로 선택/삭제 |
| **3** | 좌클릭 스폰 + Ground plane + 팔레트 UI | 빈 위치 클릭 → 새 오브젝트 스폰 |
| **4** | 드래그 이동 | 선택 오브젝트 XZ 드래그 |
| **5** | Undo/Redo + Delete 키 + Snap | 편집 편의 기능 |
| **6** | Preview 와이어프레임 (ImGui DrawList) | 스폰 위치 시각화 |
| **7** | (Phase C-2 이후) DebugDraw 교체 | 3D 와이어프레임 |

---

# 9. 검증 방법

1. **빌드**: Engine → Client Debug x64 성공
2. **Phase 1 검증**:
   - 실행 → F1 → `EditorPalette` 창 나타남
   - "Edit Mode" 체크 → 마우스 맵 오브젝트 위에 올리면 `Hover Idx: N` 변화
3. **Phase 2 검증**:
   - 기존 포탑 클릭 → Inspector 에 해당 오브젝트 정보
   - 우클릭 → 해당 오브젝트 사라짐 (bActive=false)
4. **Phase 3 검증**:
   - 빈 바닥 클릭 → 팔레트 선택 타입 스폰, Inspector 에 자동 선택
   - Hierarchy 에 새 항목 추가 확인
5. **Phase 4 검증**:
   - 오브젝트 드래그 → XZ 따라 이동, Y 고정 (m_groundPlaneY)
6. **Phase 5 검증**:
   - Ctrl+Z → 마지막 동작 Undo
   - Delete → 선택 오브젝트 삭제
   - G → Snap on/off, DragFloat SnapSize 조절 가능
7. **회귀**:
   - Scene_Loading → InGame 전환 정상
   - 기존 44 오브젝트 렌더/애니 정상
   - Save Layout / Load Layout 정상 (type 필드 저장 포맷 확장 필요 — Phase 5 에서 동반)

---

# 10. 주의사항 / 결정 포인트

1. **CInput 마우스 LButton 추가 vs GetAsyncKeyState**: 본 계획서는 후자 (간단). CInput 수정 경로
   (2.1~2.2) 는 장기 개선용으로만 문서화. 빌드 시 2.1/2.2 는 **건너뛰어도 됨**.
2. **Viewport 크기 하드코딩**: Scene_InGame 이 `viewportW/H = 1280/720` 고정. `CDX11Device` 에서
   가져오도록 확장 권장 (추후 개선).
3. **슬롯 증가 44 → 64**: 스폰으로 새 오브젝트 추가 여유. 필요 시 재조정.
4. **Fallback AABB**: `ModelRenderer::HasValidAABB()` false 경우 Scene 측에서 유닛 박스. FBX 로딩
   실패해도 피킹은 동작.
5. **Save / Load Layout**: 기존 `ObjectLayout.txt` 포맷에 `type` 컬럼 추가 필요. 본 계획서엔 미포함
   (Phase 5 동반 항목으로 추후 계획).
6. **회전**: 드래그는 XZ 이동만. 회전은 Inspector 의 `DragFloat3("Rotation")` 그대로 사용.
7. **지형 Y**: 초기 `m_groundPlaneY = 3.f`. 추후 Navigation 도입 시 실제 지형 높이 샘플러로 교체.
8. **ImGui `WantCaptureMouse`**: 이미 체크 추가됨. 팔레트 창 위 클릭은 피킹 무시.

---

## 작업 체크리스트 (수정 후 검토 요청 시 자가 점검)

- [ ] `ModelRenderer.h` getter 추가 — 빌드 OK
- [ ] `ModelRenderer.cpp` Impl AABB 저장 + getter 구현
- [ ] EngineSDK `ModelRenderer.h` 동기화
- [ ] `Client/Public/Editor/EditorPicker.h` 신규 작성
- [ ] `Client/Private/Editor/EditorPicker.cpp` 신규 작성
- [ ] `Client/Public/Editor/EditorPlacer.h` 신규 작성
- [ ] `Client/Private/Editor/EditorPlacer.cpp` 신규 작성
- [ ] `Scene_InGame.h` EPaletteType include + MapObject 확장 + API 선언
- [ ] `Scene_InGame.cpp` defs[] type 필드 추가 + 초기화 루프 확장 + API 구현
- [ ] `Scene_InGame.cpp` OnUpdate/OnRender/OnImGui bActive 체크 및 Placer 통합
- [ ] `Client.vcxproj` + `.filters` 신규 파일 등록 + `06. Editor` 필터 추가
- [ ] 빌드 성공
- [ ] 런타임: Edit Mode on → hover → click 선택 → RMB 삭제 → 빈 곳 click 스폰 → 드래그 이동 확인
