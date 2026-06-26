#pragma once

#include "Shared/GameSim/Definitions/LoLMatchContext.h"
#include "WintersMath.h"
#include "WintersTypes.h"

#include <array>
#include <vector>

namespace Engine
{
	class CNavGrid;
}

enum class eTeam : uint8_t;

class CServerMinionFlowField final
{
public:
	void Clear();
	bool_t Build(const Engine::CNavGrid& navGrid, const std::vector<Vec3>(&waypoints)[2][3]);
	bool_t TryResolveDirection(eTeam team, u8_t lane, const Vec3& pos, Vec3& outDirection) const;
	bool_t HasField(eTeam team, u8_t lane) const;

private:
	struct FlowField
	{
		bool_t bReady = false;
		f32_t originX = 0.f;
		f32_t originZ = 0.f;
		std::vector<Vec3> directions{};
	};

	static u32_t ResolveFieldIndex(eTeam team, u8_t lane);

	static constexpr u32_t kFieldCount = 6u;
	std::array<FlowField, kFieldCount> m_fields{};
};
