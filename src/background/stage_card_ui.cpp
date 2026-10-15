// Stage card window (stage_card.h): one image per slot, resized to the slot's size, previewed, and written as the game's
// files for a stage id into an output folder.
#include "stage_card.h"

#include "../filedialog.h"
#include "../i18n.h"
#include "../misc.h"       // WriteFileAtomic
#include "../png_writer.h" // BrowseForFolderUtf8

#include <glad/glad.h>
#include <imgui.h>

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace stagecard {

bool Window::load(Slot s, const std::string& path)
{
	int w = 0, h = 0;
	std::vector<uint8_t> px;
	std::string err;
	if (!ReadImageRgba(path, px, w, h, err)) { m_status = std::string(TXT("Cannot read the image:")) + " " + path + " (" + err + ")"; m_statusError = true; return false; }
	Source& src = m_src[(int)s];
	const SlotInfo& i = Info(s);
	if (w == i.w && h == i.h) src.rgba = px;
	else Resize(px.data(), w, h, src.rgba, i.w, i.h);
	src.path = path;
	if (!src.tex) glGenTextures(1, &src.tex);
	glBindTexture(GL_TEXTURE_2D, src.tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, i.w, i.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, src.rgba.data());
	if (w != i.w || h != i.h) { char b[160]; snprintf(b, sizeof b, TXT("Resized %dx%d to %dx%d."), w, h, i.w, i.h); m_status = b; m_statusError = false; }
	return true;
}

void Window::draw()
{
	if (!open) return;
	ImGui::SetNextWindowSize(ImVec2(640, 520), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(LBL("Stage card (select name, preview, round call)"), &open)) { ImGui::End(); return; }
	ImGui::TextWrapped("%s", TXT("MBAACC has no stage-name strings: the select screen and the round call show these images. Load any image per slot; it is resized to the slot. The BGM and its title are in the BGM window."));
	ImGui::SetNextItemWidth(120);
	ImGui::InputInt(LBL("Stage id"), &stageId);
	stageId = std::clamp(stageId, 0, 99);
	ImGui::SameLine();
	if (ImGui::Button(LBL("Output folder..."))) { const std::string d = BrowseForFolderUtf8(outDir); if (!d.empty()) outDir = d; }
	ImGui::SameLine();
	ImGui::TextDisabled("%s", outDir.empty() ? TXT("(none)") : outDir.c_str());

	for (int k = 0; k < (int)Slot::Count; k++) {
		const Slot s = (Slot)k;
		const SlotInfo& i = Info(s);
		Source& src = m_src[k];
		ImGui::PushID(k);
		ImGui::SeparatorText(TXT(i.label));
		ImGui::TextDisabled("%s  (%dx%d, %s)", GamePath(s, stageId).c_str(), i.w, i.h, i.dxt == 3 ? "DXT3" : i.dxt == 5 ? "DXT5" : "BMP 32-bit");
		if (ImGui::Button(LBL("Load image..."))) { const std::string p = FileDialog(fileType::IMAGE); if (!p.empty()) load(s, p); }
		if (src.tex) {
			ImGui::SameLine();
			// Checkerboard-free preview on the window background; 2x so the 16-px round call is readable.
			ImGui::Image((ImTextureID)(intptr_t)src.tex, ImVec2((float)i.w * (i.h <= 32 ? 2.f : 1.f), (float)i.h * (i.h <= 32 ? 2.f : 1.f)));
		}
		ImGui::PopID();
	}

	ImGui::Separator();
	bool any = false;
	for (const auto& s : m_src) any |= !s.rgba.empty();
	ImGui::BeginDisabled(!any || outDir.empty());
	if (ImGui::Button(LBL("Write the stage card files"))) {
		int written = 0;
		std::string fail;
		for (int k = 0; k < (int)Slot::Count; k++) {
			if (m_src[k].rgba.empty()) continue;
			const Slot s = (Slot)k;
			const SlotInfo& i = Info(s);
			const std::vector<uint8_t> bytes = BuildSlot(s, m_src[k].rgba.data(), i.w, i.h);
			std::string rel = GamePath(s, stageId);
			std::replace(rel.begin(), rel.end(), '\\', '/');
			const fs::path out = fs::u8path(outDir) / fs::u8path(rel);
			std::error_code ec;
			fs::create_directories(out.parent_path(), ec);
			if (WriteFileAtomic(out.string().c_str(), bytes.data(), bytes.size())) written++;
			else fail = out.u8string();
		}
		if (fail.empty()) { char b[128]; snprintf(b, sizeof b, TXT("Wrote %d file(s)."), written); m_status = b; m_statusError = false; }
		else { m_status = std::string(TXT("Save failed.")) + " " + fail; m_statusError = true; }
	}
	ImGui::EndDisabled();
	if (!m_status.empty()) ImGui::TextColored(m_statusError ? ImVec4(1, 0.45f, 0.4f, 1) : ImVec4(0.6f, 0.9f, 0.6f, 1), "%s", m_status.c_str());
	ImGui::End();
}

} // namespace stagecard
