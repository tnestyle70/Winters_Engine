# Phase 2-4 -- Leaderboard + Matchmaking + Profile Services

---

## Phase 2: Leaderboard Service (Ranking)

> **Dependencies**: Phase 0, Phase 1 (users + player_stats must exist)
> **Estimated**: 3-5 days
> **Completion Criteria**: MMR 기반 실시간 랭킹 조회 + 순위 변동

### Architecture

Redis Sorted Set (`leaderboard:mmr`)을 사용하여 O(log N) 랭킹 쿼리를 지원한다.
PostgreSQL은 영구 저장용 (Redis 장애 시 복구 가능).
Kafka Consumer로 `match-events` 토픽에서 MMR 변동을 실시간 수신한다.

### API Endpoints

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| GET | /leaderboard/top?limit=100 | O | 상위 N명 랭킹 |
| GET | /leaderboard/rank/{user_id} | O | 특정 유저 순위 |
| POST | /leaderboard/update | Internal | MMR 갱신 (게임서버 전용) |

### Files

**`Services/internal/leaderboard/model.go`**
```go
package leaderboard

import "github.com/google/uuid"

type LeaderboardEntry struct {
	Rank     int64     `json:"rank"`
	UserID   uuid.UUID `json:"user_id"`
	Username string    `json:"username"`
	MMR      int       `json:"mmr"`
	Wins     int       `json:"wins"`
	Losses   int       `json:"losses"`
}

type RankResponse struct {
	Rank         int64     `json:"rank"`
	UserID       uuid.UUID `json:"user_id"`
	MMR          int       `json:"mmr"`
	TotalPlayers int64     `json:"total_players"`
}

type UpdateRequest struct {
	UserID    uuid.UUID `json:"user_id"`
	MMRChange int       `json:"mmr_change"`
}

type MatchEvent struct {
	Type    string        `json:"type"`
	MatchID uuid.UUID     `json:"match_id"`
	Players []MatchPlayer `json:"players"`
}

type MatchPlayer struct {
	UserID    uuid.UUID `json:"user_id"`
	MMRChange int       `json:"mmr_change"`
}
```

**`Services/internal/leaderboard/repository.go`** -- Key operations:
```go
// Redis Sorted Set operations
func (r *Repository) UpdateScore(ctx, userID uuid.UUID, mmr int) error {
    // ZADD leaderboard:mmr {mmr} {userID}
    // UPDATE player_stats SET mmr=$2 WHERE user_id=$1
}

func (r *Repository) GetTop(ctx, limit int) ([]LeaderboardEntry, error) {
    // ZREVRANGE leaderboard:mmr 0 limit-1 WITHSCORES
    // Join with users + player_stats for username, wins, losses
}

func (r *Repository) GetRank(ctx, userID uuid.UUID) (*RankResponse, error) {
    // ZREVRANK leaderboard:mmr {userID} -> 0-indexed -> +1
    // ZSCORE leaderboard:mmr {userID} -> mmr
    // ZCARD leaderboard:mmr -> total_players
}

func (r *Repository) SyncFromDB(ctx) error {
    // SELECT user_id, mmr FROM player_stats
    // Pipeline ZADD for all users
}
```

**`Services/internal/leaderboard/consumer.go`** -- Kafka consumer:
```go
// Consumer Group: "leaderboard-consumer"
// Topic: "match-events"
// On MatchCompleted event: apply MMR changes per player via UpdateScore
```

**`Services/internal/leaderboard/handler.go`** -- HTTP handlers for GET /top, GET /rank/{user_id}

**`Services/cmd/leaderboard/main.go`** -- HTTP server + Kafka consumer goroutine + DB sync on startup

### Verification

```
[ ] Register 3+ users
[ ] GET /leaderboard/top?limit=10 -> all users sorted by MMR (default 1000)
[ ] Publish test MatchCompleted event to match-events Kafka topic
[ ] Re-query -> MMR values updated
[ ] ZREVRANGE leaderboard:mmr 0 -1 WITHSCORES -> correct order
[ ] Redis restart -> SyncFromDB recovers all rankings
```

---

## Phase 3: Matchmaking Service

> **Dependencies**: Phase 0, Phase 1
> **Estimated**: 1 week
> **Completion Criteria**: MMR 기반으로 비슷한 실력의 플레이어 N명을 자동 매칭

### Architecture

Redis 전용 (PostgreSQL 없음). 플레이어가 대기열에 들어가면 Redis Sorted Set (`matchmaking:queue`)에
MMR 점수로 등록된다. 백그라운드 고루틴 `RunMatcher`가 1초 간격으로 대기열을 스캔하여 매칭을 시도한다.

**MMR 기반 그룹핑 알고리즘**:
1. 대기열 전체 조회 (ZRANGEBYSCORE)
2. MMR 순 정렬
3. 인접 플레이어끼리 그룹핑 (MMR 차이 < 허용 범위)
4. 대기 시간이 길수록 허용 범위 확장 (30초마다 +50)
5. N명(기본 2) 확보 -> 매칭 성공
6. 매칭된 플레이어 대기열에서 제거 (ZREM)
7. 매칭 결과 Redis에 저장 (SET match:{id}, TTL 5분)
8. Kafka `match-events` 토픽에 `MatchCreated` 이벤트 발행

### API Endpoints

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| POST | /matchmaking/join | O (JWT) | 매칭 대기열 등록 |
| DELETE | /matchmaking/leave | O (JWT) | 대기열 탈퇴 |
| GET | /matchmaking/status | O (JWT) | 매칭 상태 확인 |

### Files

**`Services/internal/matchmaking/model.go`**
```go
package matchmaking

import (
	"time"
	"github.com/google/uuid"
)

type QueueEntry struct {
	UserID   uuid.UUID `json:"user_id"`
	MMR      int       `json:"mmr"`
	JoinedAt time.Time `json:"joined_at"`
}

type MatchStatus struct {
	Status  string    `json:"status"` // "queued", "matched", "none"
	MatchID string    `json:"match_id,omitempty"`
}

type MatchCreatedEvent struct {
	MatchID   uuid.UUID   `json:"match_id"`
	Players   []uuid.UUID `json:"players"`
	AvgMMR    int         `json:"avg_mmr"`
	CreatedAt time.Time   `json:"created_at"`
}
```

**`Services/internal/matchmaking/service.go`** -- All logic (Redis only):
```go
func (s *Service) Join(ctx, userID uuid.UUID) error {
    // Get MMR from player_stats or Redis cache
    // ZADD matchmaking:queue {mmr} {userID}
    // SET matchmaking:status:{userID} "queued" EX 600
}

func (s *Service) Leave(ctx, userID uuid.UUID) error {
    // ZREM matchmaking:queue {userID}
    // DEL matchmaking:status:{userID}
}

func (s *Service) Status(ctx, userID uuid.UUID) (*MatchStatus, error) {
    // GET matchmaking:status:{userID}
    // If "matched:{matchID}" -> return matched status
    // If "queued" -> return queued status
}

func (s *Service) RunMatcher(ctx context.Context) {
    ticker := time.NewTicker(time.Second)
    for {
        select {
        case <-ctx.Done(): return
        case <-ticker.C: s.tryMatch(ctx)
        }
    }
}

func (s *Service) tryMatch(ctx context.Context) {
    // 1. ZRANGEBYSCORE matchmaking:queue -inf +inf WITHSCORES
    // 2. Sort by MMR, sliding window grouping
    // 3. mmrRange = 200 + (waitSeconds/30)*50
    // 4. If group.size >= matchSize -> createMatch
}

func (s *Service) createMatch(ctx, players []QueueEntry) {
    // ZREM all matched players
    // SET matchmaking:status:{userID} "matched:{matchID}" for each
    // Kafka publish MatchCreated to match-events
}
```

**`Services/internal/matchmaking/handler.go`** -- HTTP handlers (all require JWT auth)

**`Services/cmd/matchmaking/main.go`** -- HTTP server + `go svc.RunMatcher(ctx)` goroutine

### Verification

```
[ ] Login as 2 users, get tokens
[ ] POST /matchmaking/join with each user's token (both MMR 1000, within 200 range)
[ ] Wait 1-2 seconds
[ ] GET /matchmaking/status -> "matched" + match_id for both
[ ] Check Kafka match-events topic for MatchCreated event
[ ] DELETE /matchmaking/leave before match -> "left" status
[ ] MMR 1000 vs MMR 2000 -> not matched immediately (500 > 200 range)
[ ] After ~2 minutes -> range expands enough to match them
```

---

## Phase 4: Player Profile Service

> **Dependencies**: Phase 0, Phase 1, match_history table
> **Estimated**: 3-5 days
> **Completion Criteria**: 플레이어 통계 조회/갱신 + 매치 히스토리

### Architecture

**Cache-Aside Pattern**: 프로필 조회 시 Redis 먼저 확인 (`profile:{user_id}`, TTL 5분),
캐시 미스 시 PostgreSQL에서 users + player_stats JOIN 쿼리, 파생 통계 계산 (win_rate, KDA),
결과를 Redis에 캐싱 후 반환.

Kafka Consumer로 `match-events` 토픽에서 게임 결과를 수신하여:
- `match_history` 테이블에 기록 INSERT
- `player_stats` 집계 갱신 (wins, losses, kills, deaths, assists, mmr)
- 프로필 캐시 무효화 (DEL)

### API Endpoints

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| GET | /profile/{user_id} | O | 플레이어 프로필 + 통계 |
| GET | /profile/{user_id}/history?limit=20&offset=0 | O | 매치 히스토리 (페이지네이션) |

### Files

**`Services/internal/profile/model.go`**
```go
package profile

import (
	"time"
	"github.com/google/uuid"
)

type Profile struct {
	UserID   uuid.UUID `json:"user_id"`
	Username string    `json:"username"`
	MMR      int       `json:"mmr"`
	Rank     int64     `json:"rank"`
	Wins     int       `json:"wins"`
	Losses   int       `json:"losses"`
	WinRate  float64   `json:"win_rate"`
	Kills    int       `json:"kills"`
	Deaths   int       `json:"deaths"`
	Assists  int       `json:"assists"`
	KDA      float64   `json:"kda"`
}

type MatchRecord struct {
	MatchID   uuid.UUID `json:"match_id"`
	Result    string    `json:"result"`
	Kills     int       `json:"kills"`
	Deaths    int       `json:"deaths"`
	Assists   int       `json:"assists"`
	MMRChange int       `json:"mmr_change"`
	PlayedAt  time.Time `json:"played_at"`
}
```

**`Services/internal/profile/repository.go`** -- Cache-Aside + pagination:
```go
func (r *Repository) GetProfile(ctx, userID uuid.UUID) (*Profile, error) {
    // 1. GET profile:{userID} from Redis
    // 2. If hit -> unmarshal and return
    // 3. If miss -> JOIN query: users + player_stats
    // 4. Compute win_rate = wins/(wins+losses)*100, KDA = (kills+assists)/max(deaths,1)
    // 5. Get rank from Redis ZREVRANK leaderboard:mmr
    // 6. SET profile:{userID} {json} EX 300 (5 min)
    // 7. Return
}

func (r *Repository) GetMatchHistory(ctx, userID uuid.UUID, limit, offset int) ([]MatchRecord, error) {
    // SELECT FROM match_history WHERE user_id=$1
    // ORDER BY played_at DESC LIMIT $2 OFFSET $3
}

func (r *Repository) InvalidateCache(ctx, userID uuid.UUID) error {
    // DEL profile:{userID}
}
```

**`Services/internal/profile/consumer.go`** -- Kafka consumer:
```go
// Consumer Group: "profile-consumer"
// Topic: "match-events"
// On MatchCompleted:
//   For each player:
//     INSERT match_history (user_id, match_id, result, kills, deaths, assists, mmr_change)
//     UPDATE player_stats SET wins=wins+1 (or losses), kills+=, deaths+=, assists+=, mmr+=
//     DEL profile:{userID} (cache invalidation)
```

**`Services/internal/profile/handler.go`** -- HTTP handlers

**`Services/cmd/profile/main.go`** -- HTTP server + Kafka consumer

### Verification

```
[ ] Register user, publish match events via Kafka
[ ] GET /profile/{user_id} -> returns full profile with stats
[ ] Call same endpoint again within 5 min -> served from Redis cache (check logs)
[ ] Publish new match event -> cache invalidated -> next query shows updated stats
[ ] GET /profile/{user_id}/history?limit=5 -> paginated match records
[ ] GET /profile/{nonexistent_id} -> 404 Not Found
```
