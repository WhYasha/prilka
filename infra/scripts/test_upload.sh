#!/usr/bin/env bash
set -euo pipefail
BASE="http://localhost/api"

TOKEN=$(curl -sf -X POST "$BASE/auth/login" \
  -H 'Content-Type: application/json' \
  -d '{"username":"smoketest_e2e","password":"SmokeTest@2026"}' \
  | python3 -c "import json,sys; print(json.load(sys.stdin)['access_token'])")

echo "Token: ${TOKEN:0:30}..."

echo "=== MinIO PUT error in api_cpp logs ==="
docker logs repo-api_cpp-1 2>&1 | grep -v 'RedisConnection\|Failed to connect' \
  | grep -E 'MinIO|minio|PUT' | tail -5

echo "=== Direct file upload ==="
echo "hello upload test" > /tmp/up.txt
curl -sv -X POST "$BASE/files" \
  -H "Authorization: Bearer $TOKEN" \
  -F "file=@/tmp/up.txt;type=text/plain" 2>&1 | grep -E "< HTTP|^\{"

echo "=== MinIO logs ==="
docker logs repo-minio-1 2>&1 | grep -v '^$' | tail -10

echo "=== MinIO bucket contents ==="
docker exec repo-minio-1 mc ls local/messenger-files 2>/dev/null || \
  docker compose -f /opt/messenger/repo/docker-compose.yml \
    -f /opt/messenger/repo/docker-compose.prod.yml \
    exec -T minio mc ls local/messenger-files 2>/dev/null || true
