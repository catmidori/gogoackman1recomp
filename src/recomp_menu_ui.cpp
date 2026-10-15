#include "recomp_menu_controls.h"
#include "recomp_menu_hotkeys.h"
#include "recomp_menu_devices.h"
#include "recomp_menu_snes.h"
#include "recomp_menu_core.h"
#include "recomp_menu_host.h"
#include "recomp_menu_icons.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "recomp_menu_font.h"
#include "snes_osd.h"
#include "desktop/config.h"
#include "recomp_menu_widgets.h"
#include <cstdlib>
#include <cstring>
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include <cmath>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <cfloat>
#include <map>
#include <set>

static std::map<std::string,bool> shader_folders;
static const char *capture_choices[]={"Internal resolution","Window size"};
static const char *aspect_choices[]={"4:3 CRT","8:7 Square Pixels"};
#define menu_tabs RecompMenuTabs

static const int menu_tab_count=sizeof(menu_tabs)/sizeof(*menu_tabs);
static const char *output_choices[]={"SDL","OpenGL","Software"};
static const char *internal_choices[]={"1x","2x","3x (720p)","4x","5x (1080p)","6x (1440p)","7x","8x","9x (4K)","10x"};
static const char *size_choices[]={"448x224","896x448","1344x672","1792x896","2240x1120","2688x1344","3136x1568","3584x1792","4032x2016"};
static const char *volume_choices[]={"0","10","20","30","40","50","60","70","80","90","100"};
static const char *rate_choices[]={"32 kHz (original)","44,1 kHz","48 kHz"};
static std::vector<std::string> runahead_names;
static std::vector<const char *> runahead_choices;

static void menu_style(float scale) {
    ImGuiStyle &style=ImGui::GetStyle();
    style=ImGuiStyle();ImGui::StyleColorsDark();
    const ImVec4 transparent(0,0,0,0),hover(1,1,1,0.10f),white(1,1,1,0.28f);
    for(ImGuiCol color : {ImGuiCol_Button,ImGuiCol_Header,ImGuiCol_FrameBg,
                         ImGuiCol_Tab,ImGuiCol_TabDimmed,ImGuiCol_TabDimmedSelected})
        style.Colors[color]=transparent;
    for(ImGuiCol color : {ImGuiCol_ButtonHovered,ImGuiCol_HeaderHovered,
                         ImGuiCol_FrameBgHovered,ImGuiCol_TabHovered,ImGuiCol_TabSelected,
                         ImGuiCol_TabDimmedSelected,ImGuiCol_ButtonActive,ImGuiCol_HeaderActive,
                         ImGuiCol_FrameBgActive}) style.Colors[color]=hover;
    style.Colors[ImGuiCol_NavCursor]=white;
    style.Colors[ImGuiCol_NavWindowingHighlight]=white;
    style.Colors[ImGuiCol_Border]=style.Colors[ImGuiCol_Separator]=white;
    style.Colors[ImGuiCol_TabSelectedOverline]=white;
    style.Colors[ImGuiCol_TabDimmedSelectedOverline]=transparent;
    style.TabBorderSize=1;style.TabBarBorderSize=0;style.TabRounding=0;
    style.ButtonTextAlign=style.SelectableTextAlign=ImVec2(0.5f,0.5f);
    style.AntiAliasedLinesUseTex=false;
    style.ScaleAllSizes(scale);
}

// Measure all tab labels before NewFrame; the same scale applies to every
// widget. Reuse the native font atlas instead of rebuilding it on each resize.
static void fit_menu_font(ImGuiIO &io,float display_scale) {
    float preferred=std::clamp(io.DisplaySize.y/720.0f,0.75f,2.0f);
    menu_style(display_scale*std::min(preferred,1.0f));
    ImGuiStyle &style=ImGui::GetStyle();
    style.WindowPadding.x=io.DisplaySize.y*0.90f*4.0f/3.0f*0.05f;
    float available=io.DisplaySize.y*0.90f*4.0f/3.0f-2*style.WindowPadding.x-style.ScrollbarSize;
    float labels=0;
    for(const char *tab:menu_tabs)
        labels+=io.FontDefault->CalcTextSizeA(io.FontDefault->FontSize,FLT_MAX,0,tab).x;
    // Include frame padding, gaps and rounding slack, not just glyph widths.
    float overhead=2*menu_tab_count*style.FramePadding.x+(menu_tab_count-1)*style.ItemInnerSpacing.x+2*menu_tab_count;
    float hints=2*(io.FontDefault->FontSize*4.0f+io.FontDefault->CalcTextSizeA(io.FontDefault->FontSize,FLT_MAX,0,"/").x);
    io.FontGlobalScale=std::min(preferred,std::max(0.01f,(available-overhead)/(labels+hints)));
    auto measure=[&](const char *text) { return io.FontDefault->CalcTextSizeA(io.FontDefault->FontSize,FLT_MAX,0,text).x; };
    float widest=std::max({measure("8:7 Square Pixels"),measure("32 kHz (original)"),measure("Internal resolution")});
    float row=std::max(measure("Windowed mode size"),measure("Screenshot resolution"))+widest+measure("< >     ");
    io.FontGlobalScale=std::min(io.FontGlobalScale,std::max(0.01f,(available-6)/row));
    // Keep the fixed label/prompt columns and a useful warning column in view.
    float controls_row=1.10f*measure("Decrease screen size")+measure("Press button (press Esc to cancel or Del to clear)")+
                       0.75f*measure("already used in [ToggleWidescreen]")+measure("      ");
    io.FontGlobalScale=std::min(io.FontGlobalScale,std::max(0.01f,(available-8)/controls_row));
}

static SDL_Window *menu_window;
static SDL_Renderer *menu_renderer;
static SDL_Texture *blur_texture;
static float dpi;
static SDL_SystemTheme theme = SDL_SYSTEM_THEME_UNKNOWN;
static bool frame_started, shown, release_input, focus_first, capture_pending;
static bool reset_sections;
static bool controls_open[2];
static bool mouse_dormant,mouse_activity;
static bool mouse_focus_locked;
static float locked_mouse_x,locked_mouse_y;
static bool controls_was_capturing,restore_controls_focus;
static int capture_branch,capture_row=-1,selected_branch,selected_row=-1;
static ImGuiID selected_binding_id,selected_hotkey_id;
static bool hotkeys_was_capturing,restore_hotkeys_focus;
static int capture_hotkey,selected_hotkey;
static int active_tab, tab_delta, blur_width, blur_height;
static int focus_settle;
static bool dismiss(bool returning_to_game=true) {
    if(returning_to_game && !RecompMenuClosed())return false;
    RecompControlsCancel();RecompHotkeysCancel();
    controls_was_capturing=restore_controls_focus=false;selected_binding_id=selected_hotkey_id=0;
    hotkeys_was_capturing=restore_hotkeys_focus=false;
    shown = false; release_input = true;
    if (ImGui::GetCurrentContext()) {
        ImGui::ClosePopupsOverWindow(nullptr, false);
        ImGui::GetIO().ClearInputKeys();
    }
    return true;
}
static ImFont *load_font(float size) {
    bool system = !SDL_getenv(RecompMenuServices()->font_fallback_env);
    for (int attempt = 0; attempt < 2 && !SDL_getenv(RecompMenuServices()->font_no_files_env); ++attempt) {
        std::string path = RecompMenuFontPath(system && attempt == 0);
        size_t length = 0;
        void *data = SDL_LoadFile(path.c_str(), &length);
        if (!data || length < 12 || length > 32*1024*1024) { SDL_free(data); continue; }
        void *copy = ImGui::MemAlloc(length);
        std::memcpy(copy, data, length); SDL_free(data);
        ImFont *font = ImGui::GetIO().Fonts->AddFontFromMemoryTTF(copy, (int)length, size);
        if (font) return font;
    }
    // The same licensed asset is embedded so a moved exe or missing assets folder
    // cannot terminate the game or silently activate ImGui's pixel font.
    size_t length;
    const unsigned char *data = RecompMenuBundledFontData(&length);
    ImFontConfig config;
    config.FontDataOwnedByAtlas = false;
    ImFont *font = ImGui::GetIO().Fonts->AddFontFromMemoryTTF(const_cast<unsigned char *>(data), (int)length, size, &config);
    if (font) return font;
    ImFontConfig fallback; fallback.SizePixels = size;
    return ImGui::GetIO().Fonts->AddFontDefault(&fallback);
}

bool RecompMenuVisible(void) { return menu_window && shown; }
void RecompMenuUiGamepadsChanged(void) {
    if(!menu_renderer || !ImGui::GetCurrentContext())return;
    SDL_Gamepad *pads[16];int count=RecompDevicesGamepads(pads,16);
    ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_Manual,pads,count);
}
void RecompMenuUiMouseWakeOnly(void) { mouse_dormant=true;mouse_activity=false; }
#ifdef _WIN32
static void suppress_locked_mouse(void) {
    ImGuiContext &ctx=*ImGui::GetCurrentContext();
    // The SDL backend also polls global position in NewFrame. Remove only mouse
    // events, retaining keyboard/gamepad navigation and the current NavId.
    for(int i=ctx.InputEventsQueue.Size-1;i>=0;--i) {
        ImGuiInputEventType type=ctx.InputEventsQueue[i].Type;
        if(type==ImGuiInputEventType_MousePos || type==ImGuiInputEventType_MouseButton ||
           type==ImGuiInputEventType_MouseWheel)
            ctx.InputEventsQueue.erase(ctx.InputEventsQueue.Data+i);
    }
    ImGuiIO &io=ImGui::GetIO();io.ClearInputMouse();
    io.MousePosPrev=io.MousePos=ImVec2(-FLT_MAX,-FLT_MAX);io.MouseDelta=ImVec2(0,0);
    io.WantCaptureMouse=io.WantSetMousePos=false;
    io.ConfigFlags|=ImGuiConfigFlags_NoMouse;
    ctx.HoveredWindow=nullptr;
}
#endif
void RecompMenuUiLockMouseFocus(void) {
#ifdef _WIN32
    if(!ImGui::GetCurrentContext())return;
    mouse_focus_locked=true;mouse_activity=false;
    SDL_GetGlobalMouseState(&locked_mouse_x,&locked_mouse_y);
    suppress_locked_mouse();
#endif
}
void RecompMenuUiSuppressInput(void) {
    if(!ImGui::GetCurrentContext()) return;
    ImGuiIO &io=ImGui::GetIO();
    io.ClearEventsQueue();io.ClearInputKeys();io.ClearInputMouse();
}

void RecompMenuUiSync(void) {
    if (!menu_window || !ImGui::GetCurrentContext()) return;
    float next = SDL_GetWindowDisplayScale(menu_window);
    if (!(next > 0)) next = 1;
    SDL_SystemTheme next_theme = SDL_GetSystemTheme();
    if (next != dpi || next_theme != theme) {
        menu_style(next);
        if (next != dpi) {
            if (menu_renderer) ImGui_ImplSDLRenderer3_DestroyFontsTexture();
            ImGuiIO &io = ImGui::GetIO();
            io.Fonts->Clear(); io.FontDefault = load_font(20.0f * next);
            if (!io.FontDefault || !io.Fonts->Build()) {
                io.Fonts->Clear();
                ImFontConfig fallback; fallback.SizePixels = 20.0f * next;
                io.FontDefault = io.Fonts->AddFontDefault(&fallback); io.Fonts->Build();
            }
        }
        dpi = next; theme = next_theme;
    }
}
void RecompMenuUiInit(SDL_Window *window) {
    menu_window = window;
    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    RecompMenuResetSelectorOptions();
    RecompMenuRegisterSelectorOptions(aspect_choices,2);
    RecompMenuRegisterSelectorOptions(output_choices,3);
    RecompMenuRegisterSelectorOptions(internal_choices,10);
    RecompMenuRegisterSelectorOptions(size_choices,9);
    RecompMenuRegisterSelectorOptions(volume_choices,11);
    RecompMenuRegisterSelectorOptions(rate_choices,3);
    RecompMenuRegisterSelectorOptions(capture_choices,2);
    runahead_names.clear();runahead_choices.clear();
    for(int i=0;i<=RecompMenuRunAheadMax();i++)runahead_names.push_back(std::to_string(i));
    for(const auto &name:runahead_names)runahead_choices.push_back(name.c_str());
    RecompMenuRegisterSelectorOptions(runahead_choices.data(),(int)runahead_choices.size());
    ImGuiIO &io = ImGui::GetIO(); io.IniFilename = nullptr; io.LogFilename = nullptr;
    io.ConfigNavCursorVisibleAlways=true;mouse_dormant=mouse_activity=mouse_focus_locked=false;
    RecompMenuBeginFocusFrame(false);
    // Cursor visibility belongs to the host inactivity timer, even in the HUD.
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_NoMouseCursorChange;
    dpi = 0; shown = release_input = frame_started = false;
    focus_first = capture_pending = false; active_tab = tab_delta = 0;
    controls_open[0]=controls_open[1]=false;shader_folders.clear();
    controls_was_capturing=restore_controls_focus=false;selected_binding_id=selected_hotkey_id=0;
    hotkeys_was_capturing=restore_hotkeys_focus=false;
    RecompMenuUiSync();
}
void RecompMenuRendererShutdown(void) {
    if (frame_started) { ImGui::EndFrame(); frame_started = false; }
    if (blur_texture) SDL_DestroyTexture(blur_texture);
    blur_texture = nullptr; blur_width = blur_height = 0;
    if (menu_renderer) {
        ImGui_ImplSDLRenderer3_Shutdown(); ImGui_ImplSDL3_Shutdown(); menu_renderer = nullptr;
    }
    RecompMenuIconsShutdown();
    capture_pending = shown;
}
void RecompMenuUiShutdown(void) {
    RecompMenuRendererShutdown();
    if (ImGui::GetCurrentContext()) ImGui::DestroyContext();
    menu_window = nullptr; shown = false;
}
int RecompMenuHeight(int output_height) { (void)output_height; return 0; }

bool RecompMenuOwnsInput(void) {
    if(RecompMenuAltOwnsInput()) return true;
    if (shown) return true;
    if (!release_input) return false;
    const bool *keys = SDL_GetKeyboardState(nullptr);
    for (int i=1;i<SDL_SCANCODE_COUNT;++i) if(keys[i]) return true;
    SDL_Gamepad *pads[32];int count=RecompDevicesGamepads(pads,32);
    bool held=false;
    for(int i=0;i<count;++i) if(SDL_Gamepad *pad=pads[i]) {
        for(int b=0;b<SDL_GAMEPAD_BUTTON_COUNT;++b)
            held |= SDL_GetGamepadButton(pad,(SDL_GamepadButton)b);
        for(int a=0;a<4;++a) held |= std::abs((int)SDL_GetGamepadAxis(pad,(SDL_GamepadAxis)a))>8000;
    }
    release_input=held;return held;
}

void RecompMenuUiToggle(void) {
    if(!menu_window)return;
    if(shown) {dismiss();return;}
    RecompMenuUiSuppressInput();
    ImGui::ClosePopupsOverWindow(nullptr,false);ImGui::ClearActiveID();ImGui::SetWindowFocus(nullptr);
    RecompMenuOpened();RecompMenuModsOpened();RecompMenuShadersRefresh();
    shown=true;capture_pending=focus_first=reset_sections=true;active_tab=0;tab_delta=menu_tab_count;
    focus_settle=4;
    controls_open[0]=controls_open[1]=false;shader_folders.clear();
}

bool RecompMenuUiEvent(SDL_Event *e) {
    if (!menu_window) return false;
#ifdef _WIN32
    if(mouse_focus_locked) {
        if(e->type==SDL_EVENT_WINDOW_RESIZED || e->type==SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED ||
           e->type==SDL_EVENT_WINDOW_MOVED)
            SDL_GetGlobalMouseState(&locked_mouse_x,&locked_mouse_y);
        if(e->type==SDL_EVENT_MOUSE_MOTION) {
            float x=0,y=0;SDL_GetGlobalMouseState(&x,&y);
            if((e->motion.xrel==0 && e->motion.yrel==0) ||
               (x==locked_mouse_x && y==locked_mouse_y))return shown;
            mouse_focus_locked=false;
        }
        // A click at the static post-resize position must not steal focus either.
        if(mouse_focus_locked && e->type>=SDL_EVENT_MOUSE_MOTION && e->type<=SDL_EVENT_MOUSE_WHEEL)
            return shown;
    }
#endif
    if(e->type==SDL_EVENT_MOUSE_MOTION || e->type==SDL_EVENT_MOUSE_BUTTON_DOWN || e->type==SDL_EVENT_MOUSE_BUTTON_UP)
        { mouse_dormant=false;mouse_activity=true; }
    if(menu_renderer && (e->type==SDL_EVENT_MOUSE_BUTTON_DOWN || e->type==SDL_EVENT_MOUSE_BUTTON_UP))
        ImGui::GetIO().AddMousePosEvent(e->button.x,e->button.y);
    if(menu_renderer) ImGui_ImplSDL3_ProcessEvent(e);
    if(!shown) return false;
    if(e->type==SDL_EVENT_KEY_DOWN || e->type==SDL_EVENT_KEY_UP) {
        if(RecompMenuFullscreenKey(e)) { capture_pending=true;return false; }
        if(e->type==SDL_EVENT_KEY_DOWN && !e->key.repeat && !(e->key.mod&(SDL_KMOD_CTRL|SDL_KMOD_ALT|SDL_KMOD_GUI))) {
            if(e->key.key==SDLK_q) --tab_delta;
            if(e->key.key==SDLK_e) ++tab_delta;
        }
        return true;
    }
    if(e->type==SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        if(e->gbutton.button==SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) --tab_delta;
        if(e->gbutton.button==SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) ++tab_delta;
    }
    return e->type==SDL_EVENT_TEXT_INPUT ||
        (e->type>=SDL_EVENT_MOUSE_MOTION && e->type<=SDL_EVENT_MOUSE_WHEEL) ||
        e->type==SDL_EVENT_GAMEPAD_BUTTON_DOWN || e->type==SDL_EVENT_GAMEPAD_BUTTON_UP ||
        e->type==SDL_EVENT_GAMEPAD_AXIS_MOTION;
}

/* Capture the game after compositing, before any ImGui draw. A half-resolution
 * separable Gaussian has a 15-output-pixel support radius (sigma 5). Cached
 * until a new open or output-size change, never read back every paused frame. */
static void capture_blur(SDL_Renderer *renderer) {
    SDL_Surface *capture=SDL_RenderReadPixels(renderer,nullptr);
    if(!capture) return;
    SDL_Surface *rgba=SDL_ConvertSurface(capture,SDL_PIXELFORMAT_RGBA32);SDL_DestroySurface(capture);
    if(!rgba) return;
    int w=(rgba->w+1)/2,h=(rgba->h+1)/2;
    std::vector<float> source((size_t)w*h*4),horizontal(source.size());
    for(int y=0;y<h;++y) for(int x=0;x<w;++x) for(int c=0;c<4;++c) {
        float sum=0;int n=0;
        for(int dy=0;dy<2 && y*2+dy<rgba->h;++dy) for(int dx=0;dx<2 && x*2+dx<rgba->w;++dx) {
            sum+=((unsigned char *)rgba->pixels)[(y*2+dy)*rgba->pitch+(x*2+dx)*4+c];++n;
        }
        source[((size_t)y*w+x)*4+c]=sum/n;
    }
    SDL_DestroySurface(rgba);
    constexpr int radius=8;float kernel[radius*2+1],total=0;
    for(int i=-radius;i<=radius;++i) total+=(kernel[i+radius]=std::exp(-float(i*i)/(2*2.5f*2.5f)));
    for(float &v:kernel) v/=total;
    for(int y=0;y<h;++y) for(int x=0;x<w;++x) for(int c=0;c<4;++c) {
        float v=0;for(int k=-radius;k<=radius;++k) v+=source[((size_t)y*w+std::clamp(x+k,0,w-1))*4+c]*kernel[k+radius];
        horizontal[((size_t)y*w+x)*4+c]=v;
    }
    std::vector<unsigned char> pixels(source.size());
    for(int y=0;y<h;++y) for(int x=0;x<w;++x) for(int c=0;c<4;++c) {
        float v=0;for(int k=-radius;k<=radius;++k) v+=horizontal[((size_t)std::clamp(y+k,0,h-1)*w+x)*4+c]*kernel[k+radius];
        pixels[((size_t)y*w+x)*4+c]=(unsigned char)std::clamp((int)std::lround(v),0,255);
    }
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,w,h);
    if(texture && SDL_UpdateTexture(texture,nullptr,pixels.data(),w*4)) {
        SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_LINEAR);SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_NONE);
        if(blur_texture) SDL_DestroyTexture(blur_texture);blur_texture=texture;
        SDL_GetRenderOutputSize(renderer,&blur_width,&blur_height);capture_pending=false;
    } else if(texture) SDL_DestroyTexture(texture);
}

static void focus_first_item() {
    if(!focus_first) return;
    ImGui::SetWindowFocus();
    ImGui::SetItemDefaultFocus();
    // DefaultFocus handles appearing windows; explicit keyboard focus also
    // handles switching content in an already visible window (including mouse).
    ImGuiContext &ctx=*ImGui::GetCurrentContext();
    ImGuiWindow *window=ImGui::GetCurrentWindow();
    ImGui::NavMoveRequestCancel();ctx.NavInitRequest=false;ctx.NavInitResult.ID=0;
    ImGui::FocusWindow(window);
    ImGui::SetNavID(ctx.LastItemData.ID,ImGuiNavLayer_Main,ctx.CurrentFocusScopeId,
        ImGui::WindowRectAbsToRel(window,ctx.LastItemData.NavRect));
    ImGui::SetNavCursorVisible(true);
    focus_first=false;
}

static bool menu_button(const char *label) {
    return RecompMenuButton(label,focus_first);
}

static void option_row(float width) {
    ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);ImGui::SetNextItemWidth(width);
}
static void draw_game() {
    ImVec2 cursor=ImGui::GetCursorScreenPos();cursor.x=std::ceil(ImGui::GetCurrentWindow()->WorkRect.Min.x);
    ImGui::SetCursorScreenPos(cursor);
    float width=std::floor(ImGui::GetCurrentWindow()->WorkRect.Max.x)-cursor.x;
    if(ImGui::BeginTable("GameOptions",2,ImGuiTableFlags_SizingStretchSame|ImGuiTableFlags_NoPadOuterX|ImGuiTableFlags_NoPadInnerX,ImVec2(width,0))) {
        option_row(width);
        RecompMenuCheckbox("Pause when inactive",&g_config.pause_when_inactive,focus_first);focus_first_item();
        option_row(width);RecompMenuCheckbox("Autosave",&g_config.autosave);
        option_row(width);bool vibration=g_config.controller_vibration;
        if(RecompMenuCheckbox("Controller vibration",&vibration))RecompMenuSetControllerVibration(vibration);
        option_row(width);int frames=g_config.run_ahead;
        if(RecompMenuSelector("Run ahead",runahead_choices.data(),(int)runahead_choices.size(),&frames))RecompMenuSetRunAhead(frames);
        ImGui::EndTable();
    }
}

static void draw_graphics() {
    // Capture the parent content width before entering the table: a cell's
    // available width must never determine the selection row geometry.
    ImVec2 cursor=ImGui::GetCursorScreenPos();cursor.x=std::ceil(ImGui::GetCurrentWindow()->WorkRect.Min.x);
    ImGui::SetCursorScreenPos(cursor);
    float width=std::floor(ImGui::GetCurrentWindow()->WorkRect.Max.x)-cursor.x;
    if(ImGui::BeginTable("GraphicsOptions",2,ImGuiTableFlags_SizingStretchSame|ImGuiTableFlags_NoPadOuterX|ImGuiTableFlags_NoPadInnerX,ImVec2(width,0))) {
        ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);
        bool checked=snes_osd_fps_visible()!=0;
        ImGui::SetNextItemWidth(width);
        if(RecompMenuCheckbox("Show FPS",&checked,focus_first))snes_osd_set_fps_visible(checked);
        focus_first_item();
        ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);
        int aspect=g_config.display_aspect==kSnesDisplayAspect_SquarePixels8x7?1:0;
        ImGui::SetNextItemWidth(width);
        if(RecompMenuSelector("Display Aspect",aspect_choices,2,&aspect))RecompMenuSetDisplayAspect(aspect);
        option_row(width);bool fullscreen=RecompMenuFullscreen();
        if(RecompMenuCheckbox("Full screen",&fullscreen))
            RecompMenuSetFullscreenCheckbox(fullscreen,ImGui::GetCurrentContext()->NavInputSource==ImGuiInputSource_Gamepad && !ImGui::IsMouseClicked(0));
        option_row(width);bool vsync=g_config.vsync;
        if(RecompMenuCheckbox("VSync",&vsync))RecompMenuSetVSync(vsync);
        option_row(width);int output=g_config.output_method==kOutputMethod_OpenGL?1:g_config.output_method==kOutputMethod_SDLSoftware?2:0;
        if(RecompMenuSelector("Output method",output_choices,3,&output))
            RecompMenuSetOutputMethod(output==1?kOutputMethod_OpenGL:output==2?kOutputMethod_SDLSoftware:kOutputMethod_SDL);
        option_row(width);RecompMenuCheckbox("Remove sprite limit",&g_config.no_sprite_limits);
        option_row(width);bool wide=g_config.ignore_aspect_ratio;
        if(RecompMenuCheckbox("Widescreen",&wide))RecompMenuSetWidescreen(wide);
        option_row(width);int size=RecompMenuWindowMultiplier()-1;
        if(RecompMenuSelector("Windowed mode size",size_choices,9,&size))RecompMenuSetWindowSize(size+1);
        option_row(width);int scale=std::clamp((int)g_config.window_scale,1,10)-1;
        if(RecompMenuSelector("Internal resolution",internal_choices,10,&scale))RecompMenuSetInternalScale(scale+1);
        option_row(width);RecompMenuCheckbox("Integer scaling",&g_config.integer_scaling);
        option_row(width);int capture=g_config.screenshot_internal?0:1;
        if(RecompMenuSelector("Screenshot resolution",capture_choices,2,&capture))g_config.screenshot_internal=capture==0;
        ImGui::EndTable();
    }
}
static void draw_audio() {
    ImVec2 cursor=ImGui::GetCursorScreenPos();cursor.x=std::ceil(ImGui::GetCurrentWindow()->WorkRect.Min.x);
    ImGui::SetCursorScreenPos(cursor);
    float width=std::floor(ImGui::GetCurrentWindow()->WorkRect.Max.x)-cursor.x;
    if(ImGui::BeginTable("AudioOptions",2,ImGuiTableFlags_SizingStretchSame|ImGuiTableFlags_NoPadOuterX|ImGuiTableFlags_NoPadInnerX,ImVec2(width,0))) {
        option_row(width);bool audio=g_config.enable_audio;
        if(RecompMenuCheckbox("Enable audio",&audio,focus_first))RecompMenuSetAudio(audio);
        focus_first_item();
        option_row(width);int volume=std::clamp((g_config.volume+5)/10,0,10);
        if(RecompMenuSelector("Audio volume",volume_choices,11,&volume))RecompMenuSetVolume(volume*10);
        option_row(width);int rate=g_config.audio_freq==32000?0:g_config.audio_freq==44100?1:2;
        if(RecompMenuSelector("Sample rate",rate_choices,3,&rate))RecompMenuSetSampleRate(rate==0?32000:rate==1?44100:48000);
        ImGui::EndTable();
    }
}

static void draw_remapping_popup() {
    ImGuiIO &io=ImGui::GetIO();
    float width=std::min(io.DisplaySize.x*0.80f,io.DisplaySize.y*1.20f);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x/2,io.DisplaySize.y/2),ImGuiCond_Always,ImVec2(0.5f,0.5f));
    ImGui::SetNextWindowSize(ImVec2(width,width/2),ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.90f);
    ImGui::PushStyleColor(ImGuiCol_PopupBg,ImVec4(0,0,0,1));
    if(ImGui::BeginPopupModal("Configure SNES buttons",nullptr,
        ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|
        ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoScrollbar)) {
        if(!RecompControlsWizard())ImGui::CloseCurrentPopup();
        else {
            char prompt[96];std::snprintf(prompt,sizeof(prompt),"Press button for %s",RecompControlsPrompt());
            const char *lines[]={prompt,"","Press Esc to cancel"};
            float height=3*ImGui::GetTextLineHeight()+2*ImGui::GetStyle().ItemSpacing.y;
            ImGui::SetCursorPosY((ImGui::GetWindowHeight()-height)/2);
            for(const char *line:lines) {
                ImGui::SetCursorPosX((ImGui::GetWindowWidth()-ImGui::CalcTextSize(line).x)/2);
                ImGui::TextUnformatted(line);
            }
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
}
bool RecompMenuUiClearBinding(const SDL_Event *event) {
    if(shown && active_tab==5 && ImGui::GetCurrentContext() && selected_hotkey_id &&
       ImGui::GetCurrentContext()->NavId==selected_hotkey_id &&
       (event->type==SDL_EVENT_KEY_DOWN || event->type==SDL_EVENT_KEY_UP) && event->key.key==SDLK_DELETE &&
       !((event->key.mod|SDL_GetModState())&SDL_KMOD_ALT)) {
        if(event->type==SDL_EVENT_KEY_DOWN && !event->key.repeat) {
            RecompHotkeysClear(selected_hotkey);capture_hotkey=selected_hotkey;restore_hotkeys_focus=true;
        }
        return true;
    }
    if(!shown || active_tab!=4 || !ImGui::GetCurrentContext() || !selected_binding_id ||
       ImGui::GetCurrentContext()->NavId!=selected_binding_id ||
       (event->type!=SDL_EVENT_KEY_DOWN && event->type!=SDL_EVENT_KEY_UP) || event->key.key!=SDLK_DELETE ||
       ((event->key.mod|SDL_GetModState())&SDL_KMOD_ALT))return false;
    if(event->type==SDL_EVENT_KEY_DOWN && !event->key.repeat) {
        RecompControlsClear(selected_branch==0,selected_row);
        capture_branch=selected_branch;capture_row=selected_row;restore_controls_focus=true;
    }
    return true;
}
static void restore_control_item(bool restore) {
    if(!restore)return;
    ImGui::SetWindowFocus();ImGui::SetKeyboardFocusHere(-1);ImGui::SetNavCursorVisible(true);
    restore_controls_focus=false;
}
static void draw_controls() {
    static const int buttons[]={0,1,2,3,4,5,6,7,8,9,10,11};
    if(controls_was_capturing && !RecompControlsActive())restore_controls_focus=true;
    bool open_popup=false;
    ImVec2 cursor=ImGui::GetCursorScreenPos();cursor.x=std::ceil(ImGui::GetCurrentWindow()->WorkRect.Min.x);
    ImGui::SetCursorScreenPos(cursor);
    float width=std::floor(ImGui::GetCurrentWindow()->WorkRect.Max.x)-cursor.x;
    ImGui::BeginDisabled(RecompControlsWizard());
    if(ImGui::BeginTable("ControlsOptions",3,ImGuiTableFlags_SizingFixedFit|ImGuiTableFlags_NoPadOuterX|ImGuiTableFlags_NoPadInnerX,ImVec2(width,0))) {
        float label_width=0;for(int button:buttons)label_width=std::max(label_width,ImGui::CalcTextSize(RecompControlsName(button)).x);
        float previous_left=std::ceil(label_width)+2*RecompMenuRowPadding();
        float middle=std::ceil(ImGui::CalcTextSize("Press button (press Esc to cancel or Del to clear)").x)+2*RecompMenuRowPadding();
        float previous_right=std::max(1.0f,width-previous_left-middle);
        // Scale each outer column relative to its original measured width.
        // The middle stays fixed; remaining space is an invisible right gutter.
        ImGui::TableSetupColumn("Button",ImGuiTableColumnFlags_WidthFixed,previous_left*1.10f);
        ImGui::TableSetupColumn("Binding",ImGuiTableColumnFlags_WidthFixed,middle);
        ImGui::TableSetupColumn("Conflict",ImGuiTableColumnFlags_WidthFixed,previous_right*0.90f);
        for(int i=0;i<2;i++) {
            const char *label=i?"Keyboard Remapping":"Gamepad Remapping";
            option_row(width);
            RecompMenuDropdown(label,&controls_open[i],focus_first,true);
            if(i==0)focus_first_item();
            if(controls_open[i]) {
                ImGui::PushID(i);option_row(width);
                bool restore=restore_controls_focus && capture_branch==i && capture_row==-1 && !ImGui::IsPopupOpen("Configure SNES buttons",ImGuiPopupFlags_AnyPopupId);
                if(RecompMenuActionButton("Configure all buttons",restore)) {
                    RecompControlsBegin(i==0);RecompMenuUiSuppressInput();
                    capture_branch=i;capture_row=-1;
                    open_popup=true;
                }
                restore_control_item(restore);
                for(int row=0;row<12;row++) {
                    int button=buttons[row];option_row(width);
                    RecompConflict entries[RECOMP_CONFLICT_MAX];size_t count=0;
                    char value[128],conflict[256],raw[128];RecompControlsRaw(i==0,button,raw,sizeof(raw));
                    bool capturing=RecompControlsCapturing(i==0,button);
                    if(capturing) {
                        std::snprintf(value,sizeof(value),"Press button (press Esc to cancel or Del to clear)");*conflict=0;
                    } else {
                        RecompControlsLabel(i==0,button,value,sizeof(value));
                        RecompControlsConflict(i==0,button,conflict,sizeof(conflict));
                        count=RecompControlsConflicts(i==0,button,entries,RECOMP_CONFLICT_MAX);
                    }
                    restore=restore_controls_focus && capture_branch==i && capture_row==button;
                    if(RecompMenuBindingRow(RecompControlsName(button),value,conflict,restore,capturing,raw,i==0,entries,count)) {
                        RecompControlsBeginSingle(i==0,button);RecompMenuUiSuppressInput();
                        capture_branch=i;capture_row=button;
                    }
                    restore_control_item(restore);
                    if(ImGui::IsItemFocused()) {
                        selected_binding_id=ImGui::GetItemID();selected_branch=i;selected_row=button;
                    }
                }
                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }
    ImGui::EndDisabled();
    if(open_popup)ImGui::OpenPopup("Configure SNES buttons");
    draw_remapping_popup();
    controls_was_capturing=RecompControlsActive();
}

static ImGuiWindow *menu_scroll_target;
static void draw_hotkeys() {
    if(hotkeys_was_capturing && !RecompHotkeysActive())restore_hotkeys_focus=true;
    ImVec2 cursor=ImGui::GetCursorScreenPos();cursor.x=std::ceil(ImGui::GetCurrentWindow()->WorkRect.Min.x);
    ImGui::SetCursorScreenPos(cursor);float width=std::floor(ImGui::GetCurrentWindow()->WorkRect.Max.x)-cursor.x;

    if(focus_first)ImGui::SetScrollY(0);
    if(ImGui::BeginTable("HotkeyOptions",3,ImGuiTableFlags_SizingFixedFit|ImGuiTableFlags_NoPadOuterX|ImGuiTableFlags_NoPadInnerX,ImVec2(width,0))) {
        float label_width=0;for(int i=0;i<RECOMP_HOTKEY_COUNT;i++)label_width=std::max(label_width,ImGui::CalcTextSize(RecompHotkeysName(i)).x);
        float left=std::ceil(label_width)+2*RecompMenuRowPadding();
        float middle=std::ceil(ImGui::CalcTextSize("Press button (press Esc to cancel or Del to clear)").x)+2*RecompMenuRowPadding();
        ImGui::TableSetupColumn("Function",ImGuiTableColumnFlags_WidthFixed,left*1.10f);
        ImGui::TableSetupColumn("Binding",ImGuiTableColumnFlags_WidthFixed,middle);
        ImGui::TableSetupColumn("Conflict",ImGuiTableColumnFlags_WidthFixed,std::max(1.0f,width-left-middle)*0.90f);
        for(int i=0;i<RECOMP_HOTKEY_COUNT;i++) {
            option_row(width);RecompConflict entries[RECOMP_CONFLICT_MAX];size_t count=0;char value[128],conflict[512],raw[128];RecompHotkeysRaw(i,raw,sizeof(raw));bool capturing=RecompHotkeysCapturing(i);
            if(capturing){std::snprintf(value,sizeof(value),"Press button (press Esc to cancel or Del to clear)");*conflict=0;}
            else {RecompHotkeysLabel(i,value,sizeof(value));RecompHotkeysConflict(i,conflict,sizeof(conflict));count=RecompHotkeysConflicts(i,entries,RECOMP_CONFLICT_MAX);}
            bool restore=restore_hotkeys_focus && capture_hotkey==i;
            if(RecompMenuBindingRow(RecompHotkeysName(i),value,conflict,restore || focus_first,capturing,raw,false,entries,count)) {
                RecompHotkeysBegin(i);capture_hotkey=i;RecompMenuUiSuppressInput();
            }
            if(i==0)focus_first_item();
            if(restore){ImGui::SetWindowFocus();ImGui::SetKeyboardFocusHere(-1);ImGui::SetNavCursorVisible(true);restore_hotkeys_focus=false;}
            if(ImGui::IsItemFocused()){selected_hotkey=i;selected_hotkey_id=ImGui::GetItemID();}
        }
        ImGui::EndTable();
    }

    hotkeys_was_capturing=RecompHotkeysActive();
}

static void small_text(const char *text,ImU32 color) {
    float size=ImGui::GetFontSize()*0.75f,width=ImGui::GetContentRegionAvail().x;
    ImVec2 bounds=ImGui::GetFont()->CalcTextSizeA(size,FLT_MAX,width,text);
    ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(),size,ImGui::GetCursorScreenPos(),color,text,nullptr,width);
    ImGui::Dummy(ImVec2(width,std::max(size,bounds.y)));
}
static void shader_children(const std::string &prefix,float width,int depth,bool disabled) {
    std::set<std::string> drawn;
    ImGui::PushID(prefix.c_str());
    int node=0;
    for(int i=0;i<RecompMenuShaderCount();i++) {
        const char *path=RecompMenuShaderName(i);
        if(std::strncmp(path,prefix.c_str(),prefix.size()))continue;
        std::string relative=path+prefix.size();size_t slash=relative.find('/');
        if(slash!=std::string::npos) {
            std::string folder=relative.substr(0,slash),child=prefix+folder+"/";
            if(!drawn.insert(child).second)continue;
            ImGui::PushID(node++);ImGui::PushID(child.c_str());
            option_row(width);std::string label=folder;
            bool &open=shader_folders[child];bool selected=!std::strncmp(g_config.selected_shader,child.c_str(),child.size());
            bool yellow=selected && !disabled;
            if(yellow)ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(1,1,0,1));
            RecompMenuDropdown(label.c_str(),&open,false,true,depth*ImGui::CalcTextSize("  ").x);
            if(yellow)ImGui::PopStyleColor();
            if(open)shader_children(child,width,depth+1,disabled);
            ImGui::PopID();ImGui::PopID();
        } else {
            ImGui::PushID(node++);ImGui::PushID(path);
            option_row(width);std::string label=relative;
            bool selected=std::strcmp(path,g_config.selected_shader)==0;
            ImGui::BeginDisabled(disabled);
            float indent=depth?depth*ImGui::CalcTextSize("  ").x+ImGui::GetFontSize():0;
            if(RecompMenuCheckbox(label.c_str(),&selected,false,indent))RecompMenuSelectShader(i);
            ImGui::EndDisabled();ImGui::PopID();ImGui::PopID();
        }
    }
    ImGui::PopID();
}
static void draw_shaders() {
    float width=std::floor(ImGui::GetCurrentWindow()->WorkRect.Max.x)-std::ceil(ImGui::GetCurrentWindow()->WorkRect.Min.x);
    bool available=g_config.output_method==kOutputMethod_OpenGL;
    const ImVec4 dim(0.40f,0.40f,0.40f,1);
    if(ImGui::BeginTable("ShaderOptionsTable",2,ImGuiTableFlags_SizingStretchSame|ImGuiTableFlags_NoPadOuterX|ImGuiTableFlags_NoPadInnerX,ImVec2(width,0))) {
        option_row(width);
        if(!available)ImGui::PushStyleColor(ImGuiCol_Text,dim);
        ImGui::PushStyleVar(ImGuiStyleVar_DisabledAlpha,1.0f);ImGui::BeginDisabled(!available);
        bool enabled=g_config.enable_shaders;
        if(RecompMenuCheckbox("Enable shaders",&enabled,focus_first && available))RecompMenuSetShaders(enabled);
        if(available)focus_first_item();
        ImGui::EndDisabled();ImGui::PopStyleVar();if(!available)ImGui::PopStyleColor();
        option_row(width);
        small_text("Shaders are only available when 'Output method' is set to OpenGL",IM_COL32(255,255,255,255));
        bool disabled=!available || !g_config.enable_shaders;
        if(disabled)ImGui::PushStyleColor(ImGuiCol_Text,dim);
        ImGui::PushStyleVar(ImGuiStyleVar_DisabledAlpha,1.0f);
        shader_children("",width,0,disabled);
        ImGui::PopStyleVar();if(disabled)ImGui::PopStyleColor();ImGui::EndTable();
    }
}
static void draw_mods() {
    small_text(RecompMenuServices()->translation_instruction,ImGui::GetColorU32(ImGuiCol_Text));
    if(RecompMenuActionButton("Load .ips file",focus_first))RecompMenuQueueCommand(RECOMP_MENU_IPS);
    focus_first_item();
    if(RecompMenuServices()->TranslationBusy())small_text("Converting translation...",ImGui::GetColorU32(ImGuiCol_Text));
    else if(*RecompMenuServices()->TranslationError())small_text(RecompMenuServices()->TranslationError(),IM_COL32(255,64,64,255));
    float width=ImGui::GetContentRegionAvail().x;
    float remaining=ImGui::GetContentRegionAvail().y;
    float gap=2*ImGui::GetStyle().ItemSpacing.y+ImGui::GetFontSize()*0.75f;
    float old_list_height=std::max(3*ImGui::GetFrameHeight(),remaining*0.43f);
    float metadata_height=std::max(1.0f,remaining-old_list_height-gap)*0.5f;
    float height=std::max(1.0f,remaining-gap-metadata_height);
    float title_line=ImGui::GetTextLineHeightWithSpacing();
    ImVec2 origin=ImGui::GetCursorScreenPos();
    for(int column=0;column<3;column++) {
        float offset=column==0?0:column==1?0.42f:0.58f;
        ImGui::SetCursorScreenPos(ImVec2(origin.x+offset*width+RecompMenuRowPadding(),origin.y));
        if(column!=1)small_text(column==0?"Available":"Active",ImGui::GetColorU32(ImGuiCol_Text));
        ImGui::SetCursorScreenPos(ImVec2(origin.x+offset*width,origin.y+title_line));
        ImGui::PushID(column);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(RecompMenuRowPadding(),RecompMenuRowPadding()));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize,column==1?0.0f:1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,0.0f);
        ImGui::PushStyleColor(ImGuiCol_Border,ImVec4(1,1,1,1));
        ImGui::BeginChild("ModList",ImVec2(width*(column==1?0.16f:0.42f),std::max(1.0f,height-title_line)),
            ImGuiChildFlags_NavFlattened|(column==1?0:ImGuiChildFlags_Borders),ImGuiWindowFlags_NoSavedSettings);
        if(column==1) {
            float button_width=ImGui::CalcTextSize("<->").x+2*RecompMenuRowPadding();
            ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth()-button_width)/2,(height-title_line-ImGui::GetFrameHeight())/2));
            ImGui::SetNextItemWidth(button_width);
            if(RecompMenuActionButton("<->"))RecompMenuModTransfer();
        } else {
            for(int i=0;i<RecompMenuModsCount();i++)if(RecompMenuModActive(i)==(column==2)) {
                bool pressed=RecompMenuModRow(RecompMenuModName(i),RecompMenuModHighlighted(i));
                bool mouse=ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseReleased(ImGuiMouseButton_Left);
                if(ImGui::GetCurrentContext()->NavJustMovedToId==ImGui::GetItemID() &&
                   ImGui::GetCurrentContext()->NavInputSource!=ImGuiInputSource_Mouse &&
                   RecompMenuModSelected()!=i && !pressed)RecompMenuModSelect(i);
                if(pressed) {
                    if(mouse)RecompMenuModClick(i,ImGui::GetIO().KeyCtrl,ImGui::GetIO().KeyShift);
                    else RecompMenuModActivate(i);
                }
            }
        }
        ImGui::EndChild();ImGui::PopStyleColor();ImGui::PopStyleVar(3);ImGui::PopID();
    }
    ImGui::SetCursorScreenPos(ImVec2(origin.x,origin.y+height+ImGui::GetStyle().ItemSpacing.y));
    float padding=std::max(3.0f,2*RecompMenuRowPadding());
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(padding,padding));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize,1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,0.0f);
    ImGui::PushStyleColor(ImGuiCol_Border,ImVec4(1,1,1,1));
    ImGui::BeginChild("ModMetadata",ImVec2(width,metadata_height),ImGuiChildFlags_Borders,ImGuiWindowFlags_NoSavedSettings);
    int selected=RecompMenuModSelected();
    const RecompModMetadata *info=RecompMenuModMetadata(selected);
    std::string name="Name: ";if(info->name)name+=info->name;
    small_text(name.c_str(),ImGui::GetColorU32(ImGuiCol_Text));
    std::string author="Author: ";if(info->author)author+=info->author;
    std::string version="Version: ";if(info->version)version+=info->version;
    small_text(author.c_str(),ImGui::GetColorU32(ImGuiCol_Text));
    small_text(version.c_str(),ImGui::GetColorU32(ImGuiCol_Text));
    small_text("Description:",ImGui::GetColorU32(ImGuiCol_Text));
    if(info->description)small_text(info->description,ImGui::GetColorU32(ImGuiCol_Text));
    ImGui::EndChild();ImGui::PopStyleColor();ImGui::PopStyleVar(3);
}
static int draw_file() {
    int command=0;
    if(reset_sections) {
        ImGui::SetScrollY(0);
        ImGui::GetStateStorage()->SetBool(ImGui::GetID("Save"),false);
        ImGui::GetStateStorage()->SetBool(ImGui::GetID("Load"),false);
        reset_sections=false;
    }
    if(menu_button("Select ROM..."))command=RECOMP_MENU_ROM;
    focus_first_item();
    for(int load=0;load<2;++load) {
        const char *name=load?"Load":"Save";
        ImGuiID id=ImGui::GetID(name);
        ImGuiStorage *storage=ImGui::GetStateStorage();
        bool open=storage->GetBool(id);
        if(RecompMenuDropdown(name,&open,focus_first))storage->SetBool(id,open);
        if(!open) continue;
        ImGui::PushID(load);
        for(int i=0;i<10;++i) {
            char label[224],shortcut[128];RecompMenuShortcut(load!=0,i,shortcut,sizeof(shortcut));
            std::snprintf(label,sizeof(label),"Slot %d%s%s##slot%d",i+1,*shortcut?"    ":"",shortcut,i);
            if(RecompMenuButton(label,false,true)) command=(load?RECOMP_MENU_LOAD:RECOMP_MENU_SAVE)+i;
        }
        ImGui::PopID();
    }
    if(menu_button("Save to file..."))command=RECOMP_MENU_SAVE_FILE;
    if(menu_button("Load save from file..."))command=RECOMP_MENU_FILE;
    if(menu_button("Reboot"))command=RECOMP_MENU_RESET;
    if(menu_button("Quit game"))command=RECOMP_MENU_QUIT;
    return command;
}
void RecompMenuDraw(SDL_Renderer *renderer,bool popup) {
    if(!menu_window) return;
    if(!menu_renderer) {
        ImGui_ImplSDL3_InitForSDLRenderer(menu_window,renderer);
        ImGui_ImplSDLRenderer3_Init(renderer);menu_renderer=renderer;
        RecompMenuUiGamepadsChanged();
    }
    RecompMenuIconsInit(renderer);
    if(!popup) {
        RecompMenuUiSync();
        ImGui_ImplSDLRenderer3_NewFrame();ImGui_ImplSDL3_NewFrame();
        ImGuiIO &io=ImGui::GetIO();
        int physical_w=0,physical_h=0;
        if(RecompMenuSoftwareBackbufferSize(renderer,&physical_w,&physical_h)) {
            io.DisplaySize=ImVec2((float)physical_w,(float)physical_h);
            io.DisplayFramebufferScale=ImVec2(1,1);
            int logical_w=0,logical_h=0;SDL_GetWindowSize(menu_window,&logical_w,&logical_h);
            float mouse_x=0,mouse_y=0;SDL_GetMouseState(&mouse_x,&mouse_y);
            if(logical_w>0 && logical_h>0)io.MousePos=ImVec2(mouse_x*physical_w/logical_w,mouse_y*physical_h/logical_h);
        }
        fit_menu_font(io,dpi);
        bool controls_blocked=RecompControlsInputBlocked() || RecompHotkeysInputBlocked();
        if(controls_blocked) {
            // The backend polls held gamepad buttons every NewFrame; prevent
            // that second input path from navigating behind the capture modal.
            io.ConfigFlags&=~(ImGuiConfigFlags_NavEnableKeyboard|ImGuiConfigFlags_NavEnableGamepad);
            RecompMenuUiSuppressInput();
        } else io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard|ImGuiConfigFlags_NavEnableGamepad;
        if(RecompMenuAltOwnsInput()) RecompMenuUiSuppressInput();
        if(!SDL_CursorVisible() || mouse_dormant || mouse_focus_locked || controls_blocked)io.ConfigFlags|=ImGuiConfigFlags_NoMouse;
        else io.ConfigFlags&=~ImGuiConfigFlags_NoMouse;
#ifdef _WIN32
        if(mouse_focus_locked)suppress_locked_mouse();
#endif
        ImGui::NewFrame();frame_started=true;
        RecompMenuBeginFocusFrame(shown,mouse_activity);mouse_activity=false;
        if(controls_blocked && !RecompControlsActive() && !RecompHotkeysActive())ImGui::SetNavCursorVisible(true);
        if(!shown) return;
        if(focus_settle) {
            // TabBar selection completes on subsequent frames. Keep the initial
            // row through that handoff, but immediately yield to explicit input.
            bool navigating=ImGui::IsKeyPressed(ImGuiKey_UpArrow)||ImGui::IsKeyPressed(ImGuiKey_DownArrow)||
                ImGui::IsKeyPressed(ImGuiKey_LeftArrow)||ImGui::IsKeyPressed(ImGuiKey_RightArrow)||ImGui::IsKeyPressed(ImGuiKey_Home)||
                ImGui::IsKeyPressed(ImGuiKey_GamepadDpadUp)||ImGui::IsKeyPressed(ImGuiKey_GamepadDpadDown)||
                ImGui::IsKeyPressed(ImGuiKey_Enter)||ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown)||
                io.MouseClicked[0]||io.MouseDelta.x!=0||io.MouseDelta.y!=0;
            if(navigating)focus_settle=0;
            else {focus_first=true;--focus_settle;}
        }
        ImVec2 size(io.DisplaySize.y*0.90f*4.0f/3.0f,io.DisplaySize.y*0.90f);
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x/2,io.DisplaySize.y/2),ImGuiCond_Always,ImVec2(0.5f,0.5f));
        ImGui::SetNextWindowSize(size,ImGuiCond_Always);ImGui::SetNextWindowBgAlpha(0.90f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(0,0,0,1));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0.0f);
        int command=0;
        // Always reserve the native scrollbar channel. Draw its indicator below
        // only for overflowing content, including Hotkeys' independently scrolling child.
        for(ImGuiCol color:{ImGuiCol_ScrollbarBg,ImGuiCol_ScrollbarGrab,ImGuiCol_ScrollbarGrabHovered,ImGuiCol_ScrollbarGrabActive})
            ImGui::PushStyleColor(color,ImVec4(0,0,0,0));
        bool menu_contents=ImGui::Begin(RecompMenuServices()->window_id,nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_AlwaysVerticalScrollbar);
        ImGui::PopStyleColor(4);menu_scroll_target=nullptr;
        if(menu_contents) {
            if(focus_first) ImGui::SetWindowFocus();
            bool select=tab_delta!=0;
            if(select) { active_tab=(active_tab+tab_delta%menu_tab_count+menu_tab_count)%menu_tab_count;tab_delta=0;focus_first=true; }
            int selected_tab=active_tab;
            float content_x=ImGui::GetCursorPosX();
            float tabs_width=(menu_tab_count-1)*ImGui::GetStyle().ItemInnerSpacing.x;
            for(const char *tab:menu_tabs) tabs_width+=ImGui::TabItemCalcSize(tab,false).x;
            ImGui::SetCursorPosX((ImGui::GetWindowWidth()-ImGui::GetStyle().ScrollbarSize-tabs_width)/2);
            ImGuiID open_id=0;
            if(ImGui::BeginTabBar("Sections",ImGuiTabBarFlags_NoCloseWithMiddleMouseButton|ImGuiTabBarFlags_FittingPolicyScroll|ImGuiTabBarFlags_NoTabListScrollingButtons)) {
                for(int i=0;i<menu_tab_count;++i) if(ImGui::BeginTabItem(menu_tabs[i],nullptr,select && i==selected_tab?ImGuiTabItemFlags_SetSelected:0)) {
                    open_id=ImGui::GetItemID();
                    if(active_tab!=i)focus_first=true;
                    active_tab=i;ImGui::EndTabItem();
                }
                const ImRect &bar=ImGui::GetCurrentTabBar()->BarRect;
                ImVec2 pos=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();
                ImDrawList *draw=ImGui::GetForegroundDrawList();
                draw->PushClipRect(pos,ImVec2(pos.x+size.x,pos.y+size.y),false);
                draw->AddRectFilled(ImVec2(pos.x,bar.Max.y),ImVec2(pos.x+size.x,bar.Max.y+1),ImGui::GetColorU32(ImGuiCol_Border));
                draw->PopClipRect();
                ImGuiTabBar *tab_bar=ImGui::GetCurrentTabBar();
                if(tab_bar->Tabs.Size) {
                    const ImGuiTabItem &first=tab_bar->Tabs.front(),&last=tab_bar->Tabs.back();
                    ImRect labels(ImVec2(bar.Min.x+first.Offset,bar.Min.y),ImVec2(bar.Min.x+last.Offset+last.Width,bar.Max.y));
                    RecompDrawTabHints(labels,pos,ImVec2(pos.x+size.x,pos.y+size.y));
                }
                ImGui::EndTabBar();
            }
            ImGuiWindow *hud=ImGui::GetCurrentWindow();
            ImGui::SetCursorPosX(content_x);
            // The parent reserves the same right channel as this scrolling
            // child. Only MenuContent moves; tab labels stay in window space.
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
            for(ImGuiCol color:{ImGuiCol_ScrollbarBg,ImGuiCol_ScrollbarGrab,ImGuiCol_ScrollbarGrabHovered,ImGuiCol_ScrollbarGrabActive})
                ImGui::PushStyleColor(color,ImVec4(0,0,0,0));
            ImGui::BeginChild("MenuContent",ImVec2(ImGui::GetContentRegionAvail().x+ImGui::GetStyle().ScrollbarSize,ImGui::GetContentRegionAvail().y),
                ImGuiChildFlags_None,ImGuiWindowFlags_AlwaysVerticalScrollbar|ImGuiWindowFlags_NoSavedSettings);
            ImGui::PopStyleColor(4);
            ImGui::PopStyleVar();
            if(focus_first)ImGui::SetScrollY(0);
            ImGui::PushOverrideID(open_id);
            int i=active_tab;
            if(i==0)command=draw_file();else if(i==1)draw_game();else if(i==2)draw_graphics();else if(i==3)draw_audio();else if(i==4)draw_controls();else if(i==5)draw_hotkeys();else if(i==6)draw_mods();else if(i==7)draw_shaders();
            ImGui::PopID();
            ImGuiWindow *content=ImGui::GetCurrentWindow();
            float extent=content->DC.CursorMaxPos.y-content->DC.CursorStartPos.y;
            if(extent>content->InnerRect.GetHeight()+1) {
                // BeginChild already submits the native scrollbar and owns its
                // input ID. Paint its visible track/grab without a second widget.
                ImRect track=ImGui::GetWindowScrollbarRect(content,ImGuiAxis_Y),grab=track;
                const ImGuiStyle &style=ImGui::GetStyle();
                grab.Expand(ImVec2(-ImClamp(std::floor((track.GetWidth()-2)*0.5f),0.0f,3.0f),
                                  -ImClamp(std::floor((track.GetHeight()-2)*0.5f),0.0f,3.0f)));
                float length=grab.GetHeight(),visible=content->InnerRect.GetHeight();
                float thumb=ImClamp(length*visible/extent,style.GrabMinSize,length);
                float ratio=ImClamp(content->Scroll.y/std::max(1.0f,extent-visible),0.0f,1.0f);
                grab.Min.y+=ratio*(length-thumb);grab.Max.y=grab.Min.y+thumb;
                ImGuiID id=ImGui::GetWindowScrollbarID(content,ImGuiAxis_Y);
                ImGuiContext &ctx=*ImGui::GetCurrentContext();
                ImGuiCol color=ctx.ActiveId==id?ImGuiCol_ScrollbarGrabActive:
                               ctx.HoveredId==id?ImGuiCol_ScrollbarGrabHovered:ImGuiCol_ScrollbarGrab;
                ImDrawList *draw=content->DrawList;
                draw->PushClipRect(content->Pos,ImVec2(content->Pos.x+content->Size.x,content->Pos.y+content->Size.y),false);
                draw->AddRectFilled(track.Min,track.Max,ImGui::GetColorU32(ImGuiCol_ScrollbarBg),style.ScrollbarRounding);
                draw->AddRectFilled(grab.Min,grab.Max,ImGui::GetColorU32(color),style.ScrollbarRounding);
                draw->PopClipRect();
            }
            ImGui::EndChild();
            if(!controls_blocked && !RecompMenuAltOwnsInput()) {
                ImGuiTabBar *bar=ImGui::GetCurrentContext()->TabBars.GetByKey(hud->GetID("Sections"));
                if(bar)for(const ImGuiTabItem &tab:bar->Tabs)if(tab.ID==open_id) {
                    ImRect rect(ImVec2(bar->BarRect.Min.x+tab.Offset,bar->BarRect.Min.y),ImVec2(bar->BarRect.Min.x+tab.Offset+tab.Width,bar->BarRect.Max.y));
                    RecompMenuWrapVertical(tab.ID,rect);break;
                }
            }

        }
        ImGui::End();ImGui::PopStyleVar();ImGui::PopStyleColor();
        if(command && dismiss(command!=RECOMP_MENU_QUIT)) {
            if(command==RECOMP_MENU_QUIT || !RecompMenuColdBootPending())RecompMenuQueueCommand(command);
        }
    } else if(frame_started) {
        int w,h;SDL_GetRenderOutputSize(renderer,&w,&h);
        if(shown && (capture_pending || w!=blur_width || h!=blur_height)) capture_blur(renderer);
        if(shown && blur_texture) SDL_RenderTexture(renderer,blur_texture,nullptr,nullptr);
        ImGui::Render();
        // ImGui vertices are in window coordinates; SDL must scale geometry as
        // well as clip rectangles on HiDPI Linux/Wayland displays.
        float sx=1,sy=1;SDL_GetRenderScale(renderer,&sx,&sy);
        ImVec2 pixels=ImGui::GetIO().DisplayFramebufferScale;
        SDL_SetRenderScale(renderer,pixels.x>0?pixels.x:1,pixels.y>0?pixels.y:1);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(),renderer);
        SDL_SetRenderScale(renderer,sx,sy);
        RecompMenuIconsEndFrame();frame_started=false;
    }
}
