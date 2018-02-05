#ifndef STRING_BUILD_H_
#define STRING_BUILD_H_

struct Heap;
char* copy_string_to_heap(const char* original, int original_size, Heap* heap);
char* copy_string_onto_heap(const char* original, Heap* heap);
void replace_string(char** original, const char* new_string, Heap* heap);

char* append_to_path(const char* path, const char* segment, Heap* heap);

#endif // STRING_BUILD_H_
