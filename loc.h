#ifndef LOC_H_
#define LOC_H_

#if defined(__cplusplus)
extern "C" {
#endif

#include "platform.h"

#include <stdbool.h>

// Localized Text File (.loc)
bool loc_load_file(Platform* platform, const char* path);

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // LOC_H_
