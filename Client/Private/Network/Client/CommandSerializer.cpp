#include "Network/Client/ClientNetwork.h"
#include "Network/Client/CommandSerializer.h"

#include "Dev/SmokeLog.h"
#include "Shared/Network/PacketEnvelope.h"
#include "Shared/GameSim/Components/ChampionAIComponent.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include "Shared/Schemas/Generated/cpp/Command_generated.h"
#include <flatbuffers/flatbuffers.h>
#pragma pop_macro("max")
#pragma pop_macro("min")

#include <chrono>
#include <cmath>
#include <cstdio>

namespace
{
    const char* GetCommandKindName(eCommandKind kind)
    {
        switch (kind)
        {
        case eCommandKind::Move:
            return "Move";
        case eCommandKind::CastSkill:
            return "CastSkill";
        case eCommandKind::BasicAttack:
            return "BasicAttack";
        case eCommandKind::Recall:
            return "Recall";
        case eCommandKind::RecallCancel:
            return "RecallCancel";
        case eCommandKind::BuyItem:
            return "BuyItem";
        case eCommandKind::LevelSkill:
            return "LevelSkill";
        case eCommandKind::AIDebugControl:
            return "AIDebugControl";
        default:
            return "None";
        }
    }

    f32_t DirectionYawXZ(const Vec3& direction)
    {
        return std::atan2f(direction.x, direction.z);
    }

    f32_t DirectionLenXZ(const Vec3& direction)
    {
        return std::sqrt(direction.x * direction.x + direction.z * direction.z);
    }

    bool_t IsValidMoveGroundPos(const Vec3& v)
    {
        return std::isfinite(v.x) &&
            std::isfinite(v.y) &&
            std::isfinite(v.z);
    }
}

std::unique_ptr<CCommandSerializer> CCommandSerializer::Create()
{
    return std::unique_ptr<CCommandSerializer>(new CCommandSerializer());
}

u32_t CCommandSerializer::SendMove(CClientNetwork& net, const Vec3& groundPos,
    const Vec3& direction)
{
    if (!IsValidMoveGroundPos(groundPos))
        return 0;

    GameCommandWire wire{};
    wire.kind = eCommandKind::Move;
    wire.clientTick = m_clientTick++;
    wire.sequenceNum = m_nextSequenceNum++;
    wire.groundPos = groundPos;
    wire.direction = WintersMath::NormalizeXZ(direction, Vec3{}, 0.0001f);

    static u32_t s_moveOutputDebugTraceCount = 0;
    if (s_moveOutputDebugTraceCount < 512u)
    {
        char msg[768]{};
        const f32_t inputVsWireDot =
            direction.x * wire.direction.x + direction.z * wire.direction.z;
        sprintf_s(
            msg,
            "[YawTrace][ClientSendMoveOD] sID=%u myNet=%u clientTick=%llu seq=%u ground=(%.3f,%.3f,%.3f) inputDir=(%.3f,%.3f) wireDir=(%.3f,%.3f) inputLen=%.4f wireLen=%.4f inputYaw=%.4f wireYaw=%.4f inputVsWireDot=%.4f\n",
            net.GetMySessionId(),
            net.GetMyNetEntityId(),
            static_cast<unsigned long long>(wire.clientTick),
            wire.sequenceNum,
            groundPos.x,
            groundPos.y,
            groundPos.z,
            direction.x,
            direction.z,
            wire.direction.x,
            wire.direction.z,
            DirectionLenXZ(direction),
            DirectionLenXZ(wire.direction),
            DirectionYawXZ(direction),
            DirectionYawXZ(wire.direction),
            inputVsWireDot);
        Winters::DevSmoke::Log("%s", msg);
        ++s_moveOutputDebugTraceCount;
    }

    static u32_t s_moveLogCount = 0;
    if (false && s_moveLogCount < 512u)
    {
        Winters::DevSmoke::Log(
            "[YawTrace][ClientSendMove] sID=%u myNet=%u clientTick=%llu seq=%u pos=(%.3f,%.3f,%.3f) dir=(%.3f,%.3f) dirLen=%.4f dirYaw=%.4f\n",
            net.GetMySessionId(),
            net.GetMyNetEntityId(),
            static_cast<unsigned long long>(wire.clientTick),
            wire.sequenceNum,
            groundPos.x,
            groundPos.y,
            groundPos.z,
            wire.direction.x,
            wire.direction.z,
            DirectionLenXZ(wire.direction),
            DirectionYawXZ(wire.direction));
        ++s_moveLogCount;
    }

    SendSingle(net, wire);
    return wire.sequenceNum;
}

void CCommandSerializer::SendCastSkill(CClientNetwork& net, u8_t slot,
    NetEntityId targetNet, const Vec3& groundPos, const Vec3& direction,
    u8_t skillStage)
{
    GameCommandWire wire{};
    wire.kind = eCommandKind::CastSkill;
    wire.clientTick = m_clientTick++;
    wire.sequenceNum = m_nextSequenceNum++;
    wire.slot = slot;
    wire.targetNet = targetNet;
    wire.groundPos = groundPos;
    wire.direction = direction;
    wire.itemId = static_cast<u16_t>(skillStage);

    static u32_t s_castLogCount = 0;
    if (false && s_castLogCount < 256u)
    {
        Winters::DevSmoke::Log(
            "[YawTrace][ClientSendCast] sID=%u myNet=%u clientTick=%llu seq=%u slot=%u stage=%u targetNet=%u ground=(%.3f,%.3f,%.3f) dir=(%.3f,%.3f) dirLen=%.4f dirYaw=%.4f\n",
            net.GetMySessionId(),
            net.GetMyNetEntityId(),
            static_cast<unsigned long long>(wire.clientTick),
            wire.sequenceNum,
            wire.slot,
            static_cast<u32_t>(skillStage),
            wire.targetNet,
            wire.groundPos.x,
            wire.groundPos.y,
            wire.groundPos.z,
            wire.direction.x,
            wire.direction.z,
            DirectionLenXZ(wire.direction),
            DirectionYawXZ(wire.direction));
        ++s_castLogCount;
    }

    SendSingle(net, wire);
}

u32_t CCommandSerializer::SendBasicAttack(CClientNetwork& net, NetEntityId targetNet,
    const Vec3& groundPos, const Vec3& direction)
{
    GameCommandWire wire{};
    wire.kind = eCommandKind::BasicAttack;
    wire.clientTick = m_clientTick++;
    wire.sequenceNum = m_nextSequenceNum++;
    wire.slot = 0;
    wire.targetNet = targetNet;
    wire.groundPos = groundPos;
    wire.direction = direction;

    static u32_t s_baLogCount = 0;
    if (false && s_baLogCount < 256u)
    {
        Winters::DevSmoke::Log(
            "[YawTrace][ClientSendBasicAttack] sID=%u myNet=%u clientTick=%llu seq=%u targetNet=%u ground=(%.3f,%.3f,%.3f) dir=(%.3f,%.3f) dirLen=%.4f dirYaw=%.4f\n",
            net.GetMySessionId(),
            net.GetMyNetEntityId(),
            static_cast<unsigned long long>(wire.clientTick),
            wire.sequenceNum,
            targetNet,
            wire.groundPos.x,
            wire.groundPos.y,
            wire.groundPos.z,
            wire.direction.x,
            wire.direction.z,
            DirectionLenXZ(wire.direction),
            DirectionYawXZ(wire.direction));
        ++s_baLogCount;
    }

    SendSingle(net, wire);
    return wire.sequenceNum;
}

void CCommandSerializer::SendBuyItem(CClientNetwork& net, u16_t itemId)
{
    if (itemId == 0)
        return;

    GameCommandWire wire{};
    wire.kind = eCommandKind::BuyItem;
    wire.clientTick = m_clientTick++;
    wire.sequenceNum = m_nextSequenceNum++;
    wire.itemId = itemId;

    //Smoke 안 넣을게
    SendSingle(net, wire);
}

void CCommandSerializer::SendRecall(CClientNetwork& net)
{
    GameCommandWire wire{};
    wire.kind = eCommandKind::Recall;
    wire.clientTick = m_clientTick++;
    wire.sequenceNum = m_nextSequenceNum++;

    //Smoke 안 넣을게
    SendSingle(net, wire);
}

void CCommandSerializer::SendAIDebugControl(CClientNetwork& net, NetEntityId targetNet,
    eChampionAIAction action, u8_t skillSlot)
{
    if (targetNet == NULL_NET_ENTITY)
        return;

    GameCommandWire wire{};
    wire.kind = eCommandKind::AIDebugControl;
    wire.clientTick = m_clientTick++;
    wire.sequenceNum = m_nextSequenceNum++;
    wire.targetNet = targetNet;
    wire.slot = skillSlot;
    wire.itemId = static_cast<u16_t>(action);

    SendSingle(net, wire);
}

void CCommandSerializer::SendAIDebugClear(CClientNetwork& net, NetEntityId targetNet)
{
    if (targetNet == NULL_NET_ENTITY)
        return;

    GameCommandWire wire{};
    wire.kind = eCommandKind::AIDebugControl;
    wire.clientTick = m_clientTick++;
    wire.sequenceNum = m_nextSequenceNum++;
    wire.targetNet = targetNet;
    wire.itemId = kChampionAIDebugClearOverrideItemId;

    SendSingle(net, wire);
}

void CCommandSerializer::SendLevelSkill(CClientNetwork& net, u8_t slot)
{
    if (slot == 0 || slot >= 5)
        return;

    GameCommandWire wire{};
    wire.kind = eCommandKind::LevelSkill;
    wire.clientTick = m_clientTick++;
    wire.sequenceNum = m_nextSequenceNum++;
    wire.slot = slot;

    SendSingle(net, wire);
}

void CCommandSerializer::SendSingle(CClientNetwork& net, GameCommandWire& wire)
{
    std::vector<GameCommandWire> wires;
    wires.push_back(wire);

    auto payload = BuildCommandBatch(wires);
    if (payload.empty())
        return;

    if (m_onCommandSent)
        m_onCommandSent(wire);

    static u32_t s_sendSingleLogCount = 0;
    if (false && s_sendSingleLogCount < 1024u)
    {
        Winters::DevSmoke::Log(
            "[YawTrace][ClientSendPacket] sID=%u myNet=%u kind=%s seq=%u clientTick=%llu payloadBytes=%u ground=(%.3f,%.3f,%.3f) dir=(%.3f,%.3f) dirYaw=%.4f\n",
            net.GetMySessionId(),
            net.GetMyNetEntityId(),
            GetCommandKindName(wire.kind),
            wire.sequenceNum,
            static_cast<unsigned long long>(wire.clientTick),
            static_cast<u32_t>(payload.size()),
            wire.groundPos.x,
            wire.groundPos.y,
            wire.groundPos.z,
            wire.direction.x,
            wire.direction.z,
            DirectionYawXZ(wire.direction));
        ++s_sendSingleLogCount;
    }

    auto packet = WrapEnvelope(
        ePacketType::CommandBatch,
        wire.sequenceNum,
        payload.data(),
        static_cast<u32_t>(payload.size()));
    net.Send(std::move(packet));
}

std::vector<u8_t> CCommandSerializer::BuildCommandBatch(const std::vector<GameCommandWire>&wires)
{
    flatbuffers::FlatBufferBuilder fbb(256);

    std::vector<flatbuffers::Offset<Shared::Schema::CommandPacket>> commands;
    commands.reserve(wires.size());

    for (const GameCommandWire& wire : wires)
    {
        Shared::Schema::Vec3 ground(wire.groundPos.x, wire.groundPos.y, wire.groundPos.z);
        Shared::Schema::Vec3 direction(wire.direction.x, wire.direction.y, wire.direction.z);

        static u32_t s_serializeLogCount = 0;
        if (false && s_serializeLogCount < 1024u)
        {
            Winters::DevSmoke::Log(
                "[YawTrace][ClientSerializeCommand] kind=%s seq=%u clientTick=%llu slot=%u targetNet=%u item=%u ground=(%.3f,%.3f,%.3f) dir=(%.3f,%.3f) dirLen=%.4f dirYaw=%.4f\n",
                GetCommandKindName(wire.kind),
                wire.sequenceNum,
                static_cast<unsigned long long>(wire.clientTick),
                static_cast<u32_t>(wire.slot),
                wire.targetNet,
                static_cast<u32_t>(wire.itemId),
                wire.groundPos.x,
                wire.groundPos.y,
                wire.groundPos.z,
                wire.direction.x,
                wire.direction.z,
                DirectionLenXZ(wire.direction),
                DirectionYawXZ(wire.direction));
            ++s_serializeLogCount;
        }

        commands.push_back(Shared::Schema::CreateCommandPacket(
            fbb,
            static_cast<Shared::Schema::CommandKind>(wire.kind),
            wire.sequenceNum,
            wire.clientTick,
            wire.slot,
            wire.targetNet,
            &ground,
            &direction,
            wire.itemId,
            0));
    }

    const u64_t timestampMs = static_cast<u64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    auto commandsOffset = fbb.CreateVector(commands);
    auto batch = Shared::Schema::CreateCommandBatch(fbb, commandsOffset, timestampMs);
    fbb.Finish(batch);

    auto payload = fbb.Release();
    return std::vector<u8_t>(payload.data(), payload.data() + payload.size());
}
