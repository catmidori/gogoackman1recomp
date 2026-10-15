#include "ackman_cursor.h"
#include <assert.h>

static Uint64 clock_ms;
static bool video,visible;
static int shows,hides;
Uint64 __wrap_SDL_GetTicks(void) { return clock_ms; }
SDL_InitFlags __wrap_SDL_WasInit(SDL_InitFlags flags) { return video?(flags&SDL_INIT_VIDEO):0; }
bool __wrap_SDL_ShowCursor(void) { shows++;visible=true;return true; }
bool __wrap_SDL_HideCursor(void) { hides++;visible=false;return true; }
bool __wrap_SDL_CursorVisible(void) { return visible; }

int main(void) {
    AckmanCursorReset();AckmanCursorUpdate(NULL);assert(shows==0 && hides==0);
    video=true;clock_ms=100;AckmanCursorUpdate(NULL);assert(visible && shows==1);
    clock_ms=2099;AckmanCursorUpdate(NULL);assert(visible);
    SDL_Event event={0};event.type=SDL_EVENT_KEY_DOWN;
    clock_ms=2100;AckmanCursorUpdate(&event);assert(!visible && hides==1);
    AckmanCursorUpdate(NULL);assert(hides==1); // No redundant calls while hidden.
    Uint32 interactions[]={SDL_EVENT_MOUSE_MOTION,SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_EVENT_MOUSE_BUTTON_UP};
    for(unsigned i=0;i<sizeof(interactions)/sizeof(interactions[0]);++i) {
        event.type=interactions[i];AckmanCursorUpdate(&event);assert(visible);
        clock_ms+=1999;AckmanCursorUpdate(NULL);assert(visible);
        clock_ms++;AckmanCursorUpdate(NULL);assert(!visible);
    }
    event.type=SDL_EVENT_MOUSE_MOTION;AckmanCursorUpdate(&event);assert(visible);
    clock_ms+=1500;event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;AckmanCursorUpdate(&event);
    clock_ms+=1500;event.type=SDL_EVENT_MOUSE_BUTTON_UP;AckmanCursorUpdate(&event);
    clock_ms+=1999;event.type=SDL_EVENT_MOUSE_WHEEL;AckmanCursorUpdate(&event);assert(visible);
    clock_ms++;AckmanCursorUpdate(NULL);assert(!visible);
    AckmanCursorReset();AckmanCursorUpdate(NULL);assert(visible);
    return 0;
}
