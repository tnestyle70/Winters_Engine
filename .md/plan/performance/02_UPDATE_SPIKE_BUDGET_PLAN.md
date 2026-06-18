Session - Update 스파이크와 애니메이션 예산 정리

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Client/Private/Manager/Minion_Manager.cpp

기존 코드:
```cpp
    {
        WINTERS_PROFILE_SCOPE("Minion::AnimUpdate");
        uint64_t animCount = 0;
        uint64_t skippedCount = 0;
        uint64_t budgetSkippedCount = 0;
        m_pWorld->ForEach<MinionStateComponent, RenderComponent>(
            [fDeltaTime, &animCount, &skippedCount, &budgetSkippedCount](
                EntityID,
                MinionStateComponent& ms,
                RenderComponent& rc)
            {
                if (!rc.bVisible || !rc.pRenderer)
                    return;
                if (rc.bSceneManaged || !rc.bAnimated || !rc.pRenderer->HasSkeleton())
                {
                    ++skippedCount;
                    return;
                }

                const bool_t bHighPriorityAnim =
                    ms.current == MinionStateComponent::Attack ||
                    ms.current == MinionStateComponent::Dead;
                const f32_t updateInterval = bHighPriorityAnim
                    ? kMinionHighPriorityAnimUpdateInterval
                    : (std::max)(ms.animUpdateInterval, kMinionBaseAnimUpdateInterval);

                ms.animUpdateAccumulator += fDeltaTime;
                if (ms.animUpdateAccumulator < updateInterval)
                {
                    ++skippedCount;
                    return;
                }

                if (!bHighPriorityAnim && animCount >= kMinionAnimUpdateBudget)
                {
                    ++skippedCount;
                    ++budgetSkippedCount;
                    return;
                }

                rc.pRenderer->Update(ms.animUpdateAccumulator);
                ms.animUpdateAccumulator = std::fmod(ms.animUpdateAccumulator, updateInterval);
                ++animCount;
            });
        WINTERS_PROFILE_COUNT("Anim::UpdateCalls", animCount);
        WINTERS_PROFILE_COUNT("Anim::Skipped", skippedCount);
        WINTERS_PROFILE_COUNT("Anim::BudgetSkipped", budgetSkippedCount);
    }
```

아래로 교체:
```cpp
    {
        WINTERS_PROFILE_SCOPE("Minion::AnimUpdate");
        const u8_t localTeam = UI::QueryLocalTeam(*m_pWorld);
        uint64_t animCount = 0;
        uint64_t skippedCount = 0;
        uint64_t visibilitySkippedCount = 0;
        uint64_t budgetSkippedCount = 0;
        m_pWorld->ForEach<MinionStateComponent, RenderComponent>(
            [this, fDeltaTime, localTeam,
                &animCount, &skippedCount, &visibilitySkippedCount, &budgetSkippedCount](
                EntityID id,
                MinionStateComponent& ms,
                RenderComponent& rc)
            {
                if (!rc.bVisible || !rc.pRenderer)
                    return;
                if (rc.bSceneManaged || !rc.bAnimated || !rc.pRenderer->HasSkeleton())
                {
                    ++skippedCount;
                    return;
                }
                if (!UI::IsRenderableForLocal(*m_pWorld, id, localTeam, false))
                {
                    ++skippedCount;
                    ++visibilitySkippedCount;
                    return;
                }

                const bool_t bHighPriorityAnim =
                    ms.current == MinionStateComponent::Attack ||
                    ms.current == MinionStateComponent::Dead;
                const f32_t updateInterval = bHighPriorityAnim
                    ? kMinionHighPriorityAnimUpdateInterval
                    : (std::max)(ms.animUpdateInterval, kMinionBaseAnimUpdateInterval);

                ms.animUpdateAccumulator += fDeltaTime;
                if (ms.animUpdateAccumulator < updateInterval)
                {
                    ++skippedCount;
                    return;
                }

                if (!bHighPriorityAnim && animCount >= kMinionAnimUpdateBudget)
                {
                    ++skippedCount;
                    ++budgetSkippedCount;
                    return;
                }

                rc.pRenderer->Update(ms.animUpdateAccumulator);
                ms.animUpdateAccumulator = std::fmod(ms.animUpdateAccumulator, updateInterval);
                ++animCount;
            });
        WINTERS_PROFILE_COUNT("Anim::UpdateCalls", animCount);
        WINTERS_PROFILE_COUNT("Anim::Skipped", skippedCount);
        WINTERS_PROFILE_COUNT("Anim::VisibilitySkipped", visibilitySkippedCount);
        WINTERS_PROFILE_COUNT("Anim::BudgetSkipped", budgetSkippedCount);
    }
```

1-2. C:/Users/tnest/Desktop/Winters/Engine/Private/ECS/SystemScheduler.cpp

기존 코드:
```cpp
void CSystemSchedular::Execute(CWorld& world, float fTimeDelta)
{
    if (m_bExecutionPlanDirty)
        RebuildExecutionPlan();

    for (auto& [phase, phasePlan] : m_mapExecutionPlan)
    {
        (void)phase;

        for (const SystemBatch& batch : phasePlan)
        {
            if (batch.size() < kMinParallelBatchSize || m_pJobSystem == nullptr)
            {
                for (ISystem* sys : batch)
                    sys->Execute(world, fTimeDelta);
            }
            else
            {
                CJobCounter counter;
                for (ISystem* sys : batch)
                {
                    m_pJobSystem->Submit(
                        [sys, &world, fTimeDelta]()
                        {
                            sys->Execute(world, fTimeDelta);
                        },
                        &counter);
                }
                m_pJobSystem->WaitForCounter(&counter);
            }
        }
    }
}
```

아래로 교체:
```cpp
void CSystemSchedular::Execute(CWorld& world, float fTimeDelta)
{
    if (m_bExecutionPlanDirty)
        RebuildExecutionPlan();

    uint64_t sequentialBatchCount = 0;
    uint64_t parallelBatchCount = 0;
    uint64_t submittedJobCount = 0;

    for (auto& [phase, phasePlan] : m_mapExecutionPlan)
    {
        (void)phase;

        for (const SystemBatch& batch : phasePlan)
        {
            if (batch.size() < kMinParallelBatchSize || m_pJobSystem == nullptr)
            {
                ++sequentialBatchCount;
                for (ISystem* sys : batch)
                    sys->Execute(world, fTimeDelta);
            }
            else
            {
                ++parallelBatchCount;
                submittedJobCount += static_cast<uint64_t>(batch.size());

                CJobCounter counter;
                for (ISystem* sys : batch)
                {
                    m_pJobSystem->Submit(
                        [sys, &world, fTimeDelta]()
                        {
                            sys->Execute(world, fTimeDelta);
                        },
                        &counter);
                }
                m_pJobSystem->WaitForCounter(&counter);
            }
        }
    }

    WINTERS_PROFILE_COUNT("Scheduler::SequentialBatches", sequentialBatchCount);
    WINTERS_PROFILE_COUNT("Scheduler::ParallelBatches", parallelBatchCount);
    WINTERS_PROFILE_COUNT("Scheduler::SubmittedJobs", submittedJobCount);
}
```

CONFIRM_NEEDED:
```text
위 교체에는 ProfilerAPI include가 필요하다.
Engine/Private/ECS/SystemScheduler.cpp 상단 include 목록을 구현 직전에 다시 확인하고,
#include "ProfilerAPI.h"가 없으면 "ECS/SystemScheduler.h" 아래에 추가한다.
```

1-3. C:/Users/tnest/Desktop/Winters/Engine/Private/ECS/Systems/VisionSystem.cpp

기존 코드:
```cpp
void CVisionSystem::Execute(CWorld& world, float fDeltaTime)
```

아래에 추가:
```text
CONFIRM_NEEDED
VisionSystem은 시야 판정과 FOW 텍스처 갱신이 섞여 있으므로, 구현 직전 다음 정확한 분리점을 다시 확인한다.
- TickVisibility는 기존 TICK_INTERVAL을 유지하되 후보/가시 개수를 Counter로 남긴다.
- UpdateFowTexture는 dirty가 없을 때 완전히 건너뛰는지 확인한다.
- 필요한 경우 텍스처 갱신을 한 프레임 전체가 아니라 row/chunk 예산으로 쪼갠다.
```

2. 검증

```text
빌드:
- git diff --check
- msbuild Engine/Include/Engine.vcxproj /p:Configuration=Debug /p:Platform=x64
- Client/Private/Manager/Minion_Manager.cpp 단위 컴파일 또는 Client Debug x64 빌드

프로파일러:
- 99개 이상 엔티티 화면에서 Minion::AnimUpdate Avg/Max 확인
- Anim::UpdateCalls, Anim::Skipped, Anim::VisibilitySkipped, Anim::BudgetSkipped 값 확인
- Scheduler::SubmittedJobs가 작은 배치에서 과도하게 증가하지 않는지 확인

합격선:
- Minion::AnimUpdate p95 성격의 Max가 일반 화면에서 1ms 아래로 내려간다.
- Update 전체가 16.67ms를 넘는 프레임의 원인이 Minion::AnimUpdate 하나로 고정되지 않는다.
- 화면 밖/안개 속 미니언이 애니메이션 CPU를 계속 먹지 않는다.
```
