"""Local stock-ROM integration: boot sanitation, persistence and OS cold boot.

Run from the project root after building build-validation. No ROM is shipped.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'private/active-mods-candidate/runtime'
WORK.mkdir(parents=True, exist_ok=True)
(WORK / 'mods').mkdir(exist_ok=True)
EXE = WORK / 'AckmanRecomp.exe'
shutil.copy2(ROOT / 'build-validation/AckmanRecomp.exe', EXE)
PACKAGE = WORK / 'mods/GoGoAckman_EN.snesmod'
shutil.copy2(os.environ.get('ACKMAN_TEST_PACKAGE', str(ROOT / 'mods/GoGoAckman_EN.snesmod')), PACKAGE)
ROM = ROOT / 'rom/Go Go Ackman (Japan).sfc'
CONFIG = WORK / 'test.ini'
TRACE = WORK / 'translation.log'
ENV = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy',
           SNESRECOMP_RUN_FRAMES='1000', ACKMAN_MODS_TRACE=str(TRACE))
BASE = (ROOT / 'config.ini').read_text(encoding='utf-8').split('[ActiveMods]')[0]
BASE = BASE.replace('DisableFrameDelay = 0', 'DisableFrameDelay = 1').replace('VSync = 1', 'VSync = 0')
BASE = re.sub(r'^Autosave\s*=.*$', 'Autosave = 0', BASE, flags=re.M)
GOLD = '4dac0fca32d47e1d6929677b9f6ab8f6840a0504c9d20dc534a4c8cf41288f74'


def configure(names):
    CONFIG.write_text(BASE + '\n[ActiveMods]\n' + ''.join(n + '\n' for n in names), encoding='utf-8')
    TRACE.unlink(missing_ok=True)


def run(name, script=None, extra=None):
    args = [str(EXE), '--config', str(CONFIG), '--no-launcher', str(ROM)]
    if script:
        path = WORK / (name + '.txt')
        path.write_text(script)
        args[3:3] = ['--script', str(path)]
    with (WORK / (name + '.log')).open('w', encoding='utf-8') as log:
        result = subprocess.run(args, cwd=WORK, env=dict(ENV, **(extra or {})),
                                stdout=log, stderr=log, timeout=45)
    text = (WORK / (name + '.log')).read_text(encoding='utf-8')
    assert result.returncode == 0, text
    assert not re.search(r'CRASH|watchdog|plan rejected|unknown host option', text, re.I), text
    return text


configure(['GoGoAckman_EN.snesmod', 'deleted.snesmod', '../outside.snesmod'])
text = run('english')
assert GOLD in text and 'RUN_FRAMES reached after 1000 frames' in text, text
assert 'deleted.snesmod' not in CONFIG.read_text() and '../outside' not in CONFIG.read_text()
assert TRACE.read_text().count(GOLD) == 1
print('PASS actual core English hooks and 1000 frames; missing/unsafe INI entries pruned')

configure([])
text = run('disabled')
assert 'English translation applied' not in text and not TRACE.exists(), text
print('PASS empty INI disables the previously installed/active cached package')

bad = WORK / 'mods/bad.snesmod'
bad.write_bytes(b'not a ZIP archive')
configure(['bad.snesmod'])
text = run('invalid')
assert 'Skipped unavailable or incompatible package' in text and 'bad.snesmod' not in CONFIG.read_text(), text
bad.unlink()
print('PASS incompatible archive cannot crash boot or become active')

PACKAGE.rename(WORK / 'mods/temporarily-moved.snesmod')
configure(['GoGoAckman_EN.snesmod'])
text = run('deleted')
assert 'English translation applied' not in text and 'GoGoAckman_EN.snesmod' not in CONFIG.read_text(), text
(WORK / 'mods/temporarily-moved.snesmod').rename(PACKAGE)
print('PASS deleted archive cannot be resurrected from runtime cache')

msu_folder = WORK / 'mods/msu-test/audio'
msu_folder.mkdir(parents=True, exist_ok=True)
(msu_folder / 'music.msu').write_bytes(b'')
(msu_folder / 'manifest.bml').write_text('information\n  title: Test music\n  author: Test composer\n  version: 1.0\n  description: Synthetic MSU fixture\n')
configure(['msu-test/audio/music.msu'])
text = run('msu')
assert '[mods] MSU-1 source selected: msu-test/audio/music.msu' in text and '[MSU-1] enabled:' in text, text
assert 'msu-test/audio/music.msu' in CONFIG.read_text()
assert 'Skipped unavailable or incompatible' not in text
configure([])
text = run('msu-disabled')
assert '[MSU-1] enabled:' not in text, text
(msu_folder / 'music.msu').unlink()
(msu_folder / 'manifest.bml').unlink()
msu_folder.rmdir()
msu_folder.parent.rmdir()
print('PASS recursive MSU path survives INI/boot, uses native MSU device and disables on next boot')

configure([])
timeline = WORK / 'coldboot.csv'
window_trace = WORK / 'coldboot-window.log'
window_trace.unlink(missing_ok=True)
CONFIG.write_text(re.sub(r'^Fullscreen\s*=.*$', 'Fullscreen = 0', CONFIG.read_text(), flags=re.M))
text = run('coldboot', 'wait 5\nhostoption Fullscreen 1\nhostoption Mod 0\nwait 5\nhostoption Reboot 1\n',
           dict(ACKMAN_TRACE=str(timeline), ACKMAN_WINDOW_TRACE=str(window_trace),
                SDL_VIDEODRIVER='windows' if os.environ.get('ACKMAN_TEST_NATIVE_WINDOWS') else 'dummy'))
assert 'English translation applied' not in text, text  # The old core remains stock.
deadline = time.monotonic() + 25
while time.monotonic() < deadline:
    if TRACE.exists() and GOLD in TRACE.read_text():
        rows = timeline.read_text().splitlines() if timeline.exists() else []
        if rows and rows[-1].split(',')[0].isdigit() and int(rows[-1].split(',')[0]) >= 999:
            break
    time.sleep(.1)
else:
    raise AssertionError('Cold-boot child did not apply English and complete 1000 frames')
assert TRACE.read_text().count(GOLD) == 1  # One fresh child activation, not a logical CPU reset.
assert 'GoGoAckman_EN.snesmod' in CONFIG.read_text()
windows = window_trace.read_text().splitlines()
assert len(windows) == 2 and windows[0].startswith('fullscreen=0') and windows[1].startswith('fullscreen=1'), windows
if os.environ.get('ACKMAN_TEST_NATIVE_WINDOWS'):
    assert 'raised=1 foreground=1' in windows[1], windows
print('PASS actual OS cold boot: native-generated English, 1000 frames, child created directly fullscreen with raise request')
