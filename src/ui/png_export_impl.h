#ifndef UI_PNG_EXPORT_IMPL_H_GUARD
#define UI_PNG_EXPORT_IMPL_H_GUARD

// ============================================================================
// PNG export (docs/HANTEI_WAVE2.md §3)
// ============================================================================
// Included from main_frame.cpp.
//
// Renders a view off-screen into a private RenderTarget - never the window's
// back buffer, so pan/zoom/window size and ImGui do not matter - for:
//   * the current frame (what the view shows now),
//   * a tick range, or
//   * the whole pattern (tick 0 until the simulation settles),
// with spawned actors and hitboxes optional, at an integer scale.
//
// Ticks come from the view's cached preview simulator (getStateAt), so the
// export matches the viewport and costs one simulated tick per exported tick.
//
// Transparent output uses a two-background matte: every tick is rendered over
// black and over white, alpha = 1 - (white - black) and colour = black / alpha.
// That recovers straight alpha for normal blending exactly and gives additive
// / subtractive layers (which have no single "correct" alpha) a consistent
// screen-style approximation, without changing any blend state in the
// renderer.

#include "../png_writer.h"
#include <cstring>

namespace pngexport {

std::string SanitizeFileComponent(std::string value)
{
	for (char& ch : value) {
		if (ch == '<' || ch == '>' || ch == ':' || ch == '"' || ch == '/' || ch == '\\' ||
		    ch == '|' || ch == '?' || ch == '*' || (unsigned char)ch < 0x20)
			ch = '_';
	}
	while (!value.empty() && (value.back() == ' ' || value.back() == '.')) value.pop_back();
	while (!value.empty() && value.front() == ' ') value.erase(value.begin());
	return value.empty() ? std::string("export") : value;
}

struct Bounds {
	int x0 = INT_MAX, y0 = INT_MAX, x1 = INT_MIN, y1 = INT_MIN;  // inclusive pixels
	bool empty() const { return x1 < x0 || y1 < y0; }
	void add(int x, int y) { x0 = std::min(x0, x); y0 = std::min(y0, y); x1 = std::max(x1, x); y1 = std::max(y1, y); }
	void add(const Bounds& b) { if (!b.empty()) { add(b.x0, b.y0); add(b.x1, b.y1); } }
};

Bounds AlphaBounds(const std::vector<uint8_t>& rgba, int w, int h)
{
	Bounds b;
	for (int y = 0; y < h; ++y) {
		const uint8_t* row = rgba.data() + (size_t)y * w * 4;
		for (int x = 0; x < w; ++x)
			if (row[x * 4 + 3] != 0) b.add(x, y);
	}
	return b;
}

// Straight-alpha RGBA from renders over black and white.
void MatteFromBlackWhite(const std::vector<uint8_t>& black, const std::vector<uint8_t>& white,
	std::vector<uint8_t>& out)
{
	out.resize(black.size());
	for (size_t i = 0; i < black.size(); i += 4) {
		int a = 0;
		for (int c = 0; c < 3; ++c) {
			const int diff = std::clamp((int)white[i + c] - (int)black[i + c], 0, 255);
			a = std::max(a, 255 - diff);
		}
		out[i + 3] = (uint8_t)a;
		for (int c = 0; c < 3; ++c)
			out[i + c] = a ? (uint8_t)std::min(255, ((int)black[i + c] * 255 + a / 2) / a) : 0;
	}
}

bool IsFullyTransparent(const std::vector<uint8_t>& rgba)
{
	for (size_t i = 3; i < rgba.size(); i += 4)
		if (rgba[i]) return false;
	return true;
}

} // namespace pngexport

// Ticks the export covers, in order.
std::vector<int> MainFrame::ExportTicks(CharacterView* view, const ExportSettings& s, bool& liveFrame)
{
	std::vector<int> ticks;
	liveFrame = false;
	auto& st = view->getState();
	CharacterInstance* c = view->getCharacter();
	FrameData* effectData = c->effectCharacter ? &c->effectCharacter->frameData : nullptr;
	auto& sim = st.BindPreviewSim(&c->frameData, effectData);
	if (s.range == ExportSettings::currentFrame) {
		liveFrame = true;           // exactly what the view shows (selected frame)
		ticks.push_back(st.currentTick);
		return ticks;
	}
	int first = 0, last = 0;
	if (s.range == ExportSettings::tickRange) {
		first = std::max(0, s.fromTick);
		last = std::max(first, s.toTick);
	} else {
		const int settled = sim.settledTick();
		const int rootEnd = sim.rootEndTick();
		last = settled >= 0 ? settled : (rootEnd >= 0 ? rootEnd : sim.horizon());
	}
	last = std::min({last, sim.horizon(), first + std::max(1, s.maxTicks) - 1});
	sim.ensureSimulatedTo(last);
	const auto& track = sim.rootFrameTrack();
	const int step = std::max(1, s.step);
	for (int t = first; t <= last; t += step) {
		if (s.keyframesOnly && t > first && t < (int)track.size() && t - 1 < (int)track.size() &&
		    track[t] == track[t - 1])
			continue;
		ticks.push_back(t);
	}
	return ticks;
}

bool MainFrame::RunPngExport(CharacterView* view, const ExportSettings& s, ExportResult& result)
{
	result = ExportResult{};
	CharacterInstance* c = view ? view->getCharacter() : nullptr;
	if (!c || view->isStageView()) { result.message = "Export needs a character tab."; return false; }
	Sequence* seq = c->frameData.get_sequence(view->getState().pattern);
	if (!seq || seq->frames.empty()) { result.message = "The pattern has no frames."; return false; }

	const auto t0 = std::chrono::steady_clock::now();
	std::string error;
	if (!CreateDirectoriesUtf8(s.folder, error)) { result.message = error; return false; }

	bool liveFrame = false;
	const std::vector<int> ticks = ExportTicks(view, s, liveFrame);
	if (ticks.empty()) { result.message = "No ticks to export."; return false; }

	// Keep the renderer's filter choice for the export only.
	struct FilterScope { Render& r; bool prev; ~FilterScope() { r.filter = prev; } } filterScope{render, render.filter};
	render.filter = s.smooth;

	SceneOptions scene;
	scene.grid = false;
	scene.boxes = s.boxes;
	scene.spawns = s.spawns ? 1 : 0;
	scene.onion = false;
	auto sceneFor = [&](int tick) { SceneOptions o = scene; o.tick = liveFrame ? -1 : tick; return o; };

	const int scale = std::clamp(s.scale, 1, 8);
	std::vector<uint8_t> pixels, black, white, out;

	// Analysis pass (scale 1): alpha bounds of every tick, in world units.
	constexpr int A = 2048;
	constexpr float AOX = A / 2.f, AOY = A * 0.75f;
	std::vector<pngexport::Bounds> tickBounds(ticks.size());
	pngexport::Bounds unionBounds;
	if (s.crop != ExportSettings::fixedCanvas) {
		RenderTarget analysis;
		if (!analysis.ensure(A, A, false)) { result.message = "Could not create the analysis framebuffer."; return false; }
		Render::PassParams pass;
		pass.width = pass.height = A;
		pass.originX = AOX; pass.originY = AOY; pass.zoom = 1.f;
		for (size_t i = 0; i < ticks.size(); ++i) {
			{
				ScopedTargetBinding bind(analysis);
				bind.clear(0.f, 0.f, 0.f, 0.f, false);
				DrawCharacterScene(view, pass, sceneFor(ticks[i]));
			}
			if (!analysis.readRgba(pixels)) { result.message = "Could not read the analysis framebuffer."; return false; }
			tickBounds[i] = pngexport::AlphaBounds(pixels, A, A);
			unionBounds.add(tickBounds[i]);
		}
		if (unionBounds.empty()) { result.message = "Nothing visible in the exported ticks."; return false; }
	}

	// Canvas for a world-space box [wx0, wx1) x [wy0, wy1).
	struct Canvas { int w, h; float ox, oy; };
	auto canvasFor = [&](const pngexport::Bounds& b) {
		const int pad = std::max(0, s.padding);
		// Bounds are analysis pixels; world = pixel - analysis origin. One
		// extra world pixel on each side covers bilinear/PAT edge bleed.
		const float wx0 = b.x0 - AOX - 1.f, wy0 = b.y0 - AOY - 1.f;
		const float wx1 = b.x1 + 1 - AOX + 1.f, wy1 = b.y1 + 1 - AOY + 1.f;
		Canvas cv;
		cv.w = std::min(8192, (int)std::ceil((wx1 - wx0) * scale) + 2 * pad);
		cv.h = std::min(8192, (int)std::ceil((wy1 - wy0) * scale) + 2 * pad);
		cv.ox = -wx0 * scale + pad;
		cv.oy = -wy0 * scale + pad;
		return cv;
	};
	Canvas fixedCv{};
	if (s.crop == ExportSettings::fixedCanvas) {
		fixedCv.w = std::clamp(s.canvasW, 16, 8192);
		fixedCv.h = std::clamp(s.canvasH, 16, 8192);
		fixedCv.ox = (float)s.canvasOriginX;
		fixedCv.oy = (float)s.canvasOriginY;
	} else if (s.crop == ExportSettings::fitAll) {
		fixedCv = canvasFor(unionBounds);
	}

	const std::string base = pngexport::SanitizeFileComponent(s.baseName);
	nlohmann::json manifest;
	manifest["tool"] = "Hantei-chan PNG export";
	manifest["character"] = AnsiToUtf8(c->getName());
	manifest["pattern"] = view->getState().pattern;
	manifest["scale"] = scale;
	manifest["background"] = s.background == ExportSettings::transparent ? "transparent" : "solid";
	manifest["spawns"] = s.spawns;
	manifest["hitboxes"] = s.boxes;
	nlohmann::json frames = nlohmann::json::array();

	RenderTarget target;
	FrameData* effectData = c->effectCharacter ? &c->effectCharacter->frameData : nullptr;
	auto& sim = view->getState().BindPreviewSim(&c->frameData, effectData);
	preview::TickState probe;
	for (size_t i = 0; i < ticks.size(); ++i) {
		const int tick = ticks[i];
		if (s.crop == ExportSettings::fitEach && tickBounds[i].empty()) { ++result.skipped; continue; }
		const Canvas cv = s.crop == ExportSettings::fitEach ? canvasFor(tickBounds[i]) : fixedCv;
		if (!target.ensure(cv.w, cv.h, false)) { result.message = "Could not create the export framebuffer."; return false; }
		Render::PassParams pass;
		pass.width = cv.w; pass.height = cv.h;
		pass.originX = cv.ox; pass.originY = cv.oy;
		pass.zoom = (float)scale;

		auto renderOver = [&](float r, float g, float b, std::vector<uint8_t>& dst) {
			{
				ScopedTargetBinding bind(target);
				bind.clear(r, g, b, 1.f, true);
				DrawCharacterScene(view, pass, sceneFor(tick));
			}
			return target.readRgba(dst);
		};
		if (s.background == ExportSettings::transparent) {
			if (!renderOver(0.f, 0.f, 0.f, black) || !renderOver(1.f, 1.f, 1.f, white)) {
				result.message = "Could not read the export framebuffer.";
				return false;
			}
			pngexport::MatteFromBlackWhite(black, white, out);
			if (s.skipEmpty && !liveFrame && pngexport::IsFullyTransparent(out)) { ++result.skipped; continue; }
		} else {
			const float* col = s.background == ExportSettings::editorColor ? clearColor : s.customColor;
			if (!renderOver(col[0], col[1], col[2], out)) { result.message = "Could not read the export framebuffer."; return false; }
			for (size_t p = 3; p < out.size(); p += 4) out[p] = 255;
		}

		char suffix[64];
		if (liveFrame) snprintf(suffix, sizeof(suffix), "_f%03d_t%04d", view->getState().frame, tick);
		else snprintf(suffix, sizeof(suffix), "_t%04d", tick);
		const std::string file = base + suffix + ".png";
		const std::string path = s.folder + "\\" + file;
		if (!WritePngRgba(path, out.data(), cv.w, cv.h, error)) { result.message = error; return false; }

		sim.getStateAt(tick, probe);
		const preview::SimActor* root = probe.root();
		nlohmann::json fj;
		fj["file"] = file;
		fj["tick"] = tick;
		fj["root_frame"] = liveFrame ? view->getState().frame : (root ? root->frame : -1);
		fj["actors"] = (int)probe.actors.size();
		fj["width"] = cv.w; fj["height"] = cv.h;
		fj["origin_x"] = cv.ox; fj["origin_y"] = cv.oy;   // pixel of world (0,0)
		frames.push_back(fj);
		if (result.firstFile.empty()) result.firstFile = path;
		++result.files;
	}
	manifest["frames"] = frames;
	if (result.files > 1 || s.writeManifest) {
		const std::string text = manifest.dump(2);
		const std::string mpath = s.folder + "\\" + base + ".json";
		const std::wstring wpath = Utf8ToWide(mpath);
		// Same atomic helper as the project file; it takes a narrow (ANSI)
		// path, so write the manifest through a UTF-16 temp + move instead.
		HANDLE h = CreateFileW((wpath + L".tmp").c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h != INVALID_HANDLE_VALUE) {
			DWORD written = 0;
			const BOOL ok = WriteFile(h, text.data(), (DWORD)text.size(), &written, nullptr) && written == text.size();
			CloseHandle(h);
			if (!ok || !MoveFileExW((wpath + L".tmp").c_str(), wpath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
				DeleteFileW((wpath + L".tmp").c_str());
		}
	}
	result.ok = result.files > 0;
	result.folder = s.folder;
	result.ms = viewrender::MsSince(t0);
	char msg[256];
	snprintf(msg, sizeof(msg), "Exported %d PNG%s (%d skipped) in %.0f ms.", result.files,
		result.files == 1 ? "" : "s", result.skipped, result.ms);
	result.message = msg;
	if (!result.ok && result.skipped) result.message = "Every tick was empty; nothing was written.";
	return result.ok;
}

// Default output folder: "<character folder>\export" (never the exe folder).
std::string MainFrame::DefaultExportFolder(CharacterView* view) const
{
	std::string src;
	if (view && view->getCharacter()) {
		src = view->getCharacter()->getTxtPath();
		if (src.empty()) src = view->getCharacter()->getTopHA6Path();
	}
	const std::string utf8 = AnsiToUtf8(src);
	const size_t slash = utf8.find_last_of("\\/");
	if (slash == std::string::npos) return std::string();
	return utf8.substr(0, slash) + "\\export";
}

void MainFrame::PrepareExportSettings(CharacterView* view)
{
	if (!view || !view->getCharacter()) return;
	if (m_export.folder.empty()) m_export.folder = DefaultExportFolder(view);
	char base[256];
	snprintf(base, sizeof(base), "%s_p%03d", AnsiToUtf8(view->getCharacter()->getName()).c_str(),
		view->getState().pattern);
	if (m_exportBaseAuto || m_export.baseName.empty()) { m_export.baseName = base; m_exportBaseAuto = true; }
}

// Runs a queued export between frames (from DrawBack, before the main pass).
void MainFrame::ProcessPendingExport()
{
	if (m_exportRequest == ExportRequest::none) return;
	const ExportRequest req = m_exportRequest;
	m_exportRequest = ExportRequest::none;
	CharacterView* view = findViewById(m_exportViewId);
	if (!view) view = getShortcutView();
	if (!view) return;
	PrepareExportSettings(view);
	ExportSettings s = m_export;
	if (req == ExportRequest::quickFrame) s.range = ExportSettings::currentFrame;
	RunPngExport(view, s, m_lastExport);
	m_lastExportTime = ImGui::GetTime();
}

void MainFrame::DrawExportWindow()
{
	if (!m_showExportWindow) return;
	CharacterView* view = findViewById(m_exportViewId);
	if (!view || !view->getCharacter()) view = getShortcutView();
	if (view) m_exportViewId = view->getId();
	PrepareExportSettings(view);

	const ImVec2 mainPos = ImGui::GetMainViewport()->Pos;
	ImGui::SetNextWindowPos(ImVec2(mainPos.x + 160, mainPos.y + 120), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(470, 0), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Export PNG", &m_showExportWindow, ImGuiWindowFlags_NoDocking)) { ImGui::End(); return; }
	if (!view || !view->getCharacter() || view->isStageView()) {
		ImGui::TextDisabled("Select a character tab to export.");
		ImGui::End();
		return;
	}
	auto& s = m_export;
	auto& st = view->getState();
	ImGui::Text("%s  pattern %d  (frame %d, tick %d)", view->getDisplayName().c_str(), st.pattern, st.frame, st.currentTick);
	ImGui::Separator();

	ImGui::TextUnformatted("Range");
	ImGui::RadioButton("Current frame", &s.range, ExportSettings::currentFrame); ImGui::SameLine();
	ImGui::RadioButton("Tick range", &s.range, ExportSettings::tickRange); ImGui::SameLine();
	ImGui::RadioButton("Whole pattern", &s.range, ExportSettings::wholePattern);
	if (s.range == ExportSettings::tickRange) {
		ImGui::SetNextItemWidth(90); ImGui::InputInt("From##tick", &s.fromTick); ImGui::SameLine();
		ImGui::SetNextItemWidth(90); ImGui::InputInt("To##tick", &s.toTick); ImGui::SameLine();
		if (ImGui::SmallButton("Current")) s.fromTick = s.toTick = st.currentTick;
	}
	if (s.range != ExportSettings::currentFrame) {
		ImGui::SetNextItemWidth(90); ImGui::InputInt("Every N ticks", &s.step); s.step = std::max(1, s.step);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(90); ImGui::InputInt("Max ticks", &s.maxTicks); s.maxTicks = std::clamp(s.maxTicks, 1, 5000);
		ImGui::Checkbox("Only ticks where the root frame changes", &s.keyframesOnly);
		ImGui::Checkbox("Skip empty ticks", &s.skipEmpty);
	}

	ImGui::Separator();
	ImGui::TextUnformatted("Content");
	ImGui::Checkbox("Spawned actors", &s.spawns); ImGui::SameLine();
	ImGui::Checkbox("Hitboxes", &s.boxes); ImGui::SameLine();
	ImGui::Checkbox("Smooth filter", &s.smooth);
	ImGui::SetNextItemWidth(120); ImGui::SliderInt("Scale", &s.scale, 1, 8, "%dx");

	ImGui::TextUnformatted("Background");
	ImGui::RadioButton("Transparent", &s.background, ExportSettings::transparent); ImGui::SameLine();
	ImGui::RadioButton("Editor colour", &s.background, ExportSettings::editorColor); ImGui::SameLine();
	ImGui::RadioButton("Colour:", &s.background, ExportSettings::customColorBg); ImGui::SameLine();
	ImGui::ColorEdit3("##exportbg", s.customColor, ImGuiColorEditFlags_NoInputs);

	ImGui::TextUnformatted("Canvas");
	ImGui::RadioButton("Fit all ticks (one fixed size)", &s.crop, ExportSettings::fitAll);
	ImGui::RadioButton("Fit each tick", &s.crop, ExportSettings::fitEach);
	ImGui::RadioButton("Fixed size", &s.crop, ExportSettings::fixedCanvas);
	if (s.crop == ExportSettings::fixedCanvas) {
		ImGui::SetNextItemWidth(80); ImGui::InputInt("W", &s.canvasW); ImGui::SameLine();
		ImGui::SetNextItemWidth(80); ImGui::InputInt("H", &s.canvasH); ImGui::SameLine();
		ImGui::SetNextItemWidth(80); ImGui::InputInt("Origin X", &s.canvasOriginX); ImGui::SameLine();
		ImGui::SetNextItemWidth(80); ImGui::InputInt("Origin Y", &s.canvasOriginY);
	} else {
		ImGui::SetNextItemWidth(80); ImGui::InputInt("Padding (px)", &s.padding); s.padding = std::clamp(s.padding, 0, 512);
	}

	ImGui::Separator();
	char folder[1024];
	snprintf(folder, sizeof(folder), "%s", s.folder.c_str());
	ImGui::SetNextItemWidth(330);
	if (ImGui::InputText("##folder", folder, sizeof(folder))) s.folder = folder;
	ImGui::SameLine();
	if (ImGui::Button("Browse...")) {
		const std::string picked = BrowseForFolderUtf8(s.folder);
		if (!picked.empty()) s.folder = picked;
	}
	char base[256];
	snprintf(base, sizeof(base), "%s", s.baseName.c_str());
	ImGui::SetNextItemWidth(330);
	if (ImGui::InputText("File name prefix", base, sizeof(base))) { s.baseName = base; m_exportBaseAuto = false; }
	ImGui::Checkbox("Write JSON manifest (origin and tick of every file)", &s.writeManifest);

	if (ImGui::Button("Export", ImVec2(120, 0))) {
		m_exportRequest = ExportRequest::run;
		m_exportViewId = view->getId();
	}
	ImGui::SameLine();
	ImGui::TextDisabled("%s / %s", shortcuts.registry().label(ShortcutAction::exportFramePng).c_str(),
		shortcuts.registry().label(ShortcutAction::exportSequencePng).c_str());
	if (!m_lastExport.message.empty()) {
		ImGui::TextColored(m_lastExport.ok ? ImVec4(0.2f, 0.7f, 0.2f, 1.f) : ImVec4(0.9f, 0.3f, 0.2f, 1.f),
			"%s", m_lastExport.message.c_str());
		if (m_lastExport.ok) ImGui::TextDisabled("%s", m_lastExport.folder.c_str());
	}
	ImGui::End();
}

void MainFrame::DrawRenderToolWindows()
{
	DrawExportWindow();
	DrawPackageToolWindows();
	// Short confirmation for quick exports (P) made with the window closed.
	if (!m_showExportWindow && !m_lastExport.message.empty() && ImGui::GetTime() - m_lastExportTime < 4.0) {
		const ImGuiViewport* vp = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x - 16, vp->Pos.y + vp->Size.y - 16), ImGuiCond_Always, ImVec2(1, 1));
		ImGui::SetNextWindowBgAlpha(0.85f);
		ImGui::Begin("##export_toast", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDocking);
		ImGui::TextUnformatted(m_lastExport.message.c_str());
		if (m_lastExport.ok) ImGui::TextDisabled("%s", m_lastExport.firstFile.c_str());
		ImGui::End();
	}
}

#endif /* UI_PNG_EXPORT_IMPL_H_GUARD */
