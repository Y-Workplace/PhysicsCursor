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
echo "    Instalador do PhysicsCursor (Hyprland / CachyOS)    "
echo "========================================================"

mkdir -p "$BIN_DIR"
mkdir -p "$PLUGIN_DIR"

# Build against the user's Hyprland headers; install dependencies automatically.
echo "[1/4] Compilando e testando para esta versao do Hyprland..."
bash "$DIR/build.sh"
cp "$DIR/bin/physics_cursor" "$BIN_DIR/physics_cursor.new"
mv "$BIN_DIR/physics_cursor.new" "$BIN_DIR/physics_cursor"
cp "$DIR/bin/dynamic-cursors.so" "$PLUGIN_DIR/dynamic-cursors.so.new"
mv "$PLUGIN_DIR/dynamic-cursors.so.new" "$PLUGIN_DIR/dynamic-cursors.so"

chmod +x "$BIN_DIR/physics_cursor" 2>/dev/null || true

# 2. Configurar integracao com o Hyprland
echo "[2/4] Configurando integracao com o Hyprland..."

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
exec-once = physics_cursor --daemon
EOF
    fi
fi

if ! $ACTIVATE; then
    echo "[PhysicsCursor] Binarios atualizados. Configuracao existente preservada."
    echo "[PhysicsCursor] Plugin e daemon nao foram carregados/reiniciados nesta sessao."
    echo "[PhysicsCursor] Ativacao manual nesta sessao: bash install.sh --activate"
    exit 0
fi

# 3. Iniciar o daemon de física
echo "[3/4] Iniciando daemon em segundo plano..."
pkill -f "physics_cursor --daemon" 2>/dev/null || true
setsid "$BIN_DIR/physics_cursor" --daemon </dev/null >/tmp/physics_cursor_daemon.log 2>&1 &

# Explicit session activation does not reload the entire compositor config.
echo "[4/4] Ativando plugin nesta sessao..."
if ! command -v hyprctl >/dev/null 2>&1 || ! hyprctl plugin list >/dev/null 2>&1; then
    echo "[PhysicsCursor] Sem acesso ao Hyprland; plugin permanece desativado." >&2
    exit 1
fi
hyprctl plugin unload "$PLUGIN_DIR/dynamic-cursors.so" >/dev/null 2>&1 || true
hyprctl plugin load "$PLUGIN_DIR/dynamic-cursors.so"

echo ""
echo "========================================================"
echo "    PhysicsCursor instalado e ativado com sucesso!      "
echo "========================================================"
echo "O cursor do seu sistema agora responde a fisica e inercia."
echo "- Para abrir o painel interativo: physics_cursor"
echo "- Para desinstalar quando quiser: bash uninstall.sh"
echo "========================================================"
