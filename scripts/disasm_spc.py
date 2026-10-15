"""Linear SPC RAM inspection using the pinned core's opcode annotations.

Operands are raw encoded bytes (not assembler syntax). This is not CFG proof.
"""
import argparse
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]

def opcodes():
    source = (ROOT / 'external/snesrecomp/runner/src/snes/spc.c').read_text()
    result, pending = {}, []
    for line in source.splitlines():
        match = re.search(r'case 0x([0-9a-f]+):(?: \{ // (.*))?', line)
        if not match:
            continue
        pending.append(int(match[1], 16))
        if match[2]:
            name, mode = match[2].split(maxsplit=1)
            length = (1 if mode in ('imp', 'ind', 'ind+', 'ind, ind') else
                      3 if mode in ('abs', 'abx', 'aby', 'iax', 'abs.bit',
                                    'dp, dp', 'dp, imm', 'dp, rel', 'dpx, rel') else 2)
            for code in pending:
                result[code] = (name, mode, length)
            pending.clear()
    if len(result) != 256:
        raise ValueError('Incomplete opcode annotations in pinned core')
    return result

def disassemble(ram, start, end):
    table = opcodes()
    if len(ram) != 65536 or not 0 <= start < end <= 65536:
        raise ValueError('Expected 64 KiB SPC RAM and a valid address range')
    pc = start
    while pc < end:
        name, mode, length = table[ram[pc]]
        raw = ram[pc:pc+length]
        if len(raw) != length:
            raise ValueError('Truncated instruction')
        suffix = ''
        if 'rel' in mode:
            offset = raw[-1] if raw[-1] < 128 else raw[-1]-256
            suffix = f' -> {(pc+length+offset)&65535:04X}'
        yield f'{pc:04X} {raw.hex(" "):8} {name:6} {mode}{suffix}'
        pc += length

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('ram', type=Path)
    parser.add_argument('start', type=lambda v: int(v, 16))
    parser.add_argument('end', type=lambda v: int(v, 16))
    args = parser.parse_args()
    print('\n'.join(disassemble(args.ram.read_bytes(), args.start, args.end)))
