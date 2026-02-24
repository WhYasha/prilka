#!/usr/bin/env bash
# Build macOS DMG via Tauri
# Requires: Node.js, npm, Rust toolchain, Xcode CLI tools
# Output: frontend/src-tauri/target/release/bundle/dmg/SimpleMessenger_<ver>_aarch64.dmg
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

echo "==> Checking prerequisites..."
rustc --version
cargo --version
node --version
npm --version

echo "==> Installing npm dependencies..."
npm ci

echo "==> Building Vue frontend..."
npm run build

echo "==> Building Tauri (macOS DMG)..."
npx tauri build

VERSION=$(node -e "console.log(require('./src-tauri/tauri.conf.json').version)")
# Tauri outputs both aarch64 and x64 on macOS; find the built DMG
ARTIFACT=$(find src-tauri/target/release/bundle/dmg -name "*.dmg" 2>/dev/null | head -1)

if [ -n "$ARTIFACT" ] && [ -f "$ARTIFACT" ]; then
    mkdir -p dist-desktop/macos
    cp "$ARTIFACT" "dist-desktop/macos/Messenger.dmg"
    cp "$ARTIFACT" "dist-desktop/macos/Messenger-${VERSION}.dmg"

    # SHA256
    HASH=$(shasum -a 256 "dist-desktop/macos/Messenger.dmg" | awk '{print $1}')
    echo "$HASH  Messenger.dmg" > "dist-desktop/macos/Messenger.dmg.sha256"

    # latest.json
    cat > "dist-desktop/macos/latest.json" <<JSONEOF
{
  "version": "${VERSION}",
  "date": "$(date +%Y-%m-%d)",
  "url": "https://behappy.rest/downloads/macos/Messenger.dmg",
  "sha256": "${HASH}"
}
JSONEOF

    echo "==> Build complete!"
    echo "    DMG:    dist-desktop/macos/Messenger.dmg"
    echo "    SHA256: $HASH"
else
    echo "ERROR: Build failed — no DMG found in src-tauri/target/release/bundle/dmg/"
    exit 1
fi
