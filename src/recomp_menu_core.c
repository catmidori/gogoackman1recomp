#include "recomp_menu_controls.h"
#include "recomp_menu_hotkeys.h"
#include "recomp_menu_devices.h"
#include "recomp_menu_core.h"
#include "recomp_menu_host.h"
#ifdef _WIN32
#include <windows.h>
#undef FORCEINLINE
#else
#include <sys/stat.h>
#endif
#include "recomp_menu_cursor.h"
#include "desktop/config.h"
#include "common_rtl.h"
#include "launcher_cache.h"
#include "snes_osd.h"
#include "snes_runahead.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
static SDL_Window *window;
static void (*boot_observer)(bool);
void RecompMenuSetBootObserver(void (*observer)(bool)){boot_observer=observer;}
static Uint32 command_event,wake_mouse_buttons;
static SDL_AtomicInt dialog_pending;
bool RecompMenuDialogActive(void) { return SDL_GetAtomicInt(&dialog_pending)!=0; }
static char save_dir[4096],rom_dir[4096],relaunch[4096];
static char mods_dir[4096],ips_path[4096],translation_rom[4096];
static char shader_dir[4096];
static char **shader_names;
static int shader_count;
static bool shader_restart,output_initialized,state_restart,active_shaders;
static int active_output;
static char active_shader[1024];
typedef struct MenuEntryState {
    bool valid,shaders;int output;char shader[1024];char **mods;size_t count;
} MenuEntryState;
static MenuEntryState entry;
static void clear_entry(void) {
    for(size_t i=0;i<entry.count;i++)SDL_free(entry.mods[i]);
    SDL_free(entry.mods);memset(&entry,0,sizeof(entry));
}
int RecompMenuActiveOutput(void){return output_initialized?active_output:g_config.output_method;}
bool RecompMenuActiveShaders(void){return output_initialized?active_shaders:g_config.enable_shaders;}
const char *RecompMenuActiveShader(void){return output_initialized?active_shader:g_config.selected_shader;}
void RecompMenuShaderFailure(void) {
    if(shader_restart)return;
    shader_restart=true;active_shaders=false; /* runtime fault, never a widget change */
    if(!RecompMenuVisible())RecompMenuClosed();
}
const char *RecompMenuShaderDirectory(void) {
#ifdef RECOMP_MENU_FLATPAK
    if(!shader_dir[0])SDL_snprintf(shader_dir,sizeof(shader_dir),"%sshaders/",RecompMenuWritableDirectory());
#else
    if(!shader_dir[0])SDL_snprintf(shader_dir,sizeof(shader_dir),"%sshaders/",RecompMenuAssetDirectory());
#endif
    return shader_dir;
}
static int compare_shader(const void *a,const void *b) {return SDL_strcasecmp(*(char *const *)a,*(char *const *)b);}
static void free_shaders(void) {
    for(int i=0;i<shader_count;i++)SDL_free(shader_names[i]);
    SDL_free(shader_names);shader_names=NULL;shader_count=0;
}
static void scan_shaders(const char *relative,int depth) {
    if(depth>32 || shader_count>=4096)return;
    char folder[4096];SDL_snprintf(folder,sizeof(folder),"%s%s",RecompMenuShaderDirectory(),relative);
    int count=0;char **files=SDL_GlobDirectory(folder,"*",0,&count);if(!files)return;
    for(int i=0;i<count && shader_count<4096;i++) {
        if(files[i][0]=='.')continue;
        char name[1024],path[4096];SDL_PathInfo info;
        if(SDL_snprintf(name,sizeof(name),"%s%s",relative,files[i])>=(int)sizeof(name))continue;
        SDL_snprintf(path,sizeof(path),"%s%s",RecompMenuShaderDirectory(),name);
#ifdef _WIN32
        wchar_t wide[4096];if(!MultiByteToWideChar(CP_UTF8,0,path,-1,wide,4096))continue;
        DWORD attributes=GetFileAttributesW(wide);
        if(attributes==INVALID_FILE_ATTRIBUTES || (attributes&FILE_ATTRIBUTE_REPARSE_POINT))continue;
#else
        struct stat status;if(lstat(path,&status) || S_ISLNK(status.st_mode))continue;
#endif
        if(!SDL_GetPathInfo(path,&info))continue;
        if(info.type==SDL_PATHTYPE_DIRECTORY) {
            size_t length=strlen(name);if(length+2>sizeof(name))continue;
            name[length]='/';name[length+1]=0;scan_shaders(name,depth+1);continue;
        }
        const char *ext=strrchr(name,'.');
        if(info.type!=SDL_PATHTYPE_FILE || !ext || (SDL_strcasecmp(ext,".glsl") && SDL_strcasecmp(ext,".glslp")))continue;
        char *copy=SDL_strdup(name);if(!copy)continue;
        char **next=SDL_realloc(shader_names,(shader_count+1)*sizeof(*next));
        if(!next){SDL_free(copy);continue;}shader_names=next;shader_names[shader_count++]=copy;
    }
    SDL_free(files);
}
static void save_shader_preferences(void) {
    RecompIniValue values[]={ {"Graphics","EnableShaders",g_config.enable_shaders?"1":"0"},
                             {"Graphics","SelectedShader",g_config.selected_shader} };
    if(!RecompMenuServices()->ConfigUpdate(values,2))SDL_Log("Unable to persist shader preferences");
}
void RecompMenuSetShaders(bool enabled) {g_config.enable_shaders=enabled;}
void RecompMenuShadersRefresh(void) {
    free_shaders();scan_shaders("",0);
    if(shader_count>1)qsort(shader_names,shader_count,sizeof(*shader_names),compare_shader);
    bool found=false;
    for(int i=0;i<shader_count;i++)if(!strcmp(g_config.selected_shader,shader_names[i]))found=true;
    if(!found && g_config.selected_shader[0]) {g_config.selected_shader[0]=0;save_shader_preferences();}
}
int RecompMenuShaderCount(void) {return shader_count;}
const char *RecompMenuShaderName(int index) {return index>=0 && index<shader_count?shader_names[index]:"";}
void RecompMenuSelectShader(int index) {
    const char *name=RecompMenuShaderName(index);
    if(!strcmp(name,g_config.selected_shader))g_config.selected_shader[0]=0;
    else SDL_strlcpy(g_config.selected_shader,name,sizeof(g_config.selected_shader));
}
static bool cold_boot,relaunch_fullscreen;
bool RecompMenuColdBootPending(void) { return cold_boot || state_restart; }
typedef struct { char *name;bool active,initial;RecompModMetadata metadata;bool selected; } MenuMod;
static MenuMod *menu_mods;
static int mod_count,mod_selected=-1;
static int mod_anchor=-1;
static bool mods_snapshot;
static const char **active_mod_names(size_t *count) {
    const char **names=SDL_calloc(mod_count?mod_count:1,sizeof(*names));*count=0;
    if(names)for(int i=0;i<mod_count;i++)if(menu_mods[i].active)names[(*count)++]=menu_mods[i].name;
    return names;
}
static bool persist_state(const RecompIniValue *values,size_t count) {
    size_t n;const char **names=active_mod_names(&n);if(!names)return false;
    bool ok=RecompMenuServices()->ConfigCommitRuntime(values,count,names,n);SDL_free(names);return ok;
}
void RecompMenuOpened(void) {
    clear_entry();size_t n;const char **names=active_mod_names(&n);if(!names)return;
    entry.mods=SDL_calloc(n?n:1,sizeof(*entry.mods));if(!entry.mods){SDL_free(names);return;}
    for(size_t i=0;i<n;i++) {
        entry.mods[i]=SDL_strdup(names[i]);
        if(!entry.mods[i]){entry.count=i;clear_entry();SDL_free(names);return;}
    }
    SDL_free(names);entry.count=n;entry.output=g_config.output_method;entry.shaders=g_config.enable_shaders;
    SDL_strlcpy(entry.shader,g_config.selected_shader,sizeof(entry.shader));entry.valid=true;
}
bool RecompMenuClosed(void) {
    if(state_restart)return true; /* publish and queue once */
    if(!entry.valid && !shader_restart)return true;
    size_t i=0;bool mods_changed=false;
    for(int j=0;j<mod_count;j++)if(menu_mods[j].active) {
        if(i>=entry.count || strcmp(menu_mods[j].name,entry.mods[i]))mods_changed=true;
        i++;
    }
    mods_changed|=i!=entry.count;
    bool driver_changed=entry.valid && g_config.output_method!=entry.output;
    bool shaders_changed=g_config.enable_shaders!=entry.shaders;
    bool selection_changed=strcmp(g_config.selected_shader,entry.shader)!=0;
    bool changed=driver_changed || mods_changed || shaders_changed || selection_changed;
    if(!changed && !shader_restart)return true;
    /* A dormant selection is a preference only. Do not suppress recovery from
     * a renderer fault, or changes to the driver, activation flag or mod list. */
    if(entry.valid && !shader_restart && selection_changed && !driver_changed &&
       !mods_changed && !shaders_changed && !g_config.enable_shaders)return true;
    if(driver_changed && g_config.output_method!=kOutputMethod_OpenGL)g_config.enable_shaders=false;
    if(!RecompMenuAutosaveForced(true))return false;
    RecompIniValue values[]={
        {"Graphics","OutputMethod",g_config.output_method==kOutputMethod_OpenGL?"OpenGL":g_config.output_method==kOutputMethod_SDLSoftware?"SDL-Software":"SDL"},
        {"Graphics","EnableShaders",g_config.enable_shaders?"1":"0"},
        {"Graphics","SelectedShader",g_config.selected_shader}
    };
    if(!persist_state(values,3))return false;
    state_restart=true;RecompMenuQueueCommand(RECOMP_MENU_RESET);return true;
}
const char *RecompMenuModsDirectory(void) {
    if(!mods_dir[0]) {
        const char *base=RecompMenuWritableDirectory();if(!base)return "";
        SDL_snprintf(mods_dir,sizeof(mods_dir),"%smods/",base);
    }
    return mods_dir;
}
const char *RecompMenuIpsPath(void) { return ips_path; }
static int compare_mods(const void *first,const void *second) {
    return SDL_strcasecmp(((const MenuMod *)first)->name,((const MenuMod *)second)->name);
}
static void read_active_mod(void *context,const char *section,const char *name,const char *value) {
    (void)context;(void)value;
    if(SDL_strcasecmp(section,"ActiveMods") || !RecompMenuServices()->ModFilenameValid(name))return;
    for(int i=0;i<mod_count;i++)if(!strcmp(name,menu_mods[i].name)){menu_mods[i].active=true;break;}
}
bool RecompMenuModsSave(void) {
    const char **names=SDL_calloc(mod_count?mod_count:1,sizeof(*names));if(!names)return false;
    size_t count=0;
    for(int i=0;i<mod_count;i++)if(menu_mods[i].active && RecompMenuServices()->ModFilenameValid(menu_mods[i].name))names[count++]=menu_mods[i].name;
    bool ok=RecompMenuServices()->ConfigWriteActiveMods(names,count);SDL_free(names);return ok;
}
void RecompMenuModSetActive(int index,bool active) {
    if(index>=0 && index<mod_count)menu_mods[index].active=active;
}
static void scan_mods(const char *relative,int depth) {
    if(depth>32 || mod_count>=4096)return;
    char folder[4096];SDL_snprintf(folder,sizeof(folder),"%s%s",RecompMenuModsDirectory(),relative);
    int count=0;char **files=SDL_GlobDirectory(folder,"*",0,&count);if(!files)return;
    for(int i=0;i<count;i++) {
        char name[1024],path[4096];SDL_PathInfo info;
        if(files[i][0]=='.' || (!depth && !SDL_strcasecmp(files[i],"preloaded")))continue;
        if(SDL_snprintf(name,sizeof(name),"%s%s",relative,files[i])>=(int)sizeof(name))continue;
        SDL_snprintf(path,sizeof(path),"%s%s",RecompMenuModsDirectory(),name);
#ifdef _WIN32
        wchar_t wide[4096];
        if(!MultiByteToWideChar(CP_UTF8,0,path,-1,wide,4096))continue;
        DWORD attributes=GetFileAttributesW(wide);
        if(attributes==INVALID_FILE_ATTRIBUTES || (attributes&FILE_ATTRIBUTE_REPARSE_POINT))continue;
#else
        struct stat status;if(lstat(path,&status) || S_ISLNK(status.st_mode))continue;
#endif
        if(!SDL_GetPathInfo(path,&info))continue;
        if(info.type==SDL_PATHTYPE_DIRECTORY) {
            size_t size=strlen(name);if(size+1>=sizeof(name))continue;
            name[size]='/';name[size+1]=0;scan_mods(name,depth+1);continue;
        }
        if(info.type!=SDL_PATHTYPE_FILE || !RecompMenuServices()->ModFilenameValid(name))continue;
        bool found=false;for(int j=0;j<mod_count;j++)if(!strcmp(name,menu_mods[j].name)){found=true;break;}
        if(found || mod_count>=4096)continue;
        char *copy=SDL_strdup(name);if(!copy)continue;
        MenuMod *next=SDL_realloc(menu_mods,(mod_count+1)*sizeof(*next));
        if(!next){SDL_free(copy);continue;}menu_mods=next;
        menu_mods[mod_count]=(MenuMod){.name=copy};
        RecompModMetadataRead(path,&menu_mods[mod_count++].metadata);
    }
    SDL_free(files);
}
void RecompMenuModsInit(void) {
    for(int i=0;i<mod_count;i++){SDL_free(menu_mods[i].name);RecompModMetadataClear(&menu_mods[i].metadata);}
    SDL_free(menu_mods);menu_mods=NULL;mod_count=0;mod_selected=mod_anchor=-1;mods_snapshot=false;ips_path[0]=0;
    const char *directory=RecompMenuModsDirectory();SDL_CreateDirectory(directory);
    scan_mods("",0);if(mod_count>1)SDL_qsort(menu_mods,mod_count,sizeof(*menu_mods),compare_mods);
    RecompMenuServices()->IniRead(RecompMenuServices()->ConfigPath(),read_active_mod,NULL);
    RecompMenuModsSave(); /* Remove stale/unsafe entries before any core frame. */
}
/* Add newly converted archives without rebuilding the RAM activation model. */
void RecompMenuModsRefresh(void) {
    const char *directory=RecompMenuModsDirectory();
    char *selected=mod_selected>=0?SDL_strdup(menu_mods[mod_selected].name):NULL;
    char *anchor=mod_anchor>=0?SDL_strdup(menu_mods[mod_anchor].name):NULL;
    scan_mods("",0);if(mod_count>1)SDL_qsort(menu_mods,mod_count,sizeof(*menu_mods),compare_mods);
    mod_selected=-1;if(selected)for(int i=0;i<mod_count;i++)if(!strcmp(selected,menu_mods[i].name)){mod_selected=i;break;}
    SDL_free(selected);
    mod_anchor=-1;if(anchor)for(int i=0;i<mod_count;i++)if(!strcmp(anchor,menu_mods[i].name)){mod_anchor=i;break;}
    SDL_free(anchor);
    /* Re-conversion can replace an existing archive; refresh its preview too. */
    for(int i=0;i<mod_count;i++) {
        char path[4096];SDL_snprintf(path,sizeof(path),"%s%s",directory,menu_mods[i].name);
        RecompModMetadataRead(path,&menu_mods[i].metadata);
    }
}
void RecompMenuModsOpened(void) {
    for(int i=0;i<mod_count;i++)menu_mods[i].initial=menu_mods[i].active;
    mods_snapshot=true;
}
int RecompMenuModsCount(void) { return mod_count; }
const char *RecompMenuModName(int index) { return index>=0 && index<mod_count?menu_mods[index].name:""; }
const RecompModMetadata *RecompMenuModMetadata(int index) {
    static const RecompModMetadata empty={0};
    return index>=0 && index<mod_count?&menu_mods[index].metadata:&empty;
}
bool RecompMenuModActive(int index) { return index>=0 && index<mod_count && menu_mods[index].active; }
int RecompMenuModSelected(void) { return mod_selected; }
bool RecompMenuModHighlighted(int index) { return index>=0 && index<mod_count && menu_mods[index].selected; }
void RecompMenuModSelect(int index) {
    for(int i=0;i<mod_count;i++)menu_mods[i].selected=i==index;
    mod_selected=mod_anchor=index>=0 && index<mod_count?index:-1;
}
void RecompMenuModClick(int index,bool control,bool shift) {
    if(index<0 || index>=mod_count)return;
    bool active=menu_mods[index].active,was_selected=menu_mods[index].selected;
    for(int i=0;i<mod_count;i++)if(menu_mods[i].active!=active)menu_mods[i].selected=false;
    if(shift && mod_anchor>=0 && menu_mods[mod_anchor].active==active) {
        int min=index<mod_anchor?index:mod_anchor,max=index>mod_anchor?index:mod_anchor;
        for(int i=0;i<mod_count;i++) {
            if(!control)menu_mods[i].selected=false;
            if(i>=min && i<=max && menu_mods[i].active==active)menu_mods[i].selected=true;
        }
    } else {
        if(!control)for(int i=0;i<mod_count;i++)menu_mods[i].selected=false;
        menu_mods[index].selected=!was_selected;mod_anchor=index;
    }
    mod_selected=menu_mods[index].selected?index:-1;
    if(mod_selected<0)for(int i=0;i<mod_count;i++)if(menu_mods[i].selected)mod_selected=i;
}
void RecompMenuModActivate(int index) {
    if(index<0 || index>=mod_count)return;
    bool any=false;for(int i=0;i<mod_count;i++)any|=menu_mods[i].selected && menu_mods[i].active==menu_mods[index].active;
    if(!any)RecompMenuModSelect(index);
    RecompMenuModTransfer();
}
void RecompMenuModTransfer(void) {
    for(int i=0;i<mod_count;i++)if(menu_mods[i].selected){menu_mods[i].active=!menu_mods[i].active;menu_mods[i].selected=false;}
    mod_selected=mod_anchor=-1;
}
bool RecompMenuModsChanged(void) {
    if(mods_snapshot)for(int i=0;i<mod_count;i++)if(menu_mods[i].active!=menu_mods[i].initial)return true;
    return false;
}
static unsigned settings_pending;
const char *RecompMenuSaveDirectory(void) {
    if(!save_dir[0]) {
        const char *base=RecompMenuWritableDirectory();if(!base)return "";
        snprintf(save_dir,sizeof(save_dir),"%ssaves/",base);
    }
    return save_dir;
}
const char *RecompMenuRomDirectory(void) {
    if(!rom_dir[0]) {
        const char *base=RecompMenuWritableDirectory();if(!base)return "";
        snprintf(rom_dir,sizeof(rom_dir),"%srom/",base);
    }
    SDL_CreateDirectory(rom_dir);return rom_dir;
}
void RecompMenuSetWidescreen(bool active) {
    g_config.ignore_aspect_ratio=active;RecompMenuServices()->VideoSetWidescreen(active);
}
int RecompMenuWindowMultiplier(void) {
    int scale=(g_config.window_height+112)/224;return scale<1?1:scale>9?9:scale;
}
void RecompMenuSetWindowSize(int multiplier) {
    if(multiplier<1)multiplier=1;if(multiplier>9)multiplier=9;
    g_config.window_width=448*multiplier;g_config.window_height=224*multiplier;
    settings_pending|=RECOMP_SETTINGS_SIZE;
}
void RecompMenuSetInternalScale(int scale) {
    g_config.window_scale=scale<1?1:scale>10?10:scale;
}
void RecompMenuSetAudio(bool active) {
    if(g_config.enable_audio!=active) { g_config.enable_audio=active;settings_pending|=RECOMP_SETTINGS_AUDIO; }
}
void RecompMenuSetVolume(int volume) {
    g_config.volume=volume<0?0:volume>100?100:volume;settings_pending|=RECOMP_SETTINGS_VOLUME;
}
void RecompMenuSetSampleRate(int frequency) {
    if(frequency!=32000 && frequency!=44100 && frequency!=48000)return;
    if(g_config.audio_freq!=frequency) { g_config.audio_freq=frequency;settings_pending|=RECOMP_SETTINGS_AUDIO; }
}
static SDL_Keymod alt_held;
static bool focus_lost,renderer_reset;
unsigned RecompMenuTakeSettings(void) { unsigned value=settings_pending;settings_pending=0;return value; }
bool RecompMenuInactive(void) { return focus_lost && g_config.pause_when_inactive; }
int RecompMenuRunAheadMax(void) { return SNES_RUNAHEAD_MAX<10?SNES_RUNAHEAD_MAX:10; }
void RecompMenuSetRewindInterval(int interval) {
    if(interval!=2 && interval!=6)interval=4;
    if(g_config.rewind_interval!=interval){g_config.rewind_interval=interval;settings_pending|=RECOMP_SETTINGS_REWIND;}
}
void RecompMenuSetRunAhead(int frames) {
    if(frames<0)frames=0;if(frames>RecompMenuRunAheadMax())frames=RecompMenuRunAheadMax();
    if(g_config.run_ahead!=frames) { g_config.run_ahead=frames;settings_pending|=RECOMP_SETTINGS_RUNAHEAD; }
}
void RecompMenuSetFullscreen(bool active) {
#ifdef _WIN32
    RecompMenuUiLockMouseFocus();
#endif
#if defined(__linux__) && defined(RECOMP_MENU_FLATPAK)
    if(window) {
        if(SDL_SetWindowFullscreen(window,active))SDL_SyncWindow(window);
        g_config.fullscreen=(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN)!=0;
        alt_held=0;
        SDL_SetModState(SDL_GetModState()&~SDL_KMOD_ALT);
        RecompMenuUiSuppressInput();
        return;
    }
#endif
    if(g_config.fullscreen!=(unsigned)active) { g_config.fullscreen=active;settings_pending|=RECOMP_SETTINGS_FULLSCREEN; }
}
bool RecompMenuFullscreen(void) {
#if defined(__linux__) && defined(RECOMP_MENU_FLATPAK)
    if(window)g_config.fullscreen=(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN)!=0;
#endif
    return g_config.fullscreen!=0;
}
bool RecompMenuSetFullscreenCheckbox(bool active,bool gamepad) {
#if defined(__linux__) && defined(RECOMP_MENU_FLATPAK)
    static Uint64 gamepad_until;
    Uint64 now=SDL_GetTicks();
    if(gamepad && now<gamepad_until)return false;
#else
    (void)gamepad;
#endif
    RecompMenuSetFullscreen(active);
#if defined(__linux__) && defined(RECOMP_MENU_FLATPAK)
    // Start after the compositor finishes, not before a potentially slow sync.
    if(gamepad)gamepad_until=SDL_GetTicks()+300;
#endif
    return true;
}
bool RecompMenuSoftwareBackbufferSize(SDL_Renderer *renderer,int *width,int *height) {
#if defined(__linux__) && defined(RECOMP_MENU_FLATPAK)
    return RecompMenuActiveOutput()==kOutputMethod_SDLSoftware &&
        SDL_GetRenderOutputSize(renderer,width,height) && *width>0 && *height>0;
#else
    (void)renderer;(void)width;(void)height;return false;
#endif
}
void RecompMenuSetControllerVibration(bool active) {
    bool changed=g_config.controller_vibration!=active;
    g_config.controller_vibration=active;
    RecompMenuServices()->RumbleSetEnabled(active);
    if(changed && active && RecompMenuServices()->RumbleActivationFeedback)
        RecompMenuServices()->RumbleActivationFeedback();
}
void RecompMenuSetVSync(bool active) {
    if(g_config.vsync!=active) { g_config.vsync=active;settings_pending|=RECOMP_SETTINGS_VSYNC; }
}
void RecompMenuSetOutputMethod(int method) {
    if(method<kOutputMethod_SDL || method>kOutputMethod_OpenGL)return;
    if(g_config.output_method!=method) {
        g_config.output_method=(uint8_t)method;
        // Renderer is a legacy launcher override, not a second menu preference.
        renderer_reset |= g_config.renderer[0]!=0;
        g_config.renderer[0]=0; /* Preference only: the current renderer stays alive. */
    }
}
void RecompMenuSetDisplayAspect(int aspect) {
    if(aspect!=kSnesDisplayAspect_Crt4x3 && aspect!=kSnesDisplayAspect_SquarePixels8x7)return;
    if(g_config.display_aspect!=aspect) { g_config.display_aspect=(uint8_t)aspect; }
}
static bool alt_keys[SDL_SCANCODE_COUNT];
static SDL_Keymod alt_key_mods[SDL_SCANCODE_COUNT];

bool RecompMenuAltOwnsInput(void) {
#if defined(__linux__) && defined(RECOMP_MENU_FLATPAK)
    const bool *keys=SDL_GetKeyboardState(NULL);
    if(!keys[SDL_SCANCODE_LALT] && !keys[SDL_SCANCODE_RALT]) {
        alt_held=0;SDL_SetModState(SDL_GetModState()&~SDL_KMOD_ALT);
    }
    for(int i=1;i<SDL_SCANCODE_COUNT;++i)if(alt_keys[i] && !keys[i])alt_keys[i]=false;
    if(keys[SDL_SCANCODE_LALT] || keys[SDL_SCANCODE_RALT])return true;
#endif
    if(alt_held || (SDL_GetModState()&SDL_KMOD_ALT)) return true;
    for(int i=1;i<SDL_SCANCODE_COUNT;++i) if(alt_keys[i]) return true;
    return false;
}
static bool exists(const char *path) {
    SDL_PathInfo info;return SDL_GetPathInfo(path,&info) && info.type==SDL_PATHTYPE_FILE;
}
typedef struct BootPicker { SDL_AtomicInt done;char path[4096]; } BootPicker;
static void SDLCALL boot_selected(void *userdata,const char *const *files,int filter) {
    (void)filter;BootPicker *picker=userdata;
    if(files && files[0])SDL_strlcpy(picker->path,files[0],sizeof(picker->path));
    SDL_SetAtomicInt(&picker->done,1);
}
int RecompMenuResolveRom(int argc,char **argv,char *out,size_t capacity,const unsigned char *sha) {
    (void)sha;if(!capacity)return 0;out[0]=0;
    if(argc>1 && argv && argv[1] && argv[1][0])SDL_strlcpy(out,argv[1],capacity);
    else snesrecomp_rom_cache_read(out,capacity);
    for(;;) {
        size_t size=0;void *data=out[0] && exists(out)?SDL_LoadFile(out,&size):NULL;
        bool valid=data && RecompMenuServices()->RomMatches(data,size);SDL_free(data);
        if(valid) { snesrecomp_rom_cache_write(out);return 1; }
        // Resolve before SnesInit/RtlRunFrame: no guest or audio exists yet.
        if(!(SDL_WasInit(SDL_INIT_VIDEO)&SDL_INIT_VIDEO)) {
            SDL_SetMainReady();if(!SDL_Init(SDL_INIT_VIDEO))return 0;
        }
        BootPicker picker={0};
        static const SDL_DialogFileFilter filter={"SNES ROM (*.sfc; *.smc)","sfc;smc"};
        if(boot_observer)boot_observer(true);
        SDL_ShowOpenFileDialog(boot_selected,&picker,NULL,&filter,1,RecompMenuRomDirectory(),false);
        while(!SDL_GetAtomicInt(&picker.done)) { SDL_PumpEvents();SDL_Delay(10); }
        if(boot_observer)boot_observer(false);
        if(!picker.path[0])return 0; /* explicit cancellation */
        SDL_strlcpy(out,picker.path,capacity);
    }
}
void RtlSaveSlotPath(int slot,char *out,size_t cap) {
    if(slot<0 || slot>=10) { if(cap)out[0]=0;return; }
    snprintf(out,cap,RecompMenuServices()->save_slot_pattern,save_dir[0]?save_dir:"saves/",slot+1);
}
void RtlSaveLoad(int command,int slot) {
    if(slot<0 || slot>=10)return;
    char path[4096];RtlSaveSlotPath(slot,path,sizeof(path));
    if(command==kSaveLoad_Save) {
        SDL_CreateDirectory(save_dir[0]?save_dir:"saves");
        RtlSaveSnapshot(path);
    } else if(exists(path)) RtlLoadSnapshot(path); /* empty slot is entirely silent */
}
static void SDLCALL selected(void *userdata,const char *const *files,int filter) {
    (void)filter;
    SDL_Event e={0};e.type=command_event;e.user.code=(int)(intptr_t)userdata;
    if(files && files[0])e.user.data1=SDL_strdup(files[0]);
    /* Only the main loop may touch the core, cache, or window. */
    if(!SDL_PushEvent(&e)) { SDL_free(e.user.data1);SDL_SetAtomicInt(&dialog_pending,0); }
}
enum { DIALOG_ROM=1,DIALOG_LOAD=2,DIALOG_SAVE=3,DIALOG_IPS=4 };
static void dialog(int mode) {
    if(!SDL_CompareAndSwapAtomicInt(&dialog_pending,0,1))return;
    // Mandatory pause precedes the OS call, independently of window focus.
    RecompMenuUiSuppressInput();
    RecompMenuServices()->PauseForDialog();
    static const SDL_DialogFileFilter rom_filter={"SNES ROM (*.sfc; *.smc)","sfc;smc"};
    static const SDL_DialogFileFilter save_filter={"Savestate (*.sav)","sav"};
    static const SDL_DialogFileFilter ips_filter={"IPS translation (*.ips)","ips"};
    if(mode==DIALOG_IPS) {
        SDL_CreateDirectory(RecompMenuModsDirectory());
        SDL_ShowOpenFileDialog(selected,(void *)(intptr_t)mode,window,&ips_filter,1,RecompMenuModsDirectory(),false);
        return;
    }
    if(mode!=DIALOG_ROM)SDL_CreateDirectory(save_dir);
    if(mode==DIALOG_SAVE)
        SDL_ShowSaveFileDialog(selected,(void *)(intptr_t)mode,window,&save_filter,1,save_dir);
    else SDL_ShowOpenFileDialog(selected,(void *)(intptr_t)mode,window,
        mode==DIALOG_ROM?&rom_filter:&save_filter,1,mode==DIALOG_ROM?rom_dir:save_dir,false);
}
void RecompMenuCommand(int command) {
    if(command>=RECOMP_MENU_SAVE && command<RECOMP_MENU_SAVE+10)RtlSaveLoad(kSaveLoad_Save,command-RECOMP_MENU_SAVE);
    else if(command>=RECOMP_MENU_LOAD && command<RECOMP_MENU_LOAD+10)RtlSaveLoad(kSaveLoad_Load,command-RECOMP_MENU_LOAD);
    else if(command==RECOMP_MENU_ROM)dialog(DIALOG_ROM);
    else if(command==RECOMP_MENU_FILE)dialog(DIALOG_LOAD);
    else if(command==RECOMP_MENU_SAVE_FILE)dialog(DIALOG_SAVE);
    else if(command==RECOMP_MENU_IPS)dialog(DIALOG_IPS);
    else if(command==RECOMP_MENU_RESET) {
        if(!state_restart && !RecompMenuModsSave())return;
        if(translation_rom[0])SDL_strlcpy(relaunch,translation_rom,sizeof(relaunch));
        else snesrecomp_rom_cache_read(relaunch,sizeof(relaunch));
        if(!relaunch[0])return;
        relaunch_fullscreen=window?(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN)!=0:g_config.fullscreen!=0;
        cold_boot=true;RecompMenuCommand(RECOMP_MENU_QUIT);
    }
    else if(command==RECOMP_MENU_QUIT) { SDL_Event e={0};e.type=SDL_EVENT_QUIT;SDL_PushEvent(&e); }
}
void RecompMenuQueueCommand(int command) {
    SDL_Event e={0};e.type=command_event;e.user.code=command;SDL_PushEvent(&e);
}
bool RecompMenuEvent(SDL_Event *e) {
    RecompMenuServices()->RumbleEvent(e);
    RecompDevicesEvent(e);
    RecompDeviceInfo ready;
    if(RecompDevicesReady(e,&ready) && ready.id) {
        SDL_Event added={0};added.type=ready.pad?SDL_EVENT_GAMEPAD_ADDED:SDL_EVENT_JOYSTICK_ADDED;
        added.gdevice.which=ready.id;RecompMenuServices()->RumbleEvent(&added);
    }
    RecompHotkeysReleaseRewindEvent(e);
    bool cursor_hidden=!SDL_CursorVisible();
    RecompMenuCursorUpdate(e); /* Observe mouse activity before the overlay consumes it. */
    if(window && (e->type==SDL_EVENT_MOUSE_BUTTON_DOWN || e->type==SDL_EVENT_MOUSE_BUTTON_UP)) {
        Uint32 bit=SDL_BUTTON_MASK(e->button.button);
        if((cursor_hidden && RecompMenuVisible()) || (wake_mouse_buttons&bit)) {
            if(e->type==SDL_EVENT_MOUSE_BUTTON_DOWN)wake_mouse_buttons|=bit;else wake_mouse_buttons&=~bit;
            RecompMenuUiMouseWakeOnly();return true;
        }
    }
    if(!window)return false;
    if(RecompHotkeysCaptureEvent(e)) {RecompMenuUiSuppressInput();return true;}
    if(RecompControlsEvent(e)) {
        RecompMenuUiSuppressInput();return true;
    }
    if(RecompMenuUiClearBinding(e)) {
        RecompMenuUiSuppressInput();return true;
    }
    if(e->type==SDL_EVENT_WINDOW_FOCUS_GAINED && (!e->window.windowID || e->window.windowID==SDL_GetWindowID(window)))focus_lost=false;
    if(e->type==SDL_EVENT_WINDOW_FOCUS_LOST && (!e->window.windowID || e->window.windowID==SDL_GetWindowID(window)))focus_lost=true;
    if(RecompMenuDialogActive() &&
       (e->type==SDL_EVENT_KEY_DOWN || e->type==SDL_EVENT_KEY_UP ||
        e->type==SDL_EVENT_TEXT_INPUT ||
        (e->type>=SDL_EVENT_MOUSE_MOTION && e->type<=SDL_EVENT_MOUSE_WHEEL) ||
        e->type==SDL_EVENT_GAMEPAD_BUTTON_DOWN || e->type==SDL_EVENT_GAMEPAD_BUTTON_UP ||
        e->type==SDL_EVENT_GAMEPAD_AXIS_MOTION)) {
        RecompMenuUiSuppressInput();return true;
    }
    if(e->type==SDL_EVENT_KEY_DOWN || e->type==SDL_EVENT_KEY_UP) {
        if(e->key.key==SDLK_ESCAPE) {
            if(e->type==SDL_EVENT_KEY_DOWN && !e->key.repeat)RecompMenuUiToggle();
            return true; /* Immutable safety shortcut; never serialized/editable. */
        }
    }
    bool system_only=false;
#ifdef _WIN32
    if(e->type==SDL_EVENT_WINDOW_ENTER_FULLSCREEN || e->type==SDL_EVENT_WINDOW_LEAVE_FULLSCREEN)
        RecompMenuUiLockMouseFocus();
#endif
#if defined(__linux__) && defined(RECOMP_MENU_FLATPAK)
    if(e->type==SDL_EVENT_WINDOW_ENTER_FULLSCREEN || e->type==SDL_EVENT_WINDOW_LEAVE_FULLSCREEN) {
        RecompMenuFullscreen();alt_held=0;RecompMenuUiSuppressInput();
    }
    if((e->type==SDL_EVENT_KEY_DOWN || e->type==SDL_EVENT_KEY_UP) &&
       FindCmdForSdlKey(e->key.key,e->key.mod)==kKeys_Fullscreen) {
        if(e->type==SDL_EVENT_KEY_DOWN && !e->key.repeat)
            RecompMenuSetFullscreen(!RecompMenuFullscreen());
        return true;
    }
#endif
    if(e->type==SDL_EVENT_WINDOW_FOCUS_LOST) {
        alt_held=0;memset(alt_keys,0,sizeof(alt_keys));
        RecompMenuUiSuppressInput();
    }
    if(e->type==SDL_EVENT_KEY_DOWN || e->type==SDL_EVENT_KEY_UP) {
        SDL_Keymod side=e->key.key==SDLK_LALT?SDL_KMOD_LALT:e->key.key==SDLK_RALT?SDL_KMOD_RALT:0;
        if(side) {
            if(e->type==SDL_EVENT_KEY_DOWN) alt_held|=side;else alt_held&=~side;
            RecompMenuUiSuppressInput();return true;
        }
        SDL_Scancode scan=e->key.scancode;
        if(scan==SDL_SCANCODE_UNKNOWN) scan=SDL_GetScancodeFromKey(e->key.key,NULL);
        bool valid=scan>0 && scan<SDL_SCANCODE_COUNT;
        bool quarantined=valid && alt_keys[scan];
        SDL_Keymod mods=e->key.mod|SDL_GetModState()|alt_held;
        system_only=(mods&SDL_KMOD_ALT)!=0 || quarantined;
        if(system_only) {
            RecompMenuUiSuppressInput();
            if(e->type==SDL_EVENT_KEY_DOWN && valid) {
                if(!quarantined) alt_key_mods[scan]=mods;
                alt_keys[scan]=true;
            } else if(e->type==SDL_EVENT_KEY_UP && valid) {
                if(quarantined) mods=alt_key_mods[scan];
                alt_keys[scan]=false;
            }
            // A held chord remains exclusive even if Alt is released first.
            // Preserve its original modifiers for key-up (e.g. Turbo release).
            if(quarantined && e->type==SDL_EVENT_KEY_DOWN) mods=alt_key_mods[scan];
            e->key.mod=mods;
            if(FindCmdForSdlKey(e->key.key,mods)<kKeys_Load) return true;
        }
    } else if(RecompMenuAltOwnsInput() &&
              (e->type==SDL_EVENT_TEXT_INPUT ||
               (e->type>=SDL_EVENT_MOUSE_MOTION && e->type<=SDL_EVENT_MOUSE_WHEEL) ||
               e->type==SDL_EVENT_GAMEPAD_BUTTON_DOWN || e->type==SDL_EVENT_GAMEPAD_BUTTON_UP ||
               e->type==SDL_EVENT_GAMEPAD_AXIS_MOTION)) {
        RecompMenuUiSuppressInput();return true;
    }
    if(e->type==command_event) {
        if(e->user.code==DIALOG_ROM || e->user.code==DIALOG_LOAD || e->user.code==DIALOG_SAVE || e->user.code==DIALOG_IPS) {
            char *path=e->user.data1;
            if(path) {
                const char *ext=strrchr(path,'.');
                if(e->user.code==DIALOG_IPS) {
                    // Native worker selects the MD5 registry route or bounded
                    // generic IPS fallback; completion refreshes the catalog.
                    if(RecompMenuServices()->TranslationStart(path,translation_rom,RecompMenuModsDirectory()))SDL_strlcpy(ips_path,path,sizeof(ips_path));
                } else if(e->user.code==DIALOG_SAVE) {
                    // Native filters do not guarantee an extension on every OS.
                    const char *slash=strrchr(path,'/'),*backslash=strrchr(path,'\\');
                    if((slash && (!ext || slash>ext)) || (backslash && (!ext || backslash>ext)))ext=NULL;
                    if(ext && !SDL_strcasecmp(ext,".sav"))RtlSaveSnapshot(path);
                    else if(!ext) {
                        size_t length=strlen(path);
                        char *sav=SDL_malloc(length+5);
                        if(sav) { memcpy(sav,path,length);memcpy(sav+length,".sav",5);RtlSaveSnapshot(sav);SDL_free(sav); }
                    }
                } else if(e->user.code==DIALOG_LOAD && ext && !SDL_strcasecmp(ext,".sav")) {
                    if(exists(path))RtlLoadSnapshot(path);
                } else if(e->user.code==DIALOG_ROM && ext &&
                         (!SDL_strcasecmp(ext,".sfc") || !SDL_strcasecmp(ext,".smc")) && exists(path)) {
                    size_t size=0;void *rom=SDL_LoadFile(path,&size);
                    bool valid=rom && RecompMenuServices()->RomMatches(rom,size);SDL_free(rom);
                    if(valid) {
                        SDL_strlcpy(relaunch,path,sizeof(relaunch));
                        snesrecomp_rom_cache_write(path);
                        RecompMenuCommand(RECOMP_MENU_QUIT);
                    } else SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Select ROM",
                        RecompMenuServices()->unsupported_rom_message,window);
                }
                SDL_free(path);
            }
            SDL_SetAtomicInt(&dialog_pending,0);
        } else RecompMenuCommand(e->user.code);
        return true;
    }
    if(RecompHotkeysFixedEvent(e,!RecompMenuVisible() ||
       ((e->type==SDL_EVENT_KEY_DOWN || e->type==SDL_EVENT_KEY_UP) && e->key.key==SDLK_F12)))return true;
#ifdef _WIN32
    // Lock before the runner dispatches keyboard fullscreen, including Alt+Enter.
    if(e->type==SDL_EVENT_KEY_DOWN && !e->key.repeat &&
       FindCmdForSdlKey(e->key.key,e->key.mod)==kKeys_Fullscreen)
        RecompMenuUiLockMouseFocus();
#endif
    if(RecompHotkeysHostEvent(e))return true;
    if(e->type==SDL_EVENT_KEY_DOWN || e->type==SDL_EVENT_KEY_UP) {
        int command=FindCmdForSdlKey(e->key.key,e->key.mod);
        if(command==kKeys_DisplayPerf) {
            if(e->type==SDL_EVENT_KEY_DOWN && !e->key.repeat)snes_osd_toggle_fps();
            return true;
        }
        bool save=command>=kKeys_Save && command<=kKeys_Save_Last;
        bool load=command>=kKeys_Load && command<=kKeys_Load_Last;
        if(save || load) {
            if(e->type==SDL_EVENT_KEY_DOWN && !e->key.repeat)
                RtlSaveLoad(save?kSaveLoad_Save:kSaveLoad_Load,command-(save?kKeys_Save:kKeys_Load));
            return true;
        }
    }
    return system_only?false:RecompMenuUiEvent(e);
}
void RecompMenuSync(void) {
    if(RecompMenuServices()->TranslationPoll())RecompMenuModsRefresh();
    RecompMenuUiSync();
}
void RecompMenuConfigure(void) {
#ifdef _WIN32
    SDL_SetHint(SDL_HINT_WINDOWS_ENABLE_MENU_MNEMONICS,"0");
#endif
}
void RecompMenuInit(SDL_Window *w,const char *rom) {
    clear_entry();active_output=g_config.output_method;output_initialized=true;state_restart=shader_restart=false;
    RecompMenuCursorReset();wake_mouse_buttons=0;
    cold_boot=false;relaunch_fullscreen=false;relaunch[0]=0;
    RecompMenuConfigure();
    window=w;focus_lost=false;settings_pending=0;command_event=SDL_RegisterEvents(1);
    SDL_SetAtomicInt(&dialog_pending,0);
    alt_held=0;memset(alt_keys,0,sizeof(alt_keys));
    const char *base=SDL_GetBasePath();if(!base)base="./";
    RecompMenuSaveDirectory();
    RecompMenuRomDirectory();
    SDL_strlcpy(translation_rom,rom?rom:"",sizeof(translation_rom));
    if(rom && *rom)snesrecomp_rom_cache_write(rom);
    RecompControlsInit();RecompHotkeysInit();RecompMenuModsInit();RecompMenuShadersRefresh();
    active_shaders=g_config.enable_shaders;SDL_strlcpy(active_shader,g_config.selected_shader,sizeof(active_shader));
    SDL_SetWindowHitTest(w,NULL,NULL);
    RecompMenuUiInit(w);
    RecompDevicesInit(RecompMenuUiGamepadsChanged);
}
void RecompMenuShutdown(void) {
    if(relaunch[0] && !cold_boot)relaunch_fullscreen=g_config.fullscreen!=0;
    RecompMenuServices()->RumbleShutdown();
    RecompMenuServices()->TranslationShutdown();
    RecompControlsCancel();RecompHotkeysCancel();
    // Persist just the menu-owned preferences with the atomic section writer.
    // Do not materialize unrelated optional renderer/filter settings.
    char runahead[16],size[32],scale[16],volume[16],frequency[16],depth[16],interval[16];
    snprintf(depth,sizeof(depth),"%d",g_config.rewind_depth);
    snprintf(interval,sizeof(interval),"%d",g_config.rewind_interval);
    snprintf(runahead,sizeof(runahead),"%d",g_config.run_ahead);
    if(g_config.window_width>0 && g_config.window_height>0)
        snprintf(size,sizeof(size),"%dx%d",g_config.window_width,g_config.window_height);
    else SDL_strlcpy(size,"Auto",sizeof(size));
    snprintf(scale,sizeof(scale),"%u",g_config.window_scale);
    snprintf(volume,sizeof(volume),"%d",g_config.volume);
    snprintf(frequency,sizeof(frequency),"%u",g_config.audio_freq);
    RecompIniValue values[]={
        {"General","PauseWhenInactive",g_config.pause_when_inactive?"1":"0"},
        {"General","Autosave",g_config.autosave?"1":"0"},
        {"General","ControllerVibration",g_config.controller_vibration?"1":"0"},
        {"General","RunAhead",runahead},
        {"General","RewindDepth",depth},
        {"General","RewindInterval",interval},
        {"Graphics","DisplayAspect",g_config.display_aspect==1?"8:7":"4:3"},
        {"Graphics","Fullscreen",g_config.fullscreen?"1":"0"},
        {"Graphics","VSync",g_config.vsync?"1":"0"},
        {"Graphics","OutputMethod",g_config.output_method==kOutputMethod_OpenGL?"OpenGL":g_config.output_method==kOutputMethod_SDLSoftware?"SDL-Software":"SDL"},
        {"Graphics","NoSpriteLimits",g_config.no_sprite_limits?"1":"0"},
        {"Graphics","Widescreen",g_config.ignore_aspect_ratio?"1":"0"},
        {"Graphics","WindowSize",size},
        {"Graphics","InternalResolution",scale},
        {"Graphics","IntegerScaling",g_config.integer_scaling?"1":"0"},
        {"Graphics","ScreenshotResolution",g_config.screenshot_internal?"InternalResolution":"WindowSize"},
        {"Graphics","EnableShaders",g_config.enable_shaders?"1":"0"},
        {"Graphics","SelectedShader",g_config.selected_shader},
        {"Sound","EnableAudio",g_config.enable_audio?"1":"0"},
        {"Sound","Volume",volume},
        {"Sound","AudioFreq",frequency},
        {"Graphics","Renderer",g_config.renderer[0]?g_config.renderer:"auto"},
    };
    if(!persist_state(values,sizeof(values)/sizeof(values[0])-(renderer_reset?0:1)))fprintf(stderr,"[config] Unable to persist menu settings\n");
    RecompMenuUiShutdown();RecompDevicesShutdown();free_shaders();clear_entry();window=NULL;output_initialized=false;
}
int RecompMenuRelaunch(void) {
    if(!relaunch[0])return 0;
#ifdef _WIN32
    wchar_t exe[32768],rom[4096],config[4096],command[42000];
    if(!GetModuleFileNameW(NULL,exe,32768) || !MultiByteToWideChar(CP_UTF8,0,relaunch,-1,rom,4096))return 1;
    if(!MultiByteToWideChar(CP_UTF8,0,RecompMenuServices()->ConfigPath(),-1,config,4096))return 1;
    wchar_t fullscreen_arg[128],restart_arg[128];
    if(!MultiByteToWideChar(CP_UTF8,0,RecompMenuServices()->fullscreen_arg,-1,fullscreen_arg,128) ||
       !MultiByteToWideChar(CP_UTF8,0,RecompMenuServices()->restart_arg,-1,restart_arg,128))return 1;
    swprintf(command,42000,L"\"%ls\" --config \"%ls\" %ls %ls --no-launcher \"%ls\"",exe,config,
        relaunch_fullscreen?fullscreen_arg:L"",state_restart?restart_arg:L"",rom);
    STARTUPINFOW si={0};si.cb=sizeof(si);si.dwFlags=STARTF_USESHOWWINDOW;si.wShowWindow=SW_SHOWNORMAL;
    PROCESS_INFORMATION pi={0};
    if(!CreateProcessW(exe,command,NULL,NULL,FALSE,CREATE_NO_WINDOW|CREATE_SUSPENDED,NULL,NULL,&si,&pi))return 1;
    AllowSetForegroundWindow(pi.dwProcessId);
    if(ResumeThread(pi.hThread)==(DWORD)-1) {
        TerminateProcess(pi.hProcess,1);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return 1;
    }
    CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return 0;
#else
    char exe[4096];SDL_snprintf(exe,sizeof(exe),"%s%s",SDL_GetBasePath()?SDL_GetBasePath():"./",RecompMenuServices()->executable_name);
    const char *args[9]={exe,"--config",RecompMenuServices()->ConfigPath()};int count=3;
    if(relaunch_fullscreen)args[count++]=RecompMenuServices()->fullscreen_arg;
    if(state_restart)args[count++]=RecompMenuServices()->restart_arg;
    args[count++]="--no-launcher";args[count++]=relaunch;args[count]=NULL;
    SDL_PropertiesID props=SDL_CreateProperties();SDL_SetPointerProperty(props,SDL_PROP_PROCESS_CREATE_ARGS_POINTER,(void *)args);
    SDL_SetBooleanProperty(props,SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN,true);
    SDL_Process *child=SDL_CreateProcessWithProperties(props);SDL_DestroyProperties(props);
    if(!child)return 1;SDL_DestroyProcess(child);return 0;
#endif
}

void RecompMenuShortcut(bool load,int slot,char *out,size_t capacity) {
    char key[32];SDL_snprintf(key,sizeof(key),"%sSlot%d",load?"Load":"Save",slot+1);
    for(int i=0;i<RECOMP_HOTKEY_COUNT;i++)if(!strcmp(RecompHotkeysKey(i),key)){RecompHotkeysLabel(i,out,capacity);break;}
    if(!*out)ConfigFormatShortcut((load?kKeys_Load:kKeys_Save)+slot,out,capacity);
}

bool RecompMenuFullscreenKey(const SDL_Event *event) {
    return FindCmdForSdlKey(event->key.key,event->key.mod)==kKeys_Fullscreen;
}
