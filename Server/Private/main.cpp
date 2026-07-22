#include "Core/JobCounter.h"
#include "Backend/ReplayUploadQueue.h"
#include "Game/GameRoom.h"
#include "Game/ServerEntry.h"
#include "Network/IOCPCore.h"
#include "Network/ServerSessionHub.h"
#include "Network/UdpIocpCore.h"
#include "Shared/Network/UdpHandshake.h"
#include "Server/Private/Data/ThirdParty/json.hpp"
#include "Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h"

#include <WinSock2.h>
#include <Windows.h>
#include <bcrypt.h>
#include <atomic>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

CGameRoom* g_pRoom = nullptr;

namespace
{
    constexpr const char* kAIShadowPolicyPathPrefix = "--ai-shadow-policy=";
    constexpr const char* kAIShadowPolicyShaPrefix = "--ai-shadow-policy-sha256=";

    enum class eServerNetworkMode : u8_t
    {
        Tcp = 0,
        Udp,
        Dual,
    };

    struct ServerRuntimeOptions
    {
        eServerNetworkMode networkMode = eServerNetworkMode::Tcp;
        eJobExecutionMode jobMode = eJobExecutionMode::ThreadOnly;
        u32_t jobWorkerCount = 1u;
        bool_t bUdpDevAllowEmptyTicket = false;
        std::string matchTicketSecret;
        std::string gameSessionID;
        u64_t matchTicketTTLSeconds = 300u;
    };

    constexpr u32_t kMaxJobWorkerCount = 256u;
    constexpr u32_t kMaxMatchTicketTTLSeconds = 3600u;

    u32_t DefaultJobWorkerCount()
    {
        constexpr u32_t kReservedRuntimeThreads = 6u;
        const u32_t hardwareThreads = std::thread::hardware_concurrency();
        return hardwareThreads > kReservedRuntimeThreads
            ? hardwareThreads - kReservedRuntimeThreads
            : 1u;
    }

    bool_t ParsePositiveU32(const char* text, u32_t& outValue)
    {
        if (!text || text[0] == '\0')
            return false;

        errno = 0;
        char* pEnd = nullptr;
        const unsigned long value = std::strtoul(text, &pEnd, 10);
        if (errno == ERANGE || pEnd == text || *pEnd != '\0' ||
            value == 0ul ||
            value > (std::numeric_limits<u32_t>::max)() ||
            value > kMaxJobWorkerCount)
        {
            return false;
        }

        outValue = static_cast<u32_t>(value);
        return true;
    }

    bool_t ParseMatchTicketTTL(const char* text, u64_t& outValue)
    {
        if (!text || text[0] == '\0')
            return false;

        errno = 0;
        char* pEnd = nullptr;
        const unsigned long value = std::strtoul(text, &pEnd, 10);
        if (errno == ERANGE || pEnd == text || *pEnd != '\0' ||
            value == 0ul || value > kMaxMatchTicketTTLSeconds)
        {
            return false;
        }

        outValue = static_cast<u64_t>(value);
        return true;
    }

    bool_t ReadEnvironmentVariable(
        const char* name,
        std::string& outValue)
    {
        char* value = nullptr;
        size_t valueLength = 0u;
        const errno_t result = _dupenv_s(&value, &valueLength, name);
        if (result != 0 || !value || valueLength <= 1u)
        {
            std::free(value);
            outValue.clear();
            return false;
        }

        outValue.assign(value, valueLength - 1u);
        std::free(value);
        return true;
    }

    bool_t UsesTcp(eServerNetworkMode mode)
    {
        return mode == eServerNetworkMode::Tcp ||
            mode == eServerNetworkMode::Dual;
    }

    bool_t UsesUdp(eServerNetworkMode mode)
    {
        return mode == eServerNetworkMode::Udp ||
            mode == eServerNetworkMode::Dual;
    }

    struct Win32HandleCloser
    {
        void operator()(void* value) const noexcept
        {
            if (value)
                ::CloseHandle(static_cast<HANDLE>(value));
        }
    };

    using ScopedWin32Handle = std::unique_ptr<void, Win32HandleCloser>;

    bool_t AcquireAuthenticatedUdpInstanceGuard(
        const ServerRuntimeOptions& runtimeOptions,
        ScopedWin32Handle& outGuard)
    {
        outGuard.reset();
        if (!UsesUdp(runtimeOptions.networkMode) ||
            runtimeOptions.bUdpDevAllowEmptyTicket)
        {
            return true;
        }

        constexpr const wchar_t* kMutexName =
            L"Local\\WintersServer.AuthenticatedUdp9000";
        HANDLE const rawHandle = ::CreateMutexW(nullptr, FALSE, kMutexName);
        const DWORD createError = ::GetLastError();
        if (!rawHandle)
        {
            std::cerr << "[ERROR] Failed to create authenticated UDP instance guard error="
                << createError << '\n';
            return false;
        }

        ScopedWin32Handle guard(rawHandle);
        if (createError == ERROR_ALREADY_EXISTS)
        {
            std::cerr << "[ERROR] Another authenticated UDP WintersServer is already running; "
                "startup stopped before backend reconciliation\n";
            return false;
        }

        outGuard = std::move(guard);
        return true;
    }

    const char* NetworkModeName(eServerNetworkMode mode)
    {
        switch (mode)
        {
        case eServerNetworkMode::Udp:
            return "udp";
        case eServerNetworkMode::Dual:
            return "dual";
        case eServerNetworkMode::Tcp:
        default:
            return "tcp";
        }
    }

    const char* JobModeName(eJobExecutionMode mode)
    {
        switch (mode)
        {
        case eJobExecutionMode::FiberShell:
            return "fiber-shell";
        case eJobExecutionMode::FiberFull:
            return "fiber-full";
        case eJobExecutionMode::ThreadOnly:
        default:
            return "thread";
        }
    }

    bool_t ParseRuntimeOptions(
        int argc,
        char** argv,
        ServerRuntimeOptions& outOptions)
    {
        constexpr const char* kNetworkPrefix = "--net-transport=";
        constexpr const char* kJobModePrefix = "--job-mode=";
        constexpr const char* kJobWorkersPrefix = "--job-workers=";
        constexpr const char* kUdpDevEmptyTicket =
            "--udp-dev-allow-empty-ticket";

        outOptions = {};
        outOptions.jobWorkerCount = DefaultJobWorkerCount();
        ReadEnvironmentVariable(
            "WINTERS_MATCH_TICKET_SECRET",
            outOptions.matchTicketSecret);
        ReadEnvironmentVariable(
            "WINTERS_GAME_SESSION_ID",
            outOptions.gameSessionID);
        std::string ticketTTL;
        if (ReadEnvironmentVariable(
            "WINTERS_MATCH_TICKET_TTL_SECONDS",
            ticketTTL))
        {
            if (!ParseMatchTicketTTL(
                ticketTTL.c_str(),
                outOptions.matchTicketTTLSeconds))
            {
                std::cerr << "[ERROR] WINTERS_MATCH_TICKET_TTL_SECONDS expects an integer from 1 to "
                    << kMaxMatchTicketTTLSeconds << '\n';
                return false;
            }
        }

        bool_t bSawNetwork = false;
        bool_t bSawJobMode = false;
        bool_t bSawJobWorkers = false;
        bool_t bSawUdpDevTicket = false;
        for (int i = 1; i < argc; ++i)
        {
            const char* argument = argv[i];
            if (std::strncmp(
                argument,
                kNetworkPrefix,
                std::strlen(kNetworkPrefix)) == 0)
            {
                if (bSawNetwork)
                {
                    std::cerr << "[ERROR] Duplicate --net-transport option\n";
                    return false;
                }
                bSawNetwork = true;
                const char* value = argument + std::strlen(kNetworkPrefix);
                if (std::strcmp(value, "tcp") == 0)
                    outOptions.networkMode = eServerNetworkMode::Tcp;
                else if (std::strcmp(value, "udp") == 0)
                    outOptions.networkMode = eServerNetworkMode::Udp;
                else if (std::strcmp(value, "dual") == 0)
                    outOptions.networkMode = eServerNetworkMode::Dual;
                else
                {
                    std::cerr << "[ERROR] --net-transport expects tcp, udp, or dual\n";
                    return false;
                }
            }
            else if (std::strncmp(
                argument,
                kJobModePrefix,
                std::strlen(kJobModePrefix)) == 0)
            {
                if (bSawJobMode)
                {
                    std::cerr << "[ERROR] Duplicate --job-mode option\n";
                    return false;
                }
                bSawJobMode = true;
                const char* value = argument + std::strlen(kJobModePrefix);
                if (std::strcmp(value, "thread") == 0)
                    outOptions.jobMode = eJobExecutionMode::ThreadOnly;
                else if (std::strcmp(value, "fiber-shell") == 0)
                    outOptions.jobMode = eJobExecutionMode::FiberShell;
                else if (std::strcmp(value, "fiber-full") == 0)
                    outOptions.jobMode = eJobExecutionMode::FiberFull;
                else
                {
                    std::cerr << "[ERROR] --job-mode expects thread, fiber-shell, or fiber-full\n";
                    return false;
                }
            }
            else if (std::strncmp(
                argument,
                kJobWorkersPrefix,
                std::strlen(kJobWorkersPrefix)) == 0)
            {
                if (bSawJobWorkers || !ParsePositiveU32(
                    argument + std::strlen(kJobWorkersPrefix),
                    outOptions.jobWorkerCount))
                {
                    std::cerr << "[ERROR] --job-workers expects an integer from 1 to "
                        << kMaxJobWorkerCount << '\n';
                    return false;
                }
                bSawJobWorkers = true;
            }
            else if (std::strcmp(argument, kUdpDevEmptyTicket) == 0)
            {
                if (bSawUdpDevTicket)
                {
                    std::cerr << "[ERROR] Duplicate --udp-dev-allow-empty-ticket option\n";
                    return false;
                }
                bSawUdpDevTicket = true;
                outOptions.bUdpDevAllowEmptyTicket = true;
            }
        }

        if (outOptions.bUdpDevAllowEmptyTicket &&
            !UsesUdp(outOptions.networkMode))
        {
            std::cerr << "[ERROR] Empty-ticket development mode requires udp or dual transport\n";
            return false;
        }
#if !defined(_DEBUG)
        if (outOptions.bUdpDevAllowEmptyTicket)
        {
            std::cerr << "[ERROR] Empty-ticket UDP authentication is Debug-only\n";
            return false;
        }
#endif
        if (UsesUdp(outOptions.networkMode) &&
            !outOptions.bUdpDevAllowEmptyTicket &&
            outOptions.matchTicketSecret.size() < 32u)
        {
            std::cerr << "[ERROR] UDP startup requires a ticket validator; "
                "set WINTERS_MATCH_TICKET_SECRET to at least 32 bytes or use "
                "--udp-dev-allow-empty-ticket in Debug\n";
            return false;
        }
        if (UsesUdp(outOptions.networkMode) &&
            !outOptions.bUdpDevAllowEmptyTicket &&
            outOptions.gameSessionID.empty())
        {
            std::cerr << "[ERROR] Authenticated UDP startup requires WINTERS_GAME_SESSION_ID\n";
            return false;
        }
        return true;
    }

    struct JobStartupProbeState
    {
        std::atomic<bool_t> bChildStarted{ false };
        std::atomic<bool_t> bParentEnteredWait{ false };
        std::atomic<bool_t> bReleaseChild{ false };
        std::atomic<bool_t> bChildCompleted{ false };
        std::atomic<bool_t> bParentCompleted{ false };
    };

    bool_t RunJobSystemStartupProbe(
        CJobSystem& jobs,
        eJobExecutionMode mode)
    {
        const CJobSystemStats before = jobs.GetStats();
        JobStartupProbeState state{};
        CJobCounter parentCounter;

        try
        {
            jobs.Submit(
                [&jobs, &state]()
                {
                    CJobCounter childCounter;
                    jobs.Submit(
                        [&state]()
                        {
                            state.bChildStarted.store(
                                true,
                                std::memory_order_release);
                            while (!state.bReleaseChild.load(
                                std::memory_order_acquire))
                            {
                                std::this_thread::yield();
                            }
                            state.bChildCompleted.store(
                                true,
                                std::memory_order_release);
                        },
                        &childCounter);

                    state.bParentEnteredWait.store(
                        true,
                        std::memory_order_release);
                    jobs.WaitForCounter(&childCounter);
                    state.bParentCompleted.store(
                        state.bChildCompleted.load(std::memory_order_acquire),
                        std::memory_order_release);
                },
                &parentCounter);
        }
        catch (const std::exception& exception)
        {
            std::cerr << "[ServerEntry] Startup probe submit failed: "
                << exception.what() << '\n';
            return false;
        }
        catch (...)
        {
            std::cerr << "[ServerEntry] Startup probe submit failed\n";
            return false;
        }

        const auto observationDeadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (std::chrono::steady_clock::now() < observationDeadline &&
            (!state.bChildStarted.load(std::memory_order_acquire) ||
                !state.bParentEnteredWait.load(std::memory_order_acquire)))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        const bool_t bWorkerExecutionObserved =
            state.bChildStarted.load(std::memory_order_acquire) &&
            state.bParentEnteredWait.load(std::memory_order_acquire);
        bool_t bFiberWaitObserved = true;
        if (mode == eJobExecutionMode::FiberFull)
        {
            bFiberWaitObserved = false;
            while (std::chrono::steady_clock::now() < observationDeadline)
            {
                if (jobs.GetStats().uFiberWaits > before.uFiberWaits)
                {
                    bFiberWaitObserved = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        state.bReleaseChild.store(true, std::memory_order_release);
        jobs.WaitForCounter(&parentCounter);

        const CJobSystemStats after = jobs.GetStats();
        const u64_t submitted = after.uSubmitted - before.uSubmitted;
        const u64_t executed = after.uExecuted - before.uExecuted;
        const u64_t switches = after.uFiberSwitches - before.uFiberSwitches;
        const u64_t waits = after.uFiberWaits - before.uFiberWaits;
        const u64_t resumes = after.uFiberResumes - before.uFiberResumes;

        bool_t bPassed = bWorkerExecutionObserved &&
            state.bChildCompleted.load(std::memory_order_acquire) &&
            state.bParentCompleted.load(std::memory_order_acquire) &&
            submitted == 2u &&
            executed == 2u &&
            after.uFailed == before.uFailed;
        if (mode == eJobExecutionMode::FiberShell)
            bPassed = bPassed && switches > 0u;
        else if (mode == eJobExecutionMode::FiberFull)
            bPassed = bPassed && bFiberWaitObserved && waits > 0u &&
                waits == resumes;

        std::cout << "[ServerEntry] JobSystem probe mode="
            << JobModeName(mode)
            << " workers=" << jobs.GetWorkerCount()
            << " submitted=" << submitted
            << " executed=" << executed
            << " switches=" << switches
            << " waits=" << waits
            << " resumes=" << resumes
            << " result=" << (bPassed ? "PASS" : "FAIL") << '\n';
        return bPassed;
    }

    u32_t ParseSmokeSeconds(int argc, char** argv)
    {
        constexpr const char* kPrefix = "--smoke-seconds=";
        constexpr size_t kPrefixLen = 16;

        for (int i = 1; i < argc; ++i)
        {
            if (std::strncmp(argv[i], kPrefix, kPrefixLen) != 0)
                continue;

            const unsigned long value = std::strtoul(argv[i] + kPrefixLen, nullptr, 10);
            return static_cast<u32_t>(value);
        }

        return 0;
    }

    bool_t IsLowercaseSha256(const std::string& value)
    {
        if (value.size() != 64u)
            return false;
        for (char ch : value)
        {
            if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f')))
                return false;
        }
        return true;
    }

    bool_t ReadBinaryFile(const std::string& path, std::vector<u8_t>& outBytes)
    {
        std::ifstream stream(path, std::ios::binary | std::ios::ate);
        if (!stream)
            return false;

        const std::streamoff end = stream.tellg();
        if (end <= 0 || end > 1024 * 1024)
            return false;
        outBytes.resize(static_cast<size_t>(end));
        stream.seekg(0, std::ios::beg);
        stream.read(
            reinterpret_cast<char*>(outBytes.data()),
            static_cast<std::streamsize>(outBytes.size()));
        return stream.good();
    }

    bool_t ComputeSha256(
        const std::vector<u8_t>& bytes,
        std::string& outHex)
    {
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        DWORD objectLength = 0u;
        DWORD hashLength = 0u;
        DWORD written = 0u;

        NTSTATUS status = BCryptOpenAlgorithmProvider(
            &algorithm,
            BCRYPT_SHA256_ALGORITHM,
            nullptr,
            0u);
        if (status < 0)
            return false;

        status = BCryptGetProperty(
            algorithm,
            BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&objectLength),
            sizeof(objectLength),
            &written,
            0u);
        if (status >= 0)
        {
            status = BCryptGetProperty(
                algorithm,
                BCRYPT_HASH_LENGTH,
                reinterpret_cast<PUCHAR>(&hashLength),
                sizeof(hashLength),
                &written,
                0u);
        }

        std::vector<u8_t> object;
        std::vector<u8_t> digest;
        if (status >= 0 && hashLength == 32u)
        {
            object.resize(objectLength);
            digest.resize(hashLength);
            status = BCryptCreateHash(
                algorithm,
                &hash,
                object.data(),
                static_cast<ULONG>(object.size()),
                nullptr,
                0u,
                0u);
        }
        else if (status >= 0)
        {
            status = static_cast<NTSTATUS>(-1);
        }

        if (status >= 0)
        {
            status = BCryptHashData(
                hash,
                const_cast<PUCHAR>(
                    reinterpret_cast<const UCHAR*>(bytes.data())),
                static_cast<ULONG>(bytes.size()),
                0u);
        }
        if (status >= 0)
        {
            status = BCryptFinishHash(
                hash,
                digest.data(),
                static_cast<ULONG>(digest.size()),
                0u);
        }

        if (hash != nullptr)
            BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0u);
        if (status < 0)
            return false;

        static constexpr char kHex[] = "0123456789abcdef";
        outHex.clear();
        outHex.reserve(digest.size() * 2u);
        for (u8_t byte : digest)
        {
            outHex.push_back(kHex[byte >> 4u]);
            outHex.push_back(kHex[byte & 0x0Fu]);
        }
        return true;
    }

    bool_t DecodeBase64Url(
        std::string_view encoded,
        std::vector<u8_t>& outBytes)
    {
        if (encoded.empty() || encoded.size() % 4u == 1u)
            return false;

        outBytes.clear();
        outBytes.reserve((encoded.size() * 3u) / 4u);
        u32_t bits = 0u;
        u32_t bitCount = 0u;
        for (const char ch : encoded)
        {
            u32_t value = 0u;
            if (ch >= 'A' && ch <= 'Z')
                value = static_cast<u32_t>(ch - 'A');
            else if (ch >= 'a' && ch <= 'z')
                value = static_cast<u32_t>(ch - 'a' + 26);
            else if (ch >= '0' && ch <= '9')
                value = static_cast<u32_t>(ch - '0' + 52);
            else if (ch == '-')
                value = 62u;
            else if (ch == '_')
                value = 63u;
            else
                return false;

            bits = (bits << 6u) | value;
            bitCount += 6u;
            if (bitCount >= 8u)
            {
                bitCount -= 8u;
                outBytes.push_back(static_cast<u8_t>(bits >> bitCount));
                bits &= bitCount == 0u ? 0u : (1u << bitCount) - 1u;
            }
        }

        return bitCount == 0u || bits == 0u;
    }

    bool_t ComputeHmacSha256(
        std::string_view secret,
        std::string_view message,
        std::array<u8_t, 32u>& outDigest)
    {
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        DWORD objectLength = 0u;
        DWORD hashLength = 0u;
        DWORD written = 0u;

        NTSTATUS status = BCryptOpenAlgorithmProvider(
            &algorithm,
            BCRYPT_SHA256_ALGORITHM,
            nullptr,
            BCRYPT_ALG_HANDLE_HMAC_FLAG);
        if (status < 0)
            return false;

        status = BCryptGetProperty(
            algorithm,
            BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&objectLength),
            sizeof(objectLength),
            &written,
            0u);
        if (status >= 0)
        {
            status = BCryptGetProperty(
                algorithm,
                BCRYPT_HASH_LENGTH,
                reinterpret_cast<PUCHAR>(&hashLength),
                sizeof(hashLength),
                &written,
                0u);
        }

        std::vector<u8_t> object;
        if (status >= 0 && hashLength == outDigest.size())
        {
            object.resize(objectLength);
            status = BCryptCreateHash(
                algorithm,
                &hash,
                object.data(),
                static_cast<ULONG>(object.size()),
                const_cast<PUCHAR>(reinterpret_cast<const UCHAR*>(secret.data())),
                static_cast<ULONG>(secret.size()),
                0u);
        }
        else if (status >= 0)
        {
            status = static_cast<NTSTATUS>(-1);
        }

        if (status >= 0)
        {
            status = BCryptHashData(
                hash,
                const_cast<PUCHAR>(reinterpret_cast<const UCHAR*>(message.data())),
                static_cast<ULONG>(message.size()),
                0u);
        }
        if (status >= 0)
        {
            status = BCryptFinishHash(
                hash,
                outDigest.data(),
                static_cast<ULONG>(outDigest.size()),
                0u);
        }

        if (hash != nullptr)
            BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0u);
        return status >= 0;
    }

    bool_t ConstantTimeEquals(
        std::span<const u8_t> lhs,
        std::span<const u8_t> rhs)
    {
        if (lhs.size() != rhs.size())
            return false;

        u8_t difference = 0u;
        for (size_t i = 0u; i < lhs.size(); ++i)
            difference |= lhs[i] ^ rhs[i];
        return difference == 0u;
    }

    bool_t IsCanonicalUuid(std::string_view value)
    {
        if (value.size() != 36u)
            return false;
        for (size_t i = 0u; i < value.size(); ++i)
        {
            if (i == 8u || i == 13u || i == 18u || i == 23u)
            {
                if (value[i] != '-')
                    return false;
                continue;
            }
            if (!((value[i] >= '0' && value[i] <= '9') ||
                (value[i] >= 'a' && value[i] <= 'f')))
            {
                return false;
            }
        }
        return true;
    }

    bool_t VerifyMatchTicket(
        std::span<const u8_t> ticketBytes,
        std::string_view secret,
        std::string_view expectedGameSessionID,
        u64_t maximumTTLSeconds,
        UdpAuthenticatedIdentity& outIdentity)
    {
        outIdentity = {};
		if (ticketBytes.empty() || ticketBytes.size() > kUdpMaxTicketBytes ||
            secret.size() < 32u || expectedGameSessionID.empty())
        {
            return false;
        }

        const std::string ticket(
            reinterpret_cast<const char*>(ticketBytes.data()),
            ticketBytes.size());
        const size_t firstDot = ticket.find('.');
        const size_t secondDot = firstDot == std::string::npos
            ? std::string::npos
            : ticket.find('.', firstDot + 1u);
        if (firstDot != 2u || ticket.substr(0u, firstDot) != "v1" ||
            secondDot == std::string::npos ||
            ticket.find('.', secondDot + 1u) != std::string::npos)
        {
            return false;
        }

        const std::string_view unsignedTicket(ticket.data(), secondDot);
        const std::string_view encodedPayload(
            ticket.data() + firstDot + 1u,
            secondDot - firstDot - 1u);
        const std::string_view encodedSignature(
            ticket.data() + secondDot + 1u,
            ticket.size() - secondDot - 1u);

        std::vector<u8_t> signature;
        std::array<u8_t, 32u> expectedSignature{};
        if (!DecodeBase64Url(encodedSignature, signature) ||
            signature.size() != expectedSignature.size() ||
            !ComputeHmacSha256(secret, unsignedTicket, expectedSignature) ||
            !ConstantTimeEquals(signature, expectedSignature))
        {
            return false;
        }

        std::vector<u8_t> payload;
        if (!DecodeBase64Url(encodedPayload, payload) || payload.empty())
            return false;

        try
        {
            const nlohmann::json document = nlohmann::json::parse(
                payload.begin(),
                payload.end(),
                nullptr,
                false);
            if (document.is_discarded() || !document.is_object() ||
                !document.contains("m") || !document["m"].is_string() ||
                !document.contains("u") || !document["u"].is_string() ||
                !document.contains("g") || !document["g"].is_string() ||
				!document.contains("n") || !document["n"].is_number_integer() ||
                !document.contains("e") || !document["e"].is_number_integer())
            {
                return false;
            }

            const std::string matchID = document["m"].get<std::string>();
            const std::string userID = document["u"].get<std::string>();
            const std::string gameSessionID = document["g"].get<std::string>();
			const i64_t gameSessionGeneration = document["n"].get<i64_t>();
            const i64_t expiresAt = document["e"].get<i64_t>();
            const i64_t now = static_cast<i64_t>(
                std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());
            if (!IsCanonicalUuid(matchID) || !IsCanonicalUuid(userID) ||
                gameSessionID != expectedGameSessionID || expiresAt <= now ||
				gameSessionGeneration <= 0 ||
                expiresAt > now + static_cast<i64_t>(maximumTTLSeconds) + 1)
            {
                return false;
            }

            outIdentity.matchID = matchID;
            outIdentity.userID = userID;
            outIdentity.gameSessionID = gameSessionID;
			outIdentity.gameSessionGeneration =
				static_cast<u64_t>(gameSessionGeneration);
            outIdentity.expiresAtUnix = static_cast<u64_t>(expiresAt);
            outIdentity.bAuthenticated = true;
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    CUdpIocpCore::TicketValidator CreateMatchTicketValidator(
        const ServerRuntimeOptions& options)
    {
        return [
            secret = options.matchTicketSecret,
            gameSessionID = options.gameSessionID,
            maximumTTLSeconds = options.matchTicketTTLSeconds](
                std::span<const u8_t> ticket,
                UdpAuthenticatedIdentity& outIdentity)
        {
            return VerifyMatchTicket(
                ticket,
                secret,
                gameSessionID,
                maximumTTLSeconds,
                outIdentity);
        };
    }

    u64_t ParseSha256Prefix(const std::string& value)
    {
        u64_t prefix = 0u;
        for (size_t i = 0u; i < 16u; ++i)
        {
            const char ch = value[i];
            const u64_t nibble = (ch >= '0' && ch <= '9')
                ? static_cast<u64_t>(ch - '0')
                : static_cast<u64_t>(ch - 'a' + 10);
            prefix = (prefix << 4u) | nibble;
        }
        return prefix;
    }

    bool_t LoadChampionAIShadowPolicyOptions(
        int argc,
        char** argv,
        std::shared_ptr<const ChampionAIShadowPolicyArtifactV1>& outPolicy)
    {
        std::string path;
        std::string expectedSha256;
        bool_t bSawPath = false;
        bool_t bSawSha = false;
        const size_t pathPrefixLength = std::strlen(kAIShadowPolicyPathPrefix);
        const size_t shaPrefixLength = std::strlen(kAIShadowPolicyShaPrefix);

        for (int i = 1; i < argc; ++i)
        {
            if (std::strncmp(argv[i], kAIShadowPolicyPathPrefix, pathPrefixLength) == 0)
            {
                if (bSawPath)
                {
                    std::cerr << "[ERROR] Duplicate --ai-shadow-policy option\n";
                    return false;
                }
                bSawPath = true;
                path.assign(argv[i] + pathPrefixLength);
            }
            else if (std::strncmp(argv[i], kAIShadowPolicyShaPrefix, shaPrefixLength) == 0)
            {
                if (bSawSha)
                {
                    std::cerr << "[ERROR] Duplicate --ai-shadow-policy-sha256 option\n";
                    return false;
                }
                bSawSha = true;
                expectedSha256.assign(argv[i] + shaPrefixLength);
            }
        }

        if (!bSawPath && !bSawSha)
        {
            outPolicy.reset();
            return true;
        }
        if (!bSawPath || !bSawSha || path.empty() ||
            !IsLowercaseSha256(expectedSha256))
        {
            std::cerr << "[ERROR] AI shadow policy requires a path and lowercase 64-hex SHA-256\n";
            return false;
        }

        std::vector<u8_t> bytes;
        if (!ReadBinaryFile(path, bytes))
        {
            std::cerr << "[ERROR] Failed to read AI shadow policy: " << path << "\n";
            return false;
        }

        std::string actualSha256;
        if (!ComputeSha256(bytes, actualSha256) || actualSha256 != expectedSha256)
        {
            std::cerr << "[ERROR] AI shadow policy SHA-256 mismatch\n";
            return false;
        }

        ChampionAIShadowPolicyArtifactV1 artifact{};
        if (!DecodeChampionAIShadowPolicyArtifactV1(
            bytes.data(),
            bytes.size(),
            artifact))
        {
            std::cerr << "[ERROR] AI shadow policy binary contract rejected\n";
            return false;
        }

        artifact.binarySha256Prefix = ParseSha256Prefix(actualSha256);
        outPolicy = std::make_shared<const ChampionAIShadowPolicyArtifactV1>(
            artifact);
        std::cout << "[Server] AI shadow policy loaded revision="
            << artifact.policyRevision << " sha256="
            << actualSha256.substr(0u, 16u) << "\n";
        return true;
    }
}

int main(int argc, char** argv)
{
    std::cout.setf(std::ios::unitbuf);
    std::cerr.setf(std::ios::unitbuf);

    ServerRuntimeOptions runtimeOptions{};
    if (!ParseRuntimeOptions(argc, argv, runtimeOptions))
        return 5;

    ScopedWin32Handle authenticatedUdpInstanceGuard;
    if (!AcquireAuthenticatedUdpInstanceGuard(
        runtimeOptions,
        authenticatedUdpInstanceGuard))
    {
        return 9;
    }

    const u32_t smokeSeconds = ParseSmokeSeconds(argc, argv);
    std::shared_ptr<const ChampionAIShadowPolicyArtifactV1> shadowPolicy;
    if (!LoadChampionAIShadowPolicyOptions(argc, argv, shadowPolicy))
        return 4;

    if (!CServerEntry::Initialize(
        runtimeOptions.jobWorkerCount,
        runtimeOptions.jobMode))
    {
        return 6;
    }
    CJobSystem* const pJobSystem = CServerEntry::Get_JobSystem();
    if (!pJobSystem ||
        !RunJobSystemStartupProbe(*pJobSystem, runtimeOptions.jobMode))
    {
        CServerEntry::Shutdown();
        return 6;
    }

    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        std::cerr << "[ERROR] WSAStartup failed\n";
        CServerEntry::Shutdown();
        return 1;
    }

    auto room = CGameRoom::Create(1, std::move(shadowPolicy));
    if (!room)
    {
        std::cerr << "[ERROR] GameRoom create failed\n";
        CServerEntry::Shutdown();
        WSACleanup();
        return 2;
    }

    std::unique_ptr<CIOCPCore> tcpCore;
    std::unique_ptr<CUdpIocpCore> udpCore;
    if (UsesTcp(runtimeOptions.networkMode))
        tcpCore = CIOCPCore::Create(9000u, 4u);
    if (UsesUdp(runtimeOptions.networkMode))
    {
        udpCore = std::make_unique<CUdpIocpCore>();
#if defined(_DEBUG)
        if (runtimeOptions.bUdpDevAllowEmptyTicket)
        {
            udpCore->SetTicketValidator(
            [](std::span<const u8_t> ticket, UdpAuthenticatedIdentity& identity)
            {
                if (!ticket.empty())
                    return false;
                identity.matchID = "local-debug";
                identity.userID = "local-debug";
                identity.gameSessionID = "local-game-1";
				identity.gameSessionGeneration = 1u;
                identity.bAuthenticated = true;
                return true;
            });
            std::cout << "[Server] UDP empty-ticket development authentication enabled\n";
        }
        else
#endif
        {
            udpCore->SetTicketValidator(
                CreateMatchTicketValidator(runtimeOptions));
            std::cout << "[Server] UDP signed match-ticket authentication enabled"
                << " gameSession=" << runtimeOptions.gameSessionID << std::endl;
        }
    }

    g_pRoom = room.get();
    CServerSessionHub& sessionHub = CServerSessionHub::Instance();
    if (!sessionHub.Attach(*room, udpCore.get()))
    {
        std::cerr << "[ERROR] ServerSessionHub attach failed\n";
        g_pRoom = nullptr;
        CServerEntry::Shutdown();
        udpCore.reset();
        tcpCore.reset();
        room.reset();
        WSACleanup();
        return 7;
    }

    CReplayUploadQueue& replayUploadQueue = CReplayUploadQueue::Instance();
    if (!replayUploadQueue.StartFromEnvironment())
    {
        std::cerr << "[ERROR] Replay upload environment is invalid\n";
        sessionHub.BeginShutdown();
        sessionHub.Detach();
        g_pRoom = nullptr;
        CServerEntry::Shutdown();
        udpCore.reset();
        tcpCore.reset();
        room.reset();
        WSACleanup();
        return 8;
    }
    if (replayUploadQueue.IsEnabled())
        std::cout << "[Server] Replay upload queue enabled" << std::endl;
    else
        std::cout << "[Server] Replay upload queue disabled; local artifacts only" << std::endl;

    room->SetCompletedGameSessionGenerationFloor(
        replayUploadQueue.GetGameSessionGenerationFloor());

    room->Start();

    bool_t bTransportStarted = true;
    if (tcpCore && !tcpCore->Start())
    {
        std::cerr << "[ERROR] TCP IOCPCore start failed\n";
        bTransportStarted = false;
    }
    if (bTransportStarted && udpCore &&
        !udpCore->Start(9000u, 4u, 8u))
    {
        std::cerr << "[ERROR] UDP IOCPCore start failed\n";
        bTransportStarted = false;
    }

    bool_t bRuntimeStopped = false;
    auto shutdownRuntime = [&]()
    {
        if (bRuntimeStopped)
            return true;

        sessionHub.BeginShutdown();
        // TCP dispatches directly from IOCP workers, so join every transport
        // ingress producer before stopping/finalizing the authoritative room.
        if (tcpCore)
            tcpCore->Shutdown();
        if (udpCore)
            udpCore->Shutdown();
        room->Stop();
        replayUploadQueue.Shutdown(false);
        const bool_t bDetached = sessionHub.Detach();
        if (!bDetached)
            std::cerr << "[ERROR] ServerSessionHub detach failed\n";
        g_pRoom = nullptr;
        CServerEntry::Shutdown();
        udpCore.reset();
        tcpCore.reset();
        room.reset();
        WSACleanup();
        bRuntimeStopped = true;
        return bDetached;
    };

    if (!bTransportStarted)
    {
        shutdownRuntime();
        return 3;
    }

    std::cout << "[Server] WintersServer v0.3 transport="
        << NetworkModeName(runtimeOptions.networkMode)
        << " jobMode=" << JobModeName(runtimeOptions.jobMode)
        << " jobWorkers=" << runtimeOptions.jobWorkerCount
        << " endpoint=0.0.0.0:9000" << std::endl;
    if (udpCore)
    {
        std::cout << "[Server] UDP is a development vertical slice: "
            "post-handshake MAC/AEAD and congestion pacing are not complete\n";
    }
    if (smokeSeconds > 0)
    {
        std::cout << "[Server] Smoke mode: running for " << smokeSeconds << " seconds.\n";
        std::this_thread::sleep_for(std::chrono::seconds(smokeSeconds));
    }
    else
    {
        std::cout << "[Server] Press 'q' + Enter to quit.\n";

        std::string line;
        while (std::getline(std::cin, line))
        {
            if (line == "q" || line == "Q")
                break;

            if (line.rfind("hp ", 0) == 0)
            {
                std::istringstream iss(line.substr(3));
                u32_t netId = 0;
                f32_t value = 0.f;
                if (iss >> netId >> value)
                {
                    if (!room->DebugSetHealthByNetId(netId, value))
                        std::cout << "[Server] hp command failed netId=" << netId << "\n";
                }
                else
                {
                    std::cout << "[Server] usage: hp <netId> <value>\n";
                }
            }
        }
    }

    const ServerSessionHubMetrics hubMetrics = sessionHub.GetMetrics();
    std::cout << "[Server] Hub metrics tcp=" << hubMetrics.activeTcpSessions
        << " udp=" << hubMetrics.activeUdpSessions
        << " ingress=" << hubMetrics.queuedIngressEvents
        << " staleDrops=" << hubMetrics.droppedStaleFrames
        << " overflowDisconnects=" << hubMetrics.ingressOverflowDisconnects
        << " outboundRejects=" << hubMetrics.rejectedOutboundFrames << '\n';
    if (udpCore)
    {
        const UdpServerMetrics udpMetrics = udpCore->GetMetrics();
        std::cout << "[Server] UDP metrics recvDg=" << udpMetrics.recvDatagrams
            << " sendDg=" << udpMetrics.sendDatagrams
            << " invalid=" << udpMetrics.invalidDatagrams
            << " retransmits=" << udpMetrics.retransmits
            << " peers=" << udpMetrics.connectedPeers << '\n';
    }

    return shutdownRuntime() ? 0 : 8;
}
