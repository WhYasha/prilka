# ============================================================
# Nomad Job Specification — Messenger Platform
# Manages application services. Gateway/infra services
# (Vault, Consul, APISIX) run as standalone systemd units.
#
# Prerequisites:
#   - Docker network "messenger_net" exists
#   - Vault unsealed + secrets populated
#   - Consul running with service registrations
#   - APISIX running (systemd)
#   - messenger-api:latest image built locally
# ============================================================

job "messenger" {
  datacenters = ["dc1"]
  type        = "service"

  # ── Update strategy ────────────────────────────────────────────────────────
  update {
    max_parallel     = 1
    min_healthy_time = "10s"
    healthy_deadline = "5m"
    auto_revert      = true
  }

  # ========================================================================
  # Task Group: Infrastructure (postgres, redis, minio)
  # ========================================================================
  group "infra" {
    count = 1

    restart {
      attempts = 5
      delay    = "15s"
      interval = "5m"
      mode     = "delay"
    }

    # ── PostgreSQL ──────────────────────────────────────────────────────────
    task "postgres" {
      driver = "docker"

      vault {
        role = "nomad-workloads"
      }

      template {
        data = <<-EOF
        {{- with secret "secret/data/messenger/db" }}
        POSTGRES_DB={{ .Data.data.name }}
        POSTGRES_USER={{ .Data.data.user }}
        POSTGRES_PASSWORD={{ .Data.data.password }}
        {{- end }}
        EOF
        destination = "secrets/db.env"
        env         = true
      }

      config {
        image        = "postgres:16-alpine"
        hostname     = "postgres"
        network_mode = "messenger_net"
        volumes = [
          "messenger-postgres-data:/var/lib/postgresql/data",
        ]
      }

      resources {
        cpu    = 500
        memory = 512
      }

      service {
        name         = "postgres"
        port         = "5432"
        address_mode = "driver"
        check {
          type         = "tcp"
          port         = "5432"
          address_mode = "driver"
          interval     = "10s"
          timeout      = "5s"
        }
      }
    }

    # ── Redis ───────────────────────────────────────────────────────────────
    task "redis" {
      driver = "docker"

      vault {
        role = "nomad-workloads"
      }

      template {
        data = <<-EOF
        {{- with secret "secret/data/messenger/redis" }}
        REDIS_PASSWORD={{ .Data.data.password }}
        {{- end }}
        EOF
        destination = "secrets/redis.env"
        env         = true
      }

      config {
        image        = "redis:7-alpine"
        hostname     = "redis"
        network_mode = "messenger_net"
        args = [
          "redis-server",
          "--requirepass", "${REDIS_PASSWORD}",
          "--appendonly", "yes",
        ]
        volumes = [
          "messenger-redis-data:/data",
        ]
      }

      resources {
        cpu    = 200
        memory = 256
      }

      service {
        name         = "redis"
        port         = "6379"
        address_mode = "driver"
        check {
          type         = "tcp"
          port         = "6379"
          address_mode = "driver"
          interval     = "10s"
          timeout      = "5s"
        }
      }
    }

    # ── MinIO ───────────────────────────────────────────────────────────────
    task "minio" {
      driver = "docker"

      vault {
        role = "nomad-workloads"
      }

      template {
        data = <<-EOF
        {{- with secret "secret/data/messenger/minio" }}
        MINIO_ROOT_USER={{ .Data.data.access_key }}
        MINIO_ROOT_PASSWORD={{ .Data.data.secret_key }}
        {{- end }}
        MINIO_BROWSER_REDIRECT_URL=https://minio-console.behappy.rest/
        EOF
        destination = "secrets/minio.env"
        env         = true
      }

      config {
        image        = "minio/minio:latest"
        hostname     = "minio"
        network_mode = "messenger_net"
        args         = ["server", "/data", "--console-address", ":9001"]
        volumes = [
          "messenger-minio-data:/data",
        ]
      }

      resources {
        cpu    = 300
        memory = 512
      }

      service {
        name         = "minio"
        port         = "9000"
        address_mode = "driver"
        check {
          type         = "http"
          path         = "/minio/health/live"
          port         = "9000"
          address_mode = "driver"
          interval     = "15s"
          timeout      = "10s"
        }
      }

      service {
        name         = "minio-console"
        port         = "9001"
        address_mode = "driver"
      }
    }
  }

  # ========================================================================
  # Task Group: Application (api_cpp + prestart init tasks)
  # minio-init and flyway run as prestart lifecycle hooks before api_cpp.
  # ========================================================================
  group "app" {
    count = 1

    restart {
      attempts = 5
      delay    = "15s"
      interval = "5m"
      mode     = "delay"
    }

    volume "migrations" {
      type      = "host"
      source    = "migrations"
      read_only = true
    }

    volume "stickers" {
      type      = "host"
      source    = "stickers"
      read_only = true
    }

    # ── MinIO bucket init (prestart) ────────────────────────────────────────
    task "minio-init" {
      driver = "docker"

      lifecycle {
        hook    = "prestart"
        sidecar = false
      }

      vault {
        role = "nomad-workloads"
      }

      volume_mount {
        volume      = "stickers"
        destination = "/stickers"
        read_only   = true
      }

      template {
        data = <<-EOF
        {{- with secret "secret/data/messenger/minio" }}
        MC_HOST_local=http://{{ .Data.data.access_key }}:{{ .Data.data.secret_key }}@minio:9000
        {{- end }}
        EOF
        destination = "secrets/minio.env"
        env         = true
      }

      config {
        image        = "minio/mc:latest"
        hostname     = "minio-init"
        network_mode = "messenger_net"
        entrypoint   = ["/bin/sh", "-c"]
        args = [
          "sleep 10 && mc mb --ignore-existing local/bh-avatars && mc mb --ignore-existing local/bh-uploads && mc mb --ignore-existing local/bh-stickers && mc mb --ignore-existing local/bh-test-artifacts && mc anonymous set none local/bh-avatars && mc anonymous set none local/bh-uploads && mc anonymous set none local/bh-stickers && mc anonymous set none local/bh-test-artifacts && mc cp /stickers/s01.svg local/bh-stickers/stickers/s01.svg && mc cp /stickers/s02.svg local/bh-stickers/stickers/s02.svg && mc cp /stickers/s03.svg local/bh-stickers/stickers/s03.svg && mc cp /stickers/s04.svg local/bh-stickers/stickers/s04.svg && mc cp /stickers/s05.svg local/bh-stickers/stickers/s05.svg && mc cp /stickers/s06.svg local/bh-stickers/stickers/s06.svg && mc cp /stickers/s07.svg local/bh-stickers/stickers/s07.svg && mc cp /stickers/s08.svg local/bh-stickers/stickers/s08.svg && echo 'MinIO buckets and stickers ready.'",
        ]
      }

      resources {
        cpu    = 100
        memory = 128
      }
    }

    # ── Flyway migrations (prestart) ────────────────────────────────────────
    task "flyway" {
      driver = "docker"

      lifecycle {
        hook    = "prestart"
        sidecar = false
      }

      vault {
        role = "nomad-workloads"
      }

      template {
        data = <<-EOF
        {{- with secret "secret/data/messenger/db" }}
        FLYWAY_URL=jdbc:postgresql://postgres:5432/{{ .Data.data.name }}
        FLYWAY_USER={{ .Data.data.user }}
        FLYWAY_PASSWORD={{ .Data.data.password }}
        {{- end }}
        EOF
        destination = "secrets/flyway.env"
        env         = true
      }

      volume_mount {
        volume      = "migrations"
        destination = "/flyway/sql"
        read_only   = true
      }

      config {
        image        = "flyway/flyway:10-alpine"
        hostname     = "flyway"
        network_mode = "messenger_net"
        args = [
          "-url=${FLYWAY_URL}",
          "-user=${FLYWAY_USER}",
          "-password=${FLYWAY_PASSWORD}",
          "-locations=filesystem:/flyway/sql",
          "-connectRetries=10",
          "migrate",
        ]
      }

      resources {
        cpu    = 200
        memory = 256
      }
    }

    # ── API Server (main task) ──────────────────────────────────────────────
    task "api_cpp" {
      driver = "docker"

      vault {
        role = "nomad-workloads"
      }

      template {
        data = <<-EOF
        {{- with secret "secret/data/messenger/db" }}
        DB_HOST=postgres
        DB_PORT=5432
        DB_NAME={{ .Data.data.name }}
        DB_USER={{ .Data.data.user }}
        DB_PASS={{ .Data.data.password }}
        {{- end }}
        {{- with secret "secret/data/messenger/redis" }}
        REDIS_HOST=redis
        REDIS_PORT=6379
        REDIS_PASS={{ .Data.data.password }}
        {{- end }}
        {{- with secret "secret/data/messenger/minio" }}
        MINIO_ENDPOINT=minio:9000
        MINIO_ACCESS_KEY={{ .Data.data.access_key }}
        MINIO_SECRET_KEY={{ .Data.data.secret_key }}
        MINIO_PUBLIC_URL={{ .Data.data.public_url }}
        MINIO_AVATARS_BUCKET={{ .Data.data.avatars_bucket }}
        MINIO_UPLOADS_BUCKET={{ .Data.data.uploads_bucket }}
        MINIO_STICKERS_BUCKET={{ .Data.data.stickers_bucket }}
        MINIO_PRESIGN_TTL={{ .Data.data.presign_ttl }}
        {{- end }}
        {{- with secret "secret/data/messenger/jwt" }}
        JWT_SECRET={{ .Data.data.secret }}
        JWT_ACCESS_TTL={{ .Data.data.access_ttl }}
        JWT_REFRESH_TTL={{ .Data.data.refresh_ttl }}
        {{- end }}
        {{- with secret "secret/data/messenger/server" }}
        API_PORT={{ .Data.data.port }}
        API_THREADS={{ .Data.data.threads }}
        MAX_FILE_SIZE_MB={{ .Data.data.max_file_size_mb }}
        {{- end }}
        EOF
        destination = "secrets/app.env"
        env         = true
      }

      config {
        image        = "messenger-api:local"
        hostname     = "api_cpp"
        network_mode = "messenger_net"
      }

      resources {
        cpu    = 1000
        memory = 512
      }

      service {
        name         = "api-cpp"
        port         = "8080"
        address_mode = "driver"
        check {
          type         = "http"
          path         = "/health"
          port         = "8080"
          address_mode = "driver"
          interval     = "10s"
          timeout      = "5s"
        }
      }
    }
  }

  # ========================================================================
  # Task Group: Monitoring (prometheus, grafana, cadvisor)
  # ========================================================================
  group "monitoring" {
    count = 1

    restart {
      attempts = 3
      delay    = "15s"
      interval = "5m"
      mode     = "delay"
    }

    volume "prometheus_config" {
      type      = "host"
      source    = "prometheus_config"
      read_only = true
    }

    volume "grafana_provisioning" {
      type      = "host"
      source    = "grafana_provisioning"
      read_only = true
    }

    task "prometheus" {
      driver = "docker"
      user   = "root"

      volume_mount {
        volume      = "prometheus_config"
        destination = "/etc/prometheus/host"
        read_only   = true
      }

      config {
        image        = "prom/prometheus:v2.51.0"
        hostname     = "prometheus"
        network_mode = "messenger_net"
        args = [
          "--config.file=/etc/prometheus/host/prometheus.yml",
          "--storage.tsdb.path=/prometheus",
          "--storage.tsdb.retention.time=15d",
          "--web.enable-lifecycle",
        ]
        volumes = [
          "messenger-prometheus-data:/prometheus",
        ]
      }

      resources {
        cpu    = 300
        memory = 512
      }

      service {
        name         = "prometheus"
        port         = "9090"
        address_mode = "driver"
        check {
          type         = "http"
          path         = "/-/healthy"
          port         = "9090"
          address_mode = "driver"
          interval     = "15s"
          timeout      = "5s"
        }
      }
    }

    task "grafana" {
      driver = "docker"
      user   = "root"

      vault {
        role = "nomad-workloads"
      }

      template {
        data = <<-EOF
        {{- with secret "secret/data/messenger/grafana" }}
        GF_SECURITY_ADMIN_USER={{ .Data.data.user }}
        GF_SECURITY_ADMIN_PASSWORD={{ .Data.data.password }}
        {{- end }}
        GF_USERS_ALLOW_SIGN_UP=false
        GF_SERVER_ROOT_URL=https://grafana.behappy.rest/
        GF_SERVER_SERVE_FROM_SUB_PATH=false
        GF_INSTALL_PLUGINS=
        EOF
        destination = "secrets/grafana.env"
        env         = true
      }

      volume_mount {
        volume      = "grafana_provisioning"
        destination = "/etc/grafana/provisioning"
        read_only   = true
      }

      config {
        image        = "grafana/grafana:10.4.0"
        hostname     = "grafana"
        network_mode = "messenger_net"
        volumes = [
          "messenger-grafana-data:/var/lib/grafana",
        ]
      }

      resources {
        cpu    = 200
        memory = 256
      }

      service {
        name         = "grafana"
        port         = "3000"
        address_mode = "driver"
        check {
          type         = "http"
          path         = "/api/health"
          port         = "3000"
          address_mode = "driver"
          interval     = "15s"
          timeout      = "5s"
        }
      }
    }

    task "cadvisor" {
      driver = "docker"

      config {
        image        = "gcr.io/cadvisor/cadvisor:v0.49.1"
        hostname     = "cadvisor"
        network_mode = "messenger_net"
        privileged   = true
        volumes = [
          "/:/rootfs:ro",
          "/var/run:/var/run:rw",
          "/sys:/sys:ro",
          "/var/lib/docker:/var/lib/docker:ro",
        ]
        args = [
          "--housekeeping_interval=15s",
          "--docker_only=true",
        ]
      }

      resources {
        cpu    = 200
        memory = 256
      }

      service {
        name         = "cadvisor"
        port         = "8080"
        address_mode = "driver"
      }
    }
  }
}
