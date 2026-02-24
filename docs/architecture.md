# Architecture — Messenger Platform

## System Components

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              Client Layer                                   │
│   Vue 3 SPA (TypeScript)       Tauri Desktop App (Windows / macOS)         │
│   served by Drogon             wraps https://behappy.rest                   │
└──────────────┬──────────────────────────────────┬───────────────────────────┘
               │ HTTPS + WebSocket                │ HTTPS + WebSocket
               ▼                                  ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                      nginx (TLS termination, reverse proxy)                 │
│   /api/  → api_cpp:8080       /ws  → api_cpp:8080 (WebSocket upgrade)      │
│   /minio/ → minio:9000        /downloads/ → static filesystem              │
│   /grafana/ → grafana:3000    /   → api_cpp:8080 (SPA fallback)            │
└──────────────────────────────┬──────────────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                    api_cpp (Drogon C++ :8080)                                │
│                                                                              │
│   Auth:     POST /auth/register, /auth/login, /auth/refresh  GET /me        │
│   Users:    GET /users/{id}, /users/search, /users/by-username/{u}          │
│             PUT /users/{id}, PUT /users/me/avatar, GET /users/{id}/avatar   │
│   Chats:    POST /chats  GET /chats, /chats/{id}, /chats/by-name/{name}    │
│             POST /chats/{id}/favorite  DELETE /chats/{id}/favorite          │
│             POST /chats/{id}/mute      DELETE /chats/{id}/mute             │
│             DELETE /chats/{id}/leave                                        │
│   Messages: POST /chats/{id}/messages  GET /chats/{id}/messages            │
│   Files:    POST /files                GET /files/{id}/download            │
│   Stickers: GET /stickers              GET /stickers/{id}/image            │
│   Settings: GET /settings              PUT /settings                       │
│   Invites:  POST /chats/{id}/invites   GET /chats/{id}/invites            │
│             DELETE /invites/{token}     GET /invites/{token}/preview        │
│             POST /invites/{token}/join                                      │
│   Admin:    GET /admin-api/stats, /admin-api/users, /admin-api/messages    │
│             POST /admin-api/users/{id}/block|unblock|soft-delete|toggle-admin│
│             GET|POST /admin-api/support/*                                   │
│   WS:       /ws (auth, subscribe, message, presence, ping/pong)            │
│   Web:      /app, /login, /register, /@{user}, /dm/{id}, /c/{name},       │
│             /join/{token}, /admin/* → serves index.html (SPA routes)       │
│   Ops:      GET /health, GET /metrics                                      │
└──────────┬──────────────────┬──────────────────┬───────────────────────────┘
           │                  │                  │
           ▼                  ▼                  ▼
┌────────────────────┐ ┌────────────────────┐ ┌─────────────────────────────┐
│  PostgreSQL 16     │ │  Redis 7           │ │  MinIO (S3-compatible)      │
│  ─ users           │ │  ─ WS pub/sub      │ │  ─ bh-avatars               │
│  ─ chats           │ │    (chat fan-out)   │ │  ─ bh-uploads               │
│  ─ chat_members    │ │  ─ presence cache   │ │  ─ bh-stickers              │
│  ─ messages (part.)│ │  ─ rate limiting    │ │  ─ bh-test-artifacts        │
│  ─ files           │ │    (future)         │ │  ─ presigned URLs (SigV4)   │
│  ─ refresh_tokens  │ └────────────────────┘ └─────────────────────────────┘
│  ─ stickers        │
│  ─ sticker_packs   │
│  ─ user_settings   │
│  ─ invites         │
│  ─ chat_favorites  │
│  ─ chat_mute_settings│
│  ─ message_read_receipts│
└────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│                         Observability Stack                                  │
│  api_cpp /metrics  ──scrapes──▶  Prometheus  ──datasource──▶  Grafana      │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│                         Tooling                                              │
│  Flyway (migrations)    Docker Compose (local + prod)    GitHub Actions (CI)│
│  CMake + Ninja (C++)    Vite (frontend)    Tauri (desktop)    Makefile      │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Frontend Architecture

### Vue 3 SPA

The frontend is a Vue 3 + TypeScript single-page application built with Vite.

```
frontend/
├── src/
│   ├── main.ts                    App bootstrap (createApp + Pinia + Router)
│   ├── App.vue                    Root component
│   ├── router/index.ts            Vue Router (history mode, auth guards)
│   ├── api/                       Typed API client modules (axios)
│   │   ├── client.ts              Axios instance, token interceptor, 401→refresh→retry
│   │   ├── auth.ts, users.ts, chats.ts, messages.ts, files.ts,
│   │   │   stickers.ts, settings.ts, invites.ts, admin.ts
│   ├── stores/                    Pinia state management
│   │   ├── auth.ts                User, tokens, login/logout/refresh/fetchMe
│   │   ├── chats.ts               Chat list, active chat, favorites, muted
│   │   ├── messages.ts            Messages per chat, send, load history
│   │   └── settings.ts            Theme, notifications, language
│   ├── composables/
│   │   ├── useWebSocket.ts        WS connect, auth, subscribe, reconnect
│   │   ├── useRecorder.ts         Voice message recording (MediaRecorder)
│   │   └── useInfiniteScroll.ts   Scroll-based older message loading
│   ├── views/                     Page-level components
│   │   ├── LoginView.vue, RegisterView.vue, MessengerView.vue,
│   │   │   AdminView.vue, JoinView.vue
│   ├── components/                Reusable UI components
│   │   ├── layout/                Sidebar, ChatPanel, Drawer, AppBar
│   │   ├── chat/                  ChatListItem, ChatHeader, MessageBubble,
│   │   │                          Composer, StickerPicker, ContextMenu
│   │   ├── modals/                NewChatModal, ProfileModal, UserProfileModal,
│   │   │                          DownloadModal
│   │   ├── admin/                 AdminDashboard, AdminUsers, AdminUserDetail,
│   │   │                          AdminMessages, AdminSupport
│   │   └── ui/                    Avatar, Badge, Toast, Spinner
│   └── assets/                    main.css, admin.css
├── src-tauri/                     Tauri desktop app (Rust)
├── dist/                          Vite build output → copied to backend/www/
└── dist-desktop/                  Tauri build output (gitignored)
```

**Build pipeline:**
- `npm run dev` — Vite dev server at `:5173`, proxies `/api` → `:8080`
- `npm run build` — production build to `frontend/dist/`
- `frontend/deploy.sh` — copies `dist/*` to `backend/www/`, deploys via SSH

### Tauri Desktop App

The desktop app wraps the production web app in a native window using Tauri v2.

| Platform | Bundle | Size | Status |
|----------|--------|------|--------|
| Windows  | NSIS installer (`MessengerSetup.exe`) | ~1.9 MB | Published |
| macOS    | DMG (`Messenger.dmg`) | TBD | CI ready, needs macOS build |

- **Runtime:** Tauri v2 (Rust) + WebView2 (Windows) / WebKit (macOS)
- **Mode:** Remote URL — loads `https://behappy.rest` directly
- **CSP:** Allows `https://behappy.rest`, `wss://behappy.rest`, MinIO URLs
- **Distribution:** `https://behappy.rest/downloads/windows/` and `/downloads/macos/`

## Data Flow

### Register / Login
```
Client ──POST /auth/register──▶ api_cpp
  api_cpp ──INSERT users──▶ PostgreSQL
  api_cpp ──▶ { id, username }

Client ──POST /auth/login──▶ api_cpp
  api_cpp ──SELECT user WHERE username──▶ PostgreSQL
  api_cpp verifies PBKDF2-SHA256 password
  api_cpp creates JWT (access 1h + refresh 7d; is_admin in claims)
  api_cpp ──INSERT refresh_tokens──▶ PostgreSQL
  api_cpp ──▶ { access_token, refresh_token, expires_in }
```

### Send Message (REST + WebSocket fan-out)
```
Client ──POST /chats/{id}/messages──▶ api_cpp
  api_cpp ──verifies JWT (AuthFilter)──▶ ok
  api_cpp ──checks chat_members──▶ PostgreSQL
  api_cpp ──INSERT messages──▶ PostgreSQL
  api_cpp ──PUBLISH chat:{id}──▶ Redis
    Redis ──subscriber callback──▶ api_cpp (this node)
      api_cpp ──WS push──▶ all subscribed connections on this node
  api_cpp ──▶ HTTP 201 { id, content, created_at, sender_*, sticker_*, attachment_* }

# On multi-node deployment: every api_cpp instance subscribes to Redis;
# any node receives PUBLISH and fans out to its local WS connections.
```

### File Upload + Presigned URL Download
```
Client ──POST /files (multipart)──▶ api_cpp
  api_cpp ──validates size + MIME──▶ ok
  api_cpp ──PUT /{bucket}/{uuid_filename}──▶ MinIO (server-to-server, Auth header)
  api_cpp ──INSERT files──▶ PostgreSQL
  api_cpp ──▶ { id, filename, mime_type, object_key }

Client ──GET /files/{id}/download──▶ api_cpp
  api_cpp ──SELECT files WHERE id──▶ PostgreSQL
  api_cpp generates presigned GET URL (AWS SigV4, 1h TTL)
    (signs with host=minio:9000, URI-encodes / as %2F per S3 spec)
    rewrites http://minio:9000/... → https://behappy.rest/minio/...
  api_cpp ──▶ 302 Redirect to presigned MinIO URL
  Client ──GET──▶ nginx /minio/ ──(Host: minio:9000)──▶ MinIO direct download
```

### WebSocket Connection
```
Client ──wss://host/ws?token=JWT──▶ nginx ──▶ api_cpp
  api_cpp verifies JWT
  Client sends: {"type":"auth","token":"<jwt>"}
  Client sends: {"type":"subscribe","chat_id":N} (for each chat)
  api_cpp subscribes to Redis channel chat:{N}

  On new message in chat N:
    Redis PUBLISH ──▶ api_cpp ──WS push──▶ all subscribed clients

  Heartbeat: client sends {"type":"ping"} every 25s, server replies {"type":"pong"}
  Reconnect: exponential backoff (1s, 2s, 4s... max 30s)
```

## Database Schema

Managed by Flyway migrations (V1–V10):

| Table | Purpose | Key fields |
|-------|---------|------------|
| `users` | User accounts | username, email, password_hash, display_name, bio, avatar_object_key, is_admin, is_blocked, last_activity |
| `chats` | Conversations | type (direct/group/channel), name, title, description, public_name |
| `chat_members` | Chat membership | chat_id, user_id, role (owner/admin/member) |
| `messages` | Chat messages (partitioned monthly) | chat_id, sender_id, content, type (text/sticker/voice/file), sticker_id, duration_seconds |
| `files` | Uploaded files | filename, mime_type, size, object_key, bucket |
| `refresh_tokens` | JWT refresh tokens | user_id, token_hash, expires_at |
| `sticker_packs` | Sticker collections | name |
| `stickers` | Individual stickers | pack_id, label, file_id |
| `user_settings` | Per-user preferences | user_id, theme, notifications_enabled, language |
| `invites` | Chat invite links | chat_id, token, created_by, expires_at, max_uses, use_count |
| `chat_favorites` | Starred chats | user_id, chat_id |
| `chat_mute_settings` | Muted chats | user_id, chat_id, muted_until |
| `message_read_receipts` | Read tracking | message_id, user_id, read_at |

## Docker Compose Services

| Service | Image | Ports | Purpose |
|---------|-------|-------|---------|
| `postgres` | `postgres:16-alpine` | 5432 (dev) | Primary database |
| `redis` | `redis:7-alpine` | internal | WebSocket pub/sub, presence |
| `minio` | `minio/minio:latest` | 9000, 9001 | S3-compatible object storage |
| `minio_init` | `minio/mc:latest` | — | Creates buckets + seeds stickers (run-once) |
| `flyway` | `flyway/flyway:10-alpine` | — | Database migrations V1–V10 (run-once) |
| `api_cpp` | Custom Dockerfile | 8080 | C++ Drogon API + SPA + WebSocket |
| `prometheus` | `prom/prometheus:v2.51.0` | 9090 | Metrics collection |
| `grafana` | `grafana/grafana:10.4.0` | 3000 | Metrics dashboards |

## CI/CD

### GitHub Actions Workflows

**ci.yml** — Runs on push to master and PRs:
1. **build-linux** — Compile + test C++ (Ubuntu, Drogon Docker image, Ninja)
2. **build-windows** — Compile + test C++ (MSVC, vcpkg dependencies)
3. **build-macos** — Compile + test C++ (Clang, Homebrew dependencies)
4. **compose-smoke** — Docker Compose integration test (start stack, run register + login)

**desktop-build.yml** — Runs on `v*` tags or manual dispatch:
1. **build-windows** — Build Vue + Tauri NSIS installer, upload artifact
2. **build-macos** — Build Vue + Tauri DMG, upload artifact
3. **publish** — SCP installers to `behappy.rest:/opt/messenger/repo/infra/nginx/downloads/`

### Deployment
```bash
# Local
make up / make down / make logs / make test

# Production
ssh deploy@behappy.rest 'bash /opt/messenger/repo/infra/scripts/02-deploy.sh'
# Pulls latest, builds Docker images, restarts stack (~30s)
```

## Why PostgreSQL?

| Concern | Choice |
|---------|--------|
| ACID transactions | Full support; critical for message ordering |
| Relational data | Users, chats, members, tokens — naturally relational |
| Partitioning | `messages` partitioned by `created_at` (range); monthly partitions |
| Full-text search | `pg_trgm` + GIN indexes for user/message search (future) |
| JSON columns | `JSONB` available for metadata without schema sacrifice |
| Scaling | Read replicas + pgBouncer connection pooling for 10M+ users |

## Why Drogon (C++ framework)?

| Criterion | Drogon | Crow | Pistache |
|-----------|--------|------|----------|
| HTTP/1.1 + HTTP/2 | Yes | HTTP/1.1 only | HTTP/1.1 only |
| WebSocket | Native | Limited | No |
| Async DB (PostgreSQL) | Built-in pool | External | External |
| Redis client | Built-in | External | External |
| C++17/20 coroutines | Yes | No | No |
| Performance (TFB) | Top 5 C++ | Mid | Mid |
| Active maintenance | Yes | Slow | Slow |

## How This System Scales

### Stateless API tier
- `api_cpp` is fully stateless; JWT contains all auth state (including `is_admin`).
- Scale horizontally: add more `api_cpp` instances behind nginx/HAProxy/ALB.

### WebSocket fan-out
- Each `api_cpp` node subscribes to Redis Pub/Sub channels for chats with active WS connections.
- Publishing a message to Redis fans out to ALL nodes; each pushes to its local connections.

### Database
- **Connection pooling**: pgBouncer in front of PostgreSQL (transaction mode).
- **Read replicas**: route GET queries to replicas via Drogon's multi-client setup.
- **Partitioning**: `messages` partitioned by month on `created_at`. Old partitions archivable.
- **Sharding** (future): shard `messages` by `chat_id % N` across PostgreSQL shards.

### File storage
- MinIO with multi-bucket design (avatars, uploads, stickers, test artifacts).
- All files accessed via presigned URLs (CDN-cacheable in production).
- Server-to-server uploads (C++ → MinIO with Authorization header).
- Public URL rewrite: `minio:9000` → `https://behappy.rest/minio/` via nginx.

### Caching (future)
- User profiles: Redis HASH with TTL.
- Online presence: Redis SETEX per user; expire on disconnect.
- Rate limiting: Redis atomic INCR + EXPIRE per IP/user.

### Scaling milestones
| Users | Action |
|-------|--------|
| 10K   | Current setup works as-is |
| 100K  | Add pgBouncer, Redis Cluster, 2nd api_cpp instance |
| 1M    | Add read replicas, CDN for files, nginx rate limiting |
| 10M   | Shard messages table, Kafka for event streaming, separate media service |
| 100M  | Multi-region PostgreSQL (Citus/CockroachDB), global CDN, geo-routing |
