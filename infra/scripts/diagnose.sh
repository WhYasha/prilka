#!/usr/bin/env bash
# Diagnose the three failing smoke test checks
set -euo pipefail
BASE="http://localhost/api"

# Get token
TOKEN=$(curl -sf -X POST "$BASE/auth/login" \
  -H 'Content-Type: application/json' \
  -d '{"username":"smoketest_e2e","password":"SmokeTest@2026"}' \
  | python3 -c "import json,sys; print(json.load(sys.stdin)['access_token'])")
echo "TOKEN: ${TOKEN:0:40}..."

echo
echo "=== GET /chats/2/messages ==="
curl -sv "$BASE/chats/2/messages" -H "Authorization: Bearer $TOKEN" 2>&1 | grep -E "< HTTP|^\{|\[" | head -5

echo
echo "=== POST /files (curl -F) ==="
echo "test upload content" > /tmp/testfile.txt
curl -sv -X POST "$BASE/files" \
  -H "Authorization: Bearer $TOKEN" \
  -F "file=@/tmp/testfile.txt;type=text/plain" 2>&1 | grep -E "< HTTP|^\{" | head -5

echo
echo "=== MinIO reachability from host ==="
curl -sv http://localhost:9000/minio/health/live 2>&1 | grep "< HTTP" | head -3

echo
echo "=== Redis env inside api_cpp container ==="
docker exec repo-api_cpp-1 env | grep -i redis

echo
echo "=== pip3 on server ==="
pip3 --version 2>/dev/null || python3 -m pip --version 2>/dev/null || echo "no pip"

echo
echo "=== websockets installed? ==="
python3 -c "import websockets; print('websockets', websockets.__version__)" 2>/dev/null || echo "not installed"
