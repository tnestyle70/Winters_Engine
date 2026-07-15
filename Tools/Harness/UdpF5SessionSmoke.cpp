#include "UdpClient.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace
{
    using namespace std::chrono_literals;

    u16_t ParsePort(const char* text)
    {
        if (!text || text[0] == '\0')
            return 0u;
        char* end = nullptr;
        const unsigned long value = std::strtoul(text, &end, 10);
        if (end == text || *end != '\0' || value == 0u || value > 65'535u)
            return 0u;
        return static_cast<u16_t>(value);
    }
}

int main(int argc, char** argv)
{
    const char* host = argc > 1 ? argv[1] : "127.0.0.1";
    const u16_t port = argc > 2 ? ParsePort(argv[2]) : 9'000u;
    if (port == 0u)
    {
        std::fprintf(stderr, "[UdpF5SessionSmoke] invalid port\n");
        return 2;
    }

    auto client = CUdpClient::Create();
    if (!client)
    {
        std::fprintf(stderr, "[UdpF5SessionSmoke] client create failed\n");
        return 3;
    }

    std::atomic<u32_t> helloCount{ 0u };
    std::atomic<u32_t> lobbyStateCount{ 0u };
    std::atomic<u32_t> invalidFrames{ 0u };
    client->SetFrameCallback(
        [&](ePacketType type, u32_t, const u8_t* payload, u32_t payloadSize)
        {
            if (IsTransportHandshakePacketType(type) ||
                (payloadSize != 0u && !payload))
            {
                invalidFrames.fetch_add(1u, std::memory_order_relaxed);
                return;
            }
            if (type == ePacketType::Hello && payloadSize != 0u)
                helloCount.fetch_add(1u, std::memory_order_relaxed);
            else if (type == ePacketType::LobbyState && payloadSize != 0u)
                lobbyStateCount.fetch_add(1u, std::memory_order_relaxed);
        });

    if (!client->Connect(host, port))
    {
        std::fprintf(
            stderr,
            "[UdpF5SessionSmoke] connect failed host=%s port=%u\n",
            host,
            static_cast<unsigned>(port));
        return 4;
    }

    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (std::chrono::steady_clock::now() < deadline &&
        (helloCount.load(std::memory_order_relaxed) == 0u ||
            lobbyStateCount.load(std::memory_order_relaxed) == 0u))
    {
        client->PumpReceivedFrames();
        std::this_thread::sleep_for(5ms);
    }
    client->PumpReceivedFrames();

    const UdpClientMetrics metrics = client->GetMetrics();
    const u32_t hello = helloCount.load(std::memory_order_relaxed);
    const u32_t lobby = lobbyStateCount.load(std::memory_order_relaxed);
    const u32_t invalid = invalidFrames.load(std::memory_order_relaxed);
    std::printf(
        "[UdpF5SessionSmoke] hello=%u lobby=%u invalid=%u recvDg=%llu sendDg=%llu retransmit=%llu drops=%llu\n",
        hello,
        lobby,
        invalid,
        static_cast<unsigned long long>(metrics.recvDatagrams),
        static_cast<unsigned long long>(metrics.sendDatagrams),
        static_cast<unsigned long long>(metrics.retransmits),
        static_cast<unsigned long long>(metrics.outboundQueueDrops));

    client->Disconnect();
    return hello != 0u && lobby != 0u && invalid == 0u ? 0 : 5;
}
