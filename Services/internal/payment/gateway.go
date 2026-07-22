package payment

import "context"

type PaymentGateway interface {
	Name() string
	VerifyReceipt(ctx context.Context, receiptData string, expectedAmount int64) (gatewayTxID string, err error)
	Refund(ctx context.Context, gatewayTxID string, amount int64) error
}
