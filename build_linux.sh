#!/bin/bash
# Native Linux build of the non-GUI targets (lane LINUX deliverable 3, PLAN.md
# P5 step 1). CMake + Ninja + gcc into build-linux/; no mingw, no GPU.
#
#   ./build_linux.sh              configure + build the native targets, then ctest
#   ./build_linux.sh configure    configure only
#   ./build_linux.sh build        build only (uses an existing build-linux/)
#   ./build_linux.sh test         run the native ctests
#   ./build_linux.sh vkd3d        build deps/vkd3d's libvkd3d-shader (needed by the
#                                 hlsl_smoke test; run once before the first 'all')
#   ./build_linux.sh clean        remove build-linux/ (never needed for a normal
#                                 rebuild; say why if you use it)
#
# The GUI and the tools that still include windows.h are excluded by
# cmake/linux.cmake; docs/render/linux.md section 9 is the list and the reason.
set -euo pipefail

cd "$(dirname "$0")"

BUILD=build-linux
step="${1:-all}"

command -v cmake >/dev/null || { echo "cmake is missing"; exit 1; }
command -v ninja >/dev/null || { echo "ninja is missing"; exit 1; }
command -v gcc   >/dev/null || { echo "gcc is missing"; exit 1; }

# src/platform/portable is not the only thing this lane builds: the HLSL smoke
# needs libvkd3d-shader, which has its own autotools build (PLAN.md P5 step 3).
# Two system tools it wants are missing here and are worked around, never patched
# into the submodule:
#   - the perl JSON module (configure insists on it, only decode_json() is used):
#     tools/linux/perl-shims/JSON.pm on PERL5LIB.
#   - the SPIR-V headers (only in Vulkan SDKs): tools/linux/spirv-headers/ on
#     CPPFLAGS. Fetched from KhronosGroup/SPIRV-Headers, see section 11 of the doc.
# widl comes from mingw-w64 and is passed explicitly because the unqualified
# name is not on PATH.
export PERL5LIB="$PWD/tools/linux/perl-shims${PERL5LIB:+:$PERL5LIB}"
export CPPFLAGS="-I$PWD/tools/linux/spirv-headers${CPPFLAGS:+ $CPPFLAGS}"
WIDL_BIN="$(command -v widl || command -v x86_64-w64-mingw32-widl || true)"

vkd3d_build() {
	local src="$PWD/deps/vkd3d"
	local dir="$PWD/$BUILD/vkd3d"
	[ -f "$src/configure" ] || { echo "deps/vkd3d/configure is missing (submodule not initialised?)"; exit 1; }
	mkdir -p "$dir"
	# The generated headers (widl, perl) are not dependencies of the library in
	# upstream's Makefile, so build them first; then libvkd3d-shader links against
	# libvkd3d-common.
	if [ ! -f "$dir/Makefile" ]; then
		WIDL="$WIDL_BIN" "$src/configure" --disable-tests --disable-demos --prefix="$dir/install"
	fi
	make -C "$dir" -j"${HC_JOBS:-8}" \
		include/private/vkd3d_version.h include/private/spirv_grammar.h \
		include/vkd3d_d3dx9shader.h include/vkd3d_d3dcommon.h include/vkd3d_dxgitype.h \
		include/vkd3d_dxgibase.h include/vkd3d_dxgiformat.h include/vkd3d_d3d12.h \
		include/vkd3d_d3d12shader.h include/vkd3d_d3d12sdklayers.h \
		include/vkd3d_d3d10shader.h include/vkd3d_d3d10_1shader.h include/vkd3d_d3d11shader.h \
		include/vkd3d_dxgi.h include/vkd3d_dxgi1_2.h include/vkd3d_dxgi1_3.h include/vkd3d_dxgi1_4.h
	make -C "$dir" -j"${HC_JOBS:-8}" libvkd3d-shader.la
	echo "libvkd3d-shader: $dir/.libs/libvkd3d-shader.so"
}

configure() {
	# HANTEI_LINUX turns on cmake/linux.cmake (the target split + test labels).
	# CMAKE_POSITION_INDEPENDENT_CODE: DXVK's SDL3 WSI dlopen()s libSDL3.so.0, so
	# cmake/linux.cmake wraps the static SDL3 archive into a shared object. A
	# static archive built without -fPIC cannot be linked that way, and the archive
	# itself is constructed by the SDL subproject, so PIC is switched on for the
	# whole native tree (harmless for the executables).
	cmake -S . -B "$BUILD" -G Ninja \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_POSITION_INDEPENDENT_CODE=ON \
		-DHANTEI_LINUX=ON \
		"$@"
}

build() {
	# ninja with no target builds plain `all`, which still reaches an excluded target
	# through its own directory's ALL, so build the native set by name (the list
	# cmake/linux.cmake writes at configure time).
	if [ "$#" -gt 0 ]; then ninja -C "$BUILD" "$@"; return; fi
	if [ -f "$BUILD/native_targets.txt" ]; then
		xargs ninja -C "$BUILD" < "$BUILD/native_targets.txt"
	else
		ninja -C "$BUILD"
	fi
}

test_native() {
	# -L native is the label cmake/linux.cmake puts on the tests whose executable
	# is in the native set.
	ctest --test-dir "$BUILD" -L native --output-on-failure
}

case "$step" in
	configure) configure ;;
	build)     build ;;
	test)      test_native ;;
	vkd3d)     vkd3d_build ;;
	clean)     rm -rf "$BUILD" ;;
	all)       vkd3d_build && configure && build && test_native ;;
	*) echo "usage: $0 [all|configure|build|test|clean]"; exit 2 ;;
esac

