#pragma once

#include <cassert>
#include <cstdint>
#include <vector>

// EntityID remains an index-only legacy id so the current sparse component
// stores keep working. New systems should keep EntityHandle when lifetime
// safety matters.
using EntityID = uint32_t;
using EntityGeneration = uint32_t;

constexpr EntityID NULL_ENTITY = 0;
constexpr EntityGeneration NULL_ENTITY_GENERATION = 0;

struct EntityHandle
{
	uint64_t value = 0;

	bool IsValid() const
	{
		return value != 0 && GetGeneration() != NULL_ENTITY_GENERATION;
	}

	EntityID GetIndex() const
	{
		return static_cast<EntityID>(value & 0xFFFFFFFFull);
	}

	EntityGeneration GetGeneration() const
	{
		return static_cast<EntityGeneration>((value >> 32) & 0xFFFFFFFFull);
	}

	uint64_t ToU64() const
	{
		return value;
	}

	static EntityHandle Make(EntityID id, EntityGeneration generation)
	{
		EntityHandle handle{};
		if (id == NULL_ENTITY || generation == NULL_ENTITY_GENERATION)
			return handle;

		handle.value =
			(static_cast<uint64_t>(generation) << 32) |
			static_cast<uint64_t>(id);
		return handle;
	}

	static EntityHandle FromU64(uint64_t packedValue)
	{
		EntityHandle handle{};
		handle.value = packedValue;
		return handle;
	}

	bool operator==(const EntityHandle& rhs) const
	{
		return value == rhs.value;
	}

	bool operator!=(const EntityHandle& rhs) const
	{
		return value != rhs.value;
	}
};

constexpr EntityHandle NULL_ENTITY_HANDLE{};

class CEntityManager
{
public:
	EntityID Create()
	{
		EntityID id;
		if (!m_vecFreeList.empty())
		{
			id = m_vecFreeList.back();
			m_vecFreeList.pop_back();
		}
		else
		{
			id = m_iNextID++;
		}

		if (id >= m_vecAlive.size())
		{
			m_vecAlive.resize(id + 1, 0);
			m_vecGenerations.resize(id + 1, 1);
		}
		if (m_vecGenerations[id] == NULL_ENTITY_GENERATION)
			m_vecGenerations[id] = 1;

		m_vecAlive[id] = 1;
		++m_iAliveCount;
		return id;
	}

	EntityHandle CreateHandle()
	{
		const EntityID id = Create();
		return GetHandle(id);
	}

	void Destroy(EntityID id)
	{
		assert(id < m_vecAlive.size() && m_vecAlive[id]);
		m_vecAlive[id] = 0;
		BumpGeneration(id);
		m_vecFreeList.push_back(id);
		--m_iAliveCount;
	}

	bool Destroy(EntityHandle handle)
	{
		EntityID id = NULL_ENTITY;
		if (!TryResolve(handle, id))
			return false;

		Destroy(id);
		return true;
	}

	bool IsAlive(EntityID id) const
	{
		return id < m_vecAlive.size() && m_vecAlive[id] != 0;
	}

	bool IsAlive(EntityHandle handle) const
	{
		EntityID id = NULL_ENTITY;
		return TryResolve(handle, id);
	}

	EntityGeneration GetGeneration(EntityID id) const
	{
		if (id >= m_vecGenerations.size())
			return NULL_ENTITY_GENERATION;
		return m_vecGenerations[id];
	}

	EntityHandle GetHandle(EntityID id) const
	{
		if (!IsAlive(id))
			return NULL_ENTITY_HANDLE;
		return EntityHandle::Make(id, GetGeneration(id));
	}

	bool TryResolve(EntityHandle handle, EntityID& outEntity) const
	{
		if (!handle.IsValid())
			return false;

		const EntityID id = handle.GetIndex();
		if (!IsAlive(id))
			return false;
		if (GetGeneration(id) != handle.GetGeneration())
			return false;

		outEntity = id;
		return true;
	}

	EntityID Resolve(EntityHandle handle) const
	{
		EntityID id = NULL_ENTITY;
		return TryResolve(handle, id) ? id : NULL_ENTITY;
	}

	uint32_t GetAliveCount() const
	{
		return m_iAliveCount;
	}

private:
	void BumpGeneration(EntityID id)
	{
		if (id >= m_vecGenerations.size())
			m_vecGenerations.resize(id + 1, 1);

		EntityGeneration next = m_vecGenerations[id] + 1;
		if (next == NULL_ENTITY_GENERATION)
			next = 1;
		m_vecGenerations[id] = next;
	}

	EntityID m_iNextID{ 1 };
	std::vector<uint8_t> m_vecAlive;
	std::vector<EntityGeneration> m_vecGenerations;
	std::vector<EntityID> m_vecFreeList;
	uint32_t m_iAliveCount{ 0 };
};
