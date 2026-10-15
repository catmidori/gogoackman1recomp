#include "recomp_menu_icons.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

struct IconTexture {
    SDL_Texture *texture=nullptr;float aspect=1,ink_aspect=1;int frame=0;
    ImVec2 ink_min=ImVec2(0,0),ink_max=ImVec2(1,1);
};
static SDL_Renderer *icon_renderer;
static std::map<std::pair<std::string,int>,IconTexture> textures;
void RecompMenuIconsShutdown() {
    for(auto &entry:textures)SDL_DestroyTexture(entry.second.texture);
    textures.clear();icon_renderer=nullptr;
}
void RecompMenuIconsInit(SDL_Renderer *renderer) {
    if(icon_renderer!=renderer){RecompMenuIconsShutdown();icon_renderer=renderer;}
}
void RecompMenuIconsEndFrame() {
    // Retire size variants only AFTER ImGui consumed their draw commands.
    for(auto i=textures.begin();i!=textures.end();)
        if(i->second.frame<ImGui::GetFrameCount()-1) {
            SDL_DestroyTexture(i->second.texture);i=textures.erase(i);
        } else ++i;
}
static const RecompSvgAsset *asset(const char *name) {
    size_t n;const RecompSvgAsset *items=RecompButtonAssets(&n);
    for(size_t i=0;i<n;i++)if(!strcmp(name,items[i].name))return items+i;
    return nullptr;
}
static IconTexture *texture(const char *name,float height) {
    if(!icon_renderer || !name || height<=0)return nullptr;
    int pixels=std::clamp((int)std::ceil(height*std::max(1.0f,ImGui::GetIO().DisplayFramebufferScale.y)*2),2,2048);
    auto key=std::make_pair(std::string(name),pixels);auto found=textures.find(key);
    if(found==textures.end()) {
        const RecompSvgAsset *svg=asset(name);if(!svg)return nullptr;
        SDL_Surface *surface=RecompRasterizeSvg(svg->data,svg->size,pixels);if(!surface)return nullptr;
        IconTexture icon;icon.aspect=(float)surface->w/surface->h;
        // The SVG canvases contain different transparent margins (keyboard
        // keycaps vs shoulder buttons). Measure ink, not the 64x64 canvas.
        int left=surface->w,top=surface->h,right=-1,bottom=-1;
        for(int y=0;y<surface->h;y++)for(int x=0;x<surface->w;x++) {
            const unsigned char *pixel=(const unsigned char *)surface->pixels+y*surface->pitch+x*4;
            if(pixel[3]){left=std::min(left,x);right=std::max(right,x);top=std::min(top,y);bottom=std::max(bottom,y);}
        }
        if(right>=left && bottom>=top) {
            icon.ink_min=ImVec2((float)left/surface->w,(float)top/surface->h);
            icon.ink_max=ImVec2((float)(right+1)/surface->w,(float)(bottom+1)/surface->h);
            icon.ink_aspect=(float)(right-left+1)/(bottom-top+1);
        }
        icon.texture=SDL_CreateTextureFromSurface(icon_renderer,surface);SDL_DestroySurface(surface);
        if(!icon.texture)return nullptr;
        SDL_SetTextureBlendMode(icon.texture,SDL_BLENDMODE_BLEND);SDL_SetTextureScaleMode(icon.texture,SDL_SCALEMODE_LINEAR);
        found=textures.emplace(key,icon).first;
    }
    found->second.frame=ImGui::GetFrameCount();return &found->second;
}
static std::string lower(const char *text) {
    std::string s=text?text:"";for(char &c:s)c=(char)SDL_tolower((unsigned char)c);return s;
}
bool RecompDeviceIcon(bool gamepad,float height,RecompDeviceImage *image) {
    IconTexture *icon=texture(gamepad?"controller_generic.svg":"keyboard.svg",height);
    if(!icon)return false;
    *image={(ImTextureID)(intptr_t)icon->texture,ImVec2(height*icon->ink_aspect,height),icon->ink_min,icon->ink_max};
    return true;
}
const char *RecompButtonIcon(const char *binding,bool gamepad) {
    if(!binding || !*binding || !SDL_strcasecmp(binding,"None"))return nullptr;
    std::string key=lower(binding);
    if(key.rfind("gamepad ",0)==0){gamepad=true;key.erase(0,8);}
    if(gamepad) {
        static const std::pair<const char *,const char *> pairs[]={
            {"dpup","xbox_dpad_up_outline.svg"},{"dpdown","xbox_dpad_down_outline.svg"},
            {"dpleft","xbox_dpad_left_outline.svg"},{"dpright","xbox_dpad_right_outline.svg"},
            {"a","xbox_button_a_outline.svg"},{"b","xbox_button_b_outline.svg"},
            {"x","xbox_button_x_outline.svg"},{"y","xbox_button_y_outline.svg"},
            {"south","xbox_button_a_outline.svg"},{"east","xbox_button_b_outline.svg"},
            {"west","xbox_button_x_outline.svg"},{"north","xbox_button_y_outline.svg"},
            {"leftshoulder","xbox_lb_outline.svg"},{"rightshoulder","xbox_rb_outline.svg"},
            {"back","xbox_button_back_icon_outline.svg"},{"start","xbox_button_start_icon_outline.svg"},
            {"guide","xbox_button_menu_outline.svg"},
            {"axis lefttrigger +","xbox_lt_outline.svg"},{"axis righttrigger +","xbox_rt_outline.svg"},
            {"leftstick","xbox_stick_side_l.svg"},{"rightstick","xbox_stick_side_r.svg"},
            {"axis lefty -","xbox_stick_l_up.svg"},{"axis lefty +","xbox_stick_l_down.svg"},
            {"axis leftx -","xbox_stick_l_left.svg"},{"axis leftx +","xbox_stick_l_right.svg"},
            {"axis righty -","xbox_stick_r_up.svg"},{"axis righty +","xbox_stick_r_down.svg"},
            {"axis rightx -","xbox_stick_r_left.svg"},{"axis rightx +","xbox_stick_r_right.svg"}};
        for(auto &p:pairs)if(key==p.first)return p.second;
        return nullptr; // Unrecognized joystick inputs retain their truthful text label.
    }
    static const std::pair<const char *,const char *> aliases[]={
        {"up","arrow_up"},{"down","arrow_down"},{"left","arrow_left"},{"right","arrow_right"},
        {"left shift","shift"},{"right shift","shift"},{"left ctrl","ctrl"},{"right ctrl","ctrl"},
        {"left alt","alt"},{"right alt","alt"},{"left gui","win"},{"right gui","win"},{"gui","win"},
        {"space","space_icon"},{"backspace","backspace_icon_alternative"},{"enter","return"},
        {"pageup","page_up"},{"pagedown","page_down"},{"capslock","capslock"},{"printscreen","printscreen"},
        {"scrolllock","scroll_lock"},{"numlockclear","numlock"},
        {"keypad +","numpad_plus"},{"keypad -","minus"},{"keypad enter","return"},
        {"+","plus"},{"-","minus"},{"/","slash_forward"},{"\\","slash_back"},
        {".","period"},{",","comma"},{";","semicolon"},{"'","apostrophe"},{"=","equals"},
        {"[","bracket_open"},{"]","bracket_close"},{"`","tilde"},{"*","asterisk"}};
    for(auto &p:aliases)if(key==p.first){key=p.second;break;}
    if(key.rfind("mouse ",0)==0)key="mouse_"+key.substr(6);
    else if(key.rfind("mouse_",0)==0){}
    else {if(key.rfind("keypad ",0)==0)key.erase(0,7);key="keyboard_"+key;}
    std::replace(key.begin(),key.end(),' ','_');key+="_outline.svg";
    const RecompSvgAsset *svg=asset(key.c_str());return svg?svg->name:"keyboard_any_outline.svg";
}
static std::vector<const char *> binding_icons(const char *binding,bool gamepad) {
    std::vector<const char *> names;if(!binding || !*binding || !SDL_strcasecmp(binding,"None"))return names;
    std::string remaining=binding;
    if(!gamepad && SDL_strncasecmp(binding,"Gamepad ",8)) {
        while(true) {
            bool modifier=false;
            for(const char *prefix:{"Ctrl+","Alt+","Shift+","Gui+"})if(!SDL_strncasecmp(remaining.c_str(),prefix,strlen(prefix))) {
                std::string key(prefix,strlen(prefix)-1);names.push_back(RecompButtonIcon(key.c_str(),false));
                remaining.erase(0,strlen(prefix));modifier=true;break;
            }
            if(!modifier)break;
        }
    }
    const char *icon=RecompButtonIcon(remaining.c_str(),gamepad);if(icon)names.push_back(icon);return names;
}
bool RecompDrawBindingIcons(ImDrawList *draw,const char *binding,bool gamepad,ImVec2 min,ImVec2 max,float height,ImU32 color) {
    auto names=binding_icons(binding,gamepad);
    if(names.empty())return !binding || !*binding || !SDL_strcasecmp(binding,"None");
    if(max.x<=min.x || max.y<=min.y)return true;
    if(max.y<=ImGui::GetCurrentWindow()->InnerClipRect.Min.y || min.y>=ImGui::GetCurrentWindow()->InnerClipRect.Max.y)return true;
    float gap=ImGui::GetFontSize()*0.25f,plus=ImGui::CalcTextSize("+").x,total=0;
    for(auto name:names){auto icon=texture(name,height);if(!icon)return false;total+=height*icon->aspect;}
    total+=(names.size()-1)*(plus+2*gap);
    if(total>max.x-min.x){height*=std::max(0.1f,(max.x-min.x)/total);gap*=height/(ImGui::GetFontSize()*1.6f);total=0;
        for(auto name:names){auto icon=texture(name,height);if(!icon)return false;total+=height*icon->aspect;}
        total+=(names.size()-1)*(plus+2*gap);}
    float x=(min.x+max.x-total)/2,y=(min.y+max.y-height)/2;
    draw->PushClipRect(min,max,true);
    for(size_t i=0;i<names.size();i++) {
        auto icon=texture(names[i],height);float width=height*icon->aspect;
        draw->AddImage((ImTextureID)(intptr_t)icon->texture,ImVec2(x,y),ImVec2(x+width,y+height),ImVec2(0,0),ImVec2(1,1),color);x+=width;
        if(i+1<names.size()){x+=gap;draw->AddText(ImVec2(x,(min.y+max.y-ImGui::GetFontSize())/2),color,"+");x+=plus+gap;}
    }
    draw->PopClipRect();return true;
}
void RecompDrawTabHints(const ImRect &tabs,ImVec2 panel_min,ImVec2 panel_max) {
    float pad=std::max(2.0f,ImGui::GetFontSize()*0.3f),slash=ImGui::CalcTextSize("/").x;
    float available=std::min(tabs.Min.x-panel_min.x,panel_max.x-tabs.Max.x)-2*pad;
    // Old visible keyboard height was 1.6 * 48/64 = 1.2 font heights.
    // Slightly enlarge it while matching all FOUR visible glyph heights.
    float height=std::min(ImGui::GetFontSize()*1.35f,tabs.Max.y-panel_min.y-2*pad);
    if(height<=0 || available<=slash+2*pad)return;
    const char *names[]={"keyboard_q_outline.svg","xbox_lb_outline.svg","keyboard_e_outline.svg","xbox_rb_outline.svg"};
    IconTexture *icons[4];float aspect=0;
    for(int i=0;i<4;i++){icons[i]=texture(names[i],height);if(!icons[i])return;}
    aspect=std::max(icons[0]->ink_aspect+icons[1]->ink_aspect,icons[2]->ink_aspect+icons[3]->ink_aspect);
    height=std::floor(std::min(height,(available-slash-2*pad)/aspect));if(height<1)return;
    float cy=(panel_min.y+tabs.Max.y)/2,y=std::floor(cy-height/2);
    ImDrawList *draw=ImGui::GetWindowDrawList();
    for(int i=0;i<2;i++) {
        float center=i?(tabs.Max.x+panel_max.x)/2:(panel_min.x+tabs.Min.x)/2;
        float total=height*(icons[i*2]->ink_aspect+icons[i*2+1]->ink_aspect)+slash+pad*2;
        float x=center-total/2;
        for(int n=0;n<2;n++) {
            IconTexture *icon=icons[i*2+n];float width=height*icon->ink_aspect;
            draw->AddImage((ImTextureID)(intptr_t)icon->texture,ImVec2(x,y),ImVec2(x+width,y+height),icon->ink_min,icon->ink_max);x+=width;
            if(!n) {
                x+=pad;
                // Center the actual slash ink rather than the font's line box.
                const ImFontGlyph *glyph=ImGui::GetFont()->FindGlyph('/');
                float scale=ImGui::GetFontSize()/ImGui::GetFont()->FontSize;
                float slash_y=y+height/2-(glyph->Y0+glyph->Y1)*scale/2;
                draw->AddText(ImVec2(x,slash_y),IM_COL32_WHITE,"/");x+=slash+pad;
            }
        }
    }
}
