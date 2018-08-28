add_executable(TestMap "")

target_sources(
    TestMap
    PRIVATE
    ../Source/array2.c
    ../Source/ascii.c
    ../Source/assert.c
    ../Source/filesystem.c
    ../Source/int_utilities.c
    ../Source/invalid_index.c
    ../Source/map.c
    ../Source/memory.c
    ../Source/random.c
    ../Source/string_build.c
    ../Source/string_utilities.c
    Map/main.c
)

if(LINUX)
    target_link_libraries(TestMap PRIVATE m)
endif()

add_test(Map TestMap)
