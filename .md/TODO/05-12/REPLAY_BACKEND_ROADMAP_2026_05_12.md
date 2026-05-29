# Replay + Backend Roadmap

작성일: 2026-05-12
목적: `CLAUDE.md`, 기존 Replay/Backend 계획서, 실제 코드베이스를 대조해 Replay + Backend 목표를 다음 세션에서 바로 진행 가능한 순서로 고정한다.

---

## Current Sequence

```text
S10_BotAIStage1 1차 smoke baseline
-> S10D Death / TargetInvalid / Respawn 안정화
-> RB0 Server WRPL recorder stabilization
-> RB1 WRPL validator / local playback contract
-> RB2 Go Replay Service ingest/download
-> RB3 Server replay upload
-> RB4 Client replay library + playback scene
-> RB5 User-scoped replay library after sessionID -> userID bridge
```

Replay 작업은 서버 권위 gameplay의 sidecar다. S10D를 대체하지 않고, 오히려 death/stale target 회귀를 재현 가능한 파일로 남기는 디버그 축으로 붙인다.

---

## Current Codebase Finding

이미 들어온 것:

- `Shared/Replay/ReplayFormat.h`
  - `ReplayFileHeader`, `ReplayRecordHeader`, `eReplayRecordType` 존재.
  - `.wrpl` v1 컨테이너는 Snapshot/Event raw payload를 감싸는 방향으로 맞다.
- `Server/Public/Game/ReplayRecorder.h`
- `Server/Private/Game/ReplayRecorder.cpp`
- `Server/Include/Server.vcxproj`, `.filters`
  - `ReplayRecorder.cpp/.h`는 등록되어 있다.
- `Server/Private/Game/GameRoom.cpp`
  - `CGameRoom::Create`에서 recorder 생성.
  - `CGameRoom::Stop`에서 tick thread join 후 `FinalizeReplayRecorder()` 호출.

아직 비어 있는 것:

- `BroadcastEventPayload`에서 event payload를 recorder에 기록하지 않는다.
- `Phase_BroadcastSnapshot`에서 spectator snapshot을 recorder에 기록하지 않는다.
- 따라서 현재 recorder는 생성/종료 hook이 있어도 보통 empty라 `.wrpl`이 저장되지 않는다.
- Go `Services/cmd/replay`, `Services/internal/replay`, `000010_create_replays` migration 없음.
- `Services/pkg/config/config.go`에 Replay config 없음.
- `Services/pkg/messaging/kafka.go`에 `TopicReplayEvents` 없음.
- `Services/.env.example`, `Services/Makefile`에 replay 항목 없음.
- `Server/Public/Network/ReplayUploadClient.h/.cpp` 없음.
- `Client/Public/Replay`, `Client/Private/Replay`, `Scene_Replay`, `CReplayClient` 없음.

핵심 제약:

- `Server/GameRoom`은 아직 backend `user_id`를 모른다. 현재는 `sessionId`와 `NetEntityId` 중심이다.
- 따라서 "내 리플레이", bookmark, owner ACL, replay_players는 RB5로 미룬다.
- `Client::CHttpClient`는 JSON 중심 `AsyncGet/AsyncPost`만 있다. Client download는 `HttpResponse.body`를 bytes로 복사해도 되지만, Server upload는 Server 전용 `CReplayUploadClient`를 따로 두는 편이 낫다.

---

## Goal

1. 서버가 authoritative spectator Snapshot/Event stream을 `.wrpl`로 안정적으로 남긴다.
2. Go Replay Service가 `.wrpl` raw bytes를 ingest하고 metadata를 PostgreSQL에 저장한다.
3. 서버가 경기 종료 또는 Stop 시 replay를 backend로 업로드한다.
4. 클라이언트가 backend replay 목록/다운로드를 보고 `Scene_Replay`에서 재생한다.
5. userID bridge가 생긴 뒤 개인 라이브러리/권한/북마크로 확장한다.

---

## Why This Order

1. recorder가 실제 payload를 기록하지 않으면 backend를 붙여도 업로드할 파일이 없다.
2. `.wrpl`이 validator/player로 읽히지 않으면 DB 저장은 의미가 없다.
3. Go service가 먼저 떠야 Server uploader smoke가 가능하다.
4. Client replay UI는 download 가능한 파일과 parser가 있어야 테스트할 수 있다.
5. 개인 라이브러리는 `sessionId -> backend user_id` 브리지가 없으면 권한 모델이 거짓말이 된다.

---

## RB0. Server WRPL Recorder Stabilization

목표:

- 현재 들어온 `CReplayRecorder`를 실제로 기록되는 R1-local baseline으로 닫는다.

Files touched:

```text
Shared/Replay/ReplayFormat.h
Server/Public/Game/ReplayRecorder.h
Server/Private/Game/ReplayRecorder.cpp
Server/Public/Game/GameRoom.h
Server/Private/Game/GameRoom.cpp
Server/Include/Server.vcxproj
Server/Include/Server.vcxproj.filters
```

Insertion / replacement points:

- `Server/Private/Game/GameRoom.cpp:1680`
  - owner: `CGameRoom::BroadcastEventPayload`
  - anchor: `if (!payload || payloadSize == 0)`
  - add `m_pReplayRecorder->RecordEvent(sequence, payload, payloadSize)` after null check and before session loop.
- `Server/Private/Game/GameRoom.cpp:1785`
  - owner: `CGameRoom::Phase_BroadcastSnapshot`
  - anchor: `u32_t sentCount = 0;`
  - before session loop, build one spectator snapshot with `yourNetId = NULL_NET_ENTITY` and record it.
- `Server/Private/Game/ReplayRecorder.cpp:63`
  - owner: `CReplayRecorder::SaveToFile`
  - anchor: `if (fsPath.has_relative_path())`
  - replace with parent-path check: only create directories when `fsPath.has_parent_path()`.
- `Server/Public/Game/ReplayRecorder.h`
  - remove temporary review comments and keep naming as `ReplayRecord`, `m_Records`.

Verification logs:

```text
[Replay] saved Replay/room1_tick<first>_<last>.wrpl records=<n> snapshots=<s> events=<e>
```

Done:

- Server Debug build passes.
- InGame smoke for 5-10 seconds, then server Stop creates non-empty `Server/Bin/Debug/Replay/*.wrpl`.
- `snapshotCount > 0`; `eventCount > 0` when combat/skill happened.

---

## RB1. WRPL Validator / Playback Contract

목표:

- Backend 전에 `.wrpl`이 읽을 수 있는 파일인지 먼저 검증한다.

Files touched:

```text
Client/Public/Replay/ReplayPlayer.h
Client/Private/Replay/ReplayPlayer.cpp
또는 Tools/WintersReplayDump/WintersReplayDump.cpp
```

Minimum checks:

```text
magic == WRPL
version == 1
headerSize == sizeof(ReplayFileHeader)
recordCount == snapshotCount + eventCount
firstTick <= lastTick
every record headerSize == sizeof(ReplayRecordHeader)
payloadSize > 0
Snapshot payload passes VerifySnapshotBuffer
Event payload passes VerifyEventPacketBuffer
```

Done:

- RB0에서 생성한 `.wrpl` 파일을 dump/parse할 수 있다.
- corrupt header, truncated record, invalid FlatBuffer가 실패 로그로 분리된다.

---

## RB2. Go Replay Service

목표:

- `Services/cmd/replay` 8088 service를 추가해 ingest/list/metadata/download를 제공한다.

Files touched:

```text
Services/cmd/replay/main.go
Services/internal/replay/model.go
Services/internal/replay/storage.go
Services/internal/replay/repository.go
Services/internal/replay/service.go
Services/internal/replay/handler.go
Services/migrations/000010_create_replays.up.sql
Services/migrations/000010_create_replays.down.sql
Services/pkg/config/config.go
Services/pkg/messaging/kafka.go
Services/.env.example
Services/Makefile
```

API:

```text
POST /replay/upload              X-Replay-Secret, application/octet-stream
GET  /replay/recent?limit=20     JWT or debug-open
GET  /replay/{replay_id}         JWT or debug-open
GET  /replay/{replay_id}/download
POST /replay/{replay_id}/delete  X-Replay-Secret
GET  /health
```

Config additions:

```text
REPLAY_PORT=8088
REPLAY_STORAGE_PATH=./data/replays
REPLAY_INGEST_SECRET=change-me-for-dev
```

Kafka:

```text
TopicReplayEvents = "replay-events"
event type = ReplayUploaded
```

Done:

- `go test ./...` passes.
- `go run ./cmd/replay` starts on 8088.
- upload without secret returns 401/403.
- upload with valid secret writes file under `Services/data/replays/...` and inserts `replays` row.
- `GET /replay/recent` and download return the stored replay.

---

## RB3. Server Replay Upload

목표:

- Server Stop/finalize 후 `.wrpl`을 Replay Service로 업로드한다.

Files touched:

```text
Server/Public/Network/ReplayUploadClient.h
Server/Private/Network/ReplayUploadClient.cpp
Server/Public/Game/GameRoom.h
Server/Private/Game/GameRoom.cpp
Server/Include/Server.vcxproj
Server/Include/Server.vcxproj.filters
```

Design:

- Server 전용 WinHTTP uploader를 둔다. Client namespace의 `CHttpClient`를 재사용하지 않는다.
- request body는 raw `.wrpl` bytes.
- headers:

```text
Content-Type: application/octet-stream
X-Replay-Secret
X-Room-Id
X-Tick-Rate
X-First-Tick
X-Last-Tick
X-Snapshot-Count
X-Event-Count
```

Insertion points:

- `Server/Private/Game/GameRoom.cpp:853`
  - owner: `CGameRoom::FinalizeReplayRecorder`
  - after successful local save, call uploader or enqueue background upload.

Done:

```text
[Replay] saved Replay/room1_tick...wrpl records=...
[ReplayUpload] success replay_id=<uuid> bytes=<n>
```

Failure policy:

- Upload failure must not fail room shutdown.
- Local `.wrpl` remains for manual retry.

---

## RB4. Client Replay Library + Playback

목표:

- MainMenu에서 backend replay 목록을 보고 `.wrpl` 다운로드 후 `Scene_Replay`에서 재생한다.

Files touched:

```text
Client/Public/Network/Backend/CReplayClient.h
Client/Private/Network/Backend/CReplayClient.cpp
Client/Public/Replay/ReplayPlayer.h
Client/Private/Replay/ReplayPlayer.cpp
Client/Public/Scene/Scene_Replay.h
Client/Private/Scene/Scene_Replay.cpp
Client/Public/Defines.h
Client/Public/Scene/Scene_MainMenu.h
Client/Private/Scene/Scene_MainMenu.cpp
Client/Include/Client.vcxproj
Client/Include/Client.vcxproj.filters
```

Insertion points:

- `Client/Public/Defines.h`
  - add `Replay` scene before `End`.
- `Client/Private/Scene/Scene_MainMenu.cpp:179`
  - owner: `CScene_MainMenu::DrawNavigation`
  - add `Replay` panel button near Profile/Friends.
- `Client/Private/Scene/Scene_MainMenu.cpp:209`
  - owner: home/lobby panels
  - add replay list panel draw function.
- `Client/Private/Scene/InGameNetworkBridge.cpp:156`
  - optional R0 local capture hook if client-side capture is still desired.

Done:

- `GET /replay/recent` list appears in MainMenu.
- download stores or keeps bytes in memory.
- `Scene_Replay` parses WRPL header and applies snapshot payloads through `CSnapshotApplier`.
- play/pause/speed/seek basic controls work.
- Back to MainMenu works without touching the old scene after `Change_Scene`.

---

## RB5. User-Scoped Replay Library

Trigger:

```text
GameRoom session knows backend user_id
room roster can emit user_id/champion/team metadata
replay ownership / visibility policy is decided
```

Deferred files:

```text
Services/migrations/000011_create_replay_players.up.sql
Services/migrations/000012_create_replay_bookmarks.up.sql
Services/internal/replay/ownership.go
Client Replay "My Replays" / bookmark UI
Server replay upload metadata roster extension
```

Do not implement before the userID bridge. Until then, `owner_user_id`, `match_id`, and participant metadata stay nullable/debug-oriented.

---

## Verification Matrix

Build:

```powershell
MSBuild Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
MSBuild Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
cd Services
go test ./...
go run ./cmd/replay
```

Runtime smoke:

```text
1. Start infra: docker compose up -d
2. Run replay service on 8088
3. Run server + client into InGame
4. Let bot/human combat produce snapshot/event records
5. Stop server
6. Confirm local WRPL saved
7. Confirm backend upload row + stored file
8. Confirm client MainMenu recent replay list
9. Download and play in Scene_Replay
```

Expected backend logs:

```text
replay service started port=8088
replay uploaded replay_id=<uuid> room_id=1 bytes=<n>
published replay event type=ReplayUploaded topic=replay-events
```

Known failure checks:

- recorder empty: RB0 snapshot/event hooks missing.
- `.wrpl` save path fails: `Replay/` parent directory creation logic wrong.
- backend upload 401: `X-Replay-Secret` mismatch.
- client list empty: service URL/config not wired or auth policy blocking debug read.
- replay scene blank: `CSnapshotApplier` entity creation callback/resources not set.

---

## Next Slice

Start with RB0.

The immediate next implementation handoff should not jump to Go first. It should close:

```text
R1A-1 event record hook
R1A-2 spectator snapshot record hook
R1A-3 SaveToFile parent path fix
R1A-4 server smoke: non-empty WRPL
R1A-5 WRPL validator/parser smoke
```

Only after a valid `.wrpl` exists should RB2 Go Replay Service begin.
