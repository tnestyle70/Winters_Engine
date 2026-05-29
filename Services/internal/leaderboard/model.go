package leaderboard

import "github.com/google/uuid"

type LeaderboardEntry struct {
	Rank     int64     `json:"rank"`
	UserID   uuid.UUID `json:"user_id"`
	Username string    `json:"username"`
	MMR      int       `json:"mmr"`
	Wins     int       `json:"wins"`
	Losses   int       `json:"losses"`
}

type RankResponse struct {
	Rank         int64     `json:"rank"`
	UserID       uuid.UUID `json:"user_id"`
	MMR          int       `json:"mmr"`
	TotalPlayers int64     `json:"total_players"`
}

type UpdateRequest struct {
	UserID    uuid.UUID `json:"user_id"`
	MMRChange int       `json:"mmr_change"`
}

type MatchEvent struct {
	Type    string        `json:"type"`
	MatchID uuid.UUID     `json:"match_id"`
	Players []MatchPlayer `json:"players"`
}

type MatchPlayer struct {
	UserID    uuid.UUID `json:"user_id"`
	MMRChange int       `json:"mmr_change"`
}