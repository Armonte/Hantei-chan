#include "bg_file.h"
#include "../misc.h"
#include <algorithm>
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
		// objHeader[3] and [4] are reserved (-1)
		
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
			
			frame.spriteId = frameData16[0] - 10000;  // File stores as +10000
			frame.offsetX = frameData16[1];
			frame.offsetY = frameData16[2];
			frame.duration = frameData16[3];
			
			// Byte fields at offset 8
			frame.blendMode = frameData8[9];
			frame.opacity = frameData8[10];
			frame.aniType = frameData8[11];
			frame.jumpFrame = frameData8[12];

			// Debug opacity values for first few frames
			//if (i < 3 && f < 2) {
			//	printf("[BG Load] Obj %d Frame %d: opacity byte=%u (%.2f alpha), blend=%u\n",
			//	       i, f, frame.opacity, frame.opacity / 255.0f, frame.blendMode);
			//}

			// Vector fields at offsets 0x2D, 0x33, 0x35
			frame.enableXVec = frameData8[0x2D];
			frame.enableYVec = frameData8[0x2E];
			frame.xVec = *(const int16_t*)(data + pos + 0x33);
			frame.yVec = *(const int16_t*)(data + pos + 0x35);
			
			obj.frames.push_back(frame);
			pos += 132;
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
	
	return true;
}

void File::UpdateAnimations() {
	for (auto& obj : objects) {
		obj.Update();
	}
}

// Hantei4 animation_flow (anim_type, +11) values — see han4docs
// COMPLETE_HANTEI4_FIELD_MAPPING_FINAL.md:128-138:
//   0 = Ordinance/normal       3 = Next + landing rules
//   1 = Next (advance)         4 = Jump + landing rules
//   2 = Jump                   5 = Loop check (Loop ED)
// Types 2, 4 and 5 all redirect the frame cursor via jump_frame; types
// 0, 1 and 3 advance to the next frame. u4ick's bgmaketool RenderForever
// only special-cased 2, so type-5 "Loop ED" effects (bg04 obj_4/obj_5
// end on an aniType-5 frame) froze on the last frame in the editor even
// though the game loops them. We handle all three jump types.
static inline bool IsJumpType(uint8_t aniType) {
	return aniType == 2 || aniType == 4 || aniType == 5;
}

// Object animation logic — port of u4ick's bgmaketool per-object frame
// stepping (Form1.cs RenderForever, lines 213-254), extended to treat
// aniType 4/5 as jumps. Runs once per 60Hz tick.
//
// anim_type, as the stepping treats it:
//   0, 1, 3 -> "advance" frame. Move to the next frame; if already on the
//              LAST frame, stay there (the animation stops). The stepping
//              does not distinguish them — only the render path does (a
//              dur==0 && type==1 frame is skipped when drawing). A
//              multi-frame animation does NOT loop just by being type 1.
//   2, 4, 5 -> "jump" frame (IsJumpType). Redirects currentFrame via
//              jump_frame. Looping is expressed by ending an animation on
//              a jump-type frame whose jump_frame points to the loop
//              start (bg26 obj[0]: type-2 jump=0; bg04 obj[4]: type-5
//              jump=0).
//
// Duration: a frame is held for (duration + 1) ticks. u4ick increments
// frame_duration_index while it is strictly < duration, and only advances
// on the tick where it is no longer < duration. The previous code here
// advanced after `duration` ticks, making every animation run too fast.
void Object::Update() {
	if (frames.empty()) return;

	const int count = (int)frames.size();
	Frame* frame1 = (currentFrame >= 0 && currentFrame < count)
	                ? &frames[currentFrame] : &frames[0];

	if (frameDuration < frame1->duration) {
		frameDuration++;
		return;
	}

	// Advance. The loop steps again in the same tick if it lands on a
	// duration-0 frame (the condition re-tests against the new frame1
	// after frameDuration is reset by the loop's post-statement).
	for (; frameDuration >= frame1->duration; frameDuration = 0) {
		if (IsJumpType(frame1->aniType)) {
			currentFrame = frame1->jumpFrame;
			int idx = (currentFrame >= count) ? 0 : currentFrame;
			Frame& frame2 = frames[idx];
			currentFrame = (int)frame2.jumpFrame - 1;
			frameDuration = 0;
			if (IsJumpType(frame2.aniType)) {
				currentFrame = frame2.jumpFrame;
				break;
			}
		}
		if (currentFrame >= count - 1) {
			if (currentFrame > count - 1)
				currentFrame = 0;
			break;
		}
		if (currentFrame < count)
			currentFrame++;
		frame1 = &frames[currentFrame];
	}
}

void Object::Reset() {
	currentFrame = 0;
	frameDuration = 0;
	for (auto& frame : frames) {
		frame.runtimeX = 0.0f;
		frame.runtimeY = 0.0f;
	}
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
		pos += 60 + (int32_t)obj.frames.size() * 132;
	}

	for (int i = 0; i < 256; ++i) w32(offsets[i]);

	// --- Objects ---
	// 32-byte trailing buffer is 0xFF-filled per u4ick.
	uint8_t ffBuf[32];
	std::memset(ffBuf, 0xFF, sizeof(ffBuf));

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
		w32((int32_t)obj.frames.size());
		w32(obj.parallax);
		w32(obj.layer);
		w32(-1);            // reserved
		w32(-1);            // reserved
		wpad(40);

		for (const auto& fr : obj.frames)
		{
			w16(fr.spriteId + 10000);
			w16(fr.offsetX);
			w16(fr.offsetY);
			w16(fr.duration);
			w8(0);                    // unk byte (not tracked in our struct)
			w8(fr.blendMode);
			w8(fr.opacity);
			w8(fr.aniType);
			w8(fr.jumpFrame);
			wpad(32);                 // zero pad
			w8(fr.enableXVec);
			w8(fr.enableYVec);
			wpad(4);                  // zero pad
			w16(fr.xVec);
			w16(fr.yVec);
			wpad(45);                 // zero pad
			f.write((const char*)ffBuf, 32);
		}
		// Advance cursor by what we just emitted so the next gap calculation
		// is right.
		cursor += 60 + (int32_t)obj.frames.size() * 132;
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

} // namespace bg

