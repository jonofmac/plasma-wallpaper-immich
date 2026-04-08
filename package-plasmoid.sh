#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PACKAGE_DIR="$ROOT_DIR/package"
OUTPUT_DIR="${OUTPUT_DIR:-$ROOT_DIR/dist}"
METADATA_FILE="$PACKAGE_DIR/metadata.json"

if ! command -v zip >/dev/null 2>&1; then
  echo "Missing dependency: zip" >&2
  echo "Install zip and run this script again." >&2
  exit 1
fi

if [[ ! -f "$METADATA_FILE" ]]; then
  echo "metadata.json not found at: $METADATA_FILE" >&2
  exit 1
fi

PLUGIN_ID="$(python - <<'PY' "$METADATA_FILE"
import json, sys
with open(sys.argv[1], "r", encoding="utf-8") as fh:
    data = json.load(fh)
print(data.get("KPlugin", {}).get("Id", "plasma-wallpaper-immich"))
PY
)"

VERSION="$(python - <<'PY' "$METADATA_FILE"
import json, sys
with open(sys.argv[1], "r", encoding="utf-8") as fh:
    data = json.load(fh)
print(data.get("KPlugin", {}).get("Version", "0.1.0"))
PY
)"

mkdir -p "$OUTPUT_DIR"
ARCHIVE_NAME="${PLUGIN_ID}-${VERSION}.plasmoid"
ARCHIVE_PATH="$OUTPUT_DIR/$ARCHIVE_NAME"

echo "==> Packaging $PLUGIN_ID ($VERSION)"
(
  cd "$PACKAGE_DIR"
  zip -r "$ARCHIVE_PATH" . \
    -x "*.qmlc" \
    -x "*.jsc" \
    -x "*.pyc" \
    -x "__pycache__/*" \
    -x ".DS_Store"
)

echo "Created: $ARCHIVE_PATH"
