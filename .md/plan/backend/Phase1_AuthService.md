# Phase 1 -- Auth Service (Login/Registration)

> **Dependencies**: Phase 0 complete
> **Estimated**: 1 week
> **Completion Criteria**: 회원가입 -> 로그인 -> JWT 발급 -> 토큰으로 인증된 API 호출 -> 토큰 갱신 -> 로그아웃

---

## API Endpoints

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| POST | /auth/register | X | 회원가입 -> JWT pair 발급 |
| POST | /auth/login | X | 로그인 -> JWT pair 발급 |
| POST | /auth/refresh | X | Refresh token rotation |
| POST | /auth/logout | X | Refresh token 폐기 |
| GET | /health | X | 헬스체크 |

---

## Model

**File: `Services/internal/auth/model.go`**
```go
package auth

import (
	"time"
	"github.com/google/uuid"
)

type User struct {
	ID        uuid.UUID `json:"id"`
	Username  string    `json:"username"`
	Email     string    `json:"email"`
	Password  string    `json:"-"`
	CreatedAt time.Time `json:"created_at"`
	UpdatedAt time.Time `json:"updated_at"`
}

type RegisterRequest struct {
	Username string `json:"username"`
	Email    string `json:"email"`
	Password string `json:"password"`
}

type LoginRequest struct {
	Email    string `json:"email"`
	Password string `json:"password"`
}

type RefreshRequest struct {
	RefreshToken string `json:"refresh_token"`
}

type LogoutRequest struct {
	RefreshToken string `json:"refresh_token"`
}
```

---

## Repository

**File: `Services/internal/auth/repository.go`**

> Key Design: `CreateUserWithWalletAndStats`는 하나의 PostgreSQL 트랜잭션으로 users + wallets + player_stats를 원자적으로 생성한다.
> Refresh Token은 Redis에 TTL과 함께 저장한다.

```go
package auth

import (
	"context"
	"errors"
	"fmt"
	"time"

	"github.com/google/uuid"
	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgxpool"
	"github.com/redis/go-redis/v9"

	apperr "winters-backend/pkg/errors"
)

type Repository struct {
	db  *pgxpool.Pool
	rdb *redis.Client
}

func NewRepository(db *pgxpool.Pool, rdb *redis.Client) *Repository {
	return &Repository{db: db, rdb: rdb}
}

func (r *Repository) CreateUserWithWalletAndStats(ctx context.Context, user *User) error {
	tx, err := r.db.Begin(ctx)
	if err != nil {
		return fmt.Errorf("begin tx: %w", err)
	}
	defer tx.Rollback(ctx)

	err = tx.QueryRow(ctx,
		`INSERT INTO users (username, email, password)
		 VALUES ($1, $2, $3) RETURNING id, created_at, updated_at`,
		user.Username, user.Email, user.Password,
	).Scan(&user.ID, &user.CreatedAt, &user.UpdatedAt)
	if err != nil {
		return fmt.Errorf("insert user: %w", err)
	}

	_, err = tx.Exec(ctx, `INSERT INTO wallets (user_id, balance) VALUES ($1, 0)`, user.ID)
	if err != nil {
		return fmt.Errorf("insert wallet: %w", err)
	}

	_, err = tx.Exec(ctx, `INSERT INTO player_stats (user_id, mmr) VALUES ($1, 1000)`, user.ID)
	if err != nil {
		return fmt.Errorf("insert player_stats: %w", err)
	}

	return tx.Commit(ctx)
}

func (r *Repository) FindByEmail(ctx context.Context, email string) (*User, error) {
	var u User
	err := r.db.QueryRow(ctx,
		`SELECT id, username, email, password, created_at, updated_at
		 FROM users WHERE email = $1`, email,
	).Scan(&u.ID, &u.Username, &u.Email, &u.Password, &u.CreatedAt, &u.UpdatedAt)
	if err != nil {
		if errors.Is(err, pgx.ErrNoRows) {
			return nil, apperr.ErrNotFound
		}
		return nil, fmt.Errorf("find by email: %w", err)
	}
	return &u, nil
}

func (r *Repository) FindByID(ctx context.Context, id uuid.UUID) (*User, error) {
	var u User
	err := r.db.QueryRow(ctx,
		`SELECT id, username, email, password, created_at, updated_at
		 FROM users WHERE id = $1`, id,
	).Scan(&u.ID, &u.Username, &u.Email, &u.Password, &u.CreatedAt, &u.UpdatedAt)
	if err != nil {
		if errors.Is(err, pgx.ErrNoRows) {
			return nil, apperr.ErrNotFound
		}
		return nil, fmt.Errorf("find by id: %w", err)
	}
	return &u, nil
}

func (r *Repository) StoreRefreshToken(ctx context.Context, userID uuid.UUID, jti string, ttl time.Duration) error {
	return r.rdb.Set(ctx, "refresh:"+jti, userID.String(), ttl).Err()
}

func (r *Repository) DeleteRefreshToken(ctx context.Context, jti string) error {
	return r.rdb.Del(ctx, "refresh:"+jti).Err()
}

func (r *Repository) IsRefreshTokenValid(ctx context.Context, jti string) (bool, error) {
	n, err := r.rdb.Exists(ctx, "refresh:"+jti).Result()
	if err != nil {
		return false, err
	}
	return n > 0, nil
}
```

---

## Service

**File: `Services/internal/auth/service.go`**

> Key Design:
> - `Register`: 입력 검증 -> bcrypt 해시 -> 트랜잭션(users+wallets+player_stats) -> JWT 발급
> - `Login`: email로 조회 -> bcrypt 비교 -> JWT 발급
> - `Refresh`: 기존 refresh token 검증(Redis) -> 폐기 -> 새 token pair 발급 (Token Rotation)
> - `Logout`: refresh token Redis에서 삭제

```go
package auth

import (
	"context"
	"errors"
	"fmt"
	"log/slog"
	"unicode/utf8"

	"github.com/google/uuid"
	"golang.org/x/crypto/bcrypt"

	jwtauth "winters-backend/pkg/auth"
	apperr "winters-backend/pkg/errors"
)

type Service struct {
	repo   *Repository
	jwtMgr *jwtauth.JWTManager
}

func NewService(repo *Repository, jwtMgr *jwtauth.JWTManager) *Service {
	return &Service{repo: repo, jwtMgr: jwtMgr}
}

func (s *Service) Register(ctx context.Context, req RegisterRequest) (*jwtauth.TokenPair, error) {
	if utf8.RuneCountInString(req.Username) < 2 || utf8.RuneCountInString(req.Username) > 32 {
		return nil, fmt.Errorf("%w: username must be 2-32 characters", apperr.ErrInvalidInput)
	}
	if len(req.Password) < 8 {
		return nil, fmt.Errorf("%w: password must be at least 8 characters", apperr.ErrInvalidInput)
	}

	hashed, err := bcrypt.GenerateFromPassword([]byte(req.Password), bcrypt.DefaultCost)
	if err != nil {
		return nil, fmt.Errorf("hash password: %w", err)
	}

	user := &User{Username: req.Username, Email: req.Email, Password: string(hashed)}
	if err := s.repo.CreateUserWithWalletAndStats(ctx, user); err != nil {
		return nil, fmt.Errorf("create user: %w", err)
	}

	slog.Info("user registered", "user_id", user.ID, "username", user.Username)
	return s.issueTokens(ctx, user.ID, user.Username)
}

func (s *Service) Login(ctx context.Context, req LoginRequest) (*jwtauth.TokenPair, error) {
	user, err := s.repo.FindByEmail(ctx, req.Email)
	if err != nil {
		if errors.Is(err, apperr.ErrNotFound) {
			return nil, fmt.Errorf("%w: invalid credentials", apperr.ErrUnauthorized)
		}
		return nil, err
	}
	if err := bcrypt.CompareHashAndPassword([]byte(user.Password), []byte(req.Password)); err != nil {
		return nil, fmt.Errorf("%w: invalid credentials", apperr.ErrUnauthorized)
	}
	slog.Info("user logged in", "user_id", user.ID)
	return s.issueTokens(ctx, user.ID, user.Username)
}

func (s *Service) Refresh(ctx context.Context, req RefreshRequest) (*jwtauth.TokenPair, error) {
	claims, err := s.jwtMgr.ValidateRefreshToken(req.RefreshToken)
	if err != nil {
		return nil, fmt.Errorf("%w: invalid refresh token", apperr.ErrUnauthorized)
	}
	valid, err := s.repo.IsRefreshTokenValid(ctx, claims.ID)
	if err != nil {
		return nil, fmt.Errorf("check refresh token: %w", err)
	}
	if !valid {
		return nil, fmt.Errorf("%w: refresh token revoked", apperr.ErrUnauthorized)
	}
	_ = s.repo.DeleteRefreshToken(ctx, claims.ID)

	userID, _ := uuid.Parse(claims.Subject)
	user, err := s.repo.FindByID(ctx, userID)
	if err != nil {
		return nil, err
	}
	return s.issueTokens(ctx, user.ID, user.Username)
}

func (s *Service) Logout(ctx context.Context, req LogoutRequest) error {
	claims, err := s.jwtMgr.ValidateRefreshToken(req.RefreshToken)
	if err != nil {
		return nil
	}
	return s.repo.DeleteRefreshToken(ctx, claims.ID)
}

func (s *Service) issueTokens(ctx context.Context, userID uuid.UUID, username string) (*jwtauth.TokenPair, error) {
	pair, err := s.jwtMgr.GenerateTokenPair(userID, username)
	if err != nil {
		return nil, err
	}
	refreshClaims, _ := s.jwtMgr.ValidateRefreshToken(pair.RefreshToken)
	if err := s.repo.StoreRefreshToken(ctx, userID, refreshClaims.ID, s.jwtMgr.RefreshTTL()); err != nil {
		return nil, fmt.Errorf("store refresh token: %w", err)
	}
	return pair, nil
}
```

---

## Handler

**File: `Services/internal/auth/handler.go`**

> chi 라우터 패턴. 각 핸들러는 JSON 디코드 -> 서비스 호출 -> 서비스 에러를 HTTP 상태 코드로 매핑.

```go
package auth

import (
	"encoding/json"
	"errors"
	"net/http"

	"github.com/go-chi/chi/v5"
	apperr "winters-backend/pkg/errors"
	"winters-backend/pkg/response"
)

type Handler struct {
	svc *Service
}

func NewHandler(svc *Service) *Handler {
	return &Handler{svc: svc}
}

func (h *Handler) Routes() chi.Router {
	r := chi.NewRouter()
	r.Post("/register", h.Register)
	r.Post("/login", h.Login)
	r.Post("/refresh", h.Refresh)
	r.Post("/logout", h.Logout)
	return r
}

func (h *Handler) Register(w http.ResponseWriter, r *http.Request) {
	var req RegisterRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		response.Error(w, http.StatusBadRequest, "invalid request body")
		return
	}
	tokens, err := h.svc.Register(r.Context(), req)
	if err != nil {
		handleError(w, err)
		return
	}
	response.JSON(w, http.StatusCreated, tokens)
}

func (h *Handler) Login(w http.ResponseWriter, r *http.Request) {
	var req LoginRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		response.Error(w, http.StatusBadRequest, "invalid request body")
		return
	}
	tokens, err := h.svc.Login(r.Context(), req)
	if err != nil {
		handleError(w, err)
		return
	}
	response.JSON(w, http.StatusOK, tokens)
}

func (h *Handler) Refresh(w http.ResponseWriter, r *http.Request) {
	var req RefreshRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		response.Error(w, http.StatusBadRequest, "invalid request body")
		return
	}
	tokens, err := h.svc.Refresh(r.Context(), req)
	if err != nil {
		handleError(w, err)
		return
	}
	response.JSON(w, http.StatusOK, tokens)
}

func (h *Handler) Logout(w http.ResponseWriter, r *http.Request) {
	var req LogoutRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		response.Error(w, http.StatusBadRequest, "invalid request body")
		return
	}
	_ = h.svc.Logout(r.Context(), req)
	response.JSON(w, http.StatusOK, map[string]string{"message": "logged out"})
}

func handleError(w http.ResponseWriter, err error) {
	switch {
	case errors.Is(err, apperr.ErrInvalidInput):
		response.Error(w, http.StatusBadRequest, err.Error())
	case errors.Is(err, apperr.ErrUnauthorized):
		response.Error(w, http.StatusUnauthorized, err.Error())
	case errors.Is(err, apperr.ErrNotFound):
		response.Error(w, http.StatusNotFound, err.Error())
	case errors.Is(err, apperr.ErrAlreadyExists):
		response.Error(w, http.StatusConflict, err.Error())
	case errors.Is(err, apperr.ErrInsufficientBalance):
		response.Error(w, http.StatusPaymentRequired, err.Error())
	default:
		response.Error(w, http.StatusInternalServerError, "internal server error")
	}
}
```

---

## Entry Point

**File: `Services/cmd/auth/main.go`**

> 모든 서비스 진입점은 동일 패턴: config 로드 -> 인프라 연결 -> 의존성 연결 -> HTTP 서버 시작 (Graceful Shutdown)

```go
package main

import (
	"context"
	"log/slog"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/go-chi/chi/v5"
	"winters-backend/internal/auth"
	jwtauth "winters-backend/pkg/auth"
	"winters-backend/pkg/cache"
	"winters-backend/pkg/config"
	"winters-backend/pkg/database"
	"winters-backend/pkg/middleware"
)

func main() {
	slog.SetDefault(slog.New(slog.NewJSONHandler(os.Stdout, &slog.HandlerOptions{Level: slog.LevelInfo})))

	cfg, _ := config.Load()
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	db, _ := database.NewPool(ctx, cfg.DB)
	defer db.Close()
	rdb, _ := cache.NewClient(ctx, cfg.Redis)
	defer rdb.Close()

	jwtMgr := jwtauth.NewJWTManager(cfg.JWT.AccessSecret, cfg.JWT.RefreshSecret, cfg.JWT.AccessTTL, cfg.JWT.RefreshTTL)
	repo := auth.NewRepository(db, rdb)
	svc := auth.NewService(repo, jwtMgr)
	handler := auth.NewHandler(svc)

	r := chi.NewRouter()
	r.Use(middleware.Recovery, middleware.Logging)
	r.Mount("/auth", handler.Routes())
	r.Get("/health", func(w http.ResponseWriter, _ *http.Request) {
		w.WriteHeader(http.StatusOK)
		w.Write([]byte(`{"status":"ok"}`))
	})

	srv := &http.Server{Addr: ":" + cfg.AuthPort, Handler: r, ReadTimeout: 10 * time.Second, WriteTimeout: 10 * time.Second}
	go func() {
		slog.Info("auth service starting", "port", cfg.AuthPort)
		if err := srv.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			slog.Error("server error", "error", err)
			os.Exit(1)
		}
	}()

	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit
	slog.Info("shutting down auth service")
	shutdownCtx, shutdownCancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer shutdownCancel()
	srv.Shutdown(shutdownCtx)
}
```

---

## Phase 1 Verification Checklist

```
[ ] curl -X POST localhost:8081/auth/register -H "Content-Type: application/json" \
      -d '{"username":"test","email":"test@test.com","password":"password123"}'
    -> 201 Created + access_token + refresh_token

[ ] Same email -> 409 Conflict

[ ] curl -X POST localhost:8081/auth/login -H "Content-Type: application/json" \
      -d '{"email":"test@test.com","password":"password123"}'
    -> 200 OK + JWT pair

[ ] Wrong password -> 401 Unauthorized

[ ] Redis: EXISTS refresh:{jti} -> 1

[ ] PostgreSQL: SELECT * FROM users -> 1 row
    SELECT * FROM wallets -> 1 row (balance=0)
    SELECT * FROM player_stats -> 1 row (mmr=1000)

[ ] curl -X POST localhost:8081/auth/refresh -H "Content-Type: application/json" \
      -d '{"refresh_token":"..."}'
    -> 200 OK + new JWT pair

[ ] curl -X POST localhost:8081/auth/logout -H "Content-Type: application/json" \
      -d '{"refresh_token":"..."}'
    -> 200 OK

[ ] After logout, refresh with old token -> 401
```
