#pragma once
#include "WintersAPI.h"
#include "ECS/Entity.h"
#include <functional>
#include <mutex>
#include <vector>
//CommandBuffer - 지연 실행 버퍼
//System이 for each로 컴포넌트를 순회하는 도중에 엔티티를 생성하거나 삭제하면,
//순회중인 컨테이너(dense array)가 변경되면서 이터레이터가 무효화됨
//순회중에 나중에 할 명령을 기록하고 순회가 끝난 뒤에 Flush에서 한 번에 실행

class CWorld;

class CCommandBuffer
{
public:
	WINTERS_ENGINE void DeferCreate(std::function<void(CWorld&, EntityID)> initFn);
	WINTERS_ENGINE void DeferCreate(std::function<void(CWorld&, EntityHandle)> initFn);
	WINTERS_ENGINE void DeferDestroy(EntityID entity);
	WINTERS_ENGINE void DeferDestroy(EntityHandle entity);
	WINTERS_ENGINE void DeferCommand(std::function<void(CWorld&)> cmd);
	WINTERS_ENGINE void Flush(CWorld& world);
	WINTERS_ENGINE bool IsEmpty() const;
	WINTERS_ENGINE uint32_t GetPendingCommandCount() const;
private:
	std::vector<std::function<void(CWorld&, EntityID)>> m_vecCreates;
	std::vector<std::function<void(CWorld&, EntityHandle)>> m_vecHandleCreates;
	std::vector<EntityID> m_vecDestroys;
	std::vector<EntityHandle> m_vecHandleDestroys;
	std::vector<std::function<void(CWorld&)>> m_vecCommands;
	mutable std::mutex m_mutex;
};
