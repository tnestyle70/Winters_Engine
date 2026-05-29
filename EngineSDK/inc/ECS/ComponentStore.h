#pragma once
#include "ECS/Entity.h"

#include <utility>

template<typename T>

class CComponentStore
{
public:
	T& Add(EntityID entity, const T& component = T{})
	{
		assert(!Has(entity) && "Entity already has this component");
		if (entity >= m_vecSparse.size())
			m_vecSparse.resize(entity + 1, INVALID);

		uint32_t denseidx = static_cast<uint32_t>(m_vecDense.size());
		m_vecSparse[entity] = denseidx;
		m_vecDense.push_back(entity);
		m_vecData.push_back(component);
		return m_vecData.back();
	}

	void Remove(EntityID entity)
	{
		assert(Has(entity) && "Entity does not have this component");
		uint32_t index = m_vecSparse[entity];
		uint32_t last = static_cast<uint32_t>(m_vecDense.size()) - 1;

		EntityID lastEntity = m_vecDense[last];
		m_vecDense[index] = lastEntity;
		m_vecData[index] = std::move(m_vecData[last]);
		m_vecSparse[lastEntity] = index;

		m_vecDense.pop_back();
		m_vecData.pop_back();
		m_vecSparse[entity] = INVALID;
	}

	T& Get(EntityID entity) 
	{ 
		assert(Has(entity));
		return m_vecData[m_vecSparse[entity]];
	}
	const T& Get(EntityID entity) const
	{
		assert(Has(entity));
		return m_vecData[m_vecSparse[entity]];
	}
	
	bool Has(EntityID entity) const
	{
		return entity < m_vecSparse.size() && m_vecSparse[entity] != INVALID;
	}

	uint32_t Count() const { return static_cast<uint32_t>(m_vecDense.size()); }
	const EntityID* Entities() const { return m_vecDense.data(); }
	T* Data() { return m_vecData.data(); }
	const T* Data() const { return m_vecData.data(); }

private:
	static constexpr uint32_t INVALID = UINT32_MAX;
	std::vector<uint32_t> m_vecSparse;
	std::vector<EntityID> m_vecDense;
	std::vector<T> m_vecData; //연속된 메모리 사용!
};
