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

func (r *Repository) Purchase(ctx context.Context, userId, itemId uuid.UUID) (*PurchaseResponse, error) {
	tx, err := r.db.Begin(ctx)
	if err != nil {
		return nil, fmt.Errorf("begin tx: %w", err)
	}
	defer tx.Rollback(ctx)

	var price int64
	err = tx.QueryRow(ctx,
		`SELECT price FROM shop_items WHERE id = $1 AND is_active = true`, itemId,
	).Scan(&price)
	if err != nil {
		if errors.Is(err, pgx.ErrNoRows) {
			return nil, apperr.ErrNotFound
		}
		return nil, fmt.Errorf("get item price: %w", err)
	}

	var balance int64
	err = tx.QueryRow(ctx,
		`SELECT balance FROM wallets WHERE user_id = $1 FOR UPDATE`, userId,
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
		userId, price,
	).Scan(&newBalance)
	if err != nil {
		return nil, fmt.Errorf("deduct balance: %w", err)
	}

	_, err = tx.Exec(ctx,
		`INSERT INTO inventory (user_id, item_id, quantity, acquired_at)
		 VALUES ($1, $2, 1, NOW())
		 ON CONFLICT (user_id, item_id) DO UPDATE SET quantity = inventory.quantity + 1`,
		userId, itemId,
	)
	if err != nil {
		return nil, fmt.Errorf("insert inventory: %w", err)
	}

	_, err = tx.Exec(ctx,
		`INSERT INTO coin_transactions (user_id, amount, tx_type, reference, balance_after)
		 VALUES ($1, $2, 'purchase', $3, $4)`,
		userId, -price, itemId.String(), newBalance,
	)
	if err != nil {
		return nil, fmt.Errorf("insert coin_transactions: %w", err)
	}

	if err := tx.Commit(ctx); err != nil {
		return nil, fmt.Errorf("commit: %w", err)
	}

	return &PurchaseResponse{Status: "purchased", RemainingCoins: newBalance}, nil
}

func (r *Repository) GetInventory(ctx context.Context, userId uuid.UUID) ([]InventoryItem, error) {
	rows, err := r.db.Query(ctx,
		`SELECT i.item_id, s.name, s.item_type, i.quantity, i.acquired_at
		 FROM inventory i JOIN shop_items s ON i.item_id = s.id
		 WHERE i.user_id = $1`, userId)
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
