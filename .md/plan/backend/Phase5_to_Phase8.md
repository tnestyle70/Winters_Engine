# Phase 5-8 -- Payment + Shop + Kafka Integration + C++ Client SDK

> **수정일**: 2026-04-12
> **변경사항**: 의사코드 → 실제 구현 코드 전문으로 교체. cmd/ 구조, chi 라우터, 포트 5433 반영.
> **프로젝트 구조 규칙**: main.go는 `cmd/{service}/main.go`에 위치 (internal에 넣지 않음)

---

## Phase 5: Payment Service (Port 8085)

> **Dependencies**: Phase 0, Phase 1 (users, wallets)
> **Estimated**: 3-5 days
> **Completion Criteria**: 결제 → 코인 충전 → 잔액 조회 파이프라인

### Architecture (Safety-Critical)

**핵심 안전 원칙**:
1. **PaymentGateway 인터페이스**: Toss, Stripe, Mock 구현체를 추상화
2. **서버사이드 검증 필수**: 클라이언트 "성공" 메시지 절대 불신 → PG사에 직접 VerifyReceipt 호출
3. **멱등성**: `idempotency_key` UNIQUE 제약 → 중복 요청 시 동일 결과 반환
4. **원자적 DB 트랜잭션**: payment_transactions INSERT + wallets UPDATE + coin_transactions INSERT
5. **CHECK 제약**: `balance >= 0` (DB 레벨 마지막 방어선)
6. **불변 트랜잭션 로그**: payment_transactions, coin_transactions는 INSERT 전용

### API Endpoints

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| POST | /payment/charge | O (JWT) | 코인 충전 요청 |
| GET | /payment/balance | O (JWT) | 잔액 조회 |

### Files (7개)

**`Services/internal/payment/model.go`**
```go
package payment

import (
	"time"
	"github.com/google/uuid"
)

type ChargeRequest struct {
	CoinAmount     int64  `json:"coin_amount"`
	Gateway        string `json:"gateway"`
	IdempotencyKey string `json:"idempotency_key"`
	ReceiptData    string `json:"receipt_data"`
}

type ChargeResponse struct {
	TxID    uuid.UUID `json:"tx_id"`
	Balance int64     `json:"balance"`
	Charged int64     `json:"charged"`
}

type BalanceResponse struct {
	UserID  uuid.UUID `json:"user_id"`
	Balance int64     `json:"balance"`
}

type PaymentTransaction struct {
	ID             uuid.UUID  `json:"id"`
	UserID         uuid.UUID  `json:"user_id"`
	IdempotencyKey string     `json:"idempotency_key"`
	Gateway        string     `json:"gateway"`
	GatewayTxID    string     `json:"gateway_tx_id"`
	Amount         int64      `json:"amount"`
	Currency       string     `json:"currency"`
	CoinAmount     int64      `json:"coin_amount"`
	Status         string     `json:"status"`
	CreatedAt      time.Time  `json:"created_at"`
	CompletedAt    *time.Time `json:"completed_at,omitempty"`
}

type PaymentCompletedEvent struct {
	Type       string    `json:"type"`
	TxID       uuid.UUID `json:"tx_id"`
	UserID     uuid.UUID `json:"user_id"`
	CoinAmount int64     `json:"coin_amount"`
	Gateway    string    `json:"gateway"`
}
```

**`Services/internal/payment/gateway.go`**
```go
package payment

import "context"

type PaymentGateway interface {
	Name() string
	VerifyReceipt(ctx context.Context, receiptData string, expectedAmount int64) (gatewayTxID string, err error)
	Refund(ctx context.Context, gatewayTxID string, amount int64) error
}
```

**`Services/internal/payment/gateway_mock.go`**
```go
package payment

import (
	"context"
	"github.com/google/uuid"
)

type MockGateway struct{}

func (g *MockGateway) Name() string { return "mock" }

func (g *MockGateway) VerifyReceipt(ctx context.Context, receiptData string, expectedAmount int64) (string, error) {
	return "mock_" + uuid.New().String(), nil
}

func (g *MockGateway) Refund(ctx context.Context, gatewayTxID string, amount int64) error {
	return nil
}
```

**`Services/internal/payment/repository.go`**
```go
package payment

import (
	"context"
	"errors"
	"fmt"

	"github.com/google/uuid"
	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgxpool"

	apperr "winters-backend/pkg/errors"
)

type Repository struct {
	db *pgxpool.Pool
}

func NewRepository(db *pgxpool.Pool) *Repository {
	return &Repository{db: db}
}

func (r *Repository) ProcessCharge(ctx context.Context, userID uuid.UUID, req ChargeRequest, gatewayTxID string) (*ChargeResponse, error) {
	tx, err := r.db.Begin(ctx)
	if err != nil {
		return nil, fmt.Errorf("begin tx: %w", err)
	}
	defer tx.Rollback(ctx)

	var txID uuid.UUID
	err = tx.QueryRow(ctx,
		`INSERT INTO payment_transactions
		 (user_id, idempotency_key, gateway, gateway_tx_id, amount, coin_amount, currency, status, completed_at)
		 VALUES ($1, $2, $3, $4, $5, $6, 'KRW', 'completed', NOW())
		 RETURNING id`,
		userID, req.IdempotencyKey, req.Gateway, gatewayTxID,
		req.CoinAmount*100, req.CoinAmount,
	).Scan(&txID)
	if err != nil {
		return nil, fmt.Errorf("insert payment_transactions: %w", err)
	}

	var newBalance int64
	err = tx.QueryRow(ctx,
		`UPDATE wallets SET balance = balance + $2, updated_at = NOW()
		 WHERE user_id = $1 RETURNING balance`,
		userID, req.CoinAmount,
	).Scan(&newBalance)
	if err != nil {
		return nil, fmt.Errorf("update wallet: %w", err)
	}

	_, err = tx.Exec(ctx,
		`INSERT INTO coin_transactions (user_id, amount, tx_type, reference, balance_after)
		 VALUES ($1, $2, 'charge', $3, $4)`,
		userID, req.CoinAmount, txID.String(), newBalance,
	)
	if err != nil {
		return nil, fmt.Errorf("insert coin_transactions: %w", err)
	}

	if err := tx.Commit(ctx); err != nil {
		return nil, fmt.Errorf("commit: %w", err)
	}

	return &ChargeResponse{TxID: txID, Balance: newBalance, Charged: req.CoinAmount}, nil
}

func (r *Repository) FindByIdempotencyKey(ctx context.Context, key string) (*ChargeResponse, error) {
	var resp ChargeResponse
	err := r.db.QueryRow(ctx,
		`SELECT pt.id, pt.coin_amount, w.balance
		 FROM payment_transactions pt
		 JOIN wallets w ON pt.user_id = w.user_id
		 WHERE pt.idempotency_key = $1`, key,
	).Scan(&resp.TxID, &resp.Charged, &resp.Balance)
	if err != nil {
		if errors.Is(err, pgx.ErrNoRows) {
			return nil, nil
		}
		return nil, fmt.Errorf("find by idempotency_key: %w", err)
	}
	return &resp, nil
}

func (r *Repository) GetBalance(ctx context.Context, userID uuid.UUID) (int64, error) {
	var balance int64
	err := r.db.QueryRow(ctx,
		`SELECT balance FROM wallets WHERE user_id = $1`, userID,
	).Scan(&balance)
	if err != nil {
		if errors.Is(err, pgx.ErrNoRows) {
			return 0, apperr.ErrNotFound
		}
		return 0, fmt.Errorf("get balance: %w", err)
	}
	return balance, nil
}
```

**`Services/internal/payment/service.go`**
```go
package payment

import (
	"context"
	"encoding/json"
	"fmt"
	"log/slog"

	"github.com/google/uuid"
	"github.com/segmentio/kafka-go"

	apperr "winters-backend/pkg/errors"
	"winters-backend/pkg/messaging"
)

type Service struct {
	repo     *Repository
	gateways map[string]PaymentGateway
	writer   *kafka.Writer
}

func NewService(repo *Repository, writer *kafka.Writer) *Service {
	return &Service{
		repo:     repo,
		gateways: make(map[string]PaymentGateway),
		writer:   writer,
	}
}

func (s *Service) RegisterGateway(gw PaymentGateway) {
	s.gateways[gw.Name()] = gw
}

func (s *Service) Charge(ctx context.Context, userID uuid.UUID, req ChargeRequest) (*ChargeResponse, error) {
	if req.CoinAmount <= 0 {
		return nil, fmt.Errorf("%w: coin_amount must be positive", apperr.ErrInvalidInput)
	}
	if req.IdempotencyKey == "" {
		return nil, fmt.Errorf("%w: idempotency_key required", apperr.ErrInvalidInput)
	}

	existing, err := s.repo.FindByIdempotencyKey(ctx, req.IdempotencyKey)
	if err != nil {
		return nil, err
	}
	if existing != nil {
		slog.Info("idempotent charge returned", "idempotency_key", req.IdempotencyKey)
		return existing, nil
	}

	gw, ok := s.gateways[req.Gateway]
	if !ok {
		return nil, fmt.Errorf("%w: unknown gateway: %s", apperr.ErrInvalidInput, req.Gateway)
	}

	gatewayTxID, err := gw.VerifyReceipt(ctx, req.ReceiptData, req.CoinAmount*100)
	if err != nil {
		return nil, fmt.Errorf("gateway verify: %w", err)
	}

	resp, err := s.repo.ProcessCharge(ctx, userID, req, gatewayTxID)
	if err != nil {
		return nil, err
	}

	event := PaymentCompletedEvent{
		Type:       "PaymentCompleted",
		TxID:       resp.TxID,
		UserID:     userID,
		CoinAmount: req.CoinAmount,
		Gateway:    req.Gateway,
	}
	data, _ := json.Marshal(event)
	if err := s.writer.WriteMessages(ctx, kafka.Message{
		Key:   []byte(userID.String()),
		Value: data,
		Topic: messaging.TopicPaymentEvents,
	}); err != nil {
		slog.Error("kafka publish payment completed", "error", err)
	}

	slog.Info("charge completed", "user_id", userID, "amount", req.CoinAmount, "tx_id", resp.TxID)
	return resp, nil
}

func (s *Service) GetBalance(ctx context.Context, userID uuid.UUID) (*BalanceResponse, error) {
	balance, err := s.repo.GetBalance(ctx, userID)
	if err != nil {
		return nil, err
	}
	return &BalanceResponse{UserID: userID, Balance: balance}, nil
}
```

**`Services/internal/payment/handler.go`**
```go
package payment

import (
	"encoding/json"
	"errors"
	"net/http"

	"github.com/go-chi/chi/v5"
	apperr "winters-backend/pkg/errors"
	"winters-backend/pkg/middleware"
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
	r.Post("/charge", h.Charge)
	r.Get("/balance", h.GetBalance)
	return r
}

func (h *Handler) Charge(w http.ResponseWriter, r *http.Request) {
	claims := middleware.GetClaims(r.Context())
	if claims == nil {
		response.Error(w, http.StatusUnauthorized, "missing claims")
		return
	}

	var req ChargeRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		response.Error(w, http.StatusBadRequest, "invalid request body")
		return
	}

	resp, err := h.svc.Charge(r.Context(), claims.UserID, req)
	if err != nil {
		handleError(w, err)
		return
	}
	response.JSON(w, http.StatusOK, resp)
}

func (h *Handler) GetBalance(w http.ResponseWriter, r *http.Request) {
	claims := middleware.GetClaims(r.Context())
	if claims == nil {
		response.Error(w, http.StatusUnauthorized, "missing claims")
		return
	}

	resp, err := h.svc.GetBalance(r.Context(), claims.UserID)
	if err != nil {
		handleError(w, err)
		return
	}
	response.JSON(w, http.StatusOK, resp)
}

func handleError(w http.ResponseWriter, err error) {
	switch {
	case errors.Is(err, apperr.ErrInvalidInput):
		response.Error(w, http.StatusBadRequest, err.Error())
	case errors.Is(err, apperr.ErrNotFound):
		response.Error(w, http.StatusNotFound, err.Error())
	case errors.Is(err, apperr.ErrUnauthorized):
		response.Error(w, http.StatusUnauthorized, err.Error())
	case errors.Is(err, apperr.ErrInsufficientBalance):
		response.Error(w, http.StatusPaymentRequired, err.Error())
	case errors.Is(err, apperr.ErrIdempotencyConflict):
		response.Error(w, http.StatusConflict, err.Error())
	default:
		response.Error(w, http.StatusInternalServerError, "internal server error")
	}
}
```

**`Services/cmd/payment/main.go`**
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
	"winters-backend/internal/payment"
	jwtauth "winters-backend/pkg/auth"
	"winters-backend/pkg/config"
	"winters-backend/pkg/database"
	"winters-backend/pkg/messaging"
	"winters-backend/pkg/middleware"
)

func main() {
	slog.SetDefault(slog.New(slog.NewJSONHandler(os.Stdout, &slog.HandlerOptions{Level: slog.LevelInfo})))

	cfg, err := config.Load()
	if err != nil {
		slog.Error("failed to load config", "error", err)
		os.Exit(1)
	}

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	db, err := database.NewPool(ctx, cfg.DB)
	if err != nil {
		slog.Error("failed to connect database", "error", err)
		os.Exit(1)
	}
	defer db.Close()

	writer := messaging.NewWriter(cfg.Kafka.Brokers, messaging.TopicPaymentEvents)
	defer writer.Close()

	repo := payment.NewRepository(db)
	svc := payment.NewService(repo, writer)
	svc.RegisterGateway(&payment.MockGateway{})

	jwtMgr := jwtauth.NewJWTManager(cfg.JWT.AccessSecret, cfg.JWT.RefreshSecret, cfg.JWT.AccessTTL, cfg.JWT.RefreshTTL)
	handler := payment.NewHandler(svc)

	r := chi.NewRouter()
	r.Use(middleware.Recovery, middleware.Logging)
	r.Group(func(r chi.Router) {
		r.Use(middleware.JWTAuth(jwtMgr))
		r.Mount("/payment", handler.Routes())
	})
	r.Get("/health", func(w http.ResponseWriter, _ *http.Request) {
		w.WriteHeader(http.StatusOK)
		w.Write([]byte(`{"status":"ok"}`))
	})

	srv := &http.Server{Addr: ":" + cfg.PaymentPort, Handler: r, ReadTimeout: 10 * time.Second, WriteTimeout: 10 * time.Second}
	go func() {
		slog.Info("payment service starting", "port", cfg.PaymentPort)
		if err := srv.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			slog.Error("server error", "error", err)
			os.Exit(1)
		}
	}()

	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit
	slog.Info("shutting down payment service")
	shutdownCtx, shutdownCancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer shutdownCancel()
	srv.Shutdown(shutdownCtx)
}
```

### Verification

```
[ ] Login, GET /payment/balance -> 0
[ ] POST /payment/charge (gateway:"mock", coin_amount:1000, idempotency_key:"test1")
    -> 200 OK, balance: 1000
[ ] Same request with same idempotency_key -> same result, no double charge
[ ] GET /payment/balance -> 1000
[ ] Check payment_transactions table: 1 row (status=completed)
[ ] Check coin_transactions table: 1 row (tx_type=charge, balance_after=1000)
[ ] Check wallets table: balance=1000
[ ] Kafka payment-events topic: PaymentCompleted event
[ ] balance=0 user tries charge with coin_amount=-100 -> rejected
```

---

## Phase 6: Shop + Inventory Service (Port 8086)

> **Dependencies**: Phase 0, Phase 1, Phase 5 (wallets, coin_transactions)
> **Estimated**: 3-5 days
> **Completion Criteria**: 코인으로 아이템 구매 → 인벤토리에 추가

### Architecture

구매는 하나의 PostgreSQL 트랜잭션:
1. `SELECT balance FROM wallets WHERE user_id=$1 FOR UPDATE` (행 잠금)
2. `balance >= price` 확인
3. `UPDATE wallets SET balance = balance - price`
4. `INSERT inventory ON CONFLICT quantity + 1`
5. `INSERT coin_transactions` (purchase 기록)
6. Kafka `player-events` 토픽에 `ItemPurchased` 이벤트 발행

### API Endpoints

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| GET | /shop/items | X | 상점 아이템 목록 (인증 불필요) |
| POST | /shop/purchase | O (JWT) | 아이템 구매 |
| GET | /shop/inventory/{user_id} | O (JWT) | 인벤토리 조회 |

### Files (5개)

**`Services/internal/shop/model.go`**
```go
package shop

import (
	"time"
	"github.com/google/uuid"
)

type ShopItem struct {
	ID          uuid.UUID `json:"id"`
	Name        string    `json:"name"`
	Description string    `json:"description"`
	ItemType    string    `json:"item_type"`
	Price       int64     `json:"price"`
	IsActive    bool      `json:"is_active"`
}

type InventoryItem struct {
	ItemID     uuid.UUID `json:"item_id"`
	Name       string    `json:"name"`
	ItemType   string    `json:"item_type"`
	Quantity   int       `json:"quantity"`
	AcquiredAt time.Time `json:"acquired_at"`
}

type PurchaseRequest struct {
	ItemID uuid.UUID `json:"item_id"`
}

type PurchaseResponse struct {
	Status         string `json:"status"`
	RemainingCoins int64  `json:"remaining_coins"`
}

type ItemPurchasedEvent struct {
	Type   string    `json:"type"`
	UserID uuid.UUID `json:"user_id"`
	ItemID uuid.UUID `json:"item_id"`
	Price  int64     `json:"price"`
}
```

**`Services/internal/shop/repository.go`**
```go
package shop

import (
	"context"
	"errors"
	"fmt"

	"github.com/google/uuid"
	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgxpool"

	apperr "winters-backend/pkg/errors"
)

type Repository struct {
	db *pgxpool.Pool
}

func NewRepository(db *pgxpool.Pool) *Repository {
	return &Repository{db: db}
}

func (r *Repository) ListItems(ctx context.Context) ([]ShopItem, error) {
	rows, err := r.db.Query(ctx,
		`SELECT id, name, description, item_type, price, is_active
		 FROM shop_items WHERE is_active = true ORDER BY created_at`)
	if err != nil {
		return nil, fmt.Errorf("query shop_items: %w", err)
	}
	defer rows.Close()

	items := make([]ShopItem, 0)
	for rows.Next() {
		var item ShopItem
		if err := rows.Scan(&item.ID, &item.Name, &item.Description,
			&item.ItemType, &item.Price, &item.IsActive); err != nil {
			return nil, fmt.Errorf("scan shop_item: %w", err)
		}
		items = append(items, item)
	}
	return items, nil
}

func (r *Repository) Purchase(ctx context.Context, userID, itemID uuid.UUID) (*PurchaseResponse, error) {
	tx, err := r.db.Begin(ctx)
	if err != nil {
		return nil, fmt.Errorf("begin tx: %w", err)
	}
	defer tx.Rollback(ctx)

	var price int64
	err = tx.QueryRow(ctx,
		`SELECT price FROM shop_items WHERE id = $1 AND is_active = true`, itemID,
	).Scan(&price)
	if err != nil {
		if errors.Is(err, pgx.ErrNoRows) {
			return nil, apperr.ErrNotFound
		}
		return nil, fmt.Errorf("get item price: %w", err)
	}

	var balance int64
	err = tx.QueryRow(ctx,
		`SELECT balance FROM wallets WHERE user_id = $1 FOR UPDATE`, userID,
	).Scan(&balance)
	if err != nil {
		return nil, fmt.Errorf("get balance: %w", err)
	}

	if balance < price {
		return nil, apperr.ErrInsufficientBalance
	}

	var newBalance int64
	err = tx.QueryRow(ctx,
		`UPDATE wallets SET balance = balance - $2, updated_at = NOW()
		 WHERE user_id = $1 RETURNING balance`,
		userID, price,
	).Scan(&newBalance)
	if err != nil {
		return nil, fmt.Errorf("deduct balance: %w", err)
	}

	_, err = tx.Exec(ctx,
		`INSERT INTO inventory (user_id, item_id, quantity, acquired_at)
		 VALUES ($1, $2, 1, NOW())
		 ON CONFLICT (user_id, item_id) DO UPDATE SET quantity = inventory.quantity + 1`,
		userID, itemID,
	)
	if err != nil {
		return nil, fmt.Errorf("insert inventory: %w", err)
	}

	_, err = tx.Exec(ctx,
		`INSERT INTO coin_transactions (user_id, amount, tx_type, reference, balance_after)
		 VALUES ($1, $2, 'purchase', $3, $4)`,
		userID, -price, itemID.String(), newBalance,
	)
	if err != nil {
		return nil, fmt.Errorf("insert coin_transactions: %w", err)
	}

	if err := tx.Commit(ctx); err != nil {
		return nil, fmt.Errorf("commit: %w", err)
	}

	return &PurchaseResponse{Status: "purchased", RemainingCoins: newBalance}, nil
}

func (r *Repository) GetInventory(ctx context.Context, userID uuid.UUID) ([]InventoryItem, error) {
	rows, err := r.db.Query(ctx,
		`SELECT i.item_id, s.name, s.item_type, i.quantity, i.acquired_at
		 FROM inventory i JOIN shop_items s ON i.item_id = s.id
		 WHERE i.user_id = $1`, userID)
	if err != nil {
		return nil, fmt.Errorf("query inventory: %w", err)
	}
	defer rows.Close()

	items := make([]InventoryItem, 0)
	for rows.Next() {
		var item InventoryItem
		if err := rows.Scan(&item.ItemID, &item.Name, &item.ItemType,
			&item.Quantity, &item.AcquiredAt); err != nil {
			return nil, fmt.Errorf("scan inventory: %w", err)
		}
		items = append(items, item)
	}
	return items, nil
}
```

**`Services/internal/shop/service.go`**
```go
package shop

import (
	"context"
	"encoding/json"
	"log/slog"

	"github.com/google/uuid"
	"github.com/segmentio/kafka-go"

	"winters-backend/pkg/messaging"
)

type Service struct {
	repo   *Repository
	writer *kafka.Writer
}

func NewService(repo *Repository, writer *kafka.Writer) *Service {
	return &Service{repo: repo, writer: writer}
}

func (s *Service) ListItems(ctx context.Context) ([]ShopItem, error) {
	return s.repo.ListItems(ctx)
}

func (s *Service) Purchase(ctx context.Context, userID, itemID uuid.UUID) (*PurchaseResponse, error) {
	resp, err := s.repo.Purchase(ctx, userID, itemID)
	if err != nil {
		return nil, err
	}

	event := ItemPurchasedEvent{
		Type:   "ItemPurchased",
		UserID: userID,
		ItemID: itemID,
	}
	data, _ := json.Marshal(event)
	if err := s.writer.WriteMessages(ctx, kafka.Message{
		Key:   []byte(userID.String()),
		Value: data,
		Topic: messaging.TopicPlayerEvents,
	}); err != nil {
		slog.Error("kafka publish item purchased", "error", err)
	}

	slog.Info("item purchased", "user_id", userID, "item_id", itemID)
	return resp, nil
}

func (s *Service) GetInventory(ctx context.Context, userID uuid.UUID) ([]InventoryItem, error) {
	return s.repo.GetInventory(ctx, userID)
}
```

**`Services/internal/shop/handler.go`**
```go
package shop

import (
	"encoding/json"
	"errors"
	"net/http"

	"github.com/go-chi/chi/v5"
	"github.com/google/uuid"
	apperr "winters-backend/pkg/errors"
	"winters-backend/pkg/middleware"
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
	r.Get("/items", h.ListItems)
	r.Post("/purchase", h.Purchase)
	r.Get("/inventory/{user_id}", h.GetInventory)
	return r
}

func (h *Handler) ListItems(w http.ResponseWriter, r *http.Request) {
	items, err := h.svc.ListItems(r.Context())
	if err != nil {
		response.Error(w, http.StatusInternalServerError, "failed to list items")
		return
	}
	response.JSON(w, http.StatusOK, items)
}

func (h *Handler) Purchase(w http.ResponseWriter, r *http.Request) {
	claims := middleware.GetClaims(r.Context())
	if claims == nil {
		response.Error(w, http.StatusUnauthorized, "missing claims")
		return
	}

	var req PurchaseRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		response.Error(w, http.StatusBadRequest, "invalid request body")
		return
	}

	resp, err := h.svc.Purchase(r.Context(), claims.UserID, req.ItemID)
	if err != nil {
		handleError(w, err)
		return
	}
	response.JSON(w, http.StatusOK, resp)
}

func (h *Handler) GetInventory(w http.ResponseWriter, r *http.Request) {
	userIDStr := chi.URLParam(r, "user_id")
	userID, err := uuid.Parse(userIDStr)
	if err != nil {
		response.Error(w, http.StatusBadRequest, "invalid user_id")
		return
	}

	items, err := h.svc.GetInventory(r.Context(), userID)
	if err != nil {
		response.Error(w, http.StatusInternalServerError, "failed to get inventory")
		return
	}
	response.JSON(w, http.StatusOK, items)
}

func handleError(w http.ResponseWriter, err error) {
	switch {
	case errors.Is(err, apperr.ErrNotFound):
		response.Error(w, http.StatusNotFound, err.Error())
	case errors.Is(err, apperr.ErrInvalidInput):
		response.Error(w, http.StatusBadRequest, err.Error())
	case errors.Is(err, apperr.ErrInsufficientBalance):
		response.Error(w, http.StatusPaymentRequired, err.Error())
	default:
		response.Error(w, http.StatusInternalServerError, "internal server error")
	}
}
```

**`Services/cmd/shop/main.go`**
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
	"winters-backend/internal/shop"
	jwtauth "winters-backend/pkg/auth"
	"winters-backend/pkg/config"
	"winters-backend/pkg/database"
	"winters-backend/pkg/messaging"
	"winters-backend/pkg/middleware"
)

func main() {
	slog.SetDefault(slog.New(slog.NewJSONHandler(os.Stdout, &slog.HandlerOptions{Level: slog.LevelInfo})))

	cfg, err := config.Load()
	if err != nil {
		slog.Error("failed to load config", "error", err)
		os.Exit(1)
	}

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	db, err := database.NewPool(ctx, cfg.DB)
	if err != nil {
		slog.Error("failed to connect database", "error", err)
		os.Exit(1)
	}
	defer db.Close()

	writer := messaging.NewWriter(cfg.Kafka.Brokers, messaging.TopicPlayerEvents)
	defer writer.Close()

	repo := shop.NewRepository(db)
	svc := shop.NewService(repo, writer)

	jwtMgr := jwtauth.NewJWTManager(cfg.JWT.AccessSecret, cfg.JWT.RefreshSecret, cfg.JWT.AccessTTL, cfg.JWT.RefreshTTL)
	handler := shop.NewHandler(svc)

	r := chi.NewRouter()
	r.Use(middleware.Recovery, middleware.Logging)
	r.Mount("/shop", func() chi.Router {
		sr := chi.NewRouter()
		sr.Get("/items", handler.ListItems)
		sr.Group(func(r chi.Router) {
			r.Use(middleware.JWTAuth(jwtMgr))
			r.Post("/purchase", handler.Purchase)
			r.Get("/inventory/{user_id}", handler.GetInventory)
		})
		return sr
	}())
	r.Get("/health", func(w http.ResponseWriter, _ *http.Request) {
		w.WriteHeader(http.StatusOK)
		w.Write([]byte(`{"status":"ok"}`))
	})

	srv := &http.Server{Addr: ":" + cfg.ShopPort, Handler: r, ReadTimeout: 10 * time.Second, WriteTimeout: 10 * time.Second}
	go func() {
		slog.Info("shop service starting", "port", cfg.ShopPort)
		if err := srv.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			slog.Error("server error", "error", err)
			os.Exit(1)
		}
	}()

	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit
	slog.Info("shutting down shop service")
	shutdownCtx, shutdownCancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer shutdownCancel()
	srv.Shutdown(shutdownCtx)
}
```

### Verification

```
[ ] Insert test items: INSERT INTO shop_items (name, item_type, price, is_active) VALUES ('Dragon Skin', 'skin', 2000, true)
[ ] Charge 5000 coins via payment service
[ ] GET /shop/items -> shows items
[ ] POST /shop/purchase {item_id: "..."} -> 200, remaining_coins: 3000
[ ] GET /shop/inventory/{user_id} -> shows purchased item
[ ] POST /shop/purchase same item -> quantity increases (ON CONFLICT)
[ ] Try purchase with insufficient balance -> 402 Payment Required
[ ] coin_transactions: 1 row (tx_type=purchase, amount=-2000)
[ ] Kafka player-events: ItemPurchased event
```

---

## Phase 7: Kafka Event Integration (End-to-End)

> **Dependencies**: All Phases 1-6
> **Estimated**: 2-3 days
> **Completion Criteria**: 게임 종료 → 랭킹+통계+알림이 자동으로 갱신

### No New Code Required

이 Phase는 모든 서비스 간 이벤트 흐름을 검증하는 통합 테스트이다.

### Topic Schema

| Topic | Producer | Consumer Groups |
|-------|----------|-----------------|
| `match-events` | Matchmaking / Game Server | leaderboard-consumer, profile-consumer |
| `payment-events` | Payment Service | (analytics, future) |
| `player-events` | Shop Service | (notification, future) |

### End-to-End Test Scenario

```
1. Start all 6 services:
   go run ./cmd/auth           (8081)
   go run ./cmd/leaderboard    (8082)
   go run ./cmd/matchmaking    (8083)
   go run ./cmd/profile        (8084)
   go run ./cmd/payment        (8085)
   go run ./cmd/shop           (8086)

2. Register 2 users via Auth
   curl -X POST localhost:8081/auth/register -d '{"username":"A","email":"a@test.com","password":"pass1234"}'
   curl -X POST localhost:8081/auth/register -d '{"username":"B","email":"b@test.com","password":"pass1234"}'

3. Charge coins for both via Payment
   curl -X POST localhost:8085/payment/charge -H "Authorization: Bearer <TOKEN_A>" \
     -d '{"gateway":"mock","coin_amount":5000,"idempotency_key":"a-charge-1"}'

4. Both join matchmaking
   curl -X POST localhost:8083/matchmaking/join -H "Authorization: Bearer <TOKEN_A>"
   curl -X POST localhost:8083/matchmaking/join -H "Authorization: Bearer <TOKEN_B>"

5. Wait 2s -> Check status -> matched

6. Simulate MatchCompleted (kafka-console-producer):
   {"type":"MatchCompleted","match_id":"...","players":[
     {"user_id":"<A_ID>","result":"win","kills":5,"deaths":2,"assists":3,"mmr_change":25},
     {"user_id":"<B_ID>","result":"loss","kills":2,"deaths":5,"assists":1,"mmr_change":-25}
   ]}

7. Verify cascading updates:
   - Leaderboard: GET /leaderboard/top -> A: 1025 MMR, B: 975 MMR
   - Profile: GET /profile/<A_ID> -> wins:1, kills:5, deaths:2
   - Match History: GET /profile/<A_ID>/history -> 1 record

8. Purchase item via Shop
   curl -X POST localhost:8086/shop/purchase -H "Authorization: Bearer <TOKEN_A>" \
     -d '{"item_id":"<ITEM_UUID>"}'
   -> wallet deducted, inventory updated
```

### Verification Checklist

```
[ ] MatchCompleted event -> leaderboard + profile consumers both process
[ ] Consumer stopped -> restart -> resumes from offset (no message loss)
[ ] Same event processed by different consumer groups independently
[ ] Malformed event -> logged and skipped (no infinite loop)
```

---

## Phase 8: C++ Client Network SDK

> **Dependencies**: All Go services running for integration testing
> **Estimated**: 5-7 days
> **Completion Criteria**: 게임 클라이언트에서 회원가입 → 로그인 → 매칭 → 결과 확인

### Architecture

WinHTTP 기반 HTTP 클라이언트. 기존 엔진 코드 컨벤션 (RAII, private 생성자, `Create()` 팩토리)을 따른다.
비동기 요청은 `std::async`로 처리하고, 콜백은 `ProcessCallbacks()`를 통해 메인 스레드에서 디스패치한다.

### Files (12개)

```
Client/Public/Network/       ← 헤더
  CHttpClient.h
  CAuthClient.h
  CMatchClient.h
  CProfileClient.h
  CPaymentClient.h
  CShopClient.h

Client/Private/Network/      ← 구현
  CHttpClient.cpp
  CAuthClient.cpp
  CMatchClient.cpp
  CProfileClient.cpp
  CPaymentClient.cpp
  CShopClient.cpp
```

> **주의**: 기존 계획서의 `Client/Header/` → `Client/Public/`, `Client/Code/` → `Client/Private/`로 수정됨

### CHttpClient (Base)

**`Client/Public/Network/CHttpClient.h`**
```cpp
#pragma once
#include <string>
#include <functional>
#include <queue>
#include <mutex>
#include <cstdint>

struct HttpResponse
{
    int32_t     statusCode = 0;
    std::string body;
    bool        success    = false;
    std::string error;
};

using HttpCallback = std::function<void(const HttpResponse&)>;

class CHttpClient
{
public:
    ~CHttpClient() = default;

    static std::unique_ptr<CHttpClient> Create(const std::string& baseURL);

    void    SetAuthToken(const std::string& token);

    HttpResponse    Get(const std::string& path);
    HttpResponse    Post(const std::string& path, const std::string& jsonBody);
    HttpResponse    Delete(const std::string& path);

    void    AsyncGet(const std::string& path, HttpCallback callback);
    void    AsyncPost(const std::string& path, const std::string& jsonBody, HttpCallback callback);

    void    ProcessCallbacks();

private:
    CHttpClient() = default;
    HttpResponse    DoRequest(const std::string& method, const std::string& path, const std::string& body);

    std::string     m_BaseURL;
    std::string     m_AuthToken;
    std::mutex      m_CallbackMutex;
    std::queue<std::function<void()>> m_PendingCallbacks;
};
```

**`Client/Private/Network/CHttpClient.cpp`** -- Implementation:
- `WinHttpOpen`, `WinHttpConnect`, `WinHttpOpenRequest`, `WinHttpSendRequest`
- `WinHttpReceiveResponse`, `WinHttpReadData`
- Async methods: `std::async` → queue callback into `m_PendingCallbacks`
- `ProcessCallbacks()`: lock mutex, drain queue, execute on main thread

### CAuthClient

**`Client/Public/Network/CAuthClient.h`**
```cpp
#pragma once
#include "Network/CHttpClient.h"
#include <memory>

struct AuthResult
{
    bool        success      = false;
    std::string accessToken;
    std::string refreshToken;
    int64_t     expiresAt    = 0;
    std::string error;
};

using AuthCallback = std::function<void(const AuthResult&)>;

class CAuthClient
{
public:
    ~CAuthClient() = default;

    static std::unique_ptr<CAuthClient> Create(const std::string& baseURL);

    void    Register(const std::string& username, const std::string& email,
                     const std::string& password, AuthCallback callback);
    void    Login(const std::string& email, const std::string& password, AuthCallback callback);
    void    Refresh(AuthCallback callback);
    void    Logout();
    void    ProcessCallbacks();

    const std::string&  GetAccessToken() const  { return m_AccessToken; }
    bool                IsLoggedIn() const      { return !m_AccessToken.empty(); }

private:
    CAuthClient() = default;
    AuthResult  ParseAuthResponse(const HttpResponse& resp);

    std::unique_ptr<CHttpClient> m_pHttp;
    std::string m_AccessToken;
    std::string m_RefreshToken;
};
```

### Other Client Wrappers

`CMatchClient`, `CProfileClient`, `CPaymentClient`, `CShopClient`는 동일 패턴:
- `CHttpClient`를 `unique_ptr`로 보유
- 타입화된 Request/Response 구조체
- 비동기 메서드 + 콜백
- `ProcessCallbacks()` 게임 루프에서 호출

### Game Loop Integration

```
[LoginScene::OnUpdate]          [LobbyScene::OnUpdate]         [MatchScene]
   m_pAuth->ProcessCallbacks();    m_pMatch->ProcessCallbacks();   UDP connection
   if (!m_pAuth->IsLoggedIn())     if (pollingTimer > 1s)
       m_pAuth->Login(...)            m_pMatch->PollStatus(...)
   else                            if (status.matched)
       ChangeScene<LobbyScene>()       ChangeScene<MatchScene>()
```

### Required Linking

`Client.vcxproj`에 추가 필요:
```xml
<AdditionalDependencies>winhttp.lib;%(AdditionalDependencies)</AdditionalDependencies>
```

### Verification

```
[ ] 게임 클라이언트에서 회원가입 -> 로그인 성공
[ ] 로비에서 내 프로필 (MMR, 승률) 표시
[ ] 매칭 버튼 -> 대기 -> 매칭 완료 알림
[ ] 랭킹 탭에서 상위 100명 + 내 순위 표시
[ ] 상점에서 아이템 구매 -> 잔액 감소 확인
```

---

## Safety Rules (Every PR Checklist)

```
Payment Safety:
  - Server-side verification mandatory (never trust client "success")
  - Idempotency: idempotency_key prevents duplicate charges
  - Immutable tx logs: payment_transactions INSERT only
  - CHECK constraint: balance >= 0 (DB last defense line)
  - Password: bcrypt hash only (plaintext NEVER)

DB Safety:
  - Money operations = always PostgreSQL transactions (BEGIN/COMMIT/ROLLBACK)
  - Virtual currency balance = BIGINT (FLOAT/DOUBLE forbidden)
  - FK constraints prevent orphan records
  - Indexes on frequently queried columns

Redis Safety:
  - Session TTL mandatory (no-TTL = memory leak)
  - Redis is cache -- PostgreSQL is source of truth
  - Leaderboard: dual-write Redis Sorted Set + PostgreSQL

Kafka Safety:
  - Consumer restart resumes from offset (no message loss)
  - Error: log-and-skip (no infinite loop)

Docker:
  - PostgreSQL: port 5433 (호스트 5432 충돌 방지)
  - Kafka: KAFKA_ 접두사 (KAFKA_CFG_ 아님)
  - .env: DB_HOST=127.0.0.1 (IPv6 회피)

Go 구조:
  - main.go는 cmd/{service}/main.go에만 위치
  - internal/{service}/에는 package {service} 파일만
```
