ALTER TABLE game_server_capacities
    ADD COLUMN generation BIGINT NOT NULL DEFAULT 0 CHECK (generation >= 0);

ALTER TABLE matches
    ADD COLUMN game_session_generation BIGINT NOT NULL DEFAULT 0
        CHECK (game_session_generation >= 0);

CREATE TABLE match_lobby_admissions (
    match_id    UUID NOT NULL REFERENCES matches(id) ON DELETE CASCADE,
    user_id     UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    admitted_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    PRIMARY KEY (match_id, user_id)
);

CREATE INDEX idx_match_lobby_admissions_user
    ON match_lobby_admissions(user_id, match_id);
