#include "ackman_rumble.h"
#include <assert.h>
static unsigned calls;
static Uint16 low,high;
static bool SDLCALL rumble(void *userdata,Uint16 l,Uint16 h) {(void)userdata;calls++;low=l;high=h;return true;}
void TestRumble(void) {
    assert(SDL_Init(SDL_INIT_JOYSTICK));
    SDL_VirtualJoystickDesc desc={0};SDL_INIT_INTERFACE(&desc);
    desc.type=SDL_JOYSTICK_TYPE_GAMEPAD;desc.nbuttons=SDL_GAMEPAD_BUTTON_COUNT;desc.Rumble=rumble;
    SDL_JoystickID id=SDL_AttachVirtualJoystick(&desc);assert(id);
    SDL_Joystick *joy=SDL_OpenJoystick(id);assert(joy);
    SDL_Event e={0};e.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN;e.gbutton.which=id;e.gbutton.button=SDL_GAMEPAD_BUTTON_SOUTH;
    AckmanRumbleEvent(&e);AckmanRumbleSetEnabled(true);AckmanRumbleSuspend(false);
    // A press/launch and speculative frames cannot generate an effect.
    unsigned before=calls;AckmanRumbleBegin(true,0xa483,0xea);AckmanRumbleSound(5,0,0xa483);AckmanRumbleEnd(ACK_RUMBLE_NONE,0xa483,0xea);assert(calls==before);
    AckmanRumbleBegin(false,0xa483,0xea);AckmanRumbleSound(4,0,0xa483);AckmanRumbleEnd(ACK_RUMBLE_NONE,0xa483,0xea);assert(calls==before);
    AckmanRumbleBegin(true,0xa483,0xea);AckmanRumbleSound(4,0,0xa483);AckmanRumbleEnd(ACK_RUMBLE_NONE,0xa483,0xea);assert(low==0xe000 && high==0xf000);
    AckmanRumbleSuspend(true);assert(!low && !high);AckmanRumbleSuspend(false);
    AckmanRumbleBegin(true,0xa34d,0xd5);AckmanRumbleInterludeHit();AckmanRumbleEnd(ACK_RUMBLE_NONE,0xa34d,0xef);assert(low==0x9000 && high==0xa000);
    AckmanRumbleSuspend(true);AckmanRumbleSuspend(false);before=calls;
    AckmanRumbleBegin(true,0xa34d,0xef);AckmanRumbleEnd(ACK_RUMBLE_NONE,0xa34d,0xf5);assert(calls==before);
    AckmanRumbleBegin(true,0xa483,0xea);AckmanRumbleEnemyHit();AckmanRumbleEnd(ACK_RUMBLE_NONE,0xa483,0xea);assert(low==0x9000 && high==0xa000);
    AckmanRumbleSuspend(true);AckmanRumbleSuspend(false);
    AckmanRumbleBegin(true,0xa56e,0xbe);AckmanRumbleEnd(ACK_RUMBLE_ROAD,0xa56e,0xbe);assert(low==0x2500 && !high);
    AckmanRumbleSetEnabled(false);assert(!low && !high);before=calls;
    AckmanRumbleBegin(true,0xa56e,0xbe);AckmanRumbleEnd(ACK_RUMBLE_ROAD,0xa56e,0xbe);assert(calls==before);
    AckmanRumbleSetEnabled(true);AckmanRumbleBegin(true,0xa1d9,0x26);AckmanRumbleEnd(ACK_RUMBLE_CASTLE,0xa1d9,0x4f);assert(low==0x6000 && high==0x2800);
    AckmanRumbleBegin(true,0x9b2f,0);AckmanRumbleEnd(ACK_RUMBLE_END_BLASTS,0x9b2f,0x27);assert(low==0xffff && high==0xffff);
    AckmanRumbleSuspend(true);AckmanRumbleSuspend(false);
    AckmanRumbleBegin(true,0x9b2f,0x95);AckmanRumbleSound(0x84,0x2d00,0x9b2f);AckmanRumbleEnd(ACK_RUMBLE_END_BLASTS,0x9b2f,0xaf);assert(low==0xffff && high==0xffff);
    SDL_Delay(160);SDL_UpdateJoysticks();assert(!low && !high); // An explosion leaves a gap, no continuous base.
    AckmanRumbleBegin(true,0x9f08,0x47);AckmanRumbleSound(0x47,0x2d00,0x9f08);AckmanRumbleEnd(ACK_RUMBLE_FALL,0x9f08,0x47);assert(!low && !high);
    AckmanRumbleBegin(true,0x9f08,0x47);AckmanRumbleSound(0x45,0x2d00,0x9f08);AckmanRumbleEnd(ACK_RUMBLE_FALL,0x9f08,0x8b);assert(low==0xffff && high==0xe000);
    SDL_Delay(600);SDL_UpdateJoysticks();assert(low==0xffff);SDL_Delay(120);SDL_UpdateJoysticks();assert(!low && !high);
    AckmanRumbleSuspend(true);AckmanRumbleSuspend(false);
    AckmanRumbleBegin(true,0xa483,0xea);AckmanRumbleAirHit();AckmanRumbleEnd(ACK_RUMBLE_NONE,0xa483,0xea);assert(low==0x9000 && high==0xa000);
    AckmanRumbleSuspend(true);AckmanRumbleSuspend(false);
    AckmanRumbleBegin(true,0xa483,0xea);AckmanRumbleCharge(true,false);AckmanRumbleEnd(ACK_RUMBLE_NONE,0xa483,0xea);assert(low==0x6000 && high==0x7000);
    SDL_Delay(65);AckmanRumbleBegin(true,0xa483,0xea);AckmanRumbleCharge(true,false);AckmanRumbleEnd(ACK_RUMBLE_NONE,0xa483,0xea);assert(low==0x6000);
    AckmanRumbleBegin(true,0xa483,0xea);AckmanRumbleCharge(false,true);AckmanRumbleEnd(ACK_RUMBLE_NONE,0xa483,0xea);assert(low==0xb000 && high==0xc000);
    AckmanRumbleSuspend(true);AckmanRumbleSuspend(false);AckmanRumbleBegin(true,0xa483,0xea);AckmanRumbleCharge(true,false);AckmanRumbleEnd(ACK_RUMBLE_NONE,0xa483,0xea);
    AckmanRumbleBegin(true,0xa483,0xea);AckmanRumbleCharge(false,false);AckmanRumbleEnd(ACK_RUMBLE_NONE,0xa483,0xea);assert(!low && !high); // Interruption is not release.
    uint8_t ignition[8]={0};ignition[3]=0xb8;
    AckmanRumbleBegin(true,0xa22e,0x3);AckmanRumbleSound(0x38,0x2800,0xa22e);AckmanRumbleDspKeyOn(8,ignition);AckmanRumbleEnd(ACK_RUMBLE_NONE,0xa22e,0x6);assert(low==0x6000 && high==0x2800);
    for(int i=0;i<7;i++){SDL_Delay(65);AckmanRumbleBegin(true,0xa22e,0x1c);AckmanRumbleSpcCommands(ignition);AckmanRumbleEnd(ACK_RUMBLE_NONE,0xa22e,0x1c);assert(low==0x6000);} // No fixed 400 ms timeout.
    ignition[3]=0;AckmanRumbleSpcCommands(ignition);AckmanRumbleBegin(true,0xa22e,0x4b);AckmanRumbleEnd(ACK_RUMBLE_NONE,0xa22e,0x4b);assert(!low && !high);
    AckmanRumbleBegin(true,0xa483,0xea);AckmanRumbleMineExplosion();AckmanRumbleEnd(ACK_RUMBLE_NONE,0xa483,0xea);assert(low==0x9000 && high==0xa000);
    AckmanRumbleSuspend(true);AckmanRumbleSuspend(false);
    uint8_t commands[8]={0};commands[1]=0xb0;
    AckmanRumbleBegin(true,0x9be4,0x1a);AckmanRumbleDspKeyOn(2,commands);AckmanRumbleEnd(ACK_RUMBLE_END_CLIMAX,0x9be4,0x1f);assert(!low && !high);
    AckmanRumbleBegin(true,0x9be4,0x1f);AckmanRumbleDspKeyOn(1,commands);AckmanRumbleEnd(ACK_RUMBLE_END_CLIMAX,0x9be4,0x24);assert(!low);
    AckmanRumbleBegin(true,0x9be4,0x24);AckmanRumbleDspKeyOn(2,commands);AckmanRumbleEnd(ACK_RUMBLE_END_CLIMAX,0x9be4,0x24);assert(low==0xffff && high==0xffff);
    SDL_Delay(160);SDL_UpdateJoysticks();assert(!low && !high);
    AckmanRumbleBegin(false,0x9be4,0x24);AckmanRumbleDspKeyOn(2,commands);AckmanRumbleEnd(ACK_RUMBLE_END_CLIMAX,0x9be4,0x24);assert(!low);
    AckmanRumbleBegin(true,0x9be4,0x24);AckmanRumbleDspKeyOn(2,commands);AckmanRumbleEnd(ACK_RUMBLE_END_CLIMAX,0x9be4,0x24);assert(low==0xffff);
    AckmanRumbleSuspend(true);AckmanRumbleSuspend(false);
    uint8_t boss_commands[8]={0};
    AckmanRumbleBegin(true,0xa483,0xea);AckmanRumbleBossHit();AckmanRumbleBossContext(false);AckmanRumbleEnemyHit();AckmanRumbleEnd(ACK_RUMBLE_NONE,0xa483,0xea);assert(low==0x8000 && high==0x8000); // Fatal hit owns motors before the first audio burst.
    for(int i=0;i<5;i++){SDL_Delay(65);AckmanRumbleBegin(true,0xa483,0xea);AckmanRumbleBossContext(true);AckmanRumbleBossSpc(boss_commands);AckmanRumbleEnd(ACK_RUMBLE_NONE,0xa483,0xea);assert(low==0x8000);} // Stay continuous between native bursts.
    boss_commands[1]=0x9d;AckmanRumbleBegin(true,0xa483,0xea);AckmanRumbleBossContext(false);AckmanRumbleBossSpc(boss_commands);AckmanRumbleEnd(ACK_RUMBLE_NONE,0xa483,0xea);assert(low==0x8000); // Actor removed; sound tail still alive.
    boss_commands[1]=0;AckmanRumbleBossSpc(boss_commands);AckmanRumbleBegin(true,0xa483,0xea);AckmanRumbleBossContext(false);AckmanRumbleEnd(ACK_RUMBLE_NONE,0xa483,0xea);assert(!low && !high);
    boss_commands[1]=0x9d;AckmanRumbleBegin(true,0xa483,0xea);AckmanRumbleBossSpc(boss_commands);AckmanRumbleEnd(ACK_RUMBLE_NONE,0xa483,0xea);assert(!low); // Ordinary $1D explosions cannot arm a boss session.
    AckmanRumbleBegin(false,0xa483,0xea);AckmanRumbleBossHit();AckmanRumbleBossContext(true);AckmanRumbleBossSpc(boss_commands);AckmanRumbleEnd(ACK_RUMBLE_NONE,0xa483,0xea);assert(!low);
    AckmanRumbleBegin(true,0xa483,0xea);AckmanRumbleBossContext(true);AckmanRumbleEnd(ACK_RUMBLE_NONE,0xa483,0xea);assert(low==0x8000);AckmanRumbleSuspend(true);assert(!low);AckmanRumbleBossSpc(boss_commands);AckmanRumbleSuspend(false);
    AckmanRumbleBegin(true,0xa483,0xea);AckmanRumbleBossContext(false);AckmanRumbleEnd(ACK_RUMBLE_NONE,0xa483,0xea);assert(!low); // Late callback after suspension is discarded.
    AckmanRumbleShutdown();SDL_CloseJoystick(joy);assert(SDL_DetachVirtualJoystick(id));SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}
