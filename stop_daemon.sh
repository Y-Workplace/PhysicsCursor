#!/bin/bash
pkill -f "physics_cursor --daemon"
rm -f /dev/shm/physics_cursor_bridge_shm
echo "[PhysicsCursor] Daemon stopped. The plugin falls back to local physics or the standard cursor, depending on its configuration."
