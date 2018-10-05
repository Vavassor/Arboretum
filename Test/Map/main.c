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
    MaybePointer result = map_get(map, key);
    return result.valid && result.value == value;
}

static bool test_get_missing(Test* test, Heap* heap)
{
    Map* map = &test->map;
    void* key = (void*) 0x12aa5f;
    MaybePointer result = map_get(map, key);
    return !result.valid;
}

static bool test_get_overflow(Test* test, Heap* heap)
{
    Map* map = &test->map;
    void* key = (void*) 0;
    void* value = (void*) 612377;
    map_add(map, key, value, heap);
    MaybePointer result = map_get(map, key);
    return result.valid && result.value == value;
}

static bool is_key_before(Pair a, Pair b)
{
    return a.key < b.key;
}

DEFINE_QUICK_SORT(Pair, is_key_before, by_keys);

#define PAIRS_COUNT 256

static bool test_iterate(Test* test, Heap* heap)
{
    Map* map = &test->map;

    Pair insert_pairs[PAIRS_COUNT];

    random_seed(&test->generator, 1635899);

    for(int pair_index = 0; pair_index < PAIRS_COUNT; pair_index += 1)
    {
        void* key = (void*) random_generate(&test->generator);
        void* value = (void*) random_generate(&test->generator);
        insert_pairs[pair_index].key = key;
        insert_pairs[pair_index].value = value;
        map_add(map, key, value, heap);
    }

    Pair pairs[PAIRS_COUNT];
    int found = 0;

    ITERATE_MAP(it, map)
    {
        pairs[found].key = map_iterator_get_key(it);
        pairs[found].value = map_iterator_get_value(it);
        found += 1;
    }
    if(found != PAIRS_COUNT)
    {
        return false;
    }

    quick_sort_by_keys(pairs, PAIRS_COUNT);
    quick_sort_by_keys(insert_pairs, PAIRS_COUNT);

    int mismatches = 0;

    for(int pair_index = 0; pair_index < PAIRS_COUNT; pair_index += 1)
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
    MaybePointer result = map_get(map, key);
    bool was_in = result.valid;
    map_remove(map, key);
    result = map_get(map, key);
    bool is_in = result.valid;
    return was_in && !is_in;
}

static bool test_remove_overflow(Test* test, Heap* heap)
{
    Map* map = &test->map;
    void* key = (void*) 0;
    void* value = (void*) 6143;
    map_add(map, key, value, heap);
    MaybePointer result = map_get(map, key);
    bool had = result.valid;
    map_remove(map, key);
    result = map_get(map, key);
    bool got = result.valid;
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
    bool which_failed[TEST_TYPE_COUNT] = {0};

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
