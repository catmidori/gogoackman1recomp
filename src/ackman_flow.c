#include "ackman_flow.h"
#include "ackman_video.h"
#include "ackman_menu.h"
#include "ackman_rumble.h"
#include "ackman_hooks.h"
#include "desktop/config.h"
#include "common_rtl.h"
#include "cpu_state.h"
#include "game_rtl.h"
#include "desktop/host_main.h"
#include <stdio.h>
extern CpuState g_cpu;
static unsigned fade,previous_start;
static uint64_t generation;
static int video_policy;
static bool video_initialized,video_black,rebase_pending;
// A scene script can request the destination while INIDISP is still 15.
// Commit scene policies only between completely black rendered frames, so
// both outgoing fades and the next fade-in retain one presentation policy.
// Latch the entire policy ID, including castle OBJ bounds, ending dialogue
// clipping and the landscape/THE END phases of their shared script.
// Native letterboxed stories (including $2800/$A1D9) use the same latch:
// their successor script must not reopen side columns before full black.
int AckmanFlowVideoPolicy(int requested) {
    if(!video_initialized || video_black) video_policy=requested;
    video_initialized=true;
    video_black=false;
    return video_policy;
}
void AckmanFlowVideoEndpoint(bool black) { video_black=black; }
void AckmanFlowSaveRewind(uint32_t out[5]) {
    out[0]=fade;out[1]=previous_start;out[2]=(uint32_t)video_policy;
    out[3]=video_initialized;out[4]=video_black;
}
void AckmanFlowLoadRewind(const uint32_t in[5]) {
    fade=in[0];previous_start=in[1];video_policy=(int32_t)in[2];
    video_initialized=in[3]!=0;video_black=in[4]!=0;generation=RtlStateGeneration();rebase_pending=false;
}
static void reset_video_policy(void) {
    video_initialized=video_black=false;
}
enum { FADE_FRAMES=60, START=8 };
void AckmanReboot(void) {
    AckmanRumbleSuspend(true);
    uint8_t *ram=g_cpu.ram;
    cpu_state_init(&g_cpu,ram);
    RtlReset(1);
    GameSessionReset();
    snesrecomp_desktop_request_clock_reset();
    fade=0; previous_start=0; generation=RtlStateGeneration();rebase_pending=false;
    reset_video_policy();
}
bool AckmanRunFrame(uint32_t inputs) {
    if(AckmanMenuVisible() || AckmanMenuDialogActive()) return true; /* mandatory dialog lock */
    if(AckmanMenuOwnsInput())inputs=0;
    uint64_t current=RtlStateGeneration();
    bool state_rebased=current!=generation;
    if(state_rebased) { rebase_pending=true;fade=0; previous_start=0; generation=current; reset_video_policy(); }
    if(!fade && AckmanVideoScoreScreen() && (inputs&START) && !previous_start) {
        fade=1;
        fputs("[ackman-flow] final score: Start, fading to boot\n",stderr);
    }
    previous_start=inputs&START;
    if(fade>FADE_FRAMES) {
        AckmanReboot();
        inputs=0;
        fputs("[ackman-flow] core reset after final fade\n",stderr);
    }
    AckmanRumbleSetEnabled(g_config.controller_vibration);
    uint8_t *ram=g_cpu.ram;
    unsigned script=ram?(ram[0x1126]|ram[0x1127]<<8):0;
    unsigned position=ram?(ram[0x1124]|ram[0x1125]<<8):0;
    bool real=!RtlSpeculativeFrame();
    bool was_charging=real && ram && AckmanVideoRumbleCharging();
    bool was_ready=real && ram && (ram[0x414]&1);
    bool contact=real && ram && AckmanVideoRumbleInterludeContact();
    AckmanRumbleBegin(real,script,position);
    bool result=RtlRunFrame(fade ? inputs&~START : inputs);
    if(real && ram) {
        bool now_charging=AckmanVideoRumbleCharging();
        AckmanRumbleCharge(now_charging,was_charging && was_ready && !now_charging && (ram[0x43a]&0x20) && (ram[0x414]&0x80));
        // A save may already contain the contact pose: catch up once on its
        // first real frame. Ordinary pause/resume never changes this generation.
        if((!contact || rebase_pending) && AckmanVideoRumbleInterludeContact())AckmanRumbleInterludeHit();
        if(AckmanRumbleFrameActive() || !g_config.controller_vibration)rebase_pending=false;
        AckmanRumblePollSpc();
        AckmanRumbleEnd(AckmanVideoRumbleScene(),ram[0x1126]|ram[0x1127]<<8,ram[0x1124]|ram[0x1125]<<8);
    }
    if(fade) ++fade;
    return result;
}
void AckmanFlowShade(uint8_t *pixels,size_t pitch,int width,int height) {
    if(!fade || !pixels) return;
    unsigned gain=fade>=FADE_FRAMES?0:FADE_FRAMES-fade;
    for(int y=0;y<height;y++) for(int x=0;x<width;x++) {
        uint8_t *p=pixels+(size_t)y*pitch+x*4;
        for(int c=0;c<3;c++) p[c]=(unsigned)p[c]*gain/FADE_FRAMES;
    }
}
