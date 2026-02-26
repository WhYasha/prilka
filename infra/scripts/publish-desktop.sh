#!/usr/bin/env bash
# Publish desktop installers to the production server
# Usage: bash infra/scripts/publish-desktop.sh [windows|macos|both]
#
# Expects artifacts in:
#   frontend/dist-desktop/windows/BeHappySetup.exe
#   frontend/dist-desktop/macos/BeHappy.dmg
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
DIST_DIR="$REPO_DIR/frontend/dist-desktop"

DEPLOY_HOST="behappy.rest"
DEPLOY_USER="deploy"
DEPLOY_PATH="/opt/messenger/repo/infra/nginx/downloads"

TARGET="${1:-both}"

publish_windows() {
    local dir="$DIST_DIR/windows"
    if [ ! -f "$dir/BeHappySetup.exe" ]; then
        echo "ERROR: $dir/BeHappySetup.exe not found. Build first with build_windows.ps1"
        return 1
    fi
    echo "==> Uploading Windows installer..."
    scp "$dir/BeHappySetup.exe" "$dir/BeHappySetup.exe.sha256" "$dir/latest.json" \
        "${DEPLOY_USER}@${DEPLOY_HOST}:${DEPLOY_PATH}/windows/"

    # Also upload versioned copy if it exists
    for f in "$dir"/BeHappySetup-*.exe; do
        [ -f "$f" ] && scp "$f" "${DEPLOY_USER}@${DEPLOY_HOST}:${DEPLOY_PATH}/windows/"
    done

    echo "==> Verifying Windows..."
    ssh "${DEPLOY_USER}@${DEPLOY_HOST}" "ls -lh ${DEPLOY_PATH}/windows/"
    echo "==> Windows done!"
}

publish_macos() {
    local dir="$DIST_DIR/macos"
    if [ ! -f "$dir/BeHappy.dmg" ]; then
        echo "ERROR: $dir/BeHappy.dmg not found. Build first with build_macos.sh"
        return 1
    fi
    echo "==> Uploading macOS installer..."
    scp "$dir/BeHappy.dmg" "$dir/BeHappy.dmg.sha256" "$dir/latest.json" \
        "${DEPLOY_USER}@${DEPLOY_HOST}:${DEPLOY_PATH}/macos/"

    for f in "$dir"/Messenger-*.dmg; do
        [ -f "$f" ] && scp "$f" "${DEPLOY_USER}@${DEPLOY_HOST}:${DEPLOY_PATH}/macos/"
    done

    echo "==> Verifying macOS..."
    ssh "${DEPLOY_USER}@${DEPLOY_HOST}" "ls -lh ${DEPLOY_PATH}/macos/"
    echo "==> macOS done!"
}

case "$TARGET" in
    windows) publish_windows ;;
    macos)   publish_macos ;;
    both)    publish_windows; publish_macos ;;
    *)       echo "Usage: $0 [windows|macos|both]"; exit 1 ;;
esac

echo ""
echo "Download URLs:"
echo "  Windows: https://behappy.rest/downloads/windows/BeHappySetup.exe"
echo "  macOS:   https://behappy.rest/downloads/macos/BeHappy.dmg"
