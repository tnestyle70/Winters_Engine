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

// GetStorefront returns wallet balance and every active champion product
// LEFT JOINed with the account's inventory, as one consistent snapshot.
func (r *Repository) GetStorefront(ctx context.Context, userId uuid.UUID) (*StorefrontResponse, error) {
	tx, err := r.db.BeginTx(ctx, pgx.TxOptions{
		IsoLevel:   pgx.RepeatableRead,
		AccessMode: pgx.ReadOnly,
	})
	if err != nil {
		return nil, fmt.Errorf("begin storefront snapshot: %w", err)
	}
	defer tx.Rollback(ctx)

	resp := &StorefrontResponse{}
	err = tx.QueryRow(ctx,
		`SELECT balance, currency_code FROM wallets WHERE user_id = $1`, userId,
	).Scan(&resp.BalanceRP, &resp.CurrencyCode)
	if err != nil {
		if errors.Is(err, pgx.ErrNoRows) {
			return nil, apperr.ErrNotFound
		}
		return nil, fmt.Errorf("get wallet: %w", err)
	}

	rows, err := tx.Query(ctx,
		`SELECT s.id, COALESCE(s.product_key, ''), COALESCE(s.content_key, ''), s.name, s.price, s.sort_order,
		        (i.user_id IS NOT NULL) AS owned
		 FROM shop_items s
		 LEFT JOIN inventory i ON i.item_id = s.id AND i.user_id = $1
		 WHERE s.is_active = true AND s.item_type = 'champion'
		 ORDER BY s.sort_order, s.name`, userId)
	if err != nil {
		return nil, fmt.Errorf("query storefront: %w", err)
	}

	resp.Items = make([]StorefrontItem, 0)
	for rows.Next() {
		var item StorefrontItem
		if err := rows.Scan(&item.ItemID, &item.ProductKey, &item.ContentKey,
			&item.DisplayName, &item.PriceRP, &item.SortOrder, &item.Owned); err != nil {
			return nil, fmt.Errorf("scan storefront item: %w", err)
		}
		resp.Items = append(resp.Items, item)
	}
	rows.Close()
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("iterate storefront items: %w", err)
	}
	if err := tx.Commit(ctx); err != nil {
		return nil, fmt.Errorf("commit storefront snapshot: %w", err)
	}
	return resp, nil
}

// PurchaseChampion buys a non-stackable champion product exactly once.
// Re-purchase returns already_owned without charging (never quantity+1).
func (r *Repository) PurchaseChampion(ctx context.Context, userId, itemId uuid.UUID) (*PurchaseResponse, error) {
	tx, err := r.db.Begin(ctx)
	if err != nil {
		return nil, fmt.Errorf("begin tx: %w", err)
	}
	defer tx.Rollback(ctx)

	var price int64
	var itemType string
	var stackable bool
	err = tx.QueryRow(ctx,
		`SELECT price, item_type, is_stackable
		 FROM shop_items WHERE id = $1 AND is_active = true`, itemId,
	).Scan(&price, &itemType, &stackable)
	if err != nil {
		if errors.Is(err, pgx.ErrNoRows) {
			return nil, apperr.ErrNotFound
		}
		return nil, fmt.Errorf("get item: %w", err)
	}
	if itemType != "champion" || stackable {
		return nil, fmt.Errorf("%w: not a champion product", apperr.ErrInvalidInput)
	}

	var balance int64
	err = tx.QueryRow(ctx,
		`SELECT balance FROM wallets WHERE user_id = $1 FOR UPDATE`, userId,
	).Scan(&balance)
	if err != nil {
		if errors.Is(err, pgx.ErrNoRows) {
			return nil, apperr.ErrNotFound
		}
		return nil, fmt.Errorf("get balance: %w", err)
	}

	var owned bool
	err = tx.QueryRow(ctx,
		`SELECT EXISTS(SELECT 1 FROM inventory WHERE user_id = $1 AND item_id = $2)`,
		userId, itemId,
	).Scan(&owned)
	if err != nil {
		return nil, fmt.Errorf("check ownership: %w", err)
	}
	if owned {
		return &PurchaseResponse{Status: "already_owned", RemainingCoins: balance, Owned: true}, nil
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

	if _, err := tx.Exec(ctx,
		`INSERT INTO inventory (user_id, item_id, quantity, acquired_at)
		 VALUES ($1, $2, 1, NOW())`,
		userId, itemId); err != nil {
		return nil, fmt.Errorf("insert inventory: %w", err)
	}

	if _, err := tx.Exec(ctx,
		`INSERT INTO coin_transactions (user_id, amount, tx_type, reference, balance_after)
		 VALUES ($1, $2, 'purchase', $3, $4)`,
		userId, -price, itemId.String(), newBalance); err != nil {
		return nil, fmt.Errorf("insert coin_transactions: %w", err)
	}

	if err := tx.Commit(ctx); err != nil {
		return nil, fmt.Errorf("commit: %w", err)
	}
	return &PurchaseResponse{Status: "purchased", RemainingCoins: newBalance, Owned: true}, nil
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
