package main

import (
	"context"
	"log/slog"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/go-chi/chi/v5"
	"winters-backend/internal/replay"
	jwtauth "winters-backend/pkg/auth"
	"winters-backend/pkg/config"
	"winters-backend/pkg/database"
	"winters-backend/pkg/middleware"
)

func main() {
	slog.SetDefault(slog.New(slog.NewJSONHandler(
		os.Stdout, &slog.HandlerOptions{Level: slog.LevelInfo})))

	cfg, err := config.Load()
	if err != nil {
		slog.Error("failed to load config", "error", err)
		os.Exit(1)
	}
	if err := cfg.Replay.Validate(cfg.Environment); err != nil {
		slog.Error("invalid replay config", "error", err)
		os.Exit(1)
	}

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	db, err := database.NewPool(ctx, cfg.DB)
	if err != nil {
		slog.Error("failed to connect database", "error", err)
		os.Exit(1)
	}
	defer db.Close()
	storage, err := replay.NewS3Storage(ctx, cfg.Replay)
	if err != nil {
		slog.Error("failed to configure replay storage", "error", err)
		os.Exit(1)
	}
	if err := storage.EnsureBucket(ctx, cfg.Environment != "production"); err != nil {
		slog.Error("replay bucket unavailable", "bucket", cfg.Replay.Bucket, "error", err)
		os.Exit(1)
	}

	repository := replay.NewRepository(db)
	service := replay.NewService(repository, storage, cfg.Replay)
	handler := replay.NewHandler(service, cfg.Replay.InternalTokenSecret)
	jwtManager := jwtauth.NewJWTManager(
		cfg.JWT.AccessSecret, cfg.JWT.RefreshSecret,
		cfg.JWT.AccessTTL, cfg.JWT.RefreshTTL)

	router := chi.NewRouter()
	router.Use(middleware.Recovery, middleware.Logging)
	router.Mount("/internal", handler.InternalRoutes())
	router.Group(func(router chi.Router) {
		router.Use(middleware.JWTAuth(jwtManager))
		router.Mount("/replay", handler.UserRoutes())
	})
	router.Get("/health", func(w http.ResponseWriter, _ *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusOK)
		_, _ = w.Write([]byte(`{"status":"ok"}`))
	})

	server := &http.Server{
		Addr:         ":" + cfg.ReplayPort,
		Handler:      router,
		ReadTimeout:  10 * time.Second,
		WriteTimeout: 15 * time.Second,
		IdleTimeout:  60 * time.Second,
	}
	go func() {
		slog.Info("replay service starting", "port", cfg.ReplayPort, "bucket", cfg.Replay.Bucket)
		if err := server.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			slog.Error("replay service failed", "error", err)
			cancel()
		}
	}()

	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	select {
	case <-quit:
	case <-ctx.Done():
	}
	shutdownCtx, shutdownCancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer shutdownCancel()
	if err := server.Shutdown(shutdownCtx); err != nil {
		slog.Error("replay service shutdown failed", "error", err)
	}
}
