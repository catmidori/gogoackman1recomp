#include "ackman_devices.h"
#include <assert.h>
static SDL_ThreadID main_thread;
static SDL_AtomicInt opened,closed;
static bool testing;
static SDL_JoystickID fixture;
#ifdef ACK_TEST_DEVICE_DELAY
SDL_Gamepad *__real_SDL_OpenGamepad(SDL_JoystickID id);
void __real_SDL_CloseGamepad(SDL_Gamepad *pad);
SDL_Gamepad *__wrap_SDL_OpenGamepad(SDL_JoystickID id) {
    if(testing){assert(SDL_GetCurrentThreadID()!=main_thread);SDL_Delay(200);SDL_AddAtomicInt(&opened,1);}
    return __real_SDL_OpenGamepad(id);
}
void __wrap_SDL_CloseGamepad(SDL_Gamepad *pad) {
    if(testing){assert(SDL_GetCurrentThreadID()!=main_thread);SDL_AddAtomicInt(&closed,1);}
    __real_SDL_CloseGamepad(pad);
}
#endif
static void pump(void) {
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
        bool device_event=event.type==SDL_EVENT_GAMEPAD_ADDED || event.type==SDL_EVENT_GAMEPAD_REMOVED ||
                          event.type==SDL_EVENT_JOYSTICK_ADDED || event.type==SDL_EVENT_JOYSTICK_REMOVED;
        if(device_event && event.gdevice.which!=fixture)continue;
        AckmanDevicesEvent(&event);
    }
}
void TestDevices(void) {
    assert(SDL_Init(SDL_INIT_GAMEPAD));main_thread=SDL_GetCurrentThreadID();testing=true;
    assert(AckmanDevicesInit(NULL));
    SDL_VirtualJoystickDesc desc={0};SDL_INIT_INTERFACE(&desc);
    desc.type=SDL_JOYSTICK_TYPE_GAMEPAD;desc.naxes=SDL_GAMEPAD_AXIS_COUNT;desc.nbuttons=SDL_GAMEPAD_BUTTON_COUNT;
    SDL_JoystickID unrelated=SDL_AttachVirtualJoystick(&desc);assert(unrelated);
    SDL_JoystickID id=SDL_AttachVirtualJoystick(&desc);fixture=id;assert(id && SDL_IsGamepad(id));
    Uint64 begin=SDL_GetTicks();AckmanDevicesRequest(id);AckmanDevicesRequest(id);
#ifdef ACK_TEST_DEVICE_DELAY
    assert(SDL_GetTicks()-begin<100);
#endif
    SDL_Gamepad *pads[2];int turns=0;
    while(!AckmanDevicesGamepads(pads,2) && SDL_GetTicks()-begin<3000){pump();SDL_Delay(1);turns++;}
    assert(AckmanDevicesGamepads(pads,2)==1 && SDL_GetGamepadID(pads[0])==id);
    SDL_JoystickID cached[2];assert(AckmanDevicesJoysticks(cached,2)==1 && cached[0]==id);
#ifdef ACK_TEST_DEVICE_DELAY
    assert(turns>20 && SDL_GetAtomicInt(&opened)==1);
#endif
    begin=SDL_GetTicks();AckmanDevicesRemove(id);assert(AckmanDevicesGamepads(pads,2)==0 && AckmanDevicesJoysticks(cached,2)==0);
#ifdef ACK_TEST_DEVICE_DELAY
    assert(SDL_GetTicks()-begin<100);
    while(SDL_GetAtomicInt(&closed)<1 && SDL_GetTicks()-begin<3000){pump();SDL_Delay(1);}
    assert(SDL_GetAtomicInt(&closed)==1);
#endif
    assert(SDL_DetachVirtualJoystick(id));pump();
    // Unplug while the worker is opening: no late publication or leaked handle.
    id=SDL_AttachVirtualJoystick(&desc);fixture=id;assert(id);pump();AckmanDevicesRequest(id);
    SDL_Delay(20);AckmanDevicesRemove(id);assert(SDL_DetachVirtualJoystick(id));
    begin=SDL_GetTicks();while(SDL_GetTicks()-begin<300){pump();SDL_Delay(1);}
    assert(AckmanDevicesGamepads(pads,2)==0);
    assert(SDL_DetachVirtualJoystick(unrelated));
    testing=false;AckmanDevicesShutdown();SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
}
