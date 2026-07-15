# UDP Gameplay Transport 이주 계획

> [!IMPORTANT]
> **Historical migration design.** 이 문서의 “UDP 호출자 없음 / fragment 없는 M1” 전제는 2026-07-11 당시 기록이다. 2026-07-13 현재 UDP v3 socket·lane reliability·bounded fragmentation/reassembly·cookie handshake·Server IOCP·Client facade가 opt-in vertical slice로 구현·검증됐다. 기본값은 TCP이며 production cutover는 아직 아니다. 현행 코드, 측정치, 남은 gate는 [canonical implementation plan](../../plan/2026-07-13_UDP_JOB_SYSTEM_CHASE_LEV_FIBER_IMPLEMENTATION_PLAN.md)과 [S023 결과 보고서](../../build/2026-07-13_UDP_JOB_SYSTEM_CHASE_LEV_FIBER_RESULT.md)를 따른다.

작성일: 2026-05-07  
대상: InGame CommandBatch / Snapshot / Event 전송 계층  
결론: **UDP는 gameplay 전용 transport로 추가하고, BanPick TCP를 대체하지 않는다.**

---

## 1. 목표

이 문서는 기존 TCP gameplay 송수신을 UDP gameplay 송수신으로 옮기는 구현 계획이다.

첫 목표는 작게 잡는다.

```text
M1:
  TCP BanPick GameStart
  -> UDP GameplayJoin
  -> UDP CommandBatch
  -> Server authoritative tick
  -> UDP full Snapshot
```

M1에서 하지 않을 것:

- reliable UDP
- fragment/reassembly
- delta snapshot
- AOI
- lag compensation
- client prediction
- encryption
- KCP

M1의 목적은 성능 완성이 아니라 **transport 경계 분리와 end-to-end 왕복 증명**이다.

---

## 2. 기존 UDP 계획과 현재 구조의 차이

기존 문서 `.md/plan/sim/10_v2_M1_UDP_TRANSPORT.md`는 TCP transport를 UDP transport로 통째로 교체하는 관점이었다.

하지만 현재는 BanPick TCP MVP가 완성되어 있으므로 이주 방향을 다음처럼 수정한다.

| 구분 | 기존 M1 관점 | 2026-05-07 업데이트 |
|---|---|---|
| 연결 시작 | UDP Hello가 session 생성 | TCP GameStart가 ticket 발급, UDP GameplayJoin이 gameplay bind |
| BanPick | UDP 전환 가능성 있음 | TCP 유지 |
| `CGameSessionClient` | UDP로 rewrite 후보 | TCP Control client로 유지 |
| InGame transport | 기존 TCP 재사용 | 별도 `CUdpGameplayClient` 생성 |
| GameRoom join | UDP sourceAddr join | TCP sessionId + gameplay token 검증 |
| 보안 | M1에서는 거의 없음 | 최소 token/TTL/sourceAddr bind는 M1부터 고려 |

---

## 3. UDP Plane에 올릴 데이터

### 3.1 M1에 올릴 것

```text
GameplayJoin
GameplayJoinAck
CommandBatch
Snapshot
Heartbeat or AckOnly
```

`CommandBatch`와 `Snapshot`은 기존 flatbuffers schema와 serializer/applier를 최대한 재사용한다.

### 3.2 M2 이후 올릴 것

```text
Reliable Event
Unreliable Event
Snapshot Delta
Fragment
Ack
BaselineAck
ClientInputAck
Prediction correction
```

### 3.3 UDP로 올리지 않을 것

```text
LobbyCommand
LobbyState
GameStart
Auth
Shop
Payment
Profile
Matchmaking
```

---

## 4. Shared 패킷 계약

### 4.1 M1에서 `PacketHeader` 재사용

현재 `PacketHeader`는 16바이트다.

```cpp
struct PacketHeader
{
    uint16_t magic;
    uint16_t version;
    uint16_t type;
    uint16_t flags;
    uint32_t sequence;
    uint32_t payloadSize;
};
```

TCP에서는 stream frame의 header로 사용한다. UDP M1에서는 datagram payload 앞에 붙는 envelope header로 재사용한다.

M1 규칙:

- UDP datagram 하나에는 `PacketHeader + payload` 하나만 담는다.
- datagram size는 일단 MTU 위험을 로그로 감시한다.
- `payloadSize`가 실제 datagram remaining size와 다르면 폐기한다.
- `magic`, `version`, `type` 검증 실패 시 폐기한다.
- `sequence`는 M1에서는 application-level duplicate guard 정도로만 사용한다.

### 4.2 M2에서 추가할 UDP header

M2에서 reliability/fragment가 들어오면 `PacketHeader`만으로 부족하다.

추가 후보:

```cpp
enum class eUdpChannel : uint8_t
{
    Unreliable = 0,
    ReliableOrdered = 1,
    ReliableUnordered = 2,
    Snapshot = 3,
};

struct UdpPacketHeader
{
    uint16_t magic;
    uint16_t version;
    uint8_t channel;
    uint8_t flags;
    uint16_t headerBytes;
    uint32_t sequence;
    uint32_t ack;
    uint32_t ackBits;
    uint16_t packetType;
    uint16_t payloadSize;
};
```

M1에서는 이 구조를 만들지 않아도 된다. 대신 M1 코드에서 `PacketHeader` 파싱부를 캡슐화해서 M2 교체가 쉽게 만든다.

---

## 5. Server 구현 계획

### 5.1 새 파일 후보

권장 위치:

```text
Server/Public/Network/UdpCore.h
Server/Public/Network/UdpSession.h
Server/Public/Network/UdpSession_Manager.h
Server/Public/Network/UdpPacketDispatcher.h

Server/Private/Network/UdpCore.cpp
Server/Private/Network/UdpSession.cpp
Server/Private/Network/UdpSession_Manager.cpp
Server/Private/Network/UdpPacketDispatcher.cpp
```

Winters 파일명 규칙상 새 파일명은 `CUdpCore.h`가 아니라 `UdpCore.h`가 맞다. 클래스명은 `CUdpCore`다.

### 5.2 `CUdpCore`

책임:

- UDP socket 생성
- `WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, ...)`
- bind
- `SIO_UDP_CONNRESET` disable
- receive loop
- `recvfrom` 또는 IOCP 기반 `WSARecvFrom`
- datagram을 `CUdpPacketDispatcher`로 전달
- session manager를 통해 `sendto`

M1 현실 선택:

- 처음부터 IOCP UDP로 가도 된다.
- 단, 디버깅 속도를 위해 dedicated UDP recv thread + non-blocking socket으로 시작해도 된다.
- 최종 구조는 IOCP와 맞추는 것이 좋다.

중요 Windows gotcha:

```text
SIO_UDP_CONNRESET을 꺼야 한다.
```

Windows UDP는 상대 ICMP Port Unreachable 이후 recv가 `WSAECONNRESET`으로 깨지는 경우가 있다. 게임 서버에서는 이 동작을 꺼야 한다.

### 5.3 `CUdpSession`

책임:

- UDP peer address 보관
- TCP control `sessionId`와 gameplay peer 연결
- local player `netId` 보관
- last receive time
- last accepted command sequence
- send sequence
- M2 이후 channel reliability state

M1 필드 후보:

```cpp
class CUdpSession final
{
public:
    u32_t GetSessionId() const;
    NetEntityId GetPlayerNetId() const;
    const sockaddr_storage& GetRemoteAddress() const;
    bool_t TryAcceptSequence(u32_t sequence);
    void TouchRecvTime(u64_t nowMs);

private:
    u32_t m_iSessionId = 0;
    NetEntityId m_iPlayerNetId = NULL_NET_ENTITY;
    sockaddr_storage m_RemoteAddress = {};
    i32_t m_iRemoteAddressLen = 0;
    u32_t m_iLastProcessedSequence = 0;
    u64_t m_iLastRecvTimeMs = 0;
};
```

### 5.4 `CUdpSession_Manager`

책임:

- `sessionId -> CUdpSession`
- `remoteAddr -> sessionId`
- gameplay token 검증 후 bind
- NAT rebind 정책
- sorted iteration 제공

M1 API 후보:

```cpp
CUdpSession* FindBySessionId(u32_t sessionId);
CUdpSession* FindByRemoteAddress(const sockaddr_storage& addr, i32_t addrLen);
bool_t BindGameplaySession(const GameplayJoinRequest& request, const sockaddr_storage& addr, i32_t addrLen);
bool_t SendToSession(u32_t sessionId, ePacketType type, const uint8_t* payload, u32_t size);
```

M1에서는 remoteAddr가 바뀌면 일단 거부하거나 로그만 찍는다. NAT rebind 허용은 token/sequence/rebind cooldown이 들어간 후로 미룬다.

### 5.5 `CUdpPacketDispatcher`

책임:

- datagram header 검증
- packet type dispatch
- GameplayJoin 처리
- CommandBatch 처리
- Heartbeat 처리

M1 dispatch:

```cpp
void DispatchDatagram(const sockaddr_storage& from, i32_t fromLen, const uint8_t* data, u32_t size)
{
    // 1. PacketHeader 검증
    // 2. type 분기
    // 3. GameplayJoin이면 token 검증 후 CUdpSession_Manager bind
    // 4. CommandBatch면 session lookup 후 CGameRoom::OnCommandBatch
    // 5. Heartbeat면 touch
}
```

중요:

기존 TCP에서는 `IOCPCore::WorkerLoop` accept path에서 `g_pRoom->OnSessionJoin(sessionId)`를 호출했다. UDP에는 accept가 없다.

업데이트된 구조에서는 UDP가 `OnSessionJoin`을 호출하면 안 된다. 이미 TCP BanPick에서 join이 끝났기 때문이다.

대신:

```text
GameplayJoin 성공
  -> CUdpSession_Manager bind
  -> CGameRoom::OnGameplayTransportReady(sessionId)
```

이 메서드는 "로비 입장"이 아니라 "gameplay transport 준비 완료" 의미여야 한다.

---

## 6. Client 구현 계획

### 6.1 새 파일 후보

```text
Client/Public/Network/Client/UdpGameplayClient.h
Client/Private/Network/Client/UdpGameplayClient.cpp
```

클래스:

```cpp
class CUdpGameplayClient final
{
public:
    bool_t Connect(const GameplayJoinTicket& ticket);
    void Disconnect();
    bool_t IsConnected() const;
    bool_t SendCommandBatch(const uint8_t* payload, u32_t size, u32_t sequence);
    void SetFrameCallback(FrameCallback callback);
    void Pump();
};
```

M1에서는 TCP `CClientNetwork`와 동일한 callback signature를 최대한 맞춘다. 그래야 `CSnapshotApplier`와 `CCommandSerializer` 재사용이 쉽다.

### 6.2 `CCommandSerializer` 재사용

현재 `CCommandSerializer::SendMove(*m_pNetworkView, ground)`는 TCP client view에 직접 의존한다.

이 부분을 다음처럼 분리한다.

```cpp
class IGameplayPacketSink
{
public:
    virtual ~IGameplayPacketSink() = default;
    virtual bool_t SendGameplayPacket(ePacketType type, const uint8_t* payload, u32_t size, u32_t sequence) = 0;
};
```

또는 더 작게:

```cpp
static bool_t BuildMoveCommandBatch(..., std::vector<uint8_t>& outPayload, u32_t& outSequence);
```

권장 M1:

- `CCommandSerializer`에 "build" 함수와 "send" 함수를 분리한다.
- TCP fallback은 기존 send 함수를 유지한다.
- UDP path는 build된 payload를 `CUdpGameplayClient`로 보낸다.

### 6.3 `CSnapshotApplier` 재사용

`CSnapshotApplier`는 transport 독립적으로 유지해야 한다.

규칙:

- TCP인지 UDP인지 알면 안 된다.
- `Hello`, `GameplayJoinAck`, `Snapshot` payload만 받는다.
- entity bind와 Transform/Health apply만 담당한다.

UDP M1에서 필요한 변경:

- `GameplayJoinAck` 또는 UDP용 `Hello`에서 local `netId`를 확정한다.
- 이후 full snapshot 적용 흐름은 기존과 동일하게 둔다.

### 6.4 `InGameNetworkBridge` 수정

현재:

```text
BanPick TCP session이 있으면 그 TCP session을 InGame에서도 gameplay network로 사용한다.
```

목표:

```text
BanPick TCP session은 control용으로 유지한다.
InGameNetworkBridge는 UDP ticket이 있으면 CUdpGameplayClient를 생성한다.
Snapshot은 UDP frame callback으로 CSnapshotApplier에 전달한다.
```

흐름:

```text
Scene_InGame::OnEnter
  -> context.bUseNetworkRoster 확인
  -> CGameSessionClient::HasGameplayTicket 확인
  -> CUdpGameplayClient::Connect(ticket)
  -> GameplayJoin 전송
  -> JoinAck 수신
  -> Snapshot 수신
```

개발 fallback:

```text
ticket이 없고 local dev flag면
  -> TCP gameplay fallback 또는 local-only mode
```

---

## 7. GameRoom transport 분리

### 7.1 현재 문제

현재 `CGameRoom::Phase_BroadcastSnapshot` 계열은 TCP `CSession_Manager`와 직접 연결되어 있다.

문제:

- UDP 전송으로 바꾸려면 GameRoom 안에 UDP manager까지 들어오게 된다.
- TCP/UDP fallback 분기가 GameRoom 곳곳에 퍼질 수 있다.
- M2 reliability가 들어오면 더 엉킨다.

### 7.2 M1 최소 수정안

GameRoom에 private helper를 만든다.

```cpp
bool_t CGameRoom::SendGameplayPacketToSession(u32_t sessionId, ePacketType type, const uint8_t* payload, u32_t size)
{
    if (m_pGameplayTransport != nullptr && m_pGameplayTransport->IsGameplayReady(sessionId))
    {
        return m_pGameplayTransport->SendToSession(sessionId, type, payload, size);
    }

#if WINTERS_ENABLE_TCP_GAMEPLAY_FALLBACK
    return SendTcpPacketToSession(sessionId, type, payload, size);
#else
    return false;
#endif
}
```

M1에서는 interface까지 못 만들면 함수 하나라도 먼저 둔다. 중요한 것은 snapshot send 지점에서 `CSession_Manager::Find` 직접 호출을 줄이는 것이다.

### 7.3 정식 분리안

```cpp
class IGameplayTransport
{
public:
    virtual ~IGameplayTransport() = default;
    virtual bool_t IsGameplayReady(u32_t sessionId) const = 0;
    virtual bool_t SendToSession(u32_t sessionId, ePacketType type, const uint8_t* payload, u32_t payloadSize) = 0;
};
```

구현체:

```text
CTcpGameplayTransport      : 개발 fallback
CUdpGameplayTransport      : 최종 gameplay
```

---

## 8. UDP M1 상세 흐름

### 8.1 Client

```text
1. BanPick TCP GameStart 수신
2. GameStart에서 GameplayJoinTicket 저장
3. Scene_InGame 진입
4. CUdpGameplayClient socket open
5. 서버 UDP endpoint로 GameplayJoin 전송
6. GameplayJoinAck 수신
7. local netId 확정
8. 매 input tick마다 CommandBatch 전송
9. Snapshot 수신 후 CSnapshotApplier 적용
```

### 8.2 Server

```text
1. TCP BanPick에서 GameStart broadcast
2. 각 human session에 gameplay token 발급
3. UDP socket은 이미 bind/listen 상태
4. GameplayJoin datagram 수신
5. token/sessionId/netId/matchId 검증
6. sourceAddr와 sessionId bind
7. CGameRoom::OnGameplayTransportReady(sessionId)
8. CommandBatch datagram 수신
9. CGameRoom::OnCommandBatch(sessionId, payload)
10. fixed tick에서 pending command 처리
11. SnapshotBuilder로 full snapshot 생성
12. UDP로 snapshot 전송
```

---

## 9. MTU와 Snapshot 크기

M1은 fragment를 구현하지 않는다. 따라서 snapshot 크기가 가장 큰 위험이다.

UDP 안전 기준:

```text
권장 payload <= 1200 bytes
```

하지만 현재 full snapshot은 champion/entity 수에 따라 이보다 커질 수 있다.

M1 대응:

1. snapshot payload size를 매 전송 로그에 남긴다.
2. M1 smoke는 1 player + 최소 entity로 제한한다.
3. 큰 snapshot은 drop하고 경고한다.
4. localhost 개발 편의를 위해 임시 flag를 둘 수 있다.

임시 flag 후보:

```cpp
#define WINTERS_UDP_M1_ALLOW_LARGE_DATAGRAM 1
```

단, 이 flag는 인터넷 환경 acceptance로 인정하지 않는다. M2 fragment/reassembly 전까지는 payload 제한을 계속 의식해야 한다.

---

## 10. Reliability 로드맵

### M1

- unreliable datagram
- full snapshot
- no fragment
- no resend
- no delta

### M2

- channel 도입
- ack / ackBits
- retransmit queue
- reliable ordered
- reliable unordered
- fragment/reassembly
- disconnect timeout
- send budget

### M3

- SnapshotEnvelope
- full/delta
- baseline id
- baseline ack
- AOI filtering
- full resync fallback
- ack-only heartbeat

### M4

- input receive timestamp
- server rewind buffer
- lag compensation
- max rewind window 200ms
- hit validation

### M5

- client prediction
- local command buffer
- reconciliation
- interpolation buffer
- sim-only component subset

### M6

- replay logger
- deterministic replay
- cross-process sim validation
- static/rg determinism guard

---

## 11. Determinism 규칙

UDP 이주는 네트워크 문제처럼 보이지만, 실제로는 서버 authoritative sim과 client prediction을 위한 determinism 기반 작업이다.

유지할 규칙:

- server tick은 fixed timestep.
- pending command 처리 순서는 stable sort.
- 정렬 key는 `acceptedTick`, `sessionId`, `sequenceNum`.
- sim에서 unordered container iteration 금지.
- unordered_map은 lookup-only로 제한.
- `/fp:precise` 유지.
- render/fx/audio/editor/imgui component는 prediction sim subset에서 제외.
- wall clock은 tick scheduling과 receive timestamp에만 쓰고 sim 결과에 직접 섞지 않는다.

현재 확인된 좋은 상태:

- Client/Server vcxproj에 `/fp:precise`가 들어가 있다.
- `GameRoom` command drain에 stable sort가 있다.
- `EntityIdMap`류 unordered_map은 lookup 중심으로 쓰인다.

---

## 12. 보안 최소선

M1에서도 다음은 넣는 편이 좋다.

```text
GameplayJoin token
token TTL
matchId/sessionId/netId 검증
sourceAddr bind
duplicate join 방지
payload size limit
packet magic/version check
sequence sanity check
```

M1에서 하지 않을 것:

- 암호화
- ECDH
- ChaCha20
- anti-cheat full validation

이들은 M2 이후 또는 별도 security phase로 뺀다.

---

## 13. 구현 완료 기준

UDP M1 완료 기준:

- Server가 TCP control port와 UDP gameplay port를 동시에 연다.
- BanPick은 TCP로 정상 동작한다.
- GameStart에 UDP endpoint와 gameplay ticket이 포함된다.
- Client InGame 진입 시 UDP GameplayJoin을 보낸다.
- Server가 token을 검증하고 UDP session을 bind한다.
- Client movement input이 UDP CommandBatch로 간다.
- Server fixed tick이 command를 처리한다.
- Server full Snapshot이 UDP로 돌아온다.
- Client `CSnapshotApplier`가 UDP snapshot을 적용한다.
- TCP gameplay fallback 없이도 1 client localhost smoke가 돈다.

이 지점까지 가면 "UDP가 붙었다"가 아니라 "Gameplay plane이 TCP에서 분리됐다"라고 볼 수 있다.
