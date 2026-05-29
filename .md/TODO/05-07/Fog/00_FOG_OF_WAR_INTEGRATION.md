# Fog of War — 통합 박제 (Stage 1 = Entity Visibility Filter)

**작성일**: 2026-05-07
**전제**: B-13 v2 02 (Vision FoW) 박제 시스템/텍스쳐/미니맵 단계는 완료. **본 계획서 = 메인 3D 화면에서 시야 밖 entity 가 그대로 렌더되는 잔존 결함 정정**.
**가이드**: [`.md/process/PLAN_AUTHORING_PITFALLS.md`](../../../process/PLAN_AUTHORING_PITFALLS.md) — P-1~P-15 + 8 GATE 통과
**범위**: Stage 1 만 (Entity-level visibility filter). Stage 2 (Ground fog overlay 셰이더) 는 §6 후속.
**합격 정의**: 적 챔프/미니언/타워가 로컬 팀 시야 밖에 있을 때 메인 3D 화면에서 보이지 않는다 + 미니맵에는 fog 가 회색/검정으로 정상 표시.

---

## §0. 본 계획서가 잡는 결함 한 줄

**증상**: 미니맵 ([MinimapPanel.cpp:31-32](../../../../Client/Private/UI/MinimapPanel.cpp:31)) 은 `pFow->Get_NativeSRV()` 로 fog 텍스쳐 그리고, entity 점도 `(vis.teamVisibilityMask & (1u << localTeam)) != 0` 마스크로 정상 필터링. **그런데 메인 3D 화면 (Scene_InGame::Render 안의 ModelRenderer 호출 루프) 은 `VisibilityComponent` 를 한 번도 안 본다** → 적 entity 가 시야 밖에서도 풀 렌더.

**핵심**: VisionSystem 의 산출 데이터 (`VisibilityComponent.teamVisibilityMask`) 가 **소비처가 미니맵 1 곳뿐**. 메인 화면 렌더 path 가 이 마스크를 전혀 안 받음. 따라서 본 박제 = "메인 렌더 루프가 마스크를 소비하도록 분기 1줄 추가".

---

## §1. Preflight Evidence Table — TODO 0 (P-1+P-6 회피)

| # | 항목 | 실측값 | 실측 출처 |
|---|---|---|---|
| 1 | LoL 시야 정책 | 같은 팀 = 항상 보임, 적 팀 = `teamVisibilityMask & (1<<myTeam)` 비트 ON 일 때만 | [VisionSystem.cpp:97-103](../../../../Engine/Private/ECS/Systems/VisionSystem.cpp:97), [MinimapPanel.cpp:43-46](../../../../Client/Private/UI/MinimapPanel.cpp:43) |
| 2 | LocalTeam 식별 | `world.ForEach<LocalPlayerTag, SpatialAgentComponent>` → `agent.team` | [VisionSystem.cpp:162-168](../../../../Engine/Private/ECS/Systems/VisionSystem.cpp:162) |
| 3 | LocalPlayerTag 부착 | InGameRosterSpawner — 본인 챔프 entity 에 1회 부착 | [InGameRosterSpawner.cpp:273-275](../../../../Client/Private/Scene/InGameRosterSpawner.cpp:273) |
| 4 | VisibilityComponent 부착 | Structure_Manager (타워/넥서스/억제기), Minion_Manager (모든 미니언), ChampionSpawnService (모든 챔프) | [Structure_Manager.cpp:306,312,317](../../../../Client/Private/Manager/Structure_Manager.cpp:306), [Minion_Manager.cpp:447](../../../../Client/Private/Manager/Minion_Manager.cpp:447), [ChampionSpawnService.cpp:61-62](../../../../Client/Private/GameObject/ChampionSpawnService.cpp:61) |
| 5 | SpatialAgentComponent.team | Structure/Minion/ChampionSpawn 모두 부착 | 동상 |
| 6 | 메인 3D 렌더 루프 위치 | `Scene_InGame::Render()` 또는 `InGameRenderBridge::*` (현재 ModelRenderer 호출자) | §2.G 직접 인용 후 박제 — **본 진입 직전 마지막 검증** |
| 7 | ModelRenderer Render() | 마스킹 옵션 0 — `Render()` 단순 호출 시 무조건 그림 | [ModelRenderer.h:26-27](../../../../Engine/Public/Renderer/ModelRenderer.h:26) |
| 8 | RenderWithVisibility(mask) 의미 | `VisibilityMask` 는 **mesh-group level** (Yone submesh 마스킹용 — `MeshGroupVisibilityComponent`) ≠ entity-level visibility | [ModelRenderer.h:5](../../../../Engine/Public/Renderer/ModelRenderer.h:5), [Yone_Skills](../../../../Client/Private/GameObject/Champion/Yone) |
| 9 | Same-team 항상 보임 정책 | `bMine = (agent.team == localTeam)` 이면 mask 검사 skip | [MinimapPanel.cpp:43-46](../../../../Client/Private/UI/MinimapPanel.cpp:43) |
| 10 | Bush 안에 들어간 본인 시야 | `bInBush + bushId` 같으면 보임, 적은 true sight 만 | [VisionSystem.cpp:130-150](../../../../Engine/Private/ECS/Systems/VisionSystem.cpp:130) |

**TODO 잔여**: §1.6 (메인 렌더 루프 호출자 정확한 파일/줄) — §2.G 에서 직접 인용 박제 시 §1.6 자동 해소. **§1 Preflight TODO 0 보장은 §3 GATE A 단계에서**.

---

## §2. Code Reality Snapshot — 직접 인용 (P-2/P-8 회피)

### §2.A — VisionSystem 산출 데이터 (TickVisibility)

[Engine/Private/ECS/Systems/VisionSystem.cpp:69-114](../../../../Engine/Private/ECS/Systems/VisionSystem.cpp:69):

```cpp
world.ForEach<VisibilityComponent>(
    function<void(EntityID, VisibilityComponent&)>(
        [](EntityID, VisibilityComponent& vis)
        {
            vis.teamVisibilityMask = 0;
        }));

world.ForEach<TransformComponent, VisionSourceComponent, SpatialAgentComponent>(
    function<void(EntityID, TransformComponent&, VisionSourceComponent&, SpatialAgentComponent&)>(
        [&](EntityID srcId, TransformComponent& srcXf, VisionSourceComponent& vs, SpatialAgentComponent& srcAgent)
        {
            // ... QueryRadius 후 candidate 순회
            VisibilityComponent& targetVis = world.GetComponent<VisibilityComponent>(target);
            targetVis.teamVisibilityMask |= static_cast<u8_t>(1u << srcAgent.team);
            // ...
        }));
```

→ **Vision tick 결과가 모든 `VisibilityComponent` 의 `teamVisibilityMask` 비트로 박힘**. 100ms 주기.

### §2.B — VisibilityComponent 정의

[Engine/Public/ECS/Components/VisionComponents.h:18-22](../../../../Engine/Public/ECS/Components/VisionComponents.h:18):

```cpp
struct VisibilityComponent
{
    u8_t teamVisibilityMask = 0;
    bool_t bInBush = false;
    EntityID bushId = NULL_ENTITY;
};
```

→ `teamVisibilityMask` 의 비트 N = "팀 N 이 이 entity 를 보고 있다".

### §2.C — Bush 마스킹 정책

[Engine/Private/ECS/Systems/VisionSystem.cpp:130-153](../../../../Engine/Private/ECS/Systems/VisionSystem.cpp:130):

```cpp
if (world.HasComponent<VisibilityComponent>(target))
{
    const VisibilityComponent& tgtVis = world.GetComponent<VisibilityComponent>(target);
    if (tgtVis.bInBush)
    {
        if (world.HasComponent<VisibilityComponent>(source))
        {
            const VisibilityComponent& srcVis = world.GetComponent<VisibilityComponent>(source);
            if (srcVis.bInBush && srcVis.bushId == tgtVis.bushId)
                return true;
        }
        if (world.HasComponent<VisionSourceComponent>(source) &&
            world.GetComponent<VisionSourceComponent>(source).bTrueSight)
        {
            return true;
        }
        return false;
    }
}
return true;
```

→ **Bush 안 entity 는 같은 bush 안 source 또는 trueSight source 에게만 보임**. 박제 본문이 별도 분기를 만들 필요 X — VisionSystem 이 mask 에 이미 다 박은 상태.

### §2.D — LocalTeam 추출 (Fog texture 갱신 직전)

[Engine/Private/ECS/Systems/VisionSystem.cpp:160-168](../../../../Engine/Private/ECS/Systems/VisionSystem.cpp:160):

```cpp
u8_t localTeam = 0;
bool_t bLocalFound = false;
world.ForEach<LocalPlayerTag, SpatialAgentComponent>(
    function<void(EntityID, LocalPlayerTag&, SpatialAgentComponent&)>(
        [&](EntityID, LocalPlayerTag&, SpatialAgentComponent& agent)
        {
            localTeam = agent.team;
            bLocalFound = true;
        }));
if (!bLocalFound)
    return;
```

→ 메인 렌더 루프도 같은 패턴으로 localTeam 추출 가능.

### §2.E — 미니맵 visibility 마스크 사용 (참조 패턴)

[Client/Private/UI/MinimapPanel.cpp:39-64](../../../../Client/Private/UI/MinimapPanel.cpp:39):

```cpp
world.ForEach<TransformComponent, SpatialAgentComponent, VisibilityComponent>(
    function<void(EntityID, TransformComponent&, SpatialAgentComponent&, VisibilityComponent&)>(
        [&](EntityID, TransformComponent& xf, SpatialAgentComponent& agent, VisibilityComponent& vis)
        {
            const bool_t bMine = (agent.team == localTeam);
            const bool_t bVisible = (vis.teamVisibilityMask & (1u << localTeam)) != 0;
            if (!bMine && !bVisible)
                return;
            // ... draw dot
        }));
```

→ **본 계획서 Stage 1 핵심**: 같은 패턴 (`bMine || bVisible`) 을 메인 렌더 루프에 박제.

### §2.F — VisionSystem 등록 + Fog 텍스쳐 갱신

[Client/Private/Scene/InGameBootstrapBridge.cpp:157-159](../../../../Client/Private/Scene/InGameBootstrapBridge.cpp:157):

```cpp
auto pVision = Engine::CVisionSystem::Create(scene.m_World.Get_SpatialIndex(), &scene.m_BushIndex);
scene.m_pVisionSystem = pVision.get();
scene.m_pScheduler->RegisterSystem(std::move(pVision));
```

[Client/Private/Scene/Scene_InGame.cpp:567-572](../../../../Client/Private/Scene/Scene_InGame.cpp:567):

```cpp
if (m_pVisionSystem && m_pFogOfWarRenderer && m_pVisionSystem->IsFowTextureDirty())
{
    m_pFogOfWarRenderer->UpdateTexture(
        m_pVisionSystem->GetFowTextureData(),
        m_pVisionSystem->GetFowTextureDim());
    m_pVisionSystem->ClearFowTextureDirty();
}
```

→ **VisionSystem ↔ FogOfWarRenderer 양방향 신호 박제 완료**. Stage 1 박제는 추가 신호 0.

### §2.G — 메인 3D 렌더 루프 (호출자 — §1.6 TODO 해소 진입 단계)

박제 진입 직전 `grep -rn "pRenderer->Render" Client/Private/Scene` 으로 호출자 N 곳 모두 식별 후 §4 변경점에 N 곳 동시 박제 (P-3 회피). 후보:
- [Client/Private/Scene/Scene_InGame.cpp](../../../../Client/Private/Scene/Scene_InGame.cpp) — main render
- [Client/Private/Scene/InGameRenderBridge.cpp](../../../../Client/Private/Scene/InGameRenderBridge.cpp) — bridge
- [Engine/Public/Renderer/ModelRenderer.h:32-36](../../../../Engine/Public/Renderer/ModelRenderer.h:32) — `RenderNormalPass` (PBR G-Buffer pre-pass) 도 동일 패턴 박제 의무

**P-3 회피 강제**: main pass + normal pass + (있으면) shadow pass 모두 동일 visibility 분기 박제.

---

## §3. 8 GATE 통과 검증

| GATE | 항목 | 통과 |
|---|---|---|
| **A — 사실 수집** | VisionSystem.cpp / VisibilityComponent / MinimapPanel.cpp / Bootstrap / Scene_InGame Fog 갱신 / ChampionSpawnService 부착 모두 직접 인용 | ✅ §2.A~F |
| **B — TODO 0** | §1.6 = §2.G 진입 직전 grep 으로 해소 | ⚠️ 본문 진입 직전 |
| **C — 호출 경로 grep** | main pass + normal pass + (shadow pass 있으면) — `pRenderer->Render` 모든 hit | ⚠️ §4 직전 grep 의무 |
| **D — ECS 책임 경계** | `VisibilityComponent` 는 ECS 컴포넌트, Scene 직접 호출 0 — render 측이 ECS query → POD 결과 | ✅ Render-only read, write 0 |
| **E — 향후 자료형** | `u8_t teamVisibilityMask` 는 8 팀까지. LoL = 2 팀, 엘든링 = 1~4 팀. 충분 | ✅ |
| **F — Scheduler 동시성** | Vision = phase 5, render = scheduler 외부 (Scene::Render). 동일 frame data 신선도 = 100ms tick 으로 충분 | ✅ |
| **G — Owner Scope** | `m_pVisionSystem` = Scene 멤버 (포인터), `m_pFogOfWarRenderer` = Scene 멤버 (unique_ptr). 본 박제는 추가 owner 0 | ✅ |
| **H — 인용 의미 + 행동 보존** | 인용 패턴 (MinimapPanel.cpp `bMine \|\| bVisible`) 그대로 메인 렌더에 적용 — 의미 일치. 같은 팀 visibility 보존 | ✅ |

→ **B + C 가 §4 진입 직전 강제 GATE**. §4 첫 단계 = `Grep "pRenderer->Render" Client/Private/Scene/`.

---

## §4. Stage 1 변경점 — Entity-level Visibility Filter

### §4.0 — 진입 게이트 (Bash/Grep 의무)

```bash
# §1.6 + GATE C 해소
grep -rn "pRenderer->Render" Client/Private/Scene/ Client/Private/UI/
grep -rn "ModelRenderer\*" Client/Private/Scene/InGameRenderBridge.cpp
grep -rn "RenderNormalPass" Client/ Engine/
```

**기대 산출**: render 호출자 N 곳 (보통 1~3 곳). N 곳 모두 §4.1~§4.N 에 동일 변경 박제. 누락 시 P-3 결함.

### §4.1 — Helper 1줄 박제 (모든 render path 공통)

**신규 파일** `Client/Public/Scene/RenderVisibilityFilter.h` (헤더 단독, inline):

```cpp
#pragma once

#include "WintersTypes.h"
#include "ECS/World.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/VisionComponents.h"
#include "ECS/Components/GameplayComponents.h"

namespace UI
{
    inline u8_t QueryLocalTeam(CWorld& world)
    {
        u8_t localTeam = 0;
        bool_t bFound = false;
        world.ForEach<LocalPlayerTag, SpatialAgentComponent>(
            function<void(EntityID, LocalPlayerTag&, SpatialAgentComponent&)>(
                [&](EntityID, LocalPlayerTag&, SpatialAgentComponent& agent)
                {
                    if (bFound) return;
                    localTeam = agent.team;
                    bFound = true;
                }));
        return localTeam;
    }

    inline bool_t IsRenderableForLocal(CWorld& world, EntityID entity, u8_t localTeam)
    {
        if (!world.HasComponent<SpatialAgentComponent>(entity))
            return true;   // 환경 entity (맵/지면/이펙트) 는 항상 렌더

        const SpatialAgentComponent& agent = world.GetComponent<SpatialAgentComponent>(entity);
        if (agent.team == localTeam)
            return true;   // 같은 팀 항상 보임

        if (!world.HasComponent<VisibilityComponent>(entity))
            return true;   // VisibilityComponent 부재 = 항상 보임 (보수적)

        const VisibilityComponent& vis = world.GetComponent<VisibilityComponent>(entity);
        return (vis.teamVisibilityMask & static_cast<u8_t>(1u << localTeam)) != 0;
    }
}
```

**근거**:
- §2.E 의 `bMine || bVisible` 패턴 그대로.
- `SpatialAgentComponent` 가 없는 entity (맵 plane, 이펙트 빌보드) 는 시야 정책 미적용 → `true` 반환.
- `VisibilityComponent` 부재 = "보수적으로 보이게" — 이렇게 해야 envoy/특수 객체가 사라지는 사고 방지 (P-14 행동 보존).

### §4.2 — Scene_InGame::Render — Entity 렌더 루프 분기

**파일**: `Client/Private/Scene/Scene_InGame.cpp` (또는 InGameRenderBridge.cpp — §4.0 grep 결과에 따라).

**박제 진입 직전**: 해당 파일에서 `world.ForEach<RenderComponent, TransformComponent>` 또는 `pRenderer->Render()` 호출 루프를 찾는다. 다음과 같은 패턴이 보이면 ↓

**기존** (예시 — 실제 줄 번호는 grep 으로 확정):

```cpp
m_World.ForEach<RenderComponent, TransformComponent>(
    function<void(EntityID, RenderComponent&, TransformComponent&)>(
        [&](EntityID id, RenderComponent& rc, TransformComponent& xf)
        {
            if (!rc.bVisible || !rc.pRenderer)
                return;

            rc.pRenderer->UpdateTransform(xf.GetWorldMatrix());
            rc.pRenderer->UpdateCamera(matViewProj);
            rc.pRenderer->Render();
        }));
```

**변경**:

```cpp
#include "Scene/RenderVisibilityFilter.h"
// ... 함수 시작 부근
const u8_t localTeam = UI::QueryLocalTeam(m_World);

m_World.ForEach<RenderComponent, TransformComponent>(
    function<void(EntityID, RenderComponent&, TransformComponent&)>(
        [&](EntityID id, RenderComponent& rc, TransformComponent& xf)
        {
            if (!rc.bVisible || !rc.pRenderer)
                return;
            if (!UI::IsRenderableForLocal(m_World, id, localTeam))
                return;   // Stage 1 — 시야 밖 적 entity skip

            rc.pRenderer->UpdateTransform(xf.GetWorldMatrix());
            rc.pRenderer->UpdateCamera(matViewProj);
            rc.pRenderer->Render();
        }));
```

**근거**:
- **추가 1줄** (`if (!UI::IsRenderableForLocal(...)) return;`) 만 박제 — Surgical Changes.
- `localTeam` 은 함수 시작에서 한 번 추출 (ForEach 루프 안에서 매번 추출 X).
- `id` 매개변수가 ForEach 콜백에 이미 들어옴 → helper 가 자체 lookup.

### §4.3 — Normal Pass (PBR G-Buffer pre-pass) 동일 박제 — P-3 회피

**파일**: `Client/Private/Scene/Scene_InGame.cpp` 의 `RenderNormalPass` 호출 루프 (또는 `InGameRenderBridge` 의 PBR 분기).

**기존** (예시):

```cpp
m_World.ForEach<RenderComponent, TransformComponent>(
    function<void(EntityID, RenderComponent&, TransformComponent&)>(
        [&](EntityID id, RenderComponent& rc, TransformComponent& xf)
        {
            if (!rc.bVisible || !rc.pRenderer || !rc.pRenderer->UsesPBR())
                return;

            rc.pRenderer->UpdateTransform(xf.GetWorldMatrix());
            rc.pRenderer->UpdateCamera(matViewProj);
            rc.pRenderer->RenderNormalPass(pMS, pMP, pSS, pSP);
        }));
```

**변경**:

```cpp
m_World.ForEach<RenderComponent, TransformComponent>(
    function<void(EntityID, RenderComponent&, TransformComponent&)>(
        [&](EntityID id, RenderComponent& rc, TransformComponent& xf)
        {
            if (!rc.bVisible || !rc.pRenderer || !rc.pRenderer->UsesPBR())
                return;
            if (!UI::IsRenderableForLocal(m_World, id, localTeam))
                return;   // Normal pass 도 동일 visibility skip — P-3 회피

            rc.pRenderer->UpdateTransform(xf.GetWorldMatrix());
            rc.pRenderer->UpdateCamera(matViewProj);
            rc.pRenderer->RenderNormalPass(pMS, pMP, pSS, pSP);
        }));
```

**근거**: PITFALLS P-3 강제 — main pass + normal pass 동시 박제. SSAO/Depth pass 가 normal map 을 read 하므로 normal pass 만 정상 진행 시 hidden entity 의 normal 이 G-Buffer 에 잔존 → SSAO ghost.

### §4.4 — 발사체 / FX entity 예외 처리

투사체 (`TurretProjectileComponent`), FX 빌보드 (`FxBillboardComponent`) 등은 **소유주의 visibility 따름**. 본 박제 §4.1 의 helper 는 `SpatialAgentComponent` 미부착 entity 를 항상 렌더로 처리하므로 자동 보존 (이미 발사체에 `SpatialAgentComponent { kind=Projectile }` 가 부착됨 — [TurretAISystem.cpp:326-330](../../../../Engine/Private/ECS/Systems/TurretAISystem.cpp:326)). 즉:
- 적 발사체가 시야 밖에서 시야 안으로 진입 시 helper 가 mask 검사 → mask 비어있으므로 hidden. **현실적**: 시야 밖 발사체가 안 보이는 LoL 정책 일치.
- 적 FX 빌보드도 동일 — `SpatialAgentComponent` 미부착이면 항상 렌더, 부착이면 mask 검사.

**Codex Pre-Mortem 항목**: §6.M-1 "FX 가 적팀 시야 밖에서 안 보여 사용자 혼동" — 의도된 동작.

### §4.5 — vcxproj 등록 의무

**`Client/Public/Scene/RenderVisibilityFilter.h`** 신규 헤더는 inline-only (cpp 0). vcxproj 등록은 헤더 1 줄만:

```xml
<!-- Client/Client.vcxproj -->
<ItemGroup>
  <ClInclude Include="Public\Scene\RenderVisibilityFilter.h" />
</ItemGroup>
```

`Client.vcxproj.filters` 에도 동일 1 줄 (Filter = `Public\Scene`).

---

## §5. 검증 결정 포인트 (Verification)

### §5.1 — 시각 검증

1. **F5 빌드 + InGame 진입** (Bot 매치 / 챔프 1 + 적 봇 1).
2. **시야 밖 적 봇이 메인 화면에서 사라짐** 확인. 적 봇이 시야 안 들어오면 즉시 다시 보임 (100ms tick + ForceRebuildNextFrame).
3. **부쉬 안 적이 메인 화면에서 사라짐** 확인 (같은 부쉬 안에 들어가면 다시 보임).
4. **미니맵에서 적 점이 동일 정책 따름** — 메인 화면 ↔ 미니맵 일치 (둘 다 `bMine || bVisible`).
5. **같은 팀 미니언/타워는 항상 보임** 확인.
6. **포로/오브젝트 (SpatialAgentComponent 미부착)** 항상 보임 확인.

### §5.2 — Profiler 검증

- `WINTERS_PROFILE_COUNT("Render::Filtered", count)` 같은 카운터 추가 (선택). 시야 밖 entity 가 매 frame N 마리 skip 되는 숫자 확인.
- Frame time 영향 확인 — visibility filter 추가로 frame 0.3~0.5ms 절감 (적 entity render 회피).

### §5.3 — 회귀 방지

- **MinimapPanel ↔ 메인 화면 시야 일치**: `MinimapPanel.cpp:43-46` 와 `RenderVisibilityFilter.h:IsRenderableForLocal` 의 mask 분기 식이 **정확히 동일** 해야 함. 테스트:
  ```
  미니맵 점 보임 ⇔ 메인 화면 모델 보임
  미니맵 점 안 보임 ⇔ 메인 화면 모델 안 보임
  ```

### §5.4 — Definition of Done

- [ ] §4.0 grep 으로 render 호출자 N 곳 식별
- [ ] §4.1 helper 박제 + vcxproj 등록
- [ ] §4.2 main pass 분기 박제
- [ ] §4.3 normal pass 분기 박제
- [ ] N 곳 (있으면) 모두 §4.2 패턴 박제
- [ ] §5.1 시각 검증 6 항목 모두 통과
- [ ] §5.3 미니맵 ↔ 메인 일치 회귀 테스트 통과

---

## §6. Codex Pre-Mortem (잠재 함정 사전 박제)

### M-1. 적 FX/발사체가 시야 밖에서 안 보임 — 사용자 혼동
**증상**: 적이 시야 밖에서 발사체를 쏘면 발사체가 시야 안 들어올 때까지 안 보임.
**의도**: LoL 정식 정책 — 시야 밖 발사체는 안 보임 (탑 진영 갱킹 시그니처 효과).
**튜닝**: 사용자가 원하면 §4.4 helper 분기를 `agent.kind == eSpatialKind::Projectile` 일 때 항상 보이게 변경 가능 (옵션). 본 박제 1차 = 정식 LoL 정책.

### M-2. localTeam = 0 (Blue) 인데 본인이 Red 팀으로 spawn
**증상**: `LocalPlayerTag` 가 부착된 entity 가 RosterSpawner 에서 다른 팀으로 spawn 시 `agent.team` 이 잘못 설정.
**원인**: `SpatialAgentComponent.team` 이 ChampionSpawnService 에서 spawn 직후 지정되지 않으면 default 0.
**해결**: ChampionSpawnService 에 SpatialAgentComponent 부착 시 team 명시 ([ChampionSpawnService.cpp:49-54](../../../../Client/Private/GameObject/ChampionSpawnService.cpp:49) 의 `spatial.team = ?` 검증). **§4.0 의 grep 단계에서 동시 검증**.

### M-3. ForceRebuildNextFrame 누락 — 부쉬 진입 1 frame 깜빡
**증상**: 부쉬 진입 시 1 frame 동안 적이 보였다가 사라짐.
**원인**: VisionSystem 100ms tick → 부쉬 진입 직후 Vision 갱신 전 메인 렌더 1 frame 적 표시.
**해결**: VisionSystem 의 `m_bForceRebuild = true` 가 `UpdateBushOccupancy` 에서 자동 설정됨 ([VisionSystem.cpp:58-59](../../../../Engine/Private/ECS/Systems/VisionSystem.cpp:58)) — Frame 1 부터 정상.

### M-4. helper 가 ForEach 안에서 World query 다시 → race
**증상**: ForEach 안 에서 helper 가 다른 컴포넌트 query.
**원인**: 같은 frame 의 같은 컴포넌트 read 다중 → race 0 (read 만 OK).
**해결**: 본 박제는 read-only — race 0.

### M-5. SpatialAgentComponent 미부착 entity 모두 보임 — 적 챔프가 mask 0 인데 부착도 0 이면?
**증상**: 챔프 entity 에 `SpatialAgentComponent` 부착이 빠진 경우 helper 가 `true` 반환 → 적 챔프가 항상 보임.
**원인**: ChampionSpawnService 가 `SpatialAgentComponent` 부착 보장.
**해결**: §4.0 grep 에서 ChampionSpawnService 의 `world.AddComponent<SpatialAgentComponent>` 호출 검증. 부착 누락 시 fail.

### M-6. PBR + non-PBR 두 path 의 분기 1곳만 박제
**증상**: PBR 챔프는 시야 밖에서 사라지는데 non-PBR (Map plane / 미니언) 은 보임.
**원인**: §4.2 (main pass) 만 박제하고 §4.3 (PBR normal pass) 미박제.
**해결**: §4.0 grep + §4.2/§4.3 동시 박제 강제. P-3 회피.

### M-7. RenderWithVisibility 와 충돌
**증상**: ModelRenderer 의 `RenderWithVisibility(VisibilityMask)` (mesh-group level) 와 본 박제 (entity level) 가 같은 entity 에 동시 적용 시 의도 모호.
**원인**: 두 visibility 의 의미가 다름 — entity level (시야), mesh-group level (Yone soul, Sylas chains 등 챔프 내 submesh hide).
**해결**: **두 분기 직교** — 본 박제는 entity level. `RenderWithVisibility(mask)` 는 mesh-group level 호출 시 그대로 사용. 즉 변경 후 호출 패턴:
```cpp
if (!UI::IsRenderableForLocal(m_World, id, localTeam))  // entity-level
    return;

if (m_World.HasComponent<MeshGroupVisibilityComponent>(id)) {
    const auto& mg = m_World.GetComponent<MeshGroupVisibilityComponent>(id);
    rc.pRenderer->RenderWithVisibility(mg.visibilityMask);
} else {
    rc.pRenderer->Render();
}
```

---

## §7. Stage 2 후속 (별도 박제) — Ground Fog Overlay

본 계획서 외부. Stage 1 합격 후 별도 박제.

**범위**: 메인 3D 화면의 **지면 (Map plane)** 에 fog 오버레이 — 보였던 영역 = 회색, 현재 보이는 영역 = 정상, 미탐험 = 검정.

**필요**:
- 신규 셰이더 `Shaders/Mesh3D_Ground.hlsl` (현재 Mesh3D.hlsl 변경 X)
- cbuffer b3 `CBFogParams { Vec3 worldOrigin; f32_t worldSize; }` (256m square — `CVisionSystem::FOW_TEX_WORLD_SIZE`)
- t1 슬롯 fog SRV 바인딩
- PS 본문에서 `worldPos.xz → fog UV → sample → multiply`

**주의 (P-11)**: Map 의 worldOrigin/worldSize 를 InitDesc 로 주입 (도메인 상수 Engine 박제 금지). 엘든링 진입 시 다른 plane 셰이더 신설.

---

## §8. 다음 세션 진입 명령

```
"Fog Stage 1 진입.
.md/TODO/05-07/Fog/00_FOG_OF_WAR_INTEGRATION.md §4.0 grep 으로
render 호출자 N 곳 식별 → §4.1 helper 박제 (Client/Public/Scene/RenderVisibilityFilter.h 신규)
→ §4.2 main pass 분기 + §4.3 normal pass 분기 동시 박제
→ §4.5 vcxproj 등록 → §5.1 시각 검증 6 항목.
M-1 ~ M-7 사전 박제 후 진입."
```

진입 직전 체크리스트:
- [ ] devenv.exe 종료 (vc143.pdb lock 회피)
- [ ] `git checkout -b feature/fog-stage1-entity-filter`
- [ ] Engine 단독 빌드 1회 (SDK 동기화)
- [ ] §4.0 grep 결과 박제 본문 갱신

---

**END OF FOG STAGE 1 PLAN**
