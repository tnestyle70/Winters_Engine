#include "Network/FrameParser.h"

#include <cstring>

void CFrameParser::Append(const u8_t* bytes, u32_t len)
{
	if (bytes == nullptr || len == 0) return;
	m_Buffer.insert(m_Buffer.end(), bytes, bytes + len);
	if (m_Buffer.size() > kMaxBufferBytes)
		Clear();
}

eFrameParseResult CFrameParser::TryPop(ParsedFrameOwned & outFrame)
{
	outFrame = {};

	if (m_Buffer.size() < sizeof(PacketHeader))
		return eFrameParseResult::NeedMore;

	PacketHeader hdr{};
	std::memcpy(&hdr, m_Buffer.data(), sizeof(PacketHeader));

	if (hdr.magic != kPacketMagic || hdr.version != kPacketVersion)
	{
		Clear();
		return eFrameParseResult::Invalid;
	}
	if (hdr.payloadSize > kMaxPayloadBytes)
	{
		Clear();
		return eFrameParseResult::Invalid;
	}

	const u32_t totalSize = sizeof(PacketHeader) + hdr.payloadSize;
	if (m_Buffer.size() < totalSize)
		return eFrameParseResult::NeedMore;

	outFrame.type = static_cast<ePacketType>(hdr.type);
	outFrame.sequence = hdr.sequence;
	outFrame.payload.assign(
		m_Buffer.data() + sizeof(PacketHeader),
		m_Buffer.data() + totalSize
	);

	m_Buffer.erase(m_Buffer.begin(), m_Buffer.begin() + totalSize);

	return eFrameParseResult::Complete;
}

void CFrameParser::Clear()
{
	m_Buffer.clear();
}
