#pragma once

#include "WintersTypes.h"
#include "WintersMath.h"
#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Definitions/LoLMatchContext.h"
#include "Shared/GameSim/Definitions/TeamPingDef.h"
#include "Shared/GameSim/Systems/ChampionAI/ChampionAIInfluenceMap.h"
#include "Shared/GameSim/Systems/ChampionAI/ChampionAIResearchTypes.h"

#include <cstddef>
#include <type_traits>

struct ChampionAIShadowPolicyArtifactV1;

enum class eChampionAIState : u8_t
{
	MoveToOuterTurret,
	WaitForWave,
	LaneCombat,
	Diving,
	Retreat,
	Recalling,
	Dead,
	GroupMidDefense,
};

enum class eChampionAIAction : u8_t
{
	MoveToSafeAnchor,
	FollowWave,
	AttackMinion,
	AttackChampion,
	AttackStructure,
	UseFlashEscape,
	Retreat,
	Recall,
};

enum class eChampionAIIntent : u8_t
{
	FarmMinion,
	AttackChampion,
	ExecuteDive,
	SiegeStructure,
	Retreat,
	Recall,
	DefendMid,
};

enum class eChampionAIDebugControlMode : u8_t
{
	Observe,
	SingleDecision,
	ForceAction,
	TuneRuntime,
};

enum class eChampionAIDivePhase : u8_t
{
	None,
	EngageQ,
	ArmW,
	BasicAttack,
	ExtraBasicAttack,
	FlashExit,
	ExitMove,
};

enum class eChampionAIDecisionBlockReason : u8_t
{
	None,
	NoTarget,
	TargetDead,
	TargetUntargetable,
	TargetOutOfRange,
	SelfLowHp,
	TurretDanger,
	SkillCooldown,
	FlashNotReady,
	ActionLocked,
	StateBlocked,
	InvalidPath,
	CommandRejected,
	PolicyCastInterval,
	RuntimeSkillCooldown,
};

enum class eChampionAITuningId : u8_t
{
	ChampionScanRange,
	MinionScanRange,
	StructureScanRange,
	LeashRange,
	RetreatHpRatio,
	ReengageHpRatio,
	ChampionScoreMargin,
	TurretDangerThreshold,
	PostComboBASelfHpMinRatio,
	PostComboBAEnemyHpMargin,
	PostComboBAWindow,
	LowHpExecuteThreshold,
	DiveScanRange,
	DiveExtraBAWindow,
	SkillCastMinInterval,
	FollowWaveSearchRange,
	FarmPriority,
	Count,
};

static_assert(static_cast<u8_t>(eChampionAITuningId::SkillCastMinInterval) == 14u);
static_assert(static_cast<u8_t>(eChampionAITuningId::FollowWaveSearchRange) == 15u);
static_assert(static_cast<u8_t>(eChampionAITuningId::FarmPriority) == 16u);
static_assert(static_cast<u8_t>(eChampionAITuningId::Count) == 17u);

struct ChampionAIDecisionTraceEntry
{
	u64_t tick = 0;
	eChampionAIState state = eChampionAIState::MoveToOuterTurret;
	eChampionAIIntent intent = eChampionAIIntent::FarmMinion;
	eChampionAIAction action = eChampionAIAction::MoveToSafeAnchor;
	eChampionAIDivePhase divePhase = eChampionAIDivePhase::None;
	eChampionAIDecisionBlockReason blockReason = eChampionAIDecisionBlockReason::None;
	u8_t commandKind = 0;
	u8_t commandSlot = 0;
	u8_t reservedTargetAlignment = 0u;
	EntityID target = NULL_ENTITY;
	Vec3 commandPos{ 0.f, 0.f, 0.f };
	f32_t championScore = 0.f;
	f32_t farmScore = 0.f;
	f32_t structureScore = 0.f;
	f32_t selfHpRatio = 1.f;
	f32_t enemyHpRatio = 1.f;
	f32_t enemyDistance = 999.f;
	f32_t turretDanger = 0.f;
	f32_t retreatScore = 0.f;
	f32_t skillCastIntervalSec = 5.f;
	f32_t skillCastIntervalRemainingSec = 0.f;
	u32_t legalCandidateMask = 0u;
	u32_t illegalCandidateMask = 0u;
	u32_t commandSequence = 0u;
	u16_t executorReason = 0u;
	u8_t executorState = static_cast<u8_t>(AiExecutorStateV1::Unknown);
	u8_t comboStep = 0u;
	u64_t shadowPolicyRevision = 0u;
	u64_t shadowPolicySha256Prefix = 0u;
	f32_t shadowLogits[kAiDecisionCandidateCapacityV1]{};
	f32_t shadowSelectedMargin = 0.f;
	f32_t shadowTopFeatureContribution = 0.f;
	u32_t shadowLegalCandidateMask = 0u;
	u16_t shadowTopFeatureIndex = 0xFFFFu;
	u8_t shadowStatus = 0u;
	u8_t shadowActiveCandidateKind = 0u;
	u8_t shadowSelectedCandidateKind = 0u;
	bool_t bShadowDisagreed = false;
	u8_t reservedTail[6]{};
};

static_assert(sizeof(ChampionAIDecisionTraceEntry) == 144u);
static_assert(offsetof(ChampionAIDecisionTraceEntry, target) == 16u);
static_assert(offsetof(ChampionAIDecisionTraceEntry, shadowPolicyRevision) == 88u);
static_assert(offsetof(ChampionAIDecisionTraceEntry, bShadowDisagreed) == 137u);

struct ChampionAITuningParam
{
	f32_t fDefault = 0.f;
	f32_t fCurrent = 0.f;
	f32_t fMin = 0.f;
	f32_t fMax = 1.f;
	bool_t bOverride = false;
	u8_t reservedTail[3]{};
};

struct ChampionAITuning
{
	bool_t bOverrideProfile = false;
	u8_t reservedParamsAlignment[3]{};
	ChampionAITuningParam championScanRange{ 9.f, 9.f, 1.f, 40.f, false };
	ChampionAITuningParam minionScanRange{ 12.f, 12.f, 1.f, 40.f, false };
	ChampionAITuningParam structureScanRange{ 18.f, 18.f, 1.f, 60.f, false };
	ChampionAITuningParam leashRange{ 14.f, 14.f, 1.f, 60.f, false };
	ChampionAITuningParam retreatHpRatio{ 0.10f, 0.10f, 0.01f, 0.90f, false };
	ChampionAITuningParam reengageHpRatio{ 0.25f, 0.25f, 0.01f, 1.f, false };
	ChampionAITuningParam championScoreMargin{ 0.10f, 0.10f, 0.f, 1.f, false };
	ChampionAITuningParam turretDangerThreshold{ 0.85f, 0.85f, 0.f, 1.f, false };
	ChampionAITuningParam postComboBASelfHpMinRatio{ 0.10f, 0.10f, 0.f, 1.f, false };
	ChampionAITuningParam postComboBAEnemyHpMargin{ 0.f, 0.f, -1.f, 1.f, false };
	ChampionAITuningParam postComboBAWindow{ 0.80f, 0.80f, 0.f, 5.f, false };
	ChampionAITuningParam lowHpExecuteThreshold{ 0.10f, 0.10f, 0.01f, 0.50f, false };
	ChampionAITuningParam diveScanRange{ 11.f, 11.f, 1.f, 40.f, false };
	ChampionAITuningParam diveExtraBAWindow{ 1.80f, 1.80f, 0.f, 5.f, false };
	ChampionAITuningParam skillCastMinInterval{ 5.f, 5.f, 0.f, 15.f, false };
	ChampionAITuningParam followWaveSearchRange{ 80.f, 80.f, 10.f, 120.f, false };
	ChampionAITuningParam farmPriority{ 1.f, 1.f, 0.f, 3.f, false };
};

inline ChampionAITuningParam* ResolveChampionAITuningParam(
	ChampionAITuning& tuning,
	eChampionAITuningId tuningId)
{
	switch (tuningId)
	{
	case eChampionAITuningId::ChampionScanRange:
		return &tuning.championScanRange;
	case eChampionAITuningId::MinionScanRange:
		return &tuning.minionScanRange;
	case eChampionAITuningId::StructureScanRange:
		return &tuning.structureScanRange;
	case eChampionAITuningId::LeashRange:
		return &tuning.leashRange;
	case eChampionAITuningId::RetreatHpRatio:
		return &tuning.retreatHpRatio;
	case eChampionAITuningId::ReengageHpRatio:
		return &tuning.reengageHpRatio;
	case eChampionAITuningId::ChampionScoreMargin:
		return &tuning.championScoreMargin;
	case eChampionAITuningId::TurretDangerThreshold:
		return &tuning.turretDangerThreshold;
	case eChampionAITuningId::PostComboBASelfHpMinRatio:
		return &tuning.postComboBASelfHpMinRatio;
	case eChampionAITuningId::PostComboBAEnemyHpMargin:
		return &tuning.postComboBAEnemyHpMargin;
	case eChampionAITuningId::PostComboBAWindow:
		return &tuning.postComboBAWindow;
	case eChampionAITuningId::LowHpExecuteThreshold:
		return &tuning.lowHpExecuteThreshold;
	case eChampionAITuningId::DiveScanRange:
		return &tuning.diveScanRange;
	case eChampionAITuningId::DiveExtraBAWindow:
		return &tuning.diveExtraBAWindow;
	case eChampionAITuningId::SkillCastMinInterval:
		return &tuning.skillCastMinInterval;
	case eChampionAITuningId::FollowWaveSearchRange:
		return &tuning.followWaveSearchRange;
	case eChampionAITuningId::FarmPriority:
		return &tuning.farmPriority;
	default:
		return nullptr;
	}
}

inline const ChampionAITuningParam* ResolveChampionAITuningParam(
	const ChampionAITuning& tuning,
	eChampionAITuningId tuningId)
{
	return ResolveChampionAITuningParam(
		const_cast<ChampionAITuning&>(tuning), tuningId);
}

inline constexpr u32_t kChampionAIActionBitMoveToSafeAnchor = 1u << 0;
inline constexpr u32_t kChampionAIActionBitFollowWave = 1u << 1;
inline constexpr u32_t kChampionAIActionBitAttackUnit = 1u << 2;
inline constexpr u32_t kChampionAIActionBitAttackChampion = 1u << 3;
inline constexpr u32_t kChampionAIActionBitAttackStructure = 1u << 4;
inline constexpr u32_t kChampionAIActionBitRetreat = 1u << 5;
inline constexpr u32_t kChampionAIActionBitUseFlashEscape = 1u << 6;

inline constexpr u32_t kChampionAIDebugPresentFlag = 1u << 7;
inline constexpr u32_t kChampionAIStateShift = 8u;
inline constexpr u32_t kChampionAIStateMask = 0xFu << kChampionAIStateShift;
inline constexpr u32_t kChampionAIActionShift = 12u;
inline constexpr u32_t kChampionAIActionMask = 0xFu << kChampionAIActionShift;
inline constexpr u32_t kChampionAIIntentShift = 16u;
inline constexpr u32_t kChampionAIIntentMask = 0xFu << kChampionAIIntentShift;
inline constexpr u32_t kChampionAIAvailableActionShift = 20u;
inline constexpr u32_t kChampionAIAvailableActionMask = 0x3Fu << kChampionAIAvailableActionShift;
inline constexpr u32_t kChampionAIAvailableSkillShift = 26u;
inline constexpr u32_t kChampionAIAvailableSkillMask = 0xFu << kChampionAIAvailableSkillShift;
inline constexpr u32_t kChampionAIDebugOverrideFlag = 1u << 30;
inline constexpr u32_t kChampionAIDebugCanAttackChampionFlag = 1u << 0;
inline constexpr u32_t kChampionAIDebugPostComboBAAllowedFlag = 1u << 1;
inline constexpr u32_t kChampionAIDebugMidDefenseActiveFlag = 1u << 2;
inline constexpr u32_t kChampionAIDebugBrainTypeShift = 3u;
inline constexpr u32_t kChampionAIDebugBrainTypeMask =
	0x3u << kChampionAIDebugBrainTypeShift;
inline constexpr u16_t kChampionAIDebugClearOverrideItemId = 0xFFFFu;
inline constexpr u16_t kChampionAIDebugTuneRuntimeItemId = 0xFFFEu;
inline constexpr u16_t kChampionAIDebugResetTuningItemId = 0xFFFDu;
inline constexpr u8_t kChampionAIDebugForceActionSkillSlot = 0xFFu;
inline constexpr u8_t kChampionAIDebugSingleDecisionCount = 1u;
inline constexpr u8_t kChampionAIDebugForceDecisionCount = 12u;
inline constexpr u8_t kChampionAIDebugTraceCapacity = 16u;

// 봇 의사결정 주체. 새 봇 유형은 ChampionAIBrain.cpp에 brain 구현을 추가하고
// 스폰 시 이 값만 지정하면 된다 (Shared/GameSim/Systems/ChampionAI/ChampionAIBrain.h 참고).
enum class eChampionAIBrainType : u8_t
{
	RuleBased = 0,   // 점수 기반 룰 (현행 기본)
	PlayerLike,      // 사람같은 봇 — 태세 유지/교전 보수성 강화
	Decision,        // 외부 판단 모듈 연동용 (모듈 연결 전까지 RuleBased 위임)
};

struct ChampionAIComponent
{
	eChampion champion = eChampion::NONE;
	eTeam team = eTeam::Blue;
	u8_t difficulty = 1;
	u8_t lane = 1;
	u8_t activeLane = 1;
	eChampionAIBrainType brainType = eChampionAIBrainType::RuleBased;

	eChampionAIState state = eChampionAIState::MoveToOuterTurret;
	eChampionAIAction lastAction = eChampionAIAction::MoveToSafeAnchor;
	eChampionAIIntent intent = eChampionAIIntent::FarmMinion;
	u8_t reservedLaneGoalAlignment[3]{};

	Vec3 laneGoal{ 0.f, 1.f, 0.f };
	Vec3 safeAnchor{ 0.f, 1.f, 0.f };
	Vec3 retreatGoal{ 0.f, 1.f, 0.f };
	Vec3 midDefenseAnchor{ 0.f, 1.f, 0.f };

	EntityID lockedChampion = NULL_ENTITY;
	EntityID targetMinion = NULL_ENTITY;
	EntityID targetStructure = NULL_ENTITY;
	EntityID alliedWave = NULL_ENTITY;
	EntityID comboTarget = NULL_ENTITY;
	EntityID lowHpEnemyChampion = NULL_ENTITY;
	EntityID diveTarget = NULL_ENTITY;
	EntityID lastSeenEnemyChampion = NULL_ENTITY;
	Vec3 lastSeenEnemyChampionPos{ 0.f, 0.f, 0.f };
	u64_t lastSeenEnemyChampionTick = 0u;
	eChampionAIDivePhase divePhase = eChampionAIDivePhase::None;
	u8_t comboStep = 0;
	u8_t diveExtraBACount = 0;
	u8_t reservedDecisionTimerAlignment = 0u;

	f32_t decisionTimer = 0.f;
	f32_t decisionInterval = 0.20f;
	f32_t intentHoldTimer = 0.f;
	f32_t intentHoldDuration = 0.80f;
	f32_t championScanRange = 9.f;
	f32_t minionScanRange = 12.f;
	f32_t structureScanRange = 18.f;
	f32_t waveJoinRange = 8.f;
	f32_t leashRange = 14.f;
	ChampionAITuning tuning{};
	f32_t retreatHpRatio = 0.10f;
	f32_t reengageHpRatio = 0.25f;
	f32_t fChampionScoreMargin = 0.05f;
	f32_t fTurretDangerThreshold = 0.85f;
	f32_t fPostComboBASelfHpMinRatio = 0.10f;
	f32_t fPostComboBAEnemyHpMargin = 0.f;
	f32_t fPostComboBAWindow = 0.80f;
	f32_t fPostComboBATimer = 0.f;
	f32_t fLowHpExecuteThreshold = 0.10f;
	f32_t fDiveScanRange = 11.f;
	f32_t fDiveExtraBAWindow = 1.80f;
	f32_t fDiveExtraBATimer = 0.f;
	f32_t fSkillCastMinInterval = 3.f;
	f32_t fSkillCastCooldownTimer = 0.f;
	f32_t followWaveSearchRange = 80.f;
	f32_t fFarmPriority = 1.f;
	f32_t fDecisionWaveDistance = 999.f;
	f32_t fRetreatDecisionScore = 0.f;
	f32_t fChampionDecisionScore = 0.f;
	f32_t fFarmDecisionScore = 0.f;
	f32_t fStructureDecisionScore = 0.f;
	f32_t fDecisionSelfHpRatio = 1.f;
	f32_t fDecisionEnemyHpRatio = 1.f;
	f32_t fDecisionEnemyDistance = 999.f;
	f32_t fDecisionAttackRange = 1.5f;
	f32_t fDecisionTurretDanger = 0.f;
	f32_t fDecisionLowHpEnemyRatio = 1.f;
	f32_t fDecisionLowHpEnemyDistance = 999.f;
	f32_t fDecisionChampionScanRange = 0.f;
	f32_t fDecisionDiveScanRange = 0.f;
	f32_t fDecisionFlashRange = 0.f;
	u8_t debugLastCommandKind = 0;
	u8_t debugLastCommandSlot = 0;
	u8_t reservedDebugTargetAlignment[2]{};
	EntityID debugLastCommandTarget = NULL_ENTITY;
	Vec3 debugLastCommandPos{};
	eChampionAIDecisionBlockReason debugLastBlockReason = eChampionAIDecisionBlockReason::None;
	u8_t reservedDebugTraceAlignment[3]{};
	ChampionAIDecisionTraceEntry debugDecisionTrace[kChampionAIDebugTraceCapacity] = {};
	u8_t debugDecisionTraceHead = 0u;
	u8_t debugDecisionTraceCount = 0u;
	u8_t reservedCommandSequenceAlignment[2]{};
	u32_t nextCommandSequence = 1;

	bool_t bWaveJoined = false;
	bool_t bStructureWaveTanking = false;
	bool_t bInsideEnemyTurretDanger = false;
	bool_t bCanAttackChampion = false;
	bool_t bPostComboBAAllowed = false;
	bool_t bMidDefenseActive = false;
	u8_t reservedDebugMaskAlignment[2]{};

	u32_t debugAvailableActionMask = 0;
	u32_t debugAvailableSkillMask = 0;
	eChampionAIDebugControlMode debugControlMode = eChampionAIDebugControlMode::Observe;
	eChampionAIAction debugForcedAction = eChampionAIAction::FollowWave;
	u8_t debugForcedSkillSlot = 0;
	u8_t debugForcedDecisionCount = 0;
	bool_t bDebugForceAction = false;
	u8_t reservedTail[3]{};

	f32_t midDefenseThreatHoldTimer = 0.f;
	bool_t bMidTeamfightActive = false;
	bool_t bYonePostReturnUltimatePending = false;
	u8_t reservedMidDefenseAlignment[2]{};

	u64_t teamPingExpireTick = 0u;
	Vec3 teamPingAnchor{};
	eTeamPingKind teamPingKind = eTeamPingKind::None;
	bool_t bTeamPingObjectiveActive = false;
	u8_t reservedTeamPingAlignment[2]{};
};

static_assert(sizeof(ChampionAITuningParam) == 20u);
static_assert(offsetof(ChampionAITuningParam, bOverride) == 16u);
static_assert(sizeof(ChampionAITuning) == 344u);
static_assert(std::is_trivially_copyable_v<ChampionAITuning>);
static_assert(offsetof(ChampionAIComponent, laneGoal) == 12u);
static_assert(offsetof(ChampionAIComponent, decisionTimer) == 116u);
static_assert(offsetof(ChampionAIComponent, debugLastCommandTarget) == 624u);
static_assert(offsetof(ChampionAIComponent, nextCommandSequence) == 2956u);
static_assert(offsetof(ChampionAIComponent, teamPingExpireTick) == 2992u);
static_assert(offsetof(ChampionAIComponent, teamPingAnchor) == 3000u);
static_assert(offsetof(ChampionAIComponent, teamPingKind) == 3012u);
static_assert(sizeof(ChampionAIComponent) == 3016u);
static_assert(std::is_trivially_copyable_v<ChampionAIComponent>);

// Research capture is diagnostic evidence, not authoritative gameplay state.
// Keep it outside ChampionAIComponent so checkpoint blobs do not retain a
// 9x9 map and 16 raw wire records per bot. A rewind intentionally recreates
// this transient component from the restored authoritative state.
struct ChampionAIResearchDebugComponent
{
	AiInfluenceMapV1 influenceMap{};
	AiDecisionTraceV1 decisionDraft{};
	AiDecisionTraceV1 decisionTrace[kChampionAIDebugTraceCapacity] = {};
	u8_t decisionTraceHead = 0u;
	u8_t decisionTraceCount = 0u;
	ChampionAIDecisionTraceEntry shadowDecision{};
	bool_t bShadowDecisionPresent = false;
	const ChampionAIShadowPolicyArtifactV1* pShadowPolicy = nullptr;
};

struct ChampionAIDebugComponent
{
	bool_t bPresent = false;
	eChampionAIState state = eChampionAIState::MoveToOuterTurret;
	eChampionAIAction action = eChampionAIAction::MoveToSafeAnchor;
	eChampionAIIntent intent = eChampionAIIntent::FarmMinion;
	eChampionAIBrainType brainType = eChampionAIBrainType::RuleBased;
	u32_t netId = 0;
	u32_t targetNetId = 0;
	u32_t lowHpEnemyNetId = 0;
	u32_t diveTargetNetId = 0;
	u32_t lastCommandTargetNetId = 0;
	u8_t lastCommandKind = 0;
	u8_t lastCommandSlot = 0;
	eChampionAIDivePhase divePhase = eChampionAIDivePhase::None;
	eChampionAIDecisionBlockReason lastBlockReason = eChampionAIDecisionBlockReason::None;
	u32_t availableActionMask = 0;
	u32_t availableSkillMask = 0;
	bool_t bOverridePending = false;
	bool_t bCanAttackChampion = false;
	bool_t bPostComboBAAllowed = false;
	bool_t bMidDefenseActive = false;
	f32_t fChampionDecisionScore = 0.f;
	f32_t fFarmDecisionScore = 0.f;
	f32_t fStructureDecisionScore = 0.f;
	f32_t fRetreatDecisionScore = 0.f;
	f32_t fSelfHpRatio = 1.f;
	f32_t fEnemyHpRatio = 1.f;
	f32_t fEnemyDistance = 999.f;
	f32_t fAttackRange = 1.5f;
	f32_t fTurretDanger = 0.f;
	f32_t fLowHpEnemyRatio = 1.f;
	f32_t fLowHpEnemyDistance = 999.f;
	f32_t fChampionScanRange = 0.f;
	f32_t fMinionScanRange = 0.f;
	f32_t fStructureScanRange = 0.f;
	f32_t fLeashRange = 0.f;
	f32_t fRetreatHpRatio = 0.f;
	f32_t fReengageHpRatio = 0.f;
	f32_t fChampionScoreMargin = 0.f;
	f32_t fTurretDangerThreshold = 0.f;
	f32_t fPostComboBASelfHpMinRatio = 0.f;
	f32_t fPostComboBAEnemyHpMargin = 0.f;
	f32_t fPostComboBAWindow = 0.f;
	f32_t fLowHpExecuteThreshold = 0.f;
	f32_t fDiveScanRange = 0.f;
	f32_t fDiveExtraBAWindow = 0.f;
	f32_t fFlashRange = 0.f;
	f32_t fPostComboBATimer = 0.f;
	f32_t fSkillCastMinInterval = 5.f;
	f32_t fSkillCastCooldownTimer = 0.f;
	u32_t enemyMinionNetId = 0u;
	u32_t alliedWaveNetId = 0u;
	f32_t fWaveDistance = 999.f;
	f32_t fWaveSupportRange = 0.f;
	f32_t fFollowWaveSearchRange = 0.f;
	f32_t fFarmPriority = 0.f;
	u32_t lastCommandSequence = 0u;
	u16_t lastExecutorReason = 0u;
	u8_t lastExecutorState = static_cast<u8_t>(AiExecutorStateV1::Unknown);
	f32_t moveSpeed = 0.f;
	Vec3 snapshotPos{ 0.f, 0.f, 0.f };
	Vec3 lastCommandPos{ 0.f, 0.f, 0.f };
	ChampionAIDecisionTraceEntry debugDecisionTrace[kChampionAIDebugTraceCapacity] = {};
	u8_t debugDecisionTraceCount = 0u;
	u64_t utilityCandidateTick = 0u;
	u64_t utilitySelectionTick = 0u;
	AiDecisionTraceV1 utilityDecision = ChampionAIResearch::MakeDecisionTraceV1();
};
