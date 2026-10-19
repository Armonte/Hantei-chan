// fx_inventory: parses every game effect file (Shader/*.txt) and lists every
// technique / pass / shader / render state. PLAN.md P3 step 1.
//
// Writes tools/render_parity/fx_inventory.tsv: game, file, technique, pass, vs,
// ps, states, status. Names only, never shader text (the games' HLSL is
// copyrighted and stays on disk). The gate is 100% OK: any PARSE_FAIL is a
// parser bug, since D3DX loads every one of these files.
//
// Exits 77 (ctest skip) when none of the three game installs is present. The
// synthetic checks at the top always run, data or not, so a regression is
// caught in CI.
#include "rhi/d3d9/fx/fx_parse.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static int g_fail = 0;

#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

// The test is a Windows exe (WSL interop), so the install is a C: path there
// and a /mnt/c path in a native Linux build. Try both.
struct GameRoot { const char* game; const char* win; const char* wsl; };
static const GameRoot kGames[] = {
	{"uni2", "C:/dev/hantei-chan/gamedata/uni2/Shader", "/mnt/c/dev/hantei-chan/gamedata/uni2/Shader"},
	{"mbtl", "C:/dev/hantei-chan/gamedata/mbtl/Shader", "/mnt/c/dev/hantei-chan/gamedata/mbtl/Shader"},
	{"dfci", "C:/dev/transmission/dfci+transmission/Shader", "/mnt/c/dev/transmission/dfci+transmission/Shader"},
};

static fs::path ExistingDir(const GameRoot& g)
{
	std::error_code ec;
	if (fs::is_directory(g.win, ec)) return g.win;
	if (fs::is_directory(g.wsl, ec)) return g.wsl;
	return fs::path();
}

static std::string ReadFile(const fs::path& p)
{
	std::ifstream f(p, std::ios::binary);
	return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

// One file, comment-free UTF-8 text: decode CP932, then parse and walk.
static std::string StatesOf(const fx::Pass& p)
{
	std::string o;
	for (const fx::StateAssign& s : p.states) {
		if (!o.empty()) o += ';';
		o += s.name + "=" + s.value;
	}
	return o.empty() ? std::string("-") : o;
}

static std::string ShaderCol(const fx::ShaderRef& r)
{
	return r.present ? (r.profile + ":" + r.entry) : std::string("-");
}

static void Synthetic()
{
	// A file that exercises every construct the three games use: defines,
	// structs, both sampler spellings, annotation blocks, arrays with defaults,
	// a prototype, a function, and a two-pass technique with states.
	const char* src =
		"#define W (2)\n"
		"struct VS_IN { float4 Position : POSITION0; float4 Diffuse : COLOR0; };\n"
		"texture tex_Fade = NULL;\n"
		"sampler2D TexSample_Fade = sampler_state { texture = <tex_Fade>;\n"
		"    AddressU = CLAMP; MagFilter = Point; };\n"
		"sampler DiffuseTextureSampler = sampler_state { Texture = <tex_Fade>; };\n"
		"float4 g_mWorld < string UIName = \"World\"; >;\n"
		"float g_fTable[5] = { 1.0f, 0.4f, 0.3f, 0.2f, 0.1f };\n"
		"float fPalPlusV;\n"
		"int alpha_type = 0;\n"
		"float GetAngPosUV( float2 uv );\n"
		"float4 PS_0( VS_IN In ) : COLOR0 {\n"
		"    return tex2D( TexSample_Fade, In.Diffuse.xy );\n"
		"}\n"
		"float4 PS_1( VS_IN In ) : COLOR0 { return 1.0f; }\n"
		"technique TecNormal {\n"
		"    pass P0 { PixelShader = compile ps_3_0 PS_0(); AlphaBlendEnable = True; ZEnable = False; }\n"
		"    pass P1 { PixelShader = compile ps_3_0 PS_1(); }\n"
		"}\n";

	fx::Effect e;
	std::string err;
	CHECK(fx::ParseEffect(src, e, err));
	if (!err.empty()) std::fprintf(stderr, "  err: %s\n", err.c_str());
	CHECK(e.uniforms.size() == 4);   // g_mWorld, g_fTable, fPalPlusV, alpha_type
	CHECK(e.textures.size() == 1);
	CHECK(e.samplers.size() == 2);
	CHECK(e.functions.size() == 2);   // PS_0, PS_1 (a prototype is kept as Raw text)
	CHECK(e.techniques.size() == 1);
	if (e.techniques.size() == 1) {
		const fx::Technique& t = e.techniques[0];
		CHECK(t.name == "TecNormal");
		CHECK(t.passes.size() == 2);
		if (t.passes.size() == 2) {
			CHECK(t.passes[0].name == "P0");
			CHECK(t.passes[0].ps.present && t.passes[0].ps.profile == "ps_3_0" && t.passes[0].ps.entry == "PS_0");
			CHECK(!t.passes[0].vs.present);
			CHECK(t.passes[0].states.size() == 2);
			CHECK(t.passes[0].states[0].name == "AlphaBlendEnable" && t.passes[0].states[0].value == "True");
		}
	}
	if (e.samplers.size() == 2) {
		CHECK(e.samplers[0].name == "TexSample_Fade" && e.samplers[0].texture == "tex_Fade");
		CHECK(e.samplers[0].hasState);
		CHECK(e.samplers[1].keyword == "sampler" && e.samplers[1].texture == "tex_Fade");
	}
	bool foundArray = false, foundAnnotation = false;
	for (const fx::Uniform& u : e.uniforms) {
		if (u.name == "g_fTable") { foundArray = u.arraySize == "5" && !u.defaultValue.empty(); }
		if (u.name == "g_mWorld") { foundAnnotation = u.annotations.size() == 1 && u.annotations[0].name == "UIName" && u.annotations[0].value == "\"World\""; }
	}
	CHECK(foundArray);
	CHECK(foundAnnotation);

	// MakePassHlsl: samplers become register(sN) declarations, techniques drop out.
	const fx::Technique* t = fx::FindTechnique(e, "TecNormal");
	if (t) {
		std::string hlsl = fx::MakePassHlsl(e, *t, t->passes[0]);
		CHECK(hlsl.find("sampler2D TexSample_Fade : register(s0);") != std::string::npos);
		CHECK(hlsl.find("sampler2D DiffuseTextureSampler : register(s1);") != std::string::npos);
		CHECK(hlsl.rfind("technique", 0) == std::string::npos);
		CHECK(hlsl.find("float g_fTable[5] = { 1.0f, 0.4f, 0.3f, 0.2f, 0.1f };") != std::string::npos);
		CHECK(hlsl.find("#define W (2)") != std::string::npos);
	}

	// A CP932 round trip: a Japanese comment and a 0x5C trail byte.
	const char sjis[] = "// \x83\x5C\x83\x58\x83\x67\r\nfloat x;";
	CHECK(fx::Cp932ToUtf8(sjis) == "// \xE3\x82\xBD\xE3\x82\xB9\xE3\x83\x88\r\nfloat x;");
}

int main()
{
	Synthetic();

	std::vector<std::string> rows;
	bool anyPresent = false;
	int files = 0, techniques = 0, passes = 0, fails = 0;
	for (const GameRoot& g : kGames) {
		const std::string game = g.game;
		const fs::path dir = ExistingDir(g);
		if (dir.empty()) {
			std::fprintf(stderr, "fx_inventory: %s missing\n", g.win);
			continue;
		}
		anyPresent = true;
		std::vector<fs::path> filesHere;
		for (const auto& e : fs::directory_iterator(dir))
			if (e.is_regular_file() && e.path().extension() == ".txt") filesHere.push_back(e.path());
		std::sort(filesHere.begin(), filesHere.end());
		for (const fs::path& p : filesHere) {
			++files;
			const std::string utf8 = fx::Cp932ToUtf8(ReadFile(p));
			fx::Effect e;
			std::string err;
			if (!fx::ParseEffect(utf8, e, err)) {
				++fails;
				std::fprintf(stderr, "PARSE_FAIL %s: %s\n", p.filename().u8string().c_str(), err.c_str());
				rows.push_back(game + "\t" + p.filename().u8string() + "\t-\t-\t-\t-\t-\tPARSE_FAIL " + err);
				continue;
			}
			if (e.techniques.empty()) {
				++fails;
				std::fprintf(stderr, "PARSE_FAIL %s: no technique\n", p.filename().u8string().c_str());
				rows.push_back(game + "\t" + p.filename().u8string() + "\t-\t-\t-\t-\t-\tPARSE_FAIL no technique");
				continue;
			}
			for (const fx::Technique& t : e.techniques) {
				++techniques;
				for (const fx::Pass& pass : t.passes) {
					++passes;
					rows.push_back(game + "\t" + p.filename().u8string() + "\t" + t.name + "\t" + pass.name + "\t" +
						ShaderCol(pass.vs) + "\t" + ShaderCol(pass.ps) + "\t" + StatesOf(pass) + "\tOK");
				}
			}
		}
	}

	if (!anyPresent) {
		std::fprintf(stderr, "fx_inventory: no game install present, skipped\n");
		if (g_fail) {
			std::fprintf(stderr, "fx_inventory: %d synthetic failure(s)\n", g_fail);
			return 1;
		}
		return 77;
	}

	std::sort(rows.begin(), rows.end());
	fs::path out = fs::path("tools") / "render_parity" / "fx_inventory.tsv";
	std::error_code ec;
	fs::create_directories(out.parent_path(), ec);
	std::ofstream of(out, std::ios::binary | std::ios::trunc);
	of << "game\tfile\ttechnique\tpass\tvs\tps\tstates\tstatus\n";
	for (const std::string& r : rows) of << r << "\n";
	of.close();

	std::printf("fx_inventory: %d files, %d techniques, %d passes, %d PARSE_FAIL -> %s\n",
		files, techniques, passes, fails, out.string().c_str());
	if (g_fail || fails) {
		std::fprintf(stderr, "fx_inventory: FAIL (%d synthetic, %d parse)\n", g_fail, fails);
		return 1;
	}
	return 0;
}
