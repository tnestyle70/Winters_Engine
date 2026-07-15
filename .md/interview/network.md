# 네트워크 & 게임 서버 동기화 — 기술면접 대비

> **Winters as-built 동기화 (2026-07-13)**
>
> - TCP IOCP + 16-byte `PacketEnvelope` 경로는 제거되지 않았고 fallback/baseline으로 유지된다.
> - UDP v3 공용 코어는 구현됐다. wire header 40B, datagram 상한 1200B, fragment header 16B, fragment data 상한 1144B, logical message 상한 64 KiB, message당 최대 64 fragments다.
> - lane 계약은 Control/Lobby/Command/Event=`ReliableOrdered`, Snapshot/Heartbeat=`UnreliableSequenced`다. 각 lane은 독립 packet/message sequence, ACK latest+32-bit history, ordered gap buffer와 bounded reassembly를 가진다.
> - 서버 `CUdpIocpCore`는 `WSARecvFrom`/`WSASendTo` overlapped IOCP를 담당하고, `CServerSessionHub`가 TCP/UDP를 공통 logical session ID와 `SendFrame`으로 감싼다. UDP callback은 bounded ingress에 owned bytes만 넣고 `CGameRoom::Tick`이 `DrainIngress`한 뒤에만 world를 변경한다.
> - `Server/Private/main.cpp`는 `tcp|udp|dual` transport와 세 Job/Fiber mode를 연결했고 TCP·UDP·dual F5 smoke를 통과했다. 그러나 TCP가 기본 fallback이고 아래 production gate가 남아 있으므로 **UDP production cutover 완료**라고 말하지 않는다.
> - 미완료 production 조건: post-handshake MAC/AEAD, RTT estimator·pacing·congestion control·fast retransmit, IPv6/NAT rebinding/PMTU, graceful close handshake, snapshot delta/baseline/AOI, WAN loss/dup/reorder/soak와 abuse 검증.
>
> 2026-07-13 replay payload 표본(1,786 snapshots)은 평균 15,415.8B, p50 16,256B, p95 19,808B, max 22,104B였다. 현재 1144B fragment data 기준 평균 13.84 datagrams, p95 18, max 20이며 payload만 30Hz에서 약 451.63 KiB/s/client다. 이는 구현 상한을 통과한다는 뜻이지 규모 확장에 충분하다는 뜻이 아니다. p95를 4 datagrams 안으로 낮추려면 delta/AOI/quantization으로 약 4.6 KiB 이하를 목표로 해야 한다.

> 근거 코드: Winters 자체 DX11 엔진 + 서버 권위(server-authoritative) LoL 클론.
> 본 문서의 모든 "내 프로젝트" 인용은 실제 파일 기준이다:
> - IOCP 코어: `Server/Private/Network/IOCPCore.cpp`
> - 세션/송수신: `Server/Private/Network/Session.cpp`
> - 프레이밍: `Server/Private/Network/FrameParser.cpp`, `Shared/Network/PacketEnvelope.h`
> - 스냅샷 생성: `Server/Private/Game/SnapshotBuilder.cpp`
> - 스냅샷 적용/예측 보호: `Client/Private/Network/Client/SnapshotApplier.cpp`
> - 틱 루프: `Server/Private/Game/GameRoomTick.cpp`, `Shared/GameSim/Core/Determinism/DeterministicTime.h`
> - 랙 컴펜세이션: `Server/Public/Security/LagCompensation.h`
> - HTTP 백엔드: `Client/Private/Network/Backend/CHttpClient.cpp`
> - UDP wire/reliability: `Shared/Network/UdpPacketHeader.h`, `UdpFragmentHeader.h`, `PacketSemantics.h`, `UdpReliabilityChannel.*`, `UdpReassemblyBuffer.h`
> - UDP endpoints/session adapter: `Server/Private/Network/UdpIocpCore.cpp`, `Server/Private/Network/ServerSessionHub.cpp`, `Client/Private/Network/Client/UdpClient.cpp`

## 출제 경향 개요

게임회사 서버/클라 공통 면접에서 네트워크는 3단 구성으로 나온다.

1. **CS 기초 검증**: OSI/TCP-IP, TCP vs UDP, handshake, TIME_WAIT, 흐름/혼잡제어, Nagle. "정의를 아는가"가 아니라 "왜 그렇게 설계됐는지 설명할 수 있는가"를 본다.
2. **Windows 서버 실무**: blocking→select→IOCP로 이어지는 IO 모델 발전사, IOCP 워커 스레드 모델, Overlapped 버퍼 수명, TCP 스트림 경계와 프레이밍. 자체 서버를 만들어봤다면 여기서 코드 레벨 꼬리질문이 들어온다.
3. **게임 동기화 아키텍처**: 서버 권위 vs 락스텝, 틱레이트, 스냅샷/델타, 클라 예측/리컨실리에이션/보간, lag compensation. "롤은 왜 이렇게 하는가"류의 실전 설계 질문이 많고, 직접 구현 경험이 있으면 가장 크게 차별화되는 영역.

Winters에는 IOCP TCP 기준선과 UDP v3 transport core, 30Hz 고정 틱, FlatBuffers 스냅샷, 클라 예측 보호가 함께 존재한다. 모든 답변은 "일반 원리 → 실제 구현 → 아직 닫히지 않은 production 조건" 순서로 말한다. UDP core 구현과 production migration 완료를 구분하는 정직성이 핵심이다.

---

## 핵심 개념 정리

### 1. 계층 모델 — OSI 7계층 vs TCP/IP 4계층

- **정의**: OSI 7계층은 ISO가 정의한 개념적 참조 모델(물리-데이터링크-네트워크-전송-세션-표현-응용). TCP/IP 4계층은 실제 인터넷 구현 모델(네트워크 액세스-인터넷-전송-응용)로, OSI의 5~7계층(세션/표현/응용)이 응용 계층 하나로 합쳐지고 1~2계층이 네트워크 액세스로 합쳐진다.
- **동작 원리**: 송신 시 각 계층이 자신의 헤더를 붙이는 캡슐화(encapsulation), 수신 시 역순으로 벗기는 역캡슐화. 계층 간 책임 분리 덕에 이더넷을 와이파이로 바꿔도 TCP 코드는 그대로다.
- **예시**: 게임 패킷 하나의 여정 = 내 앱의 FlatBuffers 페이로드(응용) → TCP 세그먼트(전송, 포트) → IP 패킷(인터넷, 주소/라우팅) → 이더넷 프레임(링크, MAC).
- **게임 개발 맥락**: 게임 프로그래머가 실제로 만지는 층은 전송(TCP/UDP 선택, 소켓 옵션)과 응용(프레이밍, 직렬화, 동기화 프로토콜)이다. Winters에서 세션/표현/응용에 해당하는 것이 각각 `CSession`(연결 수명), FlatBuffers(표현/직렬화), 스냅샷·커맨드 프로토콜(응용)이라고 매핑해 설명하면 "모델을 외운 사람"이 아니라 "모델로 자기 코드를 설명하는 사람"이 된다.

### 2. TCP vs UDP

- **TCP**: 연결 지향, 바이트 스트림, 신뢰성(재전송), 순서 보장, 흐름제어(수신 윈도우), 혼잡제어(cwnd). 대가는 지연 — 패킷 하나가 유실되면 재전송될 때까지 뒤에 도착한 데이터도 애플리케이션에 전달되지 않는 **Head-of-Line(HOL) blocking**.
- **UDP**: 비연결, 데이터그램(메시지 경계 보존), 유실/중복/순서 뒤바뀜 그대로 노출. 대신 지연 특성이 예측 가능하고, 필요한 신뢰성만 애플리케이션이 골라 구현할 수 있다.
- **장르별 선택**:
  - FPS/격투/레이싱(틱당 최신 상태만 중요): UDP + 자체 신뢰성 계층. 오래된 위치 패킷은 재전송할 가치가 없다 — "신뢰성 있는 재전송"이 오히려 해가 된다.
  - MMO/MOBA/턴제(명령이 유실되면 안 됨): TCP 또는 RUDP. LoL도 초기에 자체 UDP 신뢰성 계층(ENet 계열)을 썼다.
  - 웹 연동/로비/결제: TCP 기반 HTTP(S).
- **내 프로젝트**: Winters는 TCP IOCP + TCP_NODELAY 기준선을 보존하면서 UDP v3를 병렬 transport로 구현했다. 명령/이벤트는 lane별 reliable-ordered, 스냅샷/하트비트는 unreliable-sequenced로 분리해 TCP의 단일 "전부·순서대로" 계약을 메시지 의미별 계약으로 바꿨다. feature-gated main/F5 배선과 TCP·UDP·dual smoke까지 완료했으므로 면접에서는 "TCP MVP → 측정 → UDP core → 제품 배선 → production hardening"의 실제 단계로 말한다.

### 3. 3-way handshake (연결 수립)

- **과정**: ① 클라 SYN(seq=x) → ② 서버 SYN+ACK(seq=y, ack=x+1) → ③ 클라 ACK(ack=y+1). 이후 ESTABLISHED.
- **왜 3번인가**: 양쪽 모두 "내 초기 시퀀스 번호(ISN)가 상대에게 전달됐음"을 확인해야 하기 때문. 2-way면 서버는 자신의 ISN이 클라에 도달했는지 알 수 없고, 지연돼 떠돌던 옛 SYN이 유령 연결을 만든다.
- **게임 맥락**: SYN flood(3단계 ACK를 보내지 않아 half-open 연결로 백로그를 채우는 공격)와 SYN cookie 정도는 꼬리질문으로 나온다. `listen(m_listenSocket, SOMAXCONN)`의 backlog가 바로 이 half-open/완료 대기 큐 크기다(`IOCPCore.cpp`).
- **애플리케이션 레벨 handshake**: TCP 연결 후에도 게임은 자체 Hello 교환이 필요하다. Winters는 접속 직후 서버가 `Hello` 패킷(ePacketType::Hello=10)으로 `yourNetId`, `sessionId`, `serverTick`, 그리고 **`dataBuildHash`(서버/클라 게임데이터 빌드 해시)**를 내려주고, 클라 `SnapshotApplier::OnHello`가 해시 불일치를 접속 시점에 즉시 로그로 가시화한다. "수치가 미묘하게 다른 드리프트"를 런타임 디버깅이 아니라 접속 핸드셰이크에서 잡는 설계다.

### 4. 4-way handshake와 TIME_WAIT

- **과정**: 종료 요청측 FIN → 상대 ACK → (상대가 남은 데이터 전송 후) 상대 FIN → 요청측 ACK. 요청측(active closer)은 마지막 ACK 후 **TIME_WAIT** 상태로 2*MSL(Maximum Segment Lifetime, 보통 30~120초) 대기.
- **TIME_WAIT의 존재 이유**: ① 마지막 ACK가 유실되면 상대의 FIN 재전송에 다시 ACK해줄 주체가 필요. ② 같은 (IP,포트) 4-tuple로 즉시 새 연결이 생기면 이전 연결의 지연 세그먼트가 새 연결 데이터로 오인될 수 있어, 옛 세그먼트가 네트워크에서 죽을 때까지 기다린다.
- **서버 재시작 문제**: 서버가 연결들을 능동 종료하고 죽으면 리슨 포트 관련 소켓들이 TIME_WAIT에 걸려 재시작 시 `bind()`가 `WSAEADDRINUSE`로 실패할 수 있다. 통상 해법은 `SO_REUSEADDR`.
- **내 프로젝트의 선택**: Winters `IOCPCore.cpp::Start()`는 `SO_REUSEADDR`가 아니라 **`SO_EXCLUSIVEADDRUSE`**를 켠다. Windows에서 `SO_REUSEADDR`는 유닉스와 달리 "동일 포트에 다른 프로세스가 강제로 겹쳐 bind"하는 포트 하이재킹을 허용하는 방향으로 동작하기 때문에, 게임 서버 리슨 소켓은 배타 점유가 안전하다는 판단이다. 재시작 지연은 "서버는 가급적 passive close가 되도록 하고(클라가 먼저 끊게), 리슨 소켓 자체는 데이터 연결이 아니므로 TIME_WAIT 대상이 거의 아니다"라는 점까지 붙이면 완결된 답이 된다.

### 5. 흐름제어 vs 혼잡제어

- **흐름제어(Flow Control)**: **수신자** 보호. 수신자가 광고하는 수신 윈도우(rwnd, 수신 버퍼 여유)만큼만 보낸다. 수신 앱이 느리면 rwnd가 줄고, 0이 되면 송신 중단(zero window) 후 window probe로 재개를 살핀다.
- **혼잡제어(Congestion Control)**: **네트워크** 보호. 송신자가 혼잡 윈도우(cwnd)를 스스로 추정. 실제 전송량 = min(rwnd, cwnd).
  - **슬로우 스타트**: cwnd를 1 MSS에서 시작해 ACK마다 지수적으로(RTT당 2배) 증가, ssthresh 도달 시 혼잡 회피로 전환.
  - **AIMD(Additive Increase Multiplicative Decrease)**: 혼잡 회피 구간에서 RTT당 +1 MSS 선형 증가, 유실 감지 시 절반으로 감소(빠른 재전송/3중복 ACK 시 절반, 타임아웃 시 cwnd=1부터 재시작). 이 비대칭이 여러 흐름 간 공평성과 안정성을 만든다.
- **게임 맥락**: 실시간 게임 트래픽은 작고 주기적이라 cwnd를 채울 일이 드물지만, **유실 후 재전송+cwnd 축소가 겹치면 지연 스파이크**가 온다. 이것이 TCP 게임에서 "평소엔 멀쩡한데 패킷 유실 순간 수백 ms 멈칫"하는 근본 원인이고, UDP 자체 프로토콜로 넘어가는 핵심 동기다. 또한 수신 앱(클라)이 렉 걸려 recv를 못 하면 rwnd가 줄어 서버 송신 큐가 쌓인다 — Winters `CSession`의 send queue가 무한히 자라는지 모니터링해야 하는 이유를 여기 연결할 수 있다.

### 6. Nagle 알고리즘과 TCP_NODELAY

- **정의**: 작은 세그먼트 난사(예: 1바이트씩 41바이트 패킷)를 막기 위해, "ACK되지 않은 미확인 데이터가 있으면 작은 데이터를 모아서(coalesce) 보낸다"는 송신측 알고리즘.
- **문제**: 수신측 **Delayed ACK**(ACK를 최대 200ms 모았다 보냄)와 결합하면, 작은 요청-응답 패턴에서 서로 기다리는 최악 조합이 되어 수십~200ms 지연이 붙는다.
- **게임에서 왜 끄나**: 게임 패킷은 이미 틱 단위로 배칭돼 있고, 1틱(33ms)짜리 입력이 200ms 늦게 가면 게임이 망가진다. 대역폭 절약보다 지연이 압도적으로 중요하므로 `TCP_NODELAY`로 Nagle을 끈다.
- **내 프로젝트**: `IOCPCore.cpp` Accept 완료 처리에서 세션 소켓에 즉시 적용한다.
  ```cpp
  BOOL on = TRUE;
  ::setsockopt(ctx->acceptSocket, IPPROTO_TCP, TCP_NODELAY,
      reinterpret_cast<const char*>(&on), sizeof(on));
  ```
  "NODELAY를 켰으니 작은 패킷 병합은 애플리케이션 책임"이라는 후속 논리도 준비: Winters는 스냅샷을 틱당 1회로 배칭하고, 클라 이동 명령은 펜딩 중 최신 Move로 교체(coalescing)해 스팸을 줄인다.

### 7. MTU, 단편화, MSS

- **MTU(Maximum Transmission Unit)**: 링크 계층이 한 번에 나를 수 있는 최대 IP 패킷 크기. 이더넷 1500바이트.
- **단편화(Fragmentation)**: IP 패킷이 MTU보다 크면 라우터/송신자가 쪼갠다. 조각 하나만 유실돼도 전체 패킷 재조립 실패 → 유실률이 조각 수만큼 곱으로 증폭. IPv6와 DF(Don't Fragment) 설정 시 라우터는 쪼개지 않고 버린다(Path MTU Discovery가 ICMP로 알려줌 — 방화벽이 ICMP를 막으면 블랙홀).
- **MSS**: TCP 페이로드 최대 크기 = MTU - IP헤더(20) - TCP헤더(20) = 이더넷 기준 1460. TCP는 MSS를 handshake에서 협상하므로 단편화를 스스로 회피한다.
- **게임 맥락 / Winters 실물**: UDP 데이터그램은 `kUdpMaxDatagramBytes=1200`으로 제한한다. v3 header 40B를 빼면 단일 packet payload는 1160B이고, 조각이면 fragment header 16B를 더 빼 실제 data는 1144B다. `messageId/messageBytes/index/count/payloadSize`를 명시적으로 big-endian encode해 최대 64 KiB·64조각까지 bounded reassembly한다. TCP 경로는 기존 64 KiB app frame 상한을 두고 세그먼트화는 TCP에 맡긴다.

### 8. 소켓 API 흐름과 blocking vs non-blocking

- **서버**: `socket()` → `setsockopt()` → `bind()`(주소:포트 결합) → `listen()`(백로그 지정, 수동 대기) → `accept()`(연결 소켓 반환) → `recv()/send()` → `closesocket()`.
- **클라**: `socket()` → `connect()`(3-way handshake 유발) → `send()/recv()` → `closesocket()`.
- **blocking**: recv 호출 시 데이터가 올 때까지 스레드가 잠든다. 커넥션당 스레드 1개 모델 → 수천 연결이면 스레드 수천 개, 컨텍스트 스위칭과 스택 메모리로 붕괴.
- **non-blocking**: `ioctlsocket(FIONBIO)`로 전환, 즉시 `WSAEWOULDBLOCK` 반환. 폴링 낭비가 생기므로 "언제 준비되는지"를 알려주는 통지 메커니즘(멀티플렉싱)과 결합해야 실용적이다.
- **내 프로젝트**: Winters 서버는 blocking/non-blocking 폴링 어느 쪽도 아닌 **비동기(Overlapped) + 완료 통지(IOCP)** 모델이다. 소켓 생성부터 `WSASocketW(..., WSA_FLAG_OVERLAPPED)`로 연다(`IOCPCore.cpp`).

### 9. IO 멀티플렉싱 발전사 — select → WSAEventSelect → Overlapped → IOCP

| 모델 | 통지 방식 | 한계 |
|---|---|---|
| `select` | 준비(readiness) 기반, fd_set 폴링 | FD_SETSIZE(64/1024) 제한, 매 호출마다 셋 복사 + 전체 스캔 O(n) |
| `WSAEventSelect` | 소켓→이벤트 객체 연결, `WaitForMultipleObjects` | 이벤트 64개 제한 → 64개마다 스레드 증설, 준비 통지일 뿐 데이터 복사는 별도 |
| Overlapped(콜백/이벤트) | **완료(completion)** 기반 — OS가 유저 버퍼에 직접 채워놓고 알림 | APC 콜백은 alertable wait 필요, 발행 스레드에 묶임, 스레드 분배 제어 불가 |
| **IOCP** | 완료 기반 + **완료 큐 + 워커 스레드 풀** | 구조 복잡성(버퍼/컨텍스트 수명 관리)을 개발자가 짊어짐 |

- **준비 vs 완료의 본질 차이**: select류는 "이제 recv 해도 안 막힘"을 알려줄 뿐 recv(데이터 복사)는 여전히 내 일이다. Overlapped/IOCP는 "이미 네 버퍼에 복사 끝났음"을 알려준다 — 시스템 콜 횟수와 복사 시점 제어에서 이득.
- **왜 IOCP인가**: ① 소켓 수 제한 없음 ② 완료 큐 하나에 워커 N개가 붙는 자연스러운 스레드 풀 ③ OS가 동시 실행 워커 수를 CPU 코어에 맞게 조절(`CreateIoCompletionPort`의 concurrency 인자) ④ LIFO 웨이크업으로 캐시 친화적. Windows 고성능 서버의 사실상 표준.

### 10. IOCP 구조 — 내 서버 코드 기준

`Server/Private/Network/IOCPCore.cpp`의 실제 골격:

1. **포트 생성**: `CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, m_workerCount)` — 완료 큐 생성, 동시 실행 스레드 힌트 = 워커 수.
2. **소켓 연계**: 리슨 소켓과 각 세션 소켓을 `CreateIoCompletionPort(socket, m_hIOCP, key, 0)`로 큐에 결합. Winters는 completion key에 **sessionId**를 넣는다(`BindIOCP`).
3. **AcceptEx 선등록**: `WSAIoctl(SIO_GET_EXTENSION_FUNCTION_POINTER)`로 `AcceptEx`/`GetAcceptExSockaddrs` 함수 포인터를 얻고, 시작 시 **AcceptEx 4개를 미리 걸어둔다**. accept 자체도 비동기 완료 이벤트가 되므로 accept 전용 스레드가 필요 없고, 접속 폭주 시에도 대기 소켓이 준비돼 있다.
4. **워커 루프(GQCS)**:
   ```cpp
   const BOOL ok = GetQueuedCompletionStatus(m_hIOCP, &bytes, &key, &pOverlapped, INFINITE);
   IOContext* ctx = CONTAINING_RECORD(pOverlapped, IOContext, overlapped);
   switch (ctx->op) { case eIOOp::Accept: ... case eIOOp::Recv: ... case eIOOp::Send: ... }
   ```
   OVERLAPPED를 IOContext 구조체의 멤버로 박아두고 `CONTAINING_RECORD`로 역참조 — "OVERLAPPED 확장 구조체" 패턴. op 종류(Accept/Recv/Send)와 sessionId를 함께 복원한다.
5. **종료**: `m_bRunning=false` 후 워커 수만큼 `PostQueuedCompletionStatus(m_hIOCP, 0, 0, nullptr)`로 null 완료를 밀어 GQCS INFINITE 대기를 깨운다. pOverlapped==nullptr 검사로 종료 신호와 실제 IO를 구분.
6. **Accept 완료 후처리**: `SO_UPDATE_ACCEPT_CONTEXT`(AcceptEx로 받은 소켓을 정상 소켓 속성으로 승격 — 이걸 빼먹으면 shutdown/getpeername이 실패하는 유명한 함정) + `TCP_NODELAY` + 세션 생성 + `PostInitialRecv()` + AcceptEx 재등록.

**Overlapped 수명 규칙**: WSARecv/WSASend에 넘긴 OVERLAPPED와 버퍼는 완료 통지가 올 때까지 살아 있어야 한다. Winters `CSession`은 IO를 걸 때 `AddPendingIo()`, 완료 시 `CompletePendingIo()`로 미결 IO를 카운트해 세션 파괴 시점을 보호한다(`Session.cpp`).

**송신 직렬화**: 같은 소켓에 WSASend를 겹쳐 걸면 완료 순서는 보장돼도 커널 버퍼 잠금/부분 전송 처리에서 복잡해지므로, Winters는 **send queue + in-flight 1개** 정책이다. `CSession::Send`는 큐에 push하고 `m_bSendPending`이면 리턴, 완료 핸들러 `OnSendComplete`가 front를 pop하고 다음 것을 체인으로 발행한다. 큐가 락(m_sendMutex)으로 보호되므로 어느 스레드에서 Send를 불러도 안전하다.

**수신 0바이트 = 우아한 종료(FIN)**: `OnRecvComplete`에서 `len == 0`이면 `OnDisconnect()` — TCP에서 recv 0은 상대가 FIN을 보냈다는 뜻이다.

### 11. TCP 스트림 경계 문제와 패킷 프레이밍

- **문제**: TCP는 메시지가 아니라 **바이트 스트림**이다. send를 3번 해도 recv 한 번에 몰려 오거나, 한 메시지가 두 recv에 걸쳐 쪼개져 온다. "패킷 = recv 1회"라는 가정은 반드시 깨진다(특히 실배포/WAN에서).
- **해법 — 길이 프리픽스 프레이밍**: 고정 크기 헤더에 페이로드 길이를 넣고, 수신 버퍼에 누적(append)하다가 [헤더 + 길이]만큼 모이면 한 프레임을 잘라낸다.
- **내 프로젝트**: `Shared/Network/PacketEnvelope.h`의 16바이트 고정 헤더:
  ```cpp
  struct PacketHeader {
      uint16_t magic = kPacketMagic;      // 0x5742 'WB'
      uint16_t version = kPacketVersion;  // 1
      uint16_t type;                      // ePacketType (CommandBatch/Snapshot/Event/Hello/Heartbeat/...)
      uint16_t flags;
      uint32_t payloadSize;
      uint32_t sequence;
  };
  static_assert(sizeof(PacketHeader) == 16, "PacketHeader must be 16 bytes for wire stability.");
  ```
  `Server/Private/Network/FrameParser.cpp::TryPop`이 상태 머신: 헤더 미달이면 `NeedMore`, magic/version 불일치 또는 `payloadSize > 64KB`면 `Invalid`(버퍼 전체 Clear — 오염된 스트림에서 재동기화를 시도하지 않고 끊는 보수적 정책), 전체 프레임이 모이면 `Complete`로 잘라 반환. 누적 버퍼 자체도 256KB 상한(`kMaxBufferBytes`)으로 악의적/버그성 무한 성장 방어.
- **static_assert의 의미**: 헤더 크기가 컴파일러/패킹 설정에 따라 흔들리면 와이어 포맷이 깨진다. `#pragma pack` + `static_assert(==16)`으로 ABI를 컴파일 타임에 못박았다 — "와이어 포맷은 계약"이라는 답변 포인트.

### 12. 직렬화 — FlatBuffers를 쓰는 이유와 verify 규율

- **후보 비교**: JSON(가독성, 느림/큼) / Protobuf(컴팩트, 접근 시 디코딩 필요) / **FlatBuffers**(zero-copy — 수신 버퍼 위에서 포인터 오프셋으로 바로 필드 접근, 파싱 단계 없음).
- **왜 FlatBuffers인가**: 30Hz 스냅샷은 매 틱 생성/소비된다. 틱마다 전체 역직렬화 없이 `GetSnapshot(payload)->entities()`로 즉시 순회할 수 있는 zero-copy가 CPU 예산에 직결. 스키마 진화(필드 추가 시 하위 호환)도 지원.
- **내 프로젝트**: 서버 `SnapshotBuilder::Build`가 `flatbuffers::FlatBufferBuilder fbb(2048)`로 엔티티별 `EntitySnapshot`(위치/yaw/HP/쿨다운/인벤토리/AI 디버그까지 ~90필드)을 만들고 `fbb.Release()`로 DetachedBuffer를 송신 경로에 넘긴다. 클라 `SnapshotApplier::OnSnapshot`은 적용 전에 반드시 검증한다:
  ```cpp
  flatbuffers::Verifier verifier(payload, len);
  if (!Shared::Schema::VerifySnapshotBuffer(verifier)) { /* bounded log 후 return */ }
  ```
- **verify 실패를 침묵 drop하면 안 되는 이유**: FlatBuffers는 zero-copy라서 검증 없이 접근하면 조작된 오프셋으로 **out-of-bounds 읽기**가 난다. 그래서 verify는 필수인데, 실패를 로그 없이 return하면 **스키마 드리프트(서버/클라 스키마 불일치)가 네트워크 정지와 구분 불가능한 "조용한 월드 프리즈"**로 나타난다. 실제로 이 팀 규칙을 gotcha로 박제했고, Winters의 모든 replication 경계(Snapshot/Hello/Event — `SnapshotApplier.cpp`, `EventApplier.cpp`)는 `s_...VerifyFailLogCount < 8` 형태의 **bounded trace**(스팸 방지 위해 8회 상한)를 남긴다. "침묵 실패 금지 + 로그 폭주 금지"를 동시에 만족하는 패턴.

### 13. 게임 동기화 아키텍처 — 권위 모델

- **서버 권위(Server-Authoritative)**: 시뮬레이션의 유일한 진실은 서버. 클라는 입력(명령)만 보내고 상태(스냅샷)를 받는다. 치트 내성 최고, 서버 비용과 지연 체감(예측으로 상쇄)이 대가. MOBA/MMO/FPS 표준.
- **P2P 락스텝(Deterministic Lockstep)**: 모든 피어가 **입력만** 교환하고 각자 동일한 결정론적 시뮬레이션을 돌린다. 대역폭 극소(RTS 유닛 수천 개도 입력만), 대신 ① 완전한 결정론 필수(부동소수점/순회 순서까지) ② 가장 느린 피어에 전체가 묶임 ③ 상태를 숨길 수 없어 맵핵에 취약.
- **내 프로젝트**: Winters는 서버 권위. 클라 입력은 `ePacketType::CommandBatch`로 올라가고 서버 `CommandExecutor`가 실행, 결과는 스냅샷으로 내려온다. 흥미로운 점은 **서버 권위인데도 결정론 인프라를 깔았다**는 것 — `DeterministicEntityIterator`로 엔티티 순회 순서 고정, `Server.vcxproj`에 `/fp:precise`, 스냅샷에 `rngState` 포함. 이유: ① 서버 리플레이/리그레션 검증(같은 입력 → byte-exact 스냅샷) ② 미래 롤백/예측 확장 대비. "권위 모델과 결정론은 독립 축"이라는 이해를 보여줄 수 있는 대목.

### 14. 틱레이트와 고정 틱 루프

- **정의**: 서버가 시뮬레이션을 갱신하고 상태를 내보내는 주기. LoL 30Hz, 발로란트 128틱, 오버워치 63틱.
- **트레이드오프**: 틱↑ = 반응성/판정 정밀도↑, CPU/대역폭↑. MOBA는 30Hz면 충분(스킬 판정이 33ms 격자여도 체감 미미), FPS는 헤드샷 판정 때문에 60+.
- **가변 dt가 아니라 고정 dt인 이유**: 물리/게임로직이 dt에 의존하면 프레임레이트가 게임플레이를 바꾼다(재현 불가, 결정론 붕괴). 고정 dt + 시간 누적이 표준.
- **내 프로젝트**: `Server/Private/Game/GameRoomTick.cpp`:
  ```cpp
  auto next = clock::now();
  const auto period = std::chrono::microseconds(33333); // 30Hz
  while (m_bRunning.load(...)) { Tick(); next += period; std::this_thread::sleep_until(next); }
  ```
  `sleep_for(period)`가 아니라 **절대 시각 기반 `sleep_until(next += period)`** — Tick 수행 시간이 주기를 잠식해도 드리프트가 누적되지 않는다. dt는 `DeterministicTime::kFixedDt = 1.f/30.f`로 고정(`Shared/GameSim/Core/Determinism/DeterministicTime.h`), 스킬 지속시간 등은 초가 아니라 틱 단위로 환산해 저장한다. 게임 시간도 `(serverTick * 1000) / kTicksPerSecond`로 틱에서 유도.

### 15. 스냅샷 동기화와 델타 압축

- **스냅샷**: 틱 시점 월드 상태의 스칼라 사본을 통째로 직렬화해 브로드캐스트. 유실에 강하다(다음 스냅샷이 오면 자동 복구) — 상태 기반 동기화의 최대 장점.
- **델타 압축**: 클라가 마지막으로 ACK한 기준(baseline) 스냅샷과의 차이만 전송. Quake 3 네트워킹 모델. 대역폭이 수분의 일로 줄지만, per-client baseline 추적과 "ACK가 한동안 없으면 full 스냅샷 강제" 로직이 필요.
- **내 프로젝트**: 현재는 **매 틱 full 스냅샷**이다(`SnapshotBuilder::Build`가 모든 replicated 엔티티를 netId 오름차순으로 정렬해 담음 — 정렬은 결정론/디프 비교 용이성 목적). LAN MVP + 엔티티 수십 개 규모에서는 단순성이 이긴다고 판단했고, 델타 압축은 UDP 네트스택 플랜(SnapshotEnvelope + ackSeq 기반 baseline)에 다음 단계로 설계돼 있다. 면접에서는 "지금 왜 안 했는지(측정 전 최적화 회피)"와 "어떻게 넣을지(ack 기반 baseline, 필드 단위 dirty bit)"를 모두 말할 수 있어야 한다.

### 16. 관심 영역(AOI, Area of Interest)

- **정의**: 각 클라이언트에게 "그 클라가 알아야 할" 엔티티만 복제하는 필터링. 대역폭 O(n²) 폭발을 막는 MMO 필수 기법.
- **구현 기법**: 그리드 셀(엔티티를 셀에 해싱, 주변 9셀 구독), 시야 반경 + 히스테리시스(경계에서 들락날락 방지), 우선순위 큐(가깝고 중요한 엔티티부터 대역폭 배분 — 오버워치/헤일로 방식).
- **게임 맥락 + 내 프로젝트**: MOBA는 전장이 작아 AOI보다 **시야(Fog of War) 기반 필터링**이 본질이다. Winters 스냅샷에는 `kSnapshotStateInvisibleFlag`(GameplayState의 invisible 플래그를 와이어로 복제)가 있고, 서버 `VisionSensorComponent`/`VisionSourceComponent`로 와드 시야를 서버가 계산한다. "AOI의 목적은 대역폭 절약이지만 시야 필터링의 목적은 **정보 치트 방지**(맵핵 원천 차단 — 서버가 안 보내면 못 본다)"라고 목적을 구분해 답하면 좋다.

### 17. 클라이언트 예측(Prediction)과 서버 리컨실리에이션(Reconciliation)

- **문제**: 서버 권위에서 입력→서버→스냅샷 왕복(RTT+틱 지연)만큼 내 캐릭터 반응이 늦다. 100ms RTT면 이동키가 100ms 늦게 반영 — 조작감 붕괴.
- **예측**: 클라가 자기 입력을 로컬에서 **즉시** 시뮬레이션해 보여준다.
- **리컨실리에이션**: 서버 스냅샷에는 "서버가 어느 입력까지 처리했는지"(last acked input sequence)가 실려 온다. 클라는 그 시점 권위 상태를 받아들인 뒤, **아직 ACK되지 않은 입력들을 다시 적용(re-apply)**해 예측을 재구성한다. 예측이 맞았으면 무보정, 틀렸으면 그 지점부터 교정(스무딩으로 스냅 방지).
- **내 프로젝트 — 실제 구현 사례(yaw 예측 보호)**: Winters 스냅샷은 `lastAckedCommandSeq`와 `yourNetId`를 함께 내려준다(`SnapshotBuilder::Build` 인자, `SnapshotApplier::OnSnapshot`에서 소비). 로컬 챔피언의 우클릭 이동 시 클라가 즉시 몸 방향(yaw)을 돌리는데, 서버가 그 Move를 아직 반영 못 한 스냅샷이 도착하면 옛 yaw로 덮어써 **"휙 돌았다 다시 돌아오는" 회전 팝**이 생겼다. 해법으로 `SnapshotApplier`에 로컬 이동 yaw 보호를 구현했다:
  - 보호 조건: 해당 엔티티가 Hello로 받은 내 netId이고, 서버 yaw가 예측 yaw를 아직 못 따라잡았고(`IsYawClose` 허용오차 0.20 rad), 서버가 액션 락(사망/공격)을 걸지 않았을 때.
  - 해제 조건: 서버 yaw가 따라잡음 / 액션 락 / `lastAckedCommandSeq`가 보호 대상 명령을 커버한 뒤 **유예 스냅샷 12장**(`kLocalMoveYawMaxProtectedSnapshots = 12u`) 경과 / 서버가 정반대(±π) 방향을 고집(반턴 감지 `IsYawHalfTurn`).
  - 시퀀스 비교는 wrap-around 안전하게 `static_cast<i32_t>(lhs - rhs) > 0` (`IsCommandSeqAtLeast`).
  이것이 "위치 전체 예측"은 아니지만, **acked seq 기반으로 로컬 예측과 권위 상태를 병합하는 리컨실리에이션의 축소판**을 직접 짜고 디버깅(양측 YawTrace 로그로 서버/클라 yaw를 틱 단위 대조)한 경험으로 어필한다.

### 18. 엔티티 보간(Interpolation)과 외삽(Extrapolation)

- **보간**: 원격 엔티티를 최신 스냅샷 시각이 아니라 **일부러 과거(보통 스냅샷 간격 2개 ≈ 100ms)** 시점으로 렌더링하고, 버퍼에 쌓인 두 스냅샷 사이를 lerp. 20~30Hz 스냅샷도 부드러운 60fps 움직임이 되고, 스냅샷 1개 유실도 흡수.
- **외삽(dead reckoning)**: 버퍼가 마르면 마지막 속도로 미래를 추정. 방향 전환 시 틀려서 되돌림(rubber-band)이 생기므로 짧게(수십 ms)만 허용.
- **트레이드오프**: 보간 지연만큼 "내가 보는 상대는 과거"다 — 이것이 lag compensation이 필요한 이유로 이어진다(꼬리 연결 포인트).
- **내 프로젝트(정직한 현재 상태)**: 현재 `SnapshotApplier`는 스냅샷 위치를 `tf.SetPosition(snapshotPos)`로 **직접 적용**한다(보간 버퍼 없음). 30Hz 스냅샷을 그대로 찍으면 계단식 움직임이 되는데, 초기에 클라 로컬 내비게이션 시스템이 그 사이를 "몰래" 메꾸다가 스냅샷 yaw를 덮어쓰는 사고가 났고, 팀 규칙으로 "복제 챔피언에 클라 이동 시스템을 다시 켜지 말고 **스냅샷 보간/예측으로 해결하라**"를 박제했다(gotcha 2026-05-22). 즉 보간 버퍼 도입이 명시된 다음 단계다 — 한계와 로드맵을 함께 말하는 것이 오히려 신뢰를 준다.

### 19. Lag Compensation (지연 보상)

- **문제**: 상대는 내 화면에서 보간 지연 + RTT/2만큼 과거에 있다. 내가 "보이는 대로" 쐈는데 서버 현재 상태로 판정하면 항상 빗나간다.
- **해법(서버 되감기)**: 서버가 매 틱 엔티티 히트박스 히스토리를 저장하고, 사격 판정 시 `클라가 본 시점 = 현재 - (RTT/2 + 보간 지연)`으로 **월드를 되감아** 판정한다. 밸브 Source 엔진 방식.
- **부작용과 상한**: 피격자 입장에선 "벽 뒤로 숨었는데 맞음"(피터의 역설). 그래서 되감기 상한을 둔다 — 고핑 유저가 무제한 과거를 우려먹지 못하게.
- **내 프로젝트**: `Server/Public/Security/LagCompensation.h`:
  ```cpp
  static constexpr u64_t kMaxRewindMs = 200;
  static constexpr u64_t kTickRate = 30;
  static constexpr u64_t kMaxRewindTicks = (kMaxRewindMs * kTickRate + 999) / 1000; // = 6틱
  ```
  `RecordHistory(world, tickIndex)`가 틱마다 엔티티 상태를 per-entity `std::deque<HistoryFrame>`에 적재하고, `TryGetHistoricalState(entity, rewindTicks, out)`로 과거 상태를 조회한다. HistoryFrame에 `EntityGeneration`을 함께 저장해 **엔티티 ID 재사용(죽고 다른 엔티티가 같은 ID를 받는 경우)으로 엉뚱한 히스토리를 읽는 버그**를 세대 비교로 차단한 디테일까지 언급 가능. 파일이 `Security/` 폴더에 있는 것 자체가 "lag comp 상한 = 치트 방어"라는 관점을 반영.

### 20. RUDP — UDP 위에 신뢰성 직접 구현

- **개념**: UDP 데이터그램 위에 seq/ack, 선택적 재전송, 순서 복원, 조각화를 **채널별로 선택 가능하게** 얹는 것. ENet, RakNet, 언리얼 NetDriver가 이 계열.
- **핵심 설계 요소**:
  - 패킷마다 sequence, 헤더에 "최신 수신 seq + 이전 32개 수신 비트마스크"(ack bitfield) — ACK 자체의 유실도 견딤.
  - 채널 분리: 이동(unreliable-sequenced: 최신만, 옛것 버림) / 스킬 명령·이벤트(reliable-ordered) / 채팅(reliable-unordered).
  - RTT 추정(지수이동평균) → RTO 산출 → 미ACK 패킷 재전송.
  - MTU 안전 크기 초과 메시지는 messageId + fragment index로 조각/재조립.
- **왜 TCP 재발명이 아닌가**: TCP는 "전부, 순서대로"만 제공한다. 게임은 "이 데이터는 유실돼도 됨(위치), 이건 안 됨(스킬), 이건 순서만 중요(애니 이벤트)"처럼 **메시지 종류별로 다른 계약**이 필요하고, 유실 시 cwnd 붕괴 같은 TCP 정책도 피하고 싶다.
- **내 프로젝트 구현**: `UdpPacketHeader` v3가 connection/generation/type/lane/flags, packetSeq, messageSeq, ackSeq+ackBitfield, payloadSize를 40B wire에 직렬화한다. Control/Lobby/Command/Event와 Snapshot/Heartbeat가 각자의 lane state를 가지며, reliable lane은 최대 32 pending packets·256 KiB, ordered gap buffer와 retry exhaustion을 bounded 처리한다. 큰 payload는 1144B씩 쪼개 reassembly한다. Hello→Retry(cookie)→Connect(ticket)→Accept→Confirm activation barrier와 HMAC-SHA256 cookie도 구현됐다.
- **아직 없는 것**: 위 일반 설계 목록의 RTT 기반 RTO는 현재 코드에 없다. fixed retry timeout이며 pacing/congestion/fast retransmit도 없다. post-handshake packet MAC/AEAD가 없어 on-path 변조/ACK spoofing을 막지 못한다. 따라서 "RUDP를 구현했다"와 "인터넷 production transport를 완성했다"를 구분한다.

### 21. 세션 계층 방어 — 시퀀스 검증, 하트비트

- **재전송/리플레이 방어**: 클라 명령 패킷의 application sequence를 서버가 검증해 중복 실행을 막는다. 현행 공통 소유자는 `CServerSessionHub::TryAcceptCommandSequence`이고 TCP/UDP logical session이 같은 상태를 쓴다. 정책은 다음과 같다:
  ```cpp
  if (seq <= m_lastProcessedSeq) return false;          // 중복/역행 거부
  if (seq > m_lastProcessedSeq + 60) { bSuspicious = true; return false; } // 비정상 점프 격리
  m_lastProcessedSeq = seq; return true;
  ```
  단조 증가 강제 + 윈도우(+60) 밖 점프는 suspicious 플래그로 상위에 보고 — 패킷 조작 클라이언트에 대한 1차 방어선.
- **하트비트**: TCP keepalive 기본값에 의존하지 않는다. UDP peer/client는 마지막 수신 시각을 추적하고 15초 idle timeout을 적용하며 Heartbeat는 unreliable-sequenced lane이다. 정상 종료용 graceful close/close-ACK state machine은 아직 없고 명시적 Disconnect와 idle cleanup의 의미를 더 닫아야 한다.
- **명령 코얼레싱**: 우클릭 스팸 시 펜딩 중인 옛 Move를 최신 Move로 교체해(같은 세션 한정, 비-이동 명령은 보존) 스팸이 서버 틱을 낭비하고 눈에 보이는 조향 턴을 만드는 것을 막았다(gotcha 2026-05-20 박제 규칙).

### 22. HTTP/REST와 게임 백엔드, std::async 함정

- **역할 분리**: 실시간 인게임 = 소켓(TCP/UDP), **계정/로그인/매치메이킹/상점 = HTTP(S) REST**. 상태 비저장, 방화벽 친화, 백엔드를 웹 스택으로 확장 가능.
- **내 프로젝트**: `Client/Private/Network/Backend/CHttpClient.cpp` — WinHTTP 기반. `WinHttpOpen → WinHttpConnect → WinHttpOpenRequest → WinHttpSendRequest → WinHttpReceiveResponse → WinHttpReadData` 흐름, Bearer 토큰 헤더(`SetAuthToken`), 2xx 판정.
- **std::async future를 버리면 동기 블로킹되는 함정**: `std::async(std::launch::async, ...)`가 반환한 `std::future`를 받지 않고 버리면, **임시 future의 소멸자가 작업 완료까지 대기**한다(표준 규정). 즉 "비동기 호출"이 문장 끝에서 조용히 동기 호출이 된다. Winters에서 실제로 겪은 버그이며(gotcha 2026-07-09), 수정 후 구조는:
  - `LaunchAsyncRequest`가 future를 `m_PendingRequests` 벡터에 **소유**시킨다(발행 시 `PruneCompletedRequests`로 `wait_for(0) == ready`인 완료 future 청소).
  - 람다가 `this`를 캡처하므로, **소멸자가 모든 pending future를 drain**(`task.wait()`)해 use-after-free를 차단 — "future 수명 = this 캡처 안전성"이라는 연결 고리가 핵심. 과거에는 '우연히 동기 블로킹이라 raw this가 안전'했던 것을, 명시적 수명 관리로 바꿨다.
  - 워커 스레드는 멤버를 직접 읽지 않고 발행 시점 복사본 `RequestSnapshot`(host/port/basePath/authToken)만 사용해 데이터 레이스 제거.
  - 콜백은 워커에서 직접 실행하지 않고 `m_PendingCallbacks` 큐에 push, 메인 루프의 `ProcessCallbacks()`가 소비 — **"완료는 임의 스레드, 게임 상태 변경은 메인 스레드"** 원칙(IOCP 워커→틱 스레드 관계와 동일한 패턴임을 언급).

### 23. IOCP × 게임 틱 × Fiber JobSystem 통합 (스레딩 모델)

- **구조**: Winters 서버는 socket completion을 처리하는 IOCP workers와 30Hz GameRoom tick owner를 분리한다. UDP `CUdpIocpCore` callback은 `CServerSessionHub`의 bounded ingress에 `(connectionId,generation)`, logical session ID, type/sequence, owned payload를 적재할 뿐 GameRoom을 만지지 않는다. `CGameRoom::Tick`은 state mutex를 잡기 전에 최대 512 events를 drain하고 connect/frame/disconnect를 재검증한 뒤 authority path로 넘긴다.
- **TCP와 UDP의 현재 차이**: 두 transport는 공통 logical session/`SendFrame`/command sequence 상태를 쓰지만, legacy TCP receive는 기존 direct dispatch 경로를 유지한다. UDP만 callback→tick drain single-writer 경계를 강제한다. 이 차이를 숨기지 않고 migration debt로 관리한다.
- **왜 IOCP worker에서 게임 로직을 돌리지 않나**: 게임 월드는 틱 스레드가 단독 변경해야 결정론과 lock-order를 유지한다. IO worker는 socket/ACK/reassembly와 owned frame 생산까지만 한다.
- **Fiber 경계**: Engine FiberFull scheduler 자체는 구현됐다. 그러나 `GetQueuedCompletionStatus`/overlapped I/O를 기다리는 IOCP thread를 fiber화하지 않는다. 서버가 JobSystem을 opt-in하더라도 completion→queue→tick 경계 뒤의 순수 CPU DAG만 job/fiber 후보이며, socket wait·mutex·thread-affine resource를 FiberFull wait 너머로 들고 가지 않는다. main startup 배선과 server-binary FiberFull wait/resume smoke는 완료됐고, 실제 GameRoom CPU DAG jobification은 별도 단계다.

---

## 예상 질문 & 모범답변

### Q1. OSI 7계층과 TCP/IP 4계층의 차이를 설명해보세요.

- **정의/원리**: OSI는 ISO의 개념적 참조 모델로 물리~응용 7계층, TCP/IP는 실제 인터넷 구현 모델로 4계층(네트워크 액세스/인터넷/전송/응용). OSI의 세션·표현·응용이 TCP/IP에선 응용 하나로 합쳐진다. 각 계층은 캡슐화로 자기 헤더를 붙이고, 아래 계층 구현이 바뀌어도 위 계층은 영향받지 않는 책임 분리가 본질.
- **왜/트레이드오프**: OSI는 교육/설계 어휘로 살아남았고 실제 프로토콜 스택은 TCP/IP다. "몇 계층이냐"보다 "경계가 왜 거기 있냐"가 중요 — 전송 계층이 프로세스 간(포트) 통신, 인터넷 계층이 호스트 간(IP) 통신을 책임진다는 구분.
- **내 프로젝트 연결**: Winters로 매핑하면 전송=TCP 소켓(IOCP), 세션=`CSession`(연결 수명·시퀀스 검증), 표현=FlatBuffers 직렬화, 응용=스냅샷/커맨드 프로토콜. 게임 서버 개발은 사실상 5~7계층을 직접 만드는 일이라고 설명한다.
- **꼬리질문 대비**: "L4 로드밸런서와 L7 로드밸런서 차이는?" — L4는 IP:포트 기반 분배(게임 TCP에 적합), L7은 HTTP 내용 기반(REST 백엔드에 적합).

### Q2. TCP와 UDP의 차이는 무엇이고, 게임에서는 무엇을 선택해야 하나요?

- **정의/원리**: TCP = 연결지향 바이트 스트림 + 신뢰성/순서/흐름·혼잡제어. UDP = 비연결 데이터그램, 경계 보존, 아무 보장 없음.
- **왜/트레이드오프**: TCP의 비용은 HOL blocking — 세그먼트 하나가 유실되면 이미 도착한 뒤 데이터도 재전송 완료까지 앱에 안 올라온다. 실시간 게임에서 "오래된 위치의 재전송"은 가치가 없으므로, 위치류는 UDP로 최신만 보내는 게 맞다. 반면 명령/이벤트는 유실되면 안 된다. 장르 기준: 빠른 액션(FPS) = UDP+자체 신뢰성, MOBA/MMO = RUDP 또는 TCP, 턴제/로비 = TCP/HTTP.
- **내 프로젝트 연결**: TCP IOCP + TCP_NODELAY + 30Hz full snapshot이 검증된 기준선이고, UDP v3 core는 lane별 reliability/ACK/fragment/reassembly와 IOCP endpoint까지 구현됐다. 다만 feature-gated main/F5 cutover와 production hardening은 남았다. 따라서 transport 선택은 "TCP냐 UDP냐" 한 단어가 아니라 메시지 의미, 현재 검증 단계, rollback 경로의 문제다.
- **꼬리질문 대비**: "TCP인데 왜 스냅샷에 sequence가 있나?" — 전송 신뢰성과 별개로 애플리케이션 레벨에서 명령 ACK(리컨실리에이션 기준)와 리플레이 방어에 필요하다고 답한다.

### Q3. 3-way handshake 과정을 설명하고, 왜 2-way로는 안 되는지 말해보세요.

- **정의/원리**: SYN(x) → SYN+ACK(y, x+1) → ACK(y+1). 목적은 연결 성립이 아니라 **양방향 초기 시퀀스 번호(ISN)의 상호 확인**이다.
- **왜**: 2-way면 서버는 자기 ISN이 클라에 도달했는지 모른다. 또 네트워크에 떠돌던 옛 SYN이 뒤늦게 도착했을 때 클라의 3번째 ACK가 없으면 서버가 유령 연결을 확립해버린다.
- **내 프로젝트 연결**: TCP handshake 위에 게임 자체 handshake가 또 있다 — Winters는 접속 직후 서버가 Hello(yourNetId/serverTick/dataBuildHash)를 내려주고, 클라 `SnapshotApplier::OnHello`가 데이터 빌드 해시를 대조해 서버/클라 데이터 버전 드리프트를 접속 시점에 가시화한다. "전송 계층 연결"과 "게임 세션 성립"은 다른 단계라는 걸 강조.
- **꼬리질문 대비**: SYN flood — ACK를 안 보내는 half-open 공격, backlog 고갈. 방어는 SYN cookie(상태 저장 없이 ISN에 정보 인코딩), backlog 튜닝. `listen(SOMAXCONN)`의 의미까지.

### Q4. 4-way handshake와 TIME_WAIT은 왜 필요한가요?

- **정의/원리**: FIN→ACK→FIN→ACK. TCP는 전이중이라 각 방향을 독립적으로 닫아야 해서 4단계다(상대가 보낼 게 남았을 수 있어 FIN과 ACK가 분리됨). 능동 종료측은 마지막 ACK 후 2*MSL 동안 TIME_WAIT.
- **왜**: ① 마지막 ACK 유실 시 상대의 FIN 재전송에 응답할 주체 유지 ② 같은 4-tuple 새 연결이 이전 연결의 지연 세그먼트를 받아버리는 혼선 방지.
- **트레이드오프**: TIME_WAIT은 정상 상태지만 서버가 대량으로 능동 종료하면 포트/메모리를 점유한다. 설계적으로 클라가 먼저 끊게(passive close) 만들면 서버측 TIME_WAIT이 줄어든다.
- **꼬리질문 대비**: "TIME_WAIT을 없애면?" — 없애는 게 아니라 재사용 옵션과 종료 방향 설계로 관리한다. RST로 끊으면 TIME_WAIT은 없지만 데이터 유실 위험.

### Q5. 서버 재시작 시 bind가 실패하는 이유와 해결책은?

- **정의/원리**: 이전 프로세스의 연결(또는 리슨 소켓에서 파생된 상태)이 TIME_WAIT에 남아 같은 포트 bind가 `WSAEADDRINUSE`로 실패. 일반 해법은 `SO_REUSEADDR`.
- **왜/트레이드오프**: Windows의 `SO_REUSEADDR`는 유닉스와 의미가 달라, **아예 다른 프로세스가 사용 중인 포트에도 겹쳐 bind를 허용**해 포트 하이재킹(다른 프로세스가 내 게임 포트를 가로채 트래픽 수신) 위험이 있다.
- **내 프로젝트 연결**: 그래서 Winters `IOCPCore.cpp`는 반대로 `SO_EXCLUSIVEADDRUSE`를 켜서 리슨 포트를 배타 점유한다. 게임 서버는 "재시작 편의"보다 "포트 탈취 차단"이 우선이라는 판단. 재시작 문제는 클라 우선 종료 정책과 짧은 재시도 루프로 흡수한다.
- **꼬리질문 대비**: "리눅스라면?" — `SO_REUSEADDR`가 TIME_WAIT 소켓에 한해 재bind 허용이라 안전하게 상용됨. `SO_REUSEPORT`는 멀티 프로세스 로드밸런싱용으로 목적이 다름.

### Q6. 흐름제어와 혼잡제어의 차이를 설명해보세요.

- **정의/원리**: 흐름제어는 수신자 버퍼 보호(수신자가 rwnd를 광고), 혼잡제어는 네트워크 경로 보호(송신자가 cwnd를 추정). 실송신량 = min(rwnd, cwnd).
- **왜**: 보호 대상이 다르므로 메커니즘이 분리된다. rwnd는 상대가 알려주는 사실, cwnd는 유실/ACK 패턴에서 추론하는 추정치.
- **내 프로젝트 연결**: 수신 앱이 멈추면 rwnd→0, 서버 send가 커널 버퍼에서 막히고 Winters `CSession::m_sendQueue`가 자라기 시작한다. "송신 큐 길이 = 클라 수신 건강도 지표"로 모니터링하고, 임계 초과 시 연결을 끊는 게 서버 메모리 보호책이라고 연결한다.
- **꼬리질문 대비**: zero window 상태에서 어떻게 재개되나 — 송신자가 주기적으로 1바이트 window probe를 보낸다.

### Q7. 슬로우 스타트와 AIMD를 설명해보세요.

- **정의/원리**: 슬로우 스타트 — cwnd 1 MSS에서 ACK마다 배증(RTT당 2배)해 가용 대역폭을 빠르게 탐색, ssthresh부터 혼잡 회피. AIMD — 혼잡 회피에서 RTT당 +1 MSS 선형 증가, 유실 시 곱셈 감소(3중복 ACK→절반+빠른 회복, 타임아웃→cwnd=1 재시작).
- **왜**: 곱셈 감소는 혼잡 붕괴를 막는 안전 장치이고, AIMD의 비대칭이 다수 흐름의 대역폭 공평 수렴을 수학적으로 보장한다.
- **게임 연결**: 게임 트래픽은 cwnd를 채우지 않아 평시 무관하지만, **유실 순간 재전송 대기 + cwnd 축소가 겹쳐 지연 스파이크**가 온다. "TCP 게임이 유실에 약한 이유"의 정확한 메커니즘으로 답한다. UDP 자체 프로토콜에서도 무제한 송신은 자기 유실을 부르므로 송신 예산(틱당 바이트 상한, 우선순위 큐) 개념은 필요하다고 덧붙인다.
- **꼬리질문 대비**: CUBIC(RTT 비의존 성장, 리눅스 기본)과 BBR(대역폭·RTT 모델 기반, 유실을 신호로 안 씀) 이름과 한 줄 차이 정도.

### Q8. Nagle 알고리즘은 무엇이고, 게임에서는 왜 끄나요?

- **정의/원리**: 미확인(un-ACKed) 데이터가 있는 동안 작은 송신을 모아 한 세그먼트로 보내는 알고리즘. 텔넷 시대 1바이트 페이로드+40바이트 헤더 낭비 방지가 기원.
- **왜/트레이드오프**: 수신측 Delayed ACK(최대 ~200ms)와 만나면 "데이터는 ACK를 기다리고 ACK는 데이터를 기다리는" 상호 대기가 생겨 소량 요청-응답 트래픽에 최대 200ms 지연. 게임 입력은 작고 즉시성이 생명이라 대역폭 몇 바이트보다 지연이 압도적으로 비싸다.
- **내 프로젝트 연결**: `IOCPCore.cpp`에서 accept 완료 직후 세션 소켓에 `TCP_NODELAY`를 설정한다. 대신 병합 책임을 앱이 진다 — 스냅샷은 틱당 1회 배칭, 이동 명령은 펜딩 큐에서 최신 Move로 교체(coalescing)해 소패킷 난사를 애플리케이션 정책으로 방지.
- **꼬리질문 대비**: "NODELAY를 켜면 항상 빨라지나?" — 소패킷 난사 코드에서는 오히려 패킷 수 폭증으로 손해. 배칭이 전제된 코드에서만 순이익.

### Q9. MTU와 단편화를 설명하고, 게임 패킷 크기를 어떻게 정해야 하는지 말해보세요.

- **정의/원리**: MTU = 링크가 나를 수 있는 최대 IP 패킷(이더넷 1500). 초과 시 IP 단편화 — 조각 중 하나만 유실돼도 전체 재조립 실패라 유실률이 증폭된다. TCP는 MSS 협상(1460)으로 회피, UDP는 개발자 책임.
- **왜**: PMTUD(경로 MTU 탐색)는 ICMP 의존이라 방화벽 환경에서 블랙홀이 나기도 한다. 그래서 UDP 게임 프로토콜은 안전 마진을 두고 ~1200바이트 이하로 데이터그램을 자르고, 큰 메시지는 앱 계층 조각화로 처리한다.
- **내 프로젝트 연결**: TCP는 앱 frame 64 KiB 상한 뒤 세그먼트화를 TCP에 맡긴다. UDP v3는 1200B datagram/40B packet header/16B fragment header/1144B fragment data와 64 KiB·64-fragment cap을 코드로 강제한다. IP fragmentation 대신 app reassembly 책임, timeout/memory cap/duplicate 검증이 새로 생겼다는 점까지 말한다.
- **꼬리질문 대비**: "스냅샷이 1200바이트를 넘으면?" — 조각화 + 유실 시 전체 스냅샷 폐기(다음 스냅샷 대기) 또는 델타로 크기 자체를 줄이는 게 정석.

### Q10. blocking과 non-blocking 소켓의 차이, 그리고 동기/비동기와의 관계는?

- **정의/원리**: blocking recv는 데이터가 올 때까지 스레드 정지, non-blocking은 즉시 WSAEWOULDBLOCK 반환. 이것은 "호출이 기다리느냐" 축이고, 동기/비동기는 "IO 완료(버퍼 복사)를 누가 하느냐" 축이다. select+non-blocking recv는 **동기**(복사는 여전히 내 스레드가 recv로 수행), Overlapped/IOCP는 **비동기**(커널이 내 버퍼를 채워놓고 완료 통지).
- **왜**: 이 2축 구분(readiness vs completion)이 IO 모델 질문의 핵심 채점 포인트다.
- **내 프로젝트 연결**: Winters는 `WSA_FLAG_OVERLAPPED` 소켓 + `WSARecv`에 버퍼를 미리 등록하는 완전 비동기 모델. 워커는 GQCS에서 "이미 채워진 버퍼"를 받아 파싱만 한다.
- **꼬리질문 대비**: "리눅스 epoll은 어느 쪽?" — readiness 기반(동기). completion 기반 대응물은 io_uring. IOCP↔io_uring 대비를 한 줄 하면 가산점.

### Q11. select부터 IOCP까지, Windows IO 멀티플렉싱의 발전 과정을 설명해보세요.

- **정의/원리**: select(fd_set 폴링, 64개 제한, O(n) 스캔) → WSAEventSelect(이벤트 객체, WaitForMultipleObjects 64개 제한 → 64개당 스레드 증설) → Overlapped IO(완료 기반, 그러나 APC/이벤트 통지는 발행 스레드에 종속) → IOCP(완료 큐 + 워커 풀, 소켓 수 무제한, OS가 동시성 관리).
- **왜 IOCP인가**: 통지 구조가 곧 스레딩 모델이다. IOCP는 "완료들을 하나의 큐에 모으고 코어 수만큼의 워커가 꺼내 쓰는" 구조를 OS가 제공해, 수천 세션을 스레드 몇 개로 감당한다. LIFO 웨이크업으로 뜨거운 스레드를 재사용해 캐시에도 유리.
- **내 프로젝트 연결**: `CIOCPCore::Start`에서 `CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, m_workerCount)`로 큐를 만들고 워커 스레드를 띄우며, 각 소켓을 completion key=sessionId로 큐에 결합한다. 이전 프로젝트(SR_MinecraftDungeons 서버)에서도 TCP IOCP를 썼고 Winters에서 AcceptEx까지 확장했다.
- **꼬리질문 대비**: "워커 수는 몇 개가 적당한가?" — 통상 코어 수(순수 IO면 코어 수, 완료 핸들러가 블로킹 작업을 하면 코어×2 등). concurrency 힌트와 실제 스레드 수의 차이(대기 진입 시 다른 워커를 깨워 동시성 유지)를 설명하면 깊이가 드러난다.

### Q12. IOCP의 동작 원리를 GetQueuedCompletionStatus 워커 모델 중심으로 설명해보세요.

- **정의/원리**: ① 소켓/파일 핸들을 완료 포트에 연계 ② WSARecv/WSASend/AcceptEx를 OVERLAPPED와 함께 발행(즉시 리턴, WSA_IO_PENDING) ③ 커널이 IO를 끝내면 완료 패킷(전송 바이트, completion key, OVERLAPPED*)을 큐에 넣음 ④ 워커들이 GQCS로 꺼내 처리.
- **원리의 핵심**: OVERLAPPED 포인터가 "어느 IO였는지"의 유일한 식별자다. 그래서 OVERLAPPED를 확장 구조체(IOContext: op 종류, 버퍼, sessionId)에 내장하고 `CONTAINING_RECORD`로 복원한다.
- **내 프로젝트 연결**: `CIOCPCore::WorkerLoop`가 정확히 이 구조다. GQCS INFINITE → `CONTAINING_RECORD(pOverlapped, IOContext, overlapped)` → op switch(Accept/Recv/Send). 종료는 워커 수만큼 `PostQueuedCompletionStatus(..., nullptr)`를 밀어 nullptr 검사로 루프 탈출 — "INFINITE 대기를 어떻게 깨우나"까지 구현했다. GQCS가 FALSE를 반환하는 경우(연결 리셋 등)도 ctx->op별로 분기해 세션 정리로 라우팅한다.
- **꼬리질문 대비**: "GQCS FALSE + pOverlapped != nullptr의 의미는?" — IO 자체가 실패로 완료된 것(원격 리셋 등). FALSE + nullptr은 GQCS 호출 자체 실패/타임아웃. 이 구분을 아는지로 실구현 여부가 갈린다.

### Q13. accept도 비동기로 처리하나요? AcceptEx는 왜 쓰나요?

- **정의/원리**: 고전 accept는 블로킹이라 accept 전용 스레드가 필요하다. AcceptEx는 "미리 만들어 둔 소켓"에 연결을 받아 IOCP 완료로 알려주는 비동기 accept. `WSAIoctl(SIO_GET_EXTENSION_FUNCTION_POINTER)`로 런타임에 함수 포인터를 얻어 쓴다(Mswsock 확장).
- **왜**: accept까지 완료 큐로 일원화하면 스레딩 모델이 단순해지고, 여러 개를 선등록해 접속 폭주에 대비할 수 있다.
- **내 프로젝트 연결**: `CIOCPCore::Start`가 AcceptEx/GetAcceptExSockaddrs 포인터를 로드하고 시작 시 4개를 선등록, Accept 완료 시마다 재등록한다. 완료 직후 `SO_UPDATE_ACCEPT_CONTEXT`를 호출하는데, 이걸 빠뜨리면 AcceptEx로 받은 소켓이 반쪽짜리(일부 소켓 API 실패)가 되는 유명한 함정까지 처리했다.
- **꼬리질문 대비**: "AcceptEx 선등록 개수가 모자라면?" — 순간적으로 accept 불가 → backlog에 쌓이다 SYN 드랍. 접속 스파이크 프로파일에 맞춰 개수를 늘리거나 완료 시 즉시 재등록(현 구조)으로 회전율을 유지.

### Q14. Overlapped IO에서 버퍼와 OVERLAPPED 구조체의 수명은 어떻게 관리하나요?

- **정의/원리**: 발행한 IO가 완료(또는 취소)될 때까지 OVERLAPPED와 데이터 버퍼는 유효해야 한다. 세션 객체가 이들을 소유하므로, **미결 IO가 남아 있는 동안 세션을 파괴하면 커널이 해제된 메모리에 쓴다** — 전형적 UAF.
- **해법**: 미결 IO 참조 카운트. IO 발행 시 +1, 완료 처리 시 -1, 0이 되고 closing일 때만 파괴.
- **내 프로젝트 연결**: `CSession`이 `AddPendingIo()/CompletePendingIo()` 쌍으로 이를 구현하고, 발행 실패(WSA_IO_PENDING가 아닌 에러) 경로에서도 반드시 CompletePendingIo로 롤백한다. 세션 수명 자체는 `std::shared_ptr<CSession>`(팩토리 `CSession::Create`)로 관리해 매니저/디스패처 간 소유를 안전하게 했다. recv 컨텍스트와 send 컨텍스트를 분리(m_recvContext/m_sendContext)해 동시 발행 시 OVERLAPPED 재사용 충돌을 원천 차단.
- **꼬리질문 대비**: "닫는 중 WSARecv가 또 들어오면?" — `m_bClosing` atomic 플래그를 발행 전 검사(`PostRecv` 첫 줄). 취소는 closesocket이 미결 IO를 에러 완료로 밀어내는 것으로 수렴시킨다.

### Q15. 하나의 소켓에 WSASend를 연속으로 여러 번 걸어도 되나요?

- **정의/원리**: 걸 수는 있지만 부분 완료/버퍼 잠금/완료 역전 처리의 복잡도가 급증한다. 표준 패턴은 **송신 큐 + in-flight 1개**: 새 송신은 큐에 넣고, 진행 중인 send가 없을 때만 발행, 완료 핸들러가 다음 것을 체인.
- **왜**: 순서 보장과 수명 관리가 단순해지고, 소켓당 커널 락 경합도 줄어든다. 트레이드오프는 체인 지연(완료→다음 발행)이지만 게임 패킷 크기에선 무시 가능.
- **내 프로젝트 연결**: `CSession::Send`가 정확히 이 구조 — `m_sendMutex`로 큐 보호, `m_bSendPending`으로 in-flight 표시, `OnSendComplete`가 pop 후 다음 front를 발행. 발행 실패 시 pending 플래그/카운트를 롤백하고 std::cerr로 wsa 에러 코드를 남긴 뒤 연결 정리(에러를 침묵시키지 않는 팀 규칙 준수).
- **꼬리질문 대비**: "send 큐가 무한히 자라면?" — 수신 느린 클라(rwnd 0) 시그널. 큐 바이트 상한을 두고 초과 시 강제 disconnect가 서버 보호의 정석.

### Q16. TCP는 왜 "패킷 경계"가 없고, 어떻게 해결하나요?

- **정의/원리**: TCP는 바이트 스트림 추상화라 send 단위와 recv 단위가 무관하다. MSS 분할, Nagle 병합, 재전송 병합 등으로 경계는 반드시 뭉개진다. 해법은 애플리케이션 프레이밍 — 길이 프리픽스가 표준: 고정 헤더에 payload 길이를 담고, 수신 버퍼에 누적하다 완성분만 잘라낸다.
- **왜**: 구분자 방식(개행 등)은 바이너리에 부적합하고 이스케이프 비용이 있다. 길이 프리픽스는 O(1) 판정.
- **내 프로젝트 연결**: `CFrameParser::Append`(누적) + `TryPop`(상태 판정: NeedMore/Complete/Invalid) 구조. `CSession::OnRecvComplete`가 수신 바이트를 parser에 Append하고 `CPacketDispatcher::DrainFrames`가 완성 프레임을 모두 빼서 디스패치한다 — "recv 1회 = 프레임 0~N개"를 자연스럽게 처리. 로컬 테스트에서는 경계가 안 깨져 버그가 숨는다는 것까지 언급(그래서 처음부터 누적 파서로 갔다).
- **꼬리질문 대비**: "길이 필드 자체가 쪼개져 오면?" — `m_Buffer.size() < sizeof(PacketHeader)`면 NeedMore로 대기, 정확히 처리된다. 이 케이스를 놓친 구현이 실무에서 가장 흔한 사고.

### Q17. 프레이밍에서 악의적이거나 깨진 헤더는 어떻게 방어하나요?

- **정의/원리**: 길이 프리픽스는 신뢰할 수 없는 입력이다. 조작된 payloadSize(4GB)로 메모리 폭탄, 쓰레기 스트림으로 파서 상태 오염이 가능하다. 방어: magic/version 검사, payload 상한, 누적 버퍼 상한, 오염 시 재동기화 시도 대신 연결 폐기.
- **내 프로젝트 연결**: `PacketHeader`에 magic 0x5742('WB')+version, `TryPop`이 `magic/version 불일치 또는 payloadSize > 64KB → Clear() 후 Invalid` 반환. 누적 버퍼도 256KB 상한에서 강제 Clear. `static_assert(sizeof(PacketHeader) == 16)`으로 와이어 크기를 컴파일 타임 고정 — 구조체 패킹이 플랫폼/옵션 따라 흔들려 와이어가 깨지는 사고를 원천 차단.
- **왜 재동기화가 아니라 Clear인가**: 스트림 중간에서 magic을 다시 찾는 건 오탐 위험이 있고, 오염된 세션은 이미 신뢰 불가이므로 버리고 끊는 게 보안상 단순·안전하다.
- **꼬리질문 대비**: "버전 필드는 왜?" — 프로토콜 진화 시 구버전 클라를 접속 즉시 식별/거부. Winters는 헤더 version + Hello의 dataBuildHash 2단으로 전송 포맷과 게임 데이터 버전을 각각 검증한다.

### Q18. 직렬화 포맷으로 FlatBuffers를 선택한 이유는? (vs JSON, Protobuf)

- **정의/원리**: JSON은 텍스트(크고 느림, 디버깅 편함), Protobuf는 바이너리지만 접근 전 디코드 필요, FlatBuffers는 **zero-copy** — 버퍼 자체가 자료구조라 파싱 단계 없이 오프셋 점프로 필드를 읽는다.
- **왜**: 30Hz × 전 엔티티 스냅샷을 매 틱 만들고 소비하는 워크로드에서 역직렬화 비용 제거가 CPU 예산에 직결된다. 스키마 기반이라 서버/클라 간 타입 계약이 코드젠으로 강제되고, 필드 추가의 하위 호환도 지원.
- **내 프로젝트 연결**: 스키마는 `Shared/Schemas`에서 코드젠(`Snapshot_generated.h`), 서버 `SnapshotBuilder::Build`가 `FlatBufferBuilder`(초기 2048바이트)로 빌드 후 `Release()`한 DetachedBuffer를 그대로 send, 클라는 수신 버퍼 위에서 `GetSnapshot(payload)`로 바로 순회한다. 복사 최소화가 설계 전반에 일관됨.
- **꼬리질문 대비**: "단점은?" — 빌더 API가 번거롭고(offset 선생성), mutation이 제한적이며, 필드가 많으면 vtable 오버헤드 존재. 그리고 zero-copy이기 때문에 **Verifier 없이는 신뢰 불가 입력에 절대 접근하면 안 된다**는 규율이 따라온다(다음 질문으로 연결).

### Q19. FlatBuffers verify 실패를 조용히 drop하면 왜 안 되나요?

- **정의/원리**: verify는 버퍼 내 오프셋들이 경계 안에 있는지 검사한다(zero-copy라서 미검증 접근 = OOB 읽기). 그런데 실패를 로그 없이 return하면, 스키마 드리프트(서버만 재빌드 등)가 **"패킷은 오는데 월드가 안 움직이는" 침묵 프리즈**로 나타나 네트워크 장애와 구분이 불가능해진다.
- **왜**: 실패 경로 진단은 관측 가능해야 한다. 단, 매 틱 실패하면 로그 폭주가 되므로 bounded여야 한다.
- **내 프로젝트 연결**: 실제 팀 규칙으로 박제된 사항이다(gotcha 2026-07-09 "replication-boundary verify 실패는 bounded trace/counter 필수"). `SnapshotApplier.cpp`/`EventApplier.cpp`의 모든 verify 실패 경로는 `static u32_t s_...FailLogCount < 8` 가드로 최대 8회만 `OutputDebugStringA`를 남긴다. "침묵 실패 금지"와 "로그 스팸 금지"를 동시에 만족.
- **꼬리질문 대비**: "로그 8회 이후엔?" — 카운터는 계속 증가하므로 디버그 UI/카운터로 노출하면 된다. 근본 해결은 접속 시점 버전 핸드셰이크(Hello dataBuildHash)로 드리프트를 사전 차단하는 것 — Winters는 둘 다 구현.

### Q20. 서버 권위 모델과 P2P 락스텝을 비교하고 장르별 선택 기준을 말해보세요.

- **정의/원리**: 서버 권위 — 서버가 유일 시뮬레이터, 클라는 입력 송신+상태 수신+예측 표시. 락스텝 — 전 피어가 입력만 교환하고 각자 동일 결정론 시뮬레이션(입력 동기화).
- **트레이드오프**: 락스텝은 대역폭 극소·상태 전송 불필요(RTS 수천 유닛 가능)지만, 완전 결정론 강제(부동소수점·순회 순서·RNG), 최저 사양/최고 핑 피어에 전체 속도 종속, 전체 상태 공유라 맵핵 취약. 서버 권위는 치트 내성·유실 복원력(상태 재전송) 대신 서버 비용과 예측 복잡도.
- **내 프로젝트 연결**: Winters는 MOBA라 서버 권위 선택. 그러나 결정론 인프라(고정 틱 dt, `DeterministicEntityIterator` 정렬 순회, `/fp:precise`, 스냅샷 내 rngState)를 함께 깔았다 — 목적은 락스텝이 아니라 **리플레이/회귀 검증(같은 입력 → byte-exact 스냅샷)과 미래 롤백 확장**. "권위 모델 선택과 결정론 확보는 별개 축"이라는 프레임으로 답한다.
- **꼬리질문 대비**: "격투 게임은?" — 롤백 넷코드(GGPO): 락스텝+예측+미스매치 시 과거로 롤백 후 재시뮬. 결정론이 필수라는 점에서 락스텝의 진화형.

### Q21. 틱레이트는 어떻게 정하나요? 서버 틱 루프를 어떻게 구현했나요?

- **정의/원리**: 틱레이트 = 시뮬레이션·복제 주기. 판정 정밀도/반응성 vs CPU·대역폭의 트레이드오프. MOBA 30Hz, FPS 60~128. 루프는 고정 dt가 원칙 — 가변 dt는 물리/판정을 프레임레이트 의존으로 만들어 재현성을 파괴한다.
- **내 프로젝트 연결**: `GameRoomTick.cpp`의 틱 스레드가 `period = microseconds(33333)`(30Hz)로 돌고, `next += period; sleep_until(next)` — **절대 시각 스케줄링**이라 Tick 소요 시간이 들쭉해도 주기 드리프트가 누적되지 않는다(sleep_for였다면 매 틱 오차 누적). dt는 `DeterministicTime::kFixedDt = 1/30`로 하드코딩, 게임 내 시간·쿨다운·지속시간 전부 틱 단위로 환산 저장(초→틱 변환 시 ceil+최소 1틱 보정)해 부동소수 누적 오차를 차단.
- **트레이드오프 언급**: 틱이 주기를 초과하면(과부하) sleep_until이 즉시 리턴하며 따라잡기를 시도한다 — 지속 초과 시 death spiral이므로 틱 예산 프로파일링이 필수라고 덧붙인다.
- **꼬리질문 대비**: "클라 렌더는 144fps인데 서버는 30Hz면?" — 그래서 보간/예측이 존재한다(Q23, Q24로 연결되는 구조로 답변 설계).

### Q22. 스냅샷 동기화 방식을 설명하고, 델타 압축은 어떻게 하는지 말해보세요.

- **정의/원리**: 스냅샷 = 틱 시점 복제 대상 상태 전체를 직렬화해 브로드캐스트. 상태 기반이라 유실 복원력이 최고(다음 스냅샷이 곧 복구). 델타 압축 = 클라별 마지막 ACK 스냅샷(baseline) 대비 변경분만 전송 — Quake3 모델. per-client baseline 관리와 장기 미ACK 시 full 스냅샷 폴백이 필요.
- **내 프로젝트 연결**: `SnapshotBuilder::Build(...)`가 매 틱 full snapshot을 만들고 netId 정렬로 결정론/byte diff를 돕는다. UDP transport는 이 22 KiB급 payload도 fragment할 수 있지만 replay 표본 p95가 18 datagrams이므로 확장 해법이 아니다. acked baseline delta, AOI, quantization/field packing을 적용해 p95 4 datagrams(약 4.6 KiB payload) 이하를 다음 data-plane 목표로 둔다.
- **트레이드오프**: full 스냅샷은 단순+무상태(클라별 추적 불필요+유실 무관)지만 대역폭 O(엔티티 수). 델타는 그 반대.
- **꼬리질문 대비**: "델타 기준 스냅샷이 유실되면?" — baseline은 "클라가 ACK한 것"만 쓰므로 서버가 유실을 이미 안다. ACK 없으면 오래된 baseline 또는 full로 강등.

### Q23. 클라이언트 예측과 서버 리컨실리에이션을 설명해보세요.

- **정의/원리**: 예측 — 내 입력을 로컬에서 즉시 시뮬레이션해 반응 지연(RTT+틱)을 숨김. 리컨실리에이션 — 스냅샷에 실린 "서버가 처리 완료한 마지막 입력 seq"를 기준으로 권위 상태를 수용한 뒤, 미ACK 입력을 재적용해 예측을 재구성. 불일치 시 그 지점부터 교정(스무딩).
- **왜**: 권위는 서버에 두되 체감은 로컬로 — 치트 방지와 조작감의 양립 장치.
- **내 프로젝트 연결**: 와이어에 필요한 3요소가 모두 있다 — 클라 명령 sequence, 스냅샷의 `lastAckedCommandSeq`, `yourNetId`(내 엔티티 식별). 실제 적용 사례가 이동 yaw 예측 보호다: 우클릭 즉시 로컬 yaw를 돌리고, `SnapshotApplier`가 "서버 yaw가 예측을 따라잡거나(허용오차 0.2rad), 액션 락이 걸리거나, ACK 후 유예 12스냅샷이 지나기 전"까지 서버 yaw 덮어쓰기를 보류한다. 이 과정에서 "ack가 왔다고 서버 상태가 예측을 반영했다는 뜻은 아니다(처리·전파 지연 존재)"는 것을 배웠고, 그래서 ack + 유예 카운트 + 실제 값 수렴의 3중 조건으로 해제한다 — 교과서 리컨실리에이션을 그대로 믿다 깨진 실전 경험으로 답변.
- **꼬리질문 대비**: "예측이 크게 틀리면?" — 스냅 대신 수 프레임 스무딩, 단 사망/텔레포트류는 즉시 스냅. seq 비교는 wrap-around 안전 비교(`static_cast<i32_t>(lhs - rhs) > 0`)를 쓴다는 디테일까지.

### Q24. 엔티티 보간은 왜 필요하고 어떻게 구현하나요?

- **정의/원리**: 원격 엔티티를 스냅샷 2개 이상 버퍼링하고 **의도적으로 과거 시점(스냅샷 간격의 1~2배, 30Hz면 ~66-100ms)**을 렌더링해 두 스냅샷 사이를 lerp. 30Hz 데이터가 144fps에서도 부드럽고, 1개 유실도 흡수. 버퍼 고갈 시에만 짧은 외삽(dead reckoning).
- **왜/트레이드오프**: 부드러움의 대가는 "내 화면의 상대는 과거"라는 지연 — 이것이 서버 lag compensation의 존재 이유가 된다(연결해서 답하면 이해도가 드러남).
- **내 프로젝트 연결(정직하게)**: 현재 클라는 스냅샷 위치를 직접 SetPosition하는 단계다. 초기에 클라 로컬 내비게이션이 스냅샷 사이를 메꾸다 서버 적용 yaw를 덮어쓰는 사고를 겪고, "복제 엔티티에 클라 이동 시스템 재활성화 금지, 계단 현상은 스냅샷 보간으로 풀 것"을 팀 규칙으로 박제했다. 즉 보간 버퍼(서버 틱 타임스탬프 기반 — 스냅샷에 serverTick/serverTimeMs가 이미 실려 있음)가 설계된 다음 단계다.
- **꼬리질문 대비**: "보간 시간은 뭘 기준으로?" — 클라 로컬 시계가 아니라 **서버 틱 시계**에 동기화한 렌더 타임라인(serverTimeMs 기반)이어야 지터에 안정적.

### Q25. Lag Compensation(지연 보상)을 설명해보세요. 부작용은요?

- **정의/원리**: 서버가 히트박스 히스토리를 틱 단위로 보관하다가, 사격/스킬 판정 시 공격자가 **화면에서 본 시점**(현재 - RTT/2 - 보간지연)으로 월드를 되감아 판정. "보이는 대로 맞는" 경험을 만든다.
- **부작용**: 피격자 시점에선 이미 엄폐했는데 맞는 "behind cover death". 되감기 무제한이면 고핑이 유리해지므로 상한이 필수.
- **내 프로젝트 연결**: `Server/Public/Security/LagCompensation.h` — `kMaxRewindMs = 200`, 30틱 기준 `kMaxRewindTicks = 6`으로 상한. `RecordHistory`가 매 틱 per-entity deque에 상태를 쌓고 `TryGetHistoricalState(entity, rewindTicks, out)`로 조회. 프레임에 `EntityGeneration`을 함께 저장해 엔티티 ID 재사용 시 죽은 엔티티의 히스토리를 새 엔티티로 오인하는 버그를 세대 비교로 차단했다. 파일 위치가 `Security/`인 것도 의도 — 상한 없는 되감기는 치트 벡터다.
- **꼬리질문 대비**: "MOBA에서도 필요한가?" — 히트스캔 중심 FPS보다 덜 절실하지만, 클릭 타겟팅(내 화면의 적을 클릭)과 스킬샷 판정의 공정성에 같은 원리가 적용된다.

### Q26. UDP 위에 신뢰성을 직접 구현한다면 어떻게 설계하겠습니까? (RUDP)

- **정의/원리**: 패킷 seq + ack(최신 수신 seq) + ack bitfield(직전 32개 수신 여부)로 유실 감지, RTT 추정→RTO로 재전송, 채널별 정책 분리(unreliable-sequenced 이동 / reliable-ordered 명령 / reliable-unordered 채팅), MTU 초과분은 messageId+fragment index 조각화.
- **왜**: TCP는 "전부·순서대로" 단일 계약뿐이고 유실 시 cwnd 붕괴·HOL이 따라온다. 게임은 메시지 종류마다 다른 신뢰성 계약이 필요하다. 핵심 통찰: **오래된 상태는 재전송하지 말고 최신 상태를 새로 보내라** — 신뢰성이 필요한 건 상태가 아니라 이벤트다.
- **내 프로젝트 연결**: Shared UDP v3 header/codec, per-lane ACK/reliability/ordered receive, fragment/reassembly, cookie handshake와 server/client endpoint가 코드에 있다. reliable lane은 32 pending packets 상한 때문에 32 fragments를 넘는 한 logical reliable message를 backpressure 처리한다. 현재는 fixed RTO이며 MAC/AEAD·RTT/pacing/congestion은 미완료다.
- **꼬리질문 대비**: "재전송 스톰은?" — 재전송에도 새 seq를 부여(원본과 구분), 채널별 미ACK 상한, 상태 채널은 재전송 대신 다음 틱 최신값으로 대체.

### Q27. 클라이언트가 조작된 패킷을 보내는 것은 어떻게 방어하나요?

- **정의/원리**: 서버 권위의 제1원칙 — 클라는 입력만, 검증은 서버가. 계층별 방어: ① 프레이밍(magic/version/크기 상한) ② 직렬화(FlatBuffers Verifier) ③ 세션(시퀀스 단조 증가) ④ 게임 로직(쿨다운/사거리/자원 서버 판정) ⑤ 정보 은닉(시야 밖 데이터 미전송).
- **내 프로젝트 연결**: TCP ①은 `CFrameParser`, UDP ①은 `UdpPacketCodec`+connection/generation/lane/size 검증, ②는 모든 FlatBuffers 수신 경계의 Verify+bounded log, ③은 공통 `CServerSessionHub::TryAcceptCommandSequence`의 단조 증가/+60 window, ④는 서버 `CommandExecutor`의 쿨다운·사거리·자원·nav 판정, ⑤는 시야 필터링이다. UDP handshake cookie는 주소 spoofing 비용을 높이지만 post-handshake MAC/AEAD가 아직 없어 인증·기밀성 완성으로 주장하지 않는다.
- **꼬리질문 대비**: "속도핵은?" — 서버가 이동을 시뮬레이션하므로 클라 좌표 조작은 무효. 입력 빈도 이상치(초당 명령 수) 탐지가 다음 층.

### Q28. HTTP/REST는 게임 어디에 쓰고, 실시간 소켓과 어떻게 역할을 나누나요?

- **정의/원리**: 로그인/계정/상점/매치메이킹/전적 = 요청-응답형, 낮은 빈도, 트랜잭션 정합성 중요 → HTTP(S) REST + 토큰 인증. 인게임 실시간 상태 = 고빈도 저지연 → 소켓. 매치메이킹 완료 시 REST가 게임 서버 주소+입장 토큰을 내려주고 소켓 접속으로 전환하는 2단 구조가 표준.
- **내 프로젝트 연결**: `CHttpClient`(WinHTTP)가 백엔드 REST 담당 — URL 파싱, Bearer 토큰(`SetAuthToken`), GET/POST/DELETE, 2xx 판정. 게임 스레드를 막지 않도록 `AsyncGet/AsyncPost`는 워커에서 요청하고 결과 콜백은 큐에 쌓아 메인 루프 `ProcessCallbacks()`에서 실행한다 — 게임 상태를 만지는 코드가 항상 메인 스레드에서 돌게 하는 스레드 규약.
- **꼬리질문 대비**: "왜 인게임도 웹소켓으로 안 하나?" — 웹소켓도 TCP 위 프레이밍이라 HOL 동일, 브라우저 클라가 아니면 자체 프레이밍 대비 이점이 없다. 네이티브 클라는 소켓 직접 제어가 우월.

### Q29. std::async를 썼는데 비동기가 아니었던 버그를 설명해보세요.

- **정의/원리**: `std::async(std::launch::async, f)`는 future를 반환하는데, 이 **future의 소멸자는 공유 상태가 async 런치일 때 작업 완료까지 블로킹**한다(C++ 표준). 반환값을 받지 않으면 임시 future가 그 문장 끝에서 파괴되며 즉시 wait — 호출부가 통째로 동기화된다.
- **실제 사례(내 프로젝트)**: `CHttpClient`의 비동기 요청이 정확히 이 패턴이었다. HTTP 왕복(수십~수백 ms) 동안 호출 스레드가 침묵 블로킹 — "비동기 API인데 프레임이 튄다"로 발현. 더 위험한 건 이 우연한 동기화 덕에 람다의 raw `this` 캡처가 **우연히** 안전했다는 점 — 진짜 비동기로 고치는 순간 소멸 경합(UAF)이 드러난다. 그래서 한 변경으로 두 문제를 함께 수정했다: future를 `m_PendingRequests`가 소유(발행 시 완료분 prune), 소멸자가 전체 drain하여 this 수명 보장, 워커는 멤버 대신 발행 시점 `RequestSnapshot` 복사본만 읽음. 팀 gotcha로 박제(2026-07-09 async lifetime).
- **꼬리질문 대비**: "그럼 뭘 썼어야 하나?" — 소유되는 future, 또는 detach 의미가 정말 필요하면 스레드풀/작업큐. "future를 버리는 코드는 컴파일은 되지만 의미가 바뀐다"가 이 함정의 교훈.

### Q30. IOCP 워커 스레드와 게임 로직 스레드는 어떻게 연결하나요?

- **정의/원리**: 완료 처리(파싱까지)는 워커, 월드 변경은 단일 틱 스레드 — 게임 월드를 멀티스레드로 직접 갱신하면 락 지옥+결정론 파괴. 워커→틱은 큐/락으로 명령을 전달하고, 틱 시작 시 일괄 소비한다.
- **내 프로젝트 연결**: UDP IOCP callback은 `CServerSessionHub` bounded ingress에 owned frame만 넣고 return한다. `CGameRoom::Tick()`이 state mutex 전에 `DrainIngress`하여 association generation과 active gate를 다시 확인한 뒤 connect/frame/disconnect를 적용한다. outbound는 logical `sessionId + type + owned payload`를 `SendFrame`에 주면 hub가 TCP envelope 또는 UDP lane으로 라우팅한다. legacy TCP inbound는 아직 기존 direct dispatch다.
- **Fiber 경계**: Engine FiberFull은 구현됐지만 IOCP workers/GQCS wait를 fiber화하지 않는다. tick 뒤 순수 CPU 작업만 immutable input/output DAG로 jobify할 수 있다. 서버 main opt-in 배선과 startup wait/resume 검증은 완료됐고 실제 workload jobification/speedup은 별도 검증 대상이다.
- **꼬리질문 대비**: "틱 병렬화는?" — 시스템 의존 그래프 기반 스테이지 병렬화(Stat/Cooldown → Buff → Move → Damage/Death)를 설계했고, 검증 기준은 '같은 입력 → byte-exact 스냅샷' 결정론 테스트다.

### Q31. 서버와 클라이언트의 버전이 어긋나면 어떤 일이 생기고, 어떻게 감지하나요?

- **정의/원리**: 어긋남의 층위가 여러 개다 — ① 와이어 포맷(헤더 구조) ② 스키마(FlatBuffers 필드) ③ 게임 데이터(밸런스 수치/생성 테이블). ①②는 verify가 잡을 수 있지만 ③은 패킷이 멀쩡히 파싱되면서 **수치만 미묘하게 다른 드리프트**로 나타나 가장 악질이다.
- **내 프로젝트 연결**: 3단 방어 — 헤더 `version` 필드(전송 포맷), FlatBuffers Verifier+bounded log(스키마), 그리고 Hello 핸드셰이크의 `dataBuildHash`: 서버가 자기 게임데이터 빌드 해시를 내려주면 클라 `OnHello`가 `ChampionGameDataDB::GetBuildHash()`와 대조해 불일치를 접속 시점에 로그로 못박는다(구버전 서버는 0 전송으로 검사 생략 — 하위 호환 처리까지). 이 항목은 자체 아키텍처 감사에서 HIGH로 뽑아 직접 배선한 것이라 "문제를 스스로 발견→우선순위화→구현"한 스토리로 쓸 수 있다.
- **꼬리질문 대비**: "불일치면 끊어야 하지 않나?" — 개발 단계에선 가시화(로그+HUD)가 우선, 배포 단계에선 접속 거부+패치 유도가 맞다. 정책 단계 구분을 언급.

### Q32. TCP만으로 실시간 게임을 만들 수 있나요? 한계는 어디서 오나요?

- **정의/원리**: 가능하다 — 조건은 낮은 유실률 환경(LAN/양호한 회선)과 지연 스파이크 허용 설계. 한계는 유실 시: 재전송 대기(최소 ~RTT, 타임아웃이면 수백 ms) 동안 이후 데이터까지 앱 전달이 막히는 HOL + cwnd 축소가 겹쳐 스냅샷 여러 개가 한꺼번에 밀려온다.
- **완화책**: TCP_NODELAY(필수), 상태 동기화를 스냅샷(최신 수용) 기반으로 — 밀린 스냅샷 중 최신만 적용하면 복구가 빠르다. 명령은 어차피 신뢰성이 필요하므로 TCP와 궁합이 맞는다.
- **내 프로젝트 연결**: Winters는 TCP+NODELAY 기준선 위에 UDP v3 transport를 실제 구현했고 feature-gated 제품 배선과 TCP·UDP·dual F5 smoke를 닫았다. TCP fallback은 migration 검증과 rollback에 필요하다. full snapshot이 UDP에서 자동으로 싸지는 것은 아니며 현재 p95 18 datagrams 측정이 delta/AOI의 우선순위를 증명한다.
- **꼬리질문 대비**: "몇 ms부터 체감하나?" — 인풋 지연은 ~100ms부터, 스냅샷 정지는 2-3틱(66-100ms)부터 보간 버퍼 고갈로 체감. 수치로 답하면 강하다.

### Q33. 패킷 하나가 서버에 도착해서 게임에 반영되기까지의 전체 경로를 당신 프로젝트 기준으로 설명해보세요.

- **답변 골격(엔드투엔드 내러티브 — 사실상 종합 문제)**:
  1. 클라: 우클릭 → Move 명령(raw 클릭 XZ + sequence)을 FlatBuffers로 직렬화 → 16바이트 헤더(magic/type/payloadSize/seq) 프리픽스 → TCP send(NODELAY라 즉시 발송).
  2. 서버 커널: 세그먼트 수신 → IOCP가 사전 등록된 `WSARecv` 버퍼에 복사 → 완료 패킷을 큐에 push.
  3. 워커: `GetQueuedCompletionStatus`로 꺼냄 → `CONTAINING_RECORD`로 IOContext 복원 → `CSession::OnRecvComplete` → `CFrameParser::Append`+`TryPop`으로 프레임 경계 복원 → `TryAcceptSequence`로 시퀀스 검증 → 디스패처가 명령 큐에 적재 → 즉시 `PostRecv` 재등록.
  4. 틱 스레드: 다음 30Hz Tick에서 `m_stateMutex` 아래 명령 소비 → `CommandExecutor`가 서버 내비게이션으로 경로 해석(클라 좌표를 믿지 않음) → ECS 월드 갱신.
  5. 스냅샷: `SnapshotBuilder::Build`가 전 엔티티를 netId 정렬로 직렬화(lastAckedSeq/yourNetId 포함) → 각 세션 send queue → WSASend 체인.
  6. 클라: 수신 → Verify → `SnapshotApplier::OnSnapshot`이 위치/yaw 적용(로컬 챔피언은 예측 yaw 보호 로직 경유) → 렌더.
- **포인트**: 각 단계에서 "왜 이 경계가 여기 있는가"(워커=IO, 틱=월드 소유권, 프레이밍=스트림 경계, 검증=신뢰 경계)를 한 줄씩 붙이면 아키텍처 이해를 통째로 증명하는 답이 된다.

UDP 변형은 1~3과 5~6의 transport adapter만 바뀐다. 클라는 40B header와 lane sequence/ACK를 붙여 1200B 이하로 fragment한다. 서버 `WSARecvFrom` completion은 endpoint/connection/generation을 검증하고 lane ACK·ordered receive·reassembly를 거쳐 hub ingress로 넘긴다. 다음 tick에서 logical session frame으로 authority path에 합류한다. outbound snapshot은 hub→UDP core가 unreliable-sequenced Snapshot lane으로 조각화한다. GameCommand/GameSim/Snapshot의 권위 계약은 TCP와 동일하다.

### Q34. Heartbeat(하트비트)는 왜 필요한가요? TCP keepalive로는 부족한가요?

- **정의/원리**: TCP keepalive는 기본 2시간 간격이라 게임 좀비 세션 감지에 무용. 애플리케이션 하트비트는 수 초 간격 ping/timeout으로 ① 죽은 연결 신속 감지(세션·엔티티 슬롯 회수) ② NAT 매핑 유지(특히 UDP) ③ RTT 측정 채널 확보까지 겸한다.
- **왜**: "TCP는 연결 지향인데 왜 또 필요하냐"가 함정 — TCP 연결의 죽음은 **다음 send가 실패할 때까지** 모른다. 수신만 하던 서버는 클라가 전원 강제 종료돼도 영원히 모를 수 있다.
- **내 프로젝트 연결**: 프로토콜에 `ePacketType::Heartbeat = 11`과 `Disconnect = 12`가 정의되어 있다(`Shared/Network/PacketEnvelope.h`). 명시적 Disconnect(정상 종료)와 하트비트 타임아웃(비정상 종료)을 구분해 처리하는 것이 세션 수명 관리의 기본이라고 답한다. 또한 zero-byte recv(FIN 수신)도 `OnRecvComplete`에서 즉시 disconnect로 처리 — 우아한 종료/급사/좀비의 3경로를 모두 커버.
- **꼬리질문 대비**: "하트비트 간격은?" — 감지 속도 vs 트래픽. 게임은 보통 1-5초 + 2-3회 미응답 시 킥. 스냅샷/명령 자체가 오가면 하트비트를 생략하는 최적화(암묵적 keepalive)도 언급.

### Q35. 이동 명령을 매우 빠르게 연타하면 어떤 문제가 생기고, 어떻게 처리했나요?

- **정의/원리**: 초당 수십 개 Move가 큐에 쌓이면 ① 서버가 스테일 명령을 순서대로 전부 실행해 눈에 보이는 지그재그 조향 ② 틱 예산 낭비 ③ 대역폭 낭비. 본질은 "이동 명령은 누적 이벤트가 아니라 **최신 의도**"라는 것 — 마지막 클릭만 의미가 있다.
- **내 프로젝트 연결**: 명령 코얼레싱을 팀 규칙으로 박제했다(gotcha 2026-05-20) — 펜딩 큐에 같은 세션의 옛 Move가 있으면 최신 Move로 **교체**하고, 스킬 등 비-이동 명령은 보존(이건 이벤트라 유실 불가). 추가로 Move 페이로드는 클라가 보정한 좌표가 아니라 **raw 클릭 XZ**를 담는다 — 클라 보정을 믿으면 서버 내비게이션이 플레이어의 원래 의도를 잃기 때문. "명령 = 의도 선언, 해석 = 서버"라는 서버 권위 원칙의 구체 사례.
- **꼬리질문 대비**: "스킬 연타는?" — 스킬은 코얼레싱하면 안 된다(각각이 유효한 시도). 대신 서버 쿨다운/상태 검증이 거른다. 명령 종류별로 다른 정책이 필요하다는 일반화로 마무리.

---

## 내 프로젝트 연결 포인트

면접에서 "직접 해봤다"를 증명하는 문장들. 각 문장은 코드 위치까지 즉답 가능해야 한다.

1. **"IOCP TCP 게임 서버를 밑바닥부터 구현했습니다."** — CreateIoCompletionPort/GQCS 워커 풀, AcceptEx 4개 선등록(WSAIoctl 확장 포인터 로드), CONTAINING_RECORD 기반 IOContext 복원, PostQueuedCompletionStatus(nullptr)로 워커 종료까지. (`Server/Private/Network/IOCPCore.cpp`)
2. **"Overlapped 수명과 송신 직렬화를 세션 계층에서 해결했습니다."** — pending IO 카운트(AddPendingIo/CompletePendingIo), send queue + in-flight 1개 체인, recv/send 컨텍스트 분리, zero-byte recv=FIN 처리. (`Server/Private/Network/Session.cpp`)
3. **"TCP 스트림 경계 문제를 16바이트 길이 프리픽스 프레이밍으로 풀고, 와이어 포맷을 static_assert로 못박았습니다."** — magic 'WB'/version/64KB 상한/오염 시 버퍼 폐기. (`Shared/Network/PacketEnvelope.h`, `Server/Private/Network/FrameParser.cpp`)
4. **"30Hz 서버 권위 스냅샷 동기화를 FlatBuffers zero-copy로 구현했습니다."** — 절대시각 sleep_until 고정 틱, netId 정렬 결정론 직렬화, lastAckedSeq/yourNetId/rngState 포함. (`Server/Private/Game/GameRoomTick.cpp`, `SnapshotBuilder.cpp`)
5. **"verify 실패 침묵 drop 금지를 팀 규칙으로 만들고 전 수신 경계에 bounded trace(8회 상한)를 배선했습니다."** — 스키마 드리프트가 '조용한 월드 프리즈'로 위장하는 것을 방지. (`Client/Private/Network/Client/SnapshotApplier.cpp`, `EventApplier.cpp`)
6. **"클라 예측과 서버 권위의 충돌을 acked command seq 기반 보호 로직으로 실제로 디버깅했습니다."** — 로컬 yaw 예측 보호(허용오차 수렴/액션 락/ACK 후 유예 12스냅샷/wrap-around 안전 seq 비교), 서버·클라 양측 YawTrace 로그로 틱 단위 대조 검증. (`SnapshotApplier.cpp`)
7. **"lag compensation을 상한(200ms=6틱)과 엔티티 세대 검증까지 넣어 구현했습니다."** — 히스토리 되감기 + ID 재사용 버그 차단. (`Server/Public/Security/LagCompensation.h`)
8. **"서버/클라 데이터 버전 드리프트를 접속 핸드셰이크(dataBuildHash)에서 잡도록 자체 감사 후 직접 배선했습니다."** — 문제 발굴→우선순위→구현의 사이클 증명. (`SnapshotApplier.cpp::OnHello`)
9. **"std::async future 소멸자 블로킹 함정을 실제로 밟고, future 소유권+스레드 규약(RequestSnapshot 복사, 메인 스레드 콜백 큐)으로 재설계했습니다."** (`Client/Private/Network/Backend/CHttpClient.cpp`)
10. **"시퀀스 검증(리플레이 차단+이상 점프 격리), 명령 코얼레싱, raw 의도 보존 등 서버 권위 원칙을 세부 정책으로 구체화했습니다."** (`CServerSessionHub::TryAcceptCommandSequence`, gotchas 박제 규칙)
11. **"TCP fallback을 보존한 채 UDP v3 core를 구현했습니다."** — 40B header, lane별 seq/ACK/reliability, 1144B fragments, bounded reassembly, cookie handshake, server/client socket endpoints. (`Shared/Network/UdpPacketHeader.h`, `Server/Private/Network/UdpIocpCore.cpp`, `Client/Private/Network/Client/UdpClient.cpp`)
12. **"transport와 game authority를 logical session hub로 분리했습니다."** — UDP callback은 bounded queue만 쓰고 tick이 drain, outbound는 TCP envelope/UDP lane을 공통 `SendFrame`으로 선택. (`Server/Private/Network/ServerSessionHub.cpp`)
13. **"구현 한계를 수치와 threat model로 말할 수 있습니다."** — full snapshot p95 19,808B/18 datagrams, delta/AOI 필요; MAC/AEAD·RTT/pacing/congestion·IPv6/NAT/PMTU·graceful close가 production gate.
14. **"IOCP wait를 fiber화하지 않고 순수 CPU DAG만 FiberFull 후보로 둡니다."** — Engine scheduler와 server startup 배선은 검증했고, 실 workload jobification/speedup은 분리 검증.

## 마지막 점검 체크리스트

- [ ] OSI 7 vs TCP/IP 4 — 캡슐화, 세션/표현/응용→응용 통합, 게임서버=5~7계층 직접 구현.
- [ ] TCP vs UDP — HOL blocking이 핵심 비용, "오래된 상태는 재전송 가치 없음", 장르별 선택.
- [ ] 3-way — ISN 상호 확인, 2-way 불가 이유(유령 연결), SYN flood/cookie.
- [ ] 4-way/TIME_WAIT — 2*MSL, 마지막 ACK 유실 대비+옛 세그먼트 혼선 방지, 능동 종료측에 발생.
- [ ] 재시작 bind 실패 — Windows SO_REUSEADDR는 하이재킹 위험 → Winters는 SO_EXCLUSIVEADDRUSE.
- [ ] 흐름제어(rwnd, 수신자)vs 혼잡제어(cwnd, 네트워크), 송신량=min(rwnd,cwnd), zero window probe.
- [ ] 슬로우스타트(지수)→ssthresh→AIMD(선형+, 절반×), 3중복 ACK vs 타임아웃 차이.
- [ ] Nagle×DelayedACK 상호 대기 → 게임은 TCP_NODELAY (IOCPCore accept 직후 설정) + 배칭은 앱 책임.
- [ ] MTU 1500/MSS 1460, 단편화=유실 증폭, UDP 게임은 ~1200B 상한+앱 조각화.
- [ ] blocking/non-blocking(대기 여부) ⊥ 동기/비동기(복사 주체) — select=readiness, IOCP=completion.
- [ ] select(64/O(n))→WSAEventSelect(64/스레드 증설)→Overlapped(스레드 종속)→IOCP(큐+워커 풀).
- [ ] IOCP: 핸들 연계→비동기 발행→완료 큐→GQCS 워커, CONTAINING_RECORD, nullptr 완료로 종료, GQCS FALSE 2종 구분.
- [ ] AcceptEx 선등록 4개 + SO_UPDATE_ACCEPT_CONTEXT 함정.
- [ ] Overlapped 수명 = pending IO refcount, 송신은 queue+in-flight 1, recv 0바이트=FIN.
- [ ] 프레이밍: 16B 헤더(magic 0x5742/version/type/payloadSize/seq), static_assert(16), 64KB/256KB 상한, 오염 시 Clear.
- [ ] FlatBuffers = zero-copy(스냅샷 30Hz에 적합), Verify 필수, 실패는 bounded log(8회) — 침묵 drop 금지.
- [ ] 서버 권위 vs 락스텝 — 치트 내성/유실 복원 vs 대역폭/결정론 강제. Winters=권위+결정론 인프라(리플레이 검증 목적).
- [ ] 틱 30Hz: microseconds(33333) + sleep_until(절대시각, 드리프트 방지), kFixedDt=1/30, 시간은 틱 단위 저장.
- [ ] 스냅샷=상태 동기화(유실 복원력), 델타=ack baseline(Quake3), Winters는 full→델타 로드맵.
- [ ] AOI=대역폭, 시야 필터링=정보 치트 방지 — 목적 구분.
- [ ] 예측/리컨실리에이션: lastAckedCommandSeq+yourNetId, yaw 보호(0.2rad 수렴/액션락/유예 12스냅샷), i32 캐스트 wrap-safe seq 비교.
- [ ] 보간: 스냅샷 2개 버퍼+과거 렌더(~100ms), 외삽은 짧게. Winters 현재 직접 적용 → 보간 도입이 명시된 다음 단계(정직하게).
- [ ] Lag comp: 히스토리 되감기, 상한 200ms=6틱, EntityGeneration으로 ID 재사용 방어, behind-cover-death 부작용.
- [ ] RUDP: v3 40B header, ack latest+32-bit history, lane별 신뢰성, 1200/16/1144B fragment 계약. 현재 fixed RTO이고 RTT/pacing/congestion/MAC은 production gap.
- [ ] 시퀀스 검증: 단조 증가+윈도우 60, 초과 점프=suspicious 보고.
- [ ] Heartbeat: TCP keepalive 2시간 무용, 앱 레벨 수 초 + Disconnect 패킷과 구분.
- [ ] std::async future 버리면 소멸자 블로킹=동기화, 우연한 this 안전성까지 세트로 수정한 스토리.
- [ ] HTTP=로비/계정(토큰), 소켓=인게임, 콜백은 메인 스레드 큐로.
- [ ] 스레드 경계: UDP IOCP callback=bounded owned frame, GameRoom tick=drain+월드 단독 소유, legacy TCP inbound 차이 인지, IOCP worker는 fiber화 안 함.
- [ ] 버전 3단 방어: 헤더 version / FlatBuffers Verify / Hello dataBuildHash.
- [ ] 종합 내러티브(Q33): 클릭→직렬화→IOCP→프레이밍→seq검증→틱 소비→스냅샷→클라 적용, 각 경계의 존재 이유 한 줄씩.

## Canonical code pointers

- TCP baseline: `Server/Private/Network/IOCPCore.cpp`, `Session.cpp`, `FrameParser.cpp`, `Shared/Network/PacketEnvelope.h`
- UDP wire semantics: `Shared/Network/UdpPacketHeader.h`, `UdpPacketCodec.h`, `UdpFragmentHeader.h`, `UdpFragmentCodec.h`, `PacketSemantics.h`
- UDP reliability/reassembly/handshake: `Shared/Network/UdpReliabilityChannel.*`, `UdpOrderedReceiveQueue.h`, `UdpReassemblyBuffer.h`, `UdpHandshake.h`
- UDP endpoints: `Server/Public/Network/UdpIocpCore.h`, `Server/Private/Network/UdpIocpCore.cpp`, `Client/Private/Network/Client/UdpClient.h`, `UdpClient.cpp`
- Transport-neutral logical session: `Server/Public/Network/ServerSessionHub.h`, `Server/Private/Network/ServerSessionHub.cpp`
- Authority handoff: `Server/Private/Game/GameRoomTick.cpp`, `CommandIngress.cpp`, `SnapshotBuilder.cpp`
- Executable/measurement proof: `Tools/Harness/UdpLoopbackHarness.cpp`, `Tools/Harness/UdpF5SessionSmoke.cpp`, `Tools/Harness/MeasureReplayNetworkPayload.py`, `.md/build/2026-07-13_NETWORK_REPLAY_PAYLOAD_MEASUREMENT.json`
