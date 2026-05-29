# Phase 0 -- Local Dev Environment & Shared Packages

> **Dependencies**: None (this is the foundation)
> **Estimated**: 2-3 days
> **Completion Criteria**: Docker Compose로 PostgreSQL+Redis+Kafka가 로컬에서 동작 + `go build ./...` 성공

---

## 0-A: Go Module

**File: `Services/go.mod`**
```go
module winters-backend

go 1.22

require (
    github.com/go-chi/chi/v5        v5.0.12
    github.com/golang-jwt/jwt/v5     v5.2.1
    github.com/jackc/pgx/v5          v5.5.5
    github.com/redis/go-redis/v9     v9.5.1
    github.com/segmentio/kafka-go     v0.4.47
    github.com/google/uuid            v1.6.0
    golang.org/x/crypto               v0.22.0
    github.com/joho/godotenv          v1.5.1
)
```

---

## 0-B: Docker Compose

**File: `Services/docker-compose.yml`**
```yaml
version: "3.9"

services:
  postgres:
    image: postgres:16-alpine
    container_name: winters-postgres
    environment:
      POSTGRES_USER: winters
      POSTGRES_PASSWORD: winters_dev_2026
      POSTGRES_DB: winters
    ports:
      - "5432:5432"
    volumes:
      - pgdata:/var/lib/postgresql/data
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U winters"]
      interval: 5s
      timeout: 3s
      retries: 5

  redis:
    image: redis:7-alpine
    container_name: winters-redis
    ports:
      - "6379:6379"
    command: redis-server --maxmemory 256mb --maxmemory-policy allkeys-lru
    healthcheck:
      test: ["CMD", "redis-cli", "ping"]
      interval: 5s
      timeout: 3s
      retries: 5

  kafka:
    image: bitnami/kafka:3.7
    container_name: winters-kafka
    environment:
      KAFKA_CFG_NODE_ID: 0
      KAFKA_CFG_PROCESS_ROLES: broker,controller
      KAFKA_CFG_CONTROLLER_QUORUM_VOTERS: 0@kafka:9093
      KAFKA_CFG_LISTENERS: PLAINTEXT://:9092,CONTROLLER://:9093
      KAFKA_CFG_ADVERTISED_LISTENERS: PLAINTEXT://localhost:9092
      KAFKA_CFG_LISTENER_SECURITY_PROTOCOL_MAP: CONTROLLER:PLAINTEXT,PLAINTEXT:PLAINTEXT
      KAFKA_CFG_CONTROLLER_LISTENER_NAMES: CONTROLLER
      KAFKA_CFG_AUTO_CREATE_TOPICS_ENABLE: "true"
    ports:
      - "9092:9092"
    healthcheck:
      test: ["CMD-SHELL", "kafka-topics.sh --bootstrap-server localhost:9092 --list"]
      interval: 10s
      timeout: 5s
      retries: 10

volumes:
  pgdata:
```

---

## 0-C: Environment Configuration

**File: `Services/.env.example`**
```
DB_HOST=localhost
DB_PORT=5432
DB_USER=winters
DB_PASSWORD=winters_dev_2026
DB_NAME=winters
DB_POOL_MAX=20
REDIS_ADDR=localhost:6379
REDIS_PASSWORD=
REDIS_DB=0
KAFKA_BROKERS=localhost:9092
JWT_ACCESS_SECRET=winters-access-secret-change-in-production
JWT_REFRESH_SECRET=winters-refresh-secret-change-in-production
JWT_ACCESS_TTL=1h
JWT_REFRESH_TTL=168h
AUTH_PORT=8081
LEADERBOARD_PORT=8082
MATCHMAKING_PORT=8083
PROFILE_PORT=8084
PAYMENT_PORT=8085
SHOP_PORT=8086
```

---

## 0-D: Config Package

**File: `Services/pkg/config/config.go`**
```go
package config

import (
	"os"
	"strconv"
	"time"

	"github.com/joho/godotenv"
)

type Config struct {
	DB          DatabaseConfig
	Redis       RedisConfig
	Kafka       KafkaConfig
	JWT         JWTConfig
	AuthPort    string
	LeaderPort  string
	MatchPort   string
	ProfilePort string
	PaymentPort string
	ShopPort    string
}

type DatabaseConfig struct {
	Host     string
	Port     int
	User     string
	Password string
	DBName   string
	PoolMax  int
}

type RedisConfig struct {
	Addr     string
	Password string
	DB       int
}

type KafkaConfig struct {
	Brokers []string
}

type JWTConfig struct {
	AccessSecret  string
	RefreshSecret string
	AccessTTL     time.Duration
	RefreshTTL    time.Duration
}

func Load() (*Config, error) {
	_ = godotenv.Load()

	dbPort, _ := strconv.Atoi(getEnv("DB_PORT", "5432"))
	poolMax, _ := strconv.Atoi(getEnv("DB_POOL_MAX", "20"))
	redisDB, _ := strconv.Atoi(getEnv("REDIS_DB", "0"))
	accessTTL, _ := time.ParseDuration(getEnv("JWT_ACCESS_TTL", "1h"))
	refreshTTL, _ := time.ParseDuration(getEnv("JWT_REFRESH_TTL", "168h"))

	return &Config{
		DB: DatabaseConfig{
			Host:     getEnv("DB_HOST", "localhost"),
			Port:     dbPort,
			User:     getEnv("DB_USER", "winters"),
			Password: getEnv("DB_PASSWORD", "winters_dev_2026"),
			DBName:   getEnv("DB_NAME", "winters"),
			PoolMax:  poolMax,
		},
		Redis: RedisConfig{
			Addr:     getEnv("REDIS_ADDR", "localhost:6379"),
			Password: getEnv("REDIS_PASSWORD", ""),
			DB:       redisDB,
		},
		Kafka: KafkaConfig{
			Brokers: []string{getEnv("KAFKA_BROKERS", "localhost:9092")},
		},
		JWT: JWTConfig{
			AccessSecret:  getEnv("JWT_ACCESS_SECRET", "winters-access-secret-change-in-production"),
			RefreshSecret: getEnv("JWT_REFRESH_SECRET", "winters-refresh-secret-change-in-production"),
			AccessTTL:     accessTTL,
			RefreshTTL:    refreshTTL,
		},
		AuthPort:    getEnv("AUTH_PORT", "8081"),
		LeaderPort:  getEnv("LEADERBOARD_PORT", "8082"),
		MatchPort:   getEnv("MATCHMAKING_PORT", "8083"),
		ProfilePort: getEnv("PROFILE_PORT", "8084"),
		PaymentPort: getEnv("PAYMENT_PORT", "8085"),
		ShopPort:    getEnv("SHOP_PORT", "8086"),
	}, nil
}

func (d DatabaseConfig) DSN() string {
	return "postgres://" + d.User + ":" + d.Password +
		"@" + d.Host + ":" + strconv.Itoa(d.Port) +
		"/" + d.DBName + "?sslmode=disable"
}

func getEnv(key, fallback string) string {
	if v := os.Getenv(key); v != "" {
		return v
	}
	return fallback
}
```

---

## 0-E: Database Package

**File: `Services/pkg/database/postgres.go`**
```go
package database

import (
	"context"
	"fmt"
	"time"

	"github.com/jackc/pgx/v5/pgxpool"
	"winters-backend/pkg/config"
)

func NewPool(ctx context.Context, cfg config.DatabaseConfig) (*pgxpool.Pool, error) {
	poolCfg, err := pgxpool.ParseConfig(cfg.DSN())
	if err != nil {
		return nil, fmt.Errorf("parse pool config: %w", err)
	}

	poolCfg.MaxConns = int32(cfg.PoolMax)
	poolCfg.MinConns = 2
	poolCfg.MaxConnLifetime = 30 * time.Minute
	poolCfg.MaxConnIdleTime = 5 * time.Minute
	poolCfg.HealthCheckPeriod = 30 * time.Second

	pool, err := pgxpool.NewWithConfig(ctx, poolCfg)
	if err != nil {
		return nil, fmt.Errorf("create pool: %w", err)
	}

	if err := pool.Ping(ctx); err != nil {
		pool.Close()
		return nil, fmt.Errorf("ping database: %w", err)
	}

	return pool, nil
}
```

---

## 0-F: Cache Package

**File: `Services/pkg/cache/redis.go`**
```go
package cache

import (
	"context"
	"fmt"

	"github.com/redis/go-redis/v9"
	"winters-backend/pkg/config"
)

func NewClient(ctx context.Context, cfg config.RedisConfig) (*redis.Client, error) {
	rdb := redis.NewClient(&redis.Options{
		Addr:     cfg.Addr,
		Password: cfg.Password,
		DB:       cfg.DB,
		PoolSize: 50,
	})

	if err := rdb.Ping(ctx).Err(); err != nil {
		return nil, fmt.Errorf("ping redis: %w", err)
	}

	return rdb, nil
}
```

---

## 0-G: Messaging Package

**File: `Services/pkg/messaging/kafka.go`**
```go
package messaging

import (
	"context"
	"log/slog"
	"time"

	"github.com/segmentio/kafka-go"
)

const (
	TopicPaymentEvents = "payment-events"
	TopicMatchEvents   = "match-events"
	TopicPlayerEvents  = "player-events"
)

func NewWriter(brokers []string, topic string) *kafka.Writer {
	return &kafka.Writer{
		Addr:         kafka.TCP(brokers...),
		Topic:        topic,
		Balancer:     &kafka.LeastBytes{},
		BatchTimeout: 10 * time.Millisecond,
		RequiredAcks: kafka.RequireOne,
	}
}

func NewReader(brokers []string, topic, groupID string) *kafka.Reader {
	return kafka.NewReader(kafka.ReaderConfig{
		Brokers:        brokers,
		Topic:          topic,
		GroupID:        groupID,
		MinBytes:       1,
		MaxBytes:       10e6,
		CommitInterval: time.Second,
		StartOffset:    kafka.LastOffset,
	})
}

func Consume(ctx context.Context, reader *kafka.Reader, handler func(ctx context.Context, msg kafka.Message) error) {
	for {
		msg, err := reader.ReadMessage(ctx)
		if err != nil {
			if ctx.Err() != nil {
				return
			}
			slog.Error("kafka read error", "error", err)
			time.Sleep(time.Second)
			continue
		}
		if err := handler(ctx, msg); err != nil {
			slog.Error("kafka handler error", "topic", msg.Topic, "offset", msg.Offset, "error", err)
		}
	}
}
```

---

## 0-H: JWT Package

**File: `Services/pkg/auth/jwt.go`**
```go
package auth

import (
	"fmt"
	"time"

	"github.com/golang-jwt/jwt/v5"
	"github.com/google/uuid"
)

type TokenPair struct {
	AccessToken  string `json:"access_token"`
	RefreshToken string `json:"refresh_token"`
	ExpiresAt    int64  `json:"expires_at"`
}

type Claims struct {
	jwt.RegisteredClaims
	UserID   uuid.UUID `json:"user_id"`
	Username string    `json:"username"`
}

type JWTManager struct {
	accessSecret  []byte
	refreshSecret []byte
	accessTTL     time.Duration
	refreshTTL    time.Duration
}

func NewJWTManager(accessSecret, refreshSecret string, accessTTL, refreshTTL time.Duration) *JWTManager {
	return &JWTManager{
		accessSecret:  []byte(accessSecret),
		refreshSecret: []byte(refreshSecret),
		accessTTL:     accessTTL,
		refreshTTL:    refreshTTL,
	}
}

func (m *JWTManager) RefreshTTL() time.Duration { return m.refreshTTL }

func (m *JWTManager) GenerateTokenPair(userID uuid.UUID, username string) (*TokenPair, error) {
	now := time.Now()

	accessClaims := Claims{
		RegisteredClaims: jwt.RegisteredClaims{
			Subject:   userID.String(),
			IssuedAt:  jwt.NewNumericDate(now),
			ExpiresAt: jwt.NewNumericDate(now.Add(m.accessTTL)),
			ID:        uuid.New().String(),
		},
		UserID:   userID,
		Username: username,
	}
	accessToken, err := jwt.NewWithClaims(jwt.SigningMethodHS256, accessClaims).SignedString(m.accessSecret)
	if err != nil {
		return nil, fmt.Errorf("sign access token: %w", err)
	}

	refreshClaims := jwt.RegisteredClaims{
		Subject:   userID.String(),
		IssuedAt:  jwt.NewNumericDate(now),
		ExpiresAt: jwt.NewNumericDate(now.Add(m.refreshTTL)),
		ID:        uuid.New().String(),
	}
	refreshToken, err := jwt.NewWithClaims(jwt.SigningMethodHS256, refreshClaims).SignedString(m.refreshSecret)
	if err != nil {
		return nil, fmt.Errorf("sign refresh token: %w", err)
	}

	return &TokenPair{
		AccessToken:  accessToken,
		RefreshToken: refreshToken,
		ExpiresAt:    now.Add(m.accessTTL).Unix(),
	}, nil
}

func (m *JWTManager) ValidateAccessToken(tokenStr string) (*Claims, error) {
	token, err := jwt.ParseWithClaims(tokenStr, &Claims{}, func(t *jwt.Token) (interface{}, error) {
		if _, ok := t.Method.(*jwt.SigningMethodHMAC); !ok {
			return nil, fmt.Errorf("unexpected signing method: %v", t.Header["alg"])
		}
		return m.accessSecret, nil
	})
	if err != nil {
		return nil, fmt.Errorf("parse access token: %w", err)
	}
	claims, ok := token.Claims.(*Claims)
	if !ok || !token.Valid {
		return nil, fmt.Errorf("invalid access token claims")
	}
	return claims, nil
}

func (m *JWTManager) ValidateRefreshToken(tokenStr string) (*jwt.RegisteredClaims, error) {
	token, err := jwt.ParseWithClaims(tokenStr, &jwt.RegisteredClaims{}, func(t *jwt.Token) (interface{}, error) {
		if _, ok := t.Method.(*jwt.SigningMethodHMAC); !ok {
			return nil, fmt.Errorf("unexpected signing method: %v", t.Header["alg"])
		}
		return m.refreshSecret, nil
	})
	if err != nil {
		return nil, fmt.Errorf("parse refresh token: %w", err)
	}
	claims, ok := token.Claims.(*jwt.RegisteredClaims)
	if !ok || !token.Valid {
		return nil, fmt.Errorf("invalid refresh token claims")
	}
	return claims, nil
}
```

---

## 0-I: Middleware Package

**File: `Services/pkg/middleware/auth.go`**
```go
package middleware

import (
	"context"
	"net/http"
	"strings"

	jwtauth "winters-backend/pkg/auth"
	"winters-backend/pkg/response"
)

type contextKey string

const UserClaimsKey contextKey = "user_claims"

func JWTAuth(jwtMgr *jwtauth.JWTManager) func(http.Handler) http.Handler {
	return func(next http.Handler) http.Handler {
		return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			authHeader := r.Header.Get("Authorization")
			if authHeader == "" {
				response.Error(w, http.StatusUnauthorized, "missing authorization header")
				return
			}
			parts := strings.SplitN(authHeader, " ", 2)
			if len(parts) != 2 || parts[0] != "Bearer" {
				response.Error(w, http.StatusUnauthorized, "invalid authorization format")
				return
			}
			claims, err := jwtMgr.ValidateAccessToken(parts[1])
			if err != nil {
				response.Error(w, http.StatusUnauthorized, "invalid token")
				return
			}
			ctx := context.WithValue(r.Context(), UserClaimsKey, claims)
			next.ServeHTTP(w, r.WithContext(ctx))
		})
	}
}

func GetClaims(ctx context.Context) *jwtauth.Claims {
	claims, _ := ctx.Value(UserClaimsKey).(*jwtauth.Claims)
	return claims
}
```

**File: `Services/pkg/middleware/logging.go`**
```go
package middleware

import (
	"log/slog"
	"net/http"
	"time"
)

type wrappedWriter struct {
	http.ResponseWriter
	statusCode int
}

func (w *wrappedWriter) WriteHeader(code int) {
	w.statusCode = code
	w.ResponseWriter.WriteHeader(code)
}

func Logging(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		start := time.Now()
		wrapped := &wrappedWriter{ResponseWriter: w, statusCode: http.StatusOK}
		next.ServeHTTP(wrapped, r)
		slog.Info("http request",
			"method", r.Method,
			"path", r.URL.Path,
			"status", wrapped.statusCode,
			"duration_ms", time.Since(start).Milliseconds(),
		)
	})
}
```

**File: `Services/pkg/middleware/recovery.go`**
```go
package middleware

import (
	"log/slog"
	"net/http"
	"runtime/debug"

	"winters-backend/pkg/response"
)

func Recovery(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		defer func() {
			if err := recover(); err != nil {
				slog.Error("panic recovered", "error", err, "stack", string(debug.Stack()))
				response.Error(w, http.StatusInternalServerError, "internal server error")
			}
		}()
		next.ServeHTTP(w, r)
	})
}
```

---

## 0-J: Response Package

**File: `Services/pkg/response/json.go`**
```go
package response

import (
	"encoding/json"
	"net/http"
)

type APIResponse struct {
	Success bool        `json:"success"`
	Data    interface{} `json:"data,omitempty"`
	Error   string      `json:"error,omitempty"`
}

func JSON(w http.ResponseWriter, status int, data interface{}) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(status)
	json.NewEncoder(w).Encode(APIResponse{Success: true, Data: data})
}

func Error(w http.ResponseWriter, status int, msg string) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(status)
	json.NewEncoder(w).Encode(APIResponse{Success: false, Error: msg})
}
```

---

## 0-K: Errors Package

**File: `Services/pkg/errors/errors.go`**
```go
package errors

import "errors"

var (
	ErrNotFound            = errors.New("resource not found")
	ErrAlreadyExists       = errors.New("resource already exists")
	ErrInvalidInput        = errors.New("invalid input")
	ErrUnauthorized        = errors.New("unauthorized")
	ErrForbidden           = errors.New("forbidden")
	ErrInsufficientBalance = errors.New("insufficient balance")
	ErrIdempotencyConflict = errors.New("idempotency conflict")
)
```

---

## 0-L: Database Migrations

**File: `Services/migrations/000001_create_users.up.sql`**
```sql
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

CREATE TABLE users (
    id          UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    username    VARCHAR(32)  NOT NULL UNIQUE,
    email       VARCHAR(255) NOT NULL UNIQUE,
    password    VARCHAR(255) NOT NULL,
    created_at  TIMESTAMPTZ  NOT NULL DEFAULT NOW(),
    updated_at  TIMESTAMPTZ  NOT NULL DEFAULT NOW()
);

CREATE INDEX idx_users_username ON users(username);
CREATE INDEX idx_users_email    ON users(email);
```

**File: `Services/migrations/000001_create_users.down.sql`**
```sql
DROP TABLE IF EXISTS users CASCADE;
```

**File: `Services/migrations/000002_create_wallets.up.sql`**
```sql
CREATE TABLE wallets (
    id          UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id     UUID NOT NULL UNIQUE REFERENCES users(id) ON DELETE CASCADE,
    balance     BIGINT NOT NULL DEFAULT 0 CHECK (balance >= 0),
    created_at  TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at  TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
```

**File: `Services/migrations/000002_create_wallets.down.sql`**
```sql
DROP TABLE IF EXISTS wallets CASCADE;
```

**File: `Services/migrations/000003_create_player_stats.up.sql`**
```sql
CREATE TABLE player_stats (
    id          UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id     UUID NOT NULL UNIQUE REFERENCES users(id) ON DELETE CASCADE,
    mmr         INT  NOT NULL DEFAULT 1000,
    wins        INT  NOT NULL DEFAULT 0,
    losses      INT  NOT NULL DEFAULT 0,
    kills       INT  NOT NULL DEFAULT 0,
    deaths      INT  NOT NULL DEFAULT 0,
    assists     INT  NOT NULL DEFAULT 0,
    updated_at  TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX idx_player_stats_mmr ON player_stats(mmr DESC);
```

**File: `Services/migrations/000003_create_player_stats.down.sql`**
```sql
DROP TABLE IF EXISTS player_stats CASCADE;
```

**File: `Services/migrations/000004_create_match_history.up.sql`**
```sql
CREATE TABLE match_history (
    id          UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id     UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    match_id    UUID NOT NULL,
    result      VARCHAR(10) NOT NULL CHECK (result IN ('win', 'loss', 'draw')),
    kills       INT NOT NULL DEFAULT 0,
    deaths      INT NOT NULL DEFAULT 0,
    assists     INT NOT NULL DEFAULT 0,
    mmr_change  INT NOT NULL DEFAULT 0,
    played_at   TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX idx_match_history_user  ON match_history(user_id, played_at DESC);
CREATE INDEX idx_match_history_match ON match_history(match_id);
```

**File: `Services/migrations/000004_create_match_history.down.sql`**
```sql
DROP TABLE IF EXISTS match_history CASCADE;
```

**File: `Services/migrations/000005_create_payment_transactions.up.sql`**
```sql
CREATE TABLE payment_transactions (
    id              UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id         UUID NOT NULL REFERENCES users(id),
    idempotency_key VARCHAR(64) NOT NULL UNIQUE,
    gateway         VARCHAR(20) NOT NULL,
    gateway_tx_id   VARCHAR(255),
    amount          BIGINT NOT NULL CHECK (amount > 0),
    currency        VARCHAR(3) NOT NULL DEFAULT 'KRW',
    coin_amount     BIGINT NOT NULL CHECK (coin_amount > 0),
    status          VARCHAR(20) NOT NULL DEFAULT 'pending'
                    CHECK (status IN ('pending', 'completed', 'failed', 'refunded')),
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    completed_at    TIMESTAMPTZ
);

CREATE INDEX idx_payment_tx_user  ON payment_transactions(user_id, created_at DESC);
CREATE INDEX idx_payment_tx_idemp ON payment_transactions(idempotency_key);
```

**File: `Services/migrations/000005_create_payment_transactions.down.sql`**
```sql
DROP TABLE IF EXISTS payment_transactions CASCADE;
```

**File: `Services/migrations/000006_create_coin_transactions.up.sql`**
```sql
CREATE TABLE coin_transactions (
    id            UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id       UUID NOT NULL REFERENCES users(id),
    amount        BIGINT NOT NULL,
    tx_type       VARCHAR(20) NOT NULL CHECK (tx_type IN ('charge', 'purchase', 'refund')),
    reference     VARCHAR(255),
    balance_after BIGINT NOT NULL,
    created_at    TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX idx_coin_tx_user ON coin_transactions(user_id, created_at DESC);
```

**File: `Services/migrations/000006_create_coin_transactions.down.sql`**
```sql
DROP TABLE IF EXISTS coin_transactions CASCADE;
```

**File: `Services/migrations/000007_create_inventory.up.sql`**
```sql
CREATE TABLE shop_items (
    id          UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    name        VARCHAR(100) NOT NULL,
    description TEXT,
    item_type   VARCHAR(30) NOT NULL,
    price       BIGINT NOT NULL CHECK (price > 0),
    is_active   BOOLEAN NOT NULL DEFAULT true,
    metadata    JSONB,
    created_at  TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TABLE inventory (
    id          UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id     UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    item_id     UUID NOT NULL REFERENCES shop_items(id),
    quantity    INT NOT NULL DEFAULT 1 CHECK (quantity > 0),
    acquired_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    UNIQUE(user_id, item_id)
);

CREATE INDEX idx_inventory_user ON inventory(user_id);
```

**File: `Services/migrations/000007_create_inventory.down.sql`**
```sql
DROP TABLE IF EXISTS inventory CASCADE;
DROP TABLE IF EXISTS shop_items CASCADE;
```

---

## 0-M: Makefile

**File: `Services/Makefile`**
```makefile
.PHONY: up down migrate auth leaderboard matchmaking profile payment shop

up:
	docker compose up -d

down:
	docker compose down

migrate:
	@for f in migrations/*.up.sql; do \
		echo "Applying $$f"; \
		PGPASSWORD=winters_dev_2026 psql -h localhost -U winters -d winters -f $$f; \
	done

auth:
	go run ./cmd/auth
leaderboard:
	go run ./cmd/leaderboard
matchmaking:
	go run ./cmd/matchmaking
profile:
	go run ./cmd/profile
payment:
	go run ./cmd/payment
shop:
	go run ./cmd/shop
```

---

## Phase 0 Verification Checklist

```
[ ] docker compose up -d -- all 3 containers healthy
[ ] Run migrations -- 8 tables created (users, wallets, player_stats, match_history, payment_transactions, coin_transactions, shop_items, inventory)
[ ] go build ./... compiles with zero errors
[ ] go vet ./... passes
[ ] psql -h localhost -U winters -d winters -c "\dt" -- shows all tables
[ ] redis-cli PING -- PONG
```
