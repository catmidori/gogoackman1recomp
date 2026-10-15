#pragma once
#include "ackman_log.h"
#ifdef __cplusplus
#include <cstdio>
namespace std {
using ::AckmanLogOpen; using ::AckmanLogClose; using ::AckmanLogFlush;
using ::AckmanLogPrint; using ::AckmanLogVprint; using ::AckmanLogPrintf;
using ::AckmanLogPuts; using ::AckmanLogFputs; using ::AckmanLogWrite;
}
#endif
/* Forced into the executable's C sources only. Binary assets/savestates
 * retain ordinary stdio; only registered diagnostics are queued. */
#ifndef ACKMAN_LOG_IMPLEMENTATION
#define fopen AckmanLogOpen
#define fclose AckmanLogClose
#define fflush AckmanLogFlush
#define fprintf AckmanLogPrint
#define vfprintf AckmanLogVprint
#define printf AckmanLogPrintf
#define puts AckmanLogPuts
#define fputs AckmanLogFputs
#define fwrite AckmanLogWrite
#endif
