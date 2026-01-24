#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR=${1:-xipher/desktop/out/build/ci}
OUT_DIR=${2:-xipher/desktop/out/artifacts/linux}

WORKSPACE="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_PATH="$WORKSPACE/$BUILD_DIR"
OUT_PATH="$WORKSPACE/$OUT_DIR"
STAGING="$OUT_PATH/xipher_desktop"

mkdir -p "$STAGING"

BIN_PATH=$(find "$BUILD_PATH" -name "xipher_desktop" -type f -maxdepth 5 | head -n 1 || true)
if [[ -z "$BIN_PATH" ]]; then
  echo "xipher_desktop binary not found in $BUILD_PATH" >&2
  exit 1
fi

cp "$BIN_PATH" "$STAGING/"

if command -v linuxdeployqt >/dev/null 2>&1; then
  linuxdeployqt "$BIN_PATH" -qmldir="$WORKSPACE/xipher/desktop/qml" -bundle-non-qt-libs -always-overwrite
fi

ZIP_PATH="$OUT_PATH/xipher_desktop_linux.zip"
rm -f "$ZIP_PATH"
(cd "$STAGING" && zip -r "$ZIP_PATH" .)

echo "Artifact: $ZIP_PATH"
