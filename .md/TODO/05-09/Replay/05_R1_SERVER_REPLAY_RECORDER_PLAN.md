# Replay R1 Server Replay Recorder Plan

작성일: 2026-05-09

---

## 0. 먼저 보여줄 진행 순서

```text
R1-1 R0 ReplayFormat 공유 확인
     Shared/Replay/ReplayFormat.h 를 Server 프로젝트에서도 사용한다.

R1-2 Server CReplayRecorder 작성
     서버 authoritative Snapshot/Event payload 를 .wrpl 로 저장한다.

R1-3 CGameRoom 생성/종료 lifecycle 연결
     Create 에서 recorder 생성, Stop 에서 local file finalize.

R1-4 Event capture 연결
     CGameRoom::BroadcastEventPayload 에서 event payload 를 한 번만 기록한다.

R1-5 Snapshot capture 연결
     CGameRoom::Phase_BroadcastSnapshot 에서 tick 당 spectator snapshot 을 한 번 기록한다.

R1-6 Server.vcxproj / .filters 등록
     신규 recorder 파일과 Shared/Replay/ReplayFormat.h 등록.

R1-7 Server smoke
     실제 InGame 후 서버 종료 시 Replay/*.wrpl 생성 확인.
```

---

## 1. 방향성

R0는 클라이언트가 수신한 snapshot/event를 저장했다. R1은 저장 원천을 서버로 옮긴다.

핵심 결정:

- 서버가 확정한 `Snapshot` / `EventPacket` raw payload 를 그대로 저장한다.
- 클라이언트별 `yourNetId`가 들어간 snapshot은 저장하지 않는다.
- replay용 snapshot은 tick마다 `yourNetId = NULL_NET_ENTITY`, `lastAckedSeq = 0` 으로 별도 1회 생성한다.
- event는 이미 session-neutral payload 이므로 `BroadcastEventPayload` 진입 시 한 번만 저장한다.
- R1은 local file 저장까지만 한다. Go upload, DB ingest, replay list API는 R2.

---

## 2. 현재 코드 기준 삽입 지점

### Snapshot

파일:

```text
Server/Private/Game/GameRoom.cpp
```

함수:

```cpp
void CGameRoom::Phase_BroadcastSnapshot(TickContext& tc)
```

현재 이 함수는 session마다 `m_pSnapBuilder->Build(...)` 를 호출한다. R1에서는 함수 초반에 replay용 spectator snapshot을 별도로 한 번 만든다.

### Event

파일:

```text
Server/Private/Game/GameRoom.cpp
```

함수:

```cpp
void CGameRoom::BroadcastEventPayload(const u8_t* payload, u32_t payloadSize, u32_t sequence)
```

현재 모든 animation/effect/projectile/damage event가 이 함수를 거쳐 각 session으로 전송된다. 따라서 여기서 recorder에 한 번 기록하면 event 중복 없이 authoritative event stream을 얻는다.

### Lifecycle

파일:

```text
Server/Public/Game/GameRoom.h
Server/Private/Game/GameRoom.cpp
```

연결:

- `CGameRoom::Create` 에서 `CReplayRecorder::Create(roomId, 30)`
- `CGameRoom::Stop` 에서 tick thread join 후 `FinalizeReplayRecorder()`

---

## 3. R1 범위

포함:

- `Server/Public/Game/ReplayRecorder.h`
- `Server/Private/Game/ReplayRecorder.cpp`
- `CGameRoom` recorder member
- snapshot/event record hook
- local `.wrpl` 저장
- 서버 종료 시 로그 출력

제외:

- `ReplayUploadClient`
- Go Replay Service upload
- DB metadata
- MainMenu online replay list
- replay 권한/소유자/매치 ID binding

---

## 4. 파일 출력 위치

R1 local 저장 위치는 서버 프로세스 working directory 기준:

```text
Replay/room{roomId}_tick{firstTick}_{lastTick}.wrpl
```

Visual Studio에서 `Server/Bin/Debug/WintersServer.exe` 기준으로 실행되면 보통:

```text
Server/Bin/Debug/Replay/*.wrpl
```

R2에서 업로드를 붙일 때 이 파일을 body로 전송하면 된다.

---

## 5. R2로 넘길 메타데이터

R1 recorder는 파일 헤더에 room/tickRate를 추가하지 않는다. R0와 같은 `.wrpl` binary contract를 유지하기 위해서다.

R2 upload header로 넘길 값:

```text
X-Room-Id
X-Tick-Rate
X-First-Tick
X-Last-Tick
X-Snapshot-Count
X-Event-Count
```

---

## 6. 검증 기준

성공 기준:

- 서버 빌드 성공
- InGame 진입 후 server tick 진행
- event 발생 시 event count 증가
- 서버 종료 시 `.wrpl` 파일 생성
- R0 `CReplayPlayer`로 파일 header/record parsing 가능

실패 시 확인:

- R0 `Shared/Replay/ReplayFormat.h` 가 실제로 추가되었는지
- Server.vcxproj에 `Shared/Replay/ReplayFormat.h` include 등록 여부
- `CGameRoom::Stop()` 이 정상 호출되는지
- InGame phase에 들어갔는지
- session이 없어도 replay snapshot을 기록해야 하는지 정책 재확인
