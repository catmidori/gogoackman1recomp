#include "recomp_menu_core.h"
#include "recomp_menu_host.h"
/* A separate process observes startup: a crashed or hung GL driver cannot
 * disable this watchdog. Recovery candidates are copied, never edited in place. */
#include "recomp_menu_boot.h"
#include <SDL3/SDL.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define change_directory _chdir
#else
#include <unistd.h>
#define change_directory chdir
#endif
static char status_path[4096],restore_path[4096],live_path[4096];
static bool stable,resume,shader_armed;
static unsigned frames;
static Uint64 started;
static void trace(const char *event,int candidate,int code) {
    const char *path=SDL_getenv(RecompMenuServices()->boot_trace_env);if(!path||!*path)return;
    FILE *f=fopen(path,"ab");if(f){fprintf(f,"%s candidate=%d code=%d\n",event,candidate,code);fclose(f);}
}
static void status(const char *text) {
    if(!status_path[0])return;
    SDL_IOStream *f=SDL_IOFromFile(status_path,"wb");
    if(f){SDL_WriteIO(f,text,strlen(text));SDL_CloseIO(f);}
}
static bool copy_atomic(const char *source,const char *target) {
    char tmp[4096];SDL_snprintf(tmp,sizeof(tmp),"%s.boot-tmp",target);
    if(!SDL_CopyFile(source,tmp))return false;
    if(SDL_RenamePath(tmp,target))return true;
    SDL_RemovePath(tmp);return false;
}
static bool history_name(const char *s) {
    if(strlen(s)!=26 || strncmp(s,"config_",7) || s[15]!='_' || strcmp(s+22,".ini"))return false;
    for(int i=7;i<22;i++)if(i!=15 && (s[i]<'0'||s[i]>'9'))return false;
    return true;
}
static int newest_first(const void *a,const void *b){return strcmp(*(char *const *)b,*(char *const *)a);}
static char **history(int *n) {
    int count=0;char **all=SDL_GlobDirectory("fallback","config_*.ini",0,&count);
    char **files=SDL_calloc(count+1,sizeof(*files));*n=0;
    if(!files){SDL_free(all);return NULL;}
    for(int i=0;i<count;i++)if(history_name(all[i])) {
        char path[4096];SDL_PathInfo info;SDL_snprintf(path,sizeof(path),"fallback/%s",all[i]);
        if(SDL_GetPathInfo(path,&info) && info.type==SDL_PATHTYPE_FILE)files[(*n)++]=SDL_strdup(all[i]);
    }
    SDL_free(all);qsort(files,*n,sizeof(*files),newest_first);return files;
}
static void free_history(char **files,int n){for(int i=0;i<n;i++)SDL_free(files[i]);SDL_free(files);}
static bool equal_files(const char *a,const char *b) {
    size_t na=0,nb=0;void *pa=SDL_LoadFile(a,&na),*pb=SDL_LoadFile(b,&nb);
    bool same=pa&&pb&&na==nb&&!memcmp(pa,pb,na);SDL_free(pa);SDL_free(pb);return same;
}
static void backup(const char *config) {
    int n=0;char **files=history(&n);char path[4096];
    if(n){SDL_snprintf(path,sizeof(path),"fallback/%s",files[0]);if(equal_files(config,path)){free_history(files,n);return;}}
    time_t now=time(NULL);struct tm stamp=*localtime(&now);
    /* Exact second precision names: wait only in this supervisor if a distinct
     * successful configuration shares the previous snapshot's second. */
    for(int tries=0;tries<3;tries++) {
        strftime(path,sizeof(path),"fallback/config_%Y%m%d_%H%M%S.ini",&stamp);
        SDL_PathInfo info;if(!SDL_GetPathInfo(path,&info))break;
        SDL_Delay(1000);now=time(NULL);stamp=*localtime(&now);
    }
    if(copy_atomic(config,path))for(int i=4;i<n;i++) {
        char old[4096];SDL_snprintf(old,sizeof(old),"fallback/%s",files[i]);SDL_RemovePath(old);
    }
    free_history(files,n);
}
bool RecompMenuBootResume(void){return resume;}
void RecompMenuBootResumed(bool success){trace(success?"resume-ok":"resume-unavailable",-1,0);}
bool RecompMenuBootStable(void){return stable;}
void RecompMenuBootApplying(bool applying){if(applying)shader_armed=true;if(stable)status(applying?"applying":"shader-ready");}
void RecompMenuBootShaderFailure(void){if(status_path[0]&&!stable){status("fail");exit(72);}}
void RecompMenuBootPresent(bool dialog) {
    if(!status_path[0] || stable)return;
    if(dialog){status("wait");started=0;frames=0;return;}
    if(!started){started=SDL_GetTicks();status("running");}
    if(++frames<120 || SDL_GetTicks()-started<3000)return;
    if(restore_path[0]) {
        if(!copy_atomic(RecompMenuServices()->ConfigPath(),restore_path)){status("fail");exit(73);}
        RecompMenuServices()->ConfigSelect(restore_path);
    }
    stable=true;status(shader_armed?"shader-ready":"stable");
}
int RecompMenuBootEnter(int *argc,char **argv) {
#ifdef RECOMP_MENU_FLATPAK
    /* Both supervisor and child keep relative reports/config/backups in the
     * persistent sandbox root. SDL_GetBasePath points at read-only /app/bin. */
    const char *storage=RecompMenuWritableDirectory();
    if(!RecompMenuPathsPrepare() || !storage || change_directory(storage))return 1;
#endif
    int retained=1;bool child=false;
    for(int i=1;i<*argc;i++) {
        if(!strcmp(argv[i],RecompMenuServices()->boot_status_arg)&&i+1<*argc){SDL_strlcpy(status_path,argv[++i],sizeof(status_path));child=true;}
        else if(!strcmp(argv[i],RecompMenuServices()->restore_config_arg)&&i+1<*argc)SDL_strlcpy(restore_path,argv[++i],sizeof(restore_path));
        else if(!strcmp(argv[i],RecompMenuServices()->resume_arg) || !strcmp(argv[i],RecompMenuServices()->restart_arg))resume=true;
        else argv[retained++]=argv[i];
    }
    *argc=retained;argv[retained]=NULL;if(child)return -1;
    /* SDL process APIs do not require video init. Keep a separate IPC marker
     * per launch, so unrelated emulator instances cannot signal this watcher. */
    const char *base=RecompMenuWritableDirectory();if(base)change_directory(base);
    SDL_strlcpy(live_path,"config.ini",sizeof(live_path));
    for(int i=1;i<*argc-1;i++)if(!strcmp(argv[i],"--config"))SDL_strlcpy(live_path,argv[i+1],sizeof(live_path));
    SDL_CreateDirectory("fallback");
    if(!RecompMenuServices()->ConfigSelect(live_path))return 1;
    int all_count=0;char **files=history(&all_count);int n=all_count>5?5:all_count;
    char ipc[4096],attempt[4096];SDL_snprintf(ipc,sizeof(ipc),"fallback/.boot-%llu",(unsigned long long)SDL_GetTicksNS());
    SDL_snprintf(attempt,sizeof(attempt),"%s.attempt.ini",ipc);
    int result=1;bool retried_runtime_profile=false;
    for(int candidate=-1;candidate<=n;candidate++) {
        trace("attempt",candidate,0);
        const char *selected=live_path;
        if(candidate>=0) {
            selected=attempt;
            if(candidate<n) {
                char source[4096];SDL_snprintf(source,sizeof(source),"fallback/%s",files[candidate]);
                if(!SDL_CopyFile(source,attempt))continue;
            } else {
                SDL_IOStream *f=SDL_IOFromFile(attempt,"wb");if(!f)break;
                bool ok=SDL_WriteIO(f,RecompMenuServices()->default_config,strlen(RecompMenuServices()->default_config))==strlen(RecompMenuServices()->default_config);
                ok=SDL_CloseIO(f)&&ok;if(!ok)break;
                /* A factory profile must still know the user's cartridge. */
                RecompMenuServices()->ConfigSelect(attempt);
            }
        }
        const char **args=SDL_calloc(*argc+10,sizeof(*args));if(!args)break;
        int k=0;args[k++]=argv[0];
        args[k++]="--config";args[k++]=selected;args[k++]=RecompMenuServices()->boot_status_arg;args[k++]=ipc;
        if(candidate>=0){args[k++]=RecompMenuServices()->restore_config_arg;args[k++]=live_path;}
        if(resume)args[k++]=RecompMenuServices()->resume_arg;
        /* Host options must precede the positional ROM argument. */
        for(int i=1;i<*argc;i++) {
            if((!strcmp(argv[i],"--config") || (resume && !strcmp(argv[i],"--script"))) && i+1<*argc){i++;continue;}
            args[k++]=argv[i];
        }
        SDL_RemovePath(ipc);
        SDL_PropertiesID props=SDL_CreateProperties();SDL_SetPointerProperty(props,SDL_PROP_PROCESS_CREATE_ARGS_POINTER,(void *)args);
        SDL_Process *process=SDL_CreateProcessWithProperties(props);SDL_DestroyProperties(props);SDL_free(args);
        if(!process)break;
#ifdef _WIN32
        AllowSetForegroundWindow((DWORD)SDL_GetNumberProperty(SDL_GetProcessProperties(process),SDL_PROP_PROCESS_PID_NUMBER,0));
#endif
        Uint64 deadline=SDL_GetTicks();bool ready=false;int exitcode=0;
        const char *timeout=SDL_getenv(RecompMenuServices()->boot_timeout_env);Uint64 limit=timeout?SDL_strtoull(timeout,NULL,10):30000;
        if(limit<1000)limit=1000;
        for(;;) {
            char *state=SDL_LoadFile(ipc,NULL);
            if(state && !strcmp(state,"wait"))deadline=SDL_GetTicks();
            if(state && (!strcmp(state,"stable") || !strcmp(state,"shader-ready"))){ready=true;SDL_free(state);break;}
            bool failed=state && !strcmp(state,"fail");SDL_free(state);
            if(SDL_WaitProcess(process,false,&exitcode))break;
            if(failed || SDL_GetTicks()-deadline>limit){SDL_KillProcess(process,true);SDL_WaitProcess(process,true,&exitcode);exitcode=72;break;}
            SDL_Delay(100);
        }
        if(ready) {
            trace("stable",candidate,0);
            if(candidate>=0)for(int i=0;i<candidate && i<n;i++){char bad[4096];SDL_snprintf(bad,sizeof(bad),"fallback/%s",files[i]);SDL_RemovePath(bad);}
            backup(live_path);
            Uint64 applying_since=0;bool application_failed=false;
            while(!SDL_WaitProcess(process,false,&exitcode)) {
                char *state=SDL_LoadFile(ipc,NULL);
                bool applying=state && !strcmp(state,"applying");SDL_free(state);
                if(applying && !applying_since)applying_since=SDL_GetTicks();
                if(!applying)applying_since=0;
                if(applying_since && SDL_GetTicks()-applying_since>limit) {
                    SDL_KillProcess(process,true);SDL_WaitProcess(process,true,&exitcode);
                    application_failed=true;break;
                }
                SDL_Delay(100);
            }
            result=exitcode;SDL_DestroyProcess(process);
            char *last_state=SDL_LoadFile(ipc,NULL);
            bool driver_crash=exitcode && last_state && (!strcmp(last_state,"applying") || !strcmp(last_state,"shader-ready"));SDL_free(last_state);
            if(application_failed || driver_crash) {
                resume=true;
                /* Refresh: the last successful boot created a new checkpoint. */
                free_history(files,all_count);files=history(&all_count);n=all_count>5?5:all_count;
                /* The for increment must lead to -1 (the new preference),
                 * not 0 (the newest backup). Only its failed boot rolls back. */
                candidate=retried_runtime_profile?candidate:-2;retried_runtime_profile=true;continue;
            }
            break;
        }
        SDL_DestroyProcess(process);
        trace("failed",candidate,exitcode);
        if(!exitcode){result=0;break;} /* user cancellation is not a crash */
        resume=true; /* Every emergency child restores the durable checkpoint. */
    }
    SDL_RemovePath(ipc);SDL_RemovePath(attempt);free_history(files,all_count);return result;
}
