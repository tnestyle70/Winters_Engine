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
#include <cstdlib>
#include <cwchar>
#include <cwctype>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace
{
	constexpr const char* kDefaultServerHost = "127.0.0.1";
	constexpr u16_t kDefaultServerPort = 9000;

	const wchar_t* GetCommandLineText()
	{
		const wchar_t* pCommandLine = ::GetCommandLineW();
		return pCommandLine ? pCommandLine : L"";
	}

	const wchar_t* FindCommandLineValue(const wchar_t* pPrefix)
	{
		if (!pPrefix)
			return nullptr;

		const wchar_t* pFound = std::wcsstr(GetCommandLineText(), pPrefix);
		return pFound ? pFound + std::wcslen(pPrefix) : nullptr;
	}

	std::string ReadCommandLineTextValue(const wchar_t* pPrefix)
	{
		const wchar_t* pValue = FindCommandLineValue(pPrefix);
		if (!pValue || *pValue == L'\0')
			return {};

		if (*pValue == L'"')
			++pValue;

		wchar_t wide[256]{};
		size_t i = 0;
		while (pValue[i] != L'\0' && i + 1 < (sizeof(wide) / sizeof(wide[0])))
		{
			const wchar_t ch = pValue[i];
			if (ch == L'"' || std::iswspace(ch))
				break;

			wide[i] = ch;
			++i;
		}

		if (i == 0)
			return {};

		char text[256]{};
		const int converted = ::WideCharToMultiByte(
			CP_UTF8,
			0,
			wide,
			-1,
			text,
			static_cast<int>(sizeof(text)),
			nullptr,
			nullptr);
		return converted > 0 ? std::string(text) : std::string{};
	}

	std::string FindServerHostOverride()
	{
		std::string host = ReadCommandLineTextValue(L"--server-host=");
		if (!host.empty())
			return host;

		host = ReadCommandLineTextValue(L"/server-host:");
		if (!host.empty())
			return host;

		host = ReadCommandLineTextValue(L"--server-ip=");
		if (!host.empty())
			return host;

		host = ReadCommandLineTextValue(L"/server-ip:");
		if (!host.empty())
			return host;

		return ReadCommandLineTextValue(L"/server:");
	}

	u16_t ParseServerPortOverride(bool_t& outOverridden)
	{
		const wchar_t* pValue = FindCommandLineValue(L"--server-port=");
		if (!pValue)
			pValue = FindCommandLineValue(L"/server-port:");
		if (!pValue)
			return kDefaultServerPort;

		wchar_t* pEnd = nullptr;
		const unsigned long parsed = std::wcstoul(pValue, &pEnd, 10);
		if (pEnd == pValue || parsed == 0 || parsed > 65535)
			return kDefaultServerPort;

		outOverridden = true;
		return static_cast<u16_t>(parsed);
	}

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
		if (port != kDefaultServerPort || !pHost)
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

CGameSessionClient::ServerEndpoint CGameSessionClient::ResolveServerEndpoint()
{
	ServerEndpoint endpoint{};
	endpoint.host = kDefaultServerHost;
	endpoint.port = kDefaultServerPort;

	const std::string hostOverride = FindServerHostOverride();
	if (!hostOverride.empty())
	{
		endpoint.host = hostOverride;
		endpoint.bFromCommandLine = true;
	}

	bool_t bPortOverridden = false;
	endpoint.port = ParseServerPortOverride(bPortOverridden);
	endpoint.bFromCommandLine = endpoint.bFromCommandLine || bPortOverridden;

	return endpoint;
}

bool CGameSessionClient::Connect(const char* host, u16_t port)
{
	if (IsConnected())
		return true;

	const ServerEndpoint endpoint = ResolveServerEndpoint();
	const char* pConnectHost = (host && host[0] != '\0') ? host : endpoint.host.c_str();
	const u16_t connectPort = port != 0 ? port : endpoint.port;

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

	const bool_t bLocalEndpoint = IsLocalEndpoint(pConnectHost, connectPort);
	const int attempts = bLocalEndpoint ? 20 : 1;
	for (int attempt = 1; attempt <= attempts; ++attempt)
	{
		if (m_pNetwork->Connect(pConnectHost, connectPort))
		{
			Winters::DevSmoke::Log(
				"[GameSessionClient] connected to lobby server host=%s port=%u attempt=%d source=%s\n",
				pConnectHost ? pConnectHost : "-",
				static_cast<u32_t>(connectPort),
				attempt,
				endpoint.bFromCommandLine ? "command-line" : "default");
			return true;
		}

		if (attempt < attempts)
			Sleep(100);
	}

	if (!m_pNetwork->IsConnected())
	{
		Winters::DevSmoke::Log(
			"[GameSessionClient] connect failed host=%s port=%u attempts=%d source=%s\n",
			pConnectHost ? pConnectHost : "-",
			static_cast<u32_t>(connectPort),
			attempts,
			endpoint.bFromCommandLine ? "command-line" : "default");
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
	m_lobbyContext = MatchContext{};
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

void CGameSessionClient::CopyLobbyToMatchContext(MatchContext& outContext) const
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
		return false;
	}

	std::string log = "[GameSessionClient] send lobby command ";
	log += m_strLastLobbyCommandText;
	log += "\n";
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
	m_lobbyContext = MatchContext{};
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
