#include "ackman_log.h"
#include "desktop/sdl_compat.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifdef _WIN32
#include <windows.h>
#endif
typedef struct LogStream {
    FILE *file;
    char *data;
    size_t size,capacity;
    int closing,closed,owned;
    struct LogStream *next;
} LogStream;
static SDL_Mutex *lock;
static SDL_Condition *ready;
static SDL_Thread *worker;
static LogStream *streams;
static int stopping,requested,failed;
static unsigned frames;
static LogStream *find(FILE *f) {
    for(LogStream *s=streams;s;s=s->next) if(s->file==f && !s->closed) return s;
    return NULL;
}
static void add(FILE *f,int owned) {
    LogStream *s=calloc(1,sizeof(*s));
    if(!s) { fputs("Ackman logger: out of memory\n",stderr); abort(); }
    s->file=f; s->owned=owned; s->next=streams; streams=s;
}
static int SDLCALL writer(void *unused) {
    (void)unused;
    SDL_LockMutex(lock);
    for(;;) {
        while(!requested && !stopping) SDL_WaitCondition(ready,lock);
        requested=0;
        for(LogStream *s=streams;s;s=s->next) {
            if(s->closed) continue;
            char *data=s->data; size_t size=s->size;
            s->data=NULL; s->size=s->capacity=0;
            int close_now=s->closing;
            SDL_UnlockMutex(lock);
            int error=0;
            if(size && fwrite(data,1,size,s->file)!=size) error=1;
            if(fflush(s->file)) error=1;
            if(close_now && fclose(s->file)) error=1;
            free(data);
            SDL_LockMutex(lock);
            if(error) failed=1;
            if(close_now) { s->closed=1; SDL_BroadcastCondition(ready); }
        }
        if(stopping) break;
    }
    SDL_UnlockMutex(lock);
    return 0;
}
int AckmanLogInit(void) {
    lock=SDL_CreateMutex(); ready=SDL_CreateCondition();
    if(!lock || !ready) return 0;
    add(stdout,0); add(stderr,0);
    worker=SDL_CreateThread(writer,"ackman-log",NULL);
    if(!worker) return 0;
    atexit(AckmanLogShutdown);
    return 1;
}
void AckmanLogFrame(void) {
    if(!worker || ++frames%180) return;
    SDL_LockMutex(lock); requested=1; SDL_SignalCondition(ready); SDL_UnlockMutex(lock);
}
void AckmanLogShutdown(void) {
    if(!worker) return;
    SDL_LockMutex(lock); stopping=1; SDL_SignalCondition(ready); SDL_UnlockMutex(lock);
    SDL_WaitThread(worker,NULL); worker=NULL;
    while(streams) { LogStream *s=streams; streams=s->next; free(s->data); free(s); }
    SDL_DestroyCondition(ready); SDL_DestroyMutex(lock); ready=NULL; lock=NULL;
    if(failed) fputs("Ackman logger: disk write failed; some diagnostics could not be saved\n",stderr);
}
static int diagnostic(const char *path) {
    const char *ext=strrchr(path,'.');
    const char *stats=getenv("SNESRECOMP_AUDIO_STATS");
    return (ext && (!strcmp(ext,".csv") || !strcmp(ext,".log"))) ||
           (stats && !strcmp(path,stats));
}
FILE *AckmanLogOpen(const char *path,const char *mode) {
#ifdef _WIN32
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,NULL,0);
    if(!n)return NULL;
    wchar_t *wide=malloc((size_t)n*sizeof(*wide)),wm[16];
    if(!wide)return NULL;
    MultiByteToWideChar(CP_UTF8,0,path,-1,wide,n);
    if(!MultiByteToWideChar(CP_UTF8,0,mode,-1,wm,16)) { free(wide);return NULL; }
    FILE *f=_wfopen(wide,wm);free(wide);
#else
    FILE *f=fopen(path,mode);
#endif
    if(f && worker && diagnostic(path) && (strchr(mode,'w') || strchr(mode,'a'))) {
        SDL_LockMutex(lock); add(f,1); SDL_UnlockMutex(lock);
    }
    return f;
}
size_t AckmanLogWrite(const void *data,size_t size,size_t count,FILE *f) {
    if(!size || !count) return 0;
    if(count>SIZE_MAX/size) return 0;
    if(!worker) return fwrite(data,size,count,f);
    size_t bytes=size*count;
    SDL_LockMutex(lock);
    LogStream *s=find(f);
    if(!s) { SDL_UnlockMutex(lock); return fwrite(data,size,count,f); }
    if(s->closing) { SDL_UnlockMutex(lock); return 0; }
    if(bytes>SIZE_MAX-s->size) { SDL_UnlockMutex(lock); return 0; }
    if(s->size+bytes>s->capacity) {
        size_t capacity=s->size+bytes;
        if(s->capacity<=SIZE_MAX/2 && capacity<s->capacity*2) capacity=s->capacity*2;
        if(capacity<65536) capacity=65536;
        char *p=realloc(s->data,capacity);
        if(!p) { SDL_UnlockMutex(lock); abort(); } /* never silently discard records */
        s->data=p; s->capacity=capacity;
    }
    memcpy(s->data+s->size,data,bytes); s->size+=bytes;
    SDL_UnlockMutex(lock);
    return count;
}
int AckmanLogVprint(FILE *f,const char *format,va_list args) {
    char small[2048]; va_list copy; va_copy(copy,args);
    int n=vsnprintf(small,sizeof(small),format,copy); va_end(copy);
    if(n<0) return n;
    char *data=small;
    if((size_t)n>=sizeof(small)) {
        data=malloc((size_t)n+1); if(!data) return -1;
        va_copy(copy,args); vsnprintf(data,(size_t)n+1,format,copy); va_end(copy);
    }
    int ok=AckmanLogWrite(data,1,(size_t)n,f)==(size_t)n;
    if(data!=small) free(data);
    return ok?n:-1;
}
int AckmanLogPrint(FILE *f,const char *format,...) {
    va_list args; va_start(args,format); int n=AckmanLogVprint(f,format,args); va_end(args); return n;
}
int AckmanLogPrintf(const char *format,...) {
    va_list args; va_start(args,format); int n=AckmanLogVprint(stdout,format,args); va_end(args); return n;
}
int AckmanLogFputs(const char *s,FILE *f) { size_t n=strlen(s); return AckmanLogWrite(s,1,n,f)==n?0:EOF; }
int AckmanLogPuts(const char *s) { return AckmanLogPrint(stdout,"%s\n",s)<0?EOF:0; }
int AckmanLogFlush(FILE *f) {
    if(!worker) return fflush(f);
    SDL_LockMutex(lock); int tracked=!f || find(f)!=NULL; SDL_UnlockMutex(lock);
    return tracked?0:fflush(f); /* disk flush is owned by the 180-frame worker */
}
int AckmanLogClose(FILE *f) {
    if(!worker) return fclose(f);
    SDL_LockMutex(lock); LogStream *s=find(f);
    if(!s) { SDL_UnlockMutex(lock); return fclose(f); }
    s->closing=1; requested=1; SDL_SignalCondition(ready);
    while(!s->closed) SDL_WaitCondition(ready,lock);
    SDL_UnlockMutex(lock); return failed?EOF:0;
}
