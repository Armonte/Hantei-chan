SET(imgui_PATH "${PROJECT_SOURCE_DIR}/imgui")

if(NOT EXISTS "${imgui_PATH}/imgui.h")
	message(FATAL_ERROR "Imgui not found. Did you forget to init the submodules?")
endif()

file(GLOB imgui_CPP
	"${imgui_PATH}/*.cpp"
)

add_library(imgui
	"${imgui_PATH}/backends/imgui_impl_opengl3.cpp"
	"${imgui_PATH}/backends/imgui_impl_win32.cpp"
	"${imgui_PATH}/misc/cpp/imgui_stdlib.cpp"
	${imgui_CPP}
)

target_include_directories(imgui PUBLIC
	"${imgui_PATH}"
	"${imgui_PATH}/backends"
	"${imgui_PATH}/misc/cpp"
	config
)


target_compile_definitions(imgui PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD IMGUI_IMPL_OPENGL_ES2 IMGUI_USER_CONFIG=<config.h> )
# 32-bit draw indices for every translation unit that sees imgui.h: the backend is built on its ES2 path (no
# glDrawElementsBaseVertex, so ImGuiBackendFlags_RendererHasVtxOffset stays off) and a 16-bit index wraps once a
# single draw list passes 65535 vertices (a large Squirrel file in the Script panel went blank).
# C/C++ only: windres (the .rc in every target that links imgui) gets the definitions too, and splits "unsigned int"
# at the space, failing with "int: No such file or directory" on the native Windows build.
target_compile_definitions(imgui PUBLIC "$<$<COMPILE_LANGUAGE:C,CXX>:ImDrawIdx=unsigned int>")
target_link_libraries(imgui PUBLIC glad imm32 dwmapi)