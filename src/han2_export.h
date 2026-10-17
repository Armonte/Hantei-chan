#ifndef HAN2_EXPORT_H_GUARD
#define HAN2_EXPORT_H_GUARD

// Sprite / pose / animation export for French-Bread RBO (and GOF2) characters. Reimplements the behaviour of icaroffa's PACNyx+
// 2026 export (pose JSON + PNG, animation JSON, aligned strip and sheet) from the decompiled build as a reference (no code copied),
// with one correction: animations are the game's real pattern table (frame order, durations, jumps, loops), not pose-name groups.
// Rendering is a plain CPU compositor so it works headless (han2tool export) and in the editor.

#include <cstdint>
#include <string>
#include <vector>

class FrameData;
class CG;
class Parts;

namespace han2 {

struct ExportOptions {
	bool cgImages = true;        // every CG image as cg/cg_<n>.png
	bool poses = true;           // every PAT pose as poses/pose_<n>.png + poses.json
	bool patterns = true;        // every pattern: frames/<pattern>/f<k>.png (aligned), animation.json, strip.png, sheet.png
	// aligned canvas (game anchor = bottom centre). canvasW == 0: sized per pattern to fit every frame (+ margin), as icaroffa does
	int canvasW = 0, canvasH = 0, originX = 256, originY = 448;
	int margin = 24;
	int scale = 1;
	// icaroffa / PACNyx+ 2026 layout instead of ours: manifest.json, poses/<name>.json (his key names), renders/, renders_aligned/<group>/
	// (animation.json with frameWidth / frameHeight / anchorCanvasX / anchorCanvasY, strip.png, sheet.png), animations.json.
	// Groups are the game's pattern table (his build guessed them from pose names).
	bool icaroffaLayout = false;
};

struct ExportReport {
	int cgPngs = 0, posePngs = 0, framePngs = 0, patternsWritten = 0, sheets = 0, writeErrors = 0;
	std::vector<std::string> notes;
};

// outDir is created. fd must be a loaded HAN2 character; cg / parts may be empty (their outputs are skipped).
bool ExportCharacter(FrameData &fd, CG &cg, Parts &parts, const std::string &outDir, const ExportOptions &opt, ExportReport &rep, std::string *err);

// Thumbnail of one frame (any dialect: RBO / GOF2 poses and CG images, GOF1 / HA6 / HA4 CG layers) for the archive browser: composited on the CPU
// (no GL, safe on a worker thread), cropped to the visible pixels and box-filtered to fit maxSide x maxSide. false when the frame draws nothing.
bool RenderFrameThumb(FrameData &fd, CG &cg, Parts &parts, int pattern, int frame, int maxSide, std::vector<uint8_t> &rgba, int &w, int &h);
// One pose composited on the CPU and cropped to its visible pixels (PACNyx "Save pose as image"). anchorX/Y = where the pose base point
// (the actor anchor) lies in the image; it may be outside it. `pad` transparent pixels around. false when the pose draws nothing.
bool RenderPoseRgba(const Parts &parts, int pose, std::vector<uint8_t> &rgba, int &w, int &h, int &anchorX, int &anchorY, int pad = 0);
// First pattern / frame that draws something (idle pose); -1 when there is none.
bool FindIdleFrame(FrameData &fd, int &pattern, int &frame);

} // namespace han2

#endif
