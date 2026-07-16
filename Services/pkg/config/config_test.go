package config

import (
	"os"
	"testing"
)

func unsetEnv(t *testing.T, key string) {
	t.Helper()
	value, existed := os.LookupEnv(key)
	if err := os.Unsetenv(key); err != nil {
		t.Fatalf("unset %s: %v", key, err)
	}
	t.Cleanup(func() {
		if existed {
			_ = os.Setenv(key, value)
		} else {
			_ = os.Unsetenv(key)
		}
	})
}

func TestLoadProductionUsesIAMAndS3Defaults(t *testing.T) {
	t.Setenv("WINTERS_ENV", "production")
	t.Setenv("DB_SSL_MODE", "require")
	t.Setenv("REDIS_TLS_ENABLED", "true")
	t.Setenv("KAFKA_TLS_ENABLED", "true")
	t.Setenv("JWT_ACCESS_SECRET", "production-access-secret")
	t.Setenv("JWT_REFRESH_SECRET", "production-refresh-secret")
	t.Setenv("WINTERS_MATCH_TICKET_SECRET", "production-match-ticket-secret")
	t.Setenv("REPLAY_INTERNAL_TOKEN", "production-replay-internal-token-32-bytes")
	for _, key := range []string{
		"S3_ENDPOINT", "S3_PUBLIC_ENDPOINT", "S3_USE_PATH_STYLE",
		"AWS_ACCESS_KEY_ID", "AWS_SECRET_ACCESS_KEY",
	} {
		unsetEnv(t, key)
	}

	cfg, err := Load()
	if err != nil {
		t.Fatalf("Load() error = %v", err)
	}
	if err := cfg.Replay.Validate(cfg.Environment); err != nil {
		t.Fatalf("Replay.Validate() error = %v", err)
	}
	if cfg.Replay.Endpoint != "" || cfg.Replay.PublicEndpoint != "" || cfg.Replay.UsePathStyle {
		t.Fatalf("production S3 endpoints/path-style = %q/%q/%v", cfg.Replay.Endpoint, cfg.Replay.PublicEndpoint, cfg.Replay.UsePathStyle)
	}
	if cfg.Replay.AccessKey != "" || cfg.Replay.SecretKey != "" {
		t.Fatal("production must use the AWS credential chain instead of development static keys")
	}
}

func TestLoadDevelopmentUsesLocalMinIODefaults(t *testing.T) {
	t.Setenv("WINTERS_ENV", "development")
	for _, key := range []string{
		"S3_ENDPOINT", "S3_PUBLIC_ENDPOINT", "S3_USE_PATH_STYLE",
		"AWS_ACCESS_KEY_ID", "AWS_SECRET_ACCESS_KEY",
	} {
		unsetEnv(t, key)
	}

	cfg, err := Load()
	if err != nil {
		t.Fatalf("Load() error = %v", err)
	}
	if cfg.Replay.Endpoint != "http://localhost:9000" || !cfg.Replay.UsePathStyle {
		t.Fatalf("development S3 endpoint/path-style = %q/%v", cfg.Replay.Endpoint, cfg.Replay.UsePathStyle)
	}
	if cfg.Replay.AccessKey == "" || cfg.Replay.SecretKey == "" {
		t.Fatal("development MinIO defaults must include local credentials")
	}
}

func TestLoadDevelopmentAllowsSeparatePublicMinIOEndpoint(t *testing.T) {
	t.Setenv("WINTERS_ENV", "development")
	t.Setenv("S3_ENDPOINT", "http://minio:9000")
	t.Setenv("S3_PUBLIC_ENDPOINT", "http://localhost:9000")

	cfg, err := Load()
	if err != nil {
		t.Fatalf("Load() error = %v", err)
	}
	if cfg.Replay.Endpoint != "http://minio:9000" || cfg.Replay.PublicEndpoint != "http://localhost:9000" {
		t.Fatalf("development S3 endpoints = %q/%q", cfg.Replay.Endpoint, cfg.Replay.PublicEndpoint)
	}
}
