#include "WintersPCH.h"
#include "ECS/SystemScheduler.h"
#include "Core/JobSystem.h"
#include "Core/JobCounter.h"
#include "ECS/World.h"
#include "ProfilerAPI.h"
#include <algorithm>

namespace
{
    constexpr size_t kMinParallelBatchSize = 2u;

    SystemAccessDesc BuildAccessDesc(const ISystem& system)
    {
        CSystemAccessBuilder builder;
        system.DescribeAccess(builder);
        return builder.GetDesc();
    }

    bool ConflictsWithBatch(
        const SystemAccessDesc& desc,
        const std::vector<SystemAccessDesc>& batchDescs)
    {
        for (const SystemAccessDesc& existing : batchDescs)
        {
            if (SystemAccessConflicts(desc, existing))
                return true;
        }
        return false;
    }
}

void CSystemSchedular::Initialize(CJobSystem* pJobSystem)
{
    m_pJobSystem = pJobSystem;
}

void CSystemSchedular::RegisterSystem(unique_ptr<ISystem> system)
{
    const uint32_t phase = system->GetPhase();
    m_mapPhases[phase].push_back(move(system));
    m_bExecutionPlanDirty = true;
}

void CSystemSchedular::RebuildExecutionPlan()
{
    m_mapExecutionPlan.clear();

    for (auto& [phase, systems] : m_mapPhases)
    {
        auto& phasePlan = m_mapExecutionPlan[phase];
        phasePlan.reserve(systems.size());

        std::vector<ISystem*> batch;
        std::vector<SystemAccessDesc> batchDescs;
        batch.reserve(systems.size());
        batchDescs.reserve(systems.size());

        auto flushBatch = [&]()
        {
            if (batch.empty())
                return;

            phasePlan.push_back(batch);
            batch.clear();
            batchDescs.clear();
        };

        for (auto& sys : systems)
        {
            SystemAccessDesc desc = BuildAccessDesc(*sys);
            if (ConflictsWithBatch(desc, batchDescs))
                flushBatch();

            batch.push_back(sys.get());
            batchDescs.push_back(std::move(desc));
        }

        flushBatch();
    }

    m_bExecutionPlanDirty = false;
}

void CSystemSchedular::Execute(CWorld& world, float fTimeDelta)
{
    if (m_bExecutionPlanDirty)
        RebuildExecutionPlan();

    uint64_t sequentialBatchCount = 0;
    uint64_t parallelBatchCount = 0;
    uint64_t submittedJobCount = 0;
    uint64_t maxBatchSize = 0;

    for (auto& [phase, phasePlan] : m_mapExecutionPlan)
    {
        (void)phase;

        for (const SystemBatch& batch : phasePlan)
        {
            maxBatchSize = (std::max)(maxBatchSize, static_cast<uint64_t>(batch.size()));
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
    WINTERS_PROFILE_COUNT("Scheduler::MaxBatchSize", maxBatchSize);
}
