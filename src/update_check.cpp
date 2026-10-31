#include "update_check.h"

#include "../third_party/json/json.hpp"

namespace updcheck {

bool ParseVersion(const std::string &tag, std::vector<int> &out)
{
	out.clear();
	size_t i = 0;
	if (i < tag.size() && (tag[i] == 'v' || tag[i] == 'V')) i++;
	if (i >= tag.size()) return false;
	while (i < tag.size()) {
		if (tag[i] < '0' || tag[i] > '9') { out.clear(); return false; }
		long n = 0;
		while (i < tag.size() && tag[i] >= '0' && tag[i] <= '9') {
			n = n * 10 + (tag[i] - '0');
			if (n > 1000000) { out.clear(); return false; }
			i++;
		}
		out.push_back((int)n);
		if (i == tag.size()) break;
		if (tag[i] != '.' || i + 1 == tag.size()) { out.clear(); return false; }
		i++;
	}
	return !out.empty();
}

int CompareVersions(const std::string &a, const std::string &b)
{
	std::vector<int> va, vb;
	const bool okA = ParseVersion(a, va), okB = ParseVersion(b, vb);
	if (!okA || !okB) return (int)okA - (int)okB;
	const size_t n = va.size() > vb.size() ? va.size() : vb.size();
	for (size_t k = 0; k < n; k++) {
		const int x = k < va.size() ? va[k] : 0, y = k < vb.size() ? vb[k] : 0;
		if (x != y) return x < y ? -1 : 1;
	}
	return 0;
}

bool SameVersion(const std::string &a, const std::string &b)
{
	std::vector<int> v;
	return ParseVersion(a, v) && ParseVersion(b, v) && CompareVersions(a, b) == 0;
}

bool IsDevBuild(const std::string &local)
{
	std::vector<int> v;
	return !ParseVersion(local, v);
}

bool ParseReleaseJson(const std::string &json, Release &out, std::string *err)
{
	auto fail = [&](const char *why) { if (err) *err = why; return false; };
	const nlohmann::json j = nlohmann::json::parse(json, nullptr, false);
	if (j.is_discarded() || !j.is_object()) return fail("response is not a JSON object");
	auto str = [&](const char *key) {
		auto it = j.find(key);
		return it != j.end() && it->is_string() ? it->get<std::string>() : std::string();
	};
	Release r;
	r.tag = str("tag_name");
	if (r.tag.empty()) return fail("release has no tag_name");
	std::vector<int> v;
	if (!ParseVersion(r.tag, v)) return fail("release tag is not a version");
	r.name = str("name");
	if (r.name.empty()) r.name = r.tag;
	r.body = str("body");
	r.htmlUrl = str("html_url");
	out = std::move(r);
	return true;
}

bool UpdateAvailable(const std::string &local, const std::string &latest, const std::string &skipped)
{
	if (IsDevBuild(local) || IsDevBuild(latest)) return false;
	if (CompareVersions(latest, local) <= 0) return false;
	return !SameVersion(latest, skipped);
}

bool PopupDue(const std::string &local, const std::string &latest, const std::string &skipped, const std::string &notified)
{
	return UpdateAvailable(local, latest, skipped) && !SameVersion(latest, notified);
}

bool AutoCheckDue(bool enabled, int64_t nowUnix, int64_t lastCheckUnix)
{
	if (!enabled) return false;
	if (lastCheckUnix <= 0 || lastCheckUnix > nowUnix) return true;
	return nowUnix - lastCheckUnix >= kAutoCheckInterval;
}

} // namespace updcheck
