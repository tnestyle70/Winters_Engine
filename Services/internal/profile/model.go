package profile

import (
	"time"

	"github.com/google/uuid"
)

type Profile struct {
	UserID   uuid.UUID `json:"user_id"`
	Username string    `json:"username"`
	MMR      int       `json:"mmr"`
	Rank     int64     `json:"rank"`
	Wins     int       `json:"wins"`
	Losses   int       `json:"losses"`
	WinRate  float64   `json:"win_rate"`
	Kills    int       `json:"kills"`
	Deaths   int       `json:"deaths"`
	Assists  int       `json:"assists"`
	KDA      float64   `json:"kda"`
}

type MatchRecord struct {
	MatchID   uuid.UUID `json:"match_id"`
	Result    string    `json:"result"`
	Kills     int       `json:"kills"`
	Deaths    int       `json:"deaths"`
	Assists   int       `json:"assists"`
	MMRChange int       `json:"mmr_change"`
	PlayedAt  time.Time `json:"played_at"`
}

// Kafka MatchCompleted 이벤트 (프로필 전용, leaderboard 패키지 미의존)
type MatchCompletedEvent struct {
	Type    string                 `json:"type"`
	MatchID uuid.UUID              `json:"match_id"`
	Players []MatchCompletedPlayer `json:"players"`
}

type MatchCompletedPlayer struct {
	UserID    uuid.UUID `json:"user_id"`
	Result    string    `json:"result"`
	Kills     int       `json:"kills"`
	Deaths    int       `json:"deaths"`
	Assists   int       `json:"assists"`
	MMRChange int       `json:"mmr_change"`
}
