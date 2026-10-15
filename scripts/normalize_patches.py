"""Normalize patch files and their existing text targets before git apply.

Byte-only CRLF -> LF conversion preserves encodings, content and final-newline
status. Only paths named by patch headers inside the supplied checkout are
touched; binary files and Git metadata are never rewritten.
"""
from pathlib import Path, PurePosixPath
import argparse


def normalize(patch_directory, framework=None):
    patch_directory = Path(patch_directory).resolve()
    if not patch_directory.is_dir():
        raise ValueError('Patch directory does not exist')
    framework = Path(framework).resolve() if framework is not None else None
    if framework is not None and not framework.is_dir():
        raise ValueError('Framework directory does not exist')
    patches = sorted(p for p in patch_directory.rglob('*') if p.suffix.lower() in ('.patch', '.diff'))
    planned, targets = {}, set()
    for patch in patches:
        if not patch.is_file() or patch.is_symlink() or not patch.resolve().is_relative_to(patch_directory):
            raise ValueError(f'Unsafe patch path: {patch}')
        data = patch.read_bytes()
        if b'\0' in data:
            raise ValueError(f'Expected a text patch: {patch}')
        canonical = data.replace(b'\r\n', b'\n')
        planned[patch] = (data, canonical)
        if framework is None:
            continue
        for line in canonical.split(b'\n'):
            if not line.startswith((b'--- a/', b'+++ b/')):
                continue
            name = line[6:].split(b'\t', 1)[0].decode('utf-8')
            relative = PurePosixPath(name)
            if relative.is_absolute() or '..' in relative.parts or '\\' in name or '.git' in relative.parts:
                raise ValueError(f'Unsafe patch target: {name}')
            target = framework.joinpath(*relative.parts)
            if target.is_symlink() or not target.resolve().is_relative_to(framework):
                raise ValueError(f'Unsafe patch target: {name}')
            targets.add(target)
    for target in sorted(targets):
        if not target.exists():
            continue  # New files are created later by git apply.
        if not target.is_file():
            raise ValueError(f'Expected a file: {target}')
        data = target.read_bytes()
        if b'\0' not in data:
            planned[target] = (data, data.replace(b'\r\n', b'\n'))
    changed = 0
    for path, (before, after) in planned.items():
        if before != after:
            path.write_bytes(after)
            changed += 1
    return len(patches), len(targets), changed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--patches', type=Path, required=True)
    parser.add_argument('--framework', type=Path)
    args = parser.parse_args()
    patches, targets, changed = normalize(args.patches, args.framework)
    print(f'Patch line endings: {patches} patches, {targets} target paths, {changed} files normalized to LF')


if __name__ == '__main__':
    main()
