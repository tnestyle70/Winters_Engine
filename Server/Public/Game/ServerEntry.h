#pragma once

#include "Core/Fiber/FiberTypes.h"
#include "Core/JobSystem.h"
#include "WintersTypes.h"

#include <atomic>

class CServerEntry
{
public:
	CServerEntry(const CServerEntry&) = delete;
	CServerEntry& operator=(const CServerEntry&) = delete;

	static bool_t Initialize(
		u32_t uWorkerCount = 0,
		eJobExecutionMode eMode = eJobExecutionMode::ThreadOnly);
	static bool_t Initialize(u32_t uWorkerCount, bool_t bEnableFiberShell)
	{
		return Initialize(
			uWorkerCount,
			bEnableFiberShell
				? eJobExecutionMode::FiberShell
				: eJobExecutionMode::ThreadOnly);
	}
	static void Shutdown();

	static CJobSystem* Get_JobSystem();
	static eJobExecutionMode Get_ExecutionMode();
	static bool_t IsInitialized();

private:
	enum class eLifecycleState : u8_t
	{
		Stopped = 0,
		Initializing,
		Ready,
		Stopping,
	};

	CServerEntry() = default;
	~CServerEntry() = default;

	static std::atomic<eLifecycleState> s_eLifecycleState;
	static CJobSystem s_JobSystem;
	static std::atomic<eJobExecutionMode> s_eExecutionMode;
};
