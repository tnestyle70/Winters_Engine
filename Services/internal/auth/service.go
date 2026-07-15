package auth

import (
	"context"
	"errors"
	"fmt"
	"log/slog"
	"strings"
	"unicode/utf8"

	"github.com/google/uuid"
	"golang.org/x/crypto/bcrypt"

	jwtauth "winters-backend/pkg/auth"
	apperr "winters-backend/pkg/errors"
)

type Service struct {
	repo              *Repository
	jwtMgr            *jwtauth.JWTManager
	startingBalanceRP int64
	idAuthEnabled     bool
}

func NewService(repo *Repository, jwtMgr *jwtauth.JWTManager, startingBalanceRP int64, idAuthEnabled bool) *Service {
	return &Service{
		repo:              repo,
		jwtMgr:            jwtMgr,
		startingBalanceRP: startingBalanceRP,
		idAuthEnabled:     idAuthEnabled,
	}
}

func (s *Service) Register(ctx context.Context, req RegisterRequest) (*jwtauth.TokenPair, error) {
	if utf8.RuneCountInString(req.Username) < 2 || utf8.RuneCountInString(req.Username) > 32 {
		return nil, fmt.Errorf("%w: username must be 2-32 characters", apperr.ErrInvalidInput)
	}
	if len(req.Password) < 8 {
		return nil, fmt.Errorf("%w: password must be at least 8 characters", apperr.ErrInvalidInput)
	}
	hashed, err := bcrypt.GenerateFromPassword([]byte(req.Password), bcrypt.DefaultCost)
	if err != nil {
		return nil, fmt.Errorf("hash password: %w", err)
	}

	user := &User{Username: req.Username, Email: req.Email, Password: string(hashed)}
	if err := s.repo.CreateUserWithWalletAndStats(ctx, user); err != nil {
		return nil, fmt.Errorf("create user: %w", err)
	}

	slog.Info("user registered", "user_id", user.ID, "username", user.Username)
	return s.issueTokens(ctx, user.ID, user.Username)
}

func (s *Service) Login(ctx context.Context, req LoginRequest) (*jwtauth.TokenPair, error) {
	user, err := s.repo.FindByEmail(ctx, req.Email)
	if err != nil {
		if errors.Is(err, apperr.ErrNotFound) {
			return nil, fmt.Errorf("%w: invalid credentials", apperr.ErrUnauthorized)
		}
		return nil, err
	}
	if err := bcrypt.CompareHashAndPassword([]byte(user.Password), []byte(req.Password)); err != nil {
		return nil, fmt.Errorf("%w: invalid credentials", apperr.ErrUnauthorized)
	}
	slog.Info("user logged in", "user_id", user.ID)
	return s.issueTokens(ctx, user.ID, user.Username)
}

// RegisterByID creates a new passwordless local-ID account.
// Returns ErrAlreadyExists (409) when the ID is already registered.
func (s *Service) RegisterByID(ctx context.Context, req IdAuthRequest) (*jwtauth.TokenPair, error) {
	loginID, err := s.normalizeLoginID(req.LoginID)
	if err != nil {
		return nil, err
	}
	user, err := s.repo.CreateIdentityAccount(
		ctx, ProviderLocalID, strings.ToLower(loginID), loginID, s.startingBalanceRP)
	if err != nil {
		if errors.Is(err, apperr.ErrAlreadyExists) {
			return nil, fmt.Errorf("%w: id already registered", apperr.ErrAlreadyExists)
		}
		return nil, fmt.Errorf("create id account: %w", err)
	}
	slog.Info("id account registered", "user_id", user.ID, "login_id", loginID)
	return s.issueTokens(ctx, user.ID, user.Username)
}

// LoginByID restores an existing local-ID account.
// Returns ErrNotFound (404) when the ID has never been registered.
func (s *Service) LoginByID(ctx context.Context, req IdAuthRequest) (*jwtauth.TokenPair, error) {
	loginID, err := s.normalizeLoginID(req.LoginID)
	if err != nil {
		return nil, err
	}
	user, err := s.repo.FindByIdentity(ctx, ProviderLocalID, strings.ToLower(loginID))
	if err != nil {
		if errors.Is(err, apperr.ErrNotFound) {
			return nil, fmt.Errorf("%w: unknown id", apperr.ErrNotFound)
		}
		return nil, err
	}
	slog.Info("id account logged in", "user_id", user.ID)
	return s.issueTokens(ctx, user.ID, user.Username)
}

func (s *Service) normalizeLoginID(raw string) (string, error) {
	if !s.idAuthEnabled {
		return "", fmt.Errorf("%w: id auth is disabled", apperr.ErrUnauthorized)
	}
	loginID := strings.TrimSpace(raw)
	if utf8.RuneCountInString(loginID) < 1 || utf8.RuneCountInString(loginID) > 32 {
		return "", fmt.Errorf("%w: id must be 1-32 characters", apperr.ErrInvalidInput)
	}
	if strings.ContainsAny(loginID, " \t\r\n") {
		return "", fmt.Errorf("%w: id must not contain whitespace", apperr.ErrInvalidInput)
	}
	return loginID, nil
}

func (s *Service) Refresh(ctx context.Context, req RefreshRequest) (*jwtauth.TokenPair, error) {
	claims, err := s.jwtMgr.ValidateRefreshToken(req.RefreshToken)
	if err != nil {
		return nil, fmt.Errorf("%w: invalid refresh token", apperr.ErrUnauthorized)
	}
	valid, err := s.repo.IsRefreshTokenValid(ctx, claims.ID)
	if err != nil {
		return nil, fmt.Errorf("check refresh token: %w", err)
	}
	if !valid {
		return nil, fmt.Errorf("%w: refresh token revoked", apperr.ErrUnauthorized)
	}
	_ = s.repo.DeleteRefreshToken(ctx, claims.ID)

	userId, err := uuid.Parse(claims.Subject)
	if err != nil {
		return nil, fmt.Errorf("parse user id: %w", err)
	}
	user, err := s.repo.FindByID(ctx, userId)
	if err != nil {
		return nil, err
	}
	return s.issueTokens(ctx, user.ID, user.Username)
}

func (s *Service) Logout(ctx context.Context, req LogoutRequest) error {
	claims, err := s.jwtMgr.ValidateRefreshToken(req.RefreshToken)
	if err != nil {
		return nil
	}
	return s.repo.DeleteRefreshToken(ctx, claims.ID)
}

func (s *Service) issueTokens(ctx context.Context, userId uuid.UUID, username string) (*jwtauth.TokenPair, error) {
	pair, err := s.jwtMgr.GenerateTokenPair(userId, username)
	if err != nil {
		return nil, err
	}
	refreshClaims, err := s.jwtMgr.ValidateRefreshToken(pair.RefreshToken)
	if err != nil {
		return nil, fmt.Errorf("validate issued refresh token: %w", err)
	}
	if err := s.repo.StoreRefreshToken(ctx, userId, refreshClaims.ID, s.jwtMgr.RefreshTTL()); err != nil {
		return nil, fmt.Errorf("store refresh token: %w", err)
	}
	return pair, nil
}
