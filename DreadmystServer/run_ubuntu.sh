#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="$ROOT/Server/build"
BIN="$BUILD/DreadmystServer"

# Fail early if the preserved database/maps are missing or were mixed with
# another data revision. This check is completely local and performs no fetch.
bash "$ROOT/setup_game_data.sh"

if [[ ! -x "$BIN" ]]; then
  echo "Server binary not found. Building first..."
  bash "$ROOT/build_ubuntu.sh"
fi

cd "$BUILD"
exec ./DreadmystServer
