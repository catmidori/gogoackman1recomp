#include "ackman_mods.h"
#include "ackman_menu.h"
#include "ackman_config.h"
#include "mod_runtime.h"
#include "snes_text_xlate.h"
#include "sha256.h"
#include "common_rtl.h"
#include "snes/snes.h"
#include "snes/msu1.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern Snes *g_snes;
static bool english;
static size_t canonical_rom_size;
static char msu_base[1024];
void AckmanMsuSetRomPath(const char *rom) {
    if(msu_base[0]) {
#ifdef _WIN32
        if(_putenv_s("SNESRECOMP_MSU1",msu_base))return;
#else
        if(setenv("SNESRECOMP_MSU1",msu_base,1))return;
#endif
        msu1_init();
    }
    msu1_set_rom_path(rom);
}
void AckmanModsRomLoaded(size_t size) { canonical_rom_size=size%32768==512?size-512:size; }
static uint64_t generation;
static void hex_digest(const void *data,size_t size,char out[65]) {
    uint8_t hash[32];sha256_compute(data,size,hash);
    for(int i=0;i<32;i++)SDL_snprintf(out+i*2,3,"%02x",hash[i]);
}
static void reset_translation(void) { english=false;snes_text_xlate_shutdown_c(); }
static void enable_translation_package(const char *id,const char *label) {
    char root[4096],table[4128];
    if(!snes_mod_runtime_package_root_c(id,NULL,root,sizeof(root)))return;
    SDL_snprintf(table,sizeof(table),"%s/translation.toml",root);
    if(!snes_text_xlate_init_c(table,"en")) {
        fprintf(stderr,"[mods] %s translation table rejected: %s\n",label,snes_text_xlate_last_error_c());return;
    }
    english=true;generation=RtlStateGeneration();snes_text_xlate_on_frame_c();
    char hash[65];hex_digest(g_snes->cart->rom,canonical_rom_size?canonical_rom_size:g_snes->cart->romSize,hash);
    fprintf(stderr,"[mods] %s translation applied: ROM SHA256=%s\n",label,hash);
    const char *trace=getenv("ACKMAN_MODS_TRACE");
    if(trace && *trace) { FILE *f=fopen(trace,"a");if(f){fprintf(f,"%s ROM SHA256=%s\n",label,hash);fclose(f);} }
}
static void enable_translation(void) { enable_translation_package("ackman.translation.en","English"); }
static void enable_ptbr(void) { enable_translation_package("ackman.translation.ptbr","PT-BR"); }
static void enable_generic(void) { enable_translation_package("ackman.translation.generic","Generic IPS"); }
SNES_MOD_CONSTRUCTOR(register_ackman_translation) {
    snes_mod_register_reset_callback(reset_translation);
    snes_mod_register_activation_plugin("ackman.translation.en",enable_translation);
    snes_mod_register_activation_plugin("ackman.translation.ptbr",enable_ptbr);
    snes_mod_register_activation_plugin("ackman.translation.generic",enable_generic);
}
int AckmanModsInitialize(const char *unused,const char *game,const char *sha256) {
    (void)unused;
    msu_base[0]=0;
    char root[4096];SDL_snprintf(root,sizeof(root),"%spreloaded",AckmanMenuModsDirectory());
    AckmanMenuModsInit();
    if(!snes_mod_runtime_initialize_c(root,game,sha256))return 0;
    // The INI is authoritative, including an empty list: cached package state
    // must never resurrect a deleted/deactivated archive on a subsequent boot.
    snes_mod_runtime_select_package_c(NULL,NULL,0);
    for(int i=0;i<AckmanMenuModsCount();i++)if(AckmanMenuModActive(i)) {
        char path[4096],id[128]={0},version[64]={0},error[512],hash[65];size_t size=0;
        SDL_snprintf(path,sizeof(path),"%s%s",AckmanMenuModsDirectory(),AckmanMenuModName(i));
        SDL_PathInfo info;void *data=NULL;
        size_t path_size=strlen(path);
        if(path_size>4 && !SDL_strcasecmp(path+path_size-4,".msu")) {
            if(!SDL_GetPathInfo(path,&info) || info.type!=SDL_PATHTYPE_FILE || path_size>=sizeof(msu_base)) {
                AckmanMenuModSetActive(i,false);continue;
            }
            /* One physical MSU-1 device has one source. Keep all RAM selections,
             * but choose the first catalog path deterministically at cold boot. */
            if(!msu_base[0]) {
                SDL_strlcpy(msu_base,path,sizeof(msu_base));msu_base[path_size-4]=0;
                fprintf(stderr,"[mods] MSU-1 source selected: %s\n",AckmanMenuModName(i));
            }
            continue;
        }
        if(SDL_GetPathInfo(path,&info) && info.type==SDL_PATHTYPE_FILE && info.size<=256*1024*1024)
            data=SDL_LoadFile(path,&size);
        bool valid=false;
        if(data && size>0 && size<=256*1024*1024) {
            hex_digest(data,size,hash);
            valid=snes_mod_runtime_install_blob_c(data,(uint32_t)size,hash,id,sizeof(id),version,sizeof(version),error,sizeof(error)) &&
                snes_mod_runtime_select_package_c(id,version,1);
        }
        SDL_free(data);
        if(!valid) {
            if(id[0])snes_mod_runtime_select_package_c(id,version,0);
            fprintf(stderr,"[mods] Skipped unavailable or incompatible package: %s\n",AckmanMenuModName(i));
            AckmanMenuModSetActive(i,false);
        }
    }
    AckmanMenuModsSave();return 1;
}
void AckmanModsBeforeFrame(void) {
    if(!english)return;
    uint64_t current=RtlStateGeneration();
    if(current!=generation){generation=current;snes_text_xlate_set_language_c("en");}
    snes_text_xlate_on_frame_c();
}
void AckmanModsShutdown(void) { reset_translation(); }
