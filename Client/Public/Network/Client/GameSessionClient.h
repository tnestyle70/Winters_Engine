#pragma once

#include "Defines.h"
#include "GameContext.h"
#include "Shared/Network/PacketEnvelope.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

class CClientNetwork;
namespace Shared::Schema { enum class LobbyCommandKind : uint8_t; }

class CGameSessionClient final
{
public:
	using FrameCallback = std::function<void(ePacketType, u32_t, const u8_t*, u32_t)>;

	struct ServerEndpoint
	{
		std::string host = "127.0.0.1";
		u16_t port = 9000;
		bool_t bFromCommandLine = false;
	};

	static CGameSessionClient& Instance();
	static ServerEndpoint ResolveServerEndpoint();

	bool Connect(const char* host = nullptr, u16_t port = 0);
	void Disconnect();
	void Pump();

	bool IsConnected() const;
	bool HasLobbyState() const { return m_bHasLobbyState; }
	bool IsGameStarting() const { return m_bGameStarting; }
	bool IsServerLoading() const { return m_bServerLoading; }
	void ClearServerLoading() { m_bServerLoading = false; }
	void ClearGameStarting() { m_bGameStarting = false; }

	const GameContext& GetLobbyContext() const { return m_lobbyContext; }
	void CopyLobbyToGameContext(GameContext& outContext) const;

	bool SendLobbyCommand(
		Shared::Schema::LobbyCommandKind kind,
		u8_t slotId,
		eChampion champion = eChampion::END,
		u8_t botDifficulty = 0,
		u32_t value = 0);

	void SetGameFrameCallback(FrameCallback callback);
	void ReplayLastHelloToGameFrameCallback();

	CClientNetwork* GetNetwork() const { return m_pNetwork.get(); }
	u32_t GetMySessionId() const { return m_lobbyContext.MySessionId; }
	u32_t GetMyNetId() const { return m_lobbyContext.MyNetId; }
	u32_t GetLobbyRevision() const { return m_uLobbyRevision; }
	u8_t GetLobbyPhase() const { return m_uLobbyPhase; }
	u32_t GetGameStartCount() const { return m_uGameStartCount; }
	const char* GetLastLobbyMessage() const { return m_strLastLobbyMessage.c_str(); }
	const char* GetLastLobbyCommandText() const { return m_strLastLobbyCommandText.c_str(); }

private:
	CGameSessionClient() = default;

	void OnFrame(ePacketType type, u32_t sequence, const u8_t* payload, u32_t len);
	void OnLobbyState(const u8_t* payload, u32_t len);
	void OnHello(const u8_t* payload, u32_t len);

	std::unique_ptr<CClientNetwork> m_pNetwork;
	GameContext m_lobbyContext{};
	bool_t m_bHasLobbyState = false;
	bool_t m_bGameStarting = false;
	u32_t m_nextLobbySequence = 1;
	u32_t m_lastHelloSequence = 0;
	u32_t m_uLobbyRevision = 0;
	u8_t m_uLobbyPhase = 0;
	u32_t m_uGameStartCount = 0;
	std::vector<u8_t> m_lastHelloPayload;
	std::string m_strLastLobbyMessage;
	std::string m_strLastLobbyCommandText;
	FrameCallback m_gameFrameCallback;
	bool_t m_bServerLoading = false;
};
