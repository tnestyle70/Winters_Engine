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

	static bool_t Initialize(u32_t uWorkerCount = 0, bool_t bEnableFiberShell = true);
	static void Shutdown();

	static CJobSystem* Get_JobSystem();
	static eJobExecutionMode Get_ExecutionMode();
	static bool_t IsInitialized();

private:
	CServerEntry() = default;
	~CServerEntry() = default;

	static std::atomic<bool_t> s_bInitialized;
	static CJobSystem s_JobSystem;
	static eJobExecutionMode s_eExecutionMode;
};