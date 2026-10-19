#!/bin/bash
# Native Linux build of the non-GUI targets (lane LINUX deliverable 3, PLAN.md
# P5 step 1). CMake + Ninja + gcc into build-linux/; no mingw, no GPU.
#
#   ./build_linux.sh              configure + build the native targets, then ctest
#   ./build_linux.sh configure    configure only
#   ./build_linux.sh build        build only (uses an existing build-linux/)
#   ./build_linux.sh test         run the native ctests
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
	clean)     rm -rf "$BUILD" ;;
	all)       configure && build && test_native ;;
	*) echo "usage: $0 [all|configure|build|test|clean]"; exit 2 ;;
esac

