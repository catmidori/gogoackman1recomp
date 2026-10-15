#pragma once
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define ACKMAN_TRANSLATION_MD5 "0dd930e3738524cc4f8a3d83326616a9"
#define ACKMAN_TRANSLATION_IPS_SIZE 19872
#define ACKMAN_TRANSLATION_PACKAGE "GoGoAckman_EN.snesmod"
#define ACKMAN_TRANSLATION_PTBR_MD5 "7cc729dde829946791d18a2b27e48da6"
#define ACKMAN_TRANSLATION_PTBR_SIZE 9943
#define ACKMAN_TRANSLATION_PTBR_PACKAGE "GoGoAckman_PTBR.snesmod"
void AckmanTranslationMd5(const unsigned char *data,size_t size,char out[33]);
bool AckmanTranslationStart(const char *ips,const char *rom,const char *directory);
bool AckmanTranslationBusy(void);
const char *AckmanTranslationError(void);
/* Returns true once, only after a newly published package is confirmed. */
bool AckmanTranslationPoll(void);
void AckmanTranslationShutdown(void);
#ifdef __cplusplus
}
#endif
