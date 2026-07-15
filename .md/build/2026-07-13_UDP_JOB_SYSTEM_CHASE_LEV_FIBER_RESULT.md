Session - S023 UDP·JobSystem·Chase-Lev·Fiber runtime 구현과 검증 결과를 Winters 코드의 정본 상태로 고정한다.

# 1. 최종 판정

검증 기준일은 2026-07-13이다. 이번 작업은 다음 두 수직 슬라이스를 실제 Server/Client 실행 경로까지 연결하고 자동 검증했다.

1. JobSystem Submit lifetime과 Chase-Lev size-1 경쟁을 수정하고, `ThreadOnly`, `FiberShell`, `FiberFull`을 Engine과 Server lifecycle에 연결했다.
2. TCP에 묶여 있던 application frame 전송 책임을 transport-neutral session hub로 옮기고, UDP v3 handshake·lane policy·reliability·ordering·fragmentation·IOCP 송수신을 feature-gated runtime으로 연결했다.

다만 “구현 완료”와 “production 전환 완료”는 같은 말이 아니다.

| 영역 | 정본 상태 | 완료한 것 | 아직 완료하지 않은 것 |
|---|---|---|---|
| Submit race / lifetime | 구현·stress 완료 | immutable `WorkItem*`, admission/shutdown 경계, counter rollback, exception-safe completion | Application Verifier/장시간 sanitizer soak |
| Chase-Lev deque | 구현·직접 경합 검증 완료 | fixed 4,096 pointer slot, owner bottom, thief top, last-item CAS 복구 | 동적 resize deque |
| Fiber scheduler | 구현·stress 완료 | 세 실행 모드, worker당 64-fiber pool, wait/resume, origin pinning, FLS | cross-worker fiber migration |
| Server Fiber lifecycle | 구현·runtime probe 완료 | CLI, `CServerEntry`, startup self-test, shutdown order | GameRoom workload jobification과 speedup 측정 |
| UDP runtime migration | feature-gated 수직 슬라이스 구현·smoke 완료 | v3 codec, handshake, lane semantics, IOCP, Hub, Client facade, TCP/UDP dual | production 보안·혼잡 제어·snapshot diet·실전 soak |
| TCP game session | rollback 기본 경로 유지 | `tcp`, `udp`, `dual` 선택 | UDP parity/production gate 뒤 제거 여부 결정 |
| S022 PyTorch BC shadow | 경계 보존·통합 검증 완료 | S022 source freeze, Jax POD padding 결정론 수정, solution 통합 build | active learned policy 승격은 S022 범위 밖 |
| Fiber 6주 학습 프로그램 | 미착수 | runtime 구현은 완료 | 계획된 장기 mastery 과정 전체 |

즉, Winters는 더 이상 “UDP 선언만 존재”하거나 “Fiber shell만 존재”하는 상태가 아니다. 실제 실행 가능한 UDP/Fiber vertical slice는 있다. 그러나 UDP는 기본 TCP를 대체한 production transport가 아니고, Fiber 역시 GameRoom tick을 병렬화해 성능을 높였다고 주장할 단계는 아니다.

# 2. 개념의 본질과 Winters 연결

## 2.1 JobSystem: thread 수가 아니라 작업의 소유권·공개·완료 계약

JobSystem의 핵심은 함수를 여러 thread에 던지는 것이 아니다.

```text
완전히 생성된 논리 작업
-> 실행 가능 queue에 publish
-> 정확히 한 worker가 소유권 획득
-> 성공/예외와 관계없이 완료를 정확히 한 번 기록
-> dependency를 기다리던 실행 흐름을 깨움
```

따라서 올바름은 다음 네 질문으로 결정된다.

- callable과 counter가 dequeue 전에 완전히 생성되었는가?
- publish의 release와 consume의 acquire 사이에 happens-before가 존재하는가?
- queue 실패, callable 예외, shutdown 경쟁에서도 counter가 정확히 한 번 끝나는가?
- local deque bottom을 정말 그 deque의 owner worker만 변경하는가?

Winters에서는 [JobSystem.h](../../Engine/Public/Core/JobSystem.h)와 [JobSystem.cpp](../../Engine/Private/Core/JobSystem.cpp)가 이 계약을 소유하고, [WorkStealingDeque.h](../../Engine/Public/Core/JobSystem/WorkStealingDeque.h)는 ready work의 owner/thief 경쟁만 소유한다. Shared/GameSim에는 Engine scheduler 타입을 노출하지 않는다.

## 2.2 Chase-Lev: 한쪽은 owner, 반대쪽은 경쟁자

Chase-Lev deque는 대칭 queue가 아니다.

```text
owner worker: bottom에서 Push/Pop
thief worker: top에서 Steal
```

대부분의 원소는 owner와 thief가 다른 끝을 만져 싸우지 않는다. 원소가 하나 남을 때만 owner `Pop`과 thief `Steal`이 같은 `top` CAS를 경쟁한다. 이 size-1 전이가 알고리즘의 본질적인 선형화 지점이다.

Deque가 thread-safe라는 말만으로는 충분하지 않다. owner identity가 JobSystem instance까지 포함되어야 한다. JobSystem A의 worker가 JobSystem B에 submit할 때 worker index가 우연히 같아도 B의 bottom을 만지면 안 된다. Winters는 동일 system의 owner worker만 local push하고, 외부 thread·foreign worker·overflow는 global injection queue로 보낸다.

## 2.3 Fiber: OS thread가 아니라 기다리는 실행 스택을 park한다

Fiber는 더 빠른 thread가 아니다. user mode에서 실행 스택과 instruction pointer를 바꾸는 cooperative scheduling primitive다.

```text
job fiber가 counter를 기다림
-> origin worker의 root fiber로 SwitchToFiber
-> 같은 OS worker가 다른 ready job 실행
-> counter 목표 도달
-> 기다리던 fiber를 origin worker inbox에 ready 등록
-> origin worker가 해당 스택을 resume
```

이 구조의 이득은 dependency wait 때문에 worker OS thread가 놀지 않는 것이다. 반대로 socket I/O나 mutex를 fiber로 바꾼다고 자동으로 빨라지지 않는다. IOCP는 kernel I/O completion을 전달하는 메커니즘이고, Fiber는 이미 실행 중인 CPU job이 dependency를 기다리는 방식을 바꾸는 메커니즘이다. Winters는 둘을 분리했다.

## 2.4 TCP에서 UDP로: 신뢰성을 없애는 것이 아니라 책임을 application으로 이동

TCP는 byte stream에 대해 연결 상태, 순서, 재전송, 중복 제거, 흐름/혼잡 제어를 제공한다. UDP는 datagram 경계만 보존하며, delivery policy는 application이 선택해야 한다.

```text
TCP:
  application message + kernel이 제공하는 하나의 ordered reliable stream

Winters UDP:
  application message
  + logical connection/handshake
  + lane별 reliability와 ordering
  + packet/message sequence
  + fragmentation/reassembly
  + queue/memory cap
```

Snapshot은 오래된 상태를 재전송하는 것보다 최신 완성본이 중요하고, Lobby·Command·Event는 손실 없이 순서대로 도달해야 한다. 그래서 “모두 reliable”이나 “모두 unreliable”이 아니라 packet 의미별 lane을 둔다.

## 2.5 Windows와 Visual Studio에서의 실제 연결

Windows primitive와 Winters 계층의 관계는 다음과 같다.

| Windows/언어 기능 | Winters 역할 | 실제 위치 |
|---|---|---|
| C++20 atomic release/acquire/CAS | WorkItem pointer publish, Chase-Lev 선형화 | Engine JobSystem/Deque |
| `ConvertThreadToFiberEx`, `CreateFiberEx`, `SwitchToFiber` | worker root와 pooled job fiber 전환 | Engine JobSystem/Fiber |
| FLS (`FlsAlloc/Get/SetValue`) | fiber를 따라가야 하는 submission/profiler context | Engine JobSystem/Profiler |
| `WSASocketW(SOCK_DGRAM)`, `WSARecvFrom`, `WSASendTo` | overlapped UDP datagram I/O | Server `CUdpIocpCore` |
| `WSASocketW(SOCK_DGRAM)`, `recvfrom`, `sendto` | dedicated receive worker 기반 UDP I/O | Client `CUdpClient` |
| IOCP / `GetQueuedCompletionStatus` | Server의 overlapped UDP completion 분배 | Server UDP core |
| MSVC x64 Debug projects | ABI/header/project wiring 통합 검증 | `Winters.sln`, 각 `.vcxproj` |

Visual Studio의 `/m:1`은 runtime 설계가 아니라 이번 shared dirty checkout에서 object/PDB와 generated source 충돌을 막기 위한 build orchestration 선택이다. 최종에는 개별 핵심 target과 solution 전체를 모두 순차 빌드했다.

# 3. JobSystem·Chase-Lev as-built

## 3.1 Submit race 수정

기존 위험은 `std::function`을 포함한 non-trivial `WorkItem` value를 ring slot에 직접 두는 설계였다. producer가 slot object를 쓰는 중 consumer가 move/read하면 top/bottom atomic이 올바르더라도 C++ object lifetime data race가 된다.

현재 publish 순서는 다음과 같다.

```text
std::function과 immutable WorkItem node 완전 생성
-> counter를 publish 전에 증가
-> owner deque의 atomic WorkItem* slot 또는 global queue에 공개
-> 성공한 consumer만 pointer 소유권 획득
-> callable을 exception boundary 안에서 실행
-> counter exactly-once 감소
-> WorkItem node exactly-once 파괴
```

추가로 반영한 lifecycle 불변식은 다음과 같다.

- Submit admission lease와 Shutdown은 mutex/CV 경계로 직렬화한다.
- publish 실패나 allocation/queue 실패는 counter를 rollback하고 node를 파괴한다.
- 같은 JobSystem의 worker identity일 때만 local bottom에 push한다.
- 외부 thread와 foreign JobSystem worker는 global queue를 사용한다.
- callable 예외는 worker loop를 죽이지 않고 failure metric을 기록한 뒤 counter를 완료한다.
- Shutdown은 신규 admission을 닫고, 이미 accepted된 작업과 외부 wait lease를 정리한 뒤 worker를 join한다.

핵심 코드는 [JobSystem.cpp](../../Engine/Private/Core/JobSystem.cpp), public contract는 [JobSystem.h](../../Engine/Public/Core/JobSystem.h), counter contract는 [JobCounter.h](../../Engine/Public/Core/JobCounter.h)에 있다.

## 3.2 Chase-Lev last-item 결함

기존 마지막 원소 경합은 다음 형태였다.

```text
t = top
CAS(t, t + 1)
bottom = t + 1
```

`compare_exchange_strong`은 실패하면 expected 인자 `t`를 실제 current top으로 덮어쓴다. thief가 먼저 이겼다면 `t`가 이미 `oldTop + 1`이므로 `bottom = t + 1`은 `oldTop + 2`까지 과진행한다. 그 결과 비어 있는 deque의 크기가 0이 아닌 phantom state가 될 수 있다.

수정은 CAS 전에 마지막 index를 보존하는 것이다.

```text
lastIndex = t
CAS(t, t + 1)
bottom = lastIndex + 1
```

[WorkStealingDeque.h](../../Engine/Public/Core/JobSystem/WorkStealingDeque.h)는 4,096개의 고정 `std::atomic<WorkItem*>` slot을 사용한다. pointer가 publish token이므로 non-trivial callable 자체를 원자 slot에서 동시에 생성·이동하지 않는다.

## 3.3 직접 검증 수치

| Probe | 결과 |
|---|---:|
| last-item owner/thief 경쟁 | 100,000회 PASS |
| owner Pop 승리 | 59,659 |
| thief Steal 승리 | 40,341 |
| exactly-one/value/size 위반 | 0 |
| 경과 시간 | 59.2859 ms |
| ThreadOnly fan-out + caller help | 100,000 jobs / 243.940 ms / 409,936 jobs/s |
| ThreadOnly pure worker | 100,000 jobs / 227.589 ms / 439,389 jobs/s |
| FiberFull fan-out + caller help | 100,000 jobs / 197.613 ms / 506,038 jobs/s |
| FiberFull pure worker | 100,000 jobs / 181.521 ms / 550,902 jobs/s |

두 fan-out 시간은 동일한 정밀 benchmark 환경에서 반복 통계·주파수·affinity를 통제한 결과가 아니므로 FiberFull 우위를 증명하지 않는다. 정확성 stress에서 관찰한 실행 시간이다.

# 4. FiberFull과 Server 적용

## 4.1 세 실행 모드

| Mode | 의미 | 기본값 |
|---|---|---|
| `ThreadOnly` | worker stack에서 실행하고 wait 중 help-execute | Server/Engine 기본 |
| `FiberShell` | Fiber API와 전환 lifecycle을 진단하는 shell | 명시적 opt-in |
| `FiberFull` | pooled fiber가 counter에서 suspend/resume | 명시적 opt-in |

`FiberFull`의 worker별 구조:

- fiber 64개를 선할당하고 재사용한다.
- stack commit 64 KiB, reserve 256 KiB다.
- `FIBER_FLAG_FLOAT_SWITCH`로 floating-point state를 보존한다.
- wait fiber는 생성된 origin worker에 고정한다.
- completion worker는 target worker의 ready inbox에만 알리고 직접 `SwitchToFiber`하지 않는다.
- pool이 포화되면 correctness를 보존하는 inline help fallback을 사용한다.

## 4.2 lost wakeup과 origin pinning

Counter 확인과 waiter 등록 사이에 zero transition이 끼면 영원히 잠드는 lost wakeup이 생긴다. 현재는 waiter mutex 아래에서 target을 재검사하고 `Waiting` 상태를 publish한 뒤 등록한다. completion은 같은 보호 경계에서 `Waiting -> Ready`를 한 번만 전이시키고 origin inbox에 넣는다.

Fiber는 thread와 다른 실행-local context를 가진다. `thread_local` 값은 같은 worker thread 위의 여러 fiber가 공유하므로 yield를 넘는 submission/profiler context는 FLS 또는 explicit fiber record로 관리한다. waiter/ready lock, GameRoom lock, live socket/OVERLAPPED 소유권을 잡은 채 yield하지 않는다.

## 4.3 stress 결과

| Probe | 결과 |
|---|---:|
| nested counter wait/resume | 16 / 16 |
| interleave wait/resume | 4 / 4 |
| interleave inline executions | 5 |
| saturation parents | 80 |
| saturation wait/resume | 64 / 64 |
| pool misses | 17 |

64개 pool보다 많은 parent가 대기한 saturation에서도 64개가 suspend/resume되고 나머지는 inline help로 진행해 deadlock이나 누락 없이 종료했다.

## 4.4 Server lifecycle과 CLI

[ServerEntry.h](../../Server/Public/Game/ServerEntry.h)와 [ServerEntry.cpp](../../Server/Private/Game/ServerEntry.cpp)는 JobSystem의 `Stopped -> Initializing -> Ready -> Stopping` lifecycle과 실패 rollback을 소유한다. [Server main](../../Server/Private/main.cpp)은 다음 option을 fail-closed로 파싱한다.

```text
--job-mode=thread|fiber-shell|fiber-full
--job-workers=1..256
```

기본 worker 수는 `max(1, hardware_concurrency - 6)`이고 기본 mode는 `ThreadOnly`다. Server startup self-test는 parent/child counter wait를 서버 binary 안에서 실행해 선택한 mode의 scheduler 경로를 확인한다.

실행 probe 결과:

- UDP F5 + FiberFull: jobs `2/2`, switches `6`, waits/resumes `1/1`.
- FiberShell Server probe: switches `4`.

중요한 경계: 현재 authoritative [GameRoom](../../Server/Public/Game/GameRoom.h) tick은 여전히 serial single-writer다. JobSystem과 Fiber lifecycle은 Server에 실제 연결되었지만, world mutation이나 live session/socket을 worker에 넘기지 않았다. 따라서 GameRoom workload의 speedup은 아직 구현·측정하지 않았다.

후속 병렬화의 안전한 단위는 다음이다.

```text
serial authoritative world
-> tick 경계에서 immutable ReplicationFrame DTO 생성
-> recipient/partition encode jobs
-> sessionId 기준 deterministic merge
-> transport outbound queue
```

Job이 mutable GameRoom, `CSession`, socket, renderer/ECS 객체를 캡처하지 않는 것이 확장성의 핵심이다.

# 5. UDP v3 as-built

## 5.1 wire와 lane policy

[UdpPacketHeader.h](../../Shared/Network/UdpPacketHeader.h)와 [UdpPacketCodec.h](../../Shared/Network/UdpPacketCodec.h)는 native packed struct memcpy 대신 명시적 big-endian v3 codec을 사용한다.

| 항목 | 값 |
|---|---:|
| datagram budget | 1,200 B |
| packet header | 40 B |
| fragment header | 16 B |
| fragment payload | 1,144 B |
| maximum logical message | 64 KiB |
| maximum fragment count | 64 |
| reliable outstanding cap | 32 datagrams/lane |

[PacketSemantics.h](../../Shared/Network/PacketSemantics.h)의 현재 policy:

| Lane | Delivery | 이유 |
|---|---|---|
| Control | ReliableOrdered | handshake/control state |
| Lobby | ReliableOrdered | lobby/BanPick/GameStart state |
| Command | ReliableOrdered | authoritative input 손실 방지 |
| Event | ReliableOrdered | one-shot event 보존 |
| Snapshot | UnreliableSequenced | 오래된 state보다 최신 완성본 우선 |
| Heartbeat | UnreliableSequenced | 다음 heartbeat로 대체 가능 |

Packet sequence는 datagram ACK/retry를, message sequence는 reassembly 이후 ordered delivery를 식별한다. reliability/ACK window는 lane별이라 Snapshot fragment가 reliable Control/Event의 32-bit ACK 관찰 범위를 밀어내지 않는다.

## 5.2 handshake와 연결 identity

[UdpHandshake.h](../../Shared/Network/UdpHandshake.h)의 흐름:

```text
ClientHello(nonce)
-> ServerRetry(stateless HMAC cookie)
-> ClientConnect(cookie, ticket)
-> ServerAccept(connectionId, generation, sessionId)
-> ClientConfirm
```

Cookie 검증 전에는 큰 peer state를 만들지 않는다. `connectionId + generation`이 logical association을 식별하고 duplicate handshake는 idempotent하게 처리한다. Confirm callback이 logical session mapping을 publish하기 전 application packet을 ACK하지 않는 activation barrier를 둔다.

## 5.3 Server IOCP와 transport-neutral Hub

[UdpIocpCore.h](../../Server/Public/Network/UdpIocpCore.h) / [UdpIocpCore.cpp](../../Server/Private/Network/UdpIocpCore.cpp)는 실제 `SOCK_DGRAM` socket, `WSARecvFrom`, `WSASendTo`, IOCP completion worker를 사용한다. 각 outstanding receive/send는 completion까지 독립 `OVERLAPPED`, buffer, endpoint storage를 소유한다.

[ServerSessionHub.h](../../Server/Public/Network/ServerSessionHub.h) / [ServerSessionHub.cpp](../../Server/Private/Network/ServerSessionHub.cpp)는 transport와 GameRoom 사이의 책임 경계다.

```text
TCP CSession 또는 UDP connection
-> 공통 logical sessionId
-> owned application frame
-> bounded ingress queue
-> GameRoom tick 시작에서 DrainIngress
-> serial authoritative mutation

GameRoom outbound frame
-> Hub SendFrame(sessionId, type, payload)
-> TCP PacketEnvelope 또는 UDP raw application message
```

UDP IOCP callback은 GameRoom을 직접 mutate하지 않는다. `(connectionId, generation)` active/tombstone 상태와 ingress gate를 drain 시점에 다시 확인해 지연 frame을 버린다. Queue는 총 4,096 event, frame 최대 3,968 + control reserve 128, 8 MiB byte cap, tick당 drain 512, logical session 최대 2,048로 bounded다. shutdown은 admission/outbound gate를 먼저 닫고 stale frame과 in-flight send lease를 정리한다.

TCP inbound는 기존 IOCP/GameRoom 경로를 rollback 호환으로 유지하고, UDP inbound가 Hub의 tick single-writer queue를 사용한다. TCP까지 완전히 같은 ingress queue로 통합했다고 과장하지 않는다.

최종 P1 감사에서 transport 실패 의미도 다음처럼 닫았다.

- UDP logical-session activation이 capacity/gate에서 거절되면 hub lock 밖에서 core peer를 제거하고, callback 복귀 후 registry identity와 `confirmed`를 다시 확인해 activation ACK를 금지한다.
- `ReliableOrdered` outbound가 reliable/outbound cap에 걸리면 조용히 message를 잃지 않고 해당 UDP association을 disconnect하는 fail-closed 정책을 쓴다. Unreliable Snapshot queue-full은 latest-wins drop으로 남긴다.
- TCP send queue는 256 packet/8 MiB로 bounded하며, overlapped `WSASend` partial completion은 `m_sendOffset` 이후의 나머지만 재게시한다. 전체 frame이 끝난 뒤에만 queue에서 제거한다.
- TCP initial recv와 recv/send repost 실패는 `CSession_Manager`의 idempotent teardown으로 귀결되어 Hub, GameRoom, dispatcher와 socket 상태가 함께 retire된다. Socket handle과 close gate도 atomic exchange라 concurrent failure가 double-close하지 않는다. GameRoom send 중 즉시 실패는 room mutex 재진입을 피하려 socket close 후 outstanding IOCP completion이 manager teardown을 수행한다.

## 5.4 Client facade

[ClientNetwork.h](../../Client/Public/Network/Client/ClientNetwork.h) / [ClientNetwork.cpp](../../Client/Private/Network/Client/ClientNetwork.cpp)는 `Tcp`와 `Udp` transport를 명시적으로 선택하고 공통 `SendFrame`을 제공한다. TCP는 `PacketEnvelope`를 구성하고 UDP는 [UdpClient](../../Client/Private/Network/Client/UdpClient.cpp)에 raw application frame을 넘긴다.

[GameSessionClient.cpp](../../Client/Private/Network/Client/GameSessionClient.cpp)는 `--net-transport=tcp|udp`를 strict parse하며 기본은 TCP다. `CUdpClient`는 bind-before-receive, reconnect 시 기존 worker join, `WSAEMSGSIZE` invalid-drop 지속 처리와 bounded Pump 경계를 갖는다.

## 5.5 Server transport option과 Debug ticket gate

[Server main](../../Server/Private/main.cpp)의 선택지는 다음과 같다.

```text
--net-transport=tcp|udp|dual
--udp-dev-allow-empty-ticket
```

기본은 `tcp`다. 현재 repository에는 production ticket validator가 없으므로 `udp` 또는 `dual`을 선택하면서 Debug-only `--udp-dev-allow-empty-ticket`를 주지 않으면 startup이 exit code `5`로 실패한다. Release에서 이 flag도 거부한다. 따라서 empty ticket은 인증 구현이 아니라 localhost 개발 gate다.

Lifecycle 순서도 고정했다.

```text
ServerEntry/JobSystem initialize + startup probe
-> SessionHub attach
-> GameRoom start
-> TCP/UDP transport start
...
-> Hub BeginShutdown
-> TCP/UDP transport shutdown + IOCP worker join
-> GameRoom stop/finalize
-> Hub detach
-> ServerEntry/JobSystem shutdown
```

# 6. 실제 network·runtime 검증

## 6.1 UDP protocol loopback

[UdpLoopbackHarness.cpp](../../Tools/Harness/UdpLoopbackHarness.cpp)의 결과:

```text
[UdpLoopbackHarness] PASS
codec=1 ordered=1 laneAck=1 reassembly22104=1 live=1
```

즉 22,104 B 실제 최대 replay Snapshot 크기에 대한 fragmentation/reassembly와 live socket 수직 슬라이스를 함께 통과했다.

## 6.2 F5 Server/Client smoke

[UdpF5SessionSmoke.cpp](../../Tools/Harness/UdpF5SessionSmoke.cpp)로 외부 Server binary를 실제 구동해 확인한 결과:

| 항목 | 결과 |
|---|---:|
| FiberFull startup jobs | 2/2 |
| fiber switches | 6 |
| waits/resumes | 1/1 |
| Client Hello / LobbyState | 1 / 1 |
| Client invalid datagrams | 0 |
| Client recv/send datagrams | 5 / 5 |
| retransmit / drop | 0 / 0 |
| Server stale drop | 0 |
| Server ingress overflow disconnect | 0 |
| Server outbound reject | 0 |

추가 transport 회귀:

- TCP smoke: receive `480`, PASS.
- `dual` mode: 같은 numeric port의 TCP와 UDP 세션을 함께 기동해 둘 다 PASS.
- UDP 인증 flag 누락: 의도한 fail-closed exit code `5`.
- FiberShell Server startup probe: switches `4`, PASS.

# 7. Replay payload와 확장성 수치

측정 원본은 [Replay payload JSON](./2026-07-13_NETWORK_REPLAY_PAYLOAD_MEASUREMENT.json), 재현 도구는 [MeasureReplayNetworkPayload.py](../../Tools/Harness/MeasureReplayNetworkPayload.py)다. 최신 `room1_tick1_1786.wrpl`을 full Snapshot 30 Hz로 계산했다.

| 지표 | 값 |
|---|---:|
| Snapshot count | 1,786 |
| 평균 payload | 15,415.77 B |
| p50 / p95 / max | 16,256 / 19,808 / 22,104 B |
| payload rate/client | 451.634 KiB/s |
| application UDP wire/client | 474.345 KiB/s |
| datagrams 평균 / p95 / max | 13.843 / 18 / 20 |
| IPv4+UDP downstream/client | 485.70 KiB/s |
| immediate ACK uplink/client | 27.58 KiB/s |
| 5 client 양방향 합계 | 약 2.506 MiB/s |

p95 Snapshot 19,808 B를 1,144 B fragment payload 4개 이하로 맞추려면 약 4,576 B 이하가 필요하다. 현재 p95에서 약 **76.9%**를 줄여야 한다.

이 수치의 의미는 “UDP가 느리다”가 아니다. Transport를 바꿔도 full authoritative Snapshot 자체가 크다는 뜻이다. 확장성은 다음 순서로 확보해야 한다.

1. AI/debug telemetry를 production Snapshot에서 분리한다.
2. team vision/AOI로 수신자에게 필요한 entity만 보낸다.
3. transform/stat을 quantize하고 change mask를 둔다.
4. acknowledged baseline 기반 delta와 baseline miss full resync를 구현한다.
5. Snapshot을 latest-complete coalescing하고 reliable control/event보다 낮은 우선순위로 pacing한다.

5-client 약 2.506 MiB/s는 localhost 계산 기준이며 Ethernet/IP option, loss retry, event/control burst, 실제 WAN congestion을 포함한 capacity 숫자가 아니다.

# 8. S022 PyTorch/IL 세션 경계와 결정론 수정

S022의 정본 결과는 [S022 PyTorch BC Shadow Policy 보고서](./2026-07-13_S022_PYTORCH_BC_SHADOW_POLICY_REPORT.md)에 있다. S022는 Tools/AIResearch, SimLab, Shared/GameSim ChampionAI, Snapshot/F9 debug seam과 Server policy injection을 소유했고, S023은 Engine Job/Fiber와 Shared/Server/Client Network를 소유했다.

같은 dirty checkout에서 partial `WorkItem -> WorkItem*` 전환 중 S022 SimLab build가 한 번 중간 source를 읽은 뒤, build gate를 다음처럼 강화했다.

```text
MSBuild process lock
+ dependency source freeze
+ generated/project wiring freeze
```

Shadow non-interference의 마지막 byte equality 실패는 정책 간섭이 아니었다. raw POD keyframe이 `JaxSimComponent`의 implicit alignment padding을 `memcpy` 직렬화하면서 미초기화 3 byte를 포함한 것이 원인이었다. [JaxSimComponent.h](../../Shared/GameSim/Components/JaxSimComponent.h)는 다섯 곳의 3-byte implicit padding을 zero-initialized reserved array로 명시했다.

- `sizeof(JaxSimComponent) == 60` 유지
- 다음 float field offset `4/20/32/44/56` 유지
- active/shadow command sequence, authoritative hash, final intent/action은 동일
- shadow evaluated/disagreed 16회는 유지
- keyframe byte equality 회복

이는 ML policy 결과를 바꾼 수정이 아니라 raw POD save의 결정론을 보장한 serialization hygiene다. S022 Python 검증은 신규 episode-boundary fail-closed 테스트를 포함해 `71/71 PASS`였다.

# 9. Build·검증 매트릭스

## 9.1 Build

공유 checkout 충돌을 피하기 위해 `/m:1`로 순차 실행했다.

| Target | Configuration | 결과 |
|---|---|---|
| Engine | Debug x64 | PASS, EngineSDK header sync |
| GameSim | Debug x64 | PASS |
| SimLab | Debug x64 | PASS |
| Server | Debug x64 | PASS |
| Client | Debug x64 | PASS |
| `Winters.sln` 전체 | Debug x64 `/m:1` | PASS |

최종 P1 transport 보정 뒤 solution을 다시 빌드했다. 전체 PASS 범위는 Engine, GameSim, Server, Client, AssetConverter, SimLab, EldenRingClient다. 관찰된 경고는 기존 Engine DLL 경계의 C4251/C4275이며, 앞선 통합 build에서는 이번 범위 밖 기존 `Client/Private/Game/ChampionSpawnService.cpp:190` C4477도 관찰됐다.

## 9.2 자동 검증 요약

| 검증 | 결과 |
|---|---|
| JobSystem thread/shell/fiber/all stress | PASS |
| Chase-Lev size-1 race 100,000회 | PASS |
| Fiber nested/interleave/saturation/shutdown | PASS |
| UDP codec/ordering/lane ACK/22,104 B reassembly/live socket | PASS |
| UDP F5 Hello + LobbyState + FiberFull | PASS |
| TCP rollback smoke | PASS |
| TCP+UDP dual smoke | PASS |
| UDP auth fail-closed | PASS, exit 5 |
| S022 Python | PASS, 71/71 |
| Shared boundary/project XML/scoped diff checks | PASS |

# 10. Production cutover 전 남은 한계

현재 custom UDP를 외부 production 기본 경로로 바꾸면 안 되는 이유를 명시한다.

- Handshake cookie HMAC은 있으나 post-handshake datagram MAC/AEAD가 없다. on-path 변조와 ACK spoof 방어가 완료되지 않았다.
- 고정 RTO/retry cap은 있으나 RTT estimator, pacing, congestion controller, fast retransmit이 없다.
- IPv4 개발 경로이며 IPv6, NAT rebinding 인증, PMTU discovery가 완료되지 않았다.
- graceful disconnect/close ACK가 없고 idle timeout에 의존한다.
- Debug empty-ticket validator는 production authentication이 아니다.
- immediate ACK-only가 fragment마다 발생해 uplink overhead가 크다.
- reliable outstanding cap은 bounded correctness를 제공하지만 큰 reliable logical message를 항상 수용하지는 않는다.
- full Snapshot은 p95 18 datagram이며 AOI/delta/quantization/baseline이 없다.
- Event stable identity, reconnect/resume, WAN loss/reorder/duplication chaos soak가 production gate로 남아 있다.
- TCP process-exit 순서는 transport worker를 room보다 먼저 join하지만, accepted session 전체의 completion-aware graceful drain과 in-process hot restart는 후속 부채다.
- FiberFull은 origin-worker pinned이며 circular dependency와 counter lifetime 위반은 caller bug다.
- GameRoom은 serial authoritative tick이다. scheduler wiring만으로 gameplay speedup을 주장할 수 없다.

# 11. 최종 구조와 다음 단계

이번 구현이 만든 확장 가능한 경계는 다음과 같다.

```text
Shared/GameSim authoritative truth
  (Engine/Fiber/Winsock 비의존)
        |
Server serial GameRoom single writer
        |
ServerSessionHub: logical session + owned application frame
        |                         |
TCP rollback transport       UDP v3 transport

Server immutable replication DTO  -- future --> JobSystem encode jobs
                                                ThreadOnly/FiberFull parity
```

후속 우선순위는 다음이다.

1. post-handshake AEAD/MAC와 production ticket validator를 먼저 닫는다.
2. RTT estimator·pacing·congestion과 WAN chaos/soak를 추가한다.
3. Snapshot p95를 4 datagram 이하로 줄이는 AOI/delta/quantization을 구현한다.
4. immutable replication DTO만 jobify하고 ThreadOnly/FiberFull byte/hash parity를 검증한다.
5. UDP parity와 rollback 기준을 만족한 뒤에만 F5 기본을 TCP에서 UDP로 전환한다.
6. 별도의 6주 Fiber mastery 프로그램은 runtime 구현 완료와 구분해 시작 여부를 결정한다.

결론적으로 S023은 **Job/Fiber scheduler와 Server lifecycle 구현·검증**, **UDP feature-gated runtime migration과 TCP/UDP/dual smoke**를 완료했다. **Production UDP cutover**, **GameRoom 병렬 workload speedup**, **6주 학습 프로그램**은 완료로 주장하지 않는다.
