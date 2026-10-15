#include "ackman_menu.h"
#include "ackman_flow.h"
#include "ackman_rumble.h"
#include "common_rtl.h"
#include "cpu_state.h"
#include <assert.h>
#include <string.h>
#include "desktop/config.h"
Config g_config;
CpuState g_cpu;
bool RtlSpeculativeFrame(void) {return false;}
void AckmanRumblePollSpc(void) {}
AckmanRumbleScene AckmanVideoRumbleScene(void) {return ACK_RUMBLE_NONE;}
bool AckmanVideoRumbleCharging(void) {return false;}
bool AckmanVideoRumbleInterludeContact(void) {return false;}
bool AckmanVideoRumbleCastleIgnition(void) {return false;}
bool AckmanVideoRumbleClimaxFlash(void) {return false;}
void TestRumble(void);
void TestDevices(void);
static bool overlay;
static bool dialog;
bool AckmanMenuDialogActive(void) { return dialog; }
static bool input_owned;
bool AckmanMenuVisible(void) { return overlay; }
bool AckmanMenuOwnsInput(void) { return overlay || input_owned; }
static uint64_t gen;
static int score,resets,sessions,clocks;
static uint32_t observed;
static unsigned char pixel[4];
uint64_t RtlStateGeneration(void) { return gen; }
int AckmanVideoScoreScreen(void) { return score; }
void cpu_state_init(CpuState *cpu,uint8 *ram) { memset(cpu,0,sizeof(*cpu)); cpu->ram=ram; }
void RtlReset(int mode) { assert(mode==1); resets++; gen++; }
void GameSessionReset(void) { sessions++; }
void snesrecomp_desktop_request_clock_reset(void) { clocks++; }
bool RtlRunFrame(uint32 inputs) {
    observed=inputs; memset(pixel,255,sizeof(pixel));
    AckmanFlowShade(pixel,4,1,1); return true;
}
int main(void) {
    TestRumble();
    TestDevices();
    // Both transition directions, including script changes before brightness
    // drops and fade-in policy changes before the destination becomes visible.
    assert(AckmanFlowVideoPolicy(1)==1);
    for(int i=0;i<60;i++) { AckmanFlowVideoEndpoint(false);assert(AckmanFlowVideoPolicy(0)==1); }
    AckmanFlowVideoEndpoint(true);assert(AckmanFlowVideoPolicy(0)==0);
    for(int i=0;i<60;i++) { AckmanFlowVideoEndpoint(false);assert(AckmanFlowVideoPolicy(-1)==0); }
    AckmanFlowVideoEndpoint(true);assert(AckmanFlowVideoPolicy(-1)==-1);
    AckmanFlowVideoEndpoint(true);assert(AckmanFlowVideoPolicy(0)==0);
    AckmanFlowVideoEndpoint(false);assert(AckmanFlowVideoPolicy(1)==0);
    // Background-only clipping is part of the same atomic fade policy.
    AckmanFlowVideoEndpoint(true);assert(AckmanFlowVideoPolicy(3)==3);
    AckmanFlowVideoEndpoint(false);assert(AckmanFlowVideoPolicy(6)==3);
    AckmanFlowVideoEndpoint(true);assert(AckmanFlowVideoPolicy(6)==6);
    // Shared ending-script layer changes remain atomic through visible fades.
    AckmanFlowVideoEndpoint(true);assert(AckmanFlowVideoPolicy(7)==7);
    AckmanFlowVideoEndpoint(false);assert(AckmanFlowVideoPolicy(6)==7);
    AckmanFlowVideoEndpoint(true);assert(AckmanFlowVideoPolicy(6)==6);
    AckmanFlowVideoEndpoint(true);assert(AckmanFlowVideoPolicy(8)==8);
    AckmanFlowVideoEndpoint(false);assert(AckmanFlowVideoPolicy(5)==8);
    AckmanFlowVideoEndpoint(true);assert(AckmanFlowVideoPolicy(5)==5);
    // Loading a state must discard the outgoing scene's latch immediately.
    // The saved stage-5 story is native; its A22E gameplay successor requests
    // mapped rendering before the fade completes. Keep pillarbox until black.
    gen++;AckmanRunFrame(0);assert(AckmanFlowVideoPolicy(0)==0);
    for(int i=0;i<30;i++) { AckmanFlowVideoEndpoint(false);assert(AckmanFlowVideoPolicy(-1)==0); }
    AckmanFlowVideoEndpoint(true);assert(AckmanFlowVideoPolicy(-1)==-1);
    gen++;AckmanRunFrame(0);assert(AckmanFlowVideoPolicy(1)==1);
    AckmanRunFrame(8); assert(observed==8 && resets==0);
    overlay=true;for(int i=0;i<100;i++) AckmanRunFrame(0);assert(observed==8 && resets==0);overlay=false;
    dialog=true;for(int i=0;i<100;i++) AckmanRunFrame(0);assert(observed==8 && resets==0);dialog=false;
    input_owned=true;AckmanRunFrame(8);assert(observed==0);input_owned=false;
    AckmanRunFrame(0); score=1;
    int last=255;
    for(int i=0;i<60;i++) {
        AckmanRunFrame(8); assert(observed==0 && resets==0);
        assert(pixel[0]<last && pixel[3]==255); last=pixel[0];
    }
    assert(last==0); AckmanRunFrame(0);
    assert(resets==1 && sessions==1 && clocks==1 && pixel[0]==255);
    AckmanRunFrame(8); gen++; score=0; AckmanRunFrame(0);
    assert(pixel[0]==255);
    for(int i=0;i<100;i++) AckmanRunFrame(0);
    assert(resets==1);assert(AckmanFlowVideoPolicy(0)==0);
    uint32_t movie[5];AckmanFlowSaveRewind(movie);
    gen++;AckmanRunFrame(0);assert(AckmanFlowVideoPolicy(1)==1);
    AckmanFlowLoadRewind(movie);assert(AckmanFlowVideoPolicy(1)==0);
    AckmanRunFrame(0);assert(AckmanFlowVideoPolicy(1)==0);return 0;
}

