Session - S031 UDP adaptive RTO(RFC 6298)와 결정론적 chaos 검증을 Winters UDP v3 스택의 정본 상태로 고정한다.

# 1. 최종 판정

검증 기준일은 2026-07-14다. 이번 슬라이스는 정본 계획(`.md/plan/2026-07-13_UDP_JOB_SYSTEM_CHASE_LEV_FIBER_IMPLEMENTATION_PLAN.md`)의 production cutover gate 중 **② RTT/pacing/congestion 축의 전반부(RTT estimator + 적응형 RTO)** 를 구현·검증했다.

| 영역 | 상태 | 완료한 것 | 아직 완료하지 않은 것 |
|---|---|---|---|
| RTT estimator | 구현·하네스 검증 완료 | SRTT/RTTVAR(RFC 6298 정수 ms 산술), Karn 규칙(재전송 패킷 샘플 배제), lane별 채널 단위 추정 | per-peer 통합 추정, timestamp echo 기반 정밀 샘플 |
| 적응형 RTO | 구현·하네스 검증 완료 | `RTO = clamp(SRTT + max(10ms, 4·RTTVAR), 60ms, 1000ms)`, 재전송 타임아웃 `min(RTO<<backoff, 2000ms)` | RTT 급변 시 spurious retransmit 감지(F-RTO류) |
| Chaos 검증 | 구현·PASS | 시드 고정 LCG 파이프(손실 25%/중복 10%/지연 10~49ms 재정렬), 200 reliable ordered 메시지 exactly-once/순서 보존, retryExhausted=0 | 실 소켓 경유 WAN chaos soak(패킷 변조/rebind 포함) |
| Pacing/Congestion | 미착수 | — | per-peer send budget, pacing, congestion window (gate ② 후반부) |

이번 슬라이스는 gate ①(post-handshake MAC/AEAD·production ticket validator), gate ③(Snapshot diet), Stage 9 cutover(기본 transport 전환)를 **완료로 주장하지 않는다**. 기본 transport는 여전히 `tcp`다.

# 2. 변경 파일과 핵심 내용

| 파일 | 변경 |
|---|---|
| `Shared/Network/UdpReliabilityChannel.h` | RTO 상수 5종(`kUdpInitialRtoMs=120/kUdpMinRtoMs=60/kUdpMaxRtoMs=1000/kUdpRtoGranularityMs=10/kUdpMaxRetransmitTimeoutMs=2000`) 공개, `OnAck(ackSeq, ackBitfield, nowMs)` 시그니처, `GetSmoothedRttMs/GetCurrentRtoMs/GetRttSampleCount` 접근자, RTT 상태 멤버 4종 |
| `Shared/Network/UdpReliabilityChannel.cpp` | `OnAck`에서 `sendAttempts==1`인 acked 패킷만 RTT 샘플 채취(Karn) → `ApplyRttSample`이 RFC 6298 갱신, `CollectRetransmit`이 고정 120ms 대신 적응 RTO 사용 |
| `Client/Private/Network/Client/UdpClient.cpp` | OnAck 호출 2곳(ServerAccept 처리, associated 패킷 처리)에 기존 `NowMs()` 값 전달 |
| `Server/Private/Network/UdpIocpCore.cpp` | OnAck 호출 2곳(Confirm 처리, associated 패킷 처리)에 기존 `NowMs()` 값 전달, `RunMaintenance` 유휴 만료 판정에 u64 언더플로 가드(아래 리뷰 결함 수정) |
| `Tools/Harness/UdpLoopbackHarness.cpp` | `TestAdaptiveRto`(첫 샘플 40ms→RTO 120, 정상 샘플 수렴→RTO 60 floor, 재전송 타이밍이 적응 RTO를 따름), `TestKarnRetransmitExclusion`(재전송 후 ack는 샘플 폐기), `CChaosPipe`+`TestReliableChaosPipe`(위 chaos 사양) 추가, PASS 라인에 `rto/karn/chaos` 필드 추가 |

vcxproj/.filters 변경 없음(전 파일 기존 배선). GameRoom·Client.vcxproj·Event.fbs·Scene_InGame(S030 Active 소유)은 무접촉.

설계 노트:
- RTT 샘플의 시계는 각 엔드포인트의 로컬 monotonic `NowMs()`다. `lastSendMs`는 `sendAttempts==1`일 때 최초 송신 시각과 동일하므로 Karn 규칙 하에서 샘플이 항상 유효하다.
- 채널이 lane별 인스턴스이므로 RTO도 lane별로 수렴한다. AckOnly 응답은 수신 lane과 같은 lane으로 돌아오므로 lane별 추정이 일관된다.
- 채널 상태 접근은 기존과 동일하게 client `stateMutex` / server `peer->mutex` 아래에서만 일어난다. 새 공유 상태 없음.

# 3. 검증 매트릭스

| 검증 | 결과 |
|---|---|
| `Tools/Harness/RunUdpLoopbackHarness.ps1` (cl /W4 /WX, UdpIocpCore+UdpClient+ReliabilityChannel 공동 컴파일) | `PASS codec=1 ordered=1 laneAck=1 rto=1 karn=1 chaos=1 reassembly22104=1 live=1` (리뷰 수정 반영 후 재실행 PASS) |
| `Server.vcxproj` Debug x64 `/m:1` | PASS (기존 Engine DLL 경계 C4251 경고만) |
| `Client.vcxproj` Debug x64 `/m:1` | PASS |
| UDP F5 smoke (`--net-transport=udp --udp-dev-allow-empty-ticket --smoke-seconds=90` 외부 서버 실구동) | `hello=1 lobby=1 invalid=0 recvDg=5 sendDg=5 retransmit=0 drops=0` — S023 기준선과 동일 |
| dual 모드(같은 9000 포트 TCP IOCP + UDP 병행) + UDP smoke | PASS (동일 카운터) |

주의: F5 smoke 서버는 콘솔 stdin이 닫히면 `q` 대기 루프가 EOF로 즉시 종료된다. 자동화에서는 `--smoke-seconds=N`으로 기동해야 한다(이번 세션에서 재확인).

TCP 단독 경로는 이번 diff가 접촉하지 않으므로(변경 파일 전부 UDP 전용 + 공유 신뢰성 채널은 UDP에서만 사용) 별도 TCP 스모크는 수행하지 않았다. dual 모드 기동이 TCP IOCP listen 회귀를 간접 확인한다.

# 4. 롤백 범위

이 문서의 변경 파일 5종의 이번 diff만 원복하면 된다. S023 as-built(미커밋 UDP 스택)와 S030 진행분은 무접촉.

# 5. 다음 슬라이스

1. **gate ① 보안**: `UdpClientConnectPayload`/`UdpServerAcceptPayload`에 ECDH P-256 공개키 필드 추가 → BCrypt secret agreement + HKDF로 방향별 키 유도 → post-handshake 전 datagram에 16B truncated HMAC-SHA256 tag(fragment payload 예산 1,144→1,128B 재계산) → 변조 drop 하네스. production ticket validator는 S030의 계정 백엔드(Services auth)와 TCP GameStart 티켓 발급에 의존하므로 S030 Handoff 후 배선.
2. **gate ② 후반부**: per-peer send budget/pacing.
3. **gate ③ Snapshot diet**: S030 Handoff 후(GameRoom 계열 파일 소유 해제 후) 착수.

# 6. 미커밋 리스크 (S023부터 누적)

UDP v3 핵심 파일 다수가 여전히 untracked다(`UdpPacketCodec.h`, `UdpHandshake.h`, `UdpIocpCore.*`, `ServerSessionHub.*`, `UdpClient.cpp`, `PacketSemantics.h`, Udp 하네스 전부 등). `git clean -fd` 한 번이면 S023~S031 산출물의 컴파일 유닛 대부분이 소실된다. **사용자 승인 checkpoint commit이 시급하다.**
