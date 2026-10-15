#pragma once
#include <stdio.h>
#include <stdarg.h>
#ifdef __cplusplus
extern "C" {
#endif
int AckmanLogInit(void);
void AckmanLogFrame(void);
void AckmanLogShutdown(void);
FILE *AckmanLogOpen(const char *,const char *);
int AckmanLogClose(FILE *);
int AckmanLogFlush(FILE *);
int AckmanLogPrint(FILE *,const char *,...);
int AckmanLogVprint(FILE *,const char *,va_list);
int AckmanLogPrintf(const char *,...);
int AckmanLogPuts(const char *);
int AckmanLogFputs(const char *,FILE *);
size_t AckmanLogWrite(const void *,size_t,size_t,FILE *);
#ifdef __cplusplus
}
#endif
