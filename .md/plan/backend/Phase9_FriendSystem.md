# Phase 9 - Friend System (Social Service) (Port 8087)

> Last Updated: 2026-05-02
> Rewrite Basis: current repository code, not greenfield assumptions
> Dependencies: Phase 1 Auth, current PostgreSQL/Redis/Kafka stack, `users` table, optional Matchmaking patch

---

## 1. Codebase Reality Check

이 계획은 현재 저장소 구조를 전제로 한다.

- `Services/docker-compose.yml` 는 인프라 전용이다.
  Postgres / Redis / Kafka 만 띄우며 Go 서비스 컨테이너는 아직 없다.
- 공용 포트는 `Services/pkg/config/config.go` 와 `Services/.env.example` 에 모여 있다.
  Social 포트도 여기 추가해야 한다.
- 공용 Kafka topic 상수는 `Services/pkg/messaging/kafka.go` 에 있다.
  별도 `topics.go` 파일을 새로 두기보다 현 구조를 그대로 확장하는 편이 안전하다.
- 공용 HTTP 응답 포맷은 `Services/pkg/response/json.go` 이다.
  새 서비스도 `APIResponse{success,data,error}` 래퍼를 그대로 따라간다.
- 공용 인증 미들웨어는 `Services/pkg/middleware/auth.go` 이다.
  Social 서비스는 JWT access token 을 그대로 재사용한다.
- C++ 백엔드 SDK 경로는 `Client/Public/Network/Backend` / `Client/Private/Network/Backend` 이다.
  기존 계획서의 `Client/Public/Network/CSocialClient.h` 경로는 현재 코드베이스와 맞지 않는다.
- `CHttpClient` 는 `AsyncGet` / `AsyncPost` 만 제공한다.
  따라서 Phase 9 API 는 현재 클라이언트 래퍼 제약을 반영해 GET/POST 위주로 설계한다.
- `internal/matchmaking/service.go` 는 아직 2인 큐 프로토타입이다 (`matchSize = 2`).
  차단 연동은 correctness 우선으로 구현하고, 대규모 큐 최적화는 뒤로 미룬다.

---

## 2. Goal

Phase 9 에서 구현할 범위:

1. 친구 요청 보내기
2. 친구 요청 수락 / 거절
3. 친구 삭제
4. 사용자 차단 / 차단 해제
5. 친구 목록 / 받은 요청 / 보낸 요청 조회
6. 닉네임(prefix / substring) 검색
7. Redis TTL 기반 온라인 heartbeat
8. Kafka `social-events` 발행
9. 선택적 Matchmaking 차단 연동

이번 Phase 에서 하지 않을 것:

- WebSocket / SSE 기반 실시간 푸시
- 파티 / 프리메이드 로비
- 친구 초대 UI 세부 설계
- 알림 서비스 consumer
- 서비스 컨테이너화

---

## 3. Files To Create / Modify

### 3.1 Create

| Path | Purpose |
|---|---|
| `Services/cmd/social/main.go` | Social 서비스 부트스트랩 |
| `Services/internal/social/model.go` | request / response / event 모델 |
| `Services/internal/social/repository.go` | PostgreSQL + Redis 접근 |
| `Services/internal/social/service.go` | 비즈니스 규칙 |
| `Services/internal/social/handler.go` | chi 라우트 + 에러 매핑 |
| `Services/migrations/000008_create_friendships.up.sql` | 친구 요청 / 수락 상태 저장 |
| `Services/migrations/000008_create_friendships.down.sql` | rollback |
| `Services/migrations/000009_create_friend_blocks.up.sql` | 차단 관계 저장 |
| `Services/migrations/000009_create_friend_blocks.down.sql` | rollback |
| `Client/Public/Network/Backend/SocialClient.h` | C++ Social SDK |
| `Client/Private/Network/Backend/SocialClient.cpp` | C++ Social SDK 구현 |

### 3.2 Modify

| Path | Change |
|---|---|
| `Services/pkg/config/config.go` | `SocialPort` 추가 |
| `Services/.env.example` | `SOCIAL_PORT=8087` 추가 |
| `Services/pkg/messaging/kafka.go` | `TopicSocialEvents` 추가 |
| `Services/pkg/errors/errors.go` | 필요한 경우 Social 전용 에러 상수 추가 |
| `Services/Makefile` | `social:` 타깃 추가 |
| `Services/internal/matchmaking/service.go` | 차단 유저끼리 매칭 금지 여부 반영 |
| `Client/Include/Client.vcxproj` / `.filters` | SocialClient 파일 등록 |

변경하지 않는 파일:

- `Services/docker-compose.yml`
  현재 인프라 전용 compose 구조를 유지한다.
- `CHttpClient.h/.cpp`
  이번 Phase 에서는 DELETE 지원을 강제하지 않는다.
  API 를 POST 액션 스타일로 맞춘다.

---

## 4. Database Design

### 4.1 Why this schema

현재 코드베이스에서는 다음 조건이 중요하다.

- `users` 테이블은 이미 존재하고 username unique index 가 있다.
- 친구 관계 조회는 "내 기준의 리스트" 가 자주 필요하다.
- C++ 클라이언트와 Matchmaking 연동에서 "양방향 accepted 여부" 를 빠르게 판단하는 편이 구현이 단순하다.

따라서 이번 Phase 에서는 다음 구조를 사용한다.

- `friendships`
  방향성 row 기반
  `pending` 는 1개 row
  `accepted` 는 양방향 2개 row
- `friend_blocks`
  방향성 row 기반

### 4.2 Migration: `000008_create_friendships.up.sql`

```sql
CREATE TABLE friendships (
    id           UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id      UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    friend_id    UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    status       VARCHAR(16) NOT NULL CHECK (status IN ('pending', 'accepted')),
    requested_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    accepted_at  TIMESTAMPTZ,
    CHECK (user_id <> friend_id),
    UNIQUE (user_id, friend_id)
);

CREATE INDEX idx_friendships_user_status
    ON friendships(user_id, status);

CREATE INDEX idx_friendships_friend_status
    ON friendships(friend_id, status);
```

### 4.3 Migration: `000009_create_friend_blocks.up.sql`

```sql
CREATE TABLE friend_blocks (
    id              UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id         UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    blocked_user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    CHECK (user_id <> blocked_user_id),
    UNIQUE (user_id, blocked_user_id)
);

CREATE INDEX idx_friend_blocks_user
    ON friend_blocks(user_id);

CREATE INDEX idx_friend_blocks_blocked_user
    ON friend_blocks(blocked_user_id);
```

### 4.4 State transitions

```text
A sends request to B:
  friendships: (A -> B, pending)

B accepts:
  update (A -> B) -> accepted
  insert (B -> A, accepted)

B declines:
  delete (A -> B)

A or B removes friend:
  delete both accepted rows

A blocks B:
  insert friend_blocks (A -> B)
  delete pending / accepted rows both directions in same transaction
```

---

## 5. Redis Design

### 5.1 Presence keys

| Key | Value | TTL |
|---|---|---:|
| `social:presence:{user_id}` | unix timestamp string | 90s |

규칙:

- 클라이언트가 30초 간격 heartbeat 를 보내면 Redis TTL 을 90초로 갱신한다.
- 키가 존재하면 online, 없으면 offline 으로 판단한다.
- DB 에 마지막 접속 시각을 쓰지 않는다.
  이번 Phase 는 presence 를 캐시/ephemeral 상태로만 유지한다.

### 5.2 Why not Redis block cache yet

현재 Matchmaking 은 2인 큐 프로토타입이다.
이번 Phase 에서는 `friend_blocks` 를 PostgreSQL 에서 직접 확인해도 충분하다.

다음 단계에서 큐 규모가 커지면 그때 아래 캐시를 검토한다.

| Future Key | Purpose |
|---|---|
| `social:blocks:{user_id}` | blocked user id set mirror |

---

## 6. API Design

`CHttpClient` 의 현재 제약을 반영해 GET/POST 로 맞춘다.
DELETE/PATCH 는 일부러 쓰지 않는다.

### 6.1 Routes

| Method | Path | Auth | Purpose |
|---|---|---|---|
| POST | `/social/friends/request` | JWT | 친구 요청 보내기 |
| POST | `/social/friends/accept` | JWT | 친구 요청 수락 |
| POST | `/social/friends/decline` | JWT | 친구 요청 거절 |
| POST | `/social/friends/remove` | JWT | 친구 삭제 |
| GET | `/social/friends` | JWT | 친구 목록 |
| GET | `/social/requests/incoming` | JWT | 받은 요청 |
| GET | `/social/requests/outgoing` | JWT | 보낸 요청 |
| POST | `/social/blocks/add` | JWT | 사용자 차단 |
| POST | `/social/blocks/remove` | JWT | 차단 해제 |
| GET | `/social/blocks` | JWT | 차단 목록 |
| GET | `/social/search?q=` | JWT | username 검색 |
| POST | `/social/presence/heartbeat` | JWT | 온라인 상태 갱신 |
| GET | `/health` | X | 헬스체크 |

### 6.2 Request / response DTO

`Services/internal/social/model.go` 에 아래 구조를 둔다.

```go
type UserRef struct {
    UserID   uuid.UUID `json:"user_id"`
    Username string    `json:"username"`
    Online   bool      `json:"online"`
}

type FriendRequestReq struct {
    TargetUsername string `json:"target_username"`
}

type FriendActionReq struct {
    TargetUserID uuid.UUID `json:"target_user_id"`
}

type FriendInfo struct {
    UserID      uuid.UUID `json:"user_id"`
    Username    string    `json:"username"`
    Online      bool      `json:"online"`
    FriendsSince time.Time `json:"friends_since"`
}
```

실제 응답은 기존 서비스와 동일하게 `response.JSON()` 을 통해 감싼다.

---

## 7. Service Rules

### 7.1 Send friend request

검증 순서:

1. 자기 자신에게 요청 금지
2. target username 존재 확인
3. 이미 친구인지 확인
4. 기존 pending 요청 존재 확인
5. 어느 방향이든 block 관계 존재 시 거부
6. `friendships (me -> target, pending)` insert
7. Kafka `FriendRequestSent` 발행

### 7.2 Accept request

검증 순서:

1. `(target -> me, pending)` row 존재 확인
2. transaction 시작
3. pending row 를 accepted 로 update
4. `(me -> target, accepted)` mirror row insert
5. transaction commit
6. Kafka `FriendAccepted` 발행

### 7.3 Decline request

- `(target -> me, pending)` row 삭제
- Kafka 는 선택적이다.
  MVP 는 accept / remove / block 같은 외부 후속 처리 가능성이 큰 이벤트만 우선 발행해도 충분하다.

### 7.4 Remove friend

- `(me -> target, accepted)` 와 `(target -> me, accepted)` 둘 다 삭제
- 양방향 accepted 가 아니면 `ErrNotFound` 반환
- Kafka `FriendRemoved`

### 7.5 Block user

transaction 안에서 처리:

1. `friend_blocks (me -> target)` insert
2. pending / accepted row 양방향 삭제
3. commit
4. Kafka `UserBlocked`

### 7.6 Unblock user

- `friend_blocks (me -> target)` 삭제
- 친구 관계는 자동 복구하지 않는다.

### 7.7 Search

검색 규칙:

- `users.username ILIKE '%' || q || '%'`
- `LIMIT 20`
- 자기 자신 제외
- 차단한 유저 / 나를 차단한 유저 제외
- 정렬은 `username ASC`

### 7.8 Presence heartbeat

- `SET social:presence:{user_id} <unix_ts> EX 90`
- 응답은 `{status:"ok"}` 정도면 충분

---

## 8. Repository Notes

`Services/internal/social/repository.go` 는 현재 다른 서비스 패턴을 그대로 따른다.

필수 메서드:

```go
type Repository struct {
    db  *pgxpool.Pool
    rdb *redis.Client
}

func (r *Repository) FindUserByUsername(ctx context.Context, username string) (*UserRef, error)
func (r *Repository) IsBlockedEitherWay(ctx context.Context, a, b uuid.UUID) (bool, error)
func (r *Repository) HasFriendship(ctx context.Context, a, b uuid.UUID) (bool, error)
func (r *Repository) CreatePendingRequest(ctx context.Context, from, to uuid.UUID) error
func (r *Repository) AcceptRequest(ctx context.Context, requester, accepter uuid.UUID) error
func (r *Repository) DeclineRequest(ctx context.Context, requester, accepter uuid.UUID) error
func (r *Repository) RemoveFriend(ctx context.Context, a, b uuid.UUID) error
func (r *Repository) BlockUser(ctx context.Context, userID, targetID uuid.UUID) error
func (r *Repository) UnblockUser(ctx context.Context, userID, targetID uuid.UUID) error
func (r *Repository) ListFriends(ctx context.Context, userID uuid.UUID) ([]FriendInfo, error)
func (r *Repository) ListIncoming(ctx context.Context, userID uuid.UUID) ([]UserRef, error)
func (r *Repository) ListOutgoing(ctx context.Context, userID uuid.UUID) ([]UserRef, error)
func (r *Repository) ListBlocks(ctx context.Context, userID uuid.UUID) ([]UserRef, error)
func (r *Repository) SearchUsers(ctx context.Context, userID uuid.UUID, q string) ([]UserRef, error)
func (r *Repository) TouchPresence(ctx context.Context, userID uuid.UUID) error
func (r *Repository) IsOnline(ctx context.Context, userID uuid.UUID) bool
```

구현 메모:

- `ListFriends` / `ListIncoming` / `ListOutgoing` 에서 online 여부는 Redis key 존재 여부로 채운다.
- 친구 / 요청 조회 쿼리는 username join 을 바로 해서 handler 에서 추가 호출이 필요 없게 만든다.
- `AcceptRequest` / `BlockUser` / `RemoveFriend` 는 transaction 으로 묶는다.

---

## 9. Kafka

### 9.1 `Services/pkg/messaging/kafka.go`

기존 파일의 const 블록을 확장한다.

```go
const (
    TopicPaymentEvents = "payment-events"
    TopicMatchEvents   = "match-events"
    TopicPlayerEvents  = "player-events"
    TopicSocialEvents  = "social-events"
)
```

### 9.2 Event shape

```go
type SocialEvent struct {
    Type      string    `json:"type"`
    UserID    uuid.UUID `json:"user_id"`
    TargetID  uuid.UUID `json:"target_id"`
    CreatedAt time.Time `json:"created_at"`
}
```

추천 발행 이벤트:

- `FriendRequestSent`
- `FriendAccepted`
- `FriendRemoved`
- `UserBlocked`
- `UserUnblocked`

이번 Phase 에서는 consumer 추가 없음.

---

## 10. `cmd/social/main.go` Pattern

현재 다른 서비스와 동일한 패턴을 따른다.

1. `config.Load()`
2. `database.NewPool()`
3. `cache.NewClient()`
4. `messaging.NewWriter(cfg.Kafka.Brokers, messaging.TopicSocialEvents)`
5. `jwt.NewJWTManager(...)`
6. `social.NewRepository(...)`
7. `social.NewService(...)`
8. `social.NewHandler(...)`
9. `chi.NewRouter()` + `Recovery` + `Logging`
10. JWT 그룹 아래 `/social` mount
11. `/health`
12. graceful shutdown

추가 수정:

- `Services/pkg/config/config.go`
  `SocialPort string`
- `Services/.env.example`
  `SOCIAL_PORT=8087`
- `Services/Makefile`
  `social: go run ./cmd/social`

---

## 11. Matchmaking Integration

### 11.1 Why this is not a separate service-to-service HTTP call yet

현재 Matchmaking 은 이미 PostgreSQL + Redis 를 직접 사용한다.
그리고 큐 규모도 작다.

이번 단계에서 Social HTTP API 를 다시 호출하게 만들면:

- 서비스 간 네트워크 hop 이 늘고
- retry / timeout / circuit breaker 가 새로 필요하고
- 현재 코드베이스의 단순한 패턴에서 벗어난다

따라서 Phase 9B 에서는 `matchmaking.Service` 가 DB 를 직접 읽어 차단 여부를 확인한다.

### 11.2 Patch point

수정 파일:

- `Services/internal/matchmaking/service.go`

추가 메서드:

```go
func (s *Service) isBlockedPair(ctx context.Context, a, b uuid.UUID) bool
```

적용 위치:

- `tryMatch()` 후보 조합 검사 시점
- block 관계가 있으면 해당 pair 는 skip

주의:

- 현재 `matchSize = 2` 이므로 pair check 한 번이면 충분하다.
- 10인 매치로 확장되면 전체 roster pair matrix 검사로 바뀐다.
  그 시점에 Redis set mirror 또는 preloaded adjacency cache 를 검토한다.

---

## 12. C++ Client SDK

### 12.1 File path

기존 코드베이스 기준 경로:

- `Client/Public/Network/Backend/SocialClient.h`
- `Client/Private/Network/Backend/SocialClient.cpp`

### 12.2 Why POST-style routes help

현재 `CHttpClient` 는 비동기 DELETE 를 지원하지 않는다.
그래서 Social API 를 POST action 스타일로 잡으면 아래 이점이 있다.

- `CHttpClient` 수정 없이 바로 연동 가능
- 기존 `AuthClient`, `MatchClient`, `PaymentClient`, `CShopClient` 와 패턴이 맞음
- UI 작업 시 callback 처리 로직을 재사용 가능

### 12.3 Suggested C++ surface

```cpp
struct FriendUserData
{
    string userId;
    string username;
    bool_t online = false;
};

using SocialActionCallback = function<void(bool_t success, const string&)>;
using FriendListCallback = function<void(const vector<FriendUserData>&)>;

class CSocialClient
{
public:
    static unique_ptr<CSocialClient> Create(const string& baseURL);
    void SetAuthToken(const string& token);

    void SendRequest(const string& targetUsername, SocialActionCallback cb);
    void AcceptRequest(const string& targetUserId, SocialActionCallback cb);
    void DeclineRequest(const string& targetUserId, SocialActionCallback cb);
    void RemoveFriend(const string& targetUserId, SocialActionCallback cb);
    void BlockUser(const string& targetUserId, SocialActionCallback cb);
    void UnblockUser(const string& targetUserId, SocialActionCallback cb);

    void GetFriends(FriendListCallback cb);
    void GetIncoming(FriendListCallback cb);
    void GetOutgoing(FriendListCallback cb);
    void Search(const string& q, FriendListCallback cb);
    void Heartbeat(SocialActionCallback cb);
    void ProcessCallbacks();
};
```

---

## 13. Implementation Order

1. `config.go`, `.env.example`, `Makefile`, `messaging/kafka.go` 확장
2. migration 000008, 000009 작성 및 적용
3. `internal/social/model.go`
4. `internal/social/repository.go`
5. `internal/social/service.go`
6. `internal/social/handler.go`
7. `cmd/social/main.go`
8. `matchmaking/service.go` 차단 체크 추가
9. `SocialClient.h/.cpp`
10. `Client.vcxproj` 등록

---

## 14. Verification Checklist

### 14.1 Go service

```text
[ ] register A, B, C users
[ ] A -> B friend request succeeds
[ ] duplicate request rejected
[ ] self request rejected
[ ] B incoming list shows A
[ ] B accepts -> A/B both friends list visible
[ ] A removes B -> both directions deleted
[ ] A blocks C -> friend/pending rows cleaned
[ ] blocked pair cannot send new request
[ ] /social/search?q=a returns username matches without self
[ ] /social/presence/heartbeat writes Redis key with TTL
[ ] Kafka social-events receives FriendRequestSent / FriendAccepted / FriendRemoved / UserBlocked
```

### 14.2 Matchmaking integration

```text
[ ] block relation 없는 두 유저는 기존처럼 매칭됨
[ ] A blocks B 상태에서는 두 유저가 같은 2인 매치로 묶이지 않음
[ ] block 체크 실패 시 서비스가 panic 하지 않고 skip 처리함
```

### 14.3 C++ client

```text
[ ] CSocialClient login token 전달 후 /social/friends GET 성공
[ ] request / accept / remove / block 액션 callback 정상 수신
[ ] ProcessCallbacks() 를 게임 루프에서 호출해도 stall 없음
```

---

## 15. Summary

이 Phase 의 핵심은 "현재 코드베이스 패턴을 깨지 않고 Social 기능을 얹는 것" 이다.

- Go 쪽은 기존 `cmd/ + internal/ + pkg/` 구조를 그대로 따른다.
- C++ 쪽은 현재 `CHttpClient` 제약을 존중해서 GET/POST 중심으로 간다.
- Matchmaking 차단 연동은 지금은 DB correctness 우선으로 붙인다.
- 이후 파티, 알림, 실시간 presence 는 별도 Phase 로 확장한다.
