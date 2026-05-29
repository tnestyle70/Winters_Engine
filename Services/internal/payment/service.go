package payment

import (
	"context"
	"encoding/json"
	"fmt"
	"log/slog"

	"github.com/google/uuid"
	"github.com/segmentio/kafka-go"

	apperr "winters-backend/pkg/errors"
	"winters-backend/pkg/messaging"
)

type Service struct {
	repo     *Repository
	gateways map[string]PaymentGateway
	writer   *kafka.Writer
}

func NewService(repo *Repository, writer *kafka.Writer) *Service {
	return &Service{
		repo:     repo,
		gateways: make(map[string]PaymentGateway),
		writer:   writer,
	}
}

func (s *Service) RegisterGateway(gw PaymentGateway) {
	s.gateways[gw.Name()] = gw
}

func (s *Service) Charge(ctx context.Context, userId uuid.UUID, req ChargeRequest) (*ChargeResponse, error) {
	if req.CoinAmount <= 0 {
		return nil, fmt.Errorf("%w: coin_amount must be positive", apperr.ErrInvalidInput)
	}
	if req.IdempotencyKey == "" {
		return nil, fmt.Errorf("%w: idempotency_key required", apperr.ErrInvalidInput)
	}

	existing, err := s.repo.FindByIdempotencyKey(ctx, req.IdempotencyKey)
	if err != nil {
		return nil, err
	}
	if existing != nil {
		slog.Info("idempotent charge returned", "idempotency_key", req.IdempotencyKey)
		return existing, nil
	}

	gw, ok := s.gateways[req.Gateway]
	if !ok {
		return nil, fmt.Errorf("%w: unknown gateway: %s", apperr.ErrInvalidInput, req.Gateway)
	}

	gatewayTxID, err := gw.VerifyReceipt(ctx, req.ReceiptData, req.CoinAmount*100)
	if err != nil {
		return nil, fmt.Errorf("gateway verify: %w", err)
	}

	resp, err := s.repo.ProcessCharge(ctx, userId, req, gatewayTxID)
	if err != nil {
		return nil, err
	}

	event := PaymentCompletedEvent{
		Type:       "PaymentCompleted",
		TxID:       resp.TxID,
		UserID:     userId,
		CoinAmount: req.CoinAmount,
		Gateway:    req.Gateway,
	}
	data, _ := json.Marshal(event)
	if err := s.writer.WriteMessages(ctx, kafka.Message{
		Key:   []byte(userId.String()),
		Value: data,
		Topic: messaging.TopicPaymentEvents,
	}); err != nil {
		slog.Error("kafka publish payment completed", "error", err)
	}

	slog.Info("charge completed", "user_id", userId, "amount", req.CoinAmount, "tx_id", resp.TxID)
	return resp, nil
}

func (s *Service) GetBalance(ctx context.Context, userId uuid.UUID) (*BalanceResponse, error) {
	balance, err := s.repo.GetBalance(ctx, userId)
	if err != nil {
		return nil, err
	}
	return &BalanceResponse{UserID: userId, Balance: balance}, nil
}
