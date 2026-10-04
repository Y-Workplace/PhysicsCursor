#!/bin/bash
set -euo pipefail

ACTIVATE=false
for option in "$@"; do
    case "$option" in
        --activate) ACTIVATE=true ;;
        --build) ;; # compatibility: every installation now builds
        *) echo "Unknown option: $option" >&2; exit 2 ;;
    esac
done

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN_DIR="$HOME/.local/bin"
PLUGIN_DIR="$HOME/.local/share/hyprland/plugins"
HYPR_CONFIG_DIR="$HOME/.config/hypr"

echo "========================================================"
echo "    PhysicsCursor Installer (Hyprland / CachyOS)    "
echo "========================================================"

mkdir -p "$BIN_DIR"
mkdir -p "$PLUGIN_DIR"

# Build against the user's Hyprland headers; install dependencies automatically.
echo "[1/4] Building and testing for this Hyprland version..."
bash "$DIR/build.sh"
cp "$DIR/bin/physics_cursor" "$BIN_DIR/physics_cursor.new"
mv "$BIN_DIR/physics_cursor.new" "$BIN_DIR/physics_cursor"
cp "$DIR/bin/dynamic-cursors.so" "$PLUGIN_DIR/dynamic-cursors.so.new"
mv "$PLUGIN_DIR/dynamic-cursors.so.new" "$PLUGIN_DIR/dynamic-cursors.so"

chmod +x "$BIN_DIR/physics_cursor" 2>/dev/null || true

bash "$DIR/scripts/install_daemon_service.sh"

# 2. Configure Hyprland integration
echo "[2/4] Configuring Hyprland integration..."

if [ -f "$HYPR_CONFIG_DIR/hyprland.lua" ]; then
    mkdir -p "$HYPR_CONFIG_DIR/config"
    # Preserve user choices, including disabling cursor transitions.
    if [ ! -f "$HYPR_CONFIG_DIR/config/cursor_bridge.lua" ]; then
    cat << 'EOF' > "$HYPR_CONFIG_DIR/config/cursor_bridge.lua"
-- PhysicsCursor Bridge Plugin Configuration
-- Explicit startup opt-in: keep disabled until you choose to enable it.
local ENABLED = false
if not ENABLED then return end

local home = os.getenv("HOME")
local plugin_path = home .. "/.local/share/hyprland/plugins/dynamic-cursors.so"

hl.plugin.load(plugin_path)

hl.config({
    plugin = {
        dynamic_cursors = {
            enabled = true,
            mode = "tilt",
            threshold = 0,
            shake = { enabled = true, effects = true },
            transition = { enabled = false, duration = 250 },
        },
    },
})

-- Shape rules (optional):
-- To make the text (I-beam) cursor static for text selection, uncomment below:
-- if hl.plugin.dynamic_cursors and hl.plugin.dynamic_cursors.shape_rule then
--     hl.plugin.dynamic_cursors.shape_rule({ shape = "text", mode = "none" })
-- end
EOF
    fi

    if ! grep -q "config.cursor_bridge" "$HYPR_CONFIG_DIR/hyprland.lua"; then
        echo 'require("config.cursor_bridge")' >> "$HYPR_CONFIG_DIR/hyprland.lua"
    fi

elif $ACTIVATE && [ -f "$HYPR_CONFIG_DIR/hyprland.conf" ]; then
    if ! grep -q "dynamic-cursors.so" "$HYPR_CONFIG_DIR/hyprland.conf"; then
        cat << 'EOF' >> "$HYPR_CONFIG_DIR/hyprland.conf"

# PhysicsCursor Bridge
plugin = ~/.local/share/hyprland/plugins/dynamic-cursors.so
plugin:dynamic-cursors {
    enabled = true
    mode = tilt
    threshold = 0
    shake:enabled = true
    shake:effects = true
    transition:enabled = false
    transition:duration = 250
    # Optional: static text cursor
    # shaperule = text, none
}
exec-once = systemctl --user start physics-cursor.service
EOF
    fi
fi

if ! $ACTIVATE; then
    echo "[PhysicsCursor] Binaries updated. Existing configuration preserved."
    echo "[PhysicsCursor] The plugin and daemon were not loaded or restarted in this session."
    echo "[PhysicsCursor] Manual activation in this session: bash install.sh --activate"
    exit 0
fi

# 3. Start the physics daemon
echo "[3/4] Starting the background daemon..."
pkill -f '(^|/)physics_cursor --daemon$' 2>/dev/null || true
bash "$DIR/scripts/install_daemon_service.sh" --activate

# Explicit session activation does not reload the entire compositor config.
echo "[4/4] Activating the plugin in this session..."
if ! command -v hyprctl >/dev/null 2>&1 || ! hyprctl plugin list >/dev/null 2>&1; then
    echo "[PhysicsCursor] Cannot access Hyprland; the plugin remains disabled." >&2
    exit 1
fi
hyprctl plugin unload "$PLUGIN_DIR/dynamic-cursors.so" >/dev/null 2>&1 || true
hyprctl plugin load "$PLUGIN_DIR/dynamic-cursors.so"

echo ""
echo "========================================================"
echo "    PhysicsCursor installed and activated successfully!      "
echo "========================================================"
echo "Your system cursor now responds to physics and inertia."
echo "- Open the interactive playground: physics_cursor"
echo "- Uninstall: bash uninstall.sh"
echo "========================================================"
