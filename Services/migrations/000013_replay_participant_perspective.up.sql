ALTER TABLE match_participants
    ADD COLUMN replay_net_id BIGINT
        CHECK (replay_net_id IS NULL OR
               (replay_net_id >= 1 AND replay_net_id <= 4294967295));

CREATE UNIQUE INDEX idx_match_participants_replay_net_id
    ON match_participants(match_id, replay_net_id)
    WHERE replay_net_id IS NOT NULL;
