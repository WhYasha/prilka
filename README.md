# Messenger Platform

A production-grade messenger with a **C++ Drogon backend**, **Vue 3 + TypeScript SPA**,
**Tauri desktop app**, PostgreSQL, Redis, MinIO — orchestrated by **Nomad** with
**Vault** secrets, **Consul** service discovery, and **APISIX** reverse proxy.

```
                  Browser / Desktop (Tauri)
                         │
                ┌────────┴────────┐
                │ HTTPS + WSS     │
                ▼                 │
     ┌──────────────────────────────────────────────────────────┐
     │             APISIX (TLS termination, reverse proxy)      │
     │   behappy.rest      → Drogon SPA                         │
     │   api.behappy.rest  → api_cpp:8080 (REST, rate-limited)  │
     │   ws.behappy.rest   → api_cpp:8080 (WebSocket)           │
     │   grafana / minio-console / downloads → subdomains       │
     └──────────────────────┬───────────────────────────────────┘
                            │
                            ▼
     ┌──────────────────────────────────────────────────────────┐
     │              api_cpp (Drogon C++ :8080)                  │
     │  REST API + WebSocket + SPA host (backend/www/)          │
     └───────┬──────────────┬──────────────┬────────────────────┘
             │              │              │
             ▼              ▼              ▼
     ┌──────────────┐ ┌──────────┐ ┌──────────────────┐
     │ PostgreSQL 16│ │ Redis 7  │ │ MinIO (S3)       │
     │  20 tables   │ │ pub/sub  │ │ 4 buckets        │
     │  partitioned │ │ presence │ │ presigned URLs   │
     └──────────────┘ └──────────┘ └──────────────────┘

     ┌──────────────────────────────────────────────────────────┐
     │  Vault (secrets) · Consul (discovery) · Nomad (orch.)   │
     │  Prometheus · Grafana · cAdvisor                         │
     └──────────────────────────────────────────────────────────┘
```

## Features

- **Real-time messaging** — WebSocket with Redis Pub/Sub fan-out
- **Direct, group, and channel chats** — with roles (owner/admin/member)
- **Telegram-like UI** — context menus, selection mode, swipe actions, bottom sheets, long press
- **Message replies, editing, pinning, forwarding** with full attribution
- **Emoji reactions** on messages
- **Online/offline/away presence** — tracks tab visibility per connection
- **Unread badges** and read tracking
- **In-chat message search**
- **Self-chat / Saved Messages**
- **File uploads** and **voice messages** (MediaRecorder)
- **Stickers** (8 seed SVGs)
- **Invite links** for channels/groups
- **User avatars** and **chat avatars**
- **Privacy settings** (last-seen visibility)
- **Admin panel** — dashboard, user management, message moderation, support
- **Browser notifications**
- **Tauri v2 desktop app** (Windows NSIS installer, macOS DMG)
- **Monitoring** — Prometheus metrics, Grafana dashboards, cAdvisor

## Production URLs

| Resource | URL |
|----------|-----|
| **Web app** | https://behappy.rest/ |
| **REST API** | https://api.behappy.rest/ |
| **WebSocket** | `wss://ws.behappy.rest/ws` |
| **Grafana** | https://grafana.behappy.rest/ |
| **MinIO Console** | https://minio-console.behappy.rest/ |
| **Downloads** | https://downloads.behappy.rest/ |
| **Vault UI** | https://vlt.behappy.rest/ |
| **Nomad UI** | https://nomad.behappy.rest/ |
| **Admin panel** | https://admin.behappy.rest/ |

---

## Quick Start (local dev)

```bash
# 1. Clone
git clone https://github.com/WhYasha/prilka.git
cd prilka

# 2. Start infrastructure (PostgreSQL, Redis, MinIO, migrations)
make up

# 3. Start frontend dev server
cd frontend && npm install && npm run dev

# 4. Check status
make ps
make info
```

`make up` automatically:
- Copies `.env.example` -> `.env` on first run
- Builds the C++ backend Docker image
- Starts PostgreSQL 16, Redis 7, MinIO
- Creates MinIO buckets and seeds stickers
- Runs Flyway migrations (V1-V20)
- Starts the C++ API, Prometheus, Grafana, cAdvisor

### Prerequisites

| Tool | Version | Why |
|------|---------|-----|
| Docker | 24+ | All services run in containers |
| Docker Compose | v2 (plugin) | Local orchestration |
| Node.js | 20+ | Frontend build (Vite + Vue 3) |
| `make` | any | Convenience commands |

### Local Service URLs

| Service | URL | Credentials |
|---------|-----|-------------|
| **Vue Dev Server** | http://localhost:5173 | — |
| **C++ API** | http://localhost:8080 | JWT tokens |
| **MinIO Console** | http://localhost:9001 | minioadmin/changeme_minio |
| **Prometheus** | http://localhost:9090 | — |
| **Grafana** | http://localhost:3000 | admin/admin |

---

## Project Structure

```
.
├── frontend/                    Vue 3 + Vite + TypeScript SPA
│   ├── src/
│   │   ├── api/                 13 typed API modules (axios)
│   │   ├── stores/              7 Pinia stores (auth, chats, messages, ...)
│   │   ├── composables/         7 composables (WebSocket, recorder, swipe, ...)
│   │   ├── views/               5 views (Login, Register, Messenger, Admin, Join)
│   │   ├── components/
│   │   │   ├── layout/          Sidebar, ChatPanel (814 lines), Drawer, AppBar
│   │   │   ├── chat/            18 chat components (MessageBubble, Composer, ...)
│   │   │   ├── modals/          7 modals (NewChat, Profile, Forward, ...)
│   │   │   ├── admin/           6 admin components (Dashboard, Users, Support, ...)
│   │   │   └── ui/              Avatar, Badge, BottomSheet, Toast, Spinner, ...
│   │   └── router/              Routes + auth/admin guards
│   ├── src-tauri/               Tauri v2 desktop app (Rust)
│   └── .env.production          VITE_API_URL + VITE_WS_URL
├── backend/                     C++ Drogon backend
│   ├── src/
│   │   ├── controllers/         14 controllers (~4500 lines total)
│   │   ├── filters/             AuthFilter (JWT), AdminFilter (is_admin claim)
│   │   ├── services/            JwtService (HS256), MetricsService (Prometheus)
│   │   ├── utils/               MinioPresign (SigV4 presigned URLs)
│   │   ├── ws/                  WsHandler (Redis pub/sub, presence, typing)
│   │   ├── config/Config.h      All env vars in one struct
│   │   └── main.cpp             App init, /health, /metrics, Redis retry
│   ├── tests/                   GTest unit tests
│   ├── Dockerfile               3-stage (build → test → runtime)
│   └── www/                     Vite build output served by Drogon
├── migrations/                  Flyway SQL V1–V20
├── infra/
│   ├── apisix/                  APISIX config + route templates
│   ├── vault/                   Vault config, policies, init/unseal scripts
│   ├── consul/                  Consul config + 6 service definitions
│   ├── nomad/                   Nomad config + messenger.nomad.hcl (3 groups)
│   ├── systemd/                 Tier 0 units (vault, consul, apisix, nomad)
│   ├── prometheus/              Dev + prod configs
│   ├── grafana/                 Datasource + dashboard provisioning
│   ├── scripts/                 Bootstrap, deploy, smoke/feature/presence tests
│   └── nginx/                   DEPRECATED (kept for rollback reference)
├── docs/
│   ├── architecture.md          Full system design + scaling strategy
│   ├── DEPLOY.md                Production deployment runbook
│   ├── env-vars.md              Environment variable reference
│   └── mobile.md                iOS/Swift API contract
├── docker-compose.yml           Dev stack
├── docker-compose.prod.yml      Production overrides (Docker Compose fallback)
├── .env.example                 Environment template
├── Makefile                     Convenience commands
└── .github/workflows/
    ├── ci.yml                   Build (Linux/Win/macOS) + smoke test
    └── desktop-build.yml        Tauri NSIS/DMG build + publish
```

---

## Production Infrastructure

### Tiered Architecture

```
Tier 0 (systemd):  Vault → Consul → APISIX → Nomad
Tier 1 (Nomad):    PostgreSQL, Redis, MinIO, api_cpp, Prometheus, Grafana, cAdvisor
```

### Tier 0 — systemd Services

| Service | Container | Purpose |
|---------|-----------|---------|
| **Vault** 1.15 | `messenger-vault` | Secrets management (KV v2, AppRole + JWT auth) |
| **Consul** 1.17 | `messenger-consul` | Service discovery + health checks |
| **APISIX** 3.15 | `messenger-apisix` | TLS termination, reverse proxy (standalone YAML) |
| **Nomad** | host-native | Container orchestration |

### Tier 1 — Nomad Job `messenger` (3 task groups)

| Group | Tasks | Notes |
|-------|-------|-------|
| **infra** | PostgreSQL 16, Redis 7, MinIO | Stateful, Docker volumes |
| **app** | minio-init (prestart), Flyway (prestart), api_cpp (main) | Init tasks run before API |
| **monitoring** | Prometheus 2.51, Grafana 10.4, cAdvisor 0.49 | Observability |

### Vault Secrets

All credentials stored in `secret/data/messenger/{db,redis,minio,jwt,server,grafana}`.
Nomad accesses Vault via workload identity (JWT signed by Nomad, validated by Vault).

### TLS

Wildcard cert `*.behappy.rest` + `behappy.rest` via certbot + Cloudflare DNS plugin.
APISIX reads certs directly from `/etc/letsencrypt/`.

---

## Storage Layout (MinIO)

All buckets are **private**. Files are delivered via presigned GET URLs (SigV4, default TTL: 900s).

| Bucket | Purpose | Key pattern |
|--------|---------|-------------|
| `bh-avatars` | User/chat profile pictures | `avatars/{user_id}/{uuid}.{ext}` |
| `bh-uploads` | File attachments | `uploads/{uuid}_{filename}` |
| `bh-stickers` | Sticker SVGs (seeded at init) | `stickers/s0N.svg` |
| `bh-test-artifacts` | Integration test assets | free-form |

---

## Database Migrations (V1–V20)

| Version | Purpose |
|---------|---------|
| V1 | Core schema: users, chats, chat_members, messages (partitioned) |
| V2–V5 | Stickers, seed data, MinIO sticker URLs |
| V6–V7 | Channels, invite links, roles |
| V8 | Message types (text/sticker/voice/file) |
| V9 | Admin panel (`is_admin` field) |
| V10–V11 | Favorites, mute, unread tracking |
| V12 | Emoji reactions |
| V13 | Soft delete, for_everyone, file_id |
| V14 | Reply-to-message |
| V15 | Message editing |
| V16 | Pinned messages |
| V17 | Last-seen privacy |
| V18 | Chat avatars |
| V19 | Pinned messages CASCADE fix |
| V20 | Forwarded message attribution |

---

## API Quick Reference

```bash
# Health
curl https://api.behappy.rest/health

# Register
curl -X POST https://api.behappy.rest/auth/register \
  -H 'Content-Type: application/json' \
  -d '{"username":"alice","email":"alice@example.com","password":"secret123"}'

# Login
TOKEN=$(curl -s -X POST https://api.behappy.rest/auth/login \
  -H 'Content-Type: application/json' \
  -d '{"username":"alice","password":"testpass"}' | jq -r .access_token)

# Profile
curl -H "Authorization: Bearer $TOKEN" https://api.behappy.rest/me

# Create chat + send message
CHAT=$(curl -s -X POST https://api.behappy.rest/chats \
  -H "Authorization: Bearer $TOKEN" \
  -H 'Content-Type: application/json' \
  -d '{"type":"direct","member_ids":[2]}' | jq .id)

curl -X POST https://api.behappy.rest/chats/$CHAT/messages \
  -H "Authorization: Bearer $TOKEN" \
  -H 'Content-Type: application/json' \
  -d '{"content":"Hello!","type":"text"}'

# WebSocket
wscat -c "wss://ws.behappy.rest/ws?token=$TOKEN"

# File upload
curl -X POST https://api.behappy.rest/files \
  -H "Authorization: Bearer $TOKEN" \
  -F "file=@photo.jpg"
```

### WebSocket Protocol

Client-to-server: `auth`, `subscribe`, `typing`, `presence_update`, `ping`
Server-to-client: `pong`, `message`, `typing`, `presence`, `reaction`, `message_deleted`, `message_updated`, `message_pinned`, `message_unpinned`

---

## Testing

### Test Suites

| Suite | File | Checks | Scope |
|-------|------|--------|-------|
| **Smoke** | `infra/scripts/smoke_test.py` | 53 | 20 sections: health, auth, chats, messages, replies, editing, files, voice, WS, presence, metrics, reactions, pinning, forwarding, privacy, search, chat avatars |
| **Feature** | `infra/scripts/feature_test.py` | 23 | Self-chat/Saved Messages, delete chat, forward with attribution |
| **Presence** | `infra/scripts/presence_test.py` | 9 | Active/away detection, presence_update, duplicate suppression, disconnect cleanup, privacy enforcement |

```bash
# Run against production
python infra/scripts/smoke_test.py
python infra/scripts/feature_test.py
python infra/scripts/presence_test.py

# Run against local
SMOKE_API_URL=http://localhost:8080 SMOKE_WS_URL=ws://localhost:8080/ws \
  python infra/scripts/smoke_test.py
```

### C++ Unit Tests

```bash
cd backend/build/linux-debug
JWT_SECRET="test-secret-at-least-16-chars" ctest --output-on-failure
```

---

## CI/CD

### GitHub Actions

**`ci.yml`** — on push to master/main/develop, PRs:
1. **build-linux** — C++ compile + test (GCC, Drogon Docker image)
2. **build-windows** — C++ compile + test (MSVC, vcpkg)
3. **build-macos** — C++ compile + test (Clang, Homebrew)
4. **compose-smoke** — Docker Compose integration test

**`desktop-build.yml`** — on `v*` tags or manual dispatch:
1. **build-windows** — Tauri NSIS installer
2. **build-macos** — Tauri DMG

Self-hosted runner `behappy-runner` on production server (Debian 12).

### Deploy

```bash
# Nomad mode (production)
ssh deploy@behappy.rest 'DEPLOY_MODE=nomad bash /opt/messenger/repo/infra/scripts/02-deploy.sh'

# Typical deploy: ~30s (cached Docker layers)
```

---

## Building the C++ Backend Locally

### Linux (GCC)
```bash
cd backend
cmake --preset linux-release
cmake --build --preset linux-release
./build/linux-release/messenger_api
```

### Windows (MSVC + vcpkg)
```bash
cd backend
set VCPKG_ROOT=C:\vcpkg
cmake --preset windows-msvc
cmake --build --preset windows-msvc
```

### macOS (Clang + Homebrew)
```bash
brew install drogon openssl ninja
cd backend
cmake --preset macos-clang
cmake --build --preset macos-clang
```

---

## Common Commands

```bash
make up              # Start all services
make down            # Stop all services
make logs            # Tail all logs
make logs svc=api_cpp  # Tail a specific service
make migrate         # Re-run Flyway migrations
make test            # Run backend unit tests in Docker
make build           # Rebuild C++ image only
make shell-api       # Open shell in api_cpp container
make shell-postgres  # Open psql session
make clean           # Remove containers + volumes (DESTRUCTIVE)
make info            # Print all service URLs
```

---

## Monitoring

### Grafana

Open https://grafana.behappy.rest/ — **Messenger Overview** dashboard:
- API RPS and latency (p50/p95/p99)
- HTTP 5xx error rate
- Active WebSocket connections
- Container CPU and memory usage

### Prometheus Metrics (api_cpp)

```
messenger_http_requests_total{method,path,status}     counter
messenger_http_request_duration_seconds{method,path}  histogram
messenger_ws_connections_active                        gauge
messenger_ws_connections_total                         counter
```

---

## Troubleshooting

**Flyway fails on first start**
```bash
make logs svc=flyway
# Most common: postgres not ready yet — increase retries or run `make migrate`
```

**api_cpp fails to start**
```bash
make logs svc=api_cpp
# Check JWT_SECRET is set and is at least 16 characters
# Check Redis DNS is resolvable (retry loop handles temporary failures)
```

**MinIO bucket not created**
```bash
make logs svc=minio_init
# Manual fix:
docker compose exec minio mc alias set local http://localhost:9000 minioadmin changeme_minio
docker compose exec minio mc mb local/bh-uploads
```

**Redis pub/sub not working (presence/typing broken)**
```bash
# Redis DNS race at startup — api_cpp retries 15 times.
# If still failing, restart api_cpp after Redis is healthy.
docker compose restart api_cpp
```

**TLS cert renewal**
```bash
sudo certbot renew --dry-run
systemctl list-timers | grep certbot
```

**Nomad job status**
```bash
nomad job status messenger
consul catalog services
curl https://api.behappy.rest/health
```

---

## Architecture Deep Dive

See [docs/architecture.md](docs/architecture.md) for:
- Full system diagram with all API endpoints
- Data flow diagrams (auth, messaging, file upload, WebSocket)
- Database schema (all 20+ tables)
- Scaling strategy (10K → 100M users)
- Technology choices (why PostgreSQL, why Drogon)

## Deployment Runbook

See [docs/DEPLOY.md](docs/DEPLOY.md) for server bootstrap, Vault/Consul/Nomad setup, and operational procedures.

## Environment Variables

See [docs/env-vars.md](docs/env-vars.md) for the full reference of all configuration options.
