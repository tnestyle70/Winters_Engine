# Phase 10 - Replay Service (Port 8088)

> Last Updated: 2026-05-02
> Rewrite Basis: current `Server/GameRoom`, `Shared/Schemas`, `Client/Network/Client`, `Services/` code
> Dependencies: current TCP game-server prototype, FlatBuffers snapshot/event schema, Go backend monorepo

---

## 1. Reality Check First

기존 계획서를 현재 코드베이스에 맞춰 다시 쓰기 위해 먼저 제약을 명시한다.

### 1.1 What already exists

- `Server/Private/Game/GameRoom.cpp`
  30Hz authoritative tick, `Phase_DrainCommands -> Phase_ExecuteCommands -> Phase_SimulationSystems -> Phase_BroadcastSnapshot`
- `Server/Private/Game/SnapshotBuilder.cpp`
  `Shared::Schema::Snapshot` FlatBuffer 생성 가능
- `Shared/Schemas/Snapshot.fbs`
  snapshot wire schema 존재
- `Shared/Schemas/Event.fbs`
  event wire schema 존재
- `Client/Private/Network/Client/SnapshotApplier.cpp`
  raw `Snapshot` bytes 를 `CWorld` 에 적용 가능
- `Services/`
  Go 서비스 공용 infra / config / DB / Kafka 패턴 이미 존재

### 1.2 What does not exist yet

- 서버 replay recorder 없음
- 서버 HTTP uploader 없음
- replay 저장용 Go 서비스 없음
- replay playback scene 없음
- `Server/GameRoom` 에 backend `user_id` 정보 없음
- 서버에서 `EventPacket` 실제 송출 로직 없음

### 1.3 Critical constraint

현재 `GameRoom` 은 `sessionId` 와 `NetEntityId` 만 안다.
Go Auth 서비스의 `user_id` 와 연결되지 않는다.

이 말은 곧:

- "내 리플레이 목록"
- `replay_players`
- `replay_bookmarks`
- owner ACL

같은 기능은 지금 바로 넣기 어렵다는 뜻이다.

따라서 Phase 10 은 두 단계로 나눈다.

---

## 2. Phase Split

### Phase 10A: Replay MVP

현재 코드베이스로 바로 가능한 범위:

1. 서버 authoritative snapshot stream 녹화
2. `.wrpl` 파일 생성
3. Go Replay Service 로 업로드
4. 로컬 디스크 저장 + PostgreSQL 메타데이터 저장
5. replay 다운로드
6. 기존 `CSnapshotApplier` 기반 offline playback

### Phase 10B: Replay Personalization

아래 기능은 `sessionId -> backend user_id` 브리지가 생긴 뒤 한다.

1. 내 리플레이 목록
2. 참가자별 replay query
3. bookmark / favorite
4. featured / shared replay
5. owner permission / visibility

이번 문서는 10A 를 구현 가능한 MVP 로 정의하고,
10B 는 후속 확장으로 분리한다.

---

## 3. Goal of Phase 10A

Deliverables:

1. `CGameRoom` 에서 spectator snapshot stream 을 tick 단위로 기록
2. `.wrpl` 컨테이너 포맷 정의
3. `Services/cmd/replay` 신규 서비스
4. local filesystem storage (`Services/data/replays`)
5. replay metadata table (`replays`)
6. internal upload endpoint
7. replay metadata 조회 / 다운로드 endpoint
8. `CReplayPlayer` + `Scene_Replay`

Non-goals in 10A:

- user ownership 보장
- bookmark / featured feed
- kill-feed / event timeline 완성형 UI
- deterministic rewind/resim
- S3 / MinIO / CDN

---

## 4. Files To Create / Modify

### 4.1 Go side

| Path | Action | Purpose |
|---|---|---|
| `Services/cmd/replay/main.go` | Create | Replay 서비스 bootstrap |
| `Services/internal/replay/model.go` | Create | DTO / metadata model |
| `Services/internal/replay/storage.go` | Create | local file storage abstraction |
| `Services/internal/replay/repository.go` | Create | PostgreSQL metadata access |
| `Services/internal/replay/service.go` | Create | upload / download / list logic |
| `Services/internal/replay/handler.go` | Create | chi routes |
| `Services/migrations/000010_create_replays.up.sql` | Create | replay metadata table |
| `Services/migrations/000010_create_replays.down.sql` | Create | rollback |
| `Services/pkg/config/config.go` | Modify | `ReplayPort`, `ReplayStoragePath`, `ReplayIngestSecret` 추가 |
| `Services/.env.example` | Modify | replay env 추가 |
| `Services/pkg/messaging/kafka.go` | Modify | `TopicReplayEvents` 추가 |
| `Services/Makefile` | Modify | `replay:` 타깃 추가 |

### 4.2 Server side

| Path | Action | Purpose |
|---|---|---|
| `Server/Public/Game/ReplayRecorder.h` | Create | snapshot/event accumulator |
| `Server/Private/Game/ReplayRecorder.cpp` | Create | `.wrpl` serializer |
| `Server/Public/Network/ReplayUploadClient.h` | Create | WinHTTP uploader |
| `Server/Private/Network/ReplayUploadClient.cpp` | Create | upload implementation |
| `Server/Public/Game/GameRoom.h` | Modify | recorder/uploader 멤버 추가 |
| `Server/Private/Game/GameRoom.cpp` | Modify | tick capture + finalize/upload |
| `Server/Include/Server.vcxproj` / `.filters` | Modify | 파일 등록 |

### 4.3 Client side

| Path | Action | Purpose |
|---|---|---|
| `Client/Public/Replay/CReplayPlayer.h` | Create | replay playback core |
| `Client/Private/Replay/CReplayPlayer.cpp` | Create | playback implementation |
| `Client/Public/Network/Backend/CReplayClient.h` | Create | replay metadata/download SDK |
| `Client/Private/Network/Backend/CReplayClient.cpp` | Create | replay SDK implementation |
| `Client/Public/Scene/Scene_Replay.h` | Create | replay scene |
| `Client/Private/Scene/Scene_Replay.cpp` | Create | replay scene implementation |
| `Client/Include/Client.vcxproj` / `.filters` | Modify | 파일 등록 |

### 4.4 Files intentionally not changed in MVP

- `Services/docker-compose.yml`
  현재 infra-only compose 를 유지한다.
- `CHttpClient.h/.cpp`
  binary download 는 기존 `HttpResponse.body` 를 그대로 사용해도 처리 가능하다.
  별도 `AsyncGetBinary` 는 편의 개선일 뿐 MVP 필수는 아니다.

---

## 5. `.wrpl` File Format

### 5.1 Design choice

기존 코드베이스에는 이미 FlatBuffers snapshot/event schema 가 있다.
따라서 replay 용으로 또 다른 entity serialization 을 새로 만들지 않는다.

원칙:

- `.wrpl` 은 기존 `Shared::Schema::Snapshot` / `Shared::Schema::EventPacket` raw payload 를 감싼다.
- replay player 는 이 payload 를 다시 parse 해서 기존 `CSnapshotApplier` 로 넣는다.
- 포맷의 역할은 "컨테이너" 이지 "새 게임 상태 스키마" 가 아니다.

### 5.2 Header

```cpp
struct WRPLHeader
{
    char   magic[4];          // "WRPL"
    u16_t  version;           // 1
    u16_t  flags;             // reserved
    u32_t  roomId;
    u32_t  tickRate;
    u64_t  firstTick;
    u64_t  lastTick;
    u32_t  snapshotCount;
    u32_t  eventCount;
};
```

### 5.3 Record layout

```cpp
enum class eReplayRecordType : u8_t
{
    Snapshot = 1,
    Event    = 2
};

struct ReplayRecordHeader
{
    u8_t   type;
    u8_t   reserved0;
    u16_t  reserved1;
    u32_t  payloadSize;
    u64_t  serverTick;
};
```

record payload:

- `Snapshot`: raw bytes from `Shared::Schema::Snapshot`
- `Event`: raw bytes from `Shared::Schema::EventPacket`

### 5.4 Why no Hello record

`Hello` packet 은 session-specific 정보(`sessionId`, `yourNetId`) 를 담는다.
Replay MVP 는 spectator 기준 playback 이므로 `Hello` 가 필요 없다.

대신 replay 파일은:

- room id
- tick rate
- ordered snapshot stream

만 있으면 충분하다.

`SnapshotBuilder::Build(... yourNetId=0)` 형태의 spectator snapshot 을 기록한다.

---

## 6. Database Design

### 6.1 MVP table only

현재는 `replays` 테이블 하나만 만든다.

이유:

- `owner_user_id` 를 강제할 수 없음
- 참가자 목록을 신뢰성 있게 알 수 없음
- bookmark / my-replays 쿼리는 아직 성립하지 않음

### 6.2 Migration: `000010_create_replays.up.sql`

```sql
CREATE TABLE replays (
    id              UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    room_id         BIGINT NOT NULL,
    match_id        UUID,
    owner_user_id   UUID REFERENCES users(id),
    file_path       TEXT NOT NULL UNIQUE,
    file_size       BIGINT NOT NULL CHECK (file_size > 0),
    tick_rate       INT NOT NULL CHECK (tick_rate > 0),
    first_tick      BIGINT NOT NULL,
    last_tick       BIGINT NOT NULL,
    snapshot_count  INT NOT NULL CHECK (snapshot_count >= 0),
    event_count     INT NOT NULL DEFAULT 0 CHECK (event_count >= 0),
    status          VARCHAR(16) NOT NULL DEFAULT 'ready'
                    CHECK (status IN ('uploading', 'ready', 'failed', 'deleted')),
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX idx_replays_created_at
    ON replays(created_at DESC);

CREATE INDEX idx_replays_room_id
    ON replays(room_id);

CREATE INDEX idx_replays_match_id
    ON replays(match_id)
    WHERE match_id IS NOT NULL;
```

메모:

- `owner_user_id` 와 `match_id` 는 nullable 로 둔다.
- Phase 10B 에서 user bridge 가 생기면 이 필드를 채우기 시작한다.

### 6.3 Deferred to Phase 10B

아래 테이블은 지금 만들지 않는다.

- `replay_players`
- `replay_bookmarks`

---

## 7. Go Replay Service API

### 7.1 Internal upload auth

Game server 는 현재 Go Auth JWT 를 쓰지 않는다.
따라서 replay upload 는 JWT 대신 shared secret header 를 쓴다.

예시:

- header: `X-Replay-Secret: <REPLAY_INGEST_SECRET>`

이 방식이 현재 코드베이스와 가장 잘 맞는다.

### 7.2 Routes

| Method | Path | Auth | Purpose |
|---|---|---|---|
| POST | `/replay/upload` | `X-Replay-Secret` | game server ingest |
| GET | `/replay/{replay_id}` | JWT or open debug | metadata 조회 |
| GET | `/replay/{replay_id}/download` | JWT or open debug | `.wrpl` 다운로드 |
| GET | `/replay/recent?limit=` | JWT or open debug | 최신 replay 목록 |
| POST | `/replay/{replay_id}/delete` | `X-Replay-Secret` | internal cleanup/debug |
| GET | `/health` | none | health |

MVP 에서는 read API 를 debug-open 으로 두거나 JWT 보호 아래 둘 수 있다.
권장안은 "JWT required, ownership check 없음" 이다.

### 7.3 Upload protocol

multipart 대신 raw octet-stream + headers 로 단순화한다.
현재 Server 에는 HTTP 유틸이 전혀 없으므로 이 편이 구현이 쉽다.

Request:

- body: raw `.wrpl` bytes
- headers:
  - `Content-Type: application/octet-stream`
  - `X-Replay-Secret`
  - `X-Room-Id`
  - `X-Tick-Rate`
  - `X-First-Tick`
  - `X-Last-Tick`
  - `X-Snapshot-Count`
  - `X-Event-Count`
  - optional `X-Match-Id`

### 7.4 Go models

`Services/internal/replay/model.go` 예시:

```go
type UploadMeta struct {
    RoomID        uint32
    MatchID       *uuid.UUID
    TickRate      int
    FirstTick     uint64
    LastTick      uint64
    SnapshotCount int
    EventCount    int
}

type ReplayMeta struct {
    ReplayID       uuid.UUID  `json:"replay_id"`
    RoomID         uint32     `json:"room_id"`
    MatchID        *uuid.UUID `json:"match_id,omitempty"`
    TickRate       int        `json:"tick_rate"`
    FirstTick      uint64     `json:"first_tick"`
    LastTick       uint64     `json:"last_tick"`
    SnapshotCount  int        `json:"snapshot_count"`
    EventCount     int        `json:"event_count"`
    FileSize       int64      `json:"file_size"`
    Status         string     `json:"status"`
    CreatedAt      time.Time  `json:"created_at"`
}
```

---

## 8. Go Service Implementation Notes

### 8.1 `storage.go`

역할:

- `os.MkdirAll(basePath)`
- 날짜 기반 또는 uuid 기반 파일 경로 생성
- write / read / delete

권장 경로:

```text
Services/data/replays/2026/05/02/<replay-id>.wrpl
```

### 8.2 `repository.go`

필수 메서드:

```go
func (r *Repository) CreateReplay(ctx context.Context, meta UploadMeta, filePath string, fileSize int64) (uuid.UUID, error)
func (r *Repository) GetReplay(ctx context.Context, replayID uuid.UUID) (*ReplayMeta, error)
func (r *Repository) GetReplayFilePath(ctx context.Context, replayID uuid.UUID) (string, error)
func (r *Repository) ListRecent(ctx context.Context, limit int) ([]ReplayMeta, error)
func (r *Repository) MarkDeleted(ctx context.Context, replayID uuid.UUID) (string, error)
```

### 8.3 `service.go`

동작:

1. header metadata 검증
2. secret 검증
3. storage write
4. repository insert
5. Kafka `ReplayUploaded` 발행

### 8.4 `handler.go`

패턴은 기존 서비스와 동일:

- `response.JSON`
- `response.Error`
- `chi.URLParam`
- 필요시 JWT middleware

---

## 9. Server Integration

### 9.1 `CReplayRecorder`

역할:

- tick 단위 snapshot bytes 누적
- optional event bytes 누적
- `Finalize()` 시 `.wrpl` byte blob 생성

권장 인터페이스:

```cpp
class CReplayRecorder final
{
public:
    static unique_ptr<CReplayRecorder> Create(u32_t roomId, u32_t tickRate);

    void RecordSnapshot(u64_t tick, const u8_t* bytes, u32_t len);
    void RecordEvent(u64_t tick, const u8_t* bytes, u32_t len);

    vector<u8_t> Finalize();

    u64_t GetFirstTick() const;
    u64_t GetLastTick() const;
    u32_t GetSnapshotCount() const;
    u32_t GetEventCount() const;
};
```

### 9.2 `CReplayUploadClient`

역할:

- WinHTTP 로 raw `.wrpl` bytes POST
- `X-Replay-*` headers 셋업
- background thread 또는 detached async upload

이 유틸은 `Game` 레이어와 분리해 `Server/Public/Network` 아래 두는 편이 낫다.

### 9.3 `GameRoom.cpp` patch points

현재 구조 기준 추천 위치:

1. `CGameRoom::Create`
   `m_pReplayRecorder` 생성
2. `Tick()`
   `Phase_SimulationSystems(tc)` 직후 spectator snapshot 생성
3. `Stop()`
   `Finalize()` + upload

### 9.4 Snapshot capture strategy

중요:

- 현재 `Phase_BroadcastSnapshot()` 는 세션별로 `yourNetId` 가 다른 snapshot 을 만든다.
- replay 는 관전자 기준이면 충분하므로 세션별 snapshot 을 재사용하지 않는다.
- tick 당 한 번 `yourNetId=0`, `lastAckedSeq=0` spectator snapshot 을 새로 빌드해 기록한다.

장점:

- 파일이 session-specific 하지 않다
- replay playback 이 단순하다
- auth/user binding 이 없어도 된다

비용:

- tick 당 snapshot build 1회 추가

현재 서버 규모에서는 acceptable 하다.
추후 비용이 커지면 `SnapshotBuilder` 를 공용 entity payload + per-client adornment 로 분리한다.

### 9.5 Match end trigger

현재 `GameRoom` 에는 확정적인 match-end lifecycle 이 없다.
MVP 에서는 `Stop()` 시점 finalize 로 충분하다.

후속:

- `GameEnd` event 생산 시 finalize 를 그 지점으로 이동

---

## 10. Client Playback

### 10.1 Why existing `CSnapshotApplier` is enough

`Client/Private/Network/Client/SnapshotApplier.cpp` 는 이미:

- raw `Snapshot` bytes 검증
- entity spawn/bind
- transform / hp / mana / anim / stat 적용

을 처리한다.

따라서 replay player 는 네트워크 대신 파일에서 읽은 snapshot 을 같은 applier 에 전달하면 된다.

### 10.2 `CReplayPlayer` responsibilities

1. `.wrpl` header parse
2. record offsets 인덱싱
3. fixed tick 또는 dt 기반 playback
4. pause / resume / speed x0.5/x1/x2/x4
5. seek by snapshot index
6. snapshot payload 를 `CSnapshotApplier::OnSnapshot()` 로 전달

권장 인터페이스:

```cpp
class CReplayPlayer final
{
public:
    static unique_ptr<CReplayPlayer> Create();

    bool LoadFromBytes(const u8_t* data, u32_t len);
    void SetPlaySpeed(f32_t speed);
    void Play();
    void Pause();
    void SeekToSnapshot(u32_t snapshotIndex);

    void Update(
        f32_t dt,
        CWorld& world,
        EntityIdMap& entityMap,
        CSnapshotApplier& applier);
};
```

### 10.3 `Scene_Replay`

초기 목표:

- 독립 `CWorld`
- 독립 `EntityIdMap`
- `CReplayPlayer`
- `CSnapshotApplier`
- ImGui transport controls

기능:

- 파일 로드
- 재생/정지
- 속도 변경
- 슬라이더로 snapshot index 이동

네트워크 송신 / 입력 명령은 없다.

### 10.4 `CReplayClient`

`CHttpClient` 기존 GET 기능만으로 충분하다.

- metadata GET: JSON parse
- download GET: `HttpResponse.body` 를 `vector<u8_t>` 로 복사

MVP 표면:

```cpp
struct ReplayListItem
{
    string replayId;
    i32_t  roomId = 0;
    i32_t  tickRate = 0;
    i32_t  snapshotCount = 0;
    string createdAt;
};

using ReplayListCallback = function<void(const vector<ReplayListItem>&)>;
using ReplayDownloadCallback = function<void(bool_t, const vector<u8_t>&, const string&)>;
```

---

## 11. Config Changes

### 11.1 `Services/pkg/config/config.go`

추가 필드:

```go
ReplayPort        string
ReplayStoragePath string
ReplayIngestSecret string
```

### 11.2 `.env.example`

```text
REPLAY_PORT=8088
REPLAY_STORAGE_PATH=./data/replays
REPLAY_INGEST_SECRET=change-me-for-dev
```

### 11.3 `Services/pkg/messaging/kafka.go`

```go
const (
    TopicPaymentEvents = "payment-events"
    TopicMatchEvents   = "match-events"
    TopicPlayerEvents  = "player-events"
    TopicSocialEvents  = "social-events"
    TopicReplayEvents  = "replay-events"
)
```

### 11.4 `Services/Makefile`

```make
replay:
	go run ./cmd/replay
```

---

## 12. Phase 10B Trigger

아래 조건이 충족되면 Phase 10B 로 넘어간다.

1. 게임 서버 session 이 backend `user_id` 를 안다
2. room 참가자 roster 가 replay metadata 로 전달 가능하다
3. replay ownership / visibility 정책이 정해진다

그때 추가할 것:

- `replay_players`
- `replay_bookmarks`
- `GET /replay/me`
- owner-only delete
- featured / shared replay

---

## 13. Implementation Order

### 13.1 Replay Service

1. `config.go`, `.env.example`, `Makefile`, `messaging/kafka.go`
2. migration `000010_create_replays`
3. `internal/replay/model.go`
4. `internal/replay/storage.go`
5. `internal/replay/repository.go`
6. `internal/replay/service.go`
7. `internal/replay/handler.go`
8. `cmd/replay/main.go`

### 13.2 Server

1. `ReplayRecorder.h/.cpp`
2. `ReplayUploadClient.h/.cpp`
3. `GameRoom.h/.cpp` patch
4. `Server.vcxproj` 등록

### 13.3 Client

1. `CReplayPlayer.h/.cpp`
2. `CReplayClient.h/.cpp`
3. `Scene_Replay.h/.cpp`
4. `Client.vcxproj` 등록

---

## 14. Verification Checklist

### 14.1 Replay ingest

```text
[ ] GameRoom tick 중 spectator snapshot 이 누적된다
[ ] Stop() 시 Finalize() 로 유효한 WRPL blob 이 생성된다
[ ] Replay service upload endpoint 가 secret header 없으면 거부한다
[ ] secret + valid headers + valid body 로 업로드 시 metadata row 생성
[ ] replay file 이 Services/data/replays/... 에 저장된다
[ ] Kafka replay-events topic 에 ReplayUploaded 발행된다
```

### 14.2 Replay metadata / download

```text
[ ] GET /replay/recent returns recent rows
[ ] GET /replay/{id} returns metadata
[ ] GET /replay/{id}/download returns original WRPL bytes
[ ] delete endpoint marks row deleted and removes file
```

### 14.3 Offline playback

```text
[ ] CReplayPlayer loads WRPL header and record index correctly
[ ] playback advances snapshots at recorded tick rate
[ ] CSnapshotApplier rehydrates entities from replay snapshots
[ ] pause / resume / speed change works
[ ] seek by snapshot index works without crash
```

---

## 15. Summary

현재 코드베이스에서 Replay 를 바로 "개인 라이브러리 서비스" 로 설계하면 과하다.
핵심 제약은 `GameRoom` 이 아직 `user_id` 를 모르기 때문이다.

그래서 이번 계획은 다음처럼 현실적으로 나눈다.

- Phase 10A: session-centric replay capture / ingest / download / playback
- Phase 10B: user-centric replay library / bookmark / ownership

이렇게 가면 현재 있는 자산을 최대한 재사용할 수 있다.

- 서버는 `SnapshotBuilder`
- 클라이언트는 `CSnapshotApplier`
- 백엔드는 기존 `Services/` 패턴

즉, 새 시스템을 발명하는 대신 이미 있는 snapshot 파이프라인을 replay 로 재사용하는 것이 이번 Phase 의 핵심이다.
