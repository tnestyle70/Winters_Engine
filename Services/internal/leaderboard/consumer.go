package leaderboard

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
	slog.Info("leaderboard kafka consumer started", "topic", messaging.TopicMatchEvents)
	messaging.Consume(ctx, c.reader, c.handleMessage)
}

func (c *Consumer) handleMessage(ctx context.Context, msg kafka.Message) error {
	var event MatchEvent
	if err := json.Unmarshal(msg.Value, &event); err != nil {
		slog.Error("unmarshal match event", "error", err)
		return nil // skip malformed messages
	}

	if event.Type != "MatchCompleted" {
		return nil
	}

	for _, p := range event.Players {
		currentMMR, err := c.repo.GetCurrentMMR(ctx, p.UserID)
		if err != nil {
			slog.Error("get current mmr", "user_id", p.UserID, "error", err)
			continue
		}
		newMMR := currentMMR + p.MMRChange
		if newMMR < 0 {
			newMMR = 0
		}
		if err := c.repo.UpdateScore(ctx, p.UserID, newMMR); err != nil {
			slog.Error("update leaderboard score", "user_id", p.UserID, "error", err)
		}
	}

	slog.Info("processed match event for leaderboard", "match_id", event.MatchID, "players", len(event.Players))
	return nil
}
