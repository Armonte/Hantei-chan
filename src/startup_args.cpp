// MainFrame startup actions (--open / --capture); see startup_args.h.
#include "startup_args.h"
#include "main_frame.h"
#include "character_instance.h"

#include <glad/glad.h>
#include <windows.h>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cctype>

StartupArgs gStartup;

namespace {

uint32_t Crc(const uint8_t *d, size_t n, uint32_t c = 0xFFFFFFFFu)
{
	static uint32_t t[256]; static bool init = false;
	if (!init) { for (uint32_t i = 0; i < 256; i++) { uint32_t v = i; for (int k = 0; k < 8; k++) v = (v & 1) ? 0xEDB88320u ^ (v >> 1) : v >> 1; t[i] = v; } init = true; }
	for (size_t i = 0; i < n; i++) c = t[(c ^ d[i]) & 0xFF] ^ (c >> 8);
	return c;
}

// Uncompressed (stored-deflate) RGB PNG.
void WritePng(const std::string &path, const uint8_t *rgb, int w, int h)
{
	std::vector<uint8_t> raw;
	for (int y = 0; y < h; y++) { raw.push_back(0); raw.insert(raw.end(), rgb + (size_t)y * w * 3, rgb + (size_t)(y + 1) * w * 3); }
	std::vector<uint8_t> z{0x78, 0x01};
	for (size_t pos = 0; pos < raw.size();) {
		size_t n = std::min<size_t>(65535, raw.size() - pos);
		z.push_back(pos + n >= raw.size());
		z.push_back(n & 0xFF); z.push_back(n >> 8); z.push_back(~n & 0xFF); z.push_back((~n >> 8) & 0xFF);
		z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + n);
		pos += n;
	}
	uint32_t a = 1, b = 0;
	for (uint8_t v : raw) { a = (a + v) % 65521; b = (b + a) % 65521; }
	uint32_t ad = (b << 16) | a;
	for (int s = 24; s >= 0; s -= 8) z.push_back((ad >> s) & 0xFF);

	std::ofstream f(path, std::ios::binary);
	auto be = [&](uint32_t v) { uint8_t x[4] = {uint8_t(v >> 24), uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v)}; f.write((char *)x, 4); };
	auto chunk = [&](const char *type, const std::vector<uint8_t> &d) {
		be((uint32_t)d.size());
		std::vector<uint8_t> td(type, type + 4); td.insert(td.end(), d.begin(), d.end());
		f.write((const char *)td.data(), td.size());
		be(Crc(td.data(), td.size()) ^ 0xFFFFFFFFu);
	};
	f.write("\x89PNG\r\n\x1a\n", 8);
	std::vector<uint8_t> ihdr = {uint8_t(w >> 24), uint8_t(w >> 16), uint8_t(w >> 8), uint8_t(w),
	                             uint8_t(h >> 24), uint8_t(h >> 16), uint8_t(h >> 8), uint8_t(h), 8, 2, 0, 0, 0};
	chunk("IHDR", ihdr); chunk("IDAT", z); chunk("IEND", {});
}

} // namespace

void MainFrame::ProcessStartupArgs()
{
	int n = ++gStartup.frameCounter;
	if (n == 2 && !gStartup.open.empty()) {
		std::string path = gStartup.open, ext;
		size_t dot = path.find_last_of('.');
		if (dot != std::string::npos) ext = path.substr(dot);
		for (auto &c : ext) c = (char)tolower((unsigned char)c);
		if (ext == ".hproj") {
			loadProjectFromPath(path, false);
		} else {
			auto character = std::make_unique<CharacterInstance>();
			bool ok = ext == ".txt" ? character->loadFromTxt(path) : character->loadHA6(path, false);
			if (ok) {
				characters.push_back(std::move(character));
				createViewForCharacter(characters.back().get());
			} else {
				requestErrorPopup("Load Error", "Could not open " + path);
			}
		}
		if (auto *v = getActiveView()) {
			if (gStartup.pattern >= 0) v->getState().pattern = gStartup.pattern;
			if (gStartup.frame >= 0) v->getState().frame = gStartup.frame;
		}
		const std::string& t = gStartup.tool;
		if (t == "hud") m_showHud = true;
		else if (t == "bgm") m_showBgm = true;
		else if (t == "compare") m_showCompare = true;
		else if (t == "patterns") m_patMgr.open = true;
		else if (t == "notes") m_showNotes = true;
		else if (t == "vars") m_varRefs.open = true;
		else if (t == "keys") m_showKeyBindings = true;
		if (auto *c = getActiveCharacter(); c && gStartup.compare >= 0) {
			m_compare.enabled = true;
			m_compare.character = c;
			m_compare.pattern = gStartup.compare;
			m_compare.offsetX = 60;
		}
		if (auto *c = getActiveCharacter(); c && gStartup.palette >= 0) {
			c->palette = gStartup.palette;
			c->cg.changePaletteNumber(gStartup.palette);
		}
	}
	if (!gStartup.capture.empty() && n == 20) {
		RECT r; GetClientRect(WindowFromDC(context->dc), &r);
		int w = r.right - r.left, h = r.bottom - r.top;
		std::vector<uint8_t> rgba((size_t)w * h * 4), rgb((size_t)w * h * 3);
		glReadBuffer(GL_BACK);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
		for (int y = 0; y < h; y++)
			for (int x = 0; x < w; x++)
				for (int k = 0; k < 3; k++)
					rgb[((size_t)(h - 1 - y) * w + x) * 3 + k] = rgba[((size_t)y * w + x) * 4 + k];
		WritePng(gStartup.capture, rgb.data(), w, h);
		PostQuitMessage(0);
	}
}
