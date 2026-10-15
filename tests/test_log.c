#include "ackman_log.h"
#include "desktop/sdl_compat.h"
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) abort(); } while(0)
static FILE *logfile;
static int SDLCALL produce(void *arg) {
    int id=*(int*)arg;
    for(int i=0;i<20000;i++) CHECK(AckmanLogPrint(logfile,"%d:%d\n",id,i)>0);
    return 0;
}
int main(void) {
    CHECK(AckmanLogInit());
    logfile=AckmanLogOpen("logger-test.csv","w"); CHECK(logfile);
    CHECK(AckmanLogFputs("header\n",logfile)==0);
    CHECK(AckmanLogFlush(logfile)==0);
    FILE *read=fopen("logger-test.csv","rb"); CHECK(read);
    CHECK(fgetc(read)==EOF); fclose(read); /* fflush must not hit disk */
    int ids[]={0,1};
    SDL_Thread *a=SDL_CreateThread(produce,"producer-a",&ids[0]);
    SDL_Thread *b=SDL_CreateThread(produce,"producer-b",&ids[1]); CHECK(a && b);
    for(int i=0;i<720;i++) AckmanLogFrame();
    SDL_WaitThread(a,NULL); SDL_WaitThread(b,NULL);
    AckmanLogShutdown(); /* final partial batch must also survive */
    CHECK(fclose(logfile)==0);
    read=fopen("logger-test.csv","rb"); CHECK(read);
    char line[128]; CHECK(fgets(line,sizeof(line),read));
    CHECK(!strcmp(line,"header\r\n") || !strcmp(line,"header\n"));
    int counts[2]={0,0},id,n;
    while(fgets(line,sizeof(line),read)) {
        CHECK(sscanf(line,"%d:%d",&id,&n)==2 && id>=0 && id<2);
        CHECK(n==counts[id]++);
    }
    CHECK(counts[0]==20000 && counts[1]==20000);
    fclose(read); remove("logger-test.csv");
    return 0;
}
