#include "recomp_menu_widgets.h"
#include "recomp_menu_icons.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include <cfloat>
#include <SDL3/SDL.h>
static void highlight(ImVec2 min,ImVec2 max,bool initial_focus,bool register_row=true,bool include_focus=true);
struct MenuFocusRow { ImGuiID id,scope;ImGuiWindow *window;ImGuiNavLayer layer;ImRect rect; };
static std::vector<MenuFocusRow> focus_rows;
static std::vector<MenuFocusRow> navigation_rows;
static ImGuiID wrapped_row,wrapped_scope;
static ImGuiContext *focus_context;
void RecompMenuBeginFocusFrame(bool visible,bool mouse_activity) {
    ImGuiContext &ctx=*ImGui::GetCurrentContext();ImGuiIO &io=ctx.IO;
    if(focus_context!=&ctx) {focus_rows.clear();wrapped_row=0;focus_context=&ctx;}
    // Selection models in child lists observe the focus change on their next
    // submission, after NewFrame has cleared ImGui's transient move marker.
    if(visible && wrapped_row && ctx.NavId==wrapped_row) {
        ctx.NavJustMovedToId=wrapped_row;ctx.NavJustMovedToFocusScopeId=wrapped_scope;
    }
    wrapped_row=0;
    bool activity=mouse_activity || io.MouseDelta.x!=0 || io.MouseDelta.y!=0;
    for(int i=0;i<ImGuiMouseButton_COUNT;i++)activity|=io.MouseClicked[i];
    if(visible && activity && SDL_CursorVisible() && !(io.ConfigFlags&ImGuiConfigFlags_NoMouse) &&
       (io.ConfigFlags&ImGuiConfigFlags_NavEnableKeyboard)) {
        for(auto it=focus_rows.rbegin();it!=focus_rows.rend();++it) {
            if(ctx.HoveredWindow!=it->window || !it->rect.Contains(io.MousePos))continue;
            ImGui::FocusWindow(it->window);
            ImGui::SetNavID(it->id,it->layer,it->scope,ImGui::WindowRectAbsToRel(it->window,it->rect));
            ctx.NavInputSource=ImGuiInputSource_Mouse;ImGui::SetNavCursorVisible(true);
            break;
        }
    }
    focus_rows.clear();
    navigation_rows.clear();
}

void RecompMenuWrapVertical(unsigned int tab_id,const ImRect &tab_rect) {
    ImGuiContext &ctx=*ImGui::GetCurrentContext();
    if(ImGui::IsKeyPressed(ImGuiKey_Home) && !ctx.OpenPopupStack.Size) {
        ImGuiWindow *window=ctx.CurrentWindow;
        ImGui::NavMoveRequestCancel();ImGui::ClearActiveID();ImGui::FocusWindow(window);
        ImGui::SetNavID(tab_id,ImGuiNavLayer_Main,ctx.CurrentFocusScopeId,ImGui::WindowRectAbsToRel(window,tab_rect));
        ImGui::SetNavCursorVisible(true);return;
    }
    if(navigation_rows.empty() || ctx.OpenPopupStack.Size)return;
    ImGuiDir direction=ctx.NavMoveSubmitted?ctx.NavMoveDir:ImGuiDir_None;
    // Dear ImGui's native left stick scrolls rather than moving item focus.
    // Use the same row navigation as the D-pad, with ImGui's repeat timing.
    bool analog=false;
    if(ImGui::IsKeyPressed(ImGuiKey_GamepadLStickUp,ImGuiInputFlags_Repeat)) {direction=ImGuiDir_Up;analog=true;}
    if(ImGui::IsKeyPressed(ImGuiKey_GamepadLStickDown,ImGuiInputFlags_Repeat)) {direction=ImGuiDir_Down;analog=true;}
    // A tab label can be focused without selecting its contents. Navigation
    // always uses rows from the open tab, never the horizontally focused label.
    bool focused_tab=false;
    if(ImGuiTabBar *bar=ctx.TabBars.GetByKey(ctx.CurrentWindow->GetID("Sections")))
        for(const ImGuiTabItem &tab:bar->Tabs)
            focused_tab|=ctx.NavWindow==ctx.CurrentWindow && ctx.NavId==tab.ID;
    const MenuFocusRow &first=navigation_rows.front(),&last=navigation_rows.back();
    bool to_last=focused_tab && direction==ImGuiDir_Up;
    bool home=ImGui::IsKeyPressed(ImGuiKey_Home);
    bool to_tab=home || (ctx.NavId==last.id && direction==ImGuiDir_Down) ||
                (ctx.NavId==first.id && direction==ImGuiDir_Up);
    const MenuFocusRow *target=to_last?&last:
        focused_tab && direction==ImGuiDir_Down?&first:nullptr;
    if(analog && !to_last && !to_tab) {
        for(size_t i=0;i<navigation_rows.size();i++)if(navigation_rows[i].id==ctx.NavId) {
            if(direction==ImGuiDir_Up) {if(i)target=&navigation_rows[i-1];else to_tab=true;}
            else if(i+1<navigation_rows.size())target=&navigation_rows[i+1];
            break;
        }
    }
    if(!target && !to_tab)return;
    ImGuiWindow *window=target?target->window:ctx.CurrentWindow;
    ImRect rect=target?target->rect:tab_rect;
    ImGuiID scope=target?target->scope:ctx.CurrentFocusScopeId;
    ImGui::NavMoveRequestCancel();
    ImGui::ClearActiveID();ImGui::FocusWindow(window);
    ImGuiID id=target?target->id:tab_id;
    ImGui::SetNavID(id,ImGuiNavLayer_Main,scope,ImGui::WindowRectAbsToRel(window,rect));
    ctx.NavJustMovedToId=id;ctx.NavJustMovedToFocusScopeId=scope;
    wrapped_row=id;wrapped_scope=scope;
    if(analog)ctx.NavInputSource=ImGuiInputSource_Gamepad;
    ImGui::SetNavCursorVisible(true);
    ImGui::ScrollToRectEx(window,rect,ImGuiScrollFlags_KeepVisibleEdgeY);
}


// Register every selector's options before drawing any tab. Copies outlive callers;
// measure against the current font each time so resizing/DPI changes stay uniform.
static std::vector<std::string> selector_options;
void RecompMenuResetSelectorOptions() { selector_options.clear(); }
void RecompMenuRegisterSelectorOptions(const char *const *choices,int count) {
    for(int i=0;i<count;i++) {
        if(std::find(selector_options.begin(),selector_options.end(),choices[i])==selector_options.end())
            selector_options.emplace_back(choices[i]);
    }
}
float RecompMenuSelectorWidth() {
    float widest=0;
    for(const std::string &option:selector_options)
        widest=std::max(widest,ImGui::CalcTextSize(option.c_str()).x);
    return std::ceil(widest)+2*RecompMenuRowPadding()+ImGui::CalcTextSize("<").x+ImGui::CalcTextSize(">").x;
}

bool RecompMenuSelector(const char *label,const char *const *choices,int count,int *value) {
    ImGuiWindow *window=ImGui::GetCurrentWindow();
    if(window->SkipItems || count<=0)return false;
    ImGuiContext &ctx=*ImGui::GetCurrentContext();
    ImGuiID id=ImGui::GetID(label);
    bool focused=ctx.NavId==id;
    const ImGuiKey keys[]={ImGuiKey_LeftArrow,ImGuiKey_RightArrow,ImGuiKey_GamepadDpadLeft,ImGuiKey_GamepadDpadRight};
    int delta=0;
    if(focused) for(int i=0;i<4;i++) {
        ImGui::SetKeyOwner(keys[i],id,ImGuiInputFlags_LockThisFrame);
        if(ImGui::IsKeyPressed(keys[i],ImGuiInputFlags_None,id))delta=(i&1)?1:-1;
    }
    ImVec2 min=ImGui::GetCursorScreenPos();
    min.x=std::ceil(min.x);ImGui::SetCursorScreenPos(min);
    float width=(ctx.NextItemData.HasFlags&ImGuiNextItemDataFlags_HasWidth)?ctx.NextItemData.Width:ImGui::GetContentRegionAvail().x;
    width=std::floor(min.x+width)-min.x;
    float height=ImGui::GetFrameHeight(),padding=RecompMenuRowPadding();
    ImVec2 max(min.x+width,min.y+height);
    ImGui::PushClipRect(ImVec2(min.x,std::max(min.y,window->ClipRect.Min.y)),ImVec2(max.x,std::min(max.y,window->ClipRect.Max.y)),false);
    ImGui::PushStyleColor(ImGuiCol_NavCursor,ImVec4(0,0,0,0));
    bool pressed=ImGui::InvisibleButton(label,ImVec2(width,height),ImGuiButtonFlags_EnableNav);
    ImGui::PopStyleColor();
    const float left_width=ImGui::CalcTextSize("<").x;
    if(focused && pressed && (ctx.NavActivateId==id || ctx.NavActivatePressedId==id))delta=1;
    const float right_width=ImGui::CalcTextSize(">").x;
    const float x=max.x-padding-RecompMenuSelectorWidth();
    const float right=max.x-padding-right_width;
    const float inner_left=x+left_width+padding,inner_right=right-padding;
    if(pressed && ImGui::IsItemHovered()) {
        float mouse=ImGui::GetIO().MousePos.x;
        if(mouse>=x && mouse<inner_left)delta=-1;
        else if(mouse>=inner_right && mouse<max.x-padding)delta=1;
    }
    if(delta) {
        *value=(*value+delta+count)%count;
        ImGui::MarkItemEdited(id);
    }
    highlight(min,max,false);
    ImDrawList *draw=ImGui::GetWindowDrawList();
    draw->PushClipRect(ImVec2(min.x+padding,min.y),ImVec2(x-padding,max.y),true);
    draw->AddText(ImVec2(min.x+padding,min.y+ImGui::GetStyle().FramePadding.y),ImGui::GetColorU32(ImGuiCol_Text),label);
    draw->PopClipRect();
    const float y=min.y+ImGui::GetStyle().FramePadding.y;
    const ImU32 color=ImGui::GetColorU32(ImGuiCol_Text);
    draw->AddText(ImVec2(x,y),color,"<");
    draw->AddText(ImVec2((inner_left+inner_right-ImGui::CalcTextSize(choices[*value]).x)/2,y),color,choices[*value]);
    draw->AddText(ImVec2(right,y),color,">");
    ImGui::PopClipRect();
    return delta!=0;
}

float RecompMenuRowPadding() {
    return std::max(2.0f,std::ceil(ImGui::CalcTextSize(" ").x));
}

// Borders are filled strips INSIDE the item rectangle. Neither frame borders
// nor ImGui's expanded navigation cursor may draw a second outline.
static void border(ImDrawList *draw,ImVec2 min,ImVec2 max,ImU32 color) {
    // Fractional five-percent insets must not clip the left filled strip.
    min.x=std::ceil(min.x);max.x=std::floor(max.x);
    draw->AddRectFilled(min,ImVec2(max.x,min.y+1),color);
    draw->AddRectFilled(ImVec2(min.x,max.y-1),max,color);
    draw->AddRectFilled(min,ImVec2(min.x+1,max.y),color);
    draw->AddRectFilled(ImVec2(max.x-1,min.y),max,color);
}

static void highlight(ImVec2 min,ImVec2 max,bool initial_focus,bool register_row,bool include_focus) {
    ImGuiContext &ctx=*ImGui::GetCurrentContext();
    if(!(ctx.LastItemData.ItemFlags&ImGuiItemFlags_Disabled))
        navigation_rows.push_back({ctx.LastItemData.ID,ctx.CurrentFocusScopeId,ctx.CurrentWindow,
                                  ctx.CurrentWindow->DC.NavLayerCurrent,ImRect(min,max)});
    if(register_row && !(ctx.LastItemData.ItemFlags&ImGuiItemFlags_Disabled))
        focus_rows.push_back({ctx.LastItemData.ID,ctx.CurrentFocusScopeId,ctx.CurrentWindow,
                              ctx.CurrentWindow->DC.NavLayerCurrent,ImRect(min,max)});
    bool focused=initial_focus || (include_focus && ImGui::IsItemFocused());
    if(focused) {
        ImDrawList *draw=ImGui::GetWindowDrawList();
        draw->AddRectFilled(min,max,ImGui::GetColorU32(ImGuiCol_HeaderHovered));
        border(draw,min,max,ImGui::GetColorU32(ImGuiCol_Border));
    }
}

static bool row_button(const char *label,bool initial_focus,bool compact,bool left_aligned,bool *open,bool selection_row=false,float label_indent=0) {
    if(ImGui::GetCurrentWindow()->SkipItems) return false;
    ImVec2 min=ImGui::GetCursorScreenPos();
    min.x=std::ceil(min.x);ImGui::SetCursorScreenPos(min);
    float height=compact?ImGui::GetFontSize():ImGui::GetFrameHeight();
    // Round the right edge inward, so the entire one-pixel border lies within
    // the usable content region at fractional font/DPI scales.
    ImGuiContext &ctx=*ImGui::GetCurrentContext();
    bool full_row=(ctx.NextItemData.HasFlags&ImGuiNextItemDataFlags_HasWidth)!=0;
    float available=full_row?ctx.NextItemData.Width:ImGui::GetContentRegionAvail().x;
    float right=full_row?min.x+available:std::min(min.x+available,ImGui::GetCurrentWindow()->WorkRect.Max.x);
    float width=std::floor(right)-min.x;
    ImVec2 max(min.x+width,min.y+height);
    // Full-width rows may cross table columns, never the scrolling child's
    // vertical viewport. Otherwise folder headers leak above/below the panel.
    ImRect clip(min,max);
    clip.Min.y=std::max(clip.Min.y,ImGui::GetCurrentWindow()->InnerClipRect.Min.y);
    clip.Max.y=std::min(clip.Max.y,ImGui::GetCurrentWindow()->InnerClipRect.Max.y);
    ImGui::PushClipRect(clip.Min,clip.Max,!full_row);
    ImGui::PushStyleColor(ImGuiCol_NavCursor,ImVec4(0,0,0,0));
    bool pressed=ImGui::InvisibleButton(label,ImVec2(width,height),ImGuiButtonFlags_EnableNav);
    ImGui::PopStyleColor();
    if(open && pressed) {
        *open=!*open;
        if(ctx.NavId==ImGui::GetItemID() &&
           (ctx.NavInputSource==ImGuiInputSource_Keyboard || ctx.NavInputSource==ImGuiInputSource_Gamepad))
            ImGui::SetNavCursorVisible(true);
    }
    highlight(min,max,initial_focus,!selection_row,!selection_row);
    float padding=RecompMenuRowPadding();
    const char *end=ImGui::FindRenderedTextEnd(label);
    ImVec2 text=ImGui::CalcTextSize(label,end);
    float h=ImGui::GetFontSize()*0.25f;
    float inset=padding+(open?4*h:0);
    ImVec2 pos(min.x+label_indent+(left_aligned?inset:std::max(inset,(width-text.x)/2)),min.y+(height-text.y)/2);
    if(open) {
        // Exactly the original Save/Load geometry, independent of rotation.
        ImVec2 p(pos.x-h*3,(min.y+max.y)/2);
        ImDrawList *draw=ImGui::GetWindowDrawList();ImU32 color=ImGui::GetColorU32(ImGuiCol_Text);
        if(*open)draw->AddTriangleFilled(ImVec2(p.x-h,p.y-h/2),ImVec2(p.x+h,p.y-h/2),ImVec2(p.x,p.y+h),color);
        else draw->AddTriangleFilled(ImVec2(p.x-h/2,p.y-h),ImVec2(p.x-h/2,p.y+h),ImVec2(p.x+h,p.y),color);
    }
    ImGui::GetWindowDrawList()->PushClipRect(ImVec2(min.x+padding,min.y),ImVec2(max.x-padding,max.y),true);
    ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(),ImGui::GetFontSize(),pos,
        ImGui::GetColorU32(ImGuiCol_Text),label,end,0,nullptr);
    ImGui::GetWindowDrawList()->PopClipRect();
    ImGui::PopClipRect();
    return pressed;
}
bool RecompMenuButton(const char *label,bool initial_focus,bool compact,bool left_aligned) {
    return row_button(label,initial_focus,compact,left_aligned,nullptr);
}
bool RecompMenuModRow(const char *label,bool selected) {
    return row_button(label,selected,false,true,nullptr,true);
}
bool RecompMenuDropdown(const char *label,bool *open,bool initial_focus,bool left_aligned,float label_indent) {
    return row_button(label,initial_focus,false,left_aligned,open,false,label_indent);
}

bool RecompMenuActionButton(const char *label,bool initial_focus) {
    ImGuiWindow *window=ImGui::GetCurrentWindow();if(window->SkipItems)return false;
    ImGuiContext &ctx=*ImGui::GetCurrentContext();float padding=RecompMenuRowPadding();
    ImVec2 text=ImGui::CalcTextSize(label),min=ImGui::GetCursorScreenPos();
    min.x=std::ceil(min.x);ImGui::SetCursorScreenPos(min);
    float width=(ctx.NextItemData.HasFlags&ImGuiNextItemDataFlags_HasWidth)?ctx.NextItemData.Width:ImGui::GetContentRegionAvail().x;
    ImVec2 max(std::floor(min.x+width),min.y+ImGui::GetFrameHeight());
    ImGui::PushClipRect(ImVec2(min.x,std::max(min.y,window->ClipRect.Min.y)),ImVec2(max.x,std::min(max.y,window->ClipRect.Max.y)),false);ImGui::PushStyleColor(ImGuiCol_NavCursor,ImVec4(0,0,0,0));
    bool pressed=ImGui::InvisibleButton(label,ImVec2(max.x-min.x,max.y-min.y),ImGuiButtonFlags_EnableNav);
    ImGui::PopStyleColor();highlight(min,max,initial_focus);
    ImDrawList *draw=ImGui::GetWindowDrawList();
    // Keep the static outline one physical pixel inside the vertical cell;
    // fractional row heights must not clip its bottom strip.
    ImVec2 box_min(min.x,std::ceil(min.y)+1);
    ImVec2 box_max(min.x+std::ceil(text.x+2*padding),std::floor(max.y)-1);
    border(draw,box_min,box_max,IM_COL32(255,255,255,255));
    draw->AddText(ImVec2(min.x+padding,min.y+(max.y-min.y-text.y)/2),ImGui::GetColorU32(ImGuiCol_Text),label);
    ImGui::PopClipRect();return pressed;
}

struct ConflictPiece {
    std::string text;RecompDeviceImage image{};bool device=false;float x=0,y=0,width=0;
};
static std::vector<ConflictPiece> conflict_layout(const RecompConflict *entries,size_t count,
                                                 float width,float size) {
    std::vector<ConflictPiece> pieces;float x=0,y=0;
    ImFont *font=ImGui::GetFont();
    auto add=[&](ConflictPiece piece) {
        if(x>0 && x+piece.width>width){x=0;y+=size;}
        piece.x=x;piece.y=y;pieces.push_back(piece);x+=piece.width;
    };
    auto text=[&](const std::string &value) {
        // Wrap between words, preserving the sentence's inline device symbols.
        size_t begin=0;
        while(begin<value.size()) {
            size_t end=value.find(' ',begin);end=end==std::string::npos?value.size():end+1;
            ConflictPiece piece;piece.text=value.substr(begin,end-begin);
            piece.width=font->CalcTextSizeA(size,FLT_MAX,0,piece.text.c_str()).x;
            add(piece);begin=end;
        }
    };
    if(count)text("Already used in ");
    for(size_t i=0;i<count;i++) {
        if(i)text(", ");
        if(entries[i].origin!=RECOMP_CONFLICT_HOTKEY) {
            ConflictPiece piece;piece.device=true;
            if(RecompDeviceIcon(entries[i].origin==RECOMP_CONFLICT_GAMEPAD,size,&piece.image)) {
                piece.width=piece.image.size.x;add(piece);text(" ");
            }
        }
        text(std::string("[")+entries[i].name+"]");
    }
    return pieces;
}
static void draw_conflicts(const std::vector<ConflictPiece> &pieces,ImVec2 pos,float size,ImVec2 clip_max) {
    ImGuiContext &ctx=*ImGui::GetCurrentContext();ImGuiWindow *window=ctx.CurrentWindow;
    ImGuiLastItemData last=ctx.LastItemData;
    ImVec2 cursor=window->DC.CursorPos,cursor_max=window->DC.CursorMaxPos,prev=window->DC.CursorPosPrevLine;
    ImVec2 current_line=window->DC.CurrLineSize,previous_line=window->DC.PrevLineSize;
    float current_base=window->DC.CurrLineTextBaseOffset,previous_base=window->DC.PrevLineTextBaseOffset;
    bool same_line=window->DC.IsSameLine,set_pos=window->DC.IsSetPos;
    float scale=window->FontWindowScale;
    ImGui::SetWindowFontScale(scale*size/ImGui::GetFontSize());
    ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(1,64.0f/255,64.0f/255,1));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,ImVec2(0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_ImageBorderSize,0);
    ImGui::PushClipRect(pos,clip_max,true);
    float line=-1;
    for(const ConflictPiece &piece:pieces) {
        if(line==piece.y)ImGui::SameLine(0,0);
        else {window->DC.CurrLineSize=ImVec2(0,0);window->DC.CurrLineTextBaseOffset=0;window->DC.IsSameLine=false;}
        ImGui::SetCursorScreenPos(ImVec2(pos.x+piece.x,pos.y+piece.y));
        if(piece.device)ImGui::ImageWithBg(piece.image.texture,piece.image.size,piece.image.uv_min,piece.image.uv_max,
                                          ImVec4(0,0,0,0),ImVec4(1,64.0f/255,64.0f/255,1));
        else ImGui::TextUnformatted(piece.text.c_str());
        line=piece.y;
    }
    ImGui::PopClipRect();ImGui::PopStyleVar(2);ImGui::PopStyleColor();ImGui::SetWindowFontScale(scale);
    // These inline decorations must not steal the row's focus/last-item ID or
    // advance the table cursor a second time after InvisibleButton sized it.
    window->DC.CursorPos=cursor;window->DC.CursorMaxPos=cursor_max;window->DC.CursorPosPrevLine=prev;
    window->DC.CurrLineSize=current_line;window->DC.PrevLineSize=previous_line;
    window->DC.CurrLineTextBaseOffset=current_base;window->DC.PrevLineTextBaseOffset=previous_base;
    window->DC.IsSameLine=same_line;window->DC.IsSetPos=set_pos;ctx.LastItemData=last;
}
bool RecompMenuBindingRow(const char *label,const char *value,const char *conflict,bool initial_focus,bool capturing,const char *binding,bool gamepad,
                         const RecompConflict *conflicts,size_t conflict_count) {
    ImGuiWindow *window=ImGui::GetCurrentWindow();if(window->SkipItems)return false;
    ImGuiContext &ctx=*ImGui::GetCurrentContext();float padding=RecompMenuRowPadding();
    ImVec2 min=ImGui::GetCursorScreenPos();min.x=std::ceil(min.x);ImGui::SetCursorScreenPos(min);
    float width=(ctx.NextItemData.HasFlags&ImGuiNextItemDataFlags_HasWidth)?ctx.NextItemData.Width:ImGui::GetContentRegionAvail().x;
    width=std::floor(min.x+width)-min.x;
    ImGuiTable *table=ImGui::GetCurrentTable();IM_ASSERT(table && table->ColumnsCount==3);
    float middle_min=table->Columns[1].WorkMinX+padding,middle_max=table->Columns[1].WorkMaxX-padding;
    float warning_min=table->Columns[2].WorkMinX+padding,warning_width=std::max(1.0f,table->Columns[2].WorkMaxX-padding-warning_min);
    ImFont *font=ImGui::GetFont();float size=ImGui::GetFontSize(),small=size*0.75f;
    std::string warning;
    if(*conflict) {
        std::string functions=conflict;size_t pos=0;
        while((pos=functions.find(", ",pos))!=std::string::npos) {functions.replace(pos,2,"], [");pos+=4;}
        warning="Already used in ["+functions+"]";
    }
    ImVec2 value_size=ImGui::CalcTextSize(value);
    ImVec2 warning_size=warning.empty()?ImVec2(0,0):font->CalcTextSizeA(small,FLT_MAX,warning_width,warning.c_str());
    std::vector<ConflictPiece> pieces=conflict_layout(conflicts,conflict_count,warning_width,small);
    if(!pieces.empty())warning_size=ImVec2(warning_width,pieces.back().y+small);
    float icon_height=size*1.6f;
    float height=std::max(ImGui::GetFrameHeight(),std::max(icon_height,warning_size.y)+2*ImGui::GetStyle().FramePadding.y);
    ImVec2 max(min.x+width,min.y+height);
    ImGui::PushClipRect(ImVec2(min.x,std::max(min.y,window->ClipRect.Min.y)),ImVec2(max.x,std::min(max.y,window->ClipRect.Max.y)),false);ImGui::PushStyleColor(ImGuiCol_NavCursor,ImVec4(0,0,0,0));
    bool pressed=ImGui::InvisibleButton(label,ImVec2(width,height),ImGuiButtonFlags_EnableNav);ImGui::PopStyleColor();
    highlight(min,max,initial_focus || capturing);
    ImDrawList *draw=ImGui::GetWindowDrawList();
    draw->AddText(ImVec2(min.x+padding,min.y+(height-size)/2),ImGui::GetColorU32(ImGuiCol_Text),label);
    ImU32 binding_color=warning.empty()?ImGui::GetColorU32(ImGuiCol_Text):IM_COL32(255,64,64,255);
    bool icons=!capturing && binding && RecompDrawBindingIcons(draw,binding,gamepad,
        ImVec2(middle_min,min.y),ImVec2(middle_max,max.y),icon_height,binding_color);
    if(!icons && *value)draw->AddText(font,size,ImVec2((middle_min+middle_max-value_size.x)/2,min.y+(height-value_size.y)/2),ImGui::GetColorU32(ImGuiCol_Text),value);
    if(!pieces.empty())draw_conflicts(pieces,ImVec2(warning_min,min.y+(height-warning_size.y)/2),small,ImVec2(warning_min+warning_width,max.y));
    else if(!warning.empty())draw->AddText(font,small,ImVec2(warning_min,min.y+(height-warning_size.y)/2),IM_COL32(255,64,64,255),warning.c_str(),nullptr,warning_width);
    ImGui::PopClipRect();return pressed;
}

bool RecompMenuCheckbox(const char *label,bool *value,bool initial_focus,float label_indent) {
    ImGuiWindow *window=ImGui::GetCurrentWindow();
    if(window->SkipItems) return false;
    ImGuiContext &ctx=*ImGui::GetCurrentContext();
    ImVec2 min=ImGui::GetCursorScreenPos();
    min.x=std::ceil(min.x);ImGui::SetCursorScreenPos(min);
    float height=ImGui::GetFrameHeight();
    // Keep the interactive row at the shared button height. Only its square
    // shrinks, with font/DPI-aware breathing room above and below.
    float vertical_padding=std::max(2.0f,std::ceil(ImGui::GetStyle().FramePadding.y));
    float side=std::max(5.0f,std::floor(height-2*vertical_padding));
    float width=(ctx.NextItemData.HasFlags&ImGuiNextItemDataFlags_HasWidth)?ctx.NextItemData.Width:ImGui::GetContentRegionAvail().x;
    float padding=RecompMenuRowPadding();
    width=std::max(side+2*padding,width);
    width=std::floor(min.x+width)-min.x;
    ImVec2 max(min.x+width,min.y+height);
    // A table may provide guides; the interactive row spans both columns.
    ImGui::PushClipRect(ImVec2(min.x,std::max(min.y,window->ClipRect.Min.y)),ImVec2(max.x,std::min(max.y,window->ClipRect.Max.y)),false);
    ImGui::PushStyleColor(ImGuiCol_NavCursor,ImVec4(0,0,0,0));
    bool pressed=ImGui::InvisibleButton(label,ImVec2(width,height),ImGuiButtonFlags_EnableNav);
    ImGui::PopStyleColor();
    if(pressed) { *value=!*value;ImGui::MarkItemEdited(ImGui::GetItemID()); }
    ImDrawList *draw=ImGui::GetWindowDrawList();
    highlight(min,max,initial_focus);
    const char *end=ImGui::FindRenderedTextEnd(label);
    draw->PushClipRect(ImVec2(min.x+padding,min.y),ImVec2(max.x-padding-side-padding,max.y),true);
    draw->AddText(ImVec2(min.x+padding+label_indent,min.y+ImGui::GetStyle().FramePadding.y),ImGui::GetColorU32(ImGuiCol_Text),label,end);
    draw->PopClipRect();
    ImVec2 box(std::floor(max.x-padding-side),std::round(min.y+(height-side)/2));
    ImVec2 box_max(box.x+side,box.y+side);
    ImU32 white=(ImGui::GetItemFlags()&ImGuiItemFlags_Disabled)?ImGui::GetColorU32(ImGuiCol_Text):IM_COL32(255,255,255,255);
    border(draw,box,box_max,white);
    if(*value || (ImGui::GetItemFlags()&ImGuiItemFlags_MixedValue)) {
        float inset=(ImGui::GetItemFlags()&ImGuiItemFlags_MixedValue)?side/3:2;
        draw->AddRectFilled(ImVec2(box.x+inset,box.y+inset),ImVec2(box_max.x-inset,box_max.y-inset),white);
    }
    ImGui::PopClipRect();
    return pressed;
}

bool ImGui::Checkbox(const char *label,bool *value) { return RecompMenuCheckbox(label,value); }
template<class T> static bool checkbox_flags(const char *label,T *flags,T mask) {
    bool checked=(*flags&mask)==mask;
    if(!checked && (*flags&mask)!=0) ImGui::GetCurrentContext()->NextItemData.ItemFlags|=ImGuiItemFlags_MixedValue;
    bool changed=ImGui::Checkbox(label,&checked);
    if(changed) *flags=checked?(*flags|mask):(*flags&~mask);
    return changed;
}
bool ImGui::CheckboxFlags(const char *label,int *flags,int mask) { return checkbox_flags(label,flags,mask); }
bool ImGui::CheckboxFlags(const char *label,unsigned *flags,unsigned mask) { return checkbox_flags(label,flags,mask); }
bool ImGui::CheckboxFlags(const char *label,ImS64 *flags,ImS64 mask) { return checkbox_flags(label,flags,mask); }
bool ImGui::CheckboxFlags(const char *label,ImU64 *flags,ImU64 mask) { return checkbox_flags(label,flags,mask); }
