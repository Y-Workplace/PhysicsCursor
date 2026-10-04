# PhysicsCursor

A C++ simulation and Hyprland plugin that tilt the cursor using a spring,
damping, and inertia. The daemon computes physics at 500 Hz and publishes the
angle through shared memory. The click hotspot stays at the mouse position.

## Install on CachyOS / Arch Linux

```bash
git clone https://github.com/Y-Workplace/PhysicsCursor.git
cd PhysicsCursor
bash install.sh
```

By default, the installer **updates files without loading the plugin**, restarting
the daemon, or reloading the compositor configuration. It also preserves an
`ENABLED = false` startup guard in `cursor_bridge.lua`.

The installer uses `pacman` to install missing dependencies (it may request your
`sudo` password), builds the project, runs the tests, and installs the binaries.
You do not need to download libraries or locate development headers manually.
The first installation requires internet access if any packages are missing.

This repository includes the simulator and plugin sources, scripts, compiled
parameter defaults, and tests. System libraries are installed through the package
manager. The plugin is rebuilt against the installed Hyprland headers because its
ABI depends on the compositor build. Run `bash install.sh` again after updating
Hyprland.

The binaries are installed at `~/.local/bin/physics_cursor` and
`~/.local/share/hyprland/plugins/dynamic-cursors.so`. The installer configures
`hyprland.lua` or `hyprland.conf` without automatically activating the plugin.
To load it in the current session, run `bash install.sh --activate`. This does
not change the plugin login startup guard or reload the entire compositor configuration.
Activation also enables `physics-cursor.service` in the user systemd manager so
physics starts again at login. This prevents the faster local fallback after a reboot.
For an existing installation, enable daemon startup with:

```bash
bash scripts/install_daemon_service.sh --activate
systemctl --user status physics-cursor.service
```

The service runs `~/.local/bin/physics_cursor` through the ELF loader and restarts
on failure. F9 updates that installed binary and restarts the managed service.
A normal installation only writes the service file; it does not enable or start it.

## Adjust parameters, build, and test in the playground

```bash
bash run.sh
# Or use a transparent overlay:
bash run.sh --overlay
```

Adjust the parameters using the controls below, then press **F9**:

1. The current values are saved to `src/PhysicsDefaults.hpp`.
2. The simulator and daemon are rebuilt with those values.
3. Tests check stability during rapid shaking, the angular limit, return to rest,
   update cadence, and parameter export.
4. If all checks pass, the daemon restarts with the compiled configuration.
   An existing installation in `~/.local/bin` is updated as well.

The window stays open during compilation and displays the result in the HUD.
The log is saved to `build/playground-build.log`. Run the playground from the
project checkout to edit and compile its sources. Run `bash install.sh` first
to prepare the plugin and build tools. The previous daemon is preserved if
compilation or tests fail.

The interface and command-line messages are always in English, regardless of
system locale.

| Key | Action |
|---|---|
| 1 / 2 | Decrease / increase drag |
| 3 / 4 | Decrease / increase spring stiffness |
| 5 / 6 | Decrease / increase damping |
| 7 / 8 | Decrease / increase inertial influence |
| F9 | Save parameters, build, test, and apply to the daemon |
| R | Restore this window's compiled defaults |
| C | Toggle the system / vector cursor |
| V / T / P | Toggle vectors / trail / pivot |
| + / - | Increase / decrease the playground cursor size |
| Space | Apply an angular impulse |
| H | Show / hide the HUD |
| F11 / O | Toggle window / overlay mode |
| Esc / Q | Close the window, preserving the daemon |

Initial values: mass `1`, spring `155`, damping `4`, drag `0.00360`, inertia
`0.00005`, and maximum deflection `45°`. The simulator and daemon use the same
parameter file.

## Enlarge the cursor by shaking

Rapid shaking enlarges the cursor while preserving its physical rotation. The
plugin preserves physics in `tilt` mode even when an older configuration sets
`shake:effects = false`. It also preserves the enlarged texture's pivot,
including when using a higher-resolution theme image. The enlarged cursor uses
software rendering with a damage region that includes its rotation.

If an older configuration disables effects, enable them:

```ini
plugin:dynamic-cursors {
    mode = tilt
    threshold = 0
    shake:enabled = true
    shake:effects = true
}
```

In Lua, inside the `plugin.dynamic_cursors` block:

```lua
shake = { enabled = true, effects = true },
```

## Cursor shape transitions

When switching between theme shapes (arrow, link hand, text, or resize), the
previous cursor shrinks and rotates 15° while fading out. The new cursor fades
in with an elastic curve equivalent to `cubic-bezier(.34, 1.56, .64, 1)`.
Transitions are **disabled by default**. Enable them with
`transition:enabled = true`, or `enabled = true` in the Lua `transition` block.
The default duration is 250 ms; the opacity animation finishes after 200 ms.
Afterward, the cursor returns to its native theme size. Physics and shake
magnification remain active, and both images rotate around the actual click
hotspot.

During a transition, the plugin uses software rendering. Afterward, it releases
that mode unless magnification or another feature still requires it. Rapid
switches retain the images that are still visible, with a limit of four outgoing
images. App-provided Wayland cursor surfaces also participate, including through
XWayland. The plugin identifies images from the XCursor theme loaded by Hyprland
and groups the frames of animated cursors so a spinner does not restart the
transition every frame. The previous image is copied before switching, preserving
its content when the surface is reused.

Shared-memory ARGB cursors up to 512 × 512 pixels are supported. For custom
images that do not match the loaded theme, changes on the same surface only
start a transition if the size or hotspot changes: the protocol does not identify
whether other commits are new shapes or animation frames. GPU-only images
continue to use physics without shape transitions.

To disable transitions, inside the Lua `plugin.dynamic_cursors` block:

```lua
transition = { enabled = false, duration = 250 },
```

Or in `hyprland.conf`:

```ini
plugin:dynamic-cursors {
    transition:enabled = false
    transition:duration = 250
}
```

The supported duration range is 50–1000 ms. Disabling transitions preserves
cursor physics and shake magnification.

## Build without installing

```bash
bash build.sh               # Dependencies, simulator, tests, and plugin
bash build.sh --daemon-only # Simulator / daemon and tests only
make test                   # Tests without SDL or a graphical session
```

Use `JOBS=4 bash build.sh` to select the number of parallel compilation jobs
(the default is 2). The CMake workflow also includes the tests:

```bash
bash scripts/bootstrap.sh --daemon-only
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/cmake -j2
ctest --test-dir build/cmake --output-on-failure
```

## Control the daemon

```bash
bash start_daemon.sh
bash stop_daemon.sh
pgrep -fa 'physics_cursor --daemon'
```

When the daemon stops, the plugin falls back to its local physics in `tilt` mode.
Opening or closing the playground does not stop the daemon. Visual effects do
not change click events or the pointer movement delivered to applications.

Tests and the simulator run through the ELF loader. This supports running from
volumes such as `/mnt/archives` that do not preserve executable permissions on
compiled binaries.

## Test the plugin lifecycle in an isolated compositor

The plugin does not request a global configuration reload during `PLUGIN_INIT`.
Visual initialization is deferred until the event loop. Unloading cancels pending
tasks, removes hooks, and destroys callbacks together with their owners.

In addition to the numerical tests, this optional test starts a nested compositor
with a temporary configuration, cache, runtime directory, and IPC socket. It tests
loading from the initial configuration, three reload/unload/load cycles, and
shutdown:

```bash
python3 tests/plugin_lifecycle.py
```

Run it from an existing Wayland session. The test does not load the plugin into
the main session, change its configuration, or update the systemd environment.
Temporary log paths are printed when it finishes.

To also exercise actual arrow, hand, and text transitions:

```bash
python3 tests/plugin_lifecycle.py --transitions
```

This mode builds a temporary plugin copy and a daemon with private shared memory.
It checks 20 rapid reversals, disabling transitions during animation,
magnification, and the release of images and rendering locks afterward. The
inspection plugin exists only inside the test environment. The main session and
its daemon keep their current configuration. If `grim` is installed, the test
saves a screenshot of the magnified cursor alongside the temporary logs.

To also test Wayland and X11/XWayland app cursor surfaces:

```bash
python3 tests/plugin_lifecycle.py --surfaces
```

This mode uses Bibata-Modern-Ice installed in `~/.local/share/icons`, a controlled
SDL client, and the nested compositor's DISPLAY. It checks the previous image
snapshot, rapid changes, animated cursor frames, disabling transitions, and
hiding the cursor without loading the plugin into the main session.
