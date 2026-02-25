#!/usr/bin/env bash
# ============================================================
# inject-certs.sh — Inject TLS cert content into APISIX config
# Called by 02-deploy.sh before starting the stack.
# Generates apisix-ssl.yaml with cert content, then combines
# with apisix.yaml template to produce the final config.
# ============================================================
set -euo pipefail

CERT_DIR="/etc/letsencrypt/live/behappy.rest"
REPO_DIR="${1:-.}"
APISIX_DIR="${REPO_DIR}/infra/apisix"
OUTPUT="${APISIX_DIR}/apisix.yaml"
TEMPLATE="${APISIX_DIR}/apisix.yaml.tpl"

# If no template exists yet, the current apisix.yaml IS the template
if [ ! -f "${TEMPLATE}" ]; then
    cp "${OUTPUT}" "${TEMPLATE}"
fi

if ! sudo test -f "${CERT_DIR}/fullchain.pem"; then
    echo "[inject-certs] No TLS certs found at ${CERT_DIR}, using template as-is."
    cp "${TEMPLATE}" "${OUTPUT}"
    exit 0
fi

echo "[inject-certs] Reading TLS certificates..."
CERT=$(sudo cat "${CERT_DIR}/fullchain.pem")
KEY=$(sudo cat "${CERT_DIR}/privkey.pem")

# Build the ssl section with indented PEM content
{
    echo "ssls:"
    echo "  - id: wildcard_behappy"
    echo "    snis:"
    echo '      - "behappy.rest"'
    echo '      - "*.behappy.rest"'
    echo "    cert: |"
    echo "${CERT}" | sed 's/^/      /'
    echo "    key: |"
    echo "${KEY}" | sed 's/^/      /'
    echo ""
} > "${APISIX_DIR}/apisix-ssl.yaml"

# Combine: ssl block + template (which has routes/upstreams)
cat "${APISIX_DIR}/apisix-ssl.yaml" "${TEMPLATE}" > "${OUTPUT}"

echo "[inject-certs] TLS certificates injected into ${OUTPUT}"
