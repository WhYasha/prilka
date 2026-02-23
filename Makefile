# ============================================================
# Messenger – Makefile
# ============================================================

COMPOSE = docker compose
ENV_FILE = .env

.PHONY: up down restart logs migrate test build shell-api shell-postgres clean \
        prod-up prod-down prod-logs prod-build deploy

## Start all services (build if needed)
up:
	@cp -n .env.example .env 2>/dev/null || true
	$(COMPOSE) up --build -d

## Stop all services
down:
	$(COMPOSE) down

## Restart a specific service: make restart svc=api_cpp
restart:
	$(COMPOSE) restart $(svc)

## Tail logs from all services (or: make logs svc=api_cpp)
logs:
ifdef svc
	$(COMPOSE) logs -f $(svc)
else
	$(COMPOSE) logs -f
endif

## Run Flyway migrations manually (also runs automatically on `make up`)
migrate:
	$(COMPOSE) run --rm flyway

## Run backend unit tests inside Docker
test:
	docker build -t messenger-backend-test \
		--target test ./backend
	docker run --rm messenger-backend-test

## Build the C++ backend image only
build:
	$(COMPOSE) build api_cpp

## Open a shell in the api_cpp container
shell-api:
	$(COMPOSE) exec api_cpp /bin/bash

## Open a psql shell
shell-postgres:
	$(COMPOSE) exec postgres psql \
		-U $${POSTGRES_USER:-messenger} \
		-d $${POSTGRES_DB:-messenger}

## Remove all volumes and containers (DESTRUCTIVE)
clean:
	$(COMPOSE) down -v --remove-orphans

## Show running services
ps:
	$(COMPOSE) ps

## ── Production targets ────────────────────────────────────────────────────────
COMPOSE_PROD = docker compose -f docker-compose.yml -f docker-compose.prod.yml

## Start the production stack
prod-up:
	$(COMPOSE_PROD) up -d --remove-orphans

## Stop the production stack
prod-down:
	$(COMPOSE_PROD) down

## Tail logs (production)
prod-logs:
ifdef svc
	$(COMPOSE_PROD) logs -f $(svc)
else
	$(COMPOSE_PROD) logs -f
endif

## Build production images
prod-build:
	$(COMPOSE_PROD) build --pull

## Full deploy: pull, build, restart (run on the server as deploy)
deploy:
	bash infra/scripts/02-deploy.sh

## Show URLs of all UIs
info:
	@echo ""
	@echo "  API (C++):    http://localhost:8080"
	@echo "  API (Legacy): http://localhost:5000"
	@echo "  MinIO UI:     http://localhost:9001"
	@echo "  Prometheus:   http://localhost:9090"
	@echo "  Grafana:      http://localhost:3000  (admin/admin)"
	@echo "  cAdvisor:     http://localhost:8081"
	@echo ""
