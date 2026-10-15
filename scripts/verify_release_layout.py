"""Verify Linux release templates and reject bundled user content."""
import argparse
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--linux-prefix', required=True, type=Path)
args = parser.parse_args()
prefix = args.linux_prefix
data = prefix / 'share/org.snesrecomp.AckmanRecomp'
for folder in ('docs', 'mods', 'rom', 'saves', 'shaders'):
    directory = data / 'user-template' / folder
    assert sorted(p.name for p in directory.iterdir()) == ['Instructions.txt']
    assert (directory / 'Instructions.txt').read_bytes() == (ROOT / folder / 'Instructions.txt').read_bytes()
for name in ('README.md', 'THIRD_PARTY_ATTRIBUTION.md', 'LICENSE', 'RELEASE_NOTES.md'):
    assert (data / 'user-template' / name).read_bytes() == (ROOT / name).read_bytes()
for path in prefix.rglob('*'):
    if path.is_file():
        assert path.suffix.lower() not in ('.sfc', '.smc', '.sav', '.ips', '.snesmod', '.msu', '.pcm', '.glsl', '.glslp'), path
exe = prefix / 'bin/AckmanRecomp'
assert exe.read_bytes()[:5] == b'\x7fELF\x02'
dynamic = subprocess.check_output(['readelf', '-d', str(exe)], text=True)
assert not any('NEEDED' in line and 'libz.so' in line for line in dynamic.splitlines())
print('PASS clean Linux x64 layout, exact instruction/notice templates and statically linked zlib.')
