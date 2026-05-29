CREATE TABLE match_history (
    id          UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id     UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    match_id    UUID NOT NULL,
    result      VARCHAR(10) NOT NULL CHECK (result IN ('win', 'loss', 'draw')),
    kills       INT NOT NULL DEFAULT 0,
    deaths      INT NOT NULL DEFAULT 0,
    assists     INT NOT NULL DEFAULT 0,
    mmr_change  INT NOT NULL DEFAULT 0,
    played_at   TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX idx_match_history_user  ON match_history(user_id, played_at DESC);
CREATE INDEX idx_match_history_match ON match_history(match_id);
