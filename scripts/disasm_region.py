"""Local inspection through SNESRecomp's decoder; linear M/X is not CFG proof."""
import argparse
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'external/snesrecomp/recompiler'))
from snes65816 import load_rom, decode_insn, lorom_offset
ap = argparse.ArgumentParser()
ap.add_argument('--rom', required=True)
ap.add_argument('--start', type=lambda s: int(s, 16), required=True)
ap.add_argument('--end', type=lambda s: int(s, 16), required=True)
ap.add_argument('--m', type=int, default=1)
ap.add_argument('--x', type=int, default=1)
a = ap.parse_args()
rom = load_rom(a.rom)
pc, m, x = a.start, a.m, a.x
while pc < a.end:
    bank, addr = pc >> 16, pc & 65535
    ins = decode_insn(rom, lorom_offset(bank, addr), addr, bank, m, x)
    print(f'{pc:06X} {ins.mnem:4} {ins.mode:8} ${ins.operand:06X} M{m}X{x}')
    if ins.mnem == 'REP':
        if ins.operand & 32: m = 0
        if ins.operand & 16: x = 0
    if ins.mnem == 'SEP':
        if ins.operand & 32: m = 1
        if ins.operand & 16: x = 1
    pc += ins.length
