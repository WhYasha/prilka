# ============================================================
# Vault Policy – messenger application
# Grants read-only access to all messenger secrets.
# Used by the api_cpp AppRole for Vault Agent templates.
# ============================================================

# Read messenger secrets (KV v2 — data/ prefix required)
path "secret/data/messenger/*" {
  capabilities = ["read"]
}

# List secret keys (for debugging / operational visibility)
path "secret/metadata/messenger/*" {
  capabilities = ["read", "list"]
}
