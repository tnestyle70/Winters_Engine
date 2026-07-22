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
	"winters-backend/internal/matchmaking"
	jwtauth "winters-backend/pkg/auth"
	"winters-backend/pkg/config"
	"winters-backend/pkg/database"
	"winters-backend/pkg/matchticket"
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

	writer := messaging.NewWriter(cfg.Kafka.Brokers, messaging.TopicMatchEvents, cfg.Kafka.UseTLS)
	defer writer.Close()

	ticketSigner, err := matchticket.NewSigner(
		cfg.GameSession.TicketSecret,
		cfg.GameSession.TicketTTL)
	if err != nil {
		slog.Error("failed to configure match tickets", "error", err)
		os.Exit(1)
	}
	svc := matchmaking.NewService(
		db,
		writer,
		ticketSigner,
		matchmaking.GameAllocation{
			Host:          cfg.GameSession.Host,
			Port:          cfg.GameSession.Port,
			Transport:     cfg.GameSession.Transport,
			GameSessionID: cfg.GameSession.GameSessionID,
		},
		cfg.GameSession.MatchMaxSize)
	handler := matchmaking.NewHandler(svc, cfg.GameSession.InternalTokenSecret)

	go svc.RunOutboxPublisher(ctx)

	jwtMgr := jwtauth.NewJWTManager(cfg.JWT.AccessSecret, cfg.JWT.RefreshSecret, cfg.JWT.AccessTTL, cfg.JWT.RefreshTTL)

	r := chi.NewRouter()
	r.Use(middleware.Recovery, middleware.Logging)
	r.Mount("/internal", handler.InternalRoutes())
	r.Group(func(r chi.Router) {
		r.Use(middleware.JWTAuth(jwtMgr))
		r.Mount("/matchmaking", handler.Routes())
	})
	r.Get("/health", func(w http.ResponseWriter, _ *http.Request) {
		w.WriteHeader(http.StatusOK)
		w.Write([]byte(`{"status":"ok"}`))
	})

	srv := &http.Server{Addr: ":" + cfg.MatchPort, Handler: r, ReadTimeout: 10 * time.Second, WriteTimeout: 10 * time.Second}
	go func() {
		slog.Info("matchmaking service starting", "port", cfg.MatchPort)
		if err := srv.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			slog.Error("server error", "error", err)
			os.Exit(1)
		}
	}()

	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit
	slog.Info("shutting down matchmaking service")
	shutdownCtx, shutdownCancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer shutdownCancel()
	srv.Shutdown(shutdownCtx)
}
