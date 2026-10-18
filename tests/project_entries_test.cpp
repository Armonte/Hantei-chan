// .hproj character entries: an HA6 character with its .pat, a standalone PAT tab, a PAT whose file
// went missing, an unsaved New PAT and a pre-PAT project entry, written to disk and read back.
#include "project_entries.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

static int g_failures = 0;
#define CHECK(expr) do { if (!(expr)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); ++g_failures; } } while (0)

using json = nlohmann::json;
using projentry::LoadPlan;
namespace fs = std::filesystem;

static void Touch(const fs::path& p) { std::ofstream(p, std::ios::binary) << "x"; }
static bool Same(const std::string& a, const fs::path& b) { return fs::weakly_canonical(a) == fs::weakly_canonical(b); }

int main()
{
	const fs::path dir = fs::temp_directory_path() / "hc_project_entries_test";
	std::error_code ec;
	fs::remove_all(dir, ec);
	fs::create_directories(dir / "chr");
	const fs::path ha6 = dir / "chr" / "akiha.ha6", ha6Pat = dir / "chr" / "akiha.pat";
	const fs::path pat = dir / "chr" / "effect.pat", gone = dir / "chr" / "gone.pat";
	Touch(ha6); Touch(ha6Pat); Touch(pat); Touch(gone);
	const std::string proj = (dir / "test.hproj").string();

	projentry::CharacterInfo h; h.name = "akiha"; h.ha6Path = ha6.string(); h.patPath = ha6Pat.string(); h.patMbaacc = true;
	projentry::CharacterInfo p; p.name = "effect.pat"; p.patDocument = true; p.patPath = pat.string(); p.patMbaacc = false;
	projentry::CharacterInfo m; m.name = "gone.pat"; m.patDocument = true; m.patPath = gone.string(); m.patMbaacc = true;
	projentry::CharacterInfo n; n.name = "New PAT"; n.patDocument = true;   // never saved

	json chars = json::array();
	for (auto* c : {&h, &p, &m, &n}) {
		json e = projentry::MakeCharacterEntry(*c, proj);
		if (!e.is_null()) chars.push_back(e);
	}
	CHECK(chars.size() == 3);   // the unsaved New PAT has nothing to reopen
	CHECK(chars[0]["type"] == "ha6" && chars[0]["pat_format"] == "mbaacc");
	CHECK(chars[1]["type"] == "pat" && chars[1]["pat_format"] == "uni");
	CHECK(chars[1]["path"].get<std::string>().find(dir.string()) == std::string::npos);   // relative to the project
	{
		json j; j["version"] = "2.0"; j["characters"] = chars;
		std::ofstream(proj) << j.dump(2);
	}
	fs::remove(gone);   // deleted after the project was saved

	json j; std::ifstream(proj) >> j;
	const json& back = j["characters"];
	CHECK(back.size() == 3);

	LoadPlan a = projentry::PlanCharacterLoad(back[0], proj);
	CHECK(a.kind == LoadPlan::Kind::Ha6 && Same(a.path, ha6) && Same(a.patPath, ha6Pat));
	CHECK(a.hasPatFormat && a.patMbaacc);

	LoadPlan b = projentry::PlanCharacterLoad(back[1], proj);
	CHECK(b.kind == LoadPlan::Kind::Pat && Same(b.path, pat) && b.patPath.empty());
	CHECK(b.hasPatFormat && !b.patMbaacc);
	CHECK(projentry::DescribeFailed(back[1], b.path) == b.path);   // exists: no "(file not found)"

	LoadPlan c = projentry::PlanCharacterLoad(back[2], proj);
	CHECK(c.kind == LoadPlan::Kind::Pat && c.hasPatFormat && c.patMbaacc);
	const std::string what = projentry::DescribeFailed(back[2], c.path);
	CHECK(what.size() > 17 && what.compare(what.size() - 17, 17, " (file not found)") == 0);

	// Projects from before PAT support: no pat_path / pat_format, and older entry shapes.
	json old = json::parse(R"({"name":"ryougi","type":"txt","path":"chr/ryougi.txt","render_x":0})");
	LoadPlan o = projentry::PlanCharacterLoad(old, proj);
	CHECK(o.kind == LoadPlan::Kind::Txt && o.patPath.empty() && !o.hasPatFormat);
	CHECK(Same(o.path, dir / "chr" / "ryougi.txt"));
	CHECK(projentry::PlanCharacterLoad(json::parse(R"({"type":"zzz","path":"a"})"), proj).kind == LoadPlan::Kind::None);
	CHECK(projentry::PlanCharacterLoad(json::parse(R"({"type":"pat"})"), proj).kind == LoadPlan::Kind::None);
	CHECK(projentry::DescribeFailed(json::parse(R"({"name":"x"})"), "") == "x");
	// A bogus format string is ignored rather than guessed.
	CHECK(!projentry::PlanCharacterLoad(json::parse(R"({"type":"pat","path":"a.pat","pat_format":"ps2"})"), proj).hasPatFormat);

	fs::remove_all(dir, ec);
	if (g_failures) { std::printf("%d failure(s)\n", g_failures); return 1; }
	std::printf("project_entries: ok\n");
	return 0;
}
