// MBAACC package tools: validator, CRLF repair, HA6 layer consolidation and
// the FilePacHeaderA reader, on synthetic files in a temp folder (no game
// data needed). A folder name with Japanese characters checks the UTF-8/UTF-16
// path handling.
#include "mbaacc_package.h"
#include "mbaacc_pack.h"
#include "framedata.h"

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
static int g_failures = 0;
#define CHECK(expr) do { if (!(expr)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); ++g_failures; } } while (0)

static std::string U(const std::wstring& w)
{
	const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
	std::string s(n, '\0');
	WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
	return s;
}

static void WriteFile(const fs::path& p, const std::string& bytes)
{
	std::ofstream f(p, std::ios::binary);
	f.write(bytes.data(), (std::streamsize)bytes.size());
}

static std::string ReadFile(const fs::path& p)
{
	std::ifstream f(p, std::ios::binary);
	return std::string(std::istreambuf_iterator<char>(f), {});
}

// An HA6 with one frame in each of the given patterns.
static bool MakeHa6(const fs::path& p, std::initializer_list<int> patterns)
{
	FrameData fd;
	fd.initEmpty(8);
	for (int id : patterns) {
		Sequence* seq = fd.get_sequence(id);
		if (!seq) return false;
		seq->frames.emplace_back();
		seq->frames.back().AF.duration = 2;
	}
	// FrameData::save takes an ANSI path; write next to the temp root and move.
	const fs::path tmp = fs::temp_directory_path() / "hantei_pkgtest_tmp.ha6";
	if (!fd.save(tmp.string().c_str())) return false;
	std::error_code ec;
	fs::rename(tmp, p, ec);
	if (ec) { fs::copy_file(tmp, p, fs::copy_options::overwrite_existing, ec); fs::remove(tmp); }
	return !ec;
}

// A FilePacHeaderA archive holding the given files (same cipher as the game).
static void MakePack(const fs::path& p, uint32_t key, mbpack::CryptMode mode,
	const std::vector<std::pair<std::string, std::string>>& files)   // "data/name", bytes
{
	auto put32 = [](std::string& s, size_t at, uint32_t v) { for (int i = 0; i < 4; ++i) s[at + i] = (char)(v >> (8 * i)); };
	auto nameXor = [&](char* data, size_t n, uint32_t inc) {
		if (!inc) inc = 1;
		uint8_t k[4] = {(uint8_t)key, (uint8_t)(key >> 8), (uint8_t)(key >> 16), (uint8_t)(key >> 24)};
		for (size_t i = 0; i < n; ++i) { data[i] ^= k[i & 3]; k[i & 3] = (uint8_t)(k[i & 3] + inc); }
	};
	const uint32_t folders = 1;
	std::string header(52, '\0');
	memcpy(&header[0], "FilePacHeaderA", 14);
	std::string table;
	std::string folder(268, '\0');
	put32(folder, 4, 0);
	put32(folder, 8, 7);                       // name increment (non-zero = used)
	memcpy(&folder[12], ".\\data", 6);
	nameXor(&folder[12], 256, 7);
	table += folder;
	std::string payload;
	for (const auto& [rel, bytes] : files) {
		std::string rec(44, '\0');
		put32(rec, 0, (uint32_t)payload.size());
		put32(rec, 4, 0);
		put32(rec, 8, (uint32_t)bytes.size());
		const std::string name = rel.substr(rel.find('/') + 1);
		memcpy(&rec[12], name.data(), name.size());
		nameXor(&rec[12], 32, (uint32_t)bytes.size() & 0xFF);
		table += rec;
		std::string enc = bytes;
		mbpack::XorPayload((uint8_t*)enc.data(), enc.size(), key, mode);
		payload += enc;
	}
	put32(header, 20, key);
	put32(header, 24, (uint32_t)(52 + table.size()));
	put32(header, 32, folders);
	put32(header, 36, (uint32_t)files.size());
	WriteFile(p, header + table + payload);
}

int main()
{
	const fs::path root = fs::temp_directory_path() / L"hantei_pkgtest_キャラ";
	std::error_code ec;
	fs::remove_all(root, ec);
	const fs::path data = root / "data";
	fs::create_directories(data);

	// --- validator: mixed line endings, loose HA6/CG, PAniFile FileNum=0 ---
	CHECK(MakeHa6(data / "tst.HA6", {0, 1, 2}));
	CHECK(MakeHa6(data / "tst_0.HA6", {0, 1}));
	WriteFile(data / "tst.CG", std::string("BMP Cutter3") + std::string(32, '\0'));
	const std::string desc =
		"[System]\r\nProjectName=tst_0\r\n\r\n[DataFile]\r\nFileNum= 2\r\nFile00=tst_0.HA6\nFile01=tst.HA6\r\n"
		"[BmpcutFile]\r\nFileNum= 1\r\nFile00=tst.CG\r\n[PAniFile]\r\nFileNum= 0\r\n";
	WriteFile(data / "tst_0.txt", desc);
	const std::string descPath = U((data / "tst_0.txt").wstring());

	auto v = mbpackage::ValidateCharacterPackage(descPath);
	CHECK(!v.valid);
	CHECK(v.loneLf == 1 && v.crlf == 11);
	CHECK(v.newlineRepairAvailable);
	CHECK(v.rows.size() == 3);
	for (const auto& r : v.rows) CHECK(r.status == mbpackage::RowStatus::loose);

	// --- CRLF repair: backup + atomic rewrite, then valid ---
	auto rep = mbpackage::RepairDescriptorNewlines(descPath);
	CHECK(rep.success);
	CHECK(!rep.backup.empty());
	CHECK(ReadFile(data / "tst_0.txt").find("\nFile01") != std::string::npos);
	CHECK(ReadFile(data / "tst_0.txt").find("HA6\nFile01") == std::string::npos);
	v = mbpackage::ValidateCharacterPackage(descPath);
	CHECK(v.valid);
	CHECK(v.loneLf == 0);
	// A second repair in the same second must not collide with the first backup.
	auto rep2 = mbpackage::RepairDescriptorNewlines(descPath);
	CHECK(rep2.success);   // already CRLF: nothing to do

	// --- bad header and missing file ---
	WriteFile(data / "tst.CG", "NOT A CG FILE....");
	v = mbpackage::ValidateCharacterPackage(descPath, mbpackage::ValidateOptions{false, {}});
	CHECK(!v.valid && v.rows.size() == 3 && v.rows[2].status == mbpackage::RowStatus::badHeader);
	fs::remove(data / "tst.CG");
	v = mbpackage::ValidateCharacterPackage(descPath, mbpackage::ValidateOptions{false, {}});
	CHECK(!v.valid);

	// --- game archive lookup: tst.CG only packed (Steam cipher, >8 KiB) ---
	std::string cg = std::string("BMP Cutter3") + std::string(20000, '\0');
	for (size_t i = 64; i < cg.size(); ++i) cg[i] = (char)((i * 7) & 0x3);   // low-entropy body
	MakePack(root / "0002.p", 0x1234ABCDu, mbpack::CryptMode::steam, {{"data/tst.CG", cg}, {"data/other.txt", "hello"}});
	{
		mbpack::Archive a;
		std::string err;
		CHECK(a.open((root / "0002.p").wstring(), err));
		CHECK(a.entries().size() == 2);
		CHECK(a.find("DATA\\TST.cg") != nullptr);
		CHECK(a.detectMode() == mbpack::CryptMode::steam);
		std::vector<uint8_t> bytes;
		CHECK(a.read(*a.find("data/tst.CG"), bytes, mbpack::CryptMode::steam));
		CHECK(std::string(bytes.begin(), bytes.end()) == cg);
		std::vector<uint8_t> wrong;
		CHECK(a.read(*a.find("data/tst.CG"), wrong, mbpack::CryptMode::pc));
		CHECK(std::string(wrong.begin(), wrong.end()) != cg);                 // tail differs
		CHECK(memcmp(wrong.data(), cg.data(), 4096) == 0);                    // head does not
	}
	v = mbpackage::ValidateCharacterPackage(descPath);
	CHECK(v.valid);
	CHECK(v.rows.size() == 3 && v.rows[2].status == mbpackage::RowStatus::packed);
	auto ex = mbpackage::ExtractPackedRows(descPath);
	CHECK(ex.success);
	CHECK(ReadFile(data / "tst.CG") == cg);
	// PC-cipher archive: detection picks PC.
	MakePack(root / "0009.p", 0x0BADF00Du, mbpack::CryptMode::pc, {{"data/big.bin", cg}});
	{
		mbpack::Archive a;
		std::string err;
		CHECK(a.open((root / "0009.p").wstring(), err));
		CHECK(a.detectMode() == mbpack::CryptMode::pc);
	}

	// --- consolidation: top layer has every pattern -> one HA6 ---
	const std::string topBytes = ReadFile(data / "tst.HA6");
	auto con = mbpackage::ConsolidateHa6Layers(descPath);
	CHECK(con.success);
	if (!con.success) std::printf("%s\n", con.message.c_str());
	CHECK(fs::exists(data / "tst_0.HA6"));                 // canonical name = descriptor stem
	CHECK(ReadFile(data / "tst_0.HA6") == topBytes);       // byte-for-byte top layer
	CHECK(!fs::exists(data / "tst.HA6"));                  // superseded layer archived
	const std::string newDesc = ReadFile(data / "tst_0.txt");
	CHECK(newDesc.find("FileNum= 1\r\nFile00=tst_0.HA6\r\n") != std::string::npos);
	CHECK(newDesc.find("tst.HA6") == std::string::npos);
	CHECK(!con.backup.empty());
	v = mbpackage::ValidateCharacterPackage(descPath, mbpackage::ValidateOptions{false, {}});
	CHECK(v.valid);

	// --- consolidation refuses when a lower layer has a pattern the top lacks ---
	CHECK(MakeHa6(data / "low.HA6", {0, 5}));
	CHECK(MakeHa6(data / "high.HA6", {0, 1}));
	WriteFile(data / "two.txt", "[DataFile]\r\nFileNum= 2\r\nFile00=low.HA6\r\nFile01=high.HA6\r\n"
		"[BmpcutFile]\r\nFileNum= 1\r\nFile00=tst.CG\r\n[PAniFile]\r\nFileNum= 0\r\n");
	const std::string before = ReadFile(data / "two.txt");
	con = mbpackage::ConsolidateHa6Layers(U((data / "two.txt").wstring()));
	CHECK(!con.success);
	CHECK(con.message.find("Pattern 5") != std::string::npos);
	CHECK(ReadFile(data / "two.txt") == before);
	CHECK(fs::exists(data / "low.HA6") && fs::exists(data / "high.HA6"));

	fs::remove_all(root, ec);
	if (g_failures) { std::printf("package_tools_test: %d failure(s)\n", g_failures); return 1; }
	std::printf("package_tools_test: PASS\n");
	return 0;
}
