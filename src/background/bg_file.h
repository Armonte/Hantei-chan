#ifndef BG_FILE_H_GUARD
#define BG_FILE_H_GUARD

#include "bg_types.h"
#include "../cg.h"
#include <memory>

class Parts;

namespace bg {

// Background file - essentially ha4 format with embedded CG
// Header structure matches bgmake .dat format
class File {
public:
	File();
	~File();
	
	// Load from .dat file
	bool Load(const char* filename);
	// Save back to .dat file. Preserves the embedded CG bytes verbatim
	// (we do not re-encode CG on save).
	bool Save(const char* filename);
	void Free();
	
	// Accessors
	std::vector<Object>& GetObjects() { return objects; }
	const std::vector<Object>& GetObjects() const { return objects; }
	
	CG* GetCG() { return cg.get(); }
	const CG* GetCG() const { return cg.get(); }

	// Embedded PAT, parsed into the editor's Parts system. Stage objects
	// whose frame sprite-id is < 10000 reference a PAT pattern (not a CG
	// sprite) — see bg_renderer. Null if the stage carries no PAT.
	Parts* GetParts() { return parts.get(); }

	bool IsLoaded() const { return loaded; }

	// Get filename
	const std::string& GetFilename() const { return filename; }

	// Update all object animations
	void UpdateAnimations();

	// Step a specific object forward/backward by one frame
	void StepObjectForward(int objIndex);
	void StepObjectBackward(int objIndex);

private:
	bool loaded = false;
	std::string filename;

	// File header (84 bytes — magic+pad=16, then 5*int32, then 48-byte reserved).
	struct Header {
		char magic[16];          // "bgmake" + zero pad
		int32_t unk;
		int32_t pat_file_off;    // Usually -1 (unused)
		int32_t pat_file_len;    // Usually 0 (unused)
		int32_t cg_file_off;     // Offset to embedded CG
		int32_t cg_file_len;     // Size of embedded CG
		uint8_t reserved[48];
	};

	// Header fields preserved from load so Save can write them back verbatim
	// instead of zeroing them out. Critical for byte-1:1 round-trip.
	int32_t loadedUnk = 0;
	int32_t loadedPatFileOff = -1;
	int32_t loadedPatFileLen = 0;

	// Raw embedded PAT file bytes (between objects and CG in stages like
	// bg01/bg20). Preserved verbatim for byte-1:1 round-trip; the editor
	// doesn't introspect them.
	std::vector<uint8_t> patData;

	// Bytes that lived in the file AFTER the embedded CG (all stages have
	// 16KB of trailing padding/alignment data).
	std::vector<uint8_t> trailingBytes;

	// Object offset table (256 entries)
	int32_t offsetTable[256];

	// Data
	std::vector<Object> objects;
	std::vector<uint8_t> cgData;     // Raw embedded CG data
	std::unique_ptr<CG> cg;          // Loaded CG file
	std::unique_ptr<Parts> parts;    // Embedded PAT, parsed (may be null)
	
	// Loading helpers
	bool LoadHeader(const char* data, size_t size, Header& header);
	bool LoadOffsetTable(const char* data, size_t size);
	bool LoadObjects(const char* data, size_t size, const Header& header);
	bool LoadEmbeddedCG(const char* data, size_t size, const Header& header);
};

} // namespace bg

#endif /* BG_FILE_H_GUARD */


