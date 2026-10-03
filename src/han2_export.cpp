#include "han2_export.h"
#include "framedata.h"
#include "framedata_han2.h"
#include "cg.h"
#include "parts/parts.h"
#include "png_writer.h"
#include "misc.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

namespace han2 {

namespace {

struct Canvas {
	int w = 0, h = 0;
	std::vector<uint8_t> px;   // RGBA, straight alpha
	Canvas(int W, int H) : w(W), h(H), px((size_t)W * H * 4, 0) {}
	void blend(int x, int y, float r, float g, float b, float a)   // all 0..255 / alpha 0..1, source-over
	{
		if (x < 0 || y < 0 || x >= w || y >= h || a <= 0.f) return;
		uint8_t *d = &px[((size_t)y * w + x) * 4];
		float da = d[3] / 255.f, oa = a + da * (1.f - a);
		if (oa <= 0.f) return;
		d[0] = (uint8_t)std::clamp((r * a + d[0] * da * (1.f - a)) / oa, 0.f, 255.f);
		d[1] = (uint8_t)std::clamp((g * a + d[1] * da * (1.f - a)) / oa, 0.f, 255.f);
		d[2] = (uint8_t)std::clamp((b * a + d[2] * da * (1.f - a)) / oa, 0.f, 255.f);
		d[3] = (uint8_t)std::clamp(oa * 255.f, 0.f, 255.f);
	}
};

std::string JsonEsc(const std::string &s)
{
	std::string o;
	for (unsigned char c : s) {
		if (c == '"') o += "\\\""; else if (c == '\\') o += "\\\\"; else if (c == '\n') o += "\\n"; else if (c < 0x20) { char b[8]; snprintf(b, sizeof(b), "\\u%04x", c); o += b; } else o += (char)c;
	}
	return o;
}

std::string SafeName(const std::string &s)
{
	std::string o;
	for (unsigned char c : s) o += (c < 0x20 || strchr("<>:\"/\\|?*", c)) ? '_' : (char)c;
	while (!o.empty() && (o.back() == ' ' || o.back() == '.')) o.pop_back();
	return o.empty() ? "unnamed" : o;
}

// ---- part (pose) compositing -----------------------------------------------------------------------------------------
struct PartDraw {
	const uint8_t *tex = nullptr; int res = 0;
	float x = 0, y = 0, dw = 0, dh = 0, ox = 0, oy = 0, sx = 1, sy = 1, rotTurns = 0;
	int flip = 0; float su = 0, sv = 0, sw = 0, sh = 0;     // source rect in 1/256 texture units
	uint8_t bgra[4]{255, 255, 255, 255}; float add[3]{};
	float priority = 0;
};

void DrawPart(Canvas &cv, const PartDraw &p, int originX, int originY, int scale)
{
	if (!p.tex || p.dw == 0 || p.dh == 0) return;
	const float th = p.rotTurns * 6.2831853f, c = cosf(th), s = sinf(th);
	// corners of the quad in canvas space (pivot P = part position + origin)
	const float px = p.x + p.ox, py = p.y + p.oy;
	auto fwd = [&](float lx, float ly, float &ox_, float &oy_) {
		float ux = (lx - p.ox) * p.sx, uy = (ly - p.oy) * p.sy;
		ox_ = (px + ux * c - uy * s) * scale + originX; oy_ = (py + ux * s + uy * c) * scale + originY;
	};
	float cx[4], cy[4]; fwd(0, 0, cx[0], cy[0]); fwd(p.dw, 0, cx[1], cy[1]); fwd(p.dw, p.dh, cx[2], cy[2]); fwd(0, p.dh, cx[3], cy[3]);
	int minx = (int)floorf(std::min({cx[0], cx[1], cx[2], cx[3]})), maxx = (int)ceilf(std::max({cx[0], cx[1], cx[2], cx[3]}));
	int miny = (int)floorf(std::min({cy[0], cy[1], cy[2], cy[3]})), maxy = (int)ceilf(std::max({cy[0], cy[1], cy[2], cy[3]}));
	minx = std::max(minx, 0); miny = std::max(miny, 0); maxx = std::min(maxx, cv.w - 1); maxy = std::min(maxy, cv.h - 1);
	const float isx = p.sx == 0 ? 0 : 1.f / p.sx, isy = p.sy == 0 ? 0 : 1.f / p.sy;
	for (int y = miny; y <= maxy; y++)
		for (int x = minx; x <= maxx; x++) {
			// inverse map the pixel centre to part-local coordinates
			float dx = ((x + 0.5f - originX) / scale) - px, dy = ((y + 0.5f - originY) / scale) - py;
			float ux = dx * c + dy * s, uy = -dx * s + dy * c;
			float lx = ux * isx + p.ox, ly = uy * isy + p.oy;
			if (lx < 0 || ly < 0 || lx >= p.dw || ly >= p.dh) continue;
			float u = lx / p.dw, v = ly / p.dh;
			if (p.flip & 1) u = 1.f - u;
			if (p.flip & 2) v = 1.f - v;
			float tx = (p.su + u * p.sw) / 256.f * p.res - 0.5f, ty = (p.sv + v * p.sh) / 256.f * p.res - 0.5f;
			int x0 = (int)floorf(tx), y0 = (int)floorf(ty); float fx = tx - x0, fy = ty - y0;
			float rgba[4] = {0, 0, 0, 0};
			for (int k = 0; k < 4; k++) {
				int xi = std::clamp(x0 + (k & 1), 0, p.res - 1), yi = std::clamp(y0 + (k >> 1), 0, p.res - 1);
				float wgt = ((k & 1) ? fx : 1 - fx) * ((k >> 1) ? fy : 1 - fy);
				const uint8_t *t = p.tex + ((size_t)yi * p.res + xi) * 4;   // B,G,R,A
				rgba[0] += t[2] * wgt; rgba[1] += t[1] * wgt; rgba[2] += t[0] * wgt; rgba[3] += t[3] * wgt;
			}
			// modulate (bytes A,R,G,B) and additive tint
			float a = rgba[3] / 255.f * (p.bgra[3] / 255.f);
			float r = std::min(255.f, rgba[0] * (p.bgra[2] / 255.f) + p.add[0] * 255.f);
			float g = std::min(255.f, rgba[1] * (p.bgra[1] / 255.f) + p.add[1] * 255.f);
			float b = std::min(255.f, rgba[2] * (p.bgra[0] / 255.f) + p.add[2] * 255.f);
			cv.blend(x, y, r, g, b, a);
		}
}

void DrawPose(Canvas &cv, const Parts &parts, int pose, int ox, int oy, int scale)
{
	if (pose < 0 || pose >= (int)parts.partSets.size()) return;
	std::vector<PartDraw> draws;
	for (const auto &pr : parts.partSets[pose].groups) {
		if (pr.ppId < 0 || pr.ppId >= (int)parts.cutOuts.size()) continue;
		const auto &co = parts.cutOuts[pr.ppId];
		if (co.texture < 0 || co.texture >= (int)parts.gfxMeta.size() || !parts.gfxMeta[co.texture].data) continue;
		PartDraw d;
		d.tex = (const uint8_t *)parts.gfxMeta[co.texture].data; d.res = parts.gfxMeta[co.texture].w;
		d.x = (float)pr.x; d.y = (float)pr.y; d.dw = (float)co.wh[0]; d.dh = (float)co.wh[1];
		d.ox = (float)co.xy[0]; d.oy = (float)co.xy[1]; d.sx = pr.scaleX; d.sy = pr.scaleY; d.rotTurns = pr.rotation[3];
		d.flip = pr.flip; d.su = (float)co.uv[0]; d.sv = (float)co.uv[1]; d.sw = (float)co.uv[2]; d.sh = (float)co.uv[3];
		memcpy(d.bgra, pr.bgra, 4); d.add[0] = pr.addColor[2]; d.add[1] = pr.addColor[1]; d.add[2] = pr.addColor[0];
		d.priority = pr.priority;
		draws.push_back(d);
	}
	std::stable_sort(draws.begin(), draws.end(), [](const PartDraw &a, const PartDraw &b) { return a.priority < b.priority; });
	for (auto &d : draws) DrawPart(cv, d, ox, oy, scale);
}

void DrawCgImage(Canvas &cv, CG &cg, int image, int ox, int oy, int scale)
{
	ImageData *im = cg.draw_texture((unsigned)image, false, false);
	if (!im) return;
	for (int y = 0; y < im->height; y++)
		for (int x = 0; x < im->width; x++) {
			const uint8_t *s = im->pixels + ((size_t)y * im->width + x) * 4;
			for (int yy = 0; yy < scale; yy++)
				for (int xx = 0; xx < scale; xx++)
					cv.blend(ox + (im->offsetX + x) * scale + xx, oy + (im->offsetY + y) * scale + yy, s[0], s[1], s[2], s[3] / 255.f);
		}
	delete im;
}

bool Png(const std::string &path, const Canvas &cv, std::string &err) { return WritePngRgba(path, cv.px.data(), cv.w, cv.h, err); }

std::string Num(float v) { char b[32]; snprintf(b, sizeof(b), "%g", v); return b; }

} // namespace

static bool WriteText(const std::string &utf8Path, const std::string &text)
{
	FILE *f = _wfopen(Utf8ToWide(utf8Path).c_str(), L"wb");
	if (!f) return false;
	bool ok = fwrite(text.data(), 1, text.size(), f) == text.size();
	fclose(f);
	return ok;
}

static int16_t RawI16(const Frame &f, int off) { int16_t v = 0; if (f.han2.valid) memcpy(&v, f.han2.rec + off, 2); return v; }

bool ExportCharacter(FrameData &fd, CG &cg, Parts &parts, const std::string &outDir, const ExportOptions &opt, ExportReport &rep, std::string *err)
{
	auto fail = [&](const std::string &m) { if (err) *err = m; return false; };
	std::string e;
	if (!CreateDirectoriesUtf8(outDir, e)) return fail(e);
	const int sc = std::max(1, opt.scale);
	char nm[96];

	// ---- CG images ----
	if (opt.cgImages && cg.get_image_count() > 0) {
		CreateDirectoriesUtf8(outDir + "/cg", e);
		std::ostringstream js; js << "[\n"; bool first = true;
		for (int i = 0; i < cg.get_image_count(); i++) {
			ImageData *im = cg.draw_texture((unsigned)i, false, false);
			if (!im) continue;
			Canvas cv(im->width, im->height);
			memcpy(cv.px.data(), im->pixels, (size_t)im->width * im->height * 4);
			snprintf(nm, sizeof(nm), "cg/cg_%04d.png", i);
			if (Png(outDir + "/" + nm, cv, e)) {
				rep.cgPngs++;
				js << (first ? "" : ",\n") << "  {\"image\":" << i << ",\"file\":\"" << nm << "\",\"canvasX\":" << im->offsetX << ",\"canvasY\":" << im->offsetY << ",\"width\":" << im->width << ",\"height\":" << im->height << "}";
				first = false;
			}
			delete im;
		}
		js << "\n]\n";
		WriteText(outDir + "/cg.json", js.str());
	}

	// ---- poses: PNG per pose (tight bounds, anchor-aligned canvas) + poses.json with every part field ----
	if (opt.poses && parts.loaded && !parts.partSets.empty()) {
		CreateDirectoriesUtf8(outDir + "/poses", e);
		std::ostringstream js; js << "{\n \"anchor\":\"bottom centre of a 640x480 screen; part x,y are pixels relative to it\",\n \"poses\":[\n"; bool first = true;
		for (int p = 0; p < (int)parts.partSets.size(); p++) {
			const auto &ps = parts.partSets[p];
			bool any = false; for (auto &g : ps.groups) if (g.ppId >= 0) any = true;
			if (!any) continue;
			Canvas cv(opt.canvasW * sc, opt.canvasH * sc);
			DrawPose(cv, parts, p, opt.originX * sc, opt.originY * sc, sc);
			snprintf(nm, sizeof(nm), "poses/pose_%04d.png", p);
			if (Png(outDir + "/" + nm, cv, e)) rep.posePngs++;
			js << (first ? "" : ",\n") << "  {\"pose\":" << p << ",\"name\":\"" << JsonEsc(ps.name) << "\",\"file\":\"" << nm << "\",\"parts\":[";
			bool pf = true;
			for (size_t k = 0; k < ps.groups.size(); k++) {
				const auto &g = ps.groups[k];
				if (g.ppId < 0 || g.ppId >= (int)parts.cutOuts.size()) continue;
				const auto &co = parts.cutOuts[g.ppId];
				js << (pf ? "" : ",") << "\n    {\"slot\":" << k << ",\"x\":" << g.x << ",\"y\":" << g.y << ",\"destWidth\":" << co.wh[0] << ",\"destHeight\":" << co.wh[1]
				   << ",\"origin\":[" << co.xy[0] << "," << co.xy[1] << "],\"scale\":[" << Num(g.scaleX) << "," << Num(g.scaleY) << "],\"rotationTurns\":" << Num(g.rotation[3])
				   << ",\"flip\":" << g.flip << ",\"layer\":" << (int)floorf(g.priority + 1e-4f) << ",\"texture\":" << co.texture
				   << ",\"srcRect256\":[" << co.uv[0] << "," << co.uv[1] << "," << co.uv[2] << "," << co.uv[3] << "],\"argb\":[" << (int)g.bgra[3] << "," << (int)g.bgra[2] << "," << (int)g.bgra[1] << "," << (int)g.bgra[0] << "]}";
				pf = false;
			}
			js << "\n  ]}";
			first = false;
		}
		js << "\n ]\n}\n";
		WriteText(outDir + "/poses.json", js.str());
	}

	// ---- patterns: the game's own animations ----
	if (opt.patterns) {
		CreateDirectoriesUtf8(outDir + "/patterns", e);
		std::ostringstream all; all << "{\n \"source\":\"pattern table of the character file (frame order, durations, jumps, loops); not pose-name groups\",\n \"canvas\":{\"width\":"
			<< opt.canvasW << ",\"height\":" << opt.canvasH << ",\"originX\":" << opt.originX << ",\"originY\":" << opt.originY << ",\"scale\":" << sc << "},\n \"patterns\":[\n"; bool firstP = true;
		for (int p = 0; p < (int)fd.m_sequences.size(); p++) {
			Sequence &seq = fd.m_sequences[p];
			if (seq.frames.empty()) continue;
			char dir[160]; snprintf(dir, sizeof(dir), "patterns/p%03d_%s", p, SafeName(seq.name).c_str());
			CreateDirectoriesUtf8(outDir + "/" + dir, e);
			std::vector<Canvas> frames;
			std::ostringstream js; js << "{\"pattern\":" << p << ",\"name\":\"" << JsonEsc(seq.name) << "\",\"flags\":" << seq.han2.patFlags << ",\"frames\":[";
			for (size_t k = 0; k < seq.frames.size(); k++) {
				const Frame &f = seq.frames[k];
				Canvas cv(opt.canvasW * sc, opt.canvasH * sc);
				int spr = f.han2.valid ? (int)RawI16(f, 0) : -1, ox = RawI16(f, 2), oy = RawI16(f, 4);
				const int cx = opt.originX * sc, cy = opt.originY * sc;
				if (spr >= 10000) DrawCgImage(cv, cg, spr - 10000, cx + ox * sc, cy + oy * sc, sc);
				else if (spr >= 0) { Canvas tmp(cv.w, cv.h); DrawPose(tmp, parts, spr, cx + ox * sc, cy + oy * sc, sc); cv = tmp; }
				if (f.han2.valid && f.han2.rec[8] == 1) {   // flip mode 1 = mirror about the anchor
					Canvas m(cv.w, cv.h);
					for (int y = 0; y < cv.h; y++) for (int x = 0; x < cv.w; x++) { int sx2 = 2 * cx - 1 - x; if (sx2 >= 0 && sx2 < cv.w) memcpy(&m.px[((size_t)y * cv.w + x) * 4], &cv.px[((size_t)y * cv.w + sx2) * 4], 4); }
					cv = m;
				}
				snprintf(nm, sizeof(nm), "%s/f%03zu.png", dir, k);
				if (Png(outDir + "/" + nm, cv, e)) rep.framePngs++;
				js << (k ? "," : "") << "\n  {\"index\":" << k << ",\"file\":\"" << nm << "\",\"sprite\":{\"kind\":\"" << (spr >= 10000 ? "cg" : "pose") << "\",\"id\":" << (spr >= 10000 ? spr - 10000 : spr)
				   << "},\"offset\":[" << ox << "," << oy << "],\"durationTicks\":" << f.AF.duration << ",\"aniFlag\":" << (f.han2.valid ? (int)f.han2.rec[0x0B] : -1)
				   << ",\"jumpTarget\":" << (f.han2.valid ? (int)f.han2.rec[0x0C] : -1) << ",\"landJumpTarget\":" << (f.han2.valid ? (int)f.han2.rec[0x0D] : -1)
				   << ",\"loopCount\":" << (f.han2.valid ? (int)f.han2.rec[0x12] : 0) << ",\"loopEndFrame\":" << (f.han2.valid ? (int)f.han2.rec[0x13] : 0)
				   << ",\"hasAttack\":" << (f.han2.hadAT ? "true" : "false") << ",\"boxes\":[";
				bool bf = true;
				for (auto &kv : f.hitboxes) { js << (bf ? "" : ",") << "{\"slot\":" << kv.first << ",\"rect\":[" << kv.second.xy[0] << "," << kv.second.xy[1] << "," << kv.second.xy[2] << "," << kv.second.xy[3] << "]}"; bf = false; }
				js << "]}";
				frames.push_back(std::move(cv));
			}
			std::vector<std::pair<int, int>> visits; std::string endNote;
			SimulateFlow(seq, visits, endNote);
			js << "\n ],\"flow\":{\"visits\":[";
			int ticks = 0;
			for (size_t v = 0; v < visits.size() && v < 400; v++) { js << (v ? "," : "") << "[" << visits[v].first << "," << visits[v].second << "]"; ticks += visits[v].second; }
			js << "],\"totalTicks\":" << ticks << ",\"note\":\"" << JsonEsc(endNote) << "\"}}\n";
			WriteText(outDir + "/" + dir + "/animation.json", js.str());
			all << (firstP ? "" : ",\n") << "  {\"pattern\":" << p << ",\"name\":\"" << JsonEsc(seq.name) << "\",\"dir\":\"" << dir << "\",\"frames\":" << seq.frames.size() << ",\"totalTicks\":" << ticks << "}";
			firstP = false;
			// strip.png (all frames side by side) and sheet.png (grid, 8 columns)
			const int fw = opt.canvasW * sc, fh = opt.canvasH * sc, n = (int)frames.size();
			Canvas strip(fw * n, fh);
			for (int k = 0; k < n; k++) for (int y = 0; y < fh; y++) memcpy(&strip.px[((size_t)y * strip.w + (size_t)k * fw) * 4], &frames[k].px[(size_t)y * fw * 4], (size_t)fw * 4);
			if (strip.w <= 16384 && Png(outDir + "/" + dir + "/strip.png", strip, e)) rep.sheets++;
			const int cols = std::min(n, 8), rows = (n + cols - 1) / cols;
			Canvas sheet(fw * cols, fh * rows);
			for (int k = 0; k < n; k++) for (int y = 0; y < fh; y++) memcpy(&sheet.px[((size_t)((k / cols) * fh + y) * sheet.w + (size_t)(k % cols) * fw) * 4], &frames[k].px[(size_t)y * fw * 4], (size_t)fw * 4);
			if (Png(outDir + "/" + dir + "/sheet.png", sheet, e)) rep.sheets++;
			rep.patternsWritten++;
		}
		all << "\n ]\n}\n";
		WriteText(outDir + "/animations.json", all.str());
	}
	return true;
}

} // namespace han2
