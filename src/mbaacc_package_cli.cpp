// mbaaccpackage: MBAACC character-package tools (docs/HANTEI_WAVE2.md §6).
//
//   mbaaccpackage validate <chara.txt> [--no-packs] [--game <dir>]
//   mbaaccpackage repair-crlf <chara.txt>
//   mbaaccpackage consolidate <chara.txt> [main.HA6] [--no-notes]
//   mbaaccpackage extract <chara.txt> [--game <dir>]      packed-only rows -> loose files
//   mbaaccpackage pack-list <archive.p> [substring]
//   mbaaccpackage pack-extract <archive.p> <data/name> <out> [--mode pc|steam|auto]
//
// Exit code 0 = valid / done, 1 = validation failed or refused, 2 = usage.
// Arguments are read as UTF-16 (wmain) so non-ASCII paths work.
#include "mbaacc_package.h"
#include "mbaacc_pack.h"

#include <windows.h>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>

static std::string Utf8(const wchar_t* w)
{
	const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
	std::string s(n > 0 ? n : 1, '\0');
	if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
	s.pop_back();
	return s;
}

static void Print(const std::string& utf8)
{
	// Console output in UTF-8 regardless of the console code page.
	SetConsoleOutputCP(CP_UTF8);
	fwrite(utf8.data(), 1, utf8.size(), stdout);
	fputc('\n', stdout);
}

static int Usage()
{
	Print("usage:\n"
		"  mbaaccpackage validate <chara.txt> [--no-packs] [--game <dir>]\n"
		"  mbaaccpackage repair-crlf <chara.txt>\n"
		"  mbaaccpackage consolidate <chara.txt> [main.HA6] [--no-notes]\n"
		"  mbaaccpackage extract <chara.txt> [--game <dir>]\n"
		"  mbaaccpackage pack-list <archive.p> [substring]\n"
		"  mbaaccpackage pack-extract <archive.p> <data/name> <out> [--mode pc|steam|auto]");
	return 2;
}

int wmain(int argc, wchar_t** argv)
{
	std::vector<std::string> a;
	for (int i = 1; i < argc; ++i) a.push_back(Utf8(argv[i]));
	if (a.size() < 2) return Usage();
	const std::string cmd = a[0];

	mbpackage::ValidateOptions opt;
	std::vector<std::string> pos;
	bool mergeNotes = true;
	std::string mode = "auto";
	for (size_t i = 1; i < a.size(); ++i) {
		if (a[i] == "--no-packs") opt.searchPacks = false;
		else if (a[i] == "--no-notes") mergeNotes = false;
		else if (a[i] == "--game" && i + 1 < a.size()) opt.gameDir = a[++i];
		else if (a[i] == "--mode" && i + 1 < a.size()) mode = a[++i];
		else pos.push_back(a[i]);
	}

	if (cmd == "validate") {
		const auto r = mbpackage::ValidateCharacterPackage(pos[0], opt);
		Print(r.report);
		return r.valid ? 0 : 1;
	}
	if (cmd == "repair-crlf") {
		const auto r = mbpackage::RepairDescriptorNewlines(pos[0]);
		Print(r.message);
		return r.success ? 0 : 1;
	}
	if (cmd == "consolidate") {
		const auto r = mbpackage::ConsolidateHa6Layers(pos[0], pos.size() > 1 ? pos[1] : std::string(), mergeNotes);
		Print(r.message);
		return r.success ? 0 : 1;
	}
	if (cmd == "extract") {
		const auto r = mbpackage::ExtractPackedRows(pos[0], opt);
		Print(r.message);
		return r.success ? 0 : 1;
	}
	if (cmd == "pack-list" || cmd == "pack-extract") {
		mbpack::Archive archive;
		std::string err;
		const int wn = MultiByteToWideChar(CP_UTF8, 0, pos[0].c_str(), -1, nullptr, 0);
		std::wstring wpath(wn > 0 ? wn : 1, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, pos[0].c_str(), -1, wpath.data(), wn);
		wpath.pop_back();
		if (!archive.open(wpath, err)) { Print(pos[0] + ": " + err); return 1; }
		if (cmd == "pack-list") {
			const mbpack::CryptMode m = archive.detectMode();
			Print(pos[0] + ": " + std::to_string(archive.entries().size()) + " files, cipher " + mbpack::CryptModeName(m));
			for (const auto& e : archive.entries()) {
				const std::string rel = e.relativePath();
				if (pos.size() > 1 && rel.find(pos[1]) == std::string::npos) continue;
				Print("  " + rel + "  " + std::to_string(e.size));
			}
			return 0;
		}
		if (pos.size() < 3) return Usage();
		const mbpack::Entry* e = archive.find(pos[1]);
		if (!e) { Print("not in archive: " + pos[1]); return 1; }
		const mbpack::CryptMode m = mode == "steam" ? mbpack::CryptMode::steam
			: mode == "pc" ? mbpack::CryptMode::pc : archive.detectMode();
		std::vector<uint8_t> bytes;
		if (!archive.read(*e, bytes, m)) { Print("read failed"); return 1; }
		const int on = MultiByteToWideChar(CP_UTF8, 0, pos[2].c_str(), -1, nullptr, 0);
		std::wstring wout(on > 0 ? on : 1, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, pos[2].c_str(), -1, wout.data(), on);
		wout.pop_back();
		std::ofstream out(std::filesystem::path(wout), std::ios::binary);
		out.write((const char*)bytes.data(), (std::streamsize)bytes.size());
		Print("wrote " + std::to_string(bytes.size()) + " bytes (" + mbpack::CryptModeName(m) + " cipher)");
		return out ? 0 : 1;
	}
	return Usage();
}
