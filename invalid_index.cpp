#include "invalid_index.h"

extern const int invalid_index = -1;

bool is_valid_index(int index)
{
    return index != invalid_index;
}
