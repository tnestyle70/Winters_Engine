#include "WintersPCH.h"
#include "ECS/CCommandBuffer.h"
#include "ECS/World.h"

void CCommandBuffer::DeferCreate(std::function<void(CWorld&, EntityID)> initFn)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_vecCreates.push_back(move(initFn));
}

void CCommandBuffer::DeferCreate(std::function<void(CWorld&, EntityHandle)> initFn)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_vecHandleCreates.push_back(move(initFn));
}

void CCommandBuffer::DeferDestroy(EntityID entity)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_vecDestroys.push_back(entity);
}

void CCommandBuffer::DeferDestroy(EntityHandle entity)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_vecHandleDestroys.push_back(entity);
}

void CCommandBuffer::DeferCommand(std::function<void(CWorld&)> cmd)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_vecCommands.push_back(move(cmd));
}

void CCommandBuffer::Flush(CWorld& world)
{
	std::vector<std::function<void(CWorld&, EntityID)>> creates;
	std::vector<std::function<void(CWorld&, EntityHandle)>> handleCreates;
	std::vector<EntityID> destroys;
	std::vector<EntityHandle> handleDestroys;
	std::vector<std::function<void(CWorld&)>> commands;

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		creates.swap(m_vecCreates);
		handleCreates.swap(m_vecHandleCreates);
		destroys.swap(m_vecDestroys);
		handleDestroys.swap(m_vecHandleDestroys);
		commands.swap(m_vecCommands);
	}

	for (auto& fn : creates)
	{
		EntityID e = world.CreateEntity();
		fn(world, e);
	}
	for (auto& fn : handleCreates)
	{
		EntityHandle e = world.CreateEntityHandle();
		fn(world, e);
	}
	for (auto& cmd : commands)
		cmd(world);
	for (EntityID e : destroys)
	{
		if (world.IsAlive(e))
			world.DestroyEntity(e);
	}
	for (EntityHandle e : handleDestroys)
	{
		world.DestroyEntity(e);
	}
}

bool CCommandBuffer::IsEmpty() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_vecCreates.empty() &&
		m_vecHandleCreates.empty() &&
		m_vecDestroys.empty() &&
		m_vecHandleDestroys.empty() &&
		m_vecCommands.empty();
}

uint32_t CCommandBuffer::GetPendingCommandCount() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return static_cast<uint32_t>(
		m_vecCreates.size() +
		m_vecHandleCreates.size() +
		m_vecDestroys.size() +
		m_vecHandleDestroys.size() +
		m_vecCommands.size());
}
