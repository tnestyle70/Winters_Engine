package payment

import (
	"context"
	"github.com/google/uuid"
)

type MockGateway struct{}

func (g *MockGateway) Name() string { return "mock" }

func (g *MockGateway) VerifyReceipt(ctx context.Context, receiptData string, expectedAmount int64) (string, error) {
	return "mock_" + uuid.New().String(), nil
}

func (g *MockGateway) Refund(ctx context.Context, gatewayTxID string, amount int64) error {
	return nil
}
