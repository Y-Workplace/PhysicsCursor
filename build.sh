#!/usr/bin/env bash
set -euo pipefail
DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
daemon_only=false
apply=false
for option in "$@"; do
    case "$option" in
        --daemon-only) daemon_only=true ;;
        --apply) apply=true ;;
        *) echo "Unknown option: $option" >&2; exit 2 ;;
    esac
done
if $daemon_only; then
    bash "$DIR/scripts/bootstrap.sh" --daemon-only
else
    bash "$DIR/scripts/bootstrap.sh"
fi
make -C "$DIR" -j"${JOBS:-2}"
make -C "$DIR" test
mkdir -p "$DIR/bin"
cp "$DIR/physics_cursor" "$DIR/bin/physics_cursor.new"
mv "$DIR/bin/physics_cursor.new" "$DIR/bin/physics_cursor"
if ! $daemon_only; then
    make -C "$DIR/plugin" -j"${JOBS:-2}"
    cp "$DIR/plugin/out/dynamic-cursors.so" "$DIR/bin/dynamic-cursors.so"
    strip --strip-debug "$DIR/bin/dynamic-cursors.so"
    pkg-config --modversion hyprland > "$DIR/bin/hyprland-version.txt"
fi
if $apply; then
    # Preserve these compiled defaults for the next login as well.
    if [[ -f "$HOME/.local/bin/physics_cursor" ]]; then
        cp "$DIR/physics_cursor" "$HOME/.local/bin/physics_cursor.new"
        mv "$HOME/.local/bin/physics_cursor.new" "$HOME/.local/bin/physics_cursor"
    fi
    bash "$DIR/start_daemon.sh"
fi
