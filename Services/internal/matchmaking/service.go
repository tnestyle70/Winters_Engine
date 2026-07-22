package matchmaking

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"log/slog"
	"sort"
	"time"

	"github.com/google/uuid"
	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgxpool"
	"github.com/segmentio/kafka-go"

	apperr "winters-backend/pkg/errors"
	"winters-backend/pkg/matchticket"
)

const matchTTL = 5 * time.Minute

var errCapacityOccupied = errors.New("game server lobby is not accepting players")
var errCapacityMismatch = errors.New("game server capacity belongs to another match")
var errRosterMismatch = errors.New("game session roster does not match")

type Service struct {
	db           *pgxpool.Pool
	writer       *kafka.Writer
	ticketSigner *matchticket.Signer
	allocation   GameAllocation
	maxPlayers   int
}

func NewService(
	db *pgxpool.Pool,
	writer *kafka.Writer,
	ticketSigner *matchticket.Signer,
	allocation GameAllocation,
	maxPlayers int,
) *Service {
	return &Service{
		db: db, writer: writer, ticketSigner: ticketSigner,
		allocation: allocation, maxPlayers: maxPlayers,
	}
}

func (s *Service) Join(ctx context.Context, userID uuid.UUID) (MatchStatus, error) {
	if userID == uuid.Nil || s.ticketSigner == nil ||
		s.allocation.GameSessionID == "" || s.maxPlayers < 1 {
		return MatchStatus{}, apperr.ErrInvalidInput
	}

	tx, err := s.db.Begin(ctx)
	if err != nil {
		return MatchStatus{}, fmt.Errorf("begin lobby admission: %w", err)
	}
	defer tx.Rollback(ctx)

	if _, err := tx.Exec(ctx, `
		INSERT INTO game_server_capacities (game_session_id)
		VALUES ($1)
		ON CONFLICT (game_session_id) DO NOTHING`, s.allocation.GameSessionID); err != nil {
		return MatchStatus{}, fmt.Errorf("ensure game server capacity: %w", err)
	}

	var activeMatchID *uuid.UUID
	var capacityGeneration int64
	if err := tx.QueryRow(ctx, `
		SELECT active_match_id, generation
		FROM game_server_capacities
		WHERE game_session_id = $1
		FOR UPDATE`, s.allocation.GameSessionID).Scan(
		&activeMatchID, &capacityGeneration); err != nil {
		return MatchStatus{}, fmt.Errorf("lock game server capacity: %w", err)
	}

	matchID := uuid.Nil
	generation := capacityGeneration
	if activeMatchID == nil {
		matchID = uuid.New()
		generation++
		if _, err := tx.Exec(ctx, `
			INSERT INTO matches (
				id, status, game_session_id, game_session_generation)
			VALUES ($1, 'allocated', $2, $3)`,
			matchID, s.allocation.GameSessionID, generation); err != nil {
			return MatchStatus{}, fmt.Errorf("create custom lobby match: %w", err)
		}
		if _, err := tx.Exec(ctx, `
			UPDATE game_server_capacities
			SET active_match_id = $2, generation = $3, updated_at = NOW()
			WHERE game_session_id = $1`,
			s.allocation.GameSessionID, matchID, generation); err != nil {
			return MatchStatus{}, fmt.Errorf("allocate custom lobby: %w", err)
		}
	} else {
		matchID = *activeMatchID
		var status string
		if err := tx.QueryRow(ctx, `
			SELECT status, game_session_generation
			FROM matches
			WHERE id = $1
			FOR UPDATE`, matchID).Scan(&status, &generation); err != nil {
			return MatchStatus{}, fmt.Errorf("lock custom lobby match: %w", err)
		}
		if status != "allocated" || generation != capacityGeneration {
			return MatchStatus{}, errCapacityOccupied
		}
	}

	var alreadyAdmitted bool
	if err := tx.QueryRow(ctx, `
		SELECT EXISTS (
			SELECT 1 FROM match_lobby_admissions
			WHERE match_id = $1 AND user_id = $2
		)`, matchID, userID).Scan(&alreadyAdmitted); err != nil {
		return MatchStatus{}, fmt.Errorf("check lobby admission: %w", err)
	}
	if !alreadyAdmitted {
		var admissionCount int
		if err := tx.QueryRow(ctx, `
			SELECT COUNT(*) FROM match_lobby_admissions WHERE match_id = $1`,
			matchID).Scan(&admissionCount); err != nil {
			return MatchStatus{}, fmt.Errorf("count lobby admissions: %w", err)
		}
		if admissionCount >= s.maxPlayers {
			return MatchStatus{}, errCapacityOccupied
		}
		if _, err := tx.Exec(ctx, `
			INSERT INTO match_lobby_admissions (match_id, user_id)
			VALUES ($1, $2)`, matchID, userID); err != nil {
			return MatchStatus{}, fmt.Errorf("insert lobby admission: %w", err)
		}
	}

	if err := tx.Commit(ctx); err != nil {
		return MatchStatus{}, fmt.Errorf("commit lobby admission: %w", err)
	}
	status, err := s.issueMatchStatus(matchID, userID, generation)
	if err != nil {
		return MatchStatus{}, err
	}
	slog.Info("player admitted to custom lobby",
		"match_id", matchID, "user_id", userID, "generation", generation)
	return status, nil
}

func (s *Service) Leave(ctx context.Context, userID uuid.UUID) error {
	status, err := s.Status(ctx, userID)
	if err != nil {
		return err
	}
	if status.Status == "matched" {
		return fmt.Errorf("%w: authenticated lobby admission remains valid for reconnect", apperr.ErrAlreadyExists)
	}
	return nil
}

func (s *Service) Status(ctx context.Context, userID uuid.UUID) (*MatchStatus, error) {
	var matchID uuid.UUID
	var generation int64
	err := s.db.QueryRow(ctx, `
		SELECT m.id, m.game_session_generation
		FROM game_server_capacities c
		JOIN matches m ON m.id = c.active_match_id
		WHERE c.game_session_id = $1
		  AND (
			(m.status = 'allocated' AND EXISTS (
				SELECT 1 FROM match_lobby_admissions a
				WHERE a.match_id = m.id AND a.user_id = $2
			))
			OR
			(m.status IN ('running', 'completed') AND EXISTS (
				SELECT 1 FROM match_participants p
				WHERE p.match_id = m.id AND p.user_id = $2
			))
		  )
		LIMIT 1`, s.allocation.GameSessionID, userID).Scan(&matchID, &generation)
	if errors.Is(err, pgx.ErrNoRows) {
		return &MatchStatus{Status: "none"}, nil
	}
	if err != nil {
		return nil, fmt.Errorf("lookup custom lobby status: %w", err)
	}
	status, err := s.issueMatchStatus(matchID, userID, generation)
	if err != nil {
		return nil, err
	}
	return &status, nil
}

func (s *Service) issueMatchStatus(
	matchID uuid.UUID,
	userID uuid.UUID,
	generation int64,
) (MatchStatus, error) {
	ticket, err := s.ticketSigner.Issue(
		matchID, userID, s.allocation.GameSessionID, generation)
	if err != nil {
		return MatchStatus{}, fmt.Errorf("issue custom lobby ticket: %w", err)
	}
	return MatchStatus{
		Status: "matched", MatchID: matchID.String(),
		GameSessionID: s.allocation.GameSessionID,
		Generation:    generation,
		Host:          s.allocation.Host, Port: s.allocation.Port,
		Transport: s.allocation.Transport, PlayerTicket: ticket,
		ExpiresAt: time.Now().Add(matchTTL).Unix(),
	}, nil
}

func validateStartPlayers(players []LobbyStartPlayer, maximum int) error {
	if len(players) == 0 || len(players) > maximum {
		return apperr.ErrInvalidInput
	}
	users := make(map[uuid.UUID]struct{}, len(players))
	slots := make(map[int16]struct{}, len(players))
	for _, player := range players {
		if player.UserID == uuid.Nil || player.Slot < 0 || player.Slot >= 10 ||
			(player.Team != 0 && player.Team != 1) {
			return apperr.ErrInvalidInput
		}
		if _, duplicate := users[player.UserID]; duplicate {
			return apperr.ErrInvalidInput
		}
		if _, duplicate := slots[player.Slot]; duplicate {
			return apperr.ErrInvalidInput
		}
		users[player.UserID] = struct{}{}
		slots[player.Slot] = struct{}{}
	}
	return nil
}

func normalizeStartPlayers(players []LobbyStartPlayer) []LobbyStartPlayer {
	result := append([]LobbyStartPlayer(nil), players...)
	sort.Slice(result, func(i, j int) bool { return result[i].Slot < result[j].Slot })
	return result
}

func equalStartPlayers(lhs, rhs []LobbyStartPlayer) bool {
	if len(lhs) != len(rhs) {
		return false
	}
	a := normalizeStartPlayers(lhs)
	b := normalizeStartPlayers(rhs)
	for index := range a {
		if a[index] != b[index] {
			return false
		}
	}
	return true
}

func (s *Service) StartGameSession(
	ctx context.Context,
	gameSessionID string,
	matchID uuid.UUID,
	generation int64,
	players []LobbyStartPlayer,
) error {
	if gameSessionID == "" || matchID == uuid.Nil || generation <= 0 ||
		validateStartPlayers(players, s.maxPlayers) != nil {
		return apperr.ErrInvalidInput
	}
	players = normalizeStartPlayers(players)

	tx, err := s.db.Begin(ctx)
	if err != nil {
		return err
	}
	defer tx.Rollback(ctx)

	var activeMatchID *uuid.UUID
	var activeGeneration int64
	if err := tx.QueryRow(ctx, `
		SELECT active_match_id, generation
		FROM game_server_capacities
		WHERE game_session_id = $1
		FOR UPDATE`, gameSessionID).Scan(&activeMatchID, &activeGeneration); err != nil {
		return err
	}
	if activeMatchID == nil || *activeMatchID != matchID || activeGeneration != generation {
		return errCapacityMismatch
	}

	var status string
	if err := tx.QueryRow(ctx, `
		SELECT status FROM matches WHERE id = $1 FOR UPDATE`, matchID).Scan(&status); err != nil {
		return err
	}
	if status == "running" {
		existing, err := loadFinalPlayers(ctx, tx, matchID)
		if err != nil {
			return err
		}
		if !equalStartPlayers(existing, players) {
			return errRosterMismatch
		}
		return tx.Commit(ctx)
	}
	if status != "allocated" {
		return errCapacityOccupied
	}

	for _, player := range players {
		var admitted bool
		if err := tx.QueryRow(ctx, `
			SELECT EXISTS (
				SELECT 1 FROM match_lobby_admissions
				WHERE match_id = $1 AND user_id = $2
			)`, matchID, player.UserID).Scan(&admitted); err != nil {
			return err
		}
		if !admitted {
			return errRosterMismatch
		}
	}

	if _, err := tx.Exec(ctx, `DELETE FROM match_participants WHERE match_id = $1`, matchID); err != nil {
		return err
	}
	userIDs := make([]uuid.UUID, 0, len(players))
	for _, player := range players {
		if _, err := tx.Exec(ctx, `
			INSERT INTO match_participants (
				match_id, user_id, team, slot, joined_at)
			VALUES ($1, $2, $3, $4, NOW())`,
			matchID, player.UserID, player.Team, player.Slot); err != nil {
			return fmt.Errorf("insert final match participant: %w", err)
		}
		userIDs = append(userIDs, player.UserID)
	}

	var avgMMR int
	if err := tx.QueryRow(ctx, `
		SELECT COALESCE(AVG(mmr), 0)::integer
		FROM player_stats WHERE user_id = ANY($1)`, userIDs).Scan(&avgMMR); err != nil {
		return err
	}
	event := MatchCreatedEvent{
		Type: "MatchCreated", MatchID: matchID, Players: userIDs,
		AvgMMR: avgMMR, CreatedAt: time.Now().UTC(),
	}
	payload, err := json.Marshal(event)
	if err != nil {
		return err
	}
	if _, err := tx.Exec(ctx, `
		UPDATE matches
		SET status = 'running', started_at = COALESCE(started_at, NOW())
		WHERE id = $1`, matchID); err != nil {
		return err
	}
	if _, err := tx.Exec(ctx, `DELETE FROM match_lobby_admissions WHERE match_id = $1`, matchID); err != nil {
		return err
	}
	if _, err := tx.Exec(ctx, `
		INSERT INTO match_events_outbox (match_id, event_type, payload)
		VALUES ($1, 'MatchCreated', $2)
		ON CONFLICT (match_id, event_type) DO NOTHING`, matchID, payload); err != nil {
		return err
	}
	return tx.Commit(ctx)
}

func loadFinalPlayers(ctx context.Context, tx pgx.Tx, matchID uuid.UUID) ([]LobbyStartPlayer, error) {
	rows, err := tx.Query(ctx, `
		SELECT user_id, slot, team
		FROM match_participants
		WHERE match_id = $1
		ORDER BY slot`, matchID)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var players []LobbyStartPlayer
	for rows.Next() {
		var player LobbyStartPlayer
		if err := rows.Scan(&player.UserID, &player.Slot, &player.Team); err != nil {
			return nil, err
		}
		players = append(players, player)
	}
	return players, rows.Err()
}

func (s *Service) ActiveGameSession(
	ctx context.Context,
	gameSessionID string,
) (ActiveGameSession, bool, error) {
	var activeMatchID *uuid.UUID
	var generation int64
	var status *string
	err := s.db.QueryRow(ctx, `
		SELECT c.active_match_id, c.generation, m.status
		FROM game_server_capacities c
		LEFT JOIN matches m ON m.id = c.active_match_id
		WHERE c.game_session_id = $1`, gameSessionID).Scan(
		&activeMatchID, &generation, &status)
	if errors.Is(err, pgx.ErrNoRows) {
		return ActiveGameSession{}, false, nil
	}
	if err != nil {
		return ActiveGameSession{}, false, err
	}
	result := ActiveGameSession{Generation: generation}
	if activeMatchID == nil {
		return result, false, nil
	}
	result.MatchID = *activeMatchID
	if status != nil {
		result.Status = *status
	}
	return result, true, nil
}

func (s *Service) AbortGameSession(
	ctx context.Context,
	gameSessionID string,
	matchID uuid.UUID,
	generation int64,
) error {
	tx, err := s.db.Begin(ctx)
	if err != nil {
		return err
	}
	defer tx.Rollback(ctx)
	var activeMatchID *uuid.UUID
	var activeGeneration int64
	if err := tx.QueryRow(ctx, `
		SELECT active_match_id, generation
		FROM game_server_capacities
		WHERE game_session_id = $1 FOR UPDATE`, gameSessionID).Scan(
		&activeMatchID, &activeGeneration); err != nil {
		return err
	}
	if activeMatchID == nil {
		return tx.Commit(ctx)
	}
	if *activeMatchID != matchID || activeGeneration != generation {
		return errCapacityMismatch
	}
	var status string
	if err := tx.QueryRow(ctx, `SELECT status FROM matches WHERE id = $1 FOR UPDATE`, matchID).Scan(&status); err != nil {
		return err
	}
	if status == "completed" {
		return errCapacityOccupied
	}
	if _, err := tx.Exec(ctx, `UPDATE matches SET status = 'aborted' WHERE id = $1`, matchID); err != nil {
		return err
	}
	if _, err := tx.Exec(ctx, `DELETE FROM match_lobby_admissions WHERE match_id = $1`, matchID); err != nil {
		return err
	}
	if _, err := tx.Exec(ctx, `
		UPDATE game_server_capacities
		SET active_match_id = NULL, updated_at = NOW()
		WHERE game_session_id = $1`, gameSessionID); err != nil {
		return err
	}
	return tx.Commit(ctx)
}

func (s *Service) ReleaseGameSession(
	ctx context.Context,
	gameSessionID string,
	matchID uuid.UUID,
) error {
	tx, err := s.db.Begin(ctx)
	if err != nil {
		return err
	}
	defer tx.Rollback(ctx)
	var activeMatchID *uuid.UUID
	var matchStatus *string
	if err := tx.QueryRow(ctx, `
		SELECT c.active_match_id, m.status
		FROM game_server_capacities c
		LEFT JOIN matches m ON m.id = c.active_match_id
		WHERE c.game_session_id = $1 FOR UPDATE OF c`, gameSessionID).Scan(
		&activeMatchID, &matchStatus); err != nil {
		if errors.Is(err, pgx.ErrNoRows) {
			return tx.Commit(ctx)
		}
		return err
	}
	if activeMatchID == nil {
		return tx.Commit(ctx)
	}
	if *activeMatchID != matchID {
		return errCapacityMismatch
	}
	if matchStatus == nil || *matchStatus != "completed" {
		return errCapacityOccupied
	}
	if _, err := tx.Exec(ctx, `
		UPDATE game_server_capacities
		SET active_match_id = NULL, updated_at = NOW()
		WHERE game_session_id = $1`, gameSessionID); err != nil {
		return err
	}
	return tx.Commit(ctx)
}

func (s *Service) RunOutboxPublisher(ctx context.Context) {
	ticker := time.NewTicker(time.Second)
	defer ticker.Stop()
	for {
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
			for index := 0; index < 32; index++ {
				published, err := s.publishNextOutbox(ctx)
				if err != nil {
					slog.Error("publish match outbox", "error", err)
					break
				}
				if !published {
					break
				}
			}
		}
	}
}

func (s *Service) publishNextOutbox(ctx context.Context) (bool, error) {
	if s.writer == nil {
		return false, nil
	}
	tx, err := s.db.Begin(ctx)
	if err != nil {
		return false, err
	}
	defer tx.Rollback(ctx)
	var outboxID uuid.UUID
	var matchID uuid.UUID
	var eventType string
	var payload []byte
	err = tx.QueryRow(ctx, `
		SELECT id, match_id, event_type, payload
		FROM match_events_outbox
		WHERE published_at IS NULL
		ORDER BY created_at
		FOR UPDATE SKIP LOCKED LIMIT 1`).Scan(
		&outboxID, &matchID, &eventType, &payload)
	if errors.Is(err, pgx.ErrNoRows) {
		return false, nil
	}
	if err != nil {
		return false, err
	}
	if err := s.writer.WriteMessages(ctx, kafka.Message{
		Key: []byte(matchID.String()), Value: payload,
	}); err != nil {
		_, _ = tx.Exec(ctx, `
			UPDATE match_events_outbox SET attempt_count = attempt_count + 1
			WHERE id = $1`, outboxID)
		_ = tx.Commit(ctx)
		return false, fmt.Errorf("publish %s: %w", eventType, err)
	}
	if _, err := tx.Exec(ctx, `
		UPDATE match_events_outbox
		SET published_at = NOW(), attempt_count = attempt_count + 1
		WHERE id = $1`, outboxID); err != nil {
		return false, err
	}
	if err := tx.Commit(ctx); err != nil {
		return false, err
	}
	return true, nil
}
