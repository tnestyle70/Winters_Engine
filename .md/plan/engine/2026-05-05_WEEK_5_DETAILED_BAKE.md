# 2026-05-05 Week 5 상세 박제 — Track 1 안정화 + Track 2 RH-2 완료

**작성일**: 2026-05-05
**상태**: 검토 대기 (계획서만 작성, 코드 변경은 codex 가 진행 / 작성자 후속 검토)
**전제**: Week 4 (SSAO + RH-2 시작) 완료
**상위 문서**: [Twin Track 계획서 §5.2](2026-05-01_TWIN_TRACK_GGX_BRDF_DX12_VULKAN_MERGE_PLAN.md), [RHI 마스터 §6.3 RH-2 합격](../rhi/00_RHI_MIGRATION_MASTER.md)

---

## 0. 한 줄

> **Week 5 = T1 (W2-W4 결과물 회귀 검증 + 튜닝) + T2 (RH-2 완료 — Public DX11 헤더 제거 + 파일 이동 + Get_NewRHIDevice → Get_RHIDevice 정식 rename + _Legacy 8개 제거). 합격: `grep "ID3D11Device|d3d11.h|RHI/DX11" Engine/Public` hit 0 + LoL 빌드 통과 + 게임 회귀 0.**

---

> **Dependency note (2026-05-02):** this bake assumes Week 4 was executed on top of the corrected Week 3 scope. If RH-1 remained a seed-only pass, public-header cleanup and rename work should be re-scoped before implementation.

## 1. Week 4 결과 검증 (Week 5 진입 전)

```bash
# 1. SSAO 인프라
ls Shaders/SSAO/{GTAO_CS,GTAO_Blur_CS}.hlsl Engine/Public/Renderer/{NormalPass,SSAOPass}.h

# 2. Public 헤더 ID3D11* 노출 (W4 끝 시점)
rg "ID3D11Device|d3d11\\.h|RHI/DX11" Engine/Public/ Client/Public/ -l
# 기대: 0~2 파일 (W5 에서 0 으로)

# 3. 빌드 + 런타임 (이렐리아 PBR + Forward+ + SSAO Frame ≤20ms)
```

---

## 2. Week 5 작업 매트릭스

| 순서 | 작업 | 파일 | 의존 |
|---|---|---|---|
| **T1.1** | Week 2~4 결과물 회귀 검증 (PBR + Forward+ + SSAO 모두 동작) | 빌드 + 런타임 | (W4 완료) |
| **T1.2** | 이렐리아 외 챔프 6명 PBR 적용 (Yasuo / Sylas / Kalista / Garen / Zed / Riven) | Champion 폴더 | T1.1 |
| **T1.3** | 맵 / 미니언 / 정글몹 PBR 미적용 검증 (unlit 그대로) | 회귀 검증 | T1.1 |
| **T1.4** | Performance 측정 표 (단일 챔프 / 5v5 / 10v10) | 문서 | T1.1 |
| **T2.1** | Public 헤더 잔여 노출 0 으로 마이그 (W4 끝 0~2 → 0) | Engine/Public/* | (W4) |
| **T2.2** | `Engine/Public/RHI/DX11/` → `Engine/Private/RHI/DX11/` 디렉토리 이동 | git mv + vcxproj 수정 | T2.1 |
| **T2.3** | `Engine/Public/RHI/CDX11Device.h` → `Engine/Private/RHI/DX11/DX11Device.h` 이동 | git mv | T2.2 |
| **T2.4** | `Engine.vcxproj` + `.filters` XML 수정 | XML | T2.2, T2.3 |
| **T2.5** | `CGameInstance::Get_NewRHIDevice()` → `Get_RHIDevice()` 정식 rename | GameInstance.h + .cpp | T2.4 |
| **T2.6** | `_Legacy` 8개 게터 + 정의 제거 | GameInstance.h + .cpp | T2.5 |
| **T2.7** | 모든 `_Legacy` 호출 사이트 → 정식 이름으로 일괄 치환 | Engine + Client 전체 | T2.5, T2.6 |
| **T2.8** | UpdateLib.bat 갱신 (Public 헤더 변경 반영) | UpdateLib.bat | T2.2~T2.4 |
| **T2.9** | 검증: `grep "ID3D11Device|d3d11.h|RHI/DX11" Engine/Public` hit 0 | bash | 모두 |

**의존**: T2 가 T1 보다 무거움. T1.1 회귀 검증 끝나면 T2.1~T2.9 순차 진행.

---

## 3. Track 1 — 안정화

### 3.1 회귀 검증 매트릭스 (T1.1)

| 검증 항목 | 명령 / 시각 | 기대 |
|---|---|---|
| 이렐리아 PBR | InGame 진입 + Tuner Metallic 0→1 | specular 강해짐 |
| 야스오/사일러스/칼리스타/비에고 PBR (W4 완료 시 적용) | InGame 진입 | (W5 T1.2 적용 후) |
| 가렌/제드/이즈리얼/리븐 (W4 미적용 — unlit) | InGame 진입 | unlit 그대로, 회귀 0 |
| Forward+ 64 라이트 | Tuner Spawn 64 | Frame ≤16ms |
| SSAO on/off | Tuner Checkbox | 그림자 변화 확인 |
| 맵 (소환사 협곡) | InGame 진입 | unlit 그대로 (W6 PBR 마이그 전) |
| 미니언 라인 클래시 | 자동 spawn | 회귀 0, FPS drop 없음 |
| 정글몹 (블루/레드/드래곤) | Camp 진입 | 회귀 0 |
| 포탑 + 넥서스 | 시야 확보 | unlit 그대로 (구조물 PBR 은 추후) |
| FX (이렐리아 Q/W/E/R) | 스킬 시전 | 회귀 0 (FxBillboard / FxMesh 기존 셰이더) |

### 3.2 챔프 6명 PBR 적용 (T1.2)

각 챔프 폴더 (`Client/Private/GameObject/Champion/<NAME>/`) 의 머티리얼 적용 위치 확인 후 패턴 동일:

```cpp
// Yasuo / Sylas / Kalista / Garen / Zed / Riven 공통 패턴 (Scene_InGame 또는 Champion 진입부)

auto pPBR = CMaterialPBR::Create(CGameInstance::Get()->Get_NewRHIDevice());

// 머티리얼 값은 챔프별 디폴트 (예시)
// - Garen: metallic=0.4 (갑옷), roughness=0.6
// - Zed:   metallic=0.2, roughness=0.4
// - Kalista: metallic=0.0, roughness=0.7
// - Yasuo: metallic=0.0 (천), roughness=0.8
// - Sylas: metallic=0.3 (체인), roughness=0.5
// - Riven: metallic=0.5 (검), roughness=0.4

pPBR->SetMetallic(<champion default>);
pPBR->SetRoughness(<champion default>);

m_pXxxMaterial = std::move(pPBR);
```

### 3.3 Performance 표 (T1.4)

본문에 박제 (W5 끝 측정 후 갱신):

| 시나리오 | Frame (ms) | GPU (%) | 비고 |
|---|---|---|---|
| 단일 챔프 (이렐리아) idle | TBD | TBD | (W5 측정) |
| 단일 챔프 + Forward+ 64 라이트 | TBD | TBD | |
| 단일 챔프 + Forward+ 64 + SSAO | TBD | TBD | |
| 5v5 (10 챔프 + 미니언) | TBD | TBD | |
| 10v10 가정 (스트레스) | TBD | TBD | |

목표:
- 단일 챔프: ≤17ms
- 5v5: ≤20ms
- 10v10: ≤25ms (W6+ 최적화 대상)

### 3.4 합격 게이트 (Track 1 W5)

- ✅ 회귀 검증 매트릭스 모두 통과
- ✅ 챔프 7명 (Irelia + 6) PBR 정상 동작
- ✅ Performance 표 기록 (W6 IRHI 마이그 후 비교 기준)
- ✅ 시각 회귀 0 (PBR 미적용 자산은 unlit 그대로)

---

## 4. Track 2 — RH-2 완료

### 4.1 Public 헤더 잔여 노출 0 (T2.1)

**W4 끝 시점 잔여 inventory** (예시 — 실제는 W4 §4.7 결과 기준):

```
Engine/Public/Editor/ImGuiLayer.h        # ImGui DX11 backend 통과 — escape 정식화
Engine/Public/Renderer/FxStaticMeshRenderer.h  # FX 정적 메쉬
```

**ImGuiLayer.h 처리** (escape 정식화):

```cpp
// BEFORE
#include <d3d11.h>
class CImGuiLayer
{
public:
    static unique_ptr<CImGuiLayer> Create(HWND hWnd, ID3D11Device* pDev, ID3D11DeviceContext* pCtx);
};

// AFTER (W5)
//   d3d11.h include 제거. 인자도 IRHIDevice* 통과.
#include "RHI/IRHIDevice.h"
class CImGuiLayer
{
public:
    static unique_ptr<CImGuiLayer> Create(HWND hWnd, IRHIDevice* pDevice);
    //   ↑ .cpp 안에서 GetNativeHandle 로 ID3D11Device*/Context* 획득해서 ImGui_ImplDX11_Init 호출
};
```

### 4.2 디렉토리 이동 (T2.2)

```bash
# git mv 권장 (history 보존)
cd C:/Users/tnest/Desktop/Winters_restored/Winters

# 1. RHI/DX11 폴더 이동
git mv Engine/Public/RHI/DX11 Engine/Private/RHI/DX11

# 2. CDX11Device.h 이동 (rename)
git mv Engine/Public/RHI/CDX11Device.h Engine/Private/RHI/DX11/DX11Device.h
#   ↑ 이름도 변경: 'CDX11Device.h' → 'DX11Device.h' (파일명 C 접두사 제거 컨벤션)
```

**클래스 이름 자체는 유지** (`class CDX11Device` 그대로). 단 헤더 파일명만 변경.

### 4.3 Engine.vcxproj + filters XML 수정 (T2.4)

`Engine.vcxproj`:

```xml
<!-- BEFORE -->
<ClInclude Include="Public\RHI\CDX11Device.h" />
<ClInclude Include="Public\RHI\DX11\DX11Buffer.h" />
<ClInclude Include="Public\RHI\DX11\DX11Pipeline.h" />
<!-- ... -->

<!-- AFTER -->
<ClInclude Include="Private\RHI\DX11\DX11Device.h" />
<ClInclude Include="Private\RHI\DX11\DX11Buffer.h" />
<ClInclude Include="Private\RHI\DX11\DX11Pipeline.h" />
<!-- ... -->
```

`Engine.vcxproj.filters`:

```xml
<!-- BEFORE -->
<ClInclude Include="Public\RHI\CDX11Device.h">
  <Filter>00. Manager\RHI</Filter>
</ClInclude>

<!-- AFTER -->
<ClInclude Include="Private\RHI\DX11\DX11Device.h">
  <Filter>00. Manager\RHI\DX11 (Private)</Filter>
</ClInclude>
```

**AdditionalIncludeDirectories** 도 갱신:

```xml
<!-- 기존 Public/RHI/DX11 삭제 + Private/RHI/DX11 추가 -->
<AdditionalIncludeDirectories>...;..\Public\RHI;..\Private\RHI\DX11;...</AdditionalIncludeDirectories>
```

### 4.4 GetNewRHIDevice → Get_RHIDevice 정식 rename (T2.5)

**단계**:

1. `Get_RHIDevice_Legacy` 모든 caller 가 정식으로 마이그된 후 (W4 RH-2 시작 시 마이그 시작),
2. `Get_NewRHIDevice` → `Get_RHIDevice` 로 rename (이름 충돌 회피 — _Legacy 제거 후 정식 이름 활용)

**변경 BEFORE → AFTER**:

```cpp
// BEFORE (W3 RH-1 추가, W5 진입 시 상태)
public:
    // RH-0 §2 (Legacy)
    [[deprecated(...)]] CDX11Device* Get_RHIDevice_Legacy();
    [[deprecated(...)]] DX11Shader*  Get_MeshShader_Legacy();
    /* ... 8개 ... */

    // RH-1 (신규 — 임시 이름)
    IRHIDevice* Get_NewRHIDevice();

// AFTER (W5 RH-2 완료)
public:
    // RH-2 (W5): 정식 인터페이스. _Legacy 8개 제거 + Get_NewRHIDevice → Get_RHIDevice rename.
    IRHIDevice* Get_RHIDevice();
```

`GameInstance.cpp`:

```cpp
// BEFORE
IRHIDevice* CGameInstance::Get_NewRHIDevice() { return &CEngineApp::Get().GetDevice(); }
CDX11Device* CGameInstance::Get_RHIDevice_Legacy() { return &CEngineApp::Get().GetDevice(); }
/* ... 8개 _Legacy 정의 ... */

// AFTER (W5)
IRHIDevice* CGameInstance::Get_RHIDevice() { return &CEngineApp::Get().GetDevice(); }
//   ↑ _Legacy 8개 정의 모두 제거. Get_NewRHIDevice 도 제거 후 Get_RHIDevice 로 통합.
```

### 4.5 _Legacy 8개 제거 (T2.6)

**제거 대상**:
- `Get_RHIDevice_Legacy`
- `Get_MeshShader_Legacy`
- `Get_MeshPipeline_Legacy`
- `Get_BlendStateCache_Legacy`
- `Get_FxSpriteShader_Legacy`
- `Get_FxSpritePipeline_Legacy`
- `Get_FxMeshShader_Legacy`
- `Get_FxMeshPipeline_Legacy`

**대안 (caller 측)**:

| _Legacy 게터 | 대안 |
|---|---|
| `Get_RHIDevice_Legacy()` | `Get_RHIDevice()` (정식 IRHIDevice*) |
| `Get_MeshShader_Legacy()` | `Get_RHIDevice()->GetNativeHandle(eRHINativeType::DX11Device)` 후 CEngineApp 의 GetMeshShader (단 W6 RH-3 IRHIShader 통과로 정식 마이그) |
| `Get_*Shader_Legacy / *Pipeline_Legacy` | (W6 RH-3 시점에 IRHIShader / IRHIPipelineState 통과) — W5 임시: ModelRenderer 등 caller 가 CEngineApp 직접 friend 또는 escape API 사용 |
| `Get_BlendStateCache_Legacy` | (RH-3 IRHIBlendState 미정 — W5 시점에 임시 escape) |

> 즉 W5 에서 _Legacy 제거 후 caller 의 일부는 CEngineApp 의 friend 패턴 (Engine 내부 한정) 또는 W6 RH-3 까지 임시 보류.
> **현실적 타협**: W5 에서 `Get_RHIDevice_Legacy` 만 제거 (정식 `Get_RHIDevice` 로 대체). 나머지 7개 `Shader/Pipeline/Cache _Legacy` 는 W6 RH-3 인터페이스 도입 후 제거.

### 4.6 caller 일괄 치환 (T2.7)

```bash
# Get_RHIDevice_Legacy → Get_RHIDevice
rg -l "Get_RHIDevice_Legacy\(\)" Engine Client \
  | grep -v "GameInstance\\.h\\|GameInstance\\.cpp" \
  | while read f; do
    sed -i "s/Get_RHIDevice_Legacy()/Get_RHIDevice()/g" "$f"
  done

# 검증: _Legacy 호출 hit 0 (Shader/Pipeline _Legacy 는 W6 까지 잔존)
rg "Get_RHIDevice_Legacy\(\)" Engine Client | wc -l
# 기대: 0
```

### 4.7 UpdateLib.bat 갱신 (T2.8)

```bat
REM BEFORE
xcopy /Y /D /S "Engine\Public\*.h" "EngineSDK\inc\"
REM ↑ Public 안의 모든 .h 가 EngineSDK 로 복사됨

REM AFTER (W5)
REM CDX11Device.h / RHI/DX11/* 가 Public 에서 제거됐으므로 자동으로 EngineSDK 에서도 사라짐.
REM 단, 명시적 제외 필요한 항목 (예: Engine_Manager.h) 가 있으면 추가:
xcopy /Y /D /S "Engine\Public\*.h" "EngineSDK\inc\"
REM 추가 변경 없음.

REM 검증
@if not exist "EngineSDK\inc\RHI\CDX11Device.h" echo "OK: CDX11Device.h removed from SDK"
@if not exist "EngineSDK\inc\RHI\DX11\DX11Buffer.h" echo "OK: RHI/DX11 dir removed from SDK"
```

### 4.8 검증 명령 (T2.9)

```bash
# 1. Public 헤더 ID3D11/d3d11.h/RHI/DX11 노출 hit 0
rg "ID3D11Device|ID3D11DeviceContext|d3d11\\.h|RHI/DX11" \
   Engine/Public/ Client/Public/ -l | wc -l
# 기대: 0

# 2. _Legacy caller hit 0 (Get_RHIDevice_Legacy 만 — Shader/Pipeline 7개는 W6 까지 잔존)
rg "Get_RHIDevice_Legacy\(\)" Engine Client | wc -l
# 기대: 0

# 3. EngineSDK/inc 정합
ls EngineSDK/inc/RHI/         # CDX11Device.h 가 없어야 함
ls EngineSDK/inc/RHI/DX11/    # 디렉토리 자체가 없어야 함
# 기대: 둘 다 없음

# 4. 빌드 통과
MSBuild Winters.sln /p:Configuration=Debug /p:Platform=x64 /m
# 기대: error 0
```

### 4.9 합격 게이트 (Track 2 W5)

- ✅ `Engine/Public/RHI/CDX11Device.h` 존재 X (Private 로 이동)
- ✅ `Engine/Public/RHI/DX11/` 디렉토리 존재 X (Private 로 이동)
- ✅ `rg "ID3D11Device|d3d11.h|RHI/DX11" Engine/Public Client/Public` hit 0
- ✅ `Get_RHIDevice()` 가 `IRHIDevice*` 반환 (정식)
- ✅ `Get_NewRHIDevice()` / `Get_RHIDevice_Legacy()` 미존재
- ✅ LoL 빌드 통과 (deprecated warning Shader/Pipeline 7개 잔존 OK)

---

## 5. 위험 시나리오

### 5.1 R-W5-1: 디렉토리 이동 후 빌드 실패 (cascade)
- 시나리오: `git mv` 후 50+ caller 가 `#include "RHI/DX11/..."` 못 찾음
- 완화: ① W4 RH-2 시작 단계에서 caller 마이그 모두 끝낸 후 W5 진입 ② vcxproj AdditionalIncludeDirectories 에 임시로 Public 와 Private 둘 다 등록 (단 Public 폴더 자체 빈 상태) → caller 수정 후 Public 등록 제거

### 5.2 R-W5-2: ImGui DX11 backend 가 ImGuiLayer 마이그 후 깨짐
- 시나리오: ImGui_ImplDX11_Init 가 raw `ID3D11Device*` 요구 → IRHIDevice* 통과 후 GetNativeHandle 로 획득해도 init 시점에 device 가 nullptr
- 완화: ① ImGuiLayer Create 가 IRHIDevice* 를 명시 인자로 받음 ② Create 안에서 GetNativeHandle 호출 후 init ③ device 가 nullptr 이면 early return + 로그

### 5.3 R-W5-3: _Legacy 7개 잔존이 codex 의도와 다름
- 시나리오: codex 가 W5 에서 8개 _Legacy 모두 제거 시도 → Shader/Pipeline 7개의 IRHIShader 인터페이스 미존재 (W6 까지)
- 완화: ① W5 작업 매트릭스에 "Get_RHIDevice_Legacy 만 제거, 나머지 7개는 W6" 명시 ② 본 §4.5 의 "현실적 타협" 절 강조

### 5.4 R-W5-4: UpdateLib.bat 갱신 누락
- 시나리오: Engine 빌드 후 EngineSDK/inc 에 옛 RHI/DX11 헤더 잔존 → Client 빌드 시 옛 헤더 참조
- 완화: ① UpdateLib.bat 에 사전 cleanup 단계 추가 (`rmdir /S /Q EngineSDK\inc\RHI\DX11` 후 xcopy) ② 또는 EngineSDK/inc 전체 wipe 후 재복사

---

## 6. Week 5 통합 합격 검증

```bash
# 1. Public 헤더 노출 0
rg "ID3D11Device|ID3D11DeviceContext|d3d11\\.h|RHI/DX11" Engine/Public/ Client/Public/ -l | tee /tmp/w5_leak.txt
[ ! -s /tmp/w5_leak.txt ] && echo "OK: 0 leak" || echo "FAIL: leak remains"

# 2. CDX11Device.h Private 위치
ls Engine/Private/RHI/DX11/DX11Device.h && echo "OK"
[ ! -e Engine/Public/RHI/CDX11Device.h ] && echo "OK: Public 에서 제거"

# 3. Get_RHIDevice 정식 (return IRHIDevice*)
grep -A 1 "Get_RHIDevice\b" Engine/Include/GameInstance.h | head -5

# 4. _Legacy 미사용
rg "Get_RHIDevice_Legacy\(\)" Engine Client | wc -l   # 0
rg "Get_NewRHIDevice\(\)" Engine Client | wc -l        # 0

# 5. EngineSDK/inc 동기화
ls EngineSDK/inc/RHI/ 2>/dev/null  # CDX11Device.h 없음, DX11/ 디렉토리 없음

# 6. 빌드 통과
MSBuild Winters.sln /p:Configuration=Debug /p:Platform=x64 /m

# 7. 런타임 회귀 0 (이렐리아 PBR + Forward+ + SSAO)
```

---

## 7. 부록 A — Week 5 진입 체크리스트

```
[ ] Week 4 결과 검증 통과 (특히 Public 노출 0~2 확인)
[ ] devenv.exe 종료 + git: feature/2026-05-05-week5 branch
[ ] Engine 단독 빌드 → EngineSDK/inc 동기화

Track 1 (안정화):
[ ] §3.1 회귀 검증 매트릭스 모두 통과
[ ] §3.2 챔프 6명 (Yasuo/Sylas/Kalista/Garen/Zed/Riven) PBR 적용
[ ] §3.3 맵/미니언/정글몹 unlit 그대로 (회귀 0)
[ ] §3.4 Performance 표 기록

Track 2 (RH-2 완료):
[ ] §4.1 Public 헤더 잔여 노출 (W4 끝 0~2 → 0)
[ ] §4.2 git mv Engine/Public/RHI/DX11 → Engine/Private/RHI/DX11
[ ] §4.3 git mv Engine/Public/RHI/CDX11Device.h → Engine/Private/RHI/DX11/DX11Device.h
[ ] §4.4 Engine.vcxproj + .filters XML 수정
[ ] §4.5 Get_NewRHIDevice → Get_RHIDevice 정식 rename
[ ] §4.6 Get_RHIDevice_Legacy 제거 (Shader/Pipeline 7개는 W6)
[ ] §4.7 caller 일괄 치환 (Get_RHIDevice_Legacy → Get_RHIDevice)
[ ] §4.8 UpdateLib.bat 갱신 (RHI/DX11 cleanup)

검증:
[ ] §6.1 Public 노출 0
[ ] §6.4 _Legacy / NewRHIDevice 미사용
[ ] §6.5 EngineSDK/inc 정합
[ ] §6.6 빌드 error 0
[ ] §6.7 런타임 회귀 0
```

---

## 8. 한 줄

> **Week 5 = T1 (W2-W4 회귀 검증 + 챔프 6명 PBR 적용 + Performance 표) + T2 (Public DX11 헤더 제거 + DX11 디렉토리 Private 이동 + Get_NewRHIDevice → Get_RHIDevice 정식 rename + Get_RHIDevice_Legacy 제거 + UpdateLib.bat 갱신). 합격: Public 노출 0 + EngineSDK/inc 정합 + 회귀 0.**

---

## 끝.
