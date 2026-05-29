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
