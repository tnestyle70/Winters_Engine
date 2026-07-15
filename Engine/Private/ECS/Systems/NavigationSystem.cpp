#include "ECS/Systems/NavigationSystem.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/NavAgentComponent.h"
#include "ECS/Components/NavigationControlComponent.h"
#include "Manager/Navigation/NavGrid.h"
#include "Manager/Navigation/Pathfinder.h"
#include "Core/JobSystem.h"
#include "Core/JobCounter.h"
#include "ProfilerAPI.h"

#include <cmath>
#include <vector>

NS_BEGIN(Engine)

static constexpr uint32_t kParallelThreshold = 16;
static constexpr bool_t kNavAgentDebugOutput = false;
static void FaceMoveDirection(
    TransformComponent& transform,
    const Vec3& dir,
    bool_t bUseReverseFacing)
{
    const Vec3 forward = WintersMath::NormalizeXZOrZero(dir, 0.0001f);
    if (forward.x == 0.f && forward.z == 0.f)
        return;

    Vec3 rot = transform.GetRotation();
    if (bUseReverseFacing)
    {
        rot.y = static_cast<f32_t>(std::atan2(-forward.x, -forward.z));
    }
    else
    {
        rot.y = WintersMath::YawFromDirectionXZ(forward);
    }
    transform.SetRotation(rot);
}

void CNavigationSystem::DescribeAccess(CSystemAccessBuilder& builder) const
{
    builder.Write<NavAgentComponent>()
        .Write<TransformComponent>()
        .Write<VelocityComponent>()
        .Read<NavigationControlComponent>();
}

void CNavigationSystem::Execute(CWorld& world, f32_t /*fTimeDelta*/)
{
    WINTERS_PROFILE_SCOPE("Nav::Execute");
    if (!m_pGrid) return;

    // 에이전트 ID 수집 (ForEach 는 Main 스레드에서만)
    std::vector<EntityID> vecAgents;
    vecAgents.reserve(128);
    world.ForEach<NavAgentComponent, TransformComponent, VelocityComponent>(
        function<void(EntityID, NavAgentComponent&, TransformComponent&, VelocityComponent&)>(
            [&](EntityID id, NavAgentComponent& agent, TransformComponent&, VelocityComponent& vel)
            {
                if (!agent.bHasGoal)
                {
                    if (vel.fSpeed != 0.f ||
                        vel.vDirection.x != 0.f ||
                        vel.vDirection.y != 0.f ||
                        vel.vDirection.z != 0.f)
                    {
                        vel.vDirection = { 0.f, 0.f, 0.f };
                        vel.fSpeed = 0.f;
                    }
                    return;
                }

                vecAgents.push_back(id);
            }));

    if (vecAgents.size() < kParallelThreshold || m_pJobSystem == nullptr)
    {
        for (EntityID id : vecAgents)
            ProcessAgent(world, id);
        return;
    }

    CJobCounter counter;
    CWorld* pWorld = &world;
    CNavigationSystem* pThis = this;
    for (EntityID id : vecAgents)
    {
        m_pJobSystem->Submit(
            [pThis, pWorld, id]()
            {
                pThis->ProcessAgent(*pWorld, id);
            },
            &counter);
    }
    m_pJobSystem->WaitForCounter(&counter);
}

void CNavigationSystem::ProcessAgent(CWorld& world, EntityID id)
{
    WINTERS_PROFILE_SCOPE("Nav::ProcessAgent");

    //Phase T-8 Stun 가드 - 스턴 시 이동 정지, 속도 0
    const NavigationControlComponent* pControl =
        world.HasComponent<NavigationControlComponent>(id)
            ? &world.GetComponent<NavigationControlComponent>(id)
            : nullptr;

    if (pControl && pControl->bMovementBlocked)
    {
        if (world.HasComponent<VelocityComponent>(id))
        {
            auto& v = world.GetComponent<VelocityComponent>(id);
            v.vDirection = { 0.f, 0.f, 0.f };
            v.fSpeed = 0.f;
        }
        return;
    }

    // 기존 Execute 람다 로직 그대로 이식.
    // 각 에이전트는 자기 NavAgent/Transform/Velocity 만 수정 → race 없음.
    if (!world.HasComponent<NavAgentComponent>(id)) return;
    if (!world.HasComponent<TransformComponent>(id)) return;
    if (!world.HasComponent<VelocityComponent>(id))  return;

    auto& agent = world.GetComponent<NavAgentComponent>(id);
    auto& xform = world.GetComponent<TransformComponent>(id);
    auto& vel = world.GetComponent<VelocityComponent>(id);

    if (!agent.bHasGoal)
    {
        vel.vDirection = { 0.f, 0.f, 0.f };
        vel.fSpeed = 0.f;
        return;
    }

    // 경로 재계산
    if (agent.bPathDirty)
    {
        WINTERS_PROFILE_SCOPE("Nav::Repath");
        WINTERS_PROFILE_COUNT("Nav::RepathCalls", 1);

        const Vec3 vPos = xform.GetWorldPosition();
        const auto start = m_pGrid->WorldToCell(vPos);
        const auto goal  = m_pGrid->WorldToCell(agent.vTarget);
        const bool_t bStartWalkable = m_pGrid->IsWalkable(start.x, start.y);
        const bool_t bGoalWalkable = m_pGrid->IsWalkable(goal.x, goal.y);

        ePathFindResult pathResult = ePathFindResult::Success;
        auto path = CPathfinder::Find_Path(m_pGrid, start, goal, &pathResult);

        WINTERS_PROFILE_COUNT("Nav::PathNodes", path.size());

#if defined(_DEBUG)
        if constexpr (kNavAgentDebugOutput)
        {
            static i32_t s_navLogCount = 0;
            if (pControl && pControl->bUseReverseFacing && s_navLogCount < 160)
            {
                char dbg[512];
                sprintf_s(dbg,
                    "[NavAgent] #%d id=%u pos=(%.2f,%.2f,%.2f) target=(%.2f,%.2f,%.2f) start=(%d,%d,%d) goal=(%d,%d,%d) path=%u result=%u speed=%.2f arrive=%.2f\n",
                    s_navLogCount,
                    static_cast<u32_t>(id),
                    vPos.x, vPos.y, vPos.z,
                    agent.vTarget.x, agent.vTarget.y, agent.vTarget.z,
                    start.x, start.y, bStartWalkable ? 1 : 0,
                    goal.x, goal.y, bGoalWalkable ? 1 : 0,
                    static_cast<u32_t>(path.size()),
                    static_cast<u32_t>(pathResult),
                    agent.fSpeed,
                    agent.fArriveRadius);
                OutputDebugStringA(dbg);
                ++s_navLogCount;
            }
        }
#endif

        agent.pathCellsX.clear();
        agent.pathCellsY.clear();
        agent.pathCellsX.reserve(path.size());
        agent.pathCellsY.reserve(path.size());
        for (const auto& c : path)
        {
            agent.pathCellsX.push_back(c.x);
            agent.pathCellsY.push_back(c.y);
        }
        agent.iPathIndex = 0;
        agent.bPathDirty = false;

        if (path.empty())
        {
            WINTERS_PROFILE_COUNT("Nav::PathEmpty", 1);

            const bool_t bChaseFallback =
                pControl && pControl->bChaseFallbackEnabled;

            if (bChaseFallback)
            {
                const Vec3 vMyPos = xform.GetWorldPosition();
                const Vec3 vDelta =
                {
                    agent.vTarget.x - vMyPos.x,
                    0.f,
                    agent.vTarget.z - vMyPos.z
                };
                const f32_t fDist = sqrtf(vDelta.x * vDelta.x + vDelta.z * vDelta.z);

                if (fDist >= agent.fArriveRadius &&
                    m_pGrid->SegmentWalkable(vMyPos, agent.vTarget))
                {
                    vel.vDirection = { vDelta.x / fDist, 0.f, vDelta.z / fDist };
                    FaceMoveDirection(
                        xform,
                        vel.vDirection,
                        pControl && pControl->bUseReverseFacing);

                    f32_t fFinalSpeed = agent.fSpeed;
                    if (pControl)
                        fFinalSpeed *= pControl->fMoveSpeedMul;
                    vel.fSpeed = fFinalSpeed;

                    agent.bPathDirty = true;

                    WINTERS_PROFILE_COUNT("Nav::DirectFallback", 1);
#if defined(_DEBUG)
                    if constexpr (kNavAgentDebugOutput)
                    {
                        static i32_t s_directFallbackLogCount = 0;
                        if (s_directFallbackLogCount < 80)
                        {
                            char dbg[512];
                            sprintf_s(dbg,
                                "[NavAgentFallback] #%d id=%u pos=(%.2f,%.2f,%.2f) target=(%.2f,%.2f,%.2f) start=(%d,%d,%d) goal=(%d,%d,%d) dist=%.2f speed=%.2f\n",
                                s_directFallbackLogCount,
                                static_cast<u32_t>(id),
                                vMyPos.x, vMyPos.y, vMyPos.z,
                                agent.vTarget.x, agent.vTarget.y, agent.vTarget.z,
                                start.x, start.y, bStartWalkable ? 1 : 0,
                                goal.x, goal.y, bGoalWalkable ? 1 : 0,
                                fDist,
                                vel.fSpeed);
                            OutputDebugStringA(dbg);
                            ++s_directFallbackLogCount;
                        }
                    }
#endif
                    return;
                }
            }

#if defined(_DEBUG)
            if constexpr (kNavAgentDebugOutput)
            {
                static i32_t s_pathEmptyLogCount = 0;
                if (pControl && pControl->bUseReverseFacing && s_pathEmptyLogCount < 80)
                {
                    char dbg[512];
                    sprintf_s(dbg,
                        "[NavAgentEmpty] #%d id=%u chase=%d pos=(%.2f,%.2f,%.2f) target=(%.2f,%.2f,%.2f) start=(%d,%d,%d) goal=(%d,%d,%d)\n",
                        s_pathEmptyLogCount,
                        static_cast<u32_t>(id),
                        bChaseFallback ? 1 : 0,
                        vPos.x, vPos.y, vPos.z,
                        agent.vTarget.x, agent.vTarget.y, agent.vTarget.z,
                        start.x, start.y, bStartWalkable ? 1 : 0,
                        goal.x, goal.y, bGoalWalkable ? 1 : 0);
                    OutputDebugStringA(dbg);
                    ++s_pathEmptyLogCount;
                }
            }
#endif
            agent.bHasGoal = false;
            vel.fSpeed = 0.f;
            return;
        }
    }
    else //경로 추종 — A* 호출 없는 경로
    {
        WINTERS_PROFILE_SCOPE("Nav::Follow");
    }

    // 웨이포인트 추적
    if (agent.iPathIndex >= agent.pathCellsX.size())
    {
        agent.bHasGoal = false;
        vel.fSpeed = 0.f;
        return;
    }

    const Vec3 vWp = m_pGrid->CellToWorld(
        agent.pathCellsX[agent.iPathIndex],
        agent.pathCellsY[agent.iPathIndex]);
    const Vec3 vMyPos = xform.GetWorldPosition();
    const Vec3 vDelta = { vWp.x - vMyPos.x, 0.f, vWp.z - vMyPos.z };
    const f32_t fDist = sqrtf(vDelta.x * vDelta.x + vDelta.z * vDelta.z);

    if (fDist < agent.fArriveRadius)
    {
        agent.iPathIndex++;
        return;
    }

    vel.vDirection = { vDelta.x / fDist, 0.f, vDelta.z / fDist };
    FaceMoveDirection(
        xform,
        vel.vDirection,
        pControl && pControl->bUseReverseFacing);
    // [Phase T-8] Slow 반영
    f32_t fFinalSpeed = agent.fSpeed;
    if (pControl)
        fFinalSpeed *= pControl->fMoveSpeedMul;
    vel.fSpeed = fFinalSpeed;
}

NS_END
