#!/usr/bin/env bash
# Build Vue frontend and copy to backend/www/ for Drogon to serve
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
FRONTEND_DIR="$SCRIPT_DIR"
BACKEND_WWW="$SCRIPT_DIR/../backend/www"

echo "==> Building Vue frontend..."
cd "$FRONTEND_DIR"
npm run build

echo "==> Cleaning old assets in backend/www/..."
# Remove old Vite output (assets/) and index.html but keep admin/ for now
rm -rf "$BACKEND_WWW/assets"
rm -f "$BACKEND_WWW/index.html"

echo "==> Copying dist/ to backend/www/..."
cp -r "$FRONTEND_DIR/dist/"* "$BACKEND_WWW/"

echo "==> Done! Files deployed to $BACKEND_WWW"
ls -la "$BACKEND_WWW/"
