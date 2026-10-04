#include "han2_pac_window.h"
#include "han2_typed_files.h"
#include <map>
#include "han2/dmp_fob.h"
#include "han2/pac_archive.h"
#include "han2/img_file.h"
#include "han2/misc_formats.h"
#include "filedialog.h"
#include "png_writer.h"
#include "misc.h"
#include "i18n.h"
#include <cmath>
#include <cstdarg>

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
std::string Fmt(const char *fmt, ...)
{
	char b[2048]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof(b), fmt, ap); va_end(ap); return b;
}

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
	if (ec) { g_c.status = Fmt(TXT("Could not copy the base PAC to a temp file: %s"), ec.message().c_str()); return; }
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
	g_c.status = Fmt(TXT("Imported %zu entries from %s"), g_c.rows.size(), path.c_str());
}

void AddOrReplaceFile(const std::string &path)
{
	std::error_code ec;
	auto sz = fs::file_size(fs::u8path(path), ec);
	if (ec) { g_c.status = Fmt(TXT("Cannot read %s"), path.c_str()); return; }
	std::string name = utf82sj(fs::u8path(path).filename().string());
	if (name.size() >= pac::kNameLen) { g_c.status = Fmt(TXT("Name too long (max 59 bytes in the archive): %s"), ToUtf8(name).c_str()); return; }
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
	if (pac::WriteArchive(g_c.outPath, src, refuse, &err, ProgressCb, nullptr)) g_c.status = Fmt(TXT("Saved %s (%zu entries)"), g_c.outPath.c_str(), src.size());
	else g_c.status = Fmt(TXT("Not saved: %s"), err.c_str());
}

} // namespace

void PacCreateAddMemory(const std::string &name, std::vector<uint8_t> data)
{
	int i = FindRow(name);
	Row r; r.size = (uint32_t)data.size(); r.src = pac::WriteSource::Memory; r.memory = std::move(data);
	if (i >= 0) { r.name = g_c.rows[i].name; r.rawName = g_c.rows[i].rawName; r.replaced = true; g_c.rows[i] = std::move(r); }
	else { r.name = name; g_c.rows.push_back(std::move(r)); }
	showPacCreate = true;
	g_c.status = Fmt(TXT("Queued %s for the new archive"), ToUtf8(name).c_str());
}

void DrawPacCreate()
{
	if (!showPacCreate) return;
	ImGui::SetNextWindowSize(ImVec2(720, 520), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(LBL("Create / patch a PAC archive"), &showPacCreate)) { ImGui::End(); return; }
	ImGui::TextWrapped("%s", TXT("Pick a base PAC (it is copied to a temp file and read from the copy), replace entries by adding files or a folder with the same names, then save as a NEW archive. The base archive is never overwritten."));
	if (ImGui::Button(LBL("Select base PAC..."))) { std::string p = FileDialog(fileType::HAN2, false); if (!p.empty()) SetBase(p); }
	ImGui::SameLine();
	if (ImGui::Button(LBL("Add file..."))) { std::string p = FileDialog(-1, false); if (!p.empty()) AddOrReplaceFile(p); }
	ImGui::SameLine();
	if (ImGui::Button(LBL("Add folder (batch override)..."))) {
		std::string d = BrowseForFolderUtf8("");
		if (!d.empty()) { std::error_code ec; int n = 0; for (auto &e : fs::directory_iterator(fs::u8path(d), ec)) if (e.is_regular_file()) { AddOrReplaceFile(e.path().u8string()); n++; } g_c.status = Fmt(TXT("Added/replaced %d files from %s"), n, d.c_str()); }
	}
	ImGui::SameLine();
	if (ImGui::Button(LBL("Remove selected"))) g_c.rows.erase(std::remove_if(g_c.rows.begin(), g_c.rows.end(), [](const Row &r) { return r.selected; }), g_c.rows.end());
	ImGui::SameLine();
	if (ImGui::Button(LBL("Clear"))) { g_c.rows.clear(); CloseBase(); g_c.basePath.clear(); }
	ImGui::PushTextWrapPos(0.0f); ImGui::TextDisabled(TXT("base: %s"), g_c.basePath.empty() ? TXT("(none)") : g_c.basePath.c_str()); ImGui::PopTextWrapPos();
	ImGui::SetNextItemWidth(200); ImGui::InputText(LBL("filter"), g_c.filter, sizeof(g_c.filter));
	if (ImGui::BeginTable("rows", 4, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable, ImVec2(0, -70))) {
		ImGui::TableSetupColumn(LBL("name")); ImGui::TableSetupColumn(LBL("size")); ImGui::TableSetupColumn(LBL("source")); ImGui::TableSetupColumn(LBL("note"));
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
			ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(r.src == pac::WriteSource::ArchiveEntry ? TXT("base") : r.src == pac::WriteSource::LooseFile ? r.path.c_str() : TXT("edited in memory"));
			ImGui::TableSetColumnIndex(3); if (r.replaced) ImGui::TextColored(ImVec4(1, .8f, .3f, 1), "%s", TXT("replaced"));
			ImGui::PopID();
		}
		ImGui::EndTable();
	}
	if (ImGui::Button(LBL("Save as new PAC..."))) { std::string p = FileDialog(-1, true); if (!p.empty()) { g_c.outPath = p; Save(); } }
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
	enum Kind { Hex, Image, Poly, Typed, Fob } kind = Hex;
	int px16 = 0;                       // 16-bit sheets: 0 = A1R5G5B5, 1 = A4R4G4B4 (the caller of the game's loader decides, dMp files do not name it)
	han2::dmpfob::File fob; std::string fobFilter;
	TypedFile typed; std::vector<int> typedSel; std::string typedFilter;
	han2::PolyObject poly; float polyYaw = 0.f, polyPitch = 0.f, polyZoom = 1.f;
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
	if (han2::IsImg(v->bytes.data(), v->bytes.size()) && han2::ParseImg(v->bytes.data(), v->bytes.size(), v->img, nullptr)) { v->kind = Viewer::Image; v->px16 = v->img.format == 1 ? 1 : 0; Upload(*v); }
	else if (name.size() > 4 && Lower(name.substr(name.size() - 4)) == ".fob" && han2::dmpfob::Parse(v->bytes.data(), v->bytes.size(), v->fob, nullptr) && v->fob.nInsns > 0) v->kind = Viewer::Fob;
	else if (name.size() > 2 && (name.compare(name.size() - 2, 2, ".B") == 0 || name.compare(name.size() - 2, 2, ".b") == 0) && han2::ParsePoly(v->bytes.data(), v->bytes.size(), v->poly, nullptr)) v->kind = Viewer::Poly;
	else if (DescribeTypedFile(name, v->bytes, v->typed, origin)) { v->kind = Viewer::Typed; v->typedSel.assign(v->typed.regions.size(), 0); }
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
		ImGui::PushTextWrapPos(0.0f); ImGui::TextDisabled(TXT("%s, %zu bytes"), v.origin.c_str(), v.bytes.size()); ImGui::PopTextWrapPos();
		if (v.kind == Viewer::Image) {
			ImGui::Text(TXT("IMG v%u, %d x %d, %s"), v.img.version, v.img.width, v.img.height, v.img.format == 0 ? "ARGB1555" : v.img.format == 1 ? "ARGB4444" : v.img.format == 3 ? "RGB24" : "RGBA");
			ImGui::SetNextItemWidth(120); ImGui::SliderFloat(LBL("zoom"), &v.zoom, 0.25f, 8.f); ImGui::SameLine(); ImGui::Checkbox(LBL("checkerboard"), &v.checker);
			if (!v.img.native.empty() && v.img.format <= 1) {
				ImGui::SameLine(); ImGui::SetNextItemWidth(150);
				const char *fm[2] = { "A1R5G5B5", "A4R4G4B4" };
				if (ImGui::Combo(LBL("16-bit pixel format"), &v.px16, fm, 2)) { han2::DecodePixels16(v.img.native.data(), (size_t)v.img.width * v.img.height, v.px16, v.img.rgba); Upload(v); }
			}
			if (ImGui::Button(LBL("Export PNG..."))) {
				std::string p = FileDialog(-1, true); if (!p.empty()) { std::string e; if (p.size() < 4 || Lower(p.substr(p.size() - 4)) != ".png") p += ".png"; v.msg = WritePngRgba(p, v.img.rgba.data(), v.img.width, v.img.height, e) ? Fmt(TXT("exported %s"), p.c_str()) : e; }
			}
			ImGui::SameLine();
			if (ImGui::Button(LBL("Import PNG (replace pixels)..."))) {
				std::string p = FileDialog(-1, false);
				if (!p.empty()) {
					std::vector<uint8_t> px; int w = 0, h = 0; std::string e;
					if (ReadImageRgba(p, px, w, h, e)) {
						const bool keep16 = !v.img.native.empty() && v.img.format <= 1;   // 16-bit sheets stay 16-bit (the game reads raw words): quantise with the chosen pixel format
						if (keep16 && (w != v.img.width || h != v.img.height)) v.msg = TXT("size mismatch: a 16-bit sheet keeps its dimensions (texture size and script rows depend on them)");
						else {
							if (keep16) { han2::EncodePixels16(px.data(), (size_t)w * h, v.px16, v.img.native); han2::DecodePixels16(v.img.native.data(), (size_t)w * h, v.px16, v.img.rgba); }
							else { v.img.rgba = std::move(px); v.img.native.clear(); v.img.format = 2; v.img.width = w; v.img.height = h; }
							Upload(v); v.dirty = true; v.msg = Fmt(TXT("replaced by %s"), p.c_str());
						} }
					else v.msg = e;
				}
			}
			ImGui::SameLine();
			if (ImGui::Button(LBL("Save IMG..."))) {
				std::string p = FileDialog(-1, true);
				if (!p.empty()) { std::vector<uint8_t> out; han2::SerializeImg(v.img, out); std::ofstream f(fs::u8path(p), std::ios::binary); f.write((const char *)out.data(), (std::streamsize)out.size()); v.msg = f ? Fmt(TXT("saved %s"), p.c_str()) : Fmt(TXT("could not write %s"), p.c_str()); if (f) v.dirty = false; }
			}
			ImGui::SameLine();
			if (ImGui::Button(LBL("Put into new PAC"))) { std::vector<uint8_t> out; han2::SerializeImg(v.img, out); PacCreateAddMemory(v.name, std::move(out)); }
			ImGui::BeginChild("img", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
			ImVec2 sz(v.img.width * v.zoom, v.img.height * v.zoom), p0 = ImGui::GetCursorScreenPos();
			if (v.checker) {
				ImDrawList *dl = ImGui::GetWindowDrawList();
				for (float y = 0; y < sz.y; y += 16) for (float x = 0; x < sz.x; x += 16)
					dl->AddRectFilled(ImVec2(p0.x + x, p0.y + y), ImVec2(std::min(p0.x + x + 16, p0.x + sz.x), std::min(p0.y + y + 16, p0.y + sz.y)), (((int)(x / 16) + (int)(y / 16)) & 1) ? IM_COL32(110, 110, 110, 255) : IM_COL32(160, 160, 160, 255));
			}
			ImGui::Image((ImTextureID)(intptr_t)v.tex, sz);
			ImGui::EndChild();
		} else if (v.kind == Viewer::Poly) {
			// GOF1 .B polygon object (docs/formats/frenchbread_rbo_gof.md section 7): wireframe of every triangle/quad face, orthographic, drag to rotate
			ImGui::Text(TXT("GOF1 polygon object: %u vertices, %u faces, %u texture slots"), (unsigned)v.poly.nVerts, v.poly.nFaces, (unsigned)v.poly.nTextures);
			ImGui::SetNextItemWidth(120); ImGui::SliderFloat(LBL("zoom"), &v.polyZoom, 0.2f, 6.f);
			ImGui::SameLine(); if (ImGui::Button(LBL("Reset view"))) { v.polyYaw = v.polyPitch = 0.f; v.polyZoom = 1.f; }
			ImGui::BeginChild("poly", ImVec2(0, 0), true);
			ImVec2 p0 = ImGui::GetCursorScreenPos(), sz = ImGui::GetContentRegionAvail();
			ImGui::InvisibleButton("polydrag", sz);
			if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) { v.polyYaw += ImGui::GetIO().MouseDelta.x * 0.01f; v.polyPitch += ImGui::GetIO().MouseDelta.y * 0.01f; }
			ImDrawList *dl = ImGui::GetWindowDrawList();
			float r = 1.f; for (auto &q : v.poly.verts) r = std::max(r, std::max(std::fabs(q.x), std::max(std::fabs(q.y), std::fabs(q.z))));
			const float sc = 0.45f * std::min(sz.x, sz.y) / r * v.polyZoom, cy = std::cos(v.polyYaw), sy = std::sin(v.polyYaw), cp = std::cos(v.polyPitch), sp = std::sin(v.polyPitch);
			auto proj = [&](const han2::PolyVertex &q) { float x = q.x * cy + q.z * sy, z = -q.x * sy + q.z * cy, y = q.y * cp - z * sp; return ImVec2(p0.x + sz.x * 0.5f + x * sc, p0.y + sz.y * 0.5f - y * sc); };
			for (auto &f : v.poly.faces) {
				if (f.nIndices < 3 || f.nIndices > 4) continue;
				bool ok = true; ImVec2 pt[4]; for (int k = 0; k < f.nIndices; k++) { if (f.index[k] >= v.poly.verts.size()) { ok = false; break; } pt[k] = proj(v.poly.verts[f.index[k]]); }
				if (!ok) continue;
				for (int k = 0; k < f.nIndices; k++) dl->AddLine(pt[k], pt[(k + 1) % f.nIndices], f.texture < v.poly.slots.size() && v.poly.slots[f.texture].name[0] ? IM_COL32(120, 220, 255, 255) : IM_COL32(230, 230, 230, 255));
			}
			ImGui::EndChild();
		} else if (v.kind == Viewer::Fob) {
			ImGui::Text(TXT("dMp script bank: %zu functions, %zu instructions, %zu bytes of data"), v.fob.funcs.size(), v.fob.nInsns, v.fob.rawBytes);
			if (ImGui::Button(LBL("Save..."))) {
				std::string p = FileDialog(-1, true, (char *)"");
				if (!p.empty()) { std::vector<uint8_t> out; han2::dmpfob::Serialize(v.fob, out); std::ofstream f(fs::u8path(p), std::ios::binary); f.write((const char *)out.data(), (std::streamsize)out.size()); v.msg = f ? Fmt(TXT("saved %s"), p.c_str()) : Fmt(TXT("could not write %s"), p.c_str()); if (f) v.dirty = false; }
			}
			ImGui::SameLine();
			if (ImGui::Button(LBL("Put into new PAC"))) { std::vector<uint8_t> out; han2::dmpfob::Serialize(v.fob, out); PacCreateAddMemory(v.name, std::move(out)); }
			ImGui::SameLine();
			if (ImGui::Button(LBL("Export disassembly..."))) { std::string p = FileDialog(-1, true, (char *)"script.txt"); if (!p.empty()) { std::string t = han2::dmpfob::Disassemble(v.fob); std::ofstream f(fs::u8path(p), std::ios::binary); f.write(t.data(), (std::streamsize)t.size()); } }
			ImGui::SetNextItemWidth(200); char fb[64]; snprintf(fb, sizeof fb, "%s", v.fobFilter.c_str()); if (ImGui::InputText(LBL("filter (op name)"), fb, sizeof fb)) v.fobFilter = fb;
			ImGui::TextDisabled("%s", TXT("PUSH_IMM / PUSH_CODE_ADDR operands are editable (constants of the script); structure edits are not offered, unedited banks save byte-identical."));
			ImGui::BeginChild("fob", ImVec2(0, 0), true);
			std::map<uint32_t, std::string> labels; for (auto &fe : v.fob.funcs) labels[fe.pc] = std::string((const char *)fe.name, strnlen((const char *)fe.name, 32));
			for (size_t i = 0; i < v.fob.items.size(); i++) {
				auto &it = v.fob.items[i];
				auto lb = labels.find(it.pc); if (lb != labels.end()) ImGui::TextColored(ImVec4(.5f, .9f, 1, 1), "%s:", lb->second.c_str());
				if (!it.isInsn) { ImGui::TextDisabled("  %05x  .data %zu bytes", it.pc, it.raw.size()); continue; }
				const char *nm = han2::dmpfob::OpName(it.insn.op); std::string name = nm[0] ? nm : "op" + std::to_string(it.insn.op);
				if (!v.fobFilter.empty() && name.find(v.fobFilter) == std::string::npos) continue;
				ImGui::PushID((int)i);
				ImGui::Text("  %05x  %-24s", it.pc, name.c_str());
				if (it.insn.op == 0x03 || it.insn.op == 0x2F) { ImGui::SameLine(); int iv = (int)it.insn.imm; ImGui::SetNextItemWidth(110); if (ImGui::InputInt("##imm", &iv, 0, 0)) { it.insn.imm = (uint32_t)iv; v.dirty = true; } }
				else if (it.insn.op == 0x04 || it.insn.op == 0x1A || it.insn.op == 0x1B || it.insn.op == 0x26 || it.insn.op == 0x51) { ImGui::SameLine(); ImGui::Text("%#x", it.insn.imm); }
				else if (it.insn.op == 0x19) { ImGui::SameLine(); ImGui::Text("kind=%#x -> %#x", it.insn.kind, it.insn.imm); }
				ImGui::PopID();
			}
			ImGui::EndChild();
		} else if (v.kind == Viewer::Typed) {
			ImGui::Text("%s", v.typed.kind.c_str());
			if (ImGui::Button(LBL("Save..."))) {
				std::string p = FileDialog(-1, true, (char *)"");
				if (!p.empty()) { std::vector<uint8_t> out; TypedFileStored(v.typed, out); std::ofstream f(fs::u8path(p), std::ios::binary); f.write((const char *)out.data(), (std::streamsize)out.size()); v.msg = f ? Fmt(TXT("saved %s"), p.c_str()) : Fmt(TXT("could not write %s"), p.c_str()); if (f) v.dirty = false; }
			}
			ImGui::SameLine();
			if (ImGui::Button(LBL("Put into new PAC"))) { std::vector<uint8_t> out; TypedFileStored(v.typed, out); PacCreateAddMemory(v.name, std::move(out)); }
			ImGui::BeginChild("typed", ImVec2(0, 0), true);
			std::string openGroup; bool groupOpen = false;
			for (size_t ri = 0; ri < v.typed.regions.size(); ri++) {
				TypedRegion &r = v.typed.regions[ri];
				if (!r.group.empty() && r.group != openGroup) {
					if (!openGroup.empty() && groupOpen) ImGui::TreePop();
					openGroup = r.group; groupOpen = ImGui::TreeNode(openGroup.c_str());
				} else if (r.group.empty() && !openGroup.empty()) { if (groupOpen) ImGui::TreePop(); openGroup.clear(); groupOpen = false; }
				if (!r.group.empty() && !groupOpen) continue;
				ImGui::PushID((int)ri);
				if (ImGui::TreeNode(r.title.c_str())) {
					int &sel = v.typedSel[ri];
					if (r.count > 1) {
						ImGui::BeginChild("rows", ImVec2(0, std::min<float>(180.f, 20.f * (float)r.count + 8.f)), true);
						for (size_t i = 0; i < r.count; i++) {
							const uint8_t *rec = v.typed.work.data() + r.offset + i * r.stride;
							char b[64]; std::string cap = r.label ? r.label(rec, i) : (snprintf(b, sizeof b, "%zu", i), std::string(b));
							if (ImGui::Selectable((cap + "##" + std::to_string(i)).c_str(), sel == (int)i)) sel = (int)i;
						}
						ImGui::EndChild();
					}
					if (r.count && (size_t)sel < r.count && r.offset + (size_t)(sel + 1) * r.stride <= v.typed.work.size())
						if (EditRecordFields("rec", v.typed.work.data() + r.offset + (size_t)sel * r.stride, r.fields, r.nfields)) v.dirty = true;
					ImGui::TreePop();
				}
				ImGui::PopID();
			}
			if (!openGroup.empty() && groupOpen) ImGui::TreePop();
			ImGui::EndChild();
		} else {
			ImGui::TextWrapped("%s", TXT("Script bank / data file. Raw view below; strings found (CP932):"));
			if (ImGui::Button(LBL("Export raw..."))) { std::string p = FileDialog(-1, true); if (!p.empty()) { std::ofstream f(fs::u8path(p), std::ios::binary); f.write((const char *)v.bytes.data(), (std::streamsize)v.bytes.size()); v.msg = f ? Fmt(TXT("saved %s"), p.c_str()) : Fmt(TXT("could not write %s"), p.c_str()); } }
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
			if (shown < v.bytes.size()) ImGui::TextDisabled(TXT("... %zu more bytes (use Export raw)"), v.bytes.size() - shown);
			ImGui::EndChild();
		}
		if (!v.msg.empty()) ImGui::TextWrapped("%s", v.msg.c_str());
		ImGui::End();
	}
	for (auto &vp : g_views) if (!vp->open && vp->tex) { glDeleteTextures(1, &vp->tex); vp->tex = 0; }
	g_views.erase(std::remove_if(g_views.begin(), g_views.end(), [](const std::unique_ptr<Viewer> &v) { return !v->open; }), g_views.end());
}

} // namespace han2ui
