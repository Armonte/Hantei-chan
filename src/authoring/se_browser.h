#ifndef AUTHORING_SE_BROWSER_H_GUARD
#define AUTHORING_SE_BROWSER_H_GUARD

// "Sound effects" window (MBAACC): a character's data\<c>_SeList.txt slots (NNN = name // comment) with the
// se\battle_se\<name>.wav each one plays, read loose first and then from the .p archives the way the game does.
// Play, level (peak dBFS), length; edit a slot's name or comment in place; replace a .wav. Writes go to an output
// folder (the install, or a mod project's files folder) as loose files, never into the .p archives.

#include "../background/bg_project.h" // bg::TextIni
#include "../mbaacc_pack.h"

#include <cstdint>
#include <string>
#include <vector>

namespace sebrowse {

struct WavInfo {
	bool ok = false;
	int channels = 0, rate = 0, bits = 0;
	double seconds = 0.0;
	double peakDb = -120.0; // 16-bit PCM only
};
// Parses a RIFF/WAVE buffer (fmt + data chunks).
WavInfo InspectWav(const std::vector<uint8_t>& bytes);

class Window {
public:
	bool open = false;
	bool openGame(const std::string& gameDirUtf8, std::string* error);
	void draw();

private:
	struct Slot {
		std::string key;     // "005"
		std::string name;    // "Arc_B0AA004"
		std::string comment; // UTF-8
		std::string source;  // "loose" / "0005.p" / "" (missing)
		WavInfo info;
	};
	// Loose file under the game dir, then the packs (highest-numbered first). Empty source = not found.
	bool readGameFile(const std::string& rel, std::vector<uint8_t>& out, std::string* source) const;
	bool selectCharacter(size_t i);
	void rebuildSlots();
	void play(const Slot& s);

	std::vector<mbpack::Archive> m_packs; // opened once per game, highest-numbered first
	std::string m_game, m_outDir, m_status;
	bool m_statusError = false;
	std::vector<std::string> m_chars; // "arc" for data\arc_SeList.txt
	int m_current = -1;
	bg::TextIni m_list;
	std::string m_listSource;
	std::vector<Slot> m_slots;
	std::vector<uint8_t> m_playing; // PlaySound(SND_MEMORY) needs the bytes alive while it plays
	std::string m_filter;
};

} // namespace sebrowse

#endif
