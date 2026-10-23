"""Loopback-only join experiment using the repository's existing integration harness.

Usage: python docs/audits/instant-join/run_local.py out/build/x64-Release [repetitions]
Requires a client built with instrument.py apply. Results stay under out/.
"""
import copy
import json
from pathlib import Path
import re
import shutil
import sys
import tempfile
import time
import random
import os

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'scripts'))
import integration_test as it

def main():
    build = Path(sys.argv[1]).resolve()
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 3
    backend = sys.argv[3] if len(sys.argv) > 3 else None
    results = []
    rng = random.Random(20261023)
    variant = os.environ.get('FAST_JOIN_VARIANT', 'baseline')
    for seven in (False, True):
        for repetition in range(count):
            work = Path(tempfile.mkdtemp(prefix=f'join_{7 if seven else 6}_{repetition}_', dir=build))
            runner = it.TestRunner(str(build / os.environ.get('FAST_JOIN_CLIENT_NAME', 'DDNet.exe')), str(build / 'DDNet-Server.exe'), None,
                str(ROOT), str(build), True, False, False, True, 1)
            # Alias names ensure maps cannot accidentally be found in shared assets.
            server_data = work / 'server-data'
            for directory in ('maps', 'maps7'):
                (server_data / directory).mkdir(parents=True)
                for name in ('join-a', 'join-b'):
                    shutil.copyfile(build / 'data' / directory / ('Tutorial.map' if name == 'join-a' else 'Sunny Side Up.map'), server_data / directory / f'{name}.map')
            server_dir = work / 'server'
            client_dir = work / 'client'
            server_dir.mkdir()
            client_dir.mkdir()
            server_runner = copy.copy(runner)
            server_runner.data_dir = str(server_data)
            server_env = it.TestEnvironment(server_runner, 'join_server', str(server_dir), timeout=90)
            client_env = it.TestEnvironment(runner, 'join_client', str(client_dir), timeout=90)
            try:
                server = server_env.server(['sv_map join-a', 'sv_map_window 8', 'sv_dnsbl 0'])
                server.wait_for_startup()
                address = ('tw-0.7+udp://' if seven else '') + f'127.0.0.1:{server.port}'
                options = ['cl_map_download_url https://127.0.0.1:1', 'cl_map_download_connect_timeout_ms 50',
                    'stdout_output_level 1', 'cl_show_welcome 0', 'gfx_vsync 0', 'gfx_refresh_rate 144',
                    'cl_refresh_rate 1000', 'cl_refresh_rate_inactive 1000', 'ui_page 8', 'cl_skip_start_menu 1']
                if backend:
                    options += [f'gfx_backend {"OpenGL" if backend == "OpenGL3" else backend}']
                if backend == 'OpenGL3':
                    options += ['gfx_gl_major 3', 'gfx_gl_minor 3']
                options += os.environ.get('FAST_JOIN_OPTIONS', '').split('|') if os.environ.get('FAST_JOIN_OPTIONS') else []
                def connect(client, case):
                    time.sleep(rng.uniform(0, 0.2))
                    client.command(f'connect {address}')
                    client.wait_for_log_prefix('joinprobe:', timeout=20)  # fail if probes absent
                    wait_frame(client)
                    if os.environ.get('FAST_JOIN_SCREENSHOT_ONLY') and case == 'cold_fallback_udp':
                        client.command('screenshot')
                        client.wait_for_log_prefix('client: Saved screenshot to', timeout=10)
                    if os.environ.get('FAST_JOIN_VALIDATE') and case == 'cold_fallback_udp':
                        client.command('joinprobe_right 1')
                        timeout = client.register_timeout(5, 'authoritative horizontal movement')
                        while True:
                            event = client.next_event(timeout)
                            if isinstance(event, it.Log) and 'event=authoritative_motion ' in event.line:
                                break
                            if isinstance(event, it.Exit):
                                raise RuntimeError('Client exited before movement')
                        client.command('joinprobe_right 0')
                    results.append({'protocol': '0.7' if seven else '0.6', 'repetition': repetition,
                        'case': case, 'work': str(work), 'client': client.name, 'peak_working_set_bytes': peak_memory(client), 'renderer': renderer(client), 'events': parse(client.full_stdout)})
                client = client_env.client(options)
                client.wait_for_startup()
                connect(client, 'cold_fallback_udp')
                client.command('disconnect')
                connect(client, 'reconnect')
                client.exit()
                client.wait_for_exit()
                client = client_env.client(options)
                client.wait_for_startup()
                connect(client, 'warm_new_process')
                time.sleep(rng.uniform(0, 0.2))
                server.command('sv_map join-b')
                wait_frame(client)
                results.append({'protocol': '0.7' if seven else '0.6', 'repetition': repetition,
                    'case': 'map_switch_cold', 'work': str(work), 'client': client.name, 'peak_working_set_bytes': peak_memory(client), 'renderer': renderer(client), 'events': parse(client.full_stdout)})
                client.exit()
                client.wait_for_exit()
                server.exit()
                server.wait_for_exit()
                print(f'protocol={7 if seven else 6} repetition={repetition} complete', flush=True)
            finally:
                client_env.kill_all()
                server_env.kill_all()
                (build / f'fast-join-{variant}.json').write_text(json.dumps(results, indent=2), encoding='utf-8')

def peak_memory(client):
    if os.name != 'nt':
        return None
    import ctypes
    from ctypes import wintypes
    class Counters(ctypes.Structure):
        _fields_ = [('cb', wintypes.DWORD), ('faults', wintypes.DWORD)] + [(name, ctypes.c_size_t) for name in
            ('peak_working_set', 'working_set', 'peak_paged', 'paged', 'peak_nonpaged', 'nonpaged', 'pagefile', 'peak_pagefile')]
    counters = Counters()
    counters.cb = ctypes.sizeof(counters)
    function = ctypes.WinDLL('psapi').GetProcessMemoryInfo
    function.argtypes = (wintypes.HANDLE, ctypes.POINTER(Counters), wintypes.DWORD)
    if not function(int(client.process._handle), ctypes.byref(counters), counters.cb):
        raise ctypes.WinError()
    return counters.peak_working_set

def renderer(client):
    lines = [line for line in client.full_stdout if 'Created OpenGL' in line]
    assert lines and '3.3' in lines[-1], lines
    return lines[-1]

def wait_frame(client):
    timeout = client.register_timeout(25, 'character frame submitted')
    while True:
        event = client.next_event(timeout)
        if isinstance(event, it.Log) and 'event=full_scene_submitted ' in event.line:
            return
        if isinstance(event, it.Exit):
            raise RuntimeError('Client exited before character frame')

def parse(lines):
    events = []
    for line in lines:
        m = re.search(r'joinprobe: epoch=(\d+) event=(\S+) ns=(\d+)', line)
        if m:
            events.append({'epoch': int(m[1]), 'event': m[2], 'ns': int(m[3])})
        m = re.search(r'joinscope: epoch=(\d+) scope=(\S+) ms=([\d.]+)', line)
        if m:
            events.append({'epoch': int(m[1]), 'scope': m[2], 'ms': float(m[3])})
    epoch = max(e['epoch'] for e in events)
    result = [e for e in events if e['epoch'] == epoch]
    assert sum(e.get('event') == 'map_change' for e in result) == 1, 'overlapping map transitions invalidate sample'
    return result

if __name__ == '__main__':
    main()
