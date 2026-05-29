#pragma once

#include "Shared/Network/UdpPacketHeader.h"
#include "Shared/Network/PacketEnvelope.h"
#include "Defines.h"

#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <tuple>
#include <vector>
#include <atomic>
#include <WinSock2.h>
#include <WS2tcpip.h>

class CUdpClient final
{
public:
	static std::unique_ptr<CUdpClient> Create();
	~CUdpClient();

	using FrameCallback = std::function<void(ePacketType, u32_t, const u8_t*, u32_t)>;

	bool Connect(const char* host, u16_t port);
	void Disconnect();
	bool Send(ePacketType type, eUdpChannel channel, const u8_t* payload, u32_t payloadSize);
	void SetFrameCallback(FrameCallback fn);
	void PumpReceivedFrames();

private:
	CUdpClient() = default;

	void RecvThread();

	SOCKET m_socket = INVALID_SOCKET;
	sockaddr_in m_serverAddr{};
	std::thread m_recvThread;
	std::atomic<bool> m_bRunning{ false };
	std::atomic<bool> m_bConnected{ false };

	std::mutex m_pendingMutex;
	std::vector<std::tuple<ePacketType, u32_t, std::vector<u8_t>>> m_pendingFrames;
	FrameCallback m_callback;
};
