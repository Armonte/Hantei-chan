#ifndef AUTHORING_MBAC_REFERENCE_H_GUARD
#define AUTHORING_MBAC_REFERENCE_H_GUARD
// [authoring] MBAC (Melty Blood Act Cadenza) reference values for the 22 characters MBAACC shares with it
// (docs/HANTEI_AUTHORING_MODE.md §8.9 "MBAC reference values ... with a 'use MBAC value' button"):
//   * per character: MBAC's [TeamChangeData] tag-in / tag-out pattern numbers (<STEM>_C.TXT) and the frame data of
//     those MBAC patterns (<STEM>.DAT through the editor's HA4 loader);
//   * global: the MBAC tag rules the levers were modelled on (cooldown 120, regen 1/tick, ...), with the RE source.
// Read-only; the MBAC install is never written. Default folder: C:\games\MB\AC\install\MBACPC\02_extracted.
#include "frame_summary.h"

#include <cstdint>
#include <string>
#include <vector>

namespace authoring {

struct MbacCharRef {
	bool ok = false;
	std::string stem;          // "SHIKI"
	std::string error;         // why not ok ("no MBAC counterpart", "cannot read ...")
	int tagIn = -1, tagOut = -1;   // [TeamChangeData] in MBAC's _C.TXT
	MoveSummary in, out;       // the MBAC patterns' frame data (valid when the .DAT loaded)
};

std::string DefaultMbacDir();
// Cached per (dir, chara). chara = the MBAACC g_CharaSelectDataTable id (roster_mirror.h MbacStemForChara).
const MbacCharRef& MbacReferenceFor(const std::string& mbacDir, int chara);

struct MbacGlobalRef {
	const char* key;           // lever key
	int32_t value;             // the MBAC value, in the lever's encoding
	const char* source;        // "mbacPC TagTeam_ProcessSwap 0x435830: 120-frame cooldown"
};
const std::vector<MbacGlobalRef>& MbacGlobalReference();
const MbacGlobalRef* FindMbacGlobal(const std::string& key);

} // namespace authoring

#endif
