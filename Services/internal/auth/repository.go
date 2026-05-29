package auth

import (
	"context"
	"errors"
	"fmt"
	"time"

	"github.com/google/uuid"
	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgxpool"
	"github.com/redis/go-redis/v9"

	apperr "winters-backend/pkg/errors"
)

type Repository struct {
	db  *pgxpool.Pool
	rdb *redis.Client
}

func NewRepository(db *pgxpool.Pool, rdb *redis.Client) *Repository {
	return &Repository{db: db, rdb: rdb}
}

func (r *Repository) CreateUserWithWalletAndStats(ctx context.Context, user *User) error {
	tx, err := r.db.Begin(ctx)
	if err != nil {
		return fmt.Errorf("begin tx: %w", err)
	}
	defer tx.Rollback(ctx)

	err = tx.QueryRow(ctx,
		`INSERT INTO users (username, email, password)
		 VALUES ($1, $2, $3) RETURNING id, created_at, updated_at`,
		user.Username, user.Email, user.Password,
	).Scan(&user.ID, &user.CreatedAt, &user.UpdatedAt)
	if err != nil {
		return fmt.Errorf("insert user: %w", err)
	}

	_, err = tx.Exec(ctx, `INSERT INTO wallets (user_id, balance) VALUES ($1, 0)`, user.ID)
	if err != nil {
		return fmt.Errorf("insert wallet: %w", err)
	}

	_, err = tx.Exec(ctx, `INSERT INTO player_stats (user_id, mmr) VALUES ($1, 1000)`, user.ID)
	if err != nil {
		return fmt.Errorf("insert player_stats: %w", err)
	}

	return tx.Commit(ctx)
}

func (r *Repository) FindByEmail(ctx context.Context, email string) (*User, error) {
	var u User
	err := r.db.QueryRow(ctx,
		`SELECT id, username, email, password, created_at, updated_at
		 FROM users WHERE email = $1`, email,
	).Scan(&u.ID, &u.Username, &u.Email, &u.Password, &u.CreatedAt, &u.UpdatedAt)

	if err != nil {
		if errors.Is(err, pgx.ErrNoRows) {
			return nil, apperr.ErrNotFound
		}
		return nil, fmt.Errorf("find by email: %w", err)
	}
	return &u, nil
}

func (r *Repository) FindByID(ctx context.Context, id uuid.UUID) (*User, error) {
	var u User
	err := r.db.QueryRow(ctx,
		`SELECT id, username, email, password, created_at, updated_at
		 FROM users WHERE id = $1`, id,
	).Scan(&u.ID, &u.Username, &u.Email, &u.Password, &u.CreatedAt, &u.UpdatedAt)
	if err != nil {
		if errors.Is(err, pgx.ErrNoRows) {
			return nil, apperr.ErrNotFound
		}
		return nil, fmt.Errorf("find by id: %w", err)
	}
	return &u, nil
}

func (r *Repository) StoreRefreshToken(ctx context.Context, userId uuid.UUID, jti string,
	ttl time.Duration) error {
	return r.rdb.Set(ctx, "refresh:"+jti, userId.String(), ttl).Err()
}

func (r *Repository) DeleteRefreshToken(ctx context.Context, jti string) error {
	return r.rdb.Del(ctx, "refresh:"+jti).Err()
}

func (r *Repository) IsRefreshTokenValid(ctx context.Context, jti string) (bool, error) {
	n, err := r.rdb.Exists(ctx, "refresh:"+jti).Result()
	if err != nil {
		return false, err
	}
	return n > 0, nil
}
