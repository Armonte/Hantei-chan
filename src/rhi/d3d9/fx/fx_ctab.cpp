#include "rhi/d3d9/fx/fx_ctab.h"

#include <cstring>

namespace fx {
namespace {

// D3D9 bytecode header (from the D3D9 shader binary format): the version token
// is the first DWORD, whose low nibble is the shader type (0 = PS, 1 = VS) and
// whose high nibble is the major version. Then comes the token stream; comments
// are recorded in the CTAB block at the end.
const uint32_t kCommentToken = 0x0000FFFEu;   // D3DSIO_COMMENT
const uint32_t kEndToken = 0x0000FFFFu;       // D3DSIO_END
const uint32_t kFourCC = 0x42415443u;         // 'CTAB'

uint32_t Rd32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
uint16_t Rd16(const uint8_t* p) { return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8)); }

std::string RdStr(const uint8_t* base, size_t size, uint32_t off)
{
	if (off >= size) return std::string();
	const char* s = (const char*)base + off;
	size_t n = 0;
	while (off + n < size && s[n]) ++n;
	return std::string(s, n);
}

}  // namespace

const char* RegisterSetName(uint16_t set)
{
	switch (set) {
	case kRegBool: return "bool";
	case kRegInt4: return "int4";
	case kRegFloat4: return "float4";
	case kRegSampler: return "sampler";
	default: return "?";
	}
}

bool ParseConstantTable(const void* blob, size_t size, ConstantTable& out, std::string& err)
{
	out = ConstantTable();
	if (!blob || size < 8) {
		err = "bytecode too small";
		return false;
	}
	const uint8_t* b = (const uint8_t*)blob;
	out.version = Rd32(b);

	// Walk the token stream for the CTAB comment. A token is opcode in bits
	// 0-15; for a comment the length in DWORDs (the comment body, which starts
	// with the CTAB fourcc) is in bits 16-30 and there is no separate length
	// DWORD. Every other instruction's length is also bits 16-30, counting its
	// own operand DWORDs.
	size_t i = 4;
	bool found = false;
	size_t ctab = 0;
	while (i + 4 <= size) {
		const uint32_t tok = Rd32(b + i);
		const uint32_t opcode = tok & 0xFFFFu;
		const uint32_t opLen = (tok >> 16) & 0x7FFFu;
		if (opcode == (kEndToken & 0xFFFFu)) break;
		if (opcode == (kCommentToken & 0xFFFFu)) {
			const size_t body = i + 4;
			if (body + size_t(opLen) * 4 > size) {
				err = "comment runs past the blob";
				return false;
			}
			if (opLen >= 1 && Rd32(b + body) == kFourCC) {
				ctab = body;
				found = true;
				break;
			}
			i = body + size_t(opLen) * 4;
			continue;
		}
		i += (size_t(opLen) + 1) * 4;
	}
	if (!found) {
		err = "no CTAB comment in the bytecode";
		return false;
	}

	const uint8_t* t = b + ctab;
	// The comment payload is the 'CTAB' fourcc followed by the
	// D3DXSHADER_CONSTANTTABLE struct; its offsets are relative to the struct,
	// not to the fourcc (the D3DX docs say "from the beginning of this
	// structure").
	const size_t avail = size - (ctab + 4);
	t += 4;
	if (avail < 28) {
		err = "CTAB header truncated";
		return false;
	}
	const uint32_t headerSize = Rd32(t + 0);
	if (headerSize < 28) {
		err = "CTAB Size too small";
		return false;
	}
	const uint32_t constants = Rd32(t + 12);
	const uint32_t constantInfo = Rd32(t + 16);
	out.flags = Rd32(t + 20);
	const uint32_t targetOff = Rd32(t + 24);
	out.creator = RdStr(t, avail, Rd32(t + 4));
	out.target = RdStr(t, avail, targetOff);

	for (uint32_t k = 0; k < constants; ++k) {
		const size_t off = constantInfo + (size_t)k * 20;
		if (off + 20 > avail) {
			err = "constant info truncated";
			return false;
		}
		const uint8_t* ci = t + off;
		ConstantEntry e;
		e.name = RdStr(t, avail, Rd32(ci + 0));
		e.registerSet = Rd16(ci + 4);
		e.registerIndex = Rd16(ci + 6);
		e.registerCount = Rd16(ci + 8);
		const uint32_t typeInfo = Rd32(ci + 12);
		if ((size_t)typeInfo + 14 <= avail) {
			const uint8_t* ti = t + typeInfo;
			e.typeClass = Rd16(ti + 0);
			e.type = Rd16(ti + 2);
			e.rows = Rd16(ti + 4);
			e.columns = Rd16(ti + 6);
			e.elements = Rd16(ti + 8);
		}
		out.constants.push_back(std::move(e));
	}
	return true;
}

}  // namespace fx
