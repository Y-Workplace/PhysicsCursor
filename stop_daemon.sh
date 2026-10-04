#!/bin/bash
systemctl --user stop physics-cursor.service 2>/dev/null || true
pkill -f '(^|/)physics_cursor --daemon$' 2>/dev/null || true
rm -f /dev/shm/physics_cursor_bridge_shm
echo "[PhysicsCursor] Daemon stopped. The plugin falls back to local physics or the standard cursor, depending on its configuration."
