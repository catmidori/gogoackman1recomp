#pragma once
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Returns -1 in the game child, otherwise the supervisor's exit status. */
int RecompMenuBootEnter(int *argc,char **argv);
void RecompMenuBootPresent(bool dialog);
void RecompMenuBootShaderFailure(void);
bool RecompMenuBootResume(void);
bool RecompMenuBootStable(void);
void RecompMenuBootApplying(bool applying);
void RecompMenuBootResumed(bool success);
#ifdef __cplusplus
}
#endif
