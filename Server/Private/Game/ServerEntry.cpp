#include "Game/ServerEntry.h"

#include <iostream>

std::atomic<CServerEntry::eLifecycleState> CServerEntry::s_eLifecycleState{
	CServerEntry::eLifecycleState::Stopped };
CJobSystem CServerEntry::s_JobSystem{};
std::atomic<eJobExecutionMode> CServerEntry::s_eExecutionMode{
	eJobExecutionMode::ThreadOnly };

bool_t CServerEntry::Initialize(u32_t uWorkerCount, eJobExecutionMode eMode)
{
	eLifecycleState expected = eLifecycleState::Stopped;
	if (!s_eLifecycleState.compare_exchange_strong(
		expected,
		eLifecycleState::Initializing,
		std::memory_order_acq_rel,
		std::memory_order_acquire))
	{
		std::cerr << "[ServerEntry] Initialize called twice.\n";
		return false;
	}

	try
	{
		s_eExecutionMode.store(eMode, std::memory_order_release);
		s_JobSystem.SetExecutionMode(eMode);
		s_JobSystem.Initialize(uWorkerCount);
		s_eLifecycleState.store(eLifecycleState::Ready, std::memory_order_release);
		return true;
	}
	catch (const std::exception& exception)
	{
		std::cerr << "[ServerEntry] JobSystem initialization failed: "
			<< exception.what() << '\n';
		s_JobSystem.Shutdown();
		s_eExecutionMode.store(eJobExecutionMode::ThreadOnly, std::memory_order_release);
		s_eLifecycleState.store(eLifecycleState::Stopped, std::memory_order_release);
		return false;
	}
	catch (...)
	{
		std::cerr << "[ServerEntry] JobSystem initialization failed.\n";
		s_JobSystem.Shutdown();
		s_eExecutionMode.store(eJobExecutionMode::ThreadOnly, std::memory_order_release);
		s_eLifecycleState.store(eLifecycleState::Stopped, std::memory_order_release);
		return false;
	}
}

void CServerEntry::Shutdown()
{
	eLifecycleState expected = eLifecycleState::Ready;
	if (!s_eLifecycleState.compare_exchange_strong(
		expected,
		eLifecycleState::Stopping,
		std::memory_order_acq_rel,
		std::memory_order_acquire))
		return;

	s_JobSystem.Shutdown();
	s_eExecutionMode.store(eJobExecutionMode::ThreadOnly, std::memory_order_release);
	s_eLifecycleState.store(eLifecycleState::Stopped, std::memory_order_release);
}

CJobSystem* CServerEntry::Get_JobSystem()
{
	return IsInitialized() ? &s_JobSystem : nullptr;
}

eJobExecutionMode CServerEntry::Get_ExecutionMode()
{
	return s_eExecutionMode.load(std::memory_order_acquire);
}

bool_t CServerEntry::IsInitialized()
{
	return s_eLifecycleState.load(std::memory_order_acquire) ==
		eLifecycleState::Ready;
}
