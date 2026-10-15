/* Exercise the real routing code with an observable MSU device boundary. */
#include "ackman_audio.h"
#include "ackman_pcm.h"
#include "snes/interp_bridge.h"
#include "snes/msu1.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint8_t g_ram[0x20000];
static unsigned writes, plays, stops;
static int active;
double RtlAudioOutputRate(void) { return 48000; }
void msu1_mix(int16_t *out,int frames) {
    for(int i=0;i<frames*2;i++) out[i]=10000;
}
static InterpPreOpcodeHook store_hook;
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
void msu1_init(void) { active=1; }
bool msu1_enabled(void) { return active!=0; }
uint8_t msu1_read(uint16_t reg) { (void)reg; return 0; }
void msu1_write(uint16_t reg,uint8_t val) {
    ++writes;
    if (reg==0x2007) { if (val&1) ++plays; else ++stops; }
}
void interp_bridge_set_pre_opcode_hook(uint32_t pc,InterpPreOpcodeHook hook) {
    if (pc==0xa284f8) store_hook=hook;
}
static void env(const char *key,const char *value) {
#ifdef _WIN32
    CHECK(!_putenv_s(key,value));
#else
    CHECK(!setenv(key,value,1));
#endif
}
static CpuState cpu(unsigned ret,unsigned command) {
    CpuState result={0};
    result.S=0x1ef; result.D=0x300; result.A=0xab00|command;
    g_ram[result.S+2]=(uint8_t)ret;
    g_ram[result.S+3]=(uint8_t)(ret>>8);
    return result;
}
int main(void) {
    env("ACKMAN_AUDIO_TRACE","");
    env("ACKMAN_MSU_EXPERIMENTAL","1");
    env("ACKMAN_MSU_PREFIX","ackman-route-test");
    AckmanAudioInstall();
    CHECK(store_hook);
    /* Typed controls preserve guest A in both handshake phases. Inert SPC
     * commands preserve PCM; unimplemented controls stop it. A mismatched
     * operand must not touch the MSU device. */
    const unsigned returns[]={0x810e,0x811d,0x812c,0x813b,0x8159};
    const unsigned commands[]={0x3b,0x3a,0x39,0x38,0x3f};
    for(unsigned i=0;i<5;i++) for(unsigned bit=0;bit<=0x80;bit+=0x80) {
        CpuState c=cpu(returns[i],commands[i]|bit);
        unsigned before=stops;
        CHECK(AckmanRouteMusic(&c,(uint8_t)c.A)==(commands[i]|bit));
        unsigned is_noop=commands[i]==0x38 || commands[i]==0x39;
        CHECK(c.A==(0xab00|commands[i]|bit) && stops==before+!is_noop);
        c=cpu(returns[i],7|bit); before=writes;
        store_hook(&c,0xa284f8);
        CHECK(writes==before && c.A==(0xab07|bit));
    }
    /* The SPC dispatch table starts at $35. Its RET handlers do not stop
     * a playing replacement, regardless of the command's bit-6 flag. */
    for (unsigned flags=0;flags<256;flags+=64) {
        const unsigned noop[]={0x35,0x38,0x39};
        for(unsigned i=0;i<3;i++) {
            CpuState c=cpu(0x80a0,noop[i]|flags); unsigned before=writes;
            store_hook(&c,0xa284f8);
            CHECK(writes==before && c.A==(0xab00|noop[i]|flags));
        }
    }
    /* Upload and port-1 callers cannot be mistaken for a music event. */
    const unsigned excluded[]={0x84b7,0x80ff,0x1234};
    for(unsigned i=0;i<3;i++) for(unsigned cmd=0;cmd<256;cmd++) {
        CpuState c=cpu(excluded[i],cmd); unsigned before=writes;
        CHECK(AckmanRouteMusic(&c,(uint8_t)cmd)==cmd);
        CHECK(writes==before && c.A==(0xab00|cmd));
    }
    const unsigned char pcm[]={'M','S','U','1',0,0,0,0,0,0,0,0};
    FILE *f=fopen("ackman-route-test-7.pcm","wb"); CHECK(f);
    CHECK(fwrite(pcm,1,sizeof(pcm),f)==sizeof(pcm)); CHECK(!fclose(f));
    for(unsigned bit=0;bit<256;bit+=0x40) {
        CpuState c=cpu(0x80f2,7|bit); unsigned before=plays;
        store_hook(&c,0xa284f8);
        CHECK(plays==before && c.A==(0xab3b|(bit&0x80)));
        uint8_t ack=(uint8_t)c.A;
        AckmanAudioPoll(ack^0x80,0,0xf7); /* stale acknowledgment */
        AckmanAudioPoll(ack,1,0xf7);      /* music is still fading */
        AckmanAudioPoll(ack,0,0);        /* handler has not run */
        CHECK(plays==before);
        AckmanAudioPoll(ack,0,0xf7);
        CHECK(plays==before+1);
        AckmanAudioPoll(ack,0,0xf7);
        CHECK(plays==before+1); /* do not restart on every frame */
        c=cpu(0x80a0,8|bit); before=stops;
        CHECK(AckmanRouteMusic(&c,(uint8_t)c.A)==(8|bit));
        CHECK(stops==before+1 && c.A==(0xab08|bit));
        c=cpu(0x80a0,7|bit); c.D=0; before=writes;
        store_hook(&c,0xa284f8); CHECK(writes==before);
    }
    CpuState pending_cpu=cpu(0x80a0,7);
    store_hook(&pending_cpu,0xa284f8);
    unsigned before=plays;
    AckmanAudioReset();
    AckmanAudioPoll(0x3b,0,0xf7);
    CHECK(plays==before);
    pending_cpu=cpu(0x80a0,7);
    store_hook(&pending_cpu,0xa284f8);
    CpuState inert=cpu(0x80a0,0xb5);
    store_hook(&inert,0xa284f8);
    AckmanAudioPoll(0x3b,0,0xf7);
    CHECK(plays==before);
    AckmanAudioPoll(0xb5,0,0xf7);
    CHECK(plays==before+1);
    /* A real music control attenuates PCM without stopping its cursor or
     * attenuating the SPC contribution already present in the buffer. */
    CpuState fade=cpu(0x810e,0x3b);
    before=stops;
    store_hook(&fade,0xa284f8);
    CHECK(stops==before);
    int16_t samples[960];
    int previous=11000;
    for(int block=0;block<40;block++) {
        for(int i=0;i<960;i++) samples[i]=1000;
        AckmanMixPcm(samples,480);
        for(int i=0;i<480;i++) {
            CHECK(samples[2*i]==samples[2*i+1]);
            CHECK(samples[2*i]<=previous && samples[2*i]>=1000);
            previous=samples[2*i];
        }
    }
    CHECK(previous==1000);
    fade=cpu(0x811d,0xba);
    store_hook(&fade,0xa284f8);
    CHECK(fade.A==0xabb9 && stops==before);
    for(int i=0;i<960;i++) samples[i]=1000;
    AckmanMixPcm(samples,480);
    CHECK(samples[958]>1000 && samples[958]<11000);
    AckmanPcmEnvelope(1);
    samples[0]=samples[1]=30000;
    AckmanMixPcm(samples,1);
    CHECK(samples[0]==32767 && samples[1]==32767);
    before=plays;
    AckmanAudioDebug(1);
    CHECK(plays==before+1); /* scans past missing tracks to fixture 7 */
    before=stops;
    AckmanAudioDebug(0);
    CHECK(stops==before); /* fade keeps the PCM cursor alive */
    CHECK(!remove("ackman-route-test-7.pcm"));
    AckmanAudioClose();
    return 0;
}
