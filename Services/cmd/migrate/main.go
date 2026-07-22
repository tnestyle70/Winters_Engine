package main

import (
	"context"
	"log/slog"
	"os"

	"winters-backend/migrations"
	"winters-backend/pkg/config"
	"winters-backend/pkg/database"
)

func main() {
	slog.SetDefault(slog.New(slog.NewJSONHandler(os.Stdout, nil)))
	configuration, err := config.Load()
	if err != nil {
		slog.Error("failed to load migration config", "error", err)
		os.Exit(1)
	}
	pool, err := database.NewPool(context.Background(), configuration.DB)
	if err != nil {
		slog.Error("failed to connect migration database", "error", err)
		os.Exit(1)
	}
	defer pool.Close()
	if err := migrations.Apply(context.Background(), pool); err != nil {
		slog.Error("database migration failed", "error", err)
		os.Exit(1)
	}
	slog.Info("database migrations are current")
}
