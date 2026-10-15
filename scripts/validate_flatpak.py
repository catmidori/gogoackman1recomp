"""Validate the package contract; accepts Flatpak's canonical JSON or YAML.

With --canonical-json no extra Python dependency is needed. Reading YAML
directly requires PyYAML (development-only, never used by the application).
"""
from pathlib import Path
import argparse
import configparser
import hashlib
import json
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from build_flatpak_icon import pixels


def validate(manifest, stage=None):
    app_id = 'org.snesrecomp.AckmanRecomp'
    assert manifest.get('app-id', manifest.get('id')) == app_id
    assert manifest['runtime'] == 'org.freedesktop.Platform'
    assert manifest['sdk'] == 'org.freedesktop.Sdk'
    assert manifest['runtime-version'] == '26.08'
    assert manifest['command'] == 'AckmanRecomp'
    grants = set(manifest['finish-args'])
    assert {'--socket=x11', '--socket=wayland', '--device=dri', '--device=all',
            '--socket=pulseaudio', '--share=ipc'} <= grants
    assert not any(g.startswith('--filesystem=') for g in grants)
    assert '--env=SDL_FILE_DIALOG_DRIVER=portal' in grants
    modules = manifest['modules']
    assert [m['name'] for m in modules] == ['zlib', 'SDL3', 'AckmanRecomp']
    for module in modules:
        assert module['buildsystem'] == 'cmake-ninja'
        assert isinstance(module['config-opts'], list)
        for source in module['sources']:
            if source['type'] == 'archive':
                assert source['url'].startswith('https://')
                assert re.fullmatch('[a-f0-9]{64}', source['sha256'])
    zlib, sdl, game = modules
    assert {'-DZLIB_BUILD_SHARED=OFF', '-DZLIB_BUILD_STATIC=ON', '-DZLIB_BUILD_TESTING=OFF'} <= set(zlib['config-opts'])
    assert zlib['sources'][0]['sha256'] == 'bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16'
    framework = (ROOT / 'external/snesrecomp/runner/runner.cmake').read_text(encoding='utf-8')
    assert sdl['sources'][0]['sha256'] in framework
    assert 'SDL3-3.4.10.tar.gz' in sdl['sources'][0]['url']
    assert '-DSDL_DBUS=ON' in sdl['config-opts']
    assert {'-DACKMAN_FLATPAK=ON', '-DBUILD_TESTING=OFF', '-DSNESRECOMP_SDL3_FETCH=OFF',
            '-DZLIB_LIBRARY_RELEASE=/app/lib/libz.a'} <= set(game['config-opts'])
    local = game['sources'][0]
    git = next(source for source in game['sources'] if source['type'] == 'git')
    preparation = game['sources'][-1]
    instruction_sources = {source['dest']: source['path'] for source in game['sources'] if source['type'] == 'file'}
    assert instruction_sources == {folder: f'{folder}/Instructions.txt' for folder in ('rom', 'mods', 'saves')}
    assert preparation == {'type': 'shell', 'commands': [
        'python3 scripts/normalize_patches.py --patches patches --framework external/snesrecomp',
        'cmake -P cmake/AckmanPatches.cmake',
        'python3 scripts/normalize_build_timestamps.py']}
    assert local['type'] == 'dir' and Path(local['path']).resolve() == ROOT
    assert {'rom', 'saves', 'mods', 'fallback', 'private', '.tools', 'config.ini',
            '.flatpak-builder', '.flatpak-build', '.flatpak-repo', 'external/snesrecomp'} <= set(local['skip'])
    assert git['type'] == 'git' and git['dest'] == 'external/snesrecomp'
    assert git['commit'] == (ROOT / 'framework.lock').read_text().strip()
    assert any('readelf' in line for line in game['post-install'])
    desktop = configparser.ConfigParser(interpolation=None)
    desktop.optionxform = str
    desktop.read(ROOT / f'{app_id}.desktop', encoding='utf-8')
    assert dict(desktop['Desktop Entry']) == {
        'Name': 'Go Go Ackman Recompiled', 'Exec': 'AckmanRecomp',
        'Icon': app_id, 'Terminal': 'false', 'Type': 'Application', 'Categories': 'Game;Emulator;'}
    if stage:
        stage = Path(stage)
        installed = stage / 'share/applications' / f'{app_id}.desktop'
        assert installed.read_bytes() == (ROOT / f'{app_id}.desktop').read_bytes()
        icon = stage / 'share/icons/hicolor/scalable/apps' / f'{app_id}.png'
        width, height, original = pixels(ROOT / 'icon.png')
        side, other_side, padded = pixels(icon)
        assert side == other_side == 360
        if max(width, height) > side:
            source_width, source_height = width, height
            width = max(1, source_width * side // max(source_width, source_height))
            height = max(1, source_height * side // max(source_width, source_height))
            original = [b''.join(original[y*source_height//height][
                (x*source_width//width)*4:(x*source_width//width)*4+4]
                for x in range(width)) for y in range(height)]
        x, y = (side-width)//2, (side-height)//2
        for row in range(side):
            if y <= row < y+height:
                assert padded[row][x*4:(x+width)*4] == original[row-y]
                assert not any(padded[row][:x*4] + padded[row][(x+width)*4:])
            else:
                assert not any(padded[row])
        data = stage / 'share' / app_id
        assert (data / 'fonts/OpenSans-Regular.ttf').is_file()
        assert (data / 'buttons/keyboard/keyboard_q_outline.svg').is_file()
        assert (data / 'readme.md').is_file()
        for notice in ('LICENSE', 'THIRD_PARTY_ATTRIBUTION.md', 'THIRD_PARTY.md'):
            assert (data / notice).read_bytes() == (ROOT / notice).read_bytes()
        assert (data / 'shaders/Instructions.txt').read_bytes() == (ROOT / 'shaders/Instructions.txt').read_bytes()
        for folder in ('docs', 'mods', 'rom', 'saves', 'shaders'):
            assert (data / 'user-template' / folder / 'Instructions.txt').read_bytes() == (ROOT / folder / 'Instructions.txt').read_bytes()
        for name in ('README.md', 'THIRD_PARTY_ATTRIBUTION.md', 'LICENSE', 'RELEASE_NOTES.md'):
            assert (data / 'user-template' / name).read_bytes() == (ROOT / name).read_bytes()
    print('PASS Flatpak manifest modules/pins, desktop identity, static zlib and sandbox contract')
    if stage:
        print('PASS install layout and square icon: every source pixel preserved, transparent padding only')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--canonical-json', type=Path)
    parser.add_argument('--stage', type=Path)
    args = parser.parse_args()
    if args.canonical_json:
        manifest = json.loads(args.canonical_json.read_text(encoding='utf-8'))
    else:
        import yaml
        class UniqueLoader(yaml.SafeLoader):
            pass
        def unique_mapping(loader, node, deep=False):
            mapping = {}
            for key_node, value_node in node.value:
                key = loader.construct_object(key_node, deep=deep)
                if key in mapping:
                    raise ValueError(f'Duplicate YAML key: {key}')
                mapping[key] = loader.construct_object(value_node, deep=deep)
            return mapping
        UniqueLoader.add_constructor(yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG, unique_mapping)
        manifest = yaml.load((ROOT / 'org.snesrecomp.AckmanRecomp.yaml').read_text(encoding='utf-8'), Loader=UniqueLoader)
    validate(manifest, args.stage)


if __name__ == '__main__':
    main()
