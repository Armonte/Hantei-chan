#include "framedata.h"
#include "misc.h"
#include <fstream>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

#define VAL(X) ((const char*)&X)
#define PTR(X) ((const char*)X)

// The order of these things is a bit different from the order the original game files use.
// (Because I haven't figured out the proper order lol)
// I don't know if it can cause trouble but it's something to keep in mind.

// Write AF with smart format detection (AFGP for single-layer, AFGX for multi-layer)
void WriteAF(std::ostream &file, const Frame_AF *af, bool usedAFGX)
{
	file.write("AFST", 4);

	// AFGP (MBAACC) for a single layer, AFGX (UNI) for several. A pattern that
	// was loaded with AFGX keeps AFGX even on single-layer frames: rewriting
	// them as AFGP changed UNI files on save (issue #71).
	if (af->layers.size() == 1 && !usedAFGX) {
		// MBAACC format (AFGP) - single layer
		const Layer_Type& layer = af->layers[0];

		file.write("AFGP", 4);
		uint32_t pat = layer.usePat;
		file.write(VAL(pat), 4);
		file.write(VAL(layer.spriteId), 4);

		// MBAACC uses the compact AFY<n> tag (4 bytes, no payload) when
		// offset_x == 0 and offset_y is in the narrow range 7..12 (verified
		// against akaakiha.HA6 — y=4..6 and y=13 always go to AFOF there).
		// Encoding: y=7..9 -> 'AFY7'..'AFY9'; y=10 -> 'AFYX'; y=11,12 -> 'AFY1','AFY2'.
		if(layer.offset_x == 0 && layer.offset_y >= 7 && layer.offset_y <= 12){
			char tag[4] = {'A', 'F', 'Y', '?'};
			if(layer.offset_y == 10)      tag[3] = 'X';
			else if(layer.offset_y >= 11) tag[3] = (layer.offset_y - 10) + '0'; // 11,12 -> '1','2'
			else                          tag[3] = layer.offset_y + '0';        // 7,8,9
			file.write(tag, 4);
		}
		else if(layer.offset_x || layer.offset_y){
			file.write("AFOF", 4);
			file.write(VAL(layer.offset_x), 4);
			file.write(VAL(layer.offset_y), 4);
		}
	} else {
		// UNI format (AFGX) - multi-layer
		for (size_t i = 0; i < af->layers.size(); i++) {
			const Layer_Type& layer = af->layers[i];

			file.write("AFGX", 4);
			uint32_t layerId = i;
			uint32_t pat = layer.usePat;
			file.write(VAL(layerId), 4);
			file.write(VAL(pat), 4);
			file.write(VAL(layer.spriteId), 4);

			// Write layer-specific properties
			if(layer.offset_x || layer.offset_y){
				file.write("AFOF", 4);
				file.write(VAL(layer.offset_x), 4);
				file.write(VAL(layer.offset_y), 4);
			}

			if(layer.blend_mode){
				file.write("AFAL", 4);
				int anormalized = layer.rgba[3]*255.f;
				file.write(VAL(layer.blend_mode), 4);
				file.write(VAL(anormalized), 4);
			}
			else if (layer.rgba[3] != 1.f){
				int type = 1;
				int anormalized = layer.rgba[3]*255.f;
				file.write("AFAL", 4);
				file.write(VAL(type), 4);
				file.write(VAL(anormalized), 4);
			}

			if(	layer.rgba[0] != 1.f ||
				layer.rgba[1] != 1.f ||
				layer.rgba[2] != 1.f)
			{
				int anormalized[3];
				for(int j = 0; j < 3; j++)
					anormalized[j] = layer.rgba[j]*255.f;
				file.write("AFRG", 4);
				file.write(PTR(anormalized), 3*sizeof(int));
			}

			if(layer.rotation[0]){
				file.write("AFAX", 4);
				file.write(VAL(layer.rotation[0]), sizeof(float));
			}
			if(layer.rotation[1]){
				file.write("AFAY", 4);
				file.write(VAL(layer.rotation[1]), sizeof(float));
			}
			if(layer.rotation[2]){
				file.write("AFAZ", 4);
				file.write(VAL(layer.rotation[2]), sizeof(float));
			}
			if(	layer.scale[0] != 1.f ||
				layer.scale[1] != 1.f){
				file.write("AFZM", 4);
				file.write(PTR(layer.scale), 2*sizeof(float));
			}

			// Write AFPL for layer priority (UNI only)
			if (layer.priority != 0) {
				file.write("AFPL", 4);
				file.write(VAL(layer.priority), 4);
			}
		}
	}

	// Continue with MBAACC single-layer format if only one layer
	if (af->layers.size() == 1 && !usedAFGX) {
		const Layer_Type& layer = af->layers[0];

		if(layer.blend_mode){
			file.write("AFAL", 4);
			int anormalized = layer.rgba[3]*255.f;
			file.write(VAL(layer.blend_mode), 4);
			file.write(VAL(anormalized), 4);
		}
		else if (layer.rgba[3] != 1.f){
			int type = 1;
			int anormalized = layer.rgba[3]*255.f;
			file.write("AFAL", 4);
			file.write(VAL(type), 4);
			file.write(VAL(anormalized), 4);
		}

		if(	layer.rgba[0] != 1.f ||
			layer.rgba[1] != 1.f ||
			layer.rgba[2] != 1.f)
		{
			int anormalized[3];
			for(int i = 0; i < 3; i++)
				anormalized[i] = layer.rgba[i]*255.f;
			file.write("AFRG", 4);
			file.write(PTR(anormalized), 3*sizeof(int));
		}

		if(layer.rotation[0]){
			file.write("AFAX", 4);
			file.write(VAL(layer.rotation[0]), sizeof(float));
		}
		if(layer.rotation[1]){
			file.write("AFAY", 4);
			file.write(VAL(layer.rotation[1]), sizeof(float));
		}
		if(layer.rotation[2]){
			file.write("AFAZ", 4);
			file.write(VAL(layer.rotation[2]), sizeof(float));
		}
		if(	layer.scale[0] != 1.f ||
			layer.scale[1] != 1.f){
			file.write("AFZM", 4);
			file.write(PTR(layer.scale), 2*sizeof(float));
		}
		// NO AFPL for MBAACC single-layer
	}

	// Frame-level properties (always written, regardless of format).
	// AFD<n> is the compact form for duration 1..9; duration 0 (and >= 10
	// / negative) goes through AFDL with a full int32, matching the
	// original encoder. We used to emit AFD0 for duration==0 which never
	// appears in original files.
	if(af->duration > 0 && af->duration < 10){
		char t = af->duration + '0';
		file.write("AFD", 3);
		file.write(VAL(t), 1);
	}
	else{
		file.write("AFDL", 4);
		file.write(VAL(af->duration), 4);
	}

	if(af->aniType){
		if(af->aniType <= 2){
			char t = af->aniType + '0';
			file.write("AFF", 3);
			file.write(VAL(t), 1);
		} else {
			file.write("AFFL", 4);
			file.write(VAL(af->aniType), 4);
		}
	}

	if(af->aniFlag){
		file.write("AFFE", 4);
		file.write(VAL(af->aniFlag), 4);
	}

	if(af->jump){
		file.write("AFJP", 4);
		file.write(VAL(af->jump), 4);
	}
	if(af->interpolationType){
		file.write("AFHK", 4);
		file.write(VAL(af->interpolationType), 4);
	}
	// UNI/Dengeki frame ID and parameters (zero-default, safe for MBAACC)
	if(af->frameId){
		file.write("AFID", 4);
		file.write(VAL(af->frameId), 4);
	}
	if(af->afjh){
		int AFJH = af->afjh;
		file.write("AFJH", 4);
		file.write(VAL(AFJH), 4);
	}
	if(memcmp(af->param, "\0\0\0\0", 4)){
		file.write("AFPA", 4);
		file.write(PTR(af->param), 4);
	}
	if(af->priority){
		file.write("AFPR", 4);
		file.write(VAL(af->priority), 4);
	}
	if(af->loopCount){
		file.write("AFCT", 4);
		file.write(VAL(af->loopCount), 4);
	}
	if(af->loopEnd){
		file.write("AFLP", 4);
		file.write(VAL(af->loopEnd), 4);
	}
	if(af->landJump){
		file.write("AFJC", 4);
		file.write(VAL(af->landJump), 4);
	}
	if(af->AFRT){
		int val = af->AFRT;
		file.write("AFRT", 4);
		file.write(VAL(val), 4);
	}

	file.write("AFED", 4);
}

void WriteAS(std::ostream &file, const Frame_AS *as)
{
	file.write("ASST", 4);

	if((as->movementFlags & 0x11) == 0x11 &&
		as->speed[0] == 0 &&
		as->speed[1] == 0 &&
		as->accel[0] == 0 &&
		as->accel[1] == 0
	){
		file.write("ASVX", 4);
	}
	else if(as->movementFlags != 0 ||
		as->speed[0] != 0 ||
		as->speed[1] != 0 ||
		as->accel[0] != 0 ||
		as->accel[1] != 0
	){
		file.write("ASV0", 4);
		file.write(VAL(as->movementFlags), 4);
		file.write(PTR(as->speed), 2*4);
		file.write(PTR(as->accel), 2*4);
	}

	if(as->canMove){
		int val = as->canMove;
		file.write("ASMV", 4);
		file.write(VAL(val), 4);
	}
	if(as->stanceState){
		char t = as->stanceState + '0';
		file.write("ASS", 3); //lmaop
		file.write(VAL(t), 1);
	}
	if(as->hitsNumber){
		file.write("ASAA", 4);
		file.write(VAL(as->hitsNumber), 4);
	}
	if(as->cancelNormal){
		file.write("ASCN", 4);
		file.write(VAL(as->cancelNormal), 4);
	}
	if(as->cancelSpecial){
		file.write("ASCS", 4);
		file.write(VAL(as->cancelSpecial), 4);
	}
	if(as->counterType){
		file.write("ASCT", 4);
		file.write(VAL(as->counterType), 4);
	}
	if(as->ascf){
		file.write("ASCF", 4);
		file.write(VAL(as->ascf), 4);
	}
	if(as->statusFlags[0])
	{
		file.write("ASF0", 4);
		file.write(VAL(as->statusFlags[0]), 4);
	}
	if(as->statusFlags[1])
	{
		file.write("ASF1", 4);
		file.write(VAL(as->statusFlags[1]), 4);
	}
	if(as->maxSpeedX){
		file.write("ASMX", 4);
		file.write(VAL(as->maxSpeedX), 4);
	}
	if(as->sineFlags)
	{
		file.write("AST0", 4);
		file.write(VAL(as->sineFlags), 4);
		file.write(PTR(as->sineParameters), 4*4);
		file.write(PTR(as->sinePhases), 2*sizeof(float));
	}
	if(as->invincibility){
		file.write("ASYS", 4);
		file.write(VAL(as->invincibility), 4);
	}


	file.write("ASED", 4);
}


void WriteAT(std::ostream &file, const Frame_AT *at, bool usedATV2)
{
	file.write("ATST", 4);

	if(at->guard_flags){
		file.write("ATGD", 4);
		file.write(VAL(at->guard_flags), 4);
	}
	if(at->correction != 100){
		file.write("ATHS", 4);
		file.write(VAL(at->correction), 4);
	}

	if(usedATV2){
		// UNI/Dengeki: combined hit+guard vector format, damage and meter as separate tags
		constexpr int sizes[2] = {3, 2};
		file.write("ATV2", 4);
		file.write(PTR(sizes), 2*4);
		for(int i = 0; i < 3; i++){
			file.write(VAL(at->hVFlags[i]), 4);
			file.write(VAL(at->hitVector[i]), 4);
			file.write(VAL(at->gVFlags[i]), 4);
			file.write(VAL(at->guardVector[i]), 4);
		}
	} else {
		// MBAACC: pack damage/meter into ATVV, separate ATHV/ATGV
		{ //Always
			short d[4];
			d[0] = at->red_damage;
			d[1] = at->damage;
			d[2] = at->guard_damage;
			d[3] = at->meter_gain;
			file.write("ATVV", 4);
			file.write(PTR(d), 2*4);
		}
		{
			constexpr int three = 3;
			int val[three];

			file.write("ATHV", 4);
			file.write(VAL(three), 4);
			for(int i = 0; i < 3; i++)
				val[i] = at->hitVector[i] | (at->hVFlags[i] << 8);
			file.write(PTR(val), sizeof(val));

			file.write("ATGV", 4);
			file.write(VAL(three), 4);
			for(int i = 0; i < 3; i++)
				val[i] = at->guardVector[i] | (at->gVFlags[i] << 8);
			file.write(PTR(val), sizeof(val));
		}
	}

	if(at->correction_type){
		file.write("ATHT", 4);
		file.write(VAL(at->correction_type), 4);
	}
	if(at->otherFlags){
		file.write("ATF1", 4);
		file.write(VAL(at->otherFlags), 4);
	}
	if(at->hitEffect || at->soundEffect){
		file.write("ATHE", 4);
		file.write(VAL(at->hitEffect), 4);
		file.write(VAL(at->soundEffect), 4);
	}
	if(at->addedEffect){
		file.write("ATKK", 4);
		file.write(VAL(at->addedEffect), 4);
	}
	if(at->hitgrab){
		int val = at->hitgrab;
		file.write("ATNG", 4);
		file.write(VAL(val), 4);
	}
	if(at->extraGravity){
		file.write("ATUH", 4);
		file.write(VAL(at->extraGravity), 4);
	}
	if(at->breakTime){
		file.write("ATBT", 4);
		file.write(VAL(at->breakTime), 4);
	}
	if(at->hitStopTime){
		file.write("ATSN", 4);
		file.write(VAL(at->hitStopTime), 4);
	}
	//ATS1..ATS6: compact hit stop preset (sits right before ATSU in vanilla files).
	if(at->hitStopLegacy >= 1 && at->hitStopLegacy <= 6){
		char t[4] = {'A','T','S',(char)('0' + at->hitStopLegacy)};
		file.write(t, 4);
	}
	if(at->untechTime){
		file.write("ATSU", 4);
		file.write(VAL(at->untechTime), 4);
	}
	if(at->atbc){ //UNI2, unknown semantics — sits right before ATSP in vanilla files.
		file.write("ATBC", 4);
		file.write(VAL(at->atbc), 4);
	}
	if(at->hitStop){
		file.write("ATSP", 4);
		file.write(VAL(at->hitStop), 4);
	}
	if(at->blockStopTime){
		file.write("ATGN", 4);
		file.write(VAL(at->blockStopTime), 4);
	}

	if(usedATV2){
		// UNI/Dengeki: damage and meter gain are separate tags (not packed into ATVV)
		if(at->damage){
			file.write("ATAT", 4);
			file.write(VAL(at->damage), 4);
		}
		if(at->meter_gain){
			file.write("ATCA", 4);
			file.write(VAL(at->meter_gain), 4);
		}
		if(at->atrf){ //UNI2, unknown semantics — sits right before ATHH in vanilla files.
			file.write("ATRF", 4);
			file.write(VAL(at->atrf), 4);
		}
		if(at->damageProration != 100){
			file.write("ATHH", 4);
			file.write(VAL(at->damageProration), 4);
		}
		if(at->minDamage){
			file.write("ATAM", 4);
			file.write(VAL(at->minDamage), 4);
		}
		if(at->hitStunDecay[0] || at->hitStunDecay[1] || at->hitStunDecay[2]){
			file.write("ATC0", 4);
			file.write(PTR(at->hitStunDecay), 3*4);
		}
		if(at->addHitStun){
			file.write("ATSA", 4);
			file.write(VAL(at->addHitStun), 4);
		}
		if(at->starterCorrection){
			file.write("ATSH", 4);
			file.write(VAL(at->starterCorrection), 4);
		}
	}

	if(at->atvd){ //MBTL, unknown semantics — sits right before ATED in vanilla files.
		file.write("ATVD", 4);
		file.write(VAL(at->atvd), 4);
	}

	file.write("ATED", 4);
}

// Trim trailing zero parameters: write EFPR/IFPR with the count of params up
// through the last non-zero one, and omit the tag entirely when all are zero.
// Matches the original encoder; saves up to 48 bytes/effect and 36 bytes/cond.
void WriteEF(std::ostream &file, const std::vector<Frame_EF> &ef)
{
	constexpr int maxParam = 12;
	for(size_t i = 0; i < ef.size(); i++)
	{
		int paramN = 0;
		for(int j = 0; j < maxParam; j++)
			if(ef[i].parameters[j]) paramN = j + 1;

		file.write("EFST", 4);
		int idx = (int)i;
		file.write(VAL(idx), 4);
		file.write("EFTP", 4);
		file.write(VAL(ef[i].type), 4);
		file.write("EFNO", 4);
		file.write(VAL(ef[i].number), 4);
		if(paramN){
			file.write("EFPR", 4);
			file.write(VAL(paramN), 4);
			file.write(PTR(ef[i].parameters), paramN * 4);
		}
		file.write("EFED", 4);
	}
}

void WriteIF(std::ostream &file, const std::vector<Frame_IF> &ifs)
{
	constexpr int maxParam = 9;
	for(size_t i = 0; i < ifs.size(); i++)
	{
		int paramN = 0;
		for(int j = 0; j < maxParam; j++)
			if(ifs[i].parameters[j]) paramN = j + 1;

		file.write("IFST", 4);
		int idx = (int)i;
		file.write(VAL(idx), 4);
		file.write("IFTP", 4);
		file.write(VAL(ifs[i].type), 4);
		if(paramN){
			file.write("IFPR", 4);
			file.write(VAL(paramN), 4);
			file.write(PTR(ifs[i].parameters), paramN * 4);
		}
		file.write("IFED", 4);
	}
}

// Per-sequence dedup state. Tracks every distinct AS block and hitbox written
// in this sequence so far. WriteFrame consults these to emit ASSM / HRAS /
// HRNS references instead of full data when a frame's AS or box matches an
// earlier one. The totalXxx counters feed PDS2[1]/[4]/[6] back in WriteSequence.
struct PatInfo
{
	std::vector<const int*> boxList;
	std::vector<const Frame_AS*> asList;
	int totalBoxes = 0;
	int totalAses = 0;
	int totalAts = 0;
};

void WriteFrame(std::ostream &file, const Frame *frame, bool usedAFGX, bool usedATV2, PatInfo &info)
{
	file.write("FSTR", 4);
	WriteAF(file, &frame->AF, usedAFGX);

	// ASSM: emit a 4-byte ref if this AS matches a previously-written one.
	int dupeAsIndex = -1;
	for(int i = 0; i < (int)info.asList.size(); ++i)
	{
		if(!memcmp(&frame->AS, info.asList[i], sizeof(Frame_AS)))
		{
			dupeAsIndex = i;
			break;
		}
	}
	if(dupeAsIndex >= 0)
	{
		file.write("ASSM", 4);
		file.write(VAL(dupeAsIndex), 4);
	}
	else
	{
		WriteAS(file, &frame->AS);
		info.asList.push_back(&frame->AS);
		info.totalAses += 1;
	}

	bool hasAt = false;
	if(!frame->hitboxes.empty())
	{
		auto maxhurt = frame->hitboxes.lower_bound(25);
		if(maxhurt != frame->hitboxes.begin())
			--maxhurt;

		if(maxhurt->first < 25)
		{
			int val = maxhurt->first+1;
			file.write("FSNH", 4);
			file.write(VAL(val), 4);
		}

		auto maxhit = --(frame->hitboxes.end());
		if(maxhit->first >= 25)
		{
			int val = maxhit->first-25+1;
			file.write("FSNA", 4);
			file.write(VAL(val), 4);
			hasAt = true;
		}
	}

	if(!frame->EF.empty())
	{
		int val = frame->EF.size();
		file.write("FSNE", 4);
		file.write(VAL(val), 4);
	}
	if(!frame->IF.empty())
	{
		int val = frame->IF.size();
		file.write("FSNI", 4);
		file.write(VAL(val), 4);
	}

	// AT only written if this frame has attack boxes (matches sosfiro/original).
	if(hasAt)
	{
		WriteAT(file, &frame->AT, usedATV2);
		info.totalAts += 1;
	}

	// HRAS / HRNS: dedup hitbox xy data against any previously-emitted box.
	// box.first < 25 → hurt (HRNM/HRNS), >= 25 → attack (HRAT/HRAS).
	for(const auto& box : frame->hitboxes)
	{
		int index = box.first;
		int dupeIndex = -1;
		for(int i = (int)info.boxList.size() - 1; i >= 0; --i)
		{
			if(!memcmp(box.second.xy, info.boxList[i], sizeof(int)*4))
			{
				dupeIndex = i;
				break;
			}
		}

		if(box.first >= 25)
		{
			index -= 25;
			file.write(dupeIndex >= 0 ? "HRAS" : "HRAT", 4);
		}
		else
		{
			file.write(dupeIndex >= 0 ? "HRNS" : "HRNM", 4);
		}

		if(dupeIndex >= 0)
		{
			file.write(VAL(index), 4);
			file.write(VAL(dupeIndex), 4);
		}
		else
		{
			file.write(VAL(index), 4);
			file.write(PTR(box.second.xy), 4*4);
			info.boxList.push_back(box.second.xy);
			info.totalBoxes += 1;
		}
	}

	WriteEF(file, frame->EF);
	WriteIF(file, frame->IF);

	file.write("FEND", 4);
}

static bool RawNameMatches(const Ha6SeqEnc &se, const std::string &name);

static void WriteSequenceMbaacc(std::ostream &file, const Sequence *seq)
{
	if(seq->psts){
		file.write("PSTS", 4);
		file.write(VAL(seq->psts), 4);
	}
	if(seq->level){
		file.write("PLVL", 4);
		file.write(VAL(seq->level), 4);
	}
	if(seq->flag){
		file.write("PFLG", 4);
		file.write(VAL(seq->flag), 4);
	}
	if(seq->pups){
		file.write("PUPS", 4);
		file.write(VAL(seq->pups), 4);
	}
	if(RawNameMatches(seq->ha6, seq->name) && seq->ha6.hasPTT2){
		// Unchanged name: keep the loaded bytes (32-byte names without a NUL
		// were cut to 31 on save; MBAACC hisui 386, warc 501, neco 708, ...).
		const uint32_t len = seq->ha6.ptt2Len;
		file.write("PTT2", 4);
		file.write(VAL(len), 4);
		file.write((const char*)seq->ha6.ptt2, std::min<uint32_t>(len, 64));
	}
	else if(!seq->name.empty()){
		char buf[32]{};
		uint32_t size = 32;

		// Always write Shift-JIS for game compatibility
		// Internal strings are always UTF-8, so convert to Shift-JIS
		std::string nameToWrite = utf82sj(seq->name);

		// Copy Shift-JIS string, ensuring we don't truncate mid-character
		// Shift-JIS uses 1-2 bytes per character, so we need to be careful
		size_t copyLen = nameToWrite.length();
		if(copyLen > 31) {
			// Truncate, but ensure we don't cut a 2-byte character in half
			copyLen = 31;
			// If the last byte is a lead byte (0x81-0x9F or 0xE0-0xFC), back up one byte
			if((unsigned char)nameToWrite[copyLen-1] >= 0x81 && (unsigned char)nameToWrite[copyLen-1] <= 0x9F) {
				copyLen--;
			} else if((unsigned char)nameToWrite[copyLen-1] >= 0xE0 && (unsigned char)nameToWrite[copyLen-1] <= 0xFC) {
				copyLen--;
			}
		}
		
		memcpy(buf, nameToWrite.c_str(), copyLen);
		buf[copyLen] = 0;
		file.write("PTT2", 4);
		file.write(VAL(size), 4);
		file.write(PTR(buf), 32);
	}
	if(!seq->codeName.empty()){
		uint32_t cnSize = seq->codeName.size() + 1;
		file.write("PTCN", 4);
		file.write(VAL(cnSize), 4);
		file.write(PTR(seq->codeName.data()), cnSize);
	}

	if(!seq->frames.empty())
	{
		// PDS2 header: [0]=frame count, [1]=unique hitbox count, [2]=total EF,
		// [3]=total IF, [4]=AT count, [6]=unique AS count, [7]=frame count.
		// Values [1], [4], [6] depend on dedup decisions made inside WriteFrame,
		// so we write the header, walk frames into a PatInfo, then seek back to
		// patch the counts (matches sosfiro's emitter).
		uint32_t pds2[8]{};
		pds2[0] = pds2[7] = seq->frames.size();
		for(const auto& frame : seq->frames)
		{
			pds2[2] += frame.EF.size();
			pds2[3] += frame.IF.size();
		}

		uint32_t pds2Size = sizeof(pds2);

		file.write("PDS2", 4);
		file.write(VAL(pds2Size), 4);
		auto dataBlockPos = file.tellp();
		file.write(PTR(pds2), pds2Size);

		PatInfo info{};
		for(const auto& frame : seq->frames)
		{
			WriteFrame(file, &frame, seq->usedAFGX, seq->usedATV2, info);
		}
		pds2[1] = info.totalBoxes;
		pds2[6] = info.totalAses;
		pds2[4] = info.totalAts;

		auto curPos = file.tellp();
		file.seekp(dataBlockPos);
		file.write(PTR(pds2), pds2Size);
		file.seekp(curPos);
	}
}

// ===========================================================================
// UNI / UNI2 / MBTL writer
// ===========================================================================
// Writes the modern French-Bread dialect (AFGX layers, ATV2, separate damage
// and meter tags) the way the games' own tool does, so a load -> save of an
// unmodified pattern is byte-identical (checked on all 262 UNI2/MBTL files by
// tools/uni/uni_regress.sh):
//  - tags in the canonical order (ha6_enc.h, ha6order::*), each written when
//    its value differs from the loader default or it was present at load;
//  - ASSM / HRNS / HRAS references kept where the loaded file had them and the
//    referenced block is unchanged (the FB tool shares blocks by pointer, so
//    this can't be derived from content); new frames dedup AS blocks;
//  - PTT2/PTCN raw buffers reused while the name is unchanged;
//  - EF/IF slot numbers and FSNE/FSNI kept while the EF/IF list is unchanged;
//  - boxes written exactly as stored: inverted boxes are valid game data
//    (MBTL chr016 p6/p25), only boxes edited this session get cleaned up.
// Field semantics: docs/HANTEI_UNI_MBTL.md.

// True if the pattern's raw PTT2/PTIT buffer (as loaded) still decodes to
// its current name, so the raw bytes -- including anything after the NUL, or
// a 32-byte name without one -- can be written back unchanged.
static bool RawNameMatches(const Ha6SeqEnc &se, const std::string &name)
{
	if (!se.valid || se.utf8Names || !(se.hasPTT2 || se.hadPTIT)) return false;
	const size_t n = strnlen((const char*)se.ptt2, std::min<uint32_t>(se.ptt2Len, 64));
	return sj2utf8(std::string((const char*)se.ptt2, n)) == name;
}

namespace {

inline int ToByte(float v) { return (int)std::lround(v * 255.f); }

struct UniW {
	std::ostream &f;
	void tag(const char *t) { f.write(t, 4); }
	void i32(int v) { f.write((const char*)&v, 4); }
	void u32(uint32_t v) { f.write((const char*)&v, 4); }
	void f32(float v) { f.write((const char*)&v, 4); }
	void tagi(const char *t, int v) { tag(t); i32(v); }
};

bool Present(uint32_t bits, int idx) { return idx >= 0 && (bits >> idx) & 1u; }

template<size_t N>
bool Has(uint32_t bits, const char *const (&list)[N], const char *t) { return Present(bits, ha6order::Index(list, t)); }

void WriteExtras(UniW &w, const Ha6FrameEnc &enc, uint8_t block)
{
	for (int i = 0; i < enc.nExtra && i < Ha6FrameEnc::kMaxExtra; ++i) {
		const Ha6ExtraTag &x = enc.extra[i];
		if (x.block != block) continue;
		w.f.write(x.tag, 4);
		w.f.write((const char*)x.w, 4 * x.nwords);
	}
}

void WriteLayerUni(UniW &w, const Layer_Type &L, int id, const Ha6FrameEnc &enc, bool frameAFRT)
{
	const uint32_t bits = (id >= 0 && id < 5) ? enc.layerTags[id] : 0;
	auto has = [&](const char *t) { return Has(bits, ha6order::kLayer, t); };
	w.tag("AFGX"); w.i32(id); w.i32(L.usePat ? 1 : 0); w.i32(L.spriteId);
	if (L.offset_x || L.offset_y || has("AFOF")) { w.tag("AFOF"); w.i32(L.offset_x); w.i32(L.offset_y); }
	if (L.rotation[2] != 0.f || has("AFAZ")) { w.tag("AFAZ"); w.f32(L.rotation[2]); }
	if (L.rotation[0] != 0.f || has("AFAX")) { w.tag("AFAX"); w.f32(L.rotation[0]); }
	const bool afrt = L.afrt || (id == 0 && frameAFRT);
	if (afrt || has("AFRT")) { w.tagi("AFRT", afrt ? 1 : 0); }
	if (L.rotation[1] != 0.f || has("AFAY")) { w.tag("AFAY"); w.f32(L.rotation[1]); }
	if (L.blend_mode || L.rgba[3] != 1.f || has("AFAL")) {
		// Blend 0 makes the game ignore the alpha (Han6Draw_DrawLayer), so an
		// alpha edit on a blend-0 layer is written with blend 1 (normal).
		const int blend = L.blend_mode ? L.blend_mode : (L.rgba[3] != 1.f ? 1 : 0);
		w.tag("AFAL"); w.i32(blend); w.i32(ToByte(L.rgba[3]));
	}
	if (L.scale[0] != 1.f || L.scale[1] != 1.f || has("AFZM")) { w.tag("AFZM"); w.f32(L.scale[0]); w.f32(L.scale[1]); }
	if (L.rgba[0] != 1.f || L.rgba[1] != 1.f || L.rgba[2] != 1.f || has("AFRG")) {
		w.tag("AFRG"); w.i32(ToByte(L.rgba[0])); w.i32(ToByte(L.rgba[1])); w.i32(ToByte(L.rgba[2]));
	}
	if (L.priority || has("AFPL")) w.tagi("AFPL", L.priority);
	if (id >= 0) WriteExtras(w, enc, (uint8_t)(HA6X_AFLAYER0 + id));
}

void WriteAFUni(UniW &w, const Frame &fr, bool afgx, int layerCount)
{
	const Frame_AF &af = fr.AF;
	const Ha6FrameEnc &enc = fr.ha6;
	auto has = [&](const char *t) { return Has(enc.afTags, ha6order::kAF, t); };
	w.tag("AFST");
	if (afgx) {
		// Every AFGX frame in the shipped files lists all the game's layers
		// (UNI2 5, MBTL 3), unused ones with sprite -1.
		const int n = std::max<int>((int)af.layers.size(), layerCount);
		static const Layer_Type kEmpty = [] { Layer_Type l; l.spriteId = -1; return l; }();
		for (int i = 0; i < n; ++i)
			WriteLayerUni(w, i < (int)af.layers.size() ? af.layers[i] : kEmpty, i, enc, af.AFRT);
	} else {
		// AFGP (single layer) pattern inside a UNI file.
		static const Layer_Type kEmpty{};
		const Layer_Type &L = af.layers.empty() ? kEmpty : af.layers[0];
		w.tag("AFGP"); w.i32(L.usePat ? 1 : 0); w.i32(L.spriteId);
		// Same per-layer tags as AFGX, minus the AFGX header.
		std::ostringstream tmp(std::ios_base::out | std::ios_base::binary);
		UniW tw{tmp};
		WriteLayerUni(tw, L, 0, enc, af.AFRT);
		const std::string b = tmp.str();
		w.f.write(b.data() + 16, (std::streamsize)b.size() - 16);
	}
	if (af.duration >= 1 && af.duration <= 9) { char t[4] = {'A','F','D',(char)('0' + af.duration)}; w.tag(t); }
	else if (af.duration || has("AFD*")) w.tagi("AFDL", af.duration);
	if (af.aniType == 1 || af.aniType == 2) { char t[4] = {'A','F','F',(char)('0' + af.aniType)}; w.tag(t); }
	else if (af.aniType || has("AFF*")) w.tagi("AFFL", af.aniType);
	if (af.aniFlag || has("AFFE")) w.tagi("AFFE", (int)af.aniFlag);
	if (af.priority || has("AFPR")) w.tagi("AFPR", af.priority);
	if (af.interpolationType || has("AFHK")) w.tagi("AFHK", af.interpolationType);
	if (af.jump || has("AFJP")) w.tagi("AFJP", af.jump);
	if (af.loopEnd || has("AFLP")) w.tagi("AFLP", af.loopEnd);
	if (af.frameId || has("AFID")) w.tagi("AFID", af.frameId);
	if (memcmp(af.param, "\0\0\0\0", 4) || has("AFPA")) { w.tag("AFPA"); w.f.write((const char*)af.param, 4); }
	if (af.afjh || has("AFJH")) w.tagi("AFJH", af.afjh ? 1 : 0);
	if (af.landJump || has("AFJC")) w.tagi("AFJC", af.landJump);
	if (af.loopCount || has("AFCT")) w.tagi("AFCT", af.loopCount);
	WriteExtras(w, enc, HA6X_AF);
	w.tag("AFED");
}

bool ASEqual(const Frame_AS &a, const Frame_AS &b)
{
	return a.movementFlags == b.movementFlags && !memcmp(a.speed, b.speed, sizeof(a.speed))
		&& !memcmp(a.accel, b.accel, sizeof(a.accel)) && a.maxSpeedX == b.maxSpeedX
		&& a.canMove == b.canMove && a.stanceState == b.stanceState && a.cancelNormal == b.cancelNormal
		&& a.cancelSpecial == b.cancelSpecial && a.counterType == b.counterType && a.hitsNumber == b.hitsNumber
		&& a.invincibility == b.invincibility && !memcmp(a.statusFlags, b.statusFlags, sizeof(a.statusFlags))
		&& a.sineFlags == b.sineFlags && !memcmp(a.sineParameters, b.sineParameters, sizeof(a.sineParameters))
		&& !memcmp(a.sinePhases, b.sinePhases, sizeof(a.sinePhases)) && a.ascf == b.ascf;
}

void WriteASUni(UniW &w, const Frame_AS &as, const Ha6FrameEnc &enc)
{
	auto has = [&](const char *t) { return Has(enc.asTags, ha6order::kAS, t); };
	w.tag("ASST");
	const bool zeroMove = !as.speed[0] && !as.speed[1] && !as.accel[0] && !as.accel[1];
	if (as.movementFlags == 0x11 && zeroMove) w.tag("ASVX");
	else if (as.movementFlags || !zeroMove || has("ASV*")) {
		w.tagi("ASV0", (int)as.movementFlags);
		w.i32(as.speed[0]); w.i32(as.speed[1]); w.i32(as.accel[0]); w.i32(as.accel[1]);
	}
	if (as.sineFlags || has("AST0")) {
		w.tagi("AST0", (int)as.sineFlags);
		w.f.write((const char*)as.sineParameters, 16);
		w.f.write((const char*)as.sinePhases, 8);
	}
	if (as.stanceState == 1) w.tag("ASS1"); else if (as.stanceState == 2) w.tag("ASS2");
	if (as.canMove || has("ASMV")) w.tagi("ASMV", as.canMove ? 1 : 0);
	if (as.hitsNumber || has("ASAA")) w.tagi("ASAA", as.hitsNumber);
	if (as.cancelNormal || has("ASCN")) w.tagi("ASCN", as.cancelNormal);
	if (as.cancelSpecial || has("ASCS")) w.tagi("ASCS", as.cancelSpecial);
	if (as.statusFlags[0] || has("ASF0")) w.tagi("ASF0", (int)as.statusFlags[0]);
	if (as.statusFlags[1] || has("ASF1")) w.tagi("ASF1", (int)as.statusFlags[1]);
	if (as.ascf || has("ASCF")) w.tagi("ASCF", as.ascf);
	if (as.maxSpeedX || has("ASMX")) w.tagi("ASMX", as.maxSpeedX);
	if (as.invincibility || has("ASYS")) w.tagi("ASYS", as.invincibility);
	if (as.counterType || has("ASCT")) w.tagi("ASCT", as.counterType);
	WriteExtras(w, enc, HA6X_AS);
	w.tag("ASED");
}

void WriteATUni(UniW &w, const Frame_AT &at, const Ha6FrameEnc &enc, bool usedATV2)
{
	auto has = [&](const char *t) { return Has(enc.atTags, ha6order::kAT, t); };
	w.tag("ATST");
	if (at.guard_flags || has("ATGD")) w.tagi("ATGD", (int)at.guard_flags);
	bool anyVec = false;
	for (int i = 0; i < 3; ++i)
		anyVec |= at.hitVector[i] || at.guardVector[i] || at.hVFlags[i] || at.gVFlags[i];
	if (anyVec || has("ATV2") || (usedATV2 && !enc.valid)) {
		w.tag("ATV2"); w.i32(3); w.i32(2);
		for (int i = 0; i < 3; ++i) {
			w.i32(at.hVFlags[i]); w.i32(at.hitVector[i]); w.i32(at.gVFlags[i]); w.i32(at.guardVector[i]);
		}
	}
	if (at.hitEffect || at.soundEffect || has("ATHE")) { w.tagi("ATHE", at.hitEffect); w.i32(at.soundEffect); }
	if (at.hitStopLegacy >= 1 && at.hitStopLegacy <= 6) { char t[4] = {'A','T','S',(char)('0' + at.hitStopLegacy)}; w.tag(t); }
	if (at.untechTime || has("ATSU")) w.tagi("ATSU", at.untechTime);
	if (at.hitStopTime || has("ATSN")) w.tagi("ATSN", at.hitStopTime);
	if (at.atbc || has("ATBC")) w.tagi("ATBC", at.atbc);
	if (at.addedEffect || has("ATKK")) w.tagi("ATKK", at.addedEffect);
	if (at.hitgrab || has("ATNG")) w.tagi("ATNG", at.hitgrab);
	if (at.hitStop || has("ATSP")) w.tagi("ATSP", at.hitStop);
	if (at.addHitStun || has("ATSA")) w.tagi("ATSA", at.addHitStun);
	if (at.correction != 100 || has("ATHS")) w.tagi("ATHS", at.correction);
	if (at.atrf || has("ATRF")) w.tagi("ATRF", at.atrf);
	if (at.correction_type || has("ATHT")) w.tagi("ATHT", at.correction_type);
	if (at.starterCorrection || has("ATSH")) w.tagi("ATSH", at.starterCorrection);
	if (at.damageProration != 100 || has("ATHH")) w.tagi("ATHH", at.damageProration);
	if (at.otherFlags || has("ATF1")) w.tagi("ATF1", (int)at.otherFlags);
	if (at.blockStopTime || has("ATGN")) w.tagi("ATGN", at.blockStopTime);
	if (at.damage || has("ATAT")) w.tagi("ATAT", at.damage);
	if (at.minDamage || has("ATAM")) w.tagi("ATAM", at.minDamage);
	if (at.meter_gain || has("ATCA")) w.tagi("ATCA", at.meter_gain);
	if (at.hitStunDecay[0] || at.hitStunDecay[1] || at.hitStunDecay[2] || has("ATC0")) {
		w.tag("ATC0"); w.f.write((const char*)at.hitStunDecay, 12);
	}
	if (at.atvd || has("ATVD")) w.tagi("ATVD", at.atvd);
	WriteExtras(w, enc, HA6X_AT);
	w.tag("ATED");
}

// One box of a frame, as it will be written.
struct BoxOut {
	int loc;            // 0..32 (25+ = attack)
	const int *xy;
	bool ref;           // HRNS/HRAS
	int refOrigPool;    // original pool index it references (ref only)
	int origPool;       // original pool index of an unchanged full box, else -1
};

bool BoxUnchanged(const Ha6FrameEnc &enc, int loc, const int *xy)
{
	return enc.valid && loc >= 0 && loc < Ha6FrameEnc::kMaxBoxes && ((enc.boxMask >> loc) & 1ull)
		&& !memcmp(enc.boxXY[loc], xy, 16);
}

} // namespace

// frames: the pattern's frames (post box cleanup). layerCount: AFGX layers of
// the game (0 = as many as each frame has).
static void WriteSequenceUni(std::ostream &file, const Sequence *seq, int layerCount)
{
	UniW w{file};
	const Ha6SeqEnc &se = seq->ha6;
	if (seq->psts) w.tagi("PSTS", seq->psts);
	if (seq->level) w.tagi("PLVL", seq->level);
	if (seq->flag) w.tagi("PFLG", seq->flag);
	if (seq->pups) w.tagi("PUPS", seq->pups);

	// Name: reuse the raw buffer while the text is unchanged.
	const std::string sjName = utf82sj(seq->name);
	const bool rawName = RawNameMatches(se, seq->name);
	if (rawName && se.hadPTIT && !se.hasPTT2) {
		w.tag("PTIT"); file.write((const char*)se.ptt2, 32);
	} else if (rawName) {
		w.tag("PTT2"); w.u32(se.ptt2Len); file.write((const char*)se.ptt2, std::min<uint32_t>(se.ptt2Len, 64));
	} else if (!seq->name.empty()) {
		char buf[32]{};
		size_t n = std::min<size_t>(sjName.size(), 31);
		// don't cut a double-byte character in half
		if (n == 31) {
			size_t i = 0;
			while (i < n) {
				unsigned char c = (unsigned char)sjName[i];
				size_t l = ((c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC)) ? 2 : 1;
				if (i + l > n) break;
				i += l;
			}
			n = i;
		}
		memcpy(buf, sjName.data(), n);
		w.tag("PTT2"); w.u32(32); file.write(buf, 32);
	}
	if (!seq->codeName.empty()) {
		const std::string &cn = seq->codeName;
		const bool rawCode = se.valid && se.hasPTCN
			&& strnlen((const char*)se.ptcn, std::min<uint32_t>(se.ptcnLen, 64)) == cn.size()
			&& !memcmp(se.ptcn, cn.data(), cn.size());
		if (rawCode) { w.tag("PTCN"); w.u32(se.ptcnLen); file.write((const char*)se.ptcn, std::min<uint32_t>(se.ptcnLen, 64)); }
		else { w.tag("PTCN"); w.u32((uint32_t)cn.size() + 1); file.write(cn.c_str(), cn.size() + 1); }
	}
	if (seq->frames.empty())
		return;

	const bool afgx = seq->usedAFGX || (!seq->usedATV2 && layerCount > 1);

	// ---- box plan (two passes: references may point forward) -------------
	const size_t nf = seq->frames.size();
	std::vector<std::vector<BoxOut>> boxes(nf);
	std::map<int, const int*> unchangedFull;   // original pool -> xy of an unchanged full box
	for (size_t fi = 0; fi < nf; ++fi) {
		const Frame &fr = seq->frames[fi];
		for (const auto &kv : fr.hitboxes) {
			BoxOut b{kv.first, kv.second.xy, false, -1, -1};
			if (BoxUnchanged(fr.ha6, kv.first, kv.second.xy) && fr.ha6.boxPool[kv.first] >= 0) {
				b.origPool = fr.ha6.boxPool[kv.first];
				unchangedFull[b.origPool] = kv.second.xy;
			}
			boxes[fi].push_back(b);
		}
	}
	for (size_t fi = 0; fi < nf; ++fi) {
		const Frame &fr = seq->frames[fi];
		for (auto &b : boxes[fi]) {
			if (b.loc < 0 || b.loc >= Ha6FrameEnc::kMaxBoxes) continue;
			const int src = fr.ha6.boxRef[b.loc];
			if (src < 0 || !BoxUnchanged(fr.ha6, b.loc, b.xy)) continue;
			auto it = unchangedFull.find(src);
			if (it != unchangedFull.end() && !memcmp(it->second, b.xy, 16)) { b.ref = true; b.refOrigPool = src; }
		}
	}
	std::map<int, int> poolMap;     // original pool -> new pool
	int nPool = 0;
	for (auto &fb : boxes)
		for (auto &b : fb)
			if (!b.ref) { if (b.origPool >= 0) poolMap[b.origPool] = nPool; ++nPool; }

	// ---- PDS2 ---------------------------------------------------------------
	uint32_t pds2[8]{};
	pds2[0] = pds2[7] = (uint32_t)nf;
	pds2[1] = (uint32_t)nPool;
	for (const auto &fr : seq->frames) { pds2[2] += (uint32_t)fr.EF.size(); pds2[3] += (uint32_t)fr.IF.size(); }
	pds2[5] = se.valid ? se.pds2Unused : 0;
	const auto pdsPos = file.tellp();
	w.tag("PDS2"); w.u32(32); file.write((const char*)pds2, 32);

	// ---- frames -------------------------------------------------------------
	struct AsBlock { const Frame_AS *as; };
	std::vector<AsBlock> asBlocks;          // new pool, in write order
	std::map<int, int> asMap;               // original AS pool -> new pool
	int nAT = 0;
	for (size_t fi = 0; fi < nf; ++fi) {
		const Frame &fr = seq->frames[fi];
		const Ha6FrameEnc &enc = fr.ha6;
		w.tag("FSTR");
		WriteAFUni(w, fr, afgx, afgx ? layerCount : 0);

		// AS: keep the loaded reference while the block it points at is unchanged.
		int ref = -1;
		if (enc.valid && enc.asRef >= 0) {
			auto it = asMap.find(enc.asRef);
			if (it != asMap.end() && ASEqual(*asBlocks[it->second].as, fr.AS)) ref = it->second;
		} else if (!enc.valid) {
			for (size_t k = 0; k < asBlocks.size(); ++k)
				if (ASEqual(*asBlocks[k].as, fr.AS)) { ref = (int)k; break; }
		}
		if (ref >= 0) { w.tagi("ASSM", ref); }
		else {
			if (enc.valid && enc.asPool >= 0) asMap[enc.asPool] = (int)asBlocks.size();
			asBlocks.push_back({&fr.AS});
			WriteASUni(w, fr.AS, enc);
		}

		// Slot counts.
		int maxHurt = -1, maxAtk = -1;
		for (const auto &b : boxes[fi]) { if (b.loc < 25) maxHurt = std::max(maxHurt, b.loc); else maxAtk = std::max(maxAtk, b.loc - 25); }
		const bool efSame = enc.valid && enc.nEF == fr.EF.size() && fr.EF.size() <= (size_t)Ha6FrameEnc::kMaxSlots;
		const bool ifSame = enc.valid && enc.nIF == fr.IF.size() && fr.IF.size() <= (size_t)Ha6FrameEnc::kMaxSlots;
		auto slotCount = [](bool same, int8_t loaded, const int8_t *slots, size_t n) {
			int mx = -1;
			for (size_t i = 0; i < n; ++i) mx = std::max(mx, same ? (int)slots[i] : (int)i);
			int c = mx + 1;
			if (same && loaded > c) c = loaded;
			return c;
		};
		const int nEFslots = slotCount(efSame, enc.fsn[2], enc.efSlot, fr.EF.size());
		const int nIFslots = slotCount(ifSame, enc.fsn[3], enc.ifSlot, fr.IF.size());
		if (maxHurt >= 0) w.tagi("FSNH", maxHurt + 1);
		if (maxAtk >= 0) w.tagi("FSNA", maxAtk + 1);
		if (!fr.EF.empty() || (efSame && enc.fsn[2] >= 0)) w.tagi("FSNE", nEFslots);
		if (!fr.IF.empty() || (ifSame && enc.fsn[3] >= 0)) w.tagi("FSNI", nIFslots);

		if (maxAtk >= 0 || enc.hadAT) { WriteATUni(w, fr.AT, enc, seq->usedATV2); ++nAT; }

		for (const auto &b : boxes[fi]) {
			const bool atk = b.loc >= 25;
			const int idx = atk ? b.loc - 25 : b.loc;
			if (b.ref) {
				w.tag(atk ? "HRAS" : "HRNS"); w.i32(idx); w.i32(poolMap[b.refOrigPool]);
			} else {
				w.tag(atk ? "HRAT" : "HRNM"); w.i32(idx); file.write((const char*)b.xy, 16);
			}
		}
		WriteExtras(w, enc, HA6X_FRAME);

		for (size_t i = 0; i < fr.EF.size(); ++i) {
			const Frame_EF &ef = fr.EF[i];
			int paramN = 0;
			for (int j = 0; j < 12; ++j) if (ef.parameters[j]) paramN = j + 1;
			w.tagi("EFST", efSame ? enc.efSlot[i] : (int)i);
			w.tagi("EFTP", ef.type);
			w.tagi("EFNO", ef.number);
			if (paramN) { w.tagi("EFPR", paramN); file.write((const char*)ef.parameters, 4 * paramN); }
			w.tag("EFED");
		}
		for (size_t i = 0; i < fr.IF.size(); ++i) {
			const Frame_IF &iff = fr.IF[i];
			int paramN = 0;
			for (int j = 0; j < 9; ++j) if (iff.parameters[j]) paramN = j + 1;
			w.tagi("IFST", ifSame ? enc.ifSlot[i] : (int)i);
			w.tagi("IFTP", iff.type);
			if (paramN) { w.tagi("IFPR", paramN); file.write((const char*)iff.parameters, 4 * paramN); }
			w.tag("IFED");
		}
		w.tag("FEND");
	}

	pds2[4] = (uint32_t)nAT;
	pds2[6] = (uint32_t)asBlocks.size();
	const auto endPos = file.tellp();
	file.seekp(pdsPos + std::streamoff(8));
	file.write((const char*)pds2, 32);
	file.seekp(endPos);
}

void WriteSequence(std::ostream &file, const Sequence *seq, int uniLayerCount)
{
	if (seq->usedAFGX || seq->usedATV2 || uniLayerCount > 0)
		WriteSequenceUni(file, seq, uniLayerCount);
	else
		WriteSequenceMbaacc(file, seq);
}
