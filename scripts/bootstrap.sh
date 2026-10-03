#!/usr/bin/env bash
set -euo pipefail
packages=(sdl3 xcursor)
if [[ ${1:-} != --daemon-only ]]; then
    packages+=(hyprland pixman-1 libdrm)
fi
if command -v g++ >/dev/null && command -v make >/dev/null &&
   command -v pkg-config >/dev/null && pkg-config --exists "${packages[@]}"; then
    exit 0
fi
if ! command -v pacman >/dev/null; then
    echo 'Automatic dependency installation supports CachyOS/Arch Linux.' >&2
    echo 'Required: C++ compiler, make, pkg-config, SDL3, libxcursor and Hyprland development headers.' >&2
    exit 1
fi
runner=()
if (( EUID != 0 )); then runner=(sudo); fi
# Use the distribution packages, including headers matching its compositor.
required=(base-devel pkgconf sdl3 libxcursor)
if [[ ${1:-} != --daemon-only ]]; then required+=(hyprland pixman libdrm); fi
"${runner[@]}" pacman -S --needed "${required[@]}"
