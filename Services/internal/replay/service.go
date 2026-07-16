package replay

import (
	"context"
	"encoding/base64"
	"encoding/json"
	"errors"
	"fmt"
	"regexp"
	"sort"
	"strings"
	"time"

	"github.com/google/uuid"
	"winters-backend/pkg/config"
	apperr "winters-backend/pkg/errors"
)

var sha256Pattern = regexp.MustCompile(`^[0-9a-f]{64}$`)

type RepositoryStore interface {
	ReserveUpload(context.Context, uuid.UUID, string, CreateUploadRequest, time.Time) (Replay, bool, error)
	SetUploadID(context.Context, uuid.UUID, string) error
	Get(context.Context, uuid.UUID) (Replay, error)
	GetAuthorized(context.Context, uuid.UUID, uuid.UUID) (Replay, error)
	MarkFailed(context.Context, uuid.UUID) error
	MarkReady(context.Context, uuid.UUID) (Replay, error)
	ListAuthorized(context.Context, uuid.UUID, int, *time.Time, uuid.UUID) ([]Replay, error)
	MarkDownloaded(context.Context, uuid.UUID, uuid.UUID) error
	Hide(context.Context, uuid.UUID, uuid.UUID) error
	CompleteMatch(context.Context, uuid.UUID, []MatchCompletionPlayer) error
}

type Service struct {
	repo    RepositoryStore
	storage Storage
	config  config.ReplayConfig
	now     func() time.Time
}

func NewService(repo RepositoryStore, storage Storage, cfg config.ReplayConfig) *Service {
	return &Service{repo: repo, storage: storage, config: cfg, now: time.Now}
}

func (s *Service) CreateUpload(ctx context.Context, req CreateUploadRequest) (UploadSession, error) {
	if err := s.validateCreateRequest(req); err != nil {
		return UploadSession{}, err
	}

	replayID := uuid.New()
	objectKey := strings.Trim(s.config.ObjectPrefix, "/") + "/" +
		req.MatchID.String() + "/" + replayID.String() + ".wrpl"
	expiresAt := s.now().UTC().AddDate(0, 0, s.config.DefaultRetentionDays)
	item, created, err := s.repo.ReserveUpload(ctx, replayID, objectKey, req, expiresAt)
	if err != nil {
		return UploadSession{}, err
	}
	if !replayMatchesCreateRequest(item, req) {
		return UploadSession{}, apperr.ErrIdempotencyConflict
	}
	if item.Status == "ready" || (item.Status == "uploading" && item.UploadID != "") {
		return UploadSession{Replay: item, PartSize: s.config.MultipartPartBytes}, nil
	}
	if item.Status != "uploading" {
		return UploadSession{}, apperr.ErrIdempotencyConflict
	}

	uploadID, err := s.storage.CreateMultipartUpload(ctx, item.ObjectKey, item.ChecksumSHA256)
	if err != nil {
		return UploadSession{}, fmt.Errorf("create multipart upload: %w", err)
	}
	if err := s.repo.SetUploadID(ctx, item.ID, uploadID); err != nil {
		_ = s.storage.AbortMultipartUpload(ctx, item.ObjectKey, uploadID)
		if !created && errors.Is(err, apperr.ErrIdempotencyConflict) {
			current, getErr := s.repo.Get(ctx, item.ID)
			if getErr == nil && current.Status == "uploading" && current.UploadID != "" {
				return UploadSession{Replay: current, PartSize: s.config.MultipartPartBytes}, nil
			}
		}
		return UploadSession{}, err
	}
	item.UploadID = uploadID
	return UploadSession{Replay: item, PartSize: s.config.MultipartPartBytes}, nil
}

func (s *Service) PresignParts(
	ctx context.Context,
	replayID uuid.UUID,
	partNumbers []int32,
) ([]PresignedPart, error) {
	item, err := s.repo.Get(ctx, replayID)
	if err != nil {
		return nil, err
	}
	if item.Status != "uploading" || item.UploadID == "" {
		return nil, apperr.ErrIdempotencyConflict
	}
	expectedParts := s.expectedPartCount(item.SizeBytes)
	if len(partNumbers) == 0 || len(partNumbers) > 100 {
		return nil, apperr.ErrInvalidInput
	}

	seen := make(map[int32]struct{}, len(partNumbers))
	result := make([]PresignedPart, 0, len(partNumbers))
	expiresAt := s.now().UTC().Add(s.config.UploadURLTTL)
	for _, partNumber := range partNumbers {
		if partNumber < 1 || int64(partNumber) > expectedParts {
			return nil, apperr.ErrInvalidInput
		}
		if _, duplicate := seen[partNumber]; duplicate {
			return nil, apperr.ErrInvalidInput
		}
		seen[partNumber] = struct{}{}
		url, err := s.storage.PresignUploadPart(
			ctx, item.ObjectKey, item.UploadID, partNumber, s.config.UploadURLTTL)
		if err != nil {
			return nil, fmt.Errorf("presign upload part %d: %w", partNumber, err)
		}
		result = append(result, PresignedPart{PartNumber: partNumber, URL: url, ExpiresAt: expiresAt})
	}
	return result, nil
}

func (s *Service) CompleteUpload(
	ctx context.Context,
	replayID uuid.UUID,
	req CompleteUploadRequest,
) (Replay, error) {
	item, err := s.repo.Get(ctx, replayID)
	if err != nil {
		return Replay{}, err
	}
	if req.SizeBytes != item.SizeBytes || req.ChecksumSHA256 != item.ChecksumSHA256 {
		return Replay{}, apperr.ErrIdempotencyConflict
	}
	if item.Status == "ready" {
		return item, nil
	}
	if item.Status != "uploading" || item.UploadID == "" {
		return Replay{}, apperr.ErrIdempotencyConflict
	}
	if err := s.validateCompletedParts(item.SizeBytes, req.Parts); err != nil {
		return Replay{}, err
	}

	if metadata, headErr := s.storage.HeadObject(ctx, item.ObjectKey); headErr == nil {
		if metadata.SizeBytes != item.SizeBytes || metadata.ChecksumSHA256 != item.ChecksumSHA256 {
			return Replay{}, apperr.ErrIdempotencyConflict
		}
		return s.repo.MarkReady(ctx, replayID)
	}

	if err := s.storage.CompleteMultipartUpload(
		ctx, item.ObjectKey, item.UploadID, req.Parts); err != nil {
		return Replay{}, fmt.Errorf("complete multipart upload: %w", err)
	}
	metadata, err := s.storage.HeadObject(ctx, item.ObjectKey)
	if err != nil {
		return Replay{}, fmt.Errorf("head completed replay: %w", err)
	}
	if metadata.SizeBytes != item.SizeBytes || metadata.ChecksumSHA256 != item.ChecksumSHA256 {
		return Replay{}, apperr.ErrIdempotencyConflict
	}
	return s.repo.MarkReady(ctx, replayID)
}

func (s *Service) AbortUpload(ctx context.Context, replayID uuid.UUID) error {
	item, err := s.repo.Get(ctx, replayID)
	if err != nil {
		return err
	}
	if item.Status == "failed" {
		return nil
	}
	if item.Status != "uploading" || item.UploadID == "" {
		return apperr.ErrIdempotencyConflict
	}
	if err := s.storage.AbortMultipartUpload(ctx, item.ObjectKey, item.UploadID); err != nil {
		return fmt.Errorf("abort multipart upload: %w", err)
	}
	return s.repo.MarkFailed(ctx, replayID)
}

func (s *Service) List(
	ctx context.Context,
	userID uuid.UUID,
	limit int,
	cursor string,
) (ReplayPage, error) {
	if limit <= 0 || limit > 100 {
		limit = 20
	}
	cursorTime, cursorID, err := decodeCursor(cursor)
	if err != nil {
		return ReplayPage{}, apperr.ErrInvalidInput
	}
	items, err := s.repo.ListAuthorized(ctx, userID, limit+1, cursorTime, cursorID)
	if err != nil {
		return ReplayPage{}, err
	}
	page := ReplayPage{Items: items}
	if len(items) > limit {
		page.Items = items[:limit]
		last := page.Items[len(page.Items)-1]
		page.NextCursor = encodeCursor(last.CreatedAt, last.ID)
	}
	return page, nil
}

func (s *Service) Get(ctx context.Context, replayID, userID uuid.UUID) (Replay, error) {
	return s.repo.GetAuthorized(ctx, replayID, userID)
}

func (s *Service) Download(ctx context.Context, replayID, userID uuid.UUID) (DownloadGrant, error) {
	item, err := s.repo.GetAuthorized(ctx, replayID, userID)
	if err != nil {
		return DownloadGrant{}, err
	}
	if item.Status != "ready" {
		return DownloadGrant{}, apperr.ErrIdempotencyConflict
	}
	url, err := s.storage.PresignDownload(ctx, item.ObjectKey, s.config.DownloadURLTTL)
	if err != nil {
		return DownloadGrant{}, fmt.Errorf("presign replay download: %w", err)
	}
	if err := s.repo.MarkDownloaded(ctx, replayID, userID); err != nil {
		return DownloadGrant{}, err
	}
	return DownloadGrant{URL: url, ExpiresAt: s.now().UTC().Add(s.config.DownloadURLTTL)}, nil
}

func (s *Service) Hide(ctx context.Context, replayID, userID uuid.UUID) error {
	return s.repo.Hide(ctx, replayID, userID)
}

func (s *Service) CompleteMatch(
	ctx context.Context,
	matchID uuid.UUID,
	request MatchCompletionRequest,
) error {
	if matchID == uuid.Nil || len(request.Players) == 0 || len(request.Players) > 10 {
		return apperr.ErrInvalidInput
	}
	return s.repo.CompleteMatch(ctx, matchID, request.Players)
}

func (s *Service) validateCreateRequest(req CreateUploadRequest) error {
	if req.MatchID == uuid.Nil || req.SizeBytes <= 0 || !sha256Pattern.MatchString(req.ChecksumSHA256) ||
		req.FormatVersion <= 0 || req.TickRate <= 0 || req.RecordCount <= 0 ||
		req.SnapshotCount < 0 || req.EventCount < 0 || req.CommandCount < 0 ||
		req.FirstTick < 0 || req.LastTick < req.FirstTick ||
		req.RecordCount != req.SnapshotCount+req.EventCount+req.CommandCount {
		return apperr.ErrInvalidInput
	}
	if s.expectedPartCount(req.SizeBytes) > 10_000 {
		return apperr.ErrInvalidInput
	}
	return nil
}

func replayMatchesCreateRequest(item Replay, request CreateUploadRequest) bool {
	return item.MatchID == request.MatchID &&
		item.SizeBytes == request.SizeBytes &&
		item.ChecksumSHA256 == request.ChecksumSHA256 &&
		item.FormatVersion == request.FormatVersion &&
		item.TickRate == request.TickRate &&
		item.RecordCount == request.RecordCount &&
		item.SnapshotCount == request.SnapshotCount &&
		item.EventCount == request.EventCount &&
		item.CommandCount == request.CommandCount &&
		item.FirstTick == request.FirstTick &&
		item.LastTick == request.LastTick
}

func (s *Service) expectedPartCount(sizeBytes int64) int64 {
	return (sizeBytes + s.config.MultipartPartBytes - 1) / s.config.MultipartPartBytes
}

func (s *Service) validateCompletedParts(sizeBytes int64, parts []CompletedPart) error {
	expectedCount := s.expectedPartCount(sizeBytes)
	if int64(len(parts)) != expectedCount {
		return apperr.ErrInvalidInput
	}
	sort.Slice(parts, func(i, j int) bool { return parts[i].PartNumber < parts[j].PartNumber })
	for index, part := range parts {
		if part.PartNumber != int32(index+1) || strings.TrimSpace(part.ETag) == "" {
			return apperr.ErrInvalidInput
		}
	}
	return nil
}

type cursorPayload struct {
	CreatedAt time.Time `json:"t"`
	ReplayID  uuid.UUID `json:"i"`
}

func encodeCursor(createdAt time.Time, replayID uuid.UUID) string {
	bytes, _ := json.Marshal(cursorPayload{CreatedAt: createdAt.UTC(), ReplayID: replayID})
	return base64.RawURLEncoding.EncodeToString(bytes)
}

func decodeCursor(cursor string) (*time.Time, uuid.UUID, error) {
	if cursor == "" {
		return nil, uuid.Nil, nil
	}
	bytes, err := base64.RawURLEncoding.DecodeString(cursor)
	if err != nil {
		return nil, uuid.Nil, err
	}
	var payload cursorPayload
	if err := json.Unmarshal(bytes, &payload); err != nil || payload.CreatedAt.IsZero() || payload.ReplayID == uuid.Nil {
		return nil, uuid.Nil, errors.New("invalid cursor")
	}
	payload.CreatedAt = payload.CreatedAt.UTC()
	return &payload.CreatedAt, payload.ReplayID, nil
}
