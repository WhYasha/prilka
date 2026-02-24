# Architecture — Messenger Platform

## System Components

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              Client Layer                                   │
│   Browser SPA (HTML/JS/CSS)          Future: iOS Swift Client               │
└──────────────┬──────────────────────────────────────┬───────────────────────┘
               │ HTTP + WebSocket                     │ HTTP + WebSocket
               ▼                                      ▼
┌──────────────────────────────┐    ┌──────────────────────────────────────────┐
│   api_legacy (Flask :5000)   │    │        api_cpp (Drogon C++ :8080)        │
│   ─ Serves existing SPA      │    │   ─ POST /auth/register, /auth/login     │
│   ─ Session-based auth       │    │   ─ POST /auth/refresh  GET /me          │
│   ─ Legacy /api/* endpoints  │    │   ─ GET  /users/{id}    /users/search    │
│   ─ Role: UI host + compat   │    │   ─ POST /chats         GET /chats       │
│   ─ TODO: retire or migrate  │    │   ─ POST /chats/{id}/messages            │
└──────────────────────────────┘    │   ─ GET  /chats/{id}/messages            │
                                    │   ─ POST /files         /files/{id}/dl   │
                                    │   ─ WS   /ws (subscribe, presence)       │
                                    │   ─ GET  /health        /metrics         │
                                    └──────────┬───────────────────────────────┘
                                               │
               ┌───────────────────────────────┼──────────────────────────────┐
               │                               │                              │
               ▼                               ▼                              ▼
┌──────────────────────┐       ┌───────────────────────┐     ┌───────────────────────┐
│  PostgreSQL 16       │       │  Redis 7              │     │  MinIO (S3)           │
│  ─ users             │       │  ─ WebSocket pub/sub  │     │  ─ file objects       │
│  ─ chats             │       │  ─ presence cache     │     │  ─ presigned URLs     │
│  ─ chat_members      │       │  ─ rate limit counters│     │  ─ bucket: messenger- │
│  ─ messages          │       │  ─ session cache      │     │    files              │
│  ─ files             │       │   (future)            │     └───────────────────────┘
│  ─ refresh_tokens    │       └───────────────────────┘
│  ─ stickers          │
└──────────────────────┘

┌──────────────────────────────────────────────────────────────────────────────┐
│                         Observability Stack                                  │
│                                                                              │
│  cAdvisor  ──scrapes──▶  Prometheus  ──datasource──▶  Grafana               │
│  api_cpp/metrics                                        (dashboards)         │
└──────────────────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────────────────┐
│                         Tooling                                              │
│  Flyway (migrations)    Docker Compose (local)    GitHub Actions (CI)        │
│  CMake + Ninja (build)  Makefile (convenience)                               │
└──────────────────────────────────────────────────────────────────────────────┘
```

## Data Flow

### Register / Login
```
Client ──POST /auth/register──▶ api_cpp
  api_cpp ──INSERT users──▶ PostgreSQL
  api_cpp ──▶ { id, username }

Client ──POST /auth/login──▶ api_cpp
  api_cpp ──SELECT user WHERE username──▶ PostgreSQL
  api_cpp verifies PBKDF2-SHA256 password
  api_cpp creates JWT (access 1h + refresh 7d)
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
  api_cpp ──▶ HTTP 201 { id, content, created_at }

# On multi-node deployment: every api_cpp instance subscribes to Redis;
# any node receives PUBLISH and fans out to its local WS connections.
```

### File Upload
```
Client ──POST /files (multipart)──▶ api_cpp
  api_cpp ──validates size + MIME──▶ ok
  api_cpp ──PUT /{bucket}/{uuid_filename}──▶ MinIO
  api_cpp ──INSERT files──▶ PostgreSQL
  api_cpp ──▶ { id, filename, mime_type, object_key }

Client ──GET /files/{id}/download──▶ api_cpp
  api_cpp ──SELECT files WHERE id──▶ PostgreSQL
  api_cpp generates presigned GET URL (AWS SigV4, 1h TTL)
  api_cpp ──▶ 302 Redirect to presigned MinIO URL
  Client ──▶ MinIO direct download
```

## Why PostgreSQL?

| Concern | Choice |
|---------|--------|
| ACID transactions | Full support; critical for message ordering |
| Relational data | Users, chats, members, tokens — naturally relational |
| Partitioning | `messages` partitioned by `created_at` (range); add monthly partitions |
| Full-text search | `pg_trgm` + GIN indexes for user/message search (future) |
| JSON columns | `JSONB` available for metadata without schema sacrifice |
| Scaling | Read replicas + pgBouncer connection pooling for 10M+ users |
| SQLite limitations | No concurrent writes, no network access, no partitioning |

## Why Drogon (C++ framework)?

| Criterion | Drogon | Crow | Pistache |
|-----------|--------|------|----------|
| HTTP/1.1 + HTTP/2 | ✓ | HTTP/1.1 only | HTTP/1.1 only |
| WebSocket | ✓ native | Limited | No |
| Async DB (PostgreSQL) | ✓ built-in pool | External | External |
| Redis client | ✓ built-in | External | External |
| C++17/20 coroutines | ✓ | No | No |
| Performance (TFB) | Top 5 C++ | Mid | Mid |
| Active maintenance | ✓ | Slow | Slow |
| Plugin system | ✓ | No | No |

Drogon's built-in async PostgreSQL client and Redis client eliminate the need
for additional ORM or caching libraries, simplifying the dependency tree.

## How This System Scales to Millions of Users

### Stateless API tier
- `api_cpp` is fully stateless; JWT contains all auth state.
- Scale horizontally: add more `api_cpp` instances behind a load balancer (nginx/HAProxy/ALB).

### WebSocket fan-out
- Each `api_cpp` node subscribes to Redis Pub/Sub channels for chats it has active WS connections for.
- Publishing a message to Redis fan-outs to ALL nodes; each node pushes to its local connections.
- This pattern supports N api_cpp nodes with no shared memory.

### Database
- **Connection pooling**: pgBouncer in front of PostgreSQL (transaction mode).
- **Read replicas**: route GET queries to replicas via Drogon's multi-client setup.
- **Partitioning**: `messages` is partitioned by month (`RANGE` on `created_at`). Old partitions can be archived/moved to cold storage.
- **Sharding** (future): shard `messages` by `chat_id % N` across PostgreSQL shards.

### File storage
- MinIO in distributed mode (Erasure Coding) locally; replace with AWS S3 in production.
- All files accessed via presigned URLs (CDN-cacheable in production).
- Never proxy file content through the API tier.

### Caching
- User profiles: Redis HASH with TTL (reduce user lookup DB hits by ~90%).
- Online presence: Redis SETEX per user; expire on disconnect.
- Rate limiting: Redis atomic INCR + EXPIRE per IP/user.

### Future scaling milestones
| Users | Action |
|-------|--------|
| 10K   | Current setup works as-is |
| 100K  | Add pgBouncer, Redis Cluster, add 2nd api_cpp instance |
| 1M    | Add read replicas, CDN for files, nginx rate limiting |
| 10M   | Shard messages table, Kafka for event streaming, separate media service |
| 100M  | Multi-region PostgreSQL (Citus/CockroachDB), global CDN, geo-routing |
