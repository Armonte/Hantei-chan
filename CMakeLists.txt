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

set(HAN2_MODEL_SOURCES src/framedata_han2.cpp src/han2/han2_container.cpp src/han2/pac_archive.cpp src/han2/img_file.cpp src/han2/misc_formats.cpp src/han2_diff.cpp src/han2_anim.cpp src/framedata_gof1.cpp src/han2/gof1_archive.cpp src/framedata_pb2k1.cpp src/framedata_qoh.cpp src/han2/qoh_dat.cpp)
# quill (header-only async logger, third_party/quill): src/app_log.h
include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/third_party/quill/include")

# PovertyCaster pcmod packages (docs/modmanager/pcmod-manifest_schema1.md): manifest, digests, zip, pack/verify. Pure C++.
# 3D stage view (docs/stage3d/SCOPE.md): fbxex model, DDS, BgList. The GL view (s3d_view.cpp) is editor-only.
set(STAGE3D_SOURCES src/stage3d/s3d_model.cpp src/stage3d/s3d_dds.cpp src/stage3d/s3d_bglist.cpp src/stage3d/s3d_fbxascii.cpp)
set(PCMOD_SOURCES src/pcmod/pcmod_sha256.cpp src/pcmod/pcmod_manifest.cpp src/pcmod/pcmod_zip.cpp src/pcmod/pcmod_package.cpp)
# Mod projects (docs/modmanager/AUTHORING_DESIGN.md): hmod.json, vanilla hash index, change tracking, conflict check, exporters.
set(HMOD_SOURCES src/modproject/hmod_ha6.cpp src/modproject/hmod_vanilla.cpp src/modproject/hmod_project.cpp src/modproject/hmod_track.cpp
	src/modproject/hmod_conflict.cpp src/modproject/hmod_export.cpp src/modproject/hmod_import.cpp)

add_executable(${EXE}
	src/main.cpp
	src/main_frame.cpp
	src/main_pane.cpp
	src/right_pane.cpp
	src/box_pane.cpp
	src/render.cpp
	src/render_target.cpp
	src/png_writer.cpp
	src/workspace_session.cpp
	src/workspace_viewports.cpp
	src/mbaacc_pack.cpp
	src/mbaacc_package.cpp
	${PCMOD_SOURCES} src/pcmod/pcmod_vanilla.cpp src/pcmod/pcmod_export_ui.cpp ${HMOD_SOURCES} src/modproject/hmod_ui.cpp
	${STAGE3D_SOURCES} src/stage3d/s3d_view.cpp
	src/vao.cpp
	src/shader.cpp
	src/texture.cpp
	src/filedialog.cpp
	src/framedata.cpp
	src/framedata_load.cpp
	src/framedata_save.cpp
	src/framedata_ha4.cpp ${HAN2_MODEL_SOURCES}
	src/ha4_parts.cpp
	src/ha4_parts_gl.cpp
	src/ha4_convert.cpp
	src/ha4_character.cpp
	src/han2_character.cpp
	src/han2_pat.cpp
	src/han2_pat_write.cpp
	src/han2_parts_edit.cpp
	src/parts/parts_undo.cpp
	src/han2_export.cpp
	src/han2_pat_gl.cpp
	src/han2_browser.cpp
	src/archive_browser.cpp
	src/archive_browser_ui.cpp
	src/game_table.cpp
	src/han2_thumbs.cpp
	src/fbarc/fb_archive.cpp
	src/fbarc/fb_filepac.cpp
	src/fbarc/fb_modern.cpp
	src/i18n.cpp
	src/i18n_layout.cpp
	src/han2_pac_window.cpp
	src/han2_inspector.cpp
	src/han2_typed_files.cpp
	src/live_reload.cpp
	src/han2/dmp_fob.cpp
	src/han2/fob_file.cpp
	src/han2/mbr_formats.cpp
	src/han2/mb_formats.cpp
	src/han2/mb_cg.cpp
	src/han2/qoh_cg.cpp
	src/cgm/cgm_bank.cpp
	src/cgm/cgm_ops.cpp
	src/cgm/cgm_export.cpp
	src/cgm/cgm_io.cpp
	src/cgm/cgm_palette.cpp
	src/cgm/cgm_struct.cpp
	src/cgm/cgm_usage.cpp
	src/cgm/cgm_project.cpp
	src/cgm/cgm_folder.cpp
	src/cgm/cgm_ccpal.cpp
	src/cgm/fxr_core.cpp
	src/cgm/fxr_panel.cpp
	src/cgm/cgm_window.cpp
	src/han2/qoh_img.cpp
	src/han2/rosa_img.cpp
	src/startup_args.cpp
	src/framestate.cpp
	src/copy_data.cpp
	src/var_refs.cpp
	src/pattern_refs.cpp
	src/ha6_notes.cpp
	src/bgm_player.cpp
	src/hud_colors.cpp
	src/hud_textures.cpp
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
	src/parts/parts_undo.cpp
	src/file_backup.cpp
	src/shortcut_router.cpp
	src/PatEditor/pat_partset_pane.cpp
	src/PatEditor/pat_partset_fb.cpp
	src/PatEditor/pat_part_pane.cpp
	src/PatEditor/pat_shape_pane.cpp
	src/PatEditor/pat_texture_pane.cpp
	src/PatEditor/pat_tool_pane.cpp
	src/parts/parts_partset.cpp
	src/parts/parts_part.cpp
	src/parts/parts_shape.cpp
	src/parts/parts_texture.cpp
	src/parts/pat_texconv.cpp
	src/parts/parts.cpp
	src/app_log.cpp
	# Background (stage) system — ported from bgmk branch.
	src/background/bg_file.cpp
	src/background/bg_renderer.cpp
	src/background/bg_gametex.cpp
	src/background/bg_pat.cpp
	src/background/bg_info.cpp
	src/background/bg_project.cpp
	src/background/bg_inspector.cpp
	src/background/bg_browser.cpp
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
	src/ui/main_frame_gamelink.cpp
	src/game_link.cpp
	src/game_link_panel.cpp
	# Tag / Team panel (experimental, docs/HANTEI_TAG_PANEL.md)
	src/tag_tuning/tag_ini.cpp
	src/tag_tuning/tag_assist.cpp
	src/tag_panel.cpp
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
if(WIN32)
	# WIC PNG export, folder picker, detached-window subclassing; winmm for BGM preview (waveOut).
	target_link_libraries(${EXE} PRIVATE windowscodecs ole32 uuid comctl32 winmm)
endif()

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
	src/framedata_ha4.cpp ${HAN2_MODEL_SOURCES}
	src/misc.cpp
	tinyalloc/tinyalloc.c
)
target_include_directories(roundtrip PRIVATE "." "third_party" "${CMAKE_BINARY_DIR}/generated")
target_compile_definitions(roundtrip PRIVATE WIN32_LEAN_AND_MEAN HA6GUIVERSION="${CMAKE_PROJECT_VERSION}")
target_link_libraries(roundtrip PRIVATE tinyalloc)
if(MINGW)
	target_link_options(roundtrip PRIVATE -static-libgcc -static-libstdc++ -static)
endif()

# Issue #71: edit one pattern of a .txt stack, save, diff (tools/uni/uni_regress.sh).
add_executable(uni_edit_test
	tests/uni_edit_test.cpp
	src/framedata.cpp
	src/framedata_load.cpp
	src/framedata_save.cpp
	src/framedata_ha4.cpp ${HAN2_MODEL_SOURCES}
	src/misc.cpp
	tinyalloc/tinyalloc.c
)
target_include_directories(uni_edit_test PRIVATE "src" "." "third_party" "${CMAKE_BINARY_DIR}/generated")
target_compile_definitions(uni_edit_test PRIVATE WIN32_LEAN_AND_MEAN HA6GUIVERSION="${CMAKE_PROJECT_VERSION}")
target_link_libraries(uni_edit_test PRIVATE tinyalloc)
if(MINGW)
	target_link_options(uni_edit_test PRIVATE -static-libgcc -static-libstdc++ -static)
endif()

# UNI2/MBTL/UNIB data-loss fixes (D13-D18): paste conversion, AT fields, 10 IF
# params, layer tools, BaseData templates, UNIB detection.
add_executable(uni_dataloss_test
	tests/uni_dataloss_test.cpp
	src/framedata.cpp
	src/framedata_load.cpp
	src/framedata_save.cpp
	src/framedata_ha4.cpp ${HAN2_MODEL_SOURCES}
	src/mv_script.cpp
	src/misc.cpp
	tinyalloc/tinyalloc.c
)
target_include_directories(uni_dataloss_test PRIVATE "src" "." "third_party" "${CMAKE_BINARY_DIR}/generated")
target_compile_definitions(uni_dataloss_test PRIVATE WIN32_LEAN_AND_MEAN HA6GUIVERSION="${CMAKE_PROJECT_VERSION}")
target_link_libraries(uni_dataloss_test PRIVATE tinyalloc)
if(MINGW)
	target_link_options(uni_dataloss_test PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
add_test(NAME uni_dataloss COMMAND uni_dataloss_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# Per-game EF/IF labels (src/game_labels.h) on UNI2 and MBTL files.
add_executable(game_labels_test
	tests/game_labels_test.cpp
	src/framedata.cpp
	src/framedata_load.cpp
	src/framedata_save.cpp
	src/framedata_ha4.cpp ${HAN2_MODEL_SOURCES}
	src/misc.cpp
	tinyalloc/tinyalloc.c
)
target_include_directories(game_labels_test PRIVATE "src" "." "third_party" "${CMAKE_BINARY_DIR}/generated")
target_compile_definitions(game_labels_test PRIVATE WIN32_LEAN_AND_MEAN HA6GUIVERSION="${CMAKE_PROJECT_VERSION}")
target_link_libraries(game_labels_test PRIVATE tinyalloc)
if(MINGW)
	target_link_options(game_labels_test PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
add_test(NAME game_labels COMMAND game_labels_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# .pal (130-palette UNI2/MBTL and MBAACC) lossless round trip.
add_executable(pal_roundtrip src/pal_roundtrip.cpp src/pal_file.cpp src/misc.cpp)
target_include_directories(pal_roundtrip PRIVATE "src" "." "third_party" "${CMAKE_BINARY_DIR}/generated")
target_compile_definitions(pal_roundtrip PRIVATE WIN32_LEAN_AND_MEAN)
if(MINGW)
	target_link_options(pal_roundtrip PRIVATE -static-libgcc -static-libstdc++ -static)
endif()

# Standalone smoke test for bg::File loader (no GL needed for parse-only).
add_executable(bg_test
	src/bg_test.cpp
	src/background/bg_file.cpp
	src/background/bg_pat.cpp
	src/background/bg_info.cpp
	src/background/bg_project.cpp
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

# Headless stage renderer for accuracy checks against game captures
# (docs/bg_research/STAGE_AUDIT.md): renders through bg::Renderer offscreen.
add_executable(bg_render
	src/bg_render.cpp
	src/background/bg_file.cpp
	src/background/bg_pat.cpp
	src/background/bg_info.cpp
	src/background/bg_project.cpp
	src/background/bg_renderer.cpp
	src/background/bg_gametex.cpp
	src/parts/parts.cpp
	src/app_log.cpp
	src/parts/parts_part.cpp
	src/parts/parts_partset.cpp
	src/parts/parts_shape.cpp
	src/parts/parts_texture.cpp
	src/cg.cpp
	src/texture.cpp
	src/vao.cpp
	src/shader.cpp
	src/misc.cpp
	src/png_writer.cpp
	tinyalloc/tinyalloc.c
)
target_include_directories(bg_render PRIVATE "." "src" "third_party" "${CMAKE_BINARY_DIR}/generated")
target_compile_definitions(bg_render PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN HA6GUIVERSION="${CMAKE_PROJECT_VERSION}")
target_link_libraries(bg_render PRIVATE tinyalloc glad glm OpenGL::GL windowscodecs ole32 uuid)
if(MINGW)
	target_link_options(bg_render PRIVATE -static-libgcc -static-libstdc++ -static)
endif()

# 32-bit DXT5 helper for game-accurate stage textures (src/bg_dxt32.c): the game
# compresses with the 32-bit d3dx9_36.dll, whose x87 results differ from the x64 DLL.
find_program(I686_MINGW_GCC i686-w64-mingw32-gcc)
if(I686_MINGW_GCC)
	add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/bg_dxt32.exe
		COMMAND ${I686_MINGW_GCC} -O2 -o ${CMAKE_BINARY_DIR}/bg_dxt32.exe ${CMAKE_SOURCE_DIR}/src/bg_dxt32.c -static
		DEPENDS ${CMAKE_SOURCE_DIR}/src/bg_dxt32.c)
	add_custom_target(bg_dxt32 ALL DEPENDS ${CMAKE_BINARY_DIR}/bg_dxt32.exe)
endif()

# Standalone PAT round-trip validator. Links the editor's parts code with
# glad/OpenGL stubs (glad calls no-op without a current context, which is
# fine for the load/save paths we exercise).
# pattex: .pat part textures <-> PNG / DDS (src/parts/pat_texconv.h), and a full-corpus decode/encode check
add_executable(pattex
	src/pattex.cpp
	src/parts/parts.cpp
	src/app_log.cpp
	src/parts/parts_part.cpp
	src/parts/parts_partset.cpp
	src/parts/parts_shape.cpp
	src/parts/parts_texture.cpp
	src/parts/pat_texconv.cpp
	src/png_writer.cpp
	src/cg.cpp
	src/texture.cpp
	src/vao.cpp
	src/shader.cpp
	src/misc.cpp
	tinyalloc/tinyalloc.c
)
target_include_directories(pattex PRIVATE "." "third_party" "${CMAKE_BINARY_DIR}/generated")
target_compile_definitions(pattex PRIVATE WIN32_LEAN_AND_MEAN HA6GUIVERSION="${CMAKE_PROJECT_VERSION}" PAT_HEADLESS)
target_link_libraries(pattex PRIVATE tinyalloc glad glm OpenGL::GL windowscodecs ole32 uuid)
if(MINGW)
	target_link_options(pattex PRIVATE -static-libgcc -static-libstdc++ -static)
endif()

add_executable(pat_roundtrip
	src/pat_roundtrip.cpp
	src/parts/parts.cpp
	src/app_log.cpp
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



# Game Link CLI: the editor's Game Link client (src/game_link.cpp) without the GUI, plus an HA6 edit through the
# editor's own FrameData load/save. Scripted end-to-end checks against a running MBAA.exe (docs/HANTEI_GAME_LINK.md).
# [authoring] shared by the editor, game_link_cli, the mock and the tests (docs/HANTEI_AUTHORING_MODE.md)
set(TAG_SIDECAR_SOURCES src/tag_tuning/tag_ini.cpp src/tag_tuning/tag_sidecar.cpp src/tag_tuning/tag_migrate.cpp
	src/tag_tuning/tag_history.cpp)
set(AUTHORING_CORE_SOURCES src/authoring/authoring_model.cpp src/authoring/authoring_policy.cpp src/authoring/game_launcher.cpp)
# [authoring] the Authoring workspace in the editor (tag_ini.cpp / tag_assist.cpp are already in the list above)
target_sources(${EXE} PRIVATE
	src/tag_tuning/tag_sidecar.cpp src/tag_tuning/tag_migrate.cpp src/tag_tuning/tag_history.cpp src/tag_tuning/tag_widgets.cpp
	${AUTHORING_CORE_SOURCES}
	src/authoring/authoring_window.cpp src/authoring/authoring_tuning_ui.cpp src/authoring/authoring_live_ui.cpp
	src/authoring/frame_summary.cpp src/authoring/mbac_reference.cpp src/authoring/hud_layout.cpp src/authoring/hud_layout_ui.cpp
	src/pal_file.cpp src/game_frame_ring.cpp src/authoring/game_view.cpp
	src/authoring/sysconst_model.cpp src/authoring/sysconst_ui.cpp src/authoring/winquote_model.cpp src/authoring/winquote_ui.cpp src/authoring/se_browser.cpp
	src/background/stage_card.cpp src/background/stage_card_ui.cpp)
add_executable(game_link_cli
	src/game_link_cli.cpp
	src/game_link.cpp
	src/game_link_mock.cpp
	src/game_frame_ring.cpp
	${TAG_SIDECAR_SOURCES}
	${AUTHORING_CORE_SOURCES}
	src/background/bg_file.cpp
	src/background/bg_project.cpp
	src/background/bg_pat.cpp
	src/background/bg_info.cpp
	src/cg.cpp
	src/framedata.cpp
	src/framedata_load.cpp
	src/framedata_save.cpp
	src/framedata_ha4.cpp ${HAN2_MODEL_SOURCES}
	src/misc.cpp
	tinyalloc/tinyalloc.c
)
target_include_directories(game_link_cli PRIVATE "." "third_party" "${CMAKE_BINARY_DIR}/generated")
target_compile_definitions(game_link_cli PRIVATE WIN32_LEAN_AND_MEAN HA6GUIVERSION="${CMAKE_PROJECT_VERSION}")
target_link_libraries(game_link_cli PRIVATE tinyalloc glm)
if(MINGW)
	target_link_options(game_link_cli PRIVATE -static-libgcc -static-libstdc++ -static)
endif()

# Debug tool: dump pattern frame data via the real loader.
add_executable(ha6_dump
	src/ha6_dump.cpp
	src/framedata.cpp
	src/framedata_load.cpp
	src/framedata_save.cpp
	src/framedata_ha4.cpp ${HAN2_MODEL_SOURCES}
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
	src/framedata_ha4.cpp ${HAN2_MODEL_SOURCES}
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
	src/app_log.cpp
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
	src/i18n.cpp
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
	src/i18n.cpp
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
	src/i18n.cpp
	src/i18n_layout.cpp
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
	src/framedata_ha4.cpp ${HAN2_MODEL_SOURCES}
	src/misc.cpp
	src/extension_profile.cpp
	src/imgui_utils.cpp
	src/i18n.cpp
	src/i18n_layout.cpp
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
# fb_part_transform_test: RBO / GOF2 / GOF1 part placement (pivot, rotation, draw order) against the engine's own
# arithmetic, decoded in IDA (docs/formats/fb_part_transform.md). Synthetic poses always; real .DAT files by argument.
add_executable(fb_part_transform_test
	tests/fb_part_transform_test.cpp
	src/han2_pat.cpp
	src/han2_pat_write.cpp
	src/parts/parts.cpp
	src/app_log.cpp
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
target_include_directories(fb_part_transform_test PRIVATE "." "src" "third_party" "${CMAKE_BINARY_DIR}/generated")
target_compile_definitions(fb_part_transform_test PRIVATE WIN32_LEAN_AND_MEAN HA6GUIVERSION="${CMAKE_PROJECT_VERSION}" PAT_HEADLESS)
target_link_libraries(fb_part_transform_test PRIVATE tinyalloc glad glm OpenGL::GL)
if(MINGW)
	target_link_options(fb_part_transform_test PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
add_test(NAME fb_part_transform COMMAND fb_part_transform_test)
# han2_parts_edit_test: PACNyx-parity edits of French-Bread parts (sheet import reaches the PAT, limits, layers, PAT-editor undo channel).
add_executable(han2_parts_edit_test
	tests/han2_parts_edit_test.cpp
	src/app_log.cpp
	src/han2_pat.cpp
	src/han2_pat_write.cpp
	src/han2_parts_edit.cpp
	src/parts/parts_undo.cpp
	src/undo_manager.cpp
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
target_include_directories(han2_parts_edit_test PRIVATE "." "src" "third_party" "${CMAKE_BINARY_DIR}/generated")
target_compile_definitions(han2_parts_edit_test PRIVATE WIN32_LEAN_AND_MEAN HA6GUIVERSION="${CMAKE_PROJECT_VERSION}" PAT_HEADLESS)
target_link_libraries(han2_parts_edit_test PRIVATE tinyalloc glad glm OpenGL::GL)
if(MINGW)
	target_link_options(han2_parts_edit_test PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
add_test(NAME han2_parts_edit COMMAND han2_parts_edit_test)
# part_render_math_test: .pat blend (PRAL), UV flip (PRRV) and AFHK interpolation rules against the game's
# arithmetic (uni2.exe / MBAA.exe, see src/parts/part_render_math.h). Header-only, no data needed.
add_executable(part_render_math_test tests/part_render_math_test.cpp)
target_include_directories(part_render_math_test PRIVATE "src")
target_link_libraries(part_render_math_test PRIVATE glm)
if(MINGW)
	target_link_options(part_render_math_test PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
add_test(NAME part_render_math COMMAND part_render_math_test)
# refs_test: variable / pattern reference tools (issues #75, #44).
add_executable(refs_test
	tests/refs_test.cpp
	src/var_refs.cpp
	src/pattern_refs.cpp
	src/ha6_notes.cpp
	src/framedata.cpp
	src/framedata_load.cpp
	src/framedata_save.cpp
	src/framedata_ha4.cpp ${HAN2_MODEL_SOURCES}
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
# bgm_test: bgm.txt parsing, Ogg decoding and loop wrap (issue #66).
add_executable(bgm_test
	tests/bgm_test.cpp
	src/bgm_player.cpp
	src/misc.cpp
)
target_include_directories(bgm_test PRIVATE "." "src" "third_party")
target_compile_definitions(bgm_test PRIVATE WIN32_LEAN_AND_MEAN)
target_link_libraries(bgm_test PRIVATE winmm)
if(MINGW)
	target_link_options(bgm_test PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
add_test(NAME bgm COMMAND bgm_test)
# hud_test: HUD colour table, MBAA.exe immediates and theme JSON (issue #56).
add_executable(hud_test
	tests/hud_test.cpp
	src/hud_colors.cpp
	src/misc.cpp
)
target_include_directories(hud_test PRIVATE "." "src" "third_party")
target_compile_definitions(hud_test PRIVATE WIN32_LEAN_AND_MEAN)
if(MINGW)
	target_link_options(hud_test PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
add_test(NAME hud COMMAND hud_test)
# MBAC Hantei4 (.DAT) tool: byte-exact round trip, model re-encode check,
# batch MBAC -> HA6 conversion (see docs/HANTEI_MBAC_SUPPORT.md).
add_executable(ha4tool
	src/ha4tool.cpp
	src/framedata.cpp
	src/framedata_load.cpp
	src/framedata_save.cpp
	src/framedata_ha4.cpp ${HAN2_MODEL_SOURCES}
	src/ha4_parts.cpp
	src/ha4_convert.cpp
	src/background/bg_pat.cpp
	src/parts/parts.cpp
	src/app_log.cpp
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

# MBAC -> HA6 converter behaviour fixes (docs/mbac_moon/CONVERTER_FIDELITY.md M1-M12): unit checks always,
# G_CHAOS/SION end-to-end when the MBAC .DAT folder exists (HA4CONV_DAT_DIR, else gamedata/mbac/02).
add_executable(ha4_convert_test
	tests/ha4_convert_test.cpp
	src/framedata.cpp
	src/framedata_load.cpp
	src/framedata_save.cpp
	src/framedata_ha4.cpp ${HAN2_MODEL_SOURCES}
	src/ha4_parts.cpp
	src/ha4_convert.cpp
	src/background/bg_pat.cpp
	src/parts/parts.cpp
	src/app_log.cpp
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
target_include_directories(ha4_convert_test PRIVATE "." "src" "third_party" "${CMAKE_BINARY_DIR}/generated")
target_compile_definitions(ha4_convert_test PRIVATE WIN32_LEAN_AND_MEAN HA6GUIVERSION="${CMAKE_PROJECT_VERSION}" PAT_HEADLESS)
target_link_libraries(ha4_convert_test PRIVATE tinyalloc glad glm OpenGL::GL)
if(MINGW)
	target_link_options(ha4_convert_test PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
add_test(NAME ha4_convert COMMAND ha4_convert_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
# Golden + behaviour checks over the whole converted roster (tools/ha4/ha4_golden.py): converts every MBAC .DAT
# with ha4tool, then compares tag patterns with shipped MBAACC and checks box scale / reaction patterns / IF 51 / ATVV.
# Needs python3, the MBAC .DAT folder and MBAACC data; skipped (not registered) when they are missing.
set(HA4_GOLDEN_DAT "/mnt/c/dev/hantei-chan/gamedata/mbac/02")
set(HA4_GOLDEN_CC "/mnt/c/games/mbaacc/data")
if(EXISTS "${HA4_GOLDEN_DAT}/G_CHAOS.DAT" AND EXISTS "${HA4_GOLDEN_CC}")
	add_test(NAME ha4_golden COMMAND bash tools/ha4/ha4_golden_ctest.sh $<TARGET_FILE:ha4tool> "${HA4_GOLDEN_DAT}" "${HA4_GOLDEN_CC}" "${CMAKE_BINARY_DIR}/ha4_golden_out"
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

# French-Bread RBO / GOF2 model sources (PAC archive, HAN2RBO container, model loader/writer): linked wherever framedata.cpp is.
add_executable(han2tool
	src/han2/han2tool.cpp
	src/cgm/cgm_bank.cpp
	src/cgm/cgm_ops.cpp
	src/cgm/cgm_struct.cpp
	src/han2/fob_file.cpp
	src/han2/replay_file.cpp
	src/han2/fnt_file.cpp
	src/han2_pat.cpp
	src/han2_pat_write.cpp
	src/han2_parts_edit.cpp
	src/han2_export.cpp
	src/png_writer.cpp
	src/framedata.cpp
	src/framedata_load.cpp
	src/framedata_save.cpp
	src/framedata_ha4.cpp ${HAN2_MODEL_SOURCES}
	src/ha4_parts.cpp
	src/ha4_convert.cpp
	src/background/bg_pat.cpp
	src/parts/parts.cpp
	src/app_log.cpp
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
target_include_directories(han2tool PRIVATE "." "src" "third_party" "${CMAKE_BINARY_DIR}/generated")
target_compile_definitions(han2tool PRIVATE WIN32_LEAN_AND_MEAN HA6GUIVERSION="${CMAKE_PROJECT_VERSION}" PAT_HEADLESS)
target_link_libraries(han2tool PRIVATE tinyalloc glad glm OpenGL::GL windowscodecs ole32 uuid)
if(MINGW)
	target_link_options(han2tool PRIVATE -static-libgcc -static-libstdc++ -static)
endif()

# Generalized French-Bread archive layer (PKFileInfo / FilePacHeaderA / PAC v1 / PB) + its command line front end.
set(FBARC_SOURCES src/fbarc/fb_archive.cpp src/fbarc/fb_filepac.cpp src/fbarc/fb_modern.cpp src/han2/pac_archive.cpp src/han2/gof1_archive.cpp src/han2/misc_formats.cpp)
add_executable(fbarctool src/fbarc/fbarctool.cpp ${FBARC_SOURCES})
target_include_directories(fbarctool PRIVATE "." "src")
target_compile_definitions(fbarctool PRIVATE WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
if(MINGW)
	target_link_options(fbarctool PRIVATE -municode -static-libgcc -static-libstdc++ -static)
endif()


# Per-title byte-exact round trip of every archive member (tools/fb/run_fb_suite.sh).
add_executable(fbchartool
	src/fbarc/fbchartool.cpp
	src/cgm/cgm_bank.cpp
	src/cgm/cgm_ops.cpp
	src/cgm/cgm_export.cpp
	src/cgm/cgm_io.cpp
	${FBARC_SOURCES}
	src/framedata.cpp
	src/framedata_load.cpp
	src/framedata_save.cpp
	src/framedata_ha4.cpp ${HAN2_MODEL_SOURCES}
	src/han2/fnt_file.cpp
	src/han2/fob_file.cpp
	src/han2/dmp_fob.cpp
	src/han2/mbr_formats.cpp
	src/han2/mb_formats.cpp
	src/han2/mb_cg.cpp
	src/han2/qoh_cg.cpp
	src/han2/qoh_img.cpp
	src/han2/rosa_img.cpp
	src/png_writer.cpp
	src/ha4_parts.cpp
	src/ha4_convert.cpp
	src/background/bg_file.cpp
	src/background/bg_pat.cpp
	src/background/bg_info.cpp
	src/background/bg_project.cpp
	src/parts/parts.cpp
	src/app_log.cpp
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
target_include_directories(fbchartool PRIVATE "." "src" "third_party" "${CMAKE_BINARY_DIR}/generated")
target_compile_definitions(fbchartool PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN HA6GUIVERSION="${CMAKE_PROJECT_VERSION}" PAT_HEADLESS)
target_link_libraries(fbchartool PRIVATE tinyalloc glad glm OpenGL::GL windowscodecs ole32 uuid)
if(MINGW)
	target_link_options(fbchartool PRIVATE -municode -static-libgcc -static-libstdc++ -static)
endif()

# Unit tests for editor infrastructure (no GL / ImGui). Run from WSL with
# ./build/<name>_test.exe, or ctest.
add_executable(shortcut_router_test tests/shortcut_router_test.cpp src/shortcut_router.cpp)
target_include_directories(shortcut_router_test PRIVATE "src")
if(MINGW)
	target_link_options(shortcut_router_test PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
add_test(NAME shortcut_router COMMAND shortcut_router_test)
add_executable(live_reload_test tests/live_reload_test.cpp src/live_reload.cpp src/misc.cpp tinyalloc/tinyalloc.c)
target_include_directories(live_reload_test PRIVATE "." "src" "third_party")
target_compile_definitions(live_reload_test PRIVATE WIN32_LEAN_AND_MEAN)
target_link_libraries(live_reload_test PRIVATE tinyalloc)
if(MINGW)
	target_link_options(live_reload_test PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
add_test(NAME live_reload COMMAND live_reload_test)

add_executable(stage3d_test tests/stage3d_test.cpp ${STAGE3D_SOURCES})
target_include_directories(stage3d_test PRIVATE "src")
if(MINGW)
	target_link_options(stage3d_test PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
add_test(NAME stage3d COMMAND stage3d_test)

add_executable(undo_manager_test tests/undo_manager_test.cpp src/undo_manager.cpp)
target_include_directories(undo_manager_test PRIVATE "src")
if(MINGW)
	target_link_options(undo_manager_test PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
add_test(NAME undo_manager COMMAND undo_manager_test)
# pat_safety_test: PAT editor file safety (S2): auto-backup rotation, atomic .pat save, the PAT document's dirty flag
# and undo through the parts undo channel (src/parts/parts_undo.h). Uses a gamedata .pat when one is passed.
add_executable(pat_safety_test
	tests/pat_safety_test.cpp
	src/file_backup.cpp
	src/parts/parts_undo.cpp
	src/undo_manager.cpp
	src/parts/parts.cpp
	src/app_log.cpp
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
target_include_directories(pat_safety_test PRIVATE "." "src" "third_party" "${CMAKE_BINARY_DIR}/generated")
target_compile_definitions(pat_safety_test PRIVATE WIN32_LEAN_AND_MEAN HA6GUIVERSION="${CMAKE_PROJECT_VERSION}" PAT_HEADLESS)
target_link_libraries(pat_safety_test PRIVATE tinyalloc glad glm OpenGL::GL)
if(MINGW)
	target_link_options(pat_safety_test PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
add_test(NAME pat_safety COMMAND pat_safety_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
# Real UNI2 / MBTL / MBAACC files when the local game data is there (never committed; docs/audit_2026_10_14 plan, Phase 0).
set(PAT_SAFETY_SAMPLES "")
foreach(_s "dev/hantei-chan/gamedata/uni2/data/chr000/chr000.pat" "dev/hantei-chan/gamedata/mbtl/data/chr000/chr000.pat" "games/mbaacc_tag/data/arc.pat")
	if(EXISTS "/mnt/c/${_s}")
		list(APPEND PAT_SAFETY_SAMPLES "C:/${_s}")
	endif()
endforeach()
if(PAT_SAFETY_SAMPLES)
	add_test(NAME pat_safety_gamedata COMMAND pat_safety_test ${PAT_SAFETY_SAMPLES} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()
add_executable(workspace_session_test tests/workspace_session_test.cpp src/workspace_session.cpp)
target_include_directories(workspace_session_test PRIVATE "src")
if(MINGW)
	target_link_options(workspace_session_test PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
add_test(NAME workspace_session COMMAND workspace_session_test)

# MBAACC package tools (docs/HANTEI_WAVE2.md §6): validator, CRLF repair,
# HA6 layer consolidation, FilePacHeaderA (.p) reader/extractor.
set(MBAACC_PACKAGE_SOURCES
	src/mbaacc_package.cpp
	src/mbaacc_pack.cpp
	src/framedata.cpp
	src/framedata_load.cpp
	src/framedata_save.cpp
	src/framedata_ha4.cpp ${HAN2_MODEL_SOURCES}
	src/misc.cpp
	tinyalloc/tinyalloc.c
)
add_executable(mbaaccpackage src/mbaacc_package_cli.cpp ${MBAACC_PACKAGE_SOURCES})
target_include_directories(mbaaccpackage PRIVATE "." "src" "third_party" "${CMAKE_BINARY_DIR}/generated")
target_compile_definitions(mbaaccpackage PRIVATE WIN32_LEAN_AND_MEAN UNICODE _UNICODE HA6GUIVERSION="${CMAKE_PROJECT_VERSION}")
target_link_libraries(mbaaccpackage PRIVATE tinyalloc)
if(MINGW)
	target_link_options(mbaaccpackage PRIVATE -municode -static-libgcc -static-libstdc++ -static)
endif()
add_executable(package_tools_test tests/package_tools_test.cpp ${MBAACC_PACKAGE_SOURCES})
target_include_directories(package_tools_test PRIVATE "." "src" "third_party" "${CMAKE_BINARY_DIR}/generated")
target_compile_definitions(package_tools_test PRIVATE WIN32_LEAN_AND_MEAN HA6GUIVERSION="${CMAKE_PROJECT_VERSION}")
target_link_libraries(package_tools_test PRIVATE tinyalloc)
if(MINGW)
	target_link_options(package_tools_test PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
add_test(NAME package_tools COMMAND package_tools_test)
add_executable(game_link_test tests/game_link_test.cpp src/game_link.cpp src/game_link_mock.cpp src/game_frame_ring.cpp ${AUTHORING_CORE_SOURCES}
	${TAG_SIDECAR_SOURCES})
target_include_directories(game_link_test PRIVATE "src")
if(MINGW)
	target_link_options(game_link_test PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
add_test(NAME game_link COMMAND game_link_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
# [game-view] the embedded game view: frame ring layout + seqlock + layered compositing + the mock producer end to end
# (docs/HANTEI_AUTHORING_MODE.md §12). Headless: no window is ever created.
add_executable(game_view_test tests/game_view_test.cpp src/game_frame_ring.cpp src/game_link.cpp src/game_link_mock.cpp
	${AUTHORING_CORE_SOURCES} ${TAG_SIDECAR_SOURCES})
target_include_directories(game_view_test PRIVATE "src")
set(PC_FRAME_SHARE_H "$ENV{HOME}/wt/pc-gameview/pc-proto/include/pc/proto/game_frame_share.h"
	CACHE FILEPATH "PovertyCaster's copy of game_frame_share.h, for the no-silent-drift check (optional)")
if(EXISTS "${PC_FRAME_SHARE_H}")
	execute_process(COMMAND wslpath -m "${PC_FRAME_SHARE_H}" OUTPUT_VARIABLE PC_FRAME_SHARE_WIN OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
	if(PC_FRAME_SHARE_WIN)
		target_compile_definitions(game_view_test PRIVATE PC_FRAME_SHARE_H="${PC_FRAME_SHARE_WIN}")
	endif()
endif()
if(MINGW)
	target_link_options(game_view_test PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
add_test(NAME game_view COMMAND game_view_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
# Tag / Team panel: tag_tuning.ini round trip, lever table vs the sample ini and (when on disk) vs PovertyCaster's
# MbaaccTagTuning.hpp, assist slot resolution and the default-assist pick (docs/HANTEI_TAG_PANEL.md).
add_executable(tag_tuning_test tests/tag_tuning_test.cpp src/tag_tuning/tag_ini.cpp src/tag_tuning/tag_assist.cpp
	src/cmdfile/cmd_document.cpp)
target_include_directories(tag_tuning_test PRIVATE "src")
set(PC_TAG_TUNING_HPP "/mnt/c/dev/castergroup/PovertyCaster/pc-adapters/mbaacc/include/mbaacc/MbaaccTagTuning.hpp"
	CACHE FILEPATH "PovertyCaster's MbaaccTagTuning.hpp, for the lever-table drift check (optional)")
if(EXISTS "${PC_TAG_TUNING_HPP}")
	get_filename_component(PC_TAG_TUNING_INC "${PC_TAG_TUNING_HPP}" DIRECTORY)
	get_filename_component(PC_TAG_TUNING_INC "${PC_TAG_TUNING_INC}" DIRECTORY)
	target_include_directories(tag_tuning_test PRIVATE "${PC_TAG_TUNING_INC}")
	target_compile_definitions(tag_tuning_test PRIVATE HAVE_PC_TAG_TUNING=1)
	file(STRINGS "${PC_TAG_TUNING_HPP}" PC_LEVER_HASH_LINES REGEX "leverTableHash\\(")
	if(PC_LEVER_HASH_LINES)
		target_compile_definitions(tag_tuning_test PRIVATE HAVE_PC_LEVER_HASH=1)
	endif()
endif()
if(MINGW)
	target_link_options(tag_tuning_test PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
add_test(NAME tag_tuning COMMAND tag_tuning_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
# [authoring] the sidecar tree, migration and the workspace undo history (docs/HANTEI_AUTHORING_MODE.md §2 / §9.1).
add_executable(tag_sidecar_test tests/tag_sidecar_test.cpp ${TAG_SIDECAR_SOURCES})
target_include_directories(tag_sidecar_test PRIVATE "src")
set(PC_SIDECAR_HPP "/mnt/c/dev/castergroup/PovertyCaster/pc-adapters/mbaacc/include/mbaacc/TagSidecar.hpp"
	CACHE FILEPATH "PovertyCaster's TagSidecar.hpp (mbaacc/authoring), for the resolver cross-check (optional)")
if(EXISTS "${PC_SIDECAR_HPP}")
	get_filename_component(PC_SIDECAR_INC "${PC_SIDECAR_HPP}" DIRECTORY)
	get_filename_component(PC_SIDECAR_INC "${PC_SIDECAR_INC}" DIRECTORY)
	target_include_directories(tag_sidecar_test PRIVATE "${PC_SIDECAR_INC}")
	target_compile_definitions(tag_sidecar_test PRIVATE HAVE_PC_SIDECAR=1)
endif()
if(MINGW)
	target_link_options(tag_sidecar_test PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
add_test(NAME tag_sidecar COMMAND tag_sidecar_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
# [authoring] the pure model: golden LinkMatchSetup bytes (shared with PovertyCaster tests/mbaacc_authoring), validation,
# files to open, the launch environment, saved setups; the roster mirror vs PovertyCaster's MbaaccRoster.hpp when on disk.
set(AUTHORING_MODEL_SOURCES src/authoring/authoring_model.cpp src/authoring/game_launcher.cpp)
add_executable(authoring_model_test tests/authoring_model_test.cpp ${AUTHORING_MODEL_SOURCES})
target_include_directories(authoring_model_test PRIVATE "src")
set(PC_ROSTER_HPP "/mnt/c/dev/castergroup/PovertyCaster/pc-adapters/mbaacc/include/mbaacc/MbaaccRoster.hpp"
	CACHE FILEPATH "PovertyCaster's MbaaccRoster.hpp, for the roster mirror drift check (optional)")
if(EXISTS "${PC_ROSTER_HPP}")
	get_filename_component(PC_ROSTER_INC "${PC_ROSTER_HPP}" DIRECTORY)
	get_filename_component(PC_ROSTER_INC "${PC_ROSTER_INC}" DIRECTORY)
	target_include_directories(authoring_model_test PRIVATE "${PC_ROSTER_INC}")
	target_compile_definitions(authoring_model_test PRIVATE HAVE_PC_ROSTER=1)
endif()
set(PC_PROTO_HPP "/mnt/c/dev/castergroup/PovertyCaster/pc-proto/include/pc/proto/Proto.hpp"
	CACHE FILEPATH "PovertyCaster's Proto.hpp (a branch with the Authoring structs), for the protocol mirror check (optional)")
if(EXISTS "${PC_PROTO_HPP}")
	file(STRINGS "${PC_PROTO_HPP}" PC_PROTO_AUTHORING REGEX "struct LinkMatchSetup")
	if(PC_PROTO_AUTHORING)
		get_filename_component(PC_PROTO_INC "${PC_PROTO_HPP}" DIRECTORY)
		get_filename_component(PC_PROTO_INC "${PC_PROTO_INC}/../.." ABSOLUTE)
		target_include_directories(authoring_model_test PRIVATE "${PC_PROTO_INC}")
		target_compile_definitions(authoring_model_test PRIVATE HAVE_PC_PROTO=1)
	endif()
endif()
if(MINGW)
	target_link_options(authoring_model_test PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
add_test(NAME authoring_model COMMAND authoring_model_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
# [authoring] inline frame data (startup / active / recovery) and the MBAC reference values (§8.9).
add_executable(frame_data_test tests/frame_data_test.cpp src/authoring/frame_summary.cpp src/authoring/mbac_reference.cpp
	src/framedata.cpp src/framedata_load.cpp src/framedata_save.cpp src/framedata_ha4.cpp ${HAN2_MODEL_SOURCES} src/misc.cpp tinyalloc/tinyalloc.c)
target_include_directories(frame_data_test PRIVATE "src" "." "third_party" "${CMAKE_BINARY_DIR}/generated")
target_compile_definitions(frame_data_test PRIVATE WIN32_LEAN_AND_MEAN HA6GUIVERSION="${CMAKE_PROJECT_VERSION}")
target_link_libraries(frame_data_test PRIVATE tinyalloc)
if(MINGW)
	target_link_options(frame_data_test PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
add_test(NAME frame_data COMMAND frame_data_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
# [authoring] the TAG HUD layout sidecar (hud.ini, §9.4): parse / resolve / byte-preserving edits.
add_executable(hud_layout_test tests/hud_layout_test.cpp src/authoring/hud_layout.cpp ${TAG_SIDECAR_SOURCES})
target_include_directories(hud_layout_test PRIVATE "src")
if(MINGW)
	target_link_options(hud_layout_test PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
add_test(NAME hud_layout COMMAND hud_layout_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# pcmod exporter CLI + tests (Hantei_Docs/pcmod_export.md). The fixture and its three pinned digests come from PovertyCaster.
add_executable(pcmodtool src/pcmod/pcmodtool.cpp ${PCMOD_SOURCES} src/pcmod/pcmod_vanilla.cpp src/mbaacc_pack.cpp)
target_include_directories(pcmodtool PRIVATE "." "src" "src/pcmod" "third_party")
target_compile_definitions(pcmodtool PRIVATE WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
if(MINGW)
	target_link_options(pcmodtool PRIVATE -municode -static-libgcc -static-libstdc++ -static)
endif()
add_executable(pcmod_test tests/pcmod_test.cpp ${PCMOD_SOURCES})
target_include_directories(pcmod_test PRIVATE "." "src" "third_party")
if(MINGW)
	target_link_options(pcmod_test PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
add_test(NAME pcmod COMMAND pcmod_test tests/fixtures/pcmod WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME pcmod_cli COMMAND bash tools/pcmod/pcmod_cli_ctest.sh $<TARGET_FILE:pcmodtool> tests/fixtures/pcmod "${CMAKE_BINARY_DIR}/pcmod_cli_out" WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
# hmodtool is the mod-project CLI, hmod_test the unit test (synthetic folders).
add_executable(hmodtool src/modproject/hmodtool.cpp ${HMOD_SOURCES} ${PCMOD_SOURCES} src/pcmod/pcmod_vanilla.cpp src/mbaacc_pack.cpp ${FBARC_SOURCES})
target_include_directories(hmodtool PRIVATE "." "src" "src/pcmod" "third_party")
target_compile_definitions(hmodtool PRIVATE WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
add_executable(hmod_test tests/hmod_test.cpp ${HMOD_SOURCES} ${PCMOD_SOURCES} src/pcmod/pcmod_vanilla.cpp src/mbaacc_pack.cpp ${FBARC_SOURCES})
target_include_directories(hmod_test PRIVATE "." "src" "src/pcmod" "third_party")
target_compile_definitions(hmod_test PRIVATE WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
if(MINGW)
	target_link_options(hmodtool PRIVATE -municode -static-libgcc -static-libstdc++ -static)
	target_link_options(hmod_test PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
add_test(NAME hmod COMMAND hmod_test build/hmod_test_out WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
# System constants editor model (_type.txt / vector.txt in-place edits); also round-trips the vanilla files when present.
add_executable(sysconst_test tests/sysconst_test.cpp src/authoring/sysconst_model.cpp src/authoring/winquote_model.cpp src/misc.cpp)
target_include_directories(sysconst_test PRIVATE "src")
if(MINGW)
	target_link_options(sysconst_test PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
add_test(NAME bg_textini COMMAND bg_test --textini-test)
add_test(NAME stagecard COMMAND pattool stagecard-selftest)
add_test(NAME han2_pacselftest COMMAND han2tool pacselftest build/pacselftest_out WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME sysconst COMMAND sysconst_test build/sysconst_test_out C:/dev/hantei-chan/gamedata/mbaacc_ref/vanilla_0002 C:/games/mbaacc/story/battle WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
# --headless view export (hidden window, FBO readback): PNG size + checksum stability, batch == single shot
add_test(NAME headless_export COMMAND bash tools/ui/headless_ctest.sh $<TARGET_FILE:gonptechan> tests/fixtures/headless/chr000.ha6 "${CMAKE_BINARY_DIR}/headless_out" WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# CG manager core + command line (docs/cg/cg_manager_design.md)
set(CGM_CORE_SOURCES src/cgm/cgm_bank.cpp src/cgm/cgm_ops.cpp src/cgm/cgm_export.cpp src/cgm/cgm_io.cpp src/cgm/cgm_palette.cpp src/cgm/cgm_struct.cpp src/cgm/cgm_folder.cpp src/cgm/cgm_ccpal.cpp)
add_executable(cgmtool src/cgm/cgmtool.cpp src/cgm/cgmtool_ops.cpp src/cgm/cgm_acmoon.cpp src/cgm/fxr_cli.cpp src/cgm/fxr_core.cpp ${CGM_CORE_SOURCES} src/cgm/cgm_usage.cpp src/cgm/cgm_project.cpp src/framedata.cpp src/framedata_load.cpp src/framedata_save.cpp src/framedata_ha4.cpp ${HAN2_MODEL_SOURCES} src/cg.cpp src/misc.cpp src/png_writer.cpp tinyalloc/tinyalloc.c)
target_include_directories(cgmtool PRIVATE "." "src" "third_party" "${CMAKE_BINARY_DIR}/generated")
target_compile_definitions(cgmtool PRIVATE WIN32_LEAN_AND_MEAN HA6GUIVERSION="${CMAKE_PROJECT_VERSION}")
target_link_libraries(cgmtool PRIVATE tinyalloc windowscodecs ole32 uuid)
if(MINGW)
	target_link_options(cgmtool PRIVATE -static-libgcc -static-libstdc++ -static)
endif()

# Effect recolour authoring (docs/cg/fxrecolor_config_spec.md): ruleset text round trip through the runtime's own parser/writer.
add_test(NAME fxr_ini COMMAND cgmtool fxr check docs/cg/effect_recolor_data/dgv_akiha.ini WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
# Shared-bank safety and cgtool workflows on real game data (skipped when the data is absent): an insert / move / delete
# rewrites the references of EVERY project that draws the bank (MBAACC moons + b_akiha, MBTL variants), shared files stay
# untouched, the inverse gives every file back byte-identical; cgtool folder import / sync / build / resized replace.
# The configure step runs in WSL (cmake is Linux, the tests are Windows exes): check the data through /mnt/c as well.
function(fb_data_exists out winpath)
	string(SUBSTRING "${winpath}" 0 1 _d)
	string(TOLOWER "${_d}" _d)
	string(SUBSTRING "${winpath}" 2 -1 _rest)
	if(EXISTS "${winpath}" OR EXISTS "/mnt/${_d}${_rest}")
		set(${out} TRUE PARENT_SCOPE)
	else()
		set(${out} FALSE PARENT_SCOPE)
	endif()
endfunction()
fb_data_exists(_has "C:/games/mbaacc/data/akiha.cg")
if(_has)
	add_test(NAME cgm_project_refs_mbaacc COMMAND cgmtool project-refs-check C:/games/mbaacc/data/akiha.cg --tmp cgm_t1)
	add_test(NAME cgm_folder_mbaacc COMMAND cgmtool folder-check C:/games/mbaacc/data/neco.cg --tmp cgm_t2)
	add_test(NAME cgm_cc_palettes COMMAND cgmtool cc-check C:/games/mbaacc/data/arc.pal C:/games/mbaacc/data/akiha.pal)
endif()
fb_data_exists(_has "C:/dev/hantei-chan/gamedata/mbtl/data/chr000/chr000.cg")
if(_has)
	add_test(NAME cgm_project_refs_mbtl COMMAND cgmtool project-refs-check C:/dev/hantei-chan/gamedata/mbtl/data/chr000/chr000.cg --tmp cgm_t3)
	add_test(NAME cgm_palette_uni_mbtl COMMAND cgmtool pal-check-uni C:/dev/hantei-chan/gamedata/mbtl/data/chr000/chr000.pal)
endif()
fb_data_exists(_has "C:/dev/hantei-chan/gamedata/mbtl/BattleRes/talk/st_pat/chr012/chr012_ed00.pat")
if(_has)
	add_test(NAME pat_textures_x8 COMMAND pattex check C:/dev/hantei-chan/gamedata/mbtl/BattleRes/talk/st_pat/chr012/chr012_ed00.pat)
endif()
# ActCadenza moon-3 rebase (cgm_acmoon.cpp): sion has re-canvassed and missing images, nero the most missing ones.
# The command re-reads its outputs and fails unless every CG reference resolves to the same picture.
fb_data_exists(_has "C:/dev/hantei-chan/gamedata/mbac_to_cc/roster_v3/sion/sion.ha6")
if(_has)
	foreach(_c sion nero)
		add_test(NAME cgm_ac_moon3_${_c} COMMAND cgmtool ac-moon3 C:/dev/hantei-chan/gamedata/mbac_to_cc/roster_v3/${_c}/${_c}.ha6
			C:/dev/hantei-chan/gamedata/mbac_to_cc/roster_v3/${_c}/${_c}.cg C:/dev/hantei-chan/gamedata/mbaacc_ref/vanilla_0002/${_c}.cg
			--ha6-out acm3_${_c}_3.HA6 --cg-out acm3_${_c}_3.cg)
	endforeach()
endif()
fb_data_exists(_has "C:/dev/hantei-chan/gamedata/mbac/02/GAKIHA.DAT")
if(_has)   # BMP Cutter2 (pre-baked pages) read from the .DAT: byte-exact, decode == CG, structure, edit locality
	add_test(NAME cgm_cutter2_gakiha COMMAND cgmtool check C:/dev/hantei-chan/gamedata/mbac/02/GAKIHA.DAT)
	add_test(NAME cgm_cutter2_gakiha_struct COMMAND cgmtool struct-check C:/dev/hantei-chan/gamedata/mbac/02/GAKIHA.DAT)
	add_test(NAME cgm_cutter2_gakiha_rt COMMAND cgmtool roundtrip C:/dev/hantei-chan/gamedata/mbac/02/GAKIHA.DAT --tmp cgm_t4)
endif()
# DGV regression oracle: needs his project (external, /home/teo/dev/refs/dgv_better_akiha/extracted) and python3 + numpy + PIL; skipped when absent.
if(EXISTS "/home/teo/dev/refs/dgv_better_akiha/extracted/Better Akiha v2")
	add_test(NAME fxr_dgv_oracle COMMAND python3 tools/fxrecolor/dgv_oracle.py "/home/teo/dev/refs/dgv_better_akiha/extracted/Better Akiha v2" $<TARGET_FILE:cgmtool> docs/cg/effect_recolor_data/dgv_akiha.ini 4 WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

# Squirrel 2.2.4 stable (zlib licence), the VM every post-MBAACC French-Bread game embeds
# (UNIB, DFCI, UNI2, MBTL). SQ_FB_COMPAT keeps the games' 32-bit integer model on x64
# (third_party/squirrel-2.2.4/PATCHES.md). docs/squirrel/SCOPE.md has the per-game evidence.
file(GLOB SQ224_SOURCES third_party/squirrel-2.2.4/squirrel/*.cpp third_party/squirrel-2.2.4/sqstdlib/*.cpp)
add_library(squirrel224 STATIC ${SQ224_SOURCES})
target_include_directories(squirrel224 PUBLIC "${CMAKE_SOURCE_DIR}/third_party/squirrel-2.2.4/include")
target_compile_definitions(squirrel224 PUBLIC SQ_FB_COMPAT)
target_compile_options(squirrel224 PRIVATE -w)

# sqtool: compile every script of a game with its exact VM; --run executes Battle_Init/Battle_Std, --chars then every character folder
# against native stubs generated from the IDB API table (docs/squirrel/api/<game>_api.json).
add_executable(sqtool src/squirrel/sqtool.cpp)
target_include_directories(sqtool PRIVATE "third_party")
target_link_libraries(sqtool PRIVATE squirrel224)
if(MINGW)
	target_link_options(sqtool PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
# Game data lives outside the repo (gamedata/ is reproducible, not committed); the test is skipped (77)
# when it is missing. The fixture test always runs.
# A Windows-side path: sqtool.exe runs as a Windows process (WSL interop), so a /mnt/c path would not resolve.
set(SQTOOL_GAMEDATA_DIR "C:/dev/hantei-chan/gamedata" CACHE STRING "Extracted French-Bread game data root (Windows path)")
add_test(NAME sqtool_fixture COMMAND sqtool compile tests/fixtures/squirrel/fb_style_move.txt WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME sqtool_fixture_bad COMMAND sqtool compile tests/fixtures/squirrel/syntax_error.txt WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(sqtool_fixture_bad PROPERTIES WILL_FAIL TRUE)
add_test(NAME sqtool_mbtl COMMAND sqtool check ${SQTOOL_GAMEDATA_DIR}/mbtl --api docs/squirrel/api/mbtl_api.json --chars WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME sqtool_uni2 COMMAND sqtool check ${SQTOOL_GAMEDATA_DIR}/uni2 --api docs/squirrel/api/uni2_api.json --chars WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(sqtool_mbtl sqtool_uni2 PROPERTIES SKIP_RETURN_CODE 77)

# Squirrel script editor (phase P2/P3, docs/squirrel/SCOPE.md §2.4): CP932 document model + sandboxed
# in-editor VM, and the dockable "Script" panel. Widget: goossens/ImGuiColorTextEdit (MIT, vendored,
# docs/squirrel/EDITOR_WIDGET_TRIAGE.md).
add_library(sqeditor_core STATIC src/squirrel/script_document.cpp src/squirrel/script_vm.cpp src/squirrel/script_index.cpp src/cmdfile/cmd_io.cpp)
target_include_directories(sqeditor_core PUBLIC "src" "third_party")
target_compile_definitions(sqeditor_core PRIVATE WIN32_LEAN_AND_MEAN)
target_link_libraries(sqeditor_core PUBLIC squirrel224)
if(TARGET ${EXE})
	target_sources(${EXE} PRIVATE src/squirrel/script_panel.cpp src/ui/main_frame_script.cpp third_party/ImGuiColorTextEdit/TextEditor.cpp)
	set_source_files_properties(third_party/ImGuiColorTextEdit/TextEditor.cpp PROPERTIES COMPILE_OPTIONS "-w")
	target_link_libraries(${EXE} PRIVATE sqeditor_core)
endif()

add_executable(squirrel_index_test tests/squirrel_index_test.cpp)
target_include_directories(squirrel_index_test PRIVATE "src")
target_link_libraries(squirrel_index_test PRIVATE sqeditor_core)
if(MINGW)
	target_link_options(squirrel_index_test PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
add_test(NAME squirrel_index COMMAND squirrel_index_test)
add_executable(squirrel_doc_test tests/squirrel_doc_test.cpp)
target_link_libraries(squirrel_doc_test PRIVATE sqeditor_core)
if(MINGW)
	target_link_options(squirrel_doc_test PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
add_test(NAME squirrel_doc COMMAND squirrel_doc_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
# Every shipped .txt of each extracted game must round-trip byte-identical (skipped when absent).
add_test(NAME squirrel_doc_roundtrip COMMAND squirrel_doc_test --games ${SQTOOL_GAMEDATA_DIR}/mbtl ${SQTOOL_GAMEDATA_DIR}/uni2 D:/Games/unib WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(squirrel_doc_roundtrip PROPERTIES SKIP_RETURN_CODE 77)

# Part set renumbering (src/parts/pat_renumber.h, Hantei_Docs/pat_renumber.md): the PAT editor's part set list and pattool.
target_sources(${EXE} PRIVATE src/parts/pat_renumber.cpp src/PatEditor/pat_partset_renumber.cpp)
add_executable(pattool src/pattool.cpp src/background/stage_card.cpp src/parts/pat_renumber.cpp src/parts/pat_append.cpp src/parts/pat_texconv.cpp src/parts/parts_undo.cpp src/undo_manager.cpp src/file_backup.cpp
	src/parts/parts.cpp src/app_log.cpp src/parts/parts_part.cpp src/parts/parts_partset.cpp src/parts/parts_shape.cpp src/parts/parts_texture.cpp
	src/cgm/cgm_project.cpp src/cgm/cgm_usage.cpp src/framedata.cpp src/framedata_load.cpp src/framedata_save.cpp src/framedata_ha4.cpp ${HAN2_MODEL_SOURCES}
	src/cg.cpp src/texture.cpp src/vao.cpp src/shader.cpp src/misc.cpp src/png_writer.cpp tinyalloc/tinyalloc.c)
target_include_directories(pattool PRIVATE "." "src" "third_party" "${CMAKE_BINARY_DIR}/generated")
target_compile_definitions(pattool PRIVATE WIN32_LEAN_AND_MEAN HA6GUIVERSION="${CMAKE_PROJECT_VERSION}" PAT_HEADLESS)
target_link_libraries(pattool PRIVATE tinyalloc glad glm OpenGL::GL windowscodecs ole32 uuid)
if(MINGW)
	target_link_options(pattool PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
fb_data_exists(_has "C:/dev/hantei-chan/gamedata/uni2/data/_csel/csel_chr007.pat")
if(_has)
	# UNI2 csel .pat files and their HA6 users: move / insert / delete / place / swap draw the same part set at every reference
	add_test(NAME pat_renumber_csel007_move COMMAND pattool check C:/dev/hantei-chan/gamedata/uni2/data/_csel/csel_chr007.pat --tmp pat_rn1 --move 1-11:40 --insert 5:3)
	add_test(NAME pat_renumber_csel023_ops COMMAND pattool check C:/dev/hantei-chan/gamedata/uni2/data/_csel/csel_chr023.pat --tmp pat_rn2 --insert 3:10 --swap 13:15 --move 100-120:3)
	add_test(NAME pat_renumber_csel018_ops COMMAND pattool check C:/dev/hantei-chan/gamedata/uni2/data/_csel/csel_chr018.pat --tmp pat_rn3 --move 20-25:0 --insert 30:2 --place 2-4:60 --swap 7:9)
	# deleting / clearing a part set a frame draws is refused; deleting unreferenced empty ids closes up
	add_test(NAME pat_renumber_csel018_refuse COMMAND pattool check C:/dev/hantei-chan/gamedata/uni2/data/_csel/csel_chr018.pat --tmp pat_rn5 --delete 2-4 --expect-refused)
	add_test(NAME pat_renumber_csel007_delete COMMAND pattool check C:/dev/hantei-chan/gamedata/uni2/data/_csel/csel_chr007.pat --tmp pat_rn6 --insert 2:5 --delete 2-6 --clear 700-720)
	add_test(NAME pat_renumber_effect COMMAND pattool check C:/dev/hantei-chan/gamedata/uni2/data/sys_effect.pat --tmp pat_rn4 --insert 0:2)
	set_tests_properties(pat_renumber_csel007_move pat_renumber_csel023_ops pat_renumber_csel018_ops pat_renumber_csel018_refuse pat_renumber_csel007_delete pat_renumber_effect PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endif()
# Appending part sets of one .pat to another (src/parts/pat_append.h): the ActCadenza moon-3 part sets into MBAACC's
# .pat. The destination's own records stay byte-identical; each appended part set draws the same parts, cut-outs,
# shapes and texture pixels as in the source.
fb_data_exists(_has "C:/dev/hantei-chan/gamedata/mbac_to_cc/roster_v3/arc/arc.pat")
fb_data_exists(_has2 "C:/dev/hantei-chan/gamedata/mbaacc_ref/vanilla_0002/arc.pat")
if(_has AND _has2)
	add_test(NAME pat_append_arc COMMAND pattool append-check C:/dev/hantei-chan/gamedata/mbaacc_ref/vanilla_0002/arc.pat C:/dev/hantei-chan/gamedata/mbac_to_cc/roster_v3/arc/arc.pat --tmp pat_ap1)
	add_test(NAME pat_append_warc COMMAND pattool append-check C:/dev/hantei-chan/gamedata/mbaacc_ref/vanilla_0002/warc.pat C:/dev/hantei-chan/gamedata/mbac_to_cc/roster_v3/warc/warc.pat --tmp pat_ap2)
	add_test(NAME pat_append_hisui_sets COMMAND pattool append-check C:/dev/hantei-chan/gamedata/mbaacc_ref/vanilla_0002/hisui.pat C:/dev/hantei-chan/gamedata/mbac_to_cc/roster_v3/hisui/hisui.pat --tmp pat_ap3 --sets 0-10,30-40)
	set_tests_properties(pat_append_arc pat_append_warc pat_append_hisui_sets PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
	fb_data_exists(_has3 "C:/dev/hantei-chan/gamedata/uni2/data/_csel/csel_chr007.pat")
	if(_has3)
		# MBAACC PGTX source into a UNI2 .pat: textures re-encoded as A8R8G8B8 PGT2, cut-outs drawn with a plane shape
		add_test(NAME pat_append_mbaacc_to_uni2 COMMAND pattool append-check C:/dev/hantei-chan/gamedata/uni2/data/_csel/csel_chr007.pat C:/dev/hantei-chan/gamedata/mbac_to_cc/roster_v3/arc/arc.pat --tmp pat_ap4 --sets 0-30)
		set_tests_properties(pat_append_mbaacc_to_uni2 PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
	endif()
endif()
