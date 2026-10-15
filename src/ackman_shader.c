/* GLSL effects share SDL's OpenGL context, then hand its state back unchanged.
 * SDL keeps ownership of presentation, ImGui, blur and screenshot capture. */
#include "gl_core_3_1.h"
#include "desktop/glsl_shader.h"
#include "desktop/config.h"
#include "ackman_shader.h"
#include "ackman_menu.h"
#include <string.h>
static GlslShader *effect;
static GlTextureWithSize input;
static GLuint framebuffer,vao;
static SDL_Texture *output;
static int output_w,output_h;
static char loaded[4096];
static SDL_GLContext context;
static void (*failure_handler)(void);
static bool (*probe_handler)(bool);
void AckmanShaderSetFailureHandler(void (*handler)(void)){failure_handler=handler;}
void AckmanShaderSetProbeHandler(bool (*handler)(bool)){probe_handler=handler;}
static bool failed(void){if(failure_handler)failure_handler();return false;}
typedef struct GlState {GLint program,buffer,framebuffer,vao,active,unpack;} GlState;
static void save_state(GlState *s) {
    glGetIntegerv(GL_CURRENT_PROGRAM,&s->program);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&s->buffer);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING,&s->framebuffer);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&s->vao);
    glGetIntegerv(GL_ACTIVE_TEXTURE,&s->active);glGetIntegerv(GL_UNPACK_ALIGNMENT,&s->unpack);
    glPushAttrib(GL_ALL_ATTRIB_BITS);glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
    glActiveTexture(GL_TEXTURE0);glBindVertexArray(vao);
    glDisable(GL_BLEND);glDisable(GL_SCISSOR_TEST);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);
}
static void restore_state(const GlState *s) {
    glBindFramebuffer(GL_FRAMEBUFFER,s->framebuffer);glBindVertexArray(s->vao);
    glBindBuffer(GL_ARRAY_BUFFER,s->buffer);glUseProgram(s->program);
    glPopClientAttrib();glPopAttrib();glActiveTexture(s->active);glPixelStorei(GL_UNPACK_ALIGNMENT,s->unpack);
}
static void clear_effect(void) {
    if(effect)GlslShader_Destroy(effect);effect=NULL;
    if(input.gl_texture)glDeleteTextures(1,&input.gl_texture);memset(&input,0,sizeof(input));
}
void AckmanShaderShutdown(void) {
    if(context && SDL_GL_GetCurrentContext()==context) {
        GlState state;save_state(&state);clear_effect();
        if(framebuffer)glDeleteFramebuffers(1,&framebuffer);
        restore_state(&state);if(vao)glDeleteVertexArrays(1,&vao);
    }
    if(output)SDL_DestroyTexture(output);output=NULL;
    context=NULL;framebuffer=vao=0;loaded[0]=0;output_w=output_h=0;
}
bool AckmanShaderDraw(SDL_Renderer *renderer,const void *pixels,int width,int height,const SDL_Rect *viewport) {
    if(AckmanMenuActiveOutput()!=kOutputMethod_OpenGL || !AckmanMenuActiveShaders() ||
       !AckmanMenuActiveShader()[0] || !pixels || width<1 || height<1)return false;
    if(strcmp(SDL_GetRendererName(renderer),"opengl") || !SDL_FlushRenderer(renderer))return false;
    SDL_GLContext current=SDL_GL_GetCurrentContext();if(!current)return false;
    if(!context) {
        ogl_LoadFunctions();
        if(!glCreateShader || !glGenFramebuffers || !glGenVertexArrays || !glPushAttrib)return false;
        context=current;glGenVertexArrays(1,&vao);glGenFramebuffers(1,&framebuffer);
    }
    if(context!=current)return false;
    int w=viewport->w,h=viewport->h;if(w<1 || h<1 || w>16384 || h>16384)return false;
    if(!output || output_w!=w || output_h!=h) {
        SDL_Texture *next=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_TARGET,w,h);
        if(!next)return false;
        if(output)SDL_DestroyTexture(output);output=next;output_w=w;output_h=h;
        SDL_SetTextureBlendMode(output,SDL_BLENDMODE_NONE);SDL_SetTextureScaleMode(output,SDL_SCALEMODE_NEAREST);
    }
    GLuint destination=(GLuint)SDL_GetNumberProperty(SDL_GetTextureProperties(output),SDL_PROP_TEXTURE_OPENGL_TEXTURE_NUMBER,0);
    if(!destination)return false;
    char path[4096];SDL_snprintf(path,sizeof(path),"%s%s",AckmanMenuShaderDirectory(),AckmanMenuActiveShader());
    GlState state;save_state(&state);
    bool applying=strcmp(loaded,path)!=0;
    if(applying) {
        if(probe_handler && !probe_handler(true)){restore_state(&state);return false;}
        clear_effect();SDL_strlcpy(loaded,path,sizeof(loaded));effect=GlslShader_CreateFromFile(path);
        /* Failed compilation falls back cleanly; don't recompile every frame. */
        if(!effect)SDL_Log("Shader failed: %s",path);
    }
    bool rendered=false;
    if(effect) {
        if(!input.gl_texture)glGenTextures(1,&input.gl_texture);
        glBindTexture(GL_TEXTURE_2D,input.gl_texture);glPixelStorei(GL_UNPACK_ALIGNMENT,4);
        glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,GL_BGRA,GL_UNSIGNED_BYTE,pixels);
        input.width=width;input.height=height;
        glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,destination,0);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE) {
            GlslShader_Render(effect,&input,0,0,w,h);rendered=true;
        }
    }
    restore_state(&state);
    if(applying && probe_handler)probe_handler(false);
    if(!effect)return failed();
    if(rendered) {
        SDL_FRect target={(float)viewport->x,(float)viewport->y,(float)w,(float)h};
        /* The preset's final pass flips for a window framebuffer. SDL target
         * textures already flip on presentation, so compensate once here. */
        SDL_RenderTextureRotated(renderer,output,NULL,&target,0,NULL,SDL_FLIP_VERTICAL);
    }
    return rendered;
}
