# Native Linux build support (lane LINUX deliverable 3, PLAN.md P5 step 1).
#
# build_linux.sh sets HANTEI_LINUX=ON. The Windows-only targets are excluded
# here rather than in the target definitions, so the Windows build is untouched:
# the GUI and the tools that still include windows.h are simply not part of the
# native default build. The list of targets that do build natively, and the exact
# error each excluded one hits, is docs/render/linux.md section 9.

if(NOT HANTEI_LINUX)
	return()
endif()

message(STATUS "Hantei-chan: native Linux build (HANTEI_LINUX=ON)")

# ---------------------------------------------------------------------------------------
# What cannot be built natively
# ---------------------------------------------------------------------------------------
# The GUI needs the GL context P0/P1 replace and the Win32 dialog paths, so it stays
# Windows-only until P5 wires the SDL3 dialog and the DXVK device.
#
# The rest is what is still blocked after src/misc.cpp and src/framedata_load.cpp moved onto
# src/platform/portable (lane LINUX task (a)). Each one names its blocker here, measured with
# `ninja -C build-linux <target>` and the first `fatal error:` line:
#   - src/vao.cpp, src/shader.cpp: glad + windows.h (the host tools run GL calls with no
#     context, which is fine at run time, but the headers are Win32 only).
#   - src/png_writer.cpp: WIC (wincodecs.h, shobjidl.h) for PNG I/O and the folder picker.
#   - src/cmdfile/cmd_io.cpp, src/i18n.cpp: their own windows.h (cp932 + GetPrivateProfile).
#   - src/tag_tuning/*.cpp, tag_sidecar/tag_migrate, src/game_frame_ring.cpp: file mappings
#     and MoveFileEx (CreateFileMappingW, MoveFileExW).
#   - src/authoring/game_launcher.cpp, src/background/bg_gametex.cpp, src/stage3d/s3d_view.cpp:
#     CreateProcess / CreateToolhelp32Snapshot.
#   - src/han2/han2tool.cpp, src/squirrel/sqtool.cpp, src/fbarc/*tool.cpp,
#     src/parts/pat_renumber.cpp, src/file_backup.cpp: their own windows.h.
#   - src/bgm_player.cpp: waveOut. src/hlsl/hlslcompile.cpp: the d3dcompiler_47 OS DLL.
set(HANTEI_LINUX_BLOCKED
	${EXE} bg_dxt32 hlslcompile
	bg_render bg_test bgm_test cgmtex cgmtool cmdfile_roundtrip cmdfile_test cmdfile_ui_smoke
	fb_part_transform_test fbchartool fpanim_install_test game_link_cli game_link_test game_view_test
	ha4_convert_test ha4tool han2_parts_edit_test han2tool hmod_test hmodtool hud_layout_test
	mbaaccpackage package_tools_test pat_dump pat_roundtrip pat_safety_test pattern_search_test
	pattex pattool pcmodtool shared_clipboard_test sqtool squirrel_doc_test squirrel_index_test
	tag_sidecar_test undo_manager_test authoring_model_test
	CACHE STRING "Targets excluded from the native Linux build" FORCE)
foreach(_hc_t IN LISTS HANTEI_LINUX_BLOCKED)
	if(TARGET ${_hc_t})
		set_target_properties(${_hc_t} PROPERTIES EXCLUDE_FROM_ALL TRUE)
	endif()
endforeach()

# ---------------------------------------------------------------------------------------
# What is built natively
# ---------------------------------------------------------------------------------------
# Targets that compile here: no GL, no SDL and no Win32. The CLI tools at the end are the ones
# lane LINUX task (a) unblocked when misc.cpp / framedata_load.cpp moved onto the portable layer.
set(HANTEI_LINUX_TARGETS
	# unit and self tests
	portable_test shortcut_router_test workspace_session_test project_entries_test
	part_render_math_test stage3d_test pcmod_test tag_tuning_test script_layers_test
	fpanim_link_test frame_data_test refs_test hud_test live_reload_test preview_sim_tool
	sysconst_test uni_dataloss_test game_labels_test fx_ctab_test fx_inventory_test hlsl_ref_test
	pal_roundtrip
	# CLI tools
	ha6_dump roundtrip uni_edit_test fbarctool
	CACHE STRING "Targets the native Linux build compiles" FORCE)

# The ctest entries whose executable is in that set. Everything else is labelled
# windows-only and DISABLED, so `ctest` on the native tree runs exactly what can run
# and a disabled test still shows in `ctest -N`.
set(HANTEI_LINUX_NATIVE_TESTS
	portable shortcut_router workspace_session project_entries part_render_math stage3d pcmod
	tag_tuning script_layers fpanim_link frame_data refs hud live_reload preview_sim sysconst
	uni_dataloss game_labels fx_ctab fx_inventory hlsl_ref
	ha6_roundtrip_fixture glsl_identity render_parity_tools rhi_lint
	CACHE STRING "ctest names the native Linux build runs" FORCE)

# ---- DXVK-native smoke (lane LINUX deliverable 4) ---------------------------------------
# The vendored release .so is loaded at run time (SDL_LoadObject), so nothing links it; the
# rpath exists only so the loader finds libdxvk_dxgi.so next to it when DXVK pulls it in.
# deps/dxvk-native is fetched by tools/linux/fetch_dxvk.sh, never committed: see section 10.
set(HANTEI_DXVK_DIR "${CMAKE_SOURCE_DIR}/deps/dxvk-native")
if(EXISTS "${HANTEI_DXVK_DIR}/lib/libdxvk_d3d9.so")
	# DXVK's SDL3 WSI dlopen()s "libSDL3.so.0" by name. Our SDL3 is static, so wrap the archive
	# into a shared object; the whole-archive link is what makes the .so export the symbols.
	if(TARGET SDL3-static)
		add_library(SDL3_shared SHARED tools/linux/sdl3_shared_stub.c)
		target_link_libraries(SDL3_shared PRIVATE
			"-Wl,--whole-archive" "$<TARGET_FILE:SDL3-static>" "-Wl,--no-whole-archive")
		set_target_properties(SDL3_shared PROPERTIES
			OUTPUT_NAME SDL3
			SOVERSION 0
			VERSION 0.2.0
			LINK_DEPENDS "$<TARGET_FILE:SDL3-static>")
	endif()
	add_executable(d3d9_smoke tools/linux/d3d9_smoke.cpp)
	target_include_directories(d3d9_smoke PRIVATE "${HANTEI_DXVK_DIR}/include")
	# SDL3_shared is a plain wrapper over the archive, so it carries no include directories of
	# its own; add the submodule's headers explicitly.
	target_include_directories(d3d9_smoke PRIVATE
		"${PROJECT_SOURCE_DIR}/deps/SDL/include"
		"${CMAKE_BINARY_DIR}/SDL/include-revision")
	target_compile_definitions(d3d9_smoke PRIVATE
		# wsi/native_wsi.h tests the macro's value (#elif DXVK_WSI_SDL3), so it must be 1.
		DXVK_WSI_SDL3=1
		HANTEI_DXVK_LIB="${HANTEI_DXVK_DIR}/lib/libdxvk_d3d9.so"
		WIN32_LEAN_AND_MEAN)
	# Link the shared SDL3, not the static one: DXVK's WSI dlopen()s libSDL3.so.0 and passes
	# SDL_Window* across the boundary. With a static copy in the exe there are two SDL3
	# instances and DXVK sees an invalid window (measured: "SDL3 WSI:
	# SDL_GetWindowSizeinPixels: Invalid window"). One shared SDL3 for both avoids that.
	target_link_libraries(d3d9_smoke PRIVATE SDL3_shared)
	target_link_options(d3d9_smoke PRIVATE "-Wl,-rpath,${HANTEI_DXVK_DIR}/lib")
	add_test(NAME d3d9_smoke COMMAND d3d9_smoke)
	# lavapipe + the offscreen SDL video driver: a real Vulkan device, no GPU.
	set_tests_properties(d3d9_smoke PROPERTIES
		LABELS gpu
		ENVIRONMENT "VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json;SDL_VIDEO_DRIVER=offscreen;DXVK_WSI_DRIVER=SDL3")
	list(APPEND HANTEI_LINUX_TARGETS d3d9_smoke)
	list(APPEND HANTEI_LINUX_NATIVE_TESTS d3d9_smoke)
	list(REMOVE_DUPLICATES HANTEI_LINUX_TARGETS)
	list(REMOVE_DUPLICATES HANTEI_LINUX_NATIVE_TESTS)
else()
	message(STATUS "  d3d9_smoke: skipped (${HANTEI_DXVK_DIR}/lib/libdxvk_d3d9.so missing)")
endif()

# ---- vkd3d-shader HLSL smoke (lane LINUX deliverable 5) ---------------------------------
# libvkd3d-shader is built out of tree into build-linux/vkd3d by build_linux.sh (autotools;
# docs/render/linux.md section 11), and linked dynamically: it is LGPL-2.1 and PLAN.md
# section 9 requires the dynamic link.
set(HANTEI_VKD3D_DIR "${CMAKE_BINARY_DIR}/vkd3d")
set(HANTEI_VKD3D_SO "${HANTEI_VKD3D_DIR}/.libs/libvkd3d-shader.so")
if(EXISTS "${HANTEI_VKD3D_SO}")
	add_executable(hlsl_smoke tools/linux/hlsl_smoke.cpp)
	target_include_directories(hlsl_smoke PRIVATE "${CMAKE_SOURCE_DIR}/deps/vkd3d/include")
	target_link_libraries(hlsl_smoke PRIVATE "${HANTEI_VKD3D_SO}")
	target_link_options(hlsl_smoke PRIVATE "-Wl,-rpath,${HANTEI_VKD3D_DIR}/.libs")
	add_test(NAME hlsl_smoke COMMAND hlsl_smoke "${CMAKE_SOURCE_DIR}/res/shaders")
	set_tests_properties(hlsl_smoke PROPERTIES LABELS native)
	list(APPEND HANTEI_LINUX_TARGETS hlsl_smoke)
	list(APPEND HANTEI_LINUX_NATIVE_TESTS hlsl_smoke)
	list(REMOVE_DUPLICATES HANTEI_LINUX_TARGETS)
	list(REMOVE_DUPLICATES HANTEI_LINUX_NATIVE_TESTS)
else()
	message(STATUS "  hlsl_smoke: skipped (${HANTEI_VKD3D_SO} missing)")
endif()

set(HANTEI_LINUX_TARGETS "${HANTEI_LINUX_TARGETS}" CACHE STRING "Targets the native Linux build compiles" FORCE)
set(HANTEI_LINUX_NATIVE_TESTS "${HANTEI_LINUX_NATIVE_TESTS}" CACHE STRING "ctest names the native Linux build runs" FORCE)

# ninja with no target builds `all`, which still reaches a target through its own directory
# even when EXCLUDE_FROM_ALL is set, so name the native targets. Write the list next to the
# build so build_linux.sh does not keep a second copy; one name per line.
set(_hc_native_list "")
foreach(_hc_t IN LISTS HANTEI_LINUX_TARGETS)
	string(APPEND _hc_native_list "${_hc_t}\n")
endforeach()
file(WRITE "${CMAKE_BINARY_DIR}/native_targets.txt" "${_hc_native_list}")

# Label every test: the ones the native build produces are `native`, the rest are
# `windows-only` and disabled so ctest runs exactly what can run.
# Append to the labels, never replace: d3d9_smoke, render_parity and headless_export already
# carry `gpu`, and `ctest -LE gpu` must still skip them.
get_property(_hc_tests DIRECTORY PROPERTY TESTS)
foreach(_hc_t IN LISTS _hc_tests)
	get_property(_hc_labels TEST ${_hc_t} PROPERTY LABELS)
	if(_hc_t IN_LIST HANTEI_LINUX_NATIVE_TESTS)
		list(APPEND _hc_labels native)
	else()
		list(APPEND _hc_labels windows-only)
		set_tests_properties(${_hc_t} PROPERTIES DISABLED TRUE)
	endif()
	list(REMOVE_DUPLICATES _hc_labels)
	set_tests_properties(${_hc_t} PROPERTIES LABELS "${_hc_labels}")
endforeach()
