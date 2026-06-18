#include "WintersPCH.h"
#include "ECS/Systems/TransformSystem.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"
#include "Core/JobSystem.h"
#include "Core/JobCounter.h"
#include "ProfilerAPI.h"

//루트수가 이 값 이상일 경우 Job분할 - Submit / steal/Counter 오버헤드가 
//병렬 이득보다 커지는 임계. 프로파일링으로 조정
static constexpr uint32_t kParallelThreshold = 16;
static constexpr uint32_t kTransformRootsPerJob = 8;

void CTransformSystem::DescribeAccess(CSystemAccessBuilder& builder) const
{
	builder.Write<TransformComponent>();
}

void CTransformSystem::Execute(CWorld& world, float /*fTimeDelta*/)
{
	WINTERS_PROFILE_SCOPE("Transform::Execute");

	const uint32_t currentEntityCount = world.GetEntityCount();
	if (m_bChildrenCacheDirty || currentEntityCount != m_uLastEntityCount)
	{
		RebuildChildrenCache(world);
		m_bChildrenCacheDirty = false;
	}

	const std::vector<EntityID>& vecRoots = m_vecRootCache;
	WINTERS_PROFILE_COUNT("Transform::RootCount", static_cast<uint64_t>(vecRoots.size()));

	// 단일 스레드 fallback (Job 오버헤드 회피 or JobSystem 미주입)
	if (vecRoots.size() < kParallelThreshold || m_pJobSystem == nullptr)
	{
		for (EntityID id : vecRoots)
			UpdateEntityRecursive(world, id, Mat4(), false);
		return;
	}

	const uint32_t rootCount = static_cast<uint32_t>(vecRoots.size());
	const uint32_t jobCount =
		(rootCount + kTransformRootsPerJob - 1u) / kTransformRootsPerJob;

	CJobCounter counter;
	CWorld* pWorld = &world;
	const std::vector<EntityID>* pRoots = &vecRoots;

	for (uint32_t jobIndex = 0; jobIndex < jobCount; ++jobIndex)
	{
		const uint32_t begin = jobIndex * kTransformRootsPerJob;
		const uint32_t end = (std::min)(begin + kTransformRootsPerJob, rootCount);

		m_pJobSystem->Submit(
			[pWorld, pRoots, begin, end]()
			{
				for (uint32_t i = begin; i < end; ++i)
					UpdateEntityRecursive(*pWorld, (*pRoots)[i], Mat4(), false);
			},
			&counter);
	}

	m_pJobSystem->WaitForCounter(&counter);

	WINTERS_PROFILE_COUNT("Transform::SubmittedRootJobs", jobCount);
	WINTERS_PROFILE_COUNT("Transform::RootsPerJob", kTransformRootsPerJob);
}

void CTransformSystem::UpdateEntityRecursive(CWorld& world, EntityID id,
	const Mat4& parentWorld, bool parentWorldDirty)
{
    if (!world.HasComponent<TransformComponent>(id))
        return;

    TransformComponent& t = world.GetComponent<TransformComponent>(id);

    // ── Step 1: LocalMatrix 재계산 ──
    if (t.m_bLocalDirty)
    {
        WINTERS_PROFILE_SCOPE("Transform::LocalRecompute");

        XMVECTOR scale = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&t.m_LocalScale));
        XMVECTOR rot = XMQuaternionRotationRollPitchYaw(
            t.m_LocalRotation.x,
            t.m_LocalRotation.y,
            t.m_LocalRotation.z);
        XMVECTOR pos = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&t.m_LocalPosition));
        XMMATRIX srt = XMMatrixAffineTransformation(
            scale, XMVectorZero(),
            rot, pos);
        XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&t.m_LocalMatrix), srt);
        t.m_bLocalDirty = false;
        t.m_bWorldDirty = true;
    }

    // ── Step 2: WorldMatrix 재계산 ──
    const bool needWorldRecompute = t.m_bWorldDirty || parentWorldDirty;
    if (needWorldRecompute)
    {
        WINTERS_PROFILE_SCOPE("Transform::WorldRecompute");

        if (t.m_Parent == NULL_ENTITY)
        {
            t.m_WorldMatrix = t.m_LocalMatrix;
        }
        else
        {
            XMMATRIX local = XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&t.m_LocalMatrix));
            XMMATRIX parent = XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&parentWorld));
            XMMATRIX worldMat = local * parent;
            XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&t.m_WorldMatrix), worldMat);
        }
        t.m_bWorldDirty = false;
    }

    // ── Step 3: 자식 재귀 (캐시된 m_vecChildren 사용) ──
    const Mat4 myWorld = t.m_WorldMatrix;  // 재귀 중 t 참조 안정화
    // t.m_vecChildren 을 복사 없이 순회 — 하지만 DestroyEntity 가 재귀 중 호출되면
    // 위험. 실제 엔진 흐름에선 Execute 중 엔티티 파괴 없음.
    for (EntityID childId : t.m_vecChildren)
    {
        UpdateEntityRecursive(world, childId, myWorld, needWorldRecompute);
    }
}

void CTransformSystem::RebuildChildrenCache(CWorld& world)
{
    m_vecRootCache.clear();
    m_uLastEntityCount = world.GetEntityCount();

    // 1) 모든 m_vecChildren 비우기
    world.ForEach<TransformComponent>(
        function<void(EntityID, TransformComponent&)>(
            [](EntityID, TransformComponent& t)
            {
                t.m_vecChildren.clear();
            }));

    // 2) m_Parent 역추적해 각 부모의 m_vecChildren 에 push
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
