# 데이터베이스 — 기술면접 대비

> 대상: 게임 클라이언트/서버 프로그래머 신입 지원자 (자체 DX11 엔진 Winters + Go 백엔드 Services 제작 경험)
> 근거 코드: 본 문서의 모든 "내 프로젝트" 인용은 레포 실물 기준이다.
> - PostgreSQL 스키마: `Services/migrations/000001~000007_*.sql`
> - 트랜잭션/락: `Services/internal/shop/repository.go`, `Services/internal/payment/repository.go`, `Services/internal/auth/repository.go`
> - Redis 랭킹/캐시/세션/매칭큐: `Services/internal/leaderboard/repository.go`, `Services/internal/profile/repository.go`, `Services/internal/matchmaking/service.go`
> - 커넥션 풀: `Services/pkg/database/postgres.go`, `Services/pkg/cache/redis.go`
> - 클라이언트 비동기 I/O(DB 워커 스레드 패턴의 동형): `Client/Public/Network/Backend/CHttpClient.h`, `Client/Private/Network/Backend/CHttpClient.cpp`
> - 기술 스택 선언: `.md/plan/backend/00_BACKEND_PLAN_INDEX.md` (PostgreSQL 16 + pgx/v5, Redis 7 + go-redis/v9, Kafka, JWT+bcrypt)

## 출제 경향 개요

게임회사 신입 면접에서 DB는 "웹 백엔드 지식"이 아니라 **"라이브 게임을 굴릴 때 돈과 아이템이 안 깨지게 만들 수 있는가"**를 검증하는 과목이다. 실제 출제 패턴은 크게 4단계로 파고든다.

1. **SQL/RDBMS 기본기** — 조인, GROUP BY, 서브쿼리를 손으로 쓸 수 있는가. 여기서 막히면 뒤가 없다.
2. **인덱스 내부** — "인덱스 걸면 빨라져요"가 아니라 B+tree 페이지 구조, 클러스터드/논클러스터드, 인덱스가 무시되는 조건까지. "인벤토리 조회가 느린데 어떻게 할래?"로 위장해서 나온다.
3. **트랜잭션/동시성** — ACID 암기가 아니라 "아이템 구매 도중 서버가 죽으면?", "두 요청이 동시에 같은 지갑을 차감하면?" 같은 시나리오 문제. 격리수준·락·MVCC가 여기서 연쇄로 나온다.
4. **게임 특화 설계** — 인벤토리 스키마, 랭킹(Redis sorted set), 중복 로그인, 중복 결제(멱등성), 샤딩, 게임 루프를 막지 않는 비동기 DB 처리. 신입에게도 "설계해 보라"는 화이트보드 문제로 자주 나온다.

핵심 전략: 모든 답을 **정의 → 원리 → 트레이드오프 → 내 프로젝트에서 실제로 한 것** 순서로 말한다. 특히 나는 레포에 실물 스키마와 트랜잭션 코드가 있으므로, "해봤다"를 파일 단위로 말할 수 있는 게 최대 무기다.

---

## 핵심 개념 정리

### 1. RDBMS와 SQL 기초

#### 1-1. RDBMS 정의와 특성

- **정의**: 데이터를 행(row)과 열(column)로 구성된 테이블에 저장하고, 테이블 간 관계(relation)를 키로 표현하며, SQL로 조작하는 데이터베이스 관리 시스템. 대표: PostgreSQL, MySQL, SQL Server, Oracle.
- **원리**: 관계 대수(relational algebra)에 기반. 스키마가 먼저 정의되고(schema-on-write), 제약조건(PK/FK/UNIQUE/CHECK/NOT NULL)을 DBMS가 강제하며, 트랜잭션으로 ACID를 보장한다.
- **게임 맥락**: 유저 계정, 지갑, 인벤토리, 결제처럼 **틀리면 안 되는 데이터(source of truth)**는 RDBMS에 둔다. Winters도 유저/지갑/스탯/인벤토리/결제 전부 PostgreSQL(`Services/migrations/`)이고, 휘발성·고빈도 데이터(랭킹 조회, 매칭 큐, 세션)만 Redis에 둔다.

#### 1-2. 조인(JOIN) 종류

- **INNER JOIN**: 양쪽 테이블에서 조인 조건이 일치하는 행만 반환.
- **LEFT (OUTER) JOIN**: 왼쪽 테이블은 전부, 오른쪽은 일치하는 것만(없으면 NULL).
- **RIGHT (OUTER) JOIN**: LEFT의 반대. 실무에선 거의 LEFT로 통일해서 쓴다.
- **FULL OUTER JOIN**: 양쪽 모두 전부, 짝 없는 쪽은 NULL.
- **CROSS JOIN**: 카티션 곱(모든 조합). N×M 행.
- **SELF JOIN**: 같은 테이블을 별칭으로 두 번 참조 (예: 친구 관계, 조직도).

조인 실행 방식(물리 연산)도 알아두면 좋다: **Nested Loop**(작은 결과 × 인덱스 있는 상대에 유리), **Hash Join**(대량 등가 조인), **Merge Join**(양쪽 정렬돼 있을 때). 옵티마이저가 통계 기반으로 선택한다.

실제 예시 — Winters 프로필 조회(`Services/internal/profile/repository.go`):

```sql
SELECT u.id, u.username, ps.mmr, ps.wins, ps.losses, ps.kills, ps.deaths, ps.assists
FROM users u JOIN player_stats ps ON u.id = ps.user_id
WHERE u.id = $1
```

인벤토리 조회(`Services/internal/shop/repository.go`)도 조인이다:

```sql
SELECT i.item_id, s.name, s.item_type, i.quantity, i.acquired_at
FROM inventory i JOIN shop_items s ON i.item_id = s.id
WHERE i.user_id = $1
```

#### 1-3. GROUP BY / HAVING

- **정의**: GROUP BY는 지정 컬럼 값이 같은 행들을 그룹으로 묶고, 집계 함수(COUNT/SUM/AVG/MIN/MAX)를 그룹 단위로 계산한다. HAVING은 **집계 결과에 대한 필터**, WHERE는 **그룹핑 전 개별 행에 대한 필터**다.
- **논리적 실행 순서**: `FROM → WHERE → GROUP BY → HAVING → SELECT → ORDER BY → LIMIT`. WHERE에서 집계함수를 못 쓰는 이유가 이 순서 때문이다.
- **게임 예시**: 최근 전적에서 승수 집계.

```sql
-- 유저별 최근 100판 중 승리 수, 10승 이상만
SELECT user_id, COUNT(*) AS wins
FROM match_history
WHERE result = 'win'            -- 행 필터: 그룹핑 전
GROUP BY user_id
HAVING COUNT(*) >= 10;          -- 그룹 필터: 집계 후
```

Winters는 이 집계를 매번 하지 않으려고 `player_stats`에 wins/losses/kills를 **미리 누적**해 둔다(의도적 비정규화, §5-3).

#### 1-4. 서브쿼리

- **스칼라 서브쿼리**: 단일 값 반환. `SELECT (SELECT MAX(mmr) FROM player_stats)` — SELECT절/WHERE절에 사용.
- **인라인 뷰 (파생 테이블)**: FROM절의 서브쿼리. 집계 후 조인할 때 유용.
- **상관 서브쿼리(correlated)**: 외부 쿼리의 행을 참조. 외부 행마다 재실행되므로 대량 데이터에서 성능 함정. `EXISTS`가 대표.
- **IN vs EXISTS vs JOIN**: `IN`은 서브쿼리 결과 집합과 비교(NULL 함정 있음 — `NOT IN`에 NULL이 섞이면 전체가 UNKNOWN), `EXISTS`는 존재 여부만 확인(한 건 찾으면 중단, semi-join), JOIN은 결과 컬럼이 필요할 때. 현대 옵티마이저는 상당 부분 같은 실행계획(semi-join)으로 변환하지만, **의미가 다른 경우**(중복 행 발생 여부)는 여전히 구분해야 한다.

```sql
-- 아이템을 하나라도 산 유저 목록: EXISTS (semi-join, 중복 없음)
SELECT u.username FROM users u
WHERE EXISTS (SELECT 1 FROM inventory i WHERE i.user_id = u.id);
```

#### 1-5. 제약조건 — 스키마가 지키는 마지막 방어선

Winters 스키마의 실물 예시가 그대로 면접 답변이 된다:

- `wallets.balance BIGINT NOT NULL DEFAULT 0 CHECK (balance >= 0)` (`Services/migrations/000002_create_wallets.up.sql`) — 애플리케이션 버그로 음수 차감이 들어와도 DB가 거부. 재화는 CHECK로 이중 방어.
- `inventory UNIQUE(user_id, item_id)` (`000007_create_inventory.up.sql`) — 같은 유저·아이템 행 중복을 구조적으로 차단하고, `ON CONFLICT ... DO UPDATE`(upsert)의 기반이 된다.
- `payment_transactions.idempotency_key VARCHAR(64) NOT NULL UNIQUE` (`000005`) — 중복 결제를 **UNIQUE 제약으로** 차단(멱등성, §7-4).
- `wallets.user_id UUID NOT NULL UNIQUE REFERENCES users(id) ON DELETE CASCADE` — FK로 참조 무결성 + 계정 삭제 시 연쇄 삭제. 1:1 관계는 FK에 UNIQUE를 얹어 표현.
- `coin_transactions.tx_type CHECK (tx_type IN ('charge','purchase','refund'))` (`000006`) — enum성 값의 오타/오염 차단.

#### 1-6. DELETE vs TRUNCATE vs DROP

- **DELETE**: 행 단위 삭제, WHERE 가능, 트랜잭션 로그에 행마다 기록, 롤백 가능, 트리거 발동. 느림.
- **TRUNCATE**: 테이블 전체 비우기(DDL성), 페이지 단위 해제라 빠름, WHERE 불가. (PostgreSQL에선 트랜잭션 내 롤백 가능, MySQL은 암묵 커밋.)
- **DROP**: 테이블 구조 자체 제거.

---

### 2. 인덱스 내부 구조

#### 2-1. B+tree — 인덱스가 빠른 진짜 이유

- **정의**: 인덱스는 (키 값 → 행 위치) 매핑을 정렬 상태로 유지하는 자료구조이고, 디스크 기반 DB의 표준 구현이 B+tree다.
- **구조**:
  - 모든 노드는 디스크 페이지 단위(MySQL InnoDB 16KB, PostgreSQL 8KB).
  - **내부 노드**는 라우팅용 키만, **리프 노드**에만 실제 (키, 행 포인터)가 있다. 리프끼리 연결 리스트로 이어져 있어 **범위 스캔이 순차 I/O**가 된다.
  - 항상 균형(balanced): 루트→리프 깊이가 모든 경로에서 같다.
- **수치 감각**: 페이지당 팬아웃(자식 수)이 수백이다. 팬아웃 500이면 깊이 3에 500³ = 1.25억 행 커버. 즉 **1억 행에서도 디스크 읽기 3~4회**로 한 행을 찾는다. 인덱스 없으면 풀 스캔 = 수백만 페이지 I/O. O(N) → O(log_fanout N)의 차이인데, 밑이 2가 아니라 수백이라는 게 핵심이다.
- **왜 이진트리/해시가 아닌가**:
  - 이진트리: 노드당 자식 2개 → 깊이가 log₂N ≈ 27 (1억 행). 디스크 I/O 27회는 재앙. B+tree는 **한 페이지 I/O로 수백 갈래**를 라우팅한다.
  - 해시 인덱스: 등가(=) 조회는 O(1)이지만 **범위 검색, 정렬, 부분 일치(prefix)가 불가**. `ORDER BY mmr DESC LIMIT 100` 같은 랭킹 쿼리에 못 쓴다.
- **쓰기 비용**: INSERT/UPDATE/DELETE마다 관련 인덱스 전부 갱신. 페이지가 가득 차면 분할(page split)이 일어나 쓰기 증폭. 인덱스를 남발하면 안 되는 이유.
- **게임 맥락**: Winters의 `CREATE INDEX idx_player_stats_mmr ON player_stats(mmr DESC)` (`000003`) — MMR 내림차순 랭킹 조회를 리프 연결 리스트 순차 스캔으로 처리하려는 인덱스. `idx_match_history_user ON match_history(user_id, played_at DESC)` (`000004`) — "이 유저의 최근 전적 N건"을 복합 인덱스 하나로 커버.

#### 2-2. 클러스터드 vs 논클러스터드 인덱스

- **클러스터드 인덱스**: 인덱스 리프 노드가 **곧 실제 데이터 행**이다. 테이블 자체가 그 키 순서로 물리 저장된다. 테이블당 1개만 가능. MySQL InnoDB에서 PK가 자동으로 클러스터드.
- **논클러스터드(세컨더리) 인덱스**: 리프에 행 자체가 아니라 **행을 찾는 포인터**가 있다. InnoDB에선 그 포인터가 PK 값이라 세컨더리 인덱스 조회 = 세컨더리 B+tree 탐색 + PK B+tree 재탐색(**2번 타기**). SQL Server 용어로는 RID/키 룩업, 통칭 북마크 룩업.
- **PostgreSQL의 특수성**: Postgres엔 클러스터드 인덱스가 없다. 모든 테이블은 힙(heap)이고 모든 인덱스는 논클러스터드로 (키 → TID(페이지,오프셋))를 가리킨다. `CLUSTER` 명령은 일회성 재정렬일 뿐 유지되지 않는다. Winters가 PostgreSQL 기반이므로 이 차이를 말할 수 있으면 강한 인상을 준다.
- **실무 시사점**:
  - InnoDB에서 PK는 **짧고 단조 증가**가 유리(UUID v4 같은 랜덤 PK는 페이지 분할 + 세컨더리 인덱스 비대화). Winters는 `UUID PRIMARY KEY DEFAULT uuid_generate_v4()`를 쓰는데, Postgres는 힙 구조라 InnoDB만큼 치명적이진 않지만, InnoDB였다면 ULID/UUIDv7 같은 시간순 ID를 검토했을 것 — 이렇게 답하면 트레이드오프를 아는 것으로 보인다.

#### 2-3. 커버링 인덱스 (Index-Only Scan)

- **정의**: 쿼리가 요구하는 **모든 컬럼이 인덱스 안에 이미 있어서** 테이블(힙) 접근 없이 인덱스만 읽고 끝나는 경우. 북마크 룩업이 제거되어 수배~수십 배 빨라진다.
- **예시**: `idx_match_history_user(user_id, played_at DESC)`가 있을 때

```sql
SELECT played_at FROM match_history WHERE user_id = $1;   -- 인덱스만으로 응답 가능
SELECT result   FROM match_history WHERE user_id = $1;    -- result는 인덱스에 없음 → 힙 접근 필요
```

- PostgreSQL은 `INCLUDE (result)`로 검색키가 아닌 컬럼을 리프에만 실어 커버링을 만들 수 있다. 단, Postgres의 index-only scan은 **visibility map**이 최신일 때만 진짜 인덱스 단독으로 끝난다(MVCC 때문에 행 가시성 확인이 필요해서, VACUUM이 밀리면 힙을 찍어본다) — 이 꼬리까지 알면 심화 가점.

#### 2-4. 인덱스가 안 타는 경우 (면접 단골)

1. **인덱스 컬럼 가공**: `WHERE LOWER(email) = '...'`, `WHERE created_at + INTERVAL '1 day' > NOW()` — 컬럼에 함수/연산을 씌우면 B+tree 키와 비교 불가. → 상수 쪽을 가공하거나 함수 기반 인덱스(expression index) 생성.
2. **선행 와일드카드 LIKE**: `LIKE '%sword'`는 불가, `LIKE 'sword%'`는 가능(정렬 구조상 prefix만 탐색 가능).
3. **암묵적 형변환**: `VARCHAR` 컬럼을 숫자와 비교(`WHERE phone = 01012345678`)하면 컬럼 전체가 캐스팅되어 풀 스캔.
4. **복합 인덱스의 선두 컬럼 누락(leftmost prefix 위반)**: `(user_id, played_at)` 인덱스에서 `WHERE played_at > ...`만 있으면 못 탄다.
5. **낮은 선택도(selectivity)**: `WHERE is_active = true`처럼 행의 90%가 매칭되면 옵티마이저가 풀 스캔을 선택하는 게 오히려 정답(랜덤 I/O로 90%를 찍는 것보다 순차 풀 스캔이 싸다). "인덱스를 안 타는 게 버그가 아니라 최적일 수 있다"고 말하면 이해도가 드러난다.
6. **OR 조건**: 서로 다른 컬럼의 OR은 단일 인덱스로 불가(각각 인덱스가 있으면 bitmap OR/index merge 가능).
7. **부정 조건**: `!=`, `NOT IN`은 대부분 범위 탐색으로 표현이 어려워 풀 스캔 경향.
8. **통계 정보 부정확**: ANALYZE가 오래됐으면 옵티마이저가 잘못 판단. → 실행계획(`EXPLAIN ANALYZE`) 확인이 항상 첫 단계.

#### 2-5. 복합 인덱스 설계 규칙

- **동등 조건 컬럼을 앞에, 범위/정렬 컬럼을 뒤에**: `WHERE user_id = ? ORDER BY played_at DESC` → `(user_id, played_at DESC)`. Winters의 `idx_match_history_user(user_id, played_at DESC)`, `idx_coin_tx_user(user_id, created_at DESC)`가 정확히 이 패턴이다.
- 카디널리티(고유값 수)가 높은 컬럼이 대체로 앞. 단, 쿼리 패턴이 우선이다.
- **중복 인덱스 경계**: `(a, b)` 인덱스가 있으면 `(a)` 단독 인덱스는 대부분 불필요. 또 하나 — PostgreSQL에서 **UNIQUE 제약은 자동으로 유니크 인덱스를 만든다**. Winters `users` 테이블은 `username`/`email`에 UNIQUE를 걸고도 `CREATE INDEX idx_users_username`을 또 만들었는데(`000001`), 이건 **중복 인덱스로 쓰기 비용만 늘리는 실수**다. 면접에서 "내 스키마에서 발견한 개선점"으로 먼저 꺼내면 자기 코드 감사 능력을 어필할 수 있다.

---

### 3. 트랜잭션과 ACID

#### 3-1. 트랜잭션 정의

- **정의**: 논리적으로 하나여야 하는 작업 묶음. 전부 성공(COMMIT)하거나 전부 취소(ROLLBACK)된다.
- **게임에서의 전형**: "지갑 차감 + 인벤토리 지급 + 장부 기록"은 셋 중 하나만 실행되면 재화 복사/증발 사고다. 반드시 한 트랜잭션.

#### 3-2. ACID — 각 글자가 "무엇으로" 보장되는지까지

- **Atomicity(원자성)**: all-or-nothing. **Undo 로그**(MySQL) / **MVCC 버전 + 트랜잭션 상태**(PostgreSQL)로, 실패 시 변경 전 상태로 복원. 코드 레벨에선 `defer tx.Rollback(ctx)` 후 마지막에 `tx.Commit(ctx)` — 중간 어느 return에서도 자동 롤백되는 패턴을 Winters `shop/repository.go:Purchase`가 그대로 쓴다.
- **Consistency(일관성)**: 트랜잭션 전후로 무결성 제약(CHECK, FK, UNIQUE)과 비즈니스 불변식이 유지. `CHECK (balance >= 0)`이 DB 레벨 일관성 장치. 나머지 절반은 애플리케이션 책임(예: "잔액 >= 가격일 때만 차감"은 코드가 검사).
- **Isolation(격리성)**: 동시 실행 트랜잭션이 서로의 중간 상태를 보지 못하게. 락 또는 MVCC로 구현(§4).
- **Durability(지속성)**: COMMIT 응답을 보낸 순간 이후 서버가 전원이 나가도 데이터가 남는다. **WAL(Write-Ahead Log)**: 데이터 페이지를 고치기 전에 로그를 먼저 순차 기록하고 fsync. 크래시 후엔 WAL 재생(redo)으로 복구. "커밋 = 디스크에 로그가 fsync된 시점"이라고 말할 수 있어야 한다.

#### 3-3. 실물 예시 — 회원가입 3-테이블 원자 생성

`Services/internal/auth/repository.go:CreateUserWithWalletAndStats` — users INSERT + wallets INSERT + player_stats INSERT를 한 트랜잭션으로 묶는다. 유저는 생겼는데 지갑이 없는 "반쪽 계정"을 구조적으로 차단. 신입 면접에서 "트랜잭션 써본 적 있냐"에 파일명까지 답할 수 있는 지점.

---

### 4. 격리수준, 이상현상, 락 vs MVCC

#### 4-1. 이상현상 3종

- **Dirty Read**: 다른 트랜잭션이 **아직 커밋 안 한** 값을 읽음. 그 트랜잭션이 롤백하면 존재한 적 없는 데이터를 읽은 것.
- **Non-Repeatable Read**: 한 트랜잭션 안에서 같은 행을 두 번 읽었는데 값이 다름(사이에 다른 트랜잭션이 UPDATE 커밋).
- **Phantom Read**: 같은 **조건 검색**을 두 번 했는데 행 **개수**가 다름(사이에 INSERT/DELETE 커밋). Non-repeatable은 "값이 변함", Phantom은 "집합이 변함"으로 구분.

#### 4-2. 격리수준 4단계 (SQL 표준)

| 격리수준 | Dirty | Non-Repeatable | Phantom | 비고 |
|---|---|---|---|---|
| READ UNCOMMITTED | 발생 | 발생 | 발생 | Postgres에선 RC로 동작 |
| READ COMMITTED | 방지 | 발생 | 발생 | Postgres/Oracle 기본값 |
| REPEATABLE READ | 방지 | 방지 | 표준상 발생 | MySQL InnoDB 기본값 |
| SERIALIZABLE | 방지 | 방지 | 방지 | 직렬 실행과 동등 |

구현별 각주(심화 가점 포인트):
- **PostgreSQL RR**은 스냅샷 격리(SI)라 팬텀도 실질적으로 안 보인다. 대신 **write skew**(두 트랜잭션이 서로 다른 행을 읽고 각자 다른 행을 갱신해 불변식이 깨짐)는 SERIALIZABLE(SSI)에서만 막힌다.
- **MySQL InnoDB RR**은 MVCC 스냅샷 + **넥스트키 락**(레코드 락 + 갭 락)으로 팬텀을 대부분 방지한다.
- 격리수준을 올릴수록 정합성↑ 동시성↓. 기본값(RC 또는 RR)으로 두고 **위험한 지점만 명시적 락으로 조인다**가 실무 정답이고, Winters `Purchase`가 그 방식이다.

#### 4-3. 락 (비관적 동시성 제어)

- **공유 락(S)**: 읽기끼리 호환. **배타 락(X)**: 누구와도 비호환. 행/페이지/테이블 granularity + 인텐션 락.
- **`SELECT ... FOR UPDATE`**: 읽는 시점에 행에 X락. "읽고 → 판단하고 → 쓴다" 사이에 끼어들기를 차단. Winters 지갑 차감의 핵심:

```sql
SELECT balance FROM wallets WHERE user_id = $1 FOR UPDATE  -- Services/internal/shop/repository.go
```

이 락이 없으면: 잔액 100, 가격 100짜리 동시 구매 2건이 둘 다 `balance(100) >= price(100)` 검사를 통과 → 둘 다 차감 → 잔액 -100(CHECK가 최후에 막아주지만 한 건은 실패해야 정상인데 어느 쪽이 실패할지 통제 불능). FOR UPDATE로 두 번째 트랜잭션은 첫 커밋까지 대기 → 갱신된 잔액 0을 읽고 잔액 부족으로 정상 거절.

- **데드락**: 두 트랜잭션이 서로가 잡은 락을 기다리는 순환. 조건 — 상호배제/점유대기/비선점/순환대기. DBMS는 대기 그래프로 감지해 한쪽을 강제 롤백(victim). **예방책은 락 획득 순서 통일**: 예컨대 선물하기(내 지갑 차감 + 상대 인벤토리 지급)를 양방향 동시에 하면 교차 대기 가능 → "항상 user_id가 작은 쪽 지갑부터 잠근다" 같은 전역 순서 규칙. 애플리케이션에선 데드락 에러(Postgres 40P01)를 잡아 재시도.

#### 4-4. MVCC (다중 버전 동시성 제어)

- **정의**: 데이터를 덮어쓰지 않고 **버전을 남겨서**, 읽기 트랜잭션은 자기 시작 시점(스냅샷)에 유효한 버전을 읽는다. **읽기가 쓰기를 안 막고, 쓰기가 읽기를 안 막는다** — 읽기 위주 워크로드에서 동시성이 크게 오른다.
- **구현**:
  - PostgreSQL: 각 행에 `xmin`(생성 트랜잭션 ID)/`xmax`(삭제 트랜잭션 ID)를 박고, UPDATE = 새 버전 INSERT + 구버전 xmax 마킹. 죽은 버전은 **VACUUM**이 회수(안 하면 테이블 비대 = bloat).
  - MySQL InnoDB: 행은 최신본만 두고, 과거 버전은 **undo 로그**에서 체인으로 복원.
- **락 vs MVCC 요약**: 락은 "기다리게 해서" 정합성 확보(쓰기-쓰기 충돌엔 여전히 필수), MVCC는 "버전을 나눠서" 읽기-쓰기 충돌 제거. 현대 DB는 **MVCC로 읽기 동시성 + 쓰기 충돌 지점만 락** 하이브리드. Winters도 조회 쿼리는 락 없이(MVCC 스냅샷), 지갑 차감만 FOR UPDATE.

#### 4-5. 낙관적 락 vs 비관적 락

- **비관적(pessimistic)**: 충돌이 잦다고 가정, 먼저 잠근다(FOR UPDATE). 대기 비용, 데드락 위험. 재화처럼 **충돌 시 절대 틀리면 안 되는 곳**에.
- **낙관적(optimistic)**: 충돌이 드물다고 가정, 안 잠그고 커밋 시점에 버전 비교로 감지 → 실패 시 재시도.

```sql
UPDATE characters SET hp = $1, version = version + 1
WHERE id = $2 AND version = $3;   -- affected rows = 0이면 충돌 → 재시도
```

  락 대기가 없어 처리량이 좋지만 재시도 로직이 애플리케이션 책임. 충돌률이 높으면 재시도 폭풍으로 오히려 손해.
- **선택 기준**: 지갑/인벤토리 = 비관적(또는 원자적 UPDATE 한 방), 프로필 편집·설정 저장 = 낙관적, 조회 = MVCC 스냅샷.

#### 4-6. 락 없이 원자적 UPDATE로 푸는 패턴

`UPDATE wallets SET balance = balance - $2 WHERE user_id = $1 AND balance >= $2 RETURNING balance` — 검사와 차감을 한 문장으로 합치면 UPDATE 자체가 행 락을 잡으므로 FOR UPDATE 없이도 안전하다(affected rows=0 → 잔액 부족). Winters `Purchase`는 "잔액 조회 후 에러 코드 분기"가 필요해서 FOR UPDATE 방식을 썼지만, 이 대안을 아는지 꼬리질문이 자주 온다.

---

### 5. 정규화와 게임 DB 설계

#### 5-1. 정규화 1~3NF

- **1NF**: 모든 컬럼 값이 원자적(atomic). 한 컬럼에 `"sword,shield,potion"` 같은 목록 금지 → 행으로 분리.
- **2NF**: 1NF + **부분 함수 종속 제거**. 복합키 `(user_id, item_id)`에서 `item_name`이 `item_id`에만 종속되면 위반 → 아이템 테이블로 분리. Winters가 정확히 이 구조: `inventory(user_id, item_id, quantity)` + `shop_items(id, name, price, ...)` (`000007`). 아이템 이름이 바뀌어도 인벤토리 수백만 행을 안 고친다.
- **3NF**: 2NF + **이행적 종속 제거**(비키 컬럼 → 비키 컬럼 종속). `users`에 `guild_id, guild_name`을 두면 guild_name이 guild_id에 종속 → guilds 테이블로 분리.
- **정규화의 목적**: 갱신 이상(update/insert/delete anomaly) 제거, 중복 제거. **대가**: 조회 시 조인 증가.

#### 5-2. BCNF 이상은?

BCNF(모든 결정자가 후보키), 4NF/5NF까지 있지만 실무는 "3NF까지 하고, 성능 근거가 있을 때만 되돌린다(denormalize)"가 통용 답변.

#### 5-3. 게임 DB의 의도적 비정규화 — Winters 실물 사례 3종

1. **집계 컬럼 사전 계산**: `player_stats(wins, losses, kills, deaths, assists)` (`000003`) — 정규화 원칙대로면 `match_history`를 COUNT/SUM 하면 되지만, 프로필 조회마다 수천 행 집계는 낭비. 매치 종료 시 Kafka 컨슈머(`Services/internal/profile/consumer.go`)가 match_history INSERT + player_stats 누적 UPDATE를 함께 수행. **읽기 빈도 >> 쓰기 빈도**일 때의 교과서적 비정규화.
2. **감사용 스냅샷 컬럼**: `coin_transactions.balance_after` (`000006`) — "그 시점 잔액"은 이전 거래를 전부 재집계하면 계산 가능하지만, CS 대응/부정거래 조사 시 즉시 조회를 위해 거래마다 박제. 원장(ledger) 패턴.
3. **저장소 계층 중복**: 랭킹 데이터가 PostgreSQL `player_stats.mmr`(원본)과 Redis `leaderboard:mmr` sorted set(조회용 사본)에 이중 존재(`Services/internal/leaderboard/repository.go`). 비정규화의 분산 시스템 버전 — 대신 두 저장소 간 정합성 관리 비용이 생기고, Winters는 `SyncFromDB`(DB → Redis 전체 재적재)를 복구 수단으로 둔다.

또한 `shop_items.metadata JSONB` — 아이템 타입별 가변 속성(공격력, 스킨 색상 등)을 고정 컬럼으로 정규화하지 않고 JSONB로 수용. 스키마 유연성 vs 타입 안전성/인덱싱의 트레이드오프이며, Postgres는 JSONB에 GIN 인덱스도 걸 수 있다.

#### 5-4. 게임 표준 스키마 골격 (화이트보드 대비)

```text
users            (id PK, username UQ, email UQ, password[bcrypt hash], created_at, ...)
wallets          (id PK, user_id FK UQ, balance CHECK>=0)              -- 1:1
player_stats     (id PK, user_id FK UQ, mmr, wins, losses, ...)        -- 1:1, 비정규화 집계
shop_items       (id PK, name, item_type, price CHECK>0, metadata JSONB)
inventory        (id PK, user_id FK, item_id FK, quantity, UQ(user_id,item_id))  -- N:M 해소
match_history    (id PK, user_id FK, match_id, result, k/d/a, mmr_change, played_at)
coin_transactions(id PK, user_id FK, amount, tx_type, balance_after)   -- append-only 원장
payment_transactions(id PK, user_id FK, idempotency_key UQ, status, ...)
```

전부 `Services/migrations/000001~000007`에 실물로 존재. 포인트: (a) 재화는 wallet 분리 + CHECK, (b) 인벤토리는 유저×아이템 N:M을 UNIQUE 복합키로, (c) 돈이 움직인 기록은 UPDATE가 아니라 **append-only 원장**, (d) 외부 결제는 멱등성 키.

---

### 6. NoSQL 분류와 Redis

#### 6-1. NoSQL 4분류

- **Key-Value**: Redis, DynamoDB — 세션, 캐시, 랭킹.
- **Document**: MongoDB — 스키마 유연, 게임 로그/유저 생성 콘텐츠.
- **Wide-Column**: Cassandra, HBase — 시계열/대량 쓰기(채팅 로그, 텔레메트리).
- **Graph**: Neo4j — 친구/소셜 그래프.

공통 배경: RDB의 수직 확장 한계 → 수평 확장을 위해 조인/트랜잭션/스키마를 완화. **CAP**: 네트워크 분단(P) 시 일관성(C)과 가용성(A) 중 택일. RDB는 CP 성향, Dynamo 계열은 AP 성향. "게임 재화는 C, 랭킹 조회는 A 우선" 식으로 데이터별로 다르게 답하는 게 정답.

#### 6-2. Redis가 빠른 이유

1. **인메모리** — 디스크 I/O가 경로에 없다 (영속화는 RDB 스냅샷/AOF로 별도).
2. **단일 스레드 이벤트 루프** (명령 실행 기준) — 락/컨텍스트 스위칭 없음, 모든 명령이 원자적. (6.0+ I/O 스레드는 네트워크 파싱만 병렬.)
3. **최적화된 자료구조** — 명령이 자료구조 연산에 1:1 매핑.

단일 스레드의 함정: `KEYS *`, 거대 컬렉션의 `ZRANGE 0 -1` 같은 O(N) 명령 하나가 전체를 블로킹. Winters `matchmaking/service.go:tryMatch`가 `ZRangeWithScores(0, -1)`로 큐 전체를 읽는데, 큐가 수십만이면 위험한 패턴 — 현재는 2인 매칭 프로토타입이라 허용, 스케일 시 배치/샤딩 필요하다고 스스로 짚을 수 있어야 한다.

#### 6-3. 자료구조와 게임 용도

| 구조 | 명령 예 | 게임 용도 | Winters 실물 |
|---|---|---|---|
| String | GET/SET/INCR, TTL | 세션, 캐시, 카운터 | `refresh:{jti}` 리프레시 토큰(TTL), `profile:{id}` 캐시, `matchmaking:status:{id}` |
| Hash | HSET/HGETALL | 객체 필드 단위 캐시 | — |
| List | LPUSH/BRPOP | 작업 큐, 최근 알림 | — |
| Set | SADD/SISMEMBER | 중복 방지, 접속자 집합 | — |
| **Sorted Set** | ZADD/ZREVRANK/ZRANGEBYSCORE | **랭킹, MMR 매칭 큐** | `leaderboard:mmr`, `matchmaking:queue` |
| Streams | XADD/XREADGROUP | 이벤트 로그 | (이벤트는 Kafka 사용) |

#### 6-4. Sorted Set 내부와 랭킹

- **내부 구현**: **skip list + hash table** 이중 구조. hash로 멤버→점수 O(1), skip list로 점수 순 정렬 유지. ZADD/ZREM/ZRANK 모두 **O(log N)**, 상위 M 조회 O(log N + M).
- **RDB 대비 왜 압도적인가**: SQL로 "내 순위"는 `SELECT COUNT(*) FROM player_stats WHERE mmr > (내 mmr)` — 매 요청 인덱스 범위 스캔 O(내 순위만큼). 100만 유저가 각자 순위를 폴링하면 DB가 죽는다. Redis는 ZREVRANK 한 방 O(log N) ≈ 20 스텝.
- **Winters 실물** (`Services/internal/leaderboard/repository.go`):
  - 갱신: `ZADD leaderboard:mmr {mmr} {userId}` + PostgreSQL `player_stats` UPDATE (이중 쓰기).
  - Top N: `ZREVRANGE 0 N-1 WITHSCORES`.
  - 내 순위: `ZREVRANK` + `ZSCORE` + `ZCARD`(전체 인원).
  - 재해 복구: `SyncFromDB` — DB 전체를 파이프라인 ZADD로 재적재. **"Redis는 사본, DB가 원본"** 원칙의 구현.
  - 동점 처리 꼬리질문: 같은 score면 member 사전순 — 공정한 tie-break가 필요하면 `score = mmr * 2^20 + (2^20 - 도달시간)` 같은 복합 점수 인코딩.

#### 6-5. 캐시 전략

- **Cache-Aside (Lazy Loading)**: 읽기 — 캐시 조회 → 미스 시 DB 조회 → 캐시에 적재(TTL). 쓰기 — DB 갱신 후 캐시 **삭제**(invalidate). 가장 보편적. **Winters 실물**: `profile/repository.go:GetProfile`이 `profile:{userId}` 키로 5분 TTL cache-aside, 매치 종료 Kafka 컨슈머가 `InvalidateCache` 호출(`profile/consumer.go`).
- **Write-Through**: 쓰기 시 캐시와 DB 동시 갱신. 캐시가 항상 최신이지만 쓰기 지연 증가.
- **Write-Behind (Write-Back)**: 캐시에 먼저 쓰고 DB엔 배치로 비동기 반영. 쓰기 처리량 최고지만 캐시 유실 = 데이터 유실 → 게임 재화엔 금지, 위치/스탯처럼 유실 허용 데이터에만.
- **함정 대비**:
  - **캐시 스탬피드**: 인기 키 만료 순간 수천 요청이 동시에 DB로 → 분산 락(한 놈만 재계산), TTL 지터, 확률적 조기 갱신.
  - **정합성 창**: cache-aside도 "DB 갱신 → 삭제 사이" 또는 "삭제 직후 구값 재적재" 레이스로 잠깐 구버전이 보일 수 있다. TTL이 최종 방어선. Winters 프로필은 5분 스테일 허용이라는 **제품 결정**을 명시한 것.
  - 갱신 시 "캐시 업데이트"가 아니라 "캐시 삭제"인 이유: 동시 쓰기 순서 역전으로 구값이 남는 사고 방지.

#### 6-6. 세션/토큰 저장소로서의 Redis

- 게임 서버가 다중 인스턴스면 세션을 서버 메모리에 못 둔다(로드밸런서가 어디로 보낼지 모름) → 공유 저장소 필요. Redis가 표준인 이유: TTL 자동 만료, 초당 수십만 조회, 원자적 연산.
- **Winters 실물** (`Services/internal/auth/repository.go`): JWT access token은 무상태 검증, **refresh token은 `refresh:{jti}` 키로 Redis에 TTL 저장** — 로그아웃 시 `DEL`로 즉시 무효화 가능(순수 JWT의 "발급 후 회수 불가" 약점 보완). 매칭 상태도 `matchmaking:status:{userId}`에 TTL 600초로 저장해 유령 큐 잔류를 자동 청소.

---

### 7. 게임 서버 DB 설계 패턴

#### 7-1. 아이템 구매/지급 트랜잭션 — 전체 시퀀스

`Services/internal/shop/repository.go:Purchase` 실물 (면접에서 그대로 설명 가능한 골격):

```text
BEGIN;
1. SELECT price FROM shop_items WHERE id=$1 AND is_active=true;   -- 가격은 서버가 조회(클라 신뢰 금지)
2. SELECT balance FROM wallets WHERE user_id=$1 FOR UPDATE;       -- 지갑 행 배타 락
3. if balance < price → ROLLBACK (에러 반환)
4. UPDATE wallets SET balance = balance - price RETURNING balance;
5. INSERT INTO inventory ... ON CONFLICT (user_id, item_id)
     DO UPDATE SET quantity = inventory.quantity + 1;              -- upsert로 소유/미소유 분기 제거
6. INSERT INTO coin_transactions (user_id, -price, 'purchase', item_id, balance_after);  -- 원장
COMMIT;
```

설계 포인트 열거: (a) 가격을 클라이언트가 보내지 않는다 — 서버 권위, (b) FOR UPDATE로 동시 구매 직렬화, (c) upsert로 UNIQUE 충돌을 로직으로 흡수, (d) 원장 기록까지 같은 트랜잭션 — 돈이 움직였는데 기록이 없는 상태 불가능, (e) Go의 `defer tx.Rollback()` — 어느 지점에서 실패해도 원자성 유지.

#### 7-2. 우편/보상 지급 (구매의 변형)

지급(무료 보상)은 차감이 없으니 upsert + 원장만. 대량 지급(전체 유저 이벤트)은 행별 트랜잭션 대신 배치 INSERT + "수령 시 지급" 방식(우편함 테이블에 넣고 유저가 열 때 인벤토리 반영)으로 쓰기 폭주를 시간축으로 분산.

#### 7-3. 거래(trade) — 두 유저 간 아이템 교환

두 지갑/인벤토리를 한 트랜잭션에서 잠글 때 **락 순서 통일**(user_id 오름차순으로 FOR UPDATE)로 데드락 예방. 교환 자체를 원장 2행(차감/지급)으로 남긴다.

#### 7-4. 중복 결제 방지 — 멱등성(Idempotency)

- **문제**: 클라이언트 타임아웃 → 재시도 → 같은 결제가 두 번 처리. 네트워크가 있는 한 "정확히 한 번 전달"은 불가능하므로 **"여러 번 와도 한 번만 처리"**로 설계한다.
- **Winters 실물**: `payment_transactions.idempotency_key UNIQUE` (`000005`) + `payment/repository.go:FindByIdempotencyKey` — 요청마다 클라이언트가 발급한 키를 싣고, (1) 먼저 키로 기존 거래 조회 → 있으면 **그때의 응답을 재반환**(에러가 아니라!), (2) 없으면 처리하되 INSERT의 UNIQUE 제약이 레이스 상황의 최종 방어선. 조회-후-삽입 사이 동시 요청 2건이 들어와도 한쪽 INSERT는 UNIQUE 위반으로 실패한다.
- 아이템 지급도 동일 원리: 지급 사유 ID(mail_id, event_id)를 UNIQUE로 걸면 중복 지급 구조적 차단.

#### 7-5. 같은 계정 중복 로그인

- **계층별 처리**:
  1. **인증 계층**: 로그인 시 새 세션 ID(또는 refresh jti)를 발급하고 Redis에 `session:{userId} → 세션ID`로 **덮어쓴다**. 이전 세션 ID는 검증 실패 → 기존 접속 강제 킥("나중 로그인 우선"이 게임 표준. LoL이 이 방식). Winters는 `refresh:{jti}` 저장(`auth/repository.go`)까지 구현돼 있고, userId당 단일 세션 강제는 `session:{userId}` 역방향 키 하나를 추가하면 되는 구조.
  2. **게임 서버 계층**: 접속 시 이미 같은 userId의 연결이 있으면 기존 연결에 킥 패킷 후 종료. 분산 환경이면 Redis `SET session:{userId} {serverId} NX` 같은 원자 연산으로 "어느 서버에 접속 중인지"를 선점 — SETNX 실패 = 이미 접속 중.
  3. **DB 계층**: 캐릭터 상태 저장이 두 세션에서 동시에 오는 최악 케이스도 낙관적 락(version)이나 "마지막 저장 시각" 비교로 방어.
- Winters 현황의 정직한 한계: `Server/`의 GameRoom은 sessionId 중심이고 Go Auth의 user_id와 브리지가 아직 없다(`.md/plan/backend/00_BACKEND_PLAN_INDEX.md`에 Phase 10B로 명시) — "여기까지 했고, 다음이 뭔지 안다"로 답하면 오히려 강점.

#### 7-6. 샤딩 vs 파티셔닝

- **파티셔닝**: **한 DB 안에서** 테이블을 물리 분할. 수평(행 분할: 범위/해시/리스트) / 수직(컬럼 분할). Postgres 선언적 파티셔닝으로 `match_history`를 `played_at` 월 단위 분할하면 — 최근 파티션만 스캔(partition pruning), 오래된 로그는 `DROP PARTITION`으로 즉시 폐기(DELETE 대비 수천 배 저렴).
- **샤딩**: **여러 DB 서버로** 수평 분할. 단일 장비의 쓰기 처리량/저장 한계 돌파.
  - 키 선택: 게임은 거의 항상 **user_id 해시 샤딩** — 한 유저의 지갑+인벤토리+스탯이 같은 샤드에 모여 **유저 단위 트랜잭션이 샤드 안에서 닫힌다**. 이게 샤드 키 설계의 제1원칙.
  - 잃는 것: 크로스 샤드 조인 불가(랭킹 같은 전역 집계는 Redis/별도 집계 스토어로 — Winters의 Redis 랭킹이 정확히 이 역할), 크로스 샤드 트랜잭션은 2PC 비용 때문에 회피하고 사가(보상 트랜잭션)/멱등 재시도로 설계.
  - 해시 vs 범위: 해시는 균등 분산(핫스팟 방지), 범위는 순회 유리하나 신규 유저 몰림. 리샤딩 고통 완화용 consistent hashing / 디렉터리(라우팅 테이블) 방식.
- 순서 답변: "단일 DB 튜닝(인덱스/쿼리) → 읽기 복제본 분리 → 캐시 → 파티셔닝 → 마지막이 샤딩". 샤딩을 먼저 말하면 감점.

#### 7-7. 매치 결과 반영 — 이벤트 기반 비동기 (실물)

매치 종료 → Kafka `MatchCompleted` 이벤트 발행 → `profile/consumer.go`가 소비: `match_history` INSERT → `player_stats` 누적 UPDATE → 프로필 캐시 무효화. 게임 서버는 DB 완료를 기다리지 않고(**게임 루프 비블로킹**), 전적 반영은 최종 일관성(eventual consistency). 컨슈머 실패 시 이벤트 재소비로 복구 — 이때 중복 INSERT 방지를 위해 `(match_id, user_id)` UNIQUE가 있으면 더 견고하다(현 스키마엔 인덱스만 있고 UNIQUE는 없음 — 개선점으로 언급 가능).

---

### 8. DB 비동기 처리와 커넥션 풀

#### 8-1. 왜 게임 서버는 DB를 동기로 부르면 안 되나

게임 서버 틱 루프(예: 30~60Hz)는 프레임당 예산이 16~33ms다. DB 쿼리 1~10ms(+네트워크 왕복, 락 대기 시 그 이상)를 틱 스레드에서 동기 호출하면 그 틱의 모든 유저 시뮬레이션이 멈춘다. 따라서:

```text
[틱 스레드]                     [DB 워커 스레드(들)]
게임 로직 → 요청 큐에 push  →   큐에서 pop → 쿼리 실행(블로킹 OK)
     ↑                              ↓
결과 큐에서 pop ← ←  ←  ←   결과(콜백) push
(틱 시작/끝에 드레인, 게임 상태 반영은 틱 스레드에서만)
```

핵심 규칙: (a) 블로킹 I/O는 전용 워커에 격리, (b) **게임 상태 변경(콜백 실행)은 반드시 메인/틱 스레드에서** — 락 지옥 방지, (c) 같은 유저의 요청은 같은 워커로 해시 라우팅해 순서 보장(차감→지급 순서 역전 방지), (d) 결과가 돌아올 때까지 해당 유저 행동을 잠그거나(구매 중 상태) 낙관적으로 진행 후 보정.

#### 8-2. Winters 실물 — 같은 패턴의 HTTP 버전

`Client/Private/Network/Backend/CHttpClient.cpp`가 이 패턴의 동형 구현이다:

- `LaunchAsyncRequest`: `std::async(launch::async, ...)`로 워커에서 WinHTTP 블로킹 요청 실행, 완료 시 결과를 `m_PendingCallbacks` 큐에 push (mutex 보호).
- `ProcessCallbacks`: **메인 스레드(게임 루프)**가 매 프레임 큐를 swap해 드레인 — 콜백(게임 상태 변경)이 항상 메인 스레드에서 실행됨을 보장.
- 실전 버그 경험: `std::async` 반환 `future`를 버리면 임시 future 소멸자가 완료를 대기해 **사실상 동기 호출**이 되는 함정을 겪고, `m_PendingRequests`가 future 수명을 소유 + 소멸자에서 드레인하는 구조로 고쳤다(파일 주석에 박제, gotcha 2026-07-09). "비동기 래퍼가 몰래 동기가 되는" 이 사례는 DB 워커 설계 질문에서 강력한 실전 근거다.
- 워커가 멤버 대신 호출 시점 스냅샷(`RequestSnapshot`)만 읽게 해서 `SetAuthToken`과의 데이터 레이스를 원천 차단 — DB 워커에 쿼리 파라미터를 값 복사로 넘기는 원칙과 동일.

#### 8-3. 커넥션 풀

- **왜**: DB 커넥션 1개 수립 = TCP 핸드셰이크 + 인증 + 백엔드 프로세스/스레드 할당으로 수 ms~수십 ms. 요청마다 만들면 연결 비용이 쿼리 비용을 압도하고, DB의 `max_connections`(Postgres 기본 100)를 폭주로 초과하면 신규 연결 거부. → 미리 N개 만들어 빌려주고 반납받는다.
- **Winters 실물** (`Services/pkg/database/postgres.go`): pgxpool로 `MaxConns`(설정값), `MinConns=2`, `MaxConnLifetime=30m`(오래된 커넥션 순환 — LB/서버 재시작 대응), `MaxConnIdleTime=5m`, `HealthCheckPeriod=30s`. Redis도 `PoolSize: 50` (`pkg/cache/redis.go`).
- **사이징 감각**: 클수록 좋지 않다. DB가 실제로 동시에 일할 수 있는 수는 코어 수 기반(경험식 `cores * 2 + effective_spindle`). 과대 풀은 DB 내부 경합만 늘린다. 서비스 인스턴스 수 × 풀 크기 ≤ max_connections도 확인.
- 꼬리질문 대비: 풀 고갈(트랜잭션 열고 오래 잡고 있는 코드, 커넥션 누수) → 대기 타임아웃 설정 + 트랜잭션은 짧게.

---

### 9. ORM

- **정의**: 객체 ↔ 관계형 테이블 매핑 자동화 계층 (JPA/Hibernate, Entity Framework, GORM, SQLAlchemy).
- **장점**: 보일러플레이트 제거(CRUD 자동), DB 벤더 추상화, 파라미터 바인딩 기본 제공으로 SQL 인젝션 방어, 마이그레이션 도구 연계, 도메인 객체 중심 코드.
- **단점**: (a) **N+1 쿼리** — 연관 객체 lazy loading이 루프에서 행마다 쿼리 발사, (b) 생성 SQL 불투명 → 실행계획 튜닝 어려움, (c) 복잡한 집계/윈도우 함수/락 힌트는 결국 raw SQL, (d) 추상화 학습 비용이 SQL 학습 비용보다 싸지 않음.
- **Winters의 선택**: ORM 없이 **pgx/v5 + raw SQL**(모든 repository가 SQL 직서술). 이유를 말할 수 있어야 한다 — 재화 트랜잭션은 `FOR UPDATE`, `ON CONFLICT`, `RETURNING`처럼 SQL 기능을 정밀 제어해야 하고, 쿼리가 코드에 그대로 보여야 리뷰/튜닝이 된다. 반대로 CRUD 화면이 많은 어드민 툴이라면 ORM이 생산성 우위 — "도구는 워크로드로 고른다"가 결론.
- **N+1 실물 자백(강점으로 전환)**: `leaderboard/repository.go:GetTop`이 ZREVRANGE로 N명을 받은 뒤 **루프에서 유저마다 QueryRow**(username/wins/losses)를 날린다 — ORM 없이도 N+1은 만들 수 있다는 산 증거. 수정안: `WHERE u.id = ANY($1)` 한 방 조회 후 맵 조인, 또는 username을 sorted set 멤버에 인코딩. 이걸 스스로 지적하면 "코드를 성능 관점으로 다시 읽을 줄 아는 신입"이 된다.

---

## 예상 질문 & 모범답변

### Q1. RDBMS와 NoSQL의 차이를 설명하고, 게임 서버에서 각각 어디에 쓰겠습니까?

**답**: RDBMS는 고정 스키마의 테이블과 관계로 데이터를 저장하고 SQL·트랜잭션·ACID·조인을 제공합니다. NoSQL은 수평 확장을 위해 이 중 일부(조인, 강한 일관성, 고정 스키마)를 완화한 저장소군으로 Key-Value/Document/Wide-Column/Graph로 분류됩니다. 선택 기준은 "이 데이터가 틀리면 사고인가, 잠깐 낡아도 되는가"입니다. 유저 계정·지갑·인벤토리·결제처럼 정합성이 생명인 데이터는 RDBMS, 랭킹 조회·세션·매칭 큐처럼 초저지연 고빈도이고 원본에서 재구축 가능한 데이터는 Redis에 둡니다. 제 프로젝트(Winters 백엔드 `Services/`)도 정확히 이렇게 나눴습니다 — 원본은 전부 PostgreSQL(migrations 7종), Redis에는 `leaderboard:mmr` sorted set과 세션 토큰·매칭 상태만 두고, Redis가 날아가면 `SyncFromDB`로 DB에서 재적재합니다(`Services/internal/leaderboard/repository.go`). **꼬리 대비**: "NoSQL은 트랜잭션이 없나요?" → MongoDB 4.0+ 멀티도큐먼트 트랜잭션, Redis MULTI/EXEC(원자 일괄 실행, 롤백은 없음)처럼 있긴 하지만 RDB만큼 일반적이지 않다고 정정해서 답합니다.

### Q2. 조인의 종류를 설명해 주세요. LEFT JOIN 결과 행 수가 왼쪽 테이블보다 많아질 수 있습니까?

**답**: INNER는 양쪽 매칭 행만, LEFT/RIGHT OUTER는 한쪽 기준 전부(+상대는 NULL 채움), FULL OUTER는 양쪽 전부, CROSS는 카티션 곱, SELF는 같은 테이블 별칭 조인입니다. **많아질 수 있습니다** — 조인 조건이 1:N이면 왼쪽 한 행이 N행으로 복제됩니다. 예로 `users LEFT JOIN inventory`는 아이템 5개 가진 유저가 5행이 됩니다. 이 "행 뻥튀기"를 모른 채 COUNT/SUM 하면 집계가 틀리는 게 실무 최다 사고입니다. 제 코드에선 `inventory JOIN shop_items`(`Services/internal/shop/repository.go:GetInventory`)처럼 N:1 방향(항상 1행 매칭)으로 조인하거나, 1:1 제약(`wallets.user_id UNIQUE`)을 스키마로 보장해 이 문제를 피했습니다. **꼬리 대비**: 물리 조인 알고리즘(Nested Loop/Hash/Merge)까지 물으면 — 소량×인덱스는 NL, 대량 등가는 Hash, 정렬 입력은 Merge, 선택은 옵티마이저가 통계로 한다고 답합니다.

### Q3. WHERE와 HAVING의 차이는 무엇입니까? GROUP BY는 내부적으로 어떻게 동작합니까?

**답**: 논리 실행 순서가 `FROM → WHERE → GROUP BY → HAVING → SELECT → ORDER BY`라서, WHERE는 그룹핑 **전** 개별 행을 거르고 HAVING은 그룹핑 **후** 집계 결과를 거릅니다. 그래서 WHERE에는 집계함수를 못 씁니다. 내부적으로 GROUP BY는 해시 집계(hash aggregate — 그룹 키로 해시테이블 구축) 또는 정렬 집계(sort + 인접 그룹 스캔)로 구현되고, 옵티마이저가 데이터량과 인덱스 유무로 선택합니다. GROUP BY 키에 인덱스가 있으면 정렬 생략이 가능합니다. 성능 팁은 "먼저 WHERE로 줄이고 집계"입니다 — 같은 조건이면 HAVING이 아니라 WHERE에 두는 게 집계 대상 자체를 줄입니다. 게임 예시로 유저별 승수 집계를 들 수 있는데, 제 프로젝트는 이 집계를 매 조회마다 하지 않으려고 `player_stats`에 wins/losses를 미리 누적하는 비정규화를 택했고(`Services/migrations/000003`), 매치 종료 이벤트 시점에만 갱신합니다(`Services/internal/profile/consumer.go`).

### Q4. 서브쿼리와 조인 중 무엇을 언제 쓰겠습니까?

**답**: 결과에 상대 테이블 컬럼이 필요하면 조인, 존재 여부/집합 필터만 필요하면 EXISTS/IN 서브쿼리가 의도를 더 정확히 표현합니다. 성능 관점에선 현대 옵티마이저가 IN/EXISTS를 세미조인으로 변환해 큰 차이가 없는 경우가 많지만, **상관 서브쿼리**(외부 행마다 재실행)가 변환되지 못하면 O(N×M)이 되므로 EXPLAIN으로 확인합니다. 의미 차이도 중요합니다 — 조인은 1:N에서 행이 복제되지만 EXISTS는 중복이 없습니다. 함정으로 `NOT IN` 서브쿼리에 NULL이 섞이면 전체 결과가 공집합이 되는 3치 논리 문제가 있어 `NOT EXISTS`를 선호합니다. **꼬리 대비**: "FROM절 서브쿼리(인라인 뷰)는요?" → 집계를 먼저 한 뒤 조인해야 행 뻥튀기 없이 정확한 집계가 될 때 씁니다. 예: 유저별 최근 전적 집계 후 users와 조인.

### Q5. 기본키와 유니크 제약의 차이, 그리고 외래키가 게임 DB에서 하는 역할을 말해 보세요.

**답**: PK는 행의 유일 식별자로 NULL 불가·테이블당 1개이며, InnoDB에선 클러스터드 인덱스의 기준이 됩니다. UNIQUE는 NULL 허용(Postgres는 NULL끼리 중복 허용)·여러 개 가능합니다. 둘 다 유니크 인덱스를 자동 생성합니다. FK는 참조 무결성 — 존재하지 않는 유저의 인벤토리 행 같은 고아 데이터를 차단합니다. 제 스키마에선 `wallets.user_id UUID NOT NULL UNIQUE REFERENCES users(id) ON DELETE CASCADE`(`Services/migrations/000002`)로 1:1 관계+계정 삭제 연쇄를 표현했고, `inventory UNIQUE(user_id, item_id)`(`000007`)는 중복 소유 행을 막으면서 `ON CONFLICT DO UPDATE` upsert의 기반이 됩니다. **트레이드오프**: 대규모 라이브 서비스에선 FK 검사 비용과 마이그레이션 제약 때문에 FK를 빼고 애플리케이션에서 보장하는 회사도 많습니다 — "FK는 정합성 방어선이지만 쓰기 비용과 운영 유연성의 대가가 있다"까지 말하면 균형 잡힌 답이 됩니다. **자기 감사 포인트**: 제 `users` 테이블은 UNIQUE가 이미 인덱스를 만드는데 같은 컬럼에 `CREATE INDEX`를 또 해서 중복 인덱스가 있습니다 — 쓰기마다 불필요한 인덱스 갱신이 생기므로 제거 대상입니다.

### Q6. 인덱스를 걸면 왜 빨라집니까? B+tree 구조로 설명해 주세요.

**답**: 인덱스가 없으면 조건 검색은 테이블 전체 페이지를 읽는 풀 스캔 O(N)입니다. B+tree 인덱스는 키를 정렬 상태로 유지하는 균형 트리인데, 노드가 디스크 페이지 단위(8~16KB)라 노드 하나가 수백 개의 자식을 가리킵니다(팬아웃 수백). 팬아웃 500이면 깊이 3에 1.25억 행이 커버되므로, **1억 행에서도 페이지 읽기 3~4번**으로 목표 행에 도달합니다. 게다가 실제 데이터는 리프에만 있고 리프끼리 연결 리스트로 이어져 있어 범위 검색·정렬(`ORDER BY mmr DESC LIMIT 100`)이 리프 순차 스캔으로 처리됩니다. 제 스키마의 `idx_player_stats_mmr ON player_stats(mmr DESC)`(`Services/migrations/000003`)와 `idx_match_history_user(user_id, played_at DESC)`(`000004`)가 각각 랭킹 정렬과 "유저별 최근 N건"을 위해 설계한 인덱스입니다. **꼬리 대비**: "그럼 왜 모든 컬럼에 안 걸어요?" → 쓰기마다 모든 인덱스 갱신 + 페이지 분할 비용, 저장 공간, 옵티마이저 혼란 때문에 쿼리 패턴이 증명된 컬럼에만 겁니다.

### Q7. 인덱스 자료구조로 해시가 아니라 B+tree를 쓰는 이유는요?

**답**: 해시는 등가 조회 O(1)로 점 조회는 더 빠르지만, 키가 해시값으로 흩어져 **순서가 없기 때문에** 범위 검색(`WHERE mmr > 1500`), 정렬(`ORDER BY played_at DESC`), 접두 매칭(`LIKE 'abc%'`), 복합 인덱스의 부분 사용이 전부 불가능합니다. 게임 쿼리는 랭킹·최근 전적·기간 조회처럼 범위+정렬이 지배적이라 B+tree가 표준입니다. 또 B+tree는 리프 연결 리스트 덕에 범위 스캔이 순차 I/O라는 디스크 친화성이 있습니다. 이진 탐색 트리 대비로도 — 깊이가 log₂N(1억이면 27회 I/O)이 아니라 log₅₀₀N(3~4회)라는 게 디스크 기반 DB에서 결정적입니다. **꼬리 대비**: "해시 인덱스를 쓰는 곳은?" → 메모리 기반 저장소. Redis가 정확히 그 예로, sorted set은 해시(멤버→점수 O(1))와 skip list(순서 연산 O(log N))를 **같이** 들고 두 종류 연산을 다 잡습니다.

### Q8. 클러스터드 인덱스와 논클러스터드 인덱스의 차이를 설명해 주세요.

**답**: 클러스터드는 인덱스 리프가 곧 데이터 행 자체라 테이블이 그 키 순서로 물리 저장되고 테이블당 1개입니다(InnoDB의 PK). 논클러스터드는 리프에 행의 위치 정보만 있어서, InnoDB 세컨더리 인덱스는 리프에 PK를 담고 있고 조회 시 세컨더리 트리 탐색 후 PK 트리를 한 번 더 타는 이중 탐색(북마크 룩업)이 발생합니다. 그래서 InnoDB에선 PK를 짧게 유지하는 게 세컨더리 인덱스 전체 크기에 영향을 줍니다. 중요한 각주로, **제가 쓰는 PostgreSQL에는 클러스터드 인덱스가 없습니다** — 모든 테이블이 힙이고 모든 인덱스가 TID(페이지, 오프셋)를 가리키는 논클러스터드입니다. `CLUSTER` 명령은 일회성 재정렬일 뿐입니다. **꼬리 대비**: "UUID를 PK로 쓰면 무슨 문제가?" → InnoDB라면 랜덤 삽입 위치 때문에 페이지 분할과 캐시 미스가 폭증하고 세컨더리 인덱스도 비대해집니다. 제 스키마는 `uuid_generate_v4()` PK인데 Postgres 힙 구조라 삽입 지점 문제는 없지만, 인덱스 지역성 관점에선 시간순 UUIDv7/ULID가 더 낫다는 걸 알고 있고, 분산 환경에서 ID 충돌 없는 생성이라는 이점과 교환한 선택이라고 설명합니다.

### Q9. 커버링 인덱스가 무엇이고 언제 씁니까?

**답**: 쿼리가 필요로 하는 모든 컬럼(SELECT/WHERE/ORDER BY)이 인덱스 안에 있어서 테이블 접근 없이 인덱스만 읽고 끝나는 경우입니다. 논클러스터드 인덱스의 최대 비용인 북마크 룩업(행마다 랜덤 I/O)이 제거돼 수배~수십 배 빨라집니다. 제 스키마의 `idx_match_history_user(user_id, played_at DESC)` 기준으로 `SELECT played_at FROM match_history WHERE user_id=$1`은 index-only로 끝나지만 `SELECT result ...`는 힙 접근이 필요합니다 — result까지 자주 필요하면 Postgres `INCLUDE (result)`로 리프에만 실을 수 있습니다. **심화**: Postgres index-only scan은 MVCC 가시성 확인 때문에 visibility map이 최신일 때만 진짜로 힙을 안 봅니다. VACUUM이 밀리면 index-only여도 힙을 찍으므로, "커버링인데 왜 느리지?"의 원인 중 하나입니다. **트레이드오프**: 컬럼을 인덱스에 실을수록 인덱스가 커지고 쓰기 비용이 늘어 무한정 INCLUDE는 답이 아닙니다.

### Q10. 인덱스가 있는데도 안 타는 경우를 아는 대로 말해 보세요.

**답**: 대표적으로 8가지입니다. (1) 인덱스 컬럼 가공 — `WHERE LOWER(email)=...`은 B+tree 키와 직접 비교가 불가하므로 함수 기반 인덱스가 없으면 풀 스캔, (2) 선행 와일드카드 `LIKE '%sword'` — 정렬 구조상 prefix만 탐색 가능, (3) 암묵적 형변환 — VARCHAR 컬럼 = 숫자 리터럴이면 컬럼 쪽이 캐스팅됨, (4) 복합 인덱스 선두 컬럼 누락(leftmost prefix 위반), (5) 낮은 선택도 — 행의 대부분이 매칭되면 랜덤 I/O로 찍는 것보다 순차 풀 스캔이 싸서 옵티마이저가 **의도적으로** 안 탑니다. 이건 버그가 아니라 최적일 수 있습니다, (6) 서로 다른 컬럼 간 OR, (7) 부정 조건(!=, NOT IN), (8) 통계 정보가 낡아 옵티마이저 오판 — ANALYZE 필요. 진단 절차는 항상 `EXPLAIN ANALYZE`로 실제 실행계획과 행 추정치를 보고, 추정과 실측의 괴리부터 확인합니다. **꼬리 대비**: "그럼 is_active 같은 boolean에 인덱스는?" → 단독으론 무의미, 다만 `is_active=true`가 극소수인 편향 분포면 부분 인덱스(`WHERE is_active`)가 유효하다고 답합니다.

### Q11. 복합 인덱스 (A, B)가 있을 때 어떤 쿼리가 이 인덱스를 탈 수 있습니까?

**답**: leftmost prefix 규칙에 따라 `WHERE A=?`, `WHERE A=? AND B=?`, `WHERE A=? ORDER BY B`는 타지만 `WHERE B=?` 단독은 못 탑니다. 정렬이 A로 먼저 되고 A가 같을 때만 B 순서가 의미 있기 때문입니다 — 전화번호부가 성(姓)으로 정렬돼 있을 때 이름만으로 못 찾는 것과 같습니다. 설계 원칙은 "동등 조건 컬럼을 앞에, 범위/정렬 컬럼을 뒤에"입니다. 범위 조건 컬럼 뒤의 컬럼은 탐색에 못 쓰이기 때문입니다. 제 스키마의 `idx_match_history_user(user_id, played_at DESC)`와 `idx_coin_tx_user(user_id, created_at DESC)`(`Services/migrations/000004`, `000006`)가 정확히 "user_id 동등 + 시간 내림차순 정렬" 쿼리에 맞춘 순서입니다. 실제 쿼리 `WHERE user_id=$1 ORDER BY played_at DESC LIMIT $2`(`profile/repository.go:GetMatchHistory`)는 이 인덱스로 정렬 없이 리프를 앞에서부터 LIMIT만큼만 읽습니다. **꼬리 대비**: "(A,B)가 있으면 (A) 인덱스도 필요한가요?" → 대부분 불필요한 중복이며 쓰기 비용만 늘립니다.

### Q12. 트랜잭션의 ACID를 설명하고, 각각 DB가 어떻게 보장하는지 말해 보세요.

**답**: Atomicity는 전부 반영되거나 전부 취소 — MySQL은 undo 로그로 변경 전 이미지를 보관해 롤백하고, PostgreSQL은 MVCC 특성상 새 버전을 만들었다가 트랜잭션을 abort 마킹하면 그 버전들이 무효가 됩니다. Consistency는 트랜잭션 전후 제약조건과 불변식 유지 — DB의 CHECK/FK/UNIQUE와 애플리케이션 검증의 합작입니다. Isolation은 동시 트랜잭션 간 간섭 차단 — 락과 MVCC로 구현하며 격리수준으로 강도를 조절합니다. Durability는 COMMIT 응답 후엔 크래시에도 생존 — WAL(Write-Ahead Log)에 변경 로그를 먼저 fsync하고, 데이터 페이지 반영은 나중에 해도 크래시 후 WAL 재생으로 복구합니다. "커밋 완료 = WAL이 디스크에 동기화된 시점"입니다. 제 프로젝트 연결로는 회원가입이 users+wallets+player_stats 3개 INSERT를 한 트랜잭션으로 묶어(`Services/internal/auth/repository.go:CreateUserWithWalletAndStats`) '지갑 없는 반쪽 계정'을 원자성으로 차단하고, 재화 일관성은 `CHECK (balance >= 0)`(`000002`)로 DB 레벨 이중 방어를 둔 것을 예로 듭니다.

### Q13. 격리수준 4단계와 각 단계에서 발생하는 이상현상을 설명해 주세요.

**답**: READ UNCOMMITTED는 커밋 안 된 값도 보여 dirty read 발생, READ COMMITTED는 커밋된 것만 보여 dirty는 막지만 한 트랜잭션 안에서 재조회 시 값이 변하는 non-repeatable read와 행 집합이 변하는 phantom read는 허용, REPEATABLE READ는 non-repeatable까지 방지(표준상 phantom은 허용), SERIALIZABLE은 직렬 실행과 동등해 전부 방지합니다. 기본값은 PostgreSQL/Oracle이 RC, MySQL InnoDB가 RR입니다. 여기에 구현 각주를 달면 — Postgres RR은 스냅샷 격리라 phantom도 실질적으로 안 보이는 대신 write skew는 남고(SSI인 SERIALIZABLE에서만 차단), InnoDB RR은 넥스트키 락으로 phantom을 대부분 막습니다. **실무 태도**: 격리수준을 전역으로 올리는 건 동시성 손해가 커서, 기본값을 유지하고 위험 지점만 명시적 락으로 조입니다. 제 구매 트랜잭션도 RC 기본값에서 지갑 행만 `FOR UPDATE`로 잠급니다(`Services/internal/shop/repository.go:Purchase`). **꼬리 대비**: "RC에서 같은 행을 두 번 읽으면?" → 사이에 다른 커밋이 있으면 다른 값 — 그래서 '읽고 판단하고 쓰는' 로직은 격리수준이 아니라 락이나 원자적 UPDATE로 풀어야 한다고 연결합니다.

### Q14. dirty read, non-repeatable read, phantom read를 게임 예시로 구분해 주세요.

**답**: (1) **Dirty read** — 결제 트랜잭션이 지갑에 +1000을 쓰고 아직 커밋 전인데, 다른 트랜잭션이 그 잔액을 읽고 구매를 허용해버림. 결제가 롤백되면 존재한 적 없는 돈으로 아이템을 산 겁니다. (2) **Non-repeatable read** — 거래 검증 트랜잭션이 잔액을 두 번 확인하는데, 처음엔 500, 두 번째엔(사이에 다른 구매가 커밋되어) 100 — 같은 행의 **값**이 변했습니다. (3) **Phantom read** — "이 유저의 전설 아이템 개수"를 COUNT했더니 2개, 지급 이벤트가 커밋된 뒤 다시 COUNT하니 3개 — 행 **집합**이 변했습니다. 구분 기준은 "이미 읽은 행의 값이 변함(non-repeatable)" vs "조건에 걸리는 행이 늘거나 줆(phantom)"입니다. phantom이 위험한 실전 예로 "닉네임 중복 검사 후 INSERT" 레이스가 있는데, 격리수준으로 풀기보다 UNIQUE 제약으로 푸는 게 정석이고 제 `users.username UNIQUE`(`000001`)가 그 방어선입니다.

### Q15. 락과 MVCC의 차이를 설명해 주세요. 현대 DB는 어떻게 조합합니까?

**답**: 락은 접근 자체를 직렬화하는 비관적 방식 — 정합성은 확실하지만 대기와 데드락 비용이 있습니다. MVCC는 데이터를 덮어쓰지 않고 버전을 남겨, 읽기 트랜잭션이 자기 스냅샷 시점의 버전을 읽는 방식 — **읽기와 쓰기가 서로를 전혀 막지 않게** 됩니다. 구현은 Postgres가 행에 xmin/xmax 트랜잭션 ID를 박고 UPDATE를 '새 버전 INSERT'로 처리(죽은 버전은 VACUUM이 회수), InnoDB는 최신 행 + undo 로그 체인으로 과거 버전을 복원합니다. 다만 MVCC도 **쓰기-쓰기 충돌은 해결 못 하므로** 그 지점엔 여전히 행 락이 필요합니다. 그래서 현대 DB는 "읽기는 MVCC 스냅샷으로 락 프리, 쓰기 충돌 지점만 락"의 하이브리드입니다. 제 코드가 그 축소판입니다 — 프로필/인벤토리 조회는 락 없이 MVCC 스냅샷으로 읽고, 잔액 차감만 `SELECT ... FOR UPDATE`로 직렬화합니다. **꼬리 대비**: "MVCC의 비용은?" → 버전 누적으로 인한 공간 팽창(bloat)과 VACUUM/GC 부담, 롱 트랜잭션이 오래된 스냅샷을 잡고 있으면 회수가 밀리는 문제를 답합니다.

### Q16. SELECT FOR UPDATE는 언제, 왜 씁니까? 실제로 써본 적 있습니까?

**답**: "읽고 → 검증하고 → 쓰는" 로직에서 읽기와 쓰기 사이에 다른 트랜잭션이 끼어드는 걸 막을 때 씁니다. 써봤습니다 — 아이템 구매(`Services/internal/shop/repository.go:Purchase`)에서 `SELECT balance FROM wallets WHERE user_id=$1 FOR UPDATE`로 지갑 행에 배타 락을 걸고, 잔액 검증 → 차감 → 인벤토리 upsert → 원장 INSERT를 커밋까지 원자로 유지합니다. 이 락이 없으면 잔액 100에 가격 100짜리 동시 구매 2건이 둘 다 검증을 통과해 이중 차감됩니다. FOR UPDATE로 두 번째 요청은 첫 커밋을 기다린 뒤 갱신된 잔액 0을 읽고 정상 거절됩니다. **대안과 트레이드오프**: `UPDATE wallets SET balance = balance - $2 WHERE user_id=$1 AND balance >= $2`처럼 검증과 차감을 한 문장에 합치면 락 문장이 없어도 안전하고 왕복이 줄어듭니다(affected rows=0 = 잔액 부족). 저는 잔액 부족과 지갑 없음을 다른 에러로 구분해 반환해야 해서 조회형을 썼는데, 처리량이 더 중요해지면 원자 UPDATE로 바꿀 수 있다고 답합니다. **꼬리 대비**: "FOR UPDATE 중 다른 트랜잭션의 일반 SELECT는?" → MVCC 스냅샷 읽기는 락과 무관하게 통과합니다. 막히는 건 같은 행에 대한 쓰기와 FOR UPDATE뿐입니다.

### Q17. 데드락이 무엇이고, 어떻게 예방/처리합니까?

**답**: 두 트랜잭션이 서로가 가진 락을 기다리는 순환 대기입니다. 조건은 상호배제·점유대기·비선점·순환대기 4가지가 동시에 성립할 때입니다. 게임 예시: 선물 교환에서 트랜잭션1이 A지갑→B인벤토리 순으로, 트랜잭션2가 B지갑→A인벤토리 순으로 잠그면 교차 대기가 됩니다. **예방**은 락 획득 순서 전역 통일이 제일 효과적입니다 — "항상 user_id 오름차순으로 잠근다" 같은 규칙이면 순환이 원천 차단됩니다. 추가로 트랜잭션을 짧게(락 잡고 외부 API 호출 금지), 한 트랜잭션이 잡는 락 수 최소화. **처리**는 DBMS가 대기 그래프로 감지해 한쪽을 강제 롤백(Postgres 에러 40P01)하므로, 애플리케이션은 그 에러를 잡아 지수 백오프 재시도합니다. 재시도가 안전하려면 트랜잭션이 멱등하거나 재실행 가능해야 하는데, 제 구매 로직은 실패 시 `defer tx.Rollback`으로 완전 원복되므로 재시도 안전합니다. **꼬리 대비**: "락 타임아웃과 데드락 감지의 차이?" → 타임아웃은 순환이 아니어도 오래 기다리면 포기, 감지는 순환을 찾아 즉시 희생자 선택 — 보통 둘 다 켭니다.

### Q18. 낙관적 락과 비관적 락의 차이와 선택 기준은요?

**답**: 비관적 락은 충돌을 전제로 먼저 잠급니다(FOR UPDATE) — 정합성이 확실하지만 대기·데드락 비용. 낙관적 락은 안 잠그고 커밋 시 버전 컬럼 비교로 충돌을 감지합니다 — `UPDATE ... SET version=version+1 WHERE id=? AND version=?`에서 affected rows가 0이면 충돌이므로 다시 읽고 재시도. 락 대기가 없어 동시성이 좋지만 충돌률이 높으면 재시도 폭풍으로 역효과입니다. **선택 기준은 충돌 빈도와 실패 허용도**입니다. 지갑·인벤토리처럼 충돌 시 틀리면 안 되고 동시 접근이 몰리는 데이터는 비관적(제 Purchase가 그 예), 프로필 편집·설정처럼 충돌이 드물고 "다시 시도해 주세요"가 허용되는 곳은 낙관적입니다. **꼬리 대비**: "낙관적 락은 DB 기능인가요?" → 아니고 UPDATE의 원자성(행 락)을 이용한 애플리케이션 패턴입니다. JPA @Version 같은 ORM 지원이 있을 뿐입니다. "ABA 문제는?" → 단조 증가 version 컬럼을 쓰면 값이 같아도 버전이 다르므로 안전합니다.

### Q19. 정규화 1NF, 2NF, 3NF를 설명하고 실제 스키마에 적용한 예를 들어 보세요.

**답**: 1NF는 모든 값이 원자적 — 인벤토리를 `items = "sword,shield"` 문자열로 두면 위반이고 행으로 분리해야 합니다. 2NF는 1NF에서 복합키의 일부에만 종속하는 컬럼 제거 — `(user_id, item_id)`가 키인 인벤토리에 item_name을 두면 item_id에만 종속이라 위반입니다. 제 스키마가 정확히 이걸 분리했습니다: `inventory(user_id, item_id, quantity)`는 소유 사실만, 이름·가격·타입은 `shop_items`에(`Services/migrations/000007`) — 아이템 밸런스 패치 때 인벤토리 수백만 행을 안 건드립니다. 3NF는 키가 아닌 컬럼 간 이행 종속 제거 — users에 guild_id와 guild_name을 같이 두면 위반이고 guilds로 분리합니다. **목적과 대가**: 정규화는 갱신 이상과 중복을 없애지만 조회 조인이 늘어납니다. 그래서 실무 원칙은 "3NF까지 설계하고, 측정된 성능 근거가 있을 때만 의도적으로 되돌린다"이고, 저도 집계 컬럼(`player_stats.wins/losses`)과 원장 스냅샷(`coin_transactions.balance_after`)은 의도적으로 비정규화했습니다 — 다음 질문에서 상세히 말씀드릴 수 있습니다.

### Q20. 게임 DB에서 의도적으로 비정규화하는 사례를 들어 보세요.

**답**: 제 레포에 실물이 3가지 있습니다. (1) **집계 사전 계산**: 승/패/KDA를 매 조회마다 match_history에서 COUNT하는 대신 `player_stats`에 누적 저장(`Services/migrations/000003`)합니다. 프로필 조회는 초당 수천 번, 매치 종료는 분당 수십 번 — 읽기:쓰기 비율이 압도적이라 쓰기 시점에 비용을 지불하는 게 이득입니다. 갱신은 매치 종료 Kafka 이벤트를 받는 컨슈머가 match_history INSERT와 함께 수행합니다(`Services/internal/profile/consumer.go`). (2) **원장 스냅샷**: `coin_transactions.balance_after`(`000006`)는 이전 거래 전체를 재집계하면 계산 가능한 값이지만, CS 분쟁·부정거래 조사에서 "그 시점 잔액"을 즉시 봐야 하므로 거래마다 박제합니다. (3) **저장소 간 중복**: MMR이 PostgreSQL(원본)과 Redis sorted set(조회용)에 이중 존재합니다 — 랭킹 조회 부하를 DB에서 떼어내는 대신 정합성 관리 비용이 생기고, 그래서 `SyncFromDB`라는 DB→Redis 재적재 복구 경로를 뒀습니다(`Services/internal/leaderboard/repository.go`). **원칙**: 비정규화는 항상 (a) 원본이 어디인지, (b) 사본이 깨졌을 때 복구 경로가 무엇인지를 함께 설계해야 한다고 마무리합니다.

### Q21. 유저-인벤토리-랭킹을 포함하는 게임 DB 스키마를 설계해 보세요. (화이트보드형)

**답**: 계정과 게임 데이터를 분리하는 것부터 시작합니다. `users(id, username UQ, email UQ, password)` — 비밀번호는 bcrypt 해시로 저장(평문/복호화 가능 암호화 금지). 재화는 users에 컬럼으로 넣지 않고 `wallets(user_id FK UQ, balance CHECK>=0)`로 분리합니다 — 락 경합 범위를 지갑 행으로 좁히고 CHECK로 음수를 차단하기 위해서입니다. 인벤토리는 유저×아이템 N:M을 `inventory(user_id, item_id, quantity, UNIQUE(user_id,item_id))` + `shop_items(id, name, price, metadata JSONB)`로 해소합니다 — 2NF 분리로 아이템 정보 변경이 인벤토리에 전파되지 않고, UNIQUE 복합키가 upsert 기반이 됩니다. 스탯/랭킹은 `player_stats(user_id UQ, mmr, wins, losses)`에 비정규화 집계 + `mmr DESC` 인덱스, 실시간 랭킹 조회는 Redis sorted set으로 이원화합니다. 돈이 움직인 기록은 `coin_transactions` append-only 원장, 외부 결제는 `payment_transactions(idempotency_key UNIQUE)`. 전적은 `match_history(user_id, played_at DESC 복합 인덱스)`로 두고 데이터가 쌓이면 월 단위 파티셔닝 대상입니다. — 이 구조 전체가 제 레포 `Services/migrations/000001~000007`에 실제로 있고, 각 결정의 이유를 방금처럼 하나씩 설명할 수 있습니다. **꼬리 대비**: "장비 강화/내구도처럼 개별 아이템 상태가 있다면?" → quantity 누적 모델이 아니라 아이템 인스턴스별 행(item instance 테이블)으로 바꿔야 하고, 그러면 UNIQUE(user_id,item_id)를 빼고 instance id가 PK가 된다고 답합니다.

### Q22. 아이템 구매 트랜잭션을 처음부터 끝까지 설계해 보세요. 동시에 두 요청이 오면 어떻게 됩니까?

**답**: 순서는 BEGIN → (1) 서버가 아이템 가격을 직접 조회(클라이언트가 보낸 가격은 절대 신뢰하지 않음) → (2) `SELECT balance ... FOR UPDATE`로 지갑 행 배타 락 → (3) 잔액 검증, 부족하면 롤백 → (4) 차감 UPDATE(RETURNING으로 신규 잔액 확보) → (5) 인벤토리 `INSERT ... ON CONFLICT (user_id,item_id) DO UPDATE quantity+1` upsert → (6) 원장 INSERT(금액, 사유, balance_after) → COMMIT입니다. 동시 요청 2건이 오면 두 번째는 (2)에서 첫 트랜잭션의 커밋/롤백까지 블로킹되고, 깨어나서 **갱신된 잔액**을 읽으므로 이중 차감이 불가능합니다. 만약 락을 빼먹어도 `CHECK (balance>=0)`이 최후 방어로 한쪽을 실패시키지만, 어느 쪽이 실패할지 통제가 안 되므로 락이 정석입니다. 이 코드는 `Services/internal/shop/repository.go:Purchase`에 그대로 있습니다. 원장까지 같은 트랜잭션에 넣는 이유는 "돈은 빠졌는데 기록이 없는" 상태를 구조적으로 불가능하게 만들기 위해서입니다. **꼬리 대비**: "구매 응답이 클라이언트에 유실되면?" → 서버 상태는 이미 커밋으로 확정, 클라이언트는 재조회로 동기화. 재시도로 이중 구매가 걱정되면 구매 요청에도 멱등성 키를 부여합니다 — 결제 쪽엔 이미 적용돼 있습니다(Q23).

### Q23. 결제/지급이 중복 처리되는 걸 어떻게 막습니까? (멱등성)

**답**: 네트워크에선 "정확히 한 번 전달"이 불가능합니다 — 타임아웃 후 재시도는 반드시 생기므로, "몇 번을 받아도 한 번만 처리"인 멱등성으로 설계합니다. 구현은 요청 단위 고유 키 + DB UNIQUE 제약입니다. 제 결제 테이블이 `idempotency_key VARCHAR(64) NOT NULL UNIQUE`(`Services/migrations/000005`)를 갖고, 처리 흐름은 (1) 키로 기존 거래 조회(`payment/repository.go:FindByIdempotencyKey`) → 있으면 **그때의 성공 응답을 재반환**(에러가 아니라 — 클라이언트 입장에선 첫 요청의 응답을 늦게 받은 것과 동일해야 하므로) → (2) 없으면 트랜잭션 처리. 조회와 삽입 사이에 동시 중복 요청이 끼어드는 레이스는 INSERT의 UNIQUE 위반이 최종적으로 잡아줍니다 — 애플리케이션 검사는 빠른 경로일 뿐, **정합성의 근거는 항상 DB 제약**이라는 게 포인트입니다. 아이템 지급도 동일하게 지급 사유 ID(mail_id, event_id)에 UNIQUE를 걸면 이벤트 재소비·재시도에도 중복 지급이 차단됩니다. **꼬리 대비**: "키는 누가 만들죠?" → 요청 발신자(클라이언트/게임서버)가 UUID로 생성해 재시도 시 같은 키를 재사용해야 합니다. 서버가 만들면 재시도가 새 요청으로 보여 의미가 없습니다.

### Q24. 같은 계정으로 두 기기에서 동시에 로그인하면 어떻게 처리하겠습니까?

**답**: 정책부터 정하고(게임은 대부분 "나중 로그인이 이김 + 기존 세션 킥") 계층별로 구현합니다. **인증 계층**: 로그인 성공 시 세션 식별자를 Redis `session:{userId}` 키에 덮어쓰고, 모든 요청 검증 시 자신의 세션 ID와 이 키를 비교 — 다르면 구세션이므로 거부/킥. Redis 단일 스레드 특성상 SET이 원자적이라 동시에 로그인 2건이 와도 마지막 SET이 이기고 나머지는 자연히 무효화됩니다. **게임 서버 계층**: 접속 시 같은 userId의 기존 연결이 있으면 킥 패킷 후 종료. 분산 환경이면 `SET lock:{userId} {serverId} NX EX ttl`로 접속 선점 — NX 실패면 이미 어딘가 접속 중이므로 해당 서버에 킥을 지시합니다. **데이터 계층**: 그래도 겹치는 최악의 순간(구세션의 마지막 저장 vs 신세션의 첫 저장)은 낙관적 버전 비교로 늦은 구세션 쓰기를 거부합니다. 제 프로젝트 현황을 정직하게 말하면 — refresh token을 `refresh:{jti}` TTL 키로 Redis에 저장하고 로그아웃 시 DEL로 즉시 무효화하는 구조까지 구현돼 있고(`Services/internal/auth/repository.go`), userId당 단일 세션 강제는 역방향 키 하나를 추가하면 되는 상태입니다. 게임 서버(GameRoom)는 아직 sessionId 중심이고 백엔드 user_id와의 브리지가 계획 단계(Phase 10B, `.md/plan/backend/00_BACKEND_PLAN_INDEX.md`)라는 한계와 다음 단계까지 알고 있습니다.

### Q25. Redis는 왜 빠릅니까? 단일 스레드인데 어떻게 그 처리량이 나오죠?

**답**: 세 가지입니다. (1) 인메모리 — 요청 경로에 디스크 I/O가 없습니다(영속화는 RDB 스냅샷/AOF가 백그라운드에서). (2) 명령 실행이 단일 스레드 이벤트 루프 — 락과 컨텍스트 스위칭이 없고, 그래서 **모든 명령이 자연히 원자적**입니다. INCR로 카운터를 만들 때 락이 필요 없는 이유입니다. (3) 명령이 자료구조 연산에 1:1 매핑된 최적화 구현(sorted set의 skip list 등). 단일 스레드로 충분한 이유는 병목이 CPU가 아니라 메모리/네트워크이기 때문이고, 6.0부터는 네트워크 파싱만 I/O 스레드로 병렬화합니다. **함정을 아는 게 중요합니다**: 단일 스레드라서 O(N) 명령 하나(`KEYS *`, 거대 컬렉션 `ZRANGE 0 -1`, `SMEMBERS`)가 전체 서버를 블로킹합니다. 운영에선 KEYS 대신 SCAN, 대형 컬렉션은 페이지 단위로 읽습니다. 제 매치메이커(`Services/internal/matchmaking/service.go:tryMatch`)가 매초 `ZRangeWithScores(queue, 0, -1)`로 큐 전체를 읽는데, 현재는 2인 매칭 프로토타입 규모라 허용이지만 큐가 커지면 점수 구간별 `ZRANGEBYSCORE` 배치로 바꿔야 한다는 개선점까지 인지하고 있습니다.

### Q26. 게임 랭킹을 Redis sorted set으로 구현하는 이유와 방법을 설명해 주세요.

**답**: SQL로 "내 순위"는 `SELECT COUNT(*) WHERE mmr > 내mmr` — 인덱스가 있어도 매 요청 범위 스캔이라 100만 유저가 랭킹 화면을 열 때마다 DB가 무너집니다. sorted set은 내부가 skip list + hash 이중 구조라 ZADD(갱신)·ZREVRANK(내 순위)·ZSCORE가 전부 O(log N), 상위 100 조회가 O(log N + 100)입니다. 100만 유저면 log N ≈ 20 스텝, 메모리 연산이라 마이크로초 단위입니다. 제 구현(`Services/internal/leaderboard/repository.go`)은 — 매치 종료 시 `ZADD leaderboard:mmr {mmr} {userId}`와 PostgreSQL `player_stats` UPDATE를 함께 수행하고, Top N은 ZREVRANGE, 내 순위는 ZREVRANK+ZSCORE+ZCARD로 반환합니다. **원본은 DB, Redis는 조회 가속용 사본**이라는 원칙으로, Redis 유실 시 `SyncFromDB`가 player_stats 전체를 파이프라인 ZADD로 재적재합니다. **자기 개선점 두 가지를 먼저 말합니다**: (1) Top N에서 유저명을 행마다 QueryRow로 가져오는 N+1이 있어 `ANY($1)` 배치 조회로 바꿔야 하고, (2) ZADD와 DB UPDATE 사이에 장애가 나면 불일치가 생기는 이중 쓰기 문제가 있어 — DB 커밋을 원본으로 삼고 Redis는 실패 시 재시도/주기 동기화로 수렴시키는 게 맞습니다. **꼬리 대비**: "동점자 순위는?" → 같은 score는 멤버 사전순이라 불공정할 수 있어, score에 `mmr × 2^20 + (2^20 - 도달순서)` 식으로 tie-break를 인코딩하는 기법을 답합니다.

### Q27. 캐시 전략(cache-aside, write-through, write-behind)을 비교하고, 캐시와 DB의 정합성은 어떻게 관리합니까?

**답**: **Cache-aside**: 읽기 시 캐시 미스면 DB에서 읽어 TTL과 함께 적재, 쓰기 시 DB 갱신 후 캐시를 **삭제**합니다. 필요한 것만 캐싱되고 캐시 장애에도 DB로 동작하는 게 장점, 미스 순간 지연과 짧은 스테일 창이 단점입니다. **Write-through**: 쓰기가 캐시와 DB를 함께 갱신 — 캐시가 항상 최신이지만 쓰기 지연 증가. **Write-behind**: 캐시에 먼저 쓰고 DB엔 배치 반영 — 쓰기 처리량이 최고지만 캐시 유실 = 데이터 유실이라 재화엔 금지, 위치·통계처럼 유실 허용 데이터 전용입니다. 제 프로필 조회가 cache-aside 실물입니다(`Services/internal/profile/repository.go:GetProfile`) — `profile:{userId}` 키, TTL 5분, 매치 종료 Kafka 컨슈머가 InvalidateCache로 삭제(`consumer.go`). **정합성 관리**: 갱신 시 캐시를 '업데이트'하지 않고 '삭제'하는 이유는 동시 쓰기 순서 역전으로 구값이 남는 사고를 피하기 위해서고, 그래도 "삭제 직후 미스 재적재가 구스냅샷을 읽는" 미세 레이스는 남으므로 TTL이 최종 수렴 장치입니다. 즉 "프로필은 최대 5분 스테일 허용"이라는 **제품 결정**을 명시한 겁니다. **꼬리 대비**: 캐시 스탬피드(인기 키 만료 순간 요청 폭주) → 재계산 분산 락, TTL 지터, 확률적 조기 갱신으로 답합니다.

### Q28. 세션을 Redis에 저장하는 이유는요? JWT만 쓰면 안 됩니까?

**답**: 게임 서버가 다중 인스턴스면 로그인한 서버와 다음 요청을 받는 서버가 다르므로 세션을 서버 메모리에 못 두고 공유 저장소가 필요합니다. Redis가 표준인 이유는 초당 수십만 조회 지연이 서브 밀리초고, TTL로 만료를 자동화하며, 단일 스레드 원자성으로 동시 로그인 경합도 자연 정리되기 때문입니다. JWT만 쓰는 무상태 방식은 저장소 조회가 없어 확장에 유리하지만 치명적 약점이 "**발급 후 회수 불가**"입니다 — 로그아웃, 강제 킥, 계정 정지를 토큰 만료 전에 반영할 수 없습니다. 그래서 실무 절충이 제 구현과 같은 하이브리드입니다(`Services/internal/auth/repository.go`): 수명이 짧은 access token은 서명 검증만으로 무상태 처리하고, **refresh token은 jti를 `refresh:{jti}` 키로 Redis에 TTL 저장** — 로그아웃/차단 시 DEL 하면 그 시점부터 재발급이 불가능해집니다. 매칭 상태도 `matchmaking:status:{userId}`를 TTL 600초로 둬서(`matchmaking/service.go`) 클라이언트가 죽어도 유령 큐 항목이 자동 청소됩니다 — TTL을 '상태 머신의 안전망'으로 쓰는 패턴입니다.

### Q29. 샤딩과 파티셔닝의 차이를 설명하고, 게임 DB라면 무엇을 기준으로 샤딩하겠습니까?

**답**: 파티셔닝은 **한 DB 인스턴스 안**에서 테이블을 물리 분할하는 것(범위/해시/리스트), 샤딩은 **여러 DB 서버**로 데이터를 나누는 수평 확장입니다. 파티셔닝의 이득은 최근 파티션만 스캔하는 pruning과, 오래된 파티션 DROP으로 대량 삭제를 즉시 처리하는 것 — 제 `match_history`가 쌓이면 `played_at` 월 단위 파티셔닝이 1순위 적용 대상입니다. 샤딩 키는 **user_id 해시**를 선택합니다. 이유는 게임 트랜잭션의 대부분(지갑 차감+인벤토리 지급+원장)이 한 유저 안에서 닫히므로, 유저의 모든 데이터를 같은 샤드에 모으면 **트랜잭션이 샤드 경계를 넘지 않기** 때문입니다 — 샤드 키 설계의 제1원칙입니다. 잃는 것은 전역 조회입니다: 전체 랭킹 같은 크로스 샤드 집계는 못 하므로 별도 집계 저장소로 빼는데, 제 Redis `leaderboard:mmr`가 정확히 그 역할을 이미 하고 있습니다. 크로스 샤드 트랜잭션(유저 간 거래)은 2PC가 비싸서 사가(보상 트랜잭션) + 멱등 재시도로 설계합니다. **순서가 중요합니다**: 인덱스/쿼리 튜닝 → 읽기 복제본 → 캐시 → 파티셔닝 → 그래도 안 되면 샤딩. 샤딩은 운영 복잡도(리샤딩, 라우팅, 백업)를 영구히 떠안는 최후 수단입니다. **꼬리 대비**: 리샤딩 → consistent hashing이나 디렉터리(라우팅 테이블) 방식으로 이동량을 최소화한다고 답합니다.

### Q30. 게임 서버에서 DB 쿼리를 어떻게 처리해야 게임 루프가 안 멈춥니까? (DB 워커 스레드)

**답**: 틱 루프의 프레임 예산(60Hz면 16ms)에 비해 DB 왕복은 수 ms에서 락 대기 시 수십 ms까지 가므로, 틱 스레드에서 동기 호출하면 전체 유저의 시뮬레이션이 멈춥니다. 구조는 — 틱 스레드는 요청을 큐에 넣기만 하고, 전용 DB 워커 스레드(들)가 큐에서 꺼내 블로킹 쿼리를 실행하며, 완료 결과는 결과 큐로 돌려보내 **틱 스레드가 프레임 경계에서 드레인**합니다. 규칙 세 가지: (1) 게임 상태 변경은 반드시 틱 스레드에서만(콜백을 워커에서 실행하면 락 지옥), (2) 같은 유저의 요청은 같은 워커로 해시 라우팅해 순서 보장(차감→지급 역전 방지), (3) 결과 대기 중 해당 유저의 관련 행동은 상태로 잠급니다("구매 처리 중"). 저는 이 패턴을 실제로 구현해 봤습니다 — `Client/Private/Network/Backend/CHttpClient.cpp`가 동형입니다: `std::async` 워커에서 WinHTTP 블로킹 요청을 실행하고, 결과 콜백을 mutex 보호 큐에 push, 게임 루프가 매 프레임 `ProcessCallbacks()`로 스왑-드레인해 **콜백이 항상 메인 스레드에서** 돕니다. 이 과정에서 실전 버그도 겪었습니다 — `std::async`의 반환 future를 버리면 임시 future의 소멸자가 완료를 기다려 **비동기 함수가 몰래 동기가 되는** 함정으로, future 수명을 멤버 컨테이너가 소유하고 소멸자에서 드레인하도록 고쳤습니다(파일 주석에 박제). 또 워커가 멤버를 직접 읽지 않고 호출 시점 스냅샷 복사본만 읽게 해 SetAuthToken과의 레이스를 차단했습니다 — DB 워커에 파라미터를 값으로 넘기는 원칙과 같습니다. **꼬리 대비**: "그럼 결제처럼 응답이 꼭 필요한 건?" → 비동기여도 유저 단위로는 요청-응답 상태 머신으로 직렬화하고, 절대 유실되면 안 되는 쓰기는 큐를 메모리로만 두지 말고 재시도 가능한 형태(멱등 키)로 설계한다고 답합니다.

### Q31. 커넥션 풀은 왜 필요하고, 크기는 어떻게 정합니까?

**답**: DB 커넥션 수립은 TCP 핸드셰이크 + 인증 + 서버측 프로세스/메모리 할당으로 수~수십 ms라, 요청마다 만들면 연결 비용이 쿼리 비용을 압도합니다. 또 Postgres `max_connections`(기본 100) 같은 상한을 폭주 트래픽이 뚫으면 신규 연결이 거부됩니다. 그래서 미리 N개를 만들어 대여-반납합니다. 제 설정(`Services/pkg/database/postgres.go`)은 pgxpool로 MaxConns/MinConns=2에 **MaxConnLifetime 30분**(LB/서버 재시작·오래된 연결 순환), MaxConnIdleTime 5분, HealthCheckPeriod 30초를 걸었고 Redis도 PoolSize 50입니다(`pkg/cache/redis.go`). **사이징**: 클수록 좋지 않습니다 — DB가 실제 동시 처리 가능한 작업 수는 코어 수 기반(경험식 `코어×2 + 스핀들 수`)이고, 과대 풀은 DB 내부 락 경합과 컨텍스트 스위칭만 늘립니다. 또 "서비스 인스턴스 수 × 풀 크기 ≤ max_connections"를 항상 검산해야 합니다. **꼬리 대비**: 풀 고갈의 주범은 트랜잭션을 열어둔 채 외부 호출을 하거나 커넥션을 반납 안 하는 누수 코드 — 대여 타임아웃과 트랜잭션 최소화로 방어한다고 답합니다.

### Q32. ORM의 장단점을 말하고, 본인 프로젝트에서는 왜 그 선택을 했습니까?

**답**: 장점은 CRUD 보일러플레이트 제거, 파라미터 바인딩이 기본이라 SQL 인젝션 방어, DB 벤더 추상화, 도메인 객체 중심 코드와 마이그레이션 도구 연계입니다. 단점은 (1) lazy loading이 루프에서 행마다 쿼리를 쏘는 N+1, (2) 생성 SQL이 불투명해 실행계획 튜닝이 어렵고, (3) FOR UPDATE·ON CONFLICT·RETURNING·윈도우 함수 같은 정밀 제어는 결국 raw SQL로 내려가야 한다는 점입니다. 제 백엔드는 ORM 없이 **pgx/v5 + raw SQL**입니다(모든 repository가 SQL 직서술, `.md/plan/backend/00_BACKEND_PLAN_INDEX.md`에 스택 명시). 이유는 재화 트랜잭션이 FOR UPDATE와 upsert, RETURNING을 정확히 제어해야 하고, 리뷰 시 쿼리가 코드에 그대로 보여야 하기 때문입니다. 다만 도구 선택은 워크로드 문제라서, CRUD 화면이 많은 어드민 툴이면 ORM 생산성이 이긴다고 균형을 맞춥니다. **결정적 한 방**: ORM을 안 썼는데도 제 코드에 N+1이 있습니다 — 랭킹 Top N에서 유저 상세를 행마다 QueryRow 하는 부분(`Services/internal/leaderboard/repository.go:GetTop`)으로, `WHERE id = ANY($1)` 배치 조회로 고치는 게 맞습니다. "N+1은 ORM의 죄가 아니라 접근 패턴의 죄"라는 걸 자기 코드로 증명할 수 있습니다.

### Q33. N+1 문제를 설명하고 해결 방법을 말해 보세요.

**답**: 목록 1번 쿼리 후 각 행의 연관 데이터를 N번 추가 쿼리하는 패턴입니다. 100행이면 101 쿼리 — 각각은 빠르지만 네트워크 왕복 × N이 쌓여 목록 화면이 수백 ms로 느려집니다. ORM lazy loading에서 흔하지만 raw SQL에서도 똑같이 만들어집니다. 실물로, 제 랭킹 Top N 구현이 Redis에서 N명을 받은 뒤 루프에서 유저마다 `SELECT username, wins, losses`를 날립니다(`Services/internal/leaderboard/repository.go:GetTop`). **해결**: (1) JOIN으로 한 방에, (2) ID 목록 배치 조회 — `WHERE u.id = ANY($1)` 후 애플리케이션에서 맵 조인(제 케이스의 정답), (3) ORM이면 eager loading(fetch join/preload) 명시, (4) 데이터 지역성 자체를 바꾸기 — 랭킹처럼 조회 전용이면 표시용 필드(username)를 Redis 값에 함께 인코딩해 DB 왕복을 0으로. **탐지**: 개발 단계에서 쿼리 로그의 동일 패턴 반복을 모니터링하고, APM에서 "요청당 쿼리 수" 지표를 봅니다. **꼬리 대비**: "JOIN이 항상 이기나요?" → 1:N 다중 조인은 행 뻥튀기로 오히려 전송량이 폭발할 수 있어 배치 분리 조회가 나을 때도 있다고 답합니다.

### Q34. 매치 결과(전적, MMR)를 DB에 반영하는 파이프라인을 설계해 보세요. 게임 서버가 DB를 직접 쓰면 안 됩니까?

**답**: 직접 써도 되지만 두 가지 문제가 있습니다 — 매치 종료 순간 스파이크가 DB에 직결되고, DB 장애가 게임 서버 루프를 오염시킵니다. 그래서 이벤트 기반으로 분리합니다. 제 구현: 매치 종료 → Kafka `MatchCompleted` 이벤트 발행 → Profile 서비스 컨슈머(`Services/internal/profile/consumer.go`)가 소비해 플레이어별로 match_history INSERT → player_stats 누적 UPDATE → 프로필 캐시 무효화를 수행합니다. 게임 서버는 발행만 하고 즉시 다음 틱으로 — 전적 반영은 최종 일관성(eventual consistency)이고, 유저 관점에선 몇 초 내 수렴이라 허용됩니다. 이 구조의 이점은 (1) 버퍼링 — 스파이크가 컨슈머 처리 속도로 평탄화, (2) 장애 격리 — DB가 죽어도 이벤트는 브로커에 남아 재소비로 복구, (3) 확장 — 같은 이벤트를 랭킹 서비스도 병렬 소비. **주의점을 스스로 짚습니다**: 재소비 시 중복 INSERT가 가능하므로 컨슈머는 멱등해야 합니다 — `(match_id, user_id)` UNIQUE 제약이 정석인데 현재 제 스키마는 인덱스만 있고 UNIQUE가 없어 보강 대상입니다(`Services/migrations/000004`). 또 history INSERT와 stats UPDATE가 별도 문장이라 부분 실패 시 불일치가 가능해, 두 쓰기를 한 트랜잭션으로 묶는 것도 개선점입니다. **꼬리 대비**: "즉시 반영이 필요한 건?" → 매치 보상 지급처럼 유저가 바로 확인하는 건 동기 트랜잭션, 통계·전적은 비동기 — 데이터별 SLA로 나눈다고 답합니다.

### Q35. DB 스키마를 어떻게 버전 관리합니까? (마이그레이션)

**답**: 스키마 변경을 순번이 매겨진 SQL 파일 쌍(up/down)으로 저장소에 커밋하고, 도구가 적용 이력 테이블로 현재 버전을 추적해 순서대로 적용합니다. 코드 리뷰가 가능해지고, 모든 환경(로컬/스테이징/운영)이 같은 경로로 같은 스키마에 도달하며, down 파일로 롤백 경로를 강제로 생각하게 됩니다. 제 레포가 이 구조입니다 — `Services/migrations/000001_create_users.up.sql`부터 `000007_create_inventory.up.sql`까지 각각 down 파일과 쌍으로 있고, 새 기능(친구 시스템)도 `000008_create_friendships.*`처럼 다음 번호로 계획돼 있습니다(`.md/plan/backend/00_BACKEND_PLAN_INDEX.md`). **운영 각주**: 라이브 서비스에선 (1) 파괴적 변경(컬럼 삭제/타입 변경)은 "추가 → 이중 쓰기 → 백필 → 전환 → 제거"의 다단계 expand-contract로, (2) 대형 테이블 인덱스 생성은 잠금을 피해 `CREATE INDEX CONCURRENTLY`로, (3) 마이그레이션과 코드 배포의 순서 호환성(구코드가 신스키마에서도 돌아야 함)을 지킨다고 답하면 신입 수준을 넘는 인상을 줍니다.

---

## 내 프로젝트 연결 포인트

면접에서 "해본 적 있어요?"에 파일 경로 단위로 답하기 위한 어필 문장 모음. 전부 레포 실물 기준.

1. **"재화 트랜잭션을 직접 설계·구현했습니다."** — 아이템 구매를 BEGIN → 가격 서버 조회 → 지갑 `FOR UPDATE` → 검증 → 차감 → 인벤토리 upsert(`ON CONFLICT DO UPDATE`) → append-only 원장 기록 → COMMIT으로 구현했고(`Services/internal/shop/repository.go:Purchase`), 동시 구매 경합·부분 실패·이중 차감 시나리오를 각각 어떤 장치가 막는지 설명할 수 있습니다.
2. **"멱등성을 UNIQUE 제약으로 보장했습니다."** — 결제에 `idempotency_key UNIQUE`(`Services/migrations/000005`) + 기존 거래 재응답(`Services/internal/payment/repository.go:FindByIdempotencyKey`)으로, 재시도 폭풍에도 중복 충전이 구조적으로 불가능합니다.
3. **"랭킹을 Redis sorted set으로 만들었고, DB를 원본으로 두는 복구 경로까지 설계했습니다."** — `ZADD/ZREVRANGE/ZREVRANK` + `SyncFromDB` 재적재(`Services/internal/leaderboard/repository.go`). 동시에 제 코드의 N+1과 이중 쓰기 불일치 창을 스스로 지적하고 수정안을 말할 수 있습니다 — 자기 코드 감사가 가능하다는 증거.
4. **"cache-aside를 TTL·무효화까지 구현했습니다."** — 프로필 캐시 `profile:{id}` 5분 TTL + 매치 종료 이벤트 시 무효화(`Services/internal/profile/repository.go`, `consumer.go`). "왜 캐시 업데이트가 아니라 삭제인가"를 레이스 관점으로 설명 가능.
5. **"게임 루프를 막지 않는 비동기 I/O 패턴을 C++로 직접 구현하고, 몰래 동기가 되는 버그까지 잡았습니다."** — 워커 스레드 + 메인 스레드 콜백 큐 드레인(`Client/Private/Network/Backend/CHttpClient.cpp:ProcessCallbacks`), `std::async` future 방치 = 동기화 함정 수정, 워커에 스냅샷 복사본만 넘겨 레이스 차단. DB 워커 스레드 질문에 이 경험을 그대로 매핑합니다.
6. **"스키마를 마이그레이션 파일로 버전 관리합니다."** — up/down SQL 7쌍(`Services/migrations/`), CHECK/UNIQUE/FK/복합 인덱스를 목적별로 배치. `users`의 중복 인덱스처럼 개선점도 파악하고 있습니다.
7. **"이벤트 기반 최종 일관성 파이프라인을 돌려봤습니다."** — 매치 종료 → Kafka → 컨슈머의 전적 INSERT + 스탯 누적 + 캐시 무효화(`Services/internal/profile/consumer.go`), 재소비 멱등성 보강 필요성까지 인지.
8. **"게임 서버 권위 원칙과 DB 설계를 연결해서 생각합니다."** — 클라이언트가 보낸 가격/좌표를 신뢰하지 않는 서버 권위 원칙(Winters 서버-권위 이동/스킬 파이프라인 경험)이 DB에선 "가격은 서버가 조회, 검증은 트랜잭션 안에서"로 이어진다고 말할 수 있습니다.

---

## 마지막 점검 체크리스트

면접 전날 1분 스캔용. 각 줄을 보고 30초 설명이 바로 나오면 통과.

- [ ] 조인 6종 + 1:N 조인의 행 뻥튀기 → 집계 왜곡 함정
- [ ] 실행 순서 FROM→WHERE→GROUP BY→HAVING→SELECT→ORDER BY, WHERE vs HAVING
- [ ] NOT IN + NULL = 공집합 함정, EXISTS 선호 이유
- [ ] B+tree: 페이지=노드, 팬아웃 수백 → 1억 행 = I/O 3~4회, 리프 연결 리스트 = 범위 스캔
- [ ] 해시 인덱스가 안 되는 것: 범위·정렬·prefix
- [ ] 클러스터드(InnoDB PK) vs 논클러스터드(2중 탐색) — Postgres는 전부 힙+논클러스터드
- [ ] 커버링 인덱스 = 북마크 룩업 제거, Postgres INCLUDE + visibility map 각주
- [ ] 인덱스 안 타는 8가지: 컬럼 가공 / %LIKE / 형변환 / leftmost 위반 / 낮은 선택도 / OR / 부정 / 낡은 통계
- [ ] 복합 인덱스: 동등 앞, 범위·정렬 뒤 — 내 실물: `(user_id, played_at DESC)`
- [ ] ACID: A=undo/버전, C=제약+앱, I=락/MVCC, D=WAL fsync
- [ ] 격리 4단계 × 이상현상 3종 표 암송 + Postgres RR=스냅샷(팬텀X, write skew O)
- [ ] dirty=미커밋 읽음 / non-repeatable=값 변함 / phantom=집합 변함
- [ ] MVCC: 읽기-쓰기 상호 비차단, 쓰기-쓰기는 여전히 락, VACUUM/bloat
- [ ] FOR UPDATE 구매 시퀀스 암송 (Purchase 6단계) + 원자 UPDATE 대안
- [ ] 데드락: 락 순서 통일로 예방, 감지되면 재시도
- [ ] 낙관(version 컬럼, 재시도) vs 비관(FOR UPDATE) 선택 기준 = 충돌률
- [ ] 1NF 원자값 / 2NF 부분 종속(inventory-shop_items 분리) / 3NF 이행 종속(guild)
- [ ] 비정규화 3종: 집계 컬럼(player_stats) / 원장 스냅샷(balance_after) / Redis 사본(+SyncFromDB)
- [ ] NoSQL 4분류 + CAP 한 줄, 데이터별로 C/A 다르게
- [ ] Redis: 인메모리+단일스레드(원자성)+자료구조, O(N) 명령 블로킹 함정
- [ ] sorted set = skip list + hash, ZADD/ZREVRANK O(log N), 동점 tie-break 인코딩
- [ ] cache-aside: 미스 적재+TTL, 쓰기는 삭제(업데이트 아님), 스탬피드 대응
- [ ] 세션: access는 무상태 JWT, refresh는 Redis jti(회수 가능) — 내 실물
- [ ] 중복 로그인: Redis session:{userId} 덮어쓰기 + SETNX 선점 + 나중 로그인 승리
- [ ] 멱등성: 클라 발급 키 + UNIQUE 제약이 최종 방어, 중복 요청엔 기존 응답 재반환
- [ ] 샤딩 키 = user_id 해시(유저 트랜잭션이 샤드 안에서 닫힘), 전역 집계는 Redis로
- [ ] 튜닝 순서: 인덱스 → 복제본 → 캐시 → 파티셔닝 → 샤딩 (샤딩 먼저 말하면 감점)
- [ ] DB 워커: 요청 큐 → 워커 블로킹 → 결과 큐 → 틱 스레드 드레인, 유저별 순서 보장 — CHttpClient 동형 + future 방치 버그 경험
- [ ] 커넥션 풀: 연결 비용 + max_connections, 과대 풀 역효과, 내 pgxpool 설정 수치
- [ ] ORM: N+1·불투명 SQL vs 생산성 — 나는 pgx raw SQL, 이유 = FOR UPDATE/upsert 정밀 제어
- [ ] 내 코드 자백 3종 세트: GetTop N+1 / users 중복 인덱스 / 랭킹 이중 쓰기 — 지적당하기 전에 먼저 말한다
