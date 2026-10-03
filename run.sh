#!/usr/bin/env bash
set -euo pipefail
DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
bash "$DIR/build.sh" --daemon-only
cd "$DIR"
exec /lib64/ld-linux-x86-64.so.2 "$DIR/physics_cursor" "$@"
