#ifndef PASTE_NOTICE_H_GUARD
#define PASTE_NOTICE_H_GUARD
// D13: the notice shown after pasted data was converted to the file's dialect
// (FrameData::conformPasted). Empty when nothing changed.
#include "framedata.h"
#include "i18n.h"
#include <cstdio>
#include <string>

inline std::string FormatConvertNotice(const Ha6ConvertReport& r)
{
	char b[256];
	std::string out;
	if (r.adopted) {
		snprintf(b, sizeof b, TXT("The file was empty: it now uses the pasted data's format (%s)."), Ha6GameName(r.to));
		return b;
	}
	if (!r.frames && !r.patterns) return out;
	snprintf(b, sizeof b, TXT("Pasted data converted to this file's format (%s)."), Ha6GameName(r.to));
	out = b;
	if (r.layersDropped) {
		snprintf(b, sizeof b, TXT(" Dropped %d layer(s) the game does not read (%d with a sprite)."), r.layersDropped, r.spriteLayersDropped);
		out += b;
	}
	if (r.mbaaccFieldsCleared) {
		snprintf(b, sizeof b, TXT(" Cleared MBAACC-only attack fields on %d frame(s)."), r.mbaaccFieldsCleared);
		out += b;
	}
	if (r.uniFieldsCleared) {
		snprintf(b, sizeof b, TXT(" Cleared UNI/MBTL-only fields on %d frame(s)."), r.uniFieldsCleared);
		out += b;
	}
	return out;
}
#endif
