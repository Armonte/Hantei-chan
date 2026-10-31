// Standalone tests for the update notice rules (update_check.h). Exit code 0 = pass.
#include "update_check.h"

#include <cstdio>
#include <string>
#include <vector>

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_failures; } } while (0)

using namespace updcheck;

int main()
{
	// Tag parsing: dotted numbers, optional leading v.
	std::vector<int> v;
	CHECK(ParseVersion("0.1.2", v) && v == std::vector<int>({0, 1, 2}));
	CHECK(ParseVersion("v0.1.10", v) && v == std::vector<int>({0, 1, 10}));
	CHECK(ParseVersion("V1", v) && v == std::vector<int>({1}));
	CHECK(!ParseVersion("", v) && v.empty());
	CHECK(!ParseVersion("v", v));
	CHECK(!ParseVersion("dev", v));
	CHECK(!ParseVersion("0.1-rc1", v));
	CHECK(!ParseVersion("1..2", v));
	CHECK(!ParseVersion("1.2.", v));
	CHECK(!ParseVersion(".1", v));
	CHECK(!ParseVersion("99999999999", v));

	// Numeric per component, missing components are 0.
	CHECK(CompareVersions("0.1.10", "0.1.2") > 0);
	CHECK(CompareVersions("0.1.2", "0.1.10") < 0);
	CHECK(CompareVersions("0.1", "0.1.0") == 0);
	CHECK(CompareVersions("v0.1.2", "0.1.2") == 0);
	CHECK(CompareVersions("0.2", "0.1.9") > 0);
	CHECK(CompareVersions("1.0", "0.99.99") > 0);
	CHECK(CompareVersions("dev", "0.1") < 0);
	CHECK(SameVersion("v0.1.2", "0.1.2.0"));
	CHECK(!SameVersion("0.1.2", ""));
	CHECK(!SameVersion("dev", "dev"));

	CHECK(IsDevBuild("dev"));
	CHECK(IsDevBuild(""));
	CHECK(!IsDevBuild("0.1.2"));

	// GitHub releases/latest body.
	Release r;
	std::string err;
	CHECK(ParseReleaseJson(R"({"tag_name":"0.1.2","name":"Hantei-chan 0.1.2","body":"## Fixes\n- a","html_url":"https://github.com/Armonte/Hantei-chan/releases/tag/0.1.2","prerelease":false,"assets":[]})", r, &err));
	CHECK(r.tag == "0.1.2" && r.name == "Hantei-chan 0.1.2" && r.body == "## Fixes\n- a");
	CHECK(r.htmlUrl == "https://github.com/Armonte/Hantei-chan/releases/tag/0.1.2");
	CHECK(ParseReleaseJson(R"({"tag_name":"v0.2","name":null,"body":null})", r) && r.name == "v0.2" && r.body.empty());
	CHECK(!ParseReleaseJson("", r, &err) && !err.empty());
	CHECK(!ParseReleaseJson("<html>rate limited</html>", r, &err));
	CHECK(!ParseReleaseJson(R"({"message":"Not Found"})", r, &err));
	CHECK(!ParseReleaseJson(R"({"tag_name":"nightly"})", r, &err));
	CHECK(!ParseReleaseJson(R"([1,2])", r, &err));

	// Badge: newer and not skipped; dev builds never.
	CHECK(UpdateAvailable("0.1.2", "0.1.3", ""));
	CHECK(UpdateAvailable("0.1.2", "0.1.10", ""));
	CHECK(!UpdateAvailable("0.1.2", "0.1.2", ""));
	CHECK(!UpdateAvailable("0.1.3", "0.1.2", ""));
	CHECK(!UpdateAvailable("dev", "0.1.3", ""));
	CHECK(!UpdateAvailable("0.1.2", "", ""));
	CHECK(!UpdateAvailable("0.1.2", "0.1.3", "0.1.3"));
	CHECK(!UpdateAvailable("0.1.2", "0.1.3", "v0.1.3"));
	CHECK(UpdateAvailable("0.1.2", "0.1.4", "0.1.3"));   // a skip covers that release only

	// Popup: once per release.
	CHECK(PopupDue("0.1.2", "0.1.3", "", ""));
	CHECK(PopupDue("0.1.2", "0.1.4", "", "0.1.3"));
	CHECK(!PopupDue("0.1.2", "0.1.3", "", "0.1.3"));
	CHECK(!PopupDue("0.1.2", "0.1.3", "0.1.3", ""));
	CHECK(!PopupDue("dev", "0.1.3", "", ""));

	// 24 h throttle.
	const int64_t now = 1793318400;   // 2026-10-30
	CHECK(AutoCheckDue(true, now, 0));
	CHECK(AutoCheckDue(true, now, now - kAutoCheckInterval));
	CHECK(!AutoCheckDue(true, now, now - kAutoCheckInterval + 1));
	CHECK(!AutoCheckDue(true, now, now));
	CHECK(AutoCheckDue(true, now, now + 3600));   // clock moved back
	CHECK(!AutoCheckDue(false, now, 0));

	if (g_failures) { std::fprintf(stderr, "%d failure(s)\n", g_failures); return 1; }
	std::printf("update_check: all passed\n");
	return 0;
}
