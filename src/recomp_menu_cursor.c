#include "recomp_menu_cursor.h"

static Uint64 last_activity;
static bool initialized;

void RecompMenuCursorReset(void) { initialized=false; }

void RecompMenuCursorUpdate(const SDL_Event *event) {
    if(!(SDL_WasInit(SDL_INIT_VIDEO)&SDL_INIT_VIDEO)) return;
    Uint64 now=SDL_GetTicks();
    if(!initialized) {
        last_activity=now;initialized=true;SDL_ShowCursor();
    }
    if(event && (event->type==SDL_EVENT_MOUSE_MOTION ||
                 event->type==SDL_EVENT_MOUSE_BUTTON_DOWN ||
                 event->type==SDL_EVENT_MOUSE_BUTTON_UP)) {
        last_activity=now;
        SDL_ShowCursor();
    } else if(now-last_activity>=2000 && SDL_CursorVisible()) {
        SDL_HideCursor();
    }
}
