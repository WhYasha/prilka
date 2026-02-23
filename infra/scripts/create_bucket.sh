#!/usr/bin/env bash
set -euo pipefail
ENV_FILE="/opt/messenger/env/.env"
COMPOSE_NET="repo_messenger_net"

MINIO_USER=$(grep '^MINIO_ROOT_USER=' "$ENV_FILE" | cut -d= -f2)
MINIO_PASS=$(grep '^MINIO_ROOT_PASSWORD=' "$ENV_FILE" | cut -d= -f2)
BUCKET=$(grep '^MINIO_BUCKET=' "$ENV_FILE" | cut -d= -f2)

echo "Credentials: user=$MINIO_USER bucket=$BUCKET"
echo "=== Creating bucket $BUCKET ==="
docker run --rm \
  --network "$COMPOSE_NET" \
  -e "MC_HOST_local=http://${MINIO_USER}:${MINIO_PASS}@minio:9000" \
  minio/mc:latest mb --ignore-existing "local/$BUCKET"

echo "=== Setting anonymous policy to none ==="
docker run --rm \
  --network "$COMPOSE_NET" \
  -e "MC_HOST_local=http://${MINIO_USER}:${MINIO_PASS}@minio:9000" \
  minio/mc:latest anonymous set none "local/$BUCKET"

echo "=== Listing buckets ==="
docker run --rm \
  --network "$COMPOSE_NET" \
  -e "MC_HOST_local=http://${MINIO_USER}:${MINIO_PASS}@minio:9000" \
  minio/mc:latest ls local/
