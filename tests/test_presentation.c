#include "ackman_menu.h"
#include "desktop/config.h"
#include "ackman_shader.h"
#include "ackman_capture.h"
#include "gl_core_3_1.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "stb/stb_image.h"
Config g_config;
int AckmanMenuActiveOutput(void){return g_config.output_method;}
bool AckmanMenuActiveShaders(void){return g_config.enable_shaders;}
const char *AckmanMenuActiveShader(void){return g_config.selected_shader;}
const char *AckmanMenuShaderDirectory(void) {return "presentation-shaders/";}
void Die(const char *format) {(void)format;abort();}
static void fixture(void) {
    SDL_CreateDirectory("presentation-shaders");
    const char *shader=
        "#version 130\n"
        "#ifdef VERTEX\n"
        "in vec2 VertexCoord; in vec2 TexCoord; out vec2 uv; uniform mat4 MVPMatrix;\n"
        "void main(){gl_Position=MVPMatrix*vec4(VertexCoord,0.0,1.0);uv=TexCoord;}\n"
        "#endif\n"
        "#ifdef FRAGMENT\n"
        "in vec2 uv; out vec4 color; uniform sampler2D Texture;\n"
        "void main(){color=vec4(texture(Texture,uv).rgb*vec3(0.0,1.0,1.0),1.0);}\n"
        "#endif\n";
    SDL_IOStream *file=SDL_IOFromFile("presentation-shaders/test.glsl","wb");assert(file);
    assert(SDL_WriteIO(file,shader,strlen(shader))==strlen(shader));assert(SDL_CloseIO(file));
    assert(SDL_CreateDirectory("presentation-shaders/nested"));
    const char *preset="shaders = 1\nshader0 = ../test.glsl\n";
    file=SDL_IOFromFile("presentation-shaders/nested/test.glslp","wb");assert(file);
    assert(SDL_WriteIO(file,preset,strlen(preset))==strlen(preset));assert(SDL_CloseIO(file));
}
static void check_pixel(SDL_Renderer *renderer,int x,int y,int red,int green,int blue) {
    SDL_Surface *frame=SDL_RenderReadPixels(renderer,NULL);assert(frame);
    Uint8 r,g,b,a;assert(SDL_ReadSurfacePixel(frame,x,y,&r,&g,&b,&a));
    fprintf(stderr,"Pixel %d,%d: %d,%d,%d (expected %d,%d,%d)\n",x,y,r,g,b,red,green,blue);
    assert(abs((int)r-red)<=1 && abs((int)g-green)<=1 && abs((int)b-blue)<=1);
    SDL_DestroySurface(frame);
}
int main(void) {
    assert(SDL_Init(SDL_INIT_VIDEO));fixture();
    SDL_Window *window=SDL_CreateWindow("Shader/capture verification",440,240,SDL_WINDOW_HIDDEN);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,"opengl");assert(renderer);
    Uint32 pixels[20*10];for(int y=0;y<10;y++)for(int x=0;x<20;x++)pixels[y*20+x]=y<5?0x00FF8040:0x00FF4080;
    SDL_Rect viewport={20,20,400,200};
    g_config.output_method=kOutputMethod_OpenGL;g_config.enable_shaders=true;strcpy(g_config.selected_shader,"test.glsl");
    SDL_SetRenderDrawColor(renderer,0,0,0,255);assert(SDL_RenderClear(renderer));
    assert(AckmanShaderDraw(renderer,pixels,20,10,&viewport));
    check_pixel(renderer,100,50,0,128,64);check_pixel(renderer,100,190,0,64,128);
    /* A subsequent SDL command must still obey SDL's cached GL state. */
    SDL_SetRenderDrawColor(renderer,255,255,255,255);SDL_FRect marker={200,80,20,20};
    assert(SDL_RenderFillRect(renderer,&marker));check_pixel(renderer,205,85,255,255,255);
    AckmanCaptureRequest();AckmanCapturePresent(renderer,pixels,20,10);assert(SDL_RenderPresent(renderer));AckmanCaptureShutdown();
    int count=0;char **names=SDL_GlobDirectory("screenshots","screenshot_*.png",0,&count);assert(names && count);
    char path[512];SDL_snprintf(path,sizeof(path),"screenshots/%s",names[count-1]);
    int sw,sh,channels;unsigned char *saved=stbi_load(path,&sw,&sh,&channels,4);assert(saved && sw==440 && sh==240);
    for(int i=0;i<sw*sh;i++)assert(saved[i*4+3]==255);stbi_image_free(saved);SDL_free(names);
    g_config.screenshot_internal=true;g_config.window_scale=7;
    AckmanCaptureRequest();AckmanCapturePresent(renderer,pixels,20,10);AckmanCaptureShutdown();
    names=SDL_GlobDirectory("screenshots","screenshot_*.png",0,&count);assert(names && count);
    SDL_snprintf(path,sizeof(path),"screenshots/%s",names[count-1]);
    saved=stbi_load(path,&sw,&sh,&channels,4);assert(saved && sw==20 && sh==10);
    assert(saved[0]==255 && saved[1]==128 && saved[2]==64);
    for(int i=0;i<sw*sh;i++)assert(saved[i*4+3]==255);stbi_image_free(saved);SDL_free(names);
    g_config.screenshot_internal=false;
    g_config.enable_shaders=false;assert(!AckmanShaderDraw(renderer,pixels,20,10,&viewport));
    assert(!strcmp(g_config.selected_shader,"test.glsl"));g_config.enable_shaders=true;
    g_config.output_method=kOutputMethod_SDL;assert(!AckmanShaderDraw(renderer,pixels,20,10,&viewport));
    g_config.output_method=kOutputMethod_OpenGL;assert(AckmanShaderDraw(renderer,pixels,20,10,&viewport));
    strcpy(g_config.selected_shader,"nested/test.glslp");
    assert(AckmanShaderDraw(renderer,pixels,20,10,&viewport));check_pixel(renderer,100,50,0,128,64);
    AckmanShaderShutdown();SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);
    /* The same capture code works without any GL context at all. */
    window=SDL_CreateWindow("Software capture",320,224,SDL_WINDOW_HIDDEN);assert(window);
    renderer=SDL_CreateRenderer(window,"software");assert(renderer);
    SDL_SetRenderDrawColor(renderer,40,80,120,255);SDL_RenderClear(renderer);
    AckmanCaptureRequest();AckmanCapturePresent(renderer,NULL,0,0);check_pixel(renderer,10,10,40,80,120);AckmanCaptureShutdown();
    SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();puts("PASS SDL/OpenGL shaders, opaque compressed PNG and native/window capture dimensions");
}
