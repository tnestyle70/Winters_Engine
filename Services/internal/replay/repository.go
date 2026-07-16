package replay

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"time"

	"github.com/google/uuid"
	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgxpool"
	apperr "winters-backend/pkg/errors"
)

type Repository struct {
	db *pgxpool.Pool
}

func NewRepository(db *pgxpool.Pool) *Repository {
	return &Repository{db: db}
}

func scanReplay(row pgx.Row) (Replay, error) {
	var item Replay
	err := row.Scan(
		&item.ID, &item.MatchID, &item.Status, &item.ObjectKey, &item.UploadID,
		&item.SizeBytes, &item.ChecksumSHA256, &item.FormatVersion, &item.TickRate,
		&item.RecordCount, &item.SnapshotCount, &item.EventCount, &item.CommandCount,
		&item.FirstTick, &item.LastTick, &item.CreatedAt, &item.ReadyAt, &item.ExpiresAt,
	)
	return item, err
}

const replayColumns = `
    id, match_id, status, object_key, COALESCE(upload_id, ''),
    COALESCE(size_bytes, 0), COALESCE(checksum_sha256, ''), format_version, tick_rate,
    record_count, snapshot_count, event_count, command_count,
    first_tick, last_tick, created_at, ready_at, expires_at`

func (r *Repository) ReserveUpload(
	ctx context.Context,
	replayID uuid.UUID,
	objectKey string,
	req CreateUploadRequest,
	expiresAt time.Time,
) (Replay, bool, error) {
	tx, err := r.db.Begin(ctx)
	if err != nil {
		return Replay{}, false, fmt.Errorf("begin reserve replay: %w", err)
	}
	defer tx.Rollback(ctx)

	var matchStatus string
	if err := tx.QueryRow(ctx,
		`SELECT status FROM matches WHERE id = $1 FOR UPDATE`, req.MatchID,
	).Scan(&matchStatus); err != nil {
		if errors.Is(err, pgx.ErrNoRows) {
			return Replay{}, false, apperr.ErrNotFound
		}
		return Replay{}, false, fmt.Errorf("lock match: %w", err)
	}
	if matchStatus == "aborted" {
		return Replay{}, false, apperr.ErrIdempotencyConflict
	}

	existing, err := scanReplay(tx.QueryRow(ctx,
		`SELECT `+replayColumns+` FROM replays WHERE match_id = $1`, req.MatchID))
	if err == nil {
		if err := tx.Commit(ctx); err != nil {
			return Replay{}, false, fmt.Errorf("commit existing replay lookup: %w", err)
		}
		return existing, false, nil
	}
	if !errors.Is(err, pgx.ErrNoRows) {
		return Replay{}, false, fmt.Errorf("query replay by match: %w", err)
	}

	var participantCount int
	if err := tx.QueryRow(ctx,
		`SELECT COUNT(*) FROM match_participants WHERE match_id = $1`, req.MatchID,
	).Scan(&participantCount); err != nil {
		return Replay{}, false, fmt.Errorf("count match participants: %w", err)
	}
	if participantCount == 0 {
		return Replay{}, false, apperr.ErrInvalidInput
	}

	created, err := scanReplay(tx.QueryRow(ctx,
		`INSERT INTO replays (
		    id, match_id, status, object_key, size_bytes, checksum_sha256,
		    format_version, tick_rate, record_count, snapshot_count,
		    event_count, command_count, first_tick, last_tick, expires_at
		 ) VALUES (
		    $1, $2, 'uploading', $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14
		 ) RETURNING `+replayColumns,
		replayID, req.MatchID, objectKey, req.SizeBytes, req.ChecksumSHA256,
		req.FormatVersion, req.TickRate, req.RecordCount, req.SnapshotCount,
		req.EventCount, req.CommandCount, req.FirstTick, req.LastTick, expiresAt,
	))
	if err != nil {
		return Replay{}, false, fmt.Errorf("insert replay: %w", err)
	}
	if err := tx.Commit(ctx); err != nil {
		return Replay{}, false, fmt.Errorf("commit reserve replay: %w", err)
	}
	return created, true, nil
}

func (r *Repository) SetUploadID(ctx context.Context, replayID uuid.UUID, uploadID string) error {
	command, err := r.db.Exec(ctx,
		`UPDATE replays SET upload_id = $2
		 WHERE id = $1 AND status = 'uploading' AND upload_id IS NULL`,
		replayID, uploadID)
	if err != nil {
		return fmt.Errorf("set replay upload id: %w", err)
	}
	if command.RowsAffected() != 1 {
		return apperr.ErrIdempotencyConflict
	}
	return nil
}

func (r *Repository) Get(ctx context.Context, replayID uuid.UUID) (Replay, error) {
	item, err := scanReplay(r.db.QueryRow(ctx,
		`SELECT `+replayColumns+` FROM replays WHERE id = $1`, replayID))
	if errors.Is(err, pgx.ErrNoRows) {
		return Replay{}, apperr.ErrNotFound
	}
	if err != nil {
		return Replay{}, fmt.Errorf("get replay: %w", err)
	}
	return item, nil
}

func (r *Repository) GetAuthorized(
	ctx context.Context,
	replayID, userID uuid.UUID,
) (Replay, error) {
	item, err := scanReplay(r.db.QueryRow(ctx,
		`SELECT `+replayColumns+`
		 FROM replays r
		 JOIN replay_user_library l ON l.replay_id = r.id
		 WHERE r.id = $1 AND l.user_id = $2 AND l.hidden_at IS NULL`,
		replayID, userID))
	if errors.Is(err, pgx.ErrNoRows) {
		return Replay{}, apperr.ErrForbidden
	}
	if err != nil {
		return Replay{}, fmt.Errorf("get authorized replay: %w", err)
	}
	return item, nil
}

func (r *Repository) MarkFailed(ctx context.Context, replayID uuid.UUID) error {
	_, err := r.db.Exec(ctx,
		`UPDATE replays SET status = 'failed' WHERE id = $1 AND status = 'uploading'`,
		replayID)
	return err
}

func (r *Repository) MarkReady(ctx context.Context, replayID uuid.UUID) (Replay, error) {
	tx, err := r.db.Begin(ctx)
	if err != nil {
		return Replay{}, fmt.Errorf("begin ready replay: %w", err)
	}
	defer tx.Rollback(ctx)

	item, err := scanReplay(tx.QueryRow(ctx,
		`SELECT `+replayColumns+` FROM replays WHERE id = $1 FOR UPDATE`, replayID))
	if errors.Is(err, pgx.ErrNoRows) {
		return Replay{}, apperr.ErrNotFound
	}
	if err != nil {
		return Replay{}, fmt.Errorf("lock replay: %w", err)
	}
	if item.Status != "ready" {
		if item.Status != "uploading" {
			return Replay{}, apperr.ErrIdempotencyConflict
		}
		item, err = scanReplay(tx.QueryRow(ctx,
			`UPDATE replays SET status = 'ready', ready_at = NOW()
			 WHERE id = $1 RETURNING `+replayColumns, replayID))
		if err != nil {
			return Replay{}, fmt.Errorf("mark replay ready: %w", err)
		}
		if _, err := tx.Exec(ctx,
			`INSERT INTO replay_user_library (replay_id, user_id, keep_until)
			 SELECT $1, user_id, $2 FROM match_participants WHERE match_id = $3
			 ON CONFLICT (replay_id, user_id) DO NOTHING`,
			replayID, item.ExpiresAt, item.MatchID); err != nil {
			return Replay{}, fmt.Errorf("populate replay library: %w", err)
		}
	}

	if err := tx.Commit(ctx); err != nil {
		return Replay{}, fmt.Errorf("commit ready replay: %w", err)
	}
	return item, nil
}

func (r *Repository) ListAuthorized(
	ctx context.Context,
	userID uuid.UUID,
	limit int,
	cursorTime *time.Time,
	cursorID uuid.UUID,
) ([]Replay, error) {
	rows, err := r.db.Query(ctx,
		`SELECT `+replayColumns+`, l.last_downloaded_at IS NOT NULL
		 FROM replays r
		 JOIN replay_user_library l ON l.replay_id = r.id
		 WHERE l.user_id = $1 AND l.hidden_at IS NULL AND r.status = 'ready'
		   AND ($2::timestamptz IS NULL OR (r.created_at, r.id) < ($2, $3))
		 ORDER BY r.created_at DESC, r.id DESC LIMIT $4`,
		userID, cursorTime, cursorID, limit)
	if err != nil {
		return nil, fmt.Errorf("list replays: %w", err)
	}
	defer rows.Close()

	items := make([]Replay, 0, limit)
	for rows.Next() {
		item, err := scanReplayWithDownloaded(rows)
		if err != nil {
			return nil, fmt.Errorf("scan replay page: %w", err)
		}
		items = append(items, item)
	}
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("iterate replay page: %w", err)
	}
	return items, nil
}

func scanReplayWithDownloaded(row pgx.Row) (Replay, error) {
	var item Replay
	err := row.Scan(
		&item.ID, &item.MatchID, &item.Status, &item.ObjectKey, &item.UploadID,
		&item.SizeBytes, &item.ChecksumSHA256, &item.FormatVersion, &item.TickRate,
		&item.RecordCount, &item.SnapshotCount, &item.EventCount, &item.CommandCount,
		&item.FirstTick, &item.LastTick, &item.CreatedAt, &item.ReadyAt, &item.ExpiresAt,
		&item.Downloaded,
	)
	return item, err
}

func (r *Repository) MarkDownloaded(ctx context.Context, replayID, userID uuid.UUID) error {
	command, err := r.db.Exec(ctx,
		`UPDATE replay_user_library SET last_downloaded_at = NOW()
		 WHERE replay_id = $1 AND user_id = $2 AND hidden_at IS NULL`,
		replayID, userID)
	if err != nil {
		return fmt.Errorf("mark replay downloaded: %w", err)
	}
	if command.RowsAffected() != 1 {
		return apperr.ErrForbidden
	}
	return nil
}

func (r *Repository) Hide(ctx context.Context, replayID, userID uuid.UUID) error {
	command, err := r.db.Exec(ctx,
		`UPDATE replay_user_library SET hidden_at = COALESCE(hidden_at, NOW())
		 WHERE replay_id = $1 AND user_id = $2`, replayID, userID)
	if err != nil {
		return fmt.Errorf("hide replay: %w", err)
	}
	if command.RowsAffected() != 1 {
		return apperr.ErrForbidden
	}
	return nil
}

func (r *Repository) CompleteMatch(
	ctx context.Context,
	matchID uuid.UUID,
	players []MatchCompletionPlayer,
) error {
	tx, err := r.db.Begin(ctx)
	if err != nil {
		return fmt.Errorf("begin match completion: %w", err)
	}
	defer tx.Rollback(ctx)

	var status string
	if err := tx.QueryRow(ctx,
		`SELECT status FROM matches WHERE id = $1 FOR UPDATE`, matchID,
	).Scan(&status); err != nil {
		if errors.Is(err, pgx.ErrNoRows) {
			return apperr.ErrNotFound
		}
		return fmt.Errorf("lock match for completion: %w", err)
	}
	if status == "aborted" {
		return apperr.ErrIdempotencyConflict
	}

	rows, err := tx.Query(ctx,
		`SELECT user_id, result FROM match_participants WHERE match_id = $1 FOR UPDATE`,
		matchID)
	if err != nil {
		return fmt.Errorf("lock match participants: %w", err)
	}
	existing := make(map[uuid.UUID]string)
	for rows.Next() {
		var userID uuid.UUID
		var result *string
		if err := rows.Scan(&userID, &result); err != nil {
			rows.Close()
			return fmt.Errorf("scan match participant: %w", err)
		}
		if result != nil {
			existing[userID] = *result
		} else {
			existing[userID] = ""
		}
	}
	if err := rows.Err(); err != nil {
		rows.Close()
		return fmt.Errorf("iterate match participants: %w", err)
	}
	rows.Close()
	if len(existing) == 0 || len(existing) != len(players) {
		return apperr.ErrInvalidInput
	}

	seen := make(map[uuid.UUID]struct{}, len(players))
	for _, player := range players {
		if player.UserID == uuid.Nil || (player.Result != "win" && player.Result != "loss" && player.Result != "draw") {
			return apperr.ErrInvalidInput
		}
		current, ok := existing[player.UserID]
		if !ok {
			return apperr.ErrForbidden
		}
		if _, duplicate := seen[player.UserID]; duplicate {
			return apperr.ErrInvalidInput
		}
		seen[player.UserID] = struct{}{}
		if status == "completed" && current != player.Result {
			return apperr.ErrIdempotencyConflict
		}
	}
	if status == "completed" {
		return tx.Commit(ctx)
	}

	eventPlayers := make([]map[string]any, 0, len(players))
	for _, player := range players {
		if _, err := tx.Exec(ctx,
			`UPDATE match_participants
			 SET result = $3, joined_at = COALESCE(joined_at, NOW())
			 WHERE match_id = $1 AND user_id = $2`,
			matchID, player.UserID, player.Result); err != nil {
			return fmt.Errorf("update participant result: %w", err)
		}
		mmrChange := 0
		if player.Result == "win" {
			mmrChange = 25
		} else if player.Result == "loss" {
			mmrChange = -25
		}
		eventPlayers = append(eventPlayers, map[string]any{
			"user_id": player.UserID, "result": player.Result,
			"kills": player.Kills, "deaths": player.Deaths, "assists": player.Assists,
			"mmr_change": mmrChange,
		})
	}
	payload, err := json.Marshal(map[string]any{
		"type": "MatchCompleted", "match_id": matchID, "players": eventPlayers,
	})
	if err != nil {
		return fmt.Errorf("marshal match completion event: %w", err)
	}
	if _, err := tx.Exec(ctx,
		`UPDATE matches SET status = 'completed', completed_at = NOW() WHERE id = $1`,
		matchID); err != nil {
		return fmt.Errorf("complete match: %w", err)
	}
	if _, err := tx.Exec(ctx,
		`INSERT INTO match_events_outbox (match_id, event_type, payload)
		 VALUES ($1, 'MatchCompleted', $2)
		 ON CONFLICT (match_id, event_type) DO NOTHING`,
		matchID, payload); err != nil {
		return fmt.Errorf("enqueue match completion: %w", err)
	}
	if err := tx.Commit(ctx); err != nil {
		return fmt.Errorf("commit match completion: %w", err)
	}
	return nil
}
