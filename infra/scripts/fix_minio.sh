#!/usr/bin/env bash
# Check MinIO bucket state and test upload directly via mc
set -euo pipefail
cd /opt/messenger/repo

echo "=== minio_init container logs ==="
docker compose -f docker-compose.yml -f docker-compose.prod.yml logs minio_init 2>&1 | tail -10 || echo "(no logs)"

echo "=== MinIO data directory ==="
docker exec repo-minio-1 ls /data/ 2>/dev/null || echo "(ls failed)"

echo "=== List buckets via mc ==="
docker run --rm --network repo_messenger_net \
  minio/mc:latest \
  sh -c "mc alias set local http://minio:9000 \
    $(grep MINIO_ROOT_USER /opt/messenger/env/.env | cut -d= -f2) \
    $(grep MINIO_ROOT_PASSWORD /opt/messenger/env/.env | cut -d= -f2) >/dev/null 2>&1 \
    && mc ls local/"

BUCKET=$(grep MINIO_BUCKET /opt/messenger/env/.env | cut -d= -f2)
echo "=== Expected bucket: $BUCKET ==="

echo "=== Create bucket if missing ==="
docker run --rm --network repo_messenger_net \
  minio/mc:latest \
  sh -c "mc alias set local http://minio:9000 \
    $(grep MINIO_ROOT_USER /opt/messenger/env/.env | cut -d= -f2) \
    $(grep MINIO_ROOT_PASSWORD /opt/messenger/env/.env | cut -d= -f2) >/dev/null 2>&1 \
    && mc mb --ignore-existing local/$BUCKET \
    && echo 'Bucket OK'"
