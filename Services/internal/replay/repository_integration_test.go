package replay

import (
	"context"
	"errors"
	"fmt"
	"os"
	"strings"
	"testing"
	"time"

	"github.com/google/uuid"
	"github.com/jackc/pgx/v5/pgxpool"
	apperr "winters-backend/pkg/errors"
)

func TestRepositoryDynamicParticipantACL(t *testing.T) {
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
	repository := NewRepository(db)

	for _, count := range []int{1, 2, 3, 4} {
		t.Run(fmt.Sprintf("players_%d", count), func(t *testing.T) {
			matchID := uuid.New()
			replayID := uuid.New()
			userIDs := make([]uuid.UUID, 0, count)
			for index := 0; index < count+1; index++ {
				userID := uuid.New()
				name := "replay_acl_" + userID.String()
				if _, err := db.Exec(ctx, `
					INSERT INTO users (id, username, email, password)
					VALUES ($1, $2, $3, 'integration')`,
					userID, name[:32], name+"@test.invalid"); err != nil {
					t.Fatal(err)
				}
				userIDs = append(userIDs, userID)
			}
			t.Cleanup(func() {
				_, _ = db.Exec(context.Background(),
					`DELETE FROM matches WHERE id = $1`, matchID)
				_, _ = db.Exec(context.Background(),
					`DELETE FROM users WHERE id = ANY($1)`, userIDs)
			})
			if _, err := db.Exec(ctx, `
				INSERT INTO matches (id, status, game_session_id)
				VALUES ($1, 'completed', $2)`, matchID, "acl-"+matchID.String()); err != nil {
				t.Fatal(err)
			}
			for slot := 0; slot < count; slot++ {
				if _, err := db.Exec(ctx, `
					INSERT INTO match_participants (match_id, user_id, slot)
					VALUES ($1, $2, $3)`, matchID, userIDs[slot], slot); err != nil {
					t.Fatal(err)
				}
			}
			if _, err := db.Exec(ctx, `
				INSERT INTO replays (
					id, match_id, status, object_key, size_bytes,
					checksum_sha256, format_version, tick_rate,
					record_count, snapshot_count, event_count, command_count,
					first_tick, last_tick, expires_at)
				VALUES (
					$1, $2, 'uploading', $3, 1024,
					$4, 2, 30, 3, 1, 1, 1, 1, 30, $5)`,
				replayID, matchID, "integration/"+replayID.String(),
				strings.Repeat("a", 64), time.Now().Add(time.Hour)); err != nil {
				t.Fatal(err)
			}
			if _, err := repository.MarkReady(ctx, replayID); err != nil {
				t.Fatal(err)
			}
			var libraryCount int
			if err := db.QueryRow(ctx, `
				SELECT count(*) FROM replay_user_library WHERE replay_id = $1`,
				replayID).Scan(&libraryCount); err != nil {
				t.Fatal(err)
			}
			if libraryCount != count {
				t.Fatalf("library rows = %d, want %d", libraryCount, count)
			}
			for _, userID := range userIDs[:count] {
				if _, err := repository.GetAuthorized(ctx, replayID, userID); err != nil {
					t.Fatalf("participant %s authorization: %v", userID, err)
				}
			}
			if _, err := repository.GetAuthorized(ctx, replayID, userIDs[count]); !errors.Is(err, apperr.ErrForbidden) {
				t.Fatalf("outsider authorization error = %v, want forbidden", err)
			}
		})
	}
}

func TestRepositoryFinalizesOnlyPlayedCustomLobbyAccounts(t *testing.T) {
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
	repository := NewRepository(db)

	for _, count := range []int{1, 2, 3, 4, 10} {
		t.Run(fmt.Sprintf("players_%d", count), func(t *testing.T) {
			matchID := uuid.New()
			replayID := uuid.New()
			userIDs := make([]uuid.UUID, 0, count+1)
			for index := 0; index < count+1; index++ {
				userID := uuid.New()
				name := "played_acl_" + userID.String()
				if _, err := db.Exec(ctx, `
					INSERT INTO users (id, username, email, password)
					VALUES ($1, $2, $3, 'integration')`,
					userID, name[:32], name+"@test.invalid"); err != nil {
					t.Fatal(err)
				}
				userIDs = append(userIDs, userID)
			}
			t.Cleanup(func() {
				_, _ = db.Exec(context.Background(),
					`DELETE FROM matches WHERE id = $1`, matchID)
				_, _ = db.Exec(context.Background(),
					`DELETE FROM users WHERE id = ANY($1)`, userIDs)
			})
			if _, err := db.Exec(ctx, `
				INSERT INTO matches (id, status, game_session_id, game_session_generation)
				VALUES ($1, 'allocated', $2, 1)`,
				matchID, "played-"+matchID.String()); err != nil {
				t.Fatal(err)
			}
			for _, userID := range userIDs {
				if _, err := db.Exec(ctx, `
					INSERT INTO match_lobby_admissions (match_id, user_id)
					VALUES ($1, $2)`, matchID, userID); err != nil {
					t.Fatal(err)
				}
			}
			players := make([]MatchCompletionPlayer, 0, count)
			for index, userID := range userIDs[:count] {
				players = append(players, MatchCompletionPlayer{
					UserID: userID, Result: "win",
					PerspectiveNetID: int64(index + 1),
				})
			}
			if err := repository.CompleteMatch(ctx, matchID, players); err != nil {
				t.Fatalf("complete match: %v", err)
			}
			if _, err := db.Exec(ctx, `
				INSERT INTO replays (
					id, match_id, status, object_key, size_bytes,
					checksum_sha256, format_version, tick_rate,
					record_count, snapshot_count, event_count, command_count,
					first_tick, last_tick, expires_at)
				VALUES (
					$1, $2, 'uploading', $3, 1024,
					$4, 2, 30, 3, 1, 1, 1, 1, 30, $5)`,
				replayID, matchID, "played-integration/"+replayID.String(),
				strings.Repeat("b", 64), time.Now().Add(time.Hour)); err != nil {
				t.Fatal(err)
			}
			if _, err := repository.MarkReady(ctx, replayID); err != nil {
				t.Fatal(err)
			}
			var participantCount, libraryCount int
			if err := db.QueryRow(ctx, `
				SELECT COUNT(*) FROM match_participants WHERE match_id = $1`,
				matchID).Scan(&participantCount); err != nil {
				t.Fatal(err)
			}
			if err := db.QueryRow(ctx, `
				SELECT COUNT(*) FROM replay_user_library WHERE replay_id = $1`,
				replayID).Scan(&libraryCount); err != nil {
				t.Fatal(err)
			}
			if participantCount != count || libraryCount != count {
				t.Fatalf("participants/library = %d/%d, want %d/%d",
					participantCount, libraryCount, count, count)
			}
			for index, userID := range userIDs[:count] {
				item, err := repository.GetAuthorized(ctx, replayID, userID)
				if err != nil {
					t.Fatalf("participant %s authorization: %v", userID, err)
				}
				if item.PerspectiveNetID != int64(index+1) {
					t.Fatalf("participant %s perspective = %d, want %d",
						userID, item.PerspectiveNetID, index+1)
				}
				page, err := repository.ListAuthorized(
					ctx, userID, 10, nil, uuid.Nil)
				if err != nil {
					t.Fatalf("participant %s replay list: %v", userID, err)
				}
				if len(page) != 1 || page[0].ID != replayID ||
					page[0].PerspectiveNetID != int64(index+1) {
					t.Fatalf(
						"participant %s list perspective = %+v, want replay %s perspective %d",
						userID, page, replayID, index+1)
				}
			}
			if _, err := repository.GetAuthorized(
				ctx, replayID, userIDs[count]); !errors.Is(err, apperr.ErrForbidden) {
				t.Fatalf("admitted non-player authorization = %v, want forbidden", err)
			}
		})
	}
}

func TestRepositoryReplayPerspectiveLegacyRetryMatrix(t *testing.T) {
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
	repository := NewRepository(db)

	matchID := uuid.New()
	replayID := uuid.New()
	userID := uuid.New()
	name := "perspective_" + userID.String()
	if _, err := db.Exec(ctx, `
		INSERT INTO users (id, username, email, password)
		VALUES ($1, $2, $3, 'integration')`,
		userID, name[:32], name+"@test.invalid"); err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() {
		_, _ = db.Exec(context.Background(),
			`DELETE FROM matches WHERE id = $1`, matchID)
		_, _ = db.Exec(context.Background(),
			`DELETE FROM users WHERE id = $1`, userID)
	})
	if _, err := db.Exec(ctx, `
		INSERT INTO matches (id, status, game_session_id, game_session_generation)
		VALUES ($1, 'allocated', $2, 1)`,
		matchID, "perspective-"+matchID.String()); err != nil {
		t.Fatal(err)
	}
	if _, err := db.Exec(ctx, `
		INSERT INTO match_lobby_admissions (match_id, user_id)
		VALUES ($1, $2)`, matchID, userID); err != nil {
		t.Fatal(err)
	}
	legacy := []MatchCompletionPlayer{{UserID: userID, Result: "win"}}
	if err := repository.CompleteMatch(ctx, matchID, legacy); err != nil {
		t.Fatalf("legacy completion: %v", err)
	}
	if _, err := db.Exec(ctx, `
		INSERT INTO replays (
			id, match_id, status, object_key, size_bytes,
			checksum_sha256, format_version, tick_rate,
			record_count, snapshot_count, event_count, command_count,
			first_tick, last_tick, expires_at)
		VALUES ($1, $2, 'uploading', $3, 1024, $4, 2, 30,
			3, 1, 1, 1, 1, 30, $5)`,
		replayID, matchID, "perspective-integration/"+replayID.String(),
		strings.Repeat("c", 64), time.Now().Add(time.Hour)); err != nil {
		t.Fatal(err)
	}
	if _, err := repository.MarkReady(ctx, replayID); err != nil {
		t.Fatal(err)
	}
	item, err := repository.GetAuthorized(ctx, replayID, userID)
	if err != nil || item.PerspectiveNetID != 0 {
		t.Fatalf("legacy perspective = %d, error = %v", item.PerspectiveNetID, err)
	}

	backfill := []MatchCompletionPlayer{{
		UserID: userID, Result: "win", PerspectiveNetID: 7,
	}}
	if err := repository.CompleteMatch(ctx, matchID, backfill); err != nil {
		t.Fatalf("backfill completion: %v", err)
	}
	if err := repository.CompleteMatch(ctx, matchID, backfill); err != nil {
		t.Fatalf("same perspective retry: %v", err)
	}
	if err := repository.CompleteMatch(ctx, matchID, legacy); err != nil {
		t.Fatalf("legacy retry after backfill: %v", err)
	}
	item, err = repository.GetAuthorized(ctx, replayID, userID)
	if err != nil || item.PerspectiveNetID != 7 {
		t.Fatalf("backfilled perspective = %d, error = %v", item.PerspectiveNetID, err)
	}

	mismatch := []MatchCompletionPlayer{{
		UserID: userID, Result: "win", PerspectiveNetID: 8,
	}}
	if err := repository.CompleteMatch(ctx, matchID, mismatch); !errors.Is(err, apperr.ErrIdempotencyConflict) {
		t.Fatalf("mismatch error = %v, want conflict", err)
	}
}
