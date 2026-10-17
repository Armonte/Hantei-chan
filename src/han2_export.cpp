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

static int16_t RawI16(const Frame &f, int off);

namespace {
// One frame of a pattern on a canvas whose game anchor is (cx, cy): its pose or CG image at the frame offset, flip mode 1 mirrored.
void RenderFrameOn(Canvas &cv, const Frame &f, CG &cg, const Parts &parts, int cx, int cy, int sc)
{
	const int spr = f.han2.valid ? (int)RawI16(f, 0) : -1, ox = RawI16(f, 2), oy = RawI16(f, 4);
	if (spr >= 10000) DrawCgImage(cv, cg, spr - 10000, cx + ox * sc, cy + oy * sc, sc);
	else if (spr >= 0) DrawPose(cv, parts, spr, cx + ox * sc, cy + oy * sc, sc);
	if (f.han2.valid && f.han2.rec[8] == 1) {   // flip mode 1 = mirror about the anchor
		Canvas m(cv.w, cv.h);
		for (int y = 0; y < cv.h; y++) for (int x = 0; x < cv.w; x++) { int sx2 = 2 * cx - 1 - x; if (sx2 >= 0 && sx2 < cv.w) memcpy(&m.px[((size_t)y * cv.w + x) * 4], &cv.px[((size_t)y * cv.w + sx2) * 4], 4); }
		cv = m;
	}
}

// Visible extent of a set of frames around the anchor (left, up, right, down in pixels at scale 1); false when nothing is drawn.
template <class Fn> bool FitExtent(Fn drawAt, int &l, int &u, int &r, int &d)
{
	const int W = 2048, H = 2048, cx = 1024, cy = 1536;
	Canvas cv(W, H);
	drawAt(cv, cx, cy);
	int x0 = W, y0 = H, x1 = -1, y1 = -1;
	for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) if (cv.px[((size_t)y * W + x) * 4 + 3]) { x0 = std::min(x0, x); x1 = std::max(x1, x); y0 = std::min(y0, y); y1 = std::max(y1, y); }
	if (x1 < 0) return false;
	l = std::max(l, cx - x0); r = std::max(r, x1 + 1 - cx); u = std::max(u, cy - y0); d = std::max(d, y1 + 1 - cy);
	return true;
}

std::string SafeFile(const std::string &utf8)
{
	std::string o;
	for (unsigned char c : utf8) o += (c < 0x20 || strchr("<>:\"/\\|?*", c)) ? '_' : (char)c;
	return o.empty() ? "pose" : o;
}
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

static bool ExportIcaroffa(FrameData &fd, CG &cg, Parts &parts, const std::string &outDir, const ExportOptions &opt, ExportReport &rep, std::string *err);

bool ExportCharacter(FrameData &fd, CG &cg, Parts &parts, const std::string &outDir, const ExportOptions &opt, ExportReport &rep, std::string *err)
{
	auto fail = [&](const std::string &m) { if (err) *err = m; return false; };
	std::string e;
	if (!CreateDirectoriesUtf8(outDir, e)) return fail(e);
	if (opt.icaroffaLayout) return ExportIcaroffa(fd, cg, parts, outDir, opt, rep, err);
	const int sc = std::max(1, opt.scale);
	char nm[96];
	// every PNG goes through here: a failed write is counted and the first message kept (never dropped silently)
	auto Png = [&](const std::string &path, const Canvas &cv, std::string &) {
		std::string we;
		if (WritePngRgba(path, cv.px.data(), cv.w, cv.h, we)) return true;
		if (!rep.writeErrors++) rep.notes.push_back(we);
		return false;
	};

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
			int cw = opt.canvasW, ch = opt.canvasH, ax = opt.originX, ay = opt.originY;
			if (cw <= 0) {   // fit this pose (+ margin) around its base point
				int l = 0, u = 0, r = 0, d = 0;
				if (!FitExtent([&](Canvas &c, int x, int y) { DrawPose(c, parts, p, x, y, 1); }, l, u, r, d)) continue;
				cw = l + r + 2 * opt.margin; ch = u + d + 2 * opt.margin; ax = opt.margin + l; ay = opt.margin + u;
			}
			Canvas cv(cw * sc, ch * sc);
			DrawPose(cv, parts, p, ax * sc, ay * sc, sc);
			snprintf(nm, sizeof(nm), "poses/pose_%04d.png", p);
			if (Png(outDir + "/" + nm, cv, e)) rep.posePngs++;
			js << (first ? "" : ",\n") << "  {\"pose\":" << p << ",\"name\":\"" << JsonEsc(ps.name) << "\",\"file\":\"" << nm << "\",\"width\":" << cw * sc << ",\"height\":" << ch * sc << ",\"anchorX\":" << ax * sc << ",\"anchorY\":" << ay * sc << ",\"parts\":[";
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
		std::ostringstream all; all << "{\n \"source\":\"pattern table of the character file (frame order, durations, jumps, loops); not pose-name groups\",\n \"canvas\":"
			<< (opt.canvasW > 0 ? "{\"width\":" + std::to_string(opt.canvasW) + ",\"height\":" + std::to_string(opt.canvasH) + ",\"originX\":" + std::to_string(opt.originX) + ",\"originY\":" + std::to_string(opt.originY) + "}"
			                    : std::string("\"per pattern: sized to fit every frame, see frameWidth / frameHeight / anchorCanvasX / anchorCanvasY in its animation.json\""))
			<< ",\n \"scale\":" << sc << ",\n \"patterns\":[\n"; bool firstP = true;
		for (int p = 0; p < (int)fd.m_sequences.size(); p++) {
			Sequence &seq = fd.m_sequences[p];
			if (seq.frames.empty()) continue;
			char dir[160]; snprintf(dir, sizeof(dir), "patterns/p%03d_%s", p, SafeName(seq.name).c_str());
			CreateDirectoriesUtf8(outDir + "/" + dir, e);
			std::vector<Canvas> frames;
			// one canvas for the whole pattern: fixed (opt.canvasW) or fitted to every frame + margin (icaroffa's aligned renders)
			int cw = opt.canvasW, chh = opt.canvasH, ax = opt.originX, ay = opt.originY;
			if (cw <= 0) {
				int l = 0, u = 0, r = 0, d = 0;
				for (const Frame &f : seq.frames) FitExtent([&](Canvas &c, int x, int y) { RenderFrameOn(c, f, cg, parts, x, y, 1); }, l, u, r, d);
				cw = std::max(64, l + r + 2 * opt.margin); chh = std::max(64, u + d + 2 * opt.margin); ax = opt.margin + l; ay = opt.margin + u;
			}
			std::ostringstream js; js << "{\"pattern\":" << p << ",\"name\":\"" << JsonEsc(seq.name) << "\",\"flags\":" << seq.han2.patFlags
				<< ",\"frameWidth\":" << cw * sc << ",\"frameHeight\":" << chh * sc << ",\"anchorCanvasX\":" << ax * sc << ",\"anchorCanvasY\":" << ay * sc << ",\"frameCount\":" << seq.frames.size() << ",\"frames\":[";
			for (size_t k = 0; k < seq.frames.size(); k++) {
				const Frame &f = seq.frames[k];
				Canvas cv(cw * sc, chh * sc);
				int spr = f.han2.valid ? (int)RawI16(f, 0) : -1, ox = RawI16(f, 2), oy = RawI16(f, 4);
				RenderFrameOn(cv, f, cg, parts, ax * sc, ay * sc, sc);
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
			const int fw = cw * sc, fh = chh * sc, n = (int)frames.size();
			if ((size_t)fw * n <= 16384) {
				Canvas strip(fw * n, fh);
				for (int k = 0; k < n; k++) for (int y = 0; y < fh; y++) memcpy(&strip.px[((size_t)y * strip.w + (size_t)k * fw) * 4], &frames[k].px[(size_t)y * fw * 4], (size_t)fw * 4);
				if (Png(outDir + "/" + dir + "/strip.png", strip, e)) rep.sheets++;
			} else rep.notes.push_back(std::string(dir) + ": strip.png would be wider than 16384 px; sheet.png holds the same frames in rows");
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


bool RenderPoseRgba(const Parts &parts, int pose, std::vector<uint8_t> &rgba, int &w, int &h, int &anchorX, int &anchorY, int pad)
{
	const int W = 2048, H = 2048, cx = 1024, cy = 1536;
	Canvas cv(W, H);
	DrawPose(cv, parts, pose, cx, cy, 1);
	int x0 = W, y0 = H, x1 = -1, y1 = -1;
	for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) if (cv.px[((size_t)y * W + x) * 4 + 3]) { x0 = std::min(x0, x); x1 = std::max(x1, x); y0 = std::min(y0, y); y1 = std::max(y1, y); }
	if (x1 < 0) return false;
	pad = std::max(0, pad);
	x0 = std::max(0, x0 - pad); y0 = std::max(0, y0 - pad); x1 = std::min(W - 1, x1 + pad); y1 = std::min(H - 1, y1 + pad);
	w = x1 - x0 + 1; h = y1 - y0 + 1;
	rgba.resize((size_t)w * h * 4);
	for (int y = 0; y < h; y++) memcpy(&rgba[(size_t)y * w * 4], &cv.px[((size_t)(y0 + y) * W + x0) * 4], (size_t)w * 4);
	anchorX = cx - x0; anchorY = cy - y0;
	return true;
}

bool FindIdleFrame(FrameData &fd, int &pattern, int &frame)
{
	for (int p = 0; p < (int)fd.m_sequences.size() && p < 64; p++) {
		const Sequence &sq = fd.m_sequences[p];
		for (int k = 0; k < (int)sq.frames.size() && k < 4; k++) {
			const Frame &f = sq.frames[k];
			if (f.han2.valid) { if ((int)RawI16(f, 0) >= 0) { pattern = p; frame = k; return true; } }
			else for (const auto &l : f.AF.layers) if (l.spriteId >= 0) { pattern = p; frame = k; return true; }
		}
	}
	return false;
}

bool RenderFrameThumb(FrameData &fd, CG &cg, Parts &parts, int pattern, int frame, int maxSide, std::vector<uint8_t> &rgba, int &w, int &h)
{
	if (pattern < 0 || pattern >= (int)fd.m_sequences.size()) return false;
	const Sequence &sq = fd.m_sequences[pattern];
	if (frame < 0 || frame >= (int)sq.frames.size()) return false;
	const Frame &f = sq.frames[frame];
	const int W = 640, H = 640, cx = 320, cy = 480;
	Canvas cv(W, H);
	if (f.han2.valid) {
		const int spr = (int)RawI16(f, 0), ox = RawI16(f, 2), oy = RawI16(f, 4);
		if (spr >= 10000) DrawCgImage(cv, cg, spr - 10000, cx + ox, cy + oy, 1);
		else if (spr >= 0) DrawPose(cv, parts, spr, cx + ox, cy + oy, 1);
	} else {
		for (const auto &l : f.AF.layers) {
			if (l.spriteId < 0 || l.usePat) continue;
			DrawCgImage(cv, cg, l.spriteId, cx + l.offset_x, cy + l.offset_y, 1);
		}
	}
	int x0 = W, y0 = H, x1 = -1, y1 = -1;
	for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) if (cv.px[((size_t)y * W + x) * 4 + 3]) { x0 = std::min(x0, x); x1 = std::max(x1, x); y0 = std::min(y0, y); y1 = std::max(y1, y); }
	if (x1 < 0) return false;
	const int cw = x1 - x0 + 1, ch = y1 - y0 + 1;
	const float sc = std::min(1.f, (float)maxSide / (float)std::max(cw, ch));
	w = std::max(1, (int)(cw * sc)); h = std::max(1, (int)(ch * sc));
	rgba.assign((size_t)w * h * 4, 0);
	for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {   // box filter over the source footprint of each output pixel (premultiplied average)
		const int sx0 = x0 + (int)(x / sc), sx1 = std::max(sx0 + 1, x0 + (int)((x + 1) / sc)), sy0 = y0 + (int)(y / sc), sy1 = std::max(sy0 + 1, y0 + (int)((y + 1) / sc));
		float r = 0, g = 0, b = 0, a = 0; int n = 0;
		for (int yy = sy0; yy < sy1 && yy < H; yy++) for (int xx = sx0; xx < sx1 && xx < W; xx++) { const uint8_t *s = &cv.px[((size_t)yy * W + xx) * 4]; float al = s[3] / 255.f; r += s[0] * al; g += s[1] * al; b += s[2] * al; a += al; n++; }
		uint8_t *d = &rgba[((size_t)y * w + x) * 4];
		if (n && a > 0.f) { d[0] = (uint8_t)(r / a); d[1] = (uint8_t)(g / a); d[2] = (uint8_t)(b / a); d[3] = (uint8_t)std::min(255.f, a / n * 255.f); }
	}
	return true;
}

// ---- icaroffa / PACNyx+ 2026 layout (key names and folders of his "Export DAT as JSON + PNG"; behaviour reimplemented, no code copied) ----
// manifest.json, poses/<name>.json (40 slots: index, isEmpty, spriteSheetIndex, spriteSheetName, srcRect, destRect, origin, layer, drawOrder,
// rotation (degrees), xScale, yScale, flip, flipValue, color{a,r,g,b}), renders/<name>.png, renders/<group>/NNN_<name>.png,
// renders_aligned/<group>/{NNN_<name>.png, animation.json, strip.png, sheet.png}, animations.json.
// Differences, all documented in docs/formats/pacnyx_parity.md: groups are the game's patterns (frames in game order, CG-image frames included),
// "poseIndex" is the pose slot the frames reference, and duplicate pose names get "_<slot>" so no file is overwritten.
static bool ExportIcaroffa(FrameData &fd, CG &cg, Parts &parts, const std::string &outDir, const ExportOptions &opt, ExportReport &rep, std::string *err)
{
	std::string e;
	for (const char *d : { "/poses", "/renders", "/renders_aligned" }) if (!CreateDirectoriesUtf8(outDir + d, e)) { if (err) *err = e; return false; }
	auto png = [&](const std::string &path, const uint8_t *px, int w, int h) {
		std::string we;
		if (WritePngRgba(path, px, w, h, we)) return true;
		if (!rep.writeErrors++) rep.notes.push_back(we);
		return false;
	};
	auto put = [&](const std::string &path, const std::string &text) { if (!WriteText(path, text)) { if (!rep.writeErrors++) rep.notes.push_back("could not write " + path); } };
	static const char *const flipName[4] = { "None", "Horizontal", "Vertical", "HorizontalAndVertical" };
	// file name per pose
	std::map<int, std::string> poseFile;
	std::map<std::string, int> taken;
	for (int p = 0; p < (int)parts.partSets.size(); p++) {
		const auto &ps = parts.partSets[p];
		bool any = false; for (auto &g : ps.groups) if (g.ppId >= 0) any = true;
		if (!any && ps.name.empty()) continue;
		std::string f = SafeFile(ps.name.empty() ? "pose" + std::to_string(p) : ps.name);
		if (taken.count(f)) f += "_" + std::to_string(p);
		taken[f] = p; poseFile[p] = f;
	}
	// manifest + poses/*.json + renders/*.png
	std::ostringstream man;
	man << "{\n  \"format\": \"rbo-dat-export\",\n  \"version\": 1,\n  \"maxBodyParts\": 40,\n  \"spriteSheets\": [\n";
	for (size_t i = 0; i < parts.gfxMeta.size(); i++) {
		const auto &g = parts.gfxMeta[i];
		man << "    { \"index\": " << i << ", \"name\": \"" << JsonEsc(g.name) << "\"";
		if (g.data) man << ", \"width\": " << g.w << ", \"height\": " << g.h;
		man << " }" << (i + 1 < parts.gfxMeta.size() ? "," : "") << "\n";
	}
	man << "  ],\n  \"poses\": [\n";
	size_t nDone = 0;
	for (auto &kv : poseFile) {
		const int p = kv.first; const std::string &f = kv.second;
		const auto &ps = parts.partSets[p];
		// draw order: icaroffa numbers parts by descending layer; ours draws ascending (layer << 8) + slot, last on top
		std::vector<std::pair<int, int>> order;
		for (size_t k = 0; k < ps.groups.size() && k < 40; k++) if (ps.groups[k].ppId >= 0) order.push_back({ (int)floorf(ps.groups[k].priority + 1e-4f) * 256 + (int)k, (int)k });
		std::sort(order.begin(), order.end());
		std::map<int, int> drawOrder; for (size_t i = 0; i < order.size(); i++) drawOrder[order[i].second] = (int)i;
		std::ostringstream js;
		js << "{\n  \"poseName\": \"" << JsonEsc(ps.name) << "\",\n  \"poseIndex\": " << p << ",\n  \"partCount\": 40,\n  \"parts\": [\n";
		for (int k = 0; k < 40; k++) {
			const PartProperty *g = k < (int)ps.groups.size() && ps.groups[k].ppId >= 0 && ps.groups[k].ppId < (int)parts.cutOuts.size() ? &ps.groups[k] : nullptr;
			js << "    {\n      \"index\": " << k << ",\n      \"isEmpty\": " << (g ? "false," : "true") << "\n";
			if (g) {
				const auto &co = parts.cutOuts[g->ppId];
				const int res = co.texture >= 0 && co.texture < (int)parts.gfxMeta.size() && parts.gfxMeta[co.texture].w > 0 ? parts.gfxMeta[co.texture].w : 256;
				auto px = [&](int units) { return (int)(units * (res / 256.0)); };   // source rect units are 1/256 of the sheet
				js << "      \"spriteSheetIndex\": " << co.texture << ",\n      \"spriteSheetName\": \"" << JsonEsc(co.texture >= 0 && co.texture < (int)parts.gfxMeta.size() ? parts.gfxMeta[co.texture].name : std::string()) << "\",\n"
				   << "      \"srcRect\": { \"x\": " << px(co.uv[0]) << ", \"y\": " << px(co.uv[1]) << ", \"width\": " << px(co.uv[2]) << ", \"height\": " << px(co.uv[3]) << " },\n"
				   << "      \"destRect\": { \"x\": " << g->x << ", \"y\": " << g->y << ", \"width\": " << co.wh[0] << ", \"height\": " << co.wh[1] << " },\n"
				   << "      \"origin\": { \"x\": " << co.xy[0] << ", \"y\": " << co.xy[1] << " },\n"
				   << "      \"layer\": " << (int)floorf(g->priority + 1e-4f) << ",\n      \"drawOrder\": " << drawOrder[k] << ",\n"
				   << "      \"rotation\": " << Num(g->rotation[3] * 360.f) << ",\n      \"xScale\": " << Num(g->scaleX) << ",\n      \"yScale\": " << Num(g->scaleY) << ",\n"
				   << "      \"flip\": \"" << flipName[g->flip & 3] << "\",\n      \"flipValue\": " << (g->flip & 3) << ",\n"
				   << "      \"color\": { \"a\": " << (int)g->bgra[3] << ", \"r\": " << (int)g->bgra[2] << ", \"g\": " << (int)g->bgra[1] << ", \"b\": " << (int)g->bgra[0] << " }\n";
			}
			js << "    }" << (k < 39 ? "," : "") << "\n";
		}
		js << "  ]\n}\n";
		put(outDir + "/poses/" + f + ".json", js.str());
		std::vector<uint8_t> px; int w = 0, h = 0, ax = 0, ay = 0;
		if (RenderPoseRgba(parts, p, px, w, h, ax, ay, 1) && png(outDir + "/renders/" + f + ".png", px.data(), w, h)) rep.posePngs++;
		man << "    {\n      \"index\": " << p << ",\n      \"name\": \"" << JsonEsc(ps.name) << "\",\n      \"file\": \"poses/" << JsonEsc(f) << ".json\",\n      \"render\": \"renders/" << JsonEsc(f) << ".png\"\n    }"
		    << (++nDone < poseFile.size() ? "," : "") << "\n";
	}
	man << "  ]\n}\n";
	put(outDir + "/manifest.json", man.str());

	// groups = patterns: renders/<group>/NNN_<pose>.png, renders_aligned/<group>/ + animation.json / strip.png / sheet.png, animations.json
	std::ostringstream an; an << "{\n  \"animations\": [\n"; bool firstA = true;
	for (int p = 0; p < (int)fd.m_sequences.size(); p++) {
		Sequence &seq = fd.m_sequences[p];
		if (seq.frames.empty()) continue;
		char gname[160]; snprintf(gname, sizeof gname, "%03d_%s", p, SafeFile(seq.name.empty() ? "pattern" : seq.name).c_str());
		const std::string raw = outDir + "/renders/" + gname, ali = outDir + "/renders_aligned/" + gname;
		CreateDirectoriesUtf8(raw, e); CreateDirectoriesUtf8(ali, e);
		int l = 0, u = 0, r = 0, d = 0;
		for (const Frame &f : seq.frames) FitExtent([&](Canvas &c, int x, int y) { RenderFrameOn(c, f, cg, parts, x, y, 1); }, l, u, r, d);
		const int M = opt.margin, fw = std::max(64, l + r + 2 * M), fh = std::max(64, u + d + 2 * M), ax = M + l, ay = M + u;
		std::ostringstream aj;
		aj << "{\n  \"name\": \"" << JsonEsc(seq.name) << "\",\n  \"frameWidth\": " << fw << ",\n  \"frameHeight\": " << fh << ",\n  \"anchorCanvasX\": " << ax << ",\n  \"anchorCanvasY\": " << ay
		   << ",\n  \"frameCount\": " << seq.frames.size() << ",\n  \"frames\": [\n";
		an << (firstA ? "" : ",\n") << "    {\n      \"name\": \"" << JsonEsc(seq.name.empty() ? std::string(gname) : seq.name) << "\",\n      \"frames\": [\n"; firstA = false;
		std::vector<Canvas> cvs;
		for (size_t k = 0; k < seq.frames.size(); k++) {
			const Frame &f = seq.frames[k];
			const int spr = f.han2.valid ? (int)RawI16(f, 0) : -1;
			std::string label = spr >= 10000 ? "cg" + std::to_string(spr - 10000) : spr >= 0 && poseFile.count(spr) ? poseFile[spr] : "empty";
			char fn[200]; snprintf(fn, sizeof fn, "%03zu_%s.png", k, label.c_str());
			Canvas cv(fw, fh);
			RenderFrameOn(cv, f, cg, parts, ax, ay, 1);
			if (png(ali + "/" + fn, cv.px.data(), cv.w, cv.h)) rep.framePngs++;
			{   // the same frame cropped (icaroffa's renders/<group>/ holds the pose pictures)
				int x0 = cv.w, y0 = cv.h, x1 = -1, y1 = -1;
				for (int y = 0; y < cv.h; y++) for (int x = 0; x < cv.w; x++) if (cv.px[((size_t)y * cv.w + x) * 4 + 3]) { x0 = std::min(x0, x); x1 = std::max(x1, x); y0 = std::min(y0, y); y1 = std::max(y1, y); }
				if (x1 >= 0) { Canvas c2(x1 - x0 + 1, y1 - y0 + 1); for (int y = 0; y < c2.h; y++) memcpy(&c2.px[(size_t)y * c2.w * 4], &cv.px[((size_t)(y0 + y) * cv.w + x0) * 4], (size_t)c2.w * 4); png(raw + "/" + fn, c2.px.data(), c2.w, c2.h); }
			}
			aj << "    {\n      \"index\": " << k << ",\n      \"poseIndex\": " << (spr >= 0 && spr < 10000 ? spr : -1) << ",\n      \"poseName\": \"" << JsonEsc(spr >= 0 && spr < 10000 && spr < (int)parts.partSets.size() ? parts.partSets[spr].name : label)
			   << "\",\n      \"file\": \"" << fn << "\",\n      \"durationTicks\": " << f.AF.duration << "\n    }" << (k + 1 < seq.frames.size() ? "," : "") << "\n";
			an << "        {\n          \"index\": " << (spr >= 0 && spr < 10000 ? spr : -1) << ",\n          \"name\": \"" << JsonEsc(label) << "\",\n          \"json\": \"" << (spr >= 0 && spr < 10000 && poseFile.count(spr) ? "poses/" + JsonEsc(label) + ".json" : std::string())
			   << "\",\n          \"render\": \"renders/" << gname << "/" << fn << "\"\n        }" << (k + 1 < seq.frames.size() ? "," : "") << "\n";
			cvs.push_back(std::move(cv));
		}
		aj << "  ]\n}\n";
		an << "      ]\n    }";
		put(ali + "/animation.json", aj.str());
		const int n = (int)cvs.size();
		if ((size_t)fw * n <= 16384) {
			Canvas strip(fw * n, fh);
			for (int k = 0; k < n; k++) for (int y = 0; y < fh; y++) memcpy(&strip.px[((size_t)y * strip.w + (size_t)k * fw) * 4], &cvs[k].px[(size_t)y * fw * 4], (size_t)fw * 4);
			if (png(ali + "/strip.png", strip.px.data(), strip.w, strip.h)) rep.sheets++;
		} else rep.notes.push_back(std::string(gname) + ": strip.png would be wider than 16384 px; sheet.png holds the same frames");
		const int cols = (int)std::ceil(std::sqrt((double)n)), rows = (n + cols - 1) / cols;   // icaroffa's grid: ceil(sqrt(n)) columns
		Canvas sheet(fw * cols, fh * rows);
		for (int k = 0; k < n; k++) for (int y = 0; y < fh; y++) memcpy(&sheet.px[((size_t)((k / cols) * fh + y) * sheet.w + (size_t)(k % cols) * fw) * 4], &cvs[k].px[(size_t)y * fw * 4], (size_t)fw * 4);
		if (png(ali + "/sheet.png", sheet.px.data(), sheet.w, sheet.h)) rep.sheets++;
		rep.patternsWritten++;
	}
	an << "\n  ]\n}\n";
	put(outDir + "/animations.json", an.str());
	if (opt.cgImages && cg.get_image_count() > 0) {   // not in icaroffa's export (RBO effects); kept so nothing is lost
		CreateDirectoriesUtf8(outDir + "/cg", e);
		for (int i = 0; i < cg.get_image_count(); i++) {
			ImageData *im = cg.draw_texture((unsigned)i, false, false);
			if (!im) continue;
			char nm[64]; snprintf(nm, sizeof nm, "/cg/cg_%04d.png", i);
			if (png(outDir + nm, im->pixels, im->width, im->height)) rep.cgPngs++;
			delete im;
		}
	}
	return true;
}

} // namespace han2

