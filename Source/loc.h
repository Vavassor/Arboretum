// File Format For Localized Text (.loc)

#ifndef LOC_H_
#define LOC_H_

#include "platform.h"

#include <stdbool.h>

bool loc_load_file(Platform* platform, const char* path);

#endif // LOC_H_
