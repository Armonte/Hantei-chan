#ifndef HAN2_EXPORT_H_GUARD
#define HAN2_EXPORT_H_GUARD

// Sprite / pose / animation export for French-Bread RBO (and GOF2) characters. Reimplements the behaviour of icaroffa's PACNyx+
// 2026 export (pose JSON + PNG, animation JSON, aligned strip and sheet) from the decompiled build as a reference (no code copied),
// with one correction: animations are the game's real pattern table (frame order, durations, jumps, loops), not pose-name groups.
// Rendering is a plain CPU compositor so it works headless (han2tool export) and in the editor.

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
	int canvasW = 512, canvasH = 512, originX = 256, originY = 448;   // aligned canvas (game anchor = bottom centre)
	int scale = 1;
};

struct ExportReport {
	int cgPngs = 0, posePngs = 0, framePngs = 0, patternsWritten = 0, sheets = 0;
	std::vector<std::string> notes;
};

// outDir is created. fd must be a loaded HAN2 character; cg / parts may be empty (their outputs are skipped).
bool ExportCharacter(FrameData &fd, CG &cg, Parts &parts, const std::string &outDir, const ExportOptions &opt, ExportReport &rep, std::string *err);

} // namespace han2

#endif
