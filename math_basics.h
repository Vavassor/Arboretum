#ifndef MATH_BASICS_H_
#define MATH_BASICS_H_

#include "platform_definitions.h"

#if defined(COMPILER_GCC)
#include <math.h>
#elif defined(COMPILER_MSVC)
#include <math.h>
#endif // defined(COMPILER_MSVC)

#undef log // Collides with log.h! We're using logf() mostly, anyhow.

#endif // MATH_BASICS_H_
