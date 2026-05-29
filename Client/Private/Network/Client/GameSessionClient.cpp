#include "Network/Client/ClientNetwork.h"
#include "Network/Client/GameSessionClient.h"
#include "Dev/SmokeLog.h"
#include "Shared/Network/PacketEnvelope.h"

#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include "Shared/Schemas/Generated/cpp/Hello_generated.h"
#include "Shared/Schemas/Generated/cpp/LobbyCommand_generated.h"
#include "Shared/Schemas/Generated/cpp/LobbyState_generated.h"
#include <flatbuffers/flatbuffers.h>
#pragma pop_macro("max")
#pragma pop_macro("min")

#include <Windows.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace
{
	const char* GetLobbyCommandKindName(Shared::Schema::LobbyCommandKind kind)
	{
		switch (kind)
		{
		case Shared::Schema::LobbyCommandKind::JoinSlot:
			return "JoinSlot";
		case Shared::Schema::LobbyCommandKind::LeaveSlot:
			return "LeaveSlot";
		case Shared::Schema::LobbyCommandKind::PickChampion:
			return "PickChampion";
		case Shared::Schema::LobbyCommandKind::SetBotChampion:
			return "SetBotChampion";
		case Shared::Schema::LobbyCommandKind::SetBotDifficulty:
			return "SetBotDifficulty";
		case Shared::Schema::LobbyCommandKind::SetBotLane:
			return "SetBotLane";
		case Shared::Schema::LobbyCommandKind::SetReady:
			return "SetReady";
		case Shared::Schema::LobbyCommandKind::StartGame:
			return "StartGame";
		case Shared::Schema::LobbyCommandKind::CancelStart:
			return "CancelStart";
		case Shared::Schema::LobbyCommandKind::SetEditPolicy:
			return "SetEditPolicy";
		default:
			return "None";
		}
	}

	std::string BuildLobbyCommandText(
		Shared::Schema::LobbyCommandKind kind,
		u8_t slotId,
		eChampion champion,
		u8_t botDifficulty,
		u32_t value)
	{
		char text[192]{};
		sprintf_s(
			text,
			"%s slot=%u champ=%u botDifficulty=%u value=%u",
			GetLobbyCommandKindName(kind),
			static_cast<u32_t>(slotId),
			static_cast<u32_t>(champion),
			static_cast<u32_t>(botDifficulty),
			value);
		return std::string(text);
	}

	bool IsLocalEndpoint(const char* pHost, u16_t port)
	{
		if (port != 9000 || !pHost)
			return false;

		return std::strcmp(pHost, "127.0.0.1") == 0 ||
			std::strcmp(pHost, "localhost") == 0;
	}
}

CGameSessionClient& CGameSessionClient::Instance()
{
	static CGameSessionClient s_instance;
	return s_instance;
}

bool CGameSessionClient::Connect(const char* host, u16_t port)
{
	if (IsConnected())
		return true;

	m_bServerLoading = false;
	m_bHasLobbyState = false;
	m_bGameStarting = false;
	m_uGameStartCount = 0;
	m_lastHelloPayload.clear();

	m_pNetwork = CClientNetwork::Create();
	if (!m_pNetwork)
		return false;

	m_pNetwork->SetFrameCallback(
		[this](ePacketType type, u32_t sequence, const u8_t* payload, u32_t len)
		{
			OnFrame(type, sequence, payload, len);
		});

	const bool_t bLocalEndpoint = IsLocalEndpoint(host, port);
	const int attempts = bLocalEndpoint ? 20 : 1;
	for (int attempt = 1; attempt <= attempts; ++attempt)
	{
		if (m_pNetwork->Connect(host, port))
		{
			Winters::DevSmoke::Log(
				"[GameSessionClient] connected to lobby server host=%s port=%u attempt=%d\n",
				host ? host : "-",
				static_cast<u32_t>(port),
				attempt);
			return true;
		}

		if (attempt < attempts)
			Sleep(100);
	}

	if (!m_pNetwork->IsConnected())
	{
		Winters::DevSmoke::Log(
			"[GameSessionClient] connect failed host=%s port=%u attempts=%d\n",
			host ? host : "-",
			static_cast<u32_t>(port),
			attempts);
		m_pNetwork.reset();
		return false;
	}

	return true;
}

void CGameSessionClient::Disconnect()
{
	if (m_pNetwork)
		m_pNetwork->Disconnect();

	m_pNetwork.reset();
	m_lobbyContext = GameContext{};
	m_bServerLoading = false;
	m_bHasLobbyState = false;
	m_bGameStarting = false;
	m_nextLobbySequence = 1;
	m_lastHelloSequence = 0;
	m_uLobbyRevision = 0;
	m_uLobbyPhase = 0;
	m_uGameStartCount = 0;
	m_lastHelloPayload.clear();
	m_strLastLobbyMessage.clear();
	m_strLastLobbyCommandText.clear();
	m_gameFrameCallback = nullptr;
}

void CGameSessionClient::Pump()
{
	if (m_pNetwork)
		m_pNetwork->PumpReceivedFrames();
}

bool CGameSessionClient::IsConnected() const
{
	return m_pNetwork && m_pNetwork->IsConnected();
}

void CGameSessionClient::CopyLobbyToGameContext(GameContext& outContext) const
{
	outContext = m_lobbyContext;
}

bool CGameSessionClient::SendLobbyCommand(
	Shared::Schema::LobbyCommandKind kind,
	u8_t slotId,
	eChampion champion,
	u8_t botDifficulty,
	u32_t value)
{
	m_strLastLobbyCommandText = BuildLobbyCommandText(kind, slotId, champion, botDifficulty, value);

	if (!IsConnected())
	{
		m_strLastLobbyMessage = "client reject: lobby server is not connected";
		OutputDebugStringA("[GameSessionClient] lobby command rejected locally: disconnected\n");
		return false;
	}

	flatbuffers::FlatBufferBuilder fbb(128);
	const auto command = Shared::Schema::CreateLobbyCommand(
		fbb,
		kind,
		slotId,
		static_cast<u8_t>(slotId < 5 ? 0 : 1),
		static_cast<u8_t>(champion),
		botDifficulty,
		value);
	fbb.Finish(command);
	auto payload = fbb.Release();

	auto packet = WrapEnvelope(
		ePacketType::LobbyCommand,
		m_nextLobbySequence++,
		payload.data(),
		static_cast<u32_t>(payload.size()));

	if (!m_pNetwork->Send(std::move(packet)))
	{
		m_strLastLobbyMessage = "client reject: send failed";
		OutputDebugStringA("[GameSessionClient] lobby command send failed\n");
		return false;
	}

	std::string log = "[GameSessionClient] send lobby command ";
	log += m_strLastLobbyCommandText;
	log += "\n";
	OutputDebugStringA(log.c_str());
	return true;
}

void CGameSessionClient::SetGameFrameCallback(FrameCallback callback)
{
	m_gameFrameCallback = std::move(callback);
}

void CGameSessionClient::ReplayLastHelloToGameFrameCallback()
{
	if (!m_gameFrameCallback || m_lastHelloPayload.empty())
		return;

	m_gameFrameCallback(
		ePacketType::Hello,
		m_lastHelloSequence,
		m_lastHelloPayload.data(),
		static_cast<u32_t>(m_lastHelloPayload.size()));
}

void CGameSessionClient::OnFrame(ePacketType type, u32_t sequence, const u8_t* payload, u32_t len)
{
	if (type == ePacketType::LobbyState)
	{
		OnLobbyState(payload, len);
	}
	else if (type == ePacketType::Hello)
	{
		m_lastHelloSequence = sequence;
		m_lastHelloPayload.assign(payload, payload + len);
		OnHello(payload, len);
	}
	else if (type == ePacketType::GameStart)
	{
		++m_uGameStartCount;
		if (m_uGameStartCount > 1)
			OutputDebugStringA("[GameSessionClient] duplicate GameStart packet received\n");
		m_bServerLoading = false;
		m_bGameStarting = true;
	}

	static u32_t s_yawFrameLogCount = 0;
	if (false &&
		(type == ePacketType::Snapshot ||
			type == ePacketType::Event ||
			type == ePacketType::Hello) &&
		s_yawFrameLogCount < 2048u)
	{
		char msg[192]{};
		sprintf_s(
			msg,
			"[YawTrace][ClientRecvFrame] type=%u seq=%u bytes=%u connected=%u serverLoading=%u gameStarting=%u\n",
			static_cast<u32_t>(type),
			sequence,
			len,
			IsConnected() ? 1u : 0u,
			m_bServerLoading ? 1u : 0u,
			m_bGameStarting ? 1u : 0u);
		OutputDebugStringA(msg);
		++s_yawFrameLogCount;
	}

	if (m_gameFrameCallback)
		m_gameFrameCallback(type, sequence, payload, len);
}

void CGameSessionClient::OnLobbyState(const u8_t* payload, u32_t len)
{
	if (!payload || len == 0)
		return;

	flatbuffers::Verifier verifier(payload, len);
	if (!Shared::Schema::VerifyLobbyStateBuffer(verifier))
	{
		OutputDebugStringA("[GameSessionClient] invalid LobbyState\n");
		return;
	}

	const auto* state = Shared::Schema::GetLobbyState(payload);
	if (!state)
		return;

	m_uLobbyRevision = state->revision();
	m_uLobbyPhase = static_cast<u8_t>(state->phase());
	m_strLastLobbyMessage.clear();
	if (const auto* message = state->debugMessage())
		m_strLastLobbyMessage = message->str();

	const u32_t mySessionId = m_lobbyContext.MySessionId;
	const u32_t myNetId = m_lobbyContext.MyNetId;
	m_lobbyContext = GameContext{};
	m_lobbyContext.bUseNetworkRoster = true;
	m_lobbyContext.MySessionId = mySessionId;
	m_lobbyContext.MyNetId = myNetId;

	if (const auto* slots = state->slots())
	{
		const u32_t count = static_cast<u32_t>(slots->size());
		for (u32_t i = 0; i < count && i < kGameRosterSlotCount; ++i)
		{
			const auto* src = slots->Get(i);
			if (!src)
				continue;

			const u32_t slotId = src->slotId();
			if (slotId >= kGameRosterSlotCount)
				continue;

			GameRosterSlot& dst = m_lobbyContext.Roster[slotId];
			dst.slotId = static_cast<u8_t>(slotId);
			dst.team = src->team();
			dst.sessionId = src->sessionId();
			dst.netId = src->netId();
			dst.champion = static_cast<eChampion>(src->championId());
			dst.botDifficulty = src->botDifficulty();
			dst.botLane = src->botLane();
			dst.bHuman = (src->seatKind() == Shared::Schema::LobbySeatKind::Human);
			dst.bBot = (src->seatKind() == Shared::Schema::LobbySeatKind::Bot);

			const bool_t bIsLocalHuman =
				dst.bHuman &&
				((mySessionId != 0 && dst.sessionId == mySessionId) ||
					(myNetId != 0 && dst.netId == myNetId));

			if (bIsLocalHuman)
			{
				m_lobbyContext.MySlotId = dst.slotId;
				m_lobbyContext.MyTeam = dst.team;
				m_lobbyContext.SelectedChampion = dst.champion;
				if (dst.netId != 0)
					m_lobbyContext.MyNetId = dst.netId;
			}
		}
	}

	if (state->phase() == Shared::Schema::LobbyPhase::Starting)
	{
		m_bServerLoading = true;
		m_bGameStarting = false;
	}
	else if (state->phase() == Shared::Schema::LobbyPhase::InGame)
	{
		m_bServerLoading = false;
		m_bGameStarting = true;
	}

	m_bHasLobbyState = true;
}

void CGameSessionClient::OnHello(const u8_t* payload, u32_t len)
{
	if (!payload || len == 0)
		return;

	flatbuffers::Verifier verifier(payload, len);
	if (!Shared::Schema::VerifyHelloBuffer(verifier))
		return;

	const auto* hello = Shared::Schema::GetHello(payload);
	if (!hello)
		return;

	m_lobbyContext.bUseNetworkRoster = true;
	m_lobbyContext.MySessionId = hello->sessionId();
	if (hello->yourNetId() != 0)
		m_lobbyContext.MyNetId = hello->yourNetId();
	if (hello->championId() != static_cast<u8_t>(eChampion::END))
		m_lobbyContext.SelectedChampion = static_cast<eChampion>(hello->championId());
	m_lobbyContext.MyTeam = hello->team();

	for (u32_t i = 0; i < kGameRosterSlotCount; ++i)
	{
		const GameRosterSlot& slot = m_lobbyContext.Roster[i];
		if (!slot.bHuman)
			continue;
		if ((m_lobbyContext.MySessionId != 0 && slot.sessionId == m_lobbyContext.MySessionId) ||
			(m_lobbyContext.MyNetId != 0 && slot.netId == m_lobbyContext.MyNetId))
		{
			m_lobbyContext.MySlotId = slot.slotId;
			m_lobbyContext.MyTeam = slot.team;
			m_lobbyContext.SelectedChampion = slot.champion;
			if (slot.netId != 0)
				m_lobbyContext.MyNetId = slot.netId;
			break;
		}
	}

	if (m_pNetwork)
	{
		m_pNetwork->SetMySessionId(hello->sessionId());
		if (hello->yourNetId() != 0)
			m_pNetwork->SetMyNetEntityId(hello->yourNetId());
	}
}
