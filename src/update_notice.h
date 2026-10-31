#ifndef UPDATE_NOTICE_H_GUARD
#define UPDATE_NOTICE_H_GUARD

// In-app update notice: asks GitHub for the latest stable release (releases/latest skips drafts and pre-releases) on a
// worker thread, at most once a day at launch, and on Help > Check for updates. A newer release than this build shows a
// popup once per release (notes, Open release page / Skip this version / Later) and a badge at the right of the main menu
// bar until the build is updated or the release skipped. Nothing is downloaded; the release page opens in the browser.
// Rules: update_check.h. Settings: the [Update check][Data] section of the editor ini.
//
// Dev builds (no HC_RELEASE_TAG) never check on their own. They honour two test overrides:
//   HC_UPDATE_FAKE_LOCAL=0.0.1   pretend to be that release (auto check runs, ignoring the 24 h throttle)
//   HC_UPDATE_URL=<url>          fetch this instead of the GitHub API (error / offline tests)
#include <string>

struct ImGuiContext;

namespace updnotice {

void RegisterSettings(ImGuiContext &context);   // before the ini is loaded (InitIni)
const char *LocalVersion();                     // release tag of this build, or "dev"

void DrawMenuItems();   // Help menu: Check for updates, automatic check toggle
void DrawMenuBadge();   // last item of the main menu bar
void Draw();            // once per frame, top level: starts the launch check, popup, manual check result
// --ui-script: update check | update later | update skip | update status (state line in *msg). False: not an update command.
bool ScriptCommand(const std::string &cmd, const std::string &arg, std::string *msg);

} // namespace updnotice

#endif /* UPDATE_NOTICE_H_GUARD */
