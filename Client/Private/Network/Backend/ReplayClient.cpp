#include "Network/Backend/ReplayClient.h"

#include "Replay/ReplayLibrary.h"

#pragma push_macro("new")
#undef new
#include "Network/Backend/json.hpp"
#pragma pop_macro("new")

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
#include <filesystem>
#include <fstream>
#include <limits>
#include <utility>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "winhttp.lib")

using namespace Client;
using json = nlohmann::json;

namespace
{
	constexpr size_t kDownloadBufferBytes = 1024u * 1024u;

	struct InternetHandle
	{
		HINTERNET value = nullptr;
		explicit InternetHandle(HINTERNET handle = nullptr) : value(handle) {}
		~InternetHandle()
		{
			if (value)
				WinHttpCloseHandle(value);
		}
		InternetHandle(const InternetHandle&) = delete;
		InternetHandle& operator=(const InternetHandle&) = delete;
		explicit operator bool() const { return value != nullptr; }
	};

	struct ParsedURL
	{
		wstring host;
		wstring pathAndQuery;
		INTERNET_PORT port = 0;
		bool_t secure = false;
	};

	wstring Utf8ToWide(const string& value)
	{
		if (value.empty())
			return {};
		const int length = MultiByteToWideChar(
			CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
			static_cast<int>(value.size()), nullptr, 0);
		if (length <= 0)
			return {};
		wstring result(static_cast<size_t>(length), L'\0');
		if (MultiByteToWideChar(
			CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
			static_cast<int>(value.size()), result.data(), length) != length)
		{
			return {};
		}
		return result;
	}

	bool_t ParseURL(const string& value, ParsedURL& outURL)
	{
		outURL = {};
		const wstring url = Utf8ToWide(value);
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

		outURL.host.assign(components.lpszHostName, components.dwHostNameLength);
		if (components.lpszUrlPath && components.dwUrlPathLength > 0u)
			outURL.pathAndQuery.assign(components.lpszUrlPath, components.dwUrlPathLength);
		else
			outURL.pathAndQuery = L"/";
		if (components.lpszExtraInfo && components.dwExtraInfoLength > 0u)
			outURL.pathAndQuery.append(components.lpszExtraInfo, components.dwExtraInfoLength);
		outURL.port = components.nPort;
		outURL.secure = components.nScheme == INTERNET_SCHEME_HTTPS;
		return outURL.port != 0u &&
			(components.nScheme == INTERNET_SCHEME_HTTP || outURL.secure);
	}

	bool_t IsSafeID(const string& value)
	{
		if (value.empty() || value.size() > 64u)
			return false;
		return std::all_of(value.begin(), value.end(), [](unsigned char ch)
		{
			return (ch >= '0' && ch <= '9') ||
				(ch >= 'a' && ch <= 'f') || ch == '-';
		});
	}

	string DigestToHex(const std::array<u8_t, 32u>& digest)
	{
		static constexpr char kHex[] = "0123456789abcdef";
		string result;
		result.reserve(64u);
		for (const u8_t byte : digest)
		{
			result.push_back(kHex[byte >> 4u]);
			result.push_back(kHex[byte & 0x0Fu]);
		}
		return result;
	}

	ReplayDownloadResult DownloadReplay(
		const CloudReplayItem& item,
		const string& accountUserID,
		const string& presignedURL)
	{
		ReplayDownloadResult result{};
		result.replayId = item.replayId;
		if (!CReplayLibrary::IsSafeAccountKey(accountUserID) ||
			!IsSafeID(item.replayId) || !IsSafeID(item.matchId) ||
			item.sizeBytes == 0u || item.checksumSha256.size() != 64u)
		{
			result.error = "invalid replay metadata";
			return result;
		}

		ParsedURL parsed{};
		if (!ParseURL(presignedURL, parsed))
		{
			result.error = "invalid download grant";
			return result;
		}

		const std::filesystem::path cacheDirectory(
			CReplayLibrary::GetAccountReplayCacheDirectory(accountUserID));
		if (cacheDirectory.empty())
		{
			result.error = "account replay directory is unavailable";
			return result;
		}
		const std::filesystem::path destination = cacheDirectory /
			Utf8ToWide(item.matchId + "_" + item.replayId + ".wrpl");
		const std::filesystem::path temporary = destination.wstring() + L".part";

		std::error_code fileError;
		std::filesystem::create_directories(cacheDirectory, fileError);
		if (fileError)
		{
			result.error = "failed to create account replay directory";
			return result;
		}

		InternetHandle session(WinHttpOpen(
			L"Winters/ReplayClient/1.0",
			WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
			WINHTTP_NO_PROXY_NAME,
			WINHTTP_NO_PROXY_BYPASS,
			0u));
		if (!session)
		{
			result.error = "download session failed";
			return result;
		}
		WinHttpSetTimeouts(session.value, 5'000, 5'000, 30'000, 30'000);
		InternetHandle connection(WinHttpConnect(
			session.value, parsed.host.c_str(), parsed.port, 0u));
		InternetHandle request(connection ? WinHttpOpenRequest(
			connection.value,
			L"GET",
			parsed.pathAndQuery.c_str(),
			nullptr,
			WINHTTP_NO_REFERER,
			WINHTTP_DEFAULT_ACCEPT_TYPES,
			parsed.secure ? WINHTTP_FLAG_SECURE : 0u) : nullptr);
		if (!connection || !request ||
			!WinHttpSendRequest(
				request.value, WINHTTP_NO_ADDITIONAL_HEADERS, 0u,
				WINHTTP_NO_REQUEST_DATA, 0u, 0u, 0u) ||
			!WinHttpReceiveResponse(request.value, nullptr))
		{
			result.error = "replay download request failed";
			return result;
		}

		DWORD statusCode = 0u;
		DWORD statusBytes = sizeof(statusCode);
		if (!WinHttpQueryHeaders(
			request.value,
			WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			WINHTTP_HEADER_NAME_BY_INDEX,
			&statusCode,
			&statusBytes,
			WINHTTP_NO_HEADER_INDEX) || statusCode != 200u)
		{
			result.error = "replay storage rejected the download";
			return result;
		}

		BCRYPT_ALG_HANDLE algorithm = nullptr;
		BCRYPT_HASH_HANDLE hash = nullptr;
		DWORD objectLength = 0u;
		DWORD hashLength = 0u;
		DWORD written = 0u;
		NTSTATUS cryptoStatus = BCryptOpenAlgorithmProvider(
			&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0u);
		if (cryptoStatus >= 0)
			cryptoStatus = BCryptGetProperty(
				algorithm, BCRYPT_OBJECT_LENGTH,
				reinterpret_cast<PUCHAR>(&objectLength), sizeof(objectLength),
				&written, 0u);
		if (cryptoStatus >= 0)
			cryptoStatus = BCryptGetProperty(
				algorithm, BCRYPT_HASH_LENGTH,
				reinterpret_cast<PUCHAR>(&hashLength), sizeof(hashLength),
				&written, 0u);
		std::vector<u8_t> hashObject(objectLength);
		if (cryptoStatus >= 0 && hashLength == 32u)
			cryptoStatus = BCryptCreateHash(
				algorithm, &hash, hashObject.data(),
				static_cast<ULONG>(hashObject.size()), nullptr, 0u, 0u);
		else if (cryptoStatus >= 0)
			cryptoStatus = static_cast<NTSTATUS>(-1);

		std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
		std::vector<u8_t> buffer(kDownloadBufferBytes);
		u64_t received = 0u;
		std::array<u8_t, 8u> prefix{};
		size_t prefixBytes = 0u;
		while (cryptoStatus >= 0 && output)
		{
			DWORD read = 0u;
			if (!WinHttpReadData(
				request.value, buffer.data(),
				static_cast<DWORD>(buffer.size()), &read))
			{
				cryptoStatus = static_cast<NTSTATUS>(-1);
				break;
			}
			if (read == 0u)
				break;
			if (received > item.sizeBytes || read > item.sizeBytes - received)
			{
				cryptoStatus = static_cast<NTSTATUS>(-1);
				break;
			}
			const size_t copyBytes = (std::min)(
				prefix.size() - prefixBytes, static_cast<size_t>(read));
			std::copy_n(buffer.data(), copyBytes, prefix.data() + prefixBytes);
			prefixBytes += copyBytes;
			output.write(
				reinterpret_cast<const char*>(buffer.data()),
				static_cast<std::streamsize>(read));
			if (!output)
			{
				cryptoStatus = static_cast<NTSTATUS>(-1);
				break;
			}
			cryptoStatus = BCryptHashData(hash, buffer.data(), read, 0u);
			received += read;
		}

		std::array<u8_t, 32u> digest{};
		if (cryptoStatus >= 0 && received == item.sizeBytes)
			cryptoStatus = BCryptFinishHash(
				hash, digest.data(), static_cast<ULONG>(digest.size()), 0u);
		else if (cryptoStatus >= 0)
			cryptoStatus = static_cast<NTSTATUS>(-1);
		if (hash)
			BCryptDestroyHash(hash);
		if (algorithm)
			BCryptCloseAlgorithmProvider(algorithm, 0u);
		output.flush();
		const bool_t outputGood = output.good();
		output.close();

		const bool_t validPrefix = prefixBytes == prefix.size() &&
			prefix[0] == 'W' && prefix[1] == 'R' &&
			prefix[2] == 'P' && prefix[3] == 'L';
		const u16_t formatVersion = prefixBytes == prefix.size()
			? static_cast<u16_t>(prefix[4] | (prefix[5] << 8u))
			: 0u;
		if (cryptoStatus < 0 || !outputGood || !validPrefix ||
			formatVersion != static_cast<u16_t>(item.formatVersion) ||
			DigestToHex(digest) != item.checksumSha256 ||
			!MoveFileExW(
				temporary.c_str(), destination.c_str(),
				MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		{
			std::filesystem::remove(temporary, fileError);
			result.error = "downloaded replay failed integrity validation";
			return result;
		}

		result.success = true;
		result.localPath = destination.wstring();
		return result;
	}

	string ResponseError(const HttpResponse& response, const char* fallback)
	{
		try
		{
			const json document = json::parse(response.body);
			return document.value("error", string(fallback));
		}
		catch (...)
		{
			return response.error.empty() ? string(fallback) : response.error;
		}
	}
}

CReplayClient::~CReplayClient()
{
	m_pHttp.reset();
	vector<future<void>> pending;
	{
		lock_guard<mutex> lock(m_RequestMutex);
		pending.swap(m_PendingRequests);
	}
	for (auto& request : pending)
	{
		if (request.valid())
			request.wait();
	}
}

unique_ptr<CReplayClient> CReplayClient::Create(const string& baseURL)
{
	auto instance = unique_ptr<CReplayClient>(new CReplayClient());
	instance->m_pHttp = CHttpClient::Create(baseURL);
	return instance;
}

void CReplayClient::SetAuthToken(const string& token)
{
	m_pHttp->SetAuthToken(token);
}

void CReplayClient::ListMine(ReplayPageCallback callback)
{
	m_pHttp->AsyncGet("/replay/me?limit=100", [callback](const HttpResponse& response)
	{
		ReplayPageResult result{};
		if (!response.success)
		{
			result.error = ResponseError(response, "replay list failed");
			callback(result);
			return;
		}
		try
		{
			const json data = json::parse(response.body).at("data");
			result.nextCursor = data.value("next_cursor", "");
			for (const auto& value : data.at("items"))
			{
				CloudReplayItem item{};
				item.replayId = value.value("replay_id", "");
				item.matchId = value.value("match_id", "");
				item.status = value.value("status", "");
				item.sizeBytes = value.value("size_bytes", 0ull);
				item.checksumSha256 = value.value("checksum_sha256", "");
				item.formatVersion = value.value("format_version", 0);
				item.tickRate = value.value("tick_rate", 0);
				item.recordCount = value.value("record_count", 0ull);
				item.createdAt = value.value("created_at", "");
				item.downloaded = value.value("downloaded", false);
				if (!item.replayId.empty() && item.status == "ready")
					result.items.push_back(std::move(item));
			}
		}
		catch (const json::exception& exception)
		{
			result.items.clear();
			result.error = exception.what();
		}
		callback(result);
	});
}

void CReplayClient::DownloadMine(
	const CloudReplayItem& item,
	const string& accountUserID,
	ReplayDownloadCallback callback)
{
	m_pHttp->AsyncPost(
		"/replay/" + item.replayId + "/download",
		"{}",
		[this, item, accountUserID, callback](const HttpResponse& response)
		{
			if (!response.success)
			{
				ReplayDownloadResult result{};
				result.replayId = item.replayId;
				result.error = ResponseError(response, "download grant failed");
				callback(result);
				return;
			}
			try
			{
				const string url = json::parse(response.body)
					.at("data").at("url").get<string>();
				LaunchDownload(item, accountUserID, url, callback);
			}
			catch (const json::exception& exception)
			{
				ReplayDownloadResult result{};
				result.replayId = item.replayId;
				result.error = exception.what();
				callback(result);
			}
		});
}

void CReplayClient::LaunchDownload(
	CloudReplayItem item,
	string accountUserID,
	string presignedURL,
	ReplayDownloadCallback callback)
{
	PruneCompletedDownloads();
	future<void> request = async(
		launch::async,
		[this, item = std::move(item), accountUserID = std::move(accountUserID),
		 presignedURL = std::move(presignedURL), callback]()
		{
			const ReplayDownloadResult result = DownloadReplay(
				item, accountUserID, presignedURL);
			lock_guard<mutex> lock(m_CallbackMutex);
			m_PendingCallbacks.push([callback, result]() { callback(result); });
		});
	lock_guard<mutex> lock(m_RequestMutex);
	m_PendingRequests.push_back(std::move(request));
}

void CReplayClient::PruneCompletedDownloads()
{
	lock_guard<mutex> lock(m_RequestMutex);
	for (size_t index = m_PendingRequests.size(); index > 0u; --index)
	{
		auto& request = m_PendingRequests[index - 1u];
		if (!request.valid() ||
			request.wait_for(chrono::seconds(0)) == future_status::ready)
		{
			m_PendingRequests.erase(m_PendingRequests.begin() + (index - 1u));
		}
	}
}

void CReplayClient::ProcessCallbacks()
{
	m_pHttp->ProcessCallbacks();
	queue<function<void()>> pending;
	{
		lock_guard<mutex> lock(m_CallbackMutex);
		swap(pending, m_PendingCallbacks);
	}
	while (!pending.empty())
	{
		pending.front()();
		pending.pop();
	}
}
