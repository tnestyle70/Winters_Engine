#pragma once
//winsock2.h 는 Windows.h 보다 먼저 — Defines.h 가 Windows.h 를 transit include 시 충돌 방지
#include <WinSock2.h>
#include <WS2tcpip.h>
#include "Defines.h"
#include "Shared/Network/PacketEnvelope.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <tuple>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

class CUdpClient;

enum class eClientNetworkTransport : u8_t
{
	Tcp = 0,
	Udp,
};

class CClientNetwork final
{
public:
	static std::unique_ptr<CClientNetwork> Create(
		eClientNetworkTransport transport = eClientNetworkTransport::Tcp);
	~CClientNetwork();

	bool Connect(const char* host, u16_t port);
	void Disconnect();

	// UDP owns lane/message sequencing; applicationSequence is retained by the
	// TCP envelope and must not be treated as an authenticated UDP identity.
	bool SendFrame(
		ePacketType type,
		u32_t applicationSequence,
		const u8_t* payload,
		u32_t payloadSize);
	// Compatibility for callers that already own a TCP PacketEnvelope. On UDP
	// the envelope is validated and reduced back to the application frame.
	bool Send(std::vector<u8_t> packet);
	//FrameCallback - main thread에서 pumpreceivedframes 호출 시 invoke
	using FrameCallback = std::function<void(ePacketType, u32_t, const u8_t*, u32_t)>;
	void SetFrameCallback(FrameCallback fn);

	void PumpReceivedFrames();

	bool IsConnected() const;
	eClientNetworkTransport GetTransport() const { return m_eTransport; }
	u32_t GetMyNetEntityId() const { return m_myNetId; }
	void SetMyNetEntityId(u32_t ID) { m_myNetId = ID; }
	u32_t GetMySessionId() const { return m_mySessionId; }
	void SetMySessionId(u32_t sessionId) { m_mySessionId = sessionId; }


private:
	CClientNetwork() = default;

	void RecvThread();

	eClientNetworkTransport m_eTransport = eClientNetworkTransport::Tcp;
	std::unique_ptr<CUdpClient> m_pUdpClient;
	SOCKET m_socket = INVALID_SOCKET;
	std::thread m_recvThread;
	std::atomic<bool> m_bRunning{ false };
	std::atomic<bool> m_bConnected{ false };

	std::vector<u8_t> m_recvAccum; //recv worker 내부 버퍼

	//recv worker -> main thread marshal queue
	std::mutex m_pendingMutex;
	std::vector<std::tuple<ePacketType, u32_t, std::vector<u8_t>>> m_pendingFrames;

	FrameCallback m_callback;
	u32_t m_myNetId = 0;
	u32_t m_mySessionId = 0; 
};
