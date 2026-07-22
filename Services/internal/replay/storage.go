package replay

import (
	"context"
	"time"
)

type ObjectMetadata struct {
	SizeBytes      int64
	ChecksumSHA256 string
}

type Storage interface {
	CreateMultipartUpload(ctx context.Context, objectKey, checksumSHA256 string) (string, error)
	PresignUploadPart(ctx context.Context, objectKey, uploadID string, partNumber int32, ttl time.Duration) (string, error)
	CompleteMultipartUpload(ctx context.Context, objectKey, uploadID string, parts []CompletedPart) error
	AbortMultipartUpload(ctx context.Context, objectKey, uploadID string) error
	HeadObject(ctx context.Context, objectKey string) (ObjectMetadata, error)
	PresignDownload(ctx context.Context, objectKey string, ttl time.Duration) (string, error)
}
