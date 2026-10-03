import os, pathlib, subprocess, tempfile, time, json, signal, argparse

parser = argparse.ArgumentParser(description="Test plugin startup/reload/unload/shutdown in an isolated nested compositor")
parser.add_argument("--plugin", type=pathlib.Path, default=pathlib.Path(__file__).resolve().parents[1] / "plugin/out/dynamic-cursors.so")
args = parser.parse_args()
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
config.write_text('hl.monitor({output="", mode="preferred", position="auto", scale=1})\nhl.plugin.load('+json.dumps(plugin)+')\nhl.config({debug={enable_stdout_logs=true,disable_logs=false},plugin={dynamic_cursors={enabled=true,mode="tilt",threshold=0,transition={enabled=false}}}})\n')
env=os.environ.copy()
env.pop("HYPRLAND_INSTANCE_SIGNATURE", None)
env.update(XDG_RUNTIME_DIR=str(runtime),XDG_CACHE_HOME=str(cache),XDG_CONFIG_HOME=str(base/'config'),WAYLAND_DISPLAY=str(outer_socket),AQ_DRM_DEVICES='/dev/nonexistent',HYPRLAND_NO_SD_VARS='1',HYPRLAND_NO_SD_NOTIFY='1',DBUS_SESSION_BUS_ADDRESS='')
log=base/'log.txt'
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
                check=subprocess.run(['hyprctl','plugin','list'],env=testenv,capture_output=True,text=True)
                if check.returncode==0 and 'dynamic-cursors' in check.stdout:
                    loaded=True
                    break
            time.sleep(.1)
        if proc.poll() is not None: raise RuntimeError('Nested compositor exited: '+str(proc.returncode))
        if not instance or not loaded: raise RuntimeError('Plugin did not load in the nested compositor')
        def ctl(*args):
            result=subprocess.run(['hyprctl',*args],env=testenv,capture_output=True,text=True,timeout=10)
            if result.returncode: raise RuntimeError(' '.join(args)+': '+result.stdout+result.stderr)
            time.sleep(.2)
            if proc.poll() is not None: raise RuntimeError('Nested compositor crashed after '+str(args))
            return result.stdout
        print(ctl('plugin','list'))
        for i in range(3):
            ctl('reload')
            ctl('plugin','unload',plugin)
            ctl('plugin','load',plugin)
        print('Nested startup, three reload/unload/load cycles passed.')
        print(ctl('getoption','plugin:dynamic-cursors:transition:enabled'))
        ctl('keyword','plugin:dynamic-cursors:transition:enabled','true')
        ctl('keyword','plugin:dynamic-cursors:transition:enabled','false')
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
        print('Lifecycle test logs:',log)
        if proc.returncode:
            print(log.read_text()[-4500:])
