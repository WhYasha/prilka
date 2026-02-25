#!/usr/bin/env bash
# ============================================================
# Vault One-Time Initialization Script
# Run this ONCE after first deploying Vault.
#
# Prerequisites:
#   - Vault container is running: docker compose up -d vault
#   - VAULT_ADDR is reachable
#
# Usage:
#   export VAULT_ADDR=http://localhost:8200
#   bash infra/vault/init.sh
#
# IMPORTANT: Save the unseal keys and root token securely!
# ============================================================
set -euo pipefail

VAULT_ADDR="${VAULT_ADDR:-http://localhost:8200}"
export VAULT_ADDR

log() { echo "[$(date +%T)] $*"; }

# ── 1. Initialize Vault ──────────────────────────────────────────────────────
log "Initializing Vault..."
INIT_OUTPUT=$(vault operator init -key-shares=5 -key-threshold=3 -format=json)

echo ""
echo "============================================================"
echo "  SAVE THESE CREDENTIALS SECURELY — THEY CANNOT BE RECOVERED"
echo "============================================================"
echo "${INIT_OUTPUT}" | jq .
echo "============================================================"
echo ""

# Extract root token and unseal keys
ROOT_TOKEN=$(echo "${INIT_OUTPUT}" | jq -r '.root_token')
UNSEAL_KEY_1=$(echo "${INIT_OUTPUT}" | jq -r '.unseal_keys_b64[0]')
UNSEAL_KEY_2=$(echo "${INIT_OUTPUT}" | jq -r '.unseal_keys_b64[1]')
UNSEAL_KEY_3=$(echo "${INIT_OUTPUT}" | jq -r '.unseal_keys_b64[2]')

# ── 2. Unseal Vault ──────────────────────────────────────────────────────────
log "Unsealing Vault (3 of 5 keys)..."
vault operator unseal "${UNSEAL_KEY_1}"
vault operator unseal "${UNSEAL_KEY_2}"
vault operator unseal "${UNSEAL_KEY_3}"

log "Vault unsealed successfully."

# ── 3. Authenticate as root ──────────────────────────────────────────────────
export VAULT_TOKEN="${ROOT_TOKEN}"

# ── 4. Enable KV v2 secrets engine ───────────────────────────────────────────
log "Enabling KV v2 secrets engine at secret/..."
vault secrets enable -version=2 -path=secret kv 2>/dev/null || \
  log "  KV v2 already enabled at secret/"

# ── 5. Write application policy ──────────────────────────────────────────────
log "Writing messenger policy..."
vault policy write messenger /vault/policies/messenger.hcl

# ── 6. Enable AppRole auth ──────────────────────────────────────────────────
log "Enabling AppRole auth method..."
vault auth enable approle 2>/dev/null || \
  log "  AppRole already enabled"

# ── 7. Create AppRole for api_cpp ─────────────────────────────────────────────
log "Creating api_cpp AppRole..."
vault write auth/approle/role/api_cpp \
  token_policies="messenger" \
  token_ttl=1h \
  token_max_ttl=4h \
  secret_id_ttl=0 \
  secret_id_num_uses=0

# ── 8. Get role-id and secret-id ─────────────────────────────────────────────
ROLE_ID=$(vault read -field=role_id auth/approle/role/api_cpp/role-id)
SECRET_ID=$(vault write -f -field=secret_id auth/approle/role/api_cpp/secret-id)

echo ""
echo "============================================================"
echo "  AppRole Credentials (save these for Vault Agent config)"
echo "============================================================"
echo "  ROLE_ID:   ${ROLE_ID}"
echo "  SECRET_ID: ${SECRET_ID}"
echo "============================================================"
echo ""

# ── 9. Populate secrets ──────────────────────────────────────────────────────
log "Populating secrets (placeholders — update with real values!)..."

vault kv put secret/messenger/db \
  host=postgres \
  port=5432 \
  name=messenger \
  user=messenger \
  password=CHANGE_ME_DB_PASSWORD

vault kv put secret/messenger/redis \
  host=redis \
  port=6379 \
  password=CHANGE_ME_REDIS_PASSWORD

vault kv put secret/messenger/minio \
  endpoint=minio:9000 \
  access_key=minioadmin \
  secret_key=CHANGE_ME_MINIO_PASSWORD \
  public_url=https://behappy.rest/minio \
  avatars_bucket=bh-avatars \
  uploads_bucket=bh-uploads \
  stickers_bucket=bh-stickers \
  tests_bucket=bh-test-artifacts \
  presign_ttl=900

vault kv put secret/messenger/jwt \
  secret=CHANGE_ME_JWT_SECRET_AT_LEAST_32_CHARS \
  access_ttl=3600 \
  refresh_ttl=604800

vault kv put secret/messenger/server \
  port=8080 \
  threads=0 \
  max_file_size_mb=50

vault kv put secret/messenger/grafana \
  user=admin \
  password=CHANGE_ME_GRAFANA_PASSWORD

log ""
log "============================================================"
log "Vault initialization complete."
log ""
log "NEXT STEPS:"
log "  1. Create Vault credentials directory:"
log "     mkdir -p /opt/messenger/vault && chmod 700 /opt/messenger/vault"
log "  2. Save unseal keys for auto-unseal:"
log "     echo '${INIT_OUTPUT}' | jq '{unseal_keys_b64}' > /opt/messenger/vault/unseal-keys.json"
log "     chmod 600 /opt/messenger/vault/unseal-keys.json"
log "  3. Save root token securely (revoke after setup):"
log "     echo '${ROOT_TOKEN}' > /opt/messenger/vault/root-token"
log "     chmod 600 /opt/messenger/vault/root-token"
log "  4. Save AppRole credentials for Vault Agent:"
log "     echo '${ROLE_ID}' > /opt/messenger/vault/role-id"
log "     echo '${SECRET_ID}' > /opt/messenger/vault/secret-id"
log "     chmod 600 /opt/messenger/vault/role-id /opt/messenger/vault/secret-id"
log "  5. Update secrets with real production values:"
log "     vault kv put secret/messenger/db password=<real_pw>"
log "     vault kv put secret/messenger/redis password=<real_pw>"
log "     vault kv put secret/messenger/minio secret_key=<real_pw>"
log "     vault kv put secret/messenger/jwt secret=<real_jwt_secret>"
log "     vault kv put secret/messenger/grafana password=<real_pw>"
log "  6. Test .env rendering:"
log "     vault agent -config=infra/vault/agent-init.hcl"
log "     cat /opt/messenger/env/.env"
log "============================================================"
