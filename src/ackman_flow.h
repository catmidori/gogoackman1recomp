#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
bool AckmanRunFrame(uint32_t inputs);
void AckmanFlowShade(uint8_t *pixels,size_t pitch,int width,int height);

void AckmanReboot(void);

// Scene presentation changes commit only at an actually black rendered frame.
int AckmanFlowVideoPolicy(int requested);
void AckmanFlowVideoEndpoint(bool black);
/* Process-local rewind presentation state; portable save format is unchanged. */
void AckmanFlowSaveRewind(uint32_t out[5]);
void AckmanFlowLoadRewind(const uint32_t in[5]);
