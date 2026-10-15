/* Music command transport: A2:84F8, DP=$0300. Upload traffic is NOT music.
 * Uses the existing MSU mixer -> host SDL3 stream. Port-1 FX stay on SPC;
 * voice/control semantics still require reference and listening tests.
 */
#include "ackman_audio.h"
#include "ackman_pcm.h"
#include "snes/interp_bridge.h"
#include "snes/msu1.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int enabled;
static char prefix[768];
static unsigned replacements, fallbacks;
static int pending;
static int pcm_playing;
static uint8_t pending_ack;
static FILE *calls;
int AckmanAudioPending(void) { return enabled && pending; }
void AckmanAudioReset(void) {
    pending=0;
    pcm_playing=0;
    AckmanPcmEnvelope(1);
    if (enabled) msu1_write(0x2007,0);
    replacements=fallbacks=0;
}
void AckmanAudioPoll(uint8_t acknowledgment,uint8_t music_volume,uint8_t fade_delta) {
    if (!enabled || !pending || acknowledgment!=pending_ack ||
        music_volume!=0 || fade_delta!=0xf7) return;
    pending=0;
    pcm_playing=1;
    AckmanPcmEnvelope(1);
    msu1_write(0x2006,255);
    msu1_write(0x2007,3);
    fprintf(stderr,"[ackman-msu] PCM started after SPC music fade\n");
}
void AckmanAudioClose(void) {
    if (calls) fclose(calls);
    calls=NULL;
}
void AckmanAudioTrace(CpuState *cpu, uint32_t pc) {
    if (!calls) return;
    /* Raw stack bytes: only the public JSL entry has a 24-bit return.
     * Reading WRAM directly avoids adding diagnostic bus cycles. */
    extern uint8_t g_ram[0x20000];
    unsigned s = cpu->S;
    fprintf(calls,"%llu,%06X,%04X,%04X,%04X,%04X,%02X%02X%02X\n",
        (unsigned long long)cpu->master_cycles,pc,cpu->A,cpu->Y,cpu->D,s,
        g_ram[(s+3)&0xffff],g_ram[(s+2)&0xffff],g_ram[(s+1)&0xffff]);
    fflush(calls);
}
static int valid_track(unsigned id) {
    char path[1024]; unsigned char header[8];
    snprintf(path,sizeof(path),"%s-%u.pcm",prefix,id);
    FILE *f=fopen(path,"rb");
    if(!f) return 0;
    int ok=fread(header,1,8,f)==8 && !memcmp(header,"MSU1",4);
    if(fseek(f,0,SEEK_END)) ok=0;
    long size=ftell(f);
    if(size<=8 || (size-8)%4) ok=0;
    if(ok) {
        uint32_t loop=(uint32_t)header[4]|(uint32_t)header[5]<<8|
                      (uint32_t)header[6]<<16|(uint32_t)header[7]<<24;
        if(loop>=(uint32_t)((size-8)/4)) ok=0;
    }
    fclose(f); return ok;
}
void AckmanAudioDebug(int next_track) {
    static unsigned debug_track=0x34;
    if(!enabled) {
        fputs("[ackman-debug] MSU disabled: set ACKMAN_MSU_EXPERIMENTAL=1 and ACKMAN_MSU_PREFIX\n",stderr);
        return;
    }
    if(!next_track) {
        AckmanPcmEnvelope(2);
        fputs("[ackman-debug] PCM fade-out (-9 per 12ms); SPC unchanged\n",stderr);
        return;
    }
    for(unsigned n=0;n<0x35;n++) {
        unsigned id=(debug_track+1+n)%0x35;
        if(!valid_track(id)) continue;
        pending=0;
        msu1_write(0x2007,0);
        msu1_write(0x2004,(uint8_t)id);
        msu1_write(0x2005,0);
        if(msu1_read(0x2000)&8) {
            pcm_playing=0;
            fputs("[ackman-debug] PCM load failed\n",stderr);
            return;
        }
        debug_track=id; pcm_playing=1;
        AckmanPcmEnvelope(1);
        msu1_write(0x2006,255);
        msu1_write(0x2007,3);
        fprintf(stderr,"[ackman-debug] PCM track=%u file=%s-%u.pcm; SPC unchanged\n",id,prefix,id);
        return;
    }
    fputs("[ackman-debug] No valid PCM tracks 0..52 found\n",stderr);
}
uint8_t AckmanRouteMusic(CpuState *cpu,uint8_t command) {
    if(!enabled || cpu->D!=0x0300) return command;
    extern uint8_t g_ram[0x20000];
    /* PHP is still on the stack at $84f8. Public music and typed control
     * paths are distinct; $84b5 upload traffic must remain untouched. */
    unsigned ret=g_ram[(cpu->S+2)&0xffff] |
                 (unsigned)g_ram[(cpu->S+3)&0xffff]<<8;
    unsigned id=command&0x7f;
    if (ret!=0x80a0 && ret!=0x80f2) {
        /* JSR return words from APIs $15..$18 and $1a. Check both the
         * caller and its immediate operand, not merely a command range. */
        unsigned expected;
        switch (ret) {
        case 0x810e: expected=0x3b; break;
        case 0x811d: expected=0x3a; break;
        case 0x812c: expected=0x39; break;
        case 0x813b: expected=0x38; break;
        case 0x8159: expected=0x3f; break;
        default: return command;
        }
        if (id!=expected) return command;
    }
    /* SPC $0880 masks bit 6 before the $35 dispatch threshold. It is a
     * playback flag, not part of the track index. $0898 contains eleven
     * special-command vectors ($35..$3f), including volume/fade/upload. */
    id &= 0x3f;
    if (id==0x35 || id==0x38 || id==0x39) {
        /* A later inert command advances the same acknowledgment port.
         * Queue order still puts our fade before it. */
        if (pending) pending_ack=command;
        return command;
    }
    pending=0; /* A new track/control supersedes any queued replacement. */
    if (pcm_playing && (id==0x3b || id==0x3a)) {
        AckmanPcmEnvelope(id==0x3b?2:3);
        if(id==0x3a) {
            /* The native music is intentionally at zero while PCM plays.
             * A native fade-in would make both tracks audible. Preserve
             * transport acknowledgment with the verified RET command. */
            command=(command&0x80)|0x39;
            cpu->A=(cpu->A&0xff00)|command;
        }
        return command;
    }
    pcm_playing=0;
    /* Unimplemented controls stop replacement and reach the original SPC;
     * never open a PCM for a command in the special dispatch table. */
    if(id>=0x35) {
        msu1_write(0x2007,0);
        fprintf(stderr,"[ackman-msu] control command=%u caller=%04X; SPC forwarded\n",id,ret);
        return command;
    }
    if(!msu1_enabled() || !valid_track(id)) {
        msu1_write(0x2007,0);
        ++fallbacks;
        fprintf(stderr,"[ackman-msu] fallback command=%u\n",id);
        return command;
    }
    msu1_write(0x2007,0);
    msu1_write(0x2004,(uint8_t)id);
    msu1_write(0x2005,0);
    if(msu1_read(0x2000)&8) return command;
    /* Track is loaded but paused. The frame observer starts it only after
     * the original transport acknowledges the fade and music reaches zero. */
    ++replacements;
    fprintf(stderr,"[ackman-msu] replacement command=%u count=%u\n",id,replacements);
    /* Fade-out code $3b: SPC $0911 sets delta $66 to -9.
     * This is NOT an immediate mute; PCM waits for the fade below.
     * Routed through the original SPC handshake.
     * Update A as well, because the next guest instruction stores A at DP+$76.
     * Dropping the bus write alone would deadlock the next acknowledgment.
     */
    uint8_t stop=(command&0x80)|0x3b;
    pending_ack=stop;
    pending=1;
    cpu->A=(cpu->A&0xff00)|stop;
    return stop;
}
static void pre_store(CpuState *cpu,uint32_t pc) {
    (void)pc;
    AckmanRouteMusic(cpu,(uint8_t)cpu->A);
}
void AckmanAudioInstall(void) {
    const char *trace=getenv("ACKMAN_AUDIO_TRACE");
    if (trace && *trace && !calls) {
        calls=fopen(trace,"w");
        if (!calls) { perror(trace); exit(1); }
        fputs("clock,pc,a,y,dp,s,stack3\n",calls);
    }
    if (calls) {
        interp_bridge_set_pre_opcode_hook(0x808773,AckmanAudioTrace);
        interp_bridge_set_pre_opcode_hook(0x8087c9,AckmanAudioTrace);
        interp_bridge_set_pre_opcode_hook(0xa28000,AckmanAudioTrace);
        interp_bridge_set_pre_opcode_hook(0xa284e3,AckmanAudioTrace);
        interp_bridge_set_pre_opcode_hook(0xa284ff,AckmanAudioTrace);
    }
    const char *enable=getenv("ACKMAN_MSU_EXPERIMENTAL");
    /* The mod manager clears SNESRECOMP_MSU1 when no package is selected.
     * Use a port-specific explicit prefix for this experimental path. */
    const char *base=getenv("ACKMAN_MSU_PREFIX");
    enabled=enable && !strcmp(enable,"1") && base && *base;
    if(enabled) {
        if(strlen(base)>=sizeof(prefix)) { fputs("MSU prefix too long\n",stderr); exit(1); }
        strcpy(prefix,base);
        const char *active_base=getenv("SNESRECOMP_MSU1");
        if (msu1_enabled() && (!active_base || strcmp(active_base,prefix))) {
            fputs("[ackman-msu] explicit prefix conflicts with selected mod pack; routing disabled\n",stderr);
            enabled=0;
            return;
        }
        if (!msu1_enabled()) {
#ifdef _WIN32
            if (_putenv_s("SNESRECOMP_MSU1",prefix)) exit(1);
#else
            if (setenv("SNESRECOMP_MSU1",prefix,1)) exit(1);
#endif
            msu1_init();
        }
        /* Explicit prefix required; 'auto' and directory autodetection are
         * deliberately not interpreted differently by this host and the core. */
        interp_bridge_set_pre_opcode_hook(0xa284f8,pre_store);
    }
}
