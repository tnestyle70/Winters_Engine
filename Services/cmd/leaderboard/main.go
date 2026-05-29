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
	"winters-backend/internal/leaderboard"
	jwtauth "winters-backend/pkg/auth"
	"winters-backend/pkg/cache"
	"winters-backend/pkg/config"
	"winters-backend/pkg/database"
	"winters-backend/pkg/messaging"
	"winters-backend/pkg/middleware"
)

func main() {
	slog.SetDefault(slog.New(slog.NewJSONHandler(os.Stdout, &slog.HandlerOptions{Level: slog.LevelInfo})))

	cfg, err := config.Load()
	if err != nil {
		slog.Error("failed to load config", "error", err)
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

	rdb, err := cache.NewClient(ctx, cfg.Redis)
	if err != nil {
		slog.Error("failed to connect redis", "error", err)
		os.Exit(1)
	}
	defer rdb.Close()

	repo := leaderboard.NewRepository(db, rdb)

	if err := repo.SyncFromDB(ctx); err != nil {
		slog.Warn("failed to sync leaderboard from db", "error", err)
	}

	reader := messaging.NewReader(cfg.Kafka.Brokers, messaging.TopicMatchEvents, "leaderboard-consumer")
	defer reader.Close()
	consumer := leaderboard.NewConsumer(repo, reader)
	go consumer.Start(ctx)

	jwtMgr := jwtauth.NewJWTManager(cfg.JWT.AccessSecret, cfg.JWT.RefreshSecret, cfg.JWT.AccessTTL, cfg.JWT.RefreshTTL)
	handler := leaderboard.NewHandler(repo)

	r := chi.NewRouter()
	r.Use(middleware.Recovery, middleware.Logging)
	r.Group(func(r chi.Router) {
		r.Use(middleware.JWTAuth(jwtMgr))
		r.Mount("/leaderboard", handler.Routes())
	})
	r.Get("/health", func(w http.ResponseWriter, _ *http.Request) {
		w.WriteHeader(http.StatusOK)
		w.Write([]byte(`{"status":"ok"}`))
	})

	srv := &http.Server{Addr: ":" + cfg.LeaderPort, Handler: r, ReadTimeout: 10 * time.Second, WriteTimeout: 10 * time.Second}
	go func() {
		slog.Info("leaderboard service starting", "port", cfg.LeaderPort)
		if err := srv.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			slog.Error("server error", "error", err)
			os.Exit(1)
		}
	}()

	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit
	slog.Info("shutting down leaderboard service")
	shutdownCtx, shutdownCancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer shutdownCancel()
	srv.Shutdown(shutdownCtx)
}
