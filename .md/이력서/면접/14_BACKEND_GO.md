# 14. 백엔드 서비스 (Go) — 면접 대비 세션

> 도메인 성숙도: **working** (핵심 경로 구현·동작, 단 자동 테스트 0개·실결제 게이트웨이 없음)
> 정직성 근거: `.md/이력서/WINTERS_DOMAIN_HONEST_MAP_2026-06-26.md` 의 "### 14. 백엔드 (Go)"
> 이 문서의 모든 코드 인용은 실제 repo 파일:라인 기준이다. 과장 = 면접 코드리뷰 즉사.

---

## 0. 한 줄 본질 + 현재 상태

**한 줄 본질**: 게임 "매치(in-game)" 안에서 다룰 수 없는 **영속 상태(persistent state) — 계정·지갑·전적·랭킹·재화·인벤토리 —** 를 C++ 게임 서버와 분리해, Go 마이크로서비스 6종 + Postgres/Redis/Kafka 위에 올린 백엔드다. 게임 루프(30Hz 권위 시뮬, 도메인 6)는 "지금 이 판"만 책임지고, "판 밖에서 영원히 남는 것"은 이 백엔드가 책임진다.

**현재 성숙도 (정직하게)**:
- **working**: Auth / Leaderboard / Matchmaking / Profile / Payment / Shop 6개가 모두 `go build` 되고 실제로 뜨며, C++ 클라이언트가 WinHTTP SDK로 `/auth/login` 등을 실제 호출한다(`Client/Private/Network/Backend/AuthClient.cpp:41`).
- **prototype-grade인 부분**: 결제는 `MockGateway`만 등록(실 PG 연동 없음), 매치메이킹은 `matchSize=2`, 이벤트 컨슈머는 DLQ 없이 실패 메시지를 nil-skip.
- **검증 공백**: `*_test.go` **0개** (`find Services -name '*_test.go'` → 빈 결과). 검증은 `go build` + 수동 curl 체크리스트 수준.
- **planned**: Social/Friend(Phase 9), Replay(Phase 10)는 계획서만 존재, 코드 0줄.

면접 한 문장: "6개 서비스가 실제로 돌고 C++ 클라와 실연동까지 했지만, **'실서비스'가 아니라 '실서비스의 정합성 핵심(트랜잭션·멱등성·이벤트 비동기 전파)을 구현한 포트폴리오'** 라는 경계를 제가 직접 긋습니다."

---

## 1. 핵심 개념 (본질부터)

### 1.1 왜 게임에 별도 백엔드가 필요한가 — 상태의 수명(lifetime) 분리
게임 상태는 수명으로 갈린다.
- **휘발성 매치 상태**: 캐릭터 위치/HP/쿨타임. 30Hz로 갱신, 매치 끝나면 버려짐. → C++ 권위 시뮬(도메인 6)이 메모리에서 처리, DB 안 감.
- **영속 상태**: 누가 이겼나(전적), 지갑 잔액, 랭킹, 산 스킨. 매치가 끝나도 **영원히 정확해야** 한다. → 이건 ACID DB가 source of truth여야 한다.

이 둘을 같은 프로세스에 섞으면 게임 틱 루프가 DB I/O(수~수십 ms)에 막혀 결정론과 프레임이 깨진다. 그래서 **프로세스/언어/저장소 경계를 그어 분리**한다. C++ 게임 서버는 짧은 결정론 루프, Go 백엔드는 요청-응답 + 비동기 이벤트.

### 1.2 왜 마이크로서비스로 쪼갰나 — 결합도와 부하 특성의 차이
한 서비스로 묶을 수도 있었다. 쪼갠 1차 원리는 **부하 프로파일과 일관성 요구가 도메인마다 다르기** 때문:
- **Auth**: 쓰기 드묾, 강한 일관성(중복 가입 차단), 토큰 회전.
- **Leaderboard**: 읽기 폭발(랭킹 조회), 근사 일관성 허용 → Redis ZSET이 source.
- **Payment/Shop**: **돈** → 절대적 ACID, FOR UPDATE 행잠금, 멱등성.
- **Matchmaking**: 큐는 휘발성 + 시간에 따라 조건 완화 → Redis ZSET + 1초 틱 매처.

신입 1인 범위에서 이건 "물리적으로 분산 운영하려는 야망"이 아니라 **도메인 경계를 코드로 못박는 학습 도구**다(전부 `go run ./cmd/<svc>`, 컨테이너화 안 함 — `00_BACKEND_PLAN_INDEX.md:120`).

### 1.3 트랜잭션 정합성 — ACID와 동시성
"지갑 차감"은 백엔드의 핵심 난제다. 두 클라가 동시에 같은 잔액을 보고 각자 구매하면 잔액이 음수가 되는 **lost update / TOCTOU**가 난다. 해법:
- **하나의 트랜잭션** 안에서 `SELECT ... FOR UPDATE`로 지갑 행을 **비관적 잠금** → 잔액 확인과 차감 사이에 다른 트랜잭션이 끼어들지 못함(`internal/shop/repository.go:64`).
- 차감·인벤토리 추가·원장(coin_transactions) 기록이 **원자적으로 커밋**되거나 통째로 롤백.

### 1.4 멱등성(idempotency) — 네트워크는 거짓말한다
결제 요청은 네트워크 타임아웃으로 클라가 재시도할 수 있다. 같은 결제가 두 번 처리되면 이중 충전. 1차 원리: **클라가 생성한 멱등키를 서버가 유일성으로 강제**해, 같은 키의 두 번째 요청은 "처리"가 아니라 "이전 결과 재반환"이 되게 한다(`internal/payment/service.go:42`, DB는 `idempotency_key VARCHAR(64) NOT NULL UNIQUE` — `migrations/000005:4`).

### 1.5 이벤트 드리븐 — 동기 호출의 결합을 끊다
"매치 종료 → 전적 갱신 + 랭킹 갱신"을 매치 서버가 직접 호출하면, 백엔드가 죽었을 때 게임이 막히고 새 후속 작업마다 호출부를 고쳐야 한다. 해법: 매치 종료 사실을 **Kafka 토픽에 producer가 1번 던지고**, Profile/Leaderboard가 각자 **consumer group으로 구독**해 비동기 처리(`internal/profile/consumer.go`, `internal/leaderboard/consumer.go`). producer는 누가 듣는지 모름 = 결합 분리.

### 1.6 JWT 무상태 인증 + refresh 회전
HTTP는 무상태라 매 요청 인증이 필요하다. 세션 테이블을 매 요청 조회하면 병목. JWT는 **서명된 토큰 자체가 신원**이라 검증이 서명 확인뿐(DB 불필요). 단점: 한 번 발급하면 만료 전 취소 불가 → 그래서 **짧은 access + 긴 refresh**로 나누고, refresh는 Redis에 jti를 저장해 **회전 시 기존 토큰 무효화**(취소 가능성 확보, `internal/auth/service.go:247`).

---

## 2. 왜 이 선택인가 — 스택 + Trade-off

### 2.1 언어: Go
| 선택지 | 장점 | 단점 | 내 결정 이유 |
|---|---|---|---|
| **Go (택함)** | goroutine 동시성·표준 net/http·정적바이너리·낮은 진입장벽·풍부한 DB/Kafka 드라이버 | 제네릭 빈약(당시), 표현력 낮음 | 게임 본체가 C++ 멀티스레드라 **백엔드는 단순·견고·동시성 쉬움**이 우선. Go가 I/O 바운드 마이크로서비스의 정석 |
| C++ 백엔드 | 게임과 언어 통일 | HTTP/DB/Kafka 생태계 빈약, 생산성 ↓, 메모리 안전 부담 | 백엔드까지 C++면 학습 비용·버그면적만 폭증 |
| Node/Spring | 생태계 큼 | 런타임 무거움, GC/JVM 부담 | 1인 운영에 과함, Go 배포 단순성 못 이김 |

### 2.2 저장소: Postgres + Redis 이원화
| 데이터 | 저장소 | 이유 |
|---|---|---|
| 계정·지갑·전적·결제원장 | **PostgreSQL** | ACID·트랜잭션·FOR UPDATE·UNIQUE 제약이 돈/정합성에 필수 |
| 랭킹·매치큐·refresh 토큰 | **Redis** | ZSET(O(log N) 랭킹), TTL(토큰/큐 만료), 읽기 폭발 흡수 |

랭킹은 Redis ZSET가 빠른 조회용 캐시이되 `player_stats.mmr`(Postgres)이 진짜 source. Redis가 비면 `SyncFromDB`로 재구축(`internal/leaderboard/repository.go:116`). → **Trade-off**: 이중 쓰기 정합성 부담을 지는 대신 조회 성능. 단일 사용자 포트폴리오 규모에선 합리적.

### 2.3 결제: 게이트웨이 추상화 + Mock
| 선택지 | 장점 | 단점 | 결정 |
|---|---|---|---|
| **`PaymentGateway` 인터페이스 + MockGateway (택함)** | PG 계약 없이 멱등성·트랜잭션·이벤트 전 경로 구현·검증 가능 | 실 영수증 검증 없음(가짜 txID) | 신입 포트폴리오에서 **실 PG 키를 얻을 수 없음** → 인터페이스로 경계만 정확히 긋고 정합성 로직에 집중 |
| 실 PG 직접 연동 | 진짜 결제 | 계정/심사/PCI 부담, 1인 범위 초과 | 범위 밖 |

`gateway.go`의 인터페이스(`VerifyReceipt`/`Refund`)는 진짜 PG로 교체할 자리를 비워둔 설계다. **이건 "결제 시스템을 만들었다"가 아니라 "결제 시스템의 정합성 골격을 만들었다"** — 면접에서 이 표현을 정확히 쓴다.

### 2.4 이벤트: Kafka (vs 동기 호출 / RabbitMQ)
- 동기 REST 호출: 결합·장애 전파 → 탈락.
- Kafka 택함: 토픽 보존·consumer group·offset으로 **at-least-once + 재처리** 기반. 단, 현재 컨슈머는 멱등 처리/DLQ 미구현이라 at-least-once의 중복/유실 방어가 약함(2.5 참조).
- RabbitMQ도 가능했으나, 게임 분석/리플레이로 확장 시 **로그 보존형 Kafka**가 더 맞다고 판단.

### 2.5 의식적으로 **버린/미룬** 것 (정직성 핵심)
- **자동 테스트 0개**: 1인 빠른 반복에서 빌드+수동 curl로 갈음. → 이건 약점이고, 면접에서 "다음 1순위가 테이블 드리븐 단위 테스트"라고 먼저 말한다.
- **DLQ/멱등 컨슈머 없음**: consumer가 unmarshal 실패 시 `return nil`로 그냥 스킵(`internal/leaderboard/consumer.go:30`) → 메시지 유실. 의도적 단순화임을 인정.
- **컨테이너화 미적용**: 인프라(Postgres/Redis/Kafka)만 docker-compose, Go 앱은 `go run`.

---

## 3. 실제 구현 (코드 근거)

### 3.1 디렉토리 구조 (실측)
```
Services/
  cmd/{auth,leaderboard,matchmaking,payment,profile,shop}/main.go   # 6개 진입점
  internal/<svc>/{handler,service,repository,model}.go              # 도메인별 레이어
  internal/{leaderboard,profile}/consumer.go                        # Kafka consumer
  internal/payment/{gateway.go, gateway_mock.go}                    # 게이트웨이 추상화
  pkg/{auth,cache,config,database,errors,messaging,middleware,response}/
  migrations/000001~000007_*.sql                                    # 7개 스키마
  go.mod   # go 1.26.2, chi/v5, pgx/v5, go-redis/v9, kafka-go, jwt/v5, x/crypto
```
레이어드 아키텍처: **handler(HTTP) → service(도메인 로직) → repository(저장소)**. 의존성은 단방향. 진입점은 전부 동일 패턴: config 로드 → 인프라 연결 → 의존성 와이어링 → graceful shutdown(SIGINT/SIGTERM, `cmd/auth/main.go:461`).

### 3.2 Auth — 트랜잭션 가입 + JWT 회전
- **원자적 가입**: `CreateUserWithWalletAndStats`가 단일 tx로 users+wallets(balance 0)+player_stats(mmr 1000)를 INSERT, 하나라도 실패하면 `defer tx.Rollback`(`Phase1_AuthService.md`의 코드가 실제 `repository.go`와 일치).
- **JWT pair**: access/refresh를 **서로 다른 secret으로 HS256 서명**, 각 토큰에 jti(`uuid.New()`) 부여(`pkg/auth/jwt.go:49,63`). 검증 시 `SigningMethodHMAC` 타입을 강제해 **alg 혼동 공격(none/RS↔HS) 차단**(`jwt.go:79`).
- **refresh 회전**: `Refresh`가 (1) 서명 검증 → (2) Redis에 jti 존재 확인 → (3) 기존 jti 삭제 → (4) 새 pair 발급·저장(`service.go:247`). 따라서 한 번 쓴 refresh는 재사용 불가, logout은 jti를 Redis에서 Del.
- **미들웨어**: `JWTAuth`가 `Authorization: Bearer` 파싱→access 검증→claims를 context 주입(`pkg/middleware/auth.go:16`). 매치메이킹 핸들러가 `middleware.GetClaims`로 user_id 획득(`internal/matchmaking/handler.go:28`).

### 3.3 Payment — 멱등성 + 트랜잭션 + 이벤트 (가장 깊게 볼 곳)
호출 경로 `Charge`(`internal/payment/service.go:34`):
1. `coin_amount>0`, `idempotency_key != ""` 검증.
2. `FindByIdempotencyKey` — **이미 처리된 키면 이전 ChargeResponse를 그대로 반환**(재처리 안 함, `service.go:42`).
3. 게이트웨이 조회 후 `VerifyReceipt` (Mock은 `mock_<uuid>` 반환).
4. `ProcessCharge` 단일 tx: payment_transactions INSERT → `UPDATE wallets SET balance = balance + $2 RETURNING balance` → coin_transactions(원장) INSERT → commit(`repository.go:23`).
5. `PaymentCompleted` 이벤트를 Kafka `payment-events`에 발행(`service.go:74`).

DB 레벨 2차 방어: `idempotency_key ... UNIQUE`라 동시 두 요청이 둘 다 단계 2를 통과해도 두 번째 INSERT가 제약 위반으로 실패 → 이중 충전 불가(`migrations/000005:4`).

### 3.4 Shop — FOR UPDATE 비관적 잠금
`Purchase`(`internal/shop/repository.go:44`): tx 안에서 아이템 가격 조회 → `SELECT balance FROM wallets WHERE user_id=$1 FOR UPDATE`로 지갑 행 잠금 → 잔액<가격이면 `ErrInsufficientBalance` 롤백 → 차감 → inventory `ON CONFLICT DO UPDATE quantity+1` → 원장 기록 → commit. **FOR UPDATE가 동시 구매 lost-update를 막는 핵심**.

### 3.5 Matchmaking — Redis ZSET + 시간 확장 매처
- `Join`(`service.go:42`): 중복 큐 차단 → player_stats에서 mmr 조회 → `ZAdd queue (score=mmr)` + jointime/status 키 TTL 설정.
- `RunMatcher`: **1초 틱**(`time.NewTicker(time.Second)`)으로 `tryMatch` 반복(`service.go:96`).
- `tryMatch`: 큐를 score순 정렬해 받아, 각 후보 i에 대해 `calcRange`로 허용 MMR 범위 계산 후 인접 j와 매칭. **대기 시간이 길수록 범위 확장**: `baseRange(200) + (waitSec/30)*50`(`service.go:172`) → 처음엔 비슷한 실력끼리, 오래 기다리면 점점 느슨하게.
- 성사 시 큐에서 제거·status를 `matched:<id>`로·`MatchCreated` 이벤트 발행(`service.go:175`). **단 matchSize=2 프로토타입**.

### 3.6 Leaderboard / Profile — Kafka consumer
- `messaging.Consume`(`pkg/messaging/kafka.go:39`): `ReadMessage` 루프, ctx 취소 시 종료, 핸들러 에러는 로깅(offset 포함)하되 **재시도/DLQ 없음**.
- Leaderboard consumer: `MatchCompleted`만 처리, 플레이어별 `currentMMR + MMRChange`(0 하한)로 Redis ZSET + player_stats 동시 갱신(`internal/leaderboard/consumer.go:26`, `repository.go:24`).
- Profile consumer: match_history INSERT + player_stats 갱신 + 캐시 무효화(`internal/profile/consumer.go:26`).
- **유실 지점(정직)**: 두 consumer 모두 unmarshal 실패 시 `return nil`로 스킵 = 메시지 영구 유실.

### 3.7 C++ 클라이언트 SDK — WinHTTP 비동기
- `CHttpClient`(`Client/Private/Network/Backend/CHttpClient.cpp`): `AsyncPost`이 `std::async`로 **별도 스레드에서 WinHTTP 요청**, 완료 콜백을 `m_PendingCallbacks` 큐에 mutex로 적재(`CHttpClient.cpp:101`). 메인 스레드가 매 프레임 `ProcessCallbacks`로 펌프(`:114`) → **게임 루프가 네트워크 I/O에 멈추지 않음**.
- `AuthClient::Login`이 실제로 `/auth/login`을 AsyncPost(`Client/Private/Network/Backend/AuthClient.cpp:41`). register/refresh도 실 배선.
- **주의(red flag)**: SDK 경로는 실연동이지만, **로그인 씬의 버튼은 오프라인 로그인만 호출** — "E2E 로그인 데모 완성"이라 말하지 않는다. "SDK·서버 경로 구현, UI 배선은 오프라인 기준".

---

## 4. 검증 — 어떻게 "됐다"를 판정했나

정직하게: **자동화 테스트 0개**(`find Services -name '*_test.go'` → 빈 결과). 검증 수단은:
1. **`go build ./...` / `go vet`**: 컴파일·타입·미사용 import 게이트.
2. **수동 curl 체크리스트**(`Phase1_AuthService.md:475`): register→201+토큰, 같은 email→409, login→200, 틀린 비번→401, `Redis EXISTS refresh:{jti}=1`, Postgres에 users/wallets(0)/player_stats(1000) 1행씩, refresh→새 pair, logout 후 old refresh→401.
3. **저장소 상태 직접 확인**: psql로 트랜잭션 결과(지갑 잔액·원장), redis-cli로 ZSET 랭킹.
4. **C++ 실연동 확인**: 클라가 실제 서버에 붙어 토큰을 받아오는지 로그/응답으로.

→ 면접 답변: "검증은 **빌드 + 수동 curl + 저장소 직접 조회 + 클라 실연동** 수준이고, 자동 테스트가 없는 게 이 도메인의 최대 약점입니다. 그래서 **다음 1순위가 멱등성·FOR UPDATE 동시성·refresh 회전에 대한 테이블 드리븐 단위/통합 테스트**입니다." (6번 참조)

---

## 5. 최적화 (한 것 / 할 것)

**실제로 한 것**:
- **랭킹 O(log N)**: Postgres `ORDER BY mmr LIMIT`로 매 조회 정렬하는 대신 Redis ZSET으로 `ZRevRange`/`ZRevRank`/`ZScore`/`ZCard` (`repository.go:41~99`). 랭킹 읽기 폭발을 메모리 자료구조로 흡수.
- **SyncFromDB 파이프라인**: Redis 재구축 시 `pipe := rdb.Pipeline()`로 ZAdd 배치 후 한 번 Exec(`repository.go:123`) → RTT 절감.
- **클라 비동기 I/O**: WinHTTP를 워커 스레드로 빼 게임 프레임 정지 제거(4 = 사실상 지연 최적화).
- **Kafka writer 배치**: `BatchTimeout 10ms`, `RequiredAcks RequireOne`(`pkg/messaging/kafka.go:17`) — 지연/내구성 균형.

**정량 수치**: 부하 테스트·벤치 미실시 → **"측정 예정"**. "N만 TPS" 같은 숫자는 절대 말하지 않는다.

**계획 중**:
- Auth `/auth/me` 등 hot path에 Redis 캐시.
- Connection pool 튜닝(pgxpool 크기), `prepared statement`.
- Leaderboard top-N 결과 캐싱(현재 매 조회마다 각 유저 username/wins를 개별 쿼리 — `repository.go:60` N+1 위험, 이건 알려진 약점).

---

## 6. 구현 예정 (Planned) — 구현된 부분과 동일한 깊이

### 6.1 Social / Friend Service (Phase 9, port 8087) — 코드 0줄, 설계만
- **무엇**: friend request/accept/decline/remove, block/unblock, presence(온라인 여부), 유저 검색.
- **왜**: 매칭/로비에 소셜 그래프가 없으면 "친구와 큐", "차단 유저 회피"가 불가. 게임성의 영속 사회 계층.
- **어떻게**:
  - 자료구조: `friendships(user_id, friend_id, status)` 대칭 관계 + `friend_blocks` 테이블(migration 000008/000009 예정 — `00_BACKEND_PLAN_INDEX.md:109`). 친구 관계는 양방향이라 (min,max) 정규화 또는 두 행 삽입 중 택일 — **나는 두 행(방향성 status: pending/accepted)으로 가서 "A가 B에게 요청" 방향을 보존**할 것.
  - presence: Redis `SETEX presence:<uid> TTL` heartbeat, 친구 목록 조회 시 MGET.
  - Auth JWT 미들웨어·users 테이블 재사용, 차단은 Matchmaking에 block 체크 패치(Phase 9B).
- **Trade-off**: presence를 Redis TTL로 하면 정확하지만 heartbeat 트래픽 발생 vs DB last_seen 폴링은 싸지만 부정확 → **실시간성 우선이라 Redis 택**. 친구 그래프를 Postgres에 두면 join 비용, Redis SET에 캐시하면 정합성 부담 → MVP는 Postgres 정공법.
- **검증**: 요청→수락→목록 반영, 차단 후 매칭 회피, presence TTL 만료 시 오프라인 표시를 curl + 저장소 조회로. (그리고 이번엔 **단위 테스트부터** 붙인다.)

### 6.2 Replay Service (Phase 10, port 8088) — 코드 0줄, 설계만
- **무엇**: 게임 서버가 매치 스냅샷 스트림을 `.wrpl`로 캡처→Go가 ingest(메타데이터 DB + 파일 저장)→클라가 다운로드해 오프라인 재생.
- **왜**: 전적의 "결과"만이 아니라 "과정"을 다시 보게. 결정론 시뮬(도메인 6)이라 **입력+스냅샷만 있으면 재현 가능**하다는 게 핵심 레버리지.
- **어떻게**:
  - 서버측 `ReplayRecorder`가 기존 SnapshotBuilder 출력(FlatBuffers, `Shared/Schemas/Snapshot.fbs`)을 프레임 단위로 append.
  - Go Replay Service: `replays(id, match_id, created_at, size, path)` 메타 + 로컬 FS(`Services/data/replays`) 저장(MVP는 S3 안 씀).
  - 클라 `CReplayPlayer` + `Scene_Replay`가 **기존 SnapshotApplier 재사용**해 라이브와 동일 경로로 재생.
  - **Phase 10A(매치 전체 익명 리플레이)와 10B(user-scoped 개인 라이브러리) 분리** — 이유: 현재 `Server/GameRoom`은 `sessionId`만 알고 백엔드 `user_id`를 모름(`00_BACKEND_PLAN_INDEX.md:35`). 신원 브리지가 생기기 전엔 "내 리플레이" 소유권을 못 매김 → **모르는 걸 안다고 안 만들고 의존성 순서대로 미룬다**.
- **Trade-off**: 스냅샷 전체 저장은 용량↑ 정확↑ vs 입력만 저장+재시뮬은 용량↓ 결정론 위험↑. MVP는 **스냅샷 저장(단순·확실)**, 이후 입력 기반 압축은 결정론 게이트(SimLab) 신뢰가 쌓이면.
- **검증**: 같은 매치를 라이브와 리플레이로 돌려 per-frame 상태가 일치하는지(도메인 6의 FNV 해시 비교 재사용).

### 6.3 검증 인프라 보강 (가장 시급)
- **테이블 드리븐 단위 테스트**: jwt(만료/위조/alg혼동), payment 멱등성(같은 키 2회→1회만 충전), shop 동시 구매(goroutine 2개로 FOR UPDATE 경합).
- **통합 테스트**: `testcontainers-go`로 Postgres/Redis 띄워 repository 실제 쿼리 검증.
- **컨슈머 안정화**: DLQ 토픽 + 멱등 처리(이벤트 id 중복 무시)로 at-least-once를 안전하게.

---

## 7. 면접 예상 질문 & 모범 답변

### Q1. (기본) 왜 게임 서버랑 백엔드를 분리했나요?
상태 수명이 다르기 때문입니다. 캐릭터 위치/HP 같은 휘발성 매치 상태는 C++ 30Hz 권위 시뮬이 메모리에서 결정론적으로 처리하고, 전적·지갑·랭킹 같은 영속 상태는 ACID DB가 source of truth여야 합니다. 둘을 같은 프로세스에 섞으면 게임 틱이 DB I/O에 막혀 프레임과 결정론이 깨집니다. 그래서 프로세스·언어·저장소 경계를 그어 분리했습니다.

### Q2. (기본) 왜 Postgres와 Redis를 같이 쓰나요?
일관성 요구가 다릅니다. 돈/계정(지갑·결제·전적)은 ACID·트랜잭션·UNIQUE 제약이 필수라 Postgres, 랭킹·매치큐·refresh 토큰은 ZSET의 O(log N) 랭킹과 TTL이 맞아 Redis입니다. 랭킹은 Redis가 빠른 조회 캐시이되 `player_stats.mmr`이 진짜 source이고, Redis가 비면 `SyncFromDB`로 재구축합니다.

### Q3. (설계) 결제 이중 충전을 어떻게 막나요?
2단 방어입니다. 애플리케이션 레벨에서 클라가 생성한 `idempotency_key`로 먼저 조회해 이미 처리된 키면 이전 결과를 그대로 반환합니다(`payment/service.go:42`). 그래도 동시 요청 둘이 그 체크를 같이 통과하면, DB의 `idempotency_key UNIQUE` 제약이 두 번째 INSERT를 거부합니다(`migrations/000005:4`). 그리고 충전·원장 기록은 단일 트랜잭션이라 부분 반영이 없습니다.

### Q4. (설계) 동시 구매로 잔액이 음수가 되는 걸 어떻게 막나요?
구매를 하나의 트랜잭션으로 묶고, 잔액을 읽기 전에 `SELECT balance FROM wallets WHERE user_id=$1 FOR UPDATE`로 지갑 행을 비관적 잠금합니다(`shop/repository.go:64`). 그러면 잔액 확인과 차감 사이에 다른 트랜잭션이 못 끼어듭니다. 잔액<가격이면 그 자리에서 롤백합니다. 이게 read-modify-write의 TOCTOU를 막는 핵심입니다.

### Q5. (설계) JWT를 쓰면 토큰을 어떻게 무효화하나요?
순수 JWT는 만료 전 취소가 안 되는 게 약점입니다. 그래서 짧은 access + 긴 refresh로 나누고, access 검증은 서명만으로(무상태), refresh는 jti를 Redis에 저장해 상태를 둡니다. refresh 회전 시 기존 jti를 삭제하고 새 토큰을 발급하므로(`service.go:247`) 쓴 refresh는 재사용 불가, logout은 jti를 Del하면 즉시 무효입니다. 그리고 검증 시 `SigningMethodHMAC`을 강제해 alg 혼동 공격을 막습니다.

### Q6. (압박/red flag) 이거 "결제 시스템"이라고 하셨는데, 실제 PG 연동은 됐나요?
아닙니다. 정확히는 **결제의 정합성 골격**을 만들었고, 게이트웨이는 `MockGateway`만 등록돼 `VerifyReceipt`가 가짜 txID를 반환합니다(`gateway_mock.go:12`). 신입 포트폴리오에서 실 PG 키·심사·PCI는 범위 밖이라, 대신 `PaymentGateway` 인터페이스로 PG가 들어올 자리를 정확히 비워두고(`gateway.go`) **멱등성·FOR UPDATE 트랜잭션·이벤트 전파라는 진짜 어려운 부분**에 집중했습니다. 실 연동은 인터페이스 구현체 하나를 교체하는 작업입니다.

### Q7. (압박/red flag) 테스트는 있나요?
없습니다. `Services`에 `*_test.go`가 0개이고, 검증은 `go build` + 수동 curl 체크리스트 + 저장소 직접 조회 수준입니다. 이게 이 도메인 최대 약점이라고 봅니다. 그래서 다음 1순위가 명확합니다 — jwt 위조/만료/alg혼동, payment 멱등성(같은 키 2회→1회 충전), shop 동시 구매를 goroutine 경합으로 검증하는 테이블 드리븐 단위 테스트와, `testcontainers-go`로 실제 Postgres/Redis를 띄우는 통합 테스트입니다.

### Q8. (압박/red flag) 이벤트 파이프라인 "신뢰성 있다"고 할 수 있나요?
아니요, 거기까진 아닙니다. Kafka producer→consumer로 매치 결과를 비동기 전파하는 경로는 동작하지만, 컨슈머가 unmarshal 실패 메시지를 `return nil`로 그냥 스킵해(`leaderboard/consumer.go:30`) 유실되고 DLQ가 없습니다. Kafka 자체는 at-least-once를 주지만 제 컨슈머가 멱등 처리를 안 해 중복도 안전하지 않습니다. 그래서 보강 계획이 DLQ 토픽 + 이벤트 id 기반 멱등 컨슈머입니다. 지금은 "비동기 전파 경로 구현", "신뢰성 보장"은 아닙니다.

### Q9. (압박) 매치메이킹이 실서비스 수준인가요?
프로토타입입니다. `matchSize=2`로 고정돼 있고(`service.go:28`), 1초 틱 매처가 Redis ZSET을 score순으로 훑어 대기 시간에 따라 MMR 허용 범위를 `200+(waitSec/30)*50`으로 확장하는 로직만 구현돼 있습니다(`service.go:172`). 5v5 팀 밸런싱·롤 큐·파티는 없습니다. 알고리즘의 뼈대(시간 기반 범위 완화)는 맞지만 규모/팀 구성은 확장 과제입니다.

### Q10. (심화) Kafka 대신 동기 REST 호출로 전적을 갱신하면 안 되나요?
가능하지만 결합이 문제입니다. 매치 서버가 Profile/Leaderboard를 직접 호출하면, 그 서비스가 죽었을 때 게임이 막히고, 새 후속 처리(예: 업적, 통계)를 추가할 때마다 호출부를 고쳐야 합니다. Kafka에 `MatchCompleted`를 한 번 던지면 producer는 누가 듣는지 모르고, consumer를 추가해도 producer는 안 바뀝니다. 장애 격리·확장성·재처리(offset 되감기)를 위해 이벤트 드리븐을 택했습니다.

### Q11. (심화) Leaderboard 조회에 성능 문제가 보이나요?
네, N+1이 있습니다. `GetTop`이 Redis ZSET으로 top-N user_id는 O(log N)에 받지만, 각 유저의 username/wins/losses를 개별 Postgres 쿼리로 가져옵니다(`repository.go:60`). N개면 N번 쿼리죠. 개선은 user_id를 모아 `WHERE id = ANY($1)` 한 방 조회로 묶거나, 자주 보는 top-N 자체를 캐싱하는 겁니다. 알고는 있고 최적화 백로그에 있습니다.

### Q12. (심화) 클라가 네트워크 요청 때문에 안 멈추게 어떻게 했나요?
WinHTTP 요청을 `std::async`로 워커 스레드에서 돌리고, 완료 콜백을 mutex 보호 큐에 적재합니다(`CHttpClient.cpp:101`). 게임 메인 스레드는 매 프레임 `ProcessCallbacks`로 그 큐를 펌프해 콜백을 실행합니다(`:114`). 그래서 로그인/매치 join 같은 요청이 프레임을 막지 않고, 응답 처리는 항상 메인 스레드에서 일어나 스레드 안전합니다.

### Q13. (확장) Replay 아직 안 만드셨죠? 어떻게 만들 건가요?
네, 코드 0줄 계획 단계입니다. 핵심 레버리지는 시뮬이 결정론이라(도메인 6) 스냅샷 스트림만 있으면 재현된다는 점입니다. 서버 `ReplayRecorder`가 기존 SnapshotBuilder의 FlatBuffers 출력을 프레임마다 append→Go Replay Service가 메타데이터(DB)와 파일(로컬 FS)로 ingest→클라가 기존 SnapshotApplier를 재사용해 오프라인 재생합니다. 단, 개인 라이브러리("내 리플레이")는 `GameRoom`이 `sessionId`만 알고 백엔드 `user_id`를 몰라서 신원 브리지가 생기는 Phase 10B로 의식적으로 미뤘습니다 — 모르는 걸 안다고 만들지 않습니다.

### Q14. (심화) refresh 토큰을 탈취당하면요?
회전이 1차 방어입니다. 정상 사용자가 한 번 refresh하면 그 jti가 Redis에서 지워지므로(`service.go:259`), 탈취자가 같은 토큰을 쓰면 Redis 존재 검사에서 막혀 401입니다(`service.go:256`). 더 강하게 하려면 "이미 무효화된 refresh가 재사용되면 해당 유저의 모든 refresh를 폐기(reuse detection)"하는 게 정석인데, 이건 아직 구현 안 했고 보강 항목입니다. 지금은 회전+Redis 무효화까지입니다.

---

## 8. 30초 엘리베이터 피치

"게임은 30Hz 결정론 시뮬이 '지금 이 판'만 책임지고, 판이 끝나도 영원히 정확해야 하는 것들 — 계정·지갑·전적·랭킹·재화 — 은 제가 Go 마이크로서비스 6개로 분리해 Postgres·Redis·Kafka 위에 올렸습니다. 돈이 걸린 결제·구매는 FOR UPDATE 행잠금과 멱등키 UNIQUE 제약으로 이중 충전과 잔액 음수를 막고, 매치 결과는 Kafka 이벤트로 전적·랭킹에 비동기 전파하고, C++ 클라는 WinHTTP를 워커 스레드로 빼 프레임을 안 멈추면서 실제로 로그인까지 붙입니다. 솔직히 자동 테스트가 0개고 결제는 MockGateway라 '실서비스'가 아니라 '실서비스의 정합성 핵심을 구현한 포트폴리오'인데, 저는 그 경계를 멱등성·트랜잭션·이벤트라는 진짜 어려운 부분에 집중하는 방식으로 그었습니다."
