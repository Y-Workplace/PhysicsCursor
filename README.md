# PhysicsCursor

[![Language](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B20)
[![Compositor](https://img.shields.io/badge/Wayland-Hyprland-brightgreen.svg)](https://hyprland.org)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

**PhysicsCursor** brings realistic physical inertia, rotational torque, and elastic harmonic spring dynamics to your mouse cursor on Linux (Wayland / Hyprland), preserving 100% of native cursor shape changes (hovering links, text selection, resizing) with **zero risk of compositor crashes**.

---

## Features

- **Exact Event Point Pivot:** The rotation anchor is strictly pinned to the cursor's interaction hotspot (`(0, 0)` / click pixel). The tip never drifts while the cursor body swings dynamically.
- **Symmetric Harmonic Physics:** Moving left or right produces smooth, symmetric air drag and inertia. Stopping suddenly produces an organic, underdamped elastic spring wobble before resting.
- **Out-of-Process Sandbox (Crash-Proof):** The physics engine runs in a lightweight, isolated user daemon and communicates with Hyprland via POSIX Shared Memory (`/dev/shm`). If the daemon stops, the cursor instantly falls back to static mode without ever taking down your desktop.
- **Full Hover Compatibility:** Operates at the compositor level so your active cursor theme (e.g., `Bibata-Modern-Ice`) continues switching to link pointers, text I-beams, and resize grips seamlessly.
- **Interactive Playground Included:** Launch `physics_cursor` directly to tweak spring constants, mass, damping, and inspect real-time force vectors on an interactive HUD.

---

## Quick Install (One-Line)

### From Cloned Repository:
```bash
git clone https://github.com/<your-username>/PhysicsCursor.git
cd PhysicsCursor
bash install.sh
```

*(Precompiled binaries for x86_64 are included in `bin/` for instant 2-second installation without needing to compile headers).*

---

## How It Works: The Shared Memory Bridge

```
┌────────────────────────────────────────────────────────┐
│         PhysicsCursor Daemon (Isolated Process)        │
│  - Calculates angular dynamics, spring & inertia       │
│  - Runs at ~120 Hz with sub-stepping                   │
│  - Writes instantaneous angle to /dev/shm              │
└──────────────────────────┬─────────────────────────────┘
                           │  POSIX Shared Memory
                           ▼  (Latency < 0.001 ms)
┌────────────────────────────────────────────────────────┐
│            Hyprland Plugin (Minimal Consumer)          │
│  - Reads atomic rotation float from shared RAM         │
│  - Rotates native hardware cursor around hotspot       │
│  - Preserves 100% of native hover shapes               │
│  - ZERO crash risk to the compositor                   │
└────────────────────────────────────────────────────────┘
```

---

## Building from Source

If you prefer building from source on your machine:
```bash
# Build Daemon & Interactive App
make

# Build Hyprland Plugin
make -C plugin

# Or run the installer in build mode:
bash install.sh --build
```

### Dependencies
- C++20 compiler (`gcc` / `g++`)
- `sdl3`
- `libXcursor`
- `hyprland-headers` (only needed if compiling the plugin from source)

---

## Usage & Controls

- **Start Background Daemon:**
  ```bash
  bash start_daemon.sh
  ```
- **Stop Daemon:**
  ```bash
  bash stop_daemon.sh
  ```
- **Launch Interactive Playground / HUD:**
  ```bash
  physics_cursor
  # or:
  bash run.sh
  ```

### Interactive HUD Hotkeys
| Key | Action |
|---|---|
| `[1] / [2]` | Decrease / Increase Air Drag Sensitivity |
| `[3] / [4]` | Decrease / Increase Spring Stiffness ($k$) |
| `[5] / [6]` | Decrease / Increase Viscous Damping ($\gamma$) |
| `[7] / [8]` | Decrease / Increase Inertial Kick ($m \cdot a$) |
| `[C]` | Toggle between System Native Cursor & Vector Cursor |
| `[V]` | Toggle Real-time Physics Vectors |
| `[T]` | Toggle Motion Trail |
| `[Space]` | Apply manual test impulse |
| `[R]` | Reset parameters to defaults |

---

## Uninstallation

To cleanly remove PhysicsCursor and restore default settings:
```bash
bash uninstall.sh
```

---

## License

This project is licensed under the [MIT License](LICENSE).
Compositor rendering layer adapted from [VirtCode/hypr-dynamic-cursors](https://github.com/VirtCode/hypr-dynamic-cursors) (MIT).
