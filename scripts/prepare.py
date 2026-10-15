"""Materialize the upstream host templates once; never overwrite hand edits."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
p = json.loads((ROOT / 'private/probe.json').read_text(encoding='utf-8'))
tokens = {
    'DISPLAY_NAME': 'Go Go Ackman', 'PROJECT_NAME': 'AckmanRecomp',
    'WINDOW_TITLE': 'Go Go Ackman - experimental SNESRecomp port',
    'CATALOG_SLUG': 'go-go-ackman', 'GAME_ID': 'go-go-ackman-jp',
    'PLAYERS': '1', 'REGION': 'JPN', 'ROM_FILE': p['rom_file'],
    'ROM_SIZE': str(p['normalized_size']), 'ROM_MAPPING': p['mapping'],
    **{'ROM_' + k.upper(): p[k] for k in ('crc32', 'md5', 'sha1', 'sha256')},
}
templates = ROOT / 'external/snesrecomp/tools/new_project/templates'
for name in ('main.c', 'game_rtl.c', 'game_rtl.h', 'host_contract.c',
             'gen_stubs.c', 'variables.h', 'rom_identity.txt'):
    dest = ROOT / ('rom_identity.txt' if name == 'rom_identity.txt' else 'src/' + name)
    if dest.exists():
        continue
    text = (templates / (name + '.in')).read_text(encoding='utf-8')
    for key, value in tokens.items():
        text = text.replace('@' + key + '@', value)
    if '@DISPLAY_NAME@' in text:
        raise RuntimeError('unexpanded template')
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_text(text, encoding='utf-8')
print('Host sources ready; existing files preserved.')
