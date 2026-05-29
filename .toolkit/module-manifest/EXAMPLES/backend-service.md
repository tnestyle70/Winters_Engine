# Auth Service Module

> **EXAMPLE** — 백엔드 microservice 의 Auth service 예시.
> Winters Services 의 `Services/internal/auth/` 를 기반으로 작성.
> 회사 코드베이스의 동급 service (Auth / Payment / User 등) 에 비슷하게 적용 가능.

---

## 책임 (Responsibility)

JWT 기반 사용자 인증 + 토큰 발급 / 갱신 / 무효화. PostgreSQL 의 `users` 테이블 + Redis 의 refresh token 저장.
Public HTTP API 4 endpoint (`/auth/register`, `/login`, `/refresh`, `/logout`).
Kafka `UserRegistered` event 발행 (downstream Profile service 가 consumer).

---

## 진입점 (Entry Points)

- `POST /auth/register` at `handler/register.go:42` — 신규 가입
- `POST /auth/login` at `handler/login.go:31` — 로그인 + Access/Refresh JWT 발급
- `POST /auth/refresh` at `handler/refresh.go:28` — Refresh token 으로 Access 갱신
- `POST /auth/logout` at `handler/logout.go:19` — Refresh token 무효화
- `func NewAuthService(...)` at `service/service.go:15` — DI 진입점 (cmd/auth/main.go 가 호출)
- `func (s *AuthService) ValidateToken(token string)` at `service/jwt.go:67` — 다른 service 가 호출하는 internal API

---

## 의존성 (Dependencies)

### Public (외부 service / 라이브러리 — 헤더/인터페이스 노출)

- `pkg/database` — PostgreSQL 연결 풀
- `pkg/cache` — Redis 클라이언트
- `pkg/messaging` — Kafka producer
- `pkg/auth` — JWT 라이브러리 (HS256)
- `pkg/middleware` — HTTP 미들웨어 (logging, CORS, rate limit)

### Private (구현 detail)

- `internal/auth/repository/` — DB 쿼리 implementation (다른 service 사용 X)
- `internal/auth/model/` — Go struct (수신 / 응답 payload)
- `pkg/errors` — 에러 코드 통합
- `pkg/response` — JSON 응답 포맷

### Forward-Decl Only (interface 만)

- (Go 는 implicit interface — 명시 안 해도 됨. 단 큰 interface 는 별도 박제)

---

## 의존받음 (Depended By)

- `Services/internal/profile/` — `ValidateToken` 호출 (인증 검증)
- `Services/internal/shop/` — `ValidateToken` 호출
- `Services/internal/payment/` — `ValidateToken` 호출
- `Client/Network/AuthClient.cpp` (게임 클라이언트) — HTTP API 호출
- Kafka topic `user.registered` consumer: profile / shop service

---

## Common Tasks (AI 매핑)

- "신규 인증 방식 (예: OAuth)" → `service/oauth.go` 신규 + `handler/oauth.go` endpoint + `cmd/auth/main.go` 라우팅 등록
- "토큰 만료 시간 변경" → `service/jwt.go` 의 `accessTokenTTL` / `refreshTokenTTL` 상수 + 환경변수 `JWT_ACCESS_TTL` / `JWT_REFRESH_TTL` 추가
- "rate limit 강화" → `pkg/middleware/ratelimit.go` 의 `LoginAttempts` 변수 + Redis key 패턴 변경
- "DB schema 마이그레이션" → `migrations/{timestamp}_{name}.up.sql` + `.down.sql` 박제 + `make migrate-up`
- "Kafka event 추가" → `internal/auth/events/` 에 event struct + `pkg/messaging` producer 호출 + Profile service 의 consumer 등록

---

## 함정 (Gotchas)

- **localhost IPv6 vs IPv4 충돌** — Go `net.Dial("tcp", "localhost:5432")` 가 IPv6 `[::1]` 우선 시도. Docker container 가 IPv4 만 listening → 연결 실패
  / 해결: `.env` 의 `DB_HOST=127.0.0.1` 명시
- **`godotenv` + 빈값 환경변수** — `.env` 의 `KEY=` (빈값) 이어도 `getEnv(key, fallback)` 이 빈 문자열 무시하고 fallback 반환. 빈값 의도 시 `os.Getenv` 직접 사용
- **JWT 비밀키 외부 노출** — Release 빌드의 binary 에 비밀키 hardcode 금지. `JWT_SECRET` 환경변수 + Vault / AWS Secrets Manager 경유
- **Refresh token rotation 누락** — refresh 시 새 token 발급 + 옛 token 즉시 무효화 안 하면 stolen token 영구 사용 가능
  / 해결: Redis 에 `refresh:<jti>` key + TTL + `DEL` on rotation
- **DB 트랜잭션 누락** — `register` 가 user insert + audit log insert 두 단계인데 트랜잭션 없으면 partial commit
  / 해결: `db.BeginTx` + defer rollback + commit on success

---

## 외부 노출 API (HTTP boundary)

### 외부 노출 (게임 클라이언트 / 외부 service 가 호출)

- `POST /auth/register` — 가입
- `POST /auth/login` — 로그인
- `POST /auth/refresh` — 갱신
- `POST /auth/logout` — 무효화
- `GET /health` — health check (k8s liveness)

### 내부 (다른 service 만 호출)

- gRPC `AuthService.ValidateToken(token)` — 다른 service 가 인증 검증
- gRPC `AuthService.GetUser(userId)` — Profile service 가 호출

---

## Service 메타

- **포트**: 8081
- **DB schema**: `users`, `refresh_tokens`, `audit_log` (3 테이블)
- **Kafka topics**: produce `user.registered`, `user.deleted`
- **환경변수**: `DB_HOST` `DB_PORT` `DB_USER` `DB_PASSWORD` `JWT_SECRET` `JWT_ACCESS_TTL` `JWT_REFRESH_TTL` `REDIS_URL` `KAFKA_BROKERS`
- **메모리 사용**: ~50MB 평균 / ~200MB 피크 (1000 RPS)

---

## 핵심 파일 (Top 5)

1. `cmd/auth/main.go` — 진입점 + DI + 라우팅 등록
2. `internal/auth/service/service.go` — 비즈니스 로직 (Register / Login / Refresh / Logout)
3. `internal/auth/service/jwt.go` — JWT 발급 + 검증
4. `internal/auth/repository/postgres.go` — DB 쿼리
5. `migrations/0001_create_users.up.sql` — schema

---

## 관련 계획서 / 문서

- `.md/plan/backend/01_AUTH_SERVICE_PLAN.md` — 초기 설계
- `.md/plan/backend/00_BACKEND_PLAN_INDEX.md` — 전체 service 목록
- 외부: https://jwt.io/ — JWT spec 참조

---

## 성능 특성

- Login throughput: ~5000 req/s (단일 instance, 4 core)
- p99 latency: 12ms (DB 쿼리 + JWT sign)
- DB connection pool: 20 (max), 5 (min)
- Redis ops: 2 / login (refresh token store + audit)

---

## 운영 (별도 섹션 — 백엔드 도메인 권장)

- **로그**: structured JSON → ELK
- **메트릭**: `/metrics` Prometheus endpoint (`auth_login_total`, `auth_token_validate_total`)
- **알림**: Login failure rate > 10% 5분 → PagerDuty
- **배포**: blue-green, k8s rolling update
