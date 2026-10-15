#pragma once
#include "cpu_state.h"
void AckmanAudioInstall(void);
void AckmanAudioReset(void);
void AckmanAudioPoll(uint8_t acknowledgment, uint8_t music_volume, uint8_t fade_delta);
int AckmanAudioPending(void);
void AckmanAudioClose(void);
void AckmanAudioDebug(int next_track);
void AckmanAudioTrace(CpuState *cpu, uint32_t pc);
uint8_t AckmanRouteMusic(CpuState *cpu, uint8_t command);
