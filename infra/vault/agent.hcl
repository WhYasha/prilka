# ============================================================
# Vault Agent – Template-based secret injection for api_cpp
# Renders a .env file from Vault KV secrets before starting the app.
# ============================================================

pid_file = "/tmp/vault-agent.pid"

vault {
  address = "http://vault:8200"
}

# ── Auto-auth via AppRole ────────────────────────────────────────────────────
auto_auth {
  method "approle" {
    config = {
      role_id_file_path   = "/vault/approle/role-id"
      secret_id_file_path = "/vault/approle/secret-id"
      remove_secret_id_file_after_reading = false
    }
  }

  sink "file" {
    config = {
      path = "/tmp/vault-token"
    }
  }
}

# ── Template: render .env from Vault secrets ─────────────────────────────────
template {
  source      = "/vault/templates/env.tpl"
  destination = "/app/.env"
  perms       = 0600

  # Wait for all secrets to be available before rendering
  error_on_missing_key = true
}

# ── Exec: start the application after template renders ────────────────────────
# Vault Agent will start this process and forward signals to it.
# When template changes (secret rotation), it re-renders and sends SIGHUP.
exec {
  command = ["/app/messenger_api"]
  restart_on_secret_changes = "always"
  restart_stop_signal       = "SIGTERM"
}
