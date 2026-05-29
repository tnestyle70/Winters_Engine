#include "Game/ServerEntry.h"

#include <iostream>

std::atomic<bool_t> CServerEntry::s_bInitialized{ false };
CJobSystem CServerEntry::s_JobSystem{};
eJobExecutionMode CServerEntry::s_eExecutionMode{ eJobExecutionMode::ThreadOnly };

bool_t CServerEntry::Initialize(u32_t uWorkerCount, bool_t bEnableFiberShell)
{
	bool_t expected = false;
	//이게 어떤 함수를 호출한 거고, memory_order_acq_rel은 무슨 의미를 가짐?
	if (!s_bInitialized.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
	{
		std::cerr << "[ServerEntry] Initialize called twice.\n";
		return false;
	}

	s_JobSystem.Initialize(uWorkerCount);

	//s_eExecutionMode = bEnableFiberShell

	return bool_t();
}

void CServerEntry::Shutdown()
{}

CJobSystem* CServerEntry::Get_JobSystem()
{
	return nullptr;
}

eJobExecutionMode CServerEntry::Get_ExecutionMode()
{
	return eJobExecutionMode();
}

bool_t CServerEntry::IsInitialized()
{
	return bool_t();
}
