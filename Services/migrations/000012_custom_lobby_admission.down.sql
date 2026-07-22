DROP TABLE IF EXISTS match_lobby_admissions;

ALTER TABLE matches
    DROP COLUMN IF EXISTS game_session_generation;

ALTER TABLE game_server_capacities
    DROP COLUMN IF EXISTS generation;
