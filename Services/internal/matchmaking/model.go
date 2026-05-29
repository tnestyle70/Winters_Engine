package matchmaking

import (
	"time"
	"github.com/google/uuid"
)

type QueueEntry struct {
	UserID   uuid.UUID `json:"user_id"`
	MMR      int       `json:"mmr"`
	JoinedAt time.Time `json:"joined_at"`
}


type MatchStatus struct {
	//queued, matched, none
	Status string `json:"status"`
	MatchID string `json:"match_id,omitempty"`
}

type MatchCreatedEvent struct {
	Type      string      `json:"type"`
	MatchID   uuid.UUID   `json:"match_id"`
	Players   []uuid.UUID `json:"players"`
	AvgMMR    int         `json:"avg_mmr"`
	CreatedAt time.Time   `json:"created_at"`
}

