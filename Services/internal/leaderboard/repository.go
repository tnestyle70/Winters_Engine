package leaderboard

import (
	"context"
	"fmt"
	"log/slog"

	"github.com/google/uuid"
	"github.com/jackc/pgx/v5/pgxpool"
	"github.com/redis/go-redis/v9"
)

const redisLeaderboardKey = "leaderboard:mmr"

type Repository struct {
	db  *pgxpool.Pool
	rdb *redis.Client
}

func NewRepository(db *pgxpool.Pool, rdb *redis.Client) *Repository {
	return &Repository{db: db, rdb: rdb}
}

func (r *Repository) UpdateScore(ctx context.Context, userId uuid.UUID, newMMR int) error {
	if err := r.rdb.ZAdd(ctx, redisLeaderboardKey, redis.Z{
		Score:  float64(newMMR),
		Member: userId.String(),
	}).Err(); err != nil {
		return fmt.Errorf("redis zadd: %w", err)
	}

	_, err := r.db.Exec(ctx,
		`UPDATE player_stats SET mmr = $1, updated_at = NOW() WHERE user_id = $2`,
		newMMR, userId)
	if err != nil {
		return fmt.Errorf("update player_stats mmr: %w", err)
	}
	return nil
}

func (r *Repository) GetTop(ctx context.Context, limit int) ([]LeaderboardEntry, error) {
	results, err := r.rdb.ZRevRangeWithScores(ctx, redisLeaderboardKey, 0, int64(limit-1)).Result()
	if err != nil {
		return nil, fmt.Errorf("redis zrevrange: %w", err)
	}

	entries := make([]LeaderboardEntry, 0, len(results))
	for i, z := range results {
		uid, err := uuid.Parse(z.Member.(string))
		if err != nil {
			slog.Warn("invalid uuid in leaderboard", "member", z.Member)
			continue
		}

		var entry LeaderboardEntry
		entry.Rank = int64(i + 1)
		entry.UserID = uid
		entry.MMR = int(z.Score)

		err = r.db.QueryRow(ctx,
			`SELECT u.username, ps.wins, ps.losses
			 FROM users u JOIN player_stats ps ON u.id = ps.user_id
			 WHERE u.id = $1`, uid,
		).Scan(&entry.Username, &entry.Wins, &entry.Losses)
		if err != nil {
			slog.Warn("failed to fetch user details", "user_id", uid, "error", err)
			continue
		}
		entries = append(entries, entry)
	}
	return entries, nil
}

func (r *Repository) GetRank(ctx context.Context, userId uuid.UUID) (*RankResponse, error) {
	rank, err := r.rdb.ZRevRank(ctx, redisLeaderboardKey, userId.String()).Result()
	if err != nil {
		if err == redis.Nil {
			return nil, fmt.Errorf("user not found in leaderboard")
		}
		return nil, fmt.Errorf("redis zrevrank: %w", err)
	}

	score, err := r.rdb.ZScore(ctx, redisLeaderboardKey, userId.String()).Result()
	if err != nil {
		return nil, fmt.Errorf("redis zscore: %w", err)
	}

	total, err := r.rdb.ZCard(ctx, redisLeaderboardKey).Result()
	if err != nil {
		return nil, fmt.Errorf("redis zcard: %w", err)
	}

	return &RankResponse{
		Rank:         rank + 1,
		UserID:       userId,
		MMR:          int(score),
		TotalPlayers: total,
	}, nil
}

func (r *Repository) GetCurrentMMR(ctx context.Context, userId uuid.UUID) (int, error) {
	score, err := r.rdb.ZScore(ctx, redisLeaderboardKey, userId.String()).Result()
	if err == nil {
		return int(score), nil
	}
	var mmr int
	err = r.db.QueryRow(ctx,
		`SELECT mmr FROM player_stats WHERE user_id = $1`, userId,
	).Scan(&mmr)
	if err != nil {
		return 0, fmt.Errorf("get current mmr: %w", err)
	}
	return mmr, nil
}

func (r *Repository) SyncFromDB(ctx context.Context) error {
	rows, err := r.db.Query(ctx, `SELECT user_id, mmr FROM player_stats`)
	if err != nil {
		return fmt.Errorf("query player_stats: %w", err)
	}
	defer rows.Close()

	pipe := r.rdb.Pipeline()
	count := 0
	for rows.Next() {
		var userId uuid.UUID
		var mmr int
		if err := rows.Scan(&userId, &mmr); err != nil {
			return fmt.Errorf("scan row: %w", err)
		}
		pipe.ZAdd(ctx, redisLeaderboardKey, redis.Z{
			Score:  float64(mmr),
			Member: userId.String(),
		})
		count++
	}

	if count > 0 {
		if _, err := pipe.Exec(ctx); err != nil {
			return fmt.Errorf("pipeline exec: %w", err)
		}
	}
	slog.Info("leaderboard synced from db", "players", count)
	return nil
}
