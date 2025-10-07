#ifndef BG_FILE_H_GUARD
#define BG_FILE_H_GUARD

#include "bg_types.h"
#include "../cg.h"
#include <memory>

namespace bg {

// Background file - essentially ha4 format with embedded CG
// Header structure matches bgmake .dat format
class File {
public:
	File();
	~File();
	
	// Load from .dat file
	bool Load(const char* filename);
	void Free();
	
	// Accessors
	std::vector<Object>& GetObjects() { return objects; }
	const std::vector<Object>& GetObjects() const { return objects; }
	
	CG* GetCG() { return cg.get(); }
	const CG* GetCG() const { return cg.get(); }
	
	bool IsLoaded() const { return loaded; }
	
	// Update all object animations
	void UpdateAnimations();
	
private:
	bool loaded = false;
	
	// File header (64 bytes)
	struct Header {
		char magic[16];          // "bgmake_array"
		int32_t unk;
		int32_t pat_file_off;    // Usually -1 (unused)
		int32_t pat_file_len;    // Usually 0 (unused)
		int32_t cg_file_off;     // Offset to embedded CG
		int32_t cg_file_len;     // Size of embedded CG
		uint8_t reserved[48];
	};
	
	// Object offset table (256 entries)
	int32_t offsetTable[256];
	
	// Data
	std::vector<Object> objects;
	std::vector<uint8_t> cgData;     // Raw embedded CG data
	std::unique_ptr<CG> cg;          // Loaded CG file
	
	// Loading helpers
	bool LoadHeader(const char* data, size_t size, Header& header);
	bool LoadOffsetTable(const char* data, size_t size);
	bool LoadObjects(const char* data, size_t size, const Header& header);
	bool LoadEmbeddedCG(const char* data, size_t size, const Header& header);
};

} // namespace bg

#endif /* BG_FILE_H_GUARD */

