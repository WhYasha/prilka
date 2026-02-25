#!/usr/bin/env bash
# ============================================================
# 01-bootstrap.sh — run as root on a fresh Debian 12 server
# Usage: bash 01-bootstrap.sh
# ============================================================
set -euo pipefail

DEPLOY_USER="deploy"
APP_NAME="messenger"
APP_DIR="/opt/${APP_NAME}"
REPO_URL="https://github.com/WhYasha/prilka.git"

DEPLOY_PUBKEY="ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAINKjEHo5jBTaj77l16LJ7xLqUtP6mVxsDiTHYoDOJ3fB deploy@your_project_name"

log() { echo "[$(date +%T)] $*"; }

# ── 1. System update ───────────────────────────────────────────────────────────
log "Updating system packages..."
apt-get update -qq
DEBIAN_FRONTEND=noninteractive apt-get upgrade -y -qq
apt-get install -y --no-install-recommends \
    git curl ca-certificates gnupg ufw fail2ban \
    unattended-upgrades apt-listchanges

# ── 2. Unattended security updates ────────────────────────────────────────────
log "Enabling unattended security updates..."
cat > /etc/apt/apt.conf.d/50unattended-upgrades-custom <<'EOF'
Unattended-Upgrade::Allowed-Origins {
    "${distro_id}:${distro_codename}-security";
};
Unattended-Upgrade::AutoFixInterruptedDpkg "true";
Unattended-Upgrade::Remove-Unused-Dependencies "true";
Unattended-Upgrade::Automatic-Reboot "false";
EOF
cat > /etc/apt/apt.conf.d/20auto-upgrades <<'EOF'
APT::Periodic::Update-Package-Lists "1";
APT::Periodic::Unattended-Upgrade "1";
EOF
systemctl enable unattended-upgrades

# ── 3. Install Docker (official repo) ─────────────────────────────────────────
log "Installing Docker..."
install -m 0755 -d /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/debian/gpg \
    | gpg --dearmor -o /etc/apt/keyrings/docker.gpg
chmod a+r /etc/apt/keyrings/docker.gpg

echo "deb [arch=$(dpkg --print-architecture) \
    signed-by=/etc/apt/keyrings/docker.gpg] \
    https://download.docker.com/linux/debian \
    $(. /etc/os-release && echo "$VERSION_CODENAME") stable" \
    > /etc/apt/sources.list.d/docker.list

apt-get update -qq
apt-get install -y docker-ce docker-ce-cli containerd.io \
    docker-buildx-plugin docker-compose-plugin

systemctl enable docker
systemctl start docker
log "Docker version: $(docker --version)"

# ── 3b. Install HashiCorp tools (Vault, Consul, Nomad CLI) ───────────────────
log "Installing HashiCorp repository..."
curl -fsSL https://apt.releases.hashicorp.com/gpg \
    | gpg --dearmor -o /usr/share/keyrings/hashicorp-archive-keyring.gpg
echo "deb [signed-by=/usr/share/keyrings/hashicorp-archive-keyring.gpg] \
    https://apt.releases.hashicorp.com $(lsb_release -cs) main" \
    > /etc/apt/sources.list.d/hashicorp.list
apt-get update -qq
apt-get install -y vault consul nomad
log "Vault version:  $(vault --version)"
log "Consul version: $(consul --version)"
log "Nomad version:  $(nomad --version)"

# ── 3c. Install certbot for TLS certificates ─────────────────────────────────
log "Installing certbot..."
apt-get install -y certbot
log "Certbot version: $(certbot --version 2>&1)"

# ── 4. Create deploy user ──────────────────────────────────────────────────────
log "Creating user '${DEPLOY_USER}'..."
if id "${DEPLOY_USER}" &>/dev/null; then
    log "  User already exists, skipping."
else
    adduser --disabled-password --gecos "" "${DEPLOY_USER}"
fi
usermod -aG sudo,docker "${DEPLOY_USER}"

# ── 5. SSH key for deploy user ─────────────────────────────────────────────────
log "Setting up SSH keys for ${DEPLOY_USER}..."
DEPLOY_HOME="/home/${DEPLOY_USER}"
SSH_DIR="${DEPLOY_HOME}/.ssh"
mkdir -p "${SSH_DIR}"
echo "${DEPLOY_PUBKEY}" > "${SSH_DIR}/authorized_keys"
chmod 700 "${SSH_DIR}"
chmod 600 "${SSH_DIR}/authorized_keys"
chown -R "${DEPLOY_USER}:${DEPLOY_USER}" "${SSH_DIR}"

# ── 6. Harden SSH ─────────────────────────────────────────────────────────────
log "Hardening SSH config..."
SSHD_CONF="/etc/ssh/sshd_config"

# Back up original
cp -n "${SSHD_CONF}" "${SSHD_CONF}.orig"

# Apply settings idempotently
set_sshd() {
    local key="$1" val="$2"
    if grep -qE "^#?${key}" "${SSHD_CONF}"; then
        sed -i "s|^#\?${key}.*|${key} ${val}|" "${SSHD_CONF}"
    else
        echo "${key} ${val}" >> "${SSHD_CONF}"
    fi
}

set_sshd PasswordAuthentication no
set_sshd PermitRootLogin no
set_sshd PubkeyAuthentication yes
set_sshd AuthorizedKeysFile ".ssh/authorized_keys"
set_sshd X11Forwarding no
set_sshd AllowTcpForwarding no
set_sshd MaxAuthTries 3

log "Validating sshd config..."
sshd -t   # exits non-zero if config is invalid; set -e will abort

log "Reloading sshd..."
systemctl reload sshd

# ── 7. UFW firewall ────────────────────────────────────────────────────────────
log "Configuring UFW..."
ufw --force reset
ufw default deny incoming
ufw default allow outgoing
ufw allow 22/tcp   comment 'SSH'
ufw allow 80/tcp   comment 'HTTP'
ufw allow 443/tcp  comment 'HTTPS'
ufw --force enable
ufw status verbose

# ── 8. Fail2ban ────────────────────────────────────────────────────────────────
log "Configuring fail2ban..."
cat > /etc/fail2ban/jail.d/sshd.local <<'EOF'
[sshd]
enabled  = true
port     = ssh
maxretry = 5
bantime  = 1h
findtime = 10m
EOF
systemctl enable fail2ban
systemctl restart fail2ban

# ── 9. Application directory layout ───────────────────────────────────────────
log "Creating application directories..."
mkdir -p "${APP_DIR}"/{repo,env,data,backups,scripts}
chown -R "${DEPLOY_USER}:${DEPLOY_USER}" "${APP_DIR}"

# ── 10. Clone repo ─────────────────────────────────────────────────────────────
log "Cloning repository..."
if [ -d "${APP_DIR}/repo/.git" ]; then
    log "  Repo already cloned, pulling latest..."
    sudo -u "${DEPLOY_USER}" git -C "${APP_DIR}/repo" pull
else
    sudo -u "${DEPLOY_USER}" git clone "${REPO_URL}" "${APP_DIR}/repo"
fi

# ── 11. .env ──────────────────────────────────────────────────────────────────
if [ ! -f "${APP_DIR}/env/.env" ]; then
    log "Creating .env from .env.example – EDIT THIS FILE before starting the stack!"
    cp "${APP_DIR}/repo/.env.example" "${APP_DIR}/env/.env"
    chown "${DEPLOY_USER}:${DEPLOY_USER}" "${APP_DIR}/env/.env"
    chmod 600 "${APP_DIR}/env/.env"
    log "  >>> IMPORTANT: Edit ${APP_DIR}/env/.env with real secrets <<<"
fi

# ── 12. Copy scripts ───────────────────────────────────────────────────────────
cp "${APP_DIR}/repo/infra/scripts/backup-postgres.sh" "${APP_DIR}/scripts/"
chmod +x "${APP_DIR}/scripts/backup-postgres.sh"
chown "${DEPLOY_USER}:${DEPLOY_USER}" "${APP_DIR}/scripts/backup-postgres.sh"

# ── 13. Install systemd units ─────────────────────────────────────────────────
log "Installing systemd units..."
cp "${APP_DIR}/repo/infra/systemd/messenger.service"         /etc/systemd/system/
cp "${APP_DIR}/repo/infra/systemd/messenger-backup.service"  /etc/systemd/system/
cp "${APP_DIR}/repo/infra/systemd/messenger-backup.timer"    /etc/systemd/system/

systemctl daemon-reload
systemctl enable messenger.service
systemctl enable messenger-backup.timer
systemctl start  messenger-backup.timer

log ""
log "============================================================"
log "Bootstrap complete."
log ""
log "NEXT STEPS:"
log "  1. Edit ${APP_DIR}/env/.env with real secrets"
log "  2. Verify deploy user SSH login from another terminal:"
log "       ssh deploy@<SERVER_IP>"
log "  3. Create DNS A records for subdomains:"
log "       api.behappy.rest, ws.behappy.rest, grafana.behappy.rest,"
log "       minio-console.behappy.rest, downloads.behappy.rest"
log "  4. Obtain wildcard TLS certificate:"
log "       certbot certonly --manual --preferred-challenges dns"
log "         -d behappy.rest -d '*.behappy.rest'"
log "  5. Initialize Vault (one-time):"
log "       docker compose up -d vault"
log "       bash infra/vault/init.sh"
log "  6. Start the stack:"
log "       systemctl start messenger"
log "  7. Check status:"
log "       systemctl status messenger"
log "       docker ps"
log "       curl https://api.behappy.rest/health"
log "============================================================"
