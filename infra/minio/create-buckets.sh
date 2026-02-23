#!/bin/sh
# Called by minio_init service — creates default bucket
set -e

ENDPOINT="http://minio:9000"
BUCKET="${MINIO_BUCKET:-messenger-files}"
ACCESS="${MINIO_ROOT_USER:-minioadmin}"
SECRET="${MINIO_ROOT_PASSWORD:-changeme_minio}"

mc alias set local "$ENDPOINT" "$ACCESS" "$SECRET"
mc mb --ignore-existing "local/$BUCKET"
# Private bucket — access only via presigned URLs
mc anonymous set none "local/$BUCKET"
echo "Bucket '$BUCKET' ready."
