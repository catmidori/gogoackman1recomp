"""Only SNESRecomp analyzes/emits the guest; Python here verifies and invokes it."""
import argparse
import hashlib
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]

def normalized(path):
    size = path.stat().st_size
    if not 32768 <= size <= 8 * 1024 * 1024 + 512:
        raise ValueError('ROM size outside allowed bounds')
    data = path.read_bytes()
    if len(data) % 32768 == 512:
        data = data[512:]
    if len(data) % 32768:
        raise ValueError('Truncated or unsupported ROM')
    return data

def identity():
    return {
        line.split('=', 1)[0].strip(): line.split('=', 1)[1].strip()
        for line in (ROOT / 'rom_identity.txt').read_text(encoding='utf-8').splitlines()
        if '=' in line and not line.lstrip().startswith('#')}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--rom', required=True, type=Path)
    ap.add_argument('--verify-only', action='store_true')
    args = ap.parse_args()
    expected = identity()
    data = normalized(args.rom)
    if len(data) != int(expected['rom_size']) or hashlib.sha256(data).hexdigest() != expected['expected_sha256']:
        raise SystemExit('ROM revision rejected: size/SHA-256 mismatch')
    print('Verified normalized ROM SHA-256; no assets embedded.')
    if not args.verify_only:
        subprocess.run([sys.executable, str(ROOT / 'external/snesrecomp/snesrecomp_cli.py'),
            'generate', '--rom', str(args.rom.resolve()), '--cfg-dir', 'recomp',
            '--out-dir', 'src/gen', '--funcs-h', 'recomp/funcs.h',
            '--project-root', str(ROOT), '--cfg-roots', '--analysis-backend', 'python',
            '--expected-sha256', expected['expected_sha256']], cwd=ROOT, check=True)
        subprocess.run([sys.executable, str(ROOT / 'scripts/inject_hooks.py')], check=True)

if __name__ == '__main__':
    main()
