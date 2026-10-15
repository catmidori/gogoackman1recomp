#pragma once
#include "recomp_menu_types.h"
// All menu checkbox APIs share this row layout and single-border renderer.
bool RecompMenuCheckbox(const char *label,bool *value,bool initial_focus=false,float label_indent=0);
// Use these shared row primitives for all sub-items in every tab.
float RecompMenuRowPadding();
bool RecompMenuButton(const char *label,bool initial_focus=false,bool compact=false,bool left_aligned=false);
// Mods-only selection row: no hover teleport, persistent model selection.
bool RecompMenuModRow(const char *label,bool selected);
// Global dropdown rule: stable label/ID, invariant text position and the
// Save/Load disclosure triangle. Keep keyboard/gamepad focus on the header.
bool RecompMenuDropdown(const char *label,bool *open,bool initial_focus=false,bool left_aligned=false,float label_indent=0);
bool RecompMenuActionButton(const char *label,bool initial_focus=false);
bool RecompMenuBindingRow(const char *label,const char *value,const char *conflict,
                         bool initial_focus=false,bool capturing=false,const char *binding=nullptr,bool gamepad=false,
                         const RecompConflict *conflicts=nullptr,size_t conflict_count=0);
bool RecompMenuSelector(const char *label,const char *const *choices,int count,int *value);
// Register ALL choices (including hidden tabs) once before rendering selectors.
void RecompMenuResetSelectorOptions();
void RecompMenuRegisterSelectorOptions(const char *const *choices,int count);
float RecompMenuSelectorWidth();

// Resolve mouse activity against last-frame rows before drawing any highlight.
void RecompMenuBeginFocusFrame(bool visible,bool mouse_activity=false);
struct ImRect;
// Route first-row Up to the OPEN tab, and any tab-label Up to the open
// content's final enabled row. Includes independently scrolling children.
void RecompMenuWrapVertical(unsigned int tab_id,const ImRect &tab_rect);
