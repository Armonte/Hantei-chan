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
	src/mv_script.cpp
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
	# Background (stage) system — ported from bgmk branch.
	src/background/bg_file.cpp
	src/background/bg_renderer.cpp
	src/background/bg_pat.cpp
	src/background/bg_info.cpp
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

# Standalone HA6 round-trip validator (load -> save -> reload -> compare).
add_executable(roundtrip
	src/roundtrip.cpp
	src/framedata.cpp
	src/framedata_load.cpp
	src/framedata_save.cpp
	src/misc.cpp
	tinyalloc/tinyalloc.c
)
target_include_directories(roundtrip PRIVATE "." "third_party" "${CMAKE_BINARY_DIR}/generated")
target_compile_definitions(roundtrip PRIVATE WIN32_LEAN_AND_MEAN HA6GUIVERSION="${CMAKE_PROJECT_VERSION}")
target_link_libraries(roundtrip PRIVATE tinyalloc)
if(MINGW)
	target_link_options(roundtrip PRIVATE -static-libgcc -static-libstdc++ -static)
endif()

# Standalone smoke test for bg::File loader (no GL needed for parse-only).
add_executable(bg_test
	src/bg_test.cpp
	src/background/bg_file.cpp
	src/background/bg_pat.cpp
	src/background/bg_info.cpp
	src/cg.cpp
	src/misc.cpp
	tinyalloc/tinyalloc.c
)
target_include_directories(bg_test PRIVATE "." "third_party" "${CMAKE_BINARY_DIR}/generated")
target_compile_definitions(bg_test PRIVATE WIN32_LEAN_AND_MEAN HA6GUIVERSION="${CMAKE_PROJECT_VERSION}")
target_link_libraries(bg_test PRIVATE tinyalloc glm)
if(MINGW)
	target_link_options(bg_test PRIVATE -static-libgcc -static-libstdc++ -static)
endif()

# Standalone PAT round-trip validator. Links the editor's parts code with
# glad/OpenGL stubs (glad calls no-op without a current context, which is
# fine for the load/save paths we exercise).
add_executable(pat_roundtrip
	src/pat_roundtrip.cpp
	src/parts/parts.cpp
	src/parts/parts_part.cpp
	src/parts/parts_partset.cpp
	src/parts/parts_shape.cpp
	src/parts/parts_texture.cpp
	src/cg.cpp
	src/texture.cpp
	src/vao.cpp
	src/shader.cpp
	src/misc.cpp
	tinyalloc/tinyalloc.c
)
target_include_directories(pat_roundtrip PRIVATE "." "third_party" "${CMAKE_BINARY_DIR}/generated")
target_compile_definitions(pat_roundtrip PRIVATE WIN32_LEAN_AND_MEAN HA6GUIVERSION="${CMAKE_PROJECT_VERSION}" PAT_HEADLESS)
target_link_libraries(pat_roundtrip PRIVATE tinyalloc glad glm OpenGL::GL)
if(MINGW)
	target_link_options(pat_roundtrip PRIVATE -static-libgcc -static-libstdc++ -static)
endif()



# Debug tool: dump pattern frame data via the real loader.
add_executable(ha6_dump
	src/ha6_dump.cpp
	src/framedata.cpp
	src/framedata_load.cpp
	src/framedata_save.cpp
	src/misc.cpp
	tinyalloc/tinyalloc.c
)
target_include_directories(ha6_dump PRIVATE "." "third_party" "${CMAKE_BINARY_DIR}/generated")
target_compile_definitions(ha6_dump PRIVATE WIN32_LEAN_AND_MEAN HA6GUIVERSION="${CMAKE_PROJECT_VERSION}")
target_link_libraries(ha6_dump PRIVATE tinyalloc)
if(MINGW)
	target_link_options(ha6_dump PRIVATE -static-libgcc -static-libstdc++ -static)
endif()

# Debug tool: dump PAT part-set data via the real loader (headless).
add_executable(pat_dump
	src/pat_dump.cpp
	src/parts/parts.cpp
	src/parts/parts_part.cpp
	src/parts/parts_partset.cpp
	src/parts/parts_shape.cpp
	src/parts/parts_texture.cpp
	src/cg.cpp
	src/texture.cpp
	src/vao.cpp
	src/shader.cpp
	src/misc.cpp
	tinyalloc/tinyalloc.c
)
target_include_directories(pat_dump PRIVATE "." "third_party" "${CMAKE_BINARY_DIR}/generated")
target_compile_definitions(pat_dump PRIVATE WIN32_LEAN_AND_MEAN HA6GUIVERSION="${CMAKE_PROJECT_VERSION}" PAT_HEADLESS)
target_link_libraries(pat_dump PRIVATE tinyalloc glad glm OpenGL::GL)
if(MINGW)
	target_link_options(pat_dump PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
