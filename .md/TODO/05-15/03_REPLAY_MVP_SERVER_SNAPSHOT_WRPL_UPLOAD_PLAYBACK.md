# Replay MVP Server Snapshot WRPL Upload Playback

Session - server authoritative snapshot stream을 `.wrpl`로 저장하고 Go replay service 업로드와 client playback scene으로 재생한다.

## 1. 반영해야 하는 코드

### 1-1. C:/Users/user/Desktop/Winters/Shared/Replay/ReplayFormat.h

목표:
- replay 파일은 새 gameplay schema가 아니라 기존 snapshot/event payload를 감싸는 container다.

반영:
- header magic은 `WRPL`, version은 1로 유지한다.
- record type은 `Snapshot`, `Event`로 유지한다.
- header에 room id, tick rate, first tick, last tick, snapshot count, event count를 둔다.
- replay payload는 `Shared::Schema::Snapshot`과 `Shared::Schema::EventPacket` raw bytes를 그대로 담는다.

### 1-2. C:/Users/user/Desktop/Winters/Server/Private/Game/ReplayRecorder.cpp

목표:
- server tick에서 spectator 기준 snapshot/event stream을 누락 없이 기록한다.

반영:
- snapshot record는 `Phase_BroadcastSnapshot`에서 build된 payload와 동일한 bytes를 사용한다.
- event record는 `Phase_BroadcastEvents`에서 broadcast된 payload와 동일한 bytes를 사용한다.
- memory accumulation limit을 두고 긴 경기에서는 chunked file writer로 전환할 수 있게 interface를 열어둔다.

확인 필요:
- 현재 recorder가 snapshot만 기록한다면 event record 연결을 추가한다.

### 1-3. C:/Users/user/Desktop/Winters/Server/Private/Network/ReplayUploadClient.cpp

새 파일:
- server가 game end 후 Go replay service로 `.wrpl` 파일을 업로드한다.

반영:
- WinHTTP 기반 multipart 또는 raw binary POST 중 하나로 고정한다.
- internal ingest secret을 header에 포함한다.
- upload 실패 시 local file은 보존하고 log에 retry 가능한 path를 남긴다.

### 1-4. C:/Users/user/Desktop/Winters/Services/cmd/replay/main.go

새 파일:
- replay service entry point를 만든다.

반영:
- `Services/pkg/config`, `database`, `response`, `middleware` 패턴을 따른다.
- port는 `REPLAY_PORT=8088`을 사용한다.
- local dev workflow는 기존처럼 docker compose에 Go service container를 추가하지 않고 `go run ./cmd/replay`로 둔다.

### 1-5. C:/Users/user/Desktop/Winters/Services/internal/replay

새 파일:
- `model.go`, `storage.go`, `repository.go`, `service.go`, `handler.go`를 만든다.

반영:
- storage는 MVP에서 `Services/data/replays` local filesystem을 사용한다.
- repository는 `replays` metadata table만 담당한다.
- handler는 upload, metadata list, download endpoint를 제공한다.
- user ownership, bookmark, featured feed는 `sessionId -> backend user_id` bridge 이후로 미룬다.

### 1-6. C:/Users/user/Desktop/Winters/Services/migrations/000010_create_replays.up.sql

새 파일:
- replay metadata table을 만든다.

반영:
- `id`, `room_id`, `tick_rate`, `first_tick`, `last_tick`, `snapshot_count`, `event_count`, `file_path`, `file_size`, `created_at`을 포함한다.
- MVP에서는 owner user id를 nullable 또는 제외한다.

### 1-7. C:/Users/user/Desktop/Winters/Client/Private/Replay/CReplayPlayer.cpp

새 파일:
- `.wrpl` record를 tick order로 읽고 기존 snapshot/event applier에 전달한다.

반영:
- playback clock은 server tick rate를 기준으로 한다.
- pause, resume, seek-to-start를 MVP에 포함한다.
- reverse playback, bookmark, timeline UI는 후속으로 미룬다.

### 1-8. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_Replay.cpp

새 파일:
- replay 전용 scene을 만든다.

반영:
- `CSnapshotApplier`를 재사용해 offline `CWorld`에 적용한다.
- camera는 free camera와 follow selected champion 중 하나를 선택 가능하게 한다.
- UI는 file open, play/pause, tick/time display만 MVP로 둔다.

### 1-9. C:/Users/user/Desktop/Winters/Client/Private/Network/Backend/CReplayClient.cpp

새 파일:
- Go replay service metadata/download API를 호출한다.

반영:
- 기존 `CHttpClient` 제약을 유지한다.
- binary download는 `HttpResponse.body`를 그대로 파일 저장 또는 memory load한다.

## 2. 검증

검증 명령:
- `git diff --check`
- `go test ./...` in `C:/Users/user/Desktop/Winters/Services`
- `msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64`

런타임 검증:
- 1분 server match 후 `.wrpl` 생성.
- replay service upload 성공 후 metadata 조회.
- client replay scene에서 `.wrpl` 다운로드/로드 후 snapshot playback.

합격 기준:
- replay playback에서 HP, position, action animation이 server match 결과와 같은 순서로 보인다.
- upload 실패 시 local `.wrpl` 파일이 사라지지 않는다.
- MVP 범위에서 user ownership 기능을 억지로 넣지 않는다.
