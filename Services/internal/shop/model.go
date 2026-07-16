package shop

import (
	"github.com/google/uuid"
	"time"
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
	Status         string `json:"status"` // "purchased" | "already_owned"
	RemainingCoins int64  `json:"remaining_coins"`
	Owned          bool   `json:"owned"`
}

// StorefrontItem/StorefrontResponse: one atomic account view
// (wallet balance + champion products + ownership) for the meta RP shop.
type StorefrontItem struct {
	ItemID      uuid.UUID `json:"item_id"`
	ProductKey  string    `json:"product_key"`
	ContentKey  string    `json:"content_key"`
	DisplayName string    `json:"display_name"`
	PriceRP     int64     `json:"price_rp"`
	Owned       bool      `json:"owned"`
	SortOrder   int       `json:"sort_order"`
}

type StorefrontResponse struct {
	CurrencyCode string           `json:"currency_code"`
	BalanceRP    int64            `json:"balance_rp"`
	Items        []StorefrontItem `json:"items"`
}

type ItemPurchasedEvent struct {
	Type   string    `json:"type"`
	UserID uuid.UUID `json:"user_id"`
	ItemID uuid.UUID `json:"item_id"`
	Price  int64     `json:"price"`
}
