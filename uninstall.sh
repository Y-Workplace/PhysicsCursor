#!/bin/bash
echo "Removendo PhysicsCursor..."

pkill -f "physics_cursor" 2>/dev/null || true
rm -f "$HOME/.local/bin/physics_cursor"
rm -f "$HOME/.local/share/hyprland/plugins/dynamic-cursors.so"
rm -f "$HOME/.config/hypr/config/cursor_bridge.lua"
rm -f /dev/shm/physics_cursor_bridge_shm

if [ -f "$HOME/.config/hypr/hyprland.lua" ]; then
    sed -i '/config.cursor_bridge/d' "$HOME/.config/hypr/hyprland.lua"
fi

if [ -f "$HOME/.config/hypr/hyprland.conf" ]; then
    sed -i '/dynamic-cursors/d' "$HOME/.config/hypr/hyprland.conf"
    sed -i '/physics_cursor/d' "$HOME/.config/hypr/hyprland.conf"
fi

if command -v hyprctl >/dev/null 2>&1; then
    hyprctl reload >/dev/null 2>&1 || true
fi

echo "PhysicsCursor desinstalado com sucesso. O cursor retornou ao padrao."
