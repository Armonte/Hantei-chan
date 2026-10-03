#include "han2_pac_window.h"
#include "han2/pac_archive.h"
#include "han2/img_file.h"
#include "filedialog.h"
#include "png_writer.h"
#include "misc.h"

#include <windows.h>
#include <glad/glad.h>
#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>

namespace fs = std::filesystem;

namespace han2ui {

bool showPacCreate = false;

namespace {

std::string Lower(std::string s) { for (auto &c : s) c = (char)tolower((unsigned char)c); return s; }
std::string ToUtf8(const std::string &cp932) { return sj2utf8(cp932); }

struct Row {
	std::string name;                 // CP932 bytes as stored in the archive
	std::vector<uint8_t> rawName;
	uint32_t size = 0;
	pac::WriteSource::Kind src = pac::WriteSource::ArchiveEntry;
	size_t baseIdx = 0;
	std::string path;                 // loose file
	std::vector<uint8_t> memory;
	bool replaced = false;
	bool selected = false;
};

struct Creator {
	std::string basePath, baseTemp;
	std::unique_ptr<pac::Archive> base;
	std::vector<Row> rows;
	std::string status;
	std::string outPath;
	int progressDone = 0, progressTotal = 0;
	char filter[64]{};
} g_c;

int FindRow(const std::string &name)
{
	std::string l = Lower(name);
	for (size_t i = 0; i < g_c.rows.size(); i++) if (Lower(g_c.rows[i].name) == l) return (int)i;
	return -1;
}

void CloseBase()
{
	g_c.base.reset();
	if (!g_c.baseTemp.empty()) { std::error_code ec; fs::remove(fs::u8path(g_c.baseTemp), ec); g_c.baseTemp.clear(); }
}

void SetBase(const std::string &path)
{
	// the base is copied to a temp file and read from the copy (the original stays untouched and unlocked)
	std::error_code ec;
	std::string tmp = (fs::temp_directory_path(ec) / ("han2_pacbase_" + std::to_string((unsigned long long)GetTickCount64()) + ".pac")).string();
	fs::copy_file(fs::u8path(path), fs::u8path(tmp), fs::copy_options::overwrite_existing, ec);
	if (ec) { g_c.status = "Could not copy the base PAC to a temp file: " + ec.message(); return; }
	auto a = std::make_unique<pac::Archive>();
	std::string err;
	if (!pac::Open(tmp, *a, &err)) { g_c.status = path + ": " + err; fs::remove(fs::u8path(tmp), ec); return; }
	CloseBase();
	g_c.base = std::move(a); g_c.baseTemp = tmp; g_c.basePath = path;
	g_c.rows.clear();
	for (size_t i = 0; i < g_c.base->entries.size(); i++) {      // importing the root imports every entry
		Row r; const auto &e = g_c.base->entries[i];
		r.name = e.name; r.rawName.assign(e.rawName, e.rawName + pac::kNameLen); r.size = e.size; r.baseIdx = i;
		g_c.rows.push_back(std::move(r));
	}
	g_c.status = "Imported " + std::to_string(g_c.rows.size()) + " entries from " + path;
}

void AddOrReplaceFile(const std::string &path)
{
	std::error_code ec;
	auto sz = fs::file_size(fs::u8path(path), ec);
	if (ec) { g_c.status = "Cannot read " + path; return; }
	std::string name = utf82sj(fs::u8path(path).filename().string());
	if (name.size() >= pac::kNameLen) { g_c.status = "Name too long (max 59 bytes in the archive): " + ToUtf8(name); return; }
	int i = FindRow(name);
	Row r; r.size = (uint32_t)sz; r.src = pac::WriteSource::LooseFile; r.path = path;
	if (i >= 0) { r.name = g_c.rows[i].name; r.rawName = g_c.rows[i].rawName; r.replaced = true; g_c.rows[i] = std::move(r); }
	else { r.name = name; g_c.rows.push_back(std::move(r)); }
}

void ProgressCb(int done, int total, void *) { g_c.progressDone = done; g_c.progressTotal = total; }

void Save()
{
	std::vector<pac::WriteSource> src;
	for (auto &r : g_c.rows) {
		pac::WriteSource w; w.name = r.name; w.rawName = r.rawName; w.kind = r.src;
		if (r.src == pac::WriteSource::ArchiveEntry) { w.archive = g_c.base.get(); w.index = r.baseIdx; }
		else if (r.src == pac::WriteSource::LooseFile) w.path = r.path;
		else w.memory = r.memory;
		src.push_back(std::move(w));
	}
	std::vector<std::string> refuse;
	if (!g_c.basePath.empty()) refuse.push_back(g_c.basePath);
	if (!g_c.baseTemp.empty()) refuse.push_back(g_c.baseTemp);
	std::string err;
	g_c.progressDone = 0;
	if (pac::WriteArchive(g_c.outPath, src, refuse, &err, ProgressCb, nullptr)) g_c.status = "Saved " + g_c.outPath + " (" + std::to_string(src.size()) + " entries)";
	else g_c.status = "Not saved: " + err;
}

} // namespace

void PacCreateAddMemory(const std::string &name, std::vector<uint8_t> data)
{
	int i = FindRow(name);
	Row r; r.size = (uint32_t)data.size(); r.src = pac::WriteSource::Memory; r.memory = std::move(data);
	if (i >= 0) { r.name = g_c.rows[i].name; r.rawName = g_c.rows[i].rawName; r.replaced = true; g_c.rows[i] = std::move(r); }
	else { r.name = name; g_c.rows.push_back(std::move(r)); }
	showPacCreate = true;
	g_c.status = "Queued " + ToUtf8(name) + " for the new archive";
}

void DrawPacCreate()
{
	if (!showPacCreate) return;
	ImGui::SetNextWindowSize(ImVec2(720, 520), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Create / patch a PAC archive", &showPacCreate)) { ImGui::End(); return; }
	ImGui::TextWrapped("Pick a base PAC (it is copied to a temp file and read from the copy), replace entries by adding files or a folder with the same names, "
	                   "then save as a NEW archive. The base archive is never overwritten.");
	if (ImGui::Button("Select base PAC...")) { std::string p = FileDialog(fileType::HAN2, false); if (!p.empty()) SetBase(p); }
	ImGui::SameLine();
	if (ImGui::Button("Add file...")) { std::string p = FileDialog(-1, false); if (!p.empty()) AddOrReplaceFile(p); }
	ImGui::SameLine();
	if (ImGui::Button("Add folder (batch override)...")) {
		std::string d = BrowseForFolderUtf8("");
		if (!d.empty()) { std::error_code ec; int n = 0; for (auto &e : fs::directory_iterator(fs::u8path(d), ec)) if (e.is_regular_file()) { AddOrReplaceFile(e.path().u8string()); n++; } g_c.status = "Added/replaced " + std::to_string(n) + " files from " + d; }
	}
	ImGui::SameLine();
	if (ImGui::Button("Remove selected")) g_c.rows.erase(std::remove_if(g_c.rows.begin(), g_c.rows.end(), [](const Row &r) { return r.selected; }), g_c.rows.end());
	ImGui::SameLine();
	if (ImGui::Button("Clear")) { g_c.rows.clear(); CloseBase(); g_c.basePath.clear(); }
	ImGui::TextDisabled("base: %s", g_c.basePath.empty() ? "(none)" : g_c.basePath.c_str());
	ImGui::SetNextItemWidth(200); ImGui::InputText("filter", g_c.filter, sizeof(g_c.filter));
	if (ImGui::BeginTable("rows", 4, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable, ImVec2(0, -70))) {
		ImGui::TableSetupColumn("name"); ImGui::TableSetupColumn("size"); ImGui::TableSetupColumn("source"); ImGui::TableSetupColumn("note");
		ImGui::TableSetupScrollFreeze(0, 1); ImGui::TableHeadersRow();
		std::string flt = Lower(g_c.filter);
		for (size_t i = 0; i < g_c.rows.size(); i++) {
			Row &r = g_c.rows[i];
			std::string u = ToUtf8(r.name);
			if (!flt.empty() && Lower(u).find(flt) == std::string::npos) continue;
			ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);
			ImGui::PushID((int)i);
			ImGui::Selectable(u.c_str(), &r.selected, ImGuiSelectableFlags_SpanAllColumns);
			ImGui::TableSetColumnIndex(1); ImGui::Text("%u", r.size);
			ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(r.src == pac::WriteSource::ArchiveEntry ? "base" : r.src == pac::WriteSource::LooseFile ? r.path.c_str() : "edited in memory");
			ImGui::TableSetColumnIndex(3); if (r.replaced) ImGui::TextColored(ImVec4(1, .8f, .3f, 1), "replaced");
			ImGui::PopID();
		}
		ImGui::EndTable();
	}
	if (ImGui::Button("Save as new PAC...")) { std::string p = FileDialog(-1, true); if (!p.empty()) { g_c.outPath = p; Save(); } }
	if (g_c.progressTotal && g_c.progressDone < g_c.progressTotal) ImGui::ProgressBar((float)g_c.progressDone / g_c.progressTotal);
	ImGui::TextWrapped("%s", g_c.status.c_str());
	ImGui::End();
}

// ---------------------------------------------------------------------------------------------------------------------------
// file viewers
// ---------------------------------------------------------------------------------------------------------------------------
namespace {

struct Viewer {
	int id = 0; bool open = true;
	std::string name, origin;           // name: CP932
	std::vector<uint8_t> bytes;
	enum Kind { Hex, Image } kind = Hex;
	han2::ImgFile img; GLuint tex = 0; float zoom = 1.f; bool checker = true; bool dirty = false; std::string msg;
	std::vector<std::string> strings;
};
std::vector<std::unique_ptr<Viewer>> g_views;
int g_nextView = 1;

void Upload(Viewer &v)
{
	if (!v.tex) glGenTextures(1, &v.tex);
	glBindTexture(GL_TEXTURE_2D, v.tex);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, v.img.width, v.img.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, v.img.rgba.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void CollectStrings(Viewer &v)
{
	std::string cur;
	for (uint8_t c : v.bytes) {
		if ((c >= 0x20 && c < 0x7F) || c >= 0x81) cur += (char)c;
		else { if (cur.size() >= 5) v.strings.push_back(sj2utf8(cur)); cur.clear(); }
		if (v.strings.size() > 2000) break;
	}
	if (cur.size() >= 5) v.strings.push_back(sj2utf8(cur));
}

} // namespace

void OpenFileViewer(const std::string &name, std::vector<uint8_t> bytes, const std::string &origin)
{
	auto v = std::make_unique<Viewer>();
	v->id = g_nextView++; v->name = name; v->origin = origin; v->bytes = std::move(bytes);
	if (han2::IsImg(v->bytes.data(), v->bytes.size()) && han2::ParseImg(v->bytes.data(), v->bytes.size(), v->img, nullptr)) { v->kind = Viewer::Image; Upload(*v); }
	else CollectStrings(*v);
	g_views.push_back(std::move(v));
}

void DrawFileViewers()
{
	for (auto &vp : g_views) {
		Viewer &v = *vp;
		char title[256]; snprintf(title, sizeof(title), "%s%s###viewer%d", sj2utf8(v.name).c_str(), v.dirty ? " *" : "", v.id);
		ImGui::SetNextWindowSize(ImVec2(560, 460), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin(title, &v.open)) { ImGui::End(); continue; }
		ImGui::TextDisabled("%s, %zu bytes", v.origin.c_str(), v.bytes.size());
		if (v.kind == Viewer::Image) {
			ImGui::Text("IMG v%u, %d x %d, RGBA", v.img.version, v.img.width, v.img.height);
			ImGui::SetNextItemWidth(120); ImGui::SliderFloat("zoom", &v.zoom, 0.25f, 8.f); ImGui::SameLine(); ImGui::Checkbox("checkerboard", &v.checker);
			if (ImGui::Button("Export PNG...")) {
				std::string p = FileDialog(-1, true); if (!p.empty()) { std::string e; if (p.size() < 4 || Lower(p.substr(p.size() - 4)) != ".png") p += ".png"; v.msg = WritePngRgba(p, v.img.rgba.data(), v.img.width, v.img.height, e) ? "exported " + p : e; }
			}
			ImGui::SameLine();
			if (ImGui::Button("Import PNG (replace pixels)...")) {
				std::string p = FileDialog(-1, false);
				if (!p.empty()) {
					std::vector<uint8_t> px; int w = 0, h = 0; std::string e;
					if (ReadImageRgba(p, px, w, h, e)) { v.img.rgba = std::move(px); v.img.width = w; v.img.height = h; Upload(v); v.dirty = true; v.msg = "replaced by " + p; }
					else v.msg = e;
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Save IMG...")) {
				std::string p = FileDialog(-1, true);
				if (!p.empty()) { std::vector<uint8_t> out; han2::SerializeImg(v.img, out); std::ofstream f(fs::u8path(p), std::ios::binary); f.write((const char *)out.data(), (std::streamsize)out.size()); v.msg = f ? "saved " + p : "could not write " + p; if (f) v.dirty = false; }
			}
			ImGui::SameLine();
			if (ImGui::Button("Put into new PAC")) { std::vector<uint8_t> out; han2::SerializeImg(v.img, out); PacCreateAddMemory(v.name, std::move(out)); }
			ImGui::BeginChild("img", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
			ImVec2 sz(v.img.width * v.zoom, v.img.height * v.zoom), p0 = ImGui::GetCursorScreenPos();
			if (v.checker) {
				ImDrawList *dl = ImGui::GetWindowDrawList();
				for (float y = 0; y < sz.y; y += 16) for (float x = 0; x < sz.x; x += 16)
					dl->AddRectFilled(ImVec2(p0.x + x, p0.y + y), ImVec2(std::min(p0.x + x + 16, p0.x + sz.x), std::min(p0.y + y + 16, p0.y + sz.y)), (((int)(x / 16) + (int)(y / 16)) & 1) ? IM_COL32(110, 110, 110, 255) : IM_COL32(160, 160, 160, 255));
			}
			ImGui::Image((ImTextureID)(intptr_t)v.tex, sz);
			ImGui::EndChild();
		} else {
			ImGui::TextWrapped("%s", "Script bank / data file. Raw view below; strings found (CP932):");
			if (ImGui::Button("Export raw...")) { std::string p = FileDialog(-1, true); if (!p.empty()) { std::ofstream f(fs::u8path(p), std::ios::binary); f.write((const char *)v.bytes.data(), (std::streamsize)v.bytes.size()); v.msg = f ? "saved " + p : "could not write " + p; } }
			ImGui::BeginChild("strs", ImVec2(0, 140), true);
			for (auto &s : v.strings) ImGui::TextUnformatted(s.c_str());
			ImGui::EndChild();
			ImGui::BeginChild("hex", ImVec2(0, 0), true);
			const size_t shown = std::min<size_t>(v.bytes.size(), 16384);
			for (size_t o = 0; o < shown; o += 16) {
				char line[128]; int n = snprintf(line, sizeof(line), "%08zX  ", o);
				for (size_t k = 0; k < 16; k++) n += (o + k < v.bytes.size()) ? snprintf(line + n, sizeof(line) - n, "%02X ", v.bytes[o + k]) : snprintf(line + n, sizeof(line) - n, "   ");
				n += snprintf(line + n, sizeof(line) - n, " ");
				for (size_t k = 0; k < 16 && o + k < v.bytes.size(); k++) { uint8_t c = v.bytes[o + k]; line[n++] = (c >= 0x20 && c < 0x7F) ? (char)c : '.'; }
				line[n] = 0; ImGui::TextUnformatted(line);
			}
			if (shown < v.bytes.size()) ImGui::TextDisabled("... %zu more bytes (use Export raw)", v.bytes.size() - shown);
			ImGui::EndChild();
		}
		if (!v.msg.empty()) ImGui::TextWrapped("%s", v.msg.c_str());
		ImGui::End();
	}
	for (auto &vp : g_views) if (!vp->open && vp->tex) { glDeleteTextures(1, &vp->tex); vp->tex = 0; }
	g_views.erase(std::remove_if(g_views.begin(), g_views.end(), [](const std::unique_ptr<Viewer> &v) { return !v->open; }), g_views.end());
}

} // namespace han2ui
