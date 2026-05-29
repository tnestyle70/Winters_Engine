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

func (s *Service) Purchase(ctx context.Context, userId, itemId uuid.UUID) (*PurchaseResponse, error) {
	resp, err := s.repo.Purchase(ctx, userId, itemId)
	if err != nil {
		return nil, err
	}

	event := ItemPurchasedEvent{
		Type:   "ItemPurchased",
		UserID: userId,
		ItemID: itemId,
	}
	data, _ := json.Marshal(event)
	if err := s.writer.WriteMessages(ctx, kafka.Message{
		Key:   []byte(userId.String()),
		Value: data,
		Topic: messaging.TopicPlayerEvents,
	}); err != nil {
		slog.Error("kafka publish item purchased", "error", err)
	}

	slog.Info("item purchased", "user_id", userId, "item_id", itemId)
	return resp, nil
}

func (s *Service) GetInventory(ctx context.Context, userId uuid.UUID) ([]InventoryItem, error) {
	return s.repo.GetInventory(ctx, userId)
}
