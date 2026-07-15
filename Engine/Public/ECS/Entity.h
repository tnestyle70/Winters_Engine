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
constexpr EntityGeneration FIRST_ENTITY_GENERATION = 1;

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
		if (m_iFreeHead != NULL_ENTITY)
		{
			id = m_iFreeHead;
			EntitySlot& slot = m_vecSlots[id];
			m_iFreeHead = slot.nextFree;
			slot.nextFree = NULL_ENTITY;
			slot.generation = NextAliveGeneration(slot.generation);
		}
		else
		{
			EnsureNullSlot();

			id = static_cast<EntityID>(m_vecSlots.size());
			m_vecSlots.push_back({ FIRST_ENTITY_GENERATION, NULL_ENTITY });
		}
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
		assert(IsAlive(id));
		if (!IsAlive(id))
			return;

		EntitySlot& slot = m_vecSlots[id];
		slot.generation = NextDeadGeneration(slot.generation);
		slot.nextFree = m_iFreeHead;
		m_iFreeHead = id;
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
		return id < m_vecSlots.size() && IsAliveGeneration(m_vecSlots[id].generation);
	}

	bool IsAlive(EntityHandle handle) const
	{
		EntityID id = NULL_ENTITY;
		return TryResolve(handle, id);
	}

	EntityGeneration GetGeneration(EntityID id) const
	{
		if (id >= m_vecSlots.size())
			return NULL_ENTITY_GENERATION;
		return m_vecSlots[id].generation;
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
		if (id >= m_vecSlots.size())
			return false;

		const EntityGeneration generation = m_vecSlots[id].generation;
		if (generation != handle.GetGeneration())
			return false;
		if (!IsAliveGeneration(generation))
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

	// Chrono Break: bit-exact allocator snapshot access.
	// slots+freeHead를 바이트 그대로 복원하면 이후 CreateEntity의 id/generation 할당 순서가 정확히 재현된다.
	struct EntitySlot
	{
		EntityGeneration generation = NULL_ENTITY_GENERATION;
		EntityID nextFree = NULL_ENTITY;
	};
	const std::vector<EntitySlot>& RawSlots() const { return m_vecSlots; }
	EntityID RawFreeHead() const { return m_iFreeHead; }
	uint32_t RawAliveCount() const { return m_iAliveCount; }
	void RestoreRaw(std::vector<EntitySlot>&& slots, EntityID freeHead, uint32_t aliveCount)
	{
		m_vecSlots = std::move(slots);
		m_iFreeHead = freeHead;
		m_iAliveCount = aliveCount;
		EnsureNullSlot();
	}
	void SwapRawState(CEntityManager& other) noexcept
	{
		m_vecSlots.swap(other.m_vecSlots);
		const EntityID freeHead = m_iFreeHead;
		m_iFreeHead = other.m_iFreeHead;
		other.m_iFreeHead = freeHead;
		const uint32_t aliveCount = m_iAliveCount;
		m_iAliveCount = other.m_iAliveCount;
		other.m_iAliveCount = aliveCount;
	}

private:

	static bool IsAliveGeneration(EntityGeneration generation)
	{
		return (generation & 1u) != 0;
	}

	static EntityGeneration NextAliveGeneration(EntityGeneration generation)
	{
		if (generation == NULL_ENTITY_GENERATION)
			return FIRST_ENTITY_GENERATION;
		if (IsAliveGeneration(generation))
			return generation;

		EntityGeneration next = generation + 1;
		if (next == NULL_ENTITY_GENERATION)
			next = FIRST_ENTITY_GENERATION;
		return next;
	}

	static EntityGeneration NextDeadGeneration(EntityGeneration generation)
	{
		EntityGeneration next = generation + 1;
		if (next == NULL_ENTITY_GENERATION)
			next = 2;
		if (IsAliveGeneration(next))
		{
			++next;
			if (next == NULL_ENTITY_GENERATION)
				next = 2;
		}
		return next;
	}

	void EnsureNullSlot()
	{
		if (m_vecSlots.empty())
			m_vecSlots.push_back({});
	}

	std::vector<EntitySlot> m_vecSlots;
	EntityID m_iFreeHead{ NULL_ENTITY };
	uint32_t m_iAliveCount{ 0 };
};
