#include "Network/Client/ClientNetwork.h"
#include "Dev/SmokeLog.h"

#include <iostream>
#include <cstring>
#include <atomic>
#include <cstdio>

namespace
{
    //WSAstartup ref count 동일 process 내 다중 Client Network 가능성 대비
    std::atomic<int> g_WsaRefCount{ 0 };

    bool EnsureWsaInit()
    {
        if (g_WsaRefCount.fetch_add(1, std::memory_order_acq_rel) == 0)
        {
            WSADATA wsa{};
            const int result = WSAStartup(MAKEWORD(2, 2), &wsa);
            if (result != 0)
            {
                g_WsaRefCount.store(0, std::memory_order_release);
                Winters::DevSmoke::Log("[ClientNetwork] WSAStartup failed wsa=%d\n", result);
                return false;
            }
        }
        return true;
    }

    void ReleaseWsa()
    {
        if (g_WsaRefCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
            WSACleanup();
    }

    void OutputSocketError(const char* op, const char* host, u16_t port, int error)
    {
        char msg[256]{};
        sprintf_s(msg,
            "[ClientNetwork] %s failed host=%s port=%u wsa=%d\n",
            op ? op : "socket op",
            host ? host : "-",
            static_cast<u32_t>(port),
            error);
        Winters::DevSmoke::Log("%s", msg);
    }
}


std::unique_ptr<CClientNetwork> CClientNetwork::Create()
{
    if (!EnsureWsaInit()) return nullptr;
    return std::unique_ptr<CClientNetwork>(new CClientNetwork());
}

CClientNetwork::~CClientNetwork()
{
    Disconnect();
    ReleaseWsa();
}

bool CClientNetwork::Connect(const char* host, u16_t port)
{
    if (m_bConnected) return true;

    m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_socket == INVALID_SOCKET)
    {
        OutputSocketError("socket()", host, port, WSAGetLastError());
        return false;
    }
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    const char* pConnectHost = host;
    if (host && std::strcmp(host, "localhost") == 0)
        pConnectHost = "127.0.0.1";

    if (inet_pton(AF_INET, pConnectHost, &addr.sin_addr) != 1)
    {
        OutputSocketError("inet_pton()", host, port, WSAGetLastError());
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        return false;
    }

    if (connect(m_socket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        OutputSocketError("connect()", host, port, WSAGetLastError());
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        return false;
    }

    BOOL on = TRUE;
    setsockopt(m_socket, IPPROTO_TCP, TCP_NODELAY, (const char*)&on, sizeof(on));

    m_bRunning = true;
    m_bConnected = true;
    m_recvAccum.clear();
    m_recvThread = std::thread(&CClientNetwork::RecvThread, this);
    
    std::cout << "[ClientNetwork] connected " << host << ":" << port << "\n";
    Winters::DevSmoke::Log(
        "[ClientNetwork] connected host=%s port=%u\n",
        host ? host : "-",
        static_cast<u32_t>(port));
    return true;
}

void CClientNetwork::Disconnect()
{
    if (!m_bRunning.exchange(false)) return;
    m_bConnected = false;

    if (m_socket != INVALID_SOCKET)
    {
        shutdown(m_socket, SD_BOTH);
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }

    if (m_recvThread.joinable()) m_recvThread.join();
}

bool CClientNetwork::Send(std::vector<u8_t> packet)
{
    if (!m_bConnected || m_socket == INVALID_SOCKET || packet.empty()) return false;

    //단순 blocking send(MVP - 동시 호출은 inputsystem 단일 thread 가정)
    int total = 0;
    while (total < static_cast<int>(packet.size()))
    {
        int sent = send(m_socket,
            reinterpret_cast<const char*>(packet.data() + total),
            static_cast<int>(packet.size() - total), 0);
        if (sent == SOCKET_ERROR)
        {
            OutputSocketError("send()", nullptr, 0, WSAGetLastError());
            m_bConnected = false;
            return false;
        }
        total += sent;
    }

    return true;
}

void CClientNetwork::SetFrameCallback(FrameCallback fn)
{
    m_callback = std::move(fn);
}

void CClientNetwork::PumpReceivedFrames()
{
    if (!m_callback) return;

    std::vector<std::tuple<ePacketType, u32_t, std::vector<u8_t>>> drained;
    {
        std::lock_guard lk(m_pendingMutex);
        drained.swap(m_pendingFrames);
    }
    for (auto& [type, seq, payload] : drained)
        m_callback(type, seq, payload.data(), static_cast<u32_t>(payload.size()));
}

void CClientNetwork::RecvThread()
{
    char buf[8192]{};
    
    while (m_bRunning)
    {
        int n = recv(m_socket, buf, sizeof(buf), 0);
        if (n <= 0)
        {
            Winters::DevSmoke::Log(
                "[ClientNetwork] recv closed bytes=%d wsa=%d\n",
                n,
                n == SOCKET_ERROR ? WSAGetLastError() : 0);
            m_bConnected = false;
            break;
        }
        m_recvAccum.insert(m_recvAccum.end(),
            reinterpret_cast<u8_t*>(buf),
            reinterpret_cast<u8_t*>(buf) + n);
        //frame extract
        for (;;)
        {
            if (m_recvAccum.size() < sizeof(PacketHeader)) break;

            PacketHeader hdr{};
            std::memcpy(&hdr, m_recvAccum.data(), sizeof(PacketHeader));

            if (hdr.magic != kPacketMagic || hdr.version != kPacketVersion ||
                hdr.payloadSize > kMaxPacketPayloadSize)
            {
                Winters::DevSmoke::Log(
                    "[ClientNetwork] invalid packet header magic=0x%08X version=%u payload=%u\n",
                    hdr.magic,
                    hdr.version,
                    hdr.payloadSize);
                m_recvAccum.clear();
                m_bConnected = false;
                break;
            }
            const u32_t total = sizeof(PacketHeader) + hdr.payloadSize;
            if (m_recvAccum.size() < total) break;

            std::vector<u8_t> payload(
                m_recvAccum.begin() + sizeof(PacketHeader),
                m_recvAccum.begin() + total);
            {
                std::lock_guard lk(m_pendingMutex);
                m_pendingFrames.emplace_back(
                    static_cast<ePacketType>(hdr.type), hdr.sequence, std::move(payload));
            }
            m_recvAccum.erase(m_recvAccum.begin(), m_recvAccum.begin() + total);
        }
    }
}
