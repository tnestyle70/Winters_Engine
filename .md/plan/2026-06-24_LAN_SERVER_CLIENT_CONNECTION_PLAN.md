Session - 2026-06-24 LAN server/client connection plan. 현재 서버는 `C:/Users/tnest/Desktop/Winters/Server/Private/Network/IOCPCore.cpp`에서 `INADDR_ANY`로 `0.0.0.0:9000`에 바인딩한다. 접속 실패의 1차 원인은 서버 바인딩보다 클라이언트 진입점들이 `127.0.0.1:9000`을 고정 호출하고, 실행 인자로 데스크탑 서버 IP를 전달할 경로가 없는 구조다. 목표는 서버 권위 흐름을 유지하면서 같은 Wi-Fi의 노트북 클라이언트가 `--server-host=<desktop-ip> --server-port=9000`로 데스크탑 서버에 접속하도록 만드는 것이다.

1. 반영해야 하는 코드

현재 판정:
- `C:/Users/tnest/Desktop/Winters/Shared/Network/PacketDef.h`의 레거시 상수는 `WINTERS_SERVER_PORT = 9000`이다.
- `C:/Users/tnest/Desktop/Winters/Server/Private/Network/IOCPCore.cpp`는 `addr.sin_addr.s_addr = INADDR_ANY;`로 이미 외부 인터페이스를 수신한다.
- `C:/Users/tnest/Desktop/Winters/Client/Public/Network/Client/GameSessionClient.h`는 `Connect(const char* host = "127.0.0.1", u16_t port = 9000)` 기본값을 둔다.
- `C:/Users/tnest/Desktop/Winters/Client/Private/Scene/Scene_CustomMode.cpp`, `C:/Users/tnest/Desktop/Winters/Client/Private/Scene/Scene_BanPick.cpp`, `C:/Users/tnest/Desktop/Winters/Client/Private/Scene/Scene_InGameNetwork.cpp`는 직접 `127.0.0.1:9000`을 호출한다.
- `C:/Users/tnest/Desktop/Winters/Client/Private/Network/Client/ClientNetwork.cpp`는 `inet_pton(AF_INET, ...)`만 사용하므로 숫자 IPv4는 가능하지만 데스크탑 이름 같은 호스트명은 실패한다.

파일: `C:/Users/tnest/Desktop/Winters/Client/Public/Network/Client/GameSessionClient.h`

기존 코드:
```cpp
class CGameSessionClient final
{
public:
	using FrameCallback = std::function<void(ePacketType, u32_t, const u8_t*, u32_t)>;

	static CGameSessionClient& Instance();

	bool Connect(const char* host = "127.0.0.1", u16_t port = 9000);
```

아래로 교체:
```cpp
class CGameSessionClient final
{
public:
	using FrameCallback = std::function<void(ePacketType, u32_t, const u8_t*, u32_t)>;

	struct ServerEndpoint
	{
		std::string host = "127.0.0.1";
		u16_t port = 9000;
		bool_t bFromCommandLine = false;
	};

	static CGameSessionClient& Instance();
	static ServerEndpoint ResolveServerEndpoint();

	bool Connect(const char* host = nullptr, u16_t port = 0);
```

파일: `C:/Users/tnest/Desktop/Winters/Client/Private/Network/Client/GameSessionClient.cpp`

기존 코드:
```cpp
#include <Windows.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>
```

아래로 교체:
```cpp
#include <Windows.h>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <cwctype>
#include <cstring>
#include <string>
#include <utility>
#include <vector>
```

기존 코드:
```cpp
namespace
{
	const char* GetLobbyCommandKindName(Shared::Schema::LobbyCommandKind kind)
```

아래에 추가:
```cpp
	constexpr const char* kDefaultServerHost = "127.0.0.1";
	constexpr u16_t kDefaultServerPort = 9000;

	const wchar_t* GetCommandLineText()
	{
		const wchar_t* pCommandLine = ::GetCommandLineW();
		return pCommandLine ? pCommandLine : L"";
	}

	const wchar_t* FindCommandLineValue(const wchar_t* pPrefix)
	{
		if (!pPrefix)
			return nullptr;

		const wchar_t* pFound = std::wcsstr(GetCommandLineText(), pPrefix);
		return pFound ? pFound + std::wcslen(pPrefix) : nullptr;
	}

	std::string ReadCommandLineTextValue(const wchar_t* pPrefix)
	{
		const wchar_t* pValue = FindCommandLineValue(pPrefix);
		if (!pValue || *pValue == L'\0')
			return {};

		if (*pValue == L'"')
			++pValue;

		wchar_t wide[256]{};
		size_t i = 0;
		while (pValue[i] != L'\0' && i + 1 < (sizeof(wide) / sizeof(wide[0])))
		{
			const wchar_t ch = pValue[i];
			if (ch == L'"' || std::iswspace(ch))
				break;

			wide[i] = ch;
			++i;
		}

		if (i == 0)
			return {};

		char text[256]{};
		const int converted = ::WideCharToMultiByte(
			CP_UTF8,
			0,
			wide,
			-1,
			text,
			static_cast<int>(sizeof(text)),
			nullptr,
			nullptr);
		return converted > 0 ? std::string(text) : std::string{};
	}

	std::string FindServerHostOverride()
	{
		std::string host = ReadCommandLineTextValue(L"--server-host=");
		if (!host.empty())
			return host;

		host = ReadCommandLineTextValue(L"/server-host:");
		if (!host.empty())
			return host;

		host = ReadCommandLineTextValue(L"--server-ip=");
		if (!host.empty())
			return host;

		host = ReadCommandLineTextValue(L"/server-ip:");
		if (!host.empty())
			return host;

		return ReadCommandLineTextValue(L"/server:");
	}

	u16_t ParseServerPortOverride(bool_t& outOverridden)
	{
		const wchar_t* pValue = FindCommandLineValue(L"--server-port=");
		if (!pValue)
			pValue = FindCommandLineValue(L"/server-port:");
		if (!pValue)
			return kDefaultServerPort;

		wchar_t* pEnd = nullptr;
		const unsigned long parsed = std::wcstoul(pValue, &pEnd, 10);
		if (pEnd == pValue || parsed == 0 || parsed > 65535)
			return kDefaultServerPort;

		outOverridden = true;
		return static_cast<u16_t>(parsed);
	}
```

기존 코드:
```cpp
CGameSessionClient& CGameSessionClient::Instance()
{
	static CGameSessionClient s_instance;
	return s_instance;
}

bool CGameSessionClient::Connect(const char* host, u16_t port)
{
	if (IsConnected())
		return true;
```

아래로 교체:
```cpp
CGameSessionClient& CGameSessionClient::Instance()
{
	static CGameSessionClient s_instance;
	return s_instance;
}

CGameSessionClient::ServerEndpoint CGameSessionClient::ResolveServerEndpoint()
{
	ServerEndpoint endpoint{};
	endpoint.host = kDefaultServerHost;
	endpoint.port = kDefaultServerPort;

	const std::string hostOverride = FindServerHostOverride();
	if (!hostOverride.empty())
	{
		endpoint.host = hostOverride;
		endpoint.bFromCommandLine = true;
	}

	bool_t bPortOverridden = false;
	endpoint.port = ParseServerPortOverride(bPortOverridden);
	endpoint.bFromCommandLine = endpoint.bFromCommandLine || bPortOverridden;

	return endpoint;
}

bool CGameSessionClient::Connect(const char* host, u16_t port)
{
	if (IsConnected())
		return true;

	const ServerEndpoint endpoint = ResolveServerEndpoint();
	const char* pConnectHost = (host && host[0] != '\0') ? host : endpoint.host.c_str();
	const u16_t connectPort = port != 0 ? port : endpoint.port;
```

기존 코드:
```cpp
	const bool_t bLocalEndpoint = IsLocalEndpoint(host, port);
	const int attempts = bLocalEndpoint ? 20 : 1;
	for (int attempt = 1; attempt <= attempts; ++attempt)
	{
		if (m_pNetwork->Connect(host, port))
		{
			Winters::DevSmoke::Log(
				"[GameSessionClient] connected to lobby server host=%s port=%u attempt=%d\n",
				host ? host : "-",
				static_cast<u32_t>(port),
				attempt);
			return true;
		}
```

아래로 교체:
```cpp
	const bool_t bLocalEndpoint = IsLocalEndpoint(pConnectHost, connectPort);
	const int attempts = bLocalEndpoint ? 20 : 1;
	for (int attempt = 1; attempt <= attempts; ++attempt)
	{
		if (m_pNetwork->Connect(pConnectHost, connectPort))
		{
			Winters::DevSmoke::Log(
				"[GameSessionClient] connected to lobby server host=%s port=%u attempt=%d source=%s\n",
				pConnectHost ? pConnectHost : "-",
				static_cast<u32_t>(connectPort),
				attempt,
				endpoint.bFromCommandLine ? "command-line" : "default");
			return true;
		}
```

기존 코드:
```cpp
		Winters::DevSmoke::Log(
			"[GameSessionClient] connect failed host=%s port=%u attempts=%d\n",
			host ? host : "-",
			static_cast<u32_t>(port),
			attempts);
```

아래로 교체:
```cpp
		Winters::DevSmoke::Log(
			"[GameSessionClient] connect failed host=%s port=%u attempts=%d source=%s\n",
			pConnectHost ? pConnectHost : "-",
			static_cast<u32_t>(connectPort),
			attempts,
			endpoint.bFromCommandLine ? "command-line" : "default");
```

파일: `C:/Users/tnest/Desktop/Winters/Client/Private/Scene/Scene_CustomMode.cpp`

기존 코드:
```cpp
	m_bServerLobbyActive = CGameSessionClient::Instance().Connect("127.0.0.1", 9000);
```

아래로 교체:
```cpp
	m_bServerLobbyActive = CGameSessionClient::Instance().Connect();
```

파일: `C:/Users/tnest/Desktop/Winters/Client/Private/Scene/Scene_BanPick.cpp`

기존 코드:
```cpp
	m_bServerLobbyActive = CGameSessionClient::Instance().Connect("127.0.0.1", 9000);
```

아래로 교체:
```cpp
	m_bServerLobbyActive = CGameSessionClient::Instance().Connect();
```

파일: `C:/Users/tnest/Desktop/Winters/Client/Private/Scene/Scene_InGameNetwork.cpp`

기존 코드:
```cpp
    else if (m_pNetworkView->Connect("127.0.0.1", 9000))
    {
        Winters::DevSmoke::Log("[Scene_InGame] Connected to local Winters server.\n");
    }
    else
    {
        Winters::DevSmoke::Log("[Scene_InGame] Server not reachable; running local-only mode.\n");
    }
```

아래로 교체:
```cpp
    else
    {
        const CGameSessionClient::ServerEndpoint endpoint =
            CGameSessionClient::ResolveServerEndpoint();
        if (m_pNetworkView->Connect(endpoint.host.c_str(), endpoint.port))
        {
            Winters::DevSmoke::Log(
                "[Scene_InGame] Connected to Winters server host=%s port=%u source=%s.\n",
                endpoint.host.c_str(),
                static_cast<u32_t>(endpoint.port),
                endpoint.bFromCommandLine ? "command-line" : "default");
        }
        else
        {
            Winters::DevSmoke::Log(
                "[Scene_InGame] Server not reachable host=%s port=%u; running local-only mode.\n",
                endpoint.host.c_str(),
                static_cast<u32_t>(endpoint.port));
        }
    }
```

파일: `C:/Users/tnest/Desktop/Winters/Client/Private/Network/Client/ClientNetwork.cpp`

기존 코드:
```cpp
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
```

아래에 추가:
```cpp
    bool ResolveIPv4Endpoint(const char* pHost, u16_t port, sockaddr_in& outAddr)
    {
        outAddr = {};
        outAddr.sin_family = AF_INET;
        outAddr.sin_port = htons(port);

        if (!pHost || pHost[0] == '\0')
        {
            OutputSocketError("resolve(empty host)", pHost, port, 0);
            return false;
        }

        if (inet_pton(AF_INET, pHost, &outAddr.sin_addr) == 1)
            return true;

        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        char service[16]{};
        sprintf_s(service, "%u", static_cast<u32_t>(port));

        addrinfo* pResult = nullptr;
        const int gai = getaddrinfo(pHost, service, &hints, &pResult);
        if (gai != 0 || !pResult)
        {
            Winters::DevSmoke::Log(
                "[ClientNetwork] getaddrinfo failed host=%s port=%u gai=%d wsa=%d\n",
                pHost,
                static_cast<u32_t>(port),
                gai,
                WSAGetLastError());
            return false;
        }

        std::memcpy(&outAddr, pResult->ai_addr, sizeof(sockaddr_in));
        freeaddrinfo(pResult);
        return true;
    }
```

기존 코드:
```cpp
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
```

아래로 교체:
```cpp
    sockaddr_in addr{};
    const char* pConnectHost = host;
    if (host && std::strcmp(host, "localhost") == 0)
        pConnectHost = "127.0.0.1";

    if (!ResolveIPv4Endpoint(pConnectHost, port, addr))
    {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        return false;
    }
```

파일: `C:/Users/tnest/Desktop/Winters/Server/Private/Network/IOCPCore.cpp`

기존 코드:
```cpp
    std::cout << "[IOCPCore] listening on port " << m_port
        << " (workers=" << m_workerCount << ")\n";
```

아래로 교체:
```cpp
    std::cout << "[IOCPCore] listening on 0.0.0.0:" << m_port
        << " (workers=" << m_workerCount << ")\n";
```

파일: `C:/Users/tnest/Desktop/Winters/Server/Private/main.cpp`

기존 코드:
```cpp
    std::cout << "[Server] WintersServer v0.2 running on port 9000.\n";
```

아래로 교체:
```cpp
    std::cout << "[Server] WintersServer v0.2 running on 0.0.0.0:9000.\n";
```

프로젝트 파일 판정:
- `C:/Users/tnest/Desktop/Winters/Client/Include/Client.vcxproj`는 위 수정 대상 cpp/h를 이미 포함한다.
- `C:/Users/tnest/Desktop/Winters/Server/Include/Server.vcxproj`는 `main.cpp`, `IOCPCore.cpp`를 이미 포함한다.
- 새 C++ 파일을 만들지 않으므로 `.vcxproj` 및 `.filters` 수정은 필요 없다.

운영 규칙:
- 기본 실행은 계속 `127.0.0.1:9000`으로 유지한다.
- LAN 접속은 노트북 클라이언트를 `--server-host=<데스크탑 IPv4> --server-port=9000`로 실행한다.
- 서버 권위 흐름은 변경하지 않는다. 접속 대상 설정만 바꾸며, `Client Input -> GameCommand -> Server GameSim -> Snapshot/Event -> Client Visual` 구조를 유지한다.

2. 검증

빌드 검증:
```powershell
msbuild C:\Users\tnest\Desktop\Winters\Winters.sln /t:Server;Client /p:Configuration=Debug /p:Platform=x64
```

데스크탑 서버 실행:
```powershell
C:\Users\tnest\Desktop\Winters\Server\Bin\Debug\WintersServer.exe
```

데스크탑 서버 수신 상태 확인:
```powershell
Get-NetTCPConnection -LocalPort 9000 -State Listen
netstat -ano | findstr :9000
```

데스크탑 IPv4 확인:
```powershell
ipconfig
```

노트북에서 포트 도달성 확인:
```powershell
Test-NetConnection <데스크탑IPv4> -Port 9000
```

Windows 방화벽이 막는 경우 데스크탑에서 관리자 PowerShell로 1회 허용:
```powershell
New-NetFirewallRule -DisplayName "Winters Server TCP 9000" -Direction Inbound -Protocol TCP -LocalPort 9000 -Action Allow -Profile Private
```

노트북 클라이언트 실행:
```powershell
C:\Users\tnest\Desktop\Winters\Client\Bin\Debug\WintersGame.exe --server-host=<데스크탑IPv4> --server-port=9000
```

성공 로그 기준:
- 서버 콘솔에 `[IOCPCore] listening on 0.0.0.0:9000`이 출력된다.
- 서버 콘솔에 `[IOCP] Accept sid=...`가 출력된다.
- 클라이언트 로그에 `[ClientNetwork] connected host=<데스크탑IPv4> port=9000`이 출력된다.
- 클라이언트 로그에 `[GameSessionClient] connected to lobby server host=<데스크탑IPv4> port=9000 ... source=command-line`이 출력된다.
- BanPick/CustomMode를 통한 로비 접속과 InGame 직접 접속 모두 같은 인자 경로를 사용한다.

회귀 검증:
```powershell
C:\Users\tnest\Desktop\Winters\Client\Bin\Debug\WintersGame.exe
```
- 인자 없이 실행하면 기존처럼 `127.0.0.1:9000`을 사용한다.
- 로컬 서버가 없으면 기존처럼 local-only mode로 폴백한다.
- 서버 권위 스냅샷, 이벤트, 명령 송신 경로는 변경하지 않았으므로 기존 서버 smoke와 클라이언트 Debug x64 빌드를 같이 통과해야 한다.
