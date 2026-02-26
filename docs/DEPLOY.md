# Messenger – Production Deployment Runbook

Target: Debian 12 (Bookworm) · Docker Compose · HTTP only · systemd-managed

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
- UFW: allow 22/tcp, 80/tcp; default deny inbound
- Fail2ban for SSH
- Creates `/opt/messenger/{repo,env,data,backups,scripts}`
- Clones the repo, copies `.env.example` → `/opt/messenger/env/.env`
- Installs and enables systemd units

**After bootstrap**, verify from a NEW terminal before closing the root session:

```bash
ssh -i ~/.ssh/messenger_deploy deploy@<SERVER_IP>
```

---

## 3. Configure secrets

```bash
ssh deploy@<SERVER_IP>
nano /opt/messenger/env/.env    # chmod 600 already set by bootstrap
```

Minimum values to change:

| Variable | Example |
|---|---|
| `POSTGRES_PASSWORD` | `openssl rand -hex 24` |
| `REDIS_PASSWORD` | `openssl rand -hex 24` |
| `MINIO_ROOT_PASSWORD` | `openssl rand -hex 24` |
| `JWT_SECRET` | `openssl rand -hex 32` |
| `GRAFANA_PASSWORD` | strong password |

The `.env` file lives only on the server at `/opt/messenger/env/.env`.
It is **never** committed to git.

---

## 4. First deploy

```bash
ssh deploy@<SERVER_IP>
cd /opt/messenger/repo

# Symlink .env into the repo directory
ln -sf /opt/messenger/env/.env .env

# Use production Prometheus config
cp infra/prometheus/prometheus.prod.yml infra/prometheus/prometheus.yml

# Start the stack
sudo systemctl start messenger
sudo systemctl status messenger

# Watch startup logs
journalctl -u messenger -f
docker compose -f docker-compose.yml -f docker-compose.prod.yml logs -f
```

---

## 5. Verify the deployment

```bash
# Containers running
docker ps

# Nginx + app health
curl http://<SERVER_IP>/health          # nginx: "ok"
curl http://<SERVER_IP>/api/health      # C++ backend JSON
curl http://<SERVER_IP>/                # Flask frontend

# Prometheus targets
curl -s http://<SERVER_IP>:9090/api/v1/targets | python3 -m json.tool
# (Prometheus is NOT exposed on port 80 by design; check inside stack)
docker exec messenger-prometheus-1 \
    wget -qO- 'http://localhost:9090/api/v1/targets' | python3 -m json.tool

# Grafana
open http://<SERVER_IP>/grafana/        # login: admin / <GRAFANA_PASSWORD>
```

---

## 6. UFW rules summary

| Port | Protocol | Reason |
|---|---|---|
| 22 | TCP | SSH |
| 80 | TCP | HTTP (all app traffic via nginx) |
| ~~443~~ | ~~TCP~~ | ~~HTTPS (TODO: enable when domain ready)~~ |

All other ports are blocked inbound.
Internal service ports (5432, 6379, 9000, 9090, 3000, 8080) are not exposed
to the host — traffic flows inside the `messenger_net` Docker bridge only.

---

## 7. sshd_config changes (summary)

| Setting | Value | Why |
|---|---|---|
| `PasswordAuthentication` | `no` | key-only login |
| `PermitRootLogin` | `no` | no direct root SSH |
| `PubkeyAuthentication` | `yes` | explicit |
| `X11Forwarding` | `no` | attack surface reduction |
| `AllowTcpForwarding` | `no` | attack surface reduction |
| `MaxAuthTries` | `3` | brute-force mitigation |

---

## 8. systemd units

| Unit | Purpose |
|---|---|
| `messenger.service` | Starts/stops the full Docker Compose stack |
| `messenger-backup.service` | One-shot Postgres dump |
| `messenger-backup.timer` | Triggers backup daily at 03:00 |

```bash
# Manage
sudo systemctl start/stop/restart messenger
sudo systemctl status messenger

# Backup manually
sudo systemctl start messenger-backup

# List timers
systemctl list-timers --all | grep messenger
```

---

## 9. How to operate

### Deploy / update

```bash
ssh deploy@<SERVER_IP>
bash /opt/messenger/repo/infra/scripts/02-deploy.sh
```

Or manually:

```bash
cd /opt/messenger/repo
git pull
docker compose -f docker-compose.yml -f docker-compose.prod.yml build
sudo systemctl restart messenger
```

### Logs

```bash
# All services
docker compose -f docker-compose.yml -f docker-compose.prod.yml logs -f

# Single service
docker compose -f docker-compose.yml -f docker-compose.prod.yml logs -f api_cpp

# systemd journal
journalctl -u messenger -f
```

### Rollback

```bash
cd /opt/messenger/repo
git log --oneline -10                   # find the previous good commit
git checkout <COMMIT_SHA>
sudo systemctl restart messenger
```

### Rotate secrets

1. Edit `/opt/messenger/env/.env` with new values
2. Restart the stack: `sudo systemctl restart messenger`
3. For JWT secret rotation: all existing tokens are invalidated immediately
   (users will be asked to log in again — expected behaviour)

---

## 10. Backups

### Postgres

Automatic daily dump at 03:00 via `messenger-backup.timer`:

```
/opt/messenger/backups/postgres_YYYYMMDD_HHMMSS.sql.gz
```

Retention: 7 days (configurable in `backup-postgres.sh`).

Restore:

```bash
gunzip -c /opt/messenger/backups/postgres_20260223_030000.sql.gz \
    | docker exec -i messenger-postgres-1 \
        psql -U messenger -d messenger
```

### MinIO

Option A – mirror to local directory (live):

```bash
docker run --rm --network messenger_net minio/mc \
    mirror local/messenger-files /opt/messenger/backups/minio/
```

Option B – raw volume snapshot (stop MinIO first):

```bash
sudo systemctl stop messenger
docker run --rm \
    -v messenger_minio_data:/src:ro \
    -v /opt/messenger/backups/minio:/dst \
    alpine tar czf /dst/minio_$(date +%Y%m%d).tar.gz -C /src .
sudo systemctl start messenger
```

---

## 11. Download links (Windows / macOS installers)

Files are served by nginx at:

```
http://<SERVER_IP>/downloads/windows/BeHappySetup.exe
http://<SERVER_IP>/downloads/macos/BeHappy.dmg
```

Upload artifacts when they are ready:

```bash
scp BeHappySetup.exe deploy@<SERVER_IP>:/opt/messenger/repo/infra/nginx/downloads/windows/
scp BeHappy.dmg      deploy@<SERVER_IP>:/opt/messenger/repo/infra/nginx/downloads/macos/
```

The frontend (`app.html`) auto-detects the OS and shows the matching
primary download button. Both platforms are always available under
"Other platforms".

---

## 12. TODO: Enable HTTPS

When a domain is available:

1. Point DNS A-record to `<SERVER_IP>`
2. Open UFW port 443: `sudo ufw allow 443/tcp`
3. Obtain TLS certificate (e.g. Certbot + standalone, or Caddy auto-HTTPS)
4. In `infra/nginx/nginx.conf`:
   - Add `listen 443 ssl;` server block with `ssl_certificate` paths
   - Change port-80 block to `return 301 https://$host$request_uri;`
5. Restart nginx: `docker compose ... restart nginx`
