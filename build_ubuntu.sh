#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="$ROOT/Server/build"
if [[ ! -f "$ROOT/game/game.db" || ! -d "$ROOT/game/maps" ]]; then
  "$ROOT/setup_game_data.sh"
fi
echo "[Dreadmyst] Configuring Release build..."
cmake -S "$ROOT/Server" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release
echo "[Dreadmyst] Building..."
cmake --build "$BUILD" -j"$(nproc)"
echo
echo "Build complete: $BUILD/DreadmystServer"
echo "Run: ./run_ubuntu.sh"
