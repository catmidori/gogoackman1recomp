#include "ackman_menu.h"
#include "ackman_menu_icons.h"
#include "ackman_devices.h"
#include "ackman_translation.h"
#include "ackman_controls.h"
#include "ackman_hotkeys.h"
#include "ackman_config.h"
#include "keybinds.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "snes_osd.h"
#include "ackman_menu_widgets.h"
#include "ackman_cursor.h"
extern "C" {
#include "desktop/config.h"
void TestGlobalMousePosition(bool override,float x,float y);
}
#include <cassert>
#include <cstring>
static ImGuiWindow *menu_content() {
    for(ImGuiWindow *w:ImGui::GetCurrentContext()->Windows)
        if(std::strstr(w->Name,"Ackman HUD/MenuContent_"))return w;
    assert(false);return nullptr;
}
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <algorithm>
#include <vector>
extern "C" void TestDisplayScale(float scale);
extern "C" void TestCancelSaveDialog(void);
extern "C" void TestImGuiInteraction(SDL_Renderer *renderer) {
    // Parse/rasterize every embedded SVG, without reading an asset directory.
    size_t svg_count;const AckmanSvgAsset *svgs=AckmanButtonAssets(&svg_count);assert(svg_count>=130);
    for(size_t i=0;i<svg_count;i++) {
        SDL_Surface *icon=AckmanRasterizeSvg(svgs[i].data,svgs[i].size,48);assert(icon && icon->h==48);
        bool visible=false,transparent=false;
        for(int y=0;y<icon->h;y++)for(int x=0;x<icon->w;x++) {
            Uint8 r,g,b,a;assert(SDL_ReadSurfacePixel(icon,x,y,&r,&g,&b,&a));visible|=a>0;transparent|=a==0;
        }
        assert(visible && transparent);SDL_DestroySurface(icon);
    }
    assert(!AckmanButtonIcon("",false) && !AckmanButtonIcon("None",true));
    assert(!AckmanButtonIcon("Joystick Button 7",true));
    const std::pair<const char *,const char *> expected[]={
        {"Gamepad a","xbox_button_a_outline.svg"},{"Gamepad guide","xbox_button_menu_outline.svg"},
        {"dpup","xbox_dpad_up_outline.svg"},{"dpdown","xbox_dpad_down_outline.svg"},
        {"dpleft","xbox_dpad_left_outline.svg"},{"dpright","xbox_dpad_right_outline.svg"},
        {"b","xbox_button_b_outline.svg"},{"x","xbox_button_x_outline.svg"},{"y","xbox_button_y_outline.svg"},
        {"leftshoulder","xbox_lb_outline.svg"},{"rightshoulder","xbox_rb_outline.svg"},
        {"back","xbox_button_back_icon_outline.svg"},{"start","xbox_button_start_icon_outline.svg"},
        {"Axis lefttrigger +","xbox_lt_outline.svg"},{"Axis righttrigger +","xbox_rt_outline.svg"},
        {"leftstick","xbox_stick_side_l.svg"},{"rightstick","xbox_stick_side_r.svg"},
        {"Axis lefty -","xbox_stick_l_up.svg"},{"Axis lefty +","xbox_stick_l_down.svg"},
        {"Axis leftx -","xbox_stick_l_left.svg"},{"Axis leftx +","xbox_stick_l_right.svg"},
        {"Axis righty -","xbox_stick_r_up.svg"},{"Axis righty +","xbox_stick_r_down.svg"},
        {"Axis rightx -","xbox_stick_r_left.svg"},{"Axis rightx +","xbox_stick_r_right.svg"}};
    for(auto &pair:expected)assert(!strcmp(AckmanButtonIcon(pair.first,true),pair.second));
    assert(!strcmp(AckmanButtonIcon("Down",false),"keyboard_arrow_down_outline.svg"));
    assert(!strcmp(AckmanButtonIcon("Right Shift",false),"keyboard_shift_outline.svg"));
    assert(!strcmp(AckmanButtonIcon("Mouse Left",false),"mouse_left_outline.svg"));
    assert(!strcmp(AckmanButtonIcon("mouse_right",false),"mouse_right_outline.svg"));
    assert(!strcmp(AckmanButtonIcon("f",false),"keyboard_f_outline.svg"));
    ImGuiIO &io=ImGui::GetIO();
    assert(io.ConfigFlags & ImGuiConfigFlags_NavEnableKeyboard);
    assert(io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad);
    SDL_Window *window=SDL_GetRenderWindow(renderer);
    SDL_Surface *pattern=SDL_CreateSurface(640,480,SDL_PIXELFORMAT_RGBA32);assert(pattern);
    for(int y=0;y<480;y++)for(int x=0;x<640;x++) {
        unsigned char *p=(unsigned char *)pattern->pixels+y*pattern->pitch+x*4;
        p[0]=p[1]=p[2]=((x/4+y/4)&1)?255:0;p[3]=255;
    }
    SDL_Texture *background=SDL_CreateTextureFromSurface(renderer,pattern);assert(background);SDL_DestroySurface(pattern);
    bool probe=false,probe_checked=false;
    int probe_flags=1;
    ImU64 probe_flags64=1;
    ImRect probe_rows[3];
    const char *aspect_options[]={"4:3 CRT","8:7 Square Pixels"};
    const char *mode_options[]={"On","Off"};
    AckmanMenuRegisterSelectorOptions(mode_options,2);
    int probe_aspect=0,probe_mode=1;
    ImVec2 arrow_positions[2];

    auto frame=[&]() {
        AckmanCursorUpdate(nullptr); // Same idle poll performed by the host before drawing.
        SDL_SetRenderDrawColor(renderer,0,0,0,255);SDL_RenderClear(renderer);
        AckmanMenuDraw(renderer,false);
        SDL_RenderTexture(renderer,background,nullptr,nullptr);
        if(probe) {
            ImGui::SetNextWindowPos(ImVec2(0,0));ImGui::SetNextWindowSize(ImVec2(280,260));
            ImGui::Begin("Checkbox API test",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize);
            ImGui::Checkbox("Public checkbox",&probe_checked);
            probe_rows[0]=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());
            ImGui::CheckboxFlags("Public flags",&probe_flags,3);
            probe_rows[1]=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());
            ImGui::CheckboxFlags("Public 64-bit flags",&probe_flags64,ImU64(3));
            probe_rows[2]=ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax());
            for(int i=0;i<2;i++) {
                ImDrawList *draw=ImGui::GetWindowDrawList();
                int first=draw->VtxBuffer.Size;
                AckmanMenuSelector(i?"Mode":"Aspect",i?mode_options:aspect_options,2,i?&probe_mode:&probe_aspect);
                // Inspect actual glyph quads, rather than repeating the layout formula.
                const ImFontGlyph *left=ImGui::GetFont()->FindGlyph('<');
                const ImFontGlyph *right=ImGui::GetFont()->FindGlyph('>');
                bool found_left=false,found_right=false;
                for(int v=first;v<draw->VtxBuffer.Size;v++) {
                    const ImDrawVert &vertex=draw->VtxBuffer[v];
                    if(vertex.uv.x==left->U0 && vertex.uv.y==left->V0) {
                        arrow_positions[i].x=vertex.pos.x;found_left=true;
                    }
                    if(vertex.uv.x==right->U0 && vertex.uv.y==right->V0) {
                        arrow_positions[i].y=vertex.pos.x;found_right=true;
                    }
                }
                assert(found_left && found_right);
            }
            assert(arrow_positions[0].x==arrow_positions[1].x);
            assert(arrow_positions[0].y==arrow_positions[1].y);
            ImGui::End();
        }
        AckmanMenuDraw(renderer,true);
        assert(ImGui::GetCurrentContext()->DebugDrawIdConflicts==0);
    };
    auto key=[&](SDL_Keycode code) {
        SDL_Event event={};event.type=SDL_EVENT_KEY_DOWN;event.key.key=code;
        event.key.windowID=SDL_GetWindowID(window);event.key.scancode=SDL_GetScancodeFromKey(code,nullptr);
        bool consumed=AckmanMenuEvent(&event);if(!consumed)fprintf(stderr,"unconsumed key=%s visible=%d\n",SDL_GetKeyName(code),AckmanMenuVisible());assert(consumed);frame();
        event.type=SDL_EVENT_KEY_UP;bool released=AckmanMenuEvent(&event);assert(released || !AckmanMenuVisible());frame();frame();
    };
    auto click=[&](float x,float y) {
        SDL_WarpMouseInWindow(window,x,y);
        SDL_Event mouse_event={};mouse_event.type=SDL_EVENT_MOUSE_MOTION;
        mouse_event.motion.windowID=SDL_GetWindowID(window);mouse_event.motion.x=x;mouse_event.motion.y=y;
        mouse_event.motion.xrel=1; // A deliberate test mouse move, not a resize hover.
        AckmanMenuEvent(&mouse_event);frame();
        mouse_event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;mouse_event.button.windowID=SDL_GetWindowID(window);
        mouse_event.button.button=SDL_BUTTON_LEFT;mouse_event.button.down=true;mouse_event.button.x=x;mouse_event.button.y=y;
        AckmanMenuEvent(&mouse_event);frame();
        mouse_event.type=SDL_EVENT_MOUSE_BUTTON_UP;mouse_event.button.down=false;AckmanMenuEvent(&mouse_event);frame();frame();
    };
    frame();assert(!AckmanMenuVisible() && AckmanMenuHeight(480)==0);
    assert(SDL_CursorVisible());
    SDL_Delay(2010);frame();assert(!SDL_CursorVisible()); // Gameplay idle.
    SDL_Event mouse={};mouse.type=SDL_EVENT_MOUSE_MOTION;
    AckmanMenuEvent(&mouse);assert(SDL_CursorVisible());
    // Menu is customizable, while Escape is an immutable concurrent shortcut.
    AckmanIniValue menu_binding={"KeyMap","Menu","F13"};
    assert(AckmanConfigUpdate(&menu_binding,1));ConfigReloadKeyMap(AckmanConfigPath());AckmanHotkeysInit();
    key(SDLK_F13);assert(AckmanMenuVisible());key(SDLK_F13);assert(!AckmanMenuVisible());
    SDL_Event old_menu={};old_menu.type=SDL_EVENT_KEY_DOWN;old_menu.key.key=SDLK_ESCAPE;
    AckmanMenuEvent(&old_menu);frame();assert(AckmanMenuVisible());
    old_menu.type=SDL_EVENT_KEY_UP;AckmanMenuEvent(&old_menu);frame();
    key(SDLK_ESCAPE);assert(!AckmanMenuVisible());
    menu_binding.value="None";assert(AckmanConfigUpdate(&menu_binding,1));ConfigReloadKeyMap(AckmanConfigPath());AckmanHotkeysInit();
    key(SDLK_ESCAPE);assert(AckmanMenuVisible());key(SDLK_ESCAPE);assert(!AckmanMenuVisible());
    menu_binding.value="Escape";assert(AckmanConfigUpdate(&menu_binding,1));ConfigReloadKeyMap(AckmanConfigPath());AckmanHotkeysInit();
    key(SDLK_ESCAPE);assert(AckmanMenuVisible() && AckmanMenuOwnsInput());
    SDL_Delay(2010);frame();frame();assert(!SDL_CursorVisible()); // Backend must not show it each HUD frame.
    for(Uint32 type : {SDL_EVENT_MOUSE_MOTION,SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_EVENT_MOUSE_BUTTON_UP}) {
        assert(SDL_HideCursor());mouse.type=type;mouse.button.button=SDL_BUTTON_RIGHT;
        AckmanMenuEvent(&mouse);assert(SDL_CursorVisible());frame();assert(SDL_CursorVisible());
    }
    ImGuiWindow *hud=ImGui::FindWindowByName("Ackman HUD");assert(hud && hud->Active);
    assert(std::abs(hud->Size.y-io.DisplaySize.y*0.9f)<1.1f);
    assert(std::abs(hud->Size.x/hud->Size.y-4.0f/3.0f)<0.01f);
    assert(std::abs(hud->Pos.x+hud->Size.x/2-io.DisplaySize.x/2)<1.1f);
    assert(std::abs(hud->Pos.y+hud->Size.y/2-io.DisplaySize.y/2)<1.1f);
    ImGuiTabBar *bar=ImGui::GetCurrentContext()->TabBars.GetByKey(hud->GetID("Sections"));assert(bar);
    ImGuiID file_tab=bar->SelectedTabId;
    auto check_file_focus=[&]() {
        assert(ImGui::GetCurrentContext()->NavId==ImHashStr("Select ROM...",0,file_tab));
        assert(ImGui::GetCurrentContext()->NavCursorVisible);
        ImGuiContext *ctx=ImGui::GetCurrentContext();
        ImRect row=ImGui::WindowRectRelToAbs(ctx->NavWindow,ctx->NavWindow->NavRectRel[ImGuiNavLayer_Main]);
        assert(std::abs(row.Min.x-hud->WorkRect.Min.x)<1.1f);
        assert(std::abs(row.Max.x-std::floor(hud->WorkRect.Max.x))<0.1f);
        SDL_Surface *pixels=SDL_RenderReadPixels(renderer,nullptr);assert(pixels);
        Uint8 r,g,b,a;
        assert(SDL_ReadSurfacePixel(pixels,(int)(row.Min.x+row.GetWidth()*0.15f),(int)row.GetCenter().y,&r,&g,&b,&a));
        assert(r>35 && r==g && g==b); // Focus includes a filled gray background on the first item.
        // Only one outline, inside the row; no expanded navigation rectangle.
        Uint8 edge,eg,eb,ea;
        int x=(int)(row.Min.x+row.GetWidth()*0.15f), y=(int)row.Min.y;
        assert(SDL_ReadSurfacePixel(pixels,x,y,&edge,&eg,&eb,&ea));assert(edge>r);
        assert(SDL_ReadSurfacePixel(pixels,x,y+1,&edge,&eg,&eb,&ea));assert(edge==r);
        assert(SDL_ReadSurfacePixel(pixels,x,y-2,&edge,&eg,&eb,&ea));assert(edge<r);
        assert(SDL_SaveBMP(pixels,"menu-file-initial-focus.bmp"));SDL_DestroySurface(pixels);
    };
    check_file_focus();
    auto check_style_and_fit=[&]() {
        // Inspect actual textured quads: two equal-sized icons per hint group.
        std::vector<ImRect> hints;
        for(const ImDrawCmd &cmd:hud->DrawList->CmdBuffer)if(cmd.ElemCount && cmd.GetTexID()!=io.Fonts->TexID) {
            ImRect box(ImVec2(FLT_MAX,FLT_MAX),ImVec2(-FLT_MAX,-FLT_MAX));
            for(unsigned int i=0;i<cmd.ElemCount;i++)box.Add(hud->DrawList->VtxBuffer[cmd.VtxOffset+hud->DrawList->IdxBuffer[cmd.IdxOffset+i]].pos);
            hints.push_back(box);
        }
        assert(hints.size()==4);
        float first=bar->BarRect.Min.x+bar->Tabs.front().Offset;
        float last=bar->BarRect.Min.x+bar->Tabs.back().Offset+bar->Tabs.back().Width;
        for(const ImRect &hint:hints) {
            assert(std::abs(hint.GetHeight()-hints.front().GetHeight())<0.1f);
            assert(hint.Min.y==hints.front().Min.y && hint.Max.y==hints.front().Max.y);
            assert(std::abs(hint.GetHeight()-std::floor(hint.GetHeight()))<0.1f);
            assert(hint.Min.y>hud->Pos.y && hint.Max.y<bar->BarRect.Max.y);
        }
        assert(hints[0].GetWidth()<hints[1].GetWidth() && hints[2].GetWidth()<hints[3].GetWidth());
        assert(hints[1].Max.x<first && hints[2].Min.x>last);
        assert(std::abs((hints[0].Min.x+hints[1].Max.x)/2-(hud->Pos.x+first)/2)<0.1f);
        assert(std::abs((hints[2].Min.x+hints[3].Max.x)/2-(last+hud->Pos.x+hud->Size.x)/2)<0.1f);
        const ImGuiStyle &style=ImGui::GetStyle();
        for(ImGuiCol color : {ImGuiCol_Tab,ImGuiCol_TabDimmed,ImGuiCol_Button,ImGuiCol_Header})
            assert(style.Colors[color].w==0);
        assert(style.Colors[ImGuiCol_Border].x==1 && style.Colors[ImGuiCol_Border].y==1 && style.Colors[ImGuiCol_Border].z==1);
        for(ImGuiCol color : {ImGuiCol_TabSelected,ImGuiCol_TabHovered,ImGuiCol_HeaderHovered,ImGuiCol_HeaderActive,ImGuiCol_NavCursor}) {
            assert(style.Colors[color].x==style.Colors[color].y && style.Colors[color].y==style.Colors[color].z);
        }
        assert(style.Colors[ImGuiCol_TabSelected].w==style.Colors[ImGuiCol_HeaderHovered].w);
        assert(style.ButtonTextAlign.x==0.5f && style.SelectableTextAlign.x==0.5f);
        if(bar->SelectedTabId==file_tab) {
            ImGuiContext *ctx=ImGui::GetCurrentContext();
            ImRect row=ImGui::WindowRectRelToAbs(ctx->NavWindow,ctx->NavWindow->NavRectRel[ImGuiNavLayer_Main]);
            assert(std::abs(row.Min.x-hud->WorkRect.Min.x)<1.1f);
            assert(std::abs(row.Max.x-std::floor(hud->WorkRect.Max.x))<0.1f);
        }
        assert(std::abs(ImGui::GetFontSize()-io.FontDefault->FontSize*io.FontGlobalScale)<0.01f);
        assert(bar->Tabs.Size==8);
        const char *labels[]={"File","Game","Graphics","Audio","Controls","Hotkeys","Mods","Shaders"};
        float total=0;
        for(int i=0;i<8;++i) {
            // Each entire label must fit its actual tab rectangle, not an ellipsis.
            assert(bar->Tabs[i].Width+0.1f>=ImGui::CalcTextSize(labels[i]).x+2*style.FramePadding.x);
            total+=bar->Tabs[i].Width;
        }
        assert(total+7*style.ItemInnerSpacing.x<=bar->BarRect.GetWidth()+0.1f);
        float tabs_center=bar->BarRect.Min.x+(bar->Tabs[0].Offset+bar->Tabs[7].Offset+bar->Tabs[7].Width)/2;
        assert(std::abs(tabs_center-hud->Pos.x-(hud->Size.x-style.ScrollbarSize)/2)<1.1f);
    };
    check_style_and_fit();
    SDL_Event chord={};chord.type=SDL_EVENT_KEY_DOWN;chord.key.key=SDLK_LALT;
    SDL_SetModState(SDL_KMOD_LALT);assert(AckmanMenuEvent(&chord));frame();
    chord.key.key=SDLK_RETURN;chord.key.scancode=SDL_SCANCODE_RETURN;
    assert(!AckmanMenuEvent(&chord));frame();frame();
    assert(AckmanMenuVisible() && !ImGui::GetIO().KeysData[ImGuiKey_Enter-ImGuiKey_NamedKey_BEGIN].Down);
    assert(ImGui::GetCurrentContext()->NavActivateId==0);
    chord.key.key=SDLK_x;chord.key.scancode=SDL_SCANCODE_X;assert(AckmanMenuEvent(&chord));
    chord.type=SDL_EVENT_KEY_UP;assert(AckmanMenuEvent(&chord));
    SDL_SetModState(0);chord.key.key=SDLK_LALT;chord.key.scancode=SDL_SCANCODE_LALT;
    assert(AckmanMenuEvent(&chord) && AckmanMenuAltOwnsInput());frame();
    chord.key.key=SDLK_RETURN;chord.key.scancode=SDL_SCANCODE_RETURN;chord.key.mod=0;
    assert(!AckmanMenuEvent(&chord));frame();assert(!AckmanMenuAltOwnsInput());
    key(SDLK_e);assert(bar->Tabs.Size==8 && bar->SelectedTabId!=file_tab);
    ImGuiID game_tab=bar->SelectedTabId;
    ImGuiID game_table=ImHashStr("GameOptions",0,game_tab);
    assert(ImGui::GetCurrentContext()->NavId==ImHashStr("Pause when inactive",0,game_table));
    assert(g_config.pause_when_inactive && !g_config.autosave && g_config.run_ahead==0);
    assert(std::abs(hud->WorkRect.Min.x-hud->Pos.x-hud->Size.x*0.05f)<1.1f);
    key(SDLK_RETURN);assert(!g_config.pause_when_inactive);
    key(SDLK_RETURN);assert(g_config.pause_when_inactive);
    key(SDLK_DOWN);key(SDLK_RETURN);assert(g_config.autosave);
    key(SDLK_RETURN);assert(!g_config.autosave);
    key(SDLK_DOWN);assert(g_config.controller_vibration);
    key(SDLK_RETURN);assert(!g_config.controller_vibration);
    key(SDLK_RETURN);assert(g_config.controller_vibration);
    key(SDLK_DOWN);key(SDLK_LEFT);assert(g_config.run_ahead==AckmanMenuRunAheadMax());
    key(SDLK_RIGHT);assert(g_config.run_ahead==0);
    key(SDLK_RIGHT);assert(g_config.run_ahead==1);
    key(SDLK_LEFT);assert(g_config.run_ahead==0);
    ImGuiID last_game=ImHashStr("Run ahead",0,game_table);
    key(SDLK_DOWN);assert(ImGui::GetCurrentContext()->NavId==game_tab);
    key(SDLK_UP);assert(ImGui::GetCurrentContext()->NavId==last_game);
    key(SDLK_HOME);assert(ImGui::GetCurrentContext()->NavId==game_tab);
    key(SDLK_UP);assert(ImGui::GetCurrentContext()->NavId==last_game);
    key(SDLK_DOWN);assert(ImGui::GetCurrentContext()->NavId==game_tab);
    key(SDLK_DOWN);assert(ImGui::GetCurrentContext()->NavId==ImHashStr("Pause when inactive",0,game_table));
    key(SDLK_e);
    ImGuiID graphics_tab=bar->SelectedTabId;
    auto check_graphics=[&]() {
        ImGuiContext *ctx=ImGui::GetCurrentContext();
        ImGuiID table_id=ImHashStr("GraphicsOptions",0,graphics_tab);
        ImGuiTable *table=ctx->Tables.GetByKey(table_id);assert(table && table->ColumnsCount==2);
        assert(!(table->Flags&(ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg)));
        float left=hud->WorkRect.Min.x,right=std::floor(hud->WorkRect.Max.x);
        assert(std::abs(table->OuterRect.Min.x-left)<1.1f);
        assert(std::abs(table->OuterRect.Max.x-hud->WorkRect.Max.x)<1.1f);
        assert(ctx->NavId==ImHashStr("Show FPS",0,table_id) && ctx->NavCursorVisible);
        ImRect row=ImGui::WindowRectRelToAbs(ctx->NavWindow,ctx->NavWindow->NavRectRel[ImGuiNavLayer_Main]);
        assert(std::abs(row.Min.x-left)<1.1f && std::abs(row.Max.x-right)<0.1f);
        float pad=AckmanMenuRowPadding();
        float side=std::max(5.0f,std::floor(ImGui::GetFrameHeight()-2*std::max(2.0f,std::ceil(ImGui::GetStyle().FramePadding.y))));
        float box_x=std::floor(row.Max.x-pad-side);
        float box_y=std::round(row.Min.y+(row.GetHeight()-side)/2);
        ImRect box(ImVec2(box_x,box_y),ImVec2(box_x+side,box_y+side));
        assert(box.Min.y-row.Min.y>=1.5f && row.Max.y-box.Max.y>=1.5f);
        assert(std::abs(box.GetCenter().y-row.GetCenter().y)<=0.51f);
        assert(row.Max.x-box.Max.x>=ImGui::CalcTextSize(" ").x);
        assert(row.Max.x-box.Max.x<pad+1.1f);
        SDL_Surface *pixels=SDL_RenderReadPixels(renderer,nullptr);assert(pixels);
        Uint8 red,green,blue,alpha;
        Uint8 fill,fg,fb,fa;
        assert(SDL_ReadSurfacePixel(pixels,(int)row.GetCenter().x,(int)row.GetCenter().y,&fill,&fg,&fb,&fa));
        assert(fill>35 && fill==fg && fg==fb); // Label and selector share the entire row highlight.
        for(int x : {(int)std::ceil(row.Min.x),(int)row.Max.x-1}) {
            assert(SDL_ReadSurfacePixel(pixels,x,(int)row.GetCenter().y,&red,&green,&blue,&alpha));
            assert(red>fill && red==green && green==blue); // Both full-row borders remain visible.
        }
        assert(SDL_ReadSurfacePixel(pixels,(int)box.GetCenter().x,(int)box.GetCenter().y,&red,&green,&blue,&alpha));
        assert(snes_osd_fps_visible()?(red==255 && green==255 && blue==255):(red==fill && green==fg && blue==fb));
        assert(SDL_ReadSurfacePixel(pixels,(int)box.Min.x,(int)box.GetCenter().y,&red,&green,&blue,&alpha));
        assert(red==255 && green==255 && blue==255);
        assert(SDL_ReadSurfacePixel(pixels,(int)box.Min.x-2,(int)box.GetCenter().y,&red,&green,&blue,&alpha));
        assert(red==fill && green==fg && blue==fb); // No second outline around the square.
        assert(SDL_ReadSurfacePixel(pixels,(int)box.Max.x-1,(int)box.GetCenter().y,&red,&green,&blue,&alpha));
        assert(red==255 && green==255 && blue==255); // Right wall survives table clipping.
        for(int y : {(int)box.Min.y,(int)box.Max.y-1}) {
            assert(SDL_ReadSurfacePixel(pixels,(int)box.GetCenter().x,y,&red,&green,&blue,&alpha));
            assert(red==255 && green==255 && blue==255);
        }
        for(int y : {(int)box.Min.y-1,(int)box.Max.y}) {
            assert(SDL_ReadSurfacePixel(pixels,(int)box.GetCenter().x,y,&red,&green,&blue,&alpha));
            assert(red==fill && green==fg && blue==fb); // Gray breathing room above and below the square.
        }
        assert(SDL_ReadSurfacePixel(pixels,(int)row.Max.x-2,(int)box.GetCenter().y,&red,&green,&blue,&alpha));
        assert(red==fill && green==fg && blue==fb); // Visible breathing room after the checkbox.
        assert(SDL_ReadSurfacePixel(pixels,(int)row.Min.x+1,(int)box.GetCenter().y,&red,&green,&blue,&alpha));
        assert(red==fill && green==fg && blue==fb); // Text begins after the left inset.
        assert(SDL_SaveBMP(pixels,snes_osd_fps_visible()?"menu-graphics-checked.bmp":"menu-graphics-unchecked.bmp"));
        SDL_DestroySurface(pixels);
        return box;
    };
    check_graphics();
    auto check_selector=[&]() {
        ImGuiContext *ctx=ImGui::GetCurrentContext();
        ImGuiID table_id=ImHashStr("GraphicsOptions",0,graphics_tab);
        assert(ctx->NavId==ImHashStr("Display Aspect",0,table_id));
        ImRect row=ImGui::WindowRectRelToAbs(ctx->NavWindow,ctx->NavWindow->NavRectRel[ImGuiNavLayer_Main]);
        assert(std::abs(row.Min.x-hud->WorkRect.Min.x)<1.1f);
        assert(std::abs(row.Max.x-std::floor(hud->WorkRect.Max.x))<0.1f);
        const char *text=g_config.display_aspect==1?"< 8:7 Square Pixels >":"< 4:3 CRT >";
        assert(ImGui::CalcTextSize("Display Aspect").x+AckmanMenuSelectorWidth()+3*AckmanMenuRowPadding()<row.GetWidth());
        SDL_Surface *pixels=SDL_RenderReadPixels(renderer,nullptr);assert(pixels);
        Uint8 r,g,b,a,fill,fg,fb,fa;
        assert(SDL_ReadSurfacePixel(pixels,(int)row.GetCenter().x,(int)row.GetCenter().y,&fill,&fg,&fb,&fa));
        assert(fill>35 && fill==fg && fg==fb);
        for(int x : {(int)std::ceil(row.Min.x),(int)row.Max.x-1}) {
            assert(SDL_ReadSurfacePixel(pixels,x,(int)row.GetCenter().y,&r,&g,&b,&a));
            assert(r>fill && r==g && g==b);
        }
        assert(SDL_SaveBMP(pixels,g_config.display_aspect==1?"menu-aspect-square.bmp":"menu-aspect-crt.bmp"));
        SDL_DestroySurface(pixels);
        return row;
    };
    key(SDLK_DOWN);check_selector();
    key(SDLK_RIGHT);assert(g_config.display_aspect==1);check_selector();
    key(SDLK_RIGHT);assert(g_config.display_aspect==0);check_selector();
    key(SDLK_LEFT);assert(g_config.display_aspect==1);check_selector();
    key(SDLK_LEFT);assert(g_config.display_aspect==0);
    key(SDLK_RETURN);assert(g_config.display_aspect==1);check_selector();
    key(SDLK_RETURN);assert(g_config.display_aspect==0);check_selector();
    key(SDLK_DOWN);
#ifdef _WIN32
    TestGlobalMousePosition(true,100,100);
#endif
    key(SDLK_RETURN);assert(g_config.fullscreen==1);
#ifdef _WIN32
    assert(io.ConfigFlags&ImGuiConfigFlags_NoMouse);
#endif
    key(SDLK_RETURN);assert(g_config.fullscreen==0);
#ifdef _WIN32
    ImGuiID fullscreen_focus=ImGui::GetCurrentContext()->NavId;
    SDL_Event resized={};resized.type=SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED;AckmanMenuEvent(&resized);
    SDL_Event synthetic={};synthetic.type=SDL_EVENT_MOUSE_MOTION;
    synthetic.motion.x=10;synthetic.motion.y=10;AckmanMenuEvent(&synthetic);
    for(int i=0;i<5;++i) {
        // Simulate the independent backend polling path as well as SDL events.
        synthetic.motion.xrel=i?1:0;AckmanMenuEvent(&synthetic);
        io.AddMousePosEvent(10,10);io.AddMouseButtonEvent(0,true);frame();
        assert(io.ConfigFlags&ImGuiConfigFlags_NoMouse);
        assert(io.MousePos.x==-FLT_MAX && io.MousePos.y==-FLT_MAX);
        assert(!io.MouseDown[0]);
        assert(ImGui::GetCurrentContext()->NavId==fullscreen_focus);
    }
    // Window/compositor coordinate changes with zero motion cannot unlock it.
    TestGlobalMousePosition(true,102,100);
    synthetic.motion.xrel=0;AckmanMenuEvent(&synthetic);frame();
    assert(io.ConfigFlags&ImGuiConfigFlags_NoMouse);
    assert(ImGui::GetCurrentContext()->NavId==fullscreen_focus);
    synthetic.motion.xrel=2;AckmanMenuEvent(&synthetic);frame();
    assert(!(io.ConfigFlags&ImGuiConfigFlags_NoMouse));
    TestGlobalMousePosition(false,0,0);
#endif
    key(SDLK_DOWN);key(SDLK_RETURN);assert(!g_config.vsync);
    key(SDLK_RETURN);assert(g_config.vsync);
    key(SDLK_DOWN);key(SDLK_RIGHT);assert(g_config.output_method==kOutputMethod_OpenGL);
    key(SDLK_RIGHT);assert(g_config.output_method==kOutputMethod_SDLSoftware);
    key(SDLK_RIGHT);assert(g_config.output_method==kOutputMethod_SDL);
    key(SDLK_DOWN);key(SDLK_RETURN);assert(g_config.no_sprite_limits);
    key(SDLK_RETURN);assert(!g_config.no_sprite_limits);
    key(SDLK_DOWN);key(SDLK_RETURN);assert(!g_config.ignore_aspect_ratio);
    key(SDLK_RETURN);assert(g_config.ignore_aspect_ratio);
    key(SDLK_DOWN);key(SDLK_RIGHT);assert(g_config.window_width==1792 && g_config.window_height==896);
    key(SDLK_LEFT);assert(g_config.window_width==1344 && g_config.window_height==672);
    key(SDLK_DOWN);key(SDLK_LEFT);assert(g_config.window_scale==10);
    key(SDLK_RIGHT);assert(g_config.window_scale==1);
    for(int i=0;i<3;i++)key(SDLK_UP);
    for(int i=0;i<4;i++)key(SDLK_UP);
    ImRect selector=check_selector();
    click(selector.Max.x-AckmanMenuRowPadding()-ImGui::CalcTextSize(">").x/2,selector.GetCenter().y);
    assert(g_config.display_aspect==1);check_selector();
    float left_arrow=selector.Max.x-AckmanMenuRowPadding()-AckmanMenuSelectorWidth();
    click(left_arrow+ImGui::CalcTextSize("<").x/2,selector.GetCenter().y);
    assert(g_config.display_aspect==0);check_selector();
    key(SDLK_UP);check_graphics();
    key(SDLK_RETURN);assert(snes_osd_fps_visible());check_graphics();
    key(SDLK_f);assert(!snes_osd_fps_visible());check_graphics();
    ImRect checkbox=check_graphics();click(checkbox.GetCenter().x,checkbox.GetCenter().y);assert(snes_osd_fps_visible());
    key(SDLK_f);assert(!snes_osd_fps_visible());
    auto click_tab=[&](int index) {
        click(bar->BarRect.Min.x+bar->Tabs[index].Offset+bar->Tabs[index].Width/2,(bar->BarRect.Min.y+bar->BarRect.Max.y)/2);
    };
    click_tab(0);check_file_focus();click_tab(2);check_graphics();
    click_tab(3);
    assert(ImGui::GetCurrentContext()->NavId==ImHashStr("Enable audio",0,ImHashStr("AudioOptions",0,bar->SelectedTabId)));
    key(SDLK_RETURN);assert(!g_config.enable_audio);
    key(SDLK_RETURN);assert(g_config.enable_audio);
    key(SDLK_DOWN);key(SDLK_RIGHT);assert(g_config.volume==0);
    key(SDLK_LEFT);assert(g_config.volume==100);
    key(SDLK_DOWN);key(SDLK_RIGHT);assert(g_config.audio_freq==32000);
    key(SDLK_RIGHT);assert(g_config.audio_freq==44100);
    key(SDLK_RIGHT);assert(g_config.audio_freq==48000);
    assert(ImGui::CalcTextSize("Sample rate").x+AckmanMenuSelectorWidth()+3*AckmanMenuRowPadding()<hud->WorkRect.GetWidth());
    click_tab(2);check_graphics();
    for(int height : {240,360,480,720}) {
        assert(SDL_SetWindowSize(window,height*4/3,height));
        for(float scale : {1.0f,1.25f,2.0f}) {
            TestDisplayScale(scale);frame();frame();frame();check_graphics();
            key(SDLK_DOWN);key(SDLK_RIGHT);check_selector();
            key(SDLK_LEFT);check_selector();key(SDLK_UP);
        }
    }
    TestDisplayScale(1);assert(SDL_SetWindowSize(window,640,480));frame();frame();frame();
    key(SDLK_ESCAPE);key(SDLK_ESCAPE);assert(bar->SelectedTabId==file_tab);
    key(SDLK_e);assert(bar->SelectedTabId!=file_tab);
    SDL_Surface *image=SDL_RenderReadPixels(renderer,nullptr);assert(image);
    Uint8 r,g,b,a;assert(SDL_ReadSurfacePixel(image,16,16,&r,&g,&b,&a));assert(r>70 && r<190);
    assert(SDL_ReadSurfacePixel(image,320,240,&r,&g,&b,&a));assert(r<35 && g<35 && b<35);
    assert(SDL_SaveBMP(image,"menu-overlay.bmp"));SDL_DestroySurface(image);
    key(SDLK_q);assert(bar->SelectedTabId==file_tab);check_file_focus();
    float original_font=io.FontDefault->FontSize;
    for(float scale : {1.25f,2.0f,1.0f}) {
        TestDisplayScale(scale);frame();frame();
        assert(std::abs(io.FontDefault->FontSize-original_font*scale)<0.1f);
        assert(std::abs(hud->Size.x/hud->Size.y-4.0f/3.0f)<0.01f);
        check_style_and_fit();
    }
    for(int height : {240,360,480,720}) {
        assert(SDL_SetWindowSize(window,height*4/3,height));
        for(float scale : {1.0f,1.25f,2.0f}) {
            TestDisplayScale(scale);frame();frame();frame();check_style_and_fit();
        }
    }
    TestDisplayScale(1.0f);
    assert(SDL_SetWindowSize(window,960,720));frame();frame();
    assert(std::abs(hud->Size.y-io.DisplaySize.y*0.9f)<1.1f);
    assert(std::abs(hud->Pos.x+hud->Size.x/2-io.DisplaySize.x/2)<1.1f);
    assert(SDL_SetWindowSize(window,640,480));frame();frame();
    ImGuiID previous=ImGui::GetCurrentContext()->NavId;
    key(SDLK_DOWN);assert(ImGui::GetCurrentContext()->NavId && ImGui::GetCurrentContext()->NavId!=previous);
    key(SDLK_RETURN);
    SDL_VirtualJoystickDesc desc={};SDL_INIT_INTERFACE(&desc);
    desc.type=SDL_JOYSTICK_TYPE_GAMEPAD;desc.naxes=SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons=SDL_GAMEPAD_BUTTON_COUNT;desc.button_mask=(1u<<SDL_GAMEPAD_BUTTON_COUNT)-1;
    desc.axis_mask=(1u<<SDL_GAMEPAD_AXIS_COUNT)-1;desc.name="Ackman overlay test pad";
    SDL_JoystickID id=SDL_AttachVirtualJoystick(&desc);assert(id);
    SDL_Joystick *joy=SDL_OpenJoystick(id);assert(joy);
    SDL_Event event;SDL_PumpEvents();while(SDL_PollEvent(&event))AckmanMenuEvent(&event);frame();
    auto button=[&](SDL_GamepadButton button) {
        assert(SDL_SetJoystickVirtualButton(joy,(int)button,true));SDL_UpdateJoysticks();SDL_PumpEvents();
        while(SDL_PollEvent(&event))AckmanMenuEvent(&event);frame();frame();
        assert(SDL_SetJoystickVirtualButton(joy,(int)button,false));SDL_UpdateJoysticks();SDL_PumpEvents();
        while(SDL_PollEvent(&event))AckmanMenuEvent(&event);frame();frame();
    };
    // A gamepad menu binding must open AND close, without leaking activation.
    key(SDLK_ESCAPE);assert(!AckmanMenuVisible());
    menu_binding.value="Gamepad back";assert(AckmanConfigUpdate(&menu_binding,1));ConfigReloadKeyMap(AckmanConfigPath());AckmanHotkeysInit();
    button(SDL_GAMEPAD_BUTTON_BACK);assert(AckmanMenuVisible());
    button(SDL_GAMEPAD_BUTTON_BACK);assert(!AckmanMenuVisible());
    menu_binding.value="Escape";assert(AckmanConfigUpdate(&menu_binding,1));ConfigReloadKeyMap(AckmanConfigPath());AckmanHotkeysInit();
    key(SDLK_ESCAPE);check_file_focus();
    button(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);assert(bar->SelectedTabId==game_tab);
    button(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);assert(bar->SelectedTabId==graphics_tab);
    check_graphics();button(SDL_GAMEPAD_BUTTON_SOUTH);assert(snes_osd_fps_visible());check_graphics();
    button(SDL_GAMEPAD_BUTTON_SOUTH);assert(!snes_osd_fps_visible());
    button(SDL_GAMEPAD_BUTTON_DPAD_DOWN);check_selector();
    button(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);assert(g_config.display_aspect==1);check_selector();
    button(SDL_GAMEPAD_BUTTON_DPAD_LEFT);assert(g_config.display_aspect==0);check_selector();
    button(SDL_GAMEPAD_BUTTON_SOUTH);assert(g_config.display_aspect==1);check_selector();
    button(SDL_GAMEPAD_BUTTON_SOUTH);assert(g_config.display_aspect==0);check_selector();
    button(SDL_GAMEPAD_BUTTON_DPAD_UP);check_graphics();
    button(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);assert(bar->SelectedTabId==game_tab);
    button(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);assert(bar->SelectedTabId==file_tab);check_file_focus();
    previous=ImGui::GetCurrentContext()->NavId;
    button(SDL_GAMEPAD_BUTTON_DPAD_DOWN);assert(ImGui::GetCurrentContext()->NavId!=previous);
    // Open each branch through real navigation, not a test-only popup hook.
    click_tab(4);frame();
    assert(std::abs(ImGui::GetCurrentContext()->NavWindow->NavRectRel[ImGuiNavLayer_Main].GetWidth()-
        (std::floor(hud->WorkRect.Max.x)-std::ceil(hud->WorkRect.Min.x)))<1.1f);
    auto header_rect=[&]() {
        ImGuiContext *ctx=ImGui::GetCurrentContext();
        return ImGui::WindowRectRelToAbs(ctx->NavWindow,ctx->NavWindow->NavRectRel[ImGuiNavLayer_Main]);
    };
    auto header_glyph_x=[&](const ImRect &row) {
        const ImFontGlyph *glyph=ImGui::GetFont()->FindGlyph('G');
        for(const ImDrawVert &v:ImGui::GetCurrentContext()->NavWindow->DrawList->VtxBuffer)
            if(v.uv.x==glyph->U0 && v.uv.y==glyph->V0 && v.pos.y>=row.Min.y && v.pos.y<row.Max.y)return v.pos.x;
        assert(false);return 0.0f;
    };
    auto check_dropdown_focus=[&](ImGuiID id) {
        ImGuiContext *ctx=ImGui::GetCurrentContext();if(ctx->NavId!=id || !ctx->NavCursorVisible)fprintf(stderr,"focus expected=%u actual=%u visible=%d source=%d\n",id,ctx->NavId,ctx->NavCursorVisible,ctx->NavInputSource);assert(ctx->NavId==id && ctx->NavCursorVisible);
        ImRect row=header_rect();SDL_Surface *pixels=SDL_RenderReadPixels(renderer,nullptr);assert(pixels);
        Uint8 red,green,blue,alpha;
        assert(SDL_ReadSurfacePixel(pixels,(int)(row.Min.x+row.GetWidth()*0.12f),(int)row.GetCenter().y,&red,&green,&blue,&alpha));
        assert(red>30 && red==green && green==blue);SDL_DestroySurface(pixels);
    };
    ImGuiID header_id=ImGui::GetCurrentContext()->NavId;
    float closed_x=header_glyph_x(header_rect());
    key(SDLK_RETURN);check_dropdown_focus(header_id);
    assert(std::abs(header_glyph_x(header_rect())-closed_x)<0.01f);
    {
        ImRect row=header_rect();float y=std::ceil(row.Max.y+ImGui::GetStyle().ItemSpacing.y);
        image=SDL_RenderReadPixels(renderer,nullptr);assert(image);
        Uint8 red,green,blue,alpha;
        int x=(int)std::ceil(row.Min.x);
        assert(SDL_ReadSurfacePixel(image,x,(int)(y+ImGui::GetFrameHeight()/2),&red,&green,&blue,&alpha));assert(red==255 && green==255 && blue==255);
        assert(SDL_ReadSurfacePixel(image,x+2,(int)(y+ImGui::GetFrameHeight()/2),&red,&green,&blue,&alpha));assert(red<30);
        // The phrase-sized outline is closed on all four sides, including
        // the bottom pixel, at the actual font/DPI-dependent row geometry.
        float box_right=x+std::ceil(ImGui::CalcTextSize("Configure all buttons").x+2*AckmanMenuRowPadding());
        int box_top=-1,box_bottom=-1;
        for(int edge_y=(int)y-2;edge_y<=(int)(y+ImGui::GetFrameHeight())+1;edge_y++) {
            assert(SDL_ReadSurfacePixel(image,x,edge_y,&red,&green,&blue,&alpha));
            if(red==255 && green==255 && blue==255) {if(box_top<0)box_top=edge_y;box_bottom=edge_y;}
        }
        assert(box_top>=0 && box_bottom-box_top>=ImGui::GetFrameHeight()-5);
        assert(SDL_SaveBMP(image,"menu-configure-border.bmp"));
        for(int edge_y : {box_top,box_bottom})for(int edge_x=x;edge_x<(int)box_right;edge_x++) {
            assert(SDL_ReadSurfacePixel(image,edge_x,edge_y,&red,&green,&blue,&alpha));assert(red==255 && green==255 && blue==255);
        }
        for(int edge_x : {x,(int)box_right-1})for(int edge_y=box_top;edge_y<=box_bottom;edge_y++) {
            assert(SDL_ReadSurfacePixel(image,edge_x,edge_y,&red,&green,&blue,&alpha));assert(red==255 && green==255 && blue==255);
        }
        SDL_DestroySurface(image);
    }
    key(SDLK_RETURN);check_dropdown_focus(header_id);
    assert(std::abs(header_glyph_x(header_rect())-closed_x)<0.01f);
    key(SDLK_RETURN);check_dropdown_focus(header_id);
    key(SDLK_DOWN);assert(ImGui::GetCurrentContext()->NavId!=header_id);
    ImGuiID configure_id=ImGui::GetCurrentContext()->NavId;ImRect configure_row=header_rect();key(SDLK_RETURN);
    assert(AckmanControlsActive() && !strcmp(AckmanControlsPrompt(),"UP"));
    ImGuiWindow *modal=ImGui::FindWindowByName("Configure SNES buttons");assert(modal && modal->Active);
    assert(std::abs(modal->Size.x/modal->Size.y-2.0f)<0.01f);
    assert(std::abs(modal->Pos.x+modal->Size.x/2-io.DisplaySize.x/2)<1);
    assert(std::abs(modal->Pos.y+modal->Size.y/2-io.DisplaySize.y/2)<1);
    // Inspect the rendered P glyphs on both instructions: one complete blank
    // text row separates them, rather than only normal item spacing.
    const ImFontGlyph *p_glyph=ImGui::GetFont()->FindGlyph('P');
    std::vector<float> instruction_y;
    for(const ImDrawVert &v:modal->DrawList->VtxBuffer)
        if(v.uv.x==p_glyph->U0 && v.uv.y==p_glyph->V0 &&
           std::find(instruction_y.begin(),instruction_y.end(),v.pos.y)==instruction_y.end())instruction_y.push_back(v.pos.y);
    assert(instruction_y.size()==2);
    assert(std::abs(instruction_y[1]-instruction_y[0]-2*ImGui::GetTextLineHeightWithSpacing())<1.1f);
    image=SDL_RenderReadPixels(renderer,nullptr);assert(image);
    assert(SDL_SaveBMP(image,"menu-remapping-popup.bmp"));SDL_DestroySurface(image);
    char *before_map=(char *)SDL_LoadFile(AckmanConfigPath(),nullptr);assert(before_map);
    key(SDLK_q);key(SDLK_F1);assert(!strcmp(AckmanControlsPrompt(),"UP"));
    button(SDL_GAMEPAD_BUTTON_DPAD_UP);assert(!strcmp(AckmanControlsPrompt(),"DOWN"));
    key(SDLK_ESCAPE);assert(!AckmanControlsActive() && AckmanMenuVisible());
    check_dropdown_focus(configure_id);
    char *after_map=(char *)SDL_LoadFile(AckmanConfigPath(),nullptr);assert(after_map && !strcmp(before_map,after_map));SDL_free(after_map);
    // Keyboard branch captures Q/E/F1 as gameplay bindings, with no host action.
    key(SDLK_UP);assert(ImGui::GetCurrentContext()->NavId==header_id);key(SDLK_RETURN);
    key(SDLK_DOWN);key(SDLK_RETURN);key(SDLK_DOWN);configure_id=ImGui::GetCurrentContext()->NavId;configure_row=header_rect();key(SDLK_RETURN);
    assert(AckmanControlsActive());
    button(SDL_GAMEPAD_BUTTON_DPAD_UP);assert(!strcmp(AckmanControlsPrompt(),"UP"));
    const SDL_Keycode keyboard_map[]={SDLK_UP,SDLK_DOWN,SDLK_LEFT,SDLK_RIGHT,SDLK_F2,SDLK_F1,SDLK_q,SDLK_e,SDLK_c,SDLK_v,SDLK_RSHIFT,SDLK_RETURN};
    const char *prompts[]={"UP","DOWN","LEFT","RIGHT","A","B","X","Y","L","R","SELECT","START"};
    for(int i=0;i<12;i++) {
        assert(!strcmp(AckmanControlsPrompt(),prompts[i]));key(keyboard_map[i]);
        if(i<11) {
            after_map=(char *)SDL_LoadFile(AckmanConfigPath(),nullptr);assert(after_map && !strcmp(before_map,after_map));SDL_free(after_map);
            assert(keybinds_get_button(1,0)==SDL_SCANCODE_X);
        }
    }
    assert(!AckmanControlsActive() && AckmanMenuVisible());
    check_dropdown_focus(configure_id);
    keybinds_init(nullptr);assert(keybinds_get_button(1,0)==SDL_SCANCODE_F2 && keybinds_get_button(1,1)==SDL_SCANCODE_F1);
    uint8_t physical_keys[SDL_SCANCODE_COUNT]={};physical_keys[SDL_SCANCODE_F2]=1;
    assert(keybinds_read_player_runner(physical_keys,1)==(1<<8));
    // All twelve individual keyboard rows are present in exact SNES order.
    for(int button_index=0;button_index<12;button_index++) {
        key(SDLK_DOWN);key(SDLK_RETURN);
        assert(AckmanControlsCapturing(false,button_index));key(SDLK_ESCAPE);
    }
    image=SDL_RenderReadPixels(renderer,nullptr);assert(image);
    assert(SDL_SaveBMP(image,"menu-controls-twelve-keyboard.bmp"));SDL_DestroySurface(image);
    for(int i=0;i<12;i++)key(SDLK_UP);check_dropdown_focus(configure_id);
    // Individual rows retain focus on cancel/commit and support direct Delete.
    key(SDLK_DOWN);ImGuiID binding_id=ImGui::GetCurrentContext()->NavId;
    assert(binding_id!=configure_id);
    image=SDL_RenderReadPixels(renderer,nullptr);assert(image);
    Uint8 cr,cg,cb,ca;assert(SDL_ReadSurfacePixel(image,(int)(configure_row.Min.x+configure_row.GetWidth()*0.8f),(int)configure_row.GetCenter().y,&cr,&cg,&cb,&ca));assert(cr<30);SDL_DestroySurface(image);
    key(SDLK_RETURN);assert(AckmanControlsCapturing(false,0) && !AckmanControlsWizard());
    image=SDL_RenderReadPixels(renderer,nullptr);assert(image);assert(SDL_SaveBMP(image,"menu-individual-capture.bmp"));SDL_DestroySurface(image);
    char *before_single=(char *)SDL_LoadFile(AckmanConfigPath(),nullptr);assert(before_single);
    button(SDL_GAMEPAD_BUTTON_DPAD_UP);assert(AckmanControlsCapturing(false,0));
    key(SDLK_ESCAPE);assert(!AckmanControlsActive());check_dropdown_focus(binding_id);
    after_map=(char *)SDL_LoadFile(AckmanConfigPath(),nullptr);assert(after_map && !strcmp(before_single,after_map));SDL_free(after_map);SDL_free(before_single);
    key(SDLK_RETURN);key(SDLK_F2);assert(!AckmanControlsActive());check_dropdown_focus(binding_id);
    char conflict[256];AckmanControlsConflict(false,0,conflict,sizeof(conflict));assert(strstr(conflict,"A") && strstr(conflict,"Load") && !strstr(conflict,"Save"));
    bool red_warning=false;
    for(const ImDrawVert &vertex:menu_content()->DrawList->VtxBuffer)red_warning|=vertex.col==IM_COL32(255,64,64,255);
    assert(red_warning);
    // Conflict text AND the textured keycap must use the same red tint.
    bool red_icon=false;
    for(const ImDrawCmd &cmd:menu_content()->DrawList->CmdBuffer)if(cmd.ElemCount && cmd.GetTexID()!=io.Fonts->TexID) {
        const ImDrawVert &v=menu_content()->DrawList->VtxBuffer[cmd.VtxOffset+menu_content()->DrawList->IdxBuffer[cmd.IdxOffset]];
        if(v.col==IM_COL32(255,64,64,255))red_icon=true;
    }
    assert(red_icon);
    AckmanDeviceImage keyboard_marker;float alert_size=ImGui::GetFontSize()*0.75f;
    assert(AckmanDeviceIcon(false,alert_size,&keyboard_marker));bool contextual_keyboard=false;
    for(const ImDrawCmd &cmd:menu_content()->DrawList->CmdBuffer)if(cmd.ElemCount && cmd.GetTexID()==keyboard_marker.texture) {
        ImRect box(ImVec2(FLT_MAX,FLT_MAX),ImVec2(-FLT_MAX,-FLT_MAX));
        for(unsigned int i=0;i<cmd.ElemCount;i++) {
            const ImDrawVert &v=menu_content()->DrawList->VtxBuffer[cmd.VtxOffset+menu_content()->DrawList->IdxBuffer[cmd.IdxOffset+i]];
            assert(v.col==IM_COL32(255,64,64,255));box.Add(v.pos);
        }
        assert(std::abs(box.GetHeight()-alert_size)<0.1f);contextual_keyboard=true;
    }
    assert(contextual_keyboard);image=SDL_RenderReadPixels(renderer,nullptr);assert(image);assert(SDL_SaveBMP(image,"menu-individual-conflict.bmp"));SDL_DestroySurface(image);
    key(SDLK_DELETE);assert(!AckmanControlsActive() && keybinds_get_button(1,8)==SDL_SCANCODE_UNKNOWN);check_dropdown_focus(binding_id);
    after_map=(char *)SDL_LoadFile(AckmanConfigPath(),nullptr);assert(after_map && strstr(strstr(after_map,"[Keyboard.Keybinds.Player1]"),"up = \n"));SDL_free(after_map);
    key(SDLK_RETURN);assert(AckmanControlsCapturing(false,0));key(SDLK_DELETE);assert(!AckmanControlsActive());check_dropdown_focus(binding_id);
    key(SDLK_RETURN);key(SDLK_UP);assert(keybinds_get_button(1,8)==SDL_SCANCODE_UP);check_dropdown_focus(binding_id);
    // Mouse hover is the same navigation cursor, not a second highlight.
    for(int i=0;i<7;i++)key(SDLK_DOWN);
    ImGuiID y_binding=ImGui::GetCurrentContext()->NavId;ImRect y_rect=header_rect();
    key(SDLK_DOWN);ImGuiID select_binding=ImGui::GetCurrentContext()->NavId;ImRect select_rect=header_rect();
    key(SDLK_UP);assert(ImGui::GetCurrentContext()->NavId==y_binding);
    auto move_mouse=[&](const ImRect &row) {
        SDL_Event motion={};motion.type=SDL_EVENT_MOUSE_MOTION;motion.motion.windowID=SDL_GetWindowID(window);
        motion.motion.x=row.GetCenter().x;motion.motion.y=row.GetCenter().y;
        SDL_WarpMouseInWindow(window,motion.motion.x,motion.motion.y);AckmanMenuEvent(&motion);frame();frame();
    };
    move_mouse(select_rect);check_dropdown_focus(select_binding);
    image=SDL_RenderReadPixels(renderer,nullptr);assert(image);
    Uint8 yr,yg,yb,ya;assert(SDL_ReadSurfacePixel(image,(int)y_rect.Max.x-3,(int)y_rect.GetCenter().y,&yr,&yg,&yb,&ya));assert(yr<30);SDL_DestroySurface(image);
    key(SDLK_UP);check_dropdown_focus(y_binding);
    move_mouse(select_rect);button(SDL_GAMEPAD_BUTTON_DPAD_UP);check_dropdown_focus(y_binding);
    move_mouse(select_rect);SDL_Delay(2010);frame();assert(!SDL_CursorVisible());check_dropdown_focus(select_binding);
    assert(io.ConfigFlags&ImGuiConfigFlags_NoMouse);
    io.AddMousePosEvent(y_rect.GetCenter().x,y_rect.GetCenter().y);frame();check_dropdown_focus(select_binding);
    char *before_wake=(char *)SDL_LoadFile(AckmanConfigPath(),nullptr);assert(before_wake);
    SDL_Event wake={};wake.type=SDL_EVENT_MOUSE_BUTTON_DOWN;wake.button.windowID=SDL_GetWindowID(window);
    wake.button.button=SDL_BUTTON_LEFT;wake.button.down=true;wake.button.x=y_rect.GetCenter().x;wake.button.y=y_rect.GetCenter().y;
    assert(AckmanMenuEvent(&wake));frame();assert(SDL_CursorVisible());
    wake.type=SDL_EVENT_MOUSE_BUTTON_UP;wake.button.down=false;assert(AckmanMenuEvent(&wake));frame();frame();
    assert(!AckmanControlsActive());check_dropdown_focus(select_binding);assert(io.ConfigFlags&ImGuiConfigFlags_NoMouse);
    after_map=(char *)SDL_LoadFile(AckmanConfigPath(),nullptr);assert(after_map && !strcmp(before_wake,after_map));SDL_free(before_wake);SDL_free(after_map);
    key(SDLK_UP);check_dropdown_focus(y_binding);move_mouse(select_rect);check_dropdown_focus(select_binding);
    ImGuiTable *controls_table=ImGui::GetCurrentContext()->Tables.GetByKey(ImHashStr("ControlsOptions",0,bar->SelectedTabId));
    assert(controls_table && controls_table->ColumnsCount==3);
    assert(!(controls_table->Flags&(ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg)));
    float padding=AckmanMenuRowPadding();
    assert(std::abs(controls_table->Columns[0].WidthGiven-1.10f*(std::ceil(ImGui::CalcTextSize("Select").x)+2*padding))<1.1f);
    assert(std::abs(controls_table->Columns[1].WidthGiven-(std::ceil(ImGui::CalcTextSize("Press button (press Esc to cancel or Del to clear)").x)+2*padding))<1.1f);
    float old_left=std::ceil(ImGui::CalcTextSize("Select").x)+2*padding;
    float middle=std::ceil(ImGui::CalcTextSize("Press button (press Esc to cancel or Del to clear)").x)+2*padding;
    float old_right=std::floor(hud->WorkRect.Max.x)-std::ceil(hud->WorkRect.Min.x)-old_left-middle;
    assert(std::abs(controls_table->Columns[2].WidthGiven-0.90f*old_right)<1.1f);
    // Complete a gamepad map with a signed axis plus swapped face buttons.
    key(SDLK_ESCAPE);key(SDLK_ESCAPE);click_tab(4);
    header_id=ImGui::GetCurrentContext()->NavId;closed_x=header_glyph_x(header_rect());
    button(SDL_GAMEPAD_BUTTON_SOUTH);check_dropdown_focus(header_id);
    assert(std::abs(header_glyph_x(header_rect())-closed_x)<0.01f);
    button(SDL_GAMEPAD_BUTTON_DPAD_DOWN);assert(ImGui::GetCurrentContext()->NavId!=header_id);
    configure_id=ImGui::GetCurrentContext()->NavId;button(SDL_GAMEPAD_BUTTON_SOUTH);
    assert(AckmanControlsActive());
    key(SDLK_F2);assert(!strcmp(AckmanControlsPrompt(),"UP"));
    assert(SDL_SetJoystickVirtualAxis(joy,SDL_GAMEPAD_AXIS_LEFTY,-24000));SDL_UpdateJoysticks();SDL_PumpEvents();
    while(SDL_PollEvent(&event))AckmanMenuEvent(&event);frame();
    assert(!strcmp(AckmanControlsPrompt(),"DOWN"));
    assert(SDL_SetJoystickVirtualAxis(joy,SDL_GAMEPAD_AXIS_LEFTY,-26000));SDL_UpdateJoysticks();SDL_PumpEvents();
    while(SDL_PollEvent(&event))AckmanMenuEvent(&event);frame();assert(!strcmp(AckmanControlsPrompt(),"DOWN"));
    assert(SDL_SetJoystickVirtualAxis(joy,SDL_GAMEPAD_AXIS_LEFTY,0));SDL_UpdateJoysticks();SDL_PumpEvents();while(SDL_PollEvent(&event))AckmanMenuEvent(&event);frame();
    const SDL_GamepadButton pad_map[]={SDL_GAMEPAD_BUTTON_DPAD_DOWN,SDL_GAMEPAD_BUTTON_DPAD_LEFT,SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
        SDL_GAMEPAD_BUTTON_SOUTH,SDL_GAMEPAD_BUTTON_EAST,SDL_GAMEPAD_BUTTON_NORTH,SDL_GAMEPAD_BUTTON_WEST,
        SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,SDL_GAMEPAD_BUTTON_BACK,SDL_GAMEPAD_BUTTON_START};
    for(int i=0;i<11;i++){assert(!strcmp(AckmanControlsPrompt(),prompts[i+1]));button(pad_map[i]);}
    assert(!AckmanControlsActive());check_dropdown_focus(configure_id);AckmanControlsInit();
    // All twelve gamepad rows, including L/R, share the same navigation path.
    for(int button_index=0;button_index<12;button_index++) {
        button(SDL_GAMEPAD_BUTTON_DPAD_DOWN);button(SDL_GAMEPAD_BUTTON_SOUTH);
        assert(AckmanControlsCapturing(true,button_index));key(SDLK_ESCAPE);
    }
    image=SDL_RenderReadPixels(renderer,nullptr);assert(image);
    assert(SDL_SaveBMP(image,"menu-controls-twelve-gamepad.bmp"));SDL_DestroySurface(image);
    for(int i=0;i<12;i++)button(SDL_GAMEPAD_BUTTON_DPAD_UP);check_dropdown_focus(configure_id);
    // The same individual workflow is navigable by a real SDL virtual pad.
    button(SDL_GAMEPAD_BUTTON_DPAD_DOWN);binding_id=ImGui::GetCurrentContext()->NavId;
    button(SDL_GAMEPAD_BUTTON_SOUTH);assert(AckmanControlsCapturing(true,0));
    key(SDLK_F2);assert(AckmanControlsCapturing(true,0));
    button(SDL_GAMEPAD_BUTTON_EAST);assert(!AckmanControlsActive());check_dropdown_focus(binding_id);
    char pad_label[128];AckmanControlsLabel(true,0,pad_label,sizeof(pad_label));assert(!strcmp(pad_label,"B"));
    AckmanControlsConflict(true,0,conflict,sizeof(conflict));assert(strstr(conflict,"B"));
    key(SDLK_DELETE);AckmanControlsLabel(true,0,pad_label,sizeof(pad_label));assert(!*pad_label);check_dropdown_focus(binding_id);
    button(SDL_GAMEPAD_BUTTON_SOUTH);assert(AckmanControlsCapturing(true,0));
    assert(SDL_SetJoystickVirtualAxis(joy,SDL_GAMEPAD_AXIS_LEFTY,-24000));SDL_UpdateJoysticks();SDL_PumpEvents();
    while(SDL_PollEvent(&event))AckmanMenuEvent(&event);frame();frame();
    assert(!AckmanControlsActive());check_dropdown_focus(binding_id);
    assert(SDL_SetJoystickVirtualAxis(joy,SDL_GAMEPAD_AXIS_LEFTY,0));SDL_UpdateJoysticks();SDL_PumpEvents();
    while(SDL_PollEvent(&event))AckmanMenuEvent(&event);frame();
    AckmanControlsInit();
    assert(SDL_SetJoystickVirtualButton(joy,SDL_GAMEPAD_BUTTON_EAST,true));SDL_UpdateJoysticks();
    assert(AckmanControlsReadGamepad(id)==(1<<0));
    assert(SDL_SetJoystickVirtualButton(joy,SDL_GAMEPAD_BUTTON_EAST,false));SDL_UpdateJoysticks();
    assert(SDL_SetJoystickVirtualAxis(joy,SDL_GAMEPAD_AXIS_LEFTY,-24000));SDL_UpdateJoysticks();assert(AckmanControlsReadGamepad(id)==(1<<4));
    assert(SDL_SetJoystickVirtualAxis(joy,SDL_GAMEPAD_AXIS_LEFTY,0));SDL_UpdateJoysticks();
    // Populate Hotkeys through its real tab and hybrid capture event path.
    assert(AckmanConfigMigrateHotkeys());ConfigReloadKeyMap(AckmanConfigPath());AckmanHotkeysInit();
    click_tab(5);frame();frame();ImGuiID hotkey_focus=ImGui::GetCurrentContext()->NavId;
    assert(hotkey_focus);char hot_value[128],hot_conflict[512];
    ImGuiWindow *hot_window=ImGui::GetCurrentContext()->NavWindow;assert(hot_window && (hot_window->Flags&ImGuiWindowFlags_ChildWindow));
    ImGuiTable *hot_table=ImGui::GetCurrentContext()->Tables.GetByKey(ImHashStr("HotkeyOptions",0,bar->SelectedTabId));
    assert(hot_table && hot_table->ColumnsCount==3 && !(hot_table->Flags&(ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg)));
    float hot_left=std::ceil(ImGui::CalcTextSize("Decrease screen size").x)+2*AckmanMenuRowPadding();
    float hot_middle=std::ceil(ImGui::CalcTextSize("Press button (press Esc to cancel or Del to clear)").x)+2*AckmanMenuRowPadding();
    assert(std::abs(hot_table->Columns[0].WidthGiven-1.10f*hot_left)<1.1f);
    assert(std::abs(hot_table->Columns[1].WidthGiven-hot_middle)<1.1f);
    image=SDL_RenderReadPixels(renderer,nullptr);assert(image);assert(SDL_SaveBMP(image,"menu-hotkeys-top.bmp"));SDL_DestroySurface(image);
    char *hot_before=(char *)SDL_LoadFile(AckmanConfigPath(),nullptr);key(SDLK_RETURN);assert(AckmanHotkeysCapturing(0));
    key(SDLK_ESCAPE);assert(!AckmanHotkeysActive());check_dropdown_focus(hotkey_focus);
    after_map=(char *)SDL_LoadFile(AckmanConfigPath(),nullptr);assert(!strcmp(after_map,hot_before));SDL_free(after_map);SDL_free(hot_before);
    key(SDLK_RETURN);assert(AckmanHotkeysCapturing(0));
    auto hot_event=[&](SDL_Keycode code,bool down,SDL_Keymod mods) {
        SDL_Event e={};e.type=down?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;e.key.windowID=SDL_GetWindowID(window);e.key.key=code;e.key.mod=mods;e.key.scancode=SDL_GetScancodeFromKey(code,nullptr);
        assert(AckmanMenuEvent(&e));frame();frame();
    };
    hot_event(SDLK_LCTRL,true,SDL_KMOD_CTRL);hot_event(SDLK_LSHIFT,true,SDL_KMOD_CTRL|SDL_KMOD_SHIFT);
    assert(AckmanHotkeysActive());hot_event(SDLK_F14,true,SDL_KMOD_CTRL|SDL_KMOD_SHIFT);assert(!AckmanHotkeysActive());
    hot_event(SDLK_F14,false,SDL_KMOD_CTRL|SDL_KMOD_SHIFT);hot_event(SDLK_LSHIFT,false,SDL_KMOD_CTRL);hot_event(SDLK_LCTRL,false,0);
    AckmanHotkeysLabel(0,hot_value,sizeof(hot_value));assert(!strcmp(hot_value,"Ctrl+Shift+F14"));check_dropdown_focus(hotkey_focus);
    assert(FindCmdForSdlKey(SDLK_F14,SDL_KMOD_CTRL|SDL_KMOD_SHIFT)==kKeys_VolumeDown && !FindCmdForSdlKey(SDLK_F14,0));
    key(SDLK_DELETE);AckmanHotkeysLabel(0,hot_value,sizeof(hot_value));assert(!*hot_value);check_dropdown_focus(hotkey_focus);
    key(SDLK_RETURN);button(SDL_GAMEPAD_BUTTON_EAST);assert(!AckmanHotkeysActive());check_dropdown_focus(hotkey_focus);
    AckmanHotkeysLabel(0,hot_value,sizeof(hot_value));assert(!strcmp(hot_value,"B"));
    AckmanHotkeysConflict(0,hot_conflict,sizeof(hot_conflict));assert(strstr(hot_conflict,"B"));
    AckmanDeviceImage gamepad_marker;assert(AckmanDeviceIcon(true,ImGui::GetFontSize()*0.75f,&gamepad_marker));
    bool contextual_pad=false;
    for(const ImDrawCmd &cmd:menu_content()->DrawList->CmdBuffer)if(cmd.ElemCount && cmd.GetTexID()==gamepad_marker.texture) {
        const ImDrawVert &v=menu_content()->DrawList->VtxBuffer[cmd.VtxOffset+menu_content()->DrawList->IdxBuffer[cmd.IdxOffset]];
        assert(v.col==IM_COL32(255,64,64,255));contextual_pad=true;
    }
    assert(contextual_pad);
    image=SDL_RenderReadPixels(renderer,nullptr);assert(image);assert(SDL_SaveBMP(image,"menu-hotkeys-device-conflict.bmp"));SDL_DestroySurface(image);
    key(SDLK_RETURN);key(SDLK_DELETE);assert(!AckmanHotkeysActive());check_dropdown_focus(hotkey_focus);
    for(int i=1;i<ACK_HOTKEY_COUNT;i++)key(SDLK_DOWN);frame();
    assert(ImGui::GetCurrentContext()->NavId==ImHashStr("Load slot 10",0,hot_table->ID));
    assert(hot_window->Scroll.y>0 && hud->Scroll.y==0);ImRect hot_last=header_rect();
    assert(hot_last.GetCenter().y<hot_window->ClipRect.Max.y && hot_last.GetCenter().y>hot_window->ClipRect.Min.y);
    image=SDL_RenderReadPixels(renderer,nullptr);assert(image);assert(SDL_SaveBMP(image,"menu-hotkeys-slots.bmp"));SDL_DestroySurface(image);
    key(SDLK_DOWN);assert(ImGui::GetCurrentContext()->NavId==bar->SelectedTabId);
    key(SDLK_UP);assert(ImGui::GetCurrentContext()->NavId==ImHashStr("Load slot 10",0,hot_table->ID));
    assert(hot_window->Scroll.y>0); // Wrap scrolls the restored last row into view.
    key(SDLK_DELETE);AckmanHotkeysLabel(33,hot_value,sizeof(hot_value));assert(!*hot_value && !FindCmdForSdlKey(SDLK_F10,0));
    for(int tab=0;tab<7;tab++) {
        click_tab(tab);frame();
        ImGuiContext *ctx=ImGui::GetCurrentContext();
        auto focus_tab=[&]() {
            ImGuiTabItem *item=ImGui::TabBarFindTabByID(bar,bar->SelectedTabId);assert(item);
            ImRect rect(ImVec2(bar->BarRect.Min.x+item->Offset,bar->BarRect.Min.y),ImVec2(bar->BarRect.Min.x+item->Offset+item->Width,bar->BarRect.Max.y));
            ImGui::FocusWindow(hud);ImGui::SetNavID(item->ID,ImGuiNavLayer_Main,hud->NavRootFocusScopeId,ImGui::WindowRectAbsToRel(hud,rect));
        };
        ImGuiID open_tab=bar->SelectedTabId,first_row=ctx->NavId;
        // First-item Up must return to the open tab, not File's leftmost label.
        key(SDLK_UP);assert(ctx->NavId==open_tab && bar->SelectedTabId==open_tab);
        key(SDLK_DOWN);assert(ctx->NavId==first_row);
        button(SDL_GAMEPAD_BUTTON_DPAD_UP);assert(ctx->NavId==open_tab);
        button(SDL_GAMEPAD_BUTTON_DPAD_DOWN);assert(ctx->NavId==first_row);
        focus_tab();key(SDLK_UP);ImGuiID final_row=ctx->NavId;
        assert(final_row && final_row!=bar->SelectedTabId);
        key(SDLK_DOWN);assert(ctx->NavId==bar->SelectedTabId);
        button(SDL_GAMEPAD_BUTTON_DPAD_UP);assert(ctx->NavId==final_row);
        button(SDL_GAMEPAD_BUTTON_DPAD_DOWN);assert(ctx->NavId==bar->SelectedTabId);
        // Merely focusing a neighboring label must leave the open content intact.
        key(tab==0?SDLK_RIGHT:SDLK_LEFT);
        assert(ctx->NavId!=open_tab && bar->SelectedTabId==open_tab);
        key(SDLK_UP);assert(ctx->NavId==final_row && bar->SelectedTabId==open_tab);
        key(SDLK_DOWN);assert(ctx->NavId==open_tab);
        button(tab==0?SDL_GAMEPAD_BUTTON_DPAD_RIGHT:SDL_GAMEPAD_BUTTON_DPAD_LEFT);
        assert(ctx->NavId!=open_tab && bar->SelectedTabId==open_tab);
        button(SDL_GAMEPAD_BUTTON_DPAD_UP);assert(ctx->NavId==final_row && bar->SelectedTabId==open_tab);
        button(SDL_GAMEPAD_BUTTON_DPAD_DOWN);assert(ctx->NavId==open_tab);
        SDL_SetJoystickVirtualAxis(joy,SDL_GAMEPAD_AXIS_LEFTY,-24000);SDL_UpdateJoysticks();SDL_PumpEvents();
        while(SDL_PollEvent(&event))AckmanMenuEvent(&event);frame();frame();
        SDL_SetJoystickVirtualAxis(joy,SDL_GAMEPAD_AXIS_LEFTY,0);SDL_UpdateJoysticks();SDL_PumpEvents();
        while(SDL_PollEvent(&event))AckmanMenuEvent(&event);frame();frame();assert(ctx->NavId==final_row);
        key(SDLK_DOWN);assert(ctx->NavId==bar->SelectedTabId);
    }
    click_tab(5);frame();
    // The scrollbar channel is permanent; overflowing Hotkeys shows a grab,
    // while short tabs leave it empty. Tab navigation never changes row width.
    assert(hud->Flags&ImGuiWindowFlags_AlwaysVerticalScrollbar);
    float stable_left=hud->WorkRect.Min.x,stable_right=hud->WorkRect.Max.x;
    auto scroll_pixels=[&]() {
        SDL_Surface *pixels=SDL_RenderReadPixels(renderer,nullptr);assert(pixels);
        int x=(int)ImGui::GetWindowScrollbarRect(menu_content(),ImGuiAxis_Y).GetCenter().x,count=0;
        for(int y=(int)hud->Pos.y+60;y<(int)(hud->Pos.y+hud->Size.y)-8;y++) {
            Uint8 r,g,b,a;assert(SDL_ReadSurfacePixel(pixels,x,y,&r,&g,&b,&a));
            if(r>50 || g>50 || b>50)count++;
        }
        SDL_DestroySurface(pixels);return count;
    };
    assert(scroll_pixels()>10);
    click_tab(1);frame();assert(scroll_pixels()==0);
    image=SDL_RenderReadPixels(renderer,nullptr);assert(image);
    assert(SDL_SaveBMP(image,"menu-fixed-scrollbar-game.bmp"));SDL_DestroySurface(image);
    auto stable_columns=[&]() {
        assert(hud->ScrollbarY && std::abs(hud->WorkRect.Min.x-stable_left)<0.1f);
        assert(std::abs(hud->WorkRect.Max.x-stable_right)<0.1f);
    };
    for(int i=0;i<14;i++){key(SDLK_e);stable_columns();}
    for(int i=0;i<14;i++){key(SDLK_q);stable_columns();}
    for(int i=0;i<14;i++){button(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);stable_columns();}
    for(int i=0;i<14;i++){button(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);stable_columns();}
    click_tab(2);frame();g_config.screenshot_internal=false;
    for(int row=0;row<10;row++)key(SDLK_DOWN);
    key(SDLK_RIGHT);assert(g_config.screenshot_internal);
    key(SDLK_LEFT);assert(!g_config.screenshot_internal);
    // Shaders preserves one radio selection while disabled and rejects clicks
    // outside the OpenGL output preference, without dimming the white notice.
    char shader_folder[4096];std::snprintf(shader_folder,sizeof(shader_folder),"%s",AckmanMenuShaderDirectory());
    assert(SDL_CreateDirectory(shader_folder));
    const char *shader_files[]={"ackman-ui-a.glsl","ackman-ui-b.glsl"};
    for(const char *name:shader_files) {
        char path[4096];std::snprintf(path,sizeof(path),"%s%s",shader_folder,name);
        SDL_IOStream *file=SDL_IOFromFile(path,"wb");assert(file);SDL_WriteIO(file,"// fixture",10);SDL_CloseIO(file);
    }
    char nested[4096];std::snprintf(nested,sizeof(nested),"%sackman-ui-folder/nested",shader_folder);
    assert(SDL_CreateDirectory(nested));
    for(const char *extension:{"glsl","glslp"}) {
        char path[4096];std::snprintf(path,sizeof(path),"%s/pass.%s",nested,extension);
        SDL_IOStream *file=SDL_IOFromFile(path,"wb");assert(file);SDL_WriteIO(file,"// fixture",10);SDL_CloseIO(file);
    }
    AckmanMenuShadersRefresh();assert(AckmanMenuShaderCount()>=2);
    g_config.enable_shaders=false;g_config.selected_shader[0]=0;g_config.output_method=kOutputMethod_OpenGL;
    click_tab(7);frame();ImRect shader_enable=header_rect();
    char *shader_before=(char *)SDL_LoadFile(AckmanConfigPath(),nullptr);assert(shader_before);
    key(SDLK_RETURN);assert(g_config.enable_shaders);key(SDLK_DOWN);key(SDLK_RETURN);
    assert(!strcmp(g_config.selected_shader,shader_files[0]));
    key(SDLK_DOWN);key(SDLK_RETURN);assert(!strcmp(g_config.selected_shader,shader_files[1]));
    key(SDLK_RETURN);assert(!*g_config.selected_shader);key(SDLK_RETURN);
    assert(!strcmp(g_config.selected_shader,shader_files[1]));
    click(shader_enable.GetCenter().x,shader_enable.GetCenter().y);assert(!g_config.enable_shaders);
    assert(!strcmp(g_config.selected_shader,shader_files[1]));
    g_config.output_method=kOutputMethod_SDL;frame();
    click(shader_enable.GetCenter().x,shader_enable.GetCenter().y);assert(!g_config.enable_shaders);
    image=SDL_RenderReadPixels(renderer,nullptr);assert(image);assert(SDL_SaveBMP(image,"menu-shaders-disabled.bmp"));SDL_DestroySurface(image);
    g_config.output_method=kOutputMethod_OpenGL;frame();
    click(shader_enable.GetCenter().x,shader_enable.GetCenter().y);assert(g_config.enable_shaders);
    char *shader_config=(char *)SDL_LoadFile(AckmanConfigPath(),nullptr);assert(shader_config);
    assert(!std::strcmp(shader_config,shader_before));SDL_free(shader_config);
    key(SDLK_DOWN);key(SDLK_DOWN);key(SDLK_DOWN);key(SDLK_RETURN); // folder header
    key(SDLK_DOWN);key(SDLK_RETURN); // nested folder
    key(SDLK_DOWN);key(SDLK_RETURN); // native GLSL
    assert(!std::strcmp(g_config.selected_shader,"ackman-ui-folder/nested/pass.glsl"));
    key(SDLK_DOWN);key(SDLK_RETURN); // preset
    assert(!std::strcmp(g_config.selected_shader,"ackman-ui-folder/nested/pass.glslp"));
    shader_config=(char *)SDL_LoadFile(AckmanConfigPath(),nullptr);assert(shader_config);
    assert(!std::strcmp(shader_config,shader_before));SDL_free(shader_config);SDL_free(shader_before);
    AckmanMenuShadersRefresh();assert(!std::strcmp(g_config.selected_shader,"ackman-ui-folder/nested/pass.glslp"));
    image=SDL_RenderReadPixels(renderer,nullptr);assert(image);assert(SDL_SaveBMP(image,"menu-shaders-enabled.bmp"));SDL_DestroySurface(image);
    // A long recursive tree with repeated basenames exercises physical clipping
    // and the native scrollbar's single ID while dragging over multiple frames.
    for(int i=0;i<40;i++) {
        char path[4096];std::snprintf(path,sizeof(path),"%s/zz-scroll-%02d.glsl",nested,i);
        SDL_IOStream *file=SDL_IOFromFile(path,"wb");assert(file);SDL_WriteIO(file,"// fixture",10);SDL_CloseIO(file);
    }
    char repeated[4096];std::snprintf(repeated,sizeof(repeated),"%sackman-ui-repeat/nested",shader_folder);assert(SDL_CreateDirectory(repeated));
    char repeat_file[4096];std::snprintf(repeat_file,sizeof(repeat_file),"%s/pass.glsl",repeated);
    SDL_IOStream *repeat=SDL_IOFromFile(repeat_file,"wb");assert(repeat);SDL_WriteIO(repeat,"// fixture",10);SDL_CloseIO(repeat);
    AckmanMenuShadersRefresh();frame();frame();
    ImGuiWindow *scroll_content=menu_content();assert(scroll_content->ScrollMax.y>0);
    assert(scroll_content->Pos.y>=bar->BarRect.Max.y);
    assert(scroll_content->Pos.y+scroll_content->Size.y<=hud->WorkRect.Max.y+1.0f);
    auto clipped_shader_draws=[&]() {
        for(const ImDrawCmd &cmd:scroll_content->DrawList->CmdBuffer) {
            if(!cmd.ElemCount || cmd.ClipRect.w<=cmd.ClipRect.y)continue;
            assert(cmd.ClipRect.y>=scroll_content->Pos.y-0.1f);
            assert(cmd.ClipRect.w<=scroll_content->Pos.y+scroll_content->Size.y+0.1f);
        }
        assert(ImGui::GetCurrentContext()->DebugDrawIdConflicts==0);
    };
    ImGui::SetScrollY(scroll_content,scroll_content->ScrollMax.y*0.5f);frame();frame();clipped_shader_draws();
    ImRect scrollbar=ImGui::GetWindowScrollbarRect(scroll_content,ImGuiAxis_Y);
    click(scrollbar.GetCenter().x,scrollbar.GetCenter().y);frame();frame();
    ImGuiID scrollbar_id=ImGui::GetWindowScrollbarID(scroll_content,ImGuiAxis_Y);
    assert(ImGui::GetCurrentContext()->HoveredIdPreviousFrame==scrollbar_id);
    assert(ImGui::GetCurrentContext()->HoveredIdPreviousFrameItemCount==1);
    SDL_Event drag={};drag.type=SDL_EVENT_MOUSE_BUTTON_DOWN;drag.button.windowID=SDL_GetWindowID(window);
    drag.button.button=SDL_BUTTON_LEFT;drag.button.down=true;
    drag.button.x=scrollbar.GetCenter().x;drag.button.y=scrollbar.GetCenter().y;
    AckmanMenuEvent(&drag);frame();frame();assert(ImGui::GetCurrentContext()->ActiveId==scrollbar_id);
    float scroll_before_drag=scroll_content->Scroll.y;
    for(int step=1;step<=4;step++) {
        SDL_Event motion={};motion.type=SDL_EVENT_MOUSE_MOTION;motion.motion.windowID=SDL_GetWindowID(window);
        motion.motion.x=scrollbar.GetCenter().x;motion.motion.y=scrollbar.GetCenter().y+step*8;
        AckmanMenuEvent(&motion);ImGui::GetIO().AddMousePosEvent(motion.motion.x,motion.motion.y);frame();
        assert(ImGui::GetCurrentContext()->ActiveId==scrollbar_id);clipped_shader_draws();
    }
    drag.type=SDL_EVENT_MOUSE_BUTTON_UP;drag.button.down=false;drag.button.y+=32;
    AckmanMenuEvent(&drag);frame();frame();assert(scroll_content->Scroll.y>scroll_before_drag);
    clipped_shader_draws();
    image=SDL_RenderReadPixels(renderer,nullptr);assert(image);assert(SDL_SaveBMP(image,"menu-shaders-scroll-middle.bmp"));SDL_DestroySurface(image);
    ImGui::SetScrollY(scroll_content,scroll_content->ScrollMax.y);frame();frame();clipped_shader_draws();
    // Expand the second identical nested name and its pass.glsl leaf via keyboard.
    key(SDLK_HOME);key(SDLK_UP);key(SDLK_RETURN);key(SDLK_DOWN);key(SDLK_RETURN);key(SDLK_DOWN);frame();
    assert(ImGui::GetCurrentContext()->DebugDrawIdConflicts==0);
    image=SDL_RenderReadPixels(renderer,nullptr);assert(image);assert(SDL_SaveBMP(image,"menu-shaders-scroll-bottom.bmp"));SDL_DestroySurface(image);
    for(int i=0;i<40;i++) {char path[4096];std::snprintf(path,sizeof(path),"%s/zz-scroll-%02d.glsl",nested,i);SDL_RemovePath(path);}
    SDL_RemovePath(repeat_file);SDL_RemovePath(repeated);std::snprintf(repeated,sizeof(repeated),"%sackman-ui-repeat",shader_folder);SDL_RemovePath(repeated);
    ImGui::SetScrollY(scroll_content,0);frame();frame();
    g_config.enable_shaders=false;g_config.selected_shader[0]=0;g_config.output_method=kOutputMethod_SDL;
    for(const char *name:shader_files) {char path[4096];std::snprintf(path,sizeof(path),"%s%s",shader_folder,name);SDL_RemovePath(path);}
    for(const char *extension:{"glsl","glslp"}) {char path[4096];std::snprintf(path,sizeof(path),"%s/pass.%s",nested,extension);SDL_RemovePath(path);}
    SDL_RemovePath(nested);std::snprintf(nested,sizeof(nested),"%sackman-ui-folder",shader_folder);SDL_RemovePath(nested);
    AckmanMenuShadersRefresh();
    // Mods keeps the 42/16/42 geometry and compares active membership live.
    click_tab(6);frame();
    int alpha=-1;for(int i=0;i<AckmanMenuModsCount();i++)if(!strcmp(AckmanMenuModName(i),"ackman-test-alpha.snesmod"))alpha=i;
    assert(alpha>=0 && !AckmanMenuModsChanged());
    ImGuiWindow *left_list=nullptr,*middle_list=nullptr,*right_list=nullptr,*metadata=nullptr;
    for(ImGuiWindow *candidate:ImGui::GetCurrentContext()->Windows)if(candidate->Active && candidate->ParentWindow==menu_content()) {
        if(strstr(candidate->Name,"ModMetadata"))metadata=candidate;
        else if(strstr(candidate->Name,"ModList")) {
            if(!left_list || candidate->Pos.x<left_list->Pos.x)left_list=candidate;
            if(!right_list || candidate->Pos.x>right_list->Pos.x)right_list=candidate;
        }
    }
    assert(left_list && right_list && metadata);
    for(ImGuiWindow *candidate:ImGui::GetCurrentContext()->Windows)if(candidate->Active && candidate->ParentWindow==menu_content() && strstr(candidate->Name,"ModList") && candidate!=left_list && candidate!=right_list)middle_list=candidate;
    assert(middle_list);
    float mod_width=hud->WorkRect.GetWidth();
    assert(std::abs(left_list->Size.x/mod_width-0.42f)<0.01f && std::abs(right_list->Size.x-left_list->Size.x)<1.1f);
    assert(std::abs(middle_list->Size.x/mod_width-0.16f)<0.01f);
    assert(std::abs(middle_list->Pos.x-left_list->Pos.x-left_list->Size.x)<1.1f);
    assert(std::abs(right_list->Pos.x-middle_list->Pos.x-middle_list->Size.x)<1.1f);
    assert(metadata->WindowBorderSize==1.0f && metadata->Pos.y>hud->Pos.y+hud->Size.y*0.45f);
    assert(left_list->WindowBorderSize==1.0f && right_list->WindowBorderSize==1.0f && middle_list->WindowBorderSize==0.0f);
    // The list's top border is now one line below the external headings;
    // metadata retains the same height relative to the complete Mods area.
    float remaining_height=menu_content()->ContentRegionRect.Max.y-(left_list->Pos.y-ImGui::GetTextLineHeightWithSpacing());
    float original_metadata_height=remaining_height-std::max(3*ImGui::GetFrameHeight(),remaining_height*0.43f)-
        2*ImGui::GetStyle().ItemSpacing.y-ImGui::GetFontSize()*0.75f;
    // Child placement and the newly separate heading each round to pixels.
    assert(std::abs(metadata->Size.y-original_metadata_height*0.5f)<2.1f);
    key(SDLK_DOWN); // IPS -> first available mod.
    assert(!AckmanMenuModActive(alpha));key(SDLK_RETURN);assert(AckmanMenuModActive(alpha) && AckmanMenuModSelected()==-1);
    assert(AckmanMenuModsChanged());frame();
    bool restart_red=false;for(const ImDrawVert &v:menu_content()->DrawList->VtxBuffer)restart_red|=v.col==IM_COL32(255,64,64,255);
    assert(!restart_red); // Deferred batch application needs no restart warning.
    image=SDL_RenderReadPixels(renderer,nullptr);assert(image);assert(SDL_SaveBMP(image,"menu-mods-changed.bmp"));SDL_DestroySurface(image);
    assert(AckmanMenuModsChanged()); // Reverting within this opening clears the pending delta.
    AckmanMenuModSelect(alpha);frame();
    click(middle_list->Pos.x+middle_list->Size.x/2,middle_list->Pos.y+middle_list->Size.y/2);
    assert(!AckmanMenuModActive(alpha) && !AckmanMenuModsChanged() && AckmanMenuModSelected()==-1);
    frame();restart_red=false;for(const ImDrawVert &v:menu_content()->DrawList->VtxBuffer)restart_red|=v.col==IM_COL32(255,64,64,255);
    assert(!restart_red);
    image=SDL_RenderReadPixels(renderer,nullptr);assert(image);assert(SDL_SaveBMP(image,"menu-mods-restored.bmp"));SDL_DestroySurface(image);
    int beta=-1;for(int i=0;i<AckmanMenuModsCount();i++)if(!strcmp(AckmanMenuModName(i),"ackman-test-beta.snesmod"))beta=i;
    assert(beta>=0);
    auto row_y=[&](ImGuiWindow *list,int row) {
        return list->DC.CursorStartPos.y+
               row*(ImGui::GetFrameHeight()+ImGui::GetStyle().ItemSpacing.y)+ImGui::GetFrameHeight()/2;
    };
    auto modifier=[&](SDL_Keycode code,bool down) {
        SDL_Event e={};e.type=down?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;e.key.key=code;
        e.key.windowID=SDL_GetWindowID(window);
        e.key.scancode=code==SDLK_LCTRL?SDL_SCANCODE_LCTRL:SDL_SCANCODE_LSHIFT;
        e.key.mod=down?(code==SDLK_LCTRL?SDL_KMOD_CTRL:SDL_KMOD_SHIFT):SDL_KMOD_NONE;
        AckmanMenuEvent(&e);frame();
    };
    click(left_list->Pos.x+20,row_y(left_list,0));assert(AckmanMenuModHighlighted(alpha));
    click(left_list->Pos.x+20,row_y(left_list,0));assert(!AckmanMenuModHighlighted(alpha));
    click(left_list->Pos.x+20,row_y(left_list,0));
    modifier(SDLK_LCTRL,true);click(left_list->Pos.x+20,row_y(left_list,1));modifier(SDLK_LCTRL,false);
    assert(AckmanMenuModHighlighted(alpha) && AckmanMenuModHighlighted(beta));
    SDL_Event hover={};hover.type=SDL_EVENT_MOUSE_MOTION;hover.motion.windowID=SDL_GetWindowID(window);
    hover.motion.x=left_list->Pos.x+20;hover.motion.y=row_y(left_list,2);AckmanMenuEvent(&hover);frame();
    assert(AckmanMenuModHighlighted(alpha) && AckmanMenuModHighlighted(beta) && AckmanMenuModSelected()==beta);
    image=SDL_RenderReadPixels(renderer,nullptr);assert(image);assert(SDL_SaveBMP(image,"menu-mods-multiselect.bmp"));SDL_DestroySurface(image);
    key(SDLK_RETURN);assert(AckmanMenuModActive(alpha) && AckmanMenuModActive(beta));
    assert(!AckmanMenuModHighlighted(alpha) && !AckmanMenuModHighlighted(beta));
    click(right_list->Pos.x+20,row_y(right_list,0));
    modifier(SDLK_LSHIFT,true);click(right_list->Pos.x+20,row_y(right_list,1));modifier(SDLK_LSHIFT,false);
    assert(AckmanMenuModHighlighted(alpha) && AckmanMenuModHighlighted(beta));
    button(SDL_GAMEPAD_BUTTON_SOUTH);assert(!AckmanMenuModActive(alpha) && !AckmanMenuModActive(beta) && !AckmanMenuModsChanged());
    click(left_list->Pos.x+20,row_y(left_list,0));
    modifier(SDLK_LCTRL,true);click(left_list->Pos.x+20,row_y(left_list,1));modifier(SDLK_LCTRL,false);
    click(middle_list->Pos.x+middle_list->Size.x/2,middle_list->Pos.y+middle_list->Size.y/2);
    assert(AckmanMenuModActive(alpha) && AckmanMenuModActive(beta) && AckmanMenuModSelected()==-1);
    AckmanMenuModSelect(alpha);AckmanMenuModClick(beta,true,false);
    click(middle_list->Pos.x+middle_list->Size.x/2,middle_list->Pos.y+middle_list->Size.y/2);
    assert(!AckmanMenuModActive(alpha) && !AckmanMenuModActive(beta) && !AckmanMenuModsChanged());
    // Mouse selects without moving; gamepad A performs the same immediate transfer.
    click(left_list->Pos.x+20,row_y(left_list,0));
    assert(AckmanMenuModSelected()==alpha && !AckmanMenuModActive(alpha));
    frame();
    const AckmanModMetadata *preview=AckmanMenuModMetadata(alpha);
    assert(preview->name && !strcmp(preview->name,"Metadata preview fixture"));
    assert(metadata->ScrollMax.y>0 && metadata->ScrollbarY);
    image=SDL_RenderReadPixels(renderer,nullptr);assert(image);assert(SDL_SaveBMP(image,"menu-mods-selected.bmp"));SDL_DestroySurface(image);
    ImGui::SetScrollY(metadata,metadata->ScrollMax.y);frame();frame();
    image=SDL_RenderReadPixels(renderer,nullptr);assert(image);assert(SDL_SaveBMP(image,"menu-mods-readme-end.bmp"));SDL_DestroySurface(image);
    ImGui::SetScrollY(metadata,0);frame();
    button(SDL_GAMEPAD_BUTTON_SOUTH);assert(AckmanMenuModActive(alpha) && AckmanMenuModsChanged());
    AckmanMenuModSelect(alpha);click(middle_list->Pos.x+middle_list->Size.x/2,middle_list->Pos.y+middle_list->Size.y/2);
    assert(!AckmanMenuModsChanged());
    // Invalid IPS selection is a visible red error, with the list proportions intact.
    assert(!AckmanTranslationStart("missing-official.ips","unused",AckmanMenuModsDirectory()));frame();
    bool invalid_red=false;for(const ImDrawVert &v:menu_content()->DrawList->VtxBuffer)invalid_red|=v.col==IM_COL32(255,64,64,255);
    assert(invalid_red && std::abs(left_list->Size.x/mod_width-0.42f)<0.01f);
    image=SDL_RenderReadPixels(renderer,nullptr);assert(image);assert(SDL_SaveBMP(image,"menu-mods-ips-error.bmp"));SDL_DestroySurface(image);
    AckmanTranslationShutdown();frame();
    // A local official archive, when available, must show the actual Readme,
    // including its final paragraph inside the scrollable preview panel.
    int official=-1,ordinal=0;
    for(int i=0;i<AckmanMenuModsCount();i++)if(!AckmanMenuModActive(i)) {
        if(!strcmp(AckmanMenuModName(i),ACKMAN_TRANSLATION_PACKAGE)){official=i;break;}
        ordinal++;
    }
    if(official>=0) {
        ImGui::SetScrollY(left_list,0);frame();
        float y=left_list->DC.CursorStartPos.y+
                ordinal*(ImGui::GetFrameHeight()+ImGui::GetStyle().ItemSpacing.y)+ImGui::GetFrameHeight()/2;
        click(left_list->Pos.x+20,y);frame();
        assert(AckmanMenuModSelected()==official);
        const AckmanModMetadata *info=AckmanMenuModMetadata(official);
        assert(info->name && !strcmp(info->name,"Go Go Ackman english translation"));
        assert(info->description && strstr(info->description,"\n\nThanks to:\n"));
        assert(metadata->ScrollMax.y>0);
        ImGui::SetScrollY(metadata,0);frame();frame();
        image=SDL_RenderReadPixels(renderer,nullptr);assert(image);assert(SDL_SaveBMP(image,"menu-mods-translation-top.bmp"));SDL_DestroySurface(image);
        ImGui::SetScrollY(metadata,metadata->ScrollMax.y);frame();frame();
        assert(metadata->Scroll.y>0);
        image=SDL_RenderReadPixels(renderer,nullptr);assert(image);assert(SDL_SaveBMP(image,"menu-mods-translation-end.bmp"));SDL_DestroySurface(image);
    }
    for(int i=0;i<AckmanMenuModsCount();i++)if(!strcmp(AckmanMenuModName(i),ACKMAN_TRANSLATION_PTBR_PACKAGE)) {
        AckmanMenuModSelect(i);frame();frame();
        const AckmanModMetadata *info=AckmanMenuModMetadata(i);
        assert(info->name && !strcmp(info->name,"Go Go Ackman PT-BR translation"));
        assert(info->description && strstr(info->description,"\nTermo:\n\n") && metadata->ScrollMax.y>0);
        ImGui::SetScrollY(metadata,0);frame();frame();
        image=SDL_RenderReadPixels(renderer,nullptr);assert(image);assert(SDL_SaveBMP(image,"menu-mods-ptbr-top.bmp"));SDL_DestroySurface(image);
        ImGui::SetScrollY(metadata,metadata->ScrollMax.y);frame();frame();assert(metadata->Scroll.y>0);
        image=SDL_RenderReadPixels(renderer,nullptr);assert(image);assert(SDL_SaveBMP(image,"menu-mods-ptbr-end.bmp"));SDL_DestroySurface(image);
        break;
    }
    // Restore the fixture so the remaining File/Graphics regressions stay independent.
    SDL_IOStream *restore=SDL_IOFromFile(AckmanConfigPath(),"wb");assert(restore);
    assert(SDL_WriteIO(restore,before_map,strlen(before_map))==strlen(before_map));assert(SDL_CloseIO(restore));SDL_free(before_map);
    AckmanControlsInit();ConfigReloadKeyMap(AckmanConfigPath());AckmanHotkeysInit();
    key(SDLK_ESCAPE);key(SDLK_ESCAPE);frame();
    // SVG drawing must not change the raw device word forwarded to gameplay.
    assert(SDL_SetJoystickVirtualButton(joy,SDL_GAMEPAD_BUTTON_DPAD_RIGHT,true));SDL_UpdateJoysticks();frame();frame();
    uint16_t raw_before=AckmanControlsReadDevices();assert(raw_before&(1u<<7));
    frame();assert(AckmanControlsReadDevices()==raw_before);
    assert(SDL_SetJoystickVirtualButton(joy,SDL_GAMEPAD_BUTTON_DPAD_RIGHT,false));SDL_UpdateJoysticks();frame();
    assert(!(AckmanControlsReadDevices()&(1u<<7)));
    SDL_CloseJoystick(joy);assert(SDL_DetachVirtualJoystick(id));frame();
    key(SDLK_ESCAPE);key(SDLK_ESCAPE);frame();hud=ImGui::FindWindowByName("Ackman HUD");
    // Reopening must reset both branches without test-side state manipulation.
    ImGuiID save_id=ImHashStr("Save",0,file_tab),load_id=ImHashStr("Load",0,file_tab);
    assert(!menu_content()->StateStorage.GetBool(save_id) && !menu_content()->StateStorage.GetBool(load_id));
    float line=ImGui::GetFontSize()+ImGui::GetStyle().FramePadding.y*2;
    ImGuiContext *focus_ctx=ImGui::GetCurrentContext();
    float top=ImGui::WindowRectRelToAbs(focus_ctx->NavWindow,focus_ctx->NavWindow->NavRectRel[ImGuiNavLayer_Main]).Min.y;
    float spacing=ImGui::GetStyle().ItemSpacing.y;
    float save_y=top+line*1.5f+spacing;
    click(hud->Pos.x+60,save_y+line+spacing);
    if(!menu_content()->StateStorage.GetBool(load_id))fprintf(stderr,"load focus=%u expected=%u top=%f click=%f child=%s pos=%f scroll=%f selected=%u file=%u\n",ImGui::GetCurrentContext()->NavId,load_id,top,save_y+line+spacing,menu_content()->Name,menu_content()->Pos.y,menu_content()->Scroll.y,bar->SelectedTabId,file_tab);
    assert(menu_content()->StateStorage.GetBool(load_id));
    click(hud->Pos.x+60,save_y);assert(menu_content()->StateStorage.GetBool(save_id));
    key(SDLK_ESCAPE);key(SDLK_ESCAPE);frame();
    assert(!menu_content()->StateStorage.GetBool(save_id) && !menu_content()->StateStorage.GetBool(load_id));
    click(hud->WorkRect.Min.x+14,save_y);
    assert(menu_content()->StateStorage.GetBool(save_id));
    image=SDL_RenderReadPixels(renderer,nullptr);assert(image);
    // The tab rule must reach both panel edges, beyond its content padding.
    for(int x : {(int)hud->Pos.x+2,(int)(hud->Pos.x+hud->Size.x)-3}) {
        bool found=false;
        for(int y=(int)bar->BarRect.Max.y-1;y<=(int)bar->BarRect.Max.y+1;++y) {
            assert(SDL_ReadSurfacePixel(image,x,y,&r,&g,&b,&a));
            found|=r>25 && std::abs((int)r-g)<2 && std::abs((int)g-b)<2;
        }
        assert(found);
    }
    assert(SDL_SaveBMP(image,"menu-file-style.bmp"));SDL_DestroySurface(image);
    for(int i=0;i<10;i++)key(SDLK_DOWN);key(SDLK_RETURN);
    assert(!AckmanMenuVisible());
    while(SDL_PollEvent(&event))AckmanMenuEvent(&event);
    key(SDLK_ESCAPE);
    click(hud->Pos.x+hud->Size.x/2,top+line*3.5f+spacing*3);
    assert(!AckmanMenuVisible());
    while(SDL_PollEvent(&event))AckmanMenuEvent(&event);
    TestCancelSaveDialog();
    // Public ImGui entry points must use the same full-row component, including mixed flags.
    key(SDLK_ESCAPE);probe=true;frame();frame();
    assert(probe_rows[0].GetWidth()>210);
    for(int i=0;i<3;++i) click(probe_rows[i].GetCenter().x,probe_rows[i].GetCenter().y);
    assert(probe_checked && probe_flags==3 && probe_flags64==3);
    for(int i=0;i<3;++i) click(probe_rows[i].GetCenter().x,probe_rows[i].GetCenter().y);
    assert(!probe_checked && probe_flags==0 && probe_flags64==0);
    image=SDL_RenderReadPixels(renderer,nullptr);assert(image);
    for(const ImRect &row:probe_rows) {
        int first=-1,last=-1,x=(int)row.Max.x-(int)AckmanMenuRowPadding()-2;
        for(int y=(int)std::ceil(row.Min.y)+1;y<(int)row.Max.y-1;++y) {
            assert(SDL_ReadSurfacePixel(image,x,y,&r,&g,&b,&a));
            if(r==255 && g==255 && b==255) { if(first<0)first=y;last=y; }
        }
        assert(first>=0 && last>=first);
        assert(first-row.Min.y>=1.5f && row.Max.y-last-1>=1.5f);
        assert(std::abs((first+last+1)/2.0f-row.GetCenter().y)<=0.51f);
    }
    assert(SDL_SaveBMP(image,"menu-global-selectors.bmp"));
    SDL_DestroySurface(image);
    // Long/short values must leave both arrows at exactly the same coordinates.
    ImVec2 fixed=arrow_positions[0];
    probe_aspect=1;probe_mode=0;frame();
    assert(fixed.x==arrow_positions[0].x && fixed.y==arrow_positions[0].y);
    probe=false;key(SDLK_ESCAPE);assert(!AckmanMenuVisible());
    // Leave a real UI change pending for the shutdown/reload test in C.
    key(SDLK_ESCAPE);key(SDLK_e);key(SDLK_e);key(SDLK_DOWN);key(SDLK_RIGHT);
    assert(g_config.display_aspect==1);key(SDLK_ESCAPE);
    SDL_DestroyTexture(background);
    // HiDPI geometry must scale with clipping, and must not leak renderer state.
    SDL_SetRenderDrawColor(renderer,0,0,0,255);SDL_RenderClear(renderer);
    AckmanMenuDraw(renderer,false);
    ImGui::GetIO().DisplayFramebufferScale=ImVec2(2,2);
    ImGui::SetNextWindowPos(ImVec2(20,20));ImGui::SetNextWindowSize(ImVec2(120,100));
    ImGui::Begin("HiDPI geometry",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoInputs);
    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(30,30),ImVec2(50,50),IM_COL32(0,255,0,255));
    ImGui::End();AckmanMenuDraw(renderer,true);
    float sx=0,sy=0;SDL_GetRenderScale(renderer,&sx,&sy);assert(sx==1 && sy==1);
    SDL_Surface *hidpi=SDL_RenderReadPixels(renderer,nullptr);assert(hidpi);
    Uint8 hd_r,hd_g,hd_b,hd_a;
    assert(SDL_ReadSurfacePixel(hidpi,70,70,&hd_r,&hd_g,&hd_b,&hd_a));
    assert(hd_r==0 && hd_g==255 && hd_b==0);
    SDL_DestroySurface(hidpi);
}




