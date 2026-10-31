#include "update_notice.h"
#include "update_check.h"
#include "app_log.h"
#include "i18n.h"
#include "png_writer.h"   // Utf8ToWide

#include <windows.h>
#include <shellapi.h>
#include <winhttp.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#ifndef HC_RELEASE_TAG
#define HC_RELEASE_TAG "dev"
#endif
#define HC_WIDEN_(x) L##x
#define HC_WIDEN(x) HC_WIDEN_(x)

namespace updnotice {
namespace {

const char *const kLatestUrl = "https://api.github.com/repos/Armonte/Hantei-chan/releases/latest";
const char *const kReleasesPage = "https://github.com/Armonte/Hantei-chan/releases";
constexpr DWORD kTimeoutMs = 5000;

// [Update check][Data] in the editor ini. Written by the UI thread only.
struct Persisted {
	bool autoCheck = true;
	long long lastCheck = 0;   // unix seconds of the last automatic check
	std::string skippedTag;    // "Skip this version"
	std::string notifiedTag;   // last release the popup was shown for
	std::string latestTag;     // last release seen, so the badge survives launches inside the 24 h window
	std::string latestUrl;
} g_ini;

// One fetch. The worker fills it and sets done under the mutex; the UI thread polls it each frame.
struct Job {
	std::mutex m;
	bool done = false;
	bool ok = false;
	updcheck::Release release;
	std::string error;
};

std::shared_ptr<Job> g_job;
bool g_jobManual = false;
bool g_launchHandled = false;
updcheck::Release g_release;   // the release the popup shows (notes only when fetched this session)
bool g_openPopup = false;

enum class Manual { none, checking, upToDate, devBuild, failed };
Manual g_manual = Manual::none;
std::string g_manualDetail;

std::string Env(const char *name)
{
	const char *v = std::getenv(name);
	return v ? std::string(v) : std::string();
}

bool IsDev() { return updcheck::IsDevBuild(HC_RELEASE_TAG); }

// The version the rules compare against: the stamped tag, or in a dev build the HC_UPDATE_FAKE_LOCAL override.
std::string EffectiveLocal()
{
	if (IsDev()) {
		static const std::string fake = Env("HC_UPDATE_FAKE_LOCAL");
		if (!fake.empty()) return fake;
	}
	return HC_RELEASE_TAG;
}

std::string SourceUrl()
{
	if (IsDev()) {
		static const std::string url = Env("HC_UPDATE_URL");
		if (!url.empty()) return url;
	}
	return kLatestUrl;
}

std::string LastErrorText(const char *what)
{
	char buf[96];
	snprintf(buf, sizeof buf, "%s failed (error %lu)", what, (unsigned long)GetLastError());
	return buf;
}

// GET url -> body. Errors as short English text (logged; shown after a manual check).
bool HttpGet(const std::string &url, std::string &body, std::string &err)
{
	std::wstring wurl = Utf8ToWide(url);
	URL_COMPONENTS uc{};
	uc.dwStructSize = sizeof uc;
	uc.dwHostNameLength = (DWORD)-1;
	uc.dwUrlPathLength = (DWORD)-1;
	uc.dwExtraInfoLength = (DWORD)-1;
	if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc)) { err = "bad URL " + url; return false; }
	const std::wstring host(uc.lpszHostName, uc.dwHostNameLength);
	std::wstring path(uc.lpszUrlPath, uc.dwUrlPathLength);
	if (uc.lpszExtraInfo) path.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);

	HINTERNET session = WinHttpOpen(L"Hantei-chan/" HC_WIDEN(HC_RELEASE_TAG), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
		WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!session) { err = LastErrorText("WinHttpOpen"); return false; }
	WinHttpSetTimeouts(session, kTimeoutMs, kTimeoutMs, kTimeoutMs, kTimeoutMs);
	HINTERNET connect = nullptr, request = nullptr;
	bool ok = false;
	do {
		connect = WinHttpConnect(session, host.c_str(), uc.nPort, 0);
		if (!connect) { err = LastErrorText("connect"); break; }
		request = WinHttpOpenRequest(connect, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
			uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
		if (!request) { err = LastErrorText("open request"); break; }
		const wchar_t *headers = L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n";
		if (!WinHttpSendRequest(request, headers, (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) { err = LastErrorText("send"); break; }
		if (!WinHttpReceiveResponse(request, nullptr)) { err = LastErrorText("receive"); break; }
		DWORD status = 0, size = sizeof status;
		WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
			&status, &size, WINHTTP_NO_HEADER_INDEX);
		body.clear();
		for (;;) {
			DWORD avail = 0;
			if (!WinHttpQueryDataAvailable(request, &avail)) { err = LastErrorText("read"); break; }
			if (avail == 0) { ok = true; break; }
			if (body.size() + avail > (4u << 20)) { err = "response too large"; break; }
			const size_t at = body.size();
			body.resize(at + avail);
			DWORD got = 0;
			if (!WinHttpReadData(request, &body[at], avail, &got)) { err = LastErrorText("read"); break; }
			body.resize(at + got);
		}
		if (ok && status != 200) {
			ok = false;
			err = "HTTP " + std::to_string(status) + (status == 403 ? " (GitHub API rate limit?)" : status == 404 ? " (no release)" : "");
		}
	} while (false);
	if (request) WinHttpCloseHandle(request);
	if (connect) WinHttpCloseHandle(connect);
	WinHttpCloseHandle(session);
	return ok;
}

void StartFetch(bool manual)
{
	if (g_job) return;   // one at a time; a manual check during the launch check takes over its result
	auto job = std::make_shared<Job>();
	g_job = job;
	g_jobManual = manual;
	const std::string url = SourceUrl();
	HC_LOG_INFO("update check: {} ({}, local {})", url, manual ? "manual" : "launch", EffectiveLocal());
	std::thread([job, url]() {
		std::string body, err;
		updcheck::Release rel;
		bool ok = HttpGet(url, body, err) && updcheck::ParseReleaseJson(body, rel, &err);
		std::lock_guard<std::mutex> lock(job->m);
		job->ok = ok;
		job->release = std::move(rel);
		job->error = std::move(err);
		job->done = true;
	}).detach();
}

void OpenUrl(const std::string &url)
{
	// Only a web page: the URL comes from the network.
	const std::string target = url.rfind("https://", 0) == 0 ? url : std::string(kReleasesPage);
	ShellExecuteW(nullptr, L"open", Utf8ToWide(target).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void Poll()
{
	if (!g_job) return;
	bool ok;
	updcheck::Release rel;
	std::string error;
	{
		std::lock_guard<std::mutex> lock(g_job->m);
		if (!g_job->done) return;
		ok = g_job->ok;
		rel = std::move(g_job->release);
		error = std::move(g_job->error);
	}
	g_job.reset();
	const std::string local = EffectiveLocal();
	if (!ok) {
		HC_LOG_WARN("update check failed: {}", error);
		if (g_jobManual) { g_manual = Manual::failed; g_manualDetail = error; }
		return;
	}
	HC_LOG_INFO("update check: latest release {} ({}), local {}", rel.tag, rel.htmlUrl, local);
	g_release = rel;
	if (g_ini.latestTag != rel.tag || g_ini.latestUrl != rel.htmlUrl) {
		g_ini.latestTag = rel.tag;
		g_ini.latestUrl = rel.htmlUrl;
		ImGui::MarkIniSettingsDirty();
	}
	if (g_jobManual) {
		if (updcheck::IsDevBuild(local)) { g_manual = Manual::devBuild; g_manualDetail = rel.tag; }
		else if (updcheck::CompareVersions(rel.tag, local) > 0) { g_manual = Manual::none; g_openPopup = true; }   // even a skipped one
		else { g_manual = Manual::upToDate; g_manualDetail = local; }
		return;
	}
	if (updcheck::PopupDue(local, rel.tag, g_ini.skippedTag, g_ini.notifiedTag)) {
		g_openPopup = true;
		g_ini.notifiedTag = rel.tag;
		ImGui::MarkIniSettingsDirty();
	}
}

void StartLaunchCheck()
{
	g_launchHandled = true;
	const bool fake = IsDev() && !Env("HC_UPDATE_FAKE_LOCAL").empty();
	if (IsDev() && !fake) return;   // dev builds: Help > Check for updates only
	if (!g_ini.autoCheck) return;
	const long long now = (long long)std::time(nullptr);
	if (!fake && !updcheck::AutoCheckDue(true, now, g_ini.lastCheck)) {
		HC_LOG_DEBUG("update check: last one {} s ago, skipped", now - g_ini.lastCheck);
		return;
	}
	if (!fake) {   // stamped before the fetch: an offline launch does not retry on every start
		g_ini.lastCheck = now;
		ImGui::MarkIniSettingsDirty();
	}
	StartFetch(false);
}

void DrawPopup()
{
	const char *title = LBL("Update available");
	if (g_openPopup) { ImGui::OpenPopup(title); g_openPopup = false; }
	const ImGuiViewport *vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(560, 420), ImGuiCond_Appearing);
	if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_NoSavedSettings)) return;
	ImGui::TextUnformatted(TXT("A new version of Hantei-chan is available."));
	ImGui::Text("%s", g_release.name.c_str());
	ImGui::TextDisabled(TXT("Installed: %s    Latest: %s"), EffectiveLocal().c_str(), g_release.tag.c_str());
	ImGui::Separator();
	const float buttons = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
	ImGui::BeginChild("##notes", ImVec2(0, -buttons), true);
	if (g_release.body.empty()) ImGui::TextDisabled("%s", TXT("(no release notes)"));
	else {
		ImGui::PushTextWrapPos(0.f);
		ImGui::TextUnformatted(g_release.body.c_str(), g_release.body.c_str() + g_release.body.size());
		ImGui::PopTextWrapPos();
	}
	ImGui::EndChild();
	if (ImGui::Button(LBL("Open release page"))) {
		OpenUrl(g_release.htmlUrl);
		ImGui::CloseCurrentPopup();
	}
	ImGui::SameLine();
	if (ImGui::Button(LBL("Skip this version"))) {
		g_ini.skippedTag = g_release.tag;
		ImGui::MarkIniSettingsDirty();
		HC_LOG_INFO("update check: skipping {}", g_release.tag);
		ImGui::CloseCurrentPopup();
	}
	ImGui::SameLine();
	if (ImGui::Button(LBL("Later")) || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
	ImGui::EndPopup();
}

void DrawManualResult()
{
	if (g_manual == Manual::none) return;
	bool open = true;
	const ImGuiViewport *vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (ImGui::Begin(LBL("Update check"), &open, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
	                 ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse)) {
		switch (g_manual) {
		case Manual::checking: ImGui::TextUnformatted(TXT("Checking for updates...")); break;
		case Manual::upToDate: ImGui::Text(TXT("Hantei-chan is up to date (%s)."), g_manualDetail.c_str()); break;
		case Manual::devBuild:
			ImGui::Text(TXT("Development build: no update notices. The latest release is %s."), g_manualDetail.c_str());
			if (ImGui::Button(LBL("Open release page"))) OpenUrl(g_release.htmlUrl);
			ImGui::SameLine();
			break;
		case Manual::failed: ImGui::Text(TXT("The update check failed: %s"), g_manualDetail.c_str()); break;
		case Manual::none: break;
		}
		if (g_manual != Manual::checking && ImGui::Button(LBL("OK"))) open = false;
	}
	ImGui::End();
	if (!open) g_manual = Manual::none;
}

void *ReadOpen(ImGuiContext *, ImGuiSettingsHandler *, const char *) { return &g_ini; }

void ReadLine(ImGuiContext *, ImGuiSettingsHandler *, void *, const char *line)
{
	int i = 0;
	long long t = 0;
	if (sscanf(line, "AutoCheck=%d", &i) == 1) g_ini.autoCheck = i != 0;
	else if (sscanf(line, "LastCheck=%lld", &t) == 1) g_ini.lastCheck = t;
	else if (strncmp(line, "SkippedTag=", 11) == 0) g_ini.skippedTag = line + 11;
	else if (strncmp(line, "NotifiedTag=", 12) == 0) g_ini.notifiedTag = line + 12;
	else if (strncmp(line, "LatestTag=", 10) == 0) g_ini.latestTag = line + 10;
	else if (strncmp(line, "LatestUrl=", 10) == 0) g_ini.latestUrl = line + 10;
}

void WriteAll(ImGuiContext *, ImGuiSettingsHandler *handler, ImGuiTextBuffer *buf)
{
	buf->appendf("[%s][Data]\n", handler->TypeName);
	buf->appendf("AutoCheck=%d\n", g_ini.autoCheck ? 1 : 0);
	buf->appendf("LastCheck=%lld\n", g_ini.lastCheck);
	if (!g_ini.skippedTag.empty()) buf->appendf("SkippedTag=%s\n", g_ini.skippedTag.c_str());
	if (!g_ini.notifiedTag.empty()) buf->appendf("NotifiedTag=%s\n", g_ini.notifiedTag.c_str());
	if (!g_ini.latestTag.empty()) buf->appendf("LatestTag=%s\n", g_ini.latestTag.c_str());
	if (!g_ini.latestUrl.empty()) buf->appendf("LatestUrl=%s\n", g_ini.latestUrl.c_str());
	buf->append("\n");
}

} // namespace

void RegisterSettings(ImGuiContext &context)
{
	ImGuiSettingsHandler handler{};
	handler.TypeName = "Update check";
	handler.TypeHash = ImHashStr("Update check");
	handler.ReadOpenFn = ReadOpen;
	handler.ReadLineFn = ReadLine;
	handler.WriteAllFn = WriteAll;
	context.SettingsHandlers.push_back(handler);
}

const char *LocalVersion() { return HC_RELEASE_TAG; }

void DrawMenuItems()
{
	if (ImGui::MenuItem(LBL("Check for updates"), nullptr, false, !g_job || !g_jobManual)) {
		g_manual = Manual::checking;
		if (g_job) g_jobManual = true;   // the launch check is still running: report its result
		else StartFetch(true);
	}
	if (ImGui::MenuItem(LBL("Check for updates on startup"), nullptr, g_ini.autoCheck)) {
		g_ini.autoCheck = !g_ini.autoCheck;
		ImGui::MarkIniSettingsDirty();
	}
}

void DrawMenuBadge()
{
	if (!updcheck::UpdateAvailable(EffectiveLocal(), g_ini.latestTag, g_ini.skippedTag)) return;
	char label[96];
	snprintf(label, sizeof label, TXT("Update %s available"), g_ini.latestTag.c_str());
	const ImGuiStyle &style = ImGui::GetStyle();
	const float w = ImGui::CalcTextSize(label).x + style.ItemSpacing.x * 2.f;
	const float x = ImGui::GetWindowContentRegionMax().x - w;
	ImGui::SameLine();
	if (x > ImGui::GetCursorPosX()) ImGui::SetCursorPosX(x);
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.78f, 0.25f, 1.0f));
	ImGui::PushID("update_badge");
	const bool clicked = ImGui::MenuItem(label);
	ImGui::PopID();
	ImGui::PopStyleColor();
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TXT("Show the release notes"));
	if (clicked) {
		if (!updcheck::SameVersion(g_release.tag, g_ini.latestTag)) {   // seen on an earlier launch: notes not loaded
			g_release = {};
			g_release.tag = g_release.name = g_ini.latestTag;
			g_release.htmlUrl = g_ini.latestUrl;
		}
		g_openPopup = true;
	}
}

void Draw()
{
	if (!g_launchHandled) StartLaunchCheck();
	Poll();
	DrawPopup();
	DrawManualResult();
}

} // namespace updnotice
