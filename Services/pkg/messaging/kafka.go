package messaging

import (
	"context"
	"log/slog"
	"time"

	"github.com/segmentio/kafka-go"
)

const (
	TopicPaymentEvents = "payment-events"
	TopicMatchEvents   = "match-events"
	TopicPlayerEvents  = "player-events"
)

func NewWriter(brokers []string, topic string) *kafka.Writer {
	return &kafka.Writer{
		Addr:         kafka.TCP(brokers...),
		Topic:        topic,
		Balancer:     &kafka.LeastBytes{},
		BatchTimeout: 10 * time.Millisecond,
		RequiredAcks: kafka.RequireOne,
	}
}

func NewReader(brokers []string, topic, groupID string) *kafka.Reader {
	return kafka.NewReader(kafka.ReaderConfig{
		Brokers:        brokers,
		Topic:          topic,
		GroupID:        groupID,
		MinBytes:       1,
		MaxBytes:       10e6,
		CommitInterval: time.Second,
		StartOffset:    kafka.LastOffset,
	})
}

func Consume(ctx context.Context, reader *kafka.Reader, handler func(ctx context.Context, msg kafka.Message) error) {
	for {
		msg, err := reader.ReadMessage(ctx)
		if err != nil {
			if ctx.Err() != nil {
				return
			}
			slog.Error("kafka read error", "error", err)
			time.Sleep(time.Second)
			continue
		}
		if err := handler(ctx, msg); err != nil {
			slog.Error("kafka handler error", "topic", msg.Topic, "offset", msg.Offset, "error", err)
		}
	}
}
