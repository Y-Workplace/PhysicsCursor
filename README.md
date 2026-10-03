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

## 📝 License

Distributed under the MIT License. See [LICENSE](LICENSE) for details.
