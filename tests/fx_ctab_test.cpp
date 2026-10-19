// fx_ctab: unit test for the CTAB constant-table reader (fx::ParseConstantTable)
// on a bytecode blob built here, since the layout has to be right before
// fx_compile trusts it with D3DCompile's real output. PLAN.md P3 step 2.
#include "rhi/d3d9/fx/fx_ctab.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void Put32(std::vector<uint8_t>& b, uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back(uint8_t(v >> (8 * i))); }
static void Put16(std::vector<uint8_t>& b, uint16_t v) { b.push_back(uint8_t(v & 0xFF)); b.push_back(uint8_t(v >> 8)); }

// A vs_3_0 blob: version token, one comment instruction, then the CTAB payload.
// CTAB: 1 constant (g_mWorld, float4 set, register 0, 4 registers) and a target.
static std::vector<uint8_t> Synthetic()
{
	std::vector<uint8_t> body;
	Put32(body, 0x42415443u);   // CTAB fourcc
	const uint32_t headerAt = body.size();
	Put32(body, 0);             // Size of the header
	Put32(body, 0);             // Creator (offset, filled below)
	Put32(body, 0xFFFE0300u);   // Version (vs_3_0)
	Put32(body, 1);             // Constants
	Put32(body, 0);             // ConstantInfo (offset, filled below)
	Put32(body, 0);             // Flags
	Put32(body, 0);             // Target (offset, filled below)

	const uint32_t creatorOff = (uint32_t)body.size();
	body.insert(body.end(), {'M', 'i', 'c', 'r', 'o', 's', 'o', 'f', 't', 0});

	// TypeInfo for the constant: class=VECTOR(1), type=FLOAT(1), rows=1, cols=4.
	const uint32_t typeOff = (uint32_t)body.size();
	Put16(body, 1);
	Put16(body, 1);
	Put16(body, 1);
	Put16(body, 4);
	Put16(body, 0);
	Put16(body, 0);
	Put32(body, 0);

	const uint32_t nameOff = (uint32_t)body.size();
	body.insert(body.end(), {'g', '_', 'm', 'W', 'o', 'r', 'l', 'd', 0});
	while (body.size() % 4) body.push_back(0);

	const uint32_t targetOff = (uint32_t)body.size();
	body.insert(body.end(), {'v', 's', '_', '3', '_', '0', 0});

	const uint32_t constInfoOff = (uint32_t)body.size();
	Put32(body, nameOff - 4);
	Put16(body, 2);       // RegisterSet FLOAT4
	Put16(body, 0);       // RegisterIndex
	Put16(body, 4);       // RegisterCount
	Put16(body, 0);       // Reserved
	Put32(body, typeOff - 4);
	Put32(body, 0);

	// D3DXSHADER_CONSTANTTABLE fields, relative to the struct (4 bytes past the
	// CTAB fourcc):
	// +0 Size, +4 Creator, +8 Version, +12 Constants, +16 ConstantInfo, +20 Flags, +24 Target.
	auto putAt = [&](uint32_t at, uint32_t v) {
		body[at + 0] = v & 0xFF; body[at + 1] = (v >> 8) & 0xFF;
		body[at + 2] = (v >> 16) & 0xFF; body[at + 3] = (v >> 24) & 0xFF;
	};
	putAt(headerAt + 0, 28);
	putAt(headerAt + 4, creatorOff - 4);
	putAt(headerAt + 16, constInfoOff - 4);
	putAt(headerAt + 24, targetOff - 4);

	// The full blob: version token, a comment instruction around the CTAB, end token.
	// A D3DSIO_COMMENT token carries its length in DWORDs in bits 16-30; the
	// body (starting with the CTAB fourcc) follows the token immediately.
	const size_t bodyDw = (body.size() + 3) / 4;
	std::vector<uint8_t> blob;
	Put32(blob, 0xFFFE0300u);
	Put32(blob, 0x0000FFFEu | ((uint32_t)bodyDw << 16));
	blob.insert(blob.end(), body.begin(), body.end());
	while (blob.size() % 4) blob.push_back(0);
	Put32(blob, 0x0000FFFFu);            // D3DSIO_END
	return blob;
}

int main()
{
	const std::vector<uint8_t> blob = Synthetic();
	fx::ConstantTable ct;
	std::string err;
	CHECK(fx::ParseConstantTable(blob.data(), blob.size(), ct, err));
	if (!err.empty()) std::fprintf(stderr, "  err: %s\n", err.c_str());
	CHECK(ct.creator == "Microsoft");
	CHECK(ct.target == "vs_3_0");
	CHECK(ct.constants.size() == 1);
	if (ct.constants.size() == 1) {
		CHECK(ct.constants[0].name == "g_mWorld");
		CHECK(ct.constants[0].registerSet == fx::kRegFloat4);
		CHECK(ct.constants[0].registerIndex == 0);
		CHECK(ct.constants[0].registerCount == 4);
		CHECK(ct.constants[0].columns == 4 && ct.constants[0].rows == 1);
	}
	CHECK(std::string(fx::RegisterSetName(fx::kRegSampler)) == "sampler");

	// A blob with no CTAB is a hard error, not a silent empty table.
	std::vector<uint8_t> noCtab;
	Put32(noCtab, 0xFFFE0300u);
	Put32(noCtab, 0x0000FFFFu);
	fx::ConstantTable ct2;
	std::string err2;
	CHECK(!fx::ParseConstantTable(noCtab.data(), noCtab.size(), ct2, err2));
	CHECK(!err2.empty());
	CHECK(fx::ParseConstantTable(nullptr, 0, ct2, err2) == false);

	if (g_fail) {
		std::fprintf(stderr, "fx_ctab: FAIL (%d)\n", g_fail);
		return 1;
	}
	std::printf("fx_ctab: pass\n");
	return 0;
}
