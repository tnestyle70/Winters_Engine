package profile

import (
	"context"
	"encoding/json"
	"fmt"
	"math"
	"time"

	"github.com/google/uuid"
	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgxpool"
	"github.com/redis/go-redis/v9"

	apperr "winters-backend/pkg/errors"
)

const (
	profileCachePrefix = "profile:"
	leaderboardKey     = "leaderboard:mmr"
	cacheTTL           = 5 * time.Minute
)

type Repository struct {
	db  *pgxpool.Pool
	rdb *redis.Client
}

func NewRepository(db *pgxpool.Pool, rdb *redis.Client) *Repository {
	return &Repository{db: db, rdb: rdb}
}

func (r *Repository) GetProfile(ctx context.Context, userId uuid.UUID) (*Profile, error) {
	cacheKey := profileCachePrefix + userId.String()

	cached, err := r.rdb.Get(ctx, cacheKey).Result()
	if err == nil {
		var p Profile
		if json.Unmarshal([]byte(cached), &p) == nil {
			return &p, nil
		}
	}

	var p Profile
	err = r.db.QueryRow(ctx,
		`SELECT u.id, u.username, ps.mmr, ps.wins, ps.losses,
		        ps.kills, ps.deaths, ps.assists
		 FROM users u JOIN player_stats ps ON u.id = ps.user_id
		 WHERE u.id = $1`, userId,
	).Scan(&p.UserID, &p.Username, &p.MMR, &p.Wins, &p.Losses,
		&p.Kills, &p.Deaths, &p.Assists)
	if err != nil {
		if err == pgx.ErrNoRows {
			return nil, apperr.ErrNotFound
		}
		return nil, fmt.Errorf("query profile: %w", err)
	}

	totalGames := p.Wins + p.Losses
	if totalGames > 0 {
		p.WinRate = float64(p.Wins) / float64(totalGames) * 100
	}
	p.KDA = float64(p.Kills+p.Assists) / math.Max(float64(p.Deaths), 1)

	rank, err := r.rdb.ZRevRank(ctx, leaderboardKey, userId.String()).Result()
	if err == nil {
		p.Rank = rank + 1
	}

	data, _ := json.Marshal(p)
	r.rdb.Set(ctx, cacheKey, data, cacheTTL)

	return &p, nil
}

func (r *Repository) GetMatchHistory(ctx context.Context, userId uuid.UUID, limit, offset int) ([]MatchRecord, error) {
	rows, err := r.db.Query(ctx,
		`SELECT match_id, result, kills, deaths, assists, mmr_change, played_at
		 FROM match_history WHERE user_id = $1
		 ORDER BY played_at DESC LIMIT $2 OFFSET $3`,
		userId, limit, offset)
	if err != nil {
		return nil, fmt.Errorf("query match history: %w", err)
	}
	defer rows.Close()

	records := make([]MatchRecord, 0)
	for rows.Next() {
		var rec MatchRecord
		if err := rows.Scan(&rec.MatchID, &rec.Result, &rec.Kills,
			&rec.Deaths, &rec.Assists, &rec.MMRChange, &rec.PlayedAt); err != nil {
			return nil, fmt.Errorf("scan match record: %w", err)
		}
		records = append(records, rec)
	}
	return records, nil
}

func (r *Repository) InsertMatchHistory(ctx context.Context, userId, matchID uuid.UUID, p MatchCompletedPlayer) error {
	_, err := r.db.Exec(ctx,
		`INSERT INTO match_history (user_id, match_id, result, kills, deaths, assists, mmr_change)
		 VALUES ($1, $2, $3, $4, $5, $6, $7)`,
		userId, matchID, p.Result, p.Kills, p.Deaths, p.Assists, p.MMRChange)
	if err != nil {
		return fmt.Errorf("insert match history: %w", err)
	}
	return nil
}

func (r *Repository) UpdatePlayerStats(ctx context.Context, userId uuid.UUID, p MatchCompletedPlayer) error {
	winAdd, lossAdd := 0, 0
	if p.Result == "win" {
		winAdd = 1
	} else if p.Result == "loss" {
		lossAdd = 1
	}

	_, err := r.db.Exec(ctx,
		`UPDATE player_stats
		 SET wins = wins + $2, losses = losses + $3,
		     kills = kills + $4, deaths = deaths + $5, assists = assists + $6,
		     mmr = mmr + $7, updated_at = NOW()
		 WHERE user_id = $1`,
		userId, winAdd, lossAdd, p.Kills, p.Deaths, p.Assists, p.MMRChange)
	if err != nil {
		return fmt.Errorf("update player_stats: %w", err)
	}
	return nil
}

func (r *Repository) InvalidateCache(ctx context.Context, userId uuid.UUID) error {
	return r.rdb.Del(ctx, profileCachePrefix+userId.String()).Err()
}
