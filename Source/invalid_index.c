#include "invalid_index.h"

// In many circumstances, such as when an index isn't found or when an index
// is uninitialised, it's useful to have a "non-index" value. This is commonly
// -1 by convention. The purpose here is to explicitly give it a standard name
// rather than arbitrary checks for "result == -1" or setting something to -1.

const int invalid_index = -1;

bool is_valid_index(int index)
{
    return index != invalid_index;
}
