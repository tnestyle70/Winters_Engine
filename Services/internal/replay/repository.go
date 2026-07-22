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

const authorizedReplayColumns = `
    r.id, r.match_id, r.status, r.object_key, COALESCE(r.upload_id, ''),
    COALESCE(r.size_bytes, 0), COALESCE(r.checksum_sha256, ''),
    r.format_version, r.tick_rate, r.record_count, r.snapshot_count,
    r.event_count, r.command_count, r.first_tick, r.last_tick,
    r.created_at, r.ready_at, r.expires_at`

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
	item, err := scanReplayWithPerspective(r.db.QueryRow(ctx,
		`SELECT `+authorizedReplayColumns+`, COALESCE(mp.replay_net_id, 0)
		 FROM replays r
		 JOIN replay_user_library l ON l.replay_id = r.id
		 JOIN match_participants mp
		   ON mp.match_id = r.match_id AND mp.user_id = l.user_id
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
		`SELECT `+authorizedReplayColumns+`,
		        l.last_downloaded_at IS NOT NULL,
		        COALESCE(mp.replay_net_id, 0)
		 FROM replays r
		 JOIN replay_user_library l ON l.replay_id = r.id
		 JOIN match_participants mp
		   ON mp.match_id = r.match_id AND mp.user_id = l.user_id
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
		item, err := scanReplayWithDownloadedAndPerspective(rows)
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

func scanReplayWithPerspective(row pgx.Row) (Replay, error) {
	var item Replay
	err := row.Scan(
		&item.ID, &item.MatchID, &item.Status, &item.ObjectKey, &item.UploadID,
		&item.SizeBytes, &item.ChecksumSHA256, &item.FormatVersion, &item.TickRate,
		&item.RecordCount, &item.SnapshotCount, &item.EventCount, &item.CommandCount,
		&item.FirstTick, &item.LastTick, &item.CreatedAt, &item.ReadyAt, &item.ExpiresAt,
		&item.PerspectiveNetID,
	)
	return item, err
}

func scanReplayWithDownloadedAndPerspective(row pgx.Row) (Replay, error) {
	var item Replay
	err := row.Scan(
		&item.ID, &item.MatchID, &item.Status, &item.ObjectKey, &item.UploadID,
		&item.SizeBytes, &item.ChecksumSHA256, &item.FormatVersion, &item.TickRate,
		&item.RecordCount, &item.SnapshotCount, &item.EventCount, &item.CommandCount,
		&item.FirstTick, &item.LastTick, &item.CreatedAt, &item.ReadyAt, &item.ExpiresAt,
		&item.Downloaded, &item.PerspectiveNetID,
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
	if err := validateMatchCompletionPlayers(players); err != nil {
		return err
	}

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

	type storedParticipant struct {
		result           string
		perspectiveNetID int64
	}
	rows, err := tx.Query(ctx,
		`SELECT user_id, result, COALESCE(replay_net_id, 0)
		 FROM match_participants WHERE match_id = $1 FOR UPDATE`,
		matchID)
	if err != nil {
		return fmt.Errorf("lock match participants: %w", err)
	}
	existing := make(map[uuid.UUID]storedParticipant)
	for rows.Next() {
		var userID uuid.UUID
		var result *string
		var perspectiveNetID int64
		if err := rows.Scan(&userID, &result, &perspectiveNetID); err != nil {
			rows.Close()
			return fmt.Errorf("scan match participant: %w", err)
		}
		stored := storedParticipant{perspectiveNetID: perspectiveNetID}
		if result != nil {
			stored.result = *result
		}
		existing[userID] = stored
	}
	if err := rows.Err(); err != nil {
		rows.Close()
		return fmt.Errorf("iterate match participants: %w", err)
	}
	rows.Close()

	// The custom lobby admits accounts immediately, but the authoritative server
	// decides the final roster. Convert only the users present in the server's
	// completion artifact into durable match participants.
	if len(existing) == 0 && status != "completed" {
		for index, player := range players {
			var admitted bool
			if err := tx.QueryRow(ctx, `
				SELECT EXISTS (
					SELECT 1 FROM match_lobby_admissions
					WHERE match_id = $1 AND user_id = $2
				)`, matchID, player.UserID).Scan(&admitted); err != nil {
				return fmt.Errorf("check final participant admission: %w", err)
			}
			if !admitted {
				return apperr.ErrForbidden
			}
			if _, err := tx.Exec(ctx, `
				INSERT INTO match_participants (
					match_id, user_id, slot, result, replay_net_id, joined_at)
				VALUES ($1, $2, $3, $4, NULLIF($5, 0), NOW())`,
				matchID, player.UserID, index, player.Result,
				player.PerspectiveNetID); err != nil {
				return fmt.Errorf("insert final match participant: %w", err)
			}
			existing[player.UserID] = storedParticipant{
				result:           player.Result,
				perspectiveNetID: player.PerspectiveNetID,
			}
		}
	}
	if len(existing) == 0 || len(existing) != len(players) {
		return apperr.ErrInvalidInput
	}
	for _, player := range players {
		current, ok := existing[player.UserID]
		if !ok {
			return apperr.ErrForbidden
		}
		if status == "completed" {
			if current.result != player.Result ||
				(current.perspectiveNetID != 0 &&
					player.PerspectiveNetID != 0 &&
					current.perspectiveNetID != player.PerspectiveNetID) {
				return apperr.ErrIdempotencyConflict
			}
			if current.perspectiveNetID == 0 && player.PerspectiveNetID != 0 {
				if _, err := tx.Exec(ctx, `
					UPDATE match_participants SET replay_net_id = $3
					WHERE match_id = $1 AND user_id = $2 AND replay_net_id IS NULL`,
					matchID, player.UserID, player.PerspectiveNetID); err != nil {
					return fmt.Errorf("backfill replay perspective: %w", err)
				}
			}
		}
	}
	if status == "completed" {
		if err := tx.Commit(ctx); err != nil {
			return fmt.Errorf("commit completed match lookup: %w", err)
		}
		return nil
	}

	eventPlayers := make([]map[string]any, 0, len(players))
	for _, player := range players {
		if _, err := tx.Exec(ctx,
			`UPDATE match_participants
			 SET result = $3,
			     replay_net_id = COALESCE(NULLIF($4, 0), replay_net_id),
			     joined_at = COALESCE(joined_at, NOW())
			 WHERE match_id = $1 AND user_id = $2`,
			matchID, player.UserID, player.Result,
			player.PerspectiveNetID); err != nil {
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
			"perspective_net_id": player.PerspectiveNetID,
			"kills":              player.Kills, "deaths": player.Deaths, "assists": player.Assists,
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
		`DELETE FROM match_lobby_admissions WHERE match_id = $1`, matchID); err != nil {
		return fmt.Errorf("clear lobby admissions: %w", err)
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
