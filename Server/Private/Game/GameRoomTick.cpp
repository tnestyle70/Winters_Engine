#include "Game/GameRoom.h"

#include "Network/ServerSessionHub.h"
#include "Security/LagCompensation.h"
#include "Server/Private/Data/LoLGameplayDefinitionPack.h"
#include "Server/Private/Data/RuntimeGameplayDefinitionOverlay.h"

#include "Shared/GameSim/Champions/Annie/AnnieGameSim.h"
#include "Shared/GameSim/Champions/Ashe/AsheGameSim.h"
#include "Shared/GameSim/Champions/Ezreal/EzrealGameSim.h"
#include "Shared/GameSim/Champions/Fiora/FioraGameSim.h"
#include "Shared/GameSim/Champions/Irelia/IreliaGameSim.h"
#include "Shared/GameSim/Champions/Jax/JaxGameSim.h"
#include "Shared/GameSim/Champions/Kalista/KalistaGameSim.h"
#include "Shared/GameSim/Champions/Kindred/KindredGameSim.h"
#include "Shared/GameSim/Champions/LeeSin/LeeSinGameSim.h"
#include "Shared/GameSim/Champions/MasterYi/MasterYiGameSim.h"
#include "Shared/GameSim/Champions/Riven/RivenGameSim.h"
#include "Shared/GameSim/Champions/Sylas/SylasGameSim.h"
#include "Shared/GameSim/Champions/Viego/ViegoGameSim.h"
#include "Shared/GameSim/Champions/Yasuo/YasuoGameSim.h"
#include "Shared/GameSim/Champions/Yone/YoneGameSim.h"
#include "Shared/GameSim/Champions/Zed/ZedGameSim.h"
#include "Shared/GameSim/Components/NetEntityIdComponent.h"

#include "GameRoomInternal.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
#include "Shared/GameSim/Systems/ReplicatedEventQueue/ReplicatedEventQueue.h"

#include "Shared/GameSim/Systems/AreaAura/AreaAuraSystem.h"
#include "Shared/GameSim/Systems/AttackChase/AttackChaseSystem.h"
#include "Shared/GameSim/Systems/Buff/BuffSystem.h"
#include "Shared/GameSim/Systems/Combat/CombatActionSystem.h"
#include "Shared/GameSim/Systems/Damage/DamageQueueSystem.h"
#include "Shared/GameSim/Systems/Death/DeathSystem.h"
#include "Shared/GameSim/Systems/JungleAI/JungleAISystem.h"
#include "Shared/GameSim/Systems/Move/MoveSystem.h"
#include "Shared/GameSim/Systems/Recall/RecallSystem.h"
#include "Shared/GameSim/Systems/Gold/GoldIncomeSystem.h"
#include "Shared/GameSim/Systems/Rune/RuneSystem.h"
#include "Shared/GameSim/Systems/SkillCooldown/SkillCooldownSystem.h"
#include "Shared/GameSim/Systems/SpellbookFormOverride/SpellbookFormOverrideSystem.h"
#include "Shared/GameSim/Systems/Stat/StatSystem.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.h"
#include "Shared/GameSim/Systems/WaypointPatrol/WaypointPatrolSystem.h"

#include "ECS/Components/VisionComponents.h"

#include <chrono>
#include <cmath>
#include <mutex>
#include <thread>
#include <vector>

namespace
{
	void TickServerWards(CWorld& world, EntityIdMap& entityMap, const TickContext& tc)
	{
		std::vector<EntityID> expired;
		world.ForEach<VisionSensorComponent>(
			[&](EntityID entity, VisionSensorComponent& ward)
			{
				ward.remainingDuration -= tc.fDt;
				if (ward.remainingDuration <= 0.f)
					expired.push_back(entity);
			});

		for (EntityID entity : expired)
		{
			if (world.HasComponent<NetEntityIdComponent>(entity))
				entityMap.Unbind(world.GetComponent<NetEntityIdComponent>(entity).netId);
			world.DestroyEntity(entity);
		}
	}
}

void CGameRoom::TickThread()
{
	using clock = std::chrono::steady_clock;
	auto next = clock::now();

	while (m_bRunning.load(std::memory_order_relaxed))
	{
		Tick();

		f32_t speedMul = m_simSpeedMul.load(std::memory_order_relaxed);
		if (speedMul < 0.1f) speedMul = 0.1f;
		if (speedMul > 8.f) speedMul = 8.f;
		const auto period = std::chrono::microseconds(
			static_cast<long long>(33333.f / speedMul));

		next += period;
		const auto now = clock::now();
		if (next < now)
			next = now;
		std::this_thread::sleep_until(next);
	}
}

void CGameRoom::Tick()
{
	CServerSessionHub::Instance().DrainIngress(*this);
	std::lock_guard stateLock(m_stateMutex);
	if (m_bGameEnded && m_sessionIds.empty() &&
		!m_pendingReadyMatchID.empty())
	{
		if (TryResetCompletedMatchLocked())
			return;
	}

	if (!IsInGamePhase())
		return;

	if (m_pendingRewindToTick != 0)
		PerformPendingRewind();

	if (m_bSimPaused)
	{
		if (m_simStepBudget == 0)
		{
			TickPausedControlLane();
			return;
		}
		--m_simStepBudget;
	}

	++m_tickIndex;
	m_visibleTickIndex.store(m_tickIndex, std::memory_order_relaxed);

	const GameplayDefinitionPack& definitions = ServerData::GetActiveLoLGameplayDefinitionPack();
	TickContext tc{
		m_tickIndex,
		DeterministicTime::kFixedDt,
		DeterministicTime::TickToSec(m_tickIndex),
		&m_rng, &m_entityMap, NULL_ENTITY, this
	};

	tc.pLagCompensation = m_pLagCompensation.get();
	tc.pDefinitions = &definitions;

	GameplayStatus::TickStatusEffects(m_world, tc);
	GameplayStatus::TickForcedMotions(m_world, tc);
	if (CBuffSystem::PruneExpiredTickBuffs(m_world, tc))
		CStatSystem::Execute(m_world, definitions);
	Phase_DrainCommands(tc);
	Phase_ServerBotAI(tc);
	Phase_ExecuteCommands(tc);
	ExecuteExpiredSkillCharges(*m_pExecutor, m_world, tc);
	Phase_SimulationSystems(tc);
	if (m_pLagCompensation)
		m_pLagCompensation->RecordHistory(m_world, tc.tickIndex);
	Phase_CheckGameEnd(tc);
	Phase_BroadcastEvents(tc);
	Phase_BroadcastSnapshot(tc);
	CaptureKeyframeIfDue(tc);

	// 정상 종료(넥서스 파괴) 저장 보증 — 종료 이벤트가 이 틱의 브로드캐스트/기록에
	// 포함된 뒤에 리플레이를 발행한다 (S030).
	if (m_bGameEnded && !m_bReplayFinalized)
		FinalizeReplayRecorder();
}

void CGameRoom::Phase_CheckGameEnd(TickContext& tc)
{
	(void)tc;
	if (m_bGameEnded)
		return;

	u8_t losingTeam = 0xFFu;
	m_world.ForEach<NexusTag>(
		[&](EntityID entity, NexusTag&)
		{
			if (losingTeam != 0xFFu)
				return;
			if (!m_world.HasComponent<HealthComponent>(entity))
				return;
			const auto& health = m_world.GetComponent<HealthComponent>(entity);
			if (health.fCurrent > 0.f && !health.bIsDead)
				return;
			if (m_world.HasComponent<StructureComponent>(entity))
				losingTeam = static_cast<u8_t>(m_world.GetComponent<StructureComponent>(entity).team);
		});

	if (losingTeam == 0xFFu)
		return;

	m_bGameEnded = true;
	const u8_t winningTeam = losingTeam == 0u ? 1u : 0u;
	m_winningTeam = winningTeam;

	ReplicatedEventComponent event{};
	event.kind = eReplicatedEventKind::EffectTrigger;
	event.effectId = kGlobalGameEndEffect;
	event.sourceNetOverride = 0xFFFFFFFEu; // 글로벌 이벤트 sentinel (엔티티 미참조)
	event.flags = winningTeam;
	EnqueueReplicatedEvent(m_world, event);

	char msg[128]{};
	sprintf_s(msg, "[GameRoom] GameEnd nexusTeam=%u winner=%u tick=%llu\n",
		losingTeam, winningTeam, static_cast<unsigned long long>(m_tickIndex));
	OutputServerAITrace(msg);
}

void CGameRoom::Phase_ExecuteCommands(TickContext& tc)
{
	const auto recordFeedback = [&](const GameCommand& cmd, const CommandExecutionResult& result)
	{
		// This append-only snapshot feedback reconciles client skill-stage
		// prediction. Preserve the latest skill result when move/attack commands
		// are executed in the same server tick.
		if (cmd.sourceSessionId == 0u ||
			cmd.kind != eCommandKind::CastSkill ||
			cmd.slot >= 5u)
			return;

		SkillCommandFeedback feedback{};
		feedback.result = result;
		if (m_world.HasComponent<SkillStateComponent>(cmd.issuerEntity))
		{
			const auto& slot =
				m_world.GetComponent<SkillStateComponent>(cmd.issuerEntity).slots[cmd.slot];
			feedback.authoritativeSkillSlot = cmd.slot;
			feedback.authoritativeSkillStage =
				slot.currentStage == 1u && slot.stageWindow > 0.f ? 1u : 0u;
			if (feedback.authoritativeSkillStage == 1u)
			{
				feedback.stageWindowEndTick = tc.tickIndex + static_cast<u64_t>(
					std::ceil(static_cast<f64_t>(slot.stageWindow) *
						static_cast<f64_t>(DeterministicTime::kTicksPerSecond)));
			}
		}
		m_lastCommandFeedbackBySession[cmd.sourceSessionId][cmd.slot] = feedback;
	};

	for (const auto& cmd : m_pendingExecCommands)
	{
		bool_t bAccepted = false;
		if (TryHandleTeamPing(tc, cmd, bAccepted))
		{
			RecordPendingReplayCommand(
				tc.tickIndex,
				cmd,
				Winters::Replay::eReplayJournalOutcome::SubmittedPlayerInput);
			recordFeedback(
				cmd,
				bAccepted
					? CommandExecutionResult::Accepted(cmd.sequenceNum)
					: CommandExecutionResult::Rejected(
						cmd.sequenceNum,
						eCommandExecutionReason::InvalidPayload));
			continue;
		}
		if (TryHandlePracticeControl(tc, cmd, bAccepted))
		{
			const bool_t bCommitDeferred =
				m_PendingPracticeControlChange.eKind !=
					PracticeControlChangeKind::None &&
				m_PendingPracticeControlChange.uSessionId ==
					cmd.sourceSessionId &&
				m_PendingPracticeControlChange.tCommand.sequenceNum ==
					cmd.sequenceNum;
			if (!bCommitDeferred)
			{
				RecordPendingReplayCommand(
					tc.tickIndex,
					cmd,
					bAccepted
						? Winters::Replay::eReplayJournalOutcome::AcceptedToolCommand
						: Winters::Replay::eReplayJournalOutcome::RejectedToolCommand);
			}
			recordFeedback(
				cmd,
				bAccepted
					? CommandExecutionResult::Accepted(cmd.sequenceNum)
					: CommandExecutionResult::Rejected(
						cmd.sequenceNum, eCommandExecutionReason::ChampionRuleBlocked));
			continue;
		}
		if (TryHandleAIDebugControl(tc, cmd, bAccepted))
		{
			RecordPendingReplayCommand(
				tc.tickIndex,
				cmd,
				bAccepted
					? Winters::Replay::eReplayJournalOutcome::AcceptedToolCommand
					: Winters::Replay::eReplayJournalOutcome::RejectedToolCommand);
			recordFeedback(
				cmd,
				bAccepted
					? CommandExecutionResult::Accepted(cmd.sequenceNum)
					: CommandExecutionResult::Rejected(
						cmd.sequenceNum, eCommandExecutionReason::ChampionRuleBlocked));
			continue;
		}
		RecordPendingReplayCommand(
			tc.tickIndex,
			cmd,
			Winters::Replay::eReplayJournalOutcome::SubmittedPlayerInput);
		const CommandExecutionResult result =
			m_pExecutor->ExecuteCommand(m_world, tc, cmd);
		recordFeedback(cmd, result);
	}
	m_pendingExecCommands.clear();

	if (m_PendingPracticeControlChange.eKind !=
		PracticeControlChangeKind::None)
	{
		const GameCommand controlCommand =
			m_PendingPracticeControlChange.tCommand;
		const bool_t bCommitted = CommitPendingPracticeControlChange(tc);
		RecordPendingReplayCommand(
			tc.tickIndex,
			controlCommand,
			bCommitted
				? Winters::Replay::eReplayJournalOutcome::AcceptedToolCommand
				: Winters::Replay::eReplayJournalOutcome::RejectedToolCommand);
	}
}

void CGameRoom::Phase_SimulationSystems(TickContext& tc)
{
	const GameplayDefinitionPack& definitions =
		tc.pDefinitions ? *tc.pDefinitions : ServerData::GetActiveLoLGameplayDefinitionPack();
	CSpellbookFormOverrideSystem::Execute(m_world, tc);
	CAreaAuraSystem::Execute(m_world, tc);
	CBuffSystem::TickObjectiveEffects(m_world, tc);
	CStatSystem::Execute(m_world, definitions);
	CStatSystem::TickResourceRegeneration(m_world, tc);
	CBuffSystem::AdvanceDurationsAfterStat(m_world, tc);
	CSkillCooldownSystem::Execute(m_world, tc);
	CRecallSystem::Execute(m_world, tc);
	CGoldIncomeSystem::Execute(m_world, tc);
	CWaypointPatrolSystem::Execute(m_world, tc);
	CCombatActionSystem::Execute(m_world, tc);
	CMoveSystem::Execute(m_world, tc);
	CJungleAISystem::Execute(m_world, tc, m_pendingExecCommands);
	CAttackChaseSystem::Execute(m_world, tc, m_pendingExecCommands);
	Phase_ExecuteCommands(tc);
	AnnieGameSim::Tick(m_world, tc);
	AsheGameSim::Tick(m_world, tc);
	EzrealGameSim::Tick(m_world, tc);
	FioraGameSim::Tick(m_world, tc);
	IreliaGameSim::Tick(m_world, tc);
	JaxGameSim::Tick(m_world, tc);
	KalistaGameSim::Tick(m_world, tc);
	LeeSinGameSim::Tick(m_world, tc);
	TickServerWards(m_world, m_entityMap, tc);
	KindredGameSim::Tick(m_world, tc);
	MasterYiGameSim::Tick(m_world, tc);
	RivenGameSim::Tick(m_world, tc);
	SylasGameSim::Tick(m_world, tc);
	ViegoGameSim::Tick(m_world, tc);
	YoneGameSim::Tick(m_world, tc);
	YasuoGameSim::Tick(m_world, tc, m_pExecutor.get());
	ZedGameSim::Tick(m_world, tc);
	Phase_ServerMinionWave(tc);
	Phase_ServerUnitAI(tc);
	Phase_ServerMinionDepenetration(tc);
	Phase_ServerTurretAI(tc);
	Phase_ServerProjectiles(tc);
	CDamageQueueSystem::Execute(m_world, tc);
	CBuffSystem::CleanupDeadObjectiveState(m_world);
	TickPracticeControls(tc);
	CStatSystem::Execute(m_world, definitions);
	CDeathSystem::Execute(m_world, tc);
	Phase_ServerDeathAndRespawn(tc);
}
