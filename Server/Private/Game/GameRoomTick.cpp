#include "Game/GameRoom.h"

#include "Security/LagCompensation.h"

#include "Shared/GameSim/Champions/Annie/AnnieGameSim.h"
#include "Shared/GameSim/Champions/Ashe/AsheGameSim.h"
#include "Shared/GameSim/Champions/Fiora/FioraGameSim.h"
#include "Shared/GameSim/Champions/Irelia/IreliaGameSim.h"
#include "Shared/GameSim/Champions/Jax/JaxGameSim.h"
#include "Shared/GameSim/Champions/Kindred/KindredGameSim.h"
#include "Shared/GameSim/Champions/LeeSin/LeeSinGameSim.h"
#include "Shared/GameSim/Champions/MasterYi/MasterYiGameSim.h"
#include "Shared/GameSim/Champions/Viego/ViegoGameSim.h"
#include "Shared/GameSim/Champions/Yasuo/YasuoGameSim.h"
#include "Shared/GameSim/Champions/Yone/YoneGameSim.h"
#include "Shared/GameSim/Champions/Zed/ZedGameSim.h"

#include "Shared/GameSim/Systems/AreaAura/AreaAuraSystem.h"
#include "Shared/GameSim/Systems/AttackChase/AttackChaseSystem.h"
#include "Shared/GameSim/Systems/Buff/BuffSystem.h"
#include "Shared/GameSim/Systems/Combat/CombatActionSystem.h"
#include "Shared/GameSim/Systems/Damage/DamageQueueSystem.h"
#include "Shared/GameSim/Systems/Death/DeathSystem.h"
#include "Shared/GameSim/Systems/Move/MoveSystem.h"
#include "Shared/GameSim/Systems/Recall/RecallSystem.h"
#include "Shared/GameSim/Systems/SkillCooldown/SkillCooldownSystem.h"
#include "Shared/GameSim/Systems/SpellbookFormOverride/SpellbookFormOverrideSystem.h"
#include "Shared/GameSim/Systems/Stat/StatSystem.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.h"
#include "Shared/GameSim/Systems/WaypointPatrol/WaypointPatrolSystem.h"

#include "ECS/Systems/GameplayCollisionSystem.h"

#include <chrono>
#include <mutex>
#include <thread>

void CGameRoom::TickThread()
{
	using clock = std::chrono::steady_clock;
	auto next = clock::now();
	const auto period = std::chrono::microseconds(33333);

	while (m_bRunning.load(std::memory_order_relaxed))
	{
		Tick();
		next += period;
		std::this_thread::sleep_until(next);
	}
}

void CGameRoom::Tick()
{
	std::lock_guard stateLock(m_stateMutex);

	if (m_roomPhase != eRoomPhase::InGame)
		return;

	++m_tickIndex;
	m_visibleTickIndex.store(m_tickIndex, std::memory_order_relaxed);

	TickContext tc{
		m_tickIndex,
		DeterministicTime::kFixedDt,
		DeterministicTime::TickToSec(m_tickIndex),
		&m_rng, &m_entityMap, NULL_ENTITY, this
	};

	tc.pLagCompensation = m_pLagCompensation.get();

	Phase_DrainCommands(tc);
	Phase_ServerBotAI(tc);
	Phase_ExecuteCommands(tc);
	Phase_SimulationSystems(tc);
	if (m_pLagCompensation)
		m_pLagCompensation->RecordHistory(m_world, tc.tickIndex);
	Phase_BroadcastEvents(tc);
	Phase_BroadcastSnapshot(tc);
}

void CGameRoom::Phase_ExecuteCommands(TickContext& tc)
{
	for (const auto& cmd : m_pendingExecCommands)
		m_pExecutor->ExecuteCommand(m_world, tc, cmd);
	m_pendingExecCommands.clear();
}

void CGameRoom::Phase_SimulationSystems(TickContext& tc)
{
	GameplayStatus::TickStatusEffects(m_world, tc);
	CSpellbookFormOverrideSystem::Execute(m_world, tc);
	CAreaAuraSystem::Execute(m_world, tc);
	CStatSystem::Execute(m_world);
	CBuffSystem::Execute(m_world, tc);
	CSkillCooldownSystem::Execute(m_world, tc);
	CRecallSystem::Execute(m_world, tc);
	CWaypointPatrolSystem::Execute(m_world, tc);
	CCombatActionSystem::Execute(m_world, tc);
	CMoveSystem::Execute(m_world, tc);
	CAttackChaseSystem::Execute(m_world, tc, m_pendingExecCommands);
	Phase_ExecuteCommands(tc);
	AnnieGameSim::Tick(m_world, tc);
	AsheGameSim::Tick(m_world, tc);
	FioraGameSim::Tick(m_world, tc);
	IreliaGameSim::Tick(m_world, tc);
	JaxGameSim::Tick(m_world, tc);
	LeeSinGameSim::Tick(m_world, tc);
	KindredGameSim::Tick(m_world, tc);
	MasterYiGameSim::Tick(m_world, tc);
	ViegoGameSim::Tick(m_world, tc);
	YoneGameSim::Tick(m_world, tc);
	YasuoGameSim::Tick(m_world, tc);
	ZedGameSim::Tick(m_world, tc);
	Phase_ServerMinionWave(tc);
	Phase_ServerMinionAI(tc);
	if (m_pGameplayCollision)
		m_pGameplayCollision->Execute(m_world, tc.fDt);
	Phase_ServerTurretAI(tc);
	Phase_ServerProjectiles(tc);
	CDamageQueueSystem::Execute(m_world, tc);
	CStatSystem::Execute(m_world);
	CDeathSystem::Execute(m_world, tc);
	Phase_ServerDeathAndRespawn(tc);
}
