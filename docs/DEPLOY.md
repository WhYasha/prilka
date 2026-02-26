# BeHappy Messenger — Production Deployment Runbook

Target: Debian 12 (Bookworm) · Nomad + Nginx · HTTPS · systemd-managed

---

## 1. Prerequisites (local machine)

```bash
# Your ed25519 key must be registered on the server (bootstrap does this)
ssh-keygen -t ed25519 -C "deploy@messenger" -f ~/.ssh/messenger_deploy
# Public key used in bootstrap: see infra/scripts/01-bootstrap.sh
```

---

## 2. Bootstrap the server (one time, as root)

```bash
# Copy the bootstrap script to the server
scp infra/scripts/01-bootstrap.sh root@<SERVER_IP>:/tmp/

# Run it
ssh root@<SERVER_IP> "bash /tmp/01-bootstrap.sh"
```

What it does:
- `apt upgrade`, installs: git, ufw, curl, ca-certificates, gnupg, fail2ban, unattended-upgrades
- Installs Docker + Compose plugin from the official Docker apt repo
- Creates user `deploy`, adds to `sudo` and `docker` groups
- Places the ed25519 public key in `/home/deploy/.ssh/authorized_keys`
- Hardens SSH (`PasswordAuthentication no`, `PermitRootLogin no`)
- Validates `sshd_config` with `sshd -t` **before** reloading (safe)
- UFW: allow 22/tcp, 80/tcp, 443/tcp; default deny inbound
- Fail2ban for SSH
- Creates `/opt/messenger/{repo,env,data,backups,scripts}`
- Clones the repo, copies `.env.example` → `/opt/messenger/env/.env`

**After bootstrap**, verify from a NEW terminal before closing the root session:

```bash
ssh -i ~/.ssh/messenger_deploy deploy@<SERVER_IP>
```

---

## 3. Architecture Overview

### Tiered Architecture

```
Tier 0 (systemd):  Vault → Consul → Nginx → Nomad
Tier 1 (Nomad):    PostgreSQL, Redis, MinIO, api_cpp, Prometheus, Grafana, cAdvisor
```

### Tier 0 — systemd Services

| Service | Unit | Container | Purpose |
|---------|------|-----------|---------|
| Vault | `vault.service` | `messenger-vault` | Secrets management (KV v2) |
| Consul | `consul.service` | `messenger-consul` | Service discovery + health checks |
| Nginx | `nginx.service` | `messenger-nginx` | TLS termination, reverse proxy |
| Nomad | `nomad.service` | host-native | Container orchestration |

Start order: Vault → unseal → Consul → Nginx → Nomad.
All containers on `messenger_net` Docker network.

### Tier 1 — Nomad Job `messenger`

| Group | Tasks | Notes |
|-------|-------|-------|
| `infra` | PostgreSQL 16, Redis 7, MinIO | Stateful services with Docker volumes |
| `app` | minio-init (prestart), Flyway (prestart), api_cpp (main) | Init tasks run before API |
| `monitoring` | Prometheus, Grafana, cAdvisor | Observability stack |

---

## 4. Install systemd services

```bash
sudo cp /opt/messenger/repo/infra/systemd/{vault,consul,nginx,nomad}.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable vault consul nginx nomad
```

---

## 5. Vault Setup

```bash
# Start Vault
sudo systemctl start vault

# Initialize (first time only — save the unseal keys!)
export VAULT_ADDR=http://127.0.0.1:8200
vault operator init -key-shares=5 -key-threshold=3

# Save credentials securely
mkdir -p /opt/messenger/vault
# Store unseal keys in unseal-keys.json, root token in root-token

# Unseal
bash /opt/messenger/repo/infra/vault/unseal.sh

# Enable KV v2 and create secrets
vault secrets enable -path=secret kv-v2
vault kv put secret/messenger/db host=postgres port=5432 ...
vault kv put secret/messenger/redis host=redis port=6379 ...
vault kv put secret/messenger/minio endpoint=http://minio:9000 ...
vault kv put secret/messenger/jwt secret=<random-64-chars> ...
vault kv put secret/messenger/server port=8080 threads=0 ...
vault kv put secret/messenger/grafana user=admin password=<pass> ...

# Enable JWT auth for Nomad workload identity
vault auth enable -path=jwt-nomad jwt
# Configure with Nomad's public key (see infra-migration-notes.md)
```

---

## 6. Consul Setup

```bash
sudo systemctl start consul

# Verify
consul catalog services
```

---

## 7. Nginx Setup

Nginx runs as a Docker container via systemd, reading config from `infra/nginx/nginx.conf`.

```bash
# Validate config before starting
docker run --rm --network messenger_net \
    -v /opt/messenger/repo/infra/nginx/nginx.conf:/etc/nginx/nginx.conf:ro \
    -v /etc/letsencrypt:/etc/letsencrypt:ro \
    nginx:1.27-alpine nginx -t

# Start
sudo systemctl start nginx

# Verify
curl -k https://localhost/health
```

### TLS Certificates

Wildcard cert `*.behappy.rest` + `behappy.rest` via certbot + Cloudflare DNS plugin:

```bash
sudo certbot certonly --dns-cloudflare \
    --dns-cloudflare-credentials /root/.cloudflare.ini \
    -d 'behappy.rest' -d '*.behappy.rest'

# Auto-renewal
sudo certbot renew --dry-run
systemctl list-timers | grep certbot
```

Nginx reads certs directly from `/etc/letsencrypt/live/behappy.rest/`.

---

## 8. Nomad Setup

```bash
# Start Nomad
sudo systemctl start nomad

# Verify
export NOMAD_ADDR=http://127.0.0.1:4646
nomad server members
nomad node status
```

---

## 9. Deploy / Update

```bash
# Standard deploy (Nomad mode)
ssh deploy@behappy.rest 'DEPLOY_MODE=nomad bash /opt/messenger/repo/infra/scripts/02-deploy.sh'

# What it does:
# 1. git pull
# 2. Build messenger-api:local Docker image
# 3. Validate and restart Nginx
# 4. Update deploy_ts in Nomad job spec (forces new allocation)
# 5. nomad job run messenger.nomad.hcl
```

Typical deploy: ~30s (cached Docker layers).

---

## 10. Verify Deployment

```bash
# Nomad job status
nomad job status messenger

# Consul services
consul catalog services

# API health
curl https://api.behappy.rest/health

# Run test suites
python3 infra/scripts/smoke_test.py      # 58 checks
python3 infra/scripts/feature_test.py    # 23 checks
python3 infra/scripts/presence_test.py   # 9 checks
```

---

## 11. UFW Rules

| Port | Protocol | Reason |
|------|----------|--------|
| 22 | TCP | SSH |
| 80 | TCP | HTTP (ACME challenge + redirect to HTTPS) |
| 443 | TCP | HTTPS (all app traffic via Nginx) |

Internal: `allow from 172.19.0.0/16 to any port 4646 tcp` (Nginx → Nomad UI).

---

## 12. Logs

```bash
# Tier 0 services
journalctl -u vault -f
journalctl -u consul -f
journalctl -u nginx -f
journalctl -u nomad -f

# Tier 1 services (via Nomad)
nomad alloc logs <alloc-id>
nomad alloc logs -stderr <alloc-id>

# Docker Compose mode (fallback)
docker compose -f docker-compose.yml -f docker-compose.prod.yml logs -f api_cpp
```

---

## 13. Rollback

```bash
# Code rollback
cd /opt/messenger/repo
git log --oneline -10
git checkout <COMMIT_SHA>
DEPLOY_MODE=nomad bash infra/scripts/02-deploy.sh

# Nginx config rollback
git checkout HEAD~1 -- infra/nginx/nginx.conf
sudo systemctl restart nginx
```

---

## 14. Rotate Secrets

1. Update values in Vault: `vault kv put secret/messenger/jwt secret=<new>`
2. Redeploy: `DEPLOY_MODE=nomad bash infra/scripts/02-deploy.sh`
3. For JWT secret rotation: all existing tokens are invalidated (users re-login)

---

## 15. Backups

### Postgres

Automatic daily dump at 03:00 via `messenger-backup.timer`:

```
/opt/messenger/backups/postgres_YYYYMMDD_HHMMSS.sql.gz
```

Retention: 7 days. Restore:

```bash
gunzip -c /opt/messenger/backups/postgres_20260223_030000.sql.gz \
    | docker exec -i messenger-postgres-1 psql -U messenger -d messenger
```

### MinIO

```bash
docker run --rm --network messenger_net minio/mc \
    mirror local/messenger-files /opt/messenger/backups/minio/
```

---

## 16. Server File Paths

```
/opt/messenger/repo/                 Git repository
/opt/messenger/env/.env              Legacy env file (Docker Compose mode)
/opt/messenger/vault/                Vault credentials (unseal-keys, tokens, role/secret IDs)
/opt/messenger/nomad/data/           Nomad data directory
/opt/messenger/nomad/env             Nomad env file (VAULT_TOKEN)
/etc/letsencrypt/live/behappy.rest/  TLS certificates
/etc/systemd/system/{vault,consul,nginx,nomad}.service  systemd units
```

---

## 17. Download Links (Desktop Installers)

Files served by Nginx at `downloads.behappy.rest`:

```
https://downloads.behappy.rest/windows/BeHappySetup.exe
https://downloads.behappy.rest/macos/BeHappy.dmg
```

Upload:

```bash
scp BeHappySetup.exe deploy@behappy.rest:/opt/messenger/repo/infra/nginx/downloads/windows/
scp BeHappy.dmg      deploy@behappy.rest:/opt/messenger/repo/infra/nginx/downloads/macos/
```
