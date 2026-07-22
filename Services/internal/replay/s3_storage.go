package replay

import (
	"context"
	"fmt"
	"time"

	"github.com/aws/aws-sdk-go-v2/aws"
	awsconfig "github.com/aws/aws-sdk-go-v2/config"
	"github.com/aws/aws-sdk-go-v2/credentials"
	"github.com/aws/aws-sdk-go-v2/service/s3"
	"github.com/aws/aws-sdk-go-v2/service/s3/types"
	appconfig "winters-backend/pkg/config"
)

const checksumMetadataKey = "checksum-sha256"

type S3Storage struct {
	bucket              string
	client              *s3.Client
	presign             *s3.PresignClient
	useServerEncryption bool
}

func (s *S3Storage) EnsureBucket(ctx context.Context, allowCreate bool) error {
	if _, err := s.client.HeadBucket(ctx, &s3.HeadBucketInput{
		Bucket: aws.String(s.bucket),
	}); err == nil {
		return nil
	} else if !allowCreate {
		return fmt.Errorf("head replay bucket: %w", err)
	}

	if _, err := s.client.CreateBucket(ctx, &s3.CreateBucketInput{
		Bucket: aws.String(s.bucket),
	}); err != nil {
		return fmt.Errorf("create replay bucket: %w", err)
	}
	return nil
}

func NewS3Storage(ctx context.Context, cfg appconfig.ReplayConfig) (*S3Storage, error) {
	loadOptions := []func(*awsconfig.LoadOptions) error{
		awsconfig.WithRegion(cfg.Region),
	}
	if cfg.AccessKey != "" || cfg.SecretKey != "" {
		if cfg.AccessKey == "" || cfg.SecretKey == "" {
			return nil, fmt.Errorf("both AWS access key and secret key must be set")
		}
		loadOptions = append(loadOptions, awsconfig.WithCredentialsProvider(
			credentials.NewStaticCredentialsProvider(cfg.AccessKey, cfg.SecretKey, "")))
	}
	awsCfg, err := awsconfig.LoadDefaultConfig(ctx, loadOptions...)
	if err != nil {
		return nil, fmt.Errorf("load AWS config: %w", err)
	}

	client := s3.NewFromConfig(awsCfg, func(options *s3.Options) {
		options.UsePathStyle = cfg.UsePathStyle
		if cfg.Endpoint != "" {
			options.BaseEndpoint = aws.String(cfg.Endpoint)
		}
	})
	presignClient := client
	if cfg.PublicEndpoint != "" {
		presignClient = s3.NewFromConfig(awsCfg, func(options *s3.Options) {
			options.UsePathStyle = cfg.UsePathStyle
			options.BaseEndpoint = aws.String(cfg.PublicEndpoint)
		})
	}
	return &S3Storage{
		bucket:              cfg.Bucket,
		client:              client,
		presign:             s3.NewPresignClient(presignClient),
		useServerEncryption: cfg.Endpoint == "",
	}, nil
}

func (s *S3Storage) CreateMultipartUpload(
	ctx context.Context,
	objectKey, checksumSHA256 string,
) (string, error) {
	input := &s3.CreateMultipartUploadInput{
		Bucket:      aws.String(s.bucket),
		Key:         aws.String(objectKey),
		ContentType: aws.String("application/octet-stream"),
		Metadata:    map[string]string{checksumMetadataKey: checksumSHA256},
	}
	if s.useServerEncryption {
		input.ServerSideEncryption = types.ServerSideEncryptionAes256
	}
	result, err := s.client.CreateMultipartUpload(ctx, input)
	if err != nil {
		return "", err
	}
	if result.UploadId == nil || *result.UploadId == "" {
		return "", fmt.Errorf("S3 returned an empty upload id")
	}
	return *result.UploadId, nil
}

func (s *S3Storage) PresignUploadPart(
	ctx context.Context,
	objectKey, uploadID string,
	partNumber int32,
	ttl time.Duration,
) (string, error) {
	request, err := s.presign.PresignUploadPart(ctx, &s3.UploadPartInput{
		Bucket:     aws.String(s.bucket),
		Key:        aws.String(objectKey),
		UploadId:   aws.String(uploadID),
		PartNumber: aws.Int32(partNumber),
	}, func(options *s3.PresignOptions) {
		options.Expires = ttl
	})
	if err != nil {
		return "", err
	}
	return request.URL, nil
}

func (s *S3Storage) CompleteMultipartUpload(
	ctx context.Context,
	objectKey, uploadID string,
	parts []CompletedPart,
) error {
	s3Parts := make([]types.CompletedPart, 0, len(parts))
	for _, part := range parts {
		s3Parts = append(s3Parts, types.CompletedPart{
			ETag:       aws.String(part.ETag),
			PartNumber: aws.Int32(part.PartNumber),
		})
	}
	_, err := s.client.CompleteMultipartUpload(ctx, &s3.CompleteMultipartUploadInput{
		Bucket:          aws.String(s.bucket),
		Key:             aws.String(objectKey),
		UploadId:        aws.String(uploadID),
		MultipartUpload: &types.CompletedMultipartUpload{Parts: s3Parts},
	})
	return err
}

func (s *S3Storage) AbortMultipartUpload(
	ctx context.Context,
	objectKey, uploadID string,
) error {
	_, err := s.client.AbortMultipartUpload(ctx, &s3.AbortMultipartUploadInput{
		Bucket:   aws.String(s.bucket),
		Key:      aws.String(objectKey),
		UploadId: aws.String(uploadID),
	})
	return err
}

func (s *S3Storage) HeadObject(ctx context.Context, objectKey string) (ObjectMetadata, error) {
	result, err := s.client.HeadObject(ctx, &s3.HeadObjectInput{
		Bucket: aws.String(s.bucket),
		Key:    aws.String(objectKey),
	})
	if err != nil {
		return ObjectMetadata{}, err
	}
	return ObjectMetadata{
		SizeBytes:      aws.ToInt64(result.ContentLength),
		ChecksumSHA256: result.Metadata[checksumMetadataKey],
	}, nil
}

func (s *S3Storage) PresignDownload(
	ctx context.Context,
	objectKey string,
	ttl time.Duration,
) (string, error) {
	request, err := s.presign.PresignGetObject(ctx, &s3.GetObjectInput{
		Bucket: aws.String(s.bucket),
		Key:    aws.String(objectKey),
	}, func(options *s3.PresignOptions) {
		options.Expires = ttl
	})
	if err != nil {
		return "", err
	}
	return request.URL, nil
}
