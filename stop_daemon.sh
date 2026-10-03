#!/bin/bash
pkill -f "physics_cursor --daemon"
rm -f /dev/shm/physics_cursor_bridge_shm
echo "[PhysicsCursor] Daemon encerrado. O cursor do Hyprland retornou ao modo estático seguro."
