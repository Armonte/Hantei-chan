// fxcompile: compile every pass of every game effect file with D3DCompile.
// PLAN.md P3 step 2, lane FX deliverable 3.
//
// Loads d3dcompiler_47.dll dynamically (no import lib, a missing DLL is a clear
// error), compiles each pass's VS and PS from fx::MakePassHlsl, and parses the
// CTAB constant table out of the returned bytecode itself (fx::ParseConstantTable).
// The gate is 100% compile OK: a failure means our HLSL emission is wrong, not
// the game's file, because D3DX loads every one of these files.
//
// Appends compile=<OK|FAIL> and the constants to the same rows as
// fx_inventory.tsv, keyed by (game, file, technique, pass), and writes the
// result to tools/render_parity/fx_compile.tsv. Names only, never shader text.
//
// Windows-only (it needs d3dcompiler_47.dll and d3d9 types); exits 77 when none
// of the three installs is present.

#include "rhi/d3d9/fx/fx_parse.h"
#include "rhi/d3d9/fx/fx_ctab.h"

#include <windows.h>
#include <d3dcompiler.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

using D3DCompileFn = HRESULT(WINAPI*)(const void*, SIZE_T, const char*, const D3D_SHADER_MACRO*, ID3DInclude*,
	const char*, const char*, UINT, UINT, ID3DBlob**, ID3DBlob**);

static const char* kDll = "d3dcompiler_47.dll";

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

static std::string Sanitize(const std::string& s)
{
	std::string o = s;
	for (char& c : o)
		if (c == '\t' || c == '\r' || c == '\n') c = ' ';
	return o;
}

// "name(float4:r0x2x1)" for each constant, sorted by register set then index.
static std::string ConstantsOf(const fx::ConstantTable& ct)
{
	std::vector<const fx::ConstantEntry*> cs;
	for (const fx::ConstantEntry& c : ct.constants) cs.push_back(&c);
	std::sort(cs.begin(), cs.end(), [](const fx::ConstantEntry* a, const fx::ConstantEntry* b) {
		if (a->registerSet != b->registerSet) return a->registerSet < b->registerSet;
		return a->registerIndex < b->registerIndex;
	});
	std::string o;
	for (const fx::ConstantEntry* c : cs) {
		if (!o.empty()) o += ';';
		o += c->name;
		o += "{";
		o += fx::RegisterSetName(c->registerSet);
		o += ":r" + std::to_string(c->registerIndex);
		if (c->registerCount != 1) o += "x" + std::to_string(c->registerCount);
		o += "}";
	}
	return o.empty() ? std::string("-") : o;
}

struct Result {
	bool ok = false;
	std::string detail;   // the error, or the constant list
	std::string profile;
};

static Result CompileOne(D3DCompileFn compile, const std::string& hlsl, const fx::ShaderRef& ref)
{
	Result r;
	r.profile = ref.profile;
	ID3DBlob* code = nullptr;
	ID3DBlob* errs = nullptr;
	const HRESULT hr = compile(hlsl.data(), hlsl.size(), "fx.hlsl", nullptr, nullptr, ref.entry.c_str(),
		ref.profile.c_str(), D3DCOMPILE_OPTIMIZATION_LEVEL1, 0, &code, &errs);
	std::string errText;
	if (errs) {
		errText.assign((const char*)errs->GetBufferPointer(), errs->GetBufferSize());
		errs->Release();
	}
	if (FAILED(hr) || !code) {
		r.ok = false;
		r.detail = "hr=0x" + [&] { char b[16]; std::snprintf(b, sizeof b, "%08lX", (unsigned long)hr); return std::string(b); }();
		if (!errText.empty()) r.detail += " " + Sanitize(errText.substr(0, 400));
		return r;
	}
	fx::ConstantTable ct;
	std::string ctabErr;
	if (!fx::ParseConstantTable(code->GetBufferPointer(), code->GetBufferSize(), ct, ctabErr)) {
		r.ok = false;
		r.detail = "CTAB: " + ctabErr;
		code->Release();
		return r;
	}
	r.ok = true;
	r.detail = ct.target.empty() ? std::string("-") : ct.target;
	r.detail += " ";
	r.detail += ConstantsOf(ct);
	code->Release();
	return r;
}

int main()
{
	HMODULE mod = LoadLibraryA(kDll);
	if (!mod) {
		std::fprintf(stderr, "fxcompile: cannot load %s (error %lu)\n", kDll, (unsigned long)GetLastError());
		return 1;
	}
	auto compile = (D3DCompileFn)GetProcAddress(mod, "D3DCompile");
	if (!compile) {
		std::fprintf(stderr, "fxcompile: %s has no D3DCompile\n", kDll);
		return 1;
	}

	std::vector<std::string> rows;
	bool anyPresent = false;
	int total = 0, fails = 0;
	for (const GameRoot& g : kGames) {
		const fs::path dir = ExistingDir(g);
		if (dir.empty()) {
			std::fprintf(stderr, "fxcompile: %s missing\n", g.win);
			continue;
		}
		anyPresent = true;
		std::vector<fs::path> filesHere;
		for (const auto& e : fs::directory_iterator(dir))
			if (e.is_regular_file() && e.path().extension() == ".txt") filesHere.push_back(e.path());
		std::sort(filesHere.begin(), filesHere.end());
		for (const fs::path& p : filesHere) {
			fx::Effect e;
			std::string err;
			if (!fx::ParseEffect(fx::Cp932ToUtf8(ReadFile(p)), e, err)) {
				rows.push_back(std::string(g.game) + "\t" + p.filename().u8string() + "\t-\t-\t-\t-\tFAIL parse: " + Sanitize(err));
				++fails;
				continue;
			}
			for (const fx::Technique& t : e.techniques) {
				for (const fx::Pass& pass : t.passes) {
					for (int side = 0; side < 2; ++side) {
						const fx::ShaderRef& ref = side == 0 ? pass.vs : pass.ps;
						if (!ref.present) continue;
						++total;
						const Result r = CompileOne(compile, fx::MakePassHlsl(e, t, pass), ref);
						if (!r.ok) ++fails;
						rows.push_back(std::string(g.game) + "\t" + p.filename().u8string() + "\t" + t.name + "\t" +
							pass.name + "\t" + (side == 0 ? "vs" : "ps") + "\t" + ref.profile + ":" + ref.entry + "\t" +
							(r.ok ? "OK" : "FAIL") + "\t" + Sanitize(r.detail));
						if (!r.ok)
							std::fprintf(stderr, "COMPILE_FAIL %s %s %s %s.%s: %s\n", g.game,
								p.filename().u8string().c_str(), t.name.c_str(), pass.name.c_str(),
								ref.entry.c_str(), r.detail.c_str());
					}
				}
			}
		}
	}

	if (!anyPresent) {
		std::fprintf(stderr, "fxcompile: no game install present, skipped\n");
		return 77;
	}

	std::sort(rows.begin(), rows.end());
	const fs::path out = fs::path("tools") / "render_parity" / "fx_compile.tsv";
	std::error_code ec;
	fs::create_directories(out.parent_path(), ec);
	std::ofstream of(out, std::ios::binary | std::ios::trunc);
	of << "game\tfile\ttechnique\tpass\tside\tprofile\tstatus\tconstants\n";
	for (const std::string& r : rows) of << r << "\n";
	of.close();

	std::printf("fxcompile: %d shaders, %d FAIL -> %s\n", total, fails, out.string().c_str());
	if (fails) {
		std::fprintf(stderr, "fxcompile: FAIL (%d/%d)\n", fails, total);
		return 1;
	}
	return 0;
}
