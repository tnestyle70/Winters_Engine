# 면접 대비 — 도메인 5: 네트워크 (IOCP / 스냅샷 / UDP)

> 근거: `WINTERS_DOMAIN_HONEST_MAP_2026-06-26.md` §5(working). 코드 인용은 `Server/Private/Network`, `Server/Public/Network`, `Shared/Network`, `Shared/Schemas` 실파일 기준. 계획 부분은 `.md/plan/sim/10_UDP_LOL_NETSTACK_MASTER.md`, `.md/TODO/05-07/00_TCP_UDP_MIGRATION_INDEX.md` 근거.
>
> **정직성 경계(절대 위반 금지)**: UDP는 헤더/스키마/계획만 있고 게임 코드에 소켓 호출 0줄. delta/AOI는 스키마 필드만 있고 항상 full 스냅샷. "N만 동접"은 실측 없음 — localhost 스모크 수준.

---

## 0. 한 줄 본질 + 현재 상태

**한 줄 본질**: 권위 서버가 게임의 단일 진실(truth)이고, 네트워크 계층은 *클라의 "의도"를 안전하게 받아들이고(수신) 서버가 확정한 "결과"를 다수 클라에 일관되게 복제(송신)* 하는 양방향 파이프다. 그 파이프를 Windows IOCP 비동기 TCP 위에 length-prefixed framing + FlatBuffers로 구현했다.

**현재 성숙도(혼재, 정직하게)**:
- **production/working**: IOCP TCP 서버(AcceptEx/WSARecv/WSASend, 4-worker 완료포트), 16B 고정헤더 length-prefixed framing, FlatBuffers Command/Snapshot/Event 직렬화, 매 틱 full 스냅샷 세션별 복제, sequence anti-replay/suspicion 가드. round-trip(Input→Command→Snapshot→Visual)이 닫혀 있고 ReplayRecorder로 캡처 가능.
- **planned-only**: UDP 전송 계층 전체(3-channel reliability, fragment, encryption), snapshot delta, AOI, lag compensation rewind 적용, client prediction/reconciliation. 헤더(`UdpPacketHeader.h`, `UdpReliabilityChannel.h`)와 스키마 필드(`deltaBaseTick`)는 **선언만** 있고 .cpp/소켓 호출 0줄.

즉 "TCP 권위 복제는 구현, UDP 넷스택은 설계"가 정확한 한 줄이다.

---

## 1. 핵심 개념 (본질부터)

### 1.1 왜 "서버 권위(server-authoritative)"인가 — 1차 원리

멀티플레이 게임의 근본 문제는 **누가 진실을 소유하는가**다. 모든 클라가 자기 상태를 주장하면(P2P/클라권위) 한 명만 거짓말해도("나는 순간이동했다") 게임이 붕괴한다. 해법은 단순하다: **클라는 결과를 못 보내고 의도만 보낸다.** "(x=500, y=300)으로 이동했다"가 아니라 "이 방향으로 이동하고 싶다"만 보내고, 서버가 NavGrid/충돌/쿨다운을 적용해 *진짜 위치를 확정*한다. 그러면 스피드핵·텔레포트의 공격 표면 자체가 사라진다. 이 도메인의 모든 설계는 이 한 문장에서 파생된다. → `Command.fbs`의 `CommandKind`(Move/CastSkill/BasicAttack…)는 전부 "의도"다.

### 1.2 IOCP — 왜 thread-per-connection이 아닌가

가장 순진한 서버는 "접속 1개 = 스레드 1개"다. 1000명이면 1000 스레드 → 컨텍스트 스위칭 비용이 폭발하고, 대부분 스레드는 recv를 기다리며 블로킹(놀고 있음)한다. **IOCP(I/O Completion Port)는 "완료 통지 큐 + 소수의 워커 풀"** 모델로 이를 뒤집는다:
- I/O를 **비동기로 게시(post)** 한다(`AcceptEx`, `WSARecv`, `WSASend`). 즉시 리턴하고, 커널이 완료되면 완료포트에 패킷을 넣는다.
- 소수(보통 코어 수) 워커가 `GetQueuedCompletionStatus`로 완료를 꺼내 처리. **스레드 수 ≪ 연결 수**.
- 핵심은 `OVERLAPPED` 구조체다. 비동기 I/O마다 `OVERLAPPED`를 주고, 완료 통지에서 `CONTAINING_RECORD`로 그 포인터를 거꾸로 우리 컨텍스트로 복원한다. 어떤 소켓/어떤 작업이 끝났는지를 이 포인터로 식별한다.

이게 Windows 고성능 서버의 표준이고, Winters는 `IOContext`(OVERLAPPED + WSABUF + 8KB 버퍼 + op 종류)를 단위로 이 모델을 구현했다.

### 1.3 TCP는 stream이다 — 그래서 framing이 필요하다

TCP는 "메시지"를 모른다. **바이트 스트림**일 뿐이다. `send` 두 번 했다고 `recv`가 두 번 나뉘어 오지 않는다 — 합쳐지거나(코얼레싱) 쪼개져 온다(부분 수신). 따라서 "메시지 경계"를 앱이 직접 그어야 한다. 표준 해법이 **length-prefixed framing**: 매 메시지 앞에 고정 헤더를 붙이고 그 헤더에 payload 길이를 넣는다. 수신 측은 누적 버퍼에 쌓다가 (헤더+payload)가 다 모이면 한 프레임을 떼어낸다. Winters의 `PacketHeader`(16B, magic+version+type+flags+payloadSize+sequence)와 `CFrameParser`가 정확히 이것이다.

### 1.4 직렬화 — 왜 구조체 memcpy가 아니라 FlatBuffers인가

구조체를 그대로 `memcpy`해 보내면(레거시 `PacketDef.h` 방식) 패딩/엔디안/필드 추가 시 와이어 호환성이 깨진다. **FlatBuffers**는 스키마(`.fbs`)에서 코드를 생성하고, **역직렬화 없이 버퍼 위에서 바로 읽는(zero-copy access)** 직렬화다. 결정적으로 `flatbuffers::Verifier`가 *신뢰 못 할 네트워크 바이트*가 우리 메모리를 벗어나 읽지 않는지(offset 범위 검증) 보장한다 — 악의적 패킷 방어의 1차선. Winters는 Command/Snapshot/Event를 `.fbs`로 정의하고 C++/Go 양쪽 codegen을 쓴다.

### 1.5 스냅샷 복제 vs lockstep — 왜 스냅샷인가

상태 동기화에는 두 갈래가 있다. **Lockstep**(모든 클라가 같은 입력으로 같은 시뮬을 돌림 — RTS)은 대역폭이 작지만 한 명만 느려도 전부 멈추고 완벽한 결정성을 요구한다. **스냅샷 복제**(서버만 시뮬하고 결과 상태를 주기적으로 뿌림)는 대역폭이 크지만 클라가 죽어도 게임이 굴러간다 — LoL/FPS의 선택. Winters는 후자다: 30Hz 권위 틱이 끝날 때마다 살아있는 엔티티를 결정론적 순서로 모아 `Snapshot` FlatBuffer를 만들어 세션별로 보낸다.

### 1.6 결정성 — 왜 네트워크 도메인이 여기 신경 쓰나

스냅샷에 `rngState`, `serverTick`이 들어가는 이유: **같은 입력 + 같은 seed → 같은 결과**여야 리플레이/검증/(미래의)client prediction이 성립한다. 비결정 요소(hash 순서 순회, wall-clock 시간, `/fp:fast`)는 같은 입력에도 다른 결과를 만들어 이 계약을 깬다. 그래서 스냅샷 빌드는 `DeterministicEntityIterator`로 EntityID 정렬 순회를 강제한다.

---

## 2. 왜 이 선택인가 — 기술 스택 + Trade-off

| 선택지 | 장점 | 단점 | 내 결정 이유 |
|---|---|---|---|
| **IOCP** (vs thread-per-conn, select/epoll) | Windows 최고 확장성, 스레드≪연결 | Windows 종속, OVERLAPPED 수명 관리 까다로움 | 타깃이 Windows DX11 클라/서버. 고성능 비동기 I/O의 정석을 직접 구현하는 게 학습 목표였음 |
| **TCP 먼저** (vs UDP 먼저) | framing/세션/디스패치/룸 골격을 빠르게 닫음, HoL이 localhost에선 무해 | HoL blocking으로 실전 30Hz 게임엔 부적합 | "전송계층 교체는 나중, 권위 루프 닫기가 먼저." TCP로 round-trip을 먼저 증명하고 UDP는 그 골격을 포팅하는 M1로 둠 |
| **length-prefixed framing** (vs 구분자 delimiter) | 가변 payload, 1회 길이로 경계 확정 | 헤더 오버헤드 16B | 게임 패킷은 바이너리라 delimiter가 payload에 섞임 → 길이접두가 정답 |
| **FlatBuffers** (vs raw 구조체 memcpy, JSON, Protobuf) | zero-copy 읽기, Verifier로 안전, C++/Go 공유 스키마, 와이어 진화 | 빌더 API 장황, codegen 빌드 단계 | 신뢰 못 할 바이트를 검증 없이 캐스팅하는 레거시 방식을 버리고, 보안+성능+다언어 공유를 동시에 |
| **full snapshot 먼저** (vs delta 먼저) | 구현 단순, 손실 내성(다음 스냅샷이 덮음), 버그 적음 | 대역폭 큼(5v5 ~19KB/s/client 추정) | "정확성 먼저, 대역폭은 측정 후." delta는 baseline ack 미동기화 시 영구 오염 위험 → MVP에선 의도적으로 full |
| **세션별 송신 큐 + pending-IO refcount** | WSASend 직렬화(겹침 방지), 안전한 해제 | 큐 lock, 약간의 지연 | IOCP에서 동일 소켓에 WSASend 중복 게시는 순서 깨짐 → 한 번에 하나만 in-flight, 나머지는 큐 |

**근본 Trade-off 3개**:
1. **TCP HoL 단순성 ↔ UDP 실전성**: TCP는 in-order를 공짜로 주지만 1% 손실에도 1~3틱 frozen. localhost 스모크는 무해해서 골격 검증엔 충분, 하지만 실전 30Hz엔 부적합 → UDP가 production 방향(§6).
2. **full snapshot 견고함 ↔ delta 대역폭**: full은 손실에 강하고(다음 스냅샷이 전부 덮음) 버그가 없지만 비싸다. delta는 6배 절감 추정이지만 baseline 동기화 깨지면 클라가 영구 오염 → resync 로직이 필수. "정확성으로 시작해 측정 후 delta"가 1인 프로젝트엔 합리적.
3. **신입 1인 범위**: 권위 모델·결정성·복제 round-trip이라는 *개념적 골격*을 닫는 게 우선. UDP reliability/prediction은 그 골격 위에 얹는 작업이라, "골격 먼저 닫고 전송계층 교체"가 의존성상 옳다.

---

## 3. 실제 구현 (코드 근거)

### 3.1 IOCP 코어 — 수락/워커/바인딩

`Server/Private/Network/IOCPCore.cpp`:
- `Start()`: 루트 완료포트 생성(`CreateIoCompletionPort`, L44) → listen 소켓(`WSASocketW` OVERLAPPED, L51) → `SO_EXCLUSIVEADDRUSE`/bind/listen → listen 소켓을 IOCP에 연결(L87) → `WSAIoctl`로 `AcceptEx`/`GetAcceptExSockaddrs` 확장 함수 포인터 획득(L97~111) → 워커 N개 생성(L120) → **AcceptEx 4개 미리 게시(L123~129)**.
- `PostAccept()` (L166): 매번 새 accept 소켓을 만들어(`WSASocketW`) `AcceptEx`로 비동기 수락 게시. `ERROR_IO_PENDING`이 정상.
- `WorkerLoop()` (L206): `GetQueuedCompletionStatus(INFINITE)` → `pOverlapped == nullptr`면 종료 신호(`PostQueuedCompletionStatus`로 깨움) → `CONTAINING_RECORD`로 `IOContext` 복원(L225) → op별 분기:
  - **Accept**(L245): `SO_UPDATE_ACCEPT_CONTEXT` + `TCP_NODELAY`(Nagle off, 게임은 지연 민감) → `Session_Manager::OnAccept`로 세션 생성 → `BindIOCP`로 새 소켓을 sessionId를 completion key로 IOCP에 연결(L257) → `PostInitialRecv` → `g_pRoom->OnSessionJoin`으로 controlledEntity 바인딩 → **AcceptEx 재게시(L279)**.
  - **Recv**(L283): `bytes==0`이면 정상 종료(graceful disconnect) → 아니면 `OnRecvComplete`.
  - **Send**(L293): `OnSendComplete`.

종료 신호 패턴이 깔끔하다: `Shutdown()`에서 워커 수만큼 `PostQueuedCompletionStatus(0,0,nullptr)`를 게시해 블로킹 중인 워커를 깨우고 join한다(IOCPCore.cpp L142).

### 3.2 세션 — recv 루프, 송신 직렬화, pending-IO refcount

`Server/Private/Network/Session.cpp`:
- **Recv 루프**(L47 `PostRecv`, L72 `OnRecvComplete`): `WSARecv`를 게시할 때 `AddPendingIo()`로 refcount++ → 완료 시 `CompletePendingIo()`로 -- → 수신 바이트를 `m_recvParser.Append`로 누적 → `CPacketDispatcher::DrainFrames`로 완성된 프레임을 모두 소비 → 닫는 중이 아니면 다음 `PostRecv`를 다시 게시(연속 수신 체인).
- **송신 직렬화**(L89 `Send`, L126 `OnSendComplete`): `m_sendQueue`(deque) + `m_bSendPending` 플래그. 보낼 게 있는데 이미 in-flight면 큐에만 넣고 리턴(L100). 아니면 front를 `WSASend`. 완료 콜백에서 front를 pop하고 다음 게 있으면 다시 `WSASend`. **동일 소켓에 WSASend가 동시에 둘 게시되는 것을 막아 와이어 순서를 보존**한다.
- **안전 해제**(Session.h L38~52): `m_pendingIoCount`(atomic) + `m_bClosing`(atomic). `CanDestroy()`는 *닫는 중 AND pending-IO 0*일 때만 true. `Session_Manager::ReapClosingSessions`(Session_Manager.cpp L133)가 closing 리스트를 돌며 `CanDestroy()`인 것만 소거 → **커널이 아직 OVERLAPPED를 참조 중인데 세션이 free되는 use-after-free 차단**. shared_ptr + enable_shared_from_this로 수명 연장.

### 3.3 Framing — 누적 버퍼에서 프레임 떼기

`Server/Private/Network/FrameParser.cpp`:
- `Append`(L5): 바이트를 `m_Buffer`(vector)에 누적, 256KB 초과 시 `Clear`(DoS 가드).
- `TryPop`(L13): 헤더 크기 미달 → `NeedMore` / magic·version 불일치 → `Invalid`(연결 폐기) / payloadSize > 64KB → `Invalid` / (헤더+payload) 미달 → `NeedMore` / 완성 → payload를 떼어내고 버퍼 앞을 erase, `Complete`.

### 3.4 디스패치 — verify → route → room

`Server/Private/Network/PacketDispatcher.cpp`:
- `DrainFrames`(L35): `TryPop` 루프. `Invalid`면 즉시 disconnect. type별 분기(CommandBatch/LobbyCommand/Hello/Heartbeat). **알 수 없는 type → `FlagSuspicious`**(L64).
- `DispatchCommandBatch`(L71): **`flatbuffers::Verifier`로 먼저 검증**(L73) → 실패 시 `FlagSuspicious` 후 폐기 → 성공해야 `GetCommandBatch`로 접근 → sessionId→roomId→room 라우팅(lock) → `pRoom->OnCommandBatch`. 즉 *검증 통과 안 한 버퍼는 절대 역참조 안 함*.

### 3.5 anti-replay / suspicion

`Session.cpp`의 `TryAcceptSequence`(L24): mutex 하에 `seq <= m_lastProcessedSeq`면 거부(재전송/리플레이), `seq > last+60`이면 `bSuspicious=true`로 거부(점프 갭). `FlagSuspicious`는 카운트만 올리고 `IsSuspicious()`는 >5에서 true — **집계만, kick/ban 미연결**(정직성 경계).

### 3.6 스냅샷 빌드/송신 — full, 세션별, 결정론 순회

`Server/Private/Game/GameRoomReplication.cpp`:
- `Phase_BroadcastSnapshot`(L124): (선택)리플레이용 스냅샷 1개 기록 → **세션 루프**: 각 세션의 controlledEntity가 바인딩됐는지 확인 → 그 세션의 lastAckedCommandSeq 조회 → `m_pSnapBuilder->Build(...)`로 **그 세션 관점의 full 스냅샷** 생성(yourNetId 포함) → `WrapEnvelope(Snapshot, tickIndex, ...)`로 16B 헤더 씌워 `pSession->Send`.
- `SnapshotBuilder.cpp`는 `DeterministicEntityIterator`로 정렬 순회(L30 include)하며 살아있는 엔티티 전부를 `EntitySnapshot`으로 직렬화 → **항상 full**. (delta 아님 — `deltaBaseTick`은 0으로 채워짐, GameRoomReplication.cpp L131의 `Build(... 0u, NULL_NET_ENTITY)` 리플레이 경로 참조.)
- `Phase_BroadcastEvents`(L78): action-start/projectile-hit 같은 1회성 이벤트를 `ePacketType::Event`로 브로드캐스트(L73 `WrapEnvelope`).

### 3.7 와이어 포맷 공유

`Shared/Network/PacketEnvelope.h`: `kPacketMagic=0x5742('WB')`, 16B `PacketHeader`(`static_assert(sizeof==16)`로 와이어 안정성 못박음, L46), `WrapEnvelope`(송신 측 헤더 씌우기), `TryExtractFrame`(수신 측 — 단 서버는 stream용 `CFrameParser`를 쓰고 이건 datagram/단발용). Client/Server가 같은 헤더를 공유한다.

**"코드 보여달라" 했을 때 짚을 4지점**: ① IOCPCore.cpp L206 WorkerLoop(완료 디스패치 + CONTAINING_RECORD) ② Session.cpp L89~156 송신 큐 직렬화 + pending-IO refcount ③ PacketDispatcher.cpp L71~80 Verifier-우선 ④ GameRoomReplication.cpp L142~171 세션별 full 스냅샷.

---

## 4. 검증 — 동작을 어떻게 증명했나

- **Round-trip 스모크(localhost)**: 클라 1대 접속 → CommandBatch 송신 → 서버 30Hz 틱 → 세션별 Snapshot 브로드캐스트 → 클라 SnapshotApplier가 ECS 반영. 이동/HP/쿨타임/투사체가 화면에 반영되는지 시각 확인. 이게 "round-trip이 닫혔다"의 판정 기준.
- **ReplayRecorder**: `GameRoomReplication.cpp` L124~140에서 매 틱 스냅샷과 이벤트를 시퀀스와 함께 기록(`RecordSnapshot`/`RecordEvent`) → `FinalizeReplayRecorder`(L18)가 파일로 저장(records/snapshots/events 카운트 로그). 즉 *서버가 실제로 보낸 바이트 스트림을 캡처해 사후 검증*할 수 있다.
- **결정성 게이트(SimLab, 인접 도메인)**: 같은 seed+command에서 per-tick state hash 일치 검사. 스냅샷에 `rngState`/`serverTick`을 싣는 이유가 이 결정성 동기화. (SimLab 자체는 GameSim 코어 미러라 네트워크 전송계층은 안 거침 — 정직선.)
- **Suspicion/Verifier 음성 테스트**: 깨진 magic/version → `Invalid` → disconnect, 미검증 FlatBuffer → `FlagSuspicious` 후 폐기. 잘못된 입력이 서버 메모리를 못 건드리는 것을 코드 경로로 보장.

**무엇을 안 했나(정직)**: 부하 테스트(동접 N) 수치 없음, packet loss/지연 에뮬레이션(clumsy 등) 안 함, 자동화된 네트워크 통합 테스트 하니스 없음. "localhost 스모크 + ReplayRecorder 캡처 + 결정성 해시"가 검증의 전부.

---

## 5. 최적화

**실제로 한 것**:
- **`TCP_NODELAY`**(IOCPCore.cpp L251): Nagle 알고리즘 비활성화. 작은 게임 패킷이 ACK를 기다리며 묶이는 지연 제거. 30Hz 입력엔 throughput보다 latency가 중요.
- **AcceptEx 사전 게시 풀(4개)**: accept를 미리 여러 개 게시해 연속 접속 시 수락 갭을 줄임(IOCPCore.cpp L123).
- **송신 큐 단일 in-flight**: 동일 소켓 WSASend 중복 게시 방지로 재정렬/오버랩 비용 제거.
- **FlatBuffers zero-copy 읽기**: 역직렬화 단계 없이 버퍼 위에서 바로 필드 접근(Verifier만 1회).
- **세션별 controlledEntity 캐시**: 매 명령마다 sessionId→entity 재해석 안 하고 accept 시 1회 바인딩(IOCPCore.cpp L264).

**계획 중(수치는 측정 예정)**:
- **Snapshot delta**: 변경 필드만 전송. 계획서 추정 6.4배(640B→100B) — *추정치, 실측 아님*.
- **AOI(Area of Interest)**: 시야 밖 엔티티 스냅샷 제외. 계획서 추정 대역폭 80% 절감 — *추정치*.
- **양자화 LOD**: 거리별 pos 정밀도 축소(8bit/4bit). 계획.

정직선: **정량 측정된 네트워크 최적화 수치는 없다.** 위 절감치는 전부 `10_UDP_LOL_NETSTACK_MASTER.md`의 이론 추정. "구조적으로 이렇게 줄어야 하고, delta 붙이면 F-계측으로 측정하겠다"가 정확한 표현.

---

## 6. 구현 예정 (Planned) — 동일 깊이

> 여기는 **실제로 구현할 로드맵**이다. 면접에서 "그건 안 했는데 어떻게 할 건가"에 막힘없이 답하기 위한 설계.

### 6.1 UDP 전송 계층 (M1) — 왜·무엇·어떻게

**왜**: TCP는 in-order 보장 때문에 **Head-of-Line blocking**이 있다. 패킷 1개 손실 → 뒤 패킷이 OS recv 버퍼에 쌓여도 앱에 전달 안 됨. 100ms RTT·1% 손실에서도 수백 ms stall → 30Hz(33ms) 게임에선 1~3틱 캐릭터 frozen. LoL이 ENet 기반 자체 UDP reliability를 쓰는 이유.

**무엇**: `UdpCore`(WSARecvFrom/WSASendTo + IOCP), `UdpSession`(sessionId 발급 + sourceAddr 기반 lookup — UDP는 연결 개념이 없어 주소로 세션 식별), `UdpSession_Manager`, `UdpPacketDispatcher`. 와이어는 이미 박제된 `Shared/Network/UdpPacketHeader.h`의 24B `UdpPacketHeader`(magic 0x5742, version=2로 TCP와 구분, channel, channelSeq, ackSeq, ackBitfield, payloadSize).

**어떻게**: TCP 골격을 그대로 포팅한다 — IOContext/완료포트/디스패처/룸 구조는 재사용하고 소켓 호출만 datagram으로 교체. 단 framing이 다르다: TCP는 stream이라 `CFrameParser`로 경계를 긋지만, **UDP는 datagram이 곧 메시지 경계**라 framing이 불필요(헤더만 파싱). 따라서 TCP 진입(Auth/BanPick) + UDP 인게임의 **하이브리드**(`00_TCP_UDP_MIGRATION_INDEX.md`): control plane은 TCP/HTTP 유지, gameplay plane만 UDP.

**Trade-off 예상**: 신뢰성/순서를 직접 구현해야 함(공짜 TCP를 버림). NAT 통과/주소 위조(IP spoofing) 방어가 새 과제 — connect 핸드셰이크 토큰으로 완화.

**검증**: localhost 1-client connect→CommandBatch→tick→snapshot→recv 손실 0, 5분 30Hz jitter <5ms, RTT <1ms.

### 6.2 Reliability Channel (M2)

**왜**: UDP는 손실/재정렬을 그대로 노출. 입력은 반드시 도달해야 하고(reliable) 스냅샷은 최신만 중요(unreliable)하다 — 하나의 정책으로 묶으면 안 됨.

**무엇/어떻게**: 3채널(`Shared/Network/UdpPacketHeader.h`의 `eUdpChannel`):
- **ReliableOrdered**(입력 CommandBatch): 순서 보장 + 재전송. 손실 발견 시 stall.
- **ReliableUnordered**(Event kill/level): 재전송하되 순서 무관.
- **UnreliableSequenced**(Snapshot): 재전송 X, 오래된 것 폐기 — **다음 스냅샷이 전부 덮으므로 손실 무해**.

자료구조는 `Shared/Network/UdpReliabilityChannel.h`에 **선언만** 있다(.cpp 없음): `NextSendSeq`, `MarkReceived`, `BuildAck`(ackSeq+32bit bitfield), `QueueReliable`, `OnAck`, `CollectRetransmit`. ack는 헤더에 piggyback(`ackSeq`/`ackBitfield`). RTO = SRTT + 4×RTTVAR(TCP 공식), 50~500ms clamp. `SeqMath.h`의 `SeqGreater`(wrap-safe 비교)가 이미 박제 — `(int32)(a-b)>0`로 시퀀스 오버플로 안전 비교.

**Trade-go**: per-channel ring 버퍼 메모리 + ack 처리 비용. nonce reuse(암호화 시) catastrophic — `(sessionId,channel,seq)` 조합으로 회피.

**검증**: clumsy 5% loss + 100ms RTT에서 ReliableOrdered 100% 도달, UnreliableSequenced는 손실 그대로 통과(다음 스냅샷이 덮음), 1MB 스냅샷 fragment 송수신 OK.

### 6.3 Snapshot Delta + AOI (M3)

**왜**: full 스냅샷은 5v5 ~19KB/s/client(추정). 100동접 시 ~15Mbps. delta+AOI로 대역폭 절감 필요.

**무엇/어떻게**: `Snapshot.fbs`에 이미 `deltaBaseTick`/`lastAckedCommandSeq` **필드가 있다**(자리만). 본격화하면:
- 서버가 클라별 마지막 ack된 baseline tick 추적 → 그 baseline 대비 **변경된 EntitySnapshot 필드 + 사라진 netId만** 전송(`EntitySnapshotDelta` + fieldMask 비트).
- 클라가 수신 시 ack(ReliableOrdered piggyback) → 서버가 baseline 메모리 회수.
- **AOI**: 50m grid bucket → 자기 주변 3×3 cell 엔티티만 스냅샷에 포함. + vision(와드/부쉬) 필터 + 거리별 양자화 LOD.

**Trade-off**: baseline 동기화가 깨지면 클라가 **영구 오염** → baseline 미일치 감지 시 자동 full resync 로직 필수. 이게 full-first를 택한 이유.

**검증**: 5v5 룸 평균 스냅샷 <200B, 다른 라인 엔티티 미포함, baseline 미일치 시 full resync 자동.

### 6.4 Lag Compensation rewind (M4)

**왜**: 100ms RTT 클라는 *과거의* 적 위치를 보고 평타를 쏜다. 서버가 현재 위치로 판정하면 "분명 맞췄는데 빗나감". 해법: 서버가 클라가 본 시점으로 **rewind**해서 hit 검증.

**무엇/어떻게**: 인접 도메인(서버권위 시뮬)에 history 기록 인프라는 **있으나** 명령 처리 시 rewindTicks=0(미적용 — 정직성 경계). M4에서 `Frame`에 position+hp+bIsDead 저장 → `GetHistoricalState(entity, pastTick)` → 클라 `clientTick` 시점 상태로 hit 검증 → 200ms 초과 = lag exploit reject + suspicion.

**Trade-off**: 무한 rewind = 치트의 정의 → 200ms 한도. 6 frame 보존 메모리 <5KB/룸.

**검증**: 100ms RTT BA 인정 / 250ms reject, 리플레이 시 hit 결과 동일.

### 6.5 Client Prediction + Reconciliation (M5)

**왜**: 100ms RTT에서 입력 후 100ms 뒤 반응하면 게임이 안 됨. 클라가 즉시 예측 반영하고, 서버 권위 결과가 오면 무봉 보정.

**무엇/어떻게**: ClientInputBuffer(입력 ring) → PredictionWorld(서버 월드 로컬 복사)에서 즉시 실행 → 스냅샷 수신 시 `m_localRng.SetState(snap.rngState)`로 **결정성 동기화** → RollbackEngine이 lastAckedSeq 이후 입력 재실행 → RenderInterpolator가 30Hz→144Hz lerp(snap-to 아닌 lerp로 jitter 마스킹). **스냅샷에 rngState를 싣는 게 여기서 결실**: 같은 RNG 시퀀스 → mispredict 0(이상).

**Trade-off**: 예측-권위 불일치(mispredict) 시 보정 jitter. **서버 권위 결과 우선**(클라가 hit 봐도 서버 miss면 데미지 X) — 이게 권위 모델의 핵심.

**검증**: 100ms RTT 즉시 반응, mispredict <5%, snap-to 없이 jitter <2frame, bit-equal(같은 input+seed → predicted = authoritative).

---

## 7. 면접 예상 질문 & 모범 답변 (12개)

**Q1 (기본). IOCP가 뭐고 왜 thread-per-connection보다 나은가?**
A. IOCP는 비동기 I/O 완료를 큐로 모아 소수 워커가 꺼내 처리하는 모델입니다. thread-per-conn은 연결당 스레드라 1000명이면 1000 스레드 — 컨텍스트 스위칭 폭발에 대부분 recv 블로킹으로 놀고 있죠. IOCP는 I/O를 비동기 게시(AcceptEx/WSARecv)하고 커널이 완료를 완료포트에 넣으면 코어 수만큼의 워커가 처리해, 스레드 수가 연결 수와 무관합니다. 제 코드는 IOContext(OVERLAPPED+버퍼+op)를 단위로 GetQueuedCompletionStatus 후 CONTAINING_RECORD로 컨텍스트를 복원합니다.

**Q2 (기본). TCP인데 왜 framing이 필요한가?**
A. TCP는 스트림이라 메시지 경계가 없습니다. send 두 번이 recv 한 번에 합쳐지거나 쪼개져 옵니다. 그래서 16B 헤더에 payloadSize를 넣는 length-prefixed framing을 씁니다. CFrameParser가 수신 바이트를 누적 버퍼에 쌓다가 (헤더+payload)가 다 모이면 한 프레임을 떼고, 부족하면 NeedMore, magic 불일치면 Invalid로 연결을 끊습니다.

**Q3 (설계). 왜 구조체 memcpy가 아니라 FlatBuffers인가?**
A. raw 구조체는 패딩·엔디안·필드 추가에 와이어 호환이 깨지고, 무엇보다 *신뢰 못 할 네트워크 바이트를 검증 없이 캐스팅*하는 보안 구멍입니다. FlatBuffers는 zero-copy로 버퍼 위에서 바로 읽고, Verifier가 모든 offset이 버퍼 범위 안인지 검증합니다. 제 디스패처는 VerifyCommandBatchBuffer가 통과해야만 GetCommandBatch로 접근하고, 실패하면 FlagSuspicious 후 폐기합니다. C++/Go 스키마 공유도 보너스고요.

**Q4 (설계). 송신은 어떻게 처리하나? 동일 소켓에 WSASend를 여러 번 게시하면?**
A. 그러면 와이어 순서가 깨집니다. 그래서 세션마다 송신 deque + bSendPending 플래그로 **한 번에 하나만 in-flight**입니다. 보낼 게 있는데 이미 전송 중이면 큐에만 넣고, 완료 콜백에서 front를 pop한 뒤 다음을 WSASend합니다. 락은 sendMutex 하나로 큐를 보호합니다.

**Q5 (심화). IOCP에서 세션을 언제 안전하게 해제하나? use-after-free를 어떻게 막나?**
A. 커널이 아직 OVERLAPPED를 참조 중인데 세션을 free하면 use-after-free입니다. 그래서 세션에 atomic pendingIoCount와 bClosing을 두고, WSARecv/WSASend 게시 시 ++ 완료 시 --, CanDestroy()는 *닫는 중 AND pending 0*일 때만 true입니다. Session_Manager가 closing 리스트를 ReapClosingSessions로 돌며 그 조건을 만족한 것만 소거하고, 그동안 shared_ptr로 수명을 연장합니다.

**Q6 (압박 — 레드플래그). "UDP 넷코드를 했다"고 했는데, 실제 UDP 패킷을 주고받습니까?**
A. 아니요, 정직하게 말씀드리면 **UDP는 헤더·스키마·계획 설계 단계**입니다. `UdpPacketHeader.h`(24B 헤더, 3채널 enum)와 `UdpReliabilityChannel.h`(seq/ack/retransmit API 선언), `SeqMath.h`(wrap-safe 비교)까지 박제했지만 .cpp가 없고 게임 코드에 recvfrom/sendto는 0줄입니다. 지금 production은 IOCP TCP고요. 다만 *왜* UDP가 필요한지(TCP HoL blocking으로 1% 손실에도 30Hz 게임이 1~3틱 frozen), 어떻게 포팅할지(TCP 골격 재사용 + datagram이라 framing 제거 + 3채널 reliability)는 M1~M2 계획으로 설계를 끝냈습니다. TCP로 권위 round-trip을 먼저 닫고 전송계층을 교체하는 게 의존성상 옳다고 판단했습니다.

**Q7 (압박 — 레드플래그). 스냅샷에 deltaBaseTick 필드가 있던데, delta 스냅샷 구현했나요?**
A. 아니요. **필드만 있고 항상 full 스냅샷을 보냅니다.** GameRoomReplication에서 매 틱 세션별로 살아있는 엔티티 전부를 직렬화합니다. 의도적인 선택인데, delta는 baseline ack가 깨지면 클라가 영구 오염되니 resync 로직이 선결이고, full은 손실에 강합니다(다음 스냅샷이 전부 덮음). "정확성 먼저, 대역폭은 측정 후 delta"가 1인 프로젝트 우선순위였습니다. delta 설계(EntitySnapshotDelta + fieldMask)와 추정 절감(6.4배)은 M3 계획에 있지만 그건 추정치지 실측이 아닙니다.

**Q8 (압박 — 레드플래그). 동시접속 몇 명까지 버팁니까? 부하 테스트 했나요?**
A. 정직하게, **부하 테스트 수치가 없습니다.** 검증은 localhost 스모크 수준입니다. 구조적으로는 AcceptEx 다중 게시 + IOCP 워커 풀이라 다중 접속을 받을 수 있고, 세션별 송신 큐로 직렬화되지만, "N만 동접"을 주장할 측정은 안 했습니다. 부하 테스트는 안 한 게 맞고, 한다면 다중 클라 시뮬레이터로 동접을 올리며 틱 jitter와 outbound 대역폭을 F-계측으로 측정하는 게 다음 단계입니다.

**Q9 (심화). 결정성이 왜 네트워크 도메인에 중요한가? 스냅샷에 rngState를 왜 싣나?**
A. 결정성은 "같은 입력+같은 seed → 같은 결과"입니다. 이게 깨지면 리플레이도, client prediction의 reconciliation도 불가능합니다. 스냅샷에 rngState/serverTick을 싣는 이유는 클라가 예측 월드를 돌릴 때 m_localRng.SetState로 서버와 bit-equal 동기화해 같은 RNG 시퀀스로 재실행 → mispredict를 0에 가깝게 만들기 위해서입니다(M5 계획). 그래서 스냅샷 빌드도 DeterministicEntityIterator로 EntityID 정렬 순회를 강제합니다 — unordered_map 순회는 hash 의존이라 비결정이거든요.

**Q10 (심화). anti-replay는 어떻게 하나? 악의적 패킷을 어떻게 막나?**
A. 다층입니다. ① framing 단계에서 magic/version 불일치, payloadSize>64KB면 Invalid로 즉시 끊습니다. ② FlatBuffers Verifier가 통과 못 하면 역참조 안 하고 FlagSuspicious. ③ 시퀀스 단계에서 seq ≤ lastProcessed면 리플레이로 거부, seq > last+60이면 갭 점프로 suspicious. ④ 명령 issuer는 클라 주장이 아니라 sessionId→controlledEntity로 서버가 결정합니다(스푸핑 차단). 다만 suspicion은 현재 *집계만* 하고 kick/ban엔 미연결입니다 — 거기까진 아직 안 했습니다.

**Q11 (심화). lag compensation은 적용돼 있나?**
A. history **기록 인프라는 있지만 적용은 미연결**입니다. 명령 처리 시 rewindTicks=0이라 실제 rewind 검증은 안 합니다. 100ms RTT 클라는 과거 적 위치를 보고 쏘므로 서버가 그 시점으로 rewind해 hit 판정해야 하는데, 그게 M4 계획입니다 — Frame에 pos/hp/bIsDead 저장, GetHistoricalState로 clientTick 시점 복원, 200ms 초과는 exploit reject. 지금은 권위 스냅샷을 그냥 직접 적용하는 단계입니다.

**Q12 (확장). UDP로 가면 TCP에서 했던 것 중 뭘 버리고 뭘 새로 만들어야 하나?**
A. TCP가 공짜로 주던 **순서·신뢰성·framing을 직접 만들어야** 합니다. framing은 오히려 *제거*됩니다 — datagram이 곧 메시지 경계라 CFrameParser가 불필요하고 헤더만 파싱하면 됩니다. 새로 만드는 건 ① 주소 기반 세션 식별(UDP는 connect 개념이 없음) ② 3채널 reliability(reliable ordered 입력 / unreliable sequenced 스냅샷) ③ per-channel seq+ack bitfield + RTO 재전송 ④ MTU fragment ⑤ IP spoofing 방어 핸드셰이크. 반대로 IOContext/완료포트/디스패처/룸 골격은 그대로 재사용합니다. 그래서 TCP로 골격을 먼저 닫은 거고요.

---

## 8. 30초 엘리베이터 피치

"멀티플레이의 본질은 *누가 진실을 소유하느냐*입니다. 저는 클라가 결과가 아닌 의도만 보내고 서버가 모든 걸 확정하는 권위 모델을, Windows IOCP 비동기 TCP 위에 직접 구현했습니다. AcceptEx/WSARecv/WSASend 완료포트에 4워커, 16B length-prefixed framing, FlatBuffers Verifier로 신뢰 못 할 바이트를 막고, 30Hz 틱마다 결정론 순서로 full 스냅샷을 세션별 복제하는 round-trip을 닫았습니다. 핵심은 *정직한 경계*인데, UDP 3채널 reliability·snapshot delta·lag comp rewind·client prediction은 헤더와 스키마와 M1~M6 계획서까지 설계를 끝냈지만 아직 .cpp 0줄입니다. 'TCP로 권위 골격을 먼저 닫고 전송계층을 교체한다'는 의존성 판단이고, UDP가 *왜* 필요한지(TCP HoL blocking)부터 *어떻게* 포팅할지까지 막힘없이 설명할 수 있습니다."
