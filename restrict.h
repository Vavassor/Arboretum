#ifndef RESTRICT_H_
#define RESTRICT_H_

#include "platform_definitions.h"

#if defined(COMPILER_MSVC)
#define RESTRICT __restrict
#else
#define RESTRICT restrict
#endif

#endif // RESTRICT_H_
