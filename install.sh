#!/bin/bash
set -e

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
    cat << 'EOF' > "$HYPR_CONFIG_DIR/config/cursor_bridge.lua"
-- PhysicsCursor Bridge Plugin Configuration
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
        },
    },
})

-- Shape rules (optional):
-- To make the text (I-beam) cursor static for text selection, uncomment below:
-- if hl.plugin.dynamic_cursors and hl.plugin.dynamic_cursors.shape_rule then
--     hl.plugin.dynamic_cursors.shape_rule({ shape = "text", mode = "none" })
-- end
EOF

    if ! grep -q "config.cursor_bridge" "$HYPR_CONFIG_DIR/hyprland.lua"; then
        echo 'require("config.cursor_bridge")' >> "$HYPR_CONFIG_DIR/hyprland.lua"
    fi

elif [ -f "$HYPR_CONFIG_DIR/hyprland.conf" ]; then
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
    # Optional: static text cursor
    # shaperule = text, none
}
exec-once = physics_cursor --daemon
EOF
    fi
fi

# 3. Iniciar o daemon de física
echo "[3/4] Iniciando daemon em segundo plano..."
pkill -f "physics_cursor --daemon" 2>/dev/null || true
setsid "$BIN_DIR/physics_cursor" --daemon </dev/null >/tmp/physics_cursor_daemon.log 2>&1 &

# 4. Recarregar o Hyprland
echo "[4/4] Recarregando compositor Hyprland..."
if command -v hyprctl >/dev/null 2>&1; then
    # Replacing the file alone leaves the previous plugin loaded in memory.
    # Unload by its registered path, then let config reload load the new inode.
    if hyprctl plugin list >/dev/null 2>&1; then
        hyprctl plugin unload "$PLUGIN_DIR/dynamic-cursors.so" >/dev/null 2>&1 || true
        hyprctl reload
        if ! hyprctl plugin list | grep -q 'dynamic-cursors'; then
            hyprctl plugin load "$PLUGIN_DIR/dynamic-cursors.so"
        fi
    else
        echo "[PhysicsCursor] Sem acesso a sessao do Hyprland. Reinicie a sessao para carregar o plugin atualizado."
    fi
fi

echo ""
echo "========================================================"
echo "    PhysicsCursor instalado e ativado com sucesso!      "
echo "========================================================"
echo "O cursor do seu sistema agora responde a fisica e inercia."
echo "- Para abrir o painel interativo: physics_cursor"
echo "- Para desinstalar quando quiser: bash uninstall.sh"
echo "========================================================"
