#include "mbaacc_pack.h"

#include <windows.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace mbpack {

namespace {

constexpr uint32_t kIncrement = 3;
constexpr size_t kChunk = 0x1000;

uint32_t U32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

void XorStream(uint8_t* data, size_t size, uint32_t key, uint32_t increment)
{
	if (increment == 0) increment = 1;
	uint8_t k[4] = {(uint8_t)key, (uint8_t)(key >> 8), (uint8_t)(key >> 16), (uint8_t)(key >> 24)};
	for (size_t i = 0; i < size; ++i) {
		data[i] ^= k[i & 3];
		k[i & 3] = (uint8_t)(k[i & 3] + increment);
	}
}

std::string SjisToUtf8(const uint8_t* bytes, size_t maxLen)
{
	size_t len = 0;
	while (len < maxLen && bytes[len]) ++len;
	if (!len) return std::string();
	const int wn = MultiByteToWideChar(932, 0, (const char*)bytes, (int)len, nullptr, 0);
	std::wstring w(wn > 0 ? wn : 0, L'\0');
	if (wn > 0) MultiByteToWideChar(932, 0, (const char*)bytes, (int)len, w.data(), wn);
	const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
	std::string s(n > 0 ? n : 0, '\0');
	if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
	return s;
}

std::string LowerSlash(std::string s)
{
	for (char& c : s) {
		if (c == '\\') c = '/';
		c = (char)std::tolower((unsigned char)c);
	}
	while (s.rfind("./", 0) == 0) s.erase(0, 2);
	return s;
}

double Entropy(const uint8_t* data, size_t n)
{
	if (!n) return 0.0;
	size_t counts[256] = {};
	for (size_t i = 0; i < n; ++i) ++counts[data[i]];
	double e = 0.0;
	for (size_t c : counts) {
		if (!c) continue;
		const double p = (double)c / (double)n;
		e -= p * std::log2(p);
	}
	return e;
}

} // namespace

const char* CryptModeName(CryptMode mode)
{
	return mode == CryptMode::steam ? "Steam" : "PC";
}

std::string Entry::relativePath() const
{
	std::string dir = folder;
	for (char& c : dir) if (c == '\\') c = '/';
	while (dir.rfind("./", 0) == 0) dir.erase(0, 2);
	if (dir == ".") dir.clear();
	return dir.empty() ? name : dir + "/" + name;
}

void XorPayload(uint8_t* data, size_t size, uint32_t key, CryptMode mode)
{
	XorStream(data, std::min(size, kChunk), key, kIncrement);
	if (mode == CryptMode::steam && size > 2 * kChunk)
		XorStream(data + size - kChunk, kChunk, key, kIncrement);
}

bool Archive::open(const std::wstring& path, std::string& error)
{
	m_path = path;
	m_entries.clear();
	std::ifstream f(std::filesystem::path(path), std::ios::binary);
	if (!f) { error = "cannot open archive"; return false; }
	uint8_t header[52];
	if (!f.read((char*)header, sizeof(header)) || memcmp(header, "FilePacHeaderA", 14) != 0) {
		error = "not a FilePacHeaderA archive";
		return false;
	}
	m_key = U32(header + 20);
	m_dataOffset = U32(header + 24);
	const uint32_t folderCount = U32(header + 32);
	const uint32_t fileCount = U32(header + 36);
	if (folderCount > 100000 || fileCount > 1000000) { error = "implausible archive header"; return false; }

	std::vector<std::string> folders(folderCount);
	std::vector<uint8_t> rec(268);
	for (uint32_t i = 0; i < folderCount; ++i) {
		if (!f.read((char*)rec.data(), 268)) { error = "truncated folder table"; return false; }
		const uint32_t inc = U32(rec.data() + 8);
		if (inc == 0) continue;   // empty sentinel folder
		XorStream(rec.data() + 12, 256, m_key, inc & 0xFF);
		folders[i] = SjisToUtf8(rec.data() + 12, 256);
	}
	m_entries.reserve(fileCount);
	for (uint32_t i = 0; i < fileCount; ++i) {
		if (!f.read((char*)rec.data(), 44)) { error = "truncated file table"; return false; }
		Entry e;
		e.offset = U32(rec.data());
		const uint32_t owner = U32(rec.data() + 4);
		e.size = U32(rec.data() + 8);
		XorStream(rec.data() + 12, 32, m_key, e.size & 0xFF);
		e.name = SjisToUtf8(rec.data() + 12, 32);
		e.folder = owner < folders.size() ? folders[owner] : std::string();
		m_entries.push_back(std::move(e));
	}
	return true;
}

const Entry* Archive::find(const std::string& relativePath) const
{
	const std::string want = LowerSlash(relativePath);
	for (const Entry& e : m_entries)
		if (LowerSlash(e.relativePath()) == want) return &e;
	return nullptr;
}

bool Archive::read(const Entry& entry, std::vector<uint8_t>& out, CryptMode mode, size_t maxBytes) const
{
	std::ifstream f(std::filesystem::path(m_path), std::ios::binary);
	if (!f) return false;
	f.seekg((std::streamoff)m_dataOffset + entry.offset);
	out.resize(entry.size);
	if (!f.read((char*)out.data(), entry.size)) return false;
	XorPayload(out.data(), out.size(), m_key, mode);
	if (out.size() > maxBytes) out.resize(maxBytes);
	return true;
}

CryptMode Archive::detectMode() const
{
	const Entry* largest = nullptr;
	for (const Entry& e : m_entries)
		if (e.size > 2 * kChunk && (!largest || e.size > largest->size)) largest = &e;
	if (!largest) return CryptMode::pc;   // no file reaches the Steam-only region
	std::vector<uint8_t> pc, steam;
	if (!read(*largest, pc, CryptMode::pc) || !read(*largest, steam, CryptMode::steam)) return CryptMode::pc;
	const double ePc = Entropy(pc.data() + pc.size() - kChunk, kChunk);
	const double eSteam = Entropy(steam.data() + steam.size() - kChunk, kChunk);
	return eSteam < ePc ? CryptMode::steam : CryptMode::pc;
}

std::vector<std::wstring> FindPacks(const std::wstring& gameDir)
{
	std::vector<std::wstring> packs;
	std::error_code ec;
	for (const auto& it : std::filesystem::directory_iterator(std::filesystem::path(gameDir), ec)) {
		if (!it.is_regular_file(ec)) continue;
		std::wstring ext = it.path().extension().wstring();
		for (auto& c : ext) c = (wchar_t)towlower(c);
		if (ext == L".p") packs.push_back(it.path().wstring());
	}
	std::sort(packs.begin(), packs.end());
	return packs;
}

} // namespace mbpack
