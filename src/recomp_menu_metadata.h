#pragma once
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    char *name,*author,*version,*description;
} RecompModMetadata;
/* Read-only stored ZIP32 / MSU XML-BML preview. Initialize out to zero;
 * subsequent reads release and replace its allocated strings. */
bool RecompModMetadataRead(const char *path,RecompModMetadata *out);
void RecompModMetadataClear(RecompModMetadata *metadata);
#ifdef __cplusplus
}
#endif
