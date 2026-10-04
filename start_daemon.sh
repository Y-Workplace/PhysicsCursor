#!/usr/bin/env bash
set -euo pipefail
DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$DIR"
if [[ ! -x ./physics_cursor ]]; then
    echo 'Compile first: bash build.sh --daemon-only' >&2
    exit 1
fi
# Match daemon arguments only; preserve the running playground.
pkill -f '(^|/)physics_cursor --daemon$' 2>/dev/null || true
sleep 0.2
setsid /lib64/ld-linux-x86-64.so.2 ./physics_cursor --daemon </dev/null >/tmp/physics_cursor_daemon.log 2>&1 &
sleep 0.5
if pgrep -f '(^|/)physics_cursor --daemon$' >/dev/null; then
    echo '[PhysicsCursor] Daemon running with the compiled parameters.'
else
    echo '[PhysicsCursor] Failed to start. See /tmp/physics_cursor_daemon.log' >&2
    exit 1
fi
