#pragma once
#include "Defines.h"
#include "WintersMath.h"
#include "Shared/GameSim/Replication/EntityIdMap.h"

#include <functional>
#include <memory>
#include <vector>

class CClientNetwork;
enum class eChampionAIAction : u8_t;
struct GameCommandWire;

class CCommandSerializer final
{
public:
	static std::unique_ptr<CCommandSerializer> Create();

	using OnCommandSentFn = std::function<void(const GameCommandWire&)>;
	void SetOnCommandSent(OnCommandSentFn fn) { m_onCommandSent = std::move(fn); }

	u32_t SendMove(CClientNetwork& net, const Vec3& groundPos,
		const Vec3& direction = {});
	void SendCastSkill(CClientNetwork& net, u8_t slot, NetEntityId targetNet,
		const Vec3& groundPos, const Vec3& direction, u8_t skillStage = 1);
	u32_t SendBasicAttack(CClientNetwork& net, NetEntityId targetNet,
		const Vec3& groundPos = {}, const Vec3& direction = {});
	void SendBuyItem(CClientNetwork& net, u16_t itemId);
	void SendRecall(CClientNetwork& net);
	void SendFlash(CClientNetwork& net, const Vec3& groundPos,
		const Vec3& direction);
	void SendAIDebugControl(CClientNetwork& net, NetEntityId targetNet,
		eChampionAIAction action, u8_t skillSlot = 0);
	void SendAIDebugTune(CClientNetwork& net, NetEntityId targetNet,
		u8_t tuningId, f32_t value);
	void SendAIDebugResetTuning(CClientNetwork& net, NetEntityId targetNet);
	void SendAIDebugClear(CClientNetwork& net, NetEntityId targetNet);
	void SendLevelSkill(CClientNetwork& net, u8_t slot);

private:
	CCommandSerializer() = default;

	void SendSingle(CClientNetwork& net, GameCommandWire& wire);
	std::vector<u8_t> BuildCommandBatch(const std::vector<GameCommandWire>& wires);

	u32_t m_nextSequenceNum = 1;
	u64_t m_clientTick = 0;
	OnCommandSentFn m_onCommandSent;
};
