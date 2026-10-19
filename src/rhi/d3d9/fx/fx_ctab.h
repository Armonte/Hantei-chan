#ifndef RHI_D3D9_FX_CTAB_H_GUARD
#define RHI_D3D9_FX_CTAB_H_GUARD
// Reader for the constant table D3DCompile embeds in a vs_3_0/ps_3_0/ps_2_0
// bytecode blob: the CTAB comment. PLAN.md P3 step 2 asks for it parsed here by
// hand (the documented D3DXSHADER_CONSTANTTABLE layout), not through D3DX.
//
// Pure C++: no D3D, no windows.h, so the layout is unit-tested without a
// compiler DLL. tools/fx/fxcompile.cpp feeds it the bytecode D3DCompile returns.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fx {

enum RegisterSet {
	kRegBool = 0,
	kRegInt4 = 1,
	kRegFloat4 = 2,
	kRegSampler = 3,
};

struct ConstantEntry {
	std::string name;
	uint16_t registerSet = 0;   // RegisterSet
	uint16_t registerIndex = 0;
	uint16_t registerCount = 0;
	// D3DXSHADER_TYPEINFO, kept for the register mapping work in P3 step 3.
	uint16_t typeClass = 0;
	uint16_t type = 0;
	uint16_t rows = 0;
	uint16_t columns = 0;
	uint16_t elements = 0;
};

struct ConstantTable {
	std::string creator;        // e.g. "Microsoft (R) HLSL Shader Compiler 9..."
	std::string target;         // e.g. "ps_3_0"
	uint32_t version = 0;
	uint32_t flags = 0;
	std::vector<ConstantEntry> constants;
};

// Extracts the CTAB comment from a compiled bytecode blob. Returns false with
// err when the blob has no well-formed CTAB (a broken emission, not a game
// file) or the offsets run off the end.
bool ParseConstantTable(const void* blob, size_t size, ConstantTable& out, std::string& err);

const char* RegisterSetName(uint16_t set);

}  // namespace fx
#endif
