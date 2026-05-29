package profile

import (
	"context"
	"encoding/json"
	"log/slog"

	"github.com/segmentio/kafka-go"
	"winters-backend/pkg/messaging"
)

type Consumer struct {
	repo   *Repository
	reader *kafka.Reader
}

func NewConsumer(repo *Repository, reader *kafka.Reader) *Consumer {
	return &Consumer{repo: repo, reader: reader}
}

func (c *Consumer) Start(ctx context.Context) {
	slog.Info("profile kafka consumer started", "topic", messaging.TopicMatchEvents)
	messaging.Consume(ctx, c.reader, c.handleMessage)
}

func (c *Consumer) handleMessage(ctx context.Context, msg kafka.Message) error {
	var event MatchCompletedEvent
	if err := json.Unmarshal(msg.Value, &event); err != nil {
		slog.Error("unmarshal match event", "error", err)
		return nil
	}

	if event.Type != "MatchCompleted" {
		return nil
	}

	for _, p := range event.Players {
		if err := c.repo.InsertMatchHistory(ctx, p.UserID, event.MatchID, p); err != nil {
			slog.Error("insert match record", "user_id", p.UserID, "error", err)
			continue
		}

		if err := c.repo.UpdatePlayerStats(ctx, p.UserID, p); err != nil {
			slog.Error("update player stats", "user_id", p.UserID, "error", err)
			continue
		}

		if err := c.repo.InvalidateCache(ctx, p.UserID); err != nil {
			slog.Warn("invalidate cache", "user_id", p.UserID, "error", err)
		}
	}

	slog.Info("processed match completed for profile", "match_id", event.MatchID, "players", len(event.Players))
	return nil
}
