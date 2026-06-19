Session - 출시 이후 사용자 데이터, backend service, game server authority, 부하 관리를 원자 계약으로 고정한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/.md/architecture/WINTERS_BACKEND_SERVER_POSTLAUNCH_ATOMS.md

새 파일:

````md
# Winters Backend Server Postlaunch Atoms

이 문서는 출시 이후 Winters의 사용자 데이터, backend service, game server authority, 부하 관리 방향을 더 나눌 수 없는 원자 단위로 고정한다.

현재 코드 기준:
- `Services/`는 Go service 모노레포이며 `auth`, `leaderboard`, `matchmaking`, `profile`, `payment`, `shop`이 있다.
- `Services/migrations/`에는 `users`, `wallets`, `player_stats`, `match_history`, `payment_transactions`, `coin_transactions`, `inventory`가 있다.
- `Services/pkg/`에는 PostgreSQL, Redis, Kafka, JWT, middleware 공용 패키지가 있다.
- `Server/`는 `GameRoom`, `LobbyAuthority`, `SnapshotBuilder`, `ReplayRecorder`, IOCP network/session, lag compensation을 가진 authority runtime이다.
- `Shared/GameSim`과 `Shared/Schemas`는 gameplay truth와 command/snapshot/event contract의 기준이다.

## 북극성

```text
Client는 요청하고 보여준다.
Game Server는 매치 안의 truth를 결정한다.
Services는 매치 밖의 사용자 영속 상태를 기록한다.
Infra는 들어올 수 있는 부하만 받아들이고, 밀린 부하는 줄 세우거나 거절한다.
Patch/Data는 어떤 버전의 규칙과 콘텐츠가 유효한지 고정한다.
```

Winters의 출시 후 backend 방향은 하나의 흐름으로 판정한다.

```text
Client Intent
-> Authenticated Service Request 또는 GameCommand
-> Services UserState 또는 Server GameSim
-> Ledger/Event/MatchResult
-> ReadModel/ViewState
-> Client Presentation
```

## 경계

### Server

Server는 match 안의 권위만 가진다.

소유:
- `GameCommand` intake
- room/session authority
- GameSim tick
- snapshot/event/cue emission
- replay/match result emission
- bot command generation
- lag compensation과 anti-cheat gate

금지:
- account, payment, store, profile DB 직접 수정
- client visual 성공 판정
- 결제/소유권/재화 지급 결정
- Services 장애를 gameplay truth 변경으로 우회

### Services

Services는 match 밖의 영속 상태만 가진다.

소유:
- account/auth session
- profile/progression read model
- payment receipt
- wallet ledger
- entitlement
- inventory
- shop/catalog
- matchmaking ticket
- match result ingestion
- leaderboard/profile projection
- live ops policy

금지:
- match 중 HP, damage, cooldown, projectile, win/loss를 직접 판정
- GameSim truth를 Redis cache 또는 Kafka consumer 상태로 대체
- Client가 보낸 구매/보유 목록을 신뢰

### Infra

Infra는 scale 자체가 아니라 admission, queue, backpressure, degradation을 소유한다.

소유:
- service health
- capacity registry
- rate limit
- queue depth
- DB pool budget
- Redis/Kafka lag budget
- graceful degradation policy

금지:
- 기능 구현을 infra 설정으로 숨김
- 부하 테스트 통과를 위해 normal F5 runtime 기능 제거
- cache를 source of truth로 승격

## 원자

| 원자 | 소유자 | 본질 | 현재 기준 |
|---|---|---|---|
| AccountIdentity | Services/Auth | 사용자가 누구인지 | `users`, JWT |
| AuthSession | Services/Auth | 요청이 누구 권한인지 | JWT middleware |
| PurchaseReceipt | Services/Payment | 외부 결제 증빙과 idempotency | `payment_transactions` |
| WalletLedger | Services/Payment | 재화 증감 이유와 결과 | `wallets`, `coin_transactions` |
| Entitlement | Services/Shop | 접근 권리와 소유권 | 현재 별도 원자 필요 |
| InventoryGrant | Services/Shop | 보유 아이템 지급 결과 | `inventory` |
| ProfileProgression | Services/Profile | match 밖 성장/전적 조회 | `player_stats`, `match_history` |
| MatchResult | Server -> Services | Server authority가 끝낸 결과 영수증 | `ReplayRecorder`, `match-events` 흐름 확인 필요 |
| MatchmakingTicket | Services/Matchmaking | 유저를 match 후보로 세우는 입장권 | Redis queue 중심 |
| GameSessionIdentityBridge | Services + Server | backend `user_id`와 server session 연결 | 현재 `sessionId` 중심, 구현 필요 |
| GameServerCapacity | Infra + Matchmaking | 어느 server가 몇 명을 받을 수 있는지 | 구현 필요 |
| PatchContentVersion | Build/Data/Services/Server | client/server/data 규칙 버전 | 구현 필요 |
| OperationPolicy | Services/LiveOps | 점검, rollout, 구매 차단, queue 정책 | 구현 필요 |
| ObservabilityEvent | Infra | 부하와 장애를 판단하는 측정값 | 기존 load plan과 연결 필요 |

원자가 아닌 것:
- `ShopService`는 원자가 아니다. `PurchaseReceipt`, `WalletLedger`, `Entitlement`, `InventoryGrant`의 묶음이다.
- `DB`는 원자가 아니다. `Ledger`, `ReadModel`, `Outbox`, `Projection`으로 나뉜다.
- `부하 관리`는 원자가 아니다. `Admission`, `Queue`, `Backpressure`, `RateLimit`, `Capacity`, `Degradation`으로 나뉜다.
- `서버 인프라`는 원자가 아니다. match authority runtime과 backend service runtime과 observability runtime을 나눈다.

## 구현 순서

### B0. Atom Baseline

목표:
- 위 원자 이름을 backend/server 계획의 공통 언어로 고정한다.

반영:
- `.md/architecture/WINTERS_BACKEND_SERVER_POSTLAUNCH_ATOMS.md`를 추가한다.
- 이후 Go/C++/SQL 구현 계획은 이 원자 이름을 기준으로 세분화한다.

검증:
- `Services/README.md`의 `BackendState, LiveOpsContract`와 충돌하지 않는다.
- `Server/README.md`의 `AuthorityExecution`과 충돌하지 않는다.

### B1. WalletLedger Hardening

목표:
- 돈과 재화 증감은 ledger 원자 하나로 복구 가능하게 만든다.

반영 방향:
- `payment_transactions`, `wallets`, `coin_transactions`를 하나의 DB transaction boundary로 묶는다.
- `idempotency_key`와 gateway receipt는 중복 지급 방지의 기준이다.
- Kafka `payment-events`는 fanout이지 money source of truth가 아니다.

검증:
- 같은 `idempotency_key` 재시도는 한 번만 지급된다.
- wallet balance는 `coin_transactions.balance_after`와 재계산 가능하다.
- 실패, 환불, 완료 상태가 모두 audit trail로 남는다.

### B2. Entitlement / Inventory Split

목표:
- "살 권리", "접근 권리", "보유 아이템"을 섞지 않는다.

반영 방향:
- `shop_items`는 catalog다.
- `entitlements`는 account-level 권리다.
- `inventory`는 player-facing 보유 상태다.
- 구매 결과는 `EntitlementGrant` 또는 `InventoryGrant` 중 하나로 명확히 내려간다.

검증:
- Client가 보낸 inventory 목록을 신뢰하지 않는다.
- refund나 revoke가 inventory/read model에 재현 가능하게 반영된다.

### B3. GameSessionIdentityBridge

목표:
- `Server/GameRoom`의 `sessionId`와 Services의 `user_id`를 연결한다.

반영 방향:
- Matchmaking은 authenticated `user_id`로 `MatchmakingTicket`을 만든다.
- Game Server 입장 시 ticket/session proof를 검증한다.
- `GameRoom`은 match result에 backend `user_id`를 넣을 수 있어야 한다.

검증:
- guest/local smoke path와 authenticated production path가 이름과 flag로 분리된다.
- replay personalization, profile update, leaderboard update가 `sessionId` 추측에 기대지 않는다.

### B4. MatchResult Ingestion

목표:
- Server authority가 끝낸 결과만 Services의 progression/read model로 들어간다.

반영 방향:
- Server는 `MatchResult`를 생성한다.
- Services는 `match-events`를 받아 `match_history`, `player_stats`, leaderboard projection을 갱신한다.
- replay record와 match result는 같은 `match_id`, server build, content version을 공유한다.

검증:
- Services가 match 승패를 직접 계산하지 않는다.
- 중복 `MatchResult` ingest는 idempotent하다.
- Server log와 Services projection 성공을 별도 검증한다.

### B5. PatchContentVersion Gate

목표:
- 출시 이후 client/server/data 불일치가 사용자 상태를 오염시키지 않게 한다.

반영 방향:
- Client build, Server build, schema version, data manifest version을 분리해서 기록한다.
- Matchmaking은 허용된 client/content version만 queue에 넣는다.
- Game Server는 match 시작 시 version을 고정하고 match result에 기록한다.

검증:
- patch rollout 중 이전 match는 같은 version으로 끝난다.
- 보상/전적은 match가 실행된 version을 남긴다.
- migration은 rollback 또는 forward-only 복구 절차를 가진다.

### B6. Admission / Capacity / Queue

목표:
- 부하가 몰릴 때 깨지기 전에 받아들일 양을 결정한다.

반영 방향:
- Game Server는 active sessions, room count, tick budget, queue depth를 capacity로 보고한다.
- Matchmaking은 capacity reservation 없이는 match를 확정하지 않는다.
- Auth, Payment, Shop은 rate limit과 DB pool budget을 가진다.
- overload 시 queue, retry-after, maintenance response를 명확히 나눈다.

검증:
- capacity 부족 시 새 match는 대기하거나 거절되고 기존 match는 유지된다.
- Payment overload는 중복 지급으로 이어지지 않는다.
- queue depth와 reject count가 metric으로 남는다.

### B7. OperationPolicy / LiveOps

목표:
- 출시 후 운영자가 기능을 줄이고 열고 되돌리는 최소 정책을 가진다.

반영 방향:
- maintenance mode
- purchase disabled
- matchmaking disabled
- new patch canary percent
- store catalog active flag
- region capacity cap
- event reward rollout

검증:
- policy 변경은 gameplay truth를 바꾸지 않는다.
- store/catalog 변경은 entitlement/inventory ledger와 충돌하지 않는다.
- canary/rollback 결과가 telemetry와 audit에 남는다.

### B8. Observability / Load Proof

목표:
- 부하 관리를 감이 아니라 counter로 판단한다.

반영 방향:
- Server counter: tick ms, active sessions, packet recv/send, snapshot bytes, GameRoom count, command queue depth.
- Services counter: HTTP p95/p99, error rate, DB pool usage, Redis latency, Kafka lag, idempotency conflict count.
- Business counter: purchase completed/refunded/failed, wallet balance mismatch, inventory grant retry, match result ingest duplicate.

검증:
- `.md/plan/infra/LOAD_TEST_10K_PLAN.md`의 stage별 측정값과 연결된다.
- normal F5 roster/map/minion/champion/snapshot/UI/FX를 숨겨서 숫자를 만들지 않는다.

### B9. Privacy / Retention / Audit

목표:
- 출시 이후 사용자 데이터가 운영 가능한 수명과 감사 경로를 가진다.

반영 방향:
- PII는 gameplay profile/read model과 분리한다.
- purchase, wallet, entitlement, inventory grant는 audit retention을 가진다.
- user delete/export는 account/profile/social 데이터와 ledger 데이터를 다르게 처리한다.

검증:
- 결제/재화 감사 데이터는 삭제 요청과 회계/환불 요구 사이의 정책이 명확하다.
- telemetry는 사용자 상태 source of truth가 아니다.

## 구현 금지

- Server tick 안에서 Services DB를 직접 갱신하지 않는다.
- Services가 match 중 gameplay result를 계산하지 않는다.
- Kafka consumer 상태를 결제/재화 원장으로 취급하지 않는다.
- Redis cache를 user state source of truth로 취급하지 않는다.
- Client receipt, inventory, wallet 값을 신뢰하지 않는다.
- 부하 테스트 숫자를 위해 normal runtime 기능을 끄지 않는다.
- patch 통과를 위해 schema migration 실패를 무시하지 않는다.

## 세부 계획 작성 기준

각 B 단계는 실제 구현 전 아래 파일을 다시 inspect한다.

```text
Services/internal/<domain>/*.go
Services/migrations/*.sql
Services/pkg/config/config.go
Services/pkg/messaging/kafka.go
Server/Public/Game/*.h
Server/Private/Game/*.cpp
Server/Public/Network/*.h
Server/Private/Network/*.cpp
Shared/Schemas/*.fbs
```

각 세부 계획은 기존 코드/교체 코드/추가 코드/삭제 범위로 작성한다. 새 Go/C++/SQL 파일은 전체 본문을 포함한다.
````

2. 검증

미검증:
- `WINTERS_BACKEND_SERVER_POSTLAUNCH_ATOMS.md` 생성 미검증.
- Go/C++/SQL 구현 미진행.
- 빌드 미실행.

검증 명령:
- git diff --check
- Get-Content -Encoding UTF8 .md/architecture/WINTERS_BACKEND_SERVER_POSTLAUNCH_ATOMS.md -TotalCount 220
- go test ./...
- msbuild Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64

확인 필요:
- 각 B 단계 구현 전 실제 대상 Go/C++/SQL 파일을 다시 inspect한다.
- `GameRoom sessionId -> backend user_id` 연결 방식은 보안/운영 요구가 정해진 뒤 확정한다.
- `entitlements`를 `inventory`와 별도 테이블로 둘지, product별 grant table로 둘지는 B2 세부 계획에서 DB migration을 inspect한 뒤 결정한다.
- patch/content version gate는 Build/Data/Client/Server 배포 방식 확정 후 세부 schema를 결정한다.
