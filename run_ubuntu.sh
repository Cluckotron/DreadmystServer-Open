#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="$ROOT/Server/build"
BIN="$BUILD/DreadmystServer"
if [[ ! -x "$BIN" ]]; then
  echo "Server binary not found. Building first..."
  "$ROOT/build_ubuntu.sh"
fi
cd "$BUILD"
exec ./DreadmystServer
