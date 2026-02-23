# Messenger Platform

A scalable, production-minded messenger with a C++ backend, PostgreSQL, Redis,
MinIO, and a full monitoring stack — all running locally with a single command.

```
                  Browser / iOS
                       │
              ┌────────┴────────┐
              │                 │
         :5000 (UI)        :8080 (API)
         Flask Legacy       Drogon C++
              │                 │
     ┌────────┼────────┐        │
     │        │        │        │
  Postgres  Redis   MinIO  Prometheus
  :5432    :6379  :9000    :9090
                             │
                          Grafana :3000
                          cAdvisor :8081
```

## Quick Start

```bash
# 1. Clone
git clone <repo-url>
cd <repo>

# 2. Start everything (builds C++ image, runs migrations, starts all services)
make up

# 3. Check status
make ps
make info
```

`make up` automatically:
- Copies `.env.example` → `.env` on first run
- Builds the C++ backend Docker image
- Starts PostgreSQL, Redis, MinIO
- Creates the MinIO bucket
- Runs Flyway migrations (V1–V3)
- Starts the C++ API, legacy Flask UI, Prometheus, Grafana, cAdvisor

## Prerequisites

| Tool | Version | Why |
|------|---------|-----|
| Docker | 24+ | All services run in containers |
| Docker Compose | v2 (plugin) | Orchestration |
| `make` | any | Convenience commands |
| `curl` (optional) | any | Smoke testing |

No compiler, Python, or database installation required on the host machine.

## Service URLs

| Service | URL | Credentials |
|---------|-----|-------------|
| **C++ API** | http://localhost:8080 | JWT tokens |
| **Legacy UI** | http://localhost:5000 | alice/password123 |
| **MinIO Console** | http://localhost:9001 | minioadmin/changeme_minio |
| **Prometheus** | http://localhost:9090 | — |
| **Grafana** | http://localhost:3000 | admin/admin |
| **cAdvisor** | http://localhost:8081 | — |

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

## Configuration

All configuration is via environment variables. Copy `.env.example` to `.env`:

```bash
cp .env.example .env
# Edit .env and set proper passwords
```

See [docs/env-vars.md](docs/env-vars.md) for the full reference.

**Minimum changes for security:**
```bash
JWT_SECRET=$(openssl rand -hex 32)
POSTGRES_PASSWORD=<strong-password>
REDIS_PASSWORD=<strong-password>
MINIO_ROOT_PASSWORD=<strong-password>
```

## API Quick Reference

### Register + Login
```bash
# Register
curl -X POST http://localhost:8080/auth/register \
  -H 'Content-Type: application/json' \
  -d '{"username":"alice","email":"alice@example.com","password":"secret123"}'

# Login → get tokens
TOKEN=$(curl -s -X POST http://localhost:8080/auth/login \
  -H 'Content-Type: application/json' \
  -d '{"username":"alice","password":"secret123"}' \
  | jq -r .access_token)

# Use token
curl http://localhost:8080/me -H "Authorization: Bearer $TOKEN"
```

### Create chat + send message
```bash
# Create direct chat with user 2
CHAT=$(curl -s -X POST http://localhost:8080/chats \
  -H "Authorization: Bearer $TOKEN" \
  -H 'Content-Type: application/json' \
  -d '{"type":"direct","member_ids":[2]}' | jq .id)

# Send message
curl -X POST http://localhost:8080/chats/$CHAT/messages \
  -H "Authorization: Bearer $TOKEN" \
  -H 'Content-Type: application/json' \
  -d '{"content":"Hello!","type":"text"}'
```

### WebSocket
```bash
# Using wscat (npm install -g wscat)
wscat -c ws://localhost:8080/ws

# After connect:
{"type":"auth","token":"<access_token>"}
{"type":"subscribe","chat_id":1}
```

### Upload a file
```bash
curl -X POST http://localhost:8080/files \
  -H "Authorization: Bearer $TOKEN" \
  -F "file=@photo.jpg"
```

## Building the C++ Backend Locally (without Docker)

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

### Run tests
```bash
cd backend/build/linux-debug
JWT_SECRET="test-secret-at-least-16-chars" ctest --output-on-failure
```

## Database Migrations

Migrations live in `migrations/` and are managed by [Flyway](https://flywaydb.org/).
They run automatically on `make up`. To run manually:

```bash
make migrate
```

Migration files follow the naming convention `V{n}__{description}.sql`.

## Monitoring

### Grafana
Open http://localhost:3000 (admin/admin). The **Messenger Overview** dashboard
is pre-provisioned with:
- API RPS and latency (p50/p95/p99)
- HTTP 5xx error rate
- Active WebSocket connections
- Container CPU and memory usage

### Prometheus metrics (api_cpp)
```
messenger_http_requests_total{method,path,status}     counter
messenger_http_request_duration_seconds{method,path}  histogram
messenger_ws_connections_active                        gauge
messenger_ws_connections_total                         counter
```

## Project Structure

```
.
├── backend/                  C++ Drogon backend
│   ├── src/
│   │   ├── main.cpp
│   │   ├── config/Config.h
│   │   ├── services/         JwtService, MetricsService
│   │   ├── filters/          AuthFilter (JWT validation)
│   │   ├── controllers/      Auth, Users, Chats, Messages, Files
│   │   └── ws/               WebSocket handler
│   ├── tests/                GTest unit tests
│   ├── CMakeLists.txt
│   ├── CMakePresets.json
│   └── Dockerfile
├── migrations/               Flyway SQL migrations (V1–V3)
├── infra/
│   ├── prometheus/           Prometheus config
│   └── grafana/              Grafana provisioning + dashboards
├── simple-messenger/         Legacy Flask app (existing UI)
├── docs/
│   ├── architecture.md       System design + scaling strategy
│   ├── mobile.md             iOS/Swift API contract
│   └── env-vars.md           Environment variable reference
├── docker-compose.yml        Main orchestration file
├── .env.example              Environment template
├── Makefile                  Convenience commands
└── README.md                 This file
```

## Architecture

See [docs/architecture.md](docs/architecture.md) for:
- Full system diagram
- Data flow diagrams
- Why PostgreSQL
- Why Drogon
- Scaling strategy to millions of users

## iOS Mobile Client

See [docs/mobile.md](docs/mobile.md) for:
- Complete API contract
- Request/response schemas
- WebSocket protocol
- iOS implementation notes

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
```

**MinIO bucket not created**
```bash
make logs svc=minio_init
# Manual fix:
docker compose exec minio mc alias set local http://localhost:9000 minioadmin changeme_minio
docker compose exec minio mc mb local/messenger-files
```

**Windows: cAdvisor fails to start**
- cAdvisor uses Linux cgroups. On Windows/macOS Docker Desktop, either remove the
  `cadvisor` service from `docker-compose.yml` or ignore its failure — all other
  services work normally.
