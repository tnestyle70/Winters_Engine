#pragma once

#include "WintersTypes.h"

#include <WinSock2.h>
#include <atomic>
#include <memory>
#include <thread>

class CUdpCore final
{
public:
	static std::unique_ptr<CUdpCore> Create(u16_t port);
	~CUdpCore();

	bool Start();
	void Shutdown();
	bool SendTo(const sockaddr_in& addr, const u8_t* bytes, u32_t len);

private:
	explicit CUdpCore(u16_t port);

	void RecvLoop();

	SOCKET m_socket = INVALID_SOCKET;
	u16_t m_port = 0;
	std::thread m_recvThread;
	std::atomic<bool> m_bRunning{ false };
};