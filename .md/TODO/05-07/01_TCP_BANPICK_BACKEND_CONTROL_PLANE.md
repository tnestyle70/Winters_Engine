# TCP BanPick / Backend Control Plane 계획

작성일: 2026-05-07  
대상: Backend HTTP + BanPick TCP + GameStart 제어 흐름  
결론: **TCP는 BanPick과 Backend 연동을 담당하는 제어면으로 유지한다.**

---

## 1. 목표

이 문서는 UDP Gameplay 이주 중에도 TCP에 남겨야 하는 영역을 명확히 정의한다.

Winters의 최종 네트워크는 하나의 프로토콜로 모든 것을 처리하는 구조가 아니라, 성격에 따라 면을 분리한다.

```text
Backend HTTP:
  로그인, 프로필, 상점, 결제, 매칭

TCP Control:
  BanPick, LobbyState, GameStart, UDP 접속 티켓

UDP Gameplay:
  CommandBatch, Snapshot, Event, Prediction
```

TCP Control Plane의 핵심 책임은 다음이다.

- 유저가 누구인지 확인한다.
- 어떤 매치/방에 들어가는지 결정한다.
- BanPick의 슬롯 상태를 모든 클라이언트에 안정적으로 동기화한다.
- GameStart 시점에 roster를 잠근다.
- InGame에서 사용할 UDP gameplay 접속 정보를 내려준다.
- UDP gameplay 접속 실패나 reconnect를 제어한다.

---

## 2. 현재 구현 기준

### 2.1 Backend Services

현재 `Services/`에는 Go 기반 서비스들이 분리되어 있다.

```text
Services/cmd/auth          : 8081
Services/cmd/leaderboard   : 8082
Services/cmd/matchmaking   : 8083
Services/cmd/profile       : 8084
Services/cmd/payment       : 8085
Services/cmd/shop          : 8086
```

공용 인프라:

```text
Services/pkg/auth/jwt.go
Services/pkg/cache/redis.go
Services/pkg/config/config.go
Services/pkg/database/postgres.go
Services/pkg/messaging/kafka.go
Services/pkg/middleware
Services/pkg/response
```

클라이언트 C++ SDK 위치:

```text
Client/Public/Network/Backend
Client/Private/Network/Backend
```

현재 HTTP client는 WinHTTP 기반으로 `AsyncGet` / `AsyncPost` 흐름을 갖는다. 이 경로는 UDP gameplay와 섞지 않는다.

### 2.2 BanPick TCP

현재 BanPick TCP는 다음 흐름을 갖는다.

```text
Scene_BanPick
  -> CGameSessionClient::Connect
  -> CClientNetwork TCP connect
  -> LobbyCommand 전송
  <- LobbyState 수신
  <- GameStart 수신
  -> Scene_InGame 전환
```

관련 파일:

```text
Client/Private/Scene/Scene_BanPick.cpp
Client/Private/Network/Client/GameSessionClient.cpp
Client/Private/Network/Client/ClientNetwork.cpp
Server/Private/Game/GameRoom.cpp
Server/Private/Network/PacketDispatcher.cpp
```

현재 `CGameSessionClient`는 BanPick에서 연결한 TCP session을 유지하고, InGame으로 넘어간 뒤에도 마지막 `Hello`를 replay할 수 있다. 이 성질은 UDP 이주 후에도 유효하다. 다만 InGame gameplay 패킷은 더 이상 이 TCP session을 사용하지 않게 만든다.

---

## 3. TCP에 남길 패킷

현재 `Shared/Network/PacketEnvelope.h` 기준 TCP control에 남길 타입:

```cpp
Hello = 10,
Heartbeat = 11,
Disconnect = 12,
LobbyCommand = 20,
LobbyState = 21,
GameStart = 22,
```

권장 분류:

| Packet | Plane | 유지 이유 |
|---|---|---|
| `Hello` | TCP Control | BanPick/Lobby 접속 후 session/netId/room context 전달 |
| `Heartbeat` | TCP Control | 로비 연결 유지와 disconnect 감지 |
| `Disconnect` | TCP Control | 의도적 방 나가기, 로딩 실패 |
| `LobbyCommand` | TCP Control | 슬롯/팀/챔피언/ready 변경 |
| `LobbyState` | TCP Control | 안정적 전체 로비 상태 동기화 |
| `GameStart` | TCP Control | roster lock, scene transition, UDP ticket 전달 |

주의:

- `CommandBatch`, `Snapshot`, `Event`는 최종적으로 UDP Gameplay Plane으로 이동한다.
- 단, 개발 편의를 위해 TCP fallback은 일정 기간 남길 수 있다.

---

## 4. Backend와 TCP BanPick의 책임 분리

### 4.1 Backend HTTP 책임

Backend는 "영속/계정/매칭" 데이터의 주인이다.

```text
Auth:
  로그인, 회원가입, JWT 발급

Matchmaking:
  큐 입장, 매칭 성사, room allocation

Profile:
  전적, 인벤토리, 장착 스킨

Shop:
  상품 조회, 스킨 구매 요청

Payment:
  RP 충전, 결제 기록

Leaderboard:
  랭킹 조회
```

Backend가 직접 처리하지 않을 것:

- 매 프레임 이동 입력
- 스킬 판정
- 데미지 계산
- snapshot replication
- client prediction

### 4.2 TCP Game Server 책임

TCP Game Server는 "실시간 방 제어" 데이터의 주인이다.

```text
Lobby:
  session join/leave
  slot assign
  team assign
  champion select
  ready

GameStart:
  roster lock
  bot fill
  ECS champion spawn
  initial netId assign
  UDP gameplay ticket issue
```

### 4.3 UDP Game Server 책임

UDP Game Server는 "게임 진행" 데이터의 주인이다.

```text
Gameplay:
  movement command
  skill command
  attack command
  authoritative tick
  snapshot/event replication
  reliability/delta/AOI/prediction
```

---

## 5. `CGameSessionClient` 유지 전략

### 5.1 이름 유지 vs 리네임

현재 이름:

```cpp
CGameSessionClient
```

역할이 BanPick TCP control로 굳어지면 장기적으로는 다음 이름이 더 정확하다.

```cpp
CControlSessionClient
```

하지만 당장 리네임은 파급이 크다. 추천 순서는 다음이다.

1. UDP M1에서는 `CGameSessionClient` 이름을 유지한다.
2. 내부 주석과 문서에서 "TCP Control Session" 역할을 명시한다.
3. UDP M1 안정화 후 `CControlSessionClient` 리네임을 별도 PR/Phase로 진행한다.

이렇게 하면 UDP 이주와 대형 rename이 섞이지 않는다.

### 5.2 유지할 API

현재 유지:

```cpp
bool_t Connect(const char* host, uint16_t port);
bool_t IsConnected() const;
void SendLobbyCommand(...);
void SetFrameCallback(...);
void ReplayLastHelloToGameFrameCallback();
```

추가 권장:

```cpp
bool_t HasGameplayTicket() const;
const GameplayJoinTicket& GetGameplayTicket() const;
const LobbyRosterCache& GetLockedRoster() const;
```

의미:

- `CGameSessionClient`는 TCP control state의 owner다.
- InGame은 이 객체에서 UDP 접속 티켓과 locked roster만 읽는다.
- InGame gameplay 송수신은 `CUdpGameplayClient`가 맡는다.

---

## 6. `GameStart` 확장 계획

현재 `GameStart`는 scene transition과 roster lock 신호에 가깝다. UDP 이주 후에는 다음 정보가 필요하다.

```text
matchId
roomId
tcpSessionId
playerNetId
slotId
team
championId
skinId
udpHost
udpPort
gameplayToken
tokenExpireUnixMs
serverTick
snapshotRate
commandRate
```

권장 구조:

```text
GameStart
  roster[]
  localPlayerNetId
  udpEndpoint
  gameplayJoinTicket
```

`GameStart`는 TCP로 안전하게 내려온다. 클라이언트는 이 정보를 들고 UDP server에 `GameplayJoin`을 보낸다.

---

## 7. Server `CGameRoom` Control Plane 정리

현재 `CGameRoom`이 이미 로비와 게임을 모두 담당한다. UDP 이주 전후의 역할을 나누면 다음과 같다.

### 7.1 유지할 Control 메서드

```cpp
void OnSessionJoin(u32_t sessionId);
void OnSessionLeave(u32_t sessionId);
void OnLobbyCommand(u32_t sessionId, ...);
bool_t TryStartGame(...);
void BroadcastLobbyStateLocked();
void BroadcastGameStartLocked();
void SendHelloToSessionLocked(u32_t sessionId);
```

이 메서드들은 TCP control session 기준으로 유지한다.

### 7.2 추가할 Control 책임

```cpp
GameplayJoinTicket BuildGameplayJoinTicketLocked(u32_t sessionId) const;
bool_t ValidateGameplayJoinTicket(u32_t sessionId, const GameplayJoinTicket& ticket) const;
void OnGameplayTransportReady(u32_t sessionId, const GameplayTransportPeer& peer);
void OnGameplayTransportLost(u32_t sessionId);
```

초기 M1에서는 구조체를 크게 만들지 않고, `GameStart` payload에 최소 필드를 넣어도 된다. 하지만 최종적으로는 위 책임이 필요하다.

### 7.3 Bot 슬롯

현재 `TryStartGame`은 빈 슬롯을 bot으로 채운다.

UDP 이주 후:

- human slot만 UDP gameplay ticket을 받는다.
- bot은 서버 내부 command producer로 동작한다.
- bot에는 UDP session이 없다.
- Snapshot에는 bot entity도 포함된다.

---

## 8. Client Scene 전환 흐름

### 8.1 현재 흐름

```text
Scene_BanPick
  -> GameStart 수신
  -> Scene_InGame context 생성
  -> InGameNetworkBridge가 기존 TCP session 재사용
```

### 8.2 목표 흐름

```text
Scene_BanPick
  -> GameStart 수신
  -> TCP session은 유지
  -> Scene_InGame context에 roster + UDP ticket 전달
  -> InGameNetworkBridge가 CUdpGameplayClient 생성
  -> GameplayJoin 전송
  -> GameplayJoinAck 수신
  -> Snapshot 수신 시작
```

중요:

- TCP session은 끊지 않는다.
- InGame 중에도 TCP는 reconnect, surrender, post-game, control event에 쓸 수 있다.
- Gameplay command/snapshot만 UDP로 이동한다.

---

## 9. Backend 연동 확장 계획

### 9.1 단기

단기에는 Game Server가 자체적으로 room/session을 만든다.

```text
Client
  -> Backend Auth/Profile optional
  -> Game TCP localhost:9000
  -> BanPick
  -> GameStart
  -> UDP Gameplay localhost:9001
```

이 단계는 UDP M1 smoke에 적합하다.

### 9.2 중기

Matchmaking service가 Game Server room allocation 정보를 내려준다.

```text
Client -> Matchmaking HTTP
  <- matchId, gameServerHost, tcpControlPort, udpGameplayPort, authTicket

Client -> TCP Control
  -> authTicket
  <- LobbyState
```

### 9.3 장기

Backend와 Game Server 사이에도 room auth 검증이 들어간다.

```text
Matchmaking
  -> Game Server room reservation
  -> roomId / matchId / participant list

Game Server
  -> validates control join token
  -> validates gameplay join token

Profile
  -> selected skin/champion ownership check
```

---

## 10. TCP Control Plane 구현 작업

### 10.1 `GameStart` payload 확장

목표:

- UDP endpoint 전달
- gameplay token 전달
- local player의 `sessionId`, `netId`, `slotId` 명시
- locked roster 전달

작업 후보:

```text
Shared/Schemas/GameStart.fbs 확장
Shared/Generated 갱신
Server/Private/Game/GameRoom.cpp BroadcastGameStartLocked 수정
Client/Private/Network/Client/GameSessionClient.cpp OnFrame(GameStart) 수정
Client/Private/Scene/Scene_BanPick.cpp GameStart transition context 수정
```

### 10.2 Control session naming 정리

단기:

- 기존 `CGameSessionClient` 유지
- 주석에 TCP Control 역할 명시

중기:

```text
CGameSessionClient -> CControlSessionClient
```

리네임 시점은 UDP M1 이후가 좋다.

### 10.3 TCP fallback 유지

개발 편의를 위해 다음 옵션을 둔다.

```cpp
#define WINTERS_ENABLE_TCP_GAMEPLAY_FALLBACK 1
```

또는 runtime flag:

```text
--gameplay-transport=tcp
--gameplay-transport=udp
```

단, 최종 smoke와 acceptance는 UDP를 기준으로 둔다.

---

## 11. 검증 항목

### 11.1 BanPick 단독 검증

- TCP connect 성공
- 첫 `Hello` 수신
- LobbyState 수신
- 팀 변경
- 챔피언 선택
- ready 상태 동기화
- StartGame 시 빈 슬롯 bot fill
- GameStart 수신

### 11.2 Backend 연동 검증

- Auth login 성공
- Profile inventory 조회
- Shop skin 구매 후 Profile 반영
- Matchmaking queue enter 후 room metadata 수신

### 11.3 UDP 이주 후 회귀 검증

- UDP가 꺼져도 BanPick은 정상 동작해야 한다.
- UDP Join 실패 시 BanPick TCP session은 살아 있어야 한다.
- InGame에서 TCP control disconnect와 UDP gameplay disconnect를 구분해서 표시해야 한다.
- LobbyState는 UDP traffic과 무관하게 TCP에서만 와야 한다.

---

## 12. 완료 기준

TCP Control Plane은 다음 상태면 완료로 본다.

- `Scene_BanPick`이 TCP로만 로비 상태를 동기화한다.
- `GameStart`가 UDP gameplay 접속에 필요한 endpoint/token/context를 포함한다.
- `CGameSessionClient`가 UDP gameplay packet을 직접 보내지 않는다.
- `Scene_InGame`은 TCP session을 control용으로 유지하면서 별도 UDP client를 생성한다.
- Backend HTTP SDK와 UDP gameplay path 사이에 직접 의존이 없다.

이 기준이 잡히면 UDP 이주는 GameRoom sim 자체를 흔들지 않고 transport 경계만 교체하는 작업이 된다.
