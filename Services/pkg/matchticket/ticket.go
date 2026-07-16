package matchticket

import (
	"crypto/hmac"
	"crypto/sha256"
	"encoding/base64"
	"encoding/json"
	"errors"
	"fmt"
	"strings"
	"time"

	"github.com/google/uuid"
)

const (
	Version        = "v1"
	MaxTicketBytes = 256
)

var (
	ErrInvalidTicket = errors.New("invalid match ticket")
	ErrExpiredTicket = errors.New("expired match ticket")
)

type Claims struct {
	MatchID       uuid.UUID
	UserID        uuid.UUID
	GameSessionID string
	ExpiresAt     time.Time
}

type wireClaims struct {
	MatchID       string `json:"m"`
	UserID        string `json:"u"`
	GameSessionID string `json:"g"`
	ExpiresAt     int64  `json:"e"`
}

type Signer struct {
	secret []byte
	ttl    time.Duration
	now    func() time.Time
}

func NewSigner(secret string, ttl time.Duration) (*Signer, error) {
	if len(secret) < 32 {
		return nil, fmt.Errorf("match ticket secret must be at least 32 bytes")
	}
	if ttl <= 0 {
		return nil, fmt.Errorf("match ticket ttl must be positive")
	}
	return &Signer{
		secret: []byte(secret),
		ttl:    ttl,
		now:    time.Now,
	}, nil
}

func (s *Signer) Issue(matchID, userID uuid.UUID, gameSessionID string) (string, error) {
	if matchID == uuid.Nil || userID == uuid.Nil || gameSessionID == "" {
		return "", ErrInvalidTicket
	}

	payload, err := json.Marshal(wireClaims{
		MatchID:       matchID.String(),
		UserID:        userID.String(),
		GameSessionID: gameSessionID,
		ExpiresAt:     s.now().Add(s.ttl).Unix(),
	})
	if err != nil {
		return "", fmt.Errorf("marshal ticket: %w", err)
	}

	encodedPayload := base64.RawURLEncoding.EncodeToString(payload)
	unsigned := Version + "." + encodedPayload
	signature := s.sign(unsigned)
	ticket := unsigned + "." + base64.RawURLEncoding.EncodeToString(signature)
	if len(ticket) > MaxTicketBytes {
		return "", fmt.Errorf("match ticket exceeds %d bytes", MaxTicketBytes)
	}
	return ticket, nil
}

func (s *Signer) Verify(ticket string) (*Claims, error) {
	if len(ticket) == 0 || len(ticket) > MaxTicketBytes {
		return nil, ErrInvalidTicket
	}
	parts := strings.Split(ticket, ".")
	if len(parts) != 3 || parts[0] != Version {
		return nil, ErrInvalidTicket
	}

	providedSignature, err := base64.RawURLEncoding.DecodeString(parts[2])
	if err != nil || !hmac.Equal(providedSignature, s.sign(parts[0]+"."+parts[1])) {
		return nil, ErrInvalidTicket
	}
	payload, err := base64.RawURLEncoding.DecodeString(parts[1])
	if err != nil {
		return nil, ErrInvalidTicket
	}

	var wire wireClaims
	if err := json.Unmarshal(payload, &wire); err != nil {
		return nil, ErrInvalidTicket
	}
	matchID, matchErr := uuid.Parse(wire.MatchID)
	userID, userErr := uuid.Parse(wire.UserID)
	expiresAt := time.Unix(wire.ExpiresAt, 0)
	now := s.now()
	if matchErr != nil || userErr != nil || wire.GameSessionID == "" || wire.ExpiresAt <= 0 {
		return nil, ErrInvalidTicket
	}
	if !expiresAt.After(now) {
		return nil, ErrExpiredTicket
	}
	if expiresAt.After(now.Add(s.ttl + time.Second)) {
		return nil, ErrInvalidTicket
	}

	return &Claims{
		MatchID:       matchID,
		UserID:        userID,
		GameSessionID: wire.GameSessionID,
		ExpiresAt:     expiresAt,
	}, nil
}

func (s *Signer) sign(unsigned string) []byte {
	mac := hmac.New(sha256.New, s.secret)
	_, _ = mac.Write([]byte(unsigned))
	return mac.Sum(nil)
}
