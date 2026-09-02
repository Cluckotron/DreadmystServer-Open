#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEST="$ROOT/game"

if [[ -f "$DEST/game.db" && -d "$DEST/maps" ]]; then
  echo "[Dreadmyst] Game data already present: $DEST"
  exit 0
fi

if [[ -n "${DREADMYST_RUNTIME_DIR:-}" ]]; then
  SOURCE="$(cd "$DREADMYST_RUNTIME_DIR" && pwd)"
  if [[ ! -f "$SOURCE/game.db" || ! -d "$SOURCE/maps" ]]; then
    echo "DREADMYST_RUNTIME_DIR must contain game.db and maps/." >&2
    exit 1
  fi
  mkdir -p "$DEST"
  cp "$SOURCE/game.db" "$DEST/game.db"
  cp -a "$SOURCE/maps" "$DEST/maps"
  echo "[Dreadmyst] Copied game.db/maps from DREADMYST_RUNTIME_DIR."
  exit 0
fi

CACHE="$ROOT/.runtime_source"
if [[ ! -d "$CACHE/.git" ]]; then
  rm -rf "$CACHE"
  echo "[Dreadmyst] Fetching the public Dreadmyst runtime from GitHub..."
  git clone --depth 1 https://github.com/DreadmystRPG/steam.git "$CACHE"
else
  echo "[Dreadmyst] Updating cached public runtime..."
  git -C "$CACHE" pull --ff-only
fi

if [[ ! -f "$CACHE/game.db" || ! -d "$CACHE/maps" ]]; then
  echo "The fetched runtime did not contain game.db and maps/." >&2
  exit 1
fi
mkdir -p "$DEST"
cp "$CACHE/game.db" "$DEST/game.db"
cp -a "$CACHE/maps" "$DEST/maps"
echo "[Dreadmyst] Game data ready: $DEST"
