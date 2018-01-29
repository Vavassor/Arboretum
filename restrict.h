#ifndef RESTRICT_H_
#define RESTRICT_H_

#include "platform_definitions.h"

#if defined(COMPILER_MSVC)
#define RESTRICT __restrict
#elif defined(COMPILER_GCC)
#define RESTRICT __restrict__
#endif

#endif // RESTRICT_H_
