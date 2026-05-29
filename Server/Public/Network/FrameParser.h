#pragma once 
#include "Shared/Network/PacketEnvelope.h"
#include "WintersTypes.h"
#include <vector>

enum class eFrameParseResult : u8_t
{
	NeedMore = 0,
	Complete = 1,
	Invalid = 2,
};

struct ParsedFrameOwned
{
	ePacketType type = ePacketType::None;
	u32_t sequence = 0;
	std::vector<u8_t> payload;
};

class CFrameParser
{
public:
	void Append(const u8_t* bytes, u32_t len);
	eFrameParseResult TryPop(ParsedFrameOwned& outFrame);
	u32_t BufferedBytes() const { return static_cast<u32_t>(m_Buffer.size()); }
	void Clear();

private:
	static constexpr u32_t kMaxPayloadBytes = 64 * 1024;
	static constexpr u32_t kMaxBufferBytes = 256 * 1024;

	std::vector<u8_t> m_Buffer;
};