#include "Backend/ReplayUploadQueue.h"

#include "Server/Private/Data/ThirdParty/json.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <bcrypt.h>
#include <winhttp.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <mutex>
#include <optional>
#include <sstream>
#include <thread>
#include <vector>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "winhttp.lib")

namespace
{
    constexpr size_t kMaximumQueuedArtifacts = 64u;
    constexpr DWORD kMaximumControlResponseBytes = 2u * 1024u * 1024u;
    constexpr size_t kUploadBufferBytes = 1024u * 1024u;

    struct InternetHandle
    {
        HINTERNET value = nullptr;

        InternetHandle() = default;
        explicit InternetHandle(HINTERNET handle) : value(handle) {}
        ~InternetHandle()
        {
            if (value)
                WinHttpCloseHandle(value);
        }

        InternetHandle(const InternetHandle&) = delete;
        InternetHandle& operator=(const InternetHandle&) = delete;
        explicit operator bool() const { return value != nullptr; }
    };

    struct ParsedUrl
    {
        std::wstring host;
        std::wstring pathAndQuery;
        INTERNET_PORT port = 0;
        bool_t bSecure = false;
    };

    struct HttpResponse
    {
        DWORD statusCode = 0u;
        std::string body;
        std::string etag;
    };

    struct UploadSession
    {
        std::string replayID;
        std::string status;
        u64_t partSize = 0u;
    };

    bool_t ReadEnvironment(const char* name, std::string& outValue)
    {
        char* value = nullptr;
        size_t length = 0u;
        const errno_t result = _dupenv_s(&value, &length, name);
        if (result != 0 || !value || length <= 1u)
        {
            std::free(value);
            outValue.clear();
            return false;
        }
        outValue.assign(value, length - 1u);
        std::free(value);
        return true;
    }

    std::wstring Utf8ToWide(const std::string& value)
    {
        if (value.empty())
            return {};
        const int length = MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
            static_cast<int>(value.size()), nullptr, 0);
        if (length <= 0)
            return {};
        std::wstring result(static_cast<size_t>(length), L'\0');
        if (MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
            static_cast<int>(value.size()), result.data(), length) != length)
        {
            return {};
        }
        return result;
    }

    std::string WideToUtf8(const std::wstring& value)
    {
        if (value.empty())
            return {};
        const int length = WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
            static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        if (length <= 0)
            return {};
        std::string result(static_cast<size_t>(length), '\0');
        if (WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
            static_cast<int>(value.size()), result.data(), length,
            nullptr, nullptr) != length)
        {
            return {};
        }
        return result;
    }

    bool_t ParseUrl(const std::wstring& url, ParsedUrl& outUrl)
    {
        outUrl = {};
        if (url.empty())
            return false;
        URL_COMPONENTS components{};
        components.dwStructSize = sizeof(components);
        components.dwHostNameLength = static_cast<DWORD>(-1);
        components.dwUrlPathLength = static_cast<DWORD>(-1);
        components.dwExtraInfoLength = static_cast<DWORD>(-1);
        if (!WinHttpCrackUrl(url.c_str(), 0u, 0u, &components) ||
            !components.lpszHostName || components.dwHostNameLength == 0u)
        {
            return false;
        }

        outUrl.host.assign(
            components.lpszHostName,
            components.dwHostNameLength);
        if (components.lpszUrlPath && components.dwUrlPathLength != 0u)
        {
            outUrl.pathAndQuery.assign(
                components.lpszUrlPath,
                components.dwUrlPathLength);
        }
        else
        {
            outUrl.pathAndQuery = L"/";
        }
        if (components.lpszExtraInfo && components.dwExtraInfoLength != 0u)
        {
            outUrl.pathAndQuery.append(
                components.lpszExtraInfo,
                components.dwExtraInfoLength);
        }
        outUrl.port = components.nPort;
        outUrl.bSecure = components.nScheme == INTERNET_SCHEME_HTTPS;
        return outUrl.port != 0u &&
            (components.nScheme == INTERNET_SCHEME_HTTP || outUrl.bSecure);
    }

    std::string QueryHeaderUtf8(HINTERNET request, DWORD query)
    {
        DWORD bytes = 0u;
        WinHttpQueryHeaders(
            request, query, WINHTTP_HEADER_NAME_BY_INDEX,
            nullptr, &bytes, WINHTTP_NO_HEADER_INDEX);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || bytes < sizeof(wchar_t))
            return {};
        std::wstring value(bytes / sizeof(wchar_t), L'\0');
        if (!WinHttpQueryHeaders(
            request, query, WINHTTP_HEADER_NAME_BY_INDEX,
            value.data(), &bytes, WINHTTP_NO_HEADER_INDEX))
        {
            return {};
        }
        value.resize(bytes / sizeof(wchar_t));
        while (!value.empty() && value.back() == L'\0')
            value.pop_back();
        return WideToUtf8(value);
    }

    bool_t ReadResponse(HINTERNET request, HttpResponse& outResponse)
    {
        outResponse = {};
        DWORD statusBytes = sizeof(outResponse.statusCode);
        if (!WinHttpQueryHeaders(
            request,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &outResponse.statusCode,
            &statusBytes,
            WINHTTP_NO_HEADER_INDEX))
        {
            return false;
        }
        outResponse.etag = QueryHeaderUtf8(request, WINHTTP_QUERY_ETAG);

        for (;;)
        {
            DWORD available = 0u;
            if (!WinHttpQueryDataAvailable(request, &available))
                return false;
            if (available == 0u)
                return true;
            if (available > kMaximumControlResponseBytes ||
                outResponse.body.size() >
                    kMaximumControlResponseBytes - available)
            {
                return false;
            }
            const size_t offset = outResponse.body.size();
            outResponse.body.resize(offset + available);
            DWORD read = 0u;
            if (!WinHttpReadData(
                request,
                outResponse.body.data() + offset,
                available,
                &read))
            {
                return false;
            }
            outResponse.body.resize(offset + read);
        }
    }

    bool_t SendJsonRequest(
        const std::string& url,
        const wchar_t* method,
        const std::string& internalToken,
        const std::string& body,
        HttpResponse& outResponse)
    {
        const std::wstring wideUrl = Utf8ToWide(url);
        ParsedUrl parsed{};
        if (wideUrl.empty() || !ParseUrl(wideUrl, parsed) ||
            body.size() > (std::numeric_limits<DWORD>::max)())
        {
            return false;
        }

        InternetHandle session(WinHttpOpen(
            L"Winters/GameServer/1.0",
            WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0u));
        if (!session)
            return false;
        WinHttpSetTimeouts(session.value, 5'000, 5'000, 15'000, 15'000);
        InternetHandle connection(WinHttpConnect(
            session.value, parsed.host.c_str(), parsed.port, 0u));
        if (!connection)
            return false;
        InternetHandle request(WinHttpOpenRequest(
            connection.value,
            method,
            parsed.pathAndQuery.c_str(),
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            parsed.bSecure ? WINHTTP_FLAG_SECURE : 0u));
        if (!request)
            return false;

        const std::wstring headers = L"Content-Type: application/json\r\nAuthorization: Bearer " +
            Utf8ToWide(internalToken) + L"\r\n";
        if (!WinHttpSendRequest(
            request.value,
            headers.c_str(),
            static_cast<DWORD>(headers.size()),
            body.empty() ? WINHTTP_NO_REQUEST_DATA :
                const_cast<char*>(body.data()),
            static_cast<DWORD>(body.size()),
            static_cast<DWORD>(body.size()),
            0u) ||
            !WinHttpReceiveResponse(request.value, nullptr))
        {
            return false;
        }
        return ReadResponse(request.value, outResponse);
    }

    bool_t UploadFilePart(
        const std::string& url,
        const std::filesystem::path& path,
        u64_t offset,
        u64_t length,
        std::string& outETag)
    {
        outETag.clear();
        if (length == 0u || length > (std::numeric_limits<DWORD>::max)())
            return false;
        const std::wstring wideUrl = Utf8ToWide(url);
        ParsedUrl parsed{};
        if (wideUrl.empty() || !ParseUrl(wideUrl, parsed))
            return false;

        std::ifstream file(path, std::ios::binary);
        if (!file)
            return false;
        file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
        if (!file.good())
            return false;

        InternetHandle session(WinHttpOpen(
            L"Winters/GameServer/1.0",
            WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0u));
        if (!session)
            return false;
        WinHttpSetTimeouts(session.value, 5'000, 5'000, 120'000, 120'000);
        InternetHandle connection(WinHttpConnect(
            session.value, parsed.host.c_str(), parsed.port, 0u));
        if (!connection)
            return false;
        InternetHandle request(WinHttpOpenRequest(
            connection.value,
            L"PUT",
            parsed.pathAndQuery.c_str(),
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            parsed.bSecure ? WINHTTP_FLAG_SECURE : 0u));
        if (!request || !WinHttpSendRequest(
            request.value,
            WINHTTP_NO_ADDITIONAL_HEADERS,
            0u,
            WINHTTP_NO_REQUEST_DATA,
            0u,
            static_cast<DWORD>(length),
            0u))
        {
            return false;
        }

        std::vector<char> buffer(kUploadBufferBytes);
        u64_t remaining = length;
        while (remaining != 0u)
        {
            const DWORD requested = static_cast<DWORD>((std::min)(
                remaining,
                static_cast<u64_t>(buffer.size())));
            file.read(buffer.data(), requested);
            if (file.gcount() != requested)
                return false;
            DWORD written = 0u;
            if (!WinHttpWriteData(
                request.value, buffer.data(), requested, &written) ||
                written != requested)
            {
                return false;
            }
            remaining -= requested;
        }
        if (!WinHttpReceiveResponse(request.value, nullptr))
            return false;

        HttpResponse response{};
        if (!ReadResponse(request.value, response) ||
            response.statusCode < 200u || response.statusCode >= 300u ||
            response.etag.empty())
        {
            return false;
        }
        outETag = std::move(response.etag);
        return true;
    }

    bool_t ComputeFileSha256(
        const std::filesystem::path& path,
        u64_t& outSize,
        std::string& outHex)
    {
        outSize = 0u;
        outHex.clear();
        std::ifstream file(path, std::ios::binary);
        if (!file)
            return false;

        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        DWORD objectLength = 0u;
        DWORD hashLength = 0u;
        DWORD written = 0u;
        NTSTATUS status = BCryptOpenAlgorithmProvider(
            &algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0u);
        if (status < 0)
            return false;
        status = BCryptGetProperty(
            algorithm, BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&objectLength), sizeof(objectLength),
            &written, 0u);
        if (status >= 0)
        {
            status = BCryptGetProperty(
                algorithm, BCRYPT_HASH_LENGTH,
                reinterpret_cast<PUCHAR>(&hashLength), sizeof(hashLength),
                &written, 0u);
        }
        std::vector<u8_t> object(objectLength);
        std::array<u8_t, 32u> digest{};
        if (status >= 0 && hashLength == digest.size())
        {
            status = BCryptCreateHash(
                algorithm, &hash, object.data(),
                static_cast<ULONG>(object.size()), nullptr, 0u, 0u);
        }
        else if (status >= 0)
        {
            status = static_cast<NTSTATUS>(-1);
        }

        std::vector<u8_t> buffer(kUploadBufferBytes);
        while (status >= 0 && file)
        {
            file.read(
                reinterpret_cast<char*>(buffer.data()),
                static_cast<std::streamsize>(buffer.size()));
            const std::streamsize read = file.gcount();
            if (read > 0)
            {
                status = BCryptHashData(
                    hash, buffer.data(), static_cast<ULONG>(read), 0u);
                outSize += static_cast<u64_t>(read);
            }
        }
        if (status >= 0 && !file.eof())
            status = static_cast<NTSTATUS>(-1);
        if (status >= 0)
        {
            status = BCryptFinishHash(
                hash, digest.data(), static_cast<ULONG>(digest.size()), 0u);
        }
        if (hash)
            BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0u);
        if (status < 0)
            return false;

        static constexpr char kHex[] = "0123456789abcdef";
        outHex.reserve(64u);
        for (const u8_t byte : digest)
        {
            outHex.push_back(kHex[byte >> 4u]);
            outHex.push_back(kHex[byte & 0x0Fu]);
        }
        return true;
    }

    std::filesystem::path SidecarPath(const ReplayUploadArtifact& artifact)
    {
        return std::filesystem::path(artifact.path + L".upload.json");
    }

    nlohmann::json ArtifactToJson(const ReplayUploadArtifact& artifact)
    {
        nlohmann::json participants = nlohmann::json::array();
        for (const ReplayUploadParticipant& participant : artifact.participants)
        {
            participants.push_back({
                {"user_id", participant.userID},
                {"result", participant.result},
                {"kills", participant.kills},
                {"deaths", participant.deaths},
                {"assists", participant.assists},
            });
        }
        return {
            {"path", WideToUtf8(artifact.path)},
            {"match_id", artifact.matchID},
            {"size_bytes", artifact.sizeBytes},
            {"format_version", artifact.formatVersion},
            {"tick_rate", artifact.tickRate},
            {"record_count", artifact.recordCount},
            {"snapshot_count", artifact.snapshotCount},
            {"event_count", artifact.eventCount},
            {"command_count", artifact.commandCount},
            {"first_tick", artifact.firstTick},
            {"last_tick", artifact.lastTick},
            {"participants", std::move(participants)},
        };
    }

    bool_t JsonToArtifact(
        const nlohmann::json& document,
        ReplayUploadArtifact& outArtifact)
    {
        outArtifact = {};
        try
        {
            if (!document.is_object() || !document.contains("participants"))
                return false;
            outArtifact.path = Utf8ToWide(document.at("path").get<std::string>());
            outArtifact.matchID = document.at("match_id").get<std::string>();
            outArtifact.sizeBytes = document.at("size_bytes").get<u64_t>();
            outArtifact.formatVersion = document.at("format_version").get<u32_t>();
            outArtifact.tickRate = document.at("tick_rate").get<u32_t>();
            outArtifact.recordCount = document.at("record_count").get<u32_t>();
            outArtifact.snapshotCount = document.at("snapshot_count").get<u32_t>();
            outArtifact.eventCount = document.at("event_count").get<u32_t>();
            outArtifact.commandCount = document.at("command_count").get<u32_t>();
            outArtifact.firstTick = document.at("first_tick").get<u64_t>();
            outArtifact.lastTick = document.at("last_tick").get<u64_t>();
            for (const auto& item : document.at("participants"))
            {
                ReplayUploadParticipant participant{};
                participant.userID = item.at("user_id").get<std::string>();
                participant.result = item.at("result").get<std::string>();
                participant.kills = item.value("kills", 0);
                participant.deaths = item.value("deaths", 0);
                participant.assists = item.value("assists", 0);
                outArtifact.participants.push_back(std::move(participant));
            }
        }
        catch (...)
        {
            return false;
        }
        return !outArtifact.path.empty() &&
            !outArtifact.matchID.empty() &&
            !outArtifact.participants.empty();
    }

    bool_t PersistArtifact(const ReplayUploadArtifact& artifact)
    {
        const std::filesystem::path sidecar = SidecarPath(artifact);
        const std::filesystem::path temporary = sidecar.wstring() + L".tmp";
        try
        {
            if (sidecar.has_parent_path())
                std::filesystem::create_directories(sidecar.parent_path());
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output)
                return false;
            output << ArtifactToJson(artifact).dump();
            output.flush();
            if (!output.good())
                return false;
            output.close();
            if (!MoveFileExW(
                temporary.c_str(), sidecar.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            {
                std::error_code ignored;
                std::filesystem::remove(temporary, ignored);
                return false;
            }
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool_t ParseSuccessResponse(
        const HttpResponse& response,
        nlohmann::json& outData)
    {
        if (response.statusCode < 200u || response.statusCode >= 300u)
            return false;
        const nlohmann::json document = nlohmann::json::parse(
            response.body, nullptr, false);
        if (document.is_discarded() || !document.is_object() ||
            !document.value("success", false) || !document.contains("data"))
        {
            return false;
        }
        outData = document["data"];
        return true;
    }
}

struct CReplayUploadQueue::Impl
{
    mutable std::mutex mutex;
    std::condition_variable cv;
    std::deque<ReplayUploadArtifact> queue;
    std::thread worker;
    std::string serviceUrl;
    std::string internalToken;
    bool_t bStarted = false;
    bool_t bEnabled = false;
    bool_t bStopRequested = false;
    bool_t bDrain = false;

    std::string Url(const std::string& path) const
    {
        if (serviceUrl.empty())
            return {};
        if (serviceUrl.back() == '/' && !path.empty() && path.front() == '/')
            return serviceUrl.substr(0u, serviceUrl.size() - 1u) + path;
        if (serviceUrl.back() != '/' && !path.empty() && path.front() != '/')
            return serviceUrl + "/" + path;
        return serviceUrl + path;
    }

    bool_t BeginUpload(
        const ReplayUploadArtifact& artifact,
        const std::string& checksum,
        UploadSession& outSession)
    {
        nlohmann::json request = {
            {"match_id", artifact.matchID},
            {"size_bytes", artifact.sizeBytes},
            {"checksum_sha256", checksum},
            {"format_version", artifact.formatVersion},
            {"tick_rate", artifact.tickRate},
            {"record_count", artifact.recordCount},
            {"snapshot_count", artifact.snapshotCount},
            {"event_count", artifact.eventCount},
            {"command_count", artifact.commandCount},
            {"first_tick", artifact.firstTick},
            {"last_tick", artifact.lastTick},
        };
        HttpResponse response{};
        if (!SendJsonRequest(
            Url("/internal/replays/uploads"), L"POST", internalToken,
            request.dump(), response))
        {
            return false;
        }
        nlohmann::json data;
        if (!ParseSuccessResponse(response, data))
            return false;
        try
        {
            const auto& replay = data.at("replay");
            outSession.replayID = replay.at("replay_id").get<std::string>();
            outSession.status = replay.at("status").get<std::string>();
            outSession.partSize = data.at("part_size").get<u64_t>();
        }
        catch (...)
        {
            return false;
        }
        return !outSession.replayID.empty() &&
            (outSession.status == "uploading" || outSession.status == "ready") &&
            outSession.partSize >= 5u * 1024u * 1024u;
    }

    bool_t PresignPart(
        const std::string& replayID,
        u32_t partNumber,
        std::string& outUrl)
    {
        const nlohmann::json request = {
            {"part_numbers", nlohmann::json::array({partNumber})},
        };
        HttpResponse response{};
        if (!SendJsonRequest(
            Url("/internal/replays/" + replayID + "/parts"),
            L"POST", internalToken, request.dump(), response))
        {
            return false;
        }
        nlohmann::json data;
        if (!ParseSuccessResponse(response, data))
            return false;
        try
        {
            const auto& parts = data.at("parts");
            if (!parts.is_array() || parts.size() != 1u ||
                parts[0].at("part_number").get<u32_t>() != partNumber)
            {
                return false;
            }
            outUrl = parts[0].at("url").get<std::string>();
        }
        catch (...)
        {
            return false;
        }
        return !outUrl.empty();
    }

    bool_t CompleteUpload(
        const ReplayUploadArtifact& artifact,
        const std::string& checksum,
        const UploadSession& session,
        const std::vector<std::pair<u32_t, std::string>>& completedParts)
    {
        nlohmann::json parts = nlohmann::json::array();
        for (const auto& [number, etag] : completedParts)
        {
            parts.push_back({{"part_number", number}, {"etag", etag}});
        }
        const nlohmann::json request = {
            {"parts", std::move(parts)},
            {"size_bytes", artifact.sizeBytes},
            {"checksum_sha256", checksum},
        };
        HttpResponse response{};
        if (!SendJsonRequest(
            Url("/internal/replays/" + session.replayID + "/complete"),
            L"POST", internalToken, request.dump(), response))
        {
            return false;
        }
        nlohmann::json data;
        return ParseSuccessResponse(response, data) &&
            data.value("status", "") == "ready";
    }

    bool_t CompleteMatch(const ReplayUploadArtifact& artifact)
    {
        nlohmann::json players = nlohmann::json::array();
        for (const ReplayUploadParticipant& participant : artifact.participants)
        {
            players.push_back({
                {"user_id", participant.userID},
                {"result", participant.result},
                {"kills", participant.kills},
                {"deaths", participant.deaths},
                {"assists", participant.assists},
            });
        }
        const nlohmann::json request = {{"players", std::move(players)}};
        HttpResponse response{};
        if (!SendJsonRequest(
            Url("/internal/matches/" + artifact.matchID + "/complete"),
            L"POST", internalToken, request.dump(), response))
        {
            return false;
        }
        nlohmann::json data;
        return ParseSuccessResponse(response, data) &&
            data.value("status", "") == "completed";
    }

    bool_t ProcessOnce(const ReplayUploadArtifact& artifact)
    {
        u64_t actualSize = 0u;
        std::string checksum;
        if (!ComputeFileSha256(artifact.path, actualSize, checksum) ||
            actualSize != artifact.sizeBytes)
        {
            return false;
        }

        UploadSession session{};
        if (!BeginUpload(artifact, checksum, session))
            return false;
        if (session.status != "ready")
        {
            const u64_t partCount =
                (artifact.sizeBytes + session.partSize - 1u) / session.partSize;
            if (partCount == 0u || partCount > 10'000u)
                return false;
            std::vector<std::pair<u32_t, std::string>> completedParts;
            completedParts.reserve(static_cast<size_t>(partCount));
            for (u64_t index = 0u; index < partCount; ++index)
            {
                const u32_t partNumber = static_cast<u32_t>(index + 1u);
                const u64_t offset = index * session.partSize;
                const u64_t length = (std::min)(
                    session.partSize,
                    artifact.sizeBytes - offset);
                std::string etag;
                bool_t bUploaded = false;
                for (u32_t attempt = 0u; attempt < 3u && !bUploaded; ++attempt)
                {
                    std::string uploadUrl;
                    bUploaded = PresignPart(
                        session.replayID, partNumber, uploadUrl) &&
                        UploadFilePart(
                            uploadUrl, artifact.path, offset, length, etag);
                    if (!bUploaded)
                        std::this_thread::sleep_for(std::chrono::seconds(1u << attempt));
                }
                if (!bUploaded)
                    return false;
                completedParts.emplace_back(partNumber, std::move(etag));
            }
            if (!CompleteUpload(artifact, checksum, session, completedParts))
                return false;
        }
        return CompleteMatch(artifact);
    }

    bool_t ProcessWithRetry(const ReplayUploadArtifact& artifact)
    {
        for (u32_t attempt = 0u; attempt < 3u; ++attempt)
        {
            if (ProcessOnce(artifact))
                return true;
            if (attempt + 1u < 3u)
                std::this_thread::sleep_for(std::chrono::seconds(1u << attempt));
        }
        return false;
    }

    void RemoveCompletedArtifact(const ReplayUploadArtifact& artifact)
    {
        std::error_code ignored;
        std::filesystem::remove(artifact.path, ignored);
        ignored.clear();
        std::filesystem::remove(SidecarPath(artifact), ignored);
    }

    void WorkerLoop()
    {
        for (;;)
        {
            ReplayUploadArtifact artifact{};
            {
                std::unique_lock lock(mutex);
                cv.wait(lock, [this]
                {
                    return bStopRequested || !queue.empty();
                });
                if (bStopRequested && (!bDrain || queue.empty()))
                    return;
                artifact = std::move(queue.front());
                queue.pop_front();
            }

            const bool_t bCompleted = ProcessWithRetry(artifact);
            if (bCompleted)
                RemoveCompletedArtifact(artifact);
            std::ostringstream message;
            message << "[ReplayUpload] match=" << artifact.matchID
                << " status=" << (bCompleted ? "ready" : "deferred") << "\n";
            const std::string text = message.str();
            OutputDebugStringA(text.c_str());
            std::cout << text;
        }
    }

    void LoadPendingArtifacts()
    {
        std::error_code error;
        const std::filesystem::path replayDirectory(L"Replay");
        if (!std::filesystem::exists(replayDirectory, error))
            return;
        for (const auto& entry : std::filesystem::directory_iterator(
            replayDirectory, error))
        {
            if (error || !entry.is_regular_file())
                continue;
            const std::wstring name = entry.path().filename().wstring();
            if (!name.ends_with(L".wrpl.pending.upload.json"))
                continue;
            try
            {
                std::ifstream input(entry.path(), std::ios::binary);
                const nlohmann::json document = nlohmann::json::parse(
                    input, nullptr, false);
                ReplayUploadArtifact artifact{};
                if (!document.is_discarded() &&
                    JsonToArtifact(document, artifact) &&
                    std::filesystem::exists(artifact.path) &&
                    queue.size() < kMaximumQueuedArtifacts)
                {
                    queue.push_back(std::move(artifact));
                }
            }
            catch (...)
            {
                // Leave an unreadable sidecar in place for operator inspection.
            }
        }
    }
};

CReplayUploadQueue& CReplayUploadQueue::Instance()
{
    static CReplayUploadQueue instance;
    return instance;
}

CReplayUploadQueue::CReplayUploadQueue()
    : m_impl(std::make_unique<Impl>())
{
}

CReplayUploadQueue::~CReplayUploadQueue()
{
    Shutdown(false);
}

bool_t CReplayUploadQueue::StartFromEnvironment()
{
    std::lock_guard lock(m_impl->mutex);
    if (m_impl->bStarted)
        return true;

    const bool_t bHasUrl = ReadEnvironment(
        "WINTERS_REPLAY_SERVICE_URL", m_impl->serviceUrl);
    const bool_t bHasToken = ReadEnvironment(
        "WINTERS_REPLAY_INTERNAL_TOKEN", m_impl->internalToken);
    if (!bHasUrl && !bHasToken)
    {
        m_impl->bStarted = true;
        m_impl->bEnabled = false;
        return true;
    }
    ParsedUrl parsed{};
    if (!bHasUrl || !bHasToken || m_impl->internalToken.size() < 32u ||
        !ParseUrl(Utf8ToWide(m_impl->serviceUrl), parsed))
    {
        return false;
    }

    while (m_impl->serviceUrl.size() > 1u && m_impl->serviceUrl.back() == '/')
        m_impl->serviceUrl.pop_back();
    m_impl->bStarted = true;
    m_impl->bEnabled = true;
    m_impl->LoadPendingArtifacts();
    m_impl->worker = std::thread([this]
    {
        m_impl->WorkerLoop();
    });
    m_impl->cv.notify_all();
    return true;
}

bool_t CReplayUploadQueue::Enqueue(ReplayUploadArtifact artifact)
{
    std::lock_guard lock(m_impl->mutex);
    if (!m_impl->bEnabled || m_impl->bStopRequested ||
        m_impl->queue.size() >= kMaximumQueuedArtifacts ||
        !PersistArtifact(artifact))
    {
        return false;
    }
    m_impl->queue.push_back(std::move(artifact));
    m_impl->cv.notify_one();
    return true;
}

void CReplayUploadQueue::Shutdown(bool_t bDrain)
{
    {
        std::lock_guard lock(m_impl->mutex);
        if (!m_impl->bStarted)
            return;
        m_impl->bStopRequested = true;
        m_impl->bDrain = bDrain;
        m_impl->cv.notify_all();
    }
    if (m_impl->worker.joinable())
        m_impl->worker.join();

    std::lock_guard lock(m_impl->mutex);
    m_impl->bEnabled = false;
    m_impl->bStarted = false;
    m_impl->bStopRequested = false;
    m_impl->bDrain = false;
    m_impl->serviceUrl.clear();
    m_impl->internalToken.clear();
    m_impl->queue.clear();
}

bool_t CReplayUploadQueue::IsEnabled() const
{
    std::lock_guard lock(m_impl->mutex);
    return m_impl->bEnabled;
}
