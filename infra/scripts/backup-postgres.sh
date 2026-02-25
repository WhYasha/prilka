#!/usr/bin/env bash
# ============================================================
# backup-postgres.sh — daily Postgres dump with 7-day retention
# Runs as: deploy user via systemd timer (messenger-backup.timer)
# Compatible with both Docker Compose and Nomad container names.
# ============================================================
set -euo pipefail

APP_DIR="/opt/messenger"
ENV_FILE="${APP_DIR}/env/.env"
BACKUP_DIR="${APP_DIR}/backups"
RETENTION_DAYS=7
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
DUMP_FILE="${BACKUP_DIR}/postgres_${TIMESTAMP}.sql.gz"

# Load env vars (only what we need)
# shellcheck disable=SC1090
source <(grep -E '^(POSTGRES_DB|POSTGRES_USER|POSTGRES_PASSWORD)=' "${ENV_FILE}")

log() { echo "[$(date +%T)] $*"; }

mkdir -p "${BACKUP_DIR}"

# Find the Postgres container (works with both Compose and Nomad naming)
PG_CONTAINER=$(docker ps --filter "name=postgres" --format "{{.Names}}" | head -1)
if [ -z "${PG_CONTAINER}" ]; then
    log "ERROR: No running Postgres container found"
    exit 1
fi
log "Found Postgres container: ${PG_CONTAINER}"

log "Starting Postgres backup -> ${DUMP_FILE}"
docker exec "${PG_CONTAINER}" \
    pg_dump \
    -U "${POSTGRES_USER}" \
    -d "${POSTGRES_DB}" \
    --format=plain \
    --no-owner \
    --no-acl \
    | gzip > "${DUMP_FILE}"

SIZE=$(du -sh "${DUMP_FILE}" | cut -f1)
log "Backup written: ${DUMP_FILE} (${SIZE})"

# ── Rotate old backups ─────────────────────────────────────────────────────────
log "Removing backups older than ${RETENTION_DAYS} days..."
find "${BACKUP_DIR}" -name "postgres_*.sql.gz" \
    -mtime "+${RETENTION_DAYS}" -delete -print \
    | sed 's/^/  Deleted: /'

log "Backup complete."

# ── Optional: MinIO backup note ────────────────────────────────────────────────
# MinIO stores data in the named Docker volume 'minio_data'.
# To back it up, either:
#   a) Use MinIO Client (mc mirror):
#      docker run --rm --network messenger_net \
#        minio/mc mirror local/messenger-files /backup/minio/
#   b) Copy the raw volume (only safe while MinIO is stopped):
#      docker run --rm -v messenger_minio_data:/src:ro -v /opt/messenger/backups/minio:/dst \
#        alpine tar czf /dst/minio_$(date +%Y%m%d).tar.gz -C /src .
