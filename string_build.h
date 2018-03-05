#ifndef STRING_BUILD_H_
#define STRING_BUILD_H_

struct Heap;
char* copy_string_to_heap(const char* original, int original_size, Heap* heap);
char* copy_string_onto_heap(const char* original, Heap* heap);
void replace_string(char** original, const char* new_string, Heap* heap);

char* append_to_path(const char* path, const char* segment, Heap* heap);
char* insert_string(const char* string, const char* insert, int index, Heap* heap);
void remove_substring(char* string, int start, int end);

#endif // STRING_BUILD_H_
