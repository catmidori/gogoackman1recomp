#pragma once
#include "recomp_menu_types.h"
#ifdef __cplusplus
extern "C" {
#endif
#define RECOMP_MENU_HOST_VERSION 2
/* Immutable strings/callbacks must outlive the menu and watchdog. Bind before
 * any engine call. The engine copies this table; no guest state is owned here. */
typedef struct RecompMenuHost {
 unsigned version;size_t size;
 const char *executable_name,*emergency_snapshot,*save_slot_pattern,*window_id;
 const char *default_config,*translation_instruction,*unsupported_rom_message;
 const char *boot_status_arg,*restore_config_arg,*resume_arg,*restart_arg,*fullscreen_arg;
 const char *boot_trace_env,*boot_timeout_env,*font_fallback_env,*font_no_files_env;
 void (*HostHotkey)(int command,bool pressed);
 bool (*ConfigCommitRuntime)(const RecompIniValue *values,size_t count, const char *const *mods,size_t mod_count);
 const char * (*ConfigPath)(void);
 bool (*ConfigSelect)(const char *path);
 bool (*ConfigUpdate)(const RecompIniValue *values, size_t count);
 bool (*ConfigWriteActiveMods)(const char *const *names,size_t count);
 bool (*IniRead)(const char *path, RecompIniHandler handler, void *context);
 bool (*ModFilenameValid)(const char *name);
 void (*PauseForDialog)(void);
 bool (*RomMatches)(const uint8_t *data, size_t size);
 void (*RumbleEvent)(const SDL_Event *event);
 void (*RumbleSetEnabled)(bool enabled);
 void (*RumbleActivationFeedback)(void); /* Optional; UI OFF -> ON only. */
 void (*RumbleShutdown)(void);
 bool (*TranslationBusy)(void);
 const char * (*TranslationError)(void);
 bool (*TranslationPoll)(void);
 void (*TranslationShutdown)(void);
 bool (*TranslationStart)(const char *ips,const char *rom,const char *directory);
 void (*VideoSetWidescreen)(int active);
} RecompMenuHost;
/* Reject incomplete hosts; a failed bind leaves the previous host intact.
 * Bind once before boot, never while the menu/device workers are running. */
bool RecompMenuBindHost(const RecompMenuHost *host);
const RecompMenuHost *RecompMenuServices(void);
#ifdef __cplusplus
}
#endif
