# Messenger Platform

A scalable, production-minded messenger with a C++ backend, PostgreSQL, Redis,
MinIO, and a full monitoring stack — all running locally with a single command.

```
                  Browser / iOS
                       │
              ┌────────┴────────┐
              │                 │
        https://behappy.rest    │
         nginx (TLS proxy)      │
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

## Production URLs

| Resource | URL | Notes |
|----------|-----|-------|
| **Web app** | https://behappy.rest/ | Flask messenger UI |
| **API base** | https://behappy.rest/api | C++ Drogon backend |
| **API health** | https://behappy.rest/api/health | `{"status":"ok"}` |
| **WebSocket** | `wss://behappy.rest/ws` | JWT via `?token=` query param |
| **Grafana** | https://behappy.rest/grafana/ | admin / see `.env` |
| **MinIO Console** | https://behappy.rest/minio-console/ | minioadmin / see `.env` |
| **Downloads – Windows** | https://behappy.rest/downloads/windows/ | Desktop installer |
| **Downloads – macOS** | https://behappy.rest/downloads/macos/ | Desktop installer |

> Grafana and MinIO Console are not behind additional auth at the nginx level.
> Restrict by IP (`allow`/`deny` in nginx) before publishing publicly.

### Quick API test

```bash
# Health
curl https://behappy.rest/api/health

# Login (seed accounts: alice / bob / carol, password: testpass)
TOKEN=$(curl -s -X POST https://behappy.rest/api/auth/login \
  -H 'Content-Type: application/json' \
  -d '{"username":"alice","password":"testpass"}' | jq -r .access_token)

# Profile
curl -H "Authorization: Bearer $TOKEN" https://behappy.rest/api/me

# WebSocket
wscat -c "wss://behappy.rest/ws?token=$TOKEN"
```

---

## Quick Start (local dev)

```bash
# 1. Clone
git clone https://github.com/WhYasha/prilka.git
cd prilka

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
- Runs Flyway migrations (V1–V4)
- Starts the C++ API, legacy Flask UI, Prometheus, Grafana, cAdvisor

## Prerequisites

| Tool | Version | Why |
|------|---------|-----|
| Docker | 24+ | All services run in containers |
| Docker Compose | v2 (plugin) | Orchestration |
| `make` | any | Convenience commands |
| `curl` (optional) | any | Smoke testing |

No compiler, Python, or database installation required on the host machine.

## Local Service URLs

| Service | URL | Credentials |
|---------|-----|-------------|
| **C++ API** | http://localhost:8080 | JWT tokens |
| **Legacy UI** | http://localhost:5000 | alice/testpass |
| **MinIO Console** | http://localhost:9001 | minioadmin/changeme_minio |
| **Prometheus** | http://localhost:9090 | — |
| **Grafana** | http://localhost:3000 | admin/admin |
| **cAdvisor** | http://localhost:8081 | — |

## Production Deployment

### Server layout

```
/opt/messenger/
├── repo/          git clone of this repository
│   ├── docker-compose.yml
│   ├── docker-compose.prod.yml
│   └── infra/scripts/
│       ├── 01-bootstrap.sh   initial server setup
│       └── 02-deploy.sh      git pull + rebuild + restart
└── env/
    └── .env       secrets (chmod 600, never committed)
```

### Deploy / update

```bash
# SSH to server as deploy user, then:
bash /opt/messenger/repo/infra/scripts/02-deploy.sh

# Or manually:
cd /opt/messenger/repo
git pull
sudo systemctl restart messenger
```

### Environment config

Secrets live in `/opt/messenger/env/.env` (chmod 600). Never committed to git.
See [docs/env-vars.md](docs/env-vars.md) for the full reference.

**Minimum changes for security:**
```bash
JWT_SECRET=$(openssl rand -hex 32)
POSTGRES_PASSWORD=<strong-password>
REDIS_PASSWORD=<strong-password>
MINIO_ROOT_PASSWORD=<strong-password>
```

### TLS / Let's Encrypt

Certs are managed by certbot on the host:
- **Cert path**: `/etc/letsencrypt/live/behappy.rest/`
- **Renewal**: `systemctl status certbot.timer` (auto-renews 30 days before expiry)
- **Renewal hook**: `/etc/letsencrypt/renewal-hooks/deploy/reload-nginx.sh` reloads nginx after renewal

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

## API Quick Reference

### Register + Login
```bash
# Register
curl -X POST https://behappy.rest/api/auth/register \
  -H 'Content-Type: application/json' \
  -d '{"username":"alice","email":"alice@example.com","password":"secret123"}'

# Login → get tokens
TOKEN=$(curl -s -X POST https://behappy.rest/api/auth/login \
  -H 'Content-Type: application/json' \
  -d '{"username":"alice","password":"testpass"}' \
  | jq -r .access_token)

# Use token
curl https://behappy.rest/api/me -H "Authorization: Bearer $TOKEN"
```

### Create chat + send message
```bash
# Create direct chat with user 2
CHAT=$(curl -s -X POST https://behappy.rest/api/chats \
  -H "Authorization: Bearer $TOKEN" \
  -H 'Content-Type: application/json' \
  -d '{"type":"direct","member_ids":[2]}' | jq .id)

# Send message
curl -X POST https://behappy.rest/api/chats/$CHAT/messages \
  -H "Authorization: Bearer $TOKEN" \
  -H 'Content-Type: application/json' \
  -d '{"content":"Hello!","type":"text"}'
```

### WebSocket
```bash
# Using wscat (npm install -g wscat)
wscat -c "wss://behappy.rest/ws?token=$TOKEN"
```

### Upload a file
```bash
curl -X POST https://behappy.rest/api/files \
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
Open https://behappy.rest/grafana/ (admin / see `.env`). The **Messenger Overview**
dashboard is pre-provisioned with:
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
├── migrations/               Flyway SQL migrations (V1–V4)
├── infra/
│   ├── nginx/                nginx.conf (TLS reverse proxy)
│   ├── prometheus/           Prometheus config
│   ├── grafana/              Grafana provisioning + dashboards
│   └── scripts/              Bootstrap, deploy, smoke test
├── simple-messenger/         Legacy Flask app (existing UI)
├── docs/
│   ├── architecture.md       System design + scaling strategy
│   ├── mobile.md             iOS/Swift API contract
│   └── env-vars.md           Environment variable reference
├── docker-compose.yml        Main orchestration file
├── docker-compose.prod.yml   Production overrides (TLS, no dev ports)
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

**502 Bad Gateway at /**
```bash
docker logs repo-api_legacy-1
# Flask must listen on 0.0.0.0:5000, not 127.0.0.1:5000
```

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

**Check all container health**
```bash
docker compose -f docker-compose.yml -f docker-compose.prod.yml ps
docker logs repo-nginx-1 --tail 50
docker logs repo-api_cpp-1 --tail 50
```

**TLS cert renewal**
```bash
sudo certbot renew --dry-run   # test renewal
systemctl list-timers | grep certbot
```

**Windows: cAdvisor fails to start**
- cAdvisor uses Linux cgroups. On Windows/macOS Docker Desktop, either remove the
  `cadvisor` service from `docker-compose.yml` or ignore its failure — all other
  services work normally.
