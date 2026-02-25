#!/usr/bin/env bash
# ============================================================
# migrate-to-nomad.sh — Migrate from Docker Compose to Nomad
#
# Architecture after migration:
#   Tier 0 (systemd): vault, consul, apisix, nomad
#   Tier 1 (Nomad):   postgres, redis, minio, api_cpp,
#                      prometheus, grafana, cadvisor
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

    log "Stopping standalone services..."
    systemctl stop nomad 2>/dev/null || true
    systemctl stop apisix 2>/dev/null || true
    systemctl stop consul 2>/dev/null || true
    systemctl stop vault 2>/dev/null || true
    systemctl disable nomad apisix consul vault 2>/dev/null || true

    # Clean up standalone containers
    docker rm -f messenger-vault messenger-consul messenger-apisix 2>/dev/null || true

    log "Re-enabling Docker Compose stack..."
    systemctl enable messenger
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

for cmd in docker nomad consul vault jq; do
    if ! command -v "${cmd}" &>/dev/null; then
        err "${cmd} is not installed."
        exit 1
    fi
done

if [ ! -f "${APP_DIR}/vault/role-id" ] || [ ! -f "${APP_DIR}/vault/secret-id" ]; then
    err "Vault AppRole credentials not found at ${APP_DIR}/vault/"
    exit 1
fi

if [ ! -f "${APP_DIR}/vault/unseal-keys.json" ]; then
    err "Vault unseal keys not found at ${APP_DIR}/vault/unseal-keys.json"
    exit 1
fi

# ── Step 1: Build api_cpp image ──────────────────────────────────────────────
log "=== Step 1: Building messenger-api:latest image ==="
docker build -t messenger-api:latest ./backend

# ── Step 2: Create Docker network + migrate volumes ──────────────────────────
log "=== Step 2: Creating Docker network + migrating volumes ==="
docker network create messenger_net 2>/dev/null || log "  Network messenger_net already exists."

# Detect Docker Compose project name
COMPOSE_PROJECT=$(docker ps --format '{{.Names}}' | grep postgres | head -1 | sed 's/-postgres.*//' || echo "repo")
log "Detected Compose project: ${COMPOSE_PROJECT}"

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
    docker run --rm -v "${SRC}:/from:ro" -v "${DST}:/to" alpine sh -c "cp -a /from/. /to/"
    log "  Done."
done

# ── Step 3: Stop Docker Compose stack ────────────────────────────────────────
log "=== Step 3: Stopping Docker Compose stack ==="
systemctl stop messenger || true
systemctl disable messenger || true
docker compose -f docker-compose.yml -f docker-compose.prod.yml down 2>/dev/null || true

# ── Step 4: Install systemd units + start infrastructure ─────────────────────
log "=== Step 4: Starting infrastructure (Vault + Consul + APISIX + Nomad) ==="

cp infra/systemd/vault.service /etc/systemd/system/
cp infra/systemd/consul.service /etc/systemd/system/
cp infra/systemd/apisix.service /etc/systemd/system/
cp infra/systemd/nomad.service /etc/systemd/system/
systemctl daemon-reload

mkdir -p /opt/messenger/nomad/data

# Start Vault first (needed by Nomad for secrets)
systemctl enable vault
systemctl start vault
sleep 5

# Unseal Vault
log "Unsealing Vault..."
export VAULT_ADDR=http://127.0.0.1:8200
bash infra/vault/unseal.sh

# Start Consul (needed by Nomad for service registration)
systemctl enable consul
systemctl start consul
sleep 3

# Start APISIX (the gateway — needs to be running before health checks)
systemctl enable apisix
systemctl start apisix
sleep 3

# Start Nomad
systemctl enable nomad
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
    log "Architecture:"
    log "  Tier 0 (systemd): vault, consul, apisix, nomad"
    log "  Tier 1 (Nomad):   postgres, redis, minio, api_cpp,"
    log "                     prometheus, grafana, cadvisor"
    log ""
    log "Verify:"
    log "  nomad job status messenger"
    log "  consul catalog services"
    log "  curl https://api.behappy.rest/health"
    log ""
    log "Rollback:"
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
