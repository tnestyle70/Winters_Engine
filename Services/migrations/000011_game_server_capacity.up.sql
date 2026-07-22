CREATE UNIQUE INDEX uq_matches_active_game_session
    ON matches(game_session_id)
    WHERE game_session_id IS NOT NULL
      AND status IN ('allocated', 'running');

CREATE TABLE game_server_capacities (
    game_session_id TEXT PRIMARY KEY,
    active_match_id UUID REFERENCES matches(id),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
