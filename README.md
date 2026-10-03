# PhysicsCursor 🚀
### Natural Physics-Driven Cursor Simulation with Zero-Crash IPC Bridge for Hyprland

A lightweight, high-performance C++ application and Hyprland plugin bridge designed for **CachyOS / Arch Linux / Wayland**. It gives your mouse cursor realistic, organic physical inertia, drag, and harmonic spring oscillation while keeping the click hotspot strictly locked to the exact event point.

[![C++20](https://img.shields.io/badge/C%2B%2B-20%2F26-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B20)
[![Wayland](https://img.shields.io/badge/Display-Wayland-orange.svg)](https://wayland.freedesktop.org/)
[![Hyprland](https://img.shields.io/badge/Compositor-Hyprland-brightgreen.svg)](https://hyprland.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

---

## ✨ Features

- 🎯 **Locked Click Hotspot:** The click vertex / hotspot `(0, 0)` is strictly preserved so clicking precision is never compromised.
- 🌊 **Realistic Elastic Physics:** Damped harmonic oscillator with velocity drag and inertia-induced recoil and overshoot.
- 🛡️ **Zero-Crash Out-of-Process Bridge:** The physics calculations can run in an isolated daemon communicating with Hyprland via POSIX Shared Memory (`/dev/shm`), protecting the compositor from crashes.
- 🔄 **Non-Disruptive Lifecycle:** Opening or closing the interactive GUI playground (`bash run.sh`) does **not** stop or disrupt the background system daemon.
- 🌐 **Bilingual HUD & Telemetry:** Real-time debugging vectors, speed, acceleration, and angle readout in both **English** and **Portuguese** (auto-detected, toggleable with `L`).
- ✍️ **Customizable Cursor Shapes:** Works across pointer, link, and text/I-beam cursors.

---

## ⚡ Quick Install (One-Line Setup)

Clone the repository and run the automated installer:

```bash
git clone https://github.com/Y-Workplace/PhysicsCursor.git
cd PhysicsCursor
bash install.sh
```

The installer will:
1. Copy or build the optimized binaries into `~/.local/bin/physics_cursor` and `~/.local/share/hyprland/plugins/dynamic-cursors.so`.
2. Configure Hyprland (`hyprland.lua` or `hyprland.conf`).
3. Start the background physics daemon seamlessly.

---

## 🎮 Interactive Simulation / Playground

Want to test, tweak parameters, and view real-time physics vectors? Run:

```bash
bash run.sh
```

Or for transparent fullscreen overlay mode:
```bash
bash run.sh --overlay
```

### Keyboard Shortcuts

| Key | Description |
|---|---|
| **[L]** | **Toggle Language (English / Português)** |
| **[C]** | Toggle Cursor Type (System theme vs Vectorial) |
| **[1] / [2]** | Decrease / Increase **Sensitivity & Drag** |
| **[3] / [4]** | Decrease / Increase **Spring Stiffness ($k$)** |
| **[5] / [6]** | Decrease / Increase **Damping ($\gamma$) / Smoothness** |
| **[7] / [8]** | Decrease / Increase **Inertial Force ($m \cdot a$)** |
| **[V]** | Toggle Real-time **Physics Vectors** overlay |
| **[P]** | Toggle **Pivot Hotspot Point** indicator |
| **[T]** | Toggle **Motion Trail** |
| **[H]** | Toggle **Telemetry HUD Panel** |
| **[Space]** | Apply test impulse kick |
| **[R]** | Reset all physical parameters to default |
| **[F11] / [O]** | Toggle Windowed vs Fullscreen Transparent Overlay |
| **[Esc] / [Q]** | Exit simulation (leaves system daemon active) |

---

## ⚙️ Daemon Management

To control the background daemon that drives the Hyprland system cursor:

- **Start Daemon:**
  ```bash
  bash start_daemon.sh
  ```
- **Stop Daemon:**
  ```bash
  bash stop_daemon.sh
  ```
- **Check Status:**
  ```bash
  pgrep -fa "physics_cursor --daemon"
  ```

---

## 🛠️ Building from Source

### Dependencies
On CachyOS / Arch Linux:
```bash
sudo pacman -S base-devel sdl3 libxcursor hyprland
```

### Compilation
```bash
# Compile physics simulator / daemon
make clean && make

# Compile Hyprland plugin
make -C plugin clean && make -C plugin
```

---

## ❓ Frequently Asked Questions (FAQ)

### 1. Does PhysicsCursor introduce input lag or click delay?
**Zero added latency (0 ms).**
- **Hotspot Integrity:** The click event point / hotspot is mathematically pinned to $(0, 0)$ (the exact first tip pixel of the cursor). Clicking precision is never offset or delayed.
- **Hardware Cursor Plane:** Wayland hardware cursor movement is processed directly by the compositor at the native polling rate of your mouse.
- **Asynchronous Rotation:** The physics engine computes the visual tilt angle via lock-free atomic shared memory at ~120 Hz in a detached process. The compositor simply reads this pre-computed float without waiting or blocking event queues.

---

### 2. What is the computational cost (CPU / GPU)?
**Negligible (< 0.1% CPU).**
- **CPU:** The background daemon runs a lightweight semi-implicit Euler integration using single-precision math. On modern multi-core processors, CPU usage is typically **under 0.1%**.
- **GPU:** Cursor rotations are rendered through Hyprland's existing hardware cursor buffer swapchain. No additional full-screen render passes, shaders, or GPU compositing overhead are incurred.
- **Battery Impact:** Virtually zero. When the cursor stops moving, the calculation enters an idle rest state with negligible CPU wakeups.

---

### 3. How much RAM does it use?
**Around ~5 MB total.**
- The standalone daemon process consumes only **~5 MB RSS** of memory.
- The IPC bridge utilizes POSIX Shared Memory (`/dev/shm/physics_cursor_bridge_shm`), which allocates exactly **64 bytes** (a single cacheline-aligned struct). There are no heavy Unix domain socket buffers, pipelines, or message queues.

---

### 4. What happens if the daemon crashes or is terminated?
**The desktop remains 100% stable.**
- Thanks to the **Out-of-Process Bridge architecture**, the physics computation is completely isolated from Hyprland.
- If the daemon is stopped, killed, or restarts, the Hyprland plugin immediately detects the missing heartbeat and smoothly reverts the cursor to standard static orientation. Your compositor, windows, and apps will never freeze or crash.

---

### 5. Does this interfere with gaming (FPS games / Raw Input)?
**No.**
- Games and applications that capture the mouse (e.g. FPS games using Wayland relative pointer / pointer constraints) hide or lock the hardware cursor.
- Raw mouse movement deltas sent to games are untouched by the physics engine.

---

### 6. Can I disable physics on specific cursor shapes (like text selection)?
**Yes.**
- You can configure custom shape rules in `~/.config/hypr/config/cursor_bridge.lua`.
- For example, if you prefer the text cursor (I-beam) to stay strictly vertical for precision code selection, simply uncomment:
  ```lua
  hl.plugin.dynamic_cursors.shape_rule({ shape = "text", mode = "none" })
  ```

---

## 📝 License

Distributed under the MIT License. See [LICENSE](LICENSE) for details.
