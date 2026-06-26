#pragma once

#include "Shared/GameSim/Components/GameplayComponents.h"
#include "ECS/Entity.h"
#include "Manager/Navigation/NavGrid.h"
#include "Shared/GameSim/Components/PoseActionStateHelpers.h"
#include "Shared/GameSim/Definitions/MapDataFormats.h"
#include "WintersMath.h"
#include "WintersTypes.h"

#include <vector>

class CWorld;
struct TickContext;
namespace Winters::Map { struct StageData; }

// GameRoom 분할 cpp들이 공유하는 내부 헬퍼 모음.
// 한 파일에서만 쓰는 헬퍼는 해당 GameRoomXxx.cpp의 anonymous namespace에 둔다.

constexpr f32_t kMoveTargetMaxSurfaceDeltaY = 3.f;
constexpr u32_t kStructureKindNexus =
    static_cast<u32_t>(Winters::Map::eObjectKind::Structure_Nexus);
constexpr u32_t kStructureKindInhibitor =
    static_cast<u32_t>(Winters::Map::eObjectKind::Structure_Inhibitor);
constexpr u32_t kStructureKindTurret =
    static_cast<u32_t>(Winters::Map::eObjectKind::Structure_Turret);
constexpr u32_t kLaneTop = static_cast<u32_t>(Winters::Map::eLane::Top);
constexpr u32_t kLaneMid = static_cast<u32_t>(Winters::Map::eLane::Mid);
constexpr u32_t kLaneBot = static_cast<u32_t>(Winters::Map::eLane::Bot);
constexpr u32_t kLaneBase = static_cast<u32_t>(Winters::Map::eLane::Base);

u8_t TeamByte(eTeam team);
void OutputServerAITrace(const char* pText);
void OutputServerAITraceW(const wchar_t* pText);
Vec3 NormalizeXZOrForward(const Vec3& v, eTeam team);

std::vector<Engine::CNavGrid::Cell> SmoothServerPathCells(
    const Engine::CNavGrid& navGrid,
    const std::vector<Engine::CNavGrid::Cell>& path);

u8_t ResolveServerWaypointLane(eTeam team, u8_t lane);
bool_t TryResolveStageFountainSpawn(
    const Winters::Map::StageData& stage,
    u8_t slotId,
    eTeam team,
    Vec3& outSpawn);
f32_t ResolveStageStructureRadius(u32_t kind, u32_t tier);

bool_t TryResolveCombatTeam(CWorld& world, EntityID entity, eTeam& outTeam);
bool_t IsAliveHealth(CWorld& world, EntityID entity);
void StartReplicatedAction(CWorld& world, EntityID entity, eActionStateId actionId,
    const TickContext& tc, u8_t stage = 1);
void SetReplicatedPose(CWorld& world, EntityID entity, ePoseStateId poseId,
    const TickContext& tc);
