/* Both generated C and interpreter accesses enter these bus wrappers.
 * CMake renames only the upstream implementations; no generated C is edited.
 */
#include "ackman_hooks.h"
#include "ackman_audio.h"
#include "ackman_rumble.h"
#include "common_rtl.h"
#include "common_cpu_infra.h"
#include "cpu_state.h"
#include "snes/ppu.h"
#include "snes/apu.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

void AckmanCoreWriteReg(uint16 reg, uint8 value);
void AckmanCoreWriteRegWord(uint16 reg, uint16 value);
extern CpuState g_cpu;
extern uint8 g_ram[0x20000];
extern Ppu *g_ppu;
static FILE *mmio;
static unsigned frame_number;
static unsigned long records;
static bool mine_tagged,mine_audio;
static unsigned mine_frame;
static uint64_t mine_generation;

void AckmanHooksInit(void) {
    const char *p = getenv("ACKMAN_MMIO_TRACE");
    if (p && *p) {
        mmio = fopen(p, "w");
        if (!mmio) { perror(p); exit(1); }
        fputs("frame,clock,pc,dp,reg,value,width\n", mmio);
    }
}
static void observe(uint16 reg, uint16 value, unsigned width) {
    if (!mmio || records >= 200000) return;
    if (!((reg >= 0x2100 && reg <= 0x2143) || reg == 0x4200 ||
          reg == 0x420b || reg == 0x420c)) return;
    ++records;
    fprintf(mmio, "%u,%llu,%06X,%04X,%04X,%04X,%u\n", frame_number,
        (unsigned long long)g_cpu.master_cycles, g_interp816_cur_pc,
        g_cpu.D, reg, value, width);
}
void WriteReg(uint16 reg, uint8 value) {
    observe(reg, value, 1);
    if(reg==0x2141) {
        static FILE *f;static bool checked;if(!checked){const char *audit=SDL_getenv("ACKMAN_RUMBLE_AUDIT");checked=true;if(audit && *audit)f=fopen(audit,"w");}
        if(f)fprintf(f,"%u,%llu,%06X,%04X,%02X,%04X,%04X,%04X,%04X\n",frame_number,(unsigned long long)g_cpu.master_cycles,g_interp816_cur_pc,g_cpu.D,value,g_ram[0x400]|g_ram[0x401]<<8,g_ram[0x1126]|g_ram[0x1127]<<8,g_ram[0x1124]|g_ram[0x1125]<<8,g_cpu.Y);
    }
    if(reg==0x2141 && g_cpu.D==0x0300 && !RtlSpeculativeFrame())
        AckmanRumbleSound(value,g_ram[0x400]|g_ram[0x401]<<8,g_ram[0x1126]|g_ram[0x1127]<<8);
    if(reg==0x2141 && g_cpu.D==0x0300 && (value&0x7f)==0x1d && !RtlSpeculativeFrame() &&
       mine_tagged && mine_generation==RtlStateGeneration() && frame_number-mine_frame<=6)mine_audio=true;
    AckmanCoreWriteReg(reg, value);
}
void WriteRegWord(uint16 reg, uint16 value) {
    observe(reg, value, 2);
    AckmanCoreWriteRegWord(reg, value);
}
void AckmanObserveFrame(unsigned frame) {
    frame_number = frame;
    /* Sampling is read-only and shares the audio thread's APU lock.
     * Confirm the mapped fade handler before interpreting SPC variables. */
    if (AckmanAudioPending() && g_snes && g_snes->apu) {
        uint8_t ack=0,volume=255,delta=0;
        RtlApuLock();
        Apu *apu=g_snes->apu;
        if (apu->ram[0x911]==0x8f && apu->ram[0x912]==0xf7 &&
            apu->ram[0x913]==0x66 && apu->ram[0x914]==0x6f &&
            apu->portQHead==apu->portQTail) {
            ack=apu->outPorts[0]; volume=apu->ram[0x67]; delta=apu->ram[0x66];
        }
        RtlApuUnlock();
        AckmanAudioPoll(ack,volume,delta);
    }
    static FILE *ram_audit;static bool checked_ram;
    if(!checked_ram){const char *path=SDL_getenv("ACKMAN_RUMBLE_RAM");checked_ram=true;if(path && *path)ram_audit=fopen(path,"wb");}
    if(ram_audit){fwrite(g_ram,1,0x4000,ram_audit);fwrite(g_ram+0x1c000,1,0x2000,ram_audit);}
    const char *p = getenv("ACKMAN_DUMP_FRAME");
    if (!p || frame != strtoul(p, NULL, 10)) return;
    const char *base = getenv("ACKMAN_DUMP_PREFIX");
    if (!base) return;
    char name[1024];
    snprintf(name, sizeof(name), "%s.wram", base);
    FILE *f = fopen(name, "wb");
    if (f) { fwrite(g_ram, 1, sizeof(g_ram), f); fclose(f); }
    snprintf(name, sizeof(name), "%s.vram", base);
    f = fopen(name, "wb");
    if (f) { fwrite(g_ppu->vram, 1, sizeof(g_ppu->vram), f); fclose(f); }
    snprintf(name, sizeof(name), "%s.oam", base);
    f = fopen(name, "wb");
    if (f) { fwrite(g_ppu->oam, 1, sizeof(g_ppu->oam), f); fclose(f); }
    snprintf(name, sizeof(name), "%s.spc-ram", base);
    f = fopen(name, "wb");
    if (f) {
        RtlApuLock();
        fwrite(g_snes->apu->ram, 1, sizeof(g_snes->apu->ram), f);
        RtlApuUnlock();
        fclose(f);
    }
}
void AckmanHooksClose(void) { if (mmio) fclose(mmio); mmio = NULL; }

void AckmanCoreCpuWrite8(CpuState *cpu,uint8 bank,uint16 addr,uint8 value);
void AckmanCoreDspWrite(Dsp *dsp,uint8_t addr,uint8_t value);
static bool boss_actor(unsigned base) {
    unsigned room=g_ram[0x400]|g_ram[0x401]<<8;
    unsigned script=g_ram[0x1126]|g_ram[0x1127]<<8;
    unsigned kind=g_ram[base+6]; /* Native death dispatcher $86:9091 masks to byte. */
    return (room==0x0600 && script==0xa483 && kind==0x64) ||
           (room==0x1100 && script==0xaa11 && kind==0x69) ||
           (room==0x1800 && script==0xa8a6 && kind==0x6b) ||
           (room==0x2400 && script==0xa718 && kind==0x66) ||
           (room==0x2800 && script==0xa56e && kind==0x5e) ||
           (room==0x2d00 && script==0xa22e && (kind==0x6f || kind==0x75));
}
static bool boss_destroying(void) {
    for(unsigned base=0x1c000;base<0x1e000;base+=0x100)
        if((g_ram[base]&0x80) && (g_ram[base+1]&0x20) && boss_actor(base) &&
           !g_ram[base+0x64] && !g_ram[base+0x65] &&
           g_ram[base+0x96]==10 && g_ram[base+0x98]==16 && g_ram[base+0x90]<16)return true;
    return false;
}
static bool ackman_spc_driver(const uint8_t *ram) {
    // SFX table addresses are relocated for each sound bank; validate the
    // driver's code and its fixed command-tag stores, not a bank-specific table.
    return ram[0xe9a]==0xe5 && ram[0xe9b]==2 && ram[0xe9c]==2 &&
           ram[0xea6]==0xf6 && ram[0xea9]==0x68 && ram[0xeaa]==0xff &&
           ram[0xedb]==0xd4 && ram[0xedc]==0x20 &&
           ram[0xee0]==8 && ram[0xee1]==0x80 && ram[0xee2]==0xd4 && ram[0xee3]==0x28;
}
void AckmanRumblePollSpc(void) {
    // Reconstruct after file loads as well as fatal HP writes. Each of the
    // seven actors calls $86:A452: ten-frame cadence, sixteen native bursts.
    AckmanRumbleBossContext(boss_destroying());
    if(!g_snes || !g_snes->apu)return;
    RtlApuLock();
    Apu *apu=g_snes->apu;
    if(apu->ram[0x9aa2]==7 && apu->ram[0x9690]==1 && apu->ram[0x9691]==3)
        AckmanRumbleSpcCommands(apu->ram+0x28);
    if(ackman_spc_driver(apu->ram))AckmanRumbleBossSpc(apu->ram+0x28);
    RtlApuUnlock();
}
void AckmanCoreApuWrite(Apu *apu,uint16_t addr,uint8_t value);
void apu_cpuWrite(Apu *apu,uint16_t addr,uint8_t value) {
    AckmanCoreApuWrite(apu,addr,value);
    if(addr>=0x28 && addr<0x30) {
        static FILE *audit;static bool checked;
        if(!checked){const char *path=SDL_getenv("ACKMAN_RUMBLE_SPC");checked=true;if(path && *path)audit=fopen(path,"w");}
        if(audit)fprintf(audit,"%u,%llu,%04X,%02X\n",frame_number,(unsigned long long)apu->portClock,addr,value);
        if(apu->ram[0x9aa2]==7 && apu->ram[0x9690]==1 && apu->ram[0x9691]==3)
            AckmanRumbleSpcCommands(apu->ram+0x28);
        if(ackman_spc_driver(apu->ram))AckmanRumbleBossSpc(apu->ram+0x28);
    }
}
void dsp_write(Dsp *dsp,uint8_t addr,uint8_t value) {
    static FILE *audit;static bool checked;
    if(!checked){const char *path=SDL_getenv("ACKMAN_RUMBLE_DSP");checked=true;if(path && *path)audit=fopen(path,"w");}
    if(audit && addr==0x4c && value)fprintf(audit,"%u,%llu,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X\n",frame_number,(unsigned long long)g_snes->apu->portClock,value,dsp->channel[0].srcn,dsp->channel[1].srcn,dsp->channel[2].srcn,dsp->channel[3].srcn,dsp->channel[4].srcn,dsp->channel[5].srcn,dsp->channel[6].srcn,dsp->channel[7].srcn);
    if(audit && addr==0x4c && value)fprintf(audit,"tags,%u,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X\n",frame_number,dsp->apu_ram[0x28],dsp->apu_ram[0x29],dsp->apu_ram[0x2a],dsp->apu_ram[0x2b],dsp->apu_ram[0x2c],dsp->apu_ram[0x2d],dsp->apu_ram[0x2e],dsp->apu_ram[0x2f]);
    // Verified Ackman SPC mapping: command $30 -> sequence 4 at $95CA.
    // KON is the audible onset, including notes generated inside that sequence.
    // Queue atomically; never call SDL rumble or read guest CPU RAM on this thread.
    if(addr==0x4c && value && dsp->apu_ram[0x9a9a]==4 &&
       dsp->apu_ram[0x95ca]==0xff && dsp->apu_ram[0x95cb]==1)
        AckmanRumbleDspKeyOn(value,dsp->apu_ram+0x28);
    AckmanCoreDspWrite(dsp,addr,value);
}
void cpu_write8(CpuState *cpu,uint8 bank,uint16 addr,uint8 value) {
    if(addr>=0x42 && addr<0x50 && (bank&0x7f)<0x40) {
        static FILE *f;static bool checked;if(!checked){const char *audit=SDL_getenv("ACKMAN_RUMBLE_QUEUE");checked=true;if(audit && *audit)f=fopen(audit,"w");}
        unsigned stack=cpu->S;uint32_t caller=g_ram[(stack+8)&65535]|g_ram[(stack+9)&65535]<<8|g_ram[(stack+10)&65535]<<16;
        if(f)fprintf(f,"%u,%06X,%04X,%02X,%06X,%04X,%04X,%04X\n",frame_number,g_interp816_cur_pc,cpu->D,value,caller,g_ram[0x400]|g_ram[0x401]<<8,g_ram[0x1126]|g_ram[0x1127]<<8,g_ram[0x1124]|g_ram[0x1125]<<8);
    }
    if((addr>=0x2900 && addr<0x4000 && (addr&0x3f)==0) || ((bank&0x7f)==0x7f && addr>=0xc000) || addr==0x491 || addr==0x414 || addr==0x43a || addr==0x434 || addr==0x436 || addr==0x413 || (cpu->D==0x0b00 && (addr==0x34 || addr==0x36))) {
        static FILE *audit;static bool checked;if(!checked){const char *path=SDL_getenv("ACKMAN_RUMBLE_WRITES");checked=true;if(path && *path)audit=fopen(path,"w");}
        int off=cpu_wram_offset(bank,addr);
        if(audit && off>=0 && cpu->ram[off]!=value)fprintf(audit,"%u,%06X,%02X,%04X,%02X,%02X\n",frame_number,g_interp816_cur_pc,bank,addr,cpu->ram[off],value);
    }
    uint32_t pc=g_interp816_cur_pc&0x7fffff;
    // Mine launch consumes native flag $04D5 & $2000 and commits a negative
    // Y vector at $87:AB06. Require the nearby mine AND its explosion SFX,
    // in this generation, rather than a map address or ordinary jump.
    if(addr==0x436 && pc==0x07ab06 && (cpu->A&0x8000) && !RtlSpeculativeFrame() &&
       mine_tagged && mine_audio && mine_generation==RtlStateGeneration() && frame_number-mine_frame<=6 &&
       (cpu->ram[0x413]&2) && !(cpu->ram[0x437]&0x80) && cpu->ram[0x400]==0 && cpu->ram[0x401]==5) {
        AckmanRumbleMineExplosion();mine_tagged=mine_audio=false;
    }
    if(bank==0x7f && addr>=0xc064 && addr<0xe000 && (addr&0xff)==0x64 && !RtlSpeculativeFrame()) {
        int off=cpu_wram_offset(bank,addr);
        unsigned hp=cpu->ram[off]|cpu->ram[off+1]<<8;
        // Both routines commit reduced enemy HP AFTER a successful damaging
        // hitbox test. A jump/swing/contact SFX is never an impact by itself.
        if((pc==0x059905 || pc==0x069940) && value==(cpu->A&0xff) && cpu->A<hp) {
            if(!cpu->A && (cpu->ram[off&~0xff]&0x80) && boss_actor(off&~0xff))
                AckmanRumbleBossHit();
            if(!(cpu->ram[0x413]&0x82) && (cpu->ram[0x414]&2))AckmanRumbleAirHit();
            else AckmanRumbleEnemyHit();
        }
        // Ground mine's object state $30/$70, before explosion setup changes
        // it. $C006 is a map address (061C/1A1C), NOT a stable object ID.
        unsigned base=off&~0xff;
        if(pc==0x06a141 && hp && !value && (cpu->ram[0x413]&2) &&
           cpu->ram[0x400]==0 && cpu->ram[0x401]==5 &&
           cpu->ram[base+2]==0x30 && cpu->ram[base+3]==0x70) {
            int dx=(cpu->ram[base+10]|cpu->ram[base+11]<<8)-(cpu->ram[0x422]|cpu->ram[0x423]<<8);
            int dy=(cpu->ram[base+14]|cpu->ram[base+15]<<8)-(cpu->ram[0x426]|cpu->ram[0x427]<<8);
            if(dx>=-32 && dx<=32 && dy>=-32 && dy<=32) {
                mine_tagged=true;mine_audio=false;mine_frame=frame_number;mine_generation=RtlStateGeneration();
            }
        }
    }
    AckmanCoreCpuWrite8(cpu,bank,addr,value);
}

