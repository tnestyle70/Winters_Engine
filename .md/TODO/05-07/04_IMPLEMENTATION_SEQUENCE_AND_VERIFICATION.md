# UDP Gameplay 이주 구현 순서와 검증

작성일: 2026-05-07  
대상: TCP BanPick / Backend 유지, UDP Gameplay 이주 실행 계획  
결론: **GameRoom sim을 크게 건드리지 말고, Control Plane과 Gameplay Plane의 입출력 경계를 먼저 분리한다.**

---

## 1. 전체 구현 순서

```text
Phase A: 사전 정리와 guard
Phase B: TCP GameStart에 UDP ticket 추가
Phase C: Server UDP M1 skeleton
Phase D: Client UDP gameplay client
Phase E: InGameNetworkBridge dual-plane 전환
Phase F: M1 smoke test
Phase G: M2 reliability / fragment
Phase H: M3 delta / AOI
Phase I: M4 lag compensation
Phase J: M5 prediction
Phase K: M6 replay / determinism
```

---

## 2. Phase A: 사전 정리와 guard

### 목표

UDP 코드를 넣기 전에 현재 TCP/BanPick/Backend 경계를 문서와 코드 레벨에서 고정한다.

### 작업

1. 현재 TCP BanPick 경로 확인

```text
Client/Private/Scene/Scene_BanPick.cpp
Client/Private/Network/Client/GameSessionClient.cpp
Client/Private/Network/Client/ClientNetwork.cpp
Server/Private/Game/GameRoom.cpp
Server/Private/Network/PacketDispatcher.cpp
```

2. 현재 gameplay TCP 경로 확인

```text
Client/Private/Scene/InGameNetworkBridge.cpp
Client/Private/Scene/InGamePlayerControlBridge.cpp
Client/Private/Network/Client/CommandSerializer.cpp
Client/Private/Network/Client/SnapshotApplier.cpp
Server/Private/Game/GameRoom.cpp
Server/Private/Game/SnapshotBuilder.cpp
```

3. 기존 UDP 계획과 충돌하는 표현 정리

```text
.md/plan/sim/10_UDP_LOL_NETSTACK_MASTER_v2.md
.md/plan/sim/10_v2_M1_UDP_TRANSPORT.md
.md/plan/sim/12_BANPICK_ROOM_SYNC_PLAN.md
```

4. grep guard

```powershell
rg -n "CSession_Manager::Instance\(\).*Find|WrapEnvelope\(ePacketType::Snapshot|CommandBatch" Server/Private/Game Server/Public/Game
```

목적:

- GameRoom이 TCP session에 직접 send하는 지점을 찾는다.
- UDP transport helper로 감쌀 위치를 확정한다.

### 완료 기준

- TCP Control Plane과 UDP Gameplay Plane의 책임이 문서화되어 있다.
- GameRoom에서 transport 결합 지점을 확인했다.
- `CGameSessionClient`는 당장 리네임하지 않는 것으로 결정했다.

---

## 3. Phase B: TCP GameStart에 UDP ticket 추가

### 목표

BanPick TCP `GameStart`가 InGame UDP 접속에 필요한 정보를 내려준다.

### 작업 파일 후보

```text
Shared/Schemas/GameStart.fbs
Shared/Schemas/GameplayJoin.fbs
Shared/Network/PacketEnvelope.h
Server/Private/Game/GameRoom.cpp
Client/Private/Network/Client/GameSessionClient.cpp
Client/Private/Scene/Scene_BanPick.cpp
```

### 세부 작업

1. `ePacketType` 확장

```cpp
GameplayJoin = 30,
GameplayJoinAck = 31,
UdpAckOnly = 32,
```

2. `GameStart` payload에 ticket 추가

필드:

```text
matchId
roomId
sessionId
netId
slotId
team
championId
skinId
udpHost
udpPort
token
expiresAtUnixMs
issuedServerTick
```

3. `CGameRoom::TryStartGame` 이후 ticket 생성

```text
roster lock
spawn champions
assign netId
issue gameplay ticket per human session
broadcast GameStart
```

4. `CGameSessionClient`에 ticket cache 추가

```cpp
bool_t HasGameplayTicket() const;
const GameplayJoinTicket& GetGameplayTicket() const;
```

5. `Scene_BanPick`에서 `Scene_InGame` context에 ticket 전달

### 검증

- BanPick TCP flow가 기존처럼 동작한다.
- GameStart 수신 후 client log에 UDP host/port/sessionId/netId/token expiry가 찍힌다.
- UDP client가 아직 없어도 BanPick은 깨지지 않는다.

---

## 4. Phase C: Server UDP M1 skeleton

### 목표

Server가 TCP control port와 별도 UDP gameplay port를 동시에 열고, `GameplayJoin`을 받을 수 있게 한다.

### 새 파일 후보

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

### 세부 작업

1. `CUdpCore`

- UDP socket 생성
- bind
- `SIO_UDP_CONNRESET` disable
- recv loop
- datagram dispatch
- send helper

2. `CUdpSession`

- sessionId
- netId
- remoteAddr
- last recv timestamp
- sequence sanity

3. `CUdpSession_Manager`

- sessionId lookup
- remoteAddr lookup
- bind
- send

4. `CUdpPacketDispatcher`

- `PacketHeader` 검증
- `GameplayJoin` dispatch
- `CommandBatch` dispatch
- heartbeat dispatch

5. `CGameRoom` 연동

```cpp
bool_t ValidateGameplayJoin(...);
void OnGameplayTransportReady(u32_t sessionId);
void OnGameplayTransportLost(u32_t sessionId);
```

### 중요한 gotcha

기존 TCP에서는 accept 시점에 `OnSessionJoin`이 호출된다.

UDP에서는 이것을 따라 하면 안 된다.

```text
TCP OnSessionJoin:
  로비 참가 / 슬롯 할당

UDP OnGameplayTransportReady:
  이미 로비에서 승인된 player의 gameplay transport bind 완료
```

### 검증

- UDP port bind 성공.
- 잘못된 magic/version packet drop.
- 잘못된 token의 `GameplayJoin` 거부.
- 올바른 token의 `GameplayJoin` ack.
- `OnSessionJoin`이 UDP path에서 호출되지 않음.

---

## 5. Phase D: Client UDP gameplay client

### 목표

InGame에서 사용할 별도 UDP client를 만든다.

### 새 파일 후보

```text
Client/Public/Network/Client/UdpGameplayClient.h
Client/Private/Network/Client/UdpGameplayClient.cpp
```

### 세부 작업

1. UDP socket open
2. server endpoint 설정
3. `GameplayJoin` build/send
4. recv thread 또는 pump
5. `GameplayJoinAck` 처리
6. `CommandBatch` send
7. `Snapshot` frame callback
8. disconnect/timeout 처리

### 기존 코드 재사용

재사용:

```text
CSnapshotApplier
FlatBuffers Snapshot schema
FlatBuffers CommandBatch schema
EntityIdMap
```

수정:

```text
CCommandSerializer
```

`CCommandSerializer`는 TCP send와 payload build를 분리하는 편이 좋다.

권장:

```cpp
bool_t BuildMoveCommandBatch(..., vector<uint8_t>& outPayload, u32_t& outSequence);
bool_t SendMove(IGameplayPacketSink& sink, ...);
```

### 검증

- UDP JoinAck 수신.
- JoinAck 이후 network state가 Active.
- command sequence 증가.
- UDP recv thread 종료가 안전함.
- TCP `CGameSessionClient`와 수명 충돌이 없음.

---

## 6. Phase E: InGameNetworkBridge dual-plane 전환

### 목표

InGame이 TCP shared session을 gameplay transport로 쓰지 않고, UDP gameplay client를 사용하게 한다.

### 대상 파일

```text
Client/Private/Scene/InGameNetworkBridge.cpp
Client/Private/Scene/InGamePlayerControlBridge.cpp
Client/Private/Scene/Scene_InGame.cpp
Client/Public/Scene/InGameNetworkBridge.h
```

### 기존 흐름

```text
BanPick TCP session 있음
  -> InGameNetworkBridge가 CGameSessionClient.GetNetwork() 재사용
  -> CommandBatch/Snapshot도 TCP
```

### 목표 흐름

```text
BanPick TCP session 있음
  -> TCP는 control로 유지
  -> GameStart ticket 읽기
  -> CUdpGameplayClient 생성
  -> GameplayJoin
  -> CommandBatch/Snapshot은 UDP
```

### fallback

개발 중에는 다음 fallback을 둘 수 있다.

```text
ticket 없음:
  local-only mode

UDP Join 실패:
  TCP gameplay fallback, 단 debug flag 필요
```

권장 flag:

```cpp
#define WINTERS_ENABLE_TCP_GAMEPLAY_FALLBACK 1
```

단, UDP M1 완료 기준은 fallback 없이 통과해야 한다.

### 검증

- BanPick에서 InGame 전환 성공.
- InGame 진입 후 TCP frame callback과 UDP frame callback이 충돌하지 않음.
- `CSnapshotApplier`가 UDP snapshot을 적용.
- 우클릭 이동이 UDP CommandBatch로 전송.
- 서버 snapshot이 UDP로 돌아옴.

---

## 7. Phase F: M1 smoke test

### 목표

localhost 1 client 기준으로 UDP gameplay 왕복을 확인한다.

### 시나리오

```text
1. Server 실행
2. Client 실행
3. BanPick TCP 접속
4. Champion select / ready
5. StartGame
6. GameStart에서 UDP ticket 수신
7. Scene_InGame 진입
8. UDP GameplayJoin
9. GameplayJoinAck
10. 우클릭 이동
11. UDP CommandBatch
12. Server Tick command 처리
13. UDP full Snapshot
14. Client entity 이동 반영
```

### 로그 필수 항목

Server:

```text
[TCP] SessionJoin sessionId=...
[Lobby] GameStart matchId=... roomId=...
[UDP] Bind port=...
[UDP] GameplayJoin sessionId=... netId=... addr=...
[UDP] CommandBatch sessionId=... seq=... tick=...
[UDP] Snapshot sessionId=... tick=... bytes=...
```

Client:

```text
[TCP] GameStart sessionId=... netId=... udp=host:port
[UDP] Join sent
[UDP] JoinAck accepted tick=...
[UDP] CommandBatch seq=...
[UDP] Snapshot tick=... bytes=...
```

### Acceptance

- UDP JoinAck 성공.
- 60초 동안 crash 없음.
- 이동 command가 서버에 도착.
- snapshot tick 증가.
- client entity transform이 서버 snapshot으로 갱신.
- BanPick TCP 회귀 없음.

---

## 8. Phase G: M2 reliability / fragment

### 목표

UDP를 실전 gameplay transport로 만들기 위한 최소 reliability layer를 넣는다.

### 작업

- `UdpPacketHeader`
- channel enum
- ack / ackBits
- reliable ordered channel
- reliable unordered channel
- retransmit queue
- fragment/reassembly
- send budget
- packet loss stats
- RTT estimate
- disconnect timeout

### 적용 대상

| 데이터 | Channel |
|---|---|
| movement command | unreliable or input channel |
| skill cast command | reliable ordered or command stream |
| snapshot | snapshot unreliable latest-wins |
| important gameplay event | reliable ordered |
| cosmetic fx event | unreliable |

주의:

M2에서 모든 gameplay command를 reliable로 만들면 입력 지연이 늘 수 있다. 이동 command는 최신 입력이 중요하고, 스킬/상점/중요 이벤트는 신뢰성이 중요하다.

---

## 9. Phase H: M3 delta / AOI

### 목표

Snapshot bandwidth를 줄인다.

### 작업

- SnapshotEnvelope
- full snapshot
- delta snapshot
- baseline id
- baseline ack
- AOI filtering
- per-client visible set
- full resync fallback

### AOI 기준

LoL 탑뷰 기준으로 다음이 필요하다.

```text
내 챔피언 주변
아군 시야
포탑/미니언/정글몹 관심 영역
전투 이벤트 관심 영역
전역 오브젝트 이벤트
```

M3 초기에는 단순 거리 기반 AOI로 시작한다.

---

## 10. Phase I: M4 lag compensation

### 목표

서버 권위 판정에서 플레이어 입력 시점의 과거 상태를 보정한다.

### 작업

- receive timestamp 기록
- command client time/server receive time 보관
- rewind buffer
- max rewind window 200ms
- hitbox rewind
- skill shot validation
- anti-cheat range/cooldown validation과 연결

현재 `Server/Public/Security/AntiCheatServer.h`와 `LagCompensation.h`는 stub 상태다. M4에서 실제 내용이 들어간다.

---

## 11. Phase J: M5 client prediction

### 목표

이동과 일부 입력을 클라이언트에서 즉시 예측하고 서버 snapshot으로 보정한다.

### 작업

- local command buffer
- predicted transform
- server acked command seq
- reconciliation
- interpolation buffer
- correction smoothing
- prediction debug overlay

### sim-only allowlist

prediction에 넣을 수 있는 것:

```text
TransformComponent
NavAgentComponent
Velocity/Movement state
SkillStateComponent 중 deterministic subset
Health/Mana deterministic state
```

prediction에서 제외:

```text
RenderComponent
ModelRenderer
Animator raw state
FxBillboardComponent
Audio
ImGui/editor state
Resource handles
```

---

## 12. Phase K: M6 replay / determinism

### 목표

서버 authoritative sim과 prediction/replay가 같은 입력에 같은 결과를 내는지 검증한다.

### 작업

- command log
- snapshot checksum
- replay runner
- cross-process deterministic check
- unordered iteration rg guard
- floating point config check
- CI smoke

검사 후보:

```powershell
rg -n "unordered_map|unordered_set" Shared Server/Private/Game Client/Private/Scene
rg -n "std::chrono|GetTickCount|QueryPerformanceCounter|random_device|mt19937" Shared Server/Private/Game Client/Private/Scene
```

검사는 발견 즉시 금지가 아니라, sim 경로에 섞였는지 판단해야 한다. lookup-only unordered_map이나 tick scheduling clock은 허용될 수 있다.

---

## 13. 빌드 / 실행 검증 명령 후보

솔루션/프로젝트 이름은 실제 환경에 맞춰 조정한다.

```powershell
rg --version
rg -n "GameplayJoin|UdpGameplay|UdpCore|UdpSession" Shared Client Server
rg -n "CSession_Manager::Instance\(\).*Find|WrapEnvelope\(ePacketType::Snapshot" Server/Private/Game
```

MSBuild 후보:

```powershell
msbuild Winters.sln /m /p:Configuration=Debug /p:Platform=x64
```

서버/클라이언트 smoke 후보:

```powershell
.\Server\Bin\Debug\WintersServer.exe
.\Client\Bin\Debug\WintersGame.exe
```

실제 산출물 경로는 현재 vcxproj 설정과 post-build 출력 경로를 확인해야 한다.

---

## 14. 코드 리뷰 체크리스트

UDP 이주 PR/커밋마다 확인:

- 새 파일명에 `C` 접두사가 붙지 않았는가?
- 클래스명에는 `C` 접두사가 붙었는가?
- Engine/Public 또는 SDK 노출 헤더가 PCH에 의존하지 않는가?
- cross-folder include가 `"Network/UdpCore.h"`처럼 폴더 경로를 포함하는가?
- 신규 타입에 `u32_t`, `bool_t`, `f32_t` 등을 썼는가?
- UDP path에서 `OnSessionJoin`을 호출하지 않는가?
- GameRoom snapshot send가 TCP manager에 직접 박혀 있지 않은가?
- UDP datagram payload size limit이 있는가?
- invalid packet drop path가 crash 없이 끝나는가?
- socket thread shutdown이 안전한가?
- TCP BanPick 회귀가 없는가?
- fallback flag가 release 기본값을 오염시키지 않는가?

---

## 15. 첫 구현 단위 추천

가장 작은 안전한 첫 구현 단위:

```text
1. PacketEnvelope에 GameplayJoin / GameplayJoinAck 추가
2. GameStart에 UDP endpoint + dummy token 추가
3. Server CUdpCore가 port bind하고 invalid packet drop 로그 출력
4. Client CUdpGameplayClient가 GameStart ticket으로 GameplayJoin 전송
5. Server가 token 검증 없이 debug accept 후 JoinAck 응답
6. 이후 token 검증 추가
7. 이후 CommandBatch/Snapshot 연결
```

이 순서가 좋은 이유:

- BanPick TCP 회귀를 빨리 잡을 수 있다.
- UDP socket 문제와 flatbuffer/schema 문제를 분리할 수 있다.
- GameRoom sim 수정 전에 transport 왕복을 먼저 확인할 수 있다.

---

## 16. 최종 완료 기준

전체 UDP Gameplay 이주는 다음 상태면 1차 완료다.

- Backend HTTP는 기존 서비스 역할을 유지한다.
- BanPick은 TCP로만 로비 상태를 동기화한다.
- GameStart가 UDP gameplay ticket을 내려준다.
- InGame은 별도 UDP client를 생성한다.
- CommandBatch는 UDP로 전송된다.
- Snapshot은 UDP로 수신된다.
- TCP gameplay fallback 없이 1 client smoke가 통과한다.
- 이후 M2 reliability와 M3 delta를 얹을 수 있는 transport 경계가 생겼다.

이 상태가 되면 Winters의 네트워크 구조는 "MVP TCP 통합형"에서 "실전형 control/gameplay 분리 구조"로 넘어간다.
