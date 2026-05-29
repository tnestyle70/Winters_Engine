# UDP M1-M3 Transport Reliability Delta AOI

Session - LoL production netstack 방향으로 UDP transport, reliability, full snapshot, delta, AOI를 M1-M3 범위에서 구현한다.

## 1. 반영해야 하는 코드

### 1-1. C:/Users/user/Desktop/Winters/Shared/Network/UdpPacketHeader.h

새 파일:
- `eUdpChannel`, `UdpPacketHeader`, `UdpFragmentHeader`, `SeqGreater`, `SeqDistance`를 둔다.

반영:
- channel은 `ReliableOrdered`, `ReliableUnordered`, `UnreliableSequenced` 3개로 시작한다.
- ack 의미는 `ackSeq = newest received`, `ackBitfield = ackSeq 이전 32개 수신 mask`로 고정한다.
- MTU payload는 1200 bytes 기준으로 잡고 fragment header를 packet header 뒤에 붙인다.

### 1-2. C:/Users/user/Desktop/Winters/Shared/Schemas/Snapshot.fbs

목표:
- full snapshot과 delta snapshot을 안전하게 구분한다.

반영:
- root를 `SnapshotEnvelope`로 확장하는 방향을 우선 검토한다.
- `SnapshotKind { Full, Delta }`, `baselineTick`, added/changed/removed entity list를 둔다.
- 기존 client/server generated code 영향 범위를 확인한 뒤 FlatBuffers regenerate를 수행한다.

확인 필요:
- 기존 `Snapshot` root를 직접 읽는 `SnapshotApplier` 호출자를 모두 확인한 뒤 envelope migration 순서를 잡는다.

### 1-3. C:/Users/user/Desktop/Winters/Server/Private/Network/UdpCore.cpp

새 파일:
- Windows UDP IOCP 기반 `CUdpCore`를 만든다.

반영:
- `WSARecvFrom` / `WSASendTo` overlapped loop를 사용한다.
- M1에서는 hello/session, command batch receive, full snapshot send만 넣는다.
- M2에서 channel별 send queue, ack tracking, retransmit, fragment/reassembly를 추가한다.
- M3에서 baseline ack와 delta fallback을 추가한다.

### 1-4. C:/Users/user/Desktop/Winters/Server/Private/Network/UdpSession.cpp

새 파일:
- endpoint, session id, channel sequence, ack state, retransmit queue를 가진다.

반영:
- `ReliableOrdered`는 command batch에 사용한다.
- `ReliableUnordered`는 important event에 사용한다.
- `UnreliableSequenced`는 snapshot에 사용한다.
- idle client도 ack-only heartbeat로 snapshot baseline ack를 보낼 수 있게 한다.

### 1-5. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp

목표:
- TCP prototype과 UDP path가 GameSim truth를 공유하되 transport만 분리된다.

반영:
- `CGameRoom::EnqueueCommand(sessionId, GameCommandWire, acceptedTick)` 표면을 UDP/TCP 양쪽에서 호출한다.
- command ordering은 M1부터 stable sort로 고정한다.
- M3 AOI에서는 player별 visible entity set을 snapshot build 입력으로 전달한다.

### 1-6. C:/Users/user/Desktop/Winters/Server/Private/Game/SnapshotBuilder.cpp

목표:
- full snapshot과 delta snapshot을 같은 source of truth에서 만든다.

반영:
- M1은 full snapshot만 유지한다.
- M3에서 per-session baseline cache를 추가한다.
- AOI 적용 시 entity removal도 delta에 포함해 client stale entity를 정리한다.

### 1-7. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/UdpClient.cpp

새 파일:
- UDP hello, command batch send, snapshot receive, ack heartbeat를 담당한다.

반영:
- M1은 full snapshot apply까지만 연결한다.
- M2는 reliable ordered command resend와 fragment reassembly를 추가한다.
- M3는 baseline ack, delta apply, full-resync request를 추가한다.

### 1-8. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp

목표:
- full snapshot과 delta snapshot apply를 분리한다.

반영:
- full snapshot은 현재 state reset/overwrite path로 유지한다.
- delta snapshot은 added/changed/removed를 순서대로 적용한다.
- baseline mismatch 시 client는 full resync를 요청하고 delta apply를 중단한다.

## 2. 검증

검증 명령:
- `git diff --check`
- `msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64`

M1 검증:
- UDP hello/session 성공.
- command batch -> server GameSim -> full snapshot -> client apply 왕복.
- TCP path를 끄지 않고 UDP path를 별도 flag로 실행.

M2 검증:
- artificial loss 5%, reorder 5%, duplicate 2%에서 reliable command가 손실 없이 도착.
- unreliable snapshot은 오래된 sequence를 버린다.
- fragment timeout이 memory leak 없이 회수된다.

M3 검증:
- baseline ack 후 delta snapshot 전송.
- client AOI 밖 entity가 removed delta로 사라진다.
- baseline mismatch에서 full snapshot fallback이 동작한다.

확인 필요:
- Server/Shared sim 경계는 `/fp:precise` 유지.
- `unordered_map` 순회가 gameplay/order 결정에 들어가지 않는지 grep guard 추가.
