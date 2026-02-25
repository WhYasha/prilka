# ============================================================
# APISIX – Declarative Routes & Upstreams (Standalone mode)
# Subdomain-based routing for behappy.rest
# ============================================================
# Reload: APISIX watches this file and hot-reloads automatically.
# Manual reload: curl http://127.0.0.1:9092/v1/routes

# ── SSL Certificates ────────────────────────────────────────────────────────
# SSL is configured in config.yaml using file paths to Let's Encrypt certs.
# When upgrading to a wildcard cert (*.behappy.rest), update config.yaml paths.
# For SNI-based multi-cert setups, embed PEM content here under ssl: section.

# ── Upstreams ────────────────────────────────────────────────────────────────
upstreams:
  - id: 1
    name: api_cpp
    type: roundrobin
    nodes:
      "api_cpp:8080": 1
    timeout:
      connect: 5
      send: 30
      read: 30
    checks:
      active:
        type: http
        http_path: /health
        healthy:
          interval: 10
          successes: 2
        unhealthy:
          interval: 5
          http_failures: 3

  - id: 2
    name: grafana
    type: roundrobin
    nodes:
      "grafana:3000": 1

  - id: 3
    name: minio_s3
    type: roundrobin
    pass_host: rewrite
    upstream_host: "minio:9000"
    nodes:
      "minio:9000": 1

  - id: 4
    name: minio_console
    type: roundrobin
    nodes:
      "minio:9001": 1

# ── Global rules (applied to all routes) ─────────────────────────────────────
global_rules:
  - id: 1
    plugins:
      real-ip:
        source: remote_addr
      redirect:
        http_to_https: true

routes:
  # ========================================================================
  # Health check — no host restriction (used by Docker healthcheck + LB)
  # ========================================================================
  - id: 10
    name: health-check
    uri: /health
    priority: 200
    upstream_id: 1

  # ========================================================================
  # behappy.rest — MinIO S3 presigned URLs (path-based, stays unchanged)
  # ========================================================================
  - id: 20
    name: minio-s3-presigned
    uri: /minio/*
    host: behappy.rest
    priority: 90
    methods:
      - GET
      - HEAD
    upstream_id: 3
    plugins:
      proxy-rewrite:
        regex_uri:
          - "^/minio/(.*)"
          - "/$1"
      response-rewrite:
        headers:
          set:
            Strict-Transport-Security: "max-age=63072000; includeSubDomains; preload"
            X-Content-Type-Options: nosniff
            X-Frame-Options: SAMEORIGIN

  # ========================================================================
  # 301 redirects — old paths → subdomain URLs
  # ========================================================================
  - id: 30
    name: redirect-api
    uri: /api/*
    host: behappy.rest
    priority: 80
    plugins:
      redirect:
        regex_uri:
          - "^/api/(.*)"
          - "https://api.behappy.rest/$1"
        ret_code: 301

  - id: 31
    name: redirect-api-bare
    uri: /api
    host: behappy.rest
    priority: 80
    plugins:
      redirect:
        uri: "https://api.behappy.rest/"
        ret_code: 301

  - id: 32
    name: redirect-ws
    uri: /ws*
    host: behappy.rest
    priority: 80
    plugins:
      redirect:
        uri: "https://ws.behappy.rest/"
        ret_code: 301

  - id: 33
    name: redirect-grafana
    uri: /grafana/*
    host: behappy.rest
    priority: 80
    plugins:
      redirect:
        regex_uri:
          - "^/grafana/(.*)"
          - "https://grafana.behappy.rest/$1"
        ret_code: 301

  - id: 34
    name: redirect-grafana-bare
    uri: /grafana
    host: behappy.rest
    priority: 80
    plugins:
      redirect:
        uri: "https://grafana.behappy.rest/"
        ret_code: 301

  - id: 35
    name: redirect-minio-console
    uri: /minio-console/*
    host: behappy.rest
    priority: 80
    plugins:
      redirect:
        regex_uri:
          - "^/minio-console/(.*)"
          - "https://minio-console.behappy.rest/$1"
        ret_code: 301

  - id: 36
    name: redirect-minio-console-bare
    uri: /minio-console
    host: behappy.rest
    priority: 80
    plugins:
      redirect:
        uri: "https://minio-console.behappy.rest/"
        ret_code: 301

  - id: 37
    name: redirect-downloads
    uri: /downloads/*
    host: behappy.rest
    priority: 80
    plugins:
      redirect:
        regex_uri:
          - "^/downloads/(.*)"
          - "https://downloads.behappy.rest/$1"
        ret_code: 301

  - id: 38
    name: redirect-downloads-bare
    uri: /downloads
    host: behappy.rest
    priority: 80
    plugins:
      redirect:
        uri: "https://downloads.behappy.rest/"
        ret_code: 301

  # ========================================================================
  # api.behappy.rest — REST API (C++ Drogon)
  # ========================================================================
  - id: 50
    name: api-rest
    uri: /*
    host: api.behappy.rest
    priority: 50
    upstream_id: 1
    plugins:
      cors:
        allow_origins: "https://behappy.rest"
        allow_methods: "GET, POST, PUT, PATCH, DELETE, OPTIONS"
        allow_headers: "Content-Type, Authorization, X-Requested-With, Accept"
        expose_headers: "Content-Disposition"
        max_age: 86400
        allow_credential: true
      limit-req:
        rate: 30
        burst: 60
        key: remote_addr
        rejected_code: 429
      response-rewrite:
        headers:
          set:
            Strict-Transport-Security: "max-age=63072000; includeSubDomains; preload"
            X-Content-Type-Options: nosniff
            X-Frame-Options: SAMEORIGIN

  # ========================================================================
  # ws.behappy.rest — WebSocket (C++ Drogon)
  # ========================================================================
  - id: 60
    name: websocket
    uri: /*
    host: ws.behappy.rest
    priority: 50
    upstream_id: 1
    plugins:
      proxy-rewrite:
        uri: /ws
      serverless-pre-function:
        phase: access
        functions:
          - "return function(conf, ctx) if ngx.var.http_upgrade and ngx.var.http_upgrade ~= '' then ngx.var.upstream_upgrade = ngx.var.http_upgrade; ngx.var.upstream_connection = 'Upgrade' end end"
      response-rewrite:
        headers:
          set:
            Strict-Transport-Security: "max-age=63072000; includeSubDomains; preload"
            X-Content-Type-Options: nosniff
    timeout:
      connect: 5
      send: 3600
      read: 3600

  # ========================================================================
  # grafana.behappy.rest — Grafana dashboard
  # ========================================================================
  - id: 70
    name: grafana
    uri: /*
    host: grafana.behappy.rest
    priority: 50
    upstream_id: 2
    plugins:
      response-rewrite:
        headers:
          set:
            Strict-Transport-Security: "max-age=63072000; includeSubDomains; preload"
            X-Content-Type-Options: nosniff
            X-Frame-Options: SAMEORIGIN

  # ========================================================================
  # minio-console.behappy.rest — MinIO Console
  # ========================================================================
  - id: 80
    name: minio-console
    uri: /*
    host: minio-console.behappy.rest
    priority: 50
    upstream_id: 4
    plugins:
      serverless-pre-function:
        phase: access
        functions:
          - "return function(conf, ctx) if ngx.var.http_upgrade and ngx.var.http_upgrade ~= '' then ngx.var.upstream_upgrade = ngx.var.http_upgrade; ngx.var.upstream_connection = 'Upgrade' end end"
      response-rewrite:
        headers:
          set:
            Strict-Transport-Security: "max-age=63072000; includeSubDomains; preload"
            X-Content-Type-Options: nosniff
            X-Frame-Options: SAMEORIGIN

  # ========================================================================
  # downloads.behappy.rest — Static downloads (served through Drogon)
  # ========================================================================
  - id: 90
    name: downloads
    uri: /*
    host: downloads.behappy.rest
    priority: 50
    upstream_id: 1
    plugins:
      proxy-rewrite:
        regex_uri:
          - "^/(.*)"
          - "/downloads/$1"
      response-rewrite:
        headers:
          set:
            Strict-Transport-Security: "max-age=63072000; includeSubDomains; preload"
            X-Content-Type-Options: nosniff
            Cache-Control: "public, max-age=86400"

  # ========================================================================
  # admin.behappy.rest — Admin panel (part of main SPA)
  # Redirects bare / to /admin, proxies everything else to Drogon
  # ========================================================================
  - id: 95
    name: admin-redirect-root
    uri: /
    host: admin.behappy.rest
    priority: 60
    plugins:
      redirect:
        uri: "/admin"
        ret_code: 302

  - id: 96
    name: admin-spa
    uri: /*
    host: admin.behappy.rest
    priority: 50
    upstream_id: 1
    plugins:
      response-rewrite:
        headers:
          set:
            Strict-Transport-Security: "max-age=63072000; includeSubDomains; preload"
            X-Content-Type-Options: nosniff
            X-Frame-Options: SAMEORIGIN

  # ========================================================================
  # behappy.rest — SPA catch-all (C++ Drogon serves Vue app)
  # Must be LAST (lowest priority) for behappy.rest
  # ========================================================================
  - id: 100
    name: spa-catchall
    uri: /*
    host: behappy.rest
    priority: 1
    upstream_id: 1
    plugins:
      response-rewrite:
        headers:
          set:
            Strict-Transport-Security: "max-age=63072000; includeSubDomains; preload"
            X-Content-Type-Options: nosniff
            X-Frame-Options: SAMEORIGIN

#END
