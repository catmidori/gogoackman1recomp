#pragma once
enum { RECOMP_SNES_BUTTON_COUNT=12,RECOMP_MENU_TAB_COUNT=8 };
static const char *const RecompSnesButtonNames[12]={"UP","DOWN","LEFT","RIGHT","A","B","X","Y","L","R","SELECT","START"};
static const char *const RecompSnesButtonKeys[12]={"up","down","left","right","a","b","x","y","l","r","select","start"};
static const int RecompSnesKeyboardIndices[12]={8,9,10,11,0,1,2,3,4,5,7,6};
static const int RecompSnesRunnerBits[12]={4,5,6,7,8,0,9,1,10,11,2,3};
static const char *const RecompMenuTabs[8]={"File","Game","Graphics","Audio","Controls","Hotkeys","Mods","Shaders"};
