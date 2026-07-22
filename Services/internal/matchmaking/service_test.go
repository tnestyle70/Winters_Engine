package matchmaking

import (
	"context"
	"fmt"
	"net/http"
	"net/http/httptest"
	"os"
	"testing"
	"time"

	"github.com/google/uuid"
	"github.com/jackc/pgx/v5/pgxpool"
	"winters-backend/pkg/matchticket"
)

func TestValidateAndNormalizeStartPlayers(t *testing.T) {
	first := uuid.New()
	second := uuid.New()
	players := []LobbyStartPlayer{
		{UserID: second, Slot: 3, Team: 1},
		{UserID: first, Slot: 0, Team: 0},
	}
	if err := validateStartPlayers(players, 10); err != nil {
		t.Fatalf("valid roster rejected: %v", err)
	}
	normalized := normalizeStartPlayers(players)
	if normalized[0].UserID != first || normalized[1].UserID != second {
		t.Fatalf("roster was not ordered by slot: %+v", normalized)
	}
	if !equalStartPlayers(players, normalized) {
		t.Fatal("equivalent rosters did not compare equal")
	}
	if err := validateStartPlayers([]LobbyStartPlayer{
		{UserID: first, Slot: 0, Team: 0},
		{UserID: first, Slot: 1, Team: 1},
	}, 10); err == nil {
		t.Fatal("duplicate user roster was accepted")
	}
}

func TestInternalTokenGuard(t *testing.T) {
	const token = "test-matchmaking-internal-token-32-bytes"
	handler := NewHandler(nil, token)
	protected := handler.requireInternalToken(http.HandlerFunc(
		func(w http.ResponseWriter, _ *http.Request) {
			w.WriteHeader(http.StatusNoContent)
		}))
	for _, test := range []struct {
		name       string
		header     string
		wantStatus int
	}{
		{"missing", "", http.StatusUnauthorized},
		{"wrong", "Bearer wrong", http.StatusUnauthorized},
		{"correct", "Bearer " + token, http.StatusNoContent},
	} {
		t.Run(test.name, func(t *testing.T) {
			request := httptest.NewRequest(http.MethodGet, "/", nil)
			if test.header != "" {
				request.Header.Set("Authorization", test.header)
			}
			response := httptest.NewRecorder()
			protected.ServeHTTP(response, request)
			if response.Code != test.wantStatus {
				t.Fatalf("status = %d, want %d", response.Code, test.wantStatus)
			}
		})
	}
}

func TestCustomLobbyImmediateAdmissionDynamicCounts(t *testing.T) {
	databaseURL := os.Getenv("WINTERS_TEST_DATABASE_URL")
	if databaseURL == "" {
		t.Skip("WINTERS_TEST_DATABASE_URL is required")
	}
	ctx := context.Background()
	db, err := pgxpool.New(ctx, databaseURL)
	if err != nil {
		t.Fatal(err)
	}
	defer db.Close()
	signer, err := matchticket.NewSigner(
		"test-match-ticket-secret-at-least-32-bytes", 5*time.Minute)
	if err != nil {
		t.Fatal(err)
	}

	for _, count := range []int{1, 2, 3, 4, 10} {
		t.Run(fmt.Sprintf("players_%d", count), func(t *testing.T) {
			gameSessionID := "direct-lobby-" + uuid.NewString()
			service := NewService(db, nil, signer, GameAllocation{
				Host: "127.0.0.1", Port: 9000, Transport: "udp",
				GameSessionID: gameSessionID,
			}, 10)
			users := make([]uuid.UUID, 0, count)
			statuses := make([]MatchStatus, 0, count)
			for index := 0; index < count; index++ {
				userID := uuid.New()
				users = append(users, userID)
				name := "direct_" + userID.String()
				if _, err := db.Exec(ctx, `
					INSERT INTO users (id, username, email, password)
					VALUES ($1, $2, $3, 'integration')`,
					userID, name[:32], name+"@test.invalid"); err != nil {
					t.Fatal(err)
				}
				started := time.Now()
				status, err := service.Join(ctx, userID)
				if err != nil {
					t.Fatalf("join %d: %v", index, err)
				}
				if time.Since(started) >= time.Second {
					t.Fatalf("join %d waited instead of immediate admission", index)
				}
				statuses = append(statuses, status)
			}
			t.Cleanup(func() {
				_, _ = db.Exec(context.Background(),
					`DELETE FROM game_server_capacities WHERE game_session_id = $1`, gameSessionID)
				_, _ = db.Exec(context.Background(),
					`DELETE FROM matches WHERE game_session_id = $1`, gameSessionID)
				_, _ = db.Exec(context.Background(),
					`DELETE FROM users WHERE id = ANY($1)`, users)
			})

			matchID := statuses[0].MatchID
			generation := statuses[0].Generation
			for index, status := range statuses {
				if status.Status != "matched" || status.MatchID != matchID ||
					status.Generation != generation || status.PlayerTicket == "" {
					t.Fatalf("join %d assignment mismatch: %+v", index, status)
				}
			}
			parsedMatchID, err := uuid.Parse(matchID)
			if err != nil {
				t.Fatal(err)
			}
			roster := make([]LobbyStartPlayer, 0, count)
			for index, userID := range users {
				roster = append(roster, LobbyStartPlayer{
					UserID: userID, Slot: int16(index), Team: int16(index / 5),
				})
			}
			if err := service.StartGameSession(
				ctx, gameSessionID, parsedMatchID, generation, roster); err != nil {
				t.Fatalf("finalize roster: %v", err)
			}
			var participantCount int
			if err := db.QueryRow(ctx,
				`SELECT COUNT(*) FROM match_participants WHERE match_id = $1`,
				parsedMatchID).Scan(&participantCount); err != nil {
				t.Fatal(err)
			}
			if participantCount != count {
				t.Fatalf("participants = %d, want %d", participantCount, count)
			}
		})
	}
}
