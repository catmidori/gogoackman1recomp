/* Opt-in end-to-end regression probe; inert in normal gameplay. It injects
 * real SDL virtual states, never runner words or guest RAM input bits. */
#include "ackman_controls.h"
#include "ackman_devices.h"
#include "ackman_input_probe.h"
#include "common_rtl.h"
#include <stdio.h>
#include <stdlib.h>
extern uint8 g_ram[0x20000];
static SDL_Joystick *pad;
static FILE *trace;
static bool initialized,armed,keyboard_left,legacy;

void AckmanInputProbe(unsigned frame) {
    if(!initialized) {
        initialized=true;const char *enabled=getenv("ACKMAN_INPUT_SELFTEST");
        if(!enabled || strcmp(enabled,"1"))return;
        armed=true;legacy=getenv("ACKMAN_INPUT_LEGACY_SOURCE")!=NULL;
        const char *path=getenv("ACKMAN_INPUT_TRACE");trace=path?fopen(path,"w"):NULL;
        SDL_VirtualJoystickDesc desc={0};SDL_INIT_INTERFACE(&desc);
        desc.type=SDL_JOYSTICK_TYPE_GAMEPAD;desc.naxes=SDL_GAMEPAD_AXIS_COUNT;desc.nbuttons=SDL_GAMEPAD_BUTTON_COUNT;
        desc.button_mask=(1u<<SDL_GAMEPAD_BUTTON_COUNT)-1;desc.axis_mask=(1u<<SDL_GAMEPAD_AXIS_COUNT)-1;
        desc.name="Ackman gameplay regression pad (SDL seat 2)";
        SDL_JoystickID id=SDL_AttachVirtualJoystick(&desc);pad=SDL_OpenJoystick(id);
        if(pad)SDL_SetJoystickPlayerIndex(pad,1);
        if(trace){fprintf(trace,"frame,raw_p1,core_p1,joy4218,x,y\n");fflush(trace);}
    }
    if(!pad)return;
    if(trace)fprintf(trace,"%u,%04x,%04x,%04x,%u,%u\n",frame,AckmanControlsReadDevices(),RtlGetPadState(0),
        ReadReg(0x4218)|(ReadReg(0x4219)<<8),g_ram[0x422]|g_ram[0x423]<<8,g_ram[0x426]|g_ram[0x427]<<8);
    if(frame==10){const char *save=getenv("ACKMAN_INPUT_SAVE");if(save && !RtlLoadSnapshot(save))fprintf(stderr,"[input-probe] Fixture save unavailable: %s\n",save);}
    if(frame==40)SDL_SetJoystickVirtualButton(pad,SDL_GAMEPAD_BUTTON_DPAD_RIGHT,true);
    if(frame==80)SDL_SetJoystickVirtualButton(pad,SDL_GAMEPAD_BUTTON_DPAD_RIGHT,false);
    if(frame==100)keyboard_left=true;
    if(frame==140)keyboard_left=false;
    // While paused, --script navigates Controls/Hotkeys then closes the menu.
    if(frame==220)SDL_SetJoystickVirtualButton(pad,SDL_GAMEPAD_BUTTON_DPAD_RIGHT,true);
    if(frame==260)SDL_SetJoystickVirtualButton(pad,SDL_GAMEPAD_BUTTON_DPAD_RIGHT,false);
    if(frame==300)SDL_SetJoystickVirtualAxis(pad,SDL_GAMEPAD_AXIS_LEFTX,24000);
    if(frame==340)SDL_SetJoystickVirtualAxis(pad,SDL_GAMEPAD_AXIS_LEFTX,0);
    if(frame==380)SDL_SetJoystickVirtualButton(pad,SDL_GAMEPAD_BUTTON_SOUTH,true);
    if(frame==400)SDL_SetJoystickVirtualButton(pad,SDL_GAMEPAD_BUTTON_SOUTH,false);
    if(frame==900){SDL_JoystickID id=SDL_GetJoystickID(pad);SDL_CloseJoystick(pad);pad=NULL;SDL_DetachVirtualJoystick(id);}
    if(trace)fflush(trace);
}
void AckmanInputProbeClose(void){if(trace)fclose(trace);trace=NULL;}

const uint8_t *AckmanInputProbeKeyboard(const uint8_t *keys) {
    if(!armed || !keyboard_left)return keys;
    static uint8_t snapshot[SDL_SCANCODE_COUNT];memcpy(snapshot,keys,sizeof(snapshot));snapshot[SDL_SCANCODE_LEFT]=1;
    return snapshot; /* Diagnostic virtual keyboard snapshot, never mutate SDL's state. */
}
bool AckmanInputProbeLegacy(void){return armed && legacy;}
