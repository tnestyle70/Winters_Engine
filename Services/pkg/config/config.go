package config

import (
	"os"
	"strconv"
	"time"

	"github.com/joho/godotenv"
)

type Config struct {
	DB          DatabaseConfig
	Redis       RedisConfig
	Kafka       KafkaConfig
	JWT         JWTConfig
	AuthPort    string
	LeaderPort  string
	MatchPort   string
	ProfilePort string
	PaymentPort string
	ShopPort    string
}

type DatabaseConfig struct {
	Host     string
	Port     int
	User     string
	Password string
	DBName   string
	PoolMax  int
}

type RedisConfig struct {
	Addr     string
	Password string
	DB       int
}

type KafkaConfig struct {
	Brokers []string
}

type JWTConfig struct {
	AccessSecret  string
	RefreshSecret string
	AccessTTL     time.Duration
	RefreshTTL    time.Duration
}

func Load() (*Config, error) {
	_ = godotenv.Load()

	dbPort, _ := strconv.Atoi(getEnv("DB_PORT", "5432"))
	poolMax, _ := strconv.Atoi(getEnv("DB_POOL_MAX", "20"))
	redisDB, _ := strconv.Atoi(getEnv("REDIS_DB", "0"))
	accessTTL, _ := time.ParseDuration(getEnv("JWT_ACCESS_TTL", "1h"))
	refreshTTL, _ := time.ParseDuration(getEnv("JWT_REFRESH_TTL", "168h"))

	return &Config{
		DB: DatabaseConfig{
			Host:     getEnv("DB_HOST", "localhost"),
			Port:     dbPort,
			User:     getEnv("DB_USER", "winters"),
			Password: getEnv("DB_PASSWORD", "winters_dev_2026"),
			DBName:   getEnv("DB_NAME", "winters"),
			PoolMax:  poolMax,
		},
		Redis: RedisConfig{
			Addr:     getEnv("REDIS_ADDR", "localhost:6379"),
			Password: getEnv("REDIS_PASSWORD", ""),
			DB:       redisDB,
		},
		Kafka: KafkaConfig{
			Brokers: []string{getEnv("KAFKA_BROKERS", "localhost:9092")},
		},
		JWT: JWTConfig{
			AccessSecret:  getEnv("JWT_ACCESS_SECRET", "winters-access-secret-change-in-production"),
			RefreshSecret: getEnv("JWT_REFRESH_SECRET", "winters-refresh-secret-change-in-production"),
			AccessTTL:     accessTTL,
			RefreshTTL:    refreshTTL,
		},
		AuthPort:    getEnv("AUTH_PORT", "8081"),
		LeaderPort:  getEnv("LEADERBOARD_PORT", "8082"),
		MatchPort:   getEnv("MATCHMAKING_PORT", "8083"),
		ProfilePort: getEnv("PROFILE_PORT", "8084"),
		PaymentPort: getEnv("PAYMENT_PORT", "8085"),
		ShopPort:    getEnv("SHOP_PORT", "8086"),
	}, nil
}

func (d DatabaseConfig) DSN() string {
	host := d.Host
	if host == "localhost" {
		host = "127.0.0.1" // IPv6 [::1] 회피
	}
	return "postgres://" + d.User + ":" + d.Password +
		"@" + host + ":" + strconv.Itoa(d.Port) +
		"/" + d.DBName + "?sslmode=disable"
}

func getEnv(key, fallback string) string {
	if v := os.Getenv(key); v != "" {
		return v
	}
	return fallback
}
