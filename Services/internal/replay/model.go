package replay

import (
	"time"

	"github.com/google/uuid"
)

type Replay struct {
	ID               uuid.UUID  `json:"replay_id"`
	MatchID          uuid.UUID  `json:"match_id"`
	Status           string     `json:"status"`
	ObjectKey        string     `json:"-"`
	UploadID         string     `json:"-"`
	SizeBytes        int64      `json:"size_bytes"`
	ChecksumSHA256   string     `json:"checksum_sha256"`
	FormatVersion    int        `json:"format_version"`
	TickRate         int        `json:"tick_rate"`
	RecordCount      int64      `json:"record_count"`
	SnapshotCount    int64      `json:"snapshot_count"`
	EventCount       int64      `json:"event_count"`
	CommandCount     int64      `json:"command_count"`
	FirstTick        int64      `json:"first_tick"`
	LastTick         int64      `json:"last_tick"`
	CreatedAt        time.Time  `json:"created_at"`
	ReadyAt          *time.Time `json:"ready_at,omitempty"`
	ExpiresAt        *time.Time `json:"expires_at,omitempty"`
	Downloaded       bool       `json:"downloaded"`
	PerspectiveNetID int64      `json:"perspective_net_id"`
}

type CreateUploadRequest struct {
	MatchID        uuid.UUID `json:"match_id"`
	SizeBytes      int64     `json:"size_bytes"`
	ChecksumSHA256 string    `json:"checksum_sha256"`
	FormatVersion  int       `json:"format_version"`
	TickRate       int       `json:"tick_rate"`
	RecordCount    int64     `json:"record_count"`
	SnapshotCount  int64     `json:"snapshot_count"`
	EventCount     int64     `json:"event_count"`
	CommandCount   int64     `json:"command_count"`
	FirstTick      int64     `json:"first_tick"`
	LastTick       int64     `json:"last_tick"`
}

type UploadSession struct {
	Replay   Replay `json:"replay"`
	PartSize int64  `json:"part_size"`
}

type PresignPartsRequest struct {
	PartNumbers []int32 `json:"part_numbers"`
}

type PresignedPart struct {
	PartNumber int32     `json:"part_number"`
	URL        string    `json:"url"`
	ExpiresAt  time.Time `json:"expires_at"`
}

type CompletedPart struct {
	PartNumber int32  `json:"part_number"`
	ETag       string `json:"etag"`
}

type CompleteUploadRequest struct {
	Parts          []CompletedPart `json:"parts"`
	SizeBytes      int64           `json:"size_bytes"`
	ChecksumSHA256 string          `json:"checksum_sha256"`
}

type DownloadGrant struct {
	URL       string    `json:"url"`
	ExpiresAt time.Time `json:"expires_at"`
}

type ReplayPage struct {
	Items      []Replay `json:"items"`
	NextCursor string   `json:"next_cursor,omitempty"`
}

type MatchCompletionPlayer struct {
	UserID           uuid.UUID `json:"user_id"`
	Result           string    `json:"result"`
	PerspectiveNetID int64     `json:"perspective_net_id"`
	Kills            int       `json:"kills"`
	Deaths           int       `json:"deaths"`
	Assists          int       `json:"assists"`
}

type MatchCompletionRequest struct {
	Players []MatchCompletionPlayer `json:"players"`
}
