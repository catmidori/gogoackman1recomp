"""Verify the GUI subsystem and every embedded icon image in the final PE."""
import ctypes
import struct
import sys
from pathlib import Path

exe, ico = map(Path, sys.argv[1:])
pe = exe.read_bytes()
nt = struct.unpack_from('<I', pe, 0x3c)[0]
assert pe[nt:nt + 4] == b'PE\0\0'
assert struct.unpack_from('<H', pe, nt + 24 + 68)[0] == 2, 'Expected Windows GUI subsystem'
icon = ico.read_bytes()
reserved, kind, count = struct.unpack_from('<HHH', icon)
assert (reserved, kind, count) == (0, 1, 9)
images = {}
for i in range(count):
    w, h, colors, reserved, planes, bits, size, offset = struct.unpack_from('<BBBBHHII', icon, 6 + i * 16)
    images[(w or 256, h or 256)] = icon[offset:offset + size]
assert set(images) == {(n, n) for n in (16, 20, 24, 32, 40, 48, 64, 128, 256)}

kernel = ctypes.WinDLL('kernel32', use_last_error=True)
kernel.LoadLibraryExW.argtypes = [ctypes.c_wchar_p, ctypes.c_void_p, ctypes.c_uint]
kernel.LoadLibraryExW.restype = ctypes.c_void_p
kernel.FindResourceW.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]
kernel.FindResourceW.restype = ctypes.c_void_p
kernel.SizeofResource.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
kernel.SizeofResource.restype = ctypes.c_uint
kernel.LoadResource.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
kernel.LoadResource.restype = ctypes.c_void_p
kernel.LockResource.argtypes = [ctypes.c_void_p]
kernel.LockResource.restype = ctypes.c_void_p
kernel.FreeLibrary.argtypes = [ctypes.c_void_p]
module = kernel.LoadLibraryExW(str(exe.resolve()), None, 0x22)
assert module, ctypes.get_last_error()

def resource(kind, ident):
    info = kernel.FindResourceW(module, ident, kind)
    assert info, f'Missing resource {kind}/{ident}'
    size = kernel.SizeofResource(module, info)
    data = kernel.LockResource(kernel.LoadResource(module, info))
    assert data and size
    return ctypes.string_at(data, size)

try:
    group = resource(14, 1)
    assert struct.unpack_from('<HHH', group) == (0, 1, count)
    for i in range(count):
        w, h, colors, reserved, planes, bits, size, ident = struct.unpack_from('<BBBBHHIH', group, 6 + i * 14)
        payload = resource(3, ident)
        assert len(payload) == size
        assert payload == images[(w or 256, h or 256)], 'Embedded image differs from generated ICO'
finally:
    kernel.FreeLibrary(module)
print('Windows GUI subsystem and all nine embedded icon sizes verified.')
