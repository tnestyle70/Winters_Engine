ALTER TABLE match_history
    DROP CONSTRAINT IF EXISTS uq_match_history_user_match;

DROP TABLE IF EXISTS replay_user_library;
DROP TABLE IF EXISTS replays;
DROP TABLE IF EXISTS match_events_outbox;
DROP TABLE IF EXISTS match_participants;
DROP TABLE IF EXISTS matches;
