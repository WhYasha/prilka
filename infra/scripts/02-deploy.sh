#!/usr/bin/env bash
# ============================================================
# 02-deploy.sh — deploy / update the stack (run as deploy)
# Usage: bash /opt/messenger/repo/infra/scripts/02-deploy.sh
#
# Supports two modes:
#   - Docker Compose (default): orchestrates all services
#   - Nomad: delegates to Nomad job scheduler
#
# Set DEPLOY_MODE=nomad to use Nomad orchestration.
# ============================================================
set -euo pipefail

APP_DIR="/opt/messenger"
REPO_DIR="${APP_DIR}/repo"
ENV_FILE="${APP_DIR}/env/.env"
VAULT_DIR="${APP_DIR}/vault"
DEPLOY_MODE="${DEPLOY_MODE:-compose}"

log() { echo "[$(date +%T)] $*"; }

cd "${REPO_DIR}"

# ── Pull latest code ───────────────────────────────────────────────────────────
log "Pulling latest code..."
git fetch --all
git reset --hard origin/$(git rev-parse --abbrev-ref HEAD)

# ── Copy production prometheus config ─────────────────────────────────────────
cp infra/prometheus/prometheus.prod.yml infra/prometheus/prometheus.yml

# ── Inject TLS certs into APISIX config ────────────────────────────────────────
log "Injecting TLS certificates..."
bash infra/scripts/inject-certs.sh "${REPO_DIR}"

# ── Vault: unseal + render .env ───────────────────────────────────────────────
# If Vault credentials exist, use Vault Agent to render .env from secrets.
# Otherwise fall back to manual .env file.
if [ -f "${VAULT_DIR}/role-id" ] && [ -f "${VAULT_DIR}/secret-id" ]; then
    log "Vault credentials found — rendering .env from Vault..."

    # Ensure Vault container is running
    docker compose -f docker-compose.yml -f docker-compose.prod.yml up -d vault
    sleep 3

    # Auto-unseal if sealed
    bash infra/vault/unseal.sh || {
        log "WARNING: Vault unseal failed; falling back to existing .env"
    }

    # Render .env via Vault Agent (one-shot)
    vault agent -config=infra/vault/agent-init.hcl && {
        log ".env rendered from Vault successfully."
    } || {
        log "WARNING: Vault Agent render failed; falling back to existing .env"
    }
else
    log "No Vault credentials found — using manual .env file."
fi

# ── Ensure .env symlink exists in repo dir ─────────────────────────────────────
[ -f .env ] || ln -sf "${ENV_FILE}" .env

if [ "${DEPLOY_MODE}" = "nomad" ]; then
    # ── Nomad deployment ─────────────────────────────────────────────────────
    log "Deploying via Nomad..."

    # Build the api_cpp image (Nomad references it as messenger-api:latest)
    docker build -t messenger-api:latest ./backend

    # Run the Nomad job
    nomad job run infra/nomad/jobs/messenger.nomad.hcl

    log ""
    log "Nomad deploy complete. Verify:"
    log "  nomad job status messenger"
    log "  curl https://api.behappy.rest/health"
else
    # ── Docker Compose deployment ────────────────────────────────────────────
    log "Building images..."
    docker compose -f docker-compose.yml -f docker-compose.prod.yml build --pull

    log "Restarting stack..."
    sudo systemctl restart messenger

    log ""
    log "Deploy complete. Verify:"
    log "  docker ps"
    log "  curl https://api.behappy.rest/health"
fi
