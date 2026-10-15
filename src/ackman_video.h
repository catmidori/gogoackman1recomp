#pragma once
#include "desktop/display_aspect.h"
#include <stdint.h>
void AckmanVideoBlitOsd(uint8_t *dst,int pitch,int width,int height,const uint32_t *panel,
                        int panel_width,int panel_height,int scale_div,int render_scale,bool volume);
void AckmanVideoSetWidescreen(int active);
int AckmanVideoWidescreen(void);
int AckmanVideoInternalScale(void);
void AckmanVideoFrameSize(int drawable_width,int drawable_height,int *width,int *height);
void AckmanVideoComputeViewport(int frame_width,int frame_height,int drawable_width,int drawable_height,
                               SnesDisplayViewport *viewport);
void AckmanVideoInstall(void);
void AckmanVideoBegin(void);
void AckmanVideoEnd(void);
int AckmanVideoViewportExtra(int requested);

int AckmanVideoScoreScreen(void);

void AckmanVideoScanlineBegin(int line);
void AckmanVideoScanlineEnd(void);

#include "ackman_rumble.h"
AckmanRumbleScene AckmanVideoRumbleScene(void);

bool AckmanVideoRumbleCharging(void);
bool AckmanVideoRumbleInterludeContact(void);
bool AckmanVideoRumbleCastleIgnition(void);
bool AckmanVideoRumbleClimaxFlash(void);
