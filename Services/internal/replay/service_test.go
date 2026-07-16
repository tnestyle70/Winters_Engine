package replay

import (
	"context"
	"errors"
	"fmt"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"github.com/google/uuid"
	"winters-backend/pkg/config"
	apperr "winters-backend/pkg/errors"
)

type fakeRepository struct {
	item       Replay
	readyCalls int
}

func (f *fakeRepository) ReserveUpload(
	_ context.Context,
	replayID uuid.UUID,
	objectKey string,
	req CreateUploadRequest,
	expiresAt time.Time,
) (Replay, bool, error) {
	f.item = Replay{
		ID: replayID, MatchID: req.MatchID, Status: "uploading", ObjectKey: objectKey,
		SizeBytes: req.SizeBytes, ChecksumSHA256: req.ChecksumSHA256,
		FormatVersion: req.FormatVersion, TickRate: req.TickRate,
		RecordCount: req.RecordCount, SnapshotCount: req.SnapshotCount,
		EventCount: req.EventCount, CommandCount: req.CommandCount,
		FirstTick: req.FirstTick, LastTick: req.LastTick, ExpiresAt: &expiresAt,
		CreatedAt: time.Date(2026, 7, 16, 0, 0, 0, 0, time.UTC),
	}
	return f.item, true, nil
}

func (f *fakeRepository) SetUploadID(_ context.Context, _ uuid.UUID, uploadID string) error {
	f.item.UploadID = uploadID
	return nil
}

func (f *fakeRepository) Get(_ context.Context, replayID uuid.UUID) (Replay, error) {
	if f.item.ID != replayID {
		return Replay{}, apperr.ErrNotFound
	}
	return f.item, nil
}

func (f *fakeRepository) GetAuthorized(_ context.Context, replayID, _ uuid.UUID) (Replay, error) {
	return f.Get(context.Background(), replayID)
}

func (f *fakeRepository) MarkFailed(_ context.Context, _ uuid.UUID) error {
	f.item.Status = "failed"
	return nil
}

func (f *fakeRepository) MarkReady(_ context.Context, _ uuid.UUID) (Replay, error) {
	f.readyCalls++
	f.item.Status = "ready"
	return f.item, nil
}

func (f *fakeRepository) ListAuthorized(
	_ context.Context, _ uuid.UUID, _ int, _ *time.Time, _ uuid.UUID,
) ([]Replay, error) {
	return []Replay{f.item}, nil
}

func (f *fakeRepository) MarkDownloaded(context.Context, uuid.UUID, uuid.UUID) error { return nil }
func (f *fakeRepository) Hide(context.Context, uuid.UUID, uuid.UUID) error           { return nil }
func (f *fakeRepository) CompleteMatch(context.Context, uuid.UUID, []MatchCompletionPlayer) error {
	return nil
}

type fakeStorage struct {
	completed bool
	aborted   bool
	metadata  ObjectMetadata
}

func (f *fakeStorage) CreateMultipartUpload(context.Context, string, string) (string, error) {
	return "upload-1", nil
}

func (f *fakeStorage) PresignUploadPart(
	_ context.Context, _ string, _ string, partNumber int32, _ time.Duration,
) (string, error) {
	return fmt.Sprintf("https://storage.test/part/%d", partNumber), nil
}

func (f *fakeStorage) CompleteMultipartUpload(
	_ context.Context, _ string, _ string, _ []CompletedPart,
) error {
	f.completed = true
	return nil
}

func (f *fakeStorage) AbortMultipartUpload(context.Context, string, string) error {
	f.aborted = true
	return nil
}

func (f *fakeStorage) HeadObject(context.Context, string) (ObjectMetadata, error) {
	if !f.completed {
		return ObjectMetadata{}, errors.New("not found")
	}
	return f.metadata, nil
}

func (f *fakeStorage) PresignDownload(context.Context, string, time.Duration) (string, error) {
	return "https://storage.test/download", nil
}

func testReplayConfig() config.ReplayConfig {
	return config.ReplayConfig{
		Bucket: "test", Region: "ap-northeast-2", ObjectPrefix: "replays",
		UploadURLTTL: time.Minute, DownloadURLTTL: time.Minute,
		MultipartPartBytes: 5 * 1024 * 1024, DefaultRetentionDays: 30,
		InternalTokenSecret: "01234567890123456789012345678901",
	}
}

func validCreateRequest() CreateUploadRequest {
	return CreateUploadRequest{
		MatchID: uuid.New(), SizeBytes: 6 * 1024 * 1024,
		ChecksumSHA256: "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
		FormatVersion:  2, TickRate: 30, RecordCount: 6,
		SnapshotCount: 3, EventCount: 2, CommandCount: 1,
		FirstTick: 1, LastTick: 100,
	}
}

func TestCreatePresignAndCompleteUpload(t *testing.T) {
	repository := &fakeRepository{}
	storage := &fakeStorage{}
	service := NewService(repository, storage, testReplayConfig())
	service.now = func() time.Time { return time.Date(2026, 7, 16, 0, 0, 0, 0, time.UTC) }

	request := validCreateRequest()
	session, err := service.CreateUpload(context.Background(), request)
	if err != nil {
		t.Fatalf("CreateUpload() error = %v", err)
	}
	if session.Replay.UploadID != "upload-1" || session.PartSize != 5*1024*1024 {
		t.Fatalf("unexpected upload session: %+v", session)
	}
	parts, err := service.PresignParts(context.Background(), session.Replay.ID, []int32{1, 2})
	if err != nil || len(parts) != 2 {
		t.Fatalf("PresignParts() = %+v, %v", parts, err)
	}

	storage.metadata = ObjectMetadata{
		SizeBytes: request.SizeBytes, ChecksumSHA256: request.ChecksumSHA256,
	}
	completed, err := service.CompleteUpload(context.Background(), session.Replay.ID, CompleteUploadRequest{
		Parts:     []CompletedPart{{PartNumber: 1, ETag: "etag-1"}, {PartNumber: 2, ETag: "etag-2"}},
		SizeBytes: request.SizeBytes, ChecksumSHA256: request.ChecksumSHA256,
	})
	if err != nil {
		t.Fatalf("CompleteUpload() error = %v", err)
	}
	if completed.Status != "ready" || !storage.completed || repository.readyCalls != 1 {
		t.Fatalf("completion did not converge: replay=%+v storage=%+v repo=%+v", completed, storage, repository)
	}
}

func TestCompleteUploadRejectsMetadataMismatch(t *testing.T) {
	repository := &fakeRepository{}
	storage := &fakeStorage{}
	service := NewService(repository, storage, testReplayConfig())
	session, err := service.CreateUpload(context.Background(), validCreateRequest())
	if err != nil {
		t.Fatal(err)
	}

	_, err = service.CompleteUpload(context.Background(), session.Replay.ID, CompleteUploadRequest{
		Parts:     []CompletedPart{{PartNumber: 1, ETag: "etag-1"}, {PartNumber: 2, ETag: "etag-2"}},
		SizeBytes: session.Replay.SizeBytes, ChecksumSHA256: "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
	})
	if !errors.Is(err, apperr.ErrIdempotencyConflict) || storage.completed {
		t.Fatalf("expected metadata conflict before storage completion, got %v", err)
	}
}

func TestInternalRoutesRequireCredential(t *testing.T) {
	service := NewService(&fakeRepository{}, &fakeStorage{}, testReplayConfig())
	handler := NewHandler(service, testReplayConfig().InternalTokenSecret)
	request := httptest.NewRequest(http.MethodPost, "/replays/uploads", nil)
	responseRecorder := httptest.NewRecorder()

	handler.InternalRoutes().ServeHTTP(responseRecorder, request)
	if responseRecorder.Code != http.StatusUnauthorized {
		t.Fatalf("status = %d, want %d", responseRecorder.Code, http.StatusUnauthorized)
	}
}

func TestCursorRoundTrip(t *testing.T) {
	timestamp := time.Date(2026, 7, 16, 12, 30, 0, 123, time.UTC)
	id := uuid.New()
	decodedTime, decodedID, err := decodeCursor(encodeCursor(timestamp, id))
	if err != nil || decodedTime == nil || !decodedTime.Equal(timestamp) || decodedID != id {
		t.Fatalf("cursor round trip failed: time=%v id=%v error=%v", decodedTime, decodedID, err)
	}
}
