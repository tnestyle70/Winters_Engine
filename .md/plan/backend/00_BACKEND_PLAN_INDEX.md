# Winters Engine Go Backend Plan Index

> Last Updated: 2026-05-02
> Scope: current repository-aligned backend plan index
> Repo Root: `C:\Users\tnest\Desktop\Winters_restored\Winters`

---

## 1. Current State

현재 `Services/` 모노레포에는 아래 6개 Go 서비스가 이미 존재한다.

| Phase | Service | Port | Status | Code Path |
|---|---|---:|---|---|
| 1 | Auth | 8081 | 구현됨 | `Services/cmd/auth` |
| 2 | Leaderboard | 8082 | 구현됨 | `Services/cmd/leaderboard` |
| 3 | Matchmaking | 8083 | 구현됨 | `Services/cmd/matchmaking` |
| 4 | Profile | 8084 | 구현됨 | `Services/cmd/profile` |
| 5 | Payment | 8085 | 구현됨 | `Services/cmd/payment` |
| 6 | Shop | 8086 | 구현됨 | `Services/cmd/shop` |
| 9 | Social / Friend | 8087 | 계획 | `Services/cmd/social` |
| 10A | Replay MVP | 8088 | 계획 | `Services/cmd/replay` |
| 10B | Replay Personalization | 8088 | 후속 | Phase 10A 이후 |

핵심 전제:

- `Services/docker-compose.yml` 는 현재 인프라 전용이다.
  Postgres / Redis / Kafka 만 띄우고, Go 앱 컨테이너는 아직 쓰지 않는다.
- 공용 Kafka topic 상수는 `Services/pkg/messaging/kafka.go` 에 있다.
  새 topic 이 필요하면 우선 이 파일을 확장한다.
- 공용 포트 설정은 `Services/pkg/config/config.go` 와 `Services/.env.example` 이 단일 원천이다.
- C++ 백엔드 SDK 경로는 `Client/Public/Network/Backend` / `Client/Private/Network/Backend` 이다.
- 현재 `CHttpClient` 는 `AsyncGet` / `AsyncPost` 만 제공한다.
  새 API 는 이 제약을 고려해 설계한다.
- 현재 `Server/GameRoom` 은 `sessionId` 중심이며 Go Auth 의 `user_id` 를 모른다.
  Replay 의 개인 라이브러리 기능은 이 제약을 반영해 Phase 10B 로 분리한다.

---

## 2. Plan Index

| File | Phase | Purpose |
|---|---|---|
| [Phase0_LocalDevEnv.md](Phase0_LocalDevEnv.md) | 0 | Docker Compose + Go module + shared pkg |
| [Phase1_AuthService.md](Phase1_AuthService.md) | 1 | JWT auth / register / login / refresh |
| [Phase2_to_Phase4.md](Phase2_to_Phase4.md) | 2-4 | Leaderboard / Matchmaking / Profile |
| [Phase5_to_Phase8.md](Phase5_to_Phase8.md) | 5-8 | Payment / Shop / Kafka / C++ backend SDK |
| [Phase8_Review.md](Phase8_Review.md) | 8 Review | C++ SDK review notes |
| [Phase9_FriendSystem.md](Phase9_FriendSystem.md) | 9 | Social service (friend / block / presence / search) |
| [Phase10_ReplayService.md](Phase10_ReplayService.md) | 10 | Replay MVP + follow-up personalization split |

---

## 3. Actual Directory Layout

### 3.1 `Services/` today

```text
Services/
├── cmd/
│   ├── auth/
│   ├── leaderboard/
│   ├── matchmaking/
│   ├── payment/
│   ├── profile/
│   └── shop/
├── internal/
│   ├── auth/
│   ├── leaderboard/
│   ├── matchmaking/
│   ├── payment/
│   ├── profile/
│   └── shop/
├── migrations/
│   ├── 000001_create_users.*
│   ├── 000002_create_wallets.*
│   ├── 000003_create_player_stats.*
│   ├── 000004_create_match_history.*
│   ├── 000005_create_payment_transactions.*
│   ├── 000006_create_coin_transactions.*
│   └── 000007_create_inventory.*
├── pkg/
│   ├── auth/
│   ├── cache/
│   ├── config/
│   ├── database/
│   ├── errors/
│   ├── messaging/
│   ├── middleware/
│   └── response/
├── .env
├── .env.example
├── docker-compose.yml
├── go.mod
└── Makefile
```

### 3.2 Planned additions

```text
Services/
├── cmd/
│   ├── social/                 (Phase 9)
│   └── replay/                 (Phase 10A)
├── internal/
│   ├── social/                 (Phase 9)
│   └── replay/                 (Phase 10A)
├── migrations/
│   ├── 000008_create_friendships.*
│   ├── 000009_create_friend_blocks.*
│   └── 000010_create_replays.*   (Phase 10A MVP)
└── data/
    └── replays/                (local storage root, created at runtime)
```

주의:

- `000011_create_replay_players.*`, `000012_create_replay_bookmarks.*` 는 현재 코드베이스 기준으로 MVP 범위를 넘는다.
  `Server/GameRoom` 에 `user_id` 브리지가 생긴 뒤 Phase 10B 로 미룬다.
- `docker-compose.yml` 에 Go 서비스 컨테이너를 추가하지 않는다.
  현재 워크플로는 `go run ./cmd/<service>` 이며 `Makefile` 도 그 흐름을 따르고 있다.

### 3.3 C++ backend SDK today

```text
Client/
├── Public/Network/Backend/
│   ├── AuthClient.h
│   ├── CHttpClient.h
│   ├── CShopClient.h
│   ├── MatchClient.h
│   ├── PaymentClient.h
│   ├── ProfileClient.h
│   └── SnapshotApplier.h
└── Private/Network/Backend/
    ├── AuthClient.cpp
    ├── CHttpClient.cpp
    ├── CShopClient.cpp
    ├── MatchClient.cpp
    ├── PaymentClient.cpp
    ├── ProfileClient.cpp
    └── SnapshotApplier.cpp
```

Planned additions:

```text
Client/
├── Public/Network/Backend/SocialClient.h      (Phase 9)
├── Private/Network/Backend/SocialClient.cpp   (Phase 9)
├── Public/Network/Backend/CReplayClient.h     (Phase 10A)
├── Private/Network/Backend/CReplayClient.cpp  (Phase 10A)
├── Public/Replay/CReplayPlayer.h              (Phase 10A)
├── Private/Replay/CReplayPlayer.cpp           (Phase 10A)
├── Public/Scene/Scene_Replay.h                (Phase 10A)
└── Private/Scene/Scene_Replay.cpp             (Phase 10A)
```

### 3.4 Server files relevant to Phase 10

```text
Server/
├── Public/Game/GameRoom.h
├── Private/Game/GameRoom.cpp
├── Public/Game/SnapshotBuilder.h
├── Private/Game/SnapshotBuilder.cpp
└── Public/Network / Private/Network
```

Planned additions:

```text
Server/
├── Public/Game/ReplayRecorder.h
├── Private/Game/ReplayRecorder.cpp
├── Public/Network/ReplayUploadClient.h
└── Private/Network/ReplayUploadClient.cpp
```

---

## 4. Tech Stack

| Area | Current Repo Choice |
|---|---|
| Go module | `go 1.26.2` (`Services/go.mod`) |
| DB | PostgreSQL 16 + `pgx/v5` |
| Cache | Redis 7 + `go-redis/v9` |
| Broker | Kafka + `segmentio/kafka-go` |
| Router | `chi/v5` |
| Auth | JWT (`golang-jwt/jwt/v5`) + bcrypt |
| Client HTTP | WinHTTP (`Client/Private/Network/Backend/CHttpClient.cpp`) |
| Game snapshots | FlatBuffers (`Shared/Schemas/*.fbs`) |
| Replay storage MVP | Local filesystem (`Services/data/replays`) |

---

## 5. Dependency Graph

```text
Phase 0-8
  └── already implemented baseline

Phase 9 (Social / Friend)
  ├── depends on Auth JWT middleware
  ├── depends on users table (search)
  ├── uses PostgreSQL + Redis + Kafka
  └── optionally patches Matchmaking block checks

Phase 10A (Replay MVP)
  ├── depends on current TCP server snapshot pipeline
  ├── reuses Shared/Schemas Snapshot + Event FlatBuffers
  ├── uses Go Replay Service for metadata + file storage
  └── reuses Client SnapshotApplier for offline playback

Phase 10B (Replay personalization)
  ├── depends on Phase 10A
  └── depends on future session/auth bridge: GameRoom sessionId -> backend user_id
```

---

## 6. Recommended Order

### Next backend feature order

1. Phase 9A: Social core
   friend request / accept / decline / remove / block / unblock / search / presence
2. Phase 9B: Matchmaking block enforcement
   correctness first, DB lookup okay for current 2-player prototype
3. Phase 10A-1: Replay recorder + Go ingest
   `.wrpl` capture, upload, metadata, download
4. Phase 10A-2: Offline replay player
   `CReplayPlayer` + `Scene_Replay`
5. Phase 10B: user-scoped replay library
   bookmarks / my replays / ownership / featured feed after identity bridge

### Timeline estimate

| Work | Estimate |
|---|---:|
| Phase 9A | 3-4 days |
| Phase 9B | 1 day |
| Phase 10A-1 | 4-5 days |
| Phase 10A-2 | 3-4 days |
| Phase 10B | later, after session-auth bridge |

---

## 7. Read With This Index

- [Phase9_FriendSystem.md](Phase9_FriendSystem.md)
- [Phase10_ReplayService.md](Phase10_ReplayService.md)
- `Services/pkg/config/config.go`
- `Services/pkg/messaging/kafka.go`
- `Client/Public/Network/Backend/CHttpClient.h`
- `Server/Public/Game/GameRoom.h`
- `Server/Private/Game/GameRoom.cpp`
- `Shared/Schemas/Snapshot.fbs`
- `Shared/Schemas/Event.fbs`
