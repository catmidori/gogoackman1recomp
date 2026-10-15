"""Remove only this project's temporary AckmanRecomp module builds."""
from pathlib import Path
import shutil


def clean(root):
    root = Path(root).resolve(strict=True)
    state = root / '.flatpak-builder'
    builds = state / 'build'
    # Never follow a cache directory redirected outside the project.
    for directory in (state, builds):
        if directory.is_symlink():
            raise ValueError(f'Refusing symlinked cache directory: {directory}')
        if directory.exists() and not directory.resolve().is_relative_to(root):
            raise ValueError(f'Cache path escapes project: {directory}')
    targets = list(builds.glob('AckmanRecomp-*')) if builds.exists() else []
    alias = builds / 'AckmanRecomp'
    if alias.is_symlink():
        targets.append(alias)
    cache = state/'cache'
    if cache.is_symlink():
        raise ValueError(f'Refusing symlinked cache metadata: {cache}')
    if cache.exists():
        targets.append(cache)
    # Validate all real targets before removing anything.
    for target in targets:
        if not target.is_symlink() and not target.resolve().is_relative_to(state.resolve()):
            raise ValueError(f'Build path escapes cache: {target}')
    for target in targets:
        if target.is_symlink() or target.is_file():
            target.unlink()
        else:
            shutil.rmtree(target)
        print(f'Removed temporary build: {target.name}')
    return len(targets)


if __name__ == '__main__':
    count = clean(Path(__file__).resolve().parents[1])
    print(f'Flatpak clean sweep: {count} temporary module paths removed')
