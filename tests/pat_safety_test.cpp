// PAT editor file safety (S2): auto-backup rotation, atomic .pat save, the PAT document's dirty flag and undo of
// parts edits through the UndoManager parts channel. Exit code 0 = pass.
//   pat_safety_test [some.pat ...]   real files are loaded, edited, saved, reloaded and undone too
#include "file_backup.h"
#include "undo_manager.h"
#include "parts/parts.h"
#include "parts/parts_undo.h"
#include "misc.h"

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <string>
#include <vector>

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_failures; } } while (0)

static std::string TempDir(const char* tag)
{
	char base[MAX_PATH];
	GetTempPathA(MAX_PATH, base);
	std::string d = std::string(base) + "hc_pat_safety_" + tag + "_" + std::to_string(GetCurrentProcessId());
	CreateDirectoryA(d.c_str(), nullptr);
	return d;
}

static bool WriteText(const std::string& p, const std::string& s)
{
	FILE* f = fopen(p.c_str(), "wb");
	if (!f) return false;
	fwrite(s.data(), 1, s.size(), f);
	fclose(f);
	return true;
}

static std::string ReadAll(const std::string& p)
{
	char* d = nullptr; unsigned n = 0;
	if (!ReadInMem(p.c_str(), d, n)) return std::string("<missing>");
	std::string s(d, n);
	delete[] d;
	return s;
}

static int CountFiles(const std::string& dir)
{
	int n = 0;
	WIN32_FIND_DATAA fd;
	HANDLE h = FindFirstFileA((dir + "\\*").c_str(), &fd);
	if (h == INVALID_HANDLE_VALUE) return 0;
	do { if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) n++; } while (FindNextFileA(h, &fd));
	FindClose(h);
	return n;
}

static void RemoveTree(const std::string& dir)
{
	WIN32_FIND_DATAA fd;
	HANDLE h = FindFirstFileA((dir + "\\*").c_str(), &fd);
	if (h != INVALID_HANDLE_VALUE) {
		do {
			const std::string n = fd.cFileName;
			if (n == "." || n == "..") continue;
			const std::string p = dir + "\\" + n;
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) RemoveTree(p); else DeleteFileA(p.c_str());
		} while (FindNextFileA(h, &fd));
		FindClose(h);
	}
	RemoveDirectoryA(dir.c_str());
}

// ---------------------------------------------------------------------------

static void TestBackups()
{
	const std::string dir = TempDir("bk");
	const std::string target = dir + "\\chr000.pat";
	BackupOptions o;
	o.keep = 3;

	// Nothing to back up yet: success, no folder.
	std::string made, err;
	CHECK(BackupBeforeSave(target, o, &made, &err) && made.empty());
	CHECK(GetFileAttributesA((dir + "\\backup").c_str()) == INVALID_FILE_ATTRIBUTES);

	// Saves in the same second get -00, -01; names sort in time order.
	const std::time_t t0 = 1760400000;   // fixed instant
	CHECK(WriteText(target, "v1"));
	CHECK(BackupBeforeSave(target, o, &made, &err, t0));
	CHECK(made.find("chr000.pat.") != std::string::npos && made.size() > 7 && made.substr(made.size() - 7) == "-00.bak");
	CHECK(ReadAll(made) == "v1");
	CHECK(WriteText(target, "v2"));
	CHECK(BackupBeforeSave(target, o, &made, &err, t0));
	CHECK(made.substr(made.size() - 7) == "-01.bak");
	CHECK(BackupFolderFor(target, o) == dir + "\\backup");

	// Rotation keeps the newest `keep`.
	for (int i = 3; i <= 6; i++) {
		CHECK(WriteText(target, "v" + std::to_string(i)));
		CHECK(BackupBeforeSave(target, o, &made, &err, t0 + i));
	}
	auto all = ListBackups(target, o);
	CHECK(all.size() == 3);
	if (all.size() == 3) {
		CHECK(ReadAll(all[0]) == "v4");
		CHECK(ReadAll(all[1]) == "v5");
		CHECK(ReadAll(all[2]) == "v6");
	}
	// Another file's backups in the same folder are not touched, and similar names are not mistaken for backups.
	const std::string other = dir + "\\chr000.pat2";
	CHECK(WriteText(other, "o1"));
	CHECK(BackupBeforeSave(other, o, &made, &err, t0));
	CHECK(WriteText(dir + "\\backup\\chr000.pat.notes.bak", "x"));
	CHECK(ListBackups(target, o).size() == 3);
	CHECK(ListBackups(other, o).size() == 1);

	// keep = 0 keeps everything.
	BackupOptions all0 = o; all0.keep = 0;
	for (int i = 10; i < 14; i++) CHECK(BackupBeforeSave(target, all0, &made, &err, t0 + i));
	CHECK(ListBackups(target, all0).size() == 7);

	// Disabled: nothing written.
	BackupOptions off = o; off.enabled = false;
	CHECK(BackupBeforeSave(target, off, &made, &err, t0 + 100) && made.empty());
	CHECK(ListBackups(target, all0).size() == 7);

	// Relative and absolute folders.
	BackupOptions rel = o; rel.folder = "old\\saves\\";
	CHECK(BackupFolderFor(target, rel) == dir + "\\old\\saves");
	CHECK(BackupBeforeSave(target, rel, &made, &err, t0));
	CHECK(made.find(dir + "\\old\\saves\\chr000.pat.") == 0);
	BackupOptions abs = o; abs.folder = dir + "\\abs";
	CHECK(BackupBeforeSave(target, abs, &made, &err, t0));
	CHECK(ListBackups(target, abs).size() == 1);

	RemoveTree(dir);
}

// Headless Parts must never be destroyed: ~Vao calls glDeleteBuffers with no GL context (pat_roundtrip.cpp does the
// same with _exit). Every model is leaked on purpose.
static Parts& NewParts() { return *new Parts(nullptr); }

// A small UNI-style parts model built in memory.
static void BuildModel(Parts& p)
{
	p.initEmpty();
	auto& ps = p.partSets[0];
	ps.name = "set0";
	ps.groups[0].ppId = 0;
	ps.groups[0].x = 10;
	ps.groups[0].y = 20;
	ps.groups[0].bgra[3] = 255;
	p.cutOuts[0].name = "cut0";
	p.cutOuts[0].uv[2] = 64;
	p.cutOuts[0].uv[3] = 32;
	p.cutOuts[0].wh[0] = 64;
	p.cutOuts[0].wh[1] = 32;
	p.shapes[0].name = "plane";
}

static void TestAtomicSave()
{
	const std::string dir = TempDir("atomic");
	const std::string target = dir + "\\test.pat";
	Parts& p = NewParts();
	BuildModel(p);
	CHECK(p.Save(target.c_str()));
	const std::string first = ReadAll(target);
	CHECK(first.size() > 0x24 && memcmp(first.data(), "PAniDataFile", 12) == 0);
	CHECK(CountFiles(dir) == 1);   // no temp file left behind

	// Reload: the model survives the round trip.
	Parts& q = NewParts();
	CHECK(q.Load(target.c_str()));
	CHECK(q.partSets.size() >= 1 && q.partSets[0].groups.size() == 1 && q.partSets[0].groups[0].x == 10);
	CHECK(q.cutOuts.size() >= 1 && q.cutOuts[0].uv[2] == 64);

	// The target is locked by another program: the save fails, the old bytes stay, no temp file is left.
	p.partSets[0].groups[0].x = 99;
	HANDLE lock = CreateFileA(target.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	CHECK(lock != INVALID_HANDLE_VALUE);
	CHECK(!p.Save(target.c_str()));
	CloseHandle(lock);
	CHECK(ReadAll(target) == first);
	CHECK(CountFiles(dir) == 1);

	// Unlocked: the edit lands.
	CHECK(p.Save(target.c_str()));
	Parts& r = NewParts();
	CHECK(r.Load(target.c_str()) && r.partSets[0].groups[0].x == 99);

	// A folder that does not exist: nothing is created.
	CHECK(!p.Save((dir + "\\missing\\x.pat").c_str()));
	CHECK(GetFileAttributesA((dir + "\\missing").c_str()) == INVALID_FILE_ATTRIBUTES);

	RemoveTree(dir);
}

// The PAT document set-up of CharacterInstance::makePatDocument: an empty pattern list plus the parts channel.
struct PatDoc {
	Parts& parts = NewParts();
	std::vector<Sequence> doc;
	UndoManager undo;
	PartsUndoChannel channel{&parts};
	PatDoc() { undo.attach(&doc); undo.addChannel(&channel); }
	// one UI gesture: edit, then the idle frame that commits it
	template<class F> void edit(F f) { f(parts); undo.markModified(); undo.endFrame(false); }
};

static void TestDirtyAndUndo()
{
	PatDoc& d = *new PatDoc;
	BuildModel(d.parts);
	CHECK(d.undo.ensureBaseline());
	d.undo.endFrame(false);
	CHECK(d.undo.isClean() && !d.undo.canUndo());

	// A UI frame with nothing edited records nothing.
	d.undo.endFrame(false);
	CHECK(!d.undo.canUndo());

	// Move a part: dirty, undoable.
	d.edit([](Parts& p) { p.partSets[0].groups[0].x += 1; });
	CHECK(d.undo.canUndo() && !d.undo.isClean());
	CHECK(d.undo.peekUndo() && d.undo.peekUndo()->changes.empty() && d.undo.peekUndo()->channels.size() == 1);

	// Save point.
	d.undo.markClean();
	CHECK(d.undo.isClean());

	// Edit the UV rectangle and rename a texture in one gesture (mouse held): one step.
	d.parts.cutOuts[0].uv[0] = 16;
	d.undo.endFrame(true);
	d.parts.gfxMeta[0].name = "tex";
	d.undo.endFrame(true);
	d.undo.endFrame(false);
	CHECK(d.undo.undoCount() == 2 && !d.undo.isClean());

	// Undo back to the save point: clean again, values restored.
	CHECK(d.undo.undo() != nullptr);
	CHECK(d.parts.cutOuts[0].uv[0] == 0 && d.parts.gfxMeta[0].name.empty());
	CHECK(d.undo.isClean());
	CHECK(d.parts.partSets[0].groups[0].x == 11);
	CHECK(d.undo.undo() != nullptr);
	CHECK(d.parts.partSets[0].groups[0].x == 10 && !d.undo.isClean());
	CHECK(d.undo.redo() != nullptr && d.parts.partSets[0].groups[0].x == 11 && d.undo.isClean());

	// Structural edits: add a part set and a prop, delete a cut-out.
	const size_t sets = d.parts.partSets.size();
	d.edit([](Parts& p) { auto& s = p.partSets.emplace_back(); s.partId = (int)p.partSets.size() - 1; s.groups.emplace_back(); });
	d.edit([](Parts& p) { p.partSets[0].groups.emplace_back().propId = 1; });
	d.edit([](Parts& p) { p.cutOuts[0] = {}; });
	CHECK(d.parts.partSets.size() == sets + 1 && d.parts.partSets[0].groups.size() == 2 && d.parts.cutOuts[0].wh[0] == 0);
	CHECK(d.undo.undo() && d.parts.cutOuts[0].wh[0] == 64);
	CHECK(d.undo.undo() && d.parts.partSets[0].groups.size() == 1);
	CHECK(d.undo.undo() && d.parts.partSets.size() == sets);
	CHECK(d.undo.isClean());

	// A new edit after undo drops the redo branch.
	d.edit([](Parts& p) { p.shapes[0].radius = 5; });
	CHECK(!d.undo.canRedo() && !d.undo.isClean());

	// Reload / New PAT: reset() forgets the history; the first state after it is the new baseline (no edit).
	d.undo.reset();
	BuildModel(d.parts);
	d.undo.ensureBaseline();
	d.undo.endFrame(false);
	CHECK(!d.undo.canUndo() && d.undo.isClean());

	// Inactive channel (no PAT loaded): nothing is recorded.
	d.parts.loaded = false;
	d.parts.partSets[0].groups[0].x = 500;
	d.undo.markModified();
	d.undo.endFrame(false);
	CHECK(!d.undo.canUndo());
}

// A DDS file in memory: 32-bit A8R8G8B8 rows, or one DXT1 block per 4x4.
static std::string MakeDds(int w, int h, bool dxt1, const std::vector<uint8_t>& payload)
{
	uint32_t hd[31]{};
	hd[0] = 124; hd[1] = 0x100F; hd[2] = (uint32_t)h; hd[3] = (uint32_t)w;
	hd[18] = 32;                      // pixel format size
	if (dxt1) { hd[19] = 4; memcpy(&hd[20], "DXT1", 4); }
	else { hd[19] = 0x41; hd[21] = 32; hd[22] = 0x00FF0000; hd[23] = 0x0000FF00; hd[24] = 0x000000FF; hd[25] = 0xFF000000; }
	std::string out("DDS ", 4);
	out.append((const char*)hd, sizeof hd);
	out.append((const char*)payload.data(), payload.size());
	return out;
}

static void TestDds()
{
	const std::string dir = TempDir("dds");
	std::vector<Texture*> textures;

	// MBAACC: a DXT1 DDS becomes PGTX (decoded 32-bit BGRA), and Save writes PGTX, never PGT2.
	Parts& p = NewParts();
	BuildModel(p);
	p.useMBAACCFormat = true;
	p.shapes.clear();
	// one DXT1 block: colour0 = pure red (0xF800), colour1 = black, all indices 0 -> red
	std::vector<uint8_t> blk{0x00, 0xF8, 0x00, 0x00, 0, 0, 0, 0};
	CHECK(WriteText(dir + "\\red.dds", MakeDds(4, 4, true, blk)));
	auto& g = p.gfxMeta[0];
	CHECK(g.ImportTexture((dir + "\\red.dds").c_str(), textures, true).empty());
	CHECK(g.data != nullptr && g.imageSize == 0 && g.w == 4 && g.h == 4 && g.bpp == 32);
	if (g.data) CHECK((uint8_t)g.data[0] == 0 && (uint8_t)g.data[1] == 0 && (uint8_t)g.data[2] == 255 && (uint8_t)g.data[3] == 255);
	CHECK(p.Save((dir + "\\mbaacc.pat").c_str()));
	const std::string saved = ReadAll(dir + "\\mbaacc.pat");
	CHECK(saved.find("PGTX") != std::string::npos && saved.find("PGT2") == std::string::npos);
	CHECK(saved.find("VEST") == std::string::npos);

	// MBAACC export: a real 32-bit DDS (sosfiro and the old code wrote an empty one) that imports back identical.
	CHECK(g.ExportTexture((dir + "\\out.dds").c_str()).empty());
	const std::string dds = ReadAll(dir + "\\out.dds");
	CHECK(dds.size() == 128 + 4 * 4 * 4);
	std::vector<uint8_t> px; int w = 0, h = 0;
	CHECK(PartGfx<>::DecodeDds(dds.data(), dds.size(), px, w, h).empty() && w == 4 && h == 4);
	CHECK(px.size() == 64 && g.data && memcmp(px.data(), g.data, 64) == 0);

	// UNI: a non-square 32-bit DDS keeps its width and height (they were read swapped).
	std::vector<uint8_t> rgba((size_t)512 * 256 * 4, 0x80);
	CHECK(WriteText(dir + "\\wide.dds", MakeDds(512, 256, false, rgba)));
	PartGfx<> u;
	CHECK(u.ImportTexture((dir + "\\wide.dds").c_str(), textures, false).empty());
	CHECK(u.w == 512 && u.h == 256 && u.type == 21 && u.imageSize == 512 * 256 * 4 && u.data == nullptr);
	// ... and a UNI size that is not a multiple of 256 is refused.
	CHECK(!u.ImportTexture((dir + "\\red.dds").c_str(), textures, false).empty());
	// An empty slot has nothing to export.
	PartGfx<> empty;
	CHECK(!empty.ExportTexture((dir + "\\none.dds").c_str()).empty());

	RemoveTree(dir);
}

static void TestRealFile(const char* path)
{
	Parts& p = NewParts();
	if (!p.Load(path)) { std::fprintf(stderr, "skip (cannot load) %s\n", path); return; }
	const std::string original = ReadAll(path);
	const std::string dir = TempDir("real");
	const std::string out = dir + "\\copy.pat";

	// Unchanged model: byte-identical through the atomic writer.
	CHECK(p.Save(out.c_str()));
	CHECK(ReadAll(out) == original);

	// Edit through the undo channel, save, reload, undo restores the loaded bytes on the next save.
	PatDoc& d = *new PatDoc;
	CHECK(d.parts.Load(path));
	d.undo.ensureBaseline();
	d.undo.endFrame(false);
	int set = -1;
	for (size_t i = 0; i < d.parts.partSets.size(); i++) if (!d.parts.partSets[i].groups.empty()) { set = (int)i; break; }
	CHECK(set >= 0);
	if (set >= 0) {
		d.edit([set](Parts& q) { q.partSets[set].groups[0].x += 3; });
		CHECK(d.parts.Save(out.c_str()));
		Parts& r = NewParts();
		CHECK(r.Load(out.c_str()) && r.partSets[set].groups[0].x == d.parts.partSets[set].groups[0].x);
		CHECK(d.undo.undo() != nullptr);
		CHECK(d.parts.Save(out.c_str()));
		CHECK(ReadAll(out) == original);
	}
	RemoveTree(dir);
	std::printf("real file ok: %s\n", path);
}

int main(int argc, char** argv)
{
	setvbuf(stdout, nullptr, _IONBF, 0);
	std::printf("backups\n");
	TestBackups();
	std::printf("atomic\n");
	TestAtomicSave();
	std::printf("undo\n");
	TestDirtyAndUndo();
	std::printf("dds\n");
	TestDds();
	for (int i = 1; i < argc; i++) TestRealFile(argv[i]);
	if (g_failures) {
		std::fprintf(stderr, "pat_safety_test: %d failure(s)\n", g_failures);
		std::fflush(stderr);
		_exit(1);
	}
	std::printf("PAT_SAFETY_TEST_PASS\n");
	std::fflush(stdout);
	_exit(0);   // skip static/heap teardown of GL objects (see NewParts)
}
