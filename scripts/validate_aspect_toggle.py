"""Exercise real SDL host-key dispatch and guest-state isolation in two fixtures."""
import argparse
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
ap = argparse.ArgumentParser()
ap.add_argument('--exe', type=Path, required=True)
ap.add_argument('--rom', type=Path, required=True)
ap.add_argument('--out', type=Path, required=True)
args = ap.parse_args()
out = args.out.resolve()
out.mkdir(parents=True, exist_ok=True)
records = json.loads((root/'private/scene-audit/manifest.json').read_text(encoding='utf-8'))
fixtures = [r for r in records if r['id'] in (2, 37)]
assert len(fixtures) == 2
config = (root/'config.ini').read_text(encoding='utf-8')
config = config.replace('DisableFrameDelay = 0', 'DisableFrameDelay = 1').replace('VSync = 1', 'VSync = 0')

def run(job):
    fixture, mode = job
    folder = out/f"scene-{fixture['id']:02d}-{mode}"
    folder.mkdir(parents=True, exist_ok=True)
    (folder/'saves').mkdir(exist_ok=True)
    shutil.copyfile(args.exe, folder/'AckmanRecomp.exe')
    shutil.copyfile(root/'build-ninja/saves'/fixture['save'], folder/'saves/GGA1_save_01.sav')
    ini = config.replace('IgnoreAspectRatio = 1', f'IgnoreAspectRatio = {0 if mode == "native" else 1}')
    ini = ini.replace('ToggleWidescreen = u', 'ToggleWidescreen = F9')
    (folder/'config.ini').write_text(ini, encoding='utf-8')
    script = 'wait 1\nloadstate 0\nwait 20\n'
    script += ('hostkey F9\nwait 2\n'*200 if mode == 'toggle' else 'wait 1000\n')
    (folder/'replay.txt').write_text(script, encoding='utf-8')
    env = os.environ.copy()
    env.update(SDL_VIDEODRIVER='dummy', SDL_AUDIO_DRIVER='dummy', SDL_RENDER_DRIVER='software',
               SNESRECOMP_RUN_FRAMES='1000', SNESRECOMP_SCREENSHOT=str(folder/'frame.ppm'),
               SNESRECOMP_SCREENSHOT_FRAME='1000', ACKMAN_DUMP_FRAME='990',
               ACKMAN_DUMP_PREFIX=str(folder/'state'), ACKMAN_TRACE=str(folder/'trace.csv'))
    with (folder/'run.log').open('w', encoding='utf-8') as log:
        result = subprocess.run([str(folder/'AckmanRecomp.exe'), '--no-launcher', '--script',
                                 str(folder/'replay.txt'), str(args.rom.resolve())],
                                cwd=folder, env=env, stdout=log, stderr=log, timeout=60)
    log = (folder/'run.log').read_text(encoding='utf-8', errors='replace')
    assert result.returncode == 0 and 'RUN_FRAMES reached after 1000 frames' in log, folder
    assert not re.search(r'SEH CRASH|slice watchdog|CPU-port guest-clock sync timed out|load refused|Unknown|Duplicate key', log), folder
    if mode == 'toggle':
        assert log.count('Ackman aspect: native 4:3') == 100, folder
        assert log.count('Ackman aspect: adaptive widescreen') == 100, folder
    header, dimensions, maximum, pixels = (folder/'frame.ppm').read_bytes().split(b'\n', 3)
    width, height = map(int, dimensions.split())
    assert header == b'P6' and maximum == b'255' and height == 224
    assert width == (256 if mode == 'native' else 384), (folder, width)
    center = b''.join(pixels[(y*width+(width-256)//2)*3:(y*width+(width+256)//2)*3] for y in range(height))
    states = {ext: (folder/f'state.{ext}').read_bytes() for ext in ('wram', 'vram', 'oam', 'spc-ram')}
    return fixture['id'], mode, states, center

jobs = [(fixture, mode) for fixture in fixtures for mode in ('wide', 'native', 'toggle')]
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
    results = list(pool.map(run, jobs))
for fixture in fixtures:
    group = [r for r in results if r[0] == fixture['id']]
    for result in group[1:]:
        assert result[2] == group[0][2], ('guest state changed', fixture['id'], result[1])
        assert result[3] == group[0][3], ('native center changed', fixture['id'], result[1])
report = dict(frames=6000, scenes=2, sdl_toggles=400, guest_states_equal=True,
              native_centers_equal=True, sha256=hashlib.sha256(args.exe.read_bytes()).hexdigest())
(out/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
print(json.dumps(report))
