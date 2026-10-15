/* PCM-only envelope, before addition to the SPC output. The callback owns
 * gain; the guest thread publishes commands without touching its buffer. */
#include "ackman_pcm.h"
#include "snes/msu1.h"
#include <stdatomic.h>
#include <string.h>
extern double RtlAudioOutputRate(void);
static atomic_int request;
static double gain=1.0, slope;
void AckmanPcmEnvelope(int command) {
    if(command==1) { atomic_store(&request,0x100); return; }
    int old=atomic_load(&request);
    while(!atomic_compare_exchange_weak(&request,&old,(old&0x100)|command)) {}
}
void AckmanMixPcm(int16_t *out,int frames) {
    int cmd=atomic_exchange(&request,0);
    /* Timer 0 target $60 at 8 kHz: 12 ms per volume update. Linear
     * interpolation of the driver's -9/+2 steps; not a DSP-cycle model. */
    if(cmd&0x100) { gain=1; slope=0; }
    if((cmd&255)==2) slope=-9.0/(255.0*0.012);
    if((cmd&255)==3) slope=2.0/(255.0*0.012);
    double rate=RtlAudioOutputRate();
    if(rate<1) rate=44100;
    while(frames>0) {
        int n=frames>256?256:frames;
        int16_t pcm[512]={0};
        msu1_mix(pcm,n);
        for(int i=0;i<n;i++) {
            for(int ch=0;ch<2;ch++) {
                int value=out[2*i+ch]+(int)(pcm[2*i+ch]*gain);
                out[2*i+ch]=(int16_t)(value>32767?32767:value< -32768?-32768:value);
            }
            gain+=slope/rate;
            if(gain<=0) { gain=0; slope=0; }
            if(gain>=1) { gain=1; slope=0; }
        }
        out+=n*2; frames-=n;
    }
}
