#ifndef INVALID_INDEX_H_
#define INVALID_INDEX_H_

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdbool.h>

extern const int invalid_index;

bool is_valid_index(int index);

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // INVALID_INDEX_H_
