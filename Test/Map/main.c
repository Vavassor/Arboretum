#include "../../Source/map.h"
#include "../../Source/random.h"
#include "../../Source/sorting.h"

#include <stdio.h>

typedef enum TestType
{
    TEST_TYPE_GET,
    TEST_TYPE_GET_MISSING,
    TEST_TYPE_GET_OVERFLOW,
    TEST_TYPE_ITERATE,
    TEST_TYPE_REMOVE,
    TEST_TYPE_REMOVE_OVERFLOW,
    TEST_TYPE_RESERVE,
    TEST_TYPE_COUNT,
} TestType;

typedef struct Pair
{
    void* key;
    void* value;
} Pair;

typedef struct Test
{
    Map map;
    RandomGenerator generator;
    TestType type;
} Test;

static const char* describe_test(TestType type)
{
    switch(type)
    {
        default:
        case TEST_TYPE_GET:             return "Get";
        case TEST_TYPE_GET_MISSING:     return "Get Missing";
        case TEST_TYPE_GET_OVERFLOW:    return "Get Overflow";
        case TEST_TYPE_ITERATE:         return "Iterate";
        case TEST_TYPE_REMOVE:          return "Remove";
        case TEST_TYPE_REMOVE_OVERFLOW: return "Remove Overflow";
        case TEST_TYPE_RESERVE:         return "Reserve";
    }
}

static bool test_get(Test* test, Heap* heap)
{
    Map* map = &test->map;
    void* key = (void*) 253;
    void* value = (void*) 512;
    map_add(map, key, value, heap);
    void* found;
    bool got = map_get(map, key, &found);
    return got && found == value;
}

static bool test_get_missing(Test* test, Heap* heap)
{
    Map* map = &test->map;
    void* key = (void*) 0x12aa5f;
    void* discard;
    bool got = map_get(map, key, &discard);
    return !got;
}

static bool test_get_overflow(Test* test, Heap* heap)
{
    Map* map = &test->map;
    void* key = (void*) 0;
    void* value = (void*) 612377;
    map_add(map, key, value, heap);
    void* found;
    bool got = map_get(map, key, &found);
    return got && found == value;
}

static bool is_key_before(Pair a, Pair b)
{
    return a.key < b.key;
}

DEFINE_QUICK_SORT(Pair, is_key_before, by_keys);

static bool test_iterate(Test* test, Heap* heap)
{
    Map* map = &test->map;

    const int pairs_count = 256;
    Pair insert_pairs[pairs_count];

    random_seed(&test->generator, 1635899);

    for(int pair_index = 0; pair_index < pairs_count; pair_index += 1)
    {
        void* key = (void*) random_generate(&test->generator);
        void* value = (void*) random_generate(&test->generator);
        insert_pairs[pair_index].key = key;
        insert_pairs[pair_index].value = value;
        map_add(map, key, value, heap);
    }

    Pair pairs[pairs_count];
    int found = 0;

    ITERATE_MAP(it, map)
    {
        pairs[found].key = map_iterator_get_key(it);
        pairs[found].value = map_iterator_get_value(it);
        found += 1;
    }
    if(found != pairs_count)
    {
        return false;
    }

    quick_sort_by_keys(pairs, pairs_count);
    quick_sort_by_keys(insert_pairs, pairs_count);

    int mismatches = 0;

    for(int pair_index = 0; pair_index < pairs_count; pair_index += 1)
    {
        Pair insert = insert_pairs[pair_index];
        Pair found = pairs[pair_index];
        bool key_matches = insert.key == found.key;
        bool value_matches = insert.value == found.value;
        mismatches += !(key_matches && value_matches);
    }

    return mismatches == 0;
}

static bool test_remove(Test* test, Heap* heap)
{
    Map* map = &test->map;
    void* key = (void*) 6356;
    void* value = (void*) 711677;
    map_add(map, key, value, heap);
    void* discard;
    bool was_in = map_get(map, key, &discard);
    map_remove(map, key);
    bool is_in = map_get(map, key, &discard);
    return was_in && !is_in;
}

static bool test_remove_overflow(Test* test, Heap* heap)
{
    Map* map = &test->map;
    void* key = (void*) 0;
    void* value = (void*) 6143;
    map_add(map, key, value, heap);
    void* discard;
    bool had = map_get(map, key, &discard);
    map_remove(map, key);
    bool got = map_get(map, key, &discard);
    return had && !got;
}

static bool test_reserve(Test* test, Heap* heap)
{
    Map* map = &test->map;
    int reserve = 1254;
    bool was_smaller = reserve > map->cap;
    map_reserve(map, reserve, heap);
    bool is_enough = reserve <= map->cap;
    return was_smaller && is_enough;
}

static bool run_test(Test* test, Heap* heap)
{
    switch(test->type)
    {
        default:
        case TEST_TYPE_GET:             return test_get(test, heap);
        case TEST_TYPE_GET_MISSING:     return test_get_missing(test, heap);
        case TEST_TYPE_GET_OVERFLOW:    return test_get_overflow(test, heap);
        case TEST_TYPE_ITERATE:         return test_iterate(test, heap);
        case TEST_TYPE_REMOVE:          return test_remove(test, heap);
        case TEST_TYPE_REMOVE_OVERFLOW: return test_remove_overflow(test, heap);
        case TEST_TYPE_RESERVE:         return test_reserve(test, heap);
    }
}

static bool run_tests(Heap* heap)
{
    const TestType tests[TEST_TYPE_COUNT] =
    {
        TEST_TYPE_GET,
        TEST_TYPE_GET_MISSING,
        TEST_TYPE_GET_OVERFLOW,
        TEST_TYPE_ITERATE,
        TEST_TYPE_REMOVE,
        TEST_TYPE_REMOVE_OVERFLOW,
        TEST_TYPE_RESERVE,
    };
    bool which_failed[TEST_TYPE_COUNT] = {};

    int failed = 0;

    for(int test_index = 0; test_index < TEST_TYPE_COUNT; test_index += 1)
    {
        Test test = {0};
        test.type = tests[test_index];
        map_create(&test.map, 0, heap);

        bool fail = !run_test(&test, heap);
        failed += fail;
        which_failed[test_index] = fail;

        map_destroy(&test.map, heap);
    }

    FILE* file = stdout;
    if(failed > 0)
    {
        fprintf(file, "test failed: %d\n", failed);
        int printed = 0;
        for(int test_index = 0; test_index < TEST_TYPE_COUNT; test_index += 1)
        {
            const char* separator = "";
            const char* also = "";
            if(failed > 2 && printed > 0)
            {
                separator = ", ";
            }
            if(failed > 1 && printed == failed - 1)
            {
                if(failed == 2)
                {
                    also = " and ";
                }
                else
                {
                    also = "and ";
                }
            }
            if(which_failed[test_index])
            {
                const char* test = describe_test(tests[test_index]);
                fprintf(file, "%s%s%s", separator, also, test);
                printed += 1;
            }
        }
        fprintf(file, "\n\n");
    }
    else
    {
        fprintf(file, "All tests succeeded!\n\n");
    }

    return failed == 0;
}

int main(int argc, char** argv)
{
    Heap heap = {0};
    heap_create(&heap, (uint32_t) capobytes(16));
    bool success = run_tests(&heap);
    heap_destroy(&heap);
    return !success;
}