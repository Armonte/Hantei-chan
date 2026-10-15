#ifndef BACKGROUND_STAGE_CARD_H_GUARD
#define BACKGROUND_STAGE_CARD_H_GUARD

// Stage card composer (MBAACC): the per-stage images the select and round screens show, built from ordinary images.
//   name EN  GRP\BgSelect\stsel_en\chr_stsel_enNN.dds      256x32, DXT3
//   name JP  GRP\BgSelect\stsel_jp\chr_stsel_jpNN.dds      256x32, DXT3
//   preview  GRP\BgSelect\stsel_view\chr_stsel_viewNN.dds  256x96, DXT5
//   round    grp\RoundCall_AA\round\rd_stage\rd_stNN.bmp   256x16, 32-bit BGRA bottom-up (vanilla header, 2 pad bytes)
// The game has no stage-name strings: these images ARE the name. The BGM entry [BGM_0NN] and its title (the File comment)
// are edited in the BGM window / Stage Browser. Files are written as loose files into an output folder (install or a
// mod project's asset folder).

#include <cstdint>
#include <string>
#include <vector>

namespace stagecard {

enum class Slot { NameEn, NameJp, Preview, RoundCall, Count };
struct SlotInfo { const char* label; const char* folder; const char* prefix; const char* ext; int w, h, dxt; };
const SlotInfo& Info(Slot s);
// Game-relative path for a stage id ("GRP\\BgSelect\\stsel_en\\chr_stsel_en26.dds").
std::string GamePath(Slot s, int stageId);

// Bilinear resample of straight-alpha RGBA (premultiplied while filtering, so transparent edges do not bleed).
void Resize(const uint8_t* src, int sw, int sh, std::vector<uint8_t>& dst, int dw, int dh);
// DDS file bytes: DXT3 (explicit 4-bit alpha) or DXT5, one mip level. w and h are multiples of 4.
std::vector<uint8_t> MakeDds(const uint8_t* rgba, int w, int h, int dxt);
// 32-bit BMP as the vanilla rd_stNN.bmp files are stored.
std::vector<uint8_t> MakeRoundCallBmp(const uint8_t* rgba, int w, int h);
// The bytes for a slot from an RGBA image of any size (resized to the slot's size).
std::vector<uint8_t> BuildSlot(Slot s, const uint8_t* rgba, int w, int h);

class Window {
public:
	bool open = false;
	int stageId = 1;
	std::string outDir;
	void draw();

private:
	struct Source {
		std::string path;
		std::vector<uint8_t> rgba; // resized to the slot size
		unsigned tex = 0;
	};
	bool load(Slot s, const std::string& path);
	Source m_src[(int)Slot::Count];
	std::string m_status;
	bool m_statusError = false;
};

} // namespace stagecard

#endif
