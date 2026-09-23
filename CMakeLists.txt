cmake_minimum_required(VERSION 3.13...3.19)
project(hanteichan VERSION 1.1.21 LANGUAGES C CXX)

# Single-config generators default to an empty build type, which leaves
# assert()/IM_ASSERT live: CI's "MinGW Makefiles" build shipped that way, and a
# harmless ImGui bounds assert aborted the app (issue #81). Default to Release.
get_property(_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT _multi_config AND NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

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
	src/framedata_ha4.cpp
	src/ha4_parts.cpp
	src/ha4_parts_gl.cpp
	src/ha4_convert.cpp
	src/ha4_character.cpp
	src/startup_args.cpp
	src/framestate.cpp
	src/copy_data.cpp
	src/var_refs.cpp
	src/preview_sim.cpp
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
	src/undo_manager.cpp
	src/shortcut_router.cpp
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
	src/background/bg_inspector.cpp
	# _c.txt command-file editor (ported from Gonptechan EX, see docs/HANTEI_CMDFILE_EDITOR.md)
	src/extension_profile.cpp
	src/cmdfile/cmd_document.cpp
	src/cmdfile/cmd_validate.cpp
	src/cmdfile/cmd_notes.cpp
	src/cmdfile/cmd_io.cpp
	src/cmdfile/cmd_workspace.cpp
	src/cmdfile/cmd_framedata.cpp
	src/cmdfile/cmd_editor_ui.cpp
	src/ui/main_frame_cmdfile.cpp
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
	src/framedata_ha4.cpp
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
	src/framedata_ha4.cpp
	src/misc.cpp
	tinyalloc/tinyalloc.c
)
target_include_directories(ha6_dump PRIVATE "." "third_party" "${CMAKE_BINARY_DIR}/generated")
target_compile_definitions(ha6_dump PRIVATE WIN32_LEAN_AND_MEAN HA6GUIVERSION="${CMAKE_PROJECT_VERSION}")
target_link_libraries(ha6_dump PRIVATE tinyalloc)
if(MINGW)
	target_link_options(ha6_dump PRIVATE -static-libgcc -static-libstdc++ -static)
endif()

# Headless preview simulator driver: actor dumps, checkpoint verification, scrub bench.
add_executable(preview_sim_tool
	src/preview_sim_tool.cpp
	src/preview_sim.cpp
	src/framedata.cpp
	src/framedata_load.cpp
	src/framedata_save.cpp
	src/framedata_ha4.cpp
	src/misc.cpp
	tinyalloc/tinyalloc.c
)
target_include_directories(preview_sim_tool PRIVATE "." "third_party" "${CMAKE_BINARY_DIR}/generated")
target_compile_definitions(preview_sim_tool PRIVATE WIN32_LEAN_AND_MEAN HA6GUIVERSION="${CMAKE_PROJECT_VERSION}")
target_link_libraries(preview_sim_tool PRIVATE tinyalloc glm)
if(MINGW)
	target_link_options(preview_sim_tool PRIVATE -static-libgcc -static-libstdc++ -static)
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

enable_testing()

# Command-file tools (statically linked so they run from a plain Windows prompt or WSL interop).
# cmdfile_roundtrip: parse -> serialize byte compare + validation summary for any _c.txt.
add_executable(cmdfile_roundtrip
	src/cmdfile/cmdfile_roundtrip.cpp
	src/cmdfile/cmd_document.cpp
	src/cmdfile/cmd_validate.cpp
)
target_include_directories(cmdfile_roundtrip PRIVATE "src")
if(MINGW)
	target_link_options(cmdfile_roundtrip PRIVATE -static-libgcc -static-libstdc++ -static)
endif()

# cmdfile_test: unit + structural tests (ported from EX's command workspace/check tests).
add_executable(cmdfile_test
	tests/cmdfile_test.cpp
	src/cmdfile/cmd_document.cpp
	src/cmdfile/cmd_validate.cpp
	src/cmdfile/cmd_notes.cpp
	src/cmdfile/cmd_io.cpp
	src/cmdfile/cmd_workspace.cpp
	src/misc.cpp
)
target_include_directories(cmdfile_test PRIVATE "src" "third_party")
target_compile_definitions(cmdfile_test PRIVATE WIN32_LEAN_AND_MEAN)
if(MINGW)
	target_link_options(cmdfile_test PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
add_test(NAME cmdfile COMMAND cmdfile_test tests/fixtures/cmdfile WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# cmdfile_ui_smoke: draws the editor UI headlessly with ImGui assertions enabled.
add_executable(cmdfile_ui_smoke
	tests/cmdfile_ui_smoke.cpp
	src/extension_profile.cpp
	src/cmdfile/cmd_editor_ui.cpp
	src/cmdfile/cmd_document.cpp
	src/cmdfile/cmd_validate.cpp
	src/cmdfile/cmd_notes.cpp
	src/cmdfile/cmd_io.cpp
	src/cmdfile/cmd_workspace.cpp
	src/misc.cpp
	imgui/imgui.cpp
	imgui/imgui_draw.cpp
	imgui/imgui_tables.cpp
	imgui/imgui_widgets.cpp
	imgui/misc/cpp/imgui_stdlib.cpp
)
target_include_directories(cmdfile_ui_smoke PRIVATE "src" "third_party" "imgui" "imgui/misc/cpp")
target_compile_definitions(cmdfile_ui_smoke PRIVATE WIN32_LEAN_AND_MEAN IMGUI_USER_CONFIG="${CMAKE_SOURCE_DIR}/tests/imgui_assert_config.h")
if(MINGW)
	target_link_options(cmdfile_ui_smoke PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
add_test(NAME cmdfile_ui_smoke COMMAND cmdfile_ui_smoke tests/fixtures/cmdfile/ex_vanilla_ryougi_1_c.txt tests/fixtures/cmdfile/ex_bof_angel_0_c.txt WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
# pattern_search_test: pattern-name search (ImSearch) headless, asserts enabled (issue #81).
add_executable(pattern_search_test
	tests/pattern_search_test.cpp
	src/framedata.cpp
	src/framedata_load.cpp
	src/framedata_save.cpp
	src/framedata_ha4.cpp
	src/misc.cpp
	src/extension_profile.cpp
	src/imgui_utils.cpp
	tinyalloc/tinyalloc.c
	imsearch/imsearch.cpp
	imgui/imgui.cpp
	imgui/imgui_draw.cpp
	imgui/imgui_tables.cpp
	imgui/imgui_widgets.cpp
)
target_include_directories(pattern_search_test PRIVATE "." "src" "third_party" "imgui" "imsearch" "${CMAKE_BINARY_DIR}/generated")
target_compile_definitions(pattern_search_test PRIVATE WIN32_LEAN_AND_MEAN HA6GUIVERSION="${CMAKE_PROJECT_VERSION}" IMGUI_USER_CONFIG="${CMAKE_SOURCE_DIR}/tests/imgui_assert_config.h")
target_link_libraries(pattern_search_test PRIVATE tinyalloc)
if(MINGW)
	target_link_options(pattern_search_test PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
add_test(NAME pattern_search COMMAND pattern_search_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
# shared_clipboard_test: cross-instance copy/paste through the shared heap (issue #8).
add_executable(shared_clipboard_test
	tests/shared_clipboard_test.cpp
	src/copy_data.cpp
	tinyalloc/tinyalloc.c
)
target_include_directories(shared_clipboard_test PRIVATE "." "src" "third_party" "tinyalloc")
target_compile_definitions(shared_clipboard_test PRIVATE WIN32_LEAN_AND_MEAN)
if(MINGW)
	target_link_options(shared_clipboard_test PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
add_test(NAME shared_clipboard COMMAND shared_clipboard_test)
# refs_test: variable / pattern reference tools (issues #75, #44).
add_executable(refs_test
	tests/refs_test.cpp
	src/var_refs.cpp
	src/framedata.cpp
	src/framedata_load.cpp
	src/framedata_save.cpp
	src/framedata_ha4.cpp
	src/misc.cpp
	tinyalloc/tinyalloc.c
)
target_include_directories(refs_test PRIVATE "." "src" "third_party" "${CMAKE_BINARY_DIR}/generated")
target_compile_definitions(refs_test PRIVATE WIN32_LEAN_AND_MEAN HA6GUIVERSION="${CMAKE_PROJECT_VERSION}")
target_link_libraries(refs_test PRIVATE tinyalloc)
if(MINGW)
	target_link_options(refs_test PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
add_test(NAME refs COMMAND refs_test)
# MBAC Hantei4 (.DAT) tool: byte-exact round trip, model re-encode check,
# batch MBAC -> HA6 conversion (see docs/HANTEI_MBAC_SUPPORT.md).
add_executable(ha4tool
	src/ha4tool.cpp
	src/framedata.cpp
	src/framedata_load.cpp
	src/framedata_save.cpp
	src/framedata_ha4.cpp
	src/ha4_parts.cpp
	src/ha4_convert.cpp
	src/background/bg_pat.cpp
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
target_include_directories(ha4tool PRIVATE "." "third_party" "${CMAKE_BINARY_DIR}/generated")
target_compile_definitions(ha4tool PRIVATE WIN32_LEAN_AND_MEAN HA6GUIVERSION="${CMAKE_PROJECT_VERSION}" PAT_HEADLESS)
target_link_libraries(ha4tool PRIVATE tinyalloc glad glm OpenGL::GL)
if(MINGW)
	target_link_options(ha4tool PRIVATE -static-libgcc -static-libstdc++ -static)
endif()

# Unit tests for editor infrastructure (no GL / ImGui). Run from WSL with
# ./build/<name>_test.exe, or ctest.
add_executable(shortcut_router_test tests/shortcut_router_test.cpp src/shortcut_router.cpp)
target_include_directories(shortcut_router_test PRIVATE "src")
if(MINGW)
	target_link_options(shortcut_router_test PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
add_test(NAME shortcut_router COMMAND shortcut_router_test)
add_executable(undo_manager_test tests/undo_manager_test.cpp src/undo_manager.cpp)
target_include_directories(undo_manager_test PRIVATE "src")
if(MINGW)
	target_link_options(undo_manager_test PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
add_test(NAME undo_manager COMMAND undo_manager_test)
