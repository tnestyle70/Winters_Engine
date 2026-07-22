ALTER TABLE match_events_outbox
    ADD CONSTRAINT uq_match_events_outbox_match_type UNIQUE (match_id, event_type);

CREATE INDEX idx_replay_user_library_visible
    ON replay_user_library(user_id, replay_id)
    WHERE hidden_at IS NULL;
