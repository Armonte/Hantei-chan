# SDL3 (deps/SDL, submodule pinned at release-3.4.18; PLAN.md 9). Built static
# from the submodule so the Windows exe and the P5 native Linux build use the
# same source without a system SDL.
if(NOT EXISTS "${PROJECT_SOURCE_DIR}/deps/SDL/CMakeLists.txt")
	message(FATAL_ERROR "SDL3 not found. Did you forget to init the submodules? (git submodule update --init deps/SDL)")
endif()

set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
# Direct3D join-statement support is not shipped by mingw one-way builds; the
# DirectX render driver is only needed for the P5 DXVK path, not for our own
# D3D9 device (which we create ourselves from the window handle).
add_subdirectory("${PROJECT_SOURCE_DIR}/deps/SDL" "${CMAKE_BINARY_DIR}/SDL" EXCLUDE_FROM_ALL)
