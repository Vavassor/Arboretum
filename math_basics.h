#ifndef MATH_BASICS_H_
#define MATH_BASICS_H_

#include "platform_definitions.h"

#if defined(COMPILER_GCC)
#include <cmath>
#endif // defined(COMPILER_GCC)

using std::abs;
using std::isfinite;
using std::signbit;

#endif // MATH_BASICS_H_
