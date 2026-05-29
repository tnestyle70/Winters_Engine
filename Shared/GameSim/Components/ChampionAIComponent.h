#pragma once

#include "WintersTypes.h"
#include "WintersMath.h"
#include "ECS/Entity.h"
#include "ECS/Components/GameplayComponents.h"

enum class eChampionAIState : u8_t
{
	MoveToOuterTurret,
	WaitForWave,
	LaneCombat,
	Retreat,
	Recalling,
	Dead,
};

enum class eChampionAIAction : u8_t
{
	MoveToSafeAnchor,
	FollowWave,
	AttackMinion,
	AttackChampion,
	AttackStructure,
	Retreat,
	Recall,
};

enum class eChampionAIIntent : u8_t
{
	FarmMinion,
	AttackChampion,
	SiegeStructure,
	Retreat,
	Recall,
};

inline constexpr u32_t kChampionAIActionBitMoveToSafeAnchor = 1u << 0;
inline constexpr u32_t kChampionAIActionBitFollowWave = 1u << 1;
inline constexpr u32_t kChampionAIActionBitAttackMinion = 1u << 2;
inline constexpr u32_t kChampionAIActionBitAttackChampion = 1u << 3;
inline constexpr u32_t kChampionAIActionBitAttackStructure = 1u << 4;
inline constexpr u32_t kChampionAIActionBitRetreat = 1u << 5;

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
inline constexpr u16_t kChampionAIDebugClearOverrideItemId = 0xFFFFu;
inline constexpr u8_t kChampionAIDebugForceActionSkillSlot = 0xFFu;
inline constexpr u8_t kChampionAIDebugSingleDecisionCount = 1u;
inline constexpr u8_t kChampionAIDebugForceDecisionCount = 12u;

struct ChampionAIComponent
{
	eChampion champion = eChampion::NONE;
	eTeam team = eTeam::Blue;
	u8_t difficulty = 1;
	u8_t lane = 1;

	eChampionAIState state = eChampionAIState::MoveToOuterTurret;
	eChampionAIAction lastAction = eChampionAIAction::MoveToSafeAnchor;
	eChampionAIIntent intent = eChampionAIIntent::FarmMinion;

	Vec3 laneGoal{ 0.f, 1.f, 0.f };
	Vec3 safeAnchor{ 0.f, 1.f, 0.f };
	Vec3 retreatGoal{ 0.f, 1.f, 0.f };

	EntityID lockedChampion = NULL_ENTITY;
	EntityID targetMinion = NULL_ENTITY;
	EntityID targetStructure = NULL_ENTITY;
	EntityID alliedWave = NULL_ENTITY;
	EntityID comboTarget = NULL_ENTITY;
	u8_t comboStep = 0;

	f32_t decisionTimer = 0.f;
	f32_t decisionInterval = 0.20f;
	f32_t intentHoldTimer = 0.f;
	f32_t intentHoldDuration = 0.80f;
	f32_t championScanRange = 9.f;
	f32_t minionScanRange = 12.f;
	f32_t structureScanRange = 18.f;
	f32_t waveJoinRange = 8.f;
	f32_t leashRange = 14.f;
	f32_t attackChampionChance = 0.30f;
	f32_t lastDecisionRoll = 1.f;
	f32_t retreatHpRatio = 0.35f;
	f32_t reengageHpRatio = 0.55f;
	u32_t nextCommandSequence = 1;

	bool_t bWaveJoined = false;
	bool_t bStructureWaveTanking = false;
	bool_t bInsideEnemyTurretDanger = false;

	u32_t debugAvailableActionMask = 0;
	u32_t debugAvailableSkillMask = 0;
	eChampionAIAction debugForcedAction = eChampionAIAction::FollowWave;
	u8_t debugForcedSkillSlot = 0;
	u8_t debugForcedDecisionCount = 0;
	bool_t bDebugForceAction = false;
};

struct ChampionAIDebugComponent
{
	bool_t bPresent = false;
	eChampionAIState state = eChampionAIState::MoveToOuterTurret;
	eChampionAIAction action = eChampionAIAction::MoveToSafeAnchor;
	eChampionAIIntent intent = eChampionAIIntent::FarmMinion;
	u32_t netId = 0;
	u32_t targetNetId = 0;
	u32_t availableActionMask = 0;
	u32_t availableSkillMask = 0;
	bool_t bOverridePending = false;
	f32_t moveSpeed = 0.f;
	Vec3 snapshotPos{ 0.f, 0.f, 0.f };
};
