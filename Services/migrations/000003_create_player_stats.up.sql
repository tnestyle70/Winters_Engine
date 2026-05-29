CREATE TABLE player_stats (
    id          UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id     UUID NOT NULL UNIQUE REFERENCES users(id) ON DELETE CASCADE,
    mmr         INT  NOT NULL DEFAULT 1000,
    wins        INT  NOT NULL DEFAULT 0,
    losses      INT  NOT NULL DEFAULT 0,
    kills       INT  NOT NULL DEFAULT 0,
    deaths      INT  NOT NULL DEFAULT 0,
    assists     INT  NOT NULL DEFAULT 0,
    updated_at  TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX idx_player_stats_mmr ON player_stats(mmr DESC);
