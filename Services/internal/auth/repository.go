package auth

import (
	"context"
	"errors"
	"fmt"
	"time"

	"github.com/google/uuid"
	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgconn"
	"github.com/jackc/pgx/v5/pgxpool"
	"github.com/redis/go-redis/v9"

	apperr "winters-backend/pkg/errors"
)

const ProviderLocalID = "local_id"

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
		`SELECT id, username, COALESCE(email, ''), COALESCE(password, ''), created_at, updated_at
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
		`SELECT id, username, COALESCE(email, ''), COALESCE(password, ''), created_at, updated_at
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

func (r *Repository) FindByIdentity(ctx context.Context, provider, providerSubject string) (*User, error) {
	var u User
	err := r.db.QueryRow(ctx,
		`SELECT u.id, u.username, COALESCE(u.email, ''), COALESCE(u.password, ''), u.created_at, u.updated_at
		 FROM user_identities ui
		 JOIN users u ON u.id = ui.user_id
		 WHERE ui.provider = $1 AND ui.provider_subject = $2`,
		provider, providerSubject,
	).Scan(&u.ID, &u.Username, &u.Email, &u.Password, &u.CreatedAt, &u.UpdatedAt)
	if err != nil {
		if errors.Is(err, pgx.ErrNoRows) {
			return nil, apperr.ErrNotFound
		}
		return nil, fmt.Errorf("find by identity: %w", err)
	}
	return &u, nil
}

// CreateIdentityAccount creates a full account (users + identity + wallet +
// player_stats + initial_grant ledger) in one transaction. Returns
// ErrAlreadyExists when the identity (or display name) is already taken.
func (r *Repository) CreateIdentityAccount(
	ctx context.Context, provider, providerSubject, displayName string, startingBalance int64,
) (*User, error) {
	tx, err := r.db.Begin(ctx)
	if err != nil {
		return nil, fmt.Errorf("begin tx: %w", err)
	}
	defer tx.Rollback(ctx)

	// Serialize concurrent first-registrations of the same identity.
	if _, err := tx.Exec(ctx,
		`SELECT pg_advisory_xact_lock(hashtext($1))`, provider+":"+providerSubject); err != nil {
		return nil, fmt.Errorf("identity advisory lock: %w", err)
	}

	var existing uuid.UUID
	err = tx.QueryRow(ctx,
		`SELECT user_id FROM user_identities WHERE provider = $1 AND provider_subject = $2`,
		provider, providerSubject,
	).Scan(&existing)
	if err == nil {
		return nil, apperr.ErrAlreadyExists
	}
	if !errors.Is(err, pgx.ErrNoRows) {
		return nil, fmt.Errorf("check identity: %w", err)
	}

	u := &User{Username: displayName}
	err = tx.QueryRow(ctx,
		`INSERT INTO users (username, email, password)
		 VALUES ($1, NULL, NULL) RETURNING id, created_at, updated_at`,
		displayName,
	).Scan(&u.ID, &u.CreatedAt, &u.UpdatedAt)
	if err != nil {
		if isUniqueViolation(err) {
			return nil, apperr.ErrAlreadyExists
		}
		return nil, fmt.Errorf("insert user: %w", err)
	}

	if _, err := tx.Exec(ctx,
		`INSERT INTO user_identities (user_id, provider, provider_subject)
		 VALUES ($1, $2, $3)`,
		u.ID, provider, providerSubject); err != nil {
		if isUniqueViolation(err) {
			return nil, apperr.ErrAlreadyExists
		}
		return nil, fmt.Errorf("insert identity: %w", err)
	}

	if _, err := tx.Exec(ctx,
		`INSERT INTO wallets (user_id, balance, currency_code) VALUES ($1, $2, 'RP')`,
		u.ID, startingBalance); err != nil {
		return nil, fmt.Errorf("insert wallet: %w", err)
	}

	if _, err := tx.Exec(ctx,
		`INSERT INTO player_stats (user_id, mmr) VALUES ($1, 1000)`, u.ID); err != nil {
		return nil, fmt.Errorf("insert player_stats: %w", err)
	}

	if _, err := tx.Exec(ctx,
		`INSERT INTO coin_transactions (user_id, amount, tx_type, reference, balance_after)
		 VALUES ($1, $2, 'initial_grant', 'initial-grant-v1', $2)`,
		u.ID, startingBalance); err != nil {
		return nil, fmt.Errorf("insert initial grant: %w", err)
	}

	if err := tx.Commit(ctx); err != nil {
		return nil, fmt.Errorf("commit: %w", err)
	}
	return u, nil
}

func isUniqueViolation(err error) bool {
	var pgErr *pgconn.PgError
	return errors.As(err, &pgErr) && pgErr.Code == "23505"
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
