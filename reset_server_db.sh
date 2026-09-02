#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DB="$ROOT/Server/build/data/server.db"
SRCDB="$ROOT/Server/data/server.db"
rm -f "$DB" "$SRCDB"
echo "Local account/character database removed. It will be recreated on next server start."
