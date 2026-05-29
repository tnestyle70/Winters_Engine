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
	"winters-backend/internal/shop"
	jwtauth "winters-backend/pkg/auth"
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

	writer := messaging.NewWriter(cfg.Kafka.Brokers, messaging.TopicPlayerEvents)
	defer writer.Close()

	repo := shop.NewRepository(db)
	svc := shop.NewService(repo, writer)

	jwtMgr := jwtauth.NewJWTManager(cfg.JWT.AccessSecret, cfg.JWT.RefreshSecret, cfg.JWT.AccessTTL, cfg.JWT.RefreshTTL)
	handler := shop.NewHandler(svc)

	r := chi.NewRouter()
	r.Use(middleware.Recovery, middleware.Logging)
	r.Mount("/shop", func() chi.Router {
		sr := chi.NewRouter()
		sr.Get("/items", handler.ListItems)
		sr.Group(func(r chi.Router) {
			r.Use(middleware.JWTAuth(jwtMgr))
			r.Post("/purchase", handler.Purchase)
			r.Get("/inventory/{user_id}", handler.GetInventory)
		})
		return sr
	}())
	r.Get("/health", func(w http.ResponseWriter, _ *http.Request) {
		w.WriteHeader(http.StatusOK)
		w.Write([]byte(`{"status":"ok"}`))
	})

	srv := &http.Server{Addr: ":" + cfg.ShopPort, Handler: r, ReadTimeout: 10 * time.Second, WriteTimeout: 10 * time.Second}
	go func() {
		slog.Info("shop service starting", "port", cfg.ShopPort)
		if err := srv.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			slog.Error("server error", "error", err)
			os.Exit(1)
		}
	}()

	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit
	slog.Info("shutting down shop service")
	shutdownCtx, shutdownCancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer shutdownCancel()
	srv.Shutdown(shutdownCtx)
}
