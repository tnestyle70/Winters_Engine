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
	"github.com/jackc/pgx/v5/pgxpool"
	"github.com/redis/go-redis/v9"
	"github.com/segmentio/kafka-go"

	apperr "winters-backend/pkg/errors"
	"winters-backend/pkg/messaging"
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
	db     *pgxpool.Pool
	rdb    *redis.Client
	writer *kafka.Writer
}

func NewService(db *pgxpool.Pool, rdb *redis.Client, writer *kafka.Writer) *Service {
	return &Service{db: db, rdb: rdb, writer: writer}
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

	if strings.HasPrefix(val, "matched:") {
		matchID := strings.TrimPrefix(val, "matched:")
		return &MatchStatus{Status: "matched", MatchID: matchID}, nil
	}
	return &MatchStatus{Status: val}, nil
}

func (s *Service) RunMatcher(ctx context.Context) {
	ticker := time.NewTicker(time.Second)
	defer ticker.Stop()
	slog.Info("matchmaker started")

	for {
		select {
		case <-ctx.Done():
			slog.Info("matchmaker stopped")
			return
		case <-ticker.C:
			s.tryMatch(ctx)
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

			for k, z := range group {
				uid, _ := uuid.Parse(z.Member.(string))
				players[k] = uid
				totalMMR += int(z.Score)
				used[z.Member.(string)] = true
			}

			s.createMatch(ctx, players, totalMMR/len(group))
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

func (s *Service) createMatch(ctx context.Context, players []uuid.UUID, avgMMR int) {
	matchID := uuid.New()

	for _, uid := range players {
		s.rdb.ZRem(ctx, queueKey, uid.String())
		s.rdb.Set(ctx, statusPrefix+uid.String(),
			"matched:"+matchID.String(), matchTTL)
		s.rdb.Del(ctx, joinTimePrefix+uid.String())
	}

	event := MatchCreatedEvent{
		Type:      "MatchCreated",
		MatchID:   matchID,
		Players:   players,
		AvgMMR:    avgMMR,
		CreatedAt: time.Now(),
	}
	data, _ := json.Marshal(event)

	if err := s.writer.WriteMessages(ctx, kafka.Message{
		Key:   []byte(matchID.String()),
		Value: data,
		Topic: messaging.TopicMatchEvents,
	}); err != nil {
		slog.Error("kafka publish match created", "error", err)
	}

	slog.Info("match created", "match_id", matchID, "players", len(players), "avg_mmr", avgMMR)
}
