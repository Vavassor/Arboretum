#include "string_build.h"

#include "assert.h"
#include "memory.h"
#include "string_utilities.h"

char* copy_chars_to_heap(const char* original, int original_size, Heap* heap)
{
    original_size += 1;
    char* copy = HEAP_ALLOCATE(heap, char, original_size);
    copy_string(copy, original_size, original);
    return copy;
}

char* copy_string_to_heap(const char* original, Heap* heap)
{
    int size = string_size(original);
    return copy_chars_to_heap(original, size, heap);
}

char* copy_chars_to_stack(const char* original, int original_size, Stack* stack)
{
    original_size += 1;
    char* copy = STACK_ALLOCATE(stack, char, original_size);
    copy_string(copy, original_size, original);
    return copy;
}

char* copy_string_to_stack(const char* original, Stack* stack)
{
    int size = string_size(original);
    return copy_chars_to_stack(original, size, stack);
}

void replace_string(char** original, const char* new_string, Heap* heap)
{
    if(*original)
    {
        HEAP_DEALLOCATE(heap, *original);
    }
    *original = copy_string_to_heap(new_string, heap);
}

char* append_to_path(const char* path, const char* segment, Heap* heap)
{
    int size = string_size(path) + 1 + string_size(segment) + 1;
    char* extended = HEAP_ALLOCATE(heap, char, size);
    format_string(extended, size, "%s/%s", path, segment);
    return extended;
}

char* insert_string(const char* string, const char* insert, int index, Heap* heap)
{
    int outer_size = string_size(string);
    int size = outer_size + string_size(insert) + 1;
    char* result = HEAP_ALLOCATE(heap, char, size);
    int so_far = 0;
    if(index > 0)
    {
        so_far += copy_string(&result[so_far], index + 1, string);
    }
    so_far += copy_string(&result[so_far], size - so_far, insert);
    if(index < size - 1)
    {
        so_far += copy_string(&result[so_far], size - so_far, &string[index]);
    }
    return result;
}

void remove_substring(char* string, int start, int end)
{
    if(end < start)
    {
        int temp = end;
        end = start;
        start = temp;
    }
    int size = string_size(string);
    if(start >= 0 && end <= size)
    {
        int after_size = size - end;
        if(after_size > 0)
        {
            move_memory(&string[start], &string[end], after_size);
        }
        int new_end = size - (end - start);
        string[new_end] = '\0';
    }
}
