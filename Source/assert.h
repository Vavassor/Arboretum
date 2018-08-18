#ifndef ASSERT_H_
#define ASSERT_H_

void assert_fail(const char* expression, const char* file, int line);

#if defined(NDEBUG)
#define ASSERT(expression) ((void) 0)
#else
#define ASSERT(expression) ((expression) ? ((void) 0) : assert_fail(#expression, __FILE__, __LINE__))
#endif

#endif // ASSERT_H_
