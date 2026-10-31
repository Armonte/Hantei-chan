#ifndef UPDATE_CHECK_H_GUARD
#define UPDATE_CHECK_H_GUARD

// Update notice, pure part (no Win32 / ImGui): release tags, the GitHub "latest release" JSON, the skip and
// 24-hour throttle rules. The fetch and the popup / menu-bar badge are in update_notice.cpp; tests/update_check_test.cpp.
//
// Versions are the GitHub release tags ("0.1.2", tolerated "v0.1.2"), compared numerically per dotted component, a
// missing component counting as 0 (0.1 == 0.1.0 < 0.1.10). A build only knows its tag when the release build stamps it
// (CMake HC_RELEASE_TAG); every other build is "dev" and never shows the notice on its own.
#include <cstdint>
#include <string>
#include <vector>

namespace updcheck {

struct Release {
	std::string tag;      // tag_name
	std::string name;     // release title ("Hantei-chan 0.1.2"); the tag when empty
	std::string body;     // release notes (markdown, shown as text)
	std::string htmlUrl;  // release page
};

// "0.1.2" / "v0.1.2" -> {0, 1, 2}. False for anything else ("dev", "", "0.1-rc1", "1..2").
bool ParseVersion(const std::string &tag, std::vector<int> &out);
// <0, 0, >0 like strcmp. Both must parse (callers check); an unparsable side compares as older.
int CompareVersions(const std::string &a, const std::string &b);
// Same release, written with or without the "v" and trailing ".0" components.
bool SameVersion(const std::string &a, const std::string &b);
// A build without a release tag ("dev", empty, or not a version).
bool IsDevBuild(const std::string &local);

// GitHub GET /repos/{owner}/{repo}/releases/latest body -> Release. False (with *err) when it is not JSON, has no tag,
// or the tag is not a version.
bool ParseReleaseJson(const std::string &json, Release &out, std::string *err = nullptr);

// Badge: a newer release than the running build that the user did not skip. Dev builds: never.
bool UpdateAvailable(const std::string &local, const std::string &latest, const std::string &skipped);
// Popup: as the badge, once per release (notified = the last tag the popup was shown for).
bool PopupDue(const std::string &local, const std::string &latest, const std::string &skipped, const std::string &notified);

constexpr int64_t kAutoCheckInterval = 24 * 60 * 60;   // seconds
// Automatic check at launch: enabled, and the last one is at least 24 h old (or in the future: clock moved back).
bool AutoCheckDue(bool enabled, int64_t nowUnix, int64_t lastCheckUnix);

} // namespace updcheck

#endif /* UPDATE_CHECK_H_GUARD */
