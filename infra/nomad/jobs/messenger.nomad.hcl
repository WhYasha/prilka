# ============================================================
# Nomad Job Specification — Messenger Platform
# Deploys the full messenger stack via Nomad.
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

    network {
      mode = "bridge"
      port "postgres" { static = 5432 }
      port "redis"    { static = 6379 }
      port "minio"    { static = 9000 }
      port "minio_console" { static = 9001 }
    }

    # ── PostgreSQL ──────────────────────────────────────────────────────────
    task "postgres" {
      driver = "docker"

      config {
        image = "postgres:16-alpine"
        ports = ["postgres"]
        volumes = [
          "postgres_data:/var/lib/postgresql/data",
        ]
      }

      vault {
        policies = ["messenger"]
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

      resources {
        cpu    = 500
        memory = 512
      }

      service {
        name = "postgres"
        port = "postgres"
        check {
          type     = "tcp"
          interval = "10s"
          timeout  = "5s"
        }
      }
    }

    # ── Redis ───────────────────────────────────────────────────────────────
    task "redis" {
      driver = "docker"

      vault {
        policies = ["messenger"]
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
        image = "redis:7-alpine"
        ports = ["redis"]
        args = [
          "redis-server",
          "--requirepass", "${REDIS_PASSWORD}",
          "--appendonly", "yes",
        ]
        volumes = [
          "redis_data:/data",
        ]
      }

      resources {
        cpu    = 200
        memory = 256
      }

      service {
        name = "redis"
        port = "redis"
        check {
          type     = "tcp"
          interval = "10s"
          timeout  = "5s"
        }
      }
    }

    # ── MinIO ───────────────────────────────────────────────────────────────
    task "minio" {
      driver = "docker"

      vault {
        policies = ["messenger"]
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
        image = "minio/minio:latest"
        ports = ["minio", "minio_console"]
        args  = ["server", "/data", "--console-address", ":9001"]
        volumes = [
          "minio_data:/data",
        ]
      }

      resources {
        cpu    = 300
        memory = 512
      }

      service {
        name = "minio"
        port = "minio"
        check {
          type     = "http"
          path     = "/minio/health/live"
          interval = "15s"
          timeout  = "10s"
        }
      }

      service {
        name = "minio-console"
        port = "minio_console"
      }
    }
  }

  # ========================================================================
  # Task Group: Migrations (flyway — one-shot)
  # ========================================================================
  group "migrations" {
    count = 1

    # Run once and stop
    restart {
      attempts = 3
      delay    = "15s"
      mode     = "fail"
    }

    task "flyway" {
      driver = "docker"

      lifecycle {
        hook    = "prestart"
        sidecar = false
      }

      vault {
        policies = ["messenger"]
      }

      template {
        data = <<-EOF
        {{- with secret "secret/data/messenger/db" }}
        FLYWAY_URL=jdbc:postgresql://{{ .Data.data.host }}:{{ .Data.data.port }}/{{ .Data.data.name }}
        FLYWAY_USER={{ .Data.data.user }}
        FLYWAY_PASSWORD={{ .Data.data.password }}
        {{- end }}
        EOF
        destination = "secrets/flyway.env"
        env         = true
      }

      config {
        image = "flyway/flyway:10-alpine"
        args = [
          "-url=${FLYWAY_URL}",
          "-user=${FLYWAY_USER}",
          "-password=${FLYWAY_PASSWORD}",
          "-locations=filesystem:/flyway/sql",
          "-connectRetries=10",
          "migrate",
        ]
        volumes = [
          "/opt/messenger/repo/migrations:/flyway/sql:ro",
        ]
      }

      resources {
        cpu    = 200
        memory = 256
      }
    }
  }

  # ========================================================================
  # Task Group: Application (api_cpp)
  # ========================================================================
  group "app" {
    count = 1

    network {
      mode = "bridge"
      port "http" { static = 8080 }
    }

    task "api_cpp" {
      driver = "docker"

      vault {
        policies = ["messenger"]
      }

      # All secrets injected as env vars from Vault
      template {
        data = <<-EOF
        {{- with secret "secret/data/messenger/db" }}
        DB_HOST={{ .Data.data.host }}
        DB_PORT={{ .Data.data.port }}
        DB_NAME={{ .Data.data.name }}
        DB_USER={{ .Data.data.user }}
        DB_PASS={{ .Data.data.password }}
        {{- end }}
        {{- with secret "secret/data/messenger/redis" }}
        REDIS_HOST={{ .Data.data.host }}
        REDIS_PORT={{ .Data.data.port }}
        REDIS_PASS={{ .Data.data.password }}
        {{- end }}
        {{- with secret "secret/data/messenger/minio" }}
        MINIO_ENDPOINT={{ .Data.data.endpoint }}
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
        image = "messenger-api:latest"
        ports = ["http"]
      }

      resources {
        cpu    = 1000
        memory = 512
      }

      service {
        name = "api-cpp"
        port = "http"
        check {
          type     = "http"
          path     = "/health"
          interval = "10s"
          timeout  = "5s"
        }
      }
    }
  }

  # ========================================================================
  # Task Group: Monitoring (prometheus, grafana)
  # ========================================================================
  group "monitoring" {
    count = 1

    network {
      mode = "bridge"
      port "prometheus" { static = 9090 }
      port "grafana"    { static = 3000 }
    }

    task "prometheus" {
      driver = "docker"

      config {
        image = "prom/prometheus:v2.51.0"
        ports = ["prometheus"]
        args = [
          "--config.file=/etc/prometheus/prometheus.yml",
          "--storage.tsdb.path=/prometheus",
          "--storage.tsdb.retention.time=15d",
          "--web.enable-lifecycle",
        ]
        volumes = [
          "/opt/messenger/repo/infra/prometheus/prometheus.yml:/etc/prometheus/prometheus.yml:ro",
          "prometheus_data:/prometheus",
        ]
      }

      resources {
        cpu    = 300
        memory = 512
      }

      service {
        name = "prometheus"
        port = "prometheus"
        check {
          type     = "http"
          path     = "/-/healthy"
          interval = "15s"
          timeout  = "5s"
        }
      }
    }

    task "grafana" {
      driver = "docker"

      vault {
        policies = ["messenger"]
      }

      env {
        GF_USERS_ALLOW_SIGN_UP       = "false"
        GF_SERVER_ROOT_URL            = "https://grafana.behappy.rest/"
        GF_SERVER_SERVE_FROM_SUB_PATH = "false"
      }

      config {
        image = "grafana/grafana:10.4.0"
        ports = ["grafana"]
        volumes = [
          "grafana_data:/var/lib/grafana",
          "/opt/messenger/repo/infra/grafana/provisioning:/etc/grafana/provisioning:ro",
        ]
      }

      resources {
        cpu    = 200
        memory = 256
      }

      service {
        name = "grafana"
        port = "grafana"
        check {
          type     = "http"
          path     = "/api/health"
          interval = "15s"
          timeout  = "5s"
        }
      }
    }
  }

  # ========================================================================
  # Task Group: Gateway (APISIX)
  # ========================================================================
  group "gateway" {
    count = 1

    network {
      mode = "bridge"
      port "http"  { static = 80  to = 9080 }
      port "https" { static = 443 to = 9443 }
    }

    volume "letsencrypt" {
      type   = "host"
      source = "letsencrypt"
    }

    volume "downloads" {
      type   = "host"
      source = "downloads"
    }

    task "apisix" {
      driver = "docker"

      config {
        image = "apache/apisix:3.15.0-debian"
        ports = ["http", "https"]
        volumes = [
          "/opt/messenger/repo/infra/apisix/config.yaml:/usr/local/apisix/conf/config.yaml:ro",
          "/opt/messenger/repo/infra/apisix/apisix.yaml:/usr/local/apisix/conf/apisix.yaml:ro",
        ]
      }

      volume_mount {
        volume      = "letsencrypt"
        destination = "/etc/letsencrypt"
        read_only   = true
      }

      volume_mount {
        volume      = "downloads"
        destination = "/usr/share/nginx/html/downloads"
        read_only   = true
      }

      resources {
        cpu    = 500
        memory = 256
      }

      service {
        name = "apisix"
        port = "https"
        check {
          type     = "http"
          path     = "/health"
          port     = "http"
          interval = "10s"
          timeout  = "5s"
        }
      }
    }
  }
}
