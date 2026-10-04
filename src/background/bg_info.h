// Stage side files and the weather-particle system.
//
//   BgList.ini       MBAACC stage list (StageSelect_LoadStageData 0x4b4a80)
//   bgNNInfo.txt     MBAACC lights + DropObj weather (DropObject_LoadConfigFromFile
//                    0x4b6380, loaded for EVERY stage by Background_LoadInfoFile
//                    0x4b6ab0 — the BgList InfoFile key is parsed but not used)
//   bgNNlight.txt    MBAC lights (mbacPC LoadLightingData 0x401680); MBAACC
//                    ships copies but never reads them
//
// Weather particles are a port of DropObject_InitializeParticles 0x4b4fc0 /
// DropObject_UpdateParticles 0x4b5210 / DropObject_RenderWithBloom 0x4b5cc0.
// See docs/bg_research/BG_HA4_RE.md, "Wave 2".
#ifndef BG_INFO_H_GUARD
#define BG_INFO_H_GUARD

#include "bg_rng.h"
#include <cstdint>
#include <string>
#include <vector>

namespace bg {

// One [Bg_NNN] section of BgList.ini. Keys that are absent read as 0.
struct StageListEntry {
	int         index = 0;          // NNN
	std::string dataFile;           // "bg41" (no extension)
	int         infoFile = 0;       // parsed by the game, never used
	int         isSelectable = 0;
	int         isGiantStage = 0;
	float       stageColorVal = 0.0f; // "fColorHosei" of the BgPointBlur post effect
};

struct StageList {
	std::string path;
	std::vector<StageListEntry> entries;
	bool Load(const std::string& iniPath);
	// Case-insensitive match on DataFile ("bg41" or "bg41.dat").
	const StageListEntry* FindByDataFile(const std::string& name) const;
};

struct StageLight {
	int pos = 0;     // MBAACC: world x = pos - 512.  MBAC: pos - 256 (see RE doc)
	int power = 0;   // falloff range in px: intensity = 1 - |x - lightX| / power
};

// bgNNInfo.txt [Data]. Defaults are the game's (absent key -> value below).
struct StageInfo {
	bool        loaded = false;
	std::string path;
	std::vector<StageLight> lights;   // LightNum (max 10), Light%02dPos/Power
	int         dropObj = 0;          // non-zero enables weather
	int         dropType = -1;        // 0 bitmap particles (sakura), 1 rain lines, -1 grid effect
	std::string dropFile;             // type 0: .\Bg\<file>.bmp
	int         patNum = 0;           // type 0: texture rows (variants)
	int         frameNum = 0;         // type 0: texture columns (anim frames)
	int         wait = 0;             // type 0: ticks per anim frame; type 1: fall speed px/tick (default 50)
	int         w = 0, h = 0;         // type 0: cell size; type 1: h = streak length (default 150)
	int         count = 100;          // particles (DropObj_Max for rain, default 100; always 100 for type 0)
	int         alpha = 100;          // type 1: streak alpha (default 100)
	bool Load(const std::string& txtPath);
	bool LoadFromText(const std::string& text, const std::string& txtPath);
};

// MBAC bgNNlight.txt: "count" then "pos, power" lines.
struct LightFile {
	bool loaded = false;
	std::string path;
	std::vector<StageLight> lights;
	bool Load(const std::string& txtPath);
};

// 44-byte game particle (array at MBAA 0x766008).
struct DropParticle {
	float   x = 0, y = 0;      // +0/+4 world px (floor y = 0, centre x = 0)
	int32_t pat = 0;           // +8  texture row (type 0)
	float   frame = 0;         // +12 texture column (type 0; an int in the game)
	float   waitCtr = 0;       // +16 anim tick counter (an int in the game)
	int32_t alpha = 255;       // +20 type 0 fade-out after y > 0
	int32_t phase = 0;         // +24 rand*256, stored but never read
	float   vx = 0, vy = 0;    // +28/+32
	float   ax = 0, ay = 0;    // +36/+40 (always 0)
};

class DropSystem {
public:
	void Init(const StageInfo& info, Rng& rng);   // DropObject_InitializeParticles
	void Update(Rng& rng);                        // DropObject_UpdateParticles
	void Clear() { particles.clear(); active = false; }
	// Overwrite the particles with a game snapshot (100 x 44 bytes, 0x766008).
	void ImportRaw(const StageInfo& info, const uint8_t* raw, size_t n);
	bool IsActive() const { return active; }
	int  Type() const { return type; }
	const std::vector<DropParticle>& Particles() const { return particles; }
	const StageInfo& Info() const { return cfg; }

private:
	bool active = false;
	int  type = -1;
	StageInfo cfg;
	std::vector<DropParticle> particles;
};

// Case-insensitive lookup of `name` inside `dir`. Returns "" if absent.
std::string FindFileNoCase(const std::string& dir, const std::string& name);

} // namespace bg

#endif // BG_INFO_H_GUARD
