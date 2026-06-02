#!/bin/zsh
set -euo pipefail

PLUGIN_NAME="multilive-signature-guard"
SCRIPT_DIR="${0:A:h}"
PAYLOAD_DIR="$SCRIPT_DIR/payload"
TARGET_DIR="$HOME/Library/Application Support/obs-studio/plugins"
SOURCE_PLUGIN="$PAYLOAD_DIR/$PLUGIN_NAME.plugin"

if pgrep -x OBS >/dev/null 2>&1; then
  echo "[Multi Live] Ferme OBS Studio avant l'installation."
  exit 1
fi

if [[ ! -d "$SOURCE_PLUGIN" ]]; then
  echo "[Multi Live] Bundle macOS introuvable: $SOURCE_PLUGIN"
  echo "[Multi Live] Compile d'abord le paquet macOS, puis place $PLUGIN_NAME.plugin dans payload/."
  exit 1
fi

echo "[Multi Live] Installation utilisateur OBS: $TARGET_DIR"
mkdir -p "$TARGET_DIR"
rm -rf "$TARGET_DIR/$PLUGIN_NAME.plugin"
cp -R "$SOURCE_PLUGIN" "$TARGET_DIR/$PLUGIN_NAME.plugin"

{
  echo "Multi Live OBS Plugin - macOS install report"
  echo "Date: $(date -Iseconds)"
  echo "Target: $TARGET_DIR/$PLUGIN_NAME.plugin"
  echo
  echo "SHA256:"
  find "$TARGET_DIR/$PLUGIN_NAME.plugin" -type f -print0 | sort -z | xargs -0 shasum -a 256
  echo
  echo "Code signature:"
  codesign --verify --verbose=2 "$TARGET_DIR/$PLUGIN_NAME.plugin" 2>&1 || true
} > "$SCRIPT_DIR/install-report-macos.txt"

echo "[Multi Live] Installation terminee. Redemarre OBS Studio."
echo "[Multi Live] Rapport: $SCRIPT_DIR/install-report-macos.txt"
read "reply?Appuie sur Entree pour fermer."
