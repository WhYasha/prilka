# ============================================================
# HashiCorp Vault – Server Configuration
# Single-server deployment with file storage backend.
# TLS is handled by APISIX — Vault listens plain HTTP internally.
# ============================================================

storage "file" {
  path = "/vault/data"
}

listener "tcp" {
  address     = "0.0.0.0:8200"
  tls_disable = 1
}

api_addr = "http://vault:8200"
ui       = true

# Disable mlock for Docker (IPC_LOCK cap_add handles it)
disable_mlock = true

# Telemetry (optional – Prometheus scraping)
telemetry {
  prometheus_retention_time = "60s"
  disable_hostname          = true
}
