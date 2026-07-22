DROP INDEX IF EXISTS idx_match_participants_replay_net_id;

ALTER TABLE match_participants
    DROP COLUMN IF EXISTS replay_net_id;
