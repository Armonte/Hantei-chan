SET(imgui_PATH "${PROJECT_SOURCE_DIR}/imgui")

if(NOT EXISTS "${imgui_PATH}/imgui.h")
	message(FATAL_ERROR "Imgui not found. Did you forget to init the submodules?")
endif()

file(GLOB imgui_CPP
	"${imgui_PATH}/*.cpp"
)

add_library(imgui
	"${imgui_PATH}/backends/imgui_impl_opengl3.cpp"
	"${imgui_PATH}/backends/imgui_impl_sdl3.cpp"
	"${imgui_PATH}/misc/cpp/imgui_stdlib.cpp"
	${imgui_CPP}
)

target_include_directories(imgui PUBLIC
	"${imgui_PATH}"
	"${imgui_PATH}/backends"
	"${imgui_PATH}/misc/cpp"
	config
)


# No IMGUI_IMPL_OPENGL_ES2: dropping it enables glDrawElementsBaseVertex, which
# is what removes the 32-bit ImDrawIdx workaround (PLAN.md 6 P0 step 4).
target_compile_definitions(imgui PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD IMGUI_USER_CONFIG=<config.h> )
target_link_libraries(imgui PUBLIC glad SDL3-static imm32 dwmapi)
