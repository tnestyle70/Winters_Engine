# EFX Progress + Next Actions — 현 박제 인용 + 다음 N 단계 가이드

**작성일**: 2026-05-07
**상태**: ⚠️ v3 보강 참고문서. 다음 실행 권위는 [`17_NIAGARA_FULL_REWRITE_MASTER.md`](17_NIAGARA_FULL_REWRITE_MASTER.md).
**선행**: [`17_NIAGARA_FULL_REWRITE_MASTER.md`](17_NIAGARA_FULL_REWRITE_MASTER.md)
**용도**: 박제 진입 시 "지금 어디까지 됐고 다음에 뭐 박제하면 되는지" 한 눈에 확인

---

## §1. 박제 현황 요약 (2026-05-07 기준)

### 1.1 헤더 박제 매트릭스

| 파일 | 줄수 | 상태 | 검증 인용 |
|---|---|---|---|
| `Engine/Public/FX/FxAsset.h` | 131 | ✅ 박제 | `FxAsset.h:84` `struct FxAsset` |
| `Engine/Public/FX/ParameterMap.h` | 156 | ✅ 박제 | `ParameterMap.h:14` `enum class eFxNamespace` |
| `Engine/Public/FX/ParticlePool.h` | 68 | ✅ 박제 | `ParticlePool.h:12` `class CParticlePool` |
| `Engine/Public/FX/DeterministicRandom.h` | 21 | ✅ 박제 | `DeterministicRandom.h:6` `class CXoroshiro128` |
| `Engine/Public/FX/FxLifecycle.h` | — | ❌ 미박제 | EFX-2 진입 |
| `Engine/Public/FX/FxSystemInstance.h` | — | ❌ 미박제 | EFX-2 진입 |
| `Engine/Public/FX/FxEmitterInstance.h` | — | ❌ 미박제 | EFX-2 진입 |
| `Engine/Public/FX/FxSimulationSystem.h` | — | ❌ 미박제 | EFX-2 진입 |
| `Engine/Public/FX/FxNodeCompiler.h` | — | ❌ 미박제 | EFX-5 진입 |
| `Engine/Public/FX/FxExpressionVM.h` | — | ❌ 미박제 | EFX-5 진입 |
| `Engine/Public/FX/FxNodeRegistry.h` | — | ❌ 미박제 | EFX-5 진입 |
| `Engine/Public/FX/FxAssetSerializer.h` | — | ❌ 미박제 | EFX-1 후반 |
| `Engine/Public/ECS/Components/FxInstanceComponent.h` | — | ❌ 미박제 | EFX-2 진입 |

### 1.2 .cpp 박제 매트릭스

| 파일 | 상태 | 비고 |
|---|---|---|
| `Engine/Private/FX/FxAsset.cpp` | 🟡 부분 | Find/Register 박제, LoadFromFile 본체 미박제 |
| `Engine/Private/FX/ParticlePool.cpp` | ✅ 박제 | Spawn/KillSwapBack 박제 |
| `Engine/Private/FX/DeterministicRandom.cpp` | ✅ 박제 | xoroshiro128 박제 |
| `Engine/Private/FX/FxSimulationSystem.cpp` | ❌ 미박제 | EFX-2 |

### 1.3 Client 측 컴포넌트 박제 매트릭스

| 파일 | 줄수 | 상태 | 비고 |
|---|---|---|---|
| `Client/Public/GameObject/FX/FxBillboardComponent.h` | 90 | ✅ 박제 | hAsset 필드 추가 (transitional 3 멤버: texturePath + texturePathOwner + hAsset) |
| `Client/Public/GameObject/FX/FxRibbonComponent.h` | 76 | ✅ 박제 | 16 point + width + lifetime |
| `Client/Public/GameObject/FX/FxBeamComponent.h` | 65 | ✅ 박제 | start/end entity + offset + UV scroll |
| `Client/Public/GameObject/FX/FxMeshComponent.h` | — | ✅ 박제 | mesh + texture + material |
| `Client/Public/GameObject/FX/FxGroundDecalComponent.h` | — | ❌ 미박제 | EFX-3 잔여 |
| `Client/Public/GameObject/FX/FxShockwaveComponent.h` | — | ❌ 미박제 | EFX-3 잔여 |
| `Client/Public/GameObject/FX/LegacyFxAdapter.h` | 43 | ✅ 박제 | EFX-0 |
| `Client/Public/GameObject/FX/FxSystem.h` | — | ✅ 박제 | SpawnFromAsset / DeferSpawnFromAsset / GetAssetRegistry 박제 |
| `Client/Public/GameObject/FX/FxBeamSystem.h` | — | ✅ 박제 | Beam 시뮬+렌더 |
| `Client/Public/GameObject/FX/FxMeshSystem.h` | — | ✅ 박제 | Mesh 시뮬+렌더 |
| `Client/Public/GameObject/FX/FxRibbonSystem.h` | — | ❌ 미박제 | EFX-3 잔여 |
| `Client/Public/Scene/Scene_EffectTool.h` | — | ❌ 미박제 | EFX-4 |

### 1.4 Champion FxPresets 박제 매트릭스 (EFX-0 LegacyFx 통과 검증)

| 챔프 | FxPresets 파일 | LegacyFx 통과 | 인용 |
|---|---|---|---|
| Irelia | `IreliaFxPresets.cpp` | ✅ 통과 | `IreliaFxPresets.cpp:31-37` `SpawnBillboardAsset()` 헬퍼 사용 |
| Yasuo | `YasuoFxPresets.cpp` | ❌ 미통과 (추정) | EFX-0 잔여 |
| Annie | (`AnnieFxPresets.cpp` 추정) | ❌ 미통과 (추정) | EFX-0 잔여 |
| Yone | (`Yone_Skills.cpp` 직접) | ❌ 미통과 | EFX-0 잔여 |
| Ezreal/Garen/Kalista/Riven/Zed | (FxPresets 미박제) | — | 보류 |
| Ashe/Fiora/Jax | (표준 3-file) | — | 보류 |

### 1.5 자산 디렉토리 박제 매트릭스

| 디렉토리 | 상태 | 자산 수 |
|---|---|---|
| `Client/Bin/Resource/FX/` | ❌ 폴더 미생성 | 0 |
| `Client/Bin/Resource/FX/Champions/` | ❌ | 0 / (target 22) |
| `Client/Bin/Resource/FX/Bosses/` | ❌ | 0 / (target 6, EFX-6) |
| `Client/Bin/Resource/FX/ClassServant/` | ❌ | 0 / (target TBD, EFX-6) |

### 1.6 Editor / UI 박제 매트릭스

| 파일 | 상태 | 비고 |
|---|---|---|
| `Client/Private/UI/EffectTuner.cpp` | 🟡 legacy | Irelia 7 hardcode — EFX-4 진입 시 deprecated 표시 또는 제거 |
| `Client/Private/Scene/Scene_Editor.cpp` | ✅ 박제 | 일반 에디터, EffectTool 과 무관 |
| `Client/Public/Scene/Scene_EffectTool.h` | ❌ | EFX-4 진입 |
| `Client/Public/UI/EffectToolPanels/AssetBrowserPanel.h` | ❌ | EFX-4 |
| `Client/Public/UI/EffectToolPanels/PreviewViewportPanel.h` | ❌ | EFX-4 |
| `Client/Public/UI/EffectToolPanels/InspectorPanel.h` | ❌ | EFX-4 |
| `Client/Public/UI/EffectToolPanels/TimelinePanel.h` | ❌ | EFX-4 |
| `Client/Public/UI/EffectToolPanels/NodeGraphPanel.h` | ❌ | EFX-5 |

### 1.7 Stage 별 진척도 (%)

| Stage | 진척도 | 잔여 가중치 |
|---|---|---|
| EFX-0 Legacy Bridge | **50%** | Yasuo/Annie/Yone preset 통과 + 22 .wfx 영속화 |
| EFX-1 FxAsset + ParameterMap | **70%** | LoadFromFile/LoadDirectory 본체 + JSON 라이브러리 |
| EFX-2 ParticlePool + Simulation System | **40%** | FxLifecycle/Component/Instance/System 박제 |
| EFX-3 Multi-Render Type | **50%** | Ribbon System + Decal/Shockwave 컴포넌트+시스템 |
| EFX-4 Scene_EffectTool | **0%** | 전량 |
| EFX-5 Node Executor + VM | **5%** (FxNodeDesc 자료형만) | 전량 |
| EFX-6 Elden VFX Pack | **0%** | 6 작품 |
| EFX-7 GPU Compute | **0%** (보류) | RHI/RG 안정 후 |

**전체 진척도**: ~35%.

---

## §2. EFX-0 잔여 — 다음 N 단계 (총 5 단계, 2 days)

### Step 0-1 — Yasuo preset LegacyFx 통과

**대상 파일**: `Client/Private/GameObject/Champion/Yasuo/YasuoFxPresets.cpp`

**현 가정** (실측 미확인 — Yasuo Spawn 함수 grep 필요):
```cpp
// 현재 (가정):
EntityID YasuoFx::SpawnQ(CWorld& world, EntityID owner, ...)
{
    FxBillboardComponent fx{};
    fx.attachTo = owner;
    fx.texturePath = L"...";
    // ...
    return CFxSystem::Spawn(world, fx);   // ★ 직접 spawn
}
```

**박제**:
```cpp
// 변경 후:
EntityID YasuoFx::SpawnQ(CWorld& world, EntityID owner, ...)
{
    FxBillboardComponent fx{};
    fx.attachTo = owner;
    fx.texturePath = L"...";
    // ...
    return SpawnBillboardAsset(world, fx, "Yasuo_Q");   // ★ LegacyFx 통과
}

// IreliaFxPresets.cpp 의 SpawnBillboardAsset 헬퍼를 별도 헬퍼 파일로 추출하거나, 챔프 별 헬퍼 정의:
namespace { /* anonymous */
    EntityID SpawnBillboardAsset(CWorld& world, const FxBillboardComponent& fx, const char* pszName)
    {
        CFxAssetRegistry& registry = CFxSystem::GetAssetRegistry();
        const FxAssetHandle handle = LegacyFx::FxAssetFromBillboard(registry, fx, pszName);
        return CFxSystem::SpawnFromAsset(world, handle, fx.vWorldPos, fx.attachTo);
    }
}
```

**또는 더 좋은 방법** (헬퍼 추출):
```cpp
// Client/Public/GameObject/FX/LegacyFxAdapter.h 에 추가:
namespace LegacyFx
{
    // 신규 — 헬퍼
    EntityID SpawnBillboardAuto(CWorld& world,
                                 const FxBillboardComponent& src,
                                 const char* pszAssetName);
}
```

```cpp
// Client/Private/GameObject/FX/LegacyFxAdapter.cpp:
EntityID LegacyFx::SpawnBillboardAuto(CWorld& world,
                                       const FxBillboardComponent& src,
                                       const char* pszAssetName)
{
    CFxAssetRegistry& registry = CFxSystem::GetAssetRegistry();
    const FxAssetHandle handle =
        LegacyFx::FxAssetFromBillboard(registry, src, pszAssetName);
    return CFxSystem::SpawnFromAsset(world, handle, src.vWorldPos, src.attachTo);
}
```

→ Yasuo/Annie/Yone preset 의 `SpawnBillboardAsset` → `LegacyFx::SpawnBillboardAuto` 사용. **헬퍼 중복 제거**.

**합격**: Yasuo Q/W/E/R/BA 5 preset 모두 LegacyFx 통과 + InGame 시각 동일.

### Step 0-2 — Annie preset LegacyFx 통과

같은 패턴. Annie Q/W/E/R/BA 5 preset 변환.

### Step 0-3 — Yone preset LegacyFx 통과

`Yone_Skills.cpp` 가 직접 `FxBillboardComponent` 사용 중일 가능성 — `LegacyFx::SpawnBillboardAuto` 통과로 변경.

### Step 0-4 — `LegacyFx::DumpAllAssetsToWfx` 박제

**파일**: `Client/Public/GameObject/FX/LegacyFxAdapter.h` 추가
```cpp
namespace LegacyFx
{
    // 신규
    u32_t DumpAllAssetsToWfx(const CFxAssetRegistry& registry,
                              const wstring_t& outDirectory);
    // 모든 등록된 FxAsset 을 .wfx 파일로 저장. 디렉토리 자동 생성.
    // 반환: 저장된 자산 개수
}
```

**구현**: `Client/Private/GameObject/FX/LegacyFxAdapter.cpp` 추가
```cpp
u32_t LegacyFx::DumpAllAssetsToWfx(const CFxAssetRegistry& registry,
                                    const wstring_t& outDirectory)
{
    std::filesystem::create_directories(outDirectory);

    u32_t count = 0;
    for (u32_t i = 0; i < registry.GetAssetCount(); ++i)
    {
        FxAssetHandle h = registry.GetHandleByIndex(i);   // 신규 메서드 필요 — registry 에 추가
        const FxAsset* pAsset = registry.Find(h);
        if (!pAsset) continue;

        wstring_t filename = outDirectory + L"/" +
                              StringToWString(pAsset->strName) + L".wfx";

        if (FxAssetSerializer::SaveToWfx(*pAsset, filename))   // EFX-1 후반 박제
            ++count;
    }
    return count;
}
```

**병행 박제**: `CFxAssetRegistry::GetAssetCount()` 는 이미 박제 (`FxAsset.h:109`). `GetHandleByIndex(u32_t)` 는 신규 — 박제 필요.

### Step 0-5 — InGame 첫 실행 시 22 자산 저장

**위치**: Client `CScene_InGame::OnEnter()` 또는 `Loader.cpp` 의 자산 로드 단계.

```cpp
// Client/Private/Scene/Scene_InGame.cpp::OnEnter() 끝부분
namespace
{
    bool g_bAssetsDumped = false;
}

void CScene_InGame::OnEnter()
{
    // ... 기존 init

    // EFX-0 Step 0-5: 첫 실행 시 22 자산 .wfx 저장
    if (!g_bAssetsDumped)
    {
        const wstring_t outDir = L"Client/Bin/Resource/FX/Champions/Auto/";
        u32_t saved = LegacyFx::DumpAllAssetsToWfx(
            CFxSystem::GetAssetRegistry(), outDir);
        OutputDebugStringW((L"[EFX-0] Dumped " + std::to_wstring(saved) + L" assets\n").c_str());
        g_bAssetsDumped = true;
    }
}
```

**검증**: InGame 진입 후 디렉토리 검사 — `Client/Bin/Resource/FX/Champions/Auto/` 에 22 `.wfx` 존재.

### EFX-0 합격 기준 (전체)

- [ ] Yasuo/Annie/Yone preset 의 `LegacyFx::SpawnBillboardAuto` 통과 (3 챔프 × 평균 5 preset = 15 hooks)
- [ ] `LegacyFx::DumpAllAssetsToWfx` 박제 + 헬퍼 메서드 (`GetHandleByIndex`)
- [ ] `Bin/Resource/FX/Champions/Auto/` 22 .wfx 파일 존재
- [ ] InGame 진입 시 22 effect 시각 결과 동일 (LegacyFx 통과 전후 비교 — diff 0)
- [ ] `EngineSDK/inc/FX/` 동기화 (UpdateLib.bat 자동, but 수동 검증)

---

## §3. EFX-1 잔여 — 다음 N 단계 (총 4 단계, 5 days)

### Step 1-1 — `nlohmann/json.hpp` 박제

**작업**:
1. https://github.com/nlohmann/json 에서 single-include `json.hpp` (v3.11.x) 다운로드
2. `Engine/ThirdPartyLib/nlohmann/json.hpp` 박제
3. `Engine.vcxproj` 의 AdditionalIncludeDirectories 에 `ThirdPartyLib/nlohmann` 추가
4. test build 통과

**대안**: `rapidjson` (빠르지만 macro 많음).

### Step 1-2 — `FxAssetSerializer.h/.cpp` 박제

**파일**: `Engine/Public/FX/FxAssetSerializer.h`
```cpp
#pragma once
#include "WintersAPI.h"
#include "FX/FxAsset.h"

namespace FxAssetSerializer
{
    WINTERS_ENGINE bool_t SaveToWfx(const FxAsset& asset, const wstring_t& path);
    WINTERS_ENGINE bool_t LoadFromWfx(const wstring_t& path, FxAsset& outAsset);
}
```

**구현**: `Engine/Private/FX/FxAssetSerializer.cpp` (15번 §4.3 의 의사 코드 참조).

**핵심 부분**:
- enum 문자열화 (`RenderTypeToString` / `BlendPresetToString`) + 역변환
- `FxValue` (variant) → JSON 변환 + 역변환
- 모든 `FxEmitterDesc` 필드 (50+ 필드) 직렬화

### Step 1-3 — `CFxAssetRegistry::LoadFromFile` 본체 박제

`13_EFFECT_TOOL_V3_MASTER.md §5.4` 의 의사 코드 참조 박제.

```cpp
FxAssetHandle CFxAssetRegistry::LoadFromFile(const wstring_t& path)
{
    FxAsset asset{};
    if (!FxAssetSerializer::LoadFromWfx(path, asset))
        return FxAssetHandle{};

    FxAssetHandle h = RegisterOrReplaceByName(std::move(asset));
    Slot* pSlot = ResolveSlot(h);
    if (pSlot) pSlot->path = path;
    return h;
}

bool_t CFxAssetRegistry::ReloadFromFile(FxAssetHandle handle)
{
    Slot* pSlot = ResolveSlot(handle);
    if (!pSlot || pSlot->path.empty()) return false;
    return LoadFromFile(pSlot->path).IsValid();
}

u32_t CFxAssetRegistry::LoadDirectory(const wstring_t& directoryPath)
{
    u32_t uCount = 0;
    if (!std::filesystem::exists(directoryPath))
        return 0;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(directoryPath))
    {
        if (entry.is_regular_file() && entry.path().extension() == L".wfx")
        {
            if (LoadFromFile(entry.path().wstring()).IsValid())
                ++uCount;
        }
    }
    return uCount;
}
```

### Step 1-4 — 22 자산 round-trip 검증

InGame 진입 → EFX-0 Step 0-5 가 22 자산 저장 → 다음 진입 시 `Loader::PreloadFxAssets()` 가 `LoadDirectory(L"Client/Bin/Resource/FX/Champions/Auto/")` 호출 → 22 자산 자동 로드.

**Loader 변경**:
```cpp
// Client/Private/Scene/Loader.cpp 어딘가:
void CLoader::PreloadFxAssets()
{
    CFxAssetRegistry& registry = CFxSystem::GetAssetRegistry();
    u32_t count = registry.LoadDirectory(L"Client/Bin/Resource/FX/Champions/Auto/");
    OutputDebugStringW((L"[Loader] Preloaded " + std::to_wstring(count) + L" FX assets\n").c_str());
}
```

**EFX-0 Step 0-5 의 g_bAssetsDumped 와 결합**:
- 처음 1회: `g_bAssetsDumped == false` → preset hook 시 자산화 + Step 0-5 가 dump
- 다음 실행: `LoadDirectory` 가 22 로드 → preset hook 의 `RegisterOrReplaceByName` 이 같은 이름 검출 → skip
- 결과: 22 effect 가 디스크 자산 사용

### EFX-1 합격 기준 (전체)

- [ ] `nlohmann/json.hpp` 박제 + 빌드 통과
- [ ] `FxAssetSerializer::SaveToWfx` + `LoadFromWfx` 박제
- [ ] `CFxAssetRegistry::LoadFromFile/LoadDirectory/ReloadFromFile` 본체 박제
- [ ] 22 자산 round-trip (저장 → 로드 → 저장 → diff 0)
- [ ] `Loader::PreloadFxAssets()` 박제 + InGame 진입 시 22 로드
- [ ] EngineSDK 동기화

---

## §4. EFX-2 — 다음 N 단계 (총 8 단계, 2 weeks)

### Step 2-1 — `FxLifecycle.h` 박제 (5 state enum)

[15_EFX_LIFECYCLE_AND_GRAPH_RUNTIME.md §1.1](15_EFX_LIFECYCLE_AND_GRAPH_RUNTIME.md#11-state-enum) 참조.

```cpp
// Engine/Public/FX/FxLifecycle.h
#pragma once
#include "WintersTypes.h"

constexpr u32_t FX_INVALID_INSTANCE = 0xFFFFFFFFu;

enum class eFxLifecycleState : u8_t
{
    Inactive,
    Active,
    Completing,
    Complete,
    PoolReturned,
};
```

### Step 2-2 — `FxInstanceComponent` (POD ECS) 박제

[13_EFFECT_TOOL_V3_MASTER.md §6.2](13_EFFECT_TOOL_V3_MASTER.md#62-신규-박제--component--system--instance) 참조.

```cpp
// Engine/Public/ECS/Components/FxInstanceComponent.h
#pragma once
#include "Defines.h"
#include "WintersMath.h"
#include "ECS/Entity.h"
#include "FX/FxAsset.h"
#include "FX/FxLifecycle.h"
#include <array>

struct FxInstanceComponent
{
    FxAssetHandle           hAsset{};
    EntityHandle            hAttachTo = NULL_ENTITY_HANDLE;
    Vec3                    vAttachOffset{};
    f32_t                   fAge = 0.f;
    f32_t                   fLifetime = 3.f;
    bool_t                  bLoop = false;
    bool_t                  bAutoDestroyEntity = true;
    eFxLifecycleState       state = eFxLifecycleState::Inactive;
    u32_t                   uInstanceSlot = FX_INVALID_INSTANCE;
    std::array<u32_t, 4>    aPoolIndices{};
};
```

ECS 등록: `World.cpp` 또는 ECS 자동 등록 매크로 사용.

### Step 2-3 — `CFxSystemInstance` 박제

[13_EFFECT_TOOL_V3_MASTER.md §6.2](13_EFFECT_TOOL_V3_MASTER.md#62-신규-박제--component--system--instance) 의 헤더 참조.

`Initialize` / `Tick` / `RequestComplete` / `Reset` / `LoopRestart` 박제.

### Step 2-4 — `CFxEmitterInstance` 박제

`Initialize` / `Tick` / `RequestComplete` / `IsComplete` 박제.

`Tick` 안 Spawn → Update → Cull 순서 ([15_EFX_LIFECYCLE_AND_GRAPH_RUNTIME.md §2.4](15_EFX_LIFECYCLE_AND_GRAPH_RUNTIME.md#24-cfxemitterinstancetick-내부-순서-spawn--update--cull) 참조).

### Step 2-5 — `CFxSimulationSystem` Phase 11 박제

ISystem 상속 + Execute 안 6 단계 ([15_EFX_LIFECYCLE_AND_GRAPH_RUNTIME.md §2.2](15_EFX_LIFECYCLE_AND_GRAPH_RUNTIME.md#22-cfxsimulationsystemexecute-내부-순서) 참조).

ECS 등록: `Scene_InGame::OnEnter()` 또는 별도 Bootstrap 에서 `world.AddSystem<CFxSimulationSystem>()`.

### Step 2-6 — `LegacyFx::SpawnFromAsset` → 새 시스템 통과

기존 `CFxSystem::SpawnFromAsset(world, h, pos, attachTo)` 의 내부 구현이 새 `CFxSimulationSystem::Instance().SpawnFromAsset(...)` 로 위임.

```cpp
// Client/Private/GameObject/FX/FxSystem.cpp::SpawnFromAsset
EntityID CFxSystem::SpawnFromAsset(CWorld& world, FxAssetHandle h,
                                    const Vec3& vWorldPos, EntityID attachTo)
{
    return CFxSimulationSystem::Instance().SpawnFromAsset(world, h, vWorldPos, attachTo);
}
```

기존 `CFxSystem::Update` / `Render` 는 호환 유지 (Render Snapshot 통과 또는 ECS World 직접 통과).

### Step 2-7 — `CCommandBuffer` Worker-Safe Spawn 박제

[15_EFX_LIFECYCLE_AND_GRAPH_RUNTIME.md §3](15_EFX_LIFECYCLE_AND_GRAPH_RUNTIME.md#3-worker-safe-spawn--commandbuffer-통합) 참조.

CCommandBuffer 가 이미 Engine 측 박제되어 있는지 확인 필요. 없으면 박제 (mutex protected lambda queue).

### Step 2-8 — Lifecycle 검증

- [ ] 5 state 진입 검증 — `OutputDebugString` 으로 매 transition log
- [ ] 8 단계 흐름 (15번 §1.3) 통과 — Inactive → Active → Completing → Complete → PoolReturned → 제거
- [ ] Loop 재시작 검증 — `bLoop = true` 인 자산 spawn 후 N 회 loop 동작
- [ ] xoroshiro128 deterministic 검증 — 같은 seed = 같은 sequence

### EFX-2 합격 기준 (전체)

- [ ] `FxLifecycle.h` + `FxInstanceComponent.h` 박제
- [ ] `CFxSystemInstance` / `CFxEmitterInstance` / `CFxSimulationSystem` 박제
- [ ] LegacyFx → 새 시스템 통과 + 22 effect 시각 동일
- [ ] CommandBuffer Worker-Safe Spawn 동작
- [ ] Lifecycle 8 단계 검증 통과
- [ ] xoroshiro128 deterministic 검증
- [ ] EngineSDK 동기화

---

## §5. EFX-3 잔여 — 다음 N 단계 (3 weeks)

### Step 3-1 — `FxRibbonSystem.h/.cpp` 박제

[13_EFFECT_TOOL_V3_MASTER.md §7.2](13_EFFECT_TOOL_V3_MASTER.md#72-fxribbonsystem-박제) 참조.

알고리즘:
1. `Update`:
   - attachTo 추종 → `vEndOffset` 위치 = head
   - points 배열 shift (FIFO — 새 point push, 오래된 point pop)
   - `fElapsed += dt`
2. `Render`:
   - point 쌍 → quad strip
   - UV V = age / lifetime
   - dynamic VB upload (매 frame)

### Step 3-2 — `FxGroundDecalComponent` + `FxGroundDecalSystem` 박제

[13_EFFECT_TOOL_V3_MASTER.md §7.3](13_EFFECT_TOOL_V3_MASTER.md#73-fxgrounddecalcomponent-박제) 참조.

DX11 렌더링:
- Quad on plane (XZ)
- View-projection + depth-aware
- UV: world position projection
- Grow-in: `radius * smoothstep(0, growDur, age)`
- Fade-out: `alpha * (1.0 - smoothstep(life-fadeOut, life, age))`

### Step 3-3 — `FxShockwaveComponent` + `FxShockwaveSystem` 박제

[13_EFFECT_TOOL_V3_MASTER.md §7.4](13_EFFECT_TOOL_V3_MASTER.md#74-fxshockwavecomponent-박제) 참조.

DX11 렌더링:
- Ring quad (inner radius, outer radius)
- `t = elapsed / duration`
- `inner = lerp(start, end - thick, t)` / `outer = lerp(start + thick, end, t)`
- alpha fade

### Step 3-4 — Shader 박제 (Decal / Shockwave 전용)

`Shaders/FX/` 디렉토리 신규:
```
Shaders/FX/
├── FxBillboard.hlsl      ← 기존 FxSprite 와 통합 또는 별도
├── FxRibbon.hlsl         ← 신규
├── FxBeam.hlsl           ← 기존 FxBeam 또는 별도
├── FxGroundDecal.hlsl    ← 신규
├── FxMeshParticle.hlsl   ← 기존 FxMesh
└── FxShockwave.hlsl      ← 신규
```

**의의**: 6 타입 별 셰이더 분리 — material slot 명확.

### Step 3-5 — Render Snapshot 패턴 도입 (선택)

[15_EFX_LIFECYCLE_AND_GRAPH_RUNTIME.md §6](15_EFX_LIFECYCLE_AND_GRAPH_RUNTIME.md#6-render-snapshot--sim-과-render-분리-efx-3-후반) 참조.

**우선순위 ⬇** — NEXTGEN §1 5 (RenderGraph) 진입 후 보강. EFX-3 단계는 Immediate-mode 유지 가능.

### EFX-3 합격 기준 (전체)

- [ ] `FxRibbonSystem` 박제 + Irelia Q Trail 자산을 Ribbon 으로 변경 시 동작
- [ ] `FxGroundDecalComponent + System` 박제 + Annie W (Tibbers warning decal) 적용
- [ ] `FxShockwaveComponent + System` 박제 + Annie R (Tibbers 소환 충격파) 적용
- [ ] 6 타입 모두 InGame 동작
- [ ] 셰이더 6 타입 별 분리

---

## §6. EFX-4 — 다음 N 단계 (3 weeks)

### Step 4-1 — `Scene_EffectTool.h/.cpp` skeleton

[13_EFFECT_TOOL_V3_MASTER.md §8.3](13_EFFECT_TOOL_V3_MASTER.md#83-scene_effecttool-본체) 참조.

`OnEnter` / `OnExit` / `OnUpdate` / `OnRender` / `OnImGui` 박제.

ImGui DockSpace 4 영역 split.

### Step 4-2 — Asset Browser Panel 박제

[13_EFFECT_TOOL_V3_MASTER.md §8.4](13_EFFECT_TOOL_V3_MASTER.md#84-4-패널-명세) 참조.

`std::filesystem::recursive_directory_iterator` 로 트리 구성.

### Step 4-3 — Preview Viewport Panel 박제

별도 RT (off-screen) + dynamic camera + 그리드/축.

ImGui::Image 로 RT 표시.

### Step 4-4 — Inspector Panel 박제

선택된 asset 의 모든 emitter 슬라이더.
"Apply" → save + reload.

### Step 4-5 — Timeline Panel 박제

play/pause/stop/loop + 시간 스크럽 + speed.

### Step 4-6 — Hot Reload 박제

[15_EFX_LIFECYCLE_AND_GRAPH_RUNTIME.md §4](15_EFX_LIFECYCLE_AND_GRAPH_RUNTIME.md#4-hot-reload-sequence-diagram-efx-4) 참조.

0.5s 주기로 mtime 검사.

### Step 4-7 — Game Select 통합

`Scene_GameSelect` (또는 `Scene_MainMenu`) 에 "Editor → Effect Tool" 버튼 추가.

### Step 4-8 — EffectTuner 정리

`Client/Private/UI/EffectTuner.cpp` 의 Irelia 7 hardcode 정리:
- 옵션 1: `#if 0` 으로 deprecated 표시
- 옵션 2: 완전 제거 (Scene_EffectTool 가 대체)

### EFX-4 합격 기준 (전체)

- [ ] `Scene_EffectTool` Game Select 진입 가능
- [ ] 4 패널 모두 동작
- [ ] Asset Browser 가 22 자산 (또는 이후 N 자산) 표시
- [ ] Preview Viewport 가 선택 자산 미리보기
- [ ] Inspector Apply → 저장 + reload
- [ ] Timeline play/pause/loop/speed 동작
- [ ] Hot Reload 동작 (외부 에디터 .wfx 수정 → 자동 반영)
- [ ] EffectTuner 정리

---

## §7. EFX-5 — 다음 N 단계 (3 weeks)

### Step 5-1 — `FxNodeRegistry` + 30+ 표준 노드

[13_EFFECT_TOOL_V3_MASTER.md §9.1](13_EFFECT_TOOL_V3_MASTER.md#91-노드-라이브러리-30-표준-노드) 참조.

```cpp
// Engine/Public/FX/FxNodeRegistry.h
class CFxNodeRegistry
{
public:
    struct NodeMetadata
    {
        std::string strType;
        std::string strCategory;     // "Spawn", "Init Position", ...
        std::vector<std::string> inputPinTypes;    // "Float", "Vec3", "ParameterRef", ...
        std::vector<std::string> outputPinTypes;
    };

    void RegisterNode(NodeMetadata md);
    const NodeMetadata* Find(std::string_view strType) const;
    std::vector<const NodeMetadata*> ListByCategory(std::string_view strCategory) const;

private:
    std::unordered_map<std::string, NodeMetadata> m_Nodes;
};
```

표준 노드 30+ 등록 (Spawn / InitPosition × 4 / InitVelocity × 3 / InitColor / Update × 8 / Math × 12).

### Step 5-2 — `FxExpressionVM.h/.cpp` 박제

[13_EFFECT_TOOL_V3_MASTER.md §9.2](13_EFFECT_TOOL_V3_MASTER.md#92-expression-vm-바이트코드) 참조.

50+ opcode 박제.

`Execute` 메인 loop + opcode 별 핸들러.

### Step 5-3 — `FxNodeCompiler.h/.cpp` 박제

[15_EFX_LIFECYCLE_AND_GRAPH_RUNTIME.md §5.2-5.5](15_EFX_LIFECYCLE_AND_GRAPH_RUNTIME.md#5-graph-runtime--node-execution-flow-efx-5) 참조.

`CompileEmitter` + `EmitNode` + `TopologicalSort`.

### Step 5-4 — Inspector 의 노드 그래프 패널 박제

`Client/Public/UI/EffectToolPanels/NodeGraphPanel.h` 신규.

ImGui 기반 노드 에디터 (`imgui-node-editor` 라이브러리 사용 또는 자체 박제).

### Step 5-5 — 22 자산 중 Irelia Q Trail 노드 그래프 박제

기존 LegacyFx 의 hardcoded 자산은 노드 0개. Q Trail 을 그래프로 표현 검증:
- Node 1: SpawnRate (rate=30/s)
- Node 2: InitPosition (Emitter.Position + offset)
- Node 3: InitVelocity (random cone)
- Node 4: InitColor (User.TintColor)
- Node 5: UpdateLifetime (delta=System.DeltaTime)

`CompileEmitter` 가 이 5 노드를 spawn/update bytecode 로 변환 + 시각 결과 = legacy 동일 검증.

### EFX-5 합격 기준 (전체)

- [ ] 30+ 표준 노드 + Registry 박제
- [ ] CFxNodeCompiler 박제 + 위상 정렬 검증 + 순환 검출
- [ ] CFxExpressionVM 50+ opcode 박제 + 단위 테스트
- [ ] EFX-4 Inspector 노드 그래프 패널 박제
- [ ] Irelia Q Trail 노드 그래프 표현 + 시각 결과 검증

---

## §8. EFX-6 — 다음 N 단계 (2 weeks)

[12_EFFECT_TOOL_NIAGARA_V2_MASTER.md §9](12_EFFECT_TOOL_NIAGARA_V2_MASTER.md#9-efx-6-elden-vfx-pack--검증-작품-6-종) 참조.

6 작품 박제:
1. **Boss Telegraph** — GroundDecal + Shockwave
2. **Sword Trail** — Ribbon
3. **Shockwave** — Shockwave + Mesh
4. **Magic Circle** — GroundDecal + Beam
5. **Lingering Field** — GroundDecal + Billboard
6. **Soul Liberation** — Beam + Ribbon + Mesh

자산 위치: `Bin/Resource/FX/Bosses/`, `Bin/Resource/FX/Spells/`, `Bin/Resource/FX/ClassServant/`.

각 작품 EFX-4 Tool 에서 박제 + 시각 검증.

---

## §9. EFX-7 — 보류 (RHI/RG 안정 후)

[12_EFFECT_TOOL_NIAGARA_V2_MASTER.md §10](12_EFFECT_TOOL_NIAGARA_V2_MASTER.md#10-efx-7-gpu-compute-보류) 참조.

진입 조건 (NEXTGEN_FRAMEWORK_MASTER §1 7 단계 통과):
- ✅ ECS Generation v1
- ✅ Worker-Safe CommandBuffer
- ✅ SystemAccess
- ✅ Fiber M3 (yield + wait list)
- ✅ RenderGraph
- ✅ GPU Driven Pipeline (GPUScene + DrawIndexedIndirect)

GPU Compute + Indirect Draw 박제.

CPU 대비 1M+ particle 동시 처리.

---

## §10. PITFALLS GATE 통과 체크리스트 (박제 진입 시 의무)

EFX-0/1/2 박제 진입 직전 다음 8 GATE 통과 확인:

| GATE | 검증 내용 | EFX-0 | EFX-1 | EFX-2 | EFX-3 | EFX-4 | EFX-5 |
|---|---|---|---|---|---|---|---|
| **A 사실 수집** | 인용 + grep 결과 | ✅ §1.1-1.7 | ✅ §3 | ✅ §4 | ✅ §5 | ✅ §6 | ✅ §7 |
| **B TODO 0** | 합격 기준 명시 | ✅ §2 끝 | ✅ §3 끝 | ✅ §4 끝 | ✅ §5 끝 | ✅ §6 끝 | ✅ §7 끝 |
| **C 호출 경로 grep** | 의존 호출 검증 | ✅ Yasuo/Annie/Yone | ✅ Loader 진입 | ✅ ECS 등록 | ✅ ECS Render | ✅ Game Select | ✅ Inspector |
| **D ECS 책임** | POD 유지 + Snapshot | ✅ Component POD | — | ✅ FxInstanceComponent | ✅ Render Snapshot | — | — |
| **E 향후 자료형** | Handle / Variant | ✅ FxAssetHandle | ✅ FxValue | ✅ EntityHandle | ✅ Render State | — | ✅ FxParameterID |
| **F Scheduler** | Phase + Access | — | — | ✅ Phase 11 + DescribeAccess | — | — | — |
| **G Owner Scope** | Tier-N | ✅ Registry Tier-1 | — | ✅ System Tier-2 | — | ✅ Scene Tier-3 | — |
| **H 인용 + include** | subdir 보존 | ✅ "FX/FxAsset.h" | ✅ "FX/FxAssetSerializer.h" | ✅ "ECS/Components/FxInstanceComponent.h" | — | ✅ "Scene/Scene_EffectTool.h" | ✅ "FX/FxNodeRegistry.h" |

각 GATE 미통과 시 [`PLAN_AUTHORING_PITFALLS.md`](../../process/PLAN_AUTHORING_PITFALLS.md) 의 P-1~P-19 참조.

---

## §11. 박제 진입 명령 — 단일 task 로 시작

```
"EFX-0 잔여 박제 진입.

선행:
- Engine 빌드 통과 (Debug|x64)
- EngineSDK 동기화 정상

Step 0-1: Yasuo preset LegacyFx 통과
  - YasuoFxPresets.cpp 의 모든 CFxSystem::Spawn 호출을 LegacyFx::SpawnBillboardAuto 로 치환
  - 합격: InGame Yasuo Q/W/E/R/BA 시각 동일

Step 0-2: Annie preset LegacyFx 통과 (Yasuo 와 동일 패턴)

Step 0-3: Yone Yone_Skills.cpp 의 직접 Spawn 도 LegacyFx 통과

Step 0-4: LegacyFx::DumpAllAssetsToWfx + GetHandleByIndex 박제

Step 0-5: Scene_InGame::OnEnter 의 g_bAssetsDumped flag + 첫 실행 시 22 .wfx 저장

검증:
- Bin/Resource/FX/Champions/Auto/ 22 .wfx 존재
- InGame 진입 시 22 effect 시각 동일 (LegacyFx 통과 전후 비교 diff 0)

진입 전 PITFALLS GATE A~H 통과 (§10)."
```

---

**END OF EFX PROGRESS AND NEXT ACTIONS**
