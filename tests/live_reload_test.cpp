// Unit tests for the editor side of the asset hot-reload seam (src/live_reload.cpp): path validation, request / ack lines, loose-path guessing,
// and the whole file protocol against a fake pchost thread.
#include "live_reload.h"
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

namespace fs = std::filesystem;
static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); g_fail++; } } while (0)

int main()
{
	using namespace livereload;
	std::string why;
	CHECK(ValidRelPath("data/grp/p_font00.img", &why));
	CHECK(!ValidRelPath("", &why));
	CHECK(!ValidRelPath("/abs/x.img", &why));
	CHECK(!ValidRelPath("C:/x.img", &why));
	CHECK(!ValidRelPath("data\\grp\\x.img", &why));
	CHECK(!ValidRelPath("data/../x.img", &why));
	CHECK(!ValidRelPath("data//x.img", &why));
	CHECK(!ValidRelPath("data/x\n.img", &why));
	CHECK(RequestLine("data/grp/a.img") == "reload data/grp/a.img\n");
	Result r;
	CHECK(ParseAck("ok data/grp/a.img", "data/grp/a.img", r) && r.ok && r.status == "ok");
	CHECK(ParseAck("refused data/grp/a.img netplay session attached", "data/grp/a.img", r) && !r.ok && r.status == "refused: netplay session attached");
	CHECK(ParseAck("failed data/grp/a.img no such slot", "data/grp/a.img", r) && !r.ok);
	CHECK(!ParseAck("ok data/grp/b.img", "data/grp/a.img", r));
	{
		const char *s = "junk\0.\\Data\\grp\\P_FONT00.img\0more";
		std::vector<uint8_t> f(s, s + 36);
		CHECK(GuessLoosePath("p_font00.IMG", { f }) == "Data/grp/P_FONT00.img");
		CHECK(GuessLoosePath("other.img", { f }).empty());
	}
	const fs::path dir = fs::temp_directory_path() / "live_reload_test_dir";
	std::error_code ec; fs::remove_all(dir, ec); fs::create_directories(dir, ec);
	// fake pchost: waits for the request, answers ok, deletes the request
	std::thread fake([&] {
		for (int i = 0; i < 100; i++) {
			if (fs::exists(dir / "pchost_reload.req")) {
				std::ifstream f(dir / "pchost_reload.req"); std::string l; std::getline(f, l); f.close();
				fs::remove(dir / "pchost_reload.req");
				std::ofstream a(dir / "pchost_reload.ack"); a << "ok " << l.substr(7) << "\n";
				return;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
		}
	});
	std::vector<uint8_t> bytes = { 1, 2, 3, 4 };
	Result rr = Reload(dir.u8string(), "data/grp/x.img", bytes, 3000);
	fake.join();
	CHECK(rr.ok);
	CHECK(fs::exists(dir / "data" / "grp" / "x.img") && fs::file_size(dir / "data" / "grp" / "x.img") == 4);
	Result bad = Reload(dir.u8string(), "../x.img", bytes, 100);
	CHECK(!bad.ok && bad.status.find("bad path") == 0);
	Result noAnswer = Reload(dir.u8string(), "data/grp/y.img", bytes, 200);
	CHECK(!noAnswer.ok && noAnswer.status.find("no answer") == 0);
	CHECK(!fs::exists(dir / "pchost_reload.req"));
	fs::remove_all(dir, ec);
	if (g_fail) { printf("%d failure(s)\n", g_fail); return 1; }
	puts("live_reload_test: all passed");
	return 0;
}
