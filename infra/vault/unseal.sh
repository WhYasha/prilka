#!/usr/bin/env bash
# ============================================================
# unseal.sh — Auto-unseal Vault from saved keys
# Reads unseal keys from /opt/messenger/vault/unseal-keys.json
#
# Usage: bash infra/vault/unseal.sh
# ============================================================
set -euo pipefail

VAULT_ADDR="${VAULT_ADDR:-http://127.0.0.1:8200}"
export VAULT_ADDR

KEYS_FILE="${VAULT_KEYS_FILE:-/opt/messenger/vault/unseal-keys.json}"

log() { echo "[$(date +%T)] $*"; }

# ── Check if Vault is reachable ──────────────────────────────────────────────
if ! vault status -format=json >/dev/null 2>&1; then
    log "ERROR: Vault is not reachable at ${VAULT_ADDR}"
    log "Ensure the Vault container is running: docker compose up -d vault"
    exit 1
fi

# ── Check if already unsealed ─────────────────────────────────────────────────
SEALED=$(vault status -format=json 2>/dev/null | jq -r '.sealed')
if [ "${SEALED}" = "false" ]; then
    log "Vault is already unsealed."
    exit 0
fi

# ── Read unseal keys ─────────────────────────────────────────────────────────
if [ ! -f "${KEYS_FILE}" ]; then
    log "ERROR: Unseal keys file not found: ${KEYS_FILE}"
    log "Run 'bash infra/vault/init.sh' first, then save the output:"
    log "  jq '{unseal_keys_b64: .unseal_keys_b64}' > ${KEYS_FILE}"
    log "  chmod 600 ${KEYS_FILE}"
    exit 1
fi

log "Unsealing Vault..."

KEY_1=$(jq -r '.unseal_keys_b64[0]' "${KEYS_FILE}")
KEY_2=$(jq -r '.unseal_keys_b64[1]' "${KEYS_FILE}")
KEY_3=$(jq -r '.unseal_keys_b64[2]' "${KEYS_FILE}")

vault operator unseal "${KEY_1}" >/dev/null
vault operator unseal "${KEY_2}" >/dev/null
vault operator unseal "${KEY_3}" >/dev/null

# ── Verify ───────────────────────────────────────────────────────────────────
SEALED=$(vault status -format=json 2>/dev/null | jq -r '.sealed')
if [ "${SEALED}" = "false" ]; then
    log "Vault unsealed successfully."
else
    log "ERROR: Vault is still sealed after unseal attempt."
    exit 1
fi
