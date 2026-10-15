"""Bounded headless SDL3 smoke; no claim about visible output or fidelity."""
import argparse
import os
import re
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
ap = argparse.ArgumentParser()
ap.add_argument('--exe', type=Path, default=root / 'build-ninja/AckmanRecomp.exe')
ap.add_argument('--rom', type=Path, required=True)
ap.add_argument('--frames', type=int, default=120)
ap.add_argument('--script', type=Path)
ap.add_argument('--timeout', type=int, default=60)
ap.add_argument('--config', type=Path, default=root / 'config.ini')
args = ap.parse_args()
exe = args.exe.resolve()
env = os.environ.copy()
env.update(SDL_VIDEODRIVER='dummy', SDL_AUDIO_DRIVER='dummy',
           SDL_RENDER_DRIVER='software', SNESRECOMP_RUN_FRAMES=str(args.frames),
           ACKMAN_TRACE=str(root / 'private/smoke.csv'))
shutil.copyfile(args.config, exe.parent / 'config.ini')
with (root / 'private/smoke.log').open('w', encoding='utf-8') as log:
    try:
        command = [str(exe)]
        if args.script: command += ['--script', str(args.script.resolve())]
        command += ['--no-launcher', str(args.rom.resolve())]
        result = subprocess.run(command,
                                cwd=exe.parent, env=env, stdout=log, stderr=log,
                                timeout=args.timeout)
    except subprocess.TimeoutExpired:
        raise SystemExit(f'Smoke timed out after {args.timeout} seconds.')
print(f'Smoke exit={result.returncode}. Read private/smoke.log and smoke.csv.')
output = (root / 'private/smoke.log').read_text(encoding='utf-8', errors='replace')
if re.search(r'SEH CRASH|slice watchdog|CPU-port guest-clock sync timed out', output):
    raise SystemExit('Smoke failed: runtime error in log despite process exit status.')
if result.returncode == 0 and f'RUN_FRAMES reached after {args.frames} frames' not in output:
    raise SystemExit('Smoke failed: requested frame count was not reached.')
raise SystemExit(result.returncode)
