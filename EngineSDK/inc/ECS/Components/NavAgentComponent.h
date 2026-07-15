#pragma once
#include "WintersTypes.h"
#include "WintersMath.h"
#include "ECS/Entity.h"
#include <vector>

namespace Engine
{
class CNavGrid;
}

// ─────────────────────────────────────────────────────────────
// NavAgentComponent — Pathfinding 수요자
//  - CommandExecutorSystem 이 MoveCommand 를 받아 vTarget 세팅 + bHasGoal.
//  - NavigationSystem 이 A* 결과 path 를 채우고 프레임마다 다음 웨이포인트로
//    VelocityComponent 를 세팅.
//  - MovementSystem 이 Velocity → Transform 반영.
//
//  동적 충돌 무시 정책: 이 그래프는 "정적 점유" 만 본다.

struct NavAgentComponent
{
	Vec3 vTarget{0.f, 0.f, 0.f};
	std::vector<int32_t> pathCellsX; //path를 cx cy 배열로 저장
	//왜  navgrid cell 배열 대신 평면을 사용하는 거지? 
	std::vector<int32_t> pathCellsY;
	uint32_t iPathIndex = 0;
	f32_t fSpeed = 5.f;
	f32_t fArriveRadius = 0.25f;
	bool_t bHasGoal = false;
	bool_t bPathDirty = true;
};