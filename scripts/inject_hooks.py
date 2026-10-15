"""Idempotent, fail-closed AOT injection into a proven exact variant.

The interpreter hook is separate. No offsets/opcode behavior are guessed.
"""
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parents[1]
def inject(text):
    for pc in ('8000','84E3','84FF'):
        marker=f'    AckmanAudioTrace(cpu, 0xA2{pc}u);'
        label=f'  L_{pc}_M0X0:\n'
        if marker not in text:
            if text.count(label)!=1: raise RuntimeError(f'Expected unique {label!r}')
            text=text.replace(label,label+marker+'\n')
    start=text.index('RecompReturn bank_A2_84E3_M0X0(CpuState *cpu) {')
    end=text.index('\nRecompReturn ',start+1)
    body=text[start:end]
    if 'AckmanRouteMusic(cpu,' in body: return text
    pattern=r'(    cpu_write8\(cpu, cpu->DB, \(uint16\)\(0x2140\), (_v\d+)\);)'
    def replace(m):
        return '    '+m[2]+' = AckmanRouteMusic(cpu, (uint8_t)'+m[2]+');\n'+m[1]
    body,count=re.subn(pattern,replace,body)
    if count!=1: raise RuntimeError(f'Expected exactly one music store, found {count}')
    return '#include "ackman_audio.h"\n'+text[:start]+body+text[end:]
if __name__=='__main__':
    path=ROOT/'src/gen/banka2_v2.c'
    original=path.read_text(encoding='utf-8')
    result=inject(original)
    if result!=original: path.write_text(result,encoding='utf-8')
    print('Ackman AOT music hook verified.')
