#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR=${1:-xipher/desktop/out/build/ci}
OUT_DIR=${2:-xipher/desktop/out/artifacts/macos}

WORKSPACE="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_PATH="$WORKSPACE/$BUILD_DIR"
OUT_PATH="$WORKSPACE/$OUT_DIR"
STAGING="$OUT_PATH/xipher_desktop"

mkdir -p "$STAGING"

APP_PATH=$(find "$BUILD_PATH" -name "xipher_desktop.app" -maxdepth 5 | head -n 1 || true)
if [[ -z "$APP_PATH" ]]; then
  echo "xipher_desktop.app not found in $BUILD_PATH" >&2
  exit 1
fi

cp -R "$APP_PATH" "$STAGING/"

if command -v macdeployqt >/dev/null 2>&1; then
  macdeployqt "$STAGING/$(basename "$APP_PATH")"
fi

ZIP_PATH="$OUT_PATH/xipher_desktop_macos.zip"
rm -f "$ZIP_PATH"
(cd "$STAGING" && zip -r "$ZIP_PATH" .)

echo "Artifact: $ZIP_PATH"
