DROP INDEX IF EXISTS idx_replay_user_library_visible;

ALTER TABLE match_events_outbox
    DROP CONSTRAINT IF EXISTS uq_match_events_outbox_match_type;
