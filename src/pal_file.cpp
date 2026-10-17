#include "pal_file.h"
#include "misc.h"
#include <cstring>

bool PalFile::load(const char *path)
{
	char *data = nullptr;
	unsigned int size = 0;
	if (!ReadInMem(path, data, size) || size < 4) { delete[] data; return false; }
	const uint32_t *d = (const uint32_t *)data;
	size_t off = 0, n = 0;
	if (d[0] == 0xFFFF && size >= 16 && 16ull + (uint64_t)d[3] * 1024 <= size) {
		modern = true; split = d[1]; reserved = d[2]; n = d[3]; off = 16;
	} else if (4ull + (uint64_t)d[0] * 1024 <= size) {
		modern = false; n = d[0]; off = 4;
	} else {
		delete[] data;
		return false;
	}
	colors.assign((const uint32_t *)(data + off), (const uint32_t *)(data + off) + n * 256);
	trailing.assign(data + off + n * 1024, data + size);
	delete[] data;
	return true;
}

std::string PalFile::serialize() const
{
	std::string out;
	auto u32 = [&](uint32_t v) { out.append((const char *)&v, 4); };
	if (modern) { u32(0xFFFF); u32(split); u32(reserved); u32((uint32_t)count()); }
	else u32((uint32_t)count());
	out.append((const char *)colors.data(), colors.size() * 4);
	out += trailing;
	return out;
}

bool PalFile::save(const char *path) const
{
	const std::string bytes = serialize();
	return WriteFileAtomic(path, bytes.data(), bytes.size());
}
