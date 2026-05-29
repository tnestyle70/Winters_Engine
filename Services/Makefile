.PHONY: up down migrate auth leaderboard matchmaking profile payment shop

up: 
	docker compose up -d

down:
	docker compose down

migrate:
	@for f in migrations/*.up.sql; do \
		echo "Applying $$f"; \
		PGPASSWORD=winters_dev_2026 psql -h localhost -U winters -d winters -f $$f; \
	done

auth:
	go run ./cmd/auth
leaderboard:
	go run ./cmd/leaderboard
matchmaking:
	go run ./cmd/matchmaking
profile:
	go run ./cmd/profile
payment:
	go run ./cmd/payment
shop:
	go run ./cmd/shop