#include "bg_file.h"
#include "../misc.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

namespace bg {

File::File() {
	memset(offsetTable, -1, sizeof(offsetTable));
}

File::~File() {
	Free();
}

void File::Free() {
	objects.clear();
	cgData.clear();
	cg.reset();
	oldPat.reset();
	patData.clear();
	instances.clear();
	loaded = false;
	memset(offsetTable, -1, sizeof(offsetTable));
}

bool File::Load(const char* filename) {
	if (loaded) {
		Free();
	}

	// Store filename
	this->filename = filename;

	char* data;
	unsigned int size;

	if (!ReadInMem(filename, data, size)) {
		std::cerr << "Failed to read file: " << filename << std::endl;
		return false;
	}
	
	// Load header
	Header header;
	if (!LoadHeader(data, size, header)) {
		delete[] data;
		return false;
	}
	
	// Load offset table
	if (!LoadOffsetTable(data, size)) {
		delete[] data;
		return false;
	}
	
	// Load objects
	if (!LoadObjects(data, size, header)) {
		delete[] data;
		return false;
	}
	
	// Load embedded CG
	if (!LoadEmbeddedCG(data, size, header)) {
		std::cerr << "Warning: Failed to load embedded CG" << std::endl;
		// Not fatal - continue without sprites
	}
	
	delete[] data;
	loaded = true;
	dirty = false;
	ReloadSideFiles();
	ResetRuntime();
	ResetHistory();
	
	std::cout << "Loaded background: " << objects.size() << " objects, "
	          << (cg ? cg->get_image_count() : 0) << " sprites" << std::endl;
	
	return true;
}

bool File::LoadHeader(const char* data, size_t size, Header& header) {
	if (size < sizeof(Header)) {
		std::cerr << "File too small for header" << std::endl;
		return false;
	}
	
	memcpy(&header, data, sizeof(Header));

	// Capture these for byte-1:1 round-trip on Save. The C# tool wrote them
	// back from its own state; we have to preserve them since the editor
	// never touches them.
	loadedUnk        = header.unk;
	loadedPatFileOff = header.pat_file_off;
	loadedPatFileLen = header.pat_file_len;

	// Show what we actually got
	//printf("Magic header bytes: ");
	//for (int i = 0; i < 16; i++) {
	//	if (header.magic[i] >= 32 && header.magic[i] < 127) {
	//		printf("%c", header.magic[i]);
	//	} else {
	//		printf("[0x%02X]", (unsigned char)header.magic[i]);
	//	}
	//}
	//printf("\n");
	
	// Verify magic - bgmake tool writes "bgmake" (6 bytes) + 10 null bytes
	if (strncmp(header.magic, "bgmake", 6) != 0) {
		std::cerr << "Invalid magic header (expected 'bgmake')" << std::endl;
		std::cerr << "File signature: '";
		for (int i = 0; i < 6; i++) {
			std::cerr << header.magic[i];
		}
		std::cerr << "'" << std::endl;
		return false;
	}
	
	return true;
}

bool File::LoadOffsetTable(const char* data, size_t size) {
	size_t offsetTablePos = sizeof(Header);
	if (size < offsetTablePos + sizeof(offsetTable)) {
		std::cerr << "File too small for offset table" << std::endl;
		return false;
	}
	
	memcpy(offsetTable, data + offsetTablePos, sizeof(offsetTable));
	return true;
}

bool File::LoadObjects(const char* data, size_t size, const Header& header) {
	objects.clear();
	
	for (int i = 0; i < 256; i++) {
		if (offsetTable[i] == -1) continue;
		
		size_t pos = offsetTable[i];
		if (pos + 60 > size) {
			std::cerr << "Invalid object offset at index " << i << std::endl;
			continue;
		}
		
		Object obj;
		obj.name = "obj_" + std::to_string(i);
		obj.originalIndex = i;
		obj.originalOffset = offsetTable[i];
		
		// Read object header (60 bytes)
		const int32_t* objHeader = (const int32_t*)(data + pos);
		int32_t numFrames = objHeader[0];
		obj.parallax = objHeader[1];
		obj.layer = objHeader[2];
		// +12/+16 are NOT reserved: relative offsets of the trigger / command
		// record tables (-1 = none). +20/+21/+22 are flags. See bg_types.h.
		std::memcpy(obj.rawHeader, data + pos, 60);
		obj.hasRawHeader = true;
		obj.triggerTableOff = objHeader[3];
		obj.commandTableOff = objHeader[4];
		obj.noAutoSpawn  = (uint8_t)data[pos + 20];
		obj.foreground   = (uint8_t)data[pos + 21];
		obj.linearFilter = (uint8_t)data[pos + 22];
		
		pos += 60;  // Skip object header
		
		// Read frames (132 bytes each)
		for (int f = 0; f < numFrames; f++) {
			if (pos + 132 > size) {
				std::cerr << "Invalid frame data for object " << i << std::endl;
				break;
			}
			
			Frame frame;
			
			// Parse frame data (132 byte structure)
			const int16_t* frameData16 = (const int16_t*)(data + pos);
			const uint8_t* frameData8 = (const uint8_t*)(data + pos);
			
			// Raw sprite-id. >= 10000 means a CG sprite (CG index =
			// spriteId - 10000); < 10000 means a PAT pattern index. The
			// game branches the same way (Background_RenderLayer).
			frame.spriteId = frameData16[0];
			frame.offsetX = frameData16[1];
			frame.offsetY = frameData16[2];
			frame.duration = frameData16[3];
			
			// Byte fields at offset 8
			frame.blendMode = frameData8[9];
			frame.opacity = frameData8[10];
			frame.aniType = frameData8[11];
			frame.jumpFrame = frameData8[12];
			// Loop control (BGFrame +21/+22) — see bg_types.h / MBAA.exe
			// BackgroundLayer_UpdateLayerState. Required for aniType-5 loops.
			frame.loopEnd   = frameData8[21];
			frame.loopCount = frameData8[22];

			// Debug opacity values for first few frames
			//if (i < 3 && f < 2) {
			//	printf("[BG Load] Obj %d Frame %d: opacity byte=%u (%.2f alpha), blend=%u\n",
			//	       i, f, frame.opacity, frame.opacity / 255.0f, frame.blendMode);
			//}

			// Movement block — frame-relative offsets verified against
			// MBAACC Background_UpdateLayerPositions. Flags at +44..47,
			// velocity int16 at +52/+54, acceleration int16 at +60/+62.
			frame.flagClearX = frameData8[44];
			frame.flagClearY = frameData8[45];
			frame.flagSetX   = frameData8[46];
			frame.flagSetY   = frameData8[47];
			frame.velX = *(const int16_t*)(data + pos + 52);
			frame.velY = *(const int16_t*)(data + pos + 54);
			frame.accX = *(const int16_t*)(data + pos + 60);
			frame.accY = *(const int16_t*)(data + pos + 62);
			// Verified extra fields (docs/bg_research/BG_HA4_RE.md).
			frame.scaleX = *(const int16_t*)(data + pos + 16);
			frame.scaleY = *(const int16_t*)(data + pos + 18);
			frame.interpolate = frameData8[20];
			for (int k = 0; k < 8; ++k) {
				frame.triggerRef[k] = *(const int16_t*)(data + pos + 100 + 2 * k);
				frame.commandRef[k] = *(const int16_t*)(data + pos + 116 + 2 * k);
			}
			std::memcpy(frame.raw, data + pos, 132);
			frame.hasRaw = true;
			
			obj.frames.push_back(frame);
			pos += 132;
		}
		
		// Event record tables: they occupy the bytes between the end of the
		// frames and the next block (next object, else PAT, else CG).
		{
			size_t framesEnd = pos;
			size_t blockEnd = size;
			for (int j = 0; j < 256; ++j)
				if (offsetTable[j] != -1 && (size_t)offsetTable[j] > (size_t)offsetTable[i]
				    && (size_t)offsetTable[j] < blockEnd)
					blockEnd = offsetTable[j];
			if (header.pat_file_len > 0 && header.pat_file_off > offsetTable[i]
			    && (size_t)header.pat_file_off < blockEnd)
				blockEnd = header.pat_file_off;
			if (header.cg_file_len > 0 && header.cg_file_off > offsetTable[i]
			    && (size_t)header.cg_file_off < blockEnd)
				blockEnd = header.cg_file_off;
			if (blockEnd > framesEnd && blockEnd <= size &&
			    (obj.triggerTableOff != -1 || obj.commandTableOff != -1))
				obj.recordBytes.assign(data + framesEnd, data + blockEnd);
			auto parseTable = [&](int32_t tableOff, bool isTrigger, std::vector<EventRecord>& out) {
				out.clear();
				if (tableOff == -1) return;
				size_t base = (size_t)offsetTable[i] + (size_t)tableOff;
				// Count = highest referenced index + 1 (records are only
				// ever reached through the frame refs).
				int maxRef = -1;
				for (const auto& fr : obj.frames)
					for (int k = 0; k < 8; ++k) {
						int r = isTrigger ? fr.triggerRef[k] : fr.commandRef[k];
						if (r > maxRef) maxRef = r;
					}
				for (int r = 0; r <= maxRef; ++r) {
					size_t q = base + 52 * (size_t)r;
					if (q + 52 > size) break;
					EventRecord rec;
					std::memcpy(rec.raw, data + q, 52);
					rec.type = *(const int16_t*)(data + q);
					rec.w2   = *(const int16_t*)(data + q + 2);
					for (int k = 0; k < 13; ++k)
						rec.d[k] = *(const int32_t*)(data + q + 4 * k);
					out.push_back(rec);
				}
			};
			parseTable(obj.triggerTableOff, true, obj.triggers);
			parseTable(obj.commandTableOff, false, obj.commands);
		}
		
		objects.push_back(obj);
	}
	
	return objects.size() > 0;
}

bool File::LoadEmbeddedCG(const char* data, size_t size, const Header& header) {
	// Capture the embedded PAT block (stages like bg01/bg20 carry one). We
	// don't parse it — just hold the raw bytes so Save can write them back.
	patData.clear();
	if (header.pat_file_len > 0 && header.pat_file_off > 0 &&
	    (size_t)(header.pat_file_off + header.pat_file_len) <= size)
	{
		patData.assign(data + header.pat_file_off,
		               data + header.pat_file_off + header.pat_file_len);
	}

	if (header.cg_file_len <= 0 || header.cg_file_off < 0) {
		return false;  // No embedded CG
	}

	if ((size_t)(header.cg_file_off + header.cg_file_len) > size) {
		std::cerr << "Invalid CG offset/length" << std::endl;
		return false;
	}

	// Extract CG data
	cgData.resize(header.cg_file_len);
	memcpy(cgData.data(), data + header.cg_file_off, header.cg_file_len);

	// Trailing padding/alignment (all MBAACC stages observed so far have
	// exactly 16384 bytes after the CG block).
	size_t cgEnd = (size_t)header.cg_file_off + (size_t)header.cg_file_len;
	if (cgEnd < size) {
		trailingBytes.assign(data + cgEnd, data + size);
	} else {
		trailingBytes.clear();
	}
	
	// Save to temporary file for CG loader
	// (In future, can add CG::loadFromMemory() to avoid temp file)
	const char* tempPath = "temp_bg_stage.cg";
	std::ofstream tempFile(tempPath, std::ios::binary);
	if (!tempFile) {
		std::cerr << "Failed to create temp CG file" << std::endl;
		return false;
	}
	tempFile.write((const char*)cgData.data(), cgData.size());
	tempFile.close();
	
	// Load using existing CG system
	cg = std::make_unique<CG>();
	if (!cg->load(tempPath)) {
		std::cerr << "Failed to load CG from embedded data" << std::endl;
		cg.reset();
		std::remove(tempPath);
		return false;
	}
	
	// Clean up temp file
	std::remove(tempPath);

	// Parse the embedded PAT (if any). MBAACC stages with PAT-based objects
	// carry the older PAT format (magic 02 00 00 00 / 0x01234567) — its
	// patterns/cutouts/textures are decoded by bg::OldPat and rendered by
	// bg_renderer's PAT path (frame sprite-id < 10000).
	if (OldPat::IsOldPat(patData.data(), patData.size())) {
		oldPat = std::make_unique<OldPat>();
		if (!oldPat->Parse(patData.data(), patData.size())) {
			oldPat.reset();
			std::cerr << "Failed to parse embedded PAT" << std::endl;
		} else {
			std::cout << "Parsed embedded PAT (old format)" << std::endl;
		}
	}

	return true;
}

void File::UpdateAnimations() {
	// Game-accurate tick over the instance pool (replaces the per-object
	// Object::Update model, which could not express despawn/respawn,
	// spawners, triggers or multiple live copies of one object). The first
	// live instance of each object is mirrored back into the Object so the
	// stage panel's "current frame" readout keeps working.
	TickRuntime();
	for (auto& obj : objects) obj.currentFrame = -1;
	for (const auto& in : instances) {
		if (in.state < 2 || in.objIndex < 0 || in.objIndex >= (int)objects.size()) continue;
		Object& o = objects[in.objIndex];
		if (o.currentFrame != -1) continue;
		o.currentFrame = in.curFrame;
		o.frameDuration = in.timer;
		o.loopCounter = in.loopCounter;
		o.posX = in.posX; o.posY = in.posY;
	}
	for (auto& obj : objects) if (obj.currentFrame == -1) obj.currentFrame = 0;
}

// Object animation stepper — 1:1 port of MBAA.exe
// Background_UpdateLayerAnimations @0x4b88b0 (the per-tick stepper) plus the
// frame-entry logic of BackgroundLayer_UpdateLayerState @0x4b6d10. Runs once
// per 60Hz tick.
//
// anim_type (BGFrame +11):
//   0       -> end of lifetime: the object respawns (Reset) — this is how a
//              single-frame scrolling layer wraps back to its start.
//   1, 3    -> advance: currentFrame + 1; running off the end respawns too.
//   2, 4    -> jump: currentFrame = jumpFrame (also decrements loopCounter).
//   5       -> Loop ED: decrement loopCounter; jump to jumpFrame while it is
//              still > 0, otherwise fall through to loopEnd (+21).
//
// loopCounter (RuntimeBGObject.frame_timer): (re)loaded from a frame's
// loopCount (+22) whenever that frame is entered, if loopCount != 0. This is
// what makes a type-5 loop run a finite number of times and then branch out.
// The previous editor code had no counter at all and its jump logic was
// nonsense (it followed the *target* frame's jumpFrame minus one), so loops
// and branches never stepped or terminated correctly.
//
// Duration: the counter increments every tick and the frame advances on the
// tick where it reaches `duration` — a frame is shown for exactly `duration`
// ticks (the game does ++counter, then tests `duration <= counter`). The
// previous code held it for duration+1 ticks. The game does ONE advance per
// tick; a duration-0 frame is shown for one tick.
void Object::Update() {
	if (frames.empty()) return;

	const int count = (int)frames.size();
	const int oldFrame = currentFrame;
	if (currentFrame < 0 || currentFrame >= count)
		currentFrame = 0;
	Frame* frame1 = &frames[currentFrame];

	// --- frame stepping ---
	frameDuration++;
	if (frameDuration >= frame1->duration) {
		// `respawn` == the object's animation finished. The game despawns and
		// respawns such objects, which RESETS posX/posY to 0 — that is the
		// only way position ever returns to the start (neither the render
		// path nor the position integrator wraps it). A scrolling layer
		// (single aniType-0 frame + a velocity) "wraps" precisely because of
		// this respawn; without it the object translates away forever.
		bool respawn = false;
		switch (frame1->aniType) {
		case 0:                          // lifetime expired -> respawn
			respawn = true;
			break;
		case 1:
		case 3:                          // advance to the next frame
			currentFrame++;
			if (currentFrame >= count)   // ran off the end -> respawn/loop
				respawn = true;
			break;
		case 2:
		case 4:                          // unconditional jump
			currentFrame = frame1->jumpFrame;
			if (loopCounter > 0) loopCounter--;
			break;
		case 5:                          // Loop ED — loop, then branch out
			if (loopCounter > 0) loopCounter--;
			currentFrame = (loopCounter > 0) ? frame1->jumpFrame
			                                 : frame1->loopEnd;
			break;
		default:
			break;
		}
		// A jump landing out of range is also treated as a finish.
		if (!respawn && (currentFrame < 0 || currentFrame >= count))
			respawn = true;

		if (respawn) {
			// Reset() restarts at frame 0 with posX/posY cleared and the
			// loop counter re-armed — i.e. the object reappears at its start.
			Reset();
			return;
		}
		frameDuration = 0;
	}

	// On entering a new frame, (re)load the loop counter from its loopCount
	// field if nonzero — BackgroundLayer_UpdateLayerState @0x4b6d10.
	if (currentFrame != oldFrame && frames[currentFrame].loopCount != 0)
		loopCounter = frames[currentFrame].loopCount;

	// --- kinematic integration (MBAACC Background_UpdateLayerPositions) ---
	// A frame change clears velLoaded so the new frame's flag bytes get a
	// chance to reset/set velocity before integration resumes.
	if (currentFrame != oldFrame)
		velLoaded = false;
	if (currentFrame >= 0 && currentFrame < count) {
		const Frame& cf = frames[currentFrame];
		if (velLoaded) {
			posX += curVelX;
			posY += curVelY;
			curVelX += curAccX;
			curVelY += curAccY;
		} else {
			if (cf.flagClearX) { curVelX = 0; curAccX = 0; }
			if (cf.flagClearY) { curVelY = 0; curAccY = 0; }
			if (cf.flagSetX)   { curVelX = cf.velX; curAccX = cf.accX; }
			if (cf.flagSetY)   { curVelY = cf.velY; curAccY = cf.accY; }
			velLoaded = true;
		}
	}
}

void Object::Reset() {
	currentFrame = 0;
	frameDuration = 0;
	posX = posY = 0;
	curVelX = curVelY = 0;
	curAccX = curAccY = 0;
	velLoaded = false;
	// Entering frame 0 arms the loop counter from its loopCount field, the
	// same as BackgroundLayer_UpdateLayerState does on any frame entry.
	loopCounter = 0;
	if (!frames.empty() && frames[0].loopCount != 0)
		loopCounter = frames[0].loopCount;
}

void File::StepObjectForward(int objIndex) {
	if (objIndex < 0 || objIndex >= (int)objects.size()) return;
	auto& obj = objects[objIndex];
	if (obj.frames.empty()) return;

	obj.currentFrame++;
	if (obj.currentFrame >= (int)obj.frames.size()) {
		obj.currentFrame = 0;
	}
	obj.frameDuration = 0;
	for (auto& in : instances)
		if (in.state >= 1 && in.objIndex == objIndex) { in.curFrame = obj.currentFrame; EnterFrame(in); break; }
}

void File::StepObjectBackward(int objIndex) {
	if (objIndex < 0 || objIndex >= (int)objects.size()) return;
	auto& obj = objects[objIndex];
	if (obj.frames.empty()) return;

	obj.currentFrame--;
	if (obj.currentFrame < 0) {
		obj.currentFrame = (int)obj.frames.size() - 1;
	}
	obj.frameDuration = 0;
	for (auto& in : instances)
		if (in.state >= 1 && in.objIndex == objIndex) { in.curFrame = obj.currentFrame; EnterFrame(in); break; }
}

// File layout (mirrors u4ick's bgmaketool SaveFile in bgmake_file.cs):
//   0x00..0x05   "bgmake" (6 bytes magic)
//   0x06..0x0F   10 zero bytes
//   0x10..0x13   unk
//   0x14..0x17   pat_file_off  (-1)
//   0x18..0x1B   pat_file_len  (0)
//   0x1C..0x1F   cg_file_off   (patched after object writes)
//   0x20..0x23   cg_file_len   (patched after CG write)
//   0x24..0x53   48 zero bytes
//   0x54..0x453  256 * int32 offset table (per-object file offsets, -1 = absent)
//   0x454..      For each object: 60-byte object header + 132 bytes per frame
//                + its 52-byte trigger/command records (verbatim)
//   trailing     Embedded CG file (bytes preserved verbatim from load)
bool File::Save(const char* filenameOut)
{
	std::ofstream f(filenameOut, std::ios::binary | std::ios::trunc);
	if (!f) return false;

	auto w32 = [&](int32_t v) { f.write((const char*)&v, 4); };
	auto w16 = [&](int16_t v) { f.write((const char*)&v, 2); };
	auto w8  = [&](uint8_t v) { f.write((const char*)&v, 1); };
	auto wpad = [&](size_t n) { for (size_t i = 0; i < n; ++i) f.put('\0'); };

	// --- Header (84 bytes) ---
	// pat_file_off / pat_file_len / cg_file_off / cg_file_len all get
	// patched after the actual writes below; we just reserve the slots here.
	const char kMagic[6] = {'b','g','m','a','k','e'};
	f.write(kMagic, 6);
	wpad(10);
	w32(loadedUnk);          // unk (preserved from load)
	std::streampos patRefPos = f.tellp();
	w32(loadedPatFileOff);   // pat_file_off (placeholder, patched later)
	w32(loadedPatFileLen);   // pat_file_len (placeholder)
	std::streampos cgRefPos = f.tellp();
	w32(0);                  // cg_file_off  (patched later)
	w32(0);                  // cg_file_len  (patched later)
	wpad(48);

	// --- Compute object offsets ---
	// The offset table sits right after the header. Object data follows the
	// table. First object lives at 84 + 1024 = 1108. Objects are placed in
	// their originalIndex slot (so sparse files round-trip correctly);
	// editor-created objects (originalIndex == -1) get the next free slot.
	constexpr int32_t kHeaderSize     = 84;
	constexpr int32_t kOffsetTableLen = 256;
	constexpr int32_t kFirstObjectOff = kHeaderSize + kOffsetTableLen * 4;  // 1108

	int32_t offsets[256];
	for (int i = 0; i < 256; ++i) offsets[i] = -1;

	// Build a (slot -> objects index) mapping in the order they should land
	// in the file, then assign sequential file offsets to those slots.
	std::vector<std::pair<int, size_t>> slotToObj; // (slot, objectsIndex)
	slotToObj.reserve(objects.size());
	bool used[256] = {};
	for (size_t i = 0; i < objects.size(); ++i) {
		int slot = objects[i].originalIndex;
		if (slot >= 0 && slot < 256 && !used[slot]) {
			slotToObj.emplace_back(slot, i);
			used[slot] = true;
		}
	}
	int nextFree = 0;
	for (size_t i = 0; i < objects.size(); ++i) {
		if (objects[i].originalIndex >= 0 && objects[i].originalIndex < 256) continue;
		while (nextFree < 256 && used[nextFree]) ++nextFree;
		if (nextFree >= 256) break;
		slotToObj.emplace_back(nextFree, i);
		used[nextFree] = true;
		++nextFree;
	}
	// Walk slots in ascending order so byte layout matches the original.
	std::sort(slotToObj.begin(), slotToObj.end());

	// Event-record tables to write per object. Untouched objects write the
	// loaded bytes verbatim (byte-identical round trip); edited records are
	// patched in place; added/removed records rebuild the block as
	// [triggers][commands] with fresh table offsets.
	struct RecOut { std::vector<uint8_t> bytes; int32_t trig = -1, cmd = -1; bool rebuilt = false; };
	std::vector<RecOut> recOut(slotToObj.size());
	for (size_t k = 0; k < slotToObj.size(); ++k) {
		const Object& o = objects[slotToObj[k].second];
		RecOut& r = recOut[k];
		r.bytes = o.recordBytes;
		r.trig = o.triggerTableOff;
		r.cmd = o.commandTableOff;
		int32_t origFrames = (int32_t)o.frames.size();
		if (o.hasRawHeader) std::memcpy(&origFrames, o.rawHeader, 4);
		const int32_t framesEnd = 60 + origFrames * 132;
		if (o.recordsRelayout) {
			r.bytes.clear();
			const int32_t base = 60 + (int32_t)o.frames.size() * 132;
			r.trig = o.triggers.empty() ? -1 : base;
			r.cmd = o.commands.empty() ? -1 : base + 52 * (int32_t)o.triggers.size();
			for (const auto& e : o.triggers) r.bytes.insert(r.bytes.end(), e.raw, e.raw + 52);
			for (const auto& e : o.commands) r.bytes.insert(r.bytes.end(), e.raw, e.raw + 52);
			r.rebuilt = true;
		} else if (o.recordsEdited) {
			auto patch = [&](int32_t tableOff, const std::vector<EventRecord>& tab) {
				if (tableOff == -1) return;
				for (size_t i = 0; i < tab.size(); ++i) {
					int64_t at = (int64_t)tableOff - framesEnd + 52 * (int64_t)i;
					if (at >= 0 && at + 52 <= (int64_t)r.bytes.size())
						std::memcpy(r.bytes.data() + at, tab[i].raw, 52);
				}
			};
			patch(o.triggerTableOff, o.triggers);
			patch(o.commandTableOff, o.commands);
		}
	}

	// Assign each object a final byte offset. If we know the original
	// offset (loaded from file), honor it; the gap-from-end-of-previous
	// becomes zero-padding emitted in the write loop below. Editor-created
	// objects (originalOffset == -1) just pack tightly after the previous.
	int32_t pos = kFirstObjectOff;
	std::vector<int32_t> writeOffset(slotToObj.size());
	for (size_t k = 0; k < slotToObj.size(); ++k) {
		const auto& obj = objects[slotToObj[k].second];
		int32_t want = obj.originalOffset;
		if (want >= pos)
			pos = want;
		writeOffset[k] = pos;
		offsets[slotToObj[k].first] = pos;
		pos += 60 + (int32_t)obj.frames.size() * 132 + (int32_t)recOut[k].bytes.size();
	}

	for (int i = 0; i < 256; ++i) w32(offsets[i]);

	// --- Objects ---

	int32_t cursor = kFirstObjectOff;
	for (size_t k = 0; k < slotToObj.size(); ++k)
	{
		const auto& obj = objects[slotToObj[k].second];
		// Bridge any gap between the previous object's end and this one's
		// honored start by emitting zero padding.
		while (cursor < writeOffset[k]) {
			f.put('\0');
			++cursor;
		}
		// Object header: start from the loaded bytes so the event-table
		// offsets (+12/+16) and flags (+20..22) survive. The tables sit right
		// after the frames, so their relative offsets move by 132 bytes per
		// added/removed frame.
		{
			uint8_t hdr[60];
			if (obj.hasRawHeader) std::memcpy(hdr, obj.rawHeader, 60);
			else { std::memset(hdr, 0, 60); int32_t m1 = -1; std::memcpy(hdr + 12, &m1, 4); std::memcpy(hdr + 16, &m1, 4); }
			int32_t origFrames = 0;
			std::memcpy(&origFrames, hdr, 4);
			if (!obj.hasRawHeader) origFrames = (int32_t)obj.frames.size();
			int32_t delta = ((int32_t)obj.frames.size() - origFrames) * 132;
			int32_t nf = (int32_t)obj.frames.size();
			int32_t trig = recOut[k].trig, cmd = recOut[k].cmd;
			if (!recOut[k].rebuilt) {
				if (trig != -1) trig += delta;
				if (cmd  != -1) cmd  += delta;
			}
			std::memcpy(hdr + 0,  &nf, 4);
			std::memcpy(hdr + 4,  &obj.parallax, 4);
			std::memcpy(hdr + 8,  &obj.layer, 4);
			std::memcpy(hdr + 12, &trig, 4);
			std::memcpy(hdr + 16, &cmd, 4);
			hdr[20] = obj.noAutoSpawn;
			hdr[21] = obj.foreground;
			hdr[22] = obj.linearFilter;
			f.write((const char*)hdr, 60);
		}

		for (const auto& fr : obj.frames)
		{
			// Start from the verbatim record (or u4ick's default for new
			// frames), then overwrite every field the editor models.
			uint8_t rec[132];
			if (fr.hasRaw) std::memcpy(rec, fr.raw, 132);
			else { std::memset(rec, 0, 132); std::memset(rec + 100, 0xFF, 32); }
			auto p16 = [&](int off, int16_t v) { std::memcpy(rec + off, &v, 2); };
			p16(0, fr.spriteId); p16(2, fr.offsetX); p16(4, fr.offsetY); p16(6, fr.duration);
			rec[9] = fr.blendMode; rec[10] = fr.opacity; rec[11] = fr.aniType; rec[12] = fr.jumpFrame;
			p16(16, fr.scaleX); p16(18, fr.scaleY); rec[20] = fr.interpolate;
			rec[21] = fr.loopEnd; rec[22] = fr.loopCount;
			rec[44] = fr.flagClearX; rec[45] = fr.flagClearY; rec[46] = fr.flagSetX; rec[47] = fr.flagSetY;
			p16(52, fr.velX); p16(54, fr.velY); p16(60, fr.accX); p16(62, fr.accY);
			for (int k = 0; k < 8; ++k) { p16(100 + 2 * k, fr.triggerRef[k]); p16(116 + 2 * k, fr.commandRef[k]); }
			f.write((const char*)rec, 132);
		}
		// Event record tables follow the frames (verbatim).
		if (!recOut[k].bytes.empty())
			f.write((const char*)recOut[k].bytes.data(), recOut[k].bytes.size());
		// Advance cursor by what we just emitted so the next gap calculation
		// is right.
		cursor += 60 + (int32_t)obj.frames.size() * 132 + (int32_t)recOut[k].bytes.size();
	}

	// PAT block sits after the last object, with whatever padding the load
	// recorded between the two regions. Re-honor loadedPatFileOff if it
	// points past where we are now (otherwise just append).
	if (loadedPatFileOff > cursor) {
		while (cursor < loadedPatFileOff) {
			f.put('\0');
			++cursor;
		}
	}

	// --- Embedded PAT (only present in some stages, e.g. bg01 / bg20) ---
	std::streampos patStart = f.tellp();
	int32_t patOff = (int32_t)patStart;
	int32_t patLen = (int32_t)patData.size();
	if (patLen > 0)
		f.write((const char*)patData.data(), patLen);

	// --- Embedded CG ---
	std::streampos cgStart = f.tellp();
	int32_t cgOff = (int32_t)cgStart;
	int32_t cgLen = (int32_t)cgData.size();
	if (cgLen > 0)
		f.write((const char*)cgData.data(), cgLen);

	// --- Trailing alignment bytes ---
	if (!trailingBytes.empty())
		f.write((const char*)trailingBytes.data(), trailingBytes.size());

	// Patch pat_file_* and cg_file_* in the header. If we didn't write a
	// PAT block, keep the loaded offset (often the same as cg_off) so the
	// game's bookkeeping stays intact.
	std::streampos endPos = f.tellp();
	f.seekp(patRefPos);
	w32(patLen > 0 ? patOff : loadedPatFileOff);
	w32(patLen);
	f.seekp(cgRefPos);
	w32(cgOff);
	w32(cgLen);
	f.seekp(endPos);

	return true;
}


// ============================================================================
// Game-accurate runtime — 1:1 with MBAA.exe (names after this session's
// renames; addresses unchanged):
//   Background_SpawnInitialInstances  0x4b6e00   BgInstance_Init   0x4b6db0
//   BgInstance_EnterFrame             0x4b6d10   Background_StepInstanceAnimations 0x4b88b0
//   Background_IntegrateInstanceMotion 0x4b8530  Background_RunFrameCommands 0x4b8cd0
//   Background_RunPositionTriggers    0x4b8df0   BgCmd_* 0x4b8aa0/0x4b8b20/0x4b8c10
//   BgInstance_PlaceRelativeToParent  0x4b89e0 (camera fixed at the neutral 0,0)
// Random draws use the game's own generator (bg_rng.h: MBAACC stream 0 of
// the subtractive RNG bank, MBAC LCG stream 0), seeded by File::SetSeed, in
// the game's call order — so a seed replays the game's spawns and weather.
// ============================================================================
static constexpr int kMaxInstances = 2000;

int File::RandInt() {
	return rng.Next();
}

const Frame* File::InstanceFrame(const Instance& in) const {
	if (in.objIndex < 0 || in.objIndex >= (int)objects.size()) return nullptr;
	const Object& o = objects[in.objIndex];
	if (in.curFrame < 0 || in.curFrame >= (int)o.frames.size()) return nullptr;
	return &o.frames[in.curFrame];
}

void File::EnterFrame(Instance& in) {
	in.timer = 0;
	in.cmdDone = false;
	in.motionLoaded = false;
	const Frame* fr = InstanceFrame(in);
	if (!fr) return;
	if (fr->loopCount) in.loopCounter = fr->loopCount;
	switch (fr->aniType) {
	case 0: in.nextFrame = in.curFrame; break;
	case 1: case 3: in.nextFrame = in.curFrame + 1; break;
	case 2: case 4: in.nextFrame = fr->jumpFrame; break;
	case 5: in.nextFrame = (in.loopCounter > 1) ? fr->jumpFrame : fr->loopEnd; break;
	default: break;
	}
}

void File::InitInstance(Instance& in, int objIndex) {
	in = Instance();
	in.state = 1;
	in.objIndex = objIndex;
	in.curFrame = 0;
	EnterFrame(in);
}

int File::AllocInstance() {
	for (int i = 0; i < (int)instances.size(); ++i)
		if (instances[i].state == 0) return i;
	if ((int)instances.size() < kMaxInstances) { instances.emplace_back(); return (int)instances.size() - 1; }
	return -1;
}

void File::PlaceRelativeToParent(Instance& child, const Instance& parent, int x, int y) {
	// camX = camY = 0 (Camera_ResetState); 0x6000 = 24576 is the game's
	// constant horizontal bias in this formula.
	const Object& po = objects[parent.objIndex];
	const Object& co = objects[child.objIndex];
	double pp = (int16_t)po.parallax * (1.0 / 256.0);
	double cp = (int16_t)co.parallax * (1.0 / 256.0);
	child.posY = (int32_t)((double)(y << 7) + (double)parent.posY * pp);
	child.posX = (int32_t)((double)(x << 7) + pp * (double)(parent.posX + 0x6000) + cp * (double)(-0x6000));
}

void File::ResetRuntime() {
	// The game seeds stream 0 before the stage spawns (RngState_Initialize).
	rng.Seed(seed, game);
	tick = 0;
	instances.clear();
	instances.resize(256);
	for (auto& o : objects) {
		int idx = o.originalIndex;
		if (idx < 0 || idx >= 256) continue;
		if (o.noAutoSpawn || o.frames.empty()) continue;
		InitInstance(instances[idx], (int)(&o - objects.data()));
	}
	for (auto& o : objects) { o.currentFrame = 0; o.frameDuration = 0; o.posX = o.posY = 0; }
	// Background_SpawnInitialInstances ends with DropObject_InitializeParticles
	// (MBAA 0x4b6ee0). MBAC has no weather system.
	drops.Clear();
	if (game == Game::MBAACC) drops.Init(stageInfo, rng);
}

void File::TickRuntime() {
	++tick;
	// Index the file slot -> objects[] mapping once (spawn commands address
	// objects by their file slot, not by our dense index).
	int slotToObj[256];
	for (int i = 0; i < 256; ++i) slotToObj[i] = -1;
	for (size_t i = 0; i < objects.size(); ++i)
		if (objects[i].originalIndex >= 0 && objects[i].originalIndex < 256)
			slotToObj[objects[i].originalIndex] = (int)i;

	// 1) animation stepping
	for (auto& in : instances) {
		if (in.state == 1) in.state = 2;
		else if (in.state < 2) continue;
		const Object& o = objects[in.objIndex];
		const Frame* fr = InstanceFrame(in);
		if (!fr) { in.state = 0; continue; }
		++in.timer;
		if ((uint16_t)fr->duration > in.timer) continue;
		switch (fr->aniType) {
		case 0: in.state = 0; break;
		case 1: case 3: in.curFrame = (in.curFrame + 1) & 0xFF; EnterFrame(in); break;
		case 2: case 4:
			in.curFrame = fr->jumpFrame;
			if (in.loopCounter) --in.loopCounter;
			EnterFrame(in); break;
		case 5:
			if (in.loopCounter) --in.loopCounter;
			in.curFrame = in.loopCounter ? fr->jumpFrame : fr->loopEnd;
			EnterFrame(in); break;
		default: break;
		}
		if ((int)o.frames.size() <= (in.curFrame & 0xFF)) in.state = 0;
	}

	// 2) motion
	for (auto& in : instances) {
		if (in.state < 2) continue;
		const Frame* fr = InstanceFrame(in);
		if (!fr) continue;
		if (in.motionLoaded) {
			in.posX += in.velX; in.posY += in.velY;
			in.velX += in.accX; in.velY += in.accY;
		} else {
			if (fr->flagClearX) { in.velX = 0; in.accX = 0; }
			if (fr->flagClearY) { in.velY = 0; in.accY = 0; }
			if (fr->flagSetX)   { in.velX = fr->velX; in.accX = fr->accX; }
			if (fr->flagSetY)   { in.velY = fr->velY; in.accY = fr->accY; }
			in.motionLoaded = true;
		}
	}

	// 3) frame commands (once per frame entry). Iterate by index: spawning
	// may grow the pool; the game's fixed array lets a new instance be
	// visited later in the same pass, which the index loop reproduces.
	for (size_t i = 0; i < instances.size(); ++i) {
		if (instances[i].state < 2 || instances[i].cmdDone) continue;
		const Frame* fr = InstanceFrame(instances[i]);
		if (!fr) continue;
		const Object& o = objects[instances[i].objIndex];
		for (int k = 0; k < 8; ++k) {
			int ref = fr->commandRef[k];
			if (ref < 0 || ref >= (int)o.commands.size()) continue;
			const EventRecord rec = o.commands[ref];
			if (rec.type == 1 || rec.type == 2) {
				int slot = rec.w2;
				if (rec.type == 2) {
					int16_t cnt = *(const int16_t*)(rec.raw + 20);
					if (cnt < 1) cnt = 1;
					slot = (int16_t)(rec.w2 + RandInt() % cnt);
				}
				if (slot < 0 || slot >= 256 || slotToObj[slot] < 0) continue;
				if (objects[slotToObj[slot]].frames.empty()) continue;
				int ni = AllocInstance();
				if (ni < 0) continue;
				Instance& child = instances[ni];
				InitInstance(child, slotToObj[slot]);
				const Instance& parent = instances[i];
				if (rec.type == 1) {
					PlaceRelativeToParent(child, parent, rec.d[1], rec.d[2]);
				} else {
					int x = rec.d[1] + RandInt() % (rec.d[3] - rec.d[1] + 1);
					int y = rec.d[2] + RandInt() % (rec.d[4] - rec.d[2] + 1);
					PlaceRelativeToParent(child, parent, x, y);
				}
			} else if (rec.type == 100 && rec.w2 == 0) {
				Instance& in = instances[i];
				auto rr = [&](int lo, int hi) { return lo == hi ? lo : lo + RandInt() % (hi - lo); };
				if (rec.d[5]) { in.velY = rr(rec.d[1], rec.d[2]); in.accY = rr(rec.d[3], rec.d[4]); }
				else          { in.velX = rr(rec.d[1], rec.d[2]); in.accX = rr(rec.d[3], rec.d[4]); }
			}
		}
		instances[i].cmdDone = true;
	}

	// 4) position triggers
	for (auto& in : instances) {
		if (in.state < 2) continue;
		const Frame* fr = InstanceFrame(in);
		if (!fr) continue;
		const Object& o = objects[in.objIndex];
		for (int k = 0; k < 8; ++k) {
			int ref = fr->triggerRef[k];
			if (ref < 0 || ref >= (int)o.triggers.size()) continue;
			const EventRecord& rec = o.triggers[ref];
			if (rec.type != 1) continue;
			int32_t pos = rec.d[3] == 0 ? in.posX : (rec.d[3] == 1 ? in.posY : 0);
			if (rec.d[3] != 0 && rec.d[3] != 1) continue;
			bool fire = (rec.d[4] == 0) ? (rec.d[2] < pos) : (rec.d[4] == 1 ? (rec.d[2] > pos) : false);
			if (!fire) continue;
			if (rec.d[1] == -1) { in.state = 0; break; }
			in.curFrame = rec.d[1] & 0xFF;
			EnterFrame(in);
			fr = InstanceFrame(in);
			if (!fr) break;
		}
	}

	// 5) weather particles (Background_UpdateAndRender calls
	// DropObject_UpdateParticles right after the triggers).
	if (game == Game::MBAACC) drops.Update(rng);
}


// ============================================================================
// Side files, game flavour, editing helpers
// ============================================================================
namespace {
std::string LowerStr(std::string v) { for (char& c : v) c = (char)std::tolower((unsigned char)c); return v; }
std::string DirOf(const std::string& p) {
	size_t s = p.find_last_of("/\\");
	return s == std::string::npos ? std::string() : p.substr(0, s);
}
std::string BaseNoExt(const std::string& p) {
	size_t s = p.find_last_of("/\\");
	std::string b = s == std::string::npos ? p : p.substr(s + 1);
	size_t d = b.find_last_of('.');
	return d == std::string::npos ? b : b.substr(0, d);
}
} // namespace

static std::string FindSiblingVariant(const std::string& filename) {
	std::string dir = DirOf(filename), base = BaseNoExt(filename);
	std::string lb = LowerStr(base);
	std::string other;
	if (lb.size() > 2 && lb.compare(lb.size() - 2, 2, "_s") == 0) other = base.substr(0, base.size() - 2) + ".dat";
	else other = base + "_s.dat";
	return FindFileNoCase(dir, other);
}

const StageListEntry* File::GetStageListEntry() const {
	return stageList.FindByDataFile(BaseNoExt(filename));
}

std::string File::DropBitmapPath() const {
	if (stageInfo.dropFile.empty()) return std::string();
	return FindFileNoCase(DirOf(filename), stageInfo.dropFile + ".bmp");
}

void File::ReloadSideFiles() {
	std::string dir = DirOf(filename), base = BaseNoExt(filename);
	std::string lb = LowerStr(base);
	shortVariant = lb.size() > 2 && lb.compare(lb.size() - 2, 2, "_s") == 0;
	siblingPath = FindSiblingVariant(filename);
	std::string stem = shortVariant ? base.substr(0, base.size() - 2) : base;

	stageList = StageList();
	std::string ini = FindFileNoCase(dir, "BgList.ini");
	if (!ini.empty()) stageList.Load(ini);

	// MBAACC: Background_LoadInfoFile always opens "<DataFile>Info.txt".
	stageInfo = StageInfo();
	std::string info = FindFileNoCase(dir, stem + "Info.txt");
	if (!info.empty()) stageInfo.Load(info);

	// MBAC: LoadLightingData opens "bg%02dlight.txt".
	lightFile = LightFile();
	std::string lt = FindFileNoCase(dir, stem + "light.txt");
	if (!lt.empty()) lightFile.Load(lt);

	// Flavour guess: a BgList.ini next to the stage means MBAACC's loose bg
	// folder; an _s variant or an upper-case MBAC dump name means MBAC.
	bool upper = !base.empty() && base.find_first_of("abcdefghijklmnopqrstuvwxyz") == std::string::npos;
	if (!ini.empty())                         game = Game::MBAACC;
	else if (shortVariant || upper || !siblingPath.empty()) game = Game::MBAC;
	else                                      game = Game::MBAACC;
}

std::vector<File::LightView> File::ActiveLights() const {
	std::vector<LightView> out;
	if (game == Game::MBAACC) {
		// Character_Render 0x41b411: lightX = Pos - 512, weight 1 - |x - lightX| / Power.
		for (const auto& l : stageInfo.lights) out.push_back({l.pos - 512, l.power});
	} else {
		// mbacPC Character_Draw 0x44839f: weight 1 - |(x>>7) - Pos + 256| / Power.
		for (const auto& l : lightFile.lights) out.push_back({l.pos - 256, l.power});
	}
	return out;
}

int File::InsertFrame(int objIndex, int at, bool duplicate) {
	if (objIndex < 0 || objIndex >= (int)objects.size()) return -1;
	Object& o = objects[objIndex];
	if (o.frames.size() >= 255) return -1;          // frame indices are bytes
	at = std::max(0, std::min(at, (int)o.frames.size()));
	Frame f;
	if (duplicate && !o.frames.empty()) {
		f = o.frames[std::min(at, (int)o.frames.size() - 1)];
		at = std::min(at + 1, (int)o.frames.size());
	}
	o.frames.insert(o.frames.begin() + at, f);
	MarkDirty();
	ResetRuntime();
	return at;
}

bool File::DeleteFrame(int objIndex, int at) {
	if (objIndex < 0 || objIndex >= (int)objects.size()) return false;
	Object& o = objects[objIndex];
	if (at < 0 || at >= (int)o.frames.size() || o.frames.size() <= 1) return false;
	o.frames.erase(o.frames.begin() + at);
	MarkDirty();
	ResetRuntime();
	return true;
}

int File::AddRecord(int objIndex, bool trigger) {
	if (objIndex < 0 || objIndex >= (int)objects.size()) return -1;
	Object& o = objects[objIndex];
	auto& tab = trigger ? o.triggers : o.commands;
	EventRecord r;
	r.type = 1;
	if (trigger) { r.d[1] = -1; r.d[3] = 1; }   // "despawn when y > 0"
	r.SyncRaw();
	tab.push_back(r);
	o.recordsRelayout = true;
	MarkDirty();
	return (int)tab.size() - 1;
}

bool File::DeleteLastRecord(int objIndex, bool trigger) {
	if (objIndex < 0 || objIndex >= (int)objects.size()) return false;
	Object& o = objects[objIndex];
	auto& tab = trigger ? o.triggers : o.commands;
	if (tab.empty()) return false;
	int gone = (int)tab.size() - 1;
	tab.pop_back();
	// Drop frame refs that pointed at it.
	for (auto& f : o.frames)
		for (int k = 0; k < 8; ++k) {
			int16_t& r = trigger ? f.triggerRef[k] : f.commandRef[k];
			if (r == gone) r = -1;
		}
	o.recordsRelayout = true;
	MarkDirty();
	return true;
}


// ---- stage edit history --------------------------------------------------------

static constexpr size_t kMaxStageUndo = 100;

void File::ResetHistory() {
	committedSerial = editSerial;
	undoStack.clear();
	redoStack.clear();
	baseline.objects = objects;
	baseline.dirty = dirty;
	committedSerial = editSerial;
}

void File::CommitEdit() {
	undoStack.push_back(std::move(baseline));
	if (undoStack.size() > kMaxStageUndo) undoStack.erase(undoStack.begin());
	redoStack.clear();
	baseline.objects = objects;
	baseline.dirty = dirty;
	committedSerial = editSerial;
}

void File::RestoreSnapshot(const EditSnapshot& snap) {
	bool reshaped = snap.objects.size() != objects.size();
	for (size_t i = 0; !reshaped && i < objects.size(); ++i)
		reshaped = snap.objects[i].frames.size() != objects[i].frames.size();
	std::vector<bool> vis;
	for (const auto& o : objects) vis.push_back(o.visible);
	objects = snap.objects;
	for (size_t i = 0; i < objects.size() && i < vis.size(); ++i) objects[i].visible = vis[i];
	dirty = snap.dirty;
	baseline.objects = objects;
	baseline.dirty = dirty;
	committedSerial = ++editSerial;
	if (reshaped) ResetRuntime();
}

bool File::Undo() {
	if (undoStack.empty()) return false;
	redoStack.push_back({objects, dirty});
	EditSnapshot snap = std::move(undoStack.back());
	undoStack.pop_back();
	RestoreSnapshot(snap);
	return true;
}

bool File::Redo() {
	if (redoStack.empty()) return false;
	undoStack.push_back({objects, dirty});
	EditSnapshot snap = std::move(redoStack.back());
	redoStack.pop_back();
	RestoreSnapshot(snap);
	return true;
}

} // namespace bg

