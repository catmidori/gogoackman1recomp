"""Build-time SVG catalog. No paths or files are used by the runtime."""
from pathlib import Path
import sys

root, output = map(Path, sys.argv[1:])
files = sorted(root.rglob('*.svg'))
names = [p.name for p in files]
if not files or len(set(names)) != len(names):
    raise SystemExit('SVG catalog is empty or has duplicate basenames')
lines = ['#include "recomp_menu_icons.h"']
for i, path in enumerate(files):
    data = path.read_bytes()
    lines.append(f'static const unsigned char svg_{i}[] = {{')
    for start in range(0, len(data), 24):
        lines.append(','.join(f'0x{byte:02x}' for byte in data[start:start+24])+',')
    lines.append('};')
lines.append('static const RecompSvgAsset assets[] = {')
for i, name in enumerate(names):
    lines.append(f'{{"{name}",svg_{i},sizeof(svg_{i})}},')
lines += ['};', 'const RecompSvgAsset *RecompButtonAssets(size_t *count) {',
          '*count=sizeof(assets)/sizeof(assets[0]);return assets;', '}']
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text('\n'.join(lines)+'\n', encoding='utf-8')
