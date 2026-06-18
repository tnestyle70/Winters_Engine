Session - 100fps 기준 측정에서 ProfilerOverlay 렌더 비용을 제거하고 JSON 캡처를 UI 없이 저장한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Engine/Public/Manager/Profiler/ProfilerOverlay.h

기존 코드:

```cpp
	void Toggle() { m_bShow = !m_bShow; }
	void Draw();
```

아래로 교체:

```cpp
	void Toggle() { m_bShow = !m_bShow; }
	bool_t IsVisible() const { return m_bShow; }
	void Draw();
	bool_t CaptureToJson(const char* pPath, bool_t bForce = true);
```

1-2. C:/Users/tnest/Desktop/Winters/Engine/Private/Manager/Profiler/ProfilerOverlay.cpp

`void CProfilerOverlay::Capture_DisplayFrame(bool_t bForce)` 바로 아래에 추가:

기존 코드:

```cpp
void CProfilerOverlay::Capture_DisplayFrame(bool_t bForce)
{
	if (!m_pCPU)
		return;
	if (m_bFreeze && !bForce)
		return;

	const f64_t now = ImGui::GetTime();
	if (!bForce && !m_vDisplayStats.empty() && now < m_fNextSampleTime)
		return;

	m_vDisplayStats = m_pCPU->Get_LastFrameScopeStats();
	m_vDisplayCounters = m_pCPU->Get_LastFrameCounters();
	m_vDisplayEvents = m_pCPU->Get_LastFrameEvents();
	m_StableView.Update(m_vDisplayStats, m_pCPU->Get_TicksToMs());
	m_fNextSampleTime = now + std::max(0.05f, m_fSampleIntervalSec);
}
```

아래에 추가:

```cpp
bool_t CProfilerOverlay::CaptureToJson(const char* pPath, bool_t bForce)
{
	if (!m_pCPU || !pPath || pPath[0] == '\0')
		return false;

	if (bForce || m_vDisplayStats.empty())
		Capture_DisplayFrame(true);

	return Save_DisplayFrameToJson(pPath);
}
```

1-3. C:/Users/tnest/Desktop/Winters/Engine/Include/GameInstance.h

기존 코드:

```cpp
    void Profiler_Begin();
    void Profiler_End();
    void Profiler_Toggle();
    void Profiler_DrawOverlay();
    class CCPUProfiler* Get_CPUProfiler() { return m_pProfiler.get(); }
```

아래로 교체:

```cpp
    void Profiler_Begin();
    void Profiler_End();
    void Profiler_Toggle();
    bool_t Profiler_IsOverlayVisible() const;
    void Profiler_DrawOverlay();
    bool_t Profiler_SaveJson(const char* pPath);
    class CCPUProfiler* Get_CPUProfiler() { return m_pProfiler.get(); }
```

1-4. C:/Users/tnest/Desktop/Winters/Engine/Private/GameInstance.cpp

`void CGameInstance::Profiler_Toggle()` 바로 아래에 추가:

기존 코드:

```cpp
void CGameInstance::Profiler_Toggle()
{
	if (m_pProfilerOverlay)
		m_pProfilerOverlay->Toggle();
}
```

아래에 추가:

```cpp
bool_t CGameInstance::Profiler_IsOverlayVisible() const
{
	return m_pProfilerOverlay && m_pProfilerOverlay->IsVisible();
}
```

`void CGameInstance::Profiler_DrawOverlay()` 바로 아래에 추가:

기존 코드:

```cpp
void CGameInstance::Profiler_DrawOverlay()
{
	if (m_pProfilerOverlay)
		m_pProfilerOverlay->Draw();
}
```

아래에 추가:

```cpp
bool_t CGameInstance::Profiler_SaveJson(const char* pPath)
{
	return m_pProfilerOverlay && m_pProfilerOverlay->CaptureToJson(pPath, true);
}
```

1-5. C:/Users/tnest/Desktop/Winters/Engine/Private/Framework/CEngineApp.cpp

`int32 CEngineApp::Run()` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
        const auto frameStart = FrameClock::now();
```

아래에 추가:

```cpp
        bool_t bSaveProfilerJson = false;
```

`int32 CEngineApp::Run()` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
            if (CInput::Get().IsKeyPressed(VK_F3))
                CGameInstance::Get()->Profiler_Toggle();
```

아래에 추가:

```cpp
            if (CInput::Get().IsKeyPressed(VK_F4))
                bSaveProfilerJson = true;
```

`int32 CEngineApp::Run()` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
        CGameInstance::Get()->Profiler_End();
```

아래에 추가:

```cpp
        if (bSaveProfilerJson)
            CGameInstance::Get()->Profiler_SaveJson("profiler.json");
```

`void CEngineApp::Render()` 안에서 아래 코드를:

```cpp
        //Profiler Overlay(F3 Toggle)
        {
            WINTERS_PROFILE_SCOPE("ProfilerOverlay::Render");
            CGameInstance::Get()->Profiler_DrawOverlay();
        }
```

아래로 교체:

```cpp
        //Profiler Overlay(F3 Toggle)
        if (CGameInstance::Get()->Profiler_IsOverlayVisible())
        {
            WINTERS_PROFILE_SCOPE("ProfilerOverlay::Render");
            CGameInstance::Get()->Profiler_DrawOverlay();
        }
```

2. 검증

검증 명령:
- `git diff --check`
- `msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64`

수동 확인:
- F3으로 Profiler 창을 끈 뒤 F4를 눌러 `profiler.json`이 저장되는지 확인.
- Profiler 창을 끈 상태의 캡처에서 `ProfilerOverlay::Render`가 사라지거나 0.05ms 이하인지 확인.

Session - TransformSystem의 루트 수집 비용을 캐시하고 Scheduler 5ms 내부 병목을 분해한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Engine/Public/ECS/Systems/TransformSystem.h

기존 코드:

```cpp
#include "WintersAPI.h"    //WINTERS_ENGINE 매크로
#include <memory>
```

아래로 교체:

```cpp
#include "WintersAPI.h"    //WINTERS_ENGINE 매크로
#include <memory>
#include <vector>
```

기존 코드:

```cpp
    static void RebuildChildrenCache(CWorld& world);
```

아래로 교체:

```cpp
    void RebuildChildrenCache(CWorld& world);
```

기존 코드:

```cpp
    CJobSystem* m_pJobSystem = nullptr;
    bool m_bChildrenCacheDirty = true; // 첫 Execute 때 빌드
```

아래로 교체:

```cpp
    CJobSystem* m_pJobSystem = nullptr;
    std::vector<EntityID> m_vecRootCache{};
    bool m_bChildrenCacheDirty = true; // 첫 Execute 때 빌드
```

1-2. C:/Users/tnest/Desktop/Winters/Engine/Private/ECS/Systems/TransformSystem.cpp

기존 코드:

```cpp
#include "Core/JobSystem.h"
#include "Core/JobCounter.h"
```

아래로 교체:

```cpp
#include "Core/JobSystem.h"
#include "Core/JobCounter.h"
#include "ProfilerAPI.h"
```

`void CTransformSystem::Execute(CWorld& world, float /*fTimeDelta*/)` 전체를 아래로 교체:

```cpp
void CTransformSystem::Execute(CWorld& world, float /*fTimeDelta*/)
{
	WINTERS_PROFILE_SCOPE("Transform::Execute");

	if (m_bChildrenCacheDirty)
	{
		RebuildChildrenCache(world);
		m_bChildrenCacheDirty = false;
	}

	const std::vector<EntityID>& vecRoots = m_vecRootCache;
	WINTERS_PROFILE_COUNT("Transform::RootCount", static_cast<uint64_t>(vecRoots.size()));

	if (vecRoots.size() < kParallelThreshold || m_pJobSystem == nullptr)
	{
		for (EntityID id : vecRoots)
			UpdateEntityRecursive(world, id, Mat4(), false);
		return;
	}

	CJobCounter counter;
	CWorld* pWorld = &world;
	for (EntityID id : vecRoots)
	{
		m_pJobSystem->Submit(
			[pWorld, id]()
			{
				UpdateEntityRecursive(*pWorld, id, Mat4(), false);
			},
			&counter);
	}
	m_pJobSystem->WaitForCounter(&counter);
}
```

`void CTransformSystem::RebuildChildrenCache(CWorld& world)` 전체를 아래로 교체:

```cpp
void CTransformSystem::RebuildChildrenCache(CWorld& world)
{
    m_vecRootCache.clear();

    world.ForEach<TransformComponent>(
        function<void(EntityID, TransformComponent&)>(
            [](EntityID, TransformComponent& t)
            {
                t.m_vecChildren.clear();
            }));

    world.ForEach<TransformComponent>(
        function<void(EntityID, TransformComponent&)>(
            [&](EntityID id, TransformComponent& t)
            {
                if (t.m_Parent == NULL_ENTITY)
                {
                    m_vecRootCache.push_back(id);
                    return;
                }

                if (!world.HasComponent<TransformComponent>(t.m_Parent))
                    return;

                auto& parentT = world.GetComponent<TransformComponent>(t.m_Parent);
                parentT.m_vecChildren.push_back(id);
            }));
}
```

2. 검증

검증 명령:
- `git diff --check`
- `msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64`

수동 확인:
- F4 캡처에서 `Transform::Execute`가 별도 row로 보이는지 확인.
- `Scheduler`가 5ms대에서 내려가고, `Transform::RootCount`가 정상적인 루트 수로 찍히는지 확인.
- 에디터에서 parent 변경을 사용하는 경로가 있으면 `MarkChildrenCacheDirty()` 호출 누락이 없는지 확인.

Session - Scheduler 배치 기준을 낮추고 병렬 실행 가능성을 카운터로 노출한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Engine/Private/ECS/SystemScheduler.cpp

기존 코드:

```cpp
#include "ProfilerAPI.h"
```

아래로 교체:

```cpp
#include "ProfilerAPI.h"
#include <algorithm>
```

기존 코드:

```cpp
    constexpr size_t kMinParallelBatchSize = 4u;
```

아래로 교체:

```cpp
    constexpr size_t kMinParallelBatchSize = 2u;
```

`void CSystemSchedular::Execute(CWorld& world, float fTimeDelta)` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    uint64_t sequentialBatchCount = 0;
    uint64_t parallelBatchCount = 0;
    uint64_t submittedJobCount = 0;
```

아래에 추가:

```cpp
    uint64_t maxBatchSize = 0;
```

`void CSystemSchedular::Execute(CWorld& world, float fTimeDelta)` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
        for (const SystemBatch& batch : phasePlan)
        {
```

아래에 추가:

```cpp
            maxBatchSize = (std::max)(maxBatchSize, static_cast<uint64_t>(batch.size()));
```

기존 코드:

```cpp
    WINTERS_PROFILE_COUNT("Scheduler::SequentialBatches", sequentialBatchCount);
    WINTERS_PROFILE_COUNT("Scheduler::ParallelBatches", parallelBatchCount);
    WINTERS_PROFILE_COUNT("Scheduler::SubmittedJobs", submittedJobCount);
```

아래로 교체:

```cpp
    WINTERS_PROFILE_COUNT("Scheduler::SequentialBatches", sequentialBatchCount);
    WINTERS_PROFILE_COUNT("Scheduler::ParallelBatches", parallelBatchCount);
    WINTERS_PROFILE_COUNT("Scheduler::SubmittedJobs", submittedJobCount);
    WINTERS_PROFILE_COUNT("Scheduler::MaxBatchSize", maxBatchSize);
```

2. 검증

검증 명령:
- `git diff --check`
- `msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64`

수동 확인:
- F4 캡처에서 `Scheduler::MaxBatchSize`가 1이면 시스템 접근 충돌 또는 phase 분리가 병렬화 차단 원인인지 확인.
- `Scheduler::ParallelBatches`가 증가했는데 frame time이 악화되면 `kMinParallelBatchSize`를 4로 되돌린다.

Session - Jungle::Update에 애니메이션 업데이트 예산을 넣어 고정 1.9ms 비용을 낮춘다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Client/Public/Manager/Jungle_Manager.h

기존 코드:

```cpp
        f32_t actionTimer = 0.f;
        bool_t bAction = false;
        bool_t bDead = false;
```

아래로 교체:

```cpp
        f32_t actionTimer = 0.f;
        f32_t animUpdateAccumulator = 0.f;
        bool_t bAction = false;
        bool_t bDead = false;
```

1-2. C:/Users/tnest/Desktop/Winters/Client/Private/Manager/Jungle_Manager.cpp

기존 코드:

```cpp
#include "ProfilerAPI.h"
#include <Windows.h>
```

아래로 교체:

```cpp
#include "ProfilerAPI.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
```

기존 코드:

```cpp
    constexpr const char* PATH_KRUG_MINI = "Client/Bin/Resource/Texture/Object/Jungle/KrugMini/krugmini_textured.wmesh";
```

아래에 추가:

```cpp
    constexpr f32_t kJungleBaseAnimUpdateInterval = 1.f / 8.f;
    constexpr f32_t kJungleHighPriorityAnimUpdateInterval = 1.f / 20.f;
    constexpr uint64_t kJungleAnimUpdateBudget = 6u;
```

`void CJungle_Manager::Update(f32_t dt)` 전체를 아래로 교체:

```cpp
void CJungle_Manager::Update(f32_t dt)
{
    WINTERS_PROFILE_SCOPE("Jungle::Update");

    if (dt <= 0.f) return;

    uint64_t animCount = 0;
    uint64_t skippedCount = 0;
    uint64_t budgetSkippedCount = 0;

    for (auto& it : m_mapRenderers)
    {
        if (!it.second)
            continue;

        Apply_NetworkAnimation(it.first, *it.second, dt);

        auto& visual = m_mapVisualStates[it.first];
        const bool_t bHighPriorityAnim = visual.bAction || visual.bDead;
        const f32_t updateInterval = bHighPriorityAnim
            ? kJungleHighPriorityAnimUpdateInterval
            : kJungleBaseAnimUpdateInterval;

        visual.animUpdateAccumulator += dt;
        if (visual.animUpdateAccumulator < updateInterval)
        {
            ++skippedCount;
            continue;
        }

        if (!bHighPriorityAnim && animCount >= kJungleAnimUpdateBudget)
        {
            ++skippedCount;
            ++budgetSkippedCount;
            continue;
        }

        it.second->Update(visual.animUpdateAccumulator);
        visual.animUpdateAccumulator = std::fmod(visual.animUpdateAccumulator, updateInterval);
        ++animCount;
    }

    WINTERS_PROFILE_COUNT("JungleAnim::UpdateCalls", animCount);
    WINTERS_PROFILE_COUNT("JungleAnim::Skipped", skippedCount);
    WINTERS_PROFILE_COUNT("JungleAnim::BudgetSkipped", budgetSkippedCount);
}
```

2. 검증

검증 명령:
- `git diff --check`
- `msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64`

수동 확인:
- F4 캡처에서 `Jungle::Update`가 0.6ms 이하로 내려가는지 확인.
- Baron/Dragon/Blue/Red의 idle, run, attack, death 애니메이션이 눈에 띄게 끊기지 않는지 확인.
- 한타 중 정글 몬스터가 화면 밖이면 애니 품질보다 frame time을 우선한다.

Session - Minion visual path가 숨겨진 미니언까지 상태 전환과 애니 업데이트를 지불하지 않게 한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Client/Private/Manager/Minion_Manager.cpp

기존 코드:

```cpp
    static constexpr f32_t kMinionBaseAnimUpdateInterval = 1.f / 10.f;
    static constexpr f32_t kMinionHighPriorityAnimUpdateInterval = 1.f / 20.f;
    static constexpr uint64_t kMinionAnimUpdateBudget = 14u;
```

아래로 교체:

```cpp
    static constexpr f32_t kMinionBaseAnimUpdateInterval = 1.f / 8.f;
    static constexpr f32_t kMinionHighPriorityAnimUpdateInterval = 1.f / 20.f;
    static constexpr uint64_t kMinionAnimUpdateBudget = 8u;
```

`void CMinion_Manager::TickVisuals(f32_t fDeltaTime)` 안에서 아래 코드를:

```cpp
    m_pWorld->ForEach<MinionStateComponent, RenderComponent>(
        [this, fDeltaTime](EntityID id, MinionStateComponent& ms, RenderComponent& rc)
        {
            UpdateMinionVisual(id, ms, rc, fDeltaTime);
        });
```

아래로 교체:

```cpp
    const u8_t localTeam = UI::QueryLocalTeam(*m_pWorld);

    m_pWorld->ForEach<MinionStateComponent, RenderComponent>(
        [this, fDeltaTime, localTeam](EntityID id, MinionStateComponent& ms, RenderComponent& rc)
        {
            if (!rc.bVisible || !rc.pRenderer)
                return;
            if (!UI::IsRenderableForLocal(*m_pWorld, id, localTeam, false))
                return;

            UpdateMinionVisual(id, ms, rc, fDeltaTime);
        });
```

`void CMinion_Manager::TickVisuals(f32_t fDeltaTime)` 안에서 아래 코드를 삭제:

```cpp
        const u8_t localTeam = UI::QueryLocalTeam(*m_pWorld);
```

2. 검증

검증 명령:
- `git diff --check`
- `msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64`

수동 확인:
- F4 캡처에서 `Minion::AnimUpdate`가 0.8ms 이하로 내려가는지 확인.
- `Anim::BudgetSkipped`가 증가해도 근접 전투 중 공격/사망 애니메이션이 유지되는지 확인.
- FOW 밖 미니언이 다시 보일 때 base animation 상태가 자연스럽게 복귀하는지 확인.

Session - DX11 billboard FX가 PlaneRenderer 단건 Render 대신 기존 batch path를 사용하게 한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Client/Private/GameObject/FX/FxSystem.cpp

`void CFxSystem::Render(CWorld& world, const CDynamicCamera* pCamera)` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    const Mat4 matVP = pCamera->GetViewProjection();
    const Vec3 vCamRight = pCamera->GetRight();
    const Vec3 vCamUp = pCamera->GetUp();
    const Vec3 vCamFwd = pCamera->GetForward();
```

아래에 추가:

```cpp
    const bool_t bUseDX11Batch = !bUseRHI && m_pPlane && m_pPlane->BeginBatch(m_pDevice, matVP);
```

`void CFxSystem::Render(CWorld& world, const CDynamicCamera* pCamera)` 안에서 아래 코드를:

```cpp
                else
                {
                    m_pPlane->SetFxParams(fxParams);
                    // Runtime FX should not be clipped by terrain depth; world actors keep normal depth.
                    m_pPlane->SetDepthMode(eFxDepthMode::OverlayNoDepth);
                    m_pPlane->SetTexture(pTex);
                    m_pPlane->SetWorld(world);
                    m_pPlane->Render(m_pDevice, matVP);

                    // Reset shared plane state after FX rendering.
                    m_pPlane->ResetFxParams();
                    m_pPlane->SetBlendCache(m_pBlendCache, eBlendPreset::AlphaBlend);
                }
```

아래로 교체:

```cpp
                else
                {
                    m_pPlane->SetFxParams(fxParams);
                    // Runtime FX should not be clipped by terrain depth; world actors keep normal depth.
                    m_pPlane->SetDepthMode(eFxDepthMode::OverlayNoDepth);
                    m_pPlane->SetTexture(pTex);
                    m_pPlane->SetWorld(world);

                    if (bUseDX11Batch)
                        m_pPlane->RenderBatched();
                    else
                        m_pPlane->Render(m_pDevice, matVP);

                    if (!bUseDX11Batch)
                    {
                        m_pPlane->ResetFxParams();
                        m_pPlane->SetBlendCache(m_pBlendCache, eBlendPreset::AlphaBlend);
                    }
                }
```

`void CFxSystem::Render(CWorld& world, const CDynamicCamera* pCamera)` 안에서 `world.ForEach<FxBillboardComponent>(...);` 호출 바로 아래에 추가:

기존 코드:

```cpp
            }));
}
```

아래로 교체:

```cpp
            }));

    if (bUseDX11Batch)
    {
        m_pPlane->EndBatch();
        m_pPlane->ResetFxParams();
        m_pPlane->SetBlendCache(m_pBlendCache, eBlendPreset::AlphaBlend);
    }
}
```

2. 검증

검증 명령:
- `git diff --check`
- `msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64`

수동 확인:
- Yasuo wind, Irelia blade, Kalista projectile, Annie/Tibbers FX를 동시에 발생시켜 `Fx::Render`가 1ms 이하로 유지되는지 확인.
- DX11 FX blend/depth 상태가 다음 world actor render나 UI render에 새지 않는지 확인.
- `Render::EndFrame`이 함께 내려가면 GPU/driver wait도 FX 제출 비용 영향을 받던 것으로 기록한다.

Session - RHI minion health bar가 숨겨진 render component를 건너뛰게 한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp

기존 코드:

```cpp
#include "ECS/Components/CoreComponents.h"          // HealthComponent
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/GameplayComponents.h"      // Minion/Champion/Structure team 조회
```

아래로 교체:

```cpp
#include "ECS/Components/CoreComponents.h"          // HealthComponent
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/GameplayComponents.h"      // Minion/Champion/Structure team 조회
#include "ECS/Components/RenderComponent.h"
```

`void CUI_Manager::DrawMinionHealthBarsRHI(const DirectX::XMMATRIX& mVP)` 안에서 아래 코드를:

```cpp
    m_pWorld->ForEach<MinionComponent, TransformComponent>(
        [&](EntityID id, MinionComponent& minion, TransformComponent& tf)
        {
            f32_t hpCurrent = minion.hp;
```

아래로 교체:

```cpp
    m_pWorld->ForEach<MinionComponent, TransformComponent, RenderComponent>(
        [&](EntityID id, MinionComponent& minion, TransformComponent& tf, RenderComponent& rc)
        {
            if (!rc.bVisible)
                return;

            f32_t hpCurrent = minion.hp;
```

`void CUI_Manager::DrawMinionHealthBars(ImDrawList* pDraw, const DirectX::XMMATRIX& mVP)` 안에서도 동일한 `ForEach<MinionComponent, TransformComponent>` 블록이 있으면 같은 방식으로 `RenderComponent`를 추가하고 `!rc.bVisible`을 먼저 return한다.

2. 검증

검증 명령:
- `git diff --check`
- `msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64`

수동 확인:
- F4 캡처에서 `UI::RHIHealthBars`가 0.25ms 이하로 내려가는지 확인.
- FOW나 사망 처리로 `RenderComponent::bVisible=false`가 된 미니언의 체력바가 같이 사라지는지 확인.

Session - Map::Render 5ms 비용을 하위 scope로 쪼갠 뒤 chunk culling 대상인지 확인한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/InGameRenderBridge.cpp

`CInGameRenderBridge::Render(CScene_InGame& scene)` 안에서 아래 코드를:

```cpp
    {
        WINTERS_PROFILE_SCOPE("Map::Render");
        scene.m_Map.UpdateCamera(vp, cameraWorld);
        scene.m_Map.UpdateTransform(scene.m_MapTransform.GetWorldMatrix());
        scene.m_Map.SetAmbientOcclusionSRV(pAmbientOcclusionSRV);
        scene.m_Map.RenderFrustumCulled(vp);
    }
```

아래로 교체:

```cpp
    {
        WINTERS_PROFILE_SCOPE("Map::Render");
        {
            WINTERS_PROFILE_SCOPE("Map::UpdateCamera");
            scene.m_Map.UpdateCamera(vp, cameraWorld);
        }
        {
            WINTERS_PROFILE_SCOPE("Map::UpdateTransform");
            scene.m_Map.UpdateTransform(scene.m_MapTransform.GetWorldMatrix());
        }
        scene.m_Map.SetAmbientOcclusionSRV(pAmbientOcclusionSRV);
        {
            WINTERS_PROFILE_SCOPE("Map::DrawFrustumCulled");
            scene.m_Map.RenderFrustumCulled(vp);
        }
    }
```

1-2. C:/Users/tnest/Desktop/Winters/Engine/Private/Renderer/ModelRenderer.cpp

CONFIRM_NEEDED:
- `ModelRenderer::RenderFrustumCulled`와 `CModel::RenderWithMask`의 draw call, mesh count, material bind count를 카운터로 노출할 정확한 위치를 다시 확인한다.
- 확인 뒤 `Map::DrawCalls`, `Map::VisibleMeshes`, `Map::MaterialBinds` 카운터를 추가한다.

2. 검증

검증 명령:
- `git diff --check`
- `msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64`

수동 확인:
- F4 캡처에서 `Map::UpdateCamera`, `Map::UpdateTransform`, `Map::DrawFrustumCulled` 중 어느 scope가 5ms 대부분을 차지하는지 확인.
- `Map::DrawFrustumCulled`가 대부분이면 다음 세션에서 map `.wmesh`를 chunk 단위로 분할하거나 renderer에서 mesh group culling/material sort를 적용한다.

Session - 100fps 유지 기준으로 세션별 예산을 고정하고 회귀를 profiler.json으로 판정한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/.md/plan/2026-06-04_100FPS_BOTTLENECK_SESSIONS_PLAN.md

기존 코드:

```text
Session - 100fps 유지 기준으로 세션별 예산을 고정하고 회귀를 profiler.json으로 판정한다.
```

아래에 추가:

```text
세션별 frame budget:
- Baseline: Frame 25.153ms, Update 13.309ms, Render 11.810ms.
- Session 01 target: ProfilerOverlay::Render 0.05ms 이하.
- Session 02 target: Scheduler 2.0ms 이하, Transform::Execute 1.2ms 이하.
- Session 03 target: Scheduler::ParallelBatches 또는 Transform 내부 병렬 효과 확인.
- Session 04 target: Jungle::Update 0.6ms 이하.
- Session 05 target: Minion::AnimUpdate 0.8ms 이하.
- Session 06 target: Fx::Render 1.0ms 이하, 한타 smoke에서 1.5ms 이하.
- Session 07 target: UI::RHIHealthBars 0.25ms 이하, UIOverlay::Render 0.75ms 이하.
- Session 08 target: Map::Render 원인 분해 후 2.5ms 이하로 줄이는 후속 plan 작성.
- Release target: Frame 10.0ms 이하, 100fps 이상.
```

2. 검증

검증 명령:
- `git diff --check`
- `msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64`

수동 확인:
- 각 세션 후 F3 off, F4 캡처를 저장하고 `profiler.json`의 `frameMs`와 target row를 비교한다.
- 한 세션이 target을 못 맞추면 다음 세션으로 넘어가지 않고 해당 병목 row를 다시 분해한다.
- normal F5 runtime에서 roster, map, minion, snapshot, champion, FX cue를 숨기지 않은 상태로 측정한다.
