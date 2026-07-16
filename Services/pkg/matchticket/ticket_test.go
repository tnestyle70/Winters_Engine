package matchticket

import (
	"errors"
	"strings"
	"testing"
	"time"

	"github.com/google/uuid"
)

func TestIssueVerifyRoundTrip(t *testing.T) {
	signer, err := NewSigner(strings.Repeat("s", 32), 5*time.Minute)
	if err != nil {
		t.Fatal(err)
	}
	now := time.Unix(1_700_000_000, 0)
	signer.now = func() time.Time { return now }

	matchID := uuid.New()
	userID := uuid.New()
	ticket, err := signer.Issue(matchID, userID, matchID.String())
	if err != nil {
		t.Fatal(err)
	}
	if len(ticket) > MaxTicketBytes {
		t.Fatalf("ticket size = %d", len(ticket))
	}

	claims, err := signer.Verify(ticket)
	if err != nil {
		t.Fatal(err)
	}
	if claims.MatchID != matchID || claims.UserID != userID || claims.GameSessionID != matchID.String() {
		t.Fatalf("unexpected claims: %+v", claims)
	}
}

func TestVerifyRejectsTampering(t *testing.T) {
	signer, err := NewSigner(strings.Repeat("s", 32), 5*time.Minute)
	if err != nil {
		t.Fatal(err)
	}
	ticket, err := signer.Issue(uuid.New(), uuid.New(), uuid.New().String())
	if err != nil {
		t.Fatal(err)
	}
	tampered := ticket[:len(ticket)-1] + "A"
	if _, err := signer.Verify(tampered); !errors.Is(err, ErrInvalidTicket) {
		t.Fatalf("expected invalid ticket, got %v", err)
	}
}

func TestVerifyRejectsExpiry(t *testing.T) {
	signer, err := NewSigner(strings.Repeat("s", 32), time.Minute)
	if err != nil {
		t.Fatal(err)
	}
	now := time.Unix(1_700_000_000, 0)
	signer.now = func() time.Time { return now }
	ticket, err := signer.Issue(uuid.New(), uuid.New(), uuid.New().String())
	if err != nil {
		t.Fatal(err)
	}
	signer.now = func() time.Time { return now.Add(2 * time.Minute) }
	if _, err := signer.Verify(ticket); !errors.Is(err, ErrExpiredTicket) {
		t.Fatalf("expected expired ticket, got %v", err)
	}
}

func TestNewSignerRejectsWeakConfiguration(t *testing.T) {
	if _, err := NewSigner("short", time.Minute); err == nil {
		t.Fatal("expected short secret rejection")
	}
	if _, err := NewSigner(strings.Repeat("s", 32), 0); err == nil {
		t.Fatal("expected ttl rejection")
	}
}
