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

func (r *Repository) ProcessCharge(ctx context.Context, userId uuid.UUID, req ChargeRequest, gatewayTxID string) (*ChargeResponse, error) {
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
		userId, req.IdempotencyKey, req.Gateway, gatewayTxID,
		req.CoinAmount*100, req.CoinAmount,
	).Scan(&txID)
	if err != nil {
		return nil, fmt.Errorf("insert payment_transactions: %w", err)
	}

	var newBalance int64
	err = tx.QueryRow(ctx,
		`UPDATE wallets SET balance = balance + $2, updated_at = NOW()
		 WHERE user_id = $1 RETURNING balance`,
		userId, req.CoinAmount,
	).Scan(&newBalance)
	if err != nil {
		return nil, fmt.Errorf("update wallet: %w", err)
	}

	_, err = tx.Exec(ctx,
		`INSERT INTO coin_transactions (user_id, amount, tx_type, reference, balance_after)
		 VALUES ($1, $2, 'charge', $3, $4)`,
		userId, req.CoinAmount, txID.String(), newBalance,
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

func (r *Repository) GetBalance(ctx context.Context, userId uuid.UUID) (int64, error) {
	var balance int64
	err := r.db.QueryRow(ctx,
		`SELECT balance FROM wallets WHERE user_id = $1`, userId,
	).Scan(&balance)
	if err != nil {
		if errors.Is(err, pgx.ErrNoRows) {
			return 0, apperr.ErrNotFound
		}
		return 0, fmt.Errorf("get balance: %w", err)
	}
	return balance, nil
}
