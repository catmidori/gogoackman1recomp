"""Stage only approved release files and resolve the complete PE DLL closure."""
import argparse
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
FOLDERS = ('docs', 'mods', 'rom', 'saves', 'shaders')


def imports(path, objdump):
    result = subprocess.run([objdump, '-p', str(path)], check=True, capture_output=True, text=True)
    return re.findall(r'DLL Name:\s*(\S+)', result.stdout)


def package(build, runtime, objdump, output):
    build = build.resolve(strict=True)
    stage = build / 'release-stage'
    if stage.is_symlink():
        raise ValueError('Release staging must not be a symlink')
    if stage.exists():
        shutil.rmtree(stage)
    stage.mkdir()
    shutil.copy2(build / 'AckmanRecomp.exe', stage)
    for name in ('README.md', 'THIRD_PARTY_ATTRIBUTION.md', 'LICENSE', 'RELEASE_NOTES.md'):
        shutil.copy2(ROOT / name, stage)
    for folder in FOLDERS:
        (stage / folder).mkdir()
        shutil.copy2(ROOT / folder / 'Instructions.txt', stage / folder)
    # SVGs and the font are embedded; retain the file fallback and its notice too.
    (stage / 'assets/fonts').mkdir(parents=True)
    for name in ('OpenSans-Regular.ttf', 'OFL.txt'):
        shutil.copy2(ROOT / 'assets/fonts' / name, stage / 'assets/fonts')
    system = Path(os.environ.get('SystemRoot', 'C:/Windows')) / 'System32'
    directories = (build, runtime.resolve())
    indexes = [{p.name.lower(): p for p in d.glob('*.dll')} for d in directories]
    pending, seen = [stage / 'AckmanRecomp.exe'], set()
    while pending:
        binary = pending.pop()
        if binary.name.lower() in seen:
            continue
        seen.add(binary.name.lower())
        # Every packaged image must be an x86-64 PE, not a 32-bit dependency.
        data = binary.read_bytes()
        offset = int.from_bytes(data[60:64], 'little')
        if data[offset:offset+6] != b'PE\0\0\x64\x86':
            raise ValueError(f'Not an x86-64 PE: {binary.name}')
        for name in imports(binary, objdump):
            key = name.lower()
            source = next((index[key] for index in indexes if key in index), None)
            # OS DLLs and API-set contracts are provided by Windows, never bundled.
            if source is None:
                if key.startswith(('api-ms-win-', 'ext-ms-win-')) or (system / name).exists():
                    continue
                raise FileNotFoundError(f'Unresolved runtime dependency: {name}')
            destination = stage / source.name
            if not destination.exists():
                shutil.copy2(source, destination)
            pending.append(destination)
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix('.zip.tmp')
    with zipfile.ZipFile(temporary, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for path in sorted(stage.rglob('*')):
            if path.is_file():
                archive.write(path, path.relative_to(stage).as_posix())
    with zipfile.ZipFile(temporary) as archive:
        assert archive.testzip() is None
        names = archive.namelist()
        for folder in FOLDERS:
            assert [n for n in names if n.startswith(folder + '/')] == [folder + '/Instructions.txt']
        assert not any(n.endswith(('.sfc', '.smc', '.sav', '.ips', '.snesmod', '.msu', '.pcm', '.glsl', '.glslp')) for n in names)
        assert 'config.ini' not in names
    temporary.replace(output)
    print(f'PASS x64 DLL closure and content-free package: {output}')
    print(f'SHA256 {hashlib.sha256(output.read_bytes()).hexdigest()}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--runtime-dir', type=Path, required=True)
    parser.add_argument('--objdump', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    package(args.build, args.runtime_dir, args.objdump, args.output)
