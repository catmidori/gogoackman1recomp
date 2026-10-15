#include "desktop/sdl_compat.h"
#include "ackman_audio.h"
#include "ackman_log.h"
#include "ackman_menu.h"
#include "ackman_devices.h"
#include "ackman_cursor.h"
#include "ackman_debug.h"
#include "desktop/config.h"
#include "common_rtl.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
void AckmanPauseForDialog(void) { AckmanHostDialogPause(); }
void AckmanDebugQuit(void) {
    AckmanDevicesShutdown();
    AckmanLogShutdown(); /* join before SDL tears down threading services */
    SDL_Quit();
}
bool AckmanSetRenderVSync(SDL_Renderer *renderer,int interval) {
    bool ok=SDL_SetRenderVSync(renderer,interval);
    int actual=-1;
    SDL_GetRenderVSync(renderer,&actual);
    fprintf(stderr,"[ackman-video] SDL VSync requested=%d actual=%d success=%d%s%s\n",
        interval,actual,ok,ok?"":" error=",ok?"":SDL_GetError());
    return ok;
}
/* Only host_main.c's event calls are renamed by CMake. Consume both edges
 * so F5/F6 cannot also invoke the host's savestate bindings. */
bool AckmanPollEvent(SDL_Event *event) {
    AckmanCursorUpdate(NULL); /* Runs even with no events or a paused core. */
    AckmanMenuSync();
    while(SDL_PollEvent(event)) {
        AckmanMenuSync();
        if(AckmanMenuEvent(event))continue;
        const char *opt=getenv("ACKMAN_DEBUG_KEYS");
        if(event && (opt && !strcmp(opt,"1")) &&
           (event->type==SDL_EVENT_KEY_DOWN || event->type==SDL_EVENT_KEY_UP) &&
           (event->key.key==SDLK_F5 || event->key.key==SDLK_F6)) {
            if(event->type==SDL_EVENT_KEY_DOWN && !event->key.repeat)
                AckmanAudioDebug(event->key.key==SDLK_F5);
            continue;
        }
        return true;
    }
    return false;
}
