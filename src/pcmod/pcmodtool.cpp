// pcmodtool: PovertyCaster pcmod package exporter / checker (docs/modmanager/pcmod-manifest_schema1.md,
// Hantei_Docs/pcmod_export.md).
//
//   pcmodtool pack <srcdir> --id <id> --version <x.y.z> [--class sim|cosmetic] --exe <sha|MBAA.exe path>...
//             [--out <dir>] [--name ..] [--author ..] [--description ..] [--priority N]
//             [--requires <id>@<x.y.z>]... [--conflicts <id>]... [--overrides <id>]... [--modes tag,team] [--no-zip] [--game <dir>]
//        --modes: the online session modes the package counts for (absent = every mode; not part of any digest).
//        <srcdir> holds the files at their game-relative paths (data\..., grp\...). Writes <out>\<id>\pcmod.json + files
//        and <out>\<id>-<version>.zip (single top-level <id>\ directory). --class defaults to sim when any file is under
//        data\, else cosmetic. --exe defaults to the SHA-256 of C:\games\mbaacc\MBAA.exe when found. --game (default
//        C:\games\mbaacc) lists files that replace a file of that install.
//   pcmodtool verify <dir|zip>          exit 0 = valid, 1 = broken
//   pcmodtool digest <dir|zip>          prints "package <hex>" and "sim <hex>" (exit 1 if the package does not verify)
//   pcmodtool session [--mode <m>] <exe> <pkgdir>...  the session sim digest; <exe> is a 64-hex SHA-256 or a path to
//        MBAA.exe. --mode builds the set for that session mode (§6): only sim packages whose modes are empty or list it.
//
// The output on stdout is the tool's result (like cgmtool / mbaaccpackage); it is not a log.
#include "pcmod_manifest.h"
#include "pcmod_package.h"
#include "pcmod_vanilla.h"

#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
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

static void Out(const std::string& s) { std::fwrite(s.data(), 1, s.size(), stdout); }
static void Err(const std::string& s) { std::fwrite(s.data(), 1, s.size(), stderr); }

static int Usage()
{
	Err("usage:\n"
		"  pcmodtool pack <srcdir> --id <id> --version <x.y.z> [--class sim|cosmetic] --exe <sha|exe path>...\n"
		"            [--out <dir>] [--name ..] [--author ..] [--description ..] [--priority N]\n"
		"            [--requires <id>@<x.y.z>]... [--conflicts <id>]... [--overrides <id>]... [--modes tag,team]\n"
		"            [--no-zip] [--game <dir>]\n"
		"  pcmodtool verify <dir|zip>\n"
		"  pcmodtool digest <dir|zip>\n"
		"  pcmodtool session [--mode <m>] <exe sha|exe path> <pkgdir>...\n");
	return 2;
}

// A 64-hex hash as given (lowercased), or the SHA-256 of the file at that path.
static std::string ExeHash(const std::string& a)
{
	std::string low = a;
	for (char& c : low) if (c >= 'A' && c <= 'F') c = char(c - 'A' + 'a');
	if (pcmod::IsLowerHex64(low)) return low;
	return pcmod::Sha256OfFile(a);
}

static void PrintVerify(const pcmod::VerifyResult& v, const std::string& what)
{
	for (const auto& e : v.errors) Out("ERROR " + e + "\n");
	for (const auto& w : v.warnings) Out("WARN  " + w + "\n");
	if (v.ok) {
		const auto& m = v.manifest;
		Out("OK " + what + ": " + m.id + " " + m.version + " (" + pcmod::ClassName(m.cls) + ", " + std::to_string(m.files.size()) + " files)\n");
		Out("modes          " + (m.modes.empty() ? std::string("(every mode)") : pcmod::JoinModes(m.modes)) + "\n");
		Out("package digest " + v.digestHex + "\n");
		Out("sim digest     " + v.simDigestHex + "\n");
	} else Out("BROKEN " + what + "\n");
}

static int Pack(const std::vector<std::string>& a)
{
	if (a.empty()) return Usage();
	const std::string src = a[0];
	pcmod::Manifest m;
	std::string out = ".", cls, game = pcmod::DefaultGameDir();
	bool zip = true;
	for (size_t i = 1; i < a.size(); ++i) {
		const std::string& k = a[i];
		auto val = [&]() -> std::string { if (i + 1 >= a.size()) { Err(k + " needs a value\n"); std::exit(2); } return a[++i]; };
		if (k == "--id") m.id = val();
		else if (k == "--version") m.version = val();
		else if (k == "--class") cls = val();
		else if (k == "--exe") {
			m.exe.push_back(val());
			while (i + 1 < a.size() && a[i + 1].rfind("--", 0) != 0) m.exe.push_back(a[++i]);   // --exe <sha>...
		}
		else if (k == "--out") out = val();
		else if (k == "--name") m.name = val();
		else if (k == "--author") m.author = val();
		else if (k == "--description") m.description = val();
		else if (k == "--priority") m.priority = std::atoi(val().c_str());
		else if (k == "--requires") {
			const std::string r = val();
			const size_t at = r.find('@');
			if (at == std::string::npos) { Err("--requires wants <id>@<x.y.z>\n"); return 2; }
			m.requires_.push_back({r.substr(0, at), r.substr(at + 1)});
		}
		else if (k == "--conflicts") m.conflicts.push_back(val());
		else if (k == "--overrides") m.overrides.push_back(val());
		else if (k == "--modes") {
			for (const std::string& md : pcmod::SplitModes(val()))
				if (std::find(m.modes.begin(), m.modes.end(), md) == m.modes.end()) m.modes.push_back(md);
		}
		else if (k == "--no-zip") zip = false;
		else if (k == "--game") game = val();
		else { Err("unknown option " + k + "\n"); return Usage(); }
	}
	for (std::string& e : m.exe) {
		const std::string h = ExeHash(e);
		if (h.empty()) { Err("--exe " + e + ": not a SHA-256 and not a readable file\n"); return 1; }
		e = h;
	}
	if (m.exe.empty() && !game.empty()) {
		const std::string h = pcmod::DefaultExeSha256(game);
		if (!h.empty()) { m.exe.push_back(h); Out("exe: " + h + " (" + game + "\\MBAA.exe)\n"); }
	}
	std::vector<pcmod::SourceFile> files;
	std::string why;
	if (!pcmod::ScanSourceDir(src, files, why)) { Err(why + "\n"); return 1; }
	// data\*.wmt is written by the game on every result screen and never read: skip it rather than refuse the pack.
	for (auto it = files.begin(); it != files.end();) {
		std::string n;
		if (pcmod::NormalisePath(it->rel, n, why) && pcmod::IsGameWrittenPath(n)) {
			Out("WARN  skipped " + it->rel + ": " + pcmod::RefusedPathReason(n) + "\n");
			it = files.erase(it);
		} else ++it;
	}
	if (cls.empty()) {
		size_t sim = 0;
		for (const auto& f : files) {
			std::string n;
			if (pcmod::NormalisePath(f.rel, n, why) && pcmod::ClassifyPath(n) == pcmod::PathClass::Sim) ++sim;
		}
		m.cls = sim ? pcmod::PackageClass::Sim : pcmod::PackageClass::Cosmetic;
		Out(std::string("class: ") + pcmod::ClassName(m.cls) + " (detected)\n");
		Out("files: " + std::to_string(files.size()) + ", " + std::to_string(sim) + " in the online sim digest\n");
	} else if (!pcmod::ParseClass(cls, m.cls)) { Err("--class must be sim or cosmetic\n"); return 2; }

	pcmod::VanillaIndex vanilla;
	if (!game.empty() && vanilla.Load(game)) {
		for (const auto& f : files) {
			std::string n;
			if (!pcmod::NormalisePath(f.rel, n, why)) continue;
			const char* w = vanilla.Where(n);
			if (*w) Out(std::string("REPLACES vanilla ") + n + " (" + w + " in " + game + "): the package must ship the whole file\n");
		}
	}
	const pcmod::PackResult r = pcmod::Pack(files, m, out, zip);
	if (!r.ok) { Err("pack refused: " + r.error + "\n"); return 1; }
	Out("wrote " + r.packageDir + "\\pcmod.json (" + std::to_string(r.manifest.files.size()) + " files)\n");
	if (!r.zipPath.empty()) Out("wrote " + r.zipPath + "\n");
	if (!r.manifest.modes.empty()) Out("modes " + pcmod::JoinModes(r.manifest.modes) + "\n");
	Out("package digest " + r.digestHex + "\n");
	Out("sim digest " + r.simDigestHex + "\n");
	return 0;
}

int wmain(int argc, wchar_t** argv)
{
	SetConsoleOutputCP(CP_UTF8);
	std::vector<std::string> a;
	for (int i = 1; i < argc; ++i) a.push_back(Utf8(argv[i]));
	if (a.empty()) return Usage();
	const std::string cmd = a[0];
	a.erase(a.begin());
	if (cmd == "pack") return Pack(a);
	if (cmd == "verify" && a.size() == 1) { const auto v = pcmod::VerifyPath(a[0]); PrintVerify(v, a[0]); return v.ok ? 0 : 1; }
	if (cmd == "digest" && a.size() == 1) {
		const auto v = pcmod::VerifyPath(a[0]);
		if (!v.ok) { PrintVerify(v, a[0]); return 1; }
		Out("package " + v.digestHex + "\n");
		Out("sim     " + v.simDigestHex + "\n");
		return 0;
	}
	if (cmd == "session") {
		std::string mode;
		if (!a.empty() && a[0] == "--mode") {
			if (a.size() < 2) return Usage();
			mode = a[1];
			a.erase(a.begin(), a.begin() + 2);
			if (!pcmod::ValidId(mode)) { Err("--mode " + mode + ": must match [a-z0-9][a-z0-9._-]{0,63}\n"); return 2; }
		}
		if (a.empty()) return Usage();
		const std::string exe = ExeHash(a[0]);
		if (exe.empty()) { Err(a[0] + ": not a SHA-256 and not a readable file\n"); return 1; }
		const pcmod::SessionResult s = pcmod::ResolveSession(exe, std::vector<std::string>(a.begin() + 1, a.end()), mode);
		for (const auto& n : s.notes) Out("note  " + n + "\n");
		for (const auto& e : s.errors) Out("ERROR " + e + "\n");
		if (!s.ok) { Out("no session digest: an enabled sim package is broken\n"); return 1; }
		Out("exe " + exe + "\n");
		Out("mode " + (mode.empty() ? std::string("(no filter)") : mode) + "\n");
		for (const auto& e : s.simOrder) Out("sim  " + e.id + " " + e.version + " priority " + std::to_string(e.priority) + " " + e.simDigestHex + " (sim digest)\n");
		if (s.simOrder.empty()) Out("(no sim package: vanilla set)\n");
		Out("session digest " + s.digestHex + "\n");
		return 0;
	}
	return Usage();
}
