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
