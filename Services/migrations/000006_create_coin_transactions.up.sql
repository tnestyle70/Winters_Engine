CREATE TABLE coin_transactions (
    id            UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id       UUID NOT NULL REFERENCES users(id),
    amount        BIGINT NOT NULL,
    tx_type       VARCHAR(20) NOT NULL CHECK (tx_type IN ('charge', 'purchase', 'refund')),
    reference     VARCHAR(255),
    balance_after BIGINT NOT NULL,
    created_at    TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX idx_coin_tx_user ON coin_transactions(user_id, created_at DESC);
