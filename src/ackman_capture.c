#include "ackman_capture.h"
#include "desktop/config.h"
#include <zlib.h>
#include <time.h>
#include <stdint.h>
typedef struct Shot {struct Shot *next;SDL_Surface *pixels;char path[256];} Shot;
static bool pending,stopping;
static SDL_Mutex *mutex;
static SDL_Condition *condition;
static SDL_Thread *thread;
static Shot *head,*tail;
static int outstanding;
static void be32(unsigned char *out,uint32_t n) {out[0]=n>>24;out[1]=n>>16;out[2]=n>>8;out[3]=n;}
static bool chunk(SDL_IOStream *file,const char *kind,const void *data,size_t size) {
    unsigned char length[4],crc[4];be32(length,(uint32_t)size);
    uLong sum=crc32(0,(const Bytef *)kind,4);if(size)sum=crc32(sum,data,(uInt)size);be32(crc,(uint32_t)sum);
    return SDL_WriteIO(file,length,4)==4 && SDL_WriteIO(file,kind,4)==4 &&
        (!size || SDL_WriteIO(file,data,size)==size) && SDL_WriteIO(file,crc,4)==4;
}
static bool write_png(const Shot *shot) {
    SDL_Surface *rgba=SDL_ConvertSurface(shot->pixels,SDL_PIXELFORMAT_RGBA32);if(!rgba)return false;
    size_t stride=(size_t)rgba->w*4,size=(stride+1)*rgba->h;
    if(size>512u*1024u*1024u){SDL_DestroySurface(rgba);return false;}
    unsigned char *filtered=SDL_malloc(size);if(!filtered){SDL_DestroySurface(rgba);return false;}
    for(int y=0;y<rgba->h;y++) {
        unsigned char *row=(unsigned char *)rgba->pixels+(size_t)y*rgba->pitch,*out=filtered+(stride+1)*y;
        for(int x=0;x<rgba->w;x++)row[x*4+3]=0xff;
        *out++=1; /* PNG Sub filter: fast and stable; zlib handles repeated rows. */
        for(size_t x=0;x<stride;x++)out[x]=(unsigned char)(row[x]-(x>=4?row[x-4]:0));
    }
    uLongf packed_size=compressBound((uLong)size);unsigned char *packed=SDL_malloc(packed_size);
    bool ok=packed && compress2(packed,&packed_size,filtered,(uLong)size,6)==Z_OK;
    SDL_free(filtered);
    char temporary[272];SDL_snprintf(temporary,sizeof(temporary),"%s.tmp",shot->path);
    SDL_IOStream *file=ok?SDL_IOFromFile(temporary,"wb"):NULL;
    if(file) {
        unsigned char ihdr[13]={0};be32(ihdr,rgba->w);be32(ihdr+4,rgba->h);ihdr[8]=8;ihdr[9]=6;
        ok=SDL_WriteIO(file,"\x89PNG\r\n\x1a\n",8)==8 && chunk(file,"IHDR",ihdr,13) &&
            chunk(file,"IDAT",packed,packed_size) && chunk(file,"IEND",NULL,0);
        if(!SDL_CloseIO(file))ok=false;
        if(ok)ok=SDL_RenamePath(temporary,shot->path);if(!ok)SDL_RemovePath(temporary);
    } else ok=false;
    SDL_free(packed);SDL_DestroySurface(rgba);return ok;
}
static int SDLCALL encode(void *unused) {
    (void)unused;
    for(;;) {
        SDL_LockMutex(mutex);while(!head && !stopping)SDL_WaitCondition(condition,mutex);
        if(!head && stopping){SDL_UnlockMutex(mutex);break;}
        Shot *shot=head;head=shot->next;if(!head)tail=NULL;SDL_UnlockMutex(mutex);
        if(write_png(shot))SDL_Log("Screenshot saved: %s",shot->path);else SDL_Log("Screenshot encoding failed: %s",shot->path);
        SDL_DestroySurface(shot->pixels);SDL_free(shot);
        SDL_LockMutex(mutex);outstanding--;SDL_UnlockMutex(mutex);
    }
    return 0;
}
void AckmanCaptureInit(void) {
    if(thread)return;
    mutex=SDL_CreateMutex();condition=SDL_CreateCondition();stopping=false;
    if(mutex && condition)thread=SDL_CreateThread(encode,"PNG encoder",NULL);
    if(!thread){SDL_DestroyMutex(mutex);SDL_DestroyCondition(condition);mutex=NULL;condition=NULL;}
}
void AckmanCaptureShutdown(void) {
    pending=false;if(!thread)return;
    SDL_LockMutex(mutex);stopping=true;SDL_SignalCondition(condition);SDL_UnlockMutex(mutex);
    SDL_WaitThread(thread,NULL);thread=NULL;SDL_DestroyMutex(mutex);SDL_DestroyCondition(condition);
    mutex=NULL;condition=NULL;stopping=false;outstanding=0;
}
void AckmanCaptureRequest(void) {pending=true;}
void AckmanCapturePresent(SDL_Renderer *renderer,const void *native,int width,int height) {
    if(!pending || !renderer)return;pending=false;
    AckmanCaptureInit();if(!thread){SDL_Log("Screenshot worker unavailable");return;}
    SDL_LockMutex(mutex);bool full=outstanding>=3;SDL_UnlockMutex(mutex);
    if(full){SDL_Log("Screenshot queue busy");return;}
    SDL_Surface *pixels=NULL;
    if(g_config.screenshot_internal) {
        if(!native || width<1 || height<1)return;
        pixels=SDL_CreateSurface(width,height,SDL_PIXELFORMAT_ARGB8888);
        if(pixels)for(int y=0;y<height;y++)SDL_memcpy((unsigned char *)pixels->pixels+(size_t)y*pixels->pitch,
            (const unsigned char *)native+(size_t)y*width*4,(size_t)width*4);
    } else pixels=SDL_RenderReadPixels(renderer,NULL);
    if(!pixels){SDL_Log("Screenshot: %s",SDL_GetError());return;}
    Shot *shot=SDL_calloc(1,sizeof(*shot));if(!shot){SDL_DestroySurface(pixels);return;}
    shot->pixels=pixels;SDL_CreateDirectory("screenshots");
    time_t now=time(NULL);struct tm stamp={0};struct tm *local=localtime(&now);if(local)stamp=*local;
    SDL_snprintf(shot->path,sizeof(shot->path),"screenshots/screenshot_%04d%02d%02d_%02d%02d%02d_%llu.png",
        stamp.tm_year+1900,stamp.tm_mon+1,stamp.tm_mday,stamp.tm_hour,stamp.tm_min,stamp.tm_sec,(unsigned long long)SDL_GetTicksNS());
    SDL_LockMutex(mutex);if(tail)tail->next=shot;else head=shot;tail=shot;outstanding++;
    SDL_SignalCondition(condition);SDL_UnlockMutex(mutex);
}
