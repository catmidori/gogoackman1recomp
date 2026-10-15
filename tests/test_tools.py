import io
from pathlib import Path
import sys
import tempfile
import unittest
import hashlib
import json
import tomllib
import zipfile
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from regen import normalized
from compare_trace import compare
from normalize_patches import normalize as normalize_patches
from clean_flatpak_build import clean as clean_flatpak_build
from normalize_build_timestamps import normalize as normalize_build_timestamps
from check_timing import check, FIELD_CLOCKS
from convert_translation import (parse_ips, rom_hooks, apply_hooks, convert,
                                 OFFICIAL_IPS_MD5, NAME, AUTHOR, DESCRIPTION)

class ToolTests(unittest.TestCase):
    def test_future_staging_timestamps_are_clamped_once(self):
        import os, time
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'CMakeLists.txt';path.write_bytes(b'unchanged')
            future=time.time_ns()+3600*1000000000
            os.utime(path,ns=(future,future))
            self.assertEqual(normalize_build_timestamps(directory),1)
            self.assertEqual(path.read_bytes(),b'unchanged')
            self.assertLessEqual(path.stat().st_mtime_ns,time.time_ns())
            self.assertEqual(normalize_build_timestamps(directory),0)

    def test_flatpak_clean_sweep_is_scoped_and_repeatable(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            builds = root/'.flatpak-builder/build'
            (builds/'AckmanRecomp-1').mkdir(parents=True)
            (builds/'AckmanRecomp-1/build.ninja').write_text('stale')
            (builds/'SDL3-1').mkdir()
            (root/'saves').mkdir()
            (root/'saves/keep.sav').write_bytes(b'keep')
            self.assertEqual(clean_flatpak_build(root), 1)
            self.assertFalse((builds/'AckmanRecomp-1').exists())
            self.assertTrue((builds/'SDL3-1').is_dir())
            self.assertEqual((root/'saves/keep.sav').read_bytes(), b'keep')
            self.assertEqual(clean_flatpak_build(root), 0)

    def test_host_report_gnu_api_is_enabled_before_forced_headers(self):
        root = Path(__file__).resolve().parents[1]
        report = (root/'external/snesrecomp/runner/src/host_report.c').read_text(encoding='utf-8')
        self.assertTrue(report.startswith('#ifndef _GNU_SOURCE\n#define _GNU_SOURCE 1\n#endif\n'))
        self.assertIn('#if defined(__linux__)\n#include <link.h>\n#endif', report)
        cmake = (root/'CMakeLists.txt').read_text(encoding='utf-8')
        self.assertIn('if(CMAKE_SYSTEM_NAME STREQUAL "Linux")\n  # -D is processed before the forced logging header includes glibc headers.\n  set_property(SOURCE "${SNESRECOMP_ROOT}/runner/src/host_report.c" APPEND\n    PROPERTY COMPILE_DEFINITIONS _GNU_SOURCE=1)\nendif()', cmake)

    def test_flatpak_preparation_is_outside_cmake_regeneration(self):
        root = Path(__file__).resolve().parents[1]
        cmake = (root/'CMakeLists.txt').read_text(encoding='utf-8')
        self.assertNotIn('normalize_patches.py', cmake)
        self.assertNotIn('ACKMAN_LOCAL_PATCHES', cmake)
        self.assertIn('if(NOT ACKMAN_FLATPAK)\n  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/AckmanPatches.cmake")\nendif()', cmake)
        manifest = (root/'org.snesrecomp.AckmanRecomp.yaml').read_text(encoding='utf-8')
        checkout = manifest.index('dest: external/snesrecomp')
        normalize = manifest.index('python3 scripts/normalize_patches.py --patches patches --framework external/snesrecomp')
        apply = manifest.index('cmake -P cmake/AckmanPatches.cmake')
        self.assertLess(checkout, normalize)
        self.assertLess(normalize, apply)

    def test_patch_crlf_normalization_is_exact_and_idempotent(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            patches, framework = root/'patches', root/'framework'
            patches.mkdir();framework.mkdir()
            text = framework/'common_rtl.c'
            original = '// café\r\nint frame;\r\n'.encode('utf-8')
            text.write_bytes(original)
            binary = framework/'opaque.bin'
            binary.write_bytes(b'\0\r\nunchanged')
            patch = patches/'first.patch'
            patch.write_bytes(b'--- a/common_rtl.c\r\n+++ b/common_rtl.c\r\n@@ -1 +1 @@\r\n-old\r\n+new')
            diff = patches/'second.diff'
            diff.write_bytes(b'--- a/opaque.bin\r\n+++ b/opaque.bin\r\n--- /dev/null\r\n+++ b/new.h\r\n')
            self.assertEqual(normalize_patches(patches, framework), (2, 3, 3))
            self.assertEqual(text.read_bytes(), original.replace(b'\r\n', b'\n'))
            self.assertFalse(patch.read_bytes().endswith(b'\n'))
            self.assertEqual(binary.read_bytes(), b'\0\r\nunchanged')
            times = {p:p.stat().st_mtime_ns for p in (patch, diff, text, binary)}
            self.assertEqual(normalize_patches(patches, framework), (2, 3, 0))
            self.assertEqual(times, {p:p.stat().st_mtime_ns for p in times})

    def test_patch_targets_cannot_escape_checkout(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            patches, framework = root/'patches', root/'framework'
            patches.mkdir();framework.mkdir()
            outside = root/'outside.c';outside.write_bytes(b'keep\r\n')
            patch = patches/'bad.patch'
            original = b'--- a/../outside.c\r\n+++ b/../outside.c\r\n'
            patch.write_bytes(original)
            with self.assertRaises(ValueError):
                normalize_patches(patches, framework)
            self.assertEqual(outside.read_bytes(), b'keep\r\n')
            self.assertEqual(patch.read_bytes(), original)

    def test_translation_parser_bounds_and_overlap_guards(self):
        records = parse_ips(b'PATCH\0\0\1\0\2AB\0\0\2\0\0\0\3ZEOF')
        self.assertEqual(records, [(1, b'AB'), (2, b'ZZZ')])
        original = b'01234567'
        hooks = rom_hooks(original, records)
        self.assertEqual(hooks, [(1, b'1234', b'AZZZ')])
        self.assertEqual(apply_hooks(original, hooks), b'0AZZZ567')
        for data in (b'', b'PATCH', b'PATCHEOFjunk', b'PATCH\0\0\1\0\5a',
                     b'PATCH\0\0\1\0\0\0\0aEOF'):
            with self.assertRaises(ValueError):
                parse_ips(data)
        with self.assertRaises(ValueError):
            rom_hooks(original, [(7, b'AB')])

    def test_translation_rejection_does_not_overwrite_output(self):
        with tempfile.TemporaryDirectory() as folder:
            folder = Path(folder)
            ips, rom, output = (folder / name for name in ('bad.ips', 'bad.sfc', 'old.snesmod'))
            ips.write_bytes(bytes(19872))
            rom.write_bytes(b'ROM')
            output.write_bytes(b'existing package')
            with self.assertRaises(ValueError):
                convert(ips, rom, output)
            self.assertEqual(output.read_bytes(), b'existing package')
            self.assertFalse(list(folder.glob('.translation-*')))
        registry = (Path(__file__).resolve().parents[1] / 'src/ackman_translation.h').read_text()
        self.assertIn(OFFICIAL_IPS_MD5, registry)

    def test_generated_translation_exact_metadata_and_source_guards(self):
        root = Path(__file__).resolve().parents[1]
        package = root / 'mods/GoGoAckman_EN.snesmod'
        rom_path = root / 'rom/Go Go Ackman (Japan).sfc'
        if not package.exists() or not rom_path.exists():
            self.skipTest('Local official translation package/stock ROM not supplied')
        with zipfile.ZipFile(package) as archive:
            self.assertIsNone(archive.testzip())
            manifest = tomllib.loads(archive.read('manifest.toml').decode())
            self.assertEqual((manifest['name'], manifest['version'], manifest['author'], manifest['description']),
                             (NAME, '1.0', AUTHOR, DESCRIPTION))
            table = tomllib.loads(archive.read('translation.toml').decode())
            hooks = [(p['address'], bytes.fromhex(p['source_hex']), bytes.fromhex(p['en_hex']))
                     for p in table['rom_patch']]
            audit = json.loads(archive.read('conversion.json'))
            self.assertEqual(audit['ips_md5'], OFFICIAL_IPS_MD5)
            self.assertEqual(len(hooks), 48)
            self.assertEqual(sum(len(h[1]) for h in hooks), 19624)
            rom = rom_path.read_bytes()
            self.assertEqual(hashlib.sha256(apply_hooks(rom, hooks)).hexdigest(),
                             audit['translated_rom_sha256'])
    def test_event_cadence_rejects_synthetic_polling_ticks(self):
        a = dict(frame=1, vblank_edge=100, cpu_boundary=118, late_master_clocks=18)
        b = dict(frame=2, vblank_edge=100+FIELD_CLOCKS,
                 cpu_boundary=104+FIELD_CLOCKS, late_master_clocks=4)
        self.assertEqual(check([a, b]), (4, 18))
        b['vblank_edge'] -= 64
        b['cpu_boundary'] -= 64
        with self.assertRaises(ValueError):
            check([a, b])
    def test_header_and_truncation(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'synthetic.smc'
            payload = bytes(range(256)) * 128
            path.write_bytes(payload)
            self.assertEqual(normalized(path), payload)
            path.write_bytes(bytes(512) + payload)
            self.assertEqual(normalized(path), payload)
            path.write_bytes(payload[:-1])
            with self.assertRaises(ValueError):
                normalized(path)

    def test_trace_detects_divergence_and_missing_tail(self):
        header = 'frame,master_cycles,hpos,vpos,irq_latches,irq_missed,guest_ms\n'
        a = header + '1,100,2,3,4,0,5\n'
        b = header + '1,101,2,3,4,0,5\n'
        self.assertIn('master_cycles', compare(io.StringIO(a), io.StringIO(b)))
        self.assertIn('lengths', compare(io.StringIO(a), io.StringIO(header)))
        self.assertIsNone(compare(io.StringIO(a), io.StringIO(a.replace(',5\n', ',9\n'))))

if __name__ == '__main__':
    if len(sys.argv)==2 and Path(sys.argv[1]).is_file():
        import subprocess
        subprocess.run([sys.argv.pop()],check=True)
    unittest.main()
