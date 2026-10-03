#!/bin/bash
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$DIR"

# Encerra qualquer instância anterior
pkill -f "physics_cursor --daemon" 2>/dev/null
sleep 0.2

# Inicia o daemon desacoplado em segundo plano
setsid /lib64/ld-linux-x86-64.so.2 ./physics_cursor --daemon </dev/null >/tmp/physics_cursor_daemon.log 2>&1 &
PID=$!
sleep 0.5

if pgrep -f "physics_cursor --daemon" > /dev/null; then
    echo "[PhysicsCursor] Daemon de Física ATIVO em segundo plano (PID: $(pgrep -f "physics_cursor --daemon"))."
    echo "[PhysicsCursor] O cursor do sistema no Hyprland agora está conectado à física!"
    echo "Para parar quando quiser, execute: bash stop_daemon.sh"
else
    echo "[PhysicsCursor] Erro ao iniciar o daemon. Verifique /tmp/physics_cursor_daemon.log"
fi
