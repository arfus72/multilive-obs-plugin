#!/usr/bin/env bash
set -euo pipefail

PLUGIN_NAME="multilive-signature-guard"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PAYLOAD_DIR="$SCRIPT_DIR/payload"
TARGET_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/obs-studio/plugins/$PLUGIN_NAME"

if pgrep -x obs >/dev/null 2>&1 || pgrep -x obs64 >/dev/null 2>&1; then
  echo "[Multi Live] Ferme OBS Studio avant l'installation."
  exit 1
fi

SOURCE_SO="$(find "$PAYLOAD_DIR" -type f -name "$PLUGIN_NAME.so" | head -n 1 || true)"
if [[ -z "$SOURCE_SO" || ! -f "$SOURCE_SO" ]]; then
  echo "[Multi Live] Plugin Linux introuvable dans payload/."
  echo "[Multi Live] Compile d'abord le paquet Ubuntu, puis place $PLUGIN_NAME.so dans payload/."
  exit 1
fi

echo "[Multi Live] Installation utilisateur OBS: $TARGET_DIR"
mkdir -p "$TARGET_DIR/bin/64bit" "$TARGET_DIR/data"
cp "$SOURCE_SO" "$TARGET_DIR/bin/64bit/$PLUGIN_NAME.so"

if [[ -d "$PAYLOAD_DIR/data" ]]; then
  cp -R "$PAYLOAD_DIR/data/." "$TARGET_DIR/data/"
elif [[ -d "$PAYLOAD_DIR/share/obs/obs-plugins/$PLUGIN_NAME" ]]; then
  cp -R "$PAYLOAD_DIR/share/obs/obs-plugins/$PLUGIN_NAME/." "$TARGET_DIR/data/"
fi

{
  echo "Multi Live OBS Plugin - Linux install report"
  echo "Date: $(date -Is)"
  echo "Target: $TARGET_DIR"
  echo
  echo "SHA256:"
  find "$TARGET_DIR" -type f -print0 | sort -z | xargs -0 sha256sum
} > "$SCRIPT_DIR/install-report-linux.txt"

echo "[Multi Live] Installation terminee. Redemarre OBS Studio."
echo "[Multi Live] Rapport: $SCRIPT_DIR/install-report-linux.txt"
