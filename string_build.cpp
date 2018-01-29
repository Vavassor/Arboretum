#include "string_build.h"

#include "memory.h"
#include "string_utilities.h"

char* copy_string_to_heap(const char* original, int original_size, Heap* heap)
{
	original_size += 1;
	char* copy = HEAP_ALLOCATE(heap, char, original_size);
	copy_string(copy, original_size, original);
	return copy;
}

char* copy_string_onto_heap(const char* original, Heap* heap)
{
	int size = string_size(original);
	return copy_string_to_heap(original, size, heap);
}

char* append_to_path(const char* path, const char* segment, Heap* heap)
{
	int size = string_size(path) + 1 + string_size(segment) + 1;
	char* extended = HEAP_ALLOCATE(heap, char, size);
	format_string(extended, size, "%s/%s", path, segment);
	return extended;
}
