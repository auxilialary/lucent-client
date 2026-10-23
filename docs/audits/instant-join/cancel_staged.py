"""Cancel staged work during snapshot bootstrap, then verify authoritative movement.
Requires instrument.py probes and cl_staged_map_load support; loopback only.
"""
import json
from pathlib import Path
import sys
import tempfile
import time
import run_local as bench

it = bench.it

def wait(client, name, epoch=None):
    import re
    timeout = client.register_timeout(25, name)
    while True:
        event = client.next_event(timeout)
        if isinstance(event, it.Exit):
            raise RuntimeError('Client exited during cancellation test')
        if isinstance(event, it.Log):
            match = re.search(r'joinprobe: epoch=(\d+) event=(\S+)', event.line)
            if match and match[2] == name and (epoch is None or int(match[1]) == epoch):
                return int(match[1])

def main():
    build = Path(sys.argv[1]).resolve()
    result = []
    for seven in [False, True]:
        work = Path(tempfile.mkdtemp(prefix='fast_join_cancel_', dir=build))
        runner = it.TestRunner(str(build/'DDNet.exe'), str(build/'DDNet-Server.exe'), None,
            str(bench.ROOT), str(build), True, False, False, True, 1)
        env = it.TestEnvironment(runner, 'fast_join_cancel', str(work), timeout=90)
        try:
            server = env.server(['sv_map Tutorial', 'sv_dnsbl 0', 'sv_connlimit 100'])
            server.wait_for_startup()
            client = env.client(['stdout_output_level 1', 'cl_show_welcome 0', 'cl_skip_start_menu 1',
                'gfx_backend OpenGL', 'gfx_gl_major 3', 'gfx_gl_minor 3', 'gfx_vsync 0',
                'cl_refresh_rate 1000', 'cl_refresh_rate_inactive 1000', 'cl_staged_map_load 1'])
            client.wait_for_startup()
            address = ('tw-0.7+udp://' if seven else '')+f'127.0.0.1:{server.port}'
            for attempt in range(10):
                client.command(f'connect {address}')
                epoch = wait(client, 'connect')
                wait(client, 'resources_done', epoch)
                # Deliberately abort while images/quads are still pending.
                client.command('disconnect')
                time.sleep((attempt % 4)*.003)
            client.command(f'connect {address}')
            epoch = wait(client, 'connect')
            wait(client, 'full_scene_submitted', epoch)
            client.command('joinprobe_right 1')
            wait(client, 'authoritative_motion', epoch)
            client.command('joinprobe_right 0')
            result.append(dict(protocol='0.7' if seven else '0.6', cancelled=10,
                authoritative_motion=True, renderer=bench.renderer(client), work=str(work)))
            client.exit(); client.wait_for_exit()
            server.exit(); server.wait_for_exit()
        finally:
            env.kill_all()
            (build/'fast-join-cancellation.json').write_text(json.dumps(result,indent=2))
    print(json.dumps(result,indent=2))

if __name__ == '__main__':
    main()

