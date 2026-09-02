#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GAME_DB="$ROOT/game/game.db"
MAP_DIR="$ROOT/game/maps"
CHECKSUMS="$ROOT/GAME_DATA_SHA256.txt"

fail() {
  echo "[Dreadmyst] $*" >&2
  exit 1
}

[[ -f "$GAME_DB" ]] || fail "Missing game/game.db. This self-contained repository should include it. Restore the file from the repository/release archive."
[[ -d "$MAP_DIR" ]] || fail "Missing game/maps/. This self-contained repository should include the map set. Restore it from the repository/release archive."

map_count=$(find "$MAP_DIR" -maxdepth 1 -type f -name '*.map' | wc -l | tr -d ' ')
[[ "$map_count" -gt 0 ]] || fail "game/maps/ exists but contains no .map files."

echo "[Dreadmyst] Included game data found:"
echo "  game.db: $GAME_DB"
echo "  maps:    $map_count map files"

if command -v sha256sum >/dev/null 2>&1 && [[ -f "$CHECKSUMS" ]]; then
  echo "[Dreadmyst] Verifying bundled game-data checksums..."
  (cd "$ROOT" && sha256sum -c GAME_DATA_SHA256.txt)
else
  echo "[Dreadmyst] sha256sum/checksum manifest unavailable; presence check only."
fi

echo "[Dreadmyst] Game data is ready. No external Dreadmyst repository is required."
