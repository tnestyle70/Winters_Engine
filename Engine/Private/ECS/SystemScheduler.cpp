#include "WintersPCH.h"
#include "ECS/SystemScheduler.h"
#include "Core/JobSystem.h"
#include "Core/JobCounter.h"
#include "ECS/World.h"

namespace
{
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
}

void CSystemSchedular::Execute(CWorld& world, float fTimeDelta)
{
    for (auto& [phase, systems] : m_mapPhases)
    {
        (void)phase;
        std::vector<ISystem*> batch;
        std::vector<SystemAccessDesc> batchDescs;

        auto flushBatch = [&]()
        {
            if (batch.empty())
                return;

            if (batch.size() == 1 || m_pJobSystem == nullptr)
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
}
