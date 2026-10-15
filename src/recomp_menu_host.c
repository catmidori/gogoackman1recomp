#include "recomp_menu_host.h"
static RecompMenuHost services;
bool RecompMenuBindHost(const RecompMenuHost *host) {
 if(!host || host->version!=RECOMP_MENU_HOST_VERSION || host->size!=sizeof(*host))return false;
 if(!host->executable_name)return false;
 if(!host->emergency_snapshot)return false;
 if(!host->save_slot_pattern)return false;
 if(!host->window_id)return false;
 if(!host->default_config)return false;
 if(!host->translation_instruction)return false;
 if(!host->unsupported_rom_message)return false;
 if(!host->boot_status_arg)return false;
 if(!host->restore_config_arg)return false;
 if(!host->resume_arg)return false;
 if(!host->restart_arg)return false;
 if(!host->fullscreen_arg)return false;
 if(!host->boot_trace_env)return false;
 if(!host->boot_timeout_env)return false;
 if(!host->font_fallback_env)return false;
 if(!host->font_no_files_env)return false;
 if(!host->ConfigCommitRuntime)return false;
 if(!host->ConfigPath)return false;
 if(!host->ConfigSelect)return false;
 if(!host->ConfigUpdate)return false;
 if(!host->ConfigWriteActiveMods)return false;
 if(!host->IniRead)return false;
 if(!host->ModFilenameValid)return false;
 if(!host->PauseForDialog)return false;
 if(!host->RomMatches)return false;
 if(!host->RumbleEvent)return false;
 if(!host->RumbleSetEnabled)return false;
 if(!host->RumbleShutdown)return false;
 if(!host->TranslationBusy)return false;
 if(!host->TranslationError)return false;
 if(!host->TranslationPoll)return false;
 if(!host->TranslationShutdown)return false;
 if(!host->TranslationStart)return false;
 if(!host->VideoSetWidescreen)return false;
 if(!host->HostHotkey)return false;
 services=*host;return true;
}
const RecompMenuHost *RecompMenuServices(void){return &services;}
