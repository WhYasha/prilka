# ============================================================
# HashiCorp Nomad – Server + Client Configuration
# Single-node deployment: runs as host-native process.
# Vault and Consul run as standalone Docker containers
# on messenger_net, reachable via localhost mapped ports.
# ============================================================

data_dir = "/opt/messenger/nomad/data"

# Single-node: bind to all interfaces but advertise on localhost.
# The advertise block prevents "unsafe localhost" errors.
bind_addr = "127.0.0.1"

advertise {
  http = "127.0.0.1"
  rpc  = "127.0.0.1"
  serf = "127.0.0.1"
}

# ── Server (leader election — single node bootstrap) ──────────────────────────
server {
  enabled          = true
  bootstrap_expect = 1
}

# ── Client (runs workloads on same node) ──────────────────────────────────────
client {
  enabled = true

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

  host_volume "migrations" {
    path      = "/opt/messenger/repo/migrations"
    read_only = true
  }

  host_volume "stickers" {
    path      = "/opt/messenger/repo/infra/minio/stickers"
    read_only = true
  }

  host_volume "prometheus_config" {
    path      = "/opt/messenger/repo/infra/prometheus"
    read_only = true
  }

  host_volume "grafana_provisioning" {
    path      = "/opt/messenger/repo/infra/grafana/provisioning"
    read_only = true
  }

  host_volume "apisix_config" {
    path      = "/opt/messenger/repo/infra/apisix"
    read_only = true
  }
}

# ── Consul integration (service registration + discovery) ─────────────────────
consul {
  address = "127.0.0.1:8500"
}

# ── Vault integration (secret injection into tasks) ──────────────────────────
vault {
  enabled          = true
  address          = "http://127.0.0.1:8200"
  create_from_role = "nomad-cluster"
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
