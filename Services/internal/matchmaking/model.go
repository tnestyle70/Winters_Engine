package matchmaking

import (
	"time"

	"github.com/google/uuid"
)

type MatchStatus struct {
	// matched, none
	Status        string `json:"status"`
	MatchID       string `json:"match_id,omitempty"`
	GameSessionID string `json:"game_session_id,omitempty"`
	Generation    int64  `json:"generation,omitempty"`
	Host          string `json:"host,omitempty"`
	Port          int    `json:"port,omitempty"`
	Transport     string `json:"transport,omitempty"`
	PlayerTicket  string `json:"player_ticket,omitempty"`
	ExpiresAt     int64  `json:"expires_at,omitempty"`
}

type GameAllocation struct {
	Host          string
	Port          int
	Transport     string
	GameSessionID string
}

type MatchCreatedEvent struct {
	Type      string      `json:"type"`
	MatchID   uuid.UUID   `json:"match_id"`
	Players   []uuid.UUID `json:"players"`
	AvgMMR    int         `json:"avg_mmr"`
	CreatedAt time.Time   `json:"created_at"`
}

type LobbyStartPlayer struct {
	UserID uuid.UUID `json:"user_id"`
	Slot   int16     `json:"slot"`
	Team   int16     `json:"team"`
}

type ActiveGameSession struct {
	MatchID    uuid.UUID
	Generation int64
	Status     string
}
