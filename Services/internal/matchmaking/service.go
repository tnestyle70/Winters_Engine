package matchmaking

import (
	"context"
	"encoding/json"
	"fmt"
	"log/slog"
	"strconv"
	"strings"
	"time"

	"github.com/google/uuid"
	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgxpool"
	"github.com/redis/go-redis/v9"
	"github.com/segmentio/kafka-go"

	apperr "winters-backend/pkg/errors"
	"winters-backend/pkg/matchticket"
)

const (
	queueKey       = "matchmaking:queue"
	statusPrefix   = "matchmaking:status:"
	joinTimePrefix = "matchmaking:jointime:"
	matchSize      = 2
	baseRange      = 200
	rangeExpand    = 50
	statusTTL      = 600 * time.Second
	matchTTL       = 300 * time.Second
)

type Service struct {
	db           *pgxpool.Pool
	rdb          *redis.Client
	writer       *kafka.Writer
	ticketSigner *matchticket.Signer
	allocation   GameAllocation
}

func NewService(
	db *pgxpool.Pool,
	rdb *redis.Client,
	writer *kafka.Writer,
	ticketSigner *matchticket.Signer,
	allocation GameAllocation,
) *Service {
	return &Service{
		db:           db,
		rdb:          rdb,
		writer:       writer,
		ticketSigner: ticketSigner,
		allocation:   allocation,
	}
}

func (s *Service) Join(ctx context.Context, userId uuid.UUID) error {
	existing, err := s.rdb.Get(ctx, statusPrefix+userId.String()).Result()
	if err == nil && existing != "" {
		return fmt.Errorf("%w: already in queue or matched", apperr.ErrAlreadyExists)
	}

	var mmr int
	err = s.db.QueryRow(ctx,
		`SELECT mmr FROM player_stats WHERE user_id = $1`, userId,
	).Scan(&mmr)
	if err != nil {
		return fmt.Errorf("get mmr: %w", err)
	}

	if err := s.rdb.ZAdd(ctx, queueKey, redis.Z{
		Score:  float64(mmr),
		Member: userId.String(),
	}).Err(); err != nil {
		return fmt.Errorf("zadd queue: %w", err)
	}

	s.rdb.Set(ctx, joinTimePrefix+userId.String(),
		time.Now().Unix(), statusTTL)
	s.rdb.Set(ctx, statusPrefix+userId.String(),
		"queued", statusTTL)

	slog.Info("player joined queue", "user_id", userId, "mmr", mmr)
	return nil
}

func (s *Service) Leave(ctx context.Context, userId uuid.UUID) error {
	s.rdb.ZRem(ctx, queueKey, userId.String())
	s.rdb.Del(ctx, statusPrefix+userId.String())
	s.rdb.Del(ctx, joinTimePrefix+userId.String())
	slog.Info("player left queue", "user_id", userId)
	return nil
}

func (s *Service) Status(ctx context.Context, userId uuid.UUID) (*MatchStatus, error) {
	val, err := s.rdb.Get(ctx, statusPrefix+userId.String()).Result()
	if err == redis.Nil {
		return &MatchStatus{Status: "none"}, nil
	}
	if err != nil {
		return nil, fmt.Errorf("get status: %w", err)
	}

	var status MatchStatus
	if json.Unmarshal([]byte(val), &status) == nil && status.Status != "" {
		return &status, nil
	}
	if strings.HasPrefix(val, "matched:") {
		matchID := strings.TrimPrefix(val, "matched:")
		return &MatchStatus{Status: "matched", MatchID: matchID}, nil
	}
	return &MatchStatus{Status: val}, nil
}

func (s *Service) RunMatcher(ctx context.Context) {
	matchTicker := time.NewTicker(time.Second)
	outboxTicker := time.NewTicker(time.Second)
	defer matchTicker.Stop()
	defer outboxTicker.Stop()
	slog.Info("matchmaker started")

	for {
		select {
		case <-ctx.Done():
			slog.Info("matchmaker stopped")
			return
		case <-matchTicker.C:
			s.tryMatch(ctx)
		case <-outboxTicker.C:
			for i := 0; i < 32; i++ {
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

func (s *Service) tryMatch(ctx context.Context) {
	results, err := s.rdb.ZRangeWithScores(ctx, queueKey, 0, -1).Result()
	if err != nil || len(results) < matchSize {
		return
	}

	now := time.Now()
	used := make(map[string]bool)

	for i := 0; i < len(results); i++ {
		memberI := results[i].Member.(string)
		if used[memberI] {
			continue
		}

		group := []redis.Z{results[i]}

		for j := i + 1; j < len(results) && len(group) < matchSize; j++ {
			memberJ := results[j].Member.(string)
			if used[memberJ] {
				continue
			}

			allowedRange := s.calcRange(ctx, memberI, now)
			mmrDiff := results[j].Score - results[i].Score
			if mmrDiff < 0 {
				mmrDiff = -mmrDiff
			}

			if int(mmrDiff) <= allowedRange {
				group = append(group, results[j])
			}
		}

		if len(group) >= matchSize {
			players := make([]uuid.UUID, len(group))
			totalMMR := 0
			validGroup := true

			for k, z := range group {
				member, ok := z.Member.(string)
				uid, err := uuid.Parse(member)
				if !ok || err != nil {
					validGroup = false
					break
				}
				players[k] = uid
				totalMMR += int(z.Score)
			}
			if !validGroup {
				continue
			}

			if err := s.createMatch(ctx, players, totalMMR/len(group)); err != nil {
				slog.Error("create match", "error", err)
				continue
			}
			for _, z := range group {
				used[z.Member.(string)] = true
			}
		}
	}
}

func (s *Service) calcRange(ctx context.Context, userIdStr string, now time.Time) int {
	val, err := s.rdb.Get(ctx, joinTimePrefix+userIdStr).Result()
	if err != nil {
		return baseRange
	}
	joinTime, err := strconv.ParseInt(val, 10, 64)
	if err != nil {
		return baseRange
	}
	waitSec := int(now.Unix() - joinTime)
	return baseRange + (waitSec/30)*rangeExpand
}

func (s *Service) createMatch(ctx context.Context, players []uuid.UUID, avgMMR int) error {
	if s.ticketSigner == nil {
		return fmt.Errorf("match ticket signer is not configured")
	}
	matchID := uuid.New()
	gameSessionID := matchID.String()
	expiresAt := time.Now().Add(matchTTL)
	statuses := make(map[uuid.UUID]MatchStatus, len(players))
	for _, uid := range players {
		ticket, err := s.ticketSigner.Issue(matchID, uid, gameSessionID)
		if err != nil {
			return fmt.Errorf("issue ticket for %s: %w", uid, err)
		}
		statuses[uid] = MatchStatus{
			Status:        "matched",
			MatchID:       matchID.String(),
			GameSessionID: gameSessionID,
			Host:          s.allocation.Host,
			Port:          s.allocation.Port,
			Transport:     s.allocation.Transport,
			PlayerTicket:  ticket,
			ExpiresAt:     expiresAt.Unix(),
		}
	}

	event := MatchCreatedEvent{
		Type:      "MatchCreated",
		MatchID:   matchID,
		Players:   players,
		AvgMMR:    avgMMR,
		CreatedAt: time.Now().UTC(),
	}
	eventData, err := json.Marshal(event)
	if err != nil {
		return fmt.Errorf("marshal match event: %w", err)
	}

	tx, err := s.db.Begin(ctx)
	if err != nil {
		return fmt.Errorf("begin match transaction: %w", err)
	}
	defer tx.Rollback(ctx)

	if _, err := tx.Exec(ctx,
		`INSERT INTO matches (id, status, game_session_id) VALUES ($1, 'allocated', $2)`,
		matchID, gameSessionID); err != nil {
		return fmt.Errorf("insert match: %w", err)
	}
	for slot, uid := range players {
		if _, err := tx.Exec(ctx,
			`INSERT INTO match_participants (match_id, user_id, slot) VALUES ($1, $2, $3)`,
			matchID, uid, slot); err != nil {
			return fmt.Errorf("insert match participant: %w", err)
		}
	}
	if _, err := tx.Exec(ctx,
		`INSERT INTO match_events_outbox (match_id, event_type, payload) VALUES ($1, $2, $3)`,
		matchID, event.Type, eventData); err != nil {
		return fmt.Errorf("insert match outbox: %w", err)
	}
	if err := tx.Commit(ctx); err != nil {
		return fmt.Errorf("commit match: %w", err)
	}

	pipe := s.rdb.TxPipeline()
	for _, uid := range players {
		statusData, marshalErr := json.Marshal(statuses[uid])
		if marshalErr != nil {
			return fmt.Errorf("marshal match status: %w", marshalErr)
		}
		pipe.ZRem(ctx, queueKey, uid.String())
		pipe.Set(ctx, statusPrefix+uid.String(), statusData, matchTTL)
		pipe.Del(ctx, joinTimePrefix+uid.String())
	}
	if _, err := pipe.Exec(ctx); err != nil {
		slog.Error("persist matched status", "match_id", matchID, "error", err)
	}

	slog.Info("match created", "match_id", matchID, "players", len(players), "avg_mmr", avgMMR)
	return nil
}

func (s *Service) publishNextOutbox(ctx context.Context) (bool, error) {
	if s.writer == nil {
		return false, nil
	}

	tx, err := s.db.Begin(ctx)
	if err != nil {
		return false, fmt.Errorf("begin outbox transaction: %w", err)
	}
	defer tx.Rollback(ctx)

	var outboxID uuid.UUID
	var matchID uuid.UUID
	var eventType string
	var payload []byte
	err = tx.QueryRow(ctx,
		`SELECT id, match_id, event_type, payload
		 FROM match_events_outbox
		 WHERE published_at IS NULL
		 ORDER BY created_at
		 FOR UPDATE SKIP LOCKED
		 LIMIT 1`).Scan(&outboxID, &matchID, &eventType, &payload)
	if err == pgx.ErrNoRows {
		return false, nil
	}
	if err != nil {
		return false, fmt.Errorf("select match outbox: %w", err)
	}
	if err := s.writer.WriteMessages(ctx, kafka.Message{
		Key:   []byte(matchID.String()),
		Value: payload,
	}); err != nil {
		_, _ = tx.Exec(ctx,
			`UPDATE match_events_outbox SET attempt_count = attempt_count + 1 WHERE id = $1`,
			outboxID)
		if commitErr := tx.Commit(ctx); commitErr != nil {
			return false, fmt.Errorf("record outbox failure: %w", commitErr)
		}
		return false, fmt.Errorf("publish %s: %w", eventType, err)
	}

	if _, err := tx.Exec(ctx,
		`UPDATE match_events_outbox
		 SET published_at = NOW(), attempt_count = attempt_count + 1
		 WHERE id = $1`, outboxID); err != nil {
		return false, fmt.Errorf("mark match outbox published: %w", err)
	}
	if err := tx.Commit(ctx); err != nil {
		return false, fmt.Errorf("commit match outbox: %w", err)
	}
	return true, nil
}
