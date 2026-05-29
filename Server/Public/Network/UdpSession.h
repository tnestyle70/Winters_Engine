#pragma once

#include "Shared/Network/UdpPacketHeader.h"
#include "WintersTypes.h"

#include <WinSock2.h>
#include <memory>

class CUdpSession final
{
public:
	static std::shared_ptr<CUdpSession> Create(u32_t sessionId, const sockaddr_in& remoteAddr);

	u32_t GetSessionId() const { return m_sessionId; }
	const sockaddr_in& GetRemoteAddr() const { return m_remoteAddr; }

private:
	CUdpSession(u32_t sessionId, const sockaddr_in& remoteAddr);

	u32_t m_sessionId = 0;
	sockaddr_in m_remoteAddr{};
};