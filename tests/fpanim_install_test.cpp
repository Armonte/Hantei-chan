// fpan-im install planning (src/fpanim_install.h): status, bundle layouts, backup/chainload, uninstall restore,
// and the "only our files" guard (UNI2-IM / MBTL-IM files are never in a plan).
#include "fpanim_install.h"

#include <cstdio>
#include <set>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

using namespace fpaninst;

static FileInfo Ours(const char* v = "0.1.0")
{
	FileInfo f; f.exists = true; f.hasVersion = true; f.productName = "fpan-im"; f.version = v; f.hasMarker = true; return f;
}
static FileInfo Foreign()
{
	FileInfo f; f.exists = true; f.hasVersion = true; f.productName = "Some Loader"; f.version = "2.0"; return f;
}
static bool Has(const Plan& p, Op::Type t, const std::string& src, const std::string& dst)
{
	for (const Op& o : p.ops) if (o.type == t && o.dst == dst && (src.empty() || o.src == src)) return true;
	return false;
}

int main()
{
	// Shim identity and version labels.
	CHECK(IsOurShim(Ours()));
	CHECK(!IsOurShim(Foreign()));
	FileInfo legacy; legacy.exists = true; legacy.hasMarker = true;   // a build before the version resource
	CHECK(IsOurShim(legacy) && VersionLabel(legacy) == "unknown version");
	CHECK(VersionLabel(FileInfo{}).empty() && VersionLabel(Ours("0.2.0")) == "0.2.0");

	// Status.
	GameState g; g.dir = "C:/games/uni2";
	CHECK(Classify(g) == Status::NotInstalled);
	g.shim = Foreign();
	CHECK(Classify(g) == Status::NotInstalled);
	g.shim = Ours(); g.core = Ours();
	CHECK(Classify(g) == Status::Installed);
	g.core = FileInfo{};
	CHECK(Classify(g) == Status::Partial);

	// Bundle layouts.
	std::set<std::string> files = { "R/winmm.dll", "R/fpan-im/fpan-im.dll", "B/winmm.dll", "B/fpan-im.dll", "X/winmm.dll" };
	auto ex = [&](const std::string& p) { return files.count(p) != 0; };
	Bundle r = ResolveBundle("R", ex), b = ResolveBundle("B\\", ex);
	CHECK(r.found && r.core == "R/fpan-im/fpan-im.dll" && r.winmm == "R/winmm.dll");
	CHECK(b.found && b.core == "B/fpan-im.dll");
	CHECK(!ResolveBundle("X", ex).found && !ResolveBundle("", ex).found && !ResolveBundle("none", ex).found);

	// Not bundled: refused.
	GameState clean; clean.dir = "C:/games/uni2";
	CHECK(!PlanInstall(clean, Bundle{}, "t").refused.empty());

	// Clean install: mkdir, core, shim, record without backup.
	Plan p = PlanInstall(clean, r, "t");
	CHECK(p.refused.empty() && Has(p, Op::Copy, r.core, "fpan-im/fpan-im.dll") && Has(p, Op::Copy, r.winmm, "winmm.dll"));
	CHECK(!Has(p, Op::Move, "", "winmm.chain.dll") && PlanTouchesOnlyOurFiles(p));

	// Foreign winmm.dll: moved to winmm.chain.dll, recorded, said.
	GameState fg = clean; fg.shim = Foreign();
	p = PlanInstall(fg, r, "t");
	CHECK(Has(p, Op::Move, "winmm.dll", "winmm.chain.dll") && !p.notes.empty() && PlanTouchesOnlyOurFiles(p));
	bool rec = false;
	for (const Op& o : p.ops) if (o.type == Op::WriteText && o.text.find("backup=winmm.chain.dll") != std::string::npos) rec = true;
	CHECK(rec);
	// The move comes before the copy over winmm.dll.
	CHECK(p.ops.front().type == Op::Move);

	// Foreign winmm.dll with an existing chain: kept, foreign saved as .bak-<stamp>.
	fg.chainExists = true;
	p = PlanInstall(fg, r, "20261017-120000");
	CHECK(Has(p, Op::Move, "winmm.dll", "winmm.dll.bak-20261017-120000") && !Has(p, Op::Move, "", "winmm.chain.dll"));
	CHECK(PlanTouchesOnlyOurFiles(p));

	// Update over our own install keeps the recorded backup.
	GameState up = clean; up.shim = Ours("0.0.9"); up.core = Ours("0.0.9"); up.chainExists = true; up.recordedBackup = "winmm.chain.dll";
	p = PlanInstall(up, r, "t");
	CHECK(!Has(p, Op::Move, "", "winmm.chain.dll") && !Has(p, Op::Move, "", "winmm.dll.bak-t"));
	rec = false;
	for (const Op& o : p.ops) if (o.type == Op::WriteText && o.text.find("backup=winmm.chain.dll") != std::string::npos) rec = true;
	CHECK(rec);

	// Uninstall: ours removed, our backup restored, mods kept.
	p = PlanUninstall(up, { "fpan-im.pdb" });
	CHECK(Has(p, Op::Remove, "", "winmm.dll") && Has(p, Op::Remove, "", "fpan-im/fpan-im.dll") && Has(p, Op::Remove, "", "fpan-im/fpan-im.pdb"));
	CHECK(Has(p, Op::Move, "winmm.chain.dll", "winmm.dll") && PlanTouchesOnlyOurFiles(p));
	CHECK(p.ops.back().type == Op::Move);   // restore after removing our shim
	// A chain we did not create stays.
	up.recordedBackup.clear();
	p = PlanUninstall(up);
	CHECK(!Has(p, Op::Move, "", "winmm.dll"));
	// A foreign winmm.dll is never removed.
	GameState half = clean; half.shim = Foreign(); half.core = Ours();
	p = PlanUninstall(half);
	CHECK(!Has(p, Op::Remove, "", "winmm.dll") && Has(p, Op::Remove, "", "fpan-im/fpan-im.dll"));
	CHECK(!PlanUninstall(clean).refused.empty());

	// The guard: UNI2-IM / MBTL-IM and the mods folder are never ours.
	for (const char* bad : { "dinput8.dll", "UNI2-IM/UNI2-IM.ini", "MBTL-IM/x.dll", "fpan-im/mods/data/chr000/chr000.ha6",
	                         "fpan-im/fpan-im.ini", "../winmm.dll", "C:/winmm.dll", "fpan-im/logs/x.pdb" }) {
		Plan q; q.ops.push_back({ Op::Remove, "", bad, "" });
		CHECK(!PlanTouchesOnlyOurFiles(q));
	}

	// install.txt and process paths.
	CHECK(ParseInstallRecord("# x\nbackup=winmm.chain.dll\r\n") == "winmm.chain.dll" && ParseInstallRecord("# x\n").empty());
	CHECK(ImageIsInDir("C:\\Games\\UNI2\\uni2.exe", "c:/games/uni2/"));
	CHECK(ImageIsInDir("D:/Steam/MELTY BLOOD TYPE LUMINA/MBTL.exe", "D:\\Steam\\MELTY BLOOD TYPE LUMINA"));
	CHECK(!ImageIsInDir("C:/Games/UNI2/sub/uni2.exe", "C:/Games/UNI2") && !ImageIsInDir("C:/Games/UNI2/notepad.exe", "C:/Games/UNI2"));

	if (g_fail) return 1;
	std::printf("fpanim_install_test: ok\n");
	return 0;
}
