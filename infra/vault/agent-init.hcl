# ============================================================
# Vault Agent — One-shot mode for deploy-time .env rendering
# Authenticates, renders compose-env.tpl → .env, then exits.
# Usage: vault agent -config=infra/vault/agent-init.hcl
# ============================================================

pid_file = "/tmp/vault-agent-init.pid"

vault {
  address = "http://127.0.0.1:8200"
}

# Exit after obtaining token and rendering all templates once
exit_after_auth = true

# ── Auto-auth via AppRole ────────────────────────────────────────────────────
auto_auth {
  method "approle" {
    config = {
      role_id_file_path   = "/opt/messenger/vault/role-id"
      secret_id_file_path = "/opt/messenger/vault/secret-id"
      remove_secret_id_file_after_reading = false
    }
  }

  sink "file" {
    config = {
      path = "/tmp/vault-token"
    }
  }
}

# ── Template: render .env for Docker Compose ─────────────────────────────────
template {
  source      = "/opt/messenger/repo/infra/vault/compose-env.tpl"
  destination = "/opt/messenger/env/.env"
  perms       = 0600

  error_on_missing_key = true
}
