"""Fit the official icon.png into a 360x360 Flatpak canvas; no Pillow needed.

The source is never rewritten. RGB/RGBA 8-bit non-interlaced PNGs are accepted;
Images that fit are copied unchanged with transparent, centred margins.
Larger images are reduced proportionally using nearest-neighbour sampling.
"""
from pathlib import Path
import struct
import sys
import zlib

ICON_SIZE = 360


def pixels(path):
    data = Path(path).read_bytes()
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError('Expected PNG')
    offset, payload, header = 8, bytearray(), None
    while offset < len(data):
        size = struct.unpack_from('>I', data, offset)[0]
        kind = data[offset+4:offset+8]
        body = data[offset+8:offset+8+size]
        crc = struct.unpack_from('>I', data, offset+8+size)[0]
        if zlib.crc32(kind+body) != crc:
            raise ValueError('PNG checksum mismatch')
        if kind == b'IHDR':
            header = struct.unpack('>IIBBBBB', body)
        elif kind == b'IDAT':
            payload.extend(body)
        offset += size + 12
        if kind == b'IEND':
            break
    width, height, depth, color, compression, filtering, interlace = header
    if depth != 8 or color not in (2, 6) or compression or filtering or interlace:
        raise ValueError('Expected non-interlaced RGB/RGBA 8-bit PNG')
    if not 1 <= max(width, height) <= 512:
        raise ValueError('Icon must be at most 512 pixels')
    channels = 3 if color == 2 else 4
    stride = width * channels
    raw = zlib.decompress(payload)
    if len(raw) != (stride+1)*height:
        raise ValueError('PNG row size mismatch')
    previous, rows = bytearray(stride), []
    for y in range(height):
        mode = raw[y*(stride+1)]
        row = bytearray(raw[y*(stride+1)+1:(y+1)*(stride+1)])
        for x in range(stride):
            left = row[x-channels] if x >= channels else 0
            up = previous[x]
            diagonal = previous[x-channels] if x >= channels else 0
            if mode == 0:
                predictor = 0
            elif mode == 1:
                predictor = left
            elif mode == 2:
                predictor = up
            elif mode == 3:
                predictor = (left+up)//2
            elif mode == 4:
                p = left+up-diagonal
                a, b, c = abs(p-left), abs(p-up), abs(p-diagonal)
                predictor = left if a <= b and a <= c else up if b <= c else diagonal
            else:
                raise ValueError('Unknown PNG filter')
            row[x] = (row[x]+predictor) & 255
        previous = row
        if channels == 3:
            row = bytearray(v for x in range(0, stride, 3) for v in (*row[x:x+3], 255))
        rows.append(bytes(row))
    return width, height, rows


def square(source, output):
    width, height, rows = pixels(source)
    side = ICON_SIZE
    if max(width, height) > side:
        original_width, original_height = width, height
        width = max(1, original_width * side // max(original_width, original_height))
        height = max(1, original_height * side // max(original_width, original_height))
        rows = [b''.join(rows[y * original_height // height][
                    (x * original_width // width)*4:(x * original_width // width)*4+4]
                    for x in range(width)) for y in range(height)]
    left, top = (side-width)//2, (side-height)//2
    canvas = [bytearray(side*4) for _ in range(side)]
    for y, row in enumerate(rows):
        canvas[y+top][left*4:(left+width)*4] = row

    def chunk(kind, body):
        return struct.pack('>I', len(body))+kind+body+struct.pack('>I', zlib.crc32(kind+body))

    data = b'\x89PNG\r\n\x1a\n'
    data += chunk(b'IHDR', struct.pack('>IIBBBBB', side, side, 8, 6, 0, 0, 0))
    data += chunk(b'IDAT', zlib.compress(b''.join(b'\0'+row for row in canvas), 6))
    data += chunk(b'IEND', b'')
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(data)


if __name__ == '__main__':
    square(*sys.argv[1:])
