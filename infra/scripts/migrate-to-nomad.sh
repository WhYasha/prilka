#!/usr/bin/env bash
# ============================================================
# migrate-to-nomad.sh — Migrate from Docker Compose to Nomad
#
# This script:
#   1. Copies Docker Compose volume data to Nomad-compatible volumes
#   2. Stops the Docker Compose stack
#   3. Starts standalone Vault + Consul + Nomad
#   4. Runs the Nomad job
#   5. Verifies health
#
# IMPORTANT: This causes a brief service outage (~30-60 seconds).
# Run during a maintenance window.
#
# Usage: sudo bash infra/scripts/migrate-to-nomad.sh
# Rollback: sudo bash infra/scripts/migrate-to-nomad.sh --rollback
# ============================================================
set -euo pipefail

APP_DIR="/opt/messenger"
REPO_DIR="${APP_DIR}/repo"

log() { echo "[$(date +%T)] $*"; }
err() { echo "[$(date +%T)] ERROR: $*" >&2; }

cd "${REPO_DIR}"

# ── Rollback mode ─────────────────────────────────────────────────────────────
if [ "${1:-}" = "--rollback" ]; then
    log "=== ROLLBACK: Reverting to Docker Compose ==="

    log "Stopping Nomad job..."
    nomad job stop -purge messenger 2>/dev/null || true

    log "Stopping Nomad, Consul, Vault services..."
    systemctl stop nomad 2>/dev/null || true
    systemctl stop consul 2>/dev/null || true
    systemctl stop vault 2>/dev/null || true
    systemctl disable nomad consul vault 2>/dev/null || true

    log "Starting Docker Compose stack..."
    systemctl start messenger

    log "Waiting for health check..."
    for i in $(seq 1 30); do
        if curl -sf https://api.behappy.rest/health >/dev/null 2>&1; then
            log "Rollback complete — Docker Compose stack is healthy."
            exit 0
        fi
        sleep 2
    done

    err "Health check failed after rollback. Check: docker ps"
    exit 1
fi

# ── Pre-flight checks ─────────────────────────────────────────────────────────
log "=== Pre-flight checks ==="

# Check required tools
for cmd in docker nomad consul vault jq; do
    if ! command -v "${cmd}" &>/dev/null; then
        err "${cmd} is not installed."
        exit 1
    fi
done

# Check Vault credentials
if [ ! -f "${APP_DIR}/vault/role-id" ] || [ ! -f "${APP_DIR}/vault/secret-id" ]; then
    err "Vault AppRole credentials not found at ${APP_DIR}/vault/"
    err "Initialize Vault first: bash infra/vault/init.sh"
    exit 1
fi

# Check unseal keys
if [ ! -f "${APP_DIR}/vault/unseal-keys.json" ]; then
    err "Vault unseal keys not found at ${APP_DIR}/vault/unseal-keys.json"
    exit 1
fi

# Build api_cpp image
log "Building messenger-api:latest image..."
docker build -t messenger-api:latest ./backend

# ── Step 1: Detect and migrate Docker volumes ─────────────────────────────────
log "=== Step 1: Migrating Docker volumes ==="

# Detect Docker Compose project name from running containers
COMPOSE_PROJECT=$(docker ps --format '{{.Names}}' | grep postgres | head -1 | sed 's/-postgres.*//' || echo "repo")
log "Detected Compose project: ${COMPOSE_PROJECT}"

# Volume mapping: compose_name -> nomad_name
declare -A VOLUMES=(
    ["${COMPOSE_PROJECT}_postgres_data"]="messenger-postgres-data"
    ["${COMPOSE_PROJECT}_redis_data"]="messenger-redis-data"
    ["${COMPOSE_PROJECT}_minio_data"]="messenger-minio-data"
    ["${COMPOSE_PROJECT}_prometheus_data"]="messenger-prometheus-data"
    ["${COMPOSE_PROJECT}_grafana_data"]="messenger-grafana-data"
)

for SRC in "${!VOLUMES[@]}"; do
    DST="${VOLUMES[$SRC]}"

    if ! docker volume inspect "${SRC}" >/dev/null 2>&1; then
        log "  Source volume ${SRC} not found, skipping."
        continue
    fi

    if docker volume inspect "${DST}" >/dev/null 2>&1; then
        log "  Target volume ${DST} already exists, skipping copy."
        continue
    fi

    log "  Copying ${SRC} → ${DST}..."
    docker volume create "${DST}"
    docker run --rm \
        -v "${SRC}:/from:ro" \
        -v "${DST}:/to" \
        alpine sh -c "cp -a /from/. /to/"
    log "  Done."
done

# ── Step 2: Create Docker network ────────────────────────────────────────────
log "=== Step 2: Creating Docker network ==="
docker network create messenger_net 2>/dev/null || log "  Network messenger_net already exists."

# ── Step 3: Stop Docker Compose ──────────────────────────────────────────────
log "=== Step 3: Stopping Docker Compose stack ==="
systemctl stop messenger
systemctl disable messenger

# Clean up Compose containers (they may linger)
docker compose -f docker-compose.yml -f docker-compose.prod.yml down 2>/dev/null || true

# ── Step 4: Install and start infrastructure services ─────────────────────────
log "=== Step 4: Starting Vault + Consul + Nomad ==="

# Copy systemd units
cp infra/systemd/vault.service /etc/systemd/system/
cp infra/systemd/consul.service /etc/systemd/system/
cp infra/systemd/nomad.service /etc/systemd/system/
systemctl daemon-reload

# Create Nomad data directory
mkdir -p /opt/messenger/nomad/data

# Start infrastructure services
systemctl enable vault consul nomad
systemctl start vault
sleep 5

# Unseal Vault
log "Unsealing Vault..."
export VAULT_ADDR=http://127.0.0.1:8200
bash infra/vault/unseal.sh

systemctl start consul
sleep 3

systemctl start nomad
sleep 5

# ── Step 5: Run Nomad job ─────────────────────────────────────────────────────
log "=== Step 5: Deploying Nomad job ==="
nomad job run infra/nomad/jobs/messenger.nomad.hcl

# ── Step 6: Verify health ────────────────────────────────────────────────────
log "=== Step 6: Verifying health ==="
log "Waiting for services to start (up to 120s)..."

HEALTHY=false
for i in $(seq 1 60); do
    if curl -sf https://api.behappy.rest/health >/dev/null 2>&1; then
        HEALTHY=true
        break
    fi
    sleep 2
done

if [ "${HEALTHY}" = "true" ]; then
    log ""
    log "============================================================"
    log "Migration to Nomad complete!"
    log ""
    log "Verify:"
    log "  nomad job status messenger"
    log "  consul catalog services"
    log "  curl https://api.behappy.rest/health"
    log "  curl -I https://grafana.behappy.rest/"
    log ""
    log "Rollback (if needed):"
    log "  sudo bash infra/scripts/migrate-to-nomad.sh --rollback"
    log "============================================================"
else
    err "Health check failed after 120s."
    err "Check: nomad job status messenger"
    err "       docker ps"
    err ""
    err "To rollback: sudo bash infra/scripts/migrate-to-nomad.sh --rollback"
    exit 1
fi
