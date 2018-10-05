add_executable(TestMap "")

if(WIN32)
	set(
		PLATFORM_SPECIFIC_SOURCES
		../Source/wide_char.c
	)
endif()

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
	${PLATFORM_SPECIFIC_SOURCES}
)

if(LINUX)
    target_link_libraries(TestMap PRIVATE m)
endif()

add_test(Map TestMap)


add_executable(TestUnicode "")

set_target_properties(
    TestUnicode
    PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)

target_sources(
    TestUnicode
    PRIVATE
    ../Source/array2.c
    ../Source/ascii.c
    ../Source/assert.c
    ../Source/asset_paths.c
    ../Source/filesystem.c
    ../Source/int_utilities.c
    ../Source/invalid_index.c
    ../Source/memory.c
    ../Source/string_build.c
    ../Source/string_utilities.c
    ../Source/unicode.c
    ../Source/unicode_grapheme_cluster_break.c
    ../Source/unicode_load_tables.c
    ../Source/unicode_line_break.c
    ../Source/unicode_word_break.c
    Unicode/main.c
	${PLATFORM_SPECIFIC_SOURCES}
)

if(LINUX)
    target_link_libraries(TestUnicode PRIVATE m)
endif()

configure_file(Unicode/GraphemeBreakTest.txt GraphemeBreakTest.txt COPYONLY)
configure_file(Unicode/LineBreakTest.txt LineBreakTest.txt COPYONLY)
configure_file(Unicode/WordBreakTest.txt WordBreakTest.txt COPYONLY)

add_test(Unicode TestUnicode)
