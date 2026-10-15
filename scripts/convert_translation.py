"""Convert only the registered Ackman Translations IPS into guarded ROM hooks.

No third-party modules are required. The stock, unheadered Japanese ROM is
verified before its source bytes are embedded; the ROM itself is never changed.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import tempfile
import zipfile

OFFICIAL_IPS_MD5 = "0dd930e3738524cc4f8a3d83326616a9"
OFFICIAL_IPS_SIZE = 19872
ROM_SHA256 = "e8a33b6c8e15cf0aa60e9694471a51926fb97ad895d4714384c6abff28a33a1f"
NAME = "Go Go Ackman english translation"
VERSION = "1.0"
AUTHOR = "Ackman Translations"
DESCRIPTION = """Go Go Ackman1 English patch
http://tasam.com/~voltron/ackman

version 1.0

Contributers:
-Rom hacking: Equinox(equinox@fireduck.com)
-Script Translation: Akujin(http://members.xoom.com/_XMCM/akujin_/index.html)
-Initial Spot translation: Teunire

What might be done in future:
-Reword a few lines
-Get rid of ugly font

Tools used:
-thingy
-hexposure
-hex workshop
-NJ star
-relsearch

Thanks to:
-Akujin for showing me the empty space in the rom along with the wonderful translation work
-Jair for having such a wonderful tuturial at http://fly.to/vale to get me started"""


def parse_ips(data):
    """Strict, bounded IPS records (including RLE); no ROM resizing supported."""
    if not data.startswith(b"PATCH"):
        raise ValueError("Invalid IPS signature")
    cursor, records = 5, []

    def take(size):
        nonlocal cursor
        if cursor + size > len(data):
            raise ValueError("Truncated IPS record")
        chunk = data[cursor:cursor + size]
        cursor += size
        return chunk

    while True:
        address = take(3)
        if address == b"EOF":
            if cursor != len(data):
                raise ValueError("IPS resizing/trailing bytes are unsupported")
            return records
        length = int.from_bytes(take(2), "big")
        if length:
            payload = take(length)
        else:
            length = int.from_bytes(take(2), "big")
            if not length:
                raise ValueError("Empty IPS RLE record")
            payload = take(1) * length
        records.append((int.from_bytes(address, "big"), payload))


def rom_hooks(rom, records):
    """Flatten overlaps in IPS order, then emit nonoverlapping source guards."""
    patched, touched = bytearray(rom), bytearray(len(rom))
    for offset, payload in records:
        end = offset + len(payload)
        if end > len(rom):
            raise ValueError("IPS record exceeds the verified ROM")
        patched[offset:end] = payload
        touched[offset:end] = b"\1" * len(payload)
    hooks, offset = [], 0
    while offset < len(rom):
        if not touched[offset]:
            offset += 1
            continue
        end = offset + 1
        while end < len(rom) and touched[end]:
            end += 1
        hooks.append((offset, rom[offset:end], bytes(patched[offset:end])))
        offset = end
    return hooks


def convert(ips_path, rom_path, output):
    ips_path, rom_path, output = map(Path, (ips_path, rom_path, output))
    if ips_path.stat().st_size != OFFICIAL_IPS_SIZE:
        raise ValueError("Unsupported IPS: official translation MD5 required")
    ips = ips_path.read_bytes()
    if hashlib.md5(ips).hexdigest() != OFFICIAL_IPS_MD5:
        raise ValueError("Unsupported IPS: official translation MD5 required")
    rom = rom_path.read_bytes()
    if len(rom) % 32768 == 512:
        rom = rom[512:]
    if hashlib.sha256(rom).hexdigest() != ROM_SHA256:
        raise ValueError("The supported stock Japanese ROM is required")
    records = parse_ips(ips)
    hooks = rom_hooks(rom, records)
    # Manifest parser accepts scalar escaped strings, not multiline TOML strings.
    quote = lambda value: json.dumps(value, ensure_ascii=False)
    manifest = '\n'.join([
        'format_version = 1', 'id = "ackman.translation.en"',
        f'version = {quote(VERSION)}', f'name = {quote(NAME)}',
        f'author = {quote(AUTHOR)}', f'description = {quote(DESCRIPTION)}',
        'resolver = "declarative"', 'save_compatibility = "shared"', '',
        '[[target]]', 'game_id = "go-go-ackman-jp"',
        f'rom_sha256 = "{ROM_SHA256}"', '', '[[feature]]',
        'id = "english"', f'name = {quote(NAME)}',
        'default_enabled = false', '', '[[plugin]]',
        'feature = "english"', 'id = "ackman.translation.en"', '',
    ])
    table = ['schema = 1', 'default_lang = "en"', '']
    for address, source, target in hooks:
        table.extend(['[[rom_patch]]', f'address = 0x{address:x}',
                      f'source_hex = "{source.hex()}"',
                      f'en_hex = "{target.hex()}"', ''])
    audit = dict(ips_md5=OFFICIAL_IPS_MD5, ips_size=len(ips),
                 ips_address_bias=0, rom_sha256=ROM_SHA256,
                 ips_records=len(records), rom_hooks=len(hooks),
                 patched_bytes=sum(len(h[1]) for h in hooks),
                 translated_rom_sha256=hashlib.sha256(
                     apply_hooks(rom, hooks)).hexdigest(),
                 activation="Stage 2: game-owned ackman.translation.en plugin")
    output.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=".translation-", suffix=".tmp",
                                     dir=output.parent)
    os.close(fd)
    try:
        with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_STORED,
                             allowZip64=False) as archive:
            for name, text in (("manifest.toml", manifest),
                               ("translation.toml", '\n'.join(table)),
                               ("conversion.json", json.dumps(audit, indent=2))):
                entry = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
                entry.external_attr = 0o100644 << 16
                archive.writestr(entry, text.encode("utf-8"))
        os.replace(temporary, output)
    finally:
        Path(temporary).unlink(missing_ok=True)
    return audit


def apply_hooks(rom, hooks):
    result = bytearray(rom)
    for address, source, target in hooks:
        assert result[address:address + len(source)] == source
        result[address:address + len(source)] = target
    return result


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ips", type=Path, required=True)
    parser.add_argument("--rom", type=Path, default=root / "rom/Go Go Ackman (Japan).sfc")
    parser.add_argument("--output", type=Path, default=root / "mods/GoGoAckman_EN.snesmod")
    parser.add_argument("--receipt", type=Path, help="Fresh completion marker for the asynchronous menu")
    args = parser.parse_args()
    if args.receipt:
        args.receipt.unlink(missing_ok=True)
    try:
        audit = convert(args.ips, args.rom, args.output)
        if args.receipt:
            args.receipt.write_text(OFFICIAL_IPS_MD5, encoding="ascii")
        print(json.dumps(audit, indent=2))
        return 0
    except (OSError, ValueError, zipfile.BadZipFile) as error:
        parser.exit(1, f"Translation conversion failed: {error}\n")


if __name__ == "__main__":
    raise SystemExit(main())
