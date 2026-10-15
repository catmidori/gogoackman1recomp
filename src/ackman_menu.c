/* Go Go Ackman adapter. No window, widget, navigation or watchdog implementation. */
#ifndef RECOMP_MENU_TRANSLATION_ONLY
#include "ackman_menu.h"
#include "ackman_config.h"
#include "ackman_controls.h"
#include "ackman_hotkeys.h"
#include "ackman_devices.h"
#include "ackman_rumble.h"
#include "ackman_translation.h"
#include "ackman_debug.h"
#include "ackman_video.h"
#include "rom_guard.h"

/* Local bindings between the standard menu vocabulary and this INI schema. */
typedef struct {const char *section,*semantic,*local;} AckmanMenuConfigBinding;
static const AckmanMenuConfigBinding config_bindings[]={
 {"General","PauseWhenInactive","PauseWhenInactive"},
 {"General","Autosave","Autosave"},
 {"General","ControllerVibration","ControllerVibration"},
 {"General","RunAhead","RunAhead"},
 {"General","RewindDepth","RewindDepth"},
 {"General","RewindInterval","RewindInterval"},
 {"Graphics","DisplayAspect","DisplayAspect"},
 {"Graphics","Fullscreen","Fullscreen"},
 {"Graphics","VSync","VSync"},
 {"Graphics","OutputMethod","OutputMethod"},
 {"Graphics","NoSpriteLimits","NoSpriteLimits"},
 {"Graphics","Widescreen","Widescreen"},
 {"Graphics","WindowSize","WindowSize"},
 {"Graphics","InternalResolution","InternalResolution"},
 {"Graphics","IntegerScaling","IntegerScaling"},
 {"Graphics","ScreenshotResolution","ScreenshotResolution"},
 {"Graphics","EnableShaders","EnableShaders"},
 {"Graphics","SelectedShader","SelectedShader"},
 {"Graphics","Renderer","Renderer"},
 {"Sound","EnableAudio","EnableAudio"},
 {"Sound","Volume","Volume"},
 {"Sound","AudioFreq","AudioFreq"},
};
static RecompIniValue bind_config_value(RecompIniValue value) {
 for(size_t i=0;i<sizeof(config_bindings)/sizeof(*config_bindings);i++)
  if(!SDL_strcmp(value.section,config_bindings[i].section) && !SDL_strcmp(value.key,config_bindings[i].semantic)) {
   value.key=config_bindings[i].local;break;
  }
 return value;
}
static bool update_config(const RecompIniValue *values,size_t count) {
 RecompIniValue *bound=SDL_malloc((count?count:1)*sizeof(*bound));if(!bound)return false;
 for(size_t i=0;i<count;i++)bound[i]=bind_config_value(values[i]);
 bool ok=AckmanConfigUpdate(bound,count);SDL_free(bound);return ok;
}
static bool commit_config(const RecompIniValue *values,size_t count,const char *const *mods,size_t mod_count) {
 RecompIniValue *bound=SDL_malloc((count?count:1)*sizeof(*bound));if(!bound)return false;
 for(size_t i=0;i<count;i++)bound[i]=bind_config_value(values[i]);
 bool ok=AckmanConfigCommitRuntime(bound,count,mods,mod_count);SDL_free(bound);return ok;
}
#ifdef _WIN32
static const char executable_name[]="AckmanRecomp.exe";
#else
static const char executable_name[]="AckmanRecomp";
#endif
bool AckmanMenuRegister(void) {
 const RecompMenuHost host={
 .version=RECOMP_MENU_HOST_VERSION,.size=sizeof(RecompMenuHost),
 .executable_name=executable_name,
 .emergency_snapshot="GGA1_autosave.sav",
 .save_slot_pattern="%sGGA1_save_%02d.sav",
 .window_id="Ackman HUD",
 .translation_instruction="Click the button below to convert a translation .ips into a native mod (.snesmod). Tested files are listed in 'readme.md'. Other patches will use a generic fallback tool, conversion success is not guaranteed.",
 .boot_status_arg="--ackman-boot-status",
 .restore_config_arg="--ackman-restore-config",
 .resume_arg="--ackman-resume-autosave",
 .restart_arg="--ackman-graphics-restart",
 .fullscreen_arg="--ackman-fullscreen",
 .boot_trace_env="ACKMAN_BOOT_TRACE",
 .boot_timeout_env="ACKMAN_BOOT_TIMEOUT_MS",
 .font_fallback_env="ACKMAN_TEST_FONT_FALLBACK",
 .font_no_files_env="ACKMAN_TEST_FONT_NO_FILES",
 .unsupported_rom_message="This build requires the supported Go Go Ackman (Japan) ROM.",
 .default_config=AckmanDefaultConfig,.HostHotkey=AckmanHostHotkey,
 .ConfigCommitRuntime=commit_config,
 .ConfigPath=AckmanConfigPath,
 .ConfigSelect=AckmanConfigSelect,
 .ConfigUpdate=update_config,
 .ConfigWriteActiveMods=AckmanConfigWriteActiveMods,
 .IniRead=AckmanIniRead,
 .ModFilenameValid=AckmanModFilenameValid,
 .PauseForDialog=AckmanPauseForDialog,
 .RomMatches=AckmanRomMatches,
 .RumbleEvent=AckmanRumbleEvent,
 .RumbleActivationFeedback=AckmanRumbleActivationFeedback,
 .RumbleSetEnabled=AckmanRumbleSetEnabled,
 .RumbleShutdown=AckmanRumbleShutdown,
 .TranslationBusy=AckmanTranslationBusy,
 .TranslationError=AckmanTranslationError,
 .TranslationPoll=AckmanTranslationPoll,
 .TranslationShutdown=AckmanTranslationShutdown,
 .TranslationStart=AckmanTranslationStart,
 .VideoSetWidescreen=AckmanVideoSetWidescreen,
 };
 return RecompMenuBindHost(&host);
}
#endif

/* Game-specific registered IPS conversion and manifest data. */
#include "ackman_translation.h"
#include <SDL3/SDL.h>
#include <stdint.h>
#include <string.h>
#include "ackman_translation_data.h"
#include "ackman_translation_ptbr.h"
#include "ackman_ips.h"

/* RFC 1321 MD5. The digest is an identity gate for this registered artifact. */
void AckmanTranslationMd5(const unsigned char *data,size_t size,char out[33]) {
    static const uint32_t k[64]={
        0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
        0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
        0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
        0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
        0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
        0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
        0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
        0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391};
    static const unsigned shifts[16]={7,12,17,22,5,9,14,20,4,11,16,23,6,10,15,21};
    uint32_t h[4]={0x67452301,0xefcdab89,0x98badcfe,0x10325476};
    uint64_t bits=(uint64_t)size*8;
    size_t blocks=size/64+1+(size%64>=56);
    for(size_t block=0;block<blocks;block++) {
        uint32_t words[16]={0};
        for(unsigned j=0;j<64;j++) {
            size_t pos=block*64+j;unsigned char byte=0;
            if(pos<size)byte=data[pos];else if(pos==size)byte=0x80;
            if(block==blocks-1 && j>=56)byte=(unsigned char)(bits>>(8*(j-56)));
            words[j/4]|=(uint32_t)byte<<(8*(j%4));
        }
        uint32_t a=h[0],b=h[1],c=h[2],d=h[3];
        for(unsigned i=0;i<64;i++) {
            uint32_t f;unsigned index;
            if(i<16){f=(b&c)|(~b&d);index=i;}
            else if(i<32){f=(d&b)|(~d&c);index=(5*i+1)%16;}
            else if(i<48){f=b^c^d;index=(3*i+5)%16;}
            else {f=c^(b|~d);index=(7*i)%16;}
            uint32_t value=a+f+k[i]+words[index];unsigned shift=shifts[(i/16)*4+i%4];
            a=d;d=c;c=b;b+=(value<<shift)|(value>>(32-shift));
        }
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;
    }
    static const char hex[]="0123456789abcdef";
    for(unsigned i=0;i<16;i++){unsigned byte=(h[i/4]>>(8*(i%4)))&255;out[2*i]=hex[byte>>4];out[2*i+1]=hex[byte&15];}
    out[32]=0;
}
static SDL_Thread *worker;
static SDL_AtomicInt finished;
static char error[160],output[4096],temporary[4096],stock_rom[4096];
static unsigned char *selected_ips;
static size_t selected_size;
static char selected_hash[33],worker_error[160];
enum { TRANSLATION_EN,TRANSLATION_PTBR,TRANSLATION_GENERIC };
static int selected_kind;
bool AckmanTranslationBusy(void) { return worker!=NULL; }
const char *AckmanTranslationError(void) { return error; }

/* ZIP32, stored entries only. English uses the audited fixed table. PT-BR
 * composes that base with the registered overlay; unknown IPS uses native C. */
static void zip16(unsigned char **p,uint16_t value) {
    *(*p)++=(unsigned char)value;*(*p)++=(unsigned char)(value>>8);
}
static void zip32(unsigned char **p,uint32_t value) {
    zip16(p,(uint16_t)value);zip16(p,(uint16_t)(value>>16));
}
static uint32_t zip_crc(const char *data,size_t size) {
    uint32_t crc=~0u;
    for(size_t i=0;i<size;i++) {
        crc^=(unsigned char)data[i];
        for(int bit=0;bit<8;bit++)crc=(crc>>1)^(0xedb88320u & (0u-(crc&1u)));
    }
    return ~crc;
}
static int SDLCALL publish_translation(void *unused) {
    (void)unused;
    char *dynamic_table=NULL,*dynamic_manifest=NULL,*dynamic_audit=NULL;
    unsigned char *patched=NULL;
    size_t rom_size=0;unsigned char *rom=SDL_LoadFile(stock_rom,&rom_size);
    size_t skip=rom_size%32768==512?512:0;char digest[33];
    bool valid=rom && rom_size-skip==1310720;
    if(valid){AckmanTranslationMd5(rom+skip,rom_size-skip,digest);valid=!strcmp(digest,ACKMAN_STOCK_ROM_MD5);}
    if(!valid){SDL_strlcpy(worker_error,"Conversion requires the verified stock Japanese ROM.",sizeof(worker_error));goto fail;}
    const char *names[]={"manifest.toml","translation.toml","conversion.json"};
    const char *entries[]={translation_manifest,translation_table,translation_audit};
    if(selected_kind!=TRANSLATION_EN) {
        patched=SDL_malloc(rom_size-skip);
        if(!patched)goto fail;
        memcpy(patched,rom+skip,rom_size-skip);
        if(selected_kind==TRANSLATION_PTBR && !AckmanTranslationApplyEnglish(patched,rom_size-skip,translation_table)) {
            SDL_strlcpy(worker_error,"Registered English base could not be applied.",sizeof(worker_error));goto fail;
        }
        if(!AckmanIpsApply(selected_ips,selected_size,patched,rom_size-skip)) {
            SDL_strlcpy(worker_error,"IPS conversion failed: malformed records, resizing or out-of-ROM data.",sizeof(worker_error));goto fail;
        }
        dynamic_table=AckmanTranslationBuildTable(rom+skip,patched,rom_size-skip);
        if(!dynamic_table)goto fail;
        if(selected_kind==TRANSLATION_PTBR)entries[0]=ptbr_manifest;
        else {
            SDL_asprintf(&dynamic_manifest,
                "format_version = 1\nid = \"ackman.translation.generic\"\nversion = \"1.0-%s\"\n"
                "name = \"Go Go Ackman custom IPS %s\"\nauthor = \"Unknown\"\n"
                "description = \"Unverified generic IPS conversion. Source MD5: %s. Gameplay compatibility is not guaranteed.\"\n"
                "resolver = \"declarative\"\nsave_compatibility = \"shared\"\n\n[[target]]\n"
                "game_id = \"go-go-ackman-jp\"\nrom_sha256 = \"e8a33b6c8e15cf0aa60e9694471a51926fb97ad895d4714384c6abff28a33a1f\"\n\n"
                "[[feature]]\nid = \"generic\"\nname = \"Custom IPS\"\ndefault_enabled = false\n\n"
                "[[plugin]]\nfeature = \"generic\"\nid = \"ackman.translation.generic\"\n",selected_hash,selected_hash,selected_hash);
            if(!dynamic_manifest)goto fail;
            entries[0]=dynamic_manifest;
        }
        SDL_asprintf(&dynamic_audit,"{\"ips_md5\":\"%s\",\"ips_size\":%zu,\"english_base_md5\":\"%s\",\"converter\":\"native-c\",\"registered\":%s}",
            selected_hash,selected_size,selected_kind==TRANSLATION_PTBR?ACKMAN_TRANSLATION_MD5:"",selected_kind==TRANSLATION_PTBR?"true":"false");
        if(!dynamic_audit)goto fail;
        entries[1]=dynamic_table;entries[2]=dynamic_audit;
    }
    SDL_free(rom);rom=NULL;SDL_free(patched);patched=NULL;
    uint32_t sizes[3],crc[3],offsets[3];size_t capacity=22;
    for(int i=0;i<3;i++) {
        sizes[i]=(uint32_t)strlen(entries[i]);crc[i]=zip_crc(entries[i],sizes[i]);
        capacity+=30+46+2*strlen(names[i])+sizes[i];
    }
    unsigned char *archive=SDL_malloc(capacity),*cursor=archive;
    if(!archive)goto fail;
    for(int i=0;i<3;i++) {
        offsets[i]=(uint32_t)(cursor-archive);
        zip32(&cursor,0x04034b50);zip16(&cursor,20);zip16(&cursor,0);zip16(&cursor,0);
        zip16(&cursor,0);zip16(&cursor,33);zip32(&cursor,crc[i]);
        zip32(&cursor,sizes[i]);zip32(&cursor,sizes[i]);zip16(&cursor,(uint16_t)strlen(names[i]));zip16(&cursor,0);
        memcpy(cursor,names[i],strlen(names[i]));cursor+=strlen(names[i]);
        memcpy(cursor,entries[i],sizes[i]);cursor+=sizes[i];
    }
    uint32_t central=(uint32_t)(cursor-archive);
    for(int i=0;i<3;i++) {
        zip32(&cursor,0x02014b50);zip16(&cursor,20);zip16(&cursor,20);zip16(&cursor,0);zip16(&cursor,0);
        zip16(&cursor,0);zip16(&cursor,33);zip32(&cursor,crc[i]);zip32(&cursor,sizes[i]);zip32(&cursor,sizes[i]);
        zip16(&cursor,(uint16_t)strlen(names[i]));zip16(&cursor,0);zip16(&cursor,0);zip16(&cursor,0);zip16(&cursor,0);
        zip32(&cursor,0100644u<<16);zip32(&cursor,offsets[i]);
        memcpy(cursor,names[i],strlen(names[i]));cursor+=strlen(names[i]);
    }
    uint32_t central_size=(uint32_t)(cursor-archive)-central;
    zip32(&cursor,0x06054b50);zip16(&cursor,0);zip16(&cursor,0);zip16(&cursor,3);zip16(&cursor,3);
    zip32(&cursor,central_size);zip32(&cursor,central);zip16(&cursor,0);
    bool saved=SDL_SaveFile(temporary,archive,(size_t)(cursor-archive));SDL_free(archive);
    if(saved && SDL_RenamePath(temporary,output)) {
        SDL_free(dynamic_table);SDL_free(dynamic_manifest);SDL_free(dynamic_audit);
        SDL_SetAtomicInt(&finished,1);return 0;
    }
    SDL_RemovePath(temporary);
fail:
    SDL_free(rom);SDL_free(patched);SDL_free(dynamic_table);SDL_free(dynamic_manifest);SDL_free(dynamic_audit);
    SDL_SetAtomicInt(&finished,-1);return 1;
}
bool AckmanTranslationStart(const char *ips,const char *rom,const char *directory) {
    if(worker)return false;
    error[0]=0;
    SDL_PathInfo info;size_t size=0;char hash[33];
    if(!ips || !SDL_GetPathInfo(ips,&info) || info.type!=SDL_PATHTYPE_FILE || info.size<8 || info.size>16*1024*1024)goto invalid;
    unsigned char *data=SDL_LoadFile(ips,&size);
    if(!data)goto invalid;
    if(size<8 || size>16*1024*1024 || memcmp(data,"PATCH",5)){SDL_free(data);goto invalid;}
    AckmanTranslationMd5(data,size,hash);
    selected_kind=!strcmp(hash,ACKMAN_TRANSLATION_MD5)?TRANSLATION_EN:
        !strcmp(hash,ACKMAN_TRANSLATION_PTBR_MD5)?TRANSLATION_PTBR:TRANSLATION_GENERIC;
    char package[128];
    if(selected_kind==TRANSLATION_EN)SDL_strlcpy(package,ACKMAN_TRANSLATION_PACKAGE,sizeof(package));
    else if(selected_kind==TRANSLATION_PTBR)SDL_strlcpy(package,ACKMAN_TRANSLATION_PTBR_PACKAGE,sizeof(package));
    else SDL_snprintf(package,sizeof(package),"GoGoAckman_Custom_%s.snesmod",hash);
    if(!rom || !directory || SDL_strlen(rom)>=sizeof(stock_rom) ||
       SDL_snprintf(output,sizeof(output),"%s%s",directory,package)>=(int)sizeof(output) ||
       SDL_snprintf(temporary,sizeof(temporary),"%s.translation-%llu.tmp",directory,(unsigned long long)SDL_GetTicksNS())>=(int)sizeof(temporary)) {
        SDL_free(data);SDL_strlcpy(error,"Translation output path is unavailable.",sizeof(error));return false;
    }
    SDL_strlcpy(stock_rom,rom,sizeof(stock_rom));SDL_CreateDirectory(directory);
    selected_ips=data;selected_size=size;SDL_strlcpy(selected_hash,hash,sizeof(selected_hash));worker_error[0]=0;
    SDL_SetAtomicInt(&finished,0);
    worker=SDL_CreateThread(publish_translation,"Ackman translation",NULL);
    if(!worker){SDL_free(selected_ips);selected_ips=NULL;SDL_strlcpy(error,"Could not start native translation conversion.",sizeof(error));}
    return worker!=NULL;
invalid:
    SDL_strlcpy(error,"Invalid IPS file (expected PATCH header, maximum 16 MiB).",sizeof(error));return false;
}
bool AckmanTranslationPoll(void) {
    int result=SDL_GetAtomicInt(&finished);
    if(!worker || !result)return false;
    SDL_WaitThread(worker,NULL);worker=NULL;
    SDL_free(selected_ips);selected_ips=NULL;
    if(result<0)SDL_strlcpy(error,worker_error[0]?worker_error:"Translation conversion failed; check the output directory.",sizeof(error));
    return result>0;
}
void AckmanTranslationShutdown(void) {
    if(worker){SDL_WaitThread(worker,NULL);worker=NULL;}
    SDL_free(selected_ips);selected_ips=NULL;
    error[0]=0;
}
