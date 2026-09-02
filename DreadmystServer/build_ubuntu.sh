#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="$ROOT/Server/build"

# Invoke through bash so a repo initially committed from Windows does not rely
# on the Unix executable bit being preserved for helper scripts.
bash "$ROOT/setup_game_data.sh"

echo "[Dreadmyst] Configuring Release build..."
cmake -S "$ROOT/Server" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release
echo "[Dreadmyst] Building..."
cmake --build "$BUILD" -j"$(nproc)"
echo
echo "Build complete: $BUILD/DreadmystServer"
echo "Run: bash run_ubuntu.sh"
