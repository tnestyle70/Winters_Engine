# Phase B-1 코드 검토 + 수정 계획

> **검토일**: 2026-04-14

---

## 발견된 문제 (7건)

### 🔴 P1: Engine_Struct.h — VTXMESH의 vTexCoord 타입 오류

**파일**: `Engine/Public/Engine_Struct.h` L13

```cpp
// 현재 (오류 — float3_t = XMFLOAT3, 3D 벡터)
float3_t vTexCoord; //texture coordinate

// 수정 (float2_t = XMFLOAT2, UV는 2D)
float2_t vTexCoord; // TEXCOORD0
```

이게 안 맞으면 VTXMESH 크기가 48B가 되어 DX11Pipeline의 오프셋(44B 기준)과 불일치 → GPU 크래시.

---

### 🔴 P2: DX11Pipeline.cpp — CreateMesh의 POSITION 포맷 오류

**파일**: `Engine/Private/RHI/DX11/DX11Pipeline.cpp` L81

```cpp
// 현재 (오류 — R32G32 = float2, 8바이트)
{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},

// 수정 (R32G32B32 = float3, 12바이트)
{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
```

Position은 float3(x,y,z) — R32G32B32_FLOAT가 맞음.

---

### 🔴 P3: CTexture.cpp — WICTextureLoader/DDSTextureLoader include 경로 오류

**파일**: `Engine/Private/Resource/CTexture.cpp` L5-6

```cpp
// 현재 (vcpkg DirectXTK 경로)
#include <WICTextureLoader.h>
#include <DDSTextureLoader.h>

// 수정 (DirectXTK는 하위 폴더에 있음)
#include <WICTextureLoader.h>      // vcpkg integrate install 했으면 이대로 동작해야 함
#include <DDSTextureLoader.h>
```

**오류가 뜨는 정확한 메시지**를 확인해야 합니다. vcpkg integrate install을 했으므로 `<WICTextureLoader.h>`로 찾아야 정상입니다.

가능한 원인:
1. Engine.vcxproj에 vcpkg triplet이 안 맞음 → `<VcpkgTriplet>x64-windows</VcpkgTriplet>` 추가 필요
2. DirectXTK 네임스페이스 문제 → `DirectX::CreateWICTextureFromFile` 명시 필요

**수정 (네임스페이스 명시)**:
```cpp
// L39, L41
hr = DirectX::CreateDDSTextureFromFile(pDevice, strFilePath.c_str(), nullptr, &pInstance->m_pSRV);
hr = DirectX::CreateWICTextureFromFile(pDevice, strFilePath.c_str(), nullptr, &pInstance->m_pSRV);
```

---

### 🔴 P4: ModelRenderer.cpp — using namespace Engine 제거 필요

**파일**: `Engine/Private/Renderer/ModelRenderer.cpp` L9

```cpp
// 현재
using namespace Engine;

// 수정: 삭제 (NS_BEGIN/NS_END로 이미 Engine 네임스페이스 안에 있어야 함)
```

하지만 ModelRenderer는 **Engine DLL 공개 API** (Include/ 폴더)라서 `namespace Engine`에 속하지 않음. `using namespace Engine;`이 필요한 이유는 CModel, CBPerFrame 등이 `namespace Engine` 안에 있기 때문.

**결론: `using namespace Engine;` 유지가 맞음.** ModelRenderer.h는 공개 API라 namespace 밖에 있고, .cpp에서 Engine 타입을 쓰려면 using이 필요.

단, **CMesh.cpp, CTexture.cpp, Model.cpp** 같은 Engine 내부 파일은 `NS_BEGIN(Engine)` ... `NS_END`로 감싸야 하는데, 현재는 감싸지 않고 있음.

---

### 🟡 P5: Model.cpp — using namespace Engine 주석 처리됨

**파일**: `Engine/Private/Resource/Model.cpp` L13

```cpp
// 현재 (주석 처리)
//using namespace Engine;

// 수정: 주석 해제 — NS_BEGIN 안 쓰면 using 필요
using namespace Engine;
```

Engine namespace 안의 타입(VTXMESH, CMesh, CTexture 등)을 사용하므로 필수.

---

### 🟡 P6: 파일명 불일치 — C접두사 누락

vcxproj 기준:
```
Mesh.h / Mesh.cpp     ← C접두사 없음
Model.h / Model.cpp   ← C접두사 없음
CTexture.h / CTexture.cpp  ← C접두사 있음
```

CLAUDE.md 컨벤션상 클래스명은 `CMesh`, `CModel`이지만 파일명이 `Mesh.h`, `Model.h`. 현재 빌드가 동작하면 나중에 일괄 변경해도 됨. **빌드 블로킹 아님**.

---

### 🟡 P7: Engine.vcxproj에 vcpkg triplet 미설정

vcpkg integrate install로 전역 통합했지만, x64-windows triplet이 명시적으로 필요할 수 있음.

Engine.vcxproj에 추가:
```xml
<PropertyGroup Label="Globals">
    ...
    <VcpkgTriplet>x64-windows</VcpkgTriplet>
</PropertyGroup>
```

---

## 수정 목록

| # | 파일 | 수정 | 심각도 |
|---|------|------|--------|
| 1 | `Engine_Struct.h` L13 | `float3_t vTexCoord` → `float2_t vTexCoord` | 🔴 |
| 2 | `DX11Pipeline.cpp` L81 | `R32G32_FLOAT` → `R32G32B32_FLOAT` | 🔴 |
| 3 | `CTexture.cpp` L39,41 | `DirectX::` 네임스페이스 명시 추가 | 🔴 |
| 4 | `Model.cpp` L13 | `using namespace Engine;` 주석 해제 | 🟡 |
| 5 | `Engine.vcxproj` | `<VcpkgTriplet>x64-windows</VcpkgTriplet>` 추가 | 🟡 |
