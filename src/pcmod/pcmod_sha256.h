#ifndef PCMOD_SHA256_H_GUARD
#define PCMOD_SHA256_H_GUARD
// SHA-256 (FIPS 180-4), streaming. Written for Hantei-chan from the FIPS text; public domain / CC0.
// Hantei-chan had no SHA-256 before the pcmod exporter (the CG manager hashes nothing; bg_renderer has only a PNG CRC32).
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace pcmod {

using Digest = std::array<uint8_t, 32>;

class Sha256 {
public:
	Sha256() { reset(); }
	void reset();
	void update(const void* data, size_t size);
	Digest finish();   // the object must be reset() before reuse
private:
	void block(const uint8_t* p);
	uint32_t m_h[8];
	uint8_t m_buf[64];
	size_t m_bufLen = 0;
	uint64_t m_total = 0;
};

Digest Sha256Of(const void* data, size_t size);
inline Digest Sha256Of(const std::string& s) { return Sha256Of(s.data(), s.size()); }
std::string ToHex(const Digest& d);   // 64 lowercase hex
std::string Sha256Hex(const void* data, size_t size);
inline std::string Sha256Hex(const std::string& s) { return Sha256Hex(s.data(), s.size()); }

} // namespace pcmod
#endif
