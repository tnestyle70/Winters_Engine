# Phase RH-0 Sub-plan: Foundation (★ Codex 2차 보정판)

**작성일**: 2026-04-30 (Codex 2차 검토 보정 2026-04-30)
**상위 문서**: `00_RHI_MIGRATION_MASTER.md` §2
**범위**: **Inventory + GameInstance Legacy rename + 9개 Public DX11 leak consumer 전수 TODO marker** (★ 2차 보정 — 이동 작업 X)
**합격**: LoL 빌드 통과 (deprecated warning 만), 9개 leak consumer 전수 marking, RH-1 진입 준비 완료

**한 줄**: **★ Codex P0-1, P0-2, P0-3 보정 — RH-0 은 "이동" 이 아닌 "inventory + Legacy rename + TODO marker" 만 수행. 실제 file move 는 RH-2 (Renderer/Resource 마이그 완료) 시점.**

---

## ★ Codex 2차 검토 변경 요약

| 변경 | 이전 RH-0 (1차) | 신규 RH-0 (2차) |
|---|---|---|
| 작업 1 | `Engine/Public/RHI/DX11/` → Private 이동 | **삭제** — RH-2 종료 시점에 일괄 이동 |
| 작업 2 | GameInstance 8개 getter `_Legacy` rename | 동일 (변경 없음) |
| 작업 3 | Scene_InGame.cpp:1898 1곳 만 마킹 | **확장** — 9개 Public DX11 leak consumer 전수 TODO marker |
| 신규 | — | **inventory 문서화 (작업 0)** |
| 명령 | bash (sed/grep) | PowerShell + `rg` 병기 |
| `vcxproj.filters` | 자동 GUID | 명시 XML |

---

## 0. 작업 0 — Public DX11 leak inventory 문서화 (1h, ★ 신규)

### 0.1 검증 명령 (PowerShell)

```powershell
# 메인 레포 루트에서 실행
Set-Location C:\Users\user\Desktop\Winters

Select-String -Path "Engine\Public\*","Client\Public\*","Client\Private\*" `
    -Pattern "RHI/DX11/|CDX11Device.h|d3d11.h" -List `
| Where-Object { $_.Path -notmatch "Bin\\|Intermediate\\" } `
| Select-Object Path
```

또는 ripgrep:
```bash
rg -l "RHI/DX11/|CDX11Device.h|d3d11.h" Engine/Public/ Client/Public/ Client/Private/ \
   --glob '!**/Bin/**' --glob '!**/Intermediate/**'
```

### 0.2 검증 결과 (2026-04-30 기준 실측)

**Public DX11 헤더 직접 consumer = 9개 + Scene_InGame 12 hit**:

```
Engine/Public/Engine_Defines.h
Engine/Public/Framework/CEngineApp.h
Engine/Public/Manager/UI/UI_Manager.h
Engine/Public/Renderer/PlaneRenderer.h
Engine/Public/Resource/Mesh.h
Engine/Public/RHI/CDX11Device.h           (★ 자체 — Public 위치)
Engine/Public/RHI/DX11/*.h                (★ 9개 — RH-0 에서 이동 대상이지만 보류)
Client/Public/GameObject/FX/FxBillboardComponent.h
Client/Public/GameObject/FX/FxMeshComponent.h
Client/Public/GameObject/FX/FxSystem.h
Client/Private/GameObject/FX/FxSystem.cpp
Client/Private/Scene/Scene_InGame.cpp     (12 hit)
```

### 0.3 합격
- ✅ 본 §0.2 결과를 `.md/plan/rhi/01_RHI_PHASE_0_FOUNDATION.md` 본 섹션에 박제
- ✅ RH-0 후속 작업 시 본 inventory 를 기준으로 진행

---

## 1. 작업 1 — `CGameInstance` 의 8개 DX11 getter Legacy bridge 분리 (4h)

### 1.1 현재 상태 (`Engine/Include/GameInstance.h:101-108`)

```cpp
// L22-25 forward decl
class CDX11Device;
class DX11Shader;
class DX11Pipeline;
class CBlendStateCache;

// L101-108 getters (모두 DX11 typename 직접 노출)
CDX11Device*      Get_RHIDevice();
DX11Shader*       Get_MeshShader();
DX11Pipeline*     Get_MeshPipeline();
CBlendStateCache* Get_BlendStateCache();
DX11Shader*       Get_FxSpriteShader();
DX11Pipeline*     Get_FxSpritePipeline();
DX11Shader*       Get_FxMeshShader();
DX11Pipeline*     Get_FxMeshPipeline();
```

**문제**:
- 이름 `Get_RHIDevice()` 가 RHI 추상화를 암시하지만 반환은 `CDX11Device*`
- DLL export 시 DX11 typename 이 SDK 헤더에 박힘 → Client 빌드가 DX11 의존

### 1.2 RH-0 패치 — `_Legacy` rename + `[[deprecated]]` 마커

**수정 후** (`Engine/Include/GameInstance.h`):

```cpp
// ─────────────────────────────────────────────────────────────
// Forward decl — DX11 구체 클래스 (★ Legacy bridge 한정)
//   RH-1 에서 IRHIDevice 인터페이스 추가 후, 신규 코드는 IRHI* 사용.
//   본 forward decl 은 _Legacy 멤버 함수 시그니처 한정.
// ─────────────────────────────────────────────────────────────
class CDX11Device;
class DX11Shader;
class DX11Pipeline;
class CBlendStateCache;

// ─────────────────────────────────────────────────────────────
// RHI Tier 2 Getters — RH-0 단계: Legacy 만. RH-1 부터 IRHI* 정식 추가.
//
// ★ Codex P0-4: RH-0 의 _Legacy 와 RH-1 의 신규 인터페이스 양립 위해
//   RH-1 단계에선 `Get_NewRHIDevice()` 를 신규 이름으로 추가.
//   RH-2 종료 시 모든 caller 마이그 후 `Get_RHIDevice()` 정식 rename.
// ─────────────────────────────────────────────────────────────

// ★ Legacy bridge — DX11 백엔드 직접 접근. RH-2 종료까지 점진 제거.
//   신규 코드는 절대 사용 금지. 기존 caller 만 한시적 사용.
[[deprecated("Use IRHIDevice (RH-1: Get_NewRHIDevice) — see .md/plan/rhi/")]]
CDX11Device*      Get_DX11Device_Legacy();

[[deprecated("Use IRHIShader (RH-1+)")]]
DX11Shader*       Get_MeshShader_Legacy();

[[deprecated("Use IRHIPipelineState (RH-3+)")]]
DX11Pipeline*     Get_MeshPipeline_Legacy();

[[deprecated("Use IRHIDevice / IRHIBindGroup (RH-3+)")]]
CBlendStateCache* Get_BlendStateCache_Legacy();

[[deprecated("Use IRHIShader (RH-1+)")]]
DX11Shader*       Get_FxSpriteShader_Legacy();

[[deprecated("Use IRHIPipelineState (RH-3+)")]]
DX11Pipeline*     Get_FxSpritePipeline_Legacy();

[[deprecated("Use IRHIShader (RH-1+)")]]
DX11Shader*       Get_FxMeshShader_Legacy();

[[deprecated("Use IRHIPipelineState (RH-3+)")]]
DX11Pipeline*     Get_FxMeshPipeline_Legacy();
```

### 1.3 `Engine/Private/GameInstance.cpp` 정의 rename

각 8개 getter 를 동일 본문 + `_Legacy` 접미사로 rename:

**수정 전** (예시):
```cpp
CDX11Device* CGameInstance::Get_RHIDevice()
{
    return m_pDX11Device.get();
}
```

**수정 후**:
```cpp
CDX11Device* CGameInstance::Get_DX11Device_Legacy()
{
    return m_pDX11Device.get();
}
```

같은 패턴으로 8개 모두 변환. 본문 동일.

### 1.4 합격
- ✅ Engine.dll 빌드 통과 (deprecated warning 8건 — 본인 정의 부)
- ✅ `EngineSDK/inc/GameInstance.h` 동기화 (Post-Build Event)

---

## 2. 작업 2 — 9개 Public DX11 leak consumer 전수 TODO marker (3h, ★ 확장)

★ **Codex P0-3 보정**: 기존 plan 의 "Scene_InGame 12 hit 만" 은 부족. 9개 consumer 전수 마킹.

### 2.1 마이그 대상 매트릭스 (★ §0.2 결과 기반)

| # | 파일 | 현재 상태 | RH-0 작업 |
|---|---|---|---|
| 1 | `Engine/Public/Engine_Defines.h` | DX11 매크로/타입 include 가능성 | (★ §2.2 분기) |
| 2 | `Engine/Public/Framework/CEngineApp.h` | DX11 헤더 직접 include | TODO marker + caller 무회귀 |
| 3 | `Engine/Public/Manager/UI/UI_Manager.h` | `ID3D11Device*` 매개변수 | TODO marker |
| 4 | `Engine/Public/Renderer/PlaneRenderer.h` | `Render(ID3D11DeviceContext*, ...)` | TODO marker |
| 5 | `Engine/Public/Resource/Mesh.h` | DX11 헤더 + 매개변수 | TODO marker |
| 6 | `Engine/Public/RHI/CDX11Device.h` | 자체가 Public 위치 + d3d11.h include | TODO marker (★ RH-2 종료 시 Private 이동) |
| 7 | `Client/Public/GameObject/FX/FxBillboardComponent.h` | DX11 직접 사용 | TODO marker |
| 8 | `Client/Public/GameObject/FX/FxMeshComponent.h` | DX11 직접 사용 | TODO marker |
| 9 | `Client/Public/GameObject/FX/FxSystem.h` | DX11 직접 사용 | TODO marker |
| 10 | `Client/Private/Scene/Scene_InGame.cpp` | 12 hit (8개 GameInstance getter + GetContext) | sed/PowerShell 치환 |

### 2.2 `Engine/Public/Engine_Defines.h` 분기 결정 (★ 확인 필요)

```powershell
# Engine_Defines.h 의 DX11 의존 실제 확인
Select-String -Path "Engine\Public\Engine_Defines.h" -Pattern "d3d11|D3D11|RHI/DX11"
```

3가지 시나리오:
- **(A)** Engine_Defines.h 가 DX11 헤더 단순 include 만 → 본 plan 범위에서 제거 (`#include "RHI/DX11/..."` 라인 삭제 + 의존 caller 의 직접 include 로 대체)
- **(B)** Engine_Defines.h 가 DX11 typedef 사용 → typedef 를 새 RHI 인터페이스 forward decl 로 교체 (RH-1)
- **(C)** Engine_Defines.h 가 DX11 macro 사용 → 현재 사용처 확인 후 별도 조치

→ RH-0 단계: **Engine_Defines.h 에 `// ★ RH-2 TODO — DX11 의존 제거` 주석만 추가**. 실제 제거는 RH-2.

### 2.3 9개 consumer 마킹 패턴

**수정 전 (예시 — `Engine/Public/Renderer/PlaneRenderer.h`)**:
```cpp
#pragma once
#include <d3d11.h>
#include "WintersMath.h"

class CPlaneRenderer
{
public:
    void Render(ID3D11DeviceContext* pCtx, const Mat4& world);
};
```

**수정 후**:
```cpp
#pragma once
// ★ RH-2 TODO — d3d11.h 제거. ID3D11DeviceContext* → IRHICommandList* 마이그 후
//   본 헤더의 d3d11.h include 제거. 참조: .md/plan/rhi/03_RHI_PHASE_2_COMMANDLIST.md
#include <d3d11.h>
#include "WintersMath.h"

class CPlaneRenderer
{
public:
    // ★ RH-2 TODO — IRHICommandList* 로 교체
    void Render(ID3D11DeviceContext* pCtx, const Mat4& world);
};
```

같은 패턴으로 9개 consumer 전수 적용.

### 2.4 Scene_InGame 12 hit 치환 (PowerShell)

```powershell
Set-Location C:\Users\user\Desktop\Winters\Client\Private\Scene

# 백업
Copy-Item Scene_InGame.cpp Scene_InGame.cpp.bak

# 8개 getter 동시 치환
$content = Get-Content Scene_InGame.cpp -Raw
$content = $content -replace 'Get_RHIDevice\(\)',          'Get_DX11Device_Legacy()'
$content = $content -replace 'Get_MeshShader\(\)',         'Get_MeshShader_Legacy()'
$content = $content -replace 'Get_MeshPipeline\(\)',       'Get_MeshPipeline_Legacy()'
$content = $content -replace 'Get_BlendStateCache\(\)',    'Get_BlendStateCache_Legacy()'
$content = $content -replace 'Get_FxSpriteShader\(\)',     'Get_FxSpriteShader_Legacy()'
$content = $content -replace 'Get_FxSpritePipeline\(\)',   'Get_FxSpritePipeline_Legacy()'
$content = $content -replace 'Get_FxMeshShader\(\)',       'Get_FxMeshShader_Legacy()'
$content = $content -replace 'Get_FxMeshPipeline\(\)',     'Get_FxMeshPipeline_Legacy()'

Set-Content Scene_InGame.cpp -Value $content -NoNewline

# 검증 (0 hit 이어야 함)
Select-String Scene_InGame.cpp -Pattern '\bGet_(RHIDevice|MeshShader|MeshPipeline|BlendStateCache|FxSpriteShader|FxSpritePipeline|FxMeshShader|FxMeshPipeline)\(\)'
```

**Scene_InGame.cpp:1898** 의 `GetContext()` 직접 호출 추가 마킹:

```cpp
// ★ RH-2 TODO — IRHICommandList 추상화 후 Get_FrameCommandList() 로 교체.
//   현재는 DX11 immediate context 직접 사용 (Legacy bridge 한정).
//   참조: .md/plan/rhi/03_RHI_PHASE_2_COMMANDLIST.md
auto* pCtx = CGameInstance::Get()->Get_DX11Device_Legacy()->GetContext();
m_pAttackRangePlane->Render(pCtx, vp);
```

### 2.5 합격
- ✅ 9개 Public DX11 leak consumer 전수 `// ★ RH-2 TODO` 주석 박힘
- ✅ Scene_InGame.cpp 12 hit `_Legacy` 호출로 변경 + 1898 줄 추가 마킹
- ✅ `rg "GetContext\(\)|GetDevice\(\)" Engine/Public/ Client/` 결과 모두 RH-2 TODO 주석 동반

---

## 3. 작업 3 — RH-2 종료 시점에 수행할 file move 사전 준비 (1h, ★ 신규)

★ **Codex P0-1, P0-2 보정**: file move 자체는 RH-2 종료 후. RH-0 은 "사전 준비" 만.

### 3.1 RH-2 종료 시 이동 대상 (RH-0 에서는 이동 X)

**RH-2 종료 시점에 수행할 작업** — 본 plan 에는 박제만, 실제 실행은 RH-2:

```powershell
# ★ RH-2 종료 시점에만 실행 — RH-0 에서는 절대 실행 X
Set-Location C:\Users\user\Desktop\Winters

# Public RHI/DX11 폴더 통째로 Private 이동
git mv Engine/Public/RHI/DX11/BlendStateCache.h    Engine/Private/RHI/DX11/
git mv Engine/Public/RHI/DX11/DX11Buffer.h         Engine/Private/RHI/DX11/
git mv Engine/Public/RHI/DX11/DX11ConstantBuffer.h Engine/Private/RHI/DX11/
git mv Engine/Public/RHI/DX11/DX11IndexBuffer.h    Engine/Private/RHI/DX11/
git mv Engine/Public/RHI/DX11/DX11Pipeline.h       Engine/Private/RHI/DX11/
git mv Engine/Public/RHI/DX11/DX11Shader.h         Engine/Private/RHI/DX11/
git mv Engine/Public/RHI/DX11/DX11StructuredBuffer.h Engine/Private/RHI/DX11/
git mv Engine/Public/RHI/DX11/DX11VertexBuffer.h   Engine/Private/RHI/DX11/
git mv Engine/Public/RHI/DX11/SamplerStateCache.h  Engine/Private/RHI/DX11/

# CDX11Device.h 도 Private 이동 (★ Codex P0-2)
git mv Engine/Public/RHI/CDX11Device.h Engine/Private/RHI/DX11/DX11Device.h

# 빈 폴더 제거 (git clean — Remove-Item -Force 금지)
git clean -fd Engine/Public/RHI/DX11
```

### 3.2 RH-2 종료 시 `Engine/Include/Engine.vcxproj` XML diff (★ Codex P2-25)

**수정 전** (RH-0 시점 그대로 — 변경 없음):
```xml
<ClInclude Include="..\Public\RHI\DX11\BlendStateCache.h" />
<ClInclude Include="..\Public\RHI\DX11\DX11Buffer.h" />
<!-- 9개 -->
<ClInclude Include="..\Public\RHI\CDX11Device.h" />
```

**RH-2 종료 시 수정 후**:
```xml
<ClInclude Include="..\Private\RHI\DX11\BlendStateCache.h" />
<ClInclude Include="..\Private\RHI\DX11\DX11Buffer.h" />
<ClInclude Include="..\Private\RHI\DX11\DX11ConstantBuffer.h" />
<ClInclude Include="..\Private\RHI\DX11\DX11IndexBuffer.h" />
<ClInclude Include="..\Private\RHI\DX11\DX11Pipeline.h" />
<ClInclude Include="..\Private\RHI\DX11\DX11Shader.h" />
<ClInclude Include="..\Private\RHI\DX11\DX11StructuredBuffer.h" />
<ClInclude Include="..\Private\RHI\DX11\DX11VertexBuffer.h" />
<ClInclude Include="..\Private\RHI\DX11\SamplerStateCache.h" />
<ClInclude Include="..\Private\RHI\DX11\DX11Device.h" />   <!-- ★ rename: CDX11Device.h → DX11Device.h -->
```

### 3.3 RH-2 종료 시 `Engine.vcxproj.filters` XML diff (★ Codex P2-25)

**수정 전** (예시 — 기존 filter):
```xml
<Filter Include="00. Manager\RHI">
  <UniqueIdentifier>{aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee}</UniqueIdentifier>
</Filter>
<Filter Include="00. Manager\RHI\DX11">
  <UniqueIdentifier>{ffffffff-1111-2222-3333-444444444444}</UniqueIdentifier>
</Filter>

<ItemGroup>
  <ClInclude Include="..\Public\RHI\CDX11Device.h">
    <Filter>00. Manager\RHI</Filter>
  </ClInclude>
  <ClInclude Include="..\Public\RHI\DX11\BlendStateCache.h">
    <Filter>00. Manager\RHI\DX11</Filter>
  </ClInclude>
  <!-- ... -->
</ItemGroup>
```

**수정 후** (RH-2 종료 시):
```xml
<Filter Include="00. Manager\RHI">
  <UniqueIdentifier>{aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee}</UniqueIdentifier>
</Filter>
<Filter Include="00. Manager\RHI\DX11 (Private)">
  <UniqueIdentifier>{ffffffff-1111-2222-3333-444444444444}</UniqueIdentifier>
</Filter>

<ItemGroup>
  <ClInclude Include="..\Private\RHI\DX11\DX11Device.h">
    <Filter>00. Manager\RHI\DX11 (Private)</Filter>
  </ClInclude>
  <ClInclude Include="..\Private\RHI\DX11\BlendStateCache.h">
    <Filter>00. Manager\RHI\DX11 (Private)</Filter>
  </ClInclude>
  <!-- ... 9개 -->
</ItemGroup>
```

GUID 는 기존 값 재사용 (Visual Studio 가 자동 매칭). 새 GUID 발급 시 IDE Solution Explorer 가 그룹 분리.

### 3.4 EngineSDK 동기화 차단 (RH-2 시점)

```powershell
# RH-2 종료 시 — DX11 헤더가 SDK 에 잘못 복사되지 않게
Get-ChildItem -Path EngineSDK\inc\RHI\DX11 -ErrorAction SilentlyContinue
# 존재하면 삭제
Remove-Item EngineSDK\inc\RHI -Recurse -ErrorAction SilentlyContinue
```

향후 SDK 에는 `IRHIDevice.h / IRHIBuffer.h` 등 인터페이스만 노출.

### 3.5 RH-0 합격 (작업 3 한정)
- ✅ 본 §3.1, §3.2, §3.3 의 명령 + XML diff 가 plan 에 박제 (실 실행 X)
- ✅ RH-2 종료 시 이 sub-plan 의 §3 부분을 그대로 실행 가능

---

## 4. 통합 합격 게이트 (RH-0 전체, ★ 2차 보정판)

### 4.1 코드 변경 합격
- ✅ **`Engine/Public/RHI/DX11/` 폴더 그대로 유지** (★ Codex P0-1 — 이동 X)
- ✅ **`Engine/Public/RHI/CDX11Device.h` 그대로 유지** (★ Codex P0-2 — 이동 X)
- ✅ `CGameInstance` 의 8개 getter 가 `_Legacy` 접미사 + `[[deprecated]]` 마커
- ✅ Scene_InGame.cpp 12개 호출 모두 `_Legacy` 사용
- ✅ Scene_InGame.cpp:1898 + 다른 GetContext/GetDevice 호출에 `// ★ RH-2 TODO` 주석
- ✅ **9개 Public DX11 leak consumer 전수 TODO marker** (★ Codex P0-3 확장):
  - Engine_Defines.h, CEngineApp.h, UI_Manager.h, PlaneRenderer.h, Mesh.h, CDX11Device.h, FxBillboardComponent.h, FxMeshComponent.h, FxSystem.h

### 4.2 빌드 합격
- ✅ Engine.vcxproj 빌드 통과 (deprecated warning 8건 — 본인 정의 부)
- ✅ Client.vcxproj 빌드 통과 (deprecated warning 12+건 — Scene_InGame caller)
- ✅ **error 0건**
- ✅ EngineSDK 동기화 통과 (Post-Build Event)

### 4.3 런타임 합격
- ✅ LoL InGame 진입 → 무회귀 (시각 동일)
- ✅ 챔피언 / 맵 / FX 모두 정상 렌더
- ✅ 우클릭 이동 정상

### 4.4 추적 합격
- ✅ `rg "Get_RHIDevice\(\)|Get_MeshShader\(\)|Get_MeshPipeline\(\)|Get_BlendStateCache\(\)" Engine/Public/ Client/`
  → 0 hit (모두 _Legacy 로 마이그)
- ✅ `rg "GetContext\(\)|GetDevice\(\)" Client/`
  → 모든 hit 에 `// ★ RH-2 TODO` 주석 동반

---

## 5. 위험 / 디버깅 메모

| 위험 | 완화 |
|---|---|
| 9개 consumer 마킹 누락 시 RH-2 진입 시 추가 발견 부담 | RH-0 종료 후 `rg "RHI/DX11/\|d3d11.h\|CDX11Device" Engine/Public/ Client/Public/ Client/Private/` 으로 전수 재확인 |
| sed/sed 의 windows 호환 문제 | PowerShell `-replace` 사용 (위 §2.4) — 유니코드 안전 |
| Engine_Defines.h 의 DX11 의존 분기 결정 보류 시 RH-2 작업 막힘 | RH-1 진입 전에 §2.2 의 (A)/(B)/(C) 결정 필수 |
| 8개 getter rename 시 `EngineSDK/inc/GameInstance.h` 동기화 누락 | Post-Build Event 자동화 + 수동 cp 명령 병행 |
| 12개 Scene_InGame 치환 시 부분 매치 사고 (예: `Get_RHIDeviceXX` 같은 fake 매치) | `\b...\(\)` 의 `\b` word boundary + `\(\)` 정확 매치 (위 §2.4 정규식) |

---

## 6. 한 줄 (★ 2차 보정판)

**RH-0 = "이동 X, inventory + Legacy rename + 9 leak consumer 전수 TODO marker". 9개 헤더 / CDX11Device.h / Engine.vcxproj 그대로 유지. CGameInstance 8개 getter `_Legacy` rename + `[[deprecated]]` + 9개 consumer + Scene_InGame 12 hit 마이그 + 1898 줄 RH-2 TODO. 1주. 합격 = LoL 빌드 통과 + 무회귀 + 9개 consumer 전수 마킹 + RH-1/RH-2 사전 준비 완료.**
