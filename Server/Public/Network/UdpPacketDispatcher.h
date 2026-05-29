#pragma once

#include "Shared/Network/UdpPacketHeader.h"
#include "WintersTypes.h"

#include <WinSock2.h>

class CUdpPacketDispatcher final
{
public:
	static CUdpPacketDispatcher& Instance();

	void DispatchDatagram(const sockaddr_in& from, const u8_t* bytes, u32_t len);

private:
	CUdpPacketDispatcher() = default;

	void DispatchHello(const sockaddr_in& from, const UdpPacketHeader& header);
	void DispatchCommandBatch(u32_t sessionId, const u8_t* payload, u32_t len);
};