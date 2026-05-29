CREATE TABLE payment_transactions (
    id              UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id         UUID NOT NULL REFERENCES users(id),
    idempotency_key VARCHAR(64) NOT NULL UNIQUE,
    gateway         VARCHAR(20) NOT NULL,
    gateway_tx_id   VARCHAR(255),
    amount          BIGINT NOT NULL CHECK (amount > 0),
    currency        VARCHAR(3) NOT NULL DEFAULT 'KRW',
    coin_amount     BIGINT NOT NULL CHECK (coin_amount > 0),
    status          VARCHAR(20) NOT NULL DEFAULT 'pending'
                    CHECK (status IN ('pending', 'completed', 'failed', 'refunded')),
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    completed_at    TIMESTAMPTZ
);

CREATE INDEX idx_payment_tx_user  ON payment_transactions(user_id, created_at DESC);
CREATE INDEX idx_payment_tx_idemp ON payment_transactions(idempotency_key);
