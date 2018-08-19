#include "string_build.h"

#include "assert.h"
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
    int path_size = string_size(path);
    char* extended;
    if(path_size == 1)
    {
        ASSERT(path[0] == '/');
        int size = path_size + string_size(segment) + 1;
        extended = HEAP_ALLOCATE(heap, char, size);
        format_string(extended, size, "%s%s", path, segment);
    }
    else
    {
        int size = path_size + 1 + string_size(segment) + 1;
        extended = HEAP_ALLOCATE(heap, char, size);
        format_string(extended, size, "%s/%s", path, segment);
    }
    return extended;
}

void append_string(char** buffer, const char* string, Stack* stack)
{
    int buffer_size;
    if(*buffer)
    {
        buffer_size = string_size(*buffer);
    }
    else
    {
        buffer_size = 0;
    }

    int extra = string_size(string);
    int append_at = buffer_size;
    buffer_size += extra;

    char* result = *buffer;
    result = STACK_REALLOCATE(stack, result, char, buffer_size + 1);
    copy_memory(&result[append_at], string, extra);
    result[buffer_size] = '\0';

    *buffer = result;
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

char* replace_substrings(const char* original, const char* pattern, const char* replacement, Stack* stack)
{
    // Determine the size of the result.
    int original_size = string_size(original);
    int pattern_size = string_size(pattern);
    int replacement_size = string_size(replacement);
    int count = count_substring_occurrences(original, pattern);
    int result_size = original_size + (replacement_size - pattern_size) * count;

    // Allocate the result.
    char* result = STACK_ALLOCATE(stack, char, result_size + 1);
    result[result_size] = '\0';

    // Do the replacement.
    const char* s1 = find_string(original, pattern);
    if(s1)
    {
        const char* s0 = original;
        int i = 0;
        do
        {
            int span = (int) (s1 - s0);
            copy_string(&result[i], span + 1, s0);
            i += span;
            copy_string(&result[i], replacement_size + 1, replacement);
            i += replacement_size;
            s0 = s1 + pattern_size;
            s1 = find_string(s0, pattern);
        } while(s1);

        int remaining = (int) ((original + original_size) - s0);
        if(remaining > 0)
        {
            copy_string(&result[i], remaining + 1, s0);
        }
    }
    else
    {
        copy_string(result, result_size + 1, original);
    }

    return result;
}
