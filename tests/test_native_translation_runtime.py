"""Local integration using the user's IPS/ROM; no copyrighted fixtures shipped."""
from pathlib import Path
import hashlib
import os
import shutil
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'private/native-translation-candidate/runtime'
WORK.mkdir(parents=True, exist_ok=True)
EXE = WORK / 'ackman_menu_test.exe'
shutil.copy2(ROOT / 'build-validation/ackman_menu_test.exe', EXE)
assert not (WORK / 'scripts').exists()
env = dict(os.environ, PATH='', SDL_VIDEODRIVER='dummy',
           ACKMAN_TEST_OFFICIAL_IPS='R:/ChatGPT/Ackman1e.ips',
           ACKMAN_TEST_STOCK_ROM=str(ROOT / 'rom/Go Go Ackman (Japan).sfc'))
with (WORK / 'native.log').open('w') as log:
    result = subprocess.run([str(EXE)], cwd=WORK, env=env,
                            stdout=log, stderr=log, timeout=30)
assert result.returncode == 0, (WORK / 'native.log').read_text()
package = WORK / 'mods/GoGoAckman_EN.snesmod'
assert hashlib.sha256(package.read_bytes()).hexdigest() == (
    '7171e0423a68bb6a571bb9e54b6fd4300ff0a7438599125c069d170c7e5ee14e'
), 'Native ZIP differs from the registered validated package'
with zipfile.ZipFile(package) as z:
    assert z.testzip() is None
    assert set(z.namelist()) == {'manifest.toml', 'translation.toml', 'conversion.json'}
assert not list(package.parent.glob('.translation-*.tmp'))
print('PASS native conversion without scripts or Python on PATH; exact manifest/table/ZIP parity')
print('SHA256=' + hashlib.sha256(package.read_bytes()).hexdigest())
