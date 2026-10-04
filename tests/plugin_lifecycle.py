import re, os, pathlib, subprocess, tempfile, time, json, signal, argparse, shutil, shlex

parser = argparse.ArgumentParser(description="Test plugin startup/reload/unload/shutdown in an isolated nested compositor")
parser.add_argument("--plugin", type=pathlib.Path, default=pathlib.Path(__file__).resolve().parents[1] / "plugin/out/dynamic-cursors.so")
parser.add_argument("--surfaces", action="store_true", help="Exercise native Wayland and XWayland client cursor surfaces")
parser.add_argument("--transitions", action="store_true", help="Build a private IPC bridge and exercise real cursor transitions")
args = parser.parse_args()
if args.surfaces: args.transitions = True
plugin = str(args.plugin.resolve())
if not pathlib.Path(plugin).is_file():
    raise SystemExit("Build the plugin first: bash build.sh")
outer_runtime = os.environ.get("XDG_RUNTIME_DIR", "")
outer_display = os.environ.get("WAYLAND_DISPLAY", "")
if not outer_runtime or not outer_display:
    raise SystemExit("Run this test from an existing Wayland session")
outer_socket = pathlib.Path(outer_display) if outer_display.startswith("/") else pathlib.Path(outer_runtime) / outer_display
if not outer_socket.exists():
    raise SystemExit("Parent Wayland socket does not exist")
base=pathlib.Path(tempfile.mkdtemp(prefix='pc-'))
runtime=base/'r'; runtime.mkdir(mode=0o700)
cache=base/'cache'; cache.mkdir()
config=base/'test.lua'
client_proc = None
client_path = None
daemon_path = None
probe_path = None
if args.transitions:
    root = pathlib.Path(__file__).resolve().parents[1]
    private_root = base / "source"
    shutil.copytree(root, private_root, ignore=shutil.ignore_patterns(".git", "__pycache__"))
    bridge_name = "/physics_cursor_transition_test_" + str(os.getpid())
    define = '-DPHYSICS_CURSOR_SHM_PATH="' + bridge_name + '"'
    make_define = define.replace('"', '\\"')
    os.utime(private_root / "src/SharedBridge.hpp", None)
    subprocess.run(["make", "-C", str(private_root / "plugin"), "-j2", "EXTRA_CXXFLAGS=" + make_define], check=True)
    plugin = str(private_root / "plugin/out/dynamic-cursors.so")
    daemon_path = base / "test-daemon"
    cflags = shlex.split(subprocess.check_output(["pkg-config", "--cflags", "sdl3", "xcursor"], text=True))
    libs = shlex.split(subprocess.check_output(["pkg-config", "--libs", "sdl3", "xcursor"], text=True))
    subprocess.run(["g++", "-std=c++20", "-O2", define, *cflags, str(private_root / "src/main.cpp"),
                    str(private_root / "src/Font8x8.cpp"), "-o", str(daemon_path), *libs, "-lm"], check=True)
    # Inspect the real cursor state, without adding debug entry points to the installed plugin.
    header = (private_root / "plugin/src/cursor.hpp").read_text().replace("  private:", "  public:")
    header = header.replace("inline UP<CDynamicCursors> g_pDynamicCursors;", "")
    (base / "cursor-probe.hpp").write_text(header)
    probe_path = base / "transition-probe.so"
    probe_flags = shlex.split(subprocess.check_output(["pkg-config", "--cflags", "hyprland", "pixman-1", "libdrm"], text=True))
    subprocess.run(["g++", "-std=c++26", "-fPIC", "-shared", "--no-gnu-unique", define,
                    *probe_flags, "-I/usr/include/hyprland/src", "-I"+str(private_root / "plugin/src"),
                    "-I"+str(base), str(root / "tests/transition_probe.cpp"), "-o", str(probe_path)], check=True)

if args.surfaces:
    client_path = base / "surface-client"
    subprocess.run(["g++", "-std=c++20", *cflags, str(root / "tests/cursor_surface_client.cpp"),
                    "-o", str(client_path), *libs], check=True)
config.write_text('hl.env("XCURSOR_THEME","Bibata-Modern-Ice")\nhl.env("XCURSOR_SIZE","24")\nhl.monitor({output="", mode="preferred", position="auto", scale=1})\nhl.plugin.load('+json.dumps(plugin)+')\nhl.config({debug={enable_stdout_logs=true,disable_logs=false},plugin={dynamic_cursors={enabled=true,mode="tilt",threshold=0,shake={threshold=1000000},transition={enabled=' + ('true' if args.transitions else 'false') + '}}}})\n')
env=os.environ.copy()
env.pop("HYPRLAND_INSTANCE_SIGNATURE", None)
env.update(XDG_RUNTIME_DIR=str(runtime),XDG_CACHE_HOME=str(cache),XDG_CONFIG_HOME=str(base/'config'),WAYLAND_DISPLAY=str(outer_socket),AQ_DRM_DEVICES='/dev/nonexistent',HYPRLAND_NO_SD_VARS='1',HYPRLAND_NO_SD_NOTIFY='1',DBUS_SESSION_BUS_ADDRESS='')
test_state = base / "state.json"
env.update(PHYSICS_CURSOR_TEST_PLUGIN=plugin, PHYSICS_CURSOR_TEST_STATE=str(test_state))
log=base/'log.txt'
daemon_proc = None
if daemon_path:
    daemon_log = (base / "daemon.log").open("w")
    daemon_proc = subprocess.Popen([str(daemon_path), "--daemon"], env=env, stdin=subprocess.DEVNULL,
                                  stdout=daemon_log, stderr=subprocess.STDOUT, start_new_session=True)
    time.sleep(.1)
with log.open('w') as out:
    proc=subprocess.Popen(['Hyprland','--config',str(config)],env=env,stdout=out,stderr=subprocess.STDOUT,start_new_session=True)
    try:
        instance=None
        loaded=False
        for i in range(100):
            if proc.poll() is not None: break
            matches=list((runtime/'hypr').glob('*/.socket.sock')) if (runtime/'hypr').exists() else []
            if matches:
                instance=matches[0].parent.name
                testenv=env.copy(); testenv['HYPRLAND_INSTANCE_SIGNATURE']=instance
                testenv['WAYLAND_DISPLAY']=str(runtime/'wayland-1')
                check=subprocess.run(['hyprctl','plugin','list'],env=testenv,capture_output=True,text=True)
                if check.returncode==0 and 'dynamic-cursors' in check.stdout:
                    loaded=True
                    break
            time.sleep(.1)
        if proc.poll() is not None: raise RuntimeError('Nested compositor exited: '+str(proc.returncode))
        if not instance or not loaded: raise RuntimeError('Plugin did not load in the nested compositor')
        def ctl(*args, delay=.2):
            result=subprocess.run(['hyprctl',*args],env=testenv,capture_output=True,text=True,timeout=10)
            if result.returncode: raise RuntimeError(' '.join(args)+': '+result.stdout+result.stderr)
            time.sleep(delay)
            if proc.poll() is not None: raise RuntimeError('Nested compositor crashed after '+str(args))
            return result.stdout
        print(ctl('plugin','list'))
        for i in range(3):
            ctl('reload')
            ctl('plugin','unload',plugin)
            ctl('plugin','load',plugin)
        print('Nested startup, three reload/unload/load cycles passed.')
        if args.transitions:
            ctl('plugin', 'load', str(probe_path))
            def shape(name, x=160, delay=0, y=160):
                ctl('dispatch', 'hl.plugin.physics_cursor_test.shape('+json.dumps(name)+','+str(x)+','+str(y)+')', delay=delay)
                return json.loads(test_state.read_text())
            def state(): return shape('state')
            def require(ok, msg):
                if not ok: raise RuntimeError(msg)
            shape('default', delay=.35)
            for name in ('pointer', 'text', 'default'):
                started = shape(name, 160, delay=.03)
                require(started['active'] and started['layers'] > 0 and started['locks'] > 0,
                        'Named cursor change did not start the real transition: '+str(started))
                time.sleep(.35)
                ended = state()
                require(not ended['active'] and ended['layers'] == 0 and ended['locks'] == 0,
                        'Transition did not release images/software lock: '+str(ended))
            for i in range(20):
                current = shape('pointer' if i % 2 == 0 else 'default', 160 + (i % 3) * 80, delay=.015)
                require(current['active'] and 0 < current['layers'] <= 4,
                        'Rapid reversal lost or over-retained transition images: '+str(current))
            ctl('eval','hl.config({plugin={dynamic_cursors={transition={enabled=false}}}})', delay=.05)
            require('bool: false' in ctl('getoption','plugin:dynamic-cursors:transition:enabled', delay=0), 'Disable setting was not applied')
            disabled = state()
            require(not disabled['active'] and disabled['layers'] == 0 and disabled['locks'] == 0,
                    'Disable during animation failed to release rendering state: '+str(disabled))
            require(not shape('text', delay=.03)['active'], 'Disabled transition still started')
            ctl('eval','hl.config({plugin={dynamic_cursors={transition={enabled=true}}}})', delay=.05)
            ctl('dispatch','hl.plugin.dynamic_cursors.dsp_magnify({duration=1500,size=4})', delay=.5)
            magnified = shape('default', 160, delay=.1)
            require(magnified['active'] and magnified['zoom'] > 1,
                    'Transition did not run while magnified: '+str(magnified))
            if shutil.which('grim'):
                subprocess.run(['grim','-c','-g','80,80 480x280',str(base/'transition-magnified.png')],
                               env=testenv, check=True, timeout=10)
            time.sleep(2.5)
            settled = state()
            require(not settled['active'] and settled['locks'] == 0 and abs(settled['zoom']-1) < .01,
                    'Magnified transition failed to return to hardware state: '+str(settled))
            require(daemon_proc.poll() is None, 'Private physics daemon exited')
            print('Real default/pointer/text transitions, 20 rapid reversals, mid-animation disable and magnification passed.')
            if args.surfaces:
                theme = pathlib.Path.home() / ".local/share/icons/Bibata-Modern-Ice"
                require(theme.is_dir(), "Surface test needs installed Bibata-Modern-Ice")
                for driver in ("wayland", "x11"):
                    command = base / (driver + ".command")
                    command.write_text("left_ptr")
                    clientenv = testenv.copy()
                    clientenv["SDL_VIDEODRIVER"] = driver
                    if driver == "x11":
                        # This compositor's DISPLAY, never the outer session's :0.
                        display = re.search(r'XWayland found a suitable display socket at DISPLAY: (:[0-9]+)', log.read_text())
                        require(display is not None, 'Nested XWayland DISPLAY was not published')
                        clientenv['DISPLAY'] = display.group(1)
                    clientlog = (base / (driver + "-client.log")).open('w')
                    client_proc = subprocess.Popen([str(client_path), str(command), str(theme)], env=clientenv,
                                                   stdout=clientlog, stderr=subprocess.STDOUT)
                    try:
                        for _ in range(50):
                            clients = json.loads(ctl('-j', 'clients', delay=.05))
                            own_clients = [c for c in clients if c['pid'] == client_proc.pid]
                            if own_clients: break
                            require(client_proc.poll() is None, 'Client exited: ' + (base / (driver + "-client.log")).read_text())
                        require(bool(own_clients), 'Client window did not map')
                        time.sleep(.4)
                        clients = json.loads(ctl('-j', 'clients', delay=0))
                        own = next(c for c in clients if c['pid'] == client_proc.pid)
                        center_x = own['at'][0] + own['size'][0] / 2
                        center_y = own['at'][1] + own['size'][1] / 2
                        shape('warp', center_x, y=center_y, delay=.4)
                        initial = state()
                        require(initial['surface'] and initial['client_shape'] != 0,
                                driver + ': theme surface was not classified: ' + str(initial))
                        def client_shape(name, delay=.05):
                            previous = state()
                            command.write_text(name)
                            ack = pathlib.Path(str(command) + '.ack')
                            deadline = time.monotonic() + 2
                            while not ack.exists() or ack.read_text() != name:
                                require(client_proc.poll() is None, 'Surface client exited')
                                require(time.monotonic() < deadline, 'Surface client did not process command ' + name)
                                time.sleep(.005)
                            time.sleep(delay)
                            require(client_proc.poll() is None, 'Surface client exited')
                            current = state()
                            deadline = time.monotonic() + 2
                            while ((name == 'hide' and (current['active'] or current['client_hash'] != 0)) or
                                   (name != 'hide' and not name.startswith('watch:') and
                                    current['client_hash'] == previous['client_hash'])):
                                require(time.monotonic() < deadline, 'Cursor commit did not reach compositor: ' + name)
                                time.sleep(.01)
                                current = state()
                            return current
                        previous_hash = initial['client_hash']
                        for name in ('hand2', 'xterm', 'left_ptr'):
                            current = client_shape(name)
                            require(current['active'] and current['layers'] > 0,
                                    driver + ': surface transition missing: ' + str(current))
                            require(current['outgoing_hash'] == previous_hash,
                                    driver + ': outgoing surface image was overwritten: ' + str(current))
                            previous_hash = current['client_hash']
                            time.sleep(.35)
                            ended = state()
                            require(not ended['active'] and ended['layers'] == 0 and ended['locks'] == 0,
                                    driver + ': surface transition leaked: ' + str(ended))
                        for i in range(12):
                            current = client_shape('hand2' if i % 2 == 0 else 'xterm', .03)
                            require(current['active'] and 0 < current['layers'] <= 4,
                                    driver + ': rapid surface changes failed: ' + str(current))
                        client_shape('watch:0', .4)
                        before = state()
                        require(before['client_shape'] != 0, 'Watch not classified')
                        for frame in (1, 2, 3, 0):
                            current = client_shape('watch:' + str(frame))
                            require(not current['active'] and current['client_shape'] == before['client_shape'],
                                    driver + ': animation frame restarted transition: ' + str(current))
                        client_shape('hand2', .05)
                        ctl('eval', 'hl.config({plugin={dynamic_cursors={transition={enabled=false}}}})', delay=.05)
                        disabled = state()
                        require(not disabled['active'] and disabled['layers'] == 0 and disabled['locks'] == 0,
                                driver + ': disabling surface transition leaked state: ' + str(disabled))
                        ctl('eval', 'hl.config({plugin={dynamic_cursors={transition={enabled=true}}}})', delay=.05)
                        client_shape('left_ptr', .4)
                        client_shape('xterm', .05)
                        hidden = client_shape('hide', .05)
                        require(not hidden['active'] and hidden['layers'] == 0 and hidden['locks'] == 0,
                                driver + ': hiding cursor leaked state: ' + str(hidden))
                        print(driver + ': theme surfaces, rapid changes, animation frames, disable and hide passed.', flush=True)
                    finally:
                        if client_proc.poll() is None:
                            client_proc.terminate(); client_proc.wait(timeout=5)
                        clientlog.close()
                        client_proc = None
            ctl('plugin','unload',str(probe_path))
        print(ctl('getoption','plugin:dynamic-cursors:transition:enabled'))
        ctl('eval','hl.config({plugin={dynamic_cursors={transition={enabled=true}}}})')
        ctl('eval','hl.config({plugin={dynamic_cursors={transition={enabled=false}}}})')
        ended = subprocess.run(['hyprctl','dispatch','hl.dsp.exit()'], env=testenv, capture_output=True, text=True, timeout=10)
        if ended.returncode:
            raise RuntimeError('Exit dispatcher failed: '+ended.stdout+ended.stderr)
        proc.wait(timeout=10)
        if proc.returncode: raise RuntimeError('Nested shutdown exit code '+str(proc.returncode))
        print('Nested orderly shutdown passed.')
    finally:
        if proc.poll() is None:
            os.killpg(proc.pid,signal.SIGTERM)
            try: proc.wait(timeout=5)
            except subprocess.TimeoutExpired: os.killpg(proc.pid,signal.SIGKILL); proc.wait()
        if client_proc and client_proc.poll() is None:
            client_proc.terminate(); client_proc.wait(timeout=5)
        if daemon_proc and daemon_proc.poll() is None:
            daemon_proc.terminate()
            try: daemon_proc.wait(timeout=5)
            except subprocess.TimeoutExpired: daemon_proc.kill(); daemon_proc.wait()
        print('Lifecycle test logs:',log)
        if args.transitions: print('Transition test artifacts:',base)
        if proc.returncode:
            print(log.read_text()[-4500:])
