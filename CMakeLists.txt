cmake_minimum_required(VERSION 2.8)
project(Arboretum)

file(GLOB FilesAll ${PROJECT_SOURCE_DIR}/*.*)

add_executable(Arboretum ${FilesAll})

set_property(TARGET Arboretum PROPERTY C_STANDARD 99)

if(WIN32)
set_property(TARGET Arboretum APPEND_STRING PROPERTY LINK_FLAGS "/SUBSYSTEM:WINDOWS")
target_link_libraries(Arboretum imm32 opengl32)
endif()

file (GLOB_RECURSE resources "Assets/*.*")
foreach(resource ${resources})
	get_filename_component(filename ${resource} NAME)
	get_filename_component(dir ${resource} DIRECTORY)
	get_filename_component(dirname ${dir} NAME)
 
	set (output "")
 
	while(NOT ${dirname} STREQUAL Assets)
		get_filename_component(path_component ${dir} NAME)
		set (output "${path_component}/${output}")
		get_filename_component(dir ${dir} DIRECTORY)
		get_filename_component(dirname ${dir} NAME)
	endwhile()
 
	set(output "${CMAKE_CURRENT_BINARY_DIR}/Assets/${output}/${filename}")
 
	add_custom_command(
		COMMENT "Moving updated asset file '${filename}'"
		OUTPUT ${output}
		DEPENDS ${resource}
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		${resource}
		${output}
	)
	add_custom_target(${filename} ALL DEPENDS ${resource} ${output})
endforeach()
