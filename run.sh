#!/bin/bash
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$DIR"

if [ ! -f "physics_cursor" ]; then
    echo "[PhysicsCursor] Compilando executável..."
    make
fi

exec /lib64/ld-linux-x86-64.so.2 ./physics_cursor "$@"
