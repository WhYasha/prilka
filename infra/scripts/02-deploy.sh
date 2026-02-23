#!/usr/bin/env bash
# ============================================================
# 02-deploy.sh — deploy / update the stack (run as deploy)
# Usage: bash /opt/messenger/repo/infra/scripts/02-deploy.sh
# ============================================================
set -euo pipefail

APP_DIR="/opt/messenger"
REPO_DIR="${APP_DIR}/repo"
ENV_FILE="${APP_DIR}/env/.env"

log() { echo "[$(date +%T)] $*"; }

cd "${REPO_DIR}"

# ── Pull latest code ───────────────────────────────────────────────────────────
log "Pulling latest code..."
git fetch --all
git reset --hard origin/$(git rev-parse --abbrev-ref HEAD)

# ── Ensure .env symlink exists in repo dir ─────────────────────────────────────
[ -f .env ] || ln -sf "${ENV_FILE}" .env

# ── Copy production prometheus config ─────────────────────────────────────────
cp infra/prometheus/prometheus.prod.yml infra/prometheus/prometheus.yml

# ── Build images ───────────────────────────────────────────────────────────────
log "Building images..."
docker compose -f docker-compose.yml -f docker-compose.prod.yml build --pull

# ── Apply migrations (Flyway runs as one-shot on compose up) ───────────────────
# Migrations run automatically via the flyway service on `up`.
# To run manually: docker compose run --rm flyway

# ── Restart stack ──────────────────────────────────────────────────────────────
log "Restarting stack..."
sudo systemctl restart messenger

log ""
log "Deploy complete. Verify:"
log "  docker ps"
log "  curl http://localhost/health"
log "  curl http://localhost/api/health"
