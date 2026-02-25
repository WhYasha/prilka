# ============================================================
# HashiCorp Nomad – Server + Client Configuration
# Single-node deployment (dev/single-agent mode).
# Integrates with Consul for service discovery and Vault for secrets.
# ============================================================

data_dir = "/nomad/data"

# ── Server (leader election — single node bootstrap) ──────────────────────────
server {
  enabled          = true
  bootstrap_expect = 1
}

# ── Client (runs workloads on same node) ──────────────────────────────────────
client {
  enabled = true

  # Docker driver configuration
  host_volume "downloads" {
    path      = "/opt/messenger/downloads"
    read_only = true
  }

  host_volume "letsencrypt" {
    path      = "/etc/letsencrypt"
    read_only = true
  }

  host_volume "certbot_webroot" {
    path      = "/opt/messenger/certbot-webroot"
    read_only = true
  }
}

# ── Consul integration (service registration + discovery) ─────────────────────
consul {
  address = "consul:8500"
}

# ── Vault integration (secret injection into tasks) ──────────────────────────
vault {
  enabled = true
  address = "http://vault:8200"
}

# ── Telemetry ──────────────────────────────────────────────────────────────────
telemetry {
  publish_allocation_metrics = true
  publish_node_metrics       = true
  prometheus_metrics         = true
}

# ── Plugin: Docker driver ─────────────────────────────────────────────────────
plugin "docker" {
  config {
    allow_privileged = true
    volumes {
      enabled = true
    }
  }
}
