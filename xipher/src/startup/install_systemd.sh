#!/bin/bash
set -euo pipefail

# Install Xipher server as a systemd service + logrotate config.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# This repo keeps startup scripts under: <repo>/src/startup
# So repo root is usually two levels above SCRIPT_DIR. We still detect it robustly.
PROJECT_DIR="$SCRIPT_DIR"
for _ in 1 2 3 4; do
  PROJECT_DIR="$(dirname "$PROJECT_DIR")"
  if [ -f "$PROJECT_DIR/CMakeLists.txt" ] && [ -d "$PROJECT_DIR/installer" ]; then
    break
  fi
done
if [ ! -f "$PROJECT_DIR/CMakeLists.txt" ]; then
  echo "ERROR: could not locate repo root from SCRIPT_DIR=$SCRIPT_DIR" >&2
  exit 1
fi

UNIT_SRC="$PROJECT_DIR/installer/systemd/xipher-server.service"
ENV_SRC="$PROJECT_DIR/installer/systemd/xipher.env.example"
LOGROTATE_SRC="$PROJECT_DIR/installer/logrotate/xipher-server"

UNIT_DST="/etc/systemd/system/xipher-server.service"
ENV_DIR="/etc/xipher"
ENV_DST="$ENV_DIR/xipher.env"
LOGROTATE_DST="/etc/logrotate.d/xipher-server"

if [ ! -f "$UNIT_SRC" ]; then
  echo "ERROR: unit file not found: $UNIT_SRC" >&2
  exit 1
fi

echo "[1/4] Installing systemd unit -> $UNIT_DST"
sudo cp "$UNIT_SRC" "$UNIT_DST"

echo "[2/4] Installing env file -> $ENV_DST (created only if missing)"
sudo mkdir -p "$ENV_DIR"
if [ ! -f "$ENV_DST" ]; then
  sudo cp "$ENV_SRC" "$ENV_DST"
  echo "Created $ENV_DST (edit it for DB credentials / port)"
else
  echo "Env file already exists: $ENV_DST (leaving as-is)"
fi

echo "[3/4] Installing logrotate config -> $LOGROTATE_DST"
if [ -f "$LOGROTATE_SRC" ]; then
  sudo cp "$LOGROTATE_SRC" "$LOGROTATE_DST"
else
  echo "WARN: logrotate template not found: $LOGROTATE_SRC (skipping)" >&2
fi

echo "[4/4] Enabling + starting service"
sudo systemctl daemon-reload
sudo systemctl enable --now xipher-server.service
sudo systemctl status xipher-server.service --no-pager


