cmake_minimum_required(VERSION 3.13...3.19)
project(hanteichan VERSION 1.1.21 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

set (CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" )
set (EXE "gonptechan" )

# Generate version info header
include(GenerateVersionInfo)

include(glad)
include(imgui)
add_subdirectory(glm)
add_subdirectory(tinyalloc)

add_executable(${EXE}
	src/main.cpp
	src/main_frame.cpp
	src/main_pane.cpp
	src/right_pane.cpp
	src/box_pane.cpp
	src/render.cpp
	src/vao.cpp
	src/shader.cpp
	src/texture.cpp
	src/filedialog.cpp
	src/framedata.cpp
	src/framedata_load.cpp
	src/framedata_save.cpp
	src/framestate.cpp
	src/character_instance.cpp
	src/character_view.cpp
	src/project_manager.cpp
	src/hud_theme_exporter.cpp
	src/cg.cpp
	src/vectors.cpp
	src/misc.cpp
	src/imgui_utils.cpp
	src/test.cpp
	src/ini.cpp
	src/preset_effects.cpp
	src/copy_manager.cpp
	src/PatEditor/pat_partset_pane.cpp
	src/PatEditor/pat_part_pane.cpp
	src/PatEditor/pat_shape_pane.cpp
	src/PatEditor/pat_texture_pane.cpp
	src/PatEditor/pat_tool_pane.cpp
	src/parts/parts_partset.cpp
	src/parts/parts_part.cpp
	src/parts/parts_shape.cpp
	src/parts/parts_texture.cpp
	src/parts/parts.cpp
	imsearch/imsearch.cpp
	res/res.rc
)

target_include_directories(${EXE} PRIVATE "." "third_party" "${CMAKE_BINARY_DIR}/generated" "imsearch")
target_compile_definitions(${EXE} PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN HA6GUIVERSION="${CMAKE_PROJECT_VERSION}")

# Add custom target to regenerate version info on every build
add_custom_target(update_version ALL
	COMMAND ${CMAKE_COMMAND}
		-DVERSION_MAJOR=${PROJECT_VERSION_MAJOR}
		-DVERSION_MINOR=${PROJECT_VERSION_MINOR}
		-DVERSION_PATCH=${PROJECT_VERSION_PATCH}
		-DCMAKE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
		-DCMAKE_BINARY_DIR=${CMAKE_BINARY_DIR}
		-P ${CMAKE_SOURCE_DIR}/cmake/GenerateVersionInfo.cmake
	BYPRODUCTS ${CMAKE_BINARY_DIR}/generated/version.h
	COMMENT "Updating version information..."
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_dependencies(${EXE} update_version)

find_package(OpenGL REQUIRED)
target_link_libraries(${EXE} PRIVATE imgui glad OpenGL::GL glm tinyalloc)

if(MINGW)
	message(status "With -municode")
	target_link_options(${EXE} PRIVATE -municode -static-libgcc -static-libstdc++ -static)
endif()

