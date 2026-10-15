// SHA-256 per FIPS 180-4 §6.2. Public domain / CC0.
#include "pcmod_sha256.h"
#include <cstring>

namespace pcmod {
namespace {
const uint32_t kK[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
inline uint32_t Rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
}

void Sha256::reset()
{
	static const uint32_t kInit[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
	std::memcpy(m_h, kInit, sizeof m_h);
	m_bufLen = 0;
	m_total = 0;
}

void Sha256::block(const uint8_t* p)
{
	uint32_t w[64];
	for (int t = 0; t < 16; ++t)
		w[t] = (uint32_t)p[4 * t] << 24 | (uint32_t)p[4 * t + 1] << 16 | (uint32_t)p[4 * t + 2] << 8 | p[4 * t + 3];
	for (int t = 16; t < 64; ++t) {
		const uint32_t s0 = Rotr(w[t - 15], 7) ^ Rotr(w[t - 15], 18) ^ (w[t - 15] >> 3);
		const uint32_t s1 = Rotr(w[t - 2], 17) ^ Rotr(w[t - 2], 19) ^ (w[t - 2] >> 10);
		w[t] = w[t - 16] + s0 + w[t - 7] + s1;
	}
	uint32_t a = m_h[0], b = m_h[1], c = m_h[2], d = m_h[3], e = m_h[4], f = m_h[5], g = m_h[6], h = m_h[7];
	for (int t = 0; t < 64; ++t) {
		const uint32_t t1 = h + (Rotr(e, 6) ^ Rotr(e, 11) ^ Rotr(e, 25)) + ((e & f) ^ (~e & g)) + kK[t] + w[t];
		const uint32_t t2 = (Rotr(a, 2) ^ Rotr(a, 13) ^ Rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
		h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
	}
	m_h[0] += a; m_h[1] += b; m_h[2] += c; m_h[3] += d; m_h[4] += e; m_h[5] += f; m_h[6] += g; m_h[7] += h;
}

void Sha256::update(const void* data, size_t size)
{
	const uint8_t* p = (const uint8_t*)data;
	m_total += size;
	if (m_bufLen) {
		const size_t take = size < 64 - m_bufLen ? size : 64 - m_bufLen;
		std::memcpy(m_buf + m_bufLen, p, take);
		m_bufLen += take; p += take; size -= take;
		if (m_bufLen == 64) { block(m_buf); m_bufLen = 0; }
	}
	while (size >= 64) { block(p); p += 64; size -= 64; }
	if (size) { std::memcpy(m_buf, p, size); m_bufLen = size; }
}

Digest Sha256::finish()
{
	const uint64_t bits = m_total * 8;
	const uint8_t pad = 0x80;
	update(&pad, 1);
	const uint8_t zero = 0;
	while (m_bufLen != 56) update(&zero, 1);
	uint8_t len[8];
	for (int i = 0; i < 8; ++i) len[i] = (uint8_t)(bits >> (56 - 8 * i));
	update(len, 8);
	Digest out;
	for (int i = 0; i < 8; ++i) {
		out[4 * i] = (uint8_t)(m_h[i] >> 24); out[4 * i + 1] = (uint8_t)(m_h[i] >> 16);
		out[4 * i + 2] = (uint8_t)(m_h[i] >> 8); out[4 * i + 3] = (uint8_t)m_h[i];
	}
	return out;
}

Digest Sha256Of(const void* data, size_t size)
{
	Sha256 s;
	s.update(data, size);
	return s.finish();
}

std::string ToHex(const Digest& d)
{
	static const char kHex[] = "0123456789abcdef";
	std::string s;
	s.reserve(64);
	for (uint8_t b : d) { s += kHex[b >> 4]; s += kHex[b & 15]; }
	return s;
}

std::string Sha256Hex(const void* data, size_t size) { return ToHex(Sha256Of(data, size)); }

} // namespace pcmod
