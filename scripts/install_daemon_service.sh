#!/usr/bin/env bash
set -euo pipefail
DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
activate=false
case "${1:-}" in
    --activate) activate=true ;;
    "") ;;
    *) echo "Unknown option: $1" >&2; exit 2 ;;
esac
UNIT_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user"
mkdir -p "$UNIT_DIR"
cp "$DIR/systemd/physics-cursor.service" "$UNIT_DIR/physics-cursor.service"
if command -v systemctl >/dev/null && systemctl --user daemon-reload; then
    if $activate; then
        systemctl --user enable physics-cursor.service
        systemctl --user restart physics-cursor.service
    fi
elif $activate; then
    echo 'Cannot activate the daemon: user systemd manager unavailable.' >&2
    exit 1
else
    echo 'Daemon service saved; enable it from a session with a user systemd manager.'
fi
