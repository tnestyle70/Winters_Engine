package config

import (
	"fmt"
	"net"
	"net/url"
	"os"
	"strconv"
	"time"

	"github.com/joho/godotenv"
)

type Config struct {
	Environment string
	DB          DatabaseConfig
	Redis       RedisConfig
	Kafka       KafkaConfig
	JWT         JWTConfig
	GameSession GameSessionConfig
	Replay      ReplayConfig
	AuthPort    string
	LeaderPort  string
	MatchPort   string
	ProfilePort string
	PaymentPort string
	ShopPort    string
	ReplayPort  string

	// WINTERS_DEV_AUTH_ENABLED=true enables passwordless /auth/id/* endpoints
	// (local development only; default off).
	DevAuthEnabled bool
	// WINTERS_ACCOUNT_POLICY_PATH points at Data/Account/AccountEconomyPolicy.json.
	AccountPolicyPath string
}

type DatabaseConfig struct {
	Host     string
	Port     int
	User     string
	Password string
	DBName   string
	PoolMax  int
	SSLMode  string
}

type RedisConfig struct {
	Addr     string
	Password string
	DB       int
	PoolMax  int
	UseTLS   bool
}

type KafkaConfig struct {
	Brokers []string
	UseTLS  bool
}

type JWTConfig struct {
	AccessSecret  string
	RefreshSecret string
	AccessTTL     time.Duration
	RefreshTTL    time.Duration
}

type GameSessionConfig struct {
	Host         string
	Port         int
	Transport    string
	TicketSecret string
	TicketTTL    time.Duration
}

type ReplayConfig struct {
	Bucket               string
	Region               string
	ObjectPrefix         string
	Endpoint             string
	PublicEndpoint       string
	AccessKey            string
	SecretKey            string
	UsePathStyle         bool
	UploadURLTTL         time.Duration
	DownloadURLTTL       time.Duration
	MultipartPartBytes   int64
	DefaultRetentionDays int
	InternalTokenSecret  string
}

func Load() (*Config, error) {
	_ = godotenv.Load()

	environment := getEnv("WINTERS_ENV", "development")
	replayEndpointDefault := "http://localhost:9000"
	replayAccessKeyDefault := "winters-minio"
	replaySecretKeyDefault := "winters-minio-secret"
	replayPathStyleDefault := "true"
	if environment == "production" {
		replayEndpointDefault = ""
		replayAccessKeyDefault = ""
		replaySecretKeyDefault = ""
		replayPathStyleDefault = "false"
	}

	dbPort, _ := strconv.Atoi(getEnv("DB_PORT", "5432"))
	poolMax, _ := strconv.Atoi(getEnv("DB_POOL_MAX", "20"))
	redisDB, _ := strconv.Atoi(getEnv("REDIS_DB", "0"))
	redisPoolMax, _ := strconv.Atoi(getEnv("REDIS_POOL_MAX", "20"))
	accessTTL, _ := time.ParseDuration(getEnv("JWT_ACCESS_TTL", "1h"))
	refreshTTL, _ := time.ParseDuration(getEnv("JWT_REFRESH_TTL", "168h"))
	gamePort, _ := strconv.Atoi(getEnv("WINTERS_GAME_PORT", "9000"))
	matchTicketTTL, _ := time.ParseDuration(getEnv("WINTERS_MATCH_TICKET_TTL", "5m"))
	replayUploadTTL, _ := time.ParseDuration(getEnv("REPLAY_UPLOAD_URL_TTL", "15m"))
	replayDownloadTTL, _ := time.ParseDuration(getEnv("REPLAY_DOWNLOAD_URL_TTL", "5m"))
	replayPartBytes, _ := strconv.ParseInt(getEnv("REPLAY_MULTIPART_PART_BYTES", "67108864"), 10, 64)
	replayRetentionDays, _ := strconv.Atoi(getEnv("REPLAY_RETENTION_DAYS", "30"))

	cfg := &Config{
		Environment: environment,
		DB: DatabaseConfig{
			Host:     getEnv("DB_HOST", "localhost"),
			Port:     dbPort,
			User:     getEnv("DB_USER", "winters"),
			Password: getEnv("DB_PASSWORD", "winters_dev_2026"),
			DBName:   getEnv("DB_NAME", "winters"),
			PoolMax:  poolMax,
			SSLMode:  getEnv("DB_SSL_MODE", "disable"),
		},
		Redis: RedisConfig{
			Addr:     getEnv("REDIS_ADDR", "localhost:6379"),
			Password: getEnv("REDIS_PASSWORD", ""),
			DB:       redisDB,
			PoolMax:  redisPoolMax,
			UseTLS:   getEnv("REDIS_TLS_ENABLED", "false") == "true",
		},
		Kafka: KafkaConfig{
			Brokers: []string{getEnv("KAFKA_BROKERS", "localhost:9092")},
			UseTLS:  getEnv("KAFKA_TLS_ENABLED", "false") == "true",
		},
		JWT: JWTConfig{
			AccessSecret:  getEnv("JWT_ACCESS_SECRET", "winters-access-secret-change-in-production"),
			RefreshSecret: getEnv("JWT_REFRESH_SECRET", "winters-refresh-secret-change-in-production"),
			AccessTTL:     accessTTL,
			RefreshTTL:    refreshTTL,
		},
		GameSession: GameSessionConfig{
			Host:         getEnv("WINTERS_GAME_HOST", "127.0.0.1"),
			Port:         gamePort,
			Transport:    getEnv("WINTERS_GAME_TRANSPORT", "udp"),
			TicketSecret: getEnv("WINTERS_MATCH_TICKET_SECRET", "winters-match-ticket-secret-change-in-production"),
			TicketTTL:    matchTicketTTL,
		},
		Replay: ReplayConfig{
			Bucket:               getEnv("REPLAY_BUCKET", "winters-replays"),
			Region:               getEnv("AWS_REGION", "ap-northeast-2"),
			ObjectPrefix:         getEnv("REPLAY_OBJECT_PREFIX", "replays"),
			Endpoint:             getEnv("S3_ENDPOINT", replayEndpointDefault),
			PublicEndpoint:       getEnv("S3_PUBLIC_ENDPOINT", ""),
			AccessKey:            getEnv("AWS_ACCESS_KEY_ID", replayAccessKeyDefault),
			SecretKey:            getEnv("AWS_SECRET_ACCESS_KEY", replaySecretKeyDefault),
			UsePathStyle:         getEnv("S3_USE_PATH_STYLE", replayPathStyleDefault) == "true",
			UploadURLTTL:         replayUploadTTL,
			DownloadURLTTL:       replayDownloadTTL,
			MultipartPartBytes:   replayPartBytes,
			DefaultRetentionDays: replayRetentionDays,
			InternalTokenSecret:  getEnv("REPLAY_INTERNAL_TOKEN", "winters-replay-internal-token-change-in-production"),
		},
		AuthPort:    getEnv("AUTH_PORT", "8081"),
		LeaderPort:  getEnv("LEADERBOARD_PORT", "8082"),
		MatchPort:   getEnv("MATCHMAKING_PORT", "8083"),
		ProfilePort: getEnv("PROFILE_PORT", "8084"),
		PaymentPort: getEnv("PAYMENT_PORT", "8085"),
		ShopPort:    getEnv("SHOP_PORT", "8086"),
		ReplayPort:  getEnv("REPLAY_PORT", "8087"),

		DevAuthEnabled:    getEnv("WINTERS_DEV_AUTH_ENABLED", "false") == "true",
		AccountPolicyPath: getEnv("WINTERS_ACCOUNT_POLICY_PATH", "../Data/Account/AccountEconomyPolicy.json"),
	}

	if cfg.DB.PoolMax <= 0 {
		return nil, fmt.Errorf("DB_POOL_MAX must be positive")
	}
	if cfg.Redis.PoolMax <= 0 {
		return nil, fmt.Errorf("REDIS_POOL_MAX must be positive")
	}
	if cfg.JWT.AccessTTL <= 0 || cfg.JWT.RefreshTTL <= 0 {
		return nil, fmt.Errorf("JWT TTL values must be positive")
	}
	if cfg.GameSession.Port <= 0 || cfg.GameSession.Port > 65535 {
		return nil, fmt.Errorf("WINTERS_GAME_PORT must be between 1 and 65535")
	}
	if cfg.GameSession.Transport != "tcp" && cfg.GameSession.Transport != "udp" {
		return nil, fmt.Errorf("WINTERS_GAME_TRANSPORT must be tcp or udp")
	}
	if cfg.GameSession.TicketTTL <= 0 {
		return nil, fmt.Errorf("WINTERS_MATCH_TICKET_TTL must be positive")
	}
	if cfg.Environment == "production" {
		if cfg.DB.SSLMode == "disable" {
			return nil, fmt.Errorf("DB_SSL_MODE=disable is forbidden in production")
		}
		if cfg.JWT.AccessSecret == "winters-access-secret-change-in-production" ||
			cfg.JWT.RefreshSecret == "winters-refresh-secret-change-in-production" ||
			cfg.GameSession.TicketSecret == "winters-match-ticket-secret-change-in-production" {
			return nil, fmt.Errorf("production secrets must be configured explicitly")
		}
		if !cfg.Redis.UseTLS || !cfg.Kafka.UseTLS {
			return nil, fmt.Errorf("Redis and Kafka TLS are required in production")
		}
	}

	return cfg, nil
}

func (r ReplayConfig) Validate(environment string) error {
	const minimumS3PartBytes = int64(5 * 1024 * 1024)
	if r.Bucket == "" || r.Region == "" || r.ObjectPrefix == "" {
		return fmt.Errorf("replay bucket, region, and object prefix are required")
	}
	if r.UploadURLTTL <= 0 || r.DownloadURLTTL <= 0 {
		return fmt.Errorf("replay URL TTL values must be positive")
	}
	if r.MultipartPartBytes < minimumS3PartBytes {
		return fmt.Errorf("REPLAY_MULTIPART_PART_BYTES must be at least %d", minimumS3PartBytes)
	}
	if r.DefaultRetentionDays <= 0 {
		return fmt.Errorf("REPLAY_RETENTION_DAYS must be positive")
	}
	if len(r.InternalTokenSecret) < 32 {
		return fmt.Errorf("REPLAY_INTERNAL_TOKEN must be at least 32 bytes")
	}
	if environment == "production" {
		if r.InternalTokenSecret == "winters-replay-internal-token-change-in-production" {
			return fmt.Errorf("production replay internal token must be configured explicitly")
		}
		if r.Endpoint != "" {
			return fmt.Errorf("S3_ENDPOINT must be empty in production")
		}
		if r.PublicEndpoint != "" {
			return fmt.Errorf("S3_PUBLIC_ENDPOINT must be empty in production")
		}
		if r.AccessKey != "" || r.SecretKey != "" {
			return fmt.Errorf("static S3 credentials are forbidden in production; use an IAM task role")
		}
	}
	return nil
}

func (d DatabaseConfig) DSN() string {
	host := d.Host
	if host == "localhost" {
		host = "127.0.0.1" // IPv6 [::1] 회피
	}
	connection := &url.URL{
		Scheme: "postgres",
		User:   url.UserPassword(d.User, d.Password),
		Host:   net.JoinHostPort(host, strconv.Itoa(d.Port)),
		Path:   d.DBName,
	}
	query := connection.Query()
	query.Set("sslmode", d.SSLMode)
	connection.RawQuery = query.Encode()
	return connection.String()
}

func getEnv(key, fallback string) string {
	if v := os.Getenv(key); v != "" {
		return v
	}
	return fallback
}
