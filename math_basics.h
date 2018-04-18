#ifndef MATH_BASICS_H_
#define MATH_BASICS_H_

#include "platform_definitions.h"

#if defined(COMPILER_GCC)
#include <cmath>
#elif defined(COMPILER_MSVC)
#include <cmath>
#endif // defined(COMPILER_MSVC)

using std::abs;
using std::atan2;
using std::cos;
using std::exp;
using std::isfinite;
using std::log;
using std::signbit;
using std::sin;
using std::sqrt;

#endif // MATH_BASICS_H_
