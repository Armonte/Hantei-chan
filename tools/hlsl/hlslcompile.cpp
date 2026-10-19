// hlslcompile.exe: compile every res/shaders/*.hlsl entry point with
// d3dcompiler_47.dll (loaded dynamically) and report the result, the
// disassembled instruction count and the CTAB constant table.
//   usage: hlslcompile <file.hlsl | dir> [more ...] [--out report.tsv]
//
// The CTAB layout is the documented D3DXSHADER_CONSTANTTABLE (D3D9 docs); the
// mingw headers do not ship it, so it is declared here. Strings are offsets
// from the start of the CTAB block, which is the DWORD after the 'CTAB' FourCC.
//
// This is the lane-HLSL compile gate (docs/plans/render_d3d9/PLAN.md 5.2):
// a file that fails here is a bug in the port, not in a game file.
#include <windows.h>
#include <d3dcompiler.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

typedef HRESULT(WINAPI* PFN_D3DCompile)(
	LPCVOID pSrcData, SIZE_T SrcDataSize, LPCSTR pSourceName,
	const D3D_SHADER_MACRO* pDefines, ID3DInclude* pInclude,
	LPCSTR pEntrypoint, LPCSTR pTarget, UINT Flags1, UINT Flags2,
	ID3DBlob** ppCode, ID3DBlob** ppErrorMsgs);

typedef HRESULT(WINAPI* PFN_D3DDisassemble)(
	LPCVOID pSrcData, SIZE_T SrcDataSize, UINT Flags, LPCSTR szComments,
	ID3DBlob** ppDisassembly);

// D3DXSHADER_CONSTANTTABLE: 7 DWORDs.
struct ConstTable {
	DWORD Size;          // 28
	DWORD Creator;
	DWORD Version;
	DWORD Constants;
	DWORD ConstantInfo;  // offset to a D3DXSHADER_CONSTANTINFO array
	DWORD Flags;
	DWORD Target;
};

// D3DXSHADER_CONSTANTINFO: 6 DWORDs.
struct ConstInfo {
	DWORD Name;          // offset to the name string
	WORD  RegisterSet;   // D3DXRS_*
	WORD  RegisterIndex;
	WORD  RegisterCount;
	WORD  Reserved;
	DWORD TypeInfo;
	DWORD DefaultValue;
};

static const char* RegisterSetName(WORD set)
{
	switch (set) {
	case 0: return "bool";
	case 1: return "int4";
	case 2: return "float4";
	case 3: return "sampler";
	default: return "?";
	}
}

// Find the CTAB comment block: the D3DXSHADER_CONSTANTTABLE starts one DWORD
// after the 'CTAB' FourCC. Returns nullptr when the blob has none.
static const ConstTable* FindCtab(const void* data, size_t size)
{
	const unsigned char* p = (const unsigned char*)data;
	for (size_t i = 0; i + 4 + sizeof(ConstTable) <= size; i += 4) {
		if (p[i] != 'C' || p[i + 1] != 'T' || p[i + 2] != 'A' || p[i + 3] != 'B') continue;
		const ConstTable* ct = (const ConstTable*)(p + i + 4);
		if (ct->Size != sizeof(ConstTable)) continue;
		if (ct->Constants > 4096) continue;
		if ((size_t)ct->ConstantInfo + (size_t)ct->Constants * sizeof(ConstInfo) > size) continue;
		return ct;
	}
	return nullptr;
}

struct Constant {
	std::string name;
	std::string regSet;
	int regIndex = 0;
	int regCount = 0;
};

static std::vector<Constant> ReadConstants(const void* data, size_t size)
{
	std::vector<Constant> out;
	const ConstTable* ct = FindCtab(data, size);
	if (!ct) return out;
	const unsigned char* base = (const unsigned char*)ct;
	const ConstInfo* info = (const ConstInfo*)(base + ct->ConstantInfo);
	for (DWORD i = 0; i < ct->Constants; i++) {
		if ((size_t)info[i].Name >= size) continue;
		Constant c;
		c.name = (const char*)(base + info[i].Name);
		c.regSet = RegisterSetName(info[i].RegisterSet);
		c.regIndex = info[i].RegisterIndex;
		c.regCount = info[i].RegisterCount;
		out.push_back(c);
	}
	return out;
}

// Instruction count from the disassembly: one line per instruction, indented
// two spaces; "# N instruction slots used" is the compiler's own count when
// present. Prefer the comment, fall back to counting instruction lines.
static int InstructionCount(PFN_D3DDisassemble disassemble, ID3DBlob* code)
{
	if (!disassemble) return -1;
	ID3DBlob* text = nullptr;
	if (FAILED(disassemble(code->GetBufferPointer(), code->GetBufferSize(), 0, nullptr, &text)) || !text) return -1;
	std::string s((const char*)text->GetBufferPointer(), text->GetBufferSize());
	text->Release();
	std::regex slots("\\/\\/ ([0-9]+) instruction slots used");
	std::smatch m;
	if (std::regex_search(s, m, slots)) return std::atoi(m[1].str().c_str());
	int n = 0;
	std::istringstream in(s);
	std::string line;
	while (std::getline(in, line)) {
		if (line.size() < 3 || line[0] != ' ' || line[1] != ' ') continue;
		if (line[2] == '/' || line[2] == '{' || line[2] == '}') continue;
		n++;
	}
	return n;
}

// Every "<type> VSMain(" / "<type> PSMain(...)" definition in the source.
static std::set<std::string> EntryPoints(const std::string& src)
{
	std::set<std::string> names;
	std::regex re("(?:^|\\n)[ \\t]*[A-Za-z_][A-Za-z0-9_:<>]*[ \\t]+((?:VS|PS)[A-Za-z0-9_]*)[ \\t]*\\(");
	for (std::sregex_iterator it(src.begin(), src.end(), re), end; it != end; ++it)
		names.insert((*it)[1].str());
	return names;
}

static std::string ReadFile(const fs::path& p)
{
	std::ifstream f(p, std::ios::binary);
	std::ostringstream ss;
	ss << f.rdbuf();
	return ss.str();
}

int main(int argc, char** argv)
{
	HMODULE lib = LoadLibraryA("d3dcompiler_47.dll");
	if (!lib) {
		std::fprintf(stderr, "hlslcompile: cannot load d3dcompiler_47.dll (error %lu)\n", GetLastError());
		return 2;
	}
	PFN_D3DCompile compile = (PFN_D3DCompile)GetProcAddress(lib, "D3DCompile");
	PFN_D3DDisassemble disassemble = (PFN_D3DDisassemble)GetProcAddress(lib, "D3DDisassemble");
	if (!compile) {
		std::fprintf(stderr, "hlslcompile: d3dcompiler_47.dll has no D3DCompile\n");
		return 2;
	}

	std::vector<fs::path> files;
	fs::path outPath;
	for (int i = 1; i < argc; i++) {
		std::string a = argv[i];
		if (a == "--out" && i + 1 < argc) outPath = argv[++i];
		else files.push_back(a);
	}
	// Expand directories to their *.hlsl files.
	std::vector<fs::path> list;
	for (const fs::path& f : files) {
		std::error_code ec;
		if (fs::is_directory(f, ec)) {
			std::vector<fs::path> found;
			for (const auto& e : fs::directory_iterator(f, ec))
				if (e.path().extension() == ".hlsl") found.push_back(e.path());
			std::sort(found.begin(), found.end());
			list.insert(list.end(), found.begin(), found.end());
		} else {
			list.push_back(f);
		}
	}
	if (list.empty()) {
		std::fprintf(stderr, "hlslcompile: no .hlsl files given\n");
		return 2;
	}

	std::ofstream out;
	if (!outPath.empty()) out.open(outPath, std::ios::binary);

	int failures = 0, shaders = 0;
	auto emit = [&](const std::string& line) {
		std::fputs(line.c_str(), stdout);
		if (out.is_open()) out << line;
	};
	emit("file\tentry\ttarget\tresult\tinstructions\tconstants\terror\n");
	for (const fs::path& file : list) {
		const std::string src = ReadFile(file);
		const std::string name = file.filename().string();
		std::set<std::string> entries = EntryPoints(src);
		if (entries.empty()) {
			char buf[1024];
			std::snprintf(buf, sizeof(buf), "%s\t-\t-\tNO_ENTRY\t-\t-\tno VSMain/PSMain style entry point found\n", name.c_str());
			emit(buf);
			failures++;
			continue;
		}
		for (const std::string& entry : entries) {
			const char* target = entry.rfind("VS", 0) == 0 ? "vs_3_0" : "ps_3_0";
			ID3DBlob* code = nullptr;
			ID3DBlob* err = nullptr;
			HRESULT hr = compile(src.data(), src.size(), name.c_str(), nullptr, nullptr,
				entry.c_str(), target, D3DCOMPILE_OPTIMIZATION_LEVEL1, 0, &code, &err);
			shaders++;
			std::string error;
			if (err) {
				error.assign((const char*)err->GetBufferPointer(), err->GetBufferSize());
				for (char& c : error) if (c == '\n' || c == '\r' || c == '\t') c = ' ';
			}
			char buf[4096];
			if (SUCCEEDED(hr) && code) {
				std::vector<Constant> constants = ReadConstants(code->GetBufferPointer(), code->GetBufferSize());
				std::snprintf(buf, sizeof(buf), "%s\t%s\t%s\tOK\t%d\t%d\t\n",
					name.c_str(), entry.c_str(), target,
					InstructionCount(disassemble, code), (int)constants.size());
				emit(buf);
				// One line per constant: name, register set, register, count (PLAN.md 5.2).
				for (const Constant& c : constants) {
					std::snprintf(buf, sizeof(buf), "%s\t%s\t%s\tCONST\t%s\t%s\t%d\t%d\n",
						name.c_str(), entry.c_str(), target, c.name.c_str(),
						c.regSet.c_str(), c.regIndex, c.regCount);
					emit(buf);
				}
			} else {
				std::snprintf(buf, sizeof(buf), "%s\t%s\t%s\tFAIL\t-\t-\t%s\n",
					name.c_str(), entry.c_str(), target, error.c_str());
				emit(buf);
				failures++;
			}
			if (code) code->Release();
			if (err) err->Release();
		}
	}
	std::fprintf(stdout, "\n%d shader(s), %d failure(s)\n", shaders, failures);
	if (out.is_open()) out << "# " << shaders << " shader(s), " << failures << " failure(s)\n";
	return failures ? 1 : 0;
}

