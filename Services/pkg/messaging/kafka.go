package messaging

import (
	"context"
	"crypto/tls"
	"log/slog"
	"time"

	"github.com/segmentio/kafka-go"
)

const (
	TopicPaymentEvents = "payment-events"
	TopicMatchEvents   = "match-events"
	TopicPlayerEvents  = "player-events"
)

func NewWriter(brokers []string, topic string, useTLS bool) *kafka.Writer {
	writer := &kafka.Writer{
		Addr:         kafka.TCP(brokers...),
		Topic:        topic,
		Balancer:     &kafka.LeastBytes{},
		BatchTimeout: 10 * time.Millisecond,
		RequiredAcks: kafka.RequireOne,
	}
	if useTLS {
		writer.Transport = &kafka.Transport{
			TLS: &tls.Config{MinVersion: tls.VersionTLS12},
		}
	}
	return writer
}

func NewReader(brokers []string, topic, groupID string, useTLS bool) *kafka.Reader {
	dialer := &kafka.Dialer{Timeout: 10 * time.Second, DualStack: true}
	if useTLS {
		dialer.TLS = &tls.Config{MinVersion: tls.VersionTLS12}
	}
	return kafka.NewReader(kafka.ReaderConfig{
		Brokers:        brokers,
		Topic:          topic,
		GroupID:        groupID,
		MinBytes:       1,
		MaxBytes:       10e6,
		CommitInterval: time.Second,
		StartOffset:    kafka.LastOffset,
		Dialer:         dialer,
	})
}

func Consume(ctx context.Context, reader *kafka.Reader, handler func(ctx context.Context, msg kafka.Message) error) {
	for {
		msg, err := reader.FetchMessage(ctx)
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
			time.Sleep(time.Second)
			continue
		}
		if err := reader.CommitMessages(ctx, msg); err != nil {
			slog.Error("kafka commit error", "topic", msg.Topic, "offset", msg.Offset, "error", err)
		}
	}
}
