# Build Windows installer via Tauri
# Requires: Node.js, npm, Rust toolchain
# Output: frontend/src-tauri/target/release/bundle/nsis/SimpleMessenger_<ver>_x64-setup.exe

$ErrorActionPreference = "Stop"

Write-Host "==> Checking prerequisites..."
rustc --version
cargo --version
node --version
npm --version

Write-Host "==> Installing npm dependencies..."
Set-Location $PSScriptRoot
npm ci

Write-Host "==> Building Vue frontend..."
npm run build

Write-Host "==> Building Tauri (Windows NSIS installer)..."
npx tauri build

$version = (Get-Content src-tauri/tauri.conf.json | ConvertFrom-Json).version
$artifact = "src-tauri/target/release/bundle/nsis/SimpleMessenger_${version}_x64-setup.exe"

if (Test-Path $artifact) {
    # Copy to dist
    New-Item -ItemType Directory -Force -Path "dist-desktop/windows" | Out-Null
    Copy-Item $artifact "dist-desktop/windows/MessengerSetup.exe"
    Copy-Item $artifact "dist-desktop/windows/MessengerSetup-${version}.exe"

    # Generate SHA256
    $hash = (Get-FileHash "dist-desktop/windows/MessengerSetup.exe" -Algorithm SHA256).Hash
    "$hash  MessengerSetup.exe" | Out-File -Encoding ascii "dist-desktop/windows/MessengerSetup.exe.sha256"

    # Generate latest.json
    @{
        version = $version
        date    = (Get-Date -Format "yyyy-MM-dd")
        url     = "https://behappy.rest/downloads/windows/MessengerSetup.exe"
        sha256  = $hash
    } | ConvertTo-Json | Out-File -Encoding utf8 "dist-desktop/windows/latest.json"

    Write-Host "==> Build complete!"
    Write-Host "    Installer: dist-desktop/windows/MessengerSetup.exe"
    Write-Host "    SHA256:    $hash"
} else {
    Write-Error "Build failed: artifact not found at $artifact"
    exit 1
}
