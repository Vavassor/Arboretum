cmake_minimum_required(VERSION 2.8)
project(Arboretum)
set(Arboretum_VERSION_MAJOR 1)
set(Arboretum_VERSION_MINOR 0)
set(Arboretum_VERSION_PATCH 0)

option(PORTABLE_APP "Put everything in a self-contained directory instead of being installed." ON)
if(PORTABLE_APP)
    set(Arboretum_PORTABLE_APP 1)
else()
    set(Arboretum_PORTABLE_APP 0)
endif()

add_executable(Arboretum WIN32 "")

set_target_properties(
	Arboretum
	PROPERTIES
	C_STANDARD 99
	C_STANDARD_REQUIRED ON
	RUNTIME_OUTPUT_DIRECTORY bin
)

if("${CMAKE_SYSTEM}" MATCHES "Linux")
    set(LINUX TRUE)
endif()

if(WIN32)
    target_link_libraries(Arboretum PRIVATE imm32 opengl32)
	set(
		PLATFORM_SPECIFIC_SOURCES
		Source/wgl_extensions.c
		Source/wide_char.c
	)
	set(DATA_DIRECTORY ./)
elseif(LINUX)
    target_link_libraries(Arboretum PRIVATE m GL X11 Xcursor)
	set(
		PLATFORM_SPECIFIC_SOURCES
		Source/glx_extensions.c
	)
	set(DATA_DIRECTORY /usr/share/arboretum)
endif()

target_sources(
	Arboretum
	PRIVATE
	Source/array2.c
	Source/ascii.c
	Source/assert.c
	Source/asset_paths.c
	Source/bitmap.c
	Source/bmfont.c
	Source/bmp.c
	Source/camera.c
	Source/closest_point_of_approach.c
	Source/colours.c
	Source/complex_math.c
	Source/debug_draw.c
	Source/debug_readout.c
	Source/dense_map.c
	Source/editor.c
	Source/file_pick_dialog.c
	Source/filesystem.c
	Source/float_utilities.c
	Source/geometry.c
	Source/gl_core_3_3.c
	Source/history.c
	Source/id_pool.c
	Source/immediate.c
	Source/input.c
	Source/int_utilities.c
	Source/int2.c
	Source/intersection.c
	Source/invalid_index.c
	Source/jan.c
	Source/jan_copy.c
	Source/jan_internal.c
	Source/jan_selection.c
	Source/jan_triangulate.c
	Source/jan_validate.c
	Source/loc.c
	Source/log.c
	Source/main.c
	Source/map.c
	Source/memory.c
	Source/obj.c
	Source/object.c
	Source/object_lady.c
	Source/platform.c
	Source/string_build.c
	Source/string_utilities.c
	Source/ui.c
	Source/ui_internal.c
	Source/unicode.c
	Source/unicode_grapheme_cluster_break.c
	Source/unicode_line_break.c
	Source/unicode_load_tables.c
	Source/unicode_word_break.c
	Source/vector_extras.c
	Source/vector_math.c
	Source/vertex_layout.c
	Source/video.c
	Source/video_gl.c
	Source/video_internal.c
	Source/video_object.c
	Source/video_ui.c
	${PLATFORM_SPECIFIC_SOURCES}
)

configure_file (
    "${PROJECT_SOURCE_DIR}/Source/arboretum_config.h.in"
    "${PROJECT_BINARY_DIR}/arboretum_config.h"
    @ONLY
)
include_directories("${PROJECT_BINARY_DIR}")

macro(mirror_files directory_name)
    file (GLOB_RECURSE resources "${directory_name}/*.*")
    foreach(resource ${resources})
        get_filename_component(filename ${resource} NAME)
        get_filename_component(dir ${resource} DIRECTORY)
        get_filename_component(dirname ${dir} NAME)
     
        set (output "")
     
        while(NOT ${dirname} STREQUAL ${directory_name})
            get_filename_component(path_component ${dir} NAME)
            set (output "${path_component}/${output}")
            get_filename_component(dir ${dir} DIRECTORY)
            get_filename_component(dirname ${dir} NAME)
        endwhile()
     
        set(output "${CMAKE_CURRENT_BINARY_DIR}/${directory_name}/${output}/${filename}")
     
        configure_file(${resource} ${output} COPYONLY)
    endforeach()
endmacro()

mirror_files(Assets)
mirror_files(Package)

install(
	TARGETS Arboretum
	RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
if(PORTABLE_APP)
    install(DIRECTORY Assets DESTINATION ./)
else()
    install(DIRECTORY Assets DESTINATION ${DATA_DIRECTORY})
endif()

include(InstallRequiredSystemLibraries)
set(CPACK_PACKAGE_DESCRIPTION_FILE "Package/Description.txt")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Arboretum is a 3D modeling app")
set(CPACK_PACKAGE_NAME "Arboretum")
set(CPACK_PACKAGE_VENDOR "Keyless Palace")
set(CPACK_PACKAGE_VERSION_MAJOR "${Arboretum_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${Arboretum_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${Arboretum_VERSION_PATCH}")
if(WIN32)
    set(CPACK_PACKAGE_EXECUTABLES "Arboretum" "Arboretum")
    set(CPACK_WIX_UPGRADE_GUID "C335D757-A9D6-4CEA-AFBE-AE7AE97B981E")
elseif(LINUX)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libgl1-mesa-glx, libx11-6, libxcursor1")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/Vavassor/Arboretum")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Andrew Dawson <dawso.andrew@gmail.com>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
endif()
include(CPack)
