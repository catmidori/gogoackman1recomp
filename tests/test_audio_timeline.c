/* Real core resampler + DSP ring: output history must not survive a load. */
#include "common_rtl.h"
#include "snes/snes.h"
#include "snes/dsp.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
static Snes snes;
static Apu apu;
static Dsp dsp;
Snes *g_snes=&snes;
double RtlAudioOutputRate(void);
void RtlApuLock(void) {}
void RtlApuUnlock(void) {}
void audio_trace_on_consume(uint32_t a,uint32_t b,uint32_t c) { (void)a;(void)b;(void)c; }
void audio_trace_on_output_underflow(uint32_t a,uint32_t b) { (void)a;(void)b; }
#include "audio-core.inc"
int main(void) {
    snes.apu=&apu;apu.dsp=&dsp;
    for(int rate=32000;rate<=48000;rate+=16000) {
        RtlSetAudioOutputRate(rate);
        memset(&dsp,0x35,sizeof(dsp));
        dsp.sampleRead=0;dsp.sampleWrite=DSP_SAMPLE_RING;
        for(int i=0;i<DSP_SAMPLE_RING*2;i++)dsp.sampleBuffer[i]=20000;
        int16_t output[512*2];
        rtl_render_native(&dsp,output,512);assert(output[100]!=0);
        unsigned char guest[offsetof(Dsp,sampleBuffer)];
        memcpy(guest,&dsp,sizeof(guest));
        RtlAudioResetTimeline();
        assert(!memcmp(guest,&dsp,sizeof(guest))); /* includes echo and voices */
        assert(dsp_available(&dsp)==0 && RtlAudioOutputRate()==rate);
        for(int i=0;i<DSP_SAMPLE_RING*2;i++)assert(dsp.sampleBuffer[i]==0);
        memset(output,0x7f,sizeof(output));
        rtl_render_native(&dsp,output,512);
        for(int i=0;i<1024;i++)assert(output[i]==0); /* no previous fade anchor */
        dsp.sampleWrite=4096;
        for(int i=0;i<8192;i++)dsp.sampleBuffer[i]=1000;
        rtl_render_native(&dsp,output,512);
        assert(output[0]==0 && output[128]==1000); /* smooth restart */
        RtlAudioResetTimeline();
    }
    puts("PASS deep audio timeline reset: silence, fresh fade-in, guest DSP preserved.");
    return 0;
}
