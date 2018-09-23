#include "../../Source/assert.h"
#include "../../Source/asset_paths.h"
#include "../../Source/filesystem.h"
#include "../../Source/memory.h"
#include "../../Source/string_utilities.h"
#include "../../Source/unicode.h"
#include "../../Source/unicode_grapheme_cluster_break.h"
#include "../../Source/unicode_line_break.h"
#include "../../Source/unicode_load_tables.h"
#include "../../Source/unicode_word_break.h"

#include <stdbool.h>
#include <stdio.h>

typedef enum TestType
{
    TEST_TYPE_GRAPHEME_CLUSTER_BREAK,
    TEST_TYPE_LINE_BREAK,
    TEST_TYPE_WORD_BREAK,
    TEST_TYPE_COUNT,
} TestType;

typedef struct Stream
{
    const char* at;
} Stream;

static int utf32_to_utf8(char* output, char32_t code)
{
    if(code <= 0x7f)
    {
        output[0] = code;
        return 1;
    }
    else if(code <= 0x7ff)
    {
        output[0] = 0xC0 | (code >> 6);
        output[1] = 0x80 | (code & 0x3f);
        return 2;
    }
    else if(code <= 0xffff)
    {
        output[0] = 0xE0 | (code >> 12);
        output[1] = 0x80 | ((code >> 6) & 0x3f);
        output[2] = 0x80 | (code & 0x3f);
        return 3;
    }
    else if(code <= 0x10ffff)
    {
        output[0] = 0xF0 | (code >> 18);
        output[1] = 0x80 | ((code >> 12) & 0x3f);
        output[2] = 0x80 | ((code >> 6) & 0x3f);
        output[3] = 0x80 | (code & 0x3f);
        return 4;
    }
    return 0;
}

static bool is_space_or_tab(char c)
{
    return c == ' ' || c == '\t';
}

static int find_comment(Stream* stream)
{
    for(int i = 0; stream->at[i]; i += 1)
    {
        if(stream->at[i] == '#')
        {
            return i;
        }
    }
    return invalid_index;
}

static void skip_spacing(Stream* stream)
{
    const char* s;
    for(s = stream->at; is_space_or_tab(*s); s += 1);
    stream->at = s;
}

static void skip_to_next_line(Stream* stream)
{
    const char* s;
    for(s = stream->at; *s && *s != '\n'; s += 1);
    if(*s == '\n')
    {
        stream->at = s + 1;
    }
    else
    {
        stream->at = s;
    }
}

static char* next_token(Stream* stream, const char* end, Stack* stack)
{
    int i;
    for(i = 0; !is_space_or_tab(stream->at[i]) && stream->at != end; i += 1);
    if(i == 0)
    {
        return NULL;
    }

    int size = i + 1;
    char* result = STACK_ALLOCATE(stack, char, size);
    copy_string(result, size, stream->at);

    stream->at += i;

    return result;
}

// The test file for Unicode 10.0.0 wasn't actually updated from 9.0.0 and
// several test cases are actually erroneous. This just throws out the
// conflicting tests.
static bool ignore_line_break_test_line(int line)
{
    switch(line)
    {
        case 1141:
        case 1143:
        case 1145:
        case 1147:
        case 1309:
        case 1311:
        case 1313:
        case 1315:
        case 2981:
        case 2983:
        case 4497:
        case 4499:
        case 4665:
        case 4667:
        case 5165:
        case 5167:
        case 7137:
        case 7146:
        case 7151:
        case 7170:
        case 7171:
        case 7174:
        case 7175:
        case 7176:
        case 7179:
        case 7180:
        case 7181:
        case 7182:
        case 7183:
        case 7184:
        case 7185:
        case 7186:
        case 7187:
        case 7206:
        case 7207:
        case 7236:
        case 7237:
        case 7238:
        case 7239:
        case 7240:
        case 7241:
        case 7242:
        case 7243:
        case 7244:
        case 7245:
        case 7246:
        case 7247:
            return true;
        default:
            return false;
    }
}

static bool test_line(TestType test_type, const char* text, int text_filled, bool* breaks, int breaks_so_far, int line, Stack* stack)
{
    switch(test_type)
    {
        case TEST_TYPE_LINE_BREAK:
        {
            if(ignore_line_break_test_line(line))
            {
                break;
            }

            for(int i = 0, index = 0; i < breaks_so_far; i += 1)
            {
                LineBreakCategory category = test_line_break(text, index, stack);

                bool fail0 = !breaks[i] && (category == LINE_BREAK_CATEGORY_OPTIONAL || category == LINE_BREAK_CATEGORY_MANDATORY);
                bool fail1 = breaks[i] && category == LINE_BREAK_CATEGORY_PROHIBITED;
                if(fail0 || fail1)
                {
                    printf("Test failed at line %d. %s at byte %d\n", line, text, index);
                    return false;
                }

                char32_t dummy;
                index = utf8_get_next_codepoint(text, text_filled, index + 1, &dummy);
                if(index == invalid_index)
                {
                    index = text_filled;
                }
            }
            break;
        }
        case TEST_TYPE_GRAPHEME_CLUSTER_BREAK:
        {
            for(int i = 0, index = 0; i < breaks_so_far; i += 1)
            {
                bool allowed = test_grapheme_cluster_break(text, index, stack);
                if(breaks[i] != allowed)
                {
                    printf("Test failed at line %d. %s at byte %d\n", line, text, index);
                    return false;
                }

                char32_t dummy;
                index = utf8_get_next_codepoint(text, text_filled, index + 1, &dummy);
                if(index == invalid_index)
                {
                    index = text_filled;
                }
            }
            break;
        }
        case TEST_TYPE_WORD_BREAK:
        {
            for(int i = 0, index = 0; i < breaks_so_far; i += 1)
            {
                bool allowed = test_word_break(text, index, stack);
                if(breaks[i] != allowed)
                {
                    printf("Test failed at line %d. %s at byte %d\n", line, text, index);
                    return false;
                }

                char32_t dummy;
                index = utf8_get_next_codepoint(text, text_filled, index + 1, &dummy);
                if(index == invalid_index)
                {
                    index = text_filled;
                }
            }
            break;
        }
    }

    return true;
}

static bool run_test(TestType test_type, const char* path, Stack* stack)
{
    WholeFile whole_file = load_whole_file(path, stack);
    if(!whole_file.loaded)
    {
        printf("%s failed to load.\n", path);
        return false;
    }

    Stream stream = {0};
    stream.at = (const char*) whole_file.contents;

    const char* optional = "÷";
    const char* prohibited = "×";

    int line = 0;
    int failures = 0;

    for(;;)
    {
        line += 1;

        int comment = find_comment(&stream);
        if(comment == invalid_index)
        {
            break;
        }
        const char* comment_point = &stream.at[comment];

        bool breaks[8];
        char text[64];
        int breaks_so_far = 0;
        int text_filled = 0;

        for(int i = 0;; i += 1)
        {
            skip_spacing(&stream);
            char* token = next_token(&stream, comment_point, stack);
            if(!token)
            {
                break;
            }

            if(i & 1)
            {
                int value;
                bool parsed = string_to_int_extra(token, NULL, 16, &value);
                if(!parsed)
                {
                    printf("failure at token: %s\n", token);
                }
                else
                {
                    char32_t code = value;
                    text_filled += utf32_to_utf8(&text[text_filled], code);
                }
            }
            else
            {
                if(strings_match(token, optional))
                {
                    breaks[breaks_so_far] = true;
                }
                else if(strings_match(token, prohibited))
                {
                    breaks[breaks_so_far] = false;
                }
                else
                {
                    ASSERT(false);
                }
                breaks_so_far += 1;
            }

            STACK_DEALLOCATE(stack, token);
        }

        text[text_filled] = '\0';

        bool passed = test_line(test_type, text, text_filled, breaks, breaks_so_far, line, stack);
        if(!passed)
        {
            failures += 1;
        }

        skip_to_next_line(&stream);
    }

    STACK_DEALLOCATE(stack, whole_file.contents);

    return failures == 0;
}

int main(int argc, char** argv)
{
    Heap heap = {0};
    Stack stack = {0};
    heap_create(&heap, capobytes(64));
    stack_create(&stack, capobytes(128));

    set_asset_path(&heap);
    unicode_load_tables(&heap, &stack);

    TestType tests[TEST_TYPE_COUNT] =
    {
        TEST_TYPE_GRAPHEME_CLUSTER_BREAK,
        TEST_TYPE_LINE_BREAK,
        TEST_TYPE_WORD_BREAK,
    };

    const char* paths[TEST_TYPE_COUNT] =
    {
        "GraphemeBreakTest.txt",
        "LineBreakTest.txt",
        "WordBreakTest.txt",
    };

    int failures = 0;

    for(int test_index = 0; test_index < TEST_TYPE_COUNT; test_index += 1)
    {
        TestType test_type = tests[test_index];
        const char* path = paths[test_index];
        bool success = run_test(test_type, path, &stack);
        if(!success)
        {
            failures += 1;
        }
    }

    if(failures == 0)
    {
        printf("All tests were passed!\n");
    }
    else
    {
        printf("%d tests failed.\n", failures);
    }

    unicode_unload_tables(&heap);

    heap_destroy(&heap);
    stack_destroy(&stack);

    return 0;
}
