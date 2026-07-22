CREATE TABLE matches (
    id              UUID PRIMARY KEY,
    status          VARCHAR(16) NOT NULL DEFAULT 'created'
        CHECK (status IN ('created', 'allocated', 'running', 'completed', 'aborted')),
    game_session_id TEXT,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    started_at      TIMESTAMPTZ,
    completed_at    TIMESTAMPTZ
);

CREATE TABLE match_participants (
    match_id     UUID NOT NULL REFERENCES matches(id) ON DELETE CASCADE,
    user_id      UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    team         SMALLINT,
    slot         SMALLINT,
    champion_key TEXT,
    result       VARCHAR(16)
        CHECK (result IS NULL OR result IN ('win', 'loss', 'draw', 'aborted')),
    joined_at    TIMESTAMPTZ,
    PRIMARY KEY (match_id, user_id),
    UNIQUE (match_id, slot)
);

CREATE INDEX idx_match_participants_user_match
    ON match_participants(user_id, match_id);

CREATE TABLE match_events_outbox (
    id           UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    match_id     UUID NOT NULL REFERENCES matches(id) ON DELETE CASCADE,
    event_type   TEXT NOT NULL,
    payload      JSONB NOT NULL,
    attempt_count INT NOT NULL DEFAULT 0,
    created_at   TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    published_at TIMESTAMPTZ
);

CREATE INDEX idx_match_events_outbox_pending
    ON match_events_outbox(created_at)
    WHERE published_at IS NULL;

CREATE TABLE replays (
    id              UUID PRIMARY KEY,
    match_id        UUID NOT NULL UNIQUE REFERENCES matches(id) ON DELETE CASCADE,
    status          VARCHAR(16) NOT NULL DEFAULT 'uploading'
        CHECK (status IN ('uploading', 'ready', 'failed', 'expired', 'deleted')),
    object_key      TEXT NOT NULL UNIQUE,
    upload_id       TEXT,
    size_bytes      BIGINT CHECK (size_bytes IS NULL OR size_bytes >= 0),
    checksum_sha256 CHAR(64),
    format_version  SMALLINT NOT NULL,
    tick_rate       INTEGER NOT NULL CHECK (tick_rate > 0),
    record_count    BIGINT NOT NULL DEFAULT 0 CHECK (record_count >= 0),
    snapshot_count  BIGINT NOT NULL DEFAULT 0 CHECK (snapshot_count >= 0),
    event_count     BIGINT NOT NULL DEFAULT 0 CHECK (event_count >= 0),
    command_count   BIGINT NOT NULL DEFAULT 0 CHECK (command_count >= 0),
    first_tick      BIGINT NOT NULL DEFAULT 0 CHECK (first_tick >= 0),
    last_tick       BIGINT NOT NULL DEFAULT 0 CHECK (last_tick >= first_tick),
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    ready_at        TIMESTAMPTZ,
    expires_at      TIMESTAMPTZ
);

CREATE INDEX idx_replays_status_created
    ON replays(status, created_at DESC);

CREATE TABLE replay_user_library (
    replay_id          UUID NOT NULL REFERENCES replays(id) ON DELETE CASCADE,
    user_id            UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    hidden_at          TIMESTAMPTZ,
    last_downloaded_at TIMESTAMPTZ,
    keep_until         TIMESTAMPTZ,
    PRIMARY KEY (replay_id, user_id)
);

CREATE INDEX idx_replay_user_library_user
    ON replay_user_library(user_id, replay_id);

ALTER TABLE match_history
    ADD CONSTRAINT uq_match_history_user_match UNIQUE (user_id, match_id);
