#include "recomp_menu_devices.h"
typedef struct Device {
    struct Device *next;
    RecompDeviceInfo info;
    bool wanted,published,dirty;
} Device;
static Device *devices;
static SDL_Mutex *mutex;
static SDL_Condition *condition;
static SDL_Thread *worker;
static Uint32 ready_event;
static bool quitting;
static void (*on_changed)(void);
static Device *find(SDL_JoystickID id) {
    for(Device *d=devices;d;d=d->next)if(d->info.id==id)return d;
    return NULL;
}
static int SDLCALL device_worker(void *unused) {
    (void)unused;
    for(;;) {
        SDL_LockMutex(mutex);
        Device *d=NULL;
        while(!quitting) {
            for(d=devices;d;d=d->next)if(d->dirty)break;
            if(d)break;
            SDL_WaitCondition(condition,mutex);
        }
        if(quitting){SDL_UnlockMutex(mutex);break;}
        d->dirty=false;bool open=d->wanted;
        RecompDeviceInfo info=d->info;
        SDL_UnlockMutex(mutex);
        // Discovery/open/close never hold our queue mutex or run on the frame thread.
        if(open && !info.pad && !info.joystick) {
            if(SDL_IsGamepad(info.id))info.pad=SDL_OpenGamepad(info.id);
            else info.joystick=SDL_OpenJoystick(info.id);
            if(info.pad) {
                info.joystick=SDL_GetGamepadJoystick(info.pad);
                info.player=SDL_GetGamepadPlayerIndex(info.pad);
                const char *name=SDL_GetGamepadName(info.pad);SDL_strlcpy(info.name,name?name:"",sizeof(info.name));
            } else if(info.joystick) {
                info.player=SDL_GetJoystickPlayerIndex(info.joystick);
                const char *name=SDL_GetJoystickName(info.joystick);SDL_strlcpy(info.name,name?name:"",sizeof(info.name));
            }
            SDL_LockMutex(mutex);
            bool keep=d->wanted && !quitting;
            if(keep)d->info=info;
            SDL_UnlockMutex(mutex);
            if(keep && info.joystick) {
                SDL_Event event={0};event.type=ready_event;event.user.code=(Sint32)info.id;
                if(!SDL_PushEvent(&event)) { // Avoid leaking an unpublished handle on event-queue failure.
                    SDL_LockMutex(mutex);d->wanted=false;d->dirty=true;SDL_UnlockMutex(mutex);
                }
            } else {
                if(info.pad)SDL_CloseGamepad(info.pad);else if(info.joystick)SDL_CloseJoystick(info.joystick);
            }
        } else if(!open && info.joystick) {
            if(info.pad)SDL_CloseGamepad(info.pad);else SDL_CloseJoystick(info.joystick);
            SDL_LockMutex(mutex);d->info.pad=NULL;d->info.joystick=NULL;SDL_UnlockMutex(mutex);
        }
    }
    return 0;
}
bool RecompDevicesInit(void (*changed)(void)) {
    if(worker){on_changed=changed;return true;}
    ready_event=SDL_RegisterEvents(1);if(!ready_event)return false;
    mutex=SDL_CreateMutex();condition=SDL_CreateCondition();quitting=false;on_changed=changed;
    if(mutex && condition)worker=SDL_CreateThread(device_worker,"SNES menu devices",NULL);
    if(!worker){SDL_DestroyCondition(condition);SDL_DestroyMutex(mutex);condition=NULL;mutex=NULL;return false;}
    return true;
}
void RecompDevicesRequest(SDL_JoystickID id) {
    if(!worker || !id || id==(SDL_JoystickID)-1)return;
    SDL_LockMutex(mutex);Device *d=find(id);
    if(!d){d=SDL_calloc(1,sizeof(*d));if(d){d->info.id=id;d->info.player=-1;d->next=devices;devices=d;}}
    if(d && !d->wanted){d->wanted=true;d->dirty=true;SDL_SignalCondition(condition);}
    SDL_UnlockMutex(mutex);
}
void RecompDevicesRemove(SDL_JoystickID id) {
    if(!worker)return;
    SDL_LockMutex(mutex);Device *d=find(id);
    bool changed=d && d->published;
    if(d){d->wanted=false;d->published=false;}
    SDL_UnlockMutex(mutex);
    // Detach ImGui's manual pointers BEFORE the worker may close them.
    if(changed && on_changed)on_changed();
    SDL_LockMutex(mutex);if(d){d->dirty=true;SDL_SignalCondition(condition);}SDL_UnlockMutex(mutex);
}
void RecompDevicesEvent(const SDL_Event *event) {
    if(event->type==SDL_EVENT_GAMEPAD_ADDED || event->type==SDL_EVENT_JOYSTICK_ADDED)RecompDevicesRequest(event->gdevice.which);
    else if(event->type==SDL_EVENT_GAMEPAD_REMOVED || event->type==SDL_EVENT_JOYSTICK_REMOVED)RecompDevicesRemove(event->gdevice.which);
    else if(worker && event->type==ready_event) {
        SDL_LockMutex(mutex);Device *d=find((SDL_JoystickID)event->user.code);
        bool changed=d && d->wanted && d->info.joystick && !d->published;
        if(changed)d->published=true;
        SDL_UnlockMutex(mutex);if(changed && on_changed)on_changed();
    }
}
bool RecompDevicesReady(const SDL_Event *event,RecompDeviceInfo *info) {
    if(!worker || event->type!=ready_event)return false;
    SDL_LockMutex(mutex);Device *d=find((SDL_JoystickID)event->user.code);
    if(d && d->wanted && d->published)*info=d->info;else SDL_zero(*info);
    SDL_UnlockMutex(mutex);return true;
}
int RecompDevicesGamepads(SDL_Gamepad **pads,int capacity) {
    if(!worker)return 0;
    int count=0;SDL_LockMutex(mutex);
    for(Device *d=devices;d && count<capacity;d=d->next)
        if(d->wanted && d->published && d->info.pad)pads[count++]=d->info.pad;
    SDL_UnlockMutex(mutex);return count;
}
int RecompDevicesJoysticks(SDL_JoystickID *ids,int capacity) {
    if(!worker || !ids || capacity<=0)return 0;
    int count=0;SDL_LockMutex(mutex);
    for(Device *d=devices;d && count<capacity;d=d->next)
        if(d->wanted && d->published && d->info.joystick)ids[count++]=d->info.id;
    SDL_UnlockMutex(mutex);return count;
}
void RecompDevicesShutdown(void) {
    if(!worker)return;
    SDL_LockMutex(mutex);quitting=true;SDL_SignalCondition(condition);SDL_UnlockMutex(mutex);
    SDL_WaitThread(worker,NULL);worker=NULL;
    // Shutdown runs after ImGui, outside gameplay. No consumer retains these pointers.
    for(Device *d=devices;d;) {
        Device *next=d->next;
        if(d->info.pad)SDL_CloseGamepad(d->info.pad);else if(d->info.joystick)SDL_CloseJoystick(d->info.joystick);
        SDL_free(d);d=next;
    }
    devices=NULL;on_changed=NULL;SDL_DestroyCondition(condition);SDL_DestroyMutex(mutex);condition=NULL;mutex=NULL;
}
