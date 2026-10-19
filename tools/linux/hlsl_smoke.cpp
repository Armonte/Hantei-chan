// vkd3d-shader smoke test (lane LINUX deliverable 5, PLAN.md P5 step 3).
//
// Compiles a small vs_3_0/ps_3_0 HLSL pair to d3dbc with vkd3d_shader_compile
// (VKD3D_SHADER_SOURCE_HLSL -> VKD3D_SHADER_TARGET_D3D_BYTECODE), then walks
// res/shaders/*.hlsl from lane HLSL on origin/render/lane-hlsl, when that branch
// has been fetched, and lists per-file OK or FAIL. This is the Linux side of the
// runtime CompileHlsl() seam the RHI will use on both platforms.
//
// Usage: hlsl_smoke [shader-dir]
// Exit 0 when the built-in pair and every shader found in shader-dir compiles.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <vkd3d_shader.h>

namespace fs = std::filesystem;

namespace {

// The two entry points vkd3d-shader knows for the d3dbc source profile. The
// profile string goes in the compile info, not the API.
const char* const kVsSource =
	"struct VS_INPUT { float4 pos : POSITION; float2 uv : TEXCOORD0; };\n"
	"struct VS_OUTPUT { float4 pos : POSITION; float2 uv : TEXCOORD0; };\n"
	"float4x4 g_mvp;\n"
	"VS_OUTPUT VSMain(VS_INPUT input)\n"
	"{\n"
	"    VS_OUTPUT o;\n"
	"    o.pos = mul(input.pos, g_mvp);\n"
	"    o.uv = input.uv;\n"
	"    return o;\n"
	"}\n";

const char* const kPsSource =
	"sampler2D g_tex : register(s0);\n"
	"float4 PSMain(float2 uv : TEXCOORD0) : COLOR\n"
	"{\n"
	"    return tex2D(g_tex, uv);\n"
	"}\n";

// Returns the profile string vkd3d wants for the d3dbc target: the "vs_3_0" /
// "ps_3_0" the games and the HLSL files use. vkd3d derives the target version
// from the profile option.
std::string ProfileFor(bool vertex) { return vertex ? "vs_3_0" : "ps_3_0"; }

int CompileOne(const char* source, const char* entryPoint, const char* profile, std::string* error)
{
	// The entry point and the profile travel in a vkd3d_shader_hlsl_source_info
	// chained off compile_info.next: that is where the HLSL front end reads them.
	// profile is the same "vs_3_0"/"ps_3_0" string the games pass to D3DX.
	struct vkd3d_shader_hlsl_source_info hlsl;
	std::memset(&hlsl, 0, sizeof hlsl);
	hlsl.type = VKD3D_SHADER_STRUCTURE_TYPE_HLSL_SOURCE_INFO;
	hlsl.entry_point = entryPoint;
	hlsl.profile = profile;

	struct vkd3d_shader_compile_info info;
	std::memset(&info, 0, sizeof info);
	info.type = VKD3D_SHADER_STRUCTURE_TYPE_COMPILE_INFO;
	info.next = &hlsl;
	info.source.code = source;
	info.source.size = std::strlen(source);
	info.source_type = VKD3D_SHADER_SOURCE_HLSL;
	info.target_type = VKD3D_SHADER_TARGET_D3D_BYTECODE;

	struct vkd3d_shader_code out;
	std::memset(&out, 0, sizeof out);
	char* messages = nullptr;
	const int rc = vkd3d_shader_compile(&info, &out, &messages);
	if (rc < 0) {
		*error = messages ? messages : "vkd3d_shader_compile failed with no message";
		if (messages) vkd3d_shader_free_messages(messages);
		return 1;
	}
	// The d3dbc container starts with the version token; report its shape so a
	// silent no-op cannot pass as success.
	if (out.size < 4) {
		vkd3d_shader_free_shader_code(&out);
		*error = "vkd3d returned an empty shader";
		return 1;
	}
	const uint32_t version = *(const uint32_t*)out.code;
	// The d3dbc version token is 0xfffeXXXX for a vertex shader and 0xffffXXXX
	// for a pixel shader (0x0300 is shader model 3.0).
	const uint32_t kind = version & 0xffff0000u;
	if (kind != 0xfffe0000u && kind != 0xffff0000u) {
		vkd3d_shader_free_shader_code(&out);
		char buf[96];
		std::snprintf(buf, sizeof buf, "unexpected d3dbc version token 0x%08x", version);
		*error = buf;
		return 1;
	}
	if (messages) vkd3d_shader_free_messages(messages);
	vkd3d_shader_free_shader_code(&out);
	return 0;
}

// One file may hold both stages (res/shaders/<name>.hlsl) or declare its entry
// points; try vs_3_0 first, then ps_3_0, and count the file as ok when either
// stage compiles. The per-file target is printed so a wrong guess is visible.
int CompileFile(const fs::path& path)
{
	std::ifstream f(path, std::ios::binary);
	if (!f) {
		std::printf("hlsl_smoke: %s FAIL (cannot read)\n", path.string().c_str());
		return 1;
	}
	const std::string src((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	std::string error;
	const bool hasVs = src.find("VSMain") != std::string::npos;
	const bool hasPs = src.find("PSMain") != std::string::npos;
	if (!hasVs && !hasPs) {
		std::printf("hlsl_smoke: %s SKIP (no VSMain/PSMain)\n", path.string().c_str());
		return 0;
	}
	int failures = 0;
	if (hasVs) {
		const int rc = CompileOne(src.c_str(), "VSMain", "vs_3_0", &error);
		std::printf("hlsl_smoke: %s vs_3_0 %s%s%s\n", path.string().c_str(), rc ? "FAIL" : "OK",
		            rc ? ": " : "", rc ? error.c_str() : "");
		failures += rc;
	}
	if (hasPs) {
		const int rc = CompileOne(src.c_str(), "PSMain", "ps_3_0", &error);
		std::printf("hlsl_smoke: %s ps_3_0 %s%s%s\n", path.string().c_str(), rc ? "FAIL" : "OK",
		            rc ? ": " : "", rc ? error.c_str() : "");
		failures += rc;
	}
	return failures;
}

}  // namespace

int main(int argc, char** argv)
{
	std::string error;
	int failures = 0;

	// 1. the built-in pair: proves vkd3d_shader_compile works at all.
	failures += CompileOne(kVsSource, "VSMain", "vs_3_0", &error);
	std::printf("hlsl_smoke: builtin vs_3_0 %s%s%s\n", failures ? "FAIL" : "OK", failures ? ": " : "",
	            failures ? error.c_str() : "");
	const int psFail = CompileOne(kPsSource, "PSMain", "ps_3_0", &error);
	std::printf("hlsl_smoke: builtin ps_3_0 %s%s%s\n", psFail ? "FAIL" : "OK", psFail ? ": " : "",
	            psFail ? error.c_str() : "");
	failures += psFail;

	// 2. the lane HLSL files, when the branch is present. The default path is the
	// worktree-relative res/shaders; the argument overrides it for the test.
	const fs::path dir = argc > 1 ? fs::path(argv[1]) : fs::path("res/shaders");
	std::error_code ec;
	if (!fs::is_directory(dir, ec)) {
		std::printf("hlsl_smoke: %s is not a directory, skipping the res/shaders walk\n", dir.string().c_str());
	} else {
		std::vector<fs::path> files;
		for (const fs::directory_entry& e : fs::directory_iterator(dir, ec))
			if (e.path().extension() == ".hlsl") files.push_back(e.path());
		std::sort(files.begin(), files.end());
		if (files.empty())
			std::printf("hlsl_smoke: no .hlsl files in %s\n", dir.string().c_str());
		for (const fs::path& p : files) failures += CompileFile(p);
	}

	if (failures) {
		std::fprintf(stderr, "hlsl_smoke: %d failure(s)\n", failures);
		return 1;
	}
	std::printf("hlsl_smoke: ok\n");
	return 0;
}

