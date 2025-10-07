#include "bg_file.h"
#include "../misc.h"
#include <cstring>
#include <fstream>
#include <iostream>

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

// Object animation logic
void Object::Update() {
	if (frames.empty()) return;
	
	frameDuration++;
	
	Frame& frame = frames[currentFrame];
	if (frameDuration >= frame.duration) {
		frameDuration = 0;

		// bgmake logic - just advance frames, no vector application in Update()
		// Vectors are NOT used during animation update in bgmake
		switch (frame.aniType) {
			case 0:  // End - stop
				break;

			case 1:  // Loop
				currentFrame++;
				if (currentFrame >= (int)frames.size()) {
					currentFrame = 0;
				}
				break;

			case 2:  // Jump
				currentFrame = frame.jumpFrame;
				if (currentFrame >= (int)frames.size()) {
					currentFrame = 0;  // Safety
				}
				break;
		}
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

} // namespace bg

