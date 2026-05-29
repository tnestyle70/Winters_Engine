#include "WintersPCH.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"
#include <algorithm>


EntityID CWorld::CreateEntity() { return m_entityMgr.Create(); }
EntityHandle CWorld::CreateEntityHandle() { return m_entityMgr.CreateHandle(); }
bool     CWorld::IsAlive(EntityID e) const { return m_entityMgr.IsAlive(e); }
bool     CWorld::IsAlive(EntityHandle e) const { return m_entityMgr.IsAlive(e); }
EntityGeneration CWorld::GetEntityGeneration(EntityID e) const { return m_entityMgr.GetGeneration(e); }
EntityHandle CWorld::GetEntityHandle(EntityID e) const { return m_entityMgr.GetHandle(e); }
bool     CWorld::TryResolveEntity(EntityHandle e, EntityID& outEntityID) const { return m_entityMgr.TryResolve(e, outEntityID); }
EntityID CWorld::ResolveEntity(EntityHandle e) const { return m_entityMgr.Resolve(e); }
uint32_t CWorld::GetEntityCount() const { return m_entityMgr.GetAliveCount(); }

void CWorld::Initialize_Spatial(const SpatialGridDesc& desc)
{
	if (!m_pSpatialIndex)
		m_pSpatialIndex = unique_ptr<CSpatialIndex>(new CSpatialIndex());
	m_pSpatialIndex->Initialize(desc);
}

void CWorld::DestroyEntity(EntityID entity)
{
	// Phase 5-A: 계층 관계 정리 — 자식은 루트로 승격, 부모의 자식 리스트에서 자기 제거
	if (HasComponent<TransformComponent>(entity))
	{
		auto& t = GetComponent<TransformComponent>(entity);

		// 자식들 고아 방지 (루트로 승격)
		for (EntityID child : t.m_vecChildren)
		{
			if (HasComponent<TransformComponent>(child))
			{
				auto& childT = GetComponent<TransformComponent>(child);
				childT.m_Parent = NULL_ENTITY;
				childT.m_bWorldDirty = true;
			}
		}
		t.m_vecChildren.clear();

		// 부모의 자식 리스트에서 자기 제거
		if (t.m_Parent != NULL_ENTITY &&
			HasComponent<TransformComponent>(t.m_Parent))
		{
			auto& pT = GetComponent<TransformComponent>(t.m_Parent);
			auto& v = pT.m_vecChildren;
			v.erase(std::remove(v.begin(), v.end(), entity), v.end());
		}
	}

	// 원본 로직 — 모든 스토어에서 컴포넌트 제거 + 엔티티 매니저 파괴
	for (auto& [type, store] : m_mapStores)
		store->Remove(entity);
	m_entityMgr.Destroy(entity);
}

bool CWorld::DestroyEntity(EntityHandle entity)
{
	EntityID resolved = NULL_ENTITY;
	if (!m_entityMgr.TryResolve(entity, resolved))
		return false;

	DestroyEntity(resolved);
	return true;
}

void CWorld::SetParent(EntityID child, EntityID newParent)
{
	if (!HasComponent<TransformComponent>(child))
		return;

	auto& childT = GetComponent<TransformComponent>(child);

	// 기존 부모의 m_vecChildren 에서 child 제거
	if (childT.m_Parent != NULL_ENTITY &&
		HasComponent<TransformComponent>(childT.m_Parent))
	{
		auto& oldP = GetComponent<TransformComponent>(childT.m_Parent);
		auto& v = oldP.m_vecChildren;
		v.erase(std::remove(v.begin(), v.end(), child), v.end());
	}
	childT.m_Parent = newParent;
	childT.m_bWorldDirty = true;

	// 새 부모에 child 추가 (로컬 변수명은 파라미터와 다르게 — 섀도잉/자기참조 금지)
	if (newParent != NULL_ENTITY &&
		HasComponent<TransformComponent>(newParent))
	{
		auto& newParentT = GetComponent<TransformComponent>(newParent);
		auto& v = newParentT.m_vecChildren;
		if (std::find(v.begin(), v.end(), child) == v.end())
			v.push_back(child);
	}
}
