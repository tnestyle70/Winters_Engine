#pragma once
#include "WintersAPI.h"   // WINTERS_API
#include "ECS/Entity.h"
#include "ECS/ComponentStore.h"
#include "ECS/SpatialIndex.h"

// STL — Engine PCH 바깥(Client·Server 빌드)에서도 안전하게 동작하도록 명시
#include <typeindex>
#include <unordered_map>
#include <memory>
#include <functional>

//왜 여기에 따로 using을 뺀 거지? -> 이거 무조건 수정 절대 헤더에 이렇게 두면 안 됨
// dll-interface 경고 (unordered_map/unique_ptr 멤버) 범위 억제
#pragma warning(push)
#pragma warning(disable: 4251)

//컴포넌트 스토어 베이스
class IComponentStoreBase
{
public:
	virtual ~IComponentStoreBase() = default;
	virtual void Remove(EntityID entity) = 0;
	virtual bool Has(EntityID entity) const = 0;
};

//컴포넌트 스토어 래퍼 
template<typename T>
class CComponentStoreWrapper : public IComponentStoreBase
{
public:
	CComponentStore<T>& GetStore() { return m_store; }
	const CComponentStore<T>& GetStore() const { return m_store; }
	void Remove(EntityID entity) override 
	{ 
		if (m_store.Has(entity)) 
			m_store.Remove(entity); 
	}
	bool Has(EntityID entity) const override
	{
		return m_store.Has(entity);
	}
private:
	CComponentStore<T> m_store;
};

class CWorld
{
public:
	CWorld() = default;
	~CWorld() = default;

	// unique_ptr 멤버 때문에 복사 불가, 이동만 허용
	CWorld(const CWorld&) = delete;
	CWorld& operator=(const CWorld&) = delete;
	CWorld(CWorld&&) = default;
	CWorld& operator=(CWorld&&) = default;

	// non-template 멤버만 DLL 경계를 넘음 (template 멤버는 header에서 인스턴스화)
	//여기만 Client 쪽에 공개를 한다는 의미? ?
	WINTERS_ENGINE EntityID CreateEntity();
	WINTERS_ENGINE EntityHandle CreateEntityHandle();
	WINTERS_ENGINE void     DestroyEntity(EntityID entityID);
	WINTERS_ENGINE bool     DestroyEntity(EntityHandle entityHandle);
	WINTERS_ENGINE bool     IsAlive(EntityID entityID) const;
	WINTERS_ENGINE bool     IsAlive(EntityHandle entityHandle) const;
	WINTERS_ENGINE EntityGeneration GetEntityGeneration(EntityID entityID) const;
	WINTERS_ENGINE EntityHandle GetEntityHandle(EntityID entityID) const;
	WINTERS_ENGINE bool     TryResolveEntity(EntityHandle entityHandle, EntityID& outEntityID) const;
	WINTERS_ENGINE EntityID ResolveEntity(EntityHandle entityHandle) const;
	WINTERS_ENGINE uint32_t GetEntityCount() const;

	//TransformComponent 계층 관계 갱신 헬퍼
	WINTERS_ENGINE void SetParent(EntityID child, EntityID newParent);
	WINTERS_ENGINE void Initialize_Spatial(const SpatialGridDesc& desc);
	CSpatialIndex* Get_SpatialIndex() const { return m_pSpatialIndex.get(); }
	
	template<typename T> T& AddComponent(EntityID e, const T& c = T{})
	{
		return GetOrCreateStore<T>().Add(e, c);
	}
	template<typename T> void RemoveComponent(EntityID e)
	{
		GetOrCreateStore<T>().Remove(e);
	}
	template<typename T> T& GetComponent(EntityID e)
	{
		return GetOrCreateStore<T>().Get(e);
	}
	template<typename T> const T& GetComponent(EntityID e) const
	{
		auto it = m_mapStores.find(std::type_index(typeid(T)));
		return static_cast<CComponentStoreWrapper<T>*>(it->second.get())->GetStore().Get(e);
	}
	template<typename T> bool HasComponent(EntityID e) const
	{
		auto it = m_mapStores.find(std::type_index(typeid(T)));
		if (it == m_mapStores.end())
			return false;
		return static_cast<CComponentStoreWrapper<T>*>(it->second.get())->GetStore().Has(e);
	}

	template<typename T>
	void ForEach(std::function<void(EntityID, T&)> fn)
	{
		auto& s = GetOrCreateStore<T>();
		for (uint32_t i = 0; i < s.Count(); ++i)
		{
			fn(s.Entities()[i], s.Data()[i]);
		}
	}

	template<typename T1, typename T2>
	void ForEach(std::function<void(EntityID, T1&, T2&)> fn)
	{
		auto& s1 = GetOrCreateStore<T1>();
		auto& s2 = GetOrCreateStore<T2>();
		for (uint32_t i = 0; i < s1.Count(); ++i)
		{
			EntityID e = s1.Entities()[i];
			if (s2.Has(e))
				fn(e, s1.Data()[i], s2.Get(e));
		}
	}
	
	template<typename T1, typename T2, typename T3>
	void ForEach(std::function<void(EntityID, T1&, T2&, T3&)> fn)
	{
		auto& s1 = GetOrCreateStore<T1>();
		auto& s2 = GetOrCreateStore<T2>();
		auto& s3 = GetOrCreateStore<T3>();

		for (uint32_t i = 0; i < s1.Count(); ++i)
		{
			EntityID e = s1.Entities()[i];
			if (s2.Has(e) && s3.Has(e))
				fn(e, s1.Data()[i], s2.Get(e), s3.Get(e));
		}
	}
private:
	template<typename T>
	CComponentStore<T>& GetOrCreateStore()
	{
		auto key = std::type_index(typeid(T));
		auto it = m_mapStores.find(key);
		if (it == m_mapStores.end())
		{
			auto w = std::make_unique<CComponentStoreWrapper<T>>();
			auto* p = w.get();
			m_mapStores[key] = std::move(w);
			return p->GetStore();
		}
		return static_cast<CComponentStoreWrapper<T>*>(it->second.get())->GetStore();
	}
	CEntityManager m_entityMgr;
	std::unordered_map<std::type_index, std::unique_ptr<IComponentStoreBase>> m_mapStores;
	std::unique_ptr<CSpatialIndex> m_pSpatialIndex;
};

#pragma warning(pop)
